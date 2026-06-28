// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Worker-backed implementation of EmulatorControls (Phase 1 of the web-worker
// migration — see docs/web-worker-emulation-scope.md). The WASM module + the
// stepFrame loop live in emulator-worker.js; this hook is a thin client that
// drives it and exposes the SAME EmulatorControls surface as useEmulator, so
// the existing UI renders unchanged.
//
// Done: cold boot, OffscreenCanvas render, keyboard/touch input, CF attach/
// detach, pause/reset, save-state SAVE + RESTORE, speaker + mic audio, OS-card
// mount/boot, SSD + Datapak attach/detach, log forwarding, and serial/PLP/IrDA
// (the protocol clients stay on the main thread; the worker streams UART bytes
// both ways and its continuous run replaces the old main-thread serial pump).
// Deferred: save-state export/import bundles only.
import { useRef, useState, useEffect, useCallback, useMemo } from 'react';
import type { DeviceInfo, DeviceProfile } from '../types/emulator';
import { browserKeyToEpocChord, charToEpocChord, keyboardLayoutForDevice } from '../lib/keymap';
import { EmulatorWorkerClient, type WorkerStatus } from '../lib/emulatorWorkerClient';
import { createAudioEngine, type AudioEngine } from '../lib/audioEngine';
import { createWorkerAudioShim, type WorkerAudioShim } from '../lib/workerAudioShim';
import { setSerialPumpForward, setSimKeepAliveForward } from '../lib/wasmBridge';
import { quiesceActiveSessions } from '../lib/plp/client-spec';
import { osCardSpec, buildOsCardImage, collectStatesBundle, applyStatesBundle, listSavedDevices, triggerDownload } from './useEmulator';
import type { EmulatorControls, EmulatorState, UseEmulatorOptions } from './useEmulator';

type Ev = { key: number; down: boolean };
function expandChord(modifiers: number[], key: number): Ev[] {
  const q: Ev[] = [];
  for (const m of modifiers) q.push({ key: m, down: true });
  q.push({ key, down: true });
  q.push({ key, down: false });
  for (let i = modifiers.length - 1; i >= 0; i--) q.push({ key: modifiers[i], down: false });
  return q;
}

export function useEmulatorWorker(options: UseEmulatorOptions = {}): EmulatorControls {
  const { embedMode = false, initialDeviceId } = options;
  const [state, setState] = useState<EmulatorState>('loading-wasm');
  const [loadProgress, setLoadProgress] = useState<number | null>(null);
  const [loadStatus, setLoadStatus] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [deviceInfo, setDeviceInfo] = useState<DeviceInfo | null>(null);
  const [profiles, setProfiles] = useState<DeviceProfile[]>([]);
  const [paused, setPaused] = useState(false);
  const [currentDeviceId, setCurrentDeviceId] = useState<string | null>(null);
  const [audioError, setAudioError] = useState<string | null>(null);
  const [speakerEnabled, setSpeakerEnabled] = useState(false);
  const [micEnabled, setMicEnabled] = useState(false);
  const [cardAttached, setCardAttached] = useState(false);
  const [osDownloading, setOsDownloading] = useState(false);
  const [logs, setLogs] = useState<string[]>([]);
  const [ssdAttached, setSsdAttached] = useState<boolean[]>([false, false, false, false]);
  const [datapakAttached, setDatapakAttached] = useState<boolean[]>([false, false]);
  const datapakKindRef = useRef<number[]>([0, 0]);
  const [savedDevices, setSavedDevices] = useState<string[]>([]);
  // Serial (PLP/IrDA): the protocol clients stay on the main thread and drive the
  // sync serialReadBytes/serialWriteBytes interface. The worker streams UART
  // output here (onSerialRx → per-uart buffer) and we ship host writes back; the
  // worker's continuous run replaces the old main-thread pump.
  const serialAttachedRef = useRef<Record<number, boolean>>({});
  const serialRxRef = useRef<Record<number, Uint8Array[]>>({});
  const serialRxDiagRef = useRef<Record<number, number>>({});   // capped serial-RX trace counters
  const loggingOnRef = useRef(false);   // mirrors the Show Logs toggle; gates the serial diag
  // Speaker audio (Phase 2): the worker drains the DAC and posts int16 batches;
  // a shim `mod` hands them to the full main-thread AudioEngine so the worklet/
  // AudioContext machinery is reused unchanged. Mic input is still deferred.
  const audioEngineRef = useRef<AudioEngine | null>(null);
  const audioShimRef = useRef<WorkerAudioShim | null>(null);

  const clientRef = useRef<EmulatorWorkerClient | null>(null);
  const statusRef = useRef<WorkerStatus>({ paused: false, simCycles: 0, backlight: false, cfGap: false, cardAttached: false });
  const transferredElRef = useRef<HTMLCanvasElement | null>(null);
  // canvasRef is a CALLBACK ref (not a plain RefObject): the <canvas> is
  // unmounted/remounted when the user opens Settings or while a device loads, and
  // an OffscreenCanvas can only be transferred once per element — so we must
  // re-transfer the NEW element to the worker each time it mounts, or the worker
  // keeps painting the old (detached) offscreen and the UI shows a grey screen.
  const canvasRef = useMemo(() => {
    const fn = ((el: HTMLCanvasElement | null) => {
      fn.current = el;
      if (el && clientRef.current && transferredElRef.current !== el) {
        if (typeof el.transferControlToOffscreen !== 'function') {
          setError('This browser lacks OffscreenCanvas; worker mode is unavailable.');
          return;
        }
        transferredElRef.current = el;
        clientRef.current.setCanvas(el.transferControlToOffscreen());
      }
    }) as ((el: HTMLCanvasElement | null) => void) & { current: HTMLCanvasElement | null };
    fn.current = null;
    return fn;
  }, []) as unknown as React.RefObject<HTMLCanvasElement>;
  const lastLoadRef = useRef<{ deviceId: string; romUrl: string } | null>(null);
  const epocShiftRef = useRef(false);
  const keydownHandledRef = useRef(false);
  // Mirror of currentDeviceId for the input callbacks (which are memoised with
  // an empty dep list, so they can't read the state value without going
  // stale).  Kept in sync wherever setCurrentDeviceId runs.
  const deviceIdRef = useRef<string | null>(null);
  const lastDeviceModeSent = useRef<boolean | null>(null);

  // deviceModeRef is written directly by EmulatorView each render; intercept the
  // write so the worker (which owns the LUT) stays in sync without a poll.
  const deviceModeRef = useRef<{ current: boolean }>(
    Object.defineProperty({ _v: false } as { _v: boolean; current: boolean }, 'current', {
      get() { return this._v; },
      set(v: boolean) {
        this._v = v;
        if (v !== lastDeviceModeSent.current) { lastDeviceModeSent.current = v; clientRef.current?.setDeviceMode(v); }
      },
    }),
  ).current as unknown as React.MutableRefObject<boolean>;

  // Spin up the worker + module once.
  useEffect(() => {
    const base = import.meta.env.BASE_URL;
    const client = new EmulatorWorkerClient(`${base}emulator-worker.js`);
    clientRef.current = client;
    // Route the dialogs'/PlpClient's setSerialPumpEnabled calls into the worker
    // (the main-thread flag is never consulted there). Without this the
    // transfer-mode pump is silently lost in worker mode.
    setSerialPumpForward(on => clientRef.current?.setSerialPump(on));
    // Worker-mode sim-time keepalive: forward PlpClient's keepalive frame to
    // the worker, which replays it once per sim-frame so the device's
    // RemoteLinkServer stays rescheduled regardless of sim speed.
    setSimKeepAliveForward(bytes => clientRef.current?.setSimKeepAlive(bytes));
    client.onStatus = (s) => {
      statusRef.current = s;
      setPaused(s.paused);
      setCardAttached(s.cardAttached);
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      (window as any).__psionWorkerStatus = s;   // test hook
    };
    client.onError = (m) => setError(m);
    // Real load progress streamed from the worker (download fraction + phase
    // text) — previously the bar sat frozen at a hardcoded 30% for the whole
    // load, which on slow mobile connections looked hung.
    client.onLoadProgress = (value, status) => { setLoadProgress(value); setLoadStatus(status); };
    client.onLog = (text, err) => setLogs(l => [...l.slice(-499), (err ? '⚠ ' : '') + text]);
    // Speaker batches from the worker → shim queue → engine.pump() feeds the
    // worklet (and keeps the AudioContext resumed) at the worker's tick cadence.
    client.onAudio = (samples) => {
      audioShimRef.current?.pushSamples(samples);
      audioEngineRef.current?.pump();
    };
    client.onSerialRx = (uart, bytes) => {
      (serialRxRef.current[uart] ??= []).push(bytes);
      // Diagnostic (capped, gated on Show Logs): confirm device→host bytes cross
      // the worker boundary into the per-UART read buffer (remote-link/IrDA).
      const k = serialRxDiagRef.current;
      if (loggingOnRef.current && (k[uart] = (k[uart] ?? 0) + 1) <= 6) {
        const head = Array.from(bytes.subarray(0, 12)).map(b => b.toString(16).padStart(2, '0')).join(' ');
        setLogs(l => [...l.slice(-499), `[serial] main onSerialRx uart=${uart} n=${bytes.length} head=${head}`]);
      }
    };
    let cancelled = false;
    client.init(base).then(async () => {
      if (cancelled) return;
      try { setProfiles((await client.getProfiles()) as DeviceProfile[]); } catch { /* keep empty */ }
      void listSavedDevices().then(setSavedDevices);   // populate the saved-state list
      setState('ready');
      const lastId = initialDeviceId || (embedMode ? null : localStorage.getItem('psion-last-device'));
      if (lastId) {
        const profs = (await client.getProfiles().catch(() => [])) as DeviceProfile[];
        const p = profs.find(x => x.id === lastId && x.status === 'supported');
        if (p) void doLoad(lastId, `${base}roms/${p.romFilename}`);
      }
    }).catch(err => { setError(String(err)); setState('error'); });
    return () => {
      cancelled = true; setSerialPumpForward(null); setSimKeepAliveForward(null);
      clientRef.current = null;
      // Save the running device before tearing the worker down. Navigating
      // to a non-emulator route (the app library, usage page) unmounts this
      // hook, and terminating immediately dropped everything since the last
      // autosave. The worker stays alive until the save settles; the 5 s
      // deadline stops a wedged worker leaking forever.
      const grace = new Promise<void>(r => setTimeout(r, 5000));
      void Promise.race([client.saveState().then(() => undefined, () => undefined), grace])
        .finally(() => client.destroy());
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // (Canvas transfer is handled by the callback ref above — it re-binds the
  // worker's OffscreenCanvas whenever the <canvas> element remounts.)

  async function doLoad(deviceId: string, romUrl: string, restore = true): Promise<void> {
    const client = clientRef.current;
    if (!client) return;
    lastLoadRef.current = { deviceId, romUrl };
    // Park any live Remote Link session BEFORE the worker auto-saves the
    // outgoing device (inside loadDevice). The device is still stepping
    // here, so quiescing lets its last reply get acked; a session
    // snapshotted mid-frame dies after restore (its retransmit budget
    // expires) and the next connect can't adopt it. Runs on a clean
    // (no-link) switch as an instant no-op.
    await quiesceActiveSessions({ release: true });
    // Host serial bridges don't survive a device load (the worker resets its
    // attach flags too). Stale `true` flags here made serialAttachHost
    // early-return without re-attaching the NEW device instance, so the
    // remote-link/IrDA writes were silently dropped and reads stayed empty.
    serialAttachedRef.current = {};
    serialRxRef.current = {};
    setError(null);
    setState('loading-rom');
    setLoadProgress(0);
    setLoadStatus(null);
    try {
      const { info, ssdAttached: ssdSlots, datapakAttached: pakSlots, serialAttached: serialState } =
        await client.loadDevice(deviceId, romUrl, 0, restore);
      // Seed the host serial-bridge tracking from the restored heap's
      // actual UART hostAttached flags (reported by the worker). A
      // restored device with a parked Remote Link session comes back
      // attached; keeping the flag accurate lets RemoteLinkDialog's
      // connect skip a redundant serialAttachHost() — which would clear
      // the UART FIFOs and raise a cable-plug IRQ that the ROM treats as
      // session-fatal, breaking adoption on the device-switch reconnect.
      // (We reset serialAttachedRef to {} above so a stale flag from the
      // outgoing device can't linger; this re-derives only the genuine
      // ones.) Matches the main-thread hook, which reads serialIsAttached
      // straight from the module.
      serialAttachedRef.current = { ...(serialState ?? {}) };
      setDeviceInfo(info);
      setCurrentDeviceId(deviceId);
      deviceIdRef.current = deviceId;
      // The worker re-attaches persisted SSD packs / Datapaks during load;
      // mirror its per-slot state so the dialogs (and the app-library
      // free-slot picker) see them.
      setSsdAttached(ssdSlots ?? [false, false, false, false]);
      setDatapakAttached(pakSlots ?? [false, false]);
      if (!embedMode) localStorage.setItem('psion-last-device', deviceId);
      setLoadProgress(null);
      setLoadStatus(null);
      setState('running');
      // The worker auto-saves the outgoing device inside loadDevice; refresh
      // the saved-devices list so its "saved" state shows immediately.
      refreshSaved();
    } catch (err) {
      setLoadProgress(null);
      setLoadStatus(null);
      setError(String(err));
      setState('error');
    }
  }

  const loadDevice = useCallback((deviceId: string, romUrl: string) => doLoad(deviceId, romUrl), []); // eslint-disable-line react-hooks/exhaustive-deps

  // ── Input (mirrors useEmulator, routed to the worker) ──
  const handleKeyDown = useCallback((e: KeyboardEvent) => {
    keydownHandledRef.current = false;
    const fromMobile = Boolean((e.target as HTMLElement)?.dataset?.psionInput);
    if (fromMobile && e.key.length === 1) return;
    const chord = browserKeyToEpocChord(e, keyboardLayoutForDevice(deviceIdRef.current));
    if (chord === null) return;
    e.preventDefault();
    keydownHandledRef.current = true;
    if (chord.key === 18 || chord.key === 19) epocShiftRef.current = true;
    if (chord.modifiers.length === 0 && !fromMobile) { clientRef.current?.sendKey(chord.key, true); return; }
    const mods = chord.modifiers.filter(m => !((m === 18 || m === 19) && epocShiftRef.current));
    clientRef.current?.enqueue(expandChord(mods, chord.key));
  }, []);

  const handleKeyUp = useCallback((e: KeyboardEvent) => {
    const chord = browserKeyToEpocChord(e, keyboardLayoutForDevice(deviceIdRef.current));
    if (chord === null) return;
    e.preventDefault();
    if (chord.key === 18 || chord.key === 19) epocShiftRef.current = false;
    const fromMobile = Boolean((e.target as HTMLElement)?.dataset?.psionInput);
    if (chord.modifiers.length === 0 && !fromMobile) clientRef.current?.sendKey(chord.key, false);
  }, []);

  const injectText = (text: string) => {
    const layout = keyboardLayoutForDevice(deviceIdRef.current);
    const evs: Ev[] = [];
    for (const ch of text) { const c = charToEpocChord(ch, layout); if (c) evs.push(...expandChord(c.modifiers, c.key)); }
    if (evs.length) clientRef.current?.enqueue(evs);
  };

  const handleInput = useCallback((e: Event) => {
    const ie = e as InputEvent;
    const ta = e.target as HTMLTextAreaElement;
    ta.value = '';
    if (keydownHandledRef.current) { keydownHandledRef.current = false; return; }
    keydownHandledRef.current = false;
    if (ie.inputType === 'deleteContentBackward') { clientRef.current?.enqueue(expandChord([], 1)); return; }
    if (ie.inputType === 'deleteContentForward') { clientRef.current?.enqueue(expandChord([], 13)); return; }
    if (ie.inputType === 'insertLineBreak' || ie.inputType === 'insertParagraph') { clientRef.current?.enqueue(expandChord([], 3)); return; }
    if (!ie.data) return;
    injectText(ie.data);
  }, []);

  const handlePasteText = useCallback((text: string) => injectText(text), []);
  const pasteFromClipboard = useCallback(async () => {
    if (navigator.clipboard?.readText) {
      try { const t = await navigator.clipboard.readText(); if (t) injectText(t); return; } catch { /* fall through */ }
    }
    const t = window.prompt('Paste text to send to the device:');
    if (t) injectText(t);
  }, []);
  const sendEpocKey = useCallback((k: number, down: boolean) => clientRef.current?.sendKey(k, down), []);
  const pressEpocKey = useCallback((k: number) => clientRef.current?.enqueue(expandChord([], k)), []);
  const pressEpocChord = useCallback((mods: number[], k: number) => clientRef.current?.enqueue(expandChord(mods, k)), []);
  const handlePointerDown = useCallback((x: number, y: number) => clientRef.current?.sendTouch(x, y, true), []);
  const handlePointerMove = useCallback((x: number, y: number) => clientRef.current?.sendTouch(x, y, true), []);
  const handlePointerUp = useCallback(() => clientRef.current?.sendTouch(0, 0, false), []);

  const getBacklight = useCallback(() => statusRef.current.backlight, []);
  const refreshSaved = useCallback(() => { void listSavedDevices().then(setSavedDevices); }, []);
  const saveState = useCallback(async () => {
    // Flush any live Remote Link session (without disconnecting it) so an
    // explicit Save captures a cleanly-acked, adoptable session.
    await quiesceActiveSessions();
    await clientRef.current?.saveState();
    refreshSaved();
  }, [refreshSaved]);

  // Save-state bundles: IDB-only, so they run on the main thread against the same
  // IndexedDB the worker writes to (shared via collectStatesBundle/applyStatesBundle).
  const exportAllStates = useCallback(async (): Promise<void> => {
    // Snapshot the running device first so the export includes the latest
    // session; quiesce keeps any active link adoptable in the bundle.
    await quiesceActiveSessions();
    await clientRef.current?.saveState();
    refreshSaved();
    const profileMap = new Map(profiles.map(p => [p.id, p.displayName] as const));
    const blob = await collectStatesBundle(profileMap);
    if (!blob) return;
    const ts = new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19);
    triggerDownload(blob, `psion-states-${ts}.psionstate`);
  }, [profiles]);
  const importStates = useCallback(async (file: File) => {
    const result = await applyStatesBundle(file);
    refreshSaved();
    return result;
  }, [refreshSaved]);
  const powerOff = useCallback(() => { void clientRef.current?.pause(); }, []);
  const powerOn = useCallback(() => { void clientRef.current?.resume(); }, []);
  const resetDevice = useCallback(() => {
    // Cold boot (restore=false): Reset must boot the device fresh from ROM, not
    // revert to the last saved session. The CF is kept (re-attached on load), so
    // it's "as if loaded from fresh" with the card still inserted.
    const l = lastLoadRef.current; if (l) void doLoad(l.deviceId, l.romUrl, false);
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  const attachCard = useCallback(async (bytes: Uint8Array) => {
    const ok = (await clientRef.current?.attachCard(bytes)) ?? false;
    if (ok) setCardAttached(true);
    return ok;
  }, []);
  const updateCardInPlace = useCallback(async (bytes: Uint8Array) => {
    if (!bytes || bytes.byteLength === 0) return false;
    const ok = (await clientRef.current?.updateCard(bytes)) ?? false;
    if (ok) setCardAttached(true);
    return ok;
  }, []);
  const detachCard = useCallback(async () => { await clientRef.current?.detachCard(); setCardAttached(false); }, []);
  // Async in worker mode (the image lives in the worker's heap). The CF panel's
  // poller/refresh Promise.resolve()s the result — a null stub here meant the
  // files list never reflected on-device writes (.sis deleted on install, new
  // System folder, etc.) in worker mode.
  const getCardBytes = useCallback(() => clientRef.current?.getCardBytes() ?? Promise.resolve(null), []);

  // Mount the bootloader-OS image (5mxPro / netBook): build the FAT16 card on the
  // main thread (download + image build need no `mod`) and attach it via the
  // existing CF RPC — the worker's attachCFImage runs the same bootloader→OS
  // handoff (netBookLoadOsFromCard) the main-thread path does.
  const attachOsCard = useCallback(async (variant?: string): Promise<boolean> => {
    const spec = osCardSpec(currentDeviceId, variant);
    if (!spec || !clientRef.current) return false;
    setOsDownloading(true);
    try {
      const resp = await fetch(spec.url);
      if (!resp.ok) return false;
      const card = buildOsCardImage(spec, new Uint8Array(await resp.arrayBuffer()));
      if (!card) return false;
      const ok = await clientRef.current.attachCard(card);
      if (ok) setCardAttached(true);
      return ok;
    } catch { return false; }
    finally { setOsDownloading(false); }
  }, [currentDeviceId]);

  // ── Speaker audio ──
  const ensureAudioEngine = useCallback(async (): Promise<AudioEngine | null> => {
    if (audioEngineRef.current) return audioEngineRef.current;
    if (!deviceInfo?.hasAudio) return null;
    try {
      const shim = createWorkerAudioShim(
        (spk, mic) => clientRef.current?.setHostAudio(spk, mic),
        (samples) => clientRef.current?.sendMic(samples),
      );
      audioShimRef.current = shim;
      const engine = await createAudioEngine(shim.mod, deviceInfo.audioSampleRate || 8000);
      audioEngineRef.current = engine;
      setAudioError(null);
      return engine;
    } catch (err) { setAudioError(String(err)); return null; }
  }, [deviceInfo]);

  const toggleSpeaker = useCallback(async () => {
    const next = !speakerEnabled;
    setSpeakerEnabled(next);
    const engine = await ensureAudioEngine();
    if (!engine) return;
    try { await engine.setSpeaker(next); }       // → shim.setHostAudioEnabled → worker drains
    catch (err) { setAudioError(String(err)); }
  }, [speakerEnabled, ensureAudioEngine]);

  const toggleMic = useCallback(async () => {
    const next = !micEnabled;
    setMicEnabled(next);
    const engine = await ensureAudioEngine();
    if (!engine) { if (next) setMicEnabled(false); return; }
    try { await engine.setMic(next); }           // getUserMedia + MicProcessor (main thread);
    catch (err) {                                // captured samples → shim → worker writeAudioInput
      setAudioError(String(err));
      if (next) setMicEnabled(false);            // permission denied / no device → reset the toggle
    }
  }, [micEnabled, ensureAudioEngine]);

  // Tear the engine down with the worker.
  useEffect(() => () => {
    audioEngineRef.current?.destroy();
    audioEngineRef.current = null;
    audioShimRef.current = null;
  }, []);

  // ── SSD / Datapak (slot 0/1) — same upload→attach RPC pattern as CF.
  //    Sync byte-getters can't reach the worker, so they return null (matching
  //    getCardBytes); export of pack images is a later save-state phase.
  const setSlot = (set: React.Dispatch<React.SetStateAction<boolean[]>>, slot: number, v: boolean) =>
    set(prev => { const next = [...prev]; next[slot] = v; return next; });

  const attachSSD = useCallback(async (slot: number, bytes: Uint8Array,
                                       kind?: 'ram' | 'flash'): Promise<boolean> => {
    if (slot < 0 || slot > 3 || bytes.byteLength === 0) return false;
    // Pack type code mirrors PsionSSD::Type — 'flash' must attach as
    // 3 (write-protected) or EPOC16 reports the pack unformatted.
    const ssdType = kind === undefined ? undefined : (kind === 'flash' ? 3 : 1);
    const ok = (await clientRef.current?.attachSSD(slot, bytes, ssdType)) ?? false;
    if (ok) setSlot(setSsdAttached, slot, true);
    return ok;
  }, []);
  const detachSSD = useCallback(async (slot: number): Promise<void> => {
    await clientRef.current?.detachSSD(slot);
    setSlot(setSsdAttached, slot, false);
  }, []);

  const attachDatapak = useCallback(async (slot: number, bytes: Uint8Array): Promise<boolean> => {
    if (slot < 0 || slot > 1 || bytes.byteLength === 0) return false;
    const r = await clientRef.current?.attachDatapak(slot, bytes);
    if (r?.ok) { datapakKindRef.current[slot] = r.kind; setSlot(setDatapakAttached, slot, true); }
    return r?.ok ?? false;
  }, []);
  const detachDatapak = useCallback(async (slot: number): Promise<void> => {
    await clientRef.current?.detachDatapak(slot);
    datapakKindRef.current[slot] = 0;
    setSlot(setDatapakAttached, slot, false);
  }, []);
  const getDatapakKind = useCallback((slot: number): number => datapakKindRef.current[slot] ?? 0, []);

  // ── Serial host bridge (sync interface; async attach is optimistic) ──
  const serialAttachHost = useCallback((uart: number): boolean => {
    if (serialAttachedRef.current[uart]) return true;
    serialAttachedRef.current[uart] = true;          // optimistic; failure surfaces as a connect timeout
    serialRxRef.current[uart] = [];
    clientRef.current?.serialAttach(uart).then(ok => { if (!ok) serialAttachedRef.current[uart] = false; });
    return true;
  }, []);
  const serialDetachHost = useCallback((uart: number): boolean => {
    serialAttachedRef.current[uart] = false;
    serialRxRef.current[uart] = [];
    clientRef.current?.serialDetach(uart);
    return true;
  }, []);
  const serialIsAttached = useCallback((uart: number): boolean => !!serialAttachedRef.current[uart], []);
  const serialReadBytes = useCallback((uart: number): Uint8Array => {
    const chunks = serialRxRef.current[uart];
    if (!chunks || chunks.length === 0) return new Uint8Array(0);
    serialRxRef.current[uart] = [];
    if (chunks.length === 1) return chunks[0];
    let len = 0; for (const c of chunks) len += c.length;
    const out = new Uint8Array(len); let off = 0;
    for (const c of chunks) { out.set(c, off); off += c.length; }
    return out;
  }, []);
  const serialWriteBytes = useCallback((uart: number, data: Uint8Array): number => {
    if (data.length === 0 || !serialAttachedRef.current[uart]) return 0;
    clientRef.current?.serialWrite(uart, data);
    return data.length;                              // optimistic: the worker performs the write
  }, []);

  const revertToSaved = useCallback(async (): Promise<boolean> => {
    return (await clientRef.current?.revertToSaved()) ?? false;
  }, []);

  // ── Stubs for features not yet ported to the worker (Phase 3-4) ──
  const noVoid = useCallback(async () => {}, []);

  return {
    state, loadProgress, loadStatus, error, deviceInfo, canvasRef, deviceModeRef,
    logs, paused, cardAttached, osCardConsumed: false,
    osDownloading, osDownloadProgress: null,
    speakerEnabled, micEnabled, audioError,
    currentDeviceId, profiles,
    loadDevice, handleKeyDown, handleKeyUp, handleInput, handlePasteText, pasteFromClipboard,
    sendEpocKey, handlePointerDown, handlePointerMove, handlePointerUp,
    pressEpocKey, pressEpocChord, getBacklight, saveState,
    clearLogs: () => setLogs([]),
    setLoggingEnabled: (on: boolean) => { loggingOnRef.current = on; clientRef.current?.setLoggingEnabled(on); },
    powerOff, powerOn, resetDevice,
    clearSession: noVoid,
    attachCard, updateCardInPlace, detachCard,
    attachOsCard, getCardBytes,
    getRamSnapshot: () => clientRef.current?.getRamSnapshot() ?? Promise.resolve(null),
    ssdAttached, attachSSD, detachSSD,
    getSSDBytes: (slot: number) => clientRef.current?.getSSDBytes(slot) ?? null,
    datapakAttached, attachDatapak, detachDatapak,
    getDatapakBytes: () => null, getDatapakKind,
    toggleSpeaker, toggleMic,
    savedDevices, exportAllStates, importStates,
    revertToSaved,
    serialAttachHost, serialDetachHost, serialIsAttached,
    serialReadBytes, serialWriteBytes,
  };
}
