// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useState, useEffect, useRef, useCallback } from 'react';
import type { EmulatorControls } from '../hooks/useEmulator';
import { PlpClient, type ClientState } from '../lib/plp/client-spec';
import type { DirEntry } from '../lib/plp/rfsv32-spec';
import type { Pdu } from '../lib/plp/link';

interface Props {
  controls: EmulatorControls;
  // Which SoC UART index the host bridge should attach to. The cable
  // port differs between device families: Windermere (Revo / 5mx /
  // 5mxpro / mc218) wires it to UART2; SA-1100 netBook wires it to
  // UART3 (UART2 there is IrDA); SIBO2 devices (3c / 3mx / Siena) use
  // 0 = the Condor cable path. Caller resolves the mapping from the
  // device profile.
  uartIndex: number;
  // File-services dialect the device's ROM speaks: 'rfsv32' (EPOC32,
  // default) or 'rfsv16' (EPOC16 SIBO). From DeviceProfile.linkProtocol.
  protocol?: 'rfsv32' | 'rfsv16';
  // Dialog title flavour (the SIBO IR path reuses this dialog as
  // "Infrared Remote Link").
  title?: string;
  // Req_Con_Pdu seq flavour for the link handshake (see LinkConfig.conSeq):
  // omit/4 for R5 Windermere & SA-1100; 2 for the ER3/ER4 CL-PS711x devices
  // (Series 5, Osaris) whose ROMs silently drop the 0x24 form.
  conSeq?: number;
  // The device this dialog is bound to. The dialog stays mounted across
  // a device switch (the panel can change devices without closing it),
  // so a change here means the client is now bound to the wrong device —
  // the dialog parks it and resets to a clean disconnected state.
  deviceId: string | null;
  onClose: () => void;
}

// PsiWin Remote Link dialog. Connect attaches the host to the
// caller-supplied UART, runs the data-link handshake, exchanges NCP
// Information, and opens the SYS$RFSV.* channel; once connected the
// user browses drives and folders, downloads or deletes files, and
// uploads new ones via the file picker.
//
// All supported devices' kernels (Windermere Revo / 5mx / 5mxpro /
// mc218 and SA-1100 netBook) autostart the same Req_Req_Pdu /
// RFSV-32 handshake at 115200 baud, so a fresh boot is fine without
// touching anything on the device. (Confirmed empirically via the
// harness — see scripts/test-remote-link.sh.)
//
// The wire-format PlpClient lives in frontend/src/lib/plp/client-spec.ts
// and is validated end-to-end against an in-process mock in
// frontend/src/lib/__tests__/plp.client-spec.test.mts. The real
// device's response to OPEN_DIR + READ_DIR has also been captured
// from the harness and parsed correctly by the same code (see the
// rfsv32-spec test for the regression vector).
//
// Diagnostics surfaces raw RX/TX bytes plus the parsed PDU stream so
// any wire-format mismatch against the real device is immediately
// visible.

const CONNECT_TIMEOUT_MS = 15_000;

function hexLine(bytes: Uint8Array, offset: number): string {
  const hex: string[] = [];
  const ascii: string[] = [];
  const end = Math.min(offset + 16, bytes.length);
  for (let i = offset; i < end; i++) {
    const b = bytes[i];
    hex.push(b.toString(16).padStart(2, '0'));
    ascii.push(b >= 0x20 && b < 0x7f ? String.fromCharCode(b) : '.');
  }
  while (hex.length < 16) hex.push('  ');
  return `${offset.toString(16).padStart(6, '0')}  ${hex.join(' ')}  |${ascii.join('')}|`;
}
function formatHex(bytes: Uint8Array): string {
  if (bytes.length === 0) return '';
  const lines: string[] = [];
  for (let i = 0; i < bytes.length; i += 16) lines.push(hexLine(bytes, i));
  return lines.join('\n');
}

function formatBytes(n: number): string {
  if (n < 1024)        return `${n} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / (1024 * 1024)).toFixed(1)} MB`;
}

// Clamp a host filename to the EPOC16 8.3 character set (A-Z 0-9 _ $),
// uppercased — the SIBO file server refuses anything longer or with
// spaces/punctuation with status -38. Same policy as the Infrared
// beam's siboDestPath.
function sibo83Name(hostName: string): string {
  const dot = hostName.lastIndexOf('.');
  const base = (dot > 0 ? hostName.slice(0, dot) : hostName)
    .toUpperCase().replace(/[^A-Z0-9_$]/g, '').slice(0, 8) || 'UPLOAD';
  const ext = (dot > 0 ? hostName.slice(dot + 1) : '')
    .toUpperCase().replace(/[^A-Z0-9_$]/g, '').slice(0, 3);
  return ext ? `${base}.${ext}` : base;
}

// Join / strip directory-path segments. EPOC paths use backslashes.
function joinPath(base: string, leaf: string): string {
  const sep = base.endsWith('\\') ? '' : '\\';
  return base + sep + leaf + '\\';
}
function parentPath(p: string): string {
  if (!p.endsWith('\\')) return p;
  const trimmed = p.slice(0, -1);
  const idx = trimmed.lastIndexOf('\\');
  if (idx < 0) return p;
  return trimmed.slice(0, idx + 1);
}

// One row in the parsed-PDU diagnostics panel.
interface PduRow {
  index: number;
  direction: 'in' | 'out';
  cont: number;
  seq: number;
  dataLen: number;
  dataPreview: string;
}

function pduLabel(cont: number): string {
  switch (cont) {
    case 0: return 'Ack';
    case 1: return 'Disc';
    case 2: return 'Req';     // Req_Pdu / Req_Req_Pdu / Req_Con_Pdu (Revo R5 quirk)
    case 3: return 'Data';
    case 4: return 'ReqCon';
    default: return `t${cont.toString(16)}`;
  }
}

// One line in the parsed-PDU panel / copied report. Shared so the on-screen
// log and the clipboard export never drift apart.
function formatPduRow(r: PduRow): string {
  const arrow = r.direction === 'in' ? '←' : '→';
  const cs = `0x${((r.cont << 4) | (r.seq & 0xF)).toString(16).padStart(2, '0')}`;
  const seqStr = r.seq.toString().padStart(2, ' ');
  const lenStr = r.dataLen.toString().padStart(4, ' ');
  return `#${r.index.toString().padStart(3, '0')}  ${arrow} ${cs} ${pduLabel(r.cont)}/${seqStr} (${lenStr}B) ${r.dataPreview}`;
}

const STATE_LABEL: Record<ClientState, string> = {
  idle: 'Idle',
  attaching: 'Attaching…',
  'link-up': 'Link up, exchanging NCP info…',
  'rfsv-opening': 'Opening SYS$RFSV…',
  connected: 'Connected',
  failed: 'Failed',
};

// Clipboard write with a legacy fallback for non-secure contexts where
// navigator.clipboard is unavailable.
function copyText(text: string): Promise<void> {
  if (navigator.clipboard?.writeText) return navigator.clipboard.writeText(text);
  return new Promise((resolve, reject) => {
    try {
      const ta = document.createElement('textarea');
      ta.value = text;
      ta.style.position = 'fixed';
      ta.style.opacity = '0';
      document.body.appendChild(ta);
      ta.select();
      document.execCommand('copy');
      document.body.removeChild(ta);
      resolve();
    } catch (e) { reject(e); }
  });
}

export default function RemoteLinkDialog({ controls, uartIndex, protocol, title, conSeq, deviceId, onClose }: Props) {
  // ── Connection state ─────────────────────────────────────────────
  const [clientState, setClientState] = useState<ClientState>('idle');
  const [connectError, setConnectError] = useState<string>('');
  const clientRef = useRef<PlpClient | null>(null);
  // R5 active-handshake devices (Windermere / SA-1100) use the
  // parked-session flow shared with the app-delivery path: the cable
  // stays attached and the device-side session is left alive between
  // dialog sessions, to be resumed via the adoption probe — their link
  // server never survives a Disc_Pdu or a cable unplug (it goes dormant
  // until Remote Link is toggled on the device). SIBO and the passive
  // CL-PS711x devices keep the legacy teardown + per-connect re-plug.
  const parkable = (protocol ?? 'rfsv32') === 'rfsv32' && conSeq !== 2;

  // ── Browser state ────────────────────────────────────────────────
  const [drives, setDrives] = useState<string[] | null>(null);
  const [currentPath, setCurrentPath] = useState<string>('');
  const [listing, setListing] = useState<DirEntry[] | null>(null);
  const [listBusy, setListBusy] = useState<boolean>(false);

  // ── Transfer state ───────────────────────────────────────────────
  const [transferBusy, setTransferBusy] = useState<boolean>(false);
  const [transferProgress, setTransferProgress] = useState<number>(0);
  const [transferStatus, setTransferStatus] = useState<string>('');
  const uploadInputRef = useRef<HTMLInputElement>(null);

  // ── Diagnostics state (only inside the collapsed panel) ─────────
  const [showDiag, setShowDiag] = useState<boolean>(false);
  const [rxLog, setRxLog] = useState<Uint8Array>(new Uint8Array(0));
  const [txLog, setTxLog] = useState<Uint8Array>(new Uint8Array(0));
  const [pdus, setPdus] = useState<PduRow[]>([]);
  const [diagCopied, setDiagCopied] = useState<boolean>(false);
  const rxBufRef = useRef<Uint8Array>(new Uint8Array(0));
  const txBufRef = useRef<Uint8Array>(new Uint8Array(0));
  const pduCounterRef = useRef<number>(0);

  // Assemble every diagnostic surface — connection state, last error,
  // drives/path, the full parsed-PDU stream, and the complete raw RX/TX
  // hex dumps — into one plain-text blob and copy it to the clipboard.
  // This is what to paste when reporting a Remote Link problem.
  const handleCopyDiag = useCallback(() => {
    const rx = rxBufRef.current;
    const tx = txBufRef.current;
    const lines = [
      '=== Psion Remote Link diagnostics ===',
      `time:   ${new Date().toISOString()}`,
      `uart:   UART${uartIndex}`,
      `state:  ${STATE_LABEL[clientState]} (${clientState})`,
      `error:  ${connectError || '(none)'}`,
      `drives: ${drives === null ? '(not listed)' : drives.length === 0 ? '(none reported)' : drives.join(' ')}`,
      `path:   ${currentPath || '(none)'}`,
      `agent:  ${typeof navigator !== 'undefined' ? navigator.userAgent : '(unknown)'}`,
      '',
      `--- Parsed PDUs (${pdus.length}) ---`,
      pdus.length === 0 ? '(none)' : pdus.map(formatPduRow).join('\n'),
      '',
      `--- Raw bytes from device (${rx.length}) ---`,
      rx.length === 0 ? '(none)' : formatHex(rx),
      '',
      `--- Raw bytes to device (${tx.length}) ---`,
      tx.length === 0 ? '(none)' : formatHex(tx),
    ];
    void copyText(lines.join('\n')).then(
      () => { setDiagCopied(true); setTimeout(() => setDiagCopied(false), 1500); },
    ).catch(() => { /* clipboard blocked — nothing we can do */ });
  }, [uartIndex, clientState, connectError, drives, currentPath, pdus]);

  // Tear down the polling client on unmount. Parkable devices keep the
  // session alive (releaseQuiesced — no Disc_Pdu, cable stays) so the
  // next connect resumes it; the others send the Disc_Pdu their ROMs
  // expect. (Safe in StrictMode's spurious dev-mode cleanup cycles:
  // disconnect() only emits a Disc_Pdu when the link is active, and
  // releaseQuiesced on an idle client is a no-op stop.)
  useEffect(() => () => {
    const client = clientRef.current;
    clientRef.current = null;
    if (!client) return;
    if (parkable) void client.releaseQuiesced();
    else client.disconnect();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // The dialog survives a device switch (the panel can change devices
  // without closing it), so a deviceId change means clientRef is now
  // bound to the wrong device. The switch path already quiesced + parked
  // the outgoing session (hooks' quiesceActiveSessions), so just drop the
  // stale client and reset to a clean disconnected state — returning to
  // the original device then offers a fresh Connect that re-adopts the
  // parked session.
  const prevDeviceIdRef = useRef(deviceId);
  useEffect(() => {
    if (prevDeviceIdRef.current === deviceId) return;
    prevDeviceIdRef.current = deviceId;
    if (clientRef.current) { clientRef.current.release(); clientRef.current = null; }
    setClientState('idle');
    setDrives(null);
    setCurrentPath('');
    setListing(null);
    setConnectError('');
    setTransferStatus('');
  }, [deviceId]);

  // ── Connect / Retry / Disconnect / Close ─────────────────────────
  const handleConnect = useCallback(async () => {
    setConnectError('');
    setTransferStatus('');

    if (clientRef.current) { clientRef.current.stop(); clientRef.current = null; }

    // Reset derived state.
    rxBufRef.current = new Uint8Array(0); setRxLog(rxBufRef.current);
    txBufRef.current = new Uint8Array(0); setTxLog(txBufRef.current);
    pduCounterRef.current = 0; setPdus([]);

    const attachError = `Could not attach host to UART${uartIndex}. The emulator may not be running.`;
    if (parkable) {
      // NEVER detach a parkable (R5 Windermere / SA-1100) device: a
      // detach is a cable unplug, which puts the link server terminally
      // to sleep (only a Remote Link off/on toggle on the device revives
      // it). Just ensure the bridge is attached — a restored device may
      // already show attached, with its session parked. connect()'s
      // adoption probe resumes any parked session; on a fresh device with
      // no session the device's own cable-plug Req_Req burst (from the
      // attach below, or already in flight) completes the handshake.
      if (!controls.serialIsAttached(uartIndex) && !controls.serialAttachHost(uartIndex)) {
        setConnectError(attachError);
        return;
      }
    } else {
      // Legacy CL-PS711x (Series 5 / Osaris) and SIBO ROMs: a fresh cable
      // re-plug on every connect. After a failed attempt the bridge
      // deliberately stays attached (so Diagnostics stay live), but a
      // Connect on a still-attached bridge never re-fires the cable-plug
      // (modem-status) IRQ. Worse, some ROMs send their burst exactly ONCE
      // per attach and never answer a host-initiated Req_Req (Osaris;
      // harness-verified), so a missed burst can't recover without a
      // re-plug. Detach + attach gives the device a clean unplug/replug
      // edge: the kernel resets its link server and re-sends the burst.
      if (controls.serialIsAttached(uartIndex)) controls.serialDetachHost(uartIndex);
      if (!controls.serialAttachHost(uartIndex)) {
        setConnectError(attachError);
        return;
      }
    }

    const appendRx = (chunk: Uint8Array) => {
      const prev = rxBufRef.current;
      const cap = 64 * 1024;
      let combined: Uint8Array;
      if (prev.length + chunk.length <= cap) {
        combined = new Uint8Array(prev.length + chunk.length);
        combined.set(prev, 0); combined.set(chunk, prev.length);
      } else {
        const keep = cap - chunk.length;
        combined = new Uint8Array(cap);
        combined.set(prev.subarray(prev.length - keep), 0);
        combined.set(chunk, keep);
      }
      rxBufRef.current = combined;
      setRxLog(combined);
    };
    const appendTx = (chunk: Uint8Array) => {
      const prev = txBufRef.current;
      const cap = 64 * 1024;
      let combined: Uint8Array;
      if (prev.length + chunk.length <= cap) {
        combined = new Uint8Array(prev.length + chunk.length);
        combined.set(prev, 0); combined.set(chunk, prev.length);
      } else {
        const keep = cap - chunk.length;
        combined = new Uint8Array(cap);
        combined.set(prev.subarray(prev.length - keep), 0);
        combined.set(chunk, keep);
      }
      txBufRef.current = combined;
      setTxLog(combined);
    };
    const appendPdu = (direction: 'in' | 'out', pdu: Pdu) => {
      const preview = Array.from(pdu.data.subarray(0, 16))
        .map(b => b.toString(16).padStart(2, '0')).join(' ');
      const row: PduRow = {
        index: pduCounterRef.current++,
        direction,
        cont: pdu.cont,
        seq: pdu.seq,
        dataLen: pdu.data.length,
        dataPreview: preview + (pdu.data.length > 16 ? ` … (+${pdu.data.length - 16})` : ''),
      };
      setPdus(prev => {
        const next = [...prev, row];
        return next.length > 128 ? next.slice(next.length - 128) : next;
      });
    };

    const client = new PlpClient(
      () => controls.serialReadBytes(uartIndex),
      data => controls.serialWriteBytes(uartIndex, data),
      {
        pollHz: 200,
        protocol: protocol ?? 'rfsv32',
        conSeq,
        // SA-1100 (Series 7 / netBook, cable on UART3) has no host RX
        // FIFO cap, so it takes a 2 KB chunk where Windermere is held to
        // 1 KB by its 4 KB FIFO. Halving the round-trip count roughly
        // halves transfer time on these machines, where each round-trip
        // pays a large fixed EKA1 F32 wake.
        chunkSize: uartIndex === 3 ? 2048 : undefined,
        onState: setClientState,
        onRawRx: appendRx,
        onRawTx: appendTx,
        onPduIn: pdu => appendPdu('in', pdu),
        onPduOut: pdu => appendPdu('out', pdu),
      },
    );
    client.start();
    clientRef.current = client;

    try {
      await client.connect(CONNECT_TIMEOUT_MS);
    } catch (e) {
      const base = e instanceof Error ? e.message : String(e);
      // A parkable R5 device (Windermere / SA-1100) that sends back
      // nothing at all has a link server that's gone dormant — typically
      // after a previous session was lost un-gracefully (the browser tab
      // was backgrounded or closed mid-transfer). The R5 ROM never
      // re-arms that state on its own; nothing host-side (re-handshake,
      // cable re-plug, Disc) revives it. The only recovery is toggling
      // Remote Link off then on ON THE DEVICE. Surface that instead of a
      // bare "link handshake timed out".
      const deviceSilent = rxBufRef.current.length === 0;
      setConnectError(
        parkable && deviceSilent
          ? `${base}. The device isn't responding on the cable — its Remote Link ` +
            `server has likely gone dormant after an interrupted session. ` +
            `On the device, turn Remote Link OFF and then ON again ` +
            `(System screen → Tools/Control panel → Remote link, or the ` +
            `Communications/Link setting), then reconnect.`
          : base,
      );
      return;
    }

    try {
      const ds = await client.listDrives();
      setDrives(ds);
      if (ds.length > 0) {
        const root = `${ds[0]}\\`;
        setCurrentPath(root);
        await loadListing(client, root);
      }
    } catch (e) {
      setConnectError(`DRIVE_LIST failed: ${e instanceof Error ? e.message : String(e)}`);
    }
  }, [controls, uartIndex, conSeq, protocol, parkable]);

  // Parkable devices: stop the host side but leave the session alive
  // and the cable attached (releaseQuiesced) for the next connect to
  // resume. Other devices get the legacy Disc_Pdu + detach their ROMs
  // recover from via the per-connect re-plug.
  const teardown = useCallback(() => {
    const client = clientRef.current;
    clientRef.current = null;
    if (parkable) {
      if (client) void client.releaseQuiesced();
    } else {
      client?.disconnect();
      if (controls.serialIsAttached(uartIndex)) controls.serialDetachHost(uartIndex);
    }
  }, [controls, uartIndex, parkable]);

  const handleDisconnect = useCallback(() => {
    teardown();
    setClientState('idle');
    setDrives(null);
    setCurrentPath('');
    setListing(null);
    setConnectError('');
    setTransferStatus('');
  }, [teardown]);

  const handleClose = useCallback(() => {
    teardown();
    onClose();
  }, [teardown, onClose]);

  // ── Browsing ─────────────────────────────────────────────────────
  const loadListing = useCallback(async (client: PlpClient, path: string) => {
    setListBusy(true);
    try {
      const entries = await client.listDirectory(path);
      setListing(entries);
    } catch (e) {
      setListing(null);
      setConnectError(`Directory read failed: ${e instanceof Error ? e.message : String(e)}`);
    } finally {
      setListBusy(false);
    }
  }, []);

  const handleSelectDrive = useCallback((drive: string) => {
    const client = clientRef.current;
    if (!client) return;
    const root = `${drive}\\`;
    setCurrentPath(root);
    setListing(null);
    void loadListing(client, root);
  }, [loadListing]);

  const handleOpenFolder = useCallback((entry: DirEntry) => {
    const client = clientRef.current;
    if (!client) return;
    const next = joinPath(currentPath, entry.longName);
    setCurrentPath(next);
    setListing(null);
    void loadListing(client, next);
  }, [currentPath, loadListing]);

  const handleGoUp = useCallback(() => {
    const client = clientRef.current;
    if (!client) return;
    const next = parentPath(currentPath);
    if (next === currentPath) return;
    setCurrentPath(next);
    setListing(null);
    void loadListing(client, next);
  }, [currentPath, loadListing]);

  const handleRefresh = useCallback(() => {
    const client = clientRef.current;
    if (!client) return;
    void loadListing(client, currentPath);
  }, [currentPath, loadListing]);

  // ── Transfers ────────────────────────────────────────────────────
  const handleDownload = useCallback(async (entry: DirEntry) => {
    const client = clientRef.current;
    if (!client) return;
    const path = currentPath + entry.longName;
    setTransferStatus(''); setTransferProgress(0); setTransferBusy(true);
    try {
      const bytes = await client.downloadFile(path, n => setTransferProgress(n));
      const blob = new Blob([bytes as BlobPart], { type: 'application/octet-stream' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url; a.download = entry.longName;
      document.body.appendChild(a); a.click(); document.body.removeChild(a);
      URL.revokeObjectURL(url);
      setTransferStatus(`Downloaded ${entry.longName} (${formatBytes(bytes.length)})`);
    } catch (e) {
      setTransferStatus(`Download failed: ${e instanceof Error ? e.message : String(e)}`);
    } finally {
      setTransferBusy(false);
    }
  }, [currentPath]);

  const handleDelete = useCallback(async (entry: DirEntry) => {
    const client = clientRef.current;
    if (!client) return;
    const path = currentPath + entry.longName;
    if (!window.confirm(`Delete ${path}?`)) return;
    setTransferStatus(''); setTransferBusy(true);
    try {
      await client.deleteFile(path);
      setTransferStatus(`Deleted ${entry.longName}`);
      await loadListing(client, currentPath);
    } catch (e) {
      setTransferStatus(`Delete failed: ${e instanceof Error ? e.message : String(e)}`);
    } finally {
      setTransferBusy(false);
    }
  }, [currentPath, loadListing]);

  const handleUploadClick = useCallback(() => uploadInputRef.current?.click(), []);

  const handleUploadFile = useCallback(async (file: File) => {
    const client = clientRef.current;
    if (!client) return;
    // EPOC16 filesystems are strictly 8.3 with a limited character set;
    // a browser-typical name ("My Notes File.txt") makes the device
    // refuse the FOPEN with -38. Clamp the basename for SIBO targets
    // (mirrors what the Infrared beam path does) and say so in the
    // status line when the name changed.
    const deviceName = protocol === 'rfsv16' ? sibo83Name(file.name) : file.name;
    const target = currentPath + deviceName;
    setTransferStatus(''); setTransferProgress(0); setTransferBusy(true);
    try {
      const buf = new Uint8Array(await file.arrayBuffer());
      await client.uploadFile(target, buf, n => setTransferProgress(n));
      setTransferStatus(deviceName === file.name
        ? `Uploaded ${file.name} (${formatBytes(buf.length)})`
        : `Uploaded ${file.name} as ${deviceName} (${formatBytes(buf.length)})`);
      await loadListing(client, currentPath);
    } catch (e) {
      setTransferStatus(`Upload failed: ${e instanceof Error ? e.message : String(e)}`);
    } finally {
      setTransferBusy(false);
    }
  }, [currentPath, loadListing, protocol]);

  // ── Styling helpers ──────────────────────────────────────────────
  const btn = 'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap select-none cursor-pointer bg-psion-mid border border-psion-accent/50 text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors disabled:opacity-50 disabled:cursor-not-allowed';
  const btnPrimary = 'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap select-none cursor-pointer bg-psion-highlight border border-psion-accent/50 text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors disabled:opacity-50 disabled:cursor-not-allowed';
  const btnActive = btn + ' !bg-psion-highlight';
  const btnGhost = 'px-2 py-1 rounded text-xs font-mono whitespace-nowrap select-none cursor-pointer text-psion-charcoal/80 hover:bg-psion-accent/20 transition-colors disabled:opacity-50 disabled:cursor-not-allowed';

  // ── Render ───────────────────────────────────────────────────────
  const isConnected = clientState === 'connected';
  const isFailed = clientState === 'failed' || connectError !== '';
  const isWorking = clientState === 'attaching' || clientState === 'link-up' || clientState === 'rfsv-opening';

  return (
    <div className="w-full max-w-3xl bg-psion-dark border border-psion-accent/40 rounded-lg overflow-hidden flex-shrink-0 mx-4 shadow-sm flex flex-col">
        <div className="flex items-center justify-between px-4 py-3 border-b border-psion-accent/40 bg-psion-mid">
          <h2 className="text-sm font-mono font-semibold text-psion-charcoal">{title ?? 'Remote Link'}</h2>
          <button onClick={handleClose} className={btn}>Close</button>
        </div>

        {/* Top action bar */}
        <div className="flex flex-wrap gap-2 items-center px-4 py-3 border-b border-psion-accent/30 bg-psion-mid/60">
          {clientState === 'idle' && (
            <button onClick={() => void handleConnect()} className={btnPrimary}>Connect</button>
          )}
          {isWorking && (
            <button disabled className={btnPrimary}>{STATE_LABEL[clientState]}</button>
          )}
          {isFailed && !isConnected && (
            <>
              <button onClick={() => void handleConnect()} className={btnPrimary}>Retry</button>
              <button onClick={handleDisconnect} className={btn}>Disconnect</button>
              <span className="text-xs font-mono text-amber-700">● Bridge still attached — Diagnostics live</span>
            </>
          )}
          {isConnected && (
            <>
              <span className="text-xs font-mono text-green-700">● Connected</span>
              <button onClick={handleDisconnect} className={btn}>Disconnect</button>
            </>
          )}
          <span className="flex-grow" />
          {!isConnected && !isWorking && (
            <span className="text-xs font-mono text-gray-500 max-w-md text-right">
              {protocol === 'rfsv16'
                // EPOC16: enable the link from the System screen with
                // Psion+L. The `Use` option is labelled per machine, and
                // the default baud (57600 on the 3c family) is fine — the
                // emulated bridge accepts any rate.
                ? `Turn the link on via the System screen: Psion+L → ${
                    controls.currentDeviceId === 'siena' ? 'Use: Serial cable'
                    : controls.currentDeviceId === 'workaboutmx' ? 'Remote link: On'
                    : 'Use: Link cable'} → Enter (the default baud rate is fine)`
                : 'Make sure the device’s Remote Link is set to "Cable" at 115200 baud (both defaults)'}
            </span>
          )}
        </div>

        {/* Connect error */}
        {connectError && (
          <div className="px-4 py-2 border-b border-psion-accent/30 bg-red-50">
            <pre className="text-[11px] font-mono text-red-700 whitespace-pre-wrap">{connectError}</pre>
          </div>
        )}

        {/* Browser */}
        {isConnected && (
          <>
            <div className="flex flex-wrap gap-2 items-center px-4 py-2 border-b border-psion-accent/30 bg-psion-mid/40">
              <span className="text-xs font-mono text-gray-500 mr-2">Drives:</span>
              {drives === null ? (
                <span className="text-xs font-mono text-gray-500">(loading…)</span>
              ) : drives.length === 0 ? (
                <span className="text-xs font-mono text-gray-500">(none reported)</span>
              ) : (
                drives.map(d => (
                  <button key={d} onClick={() => handleSelectDrive(d)}
                          disabled={listBusy || transferBusy}
                          className={currentPath.startsWith(d) ? btnActive : btn}>
                    {d}
                  </button>
                ))
              )}
            </div>

            <div className="flex flex-wrap gap-2 items-center px-4 py-2 border-b border-psion-accent/30 bg-psion-mid/40">
              <button onClick={handleGoUp}
                      disabled={!currentPath || parentPath(currentPath) === currentPath || listBusy || transferBusy}
                      className={btnGhost} title="Go to parent directory">↑</button>
              <span className="text-xs font-mono text-psion-charcoal flex-grow truncate">
                {currentPath || '(no drive selected)'}
              </span>
              <button onClick={handleRefresh} disabled={listBusy || transferBusy || !currentPath} className={btn}>Refresh</button>
              <button onClick={handleUploadClick} disabled={listBusy || transferBusy || !currentPath} className={btn}>Upload file…</button>
              <input ref={uploadInputRef} type="file" style={{ display: 'none' }}
                     onChange={e => {
                       const f = e.target.files?.[0];
                       if (f) void handleUploadFile(f);
                       e.target.value = '';
                     }} />
            </div>

            {(transferBusy || transferStatus) && (
              <div className="px-4 py-2 border-b border-psion-accent/30 bg-psion-mid/30">
                <span className="text-xs font-mono text-psion-charcoal">
                  {transferBusy
                    ? `Transferring… ${transferProgress.toLocaleString()} bytes`
                    : transferStatus}
                </span>
              </div>
            )}

            <div className="h-64 overflow-auto p-2">
              {listBusy && (
                <div className="text-xs font-mono text-gray-500 italic p-2">Reading…</div>
              )}
              {!listBusy && listing && (listing.length === 0 ? (
                <div className="text-xs font-mono text-gray-500 italic p-2">(empty directory)</div>
              ) : (
                <table className="w-full text-xs font-mono">
                  <thead>
                    <tr className="text-gray-500 text-left border-b border-psion-accent/40">
                      <th className="py-1 pl-2">Name</th>
                      <th className="py-1 w-24 text-right">Size</th>
                      <th className="py-1 w-32 text-right pr-2">Actions</th>
                    </tr>
                  </thead>
                  <tbody>
                    {listing.map((e, i) => (
                      <tr key={i} className="border-b border-psion-accent/20 hover:bg-psion-mid/50">
                        <td className="py-1 pl-2 text-psion-charcoal">
                          {e.isDirectory ? (
                            <button onClick={() => handleOpenFolder(e)}
                                    disabled={listBusy || transferBusy}
                                    className="text-left hover:underline cursor-pointer disabled:no-underline disabled:opacity-60">
                              📁 {e.longName}
                            </button>
                          ) : (
                            <span>{e.longName}</span>
                          )}
                        </td>
                        <td className="py-1 text-right text-gray-500">
                          {e.isDirectory ? '' : formatBytes(e.size)}
                        </td>
                        <td className="py-1 text-right pr-2">
                          {!e.isDirectory && (
                            <>
                              <button onClick={() => void handleDownload(e)} disabled={transferBusy} className={btnGhost} title="Download">↓</button>
                              <button onClick={() => void handleDelete(e)} disabled={transferBusy} className={btnGhost} title="Delete">✕</button>
                            </>
                          )}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              ))}
            </div>
          </>
        )}

        {/* Diagnostics — collapsed by default. */}
        <details
          className="border-t border-psion-accent/30 bg-psion-mid/20"
          open={showDiag}
          onToggle={e => setShowDiag((e.target as HTMLDetailsElement).open)}
        >
          <summary className="px-4 py-2 text-xs font-mono text-gray-500 cursor-pointer select-none">
            Diagnostics — state: {STATE_LABEL[clientState]}
          </summary>
          <div className="px-4 py-3 space-y-3">
            <div className="flex items-center justify-between">
              <span className="text-[11px] font-mono text-gray-500">
                Paste this when reporting a Remote Link problem.
              </span>
              <button onClick={handleCopyDiag} className={btnGhost}>
                {diagCopied ? '✓ Copied' : 'Copy diagnostic data'}
              </button>
            </div>
            <div>
              <div className="text-[11px] font-mono text-gray-500 mb-1">
                Raw bytes from device ({rxLog.length})
              </div>
              <pre className="text-[11px] font-mono bg-black/40 text-green-300 p-2 rounded h-24 overflow-auto whitespace-pre">
                {rxLog.length === 0 ? '(no bytes received yet)' : formatHex(rxLog)}
              </pre>
            </div>
            <div>
              <div className="text-[11px] font-mono text-gray-500 mb-1">
                Raw bytes to device ({txLog.length})
              </div>
              <pre className="text-[11px] font-mono bg-black/40 text-blue-300 p-2 rounded h-24 overflow-auto whitespace-pre">
                {txLog.length === 0 ? '(none sent yet)' : formatHex(txLog)}
              </pre>
            </div>
            <div>
              <div className="text-[11px] font-mono text-gray-500 mb-1">
                Parsed PDUs ({pdus.length} shown)
              </div>
              <pre className="text-[11px] font-mono bg-black/40 text-amber-200 p-2 rounded h-32 overflow-auto whitespace-pre">
                {pdus.length === 0
                  ? '(no PDUs yet — the link-layer state machine logs every Req/Con/Ack/Data PDU each way once Connect has been clicked)'
                  : pdus.map(formatPduRow).join('\n')}
              </pre>
            </div>
          </div>
        </details>
    </div>
  );
}
