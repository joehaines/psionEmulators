/*
 * emulator-worker.js — runs the WASM emulator OFF the main thread.
 *
 * Phase 1 of docs/web-worker-emulation-scope.md. Loaded as a plain classic
 * Worker (static asset, like psion.js) so there's no bundler interaction: it
 * importScripts() the same MODULARIZE'd psion.js the main thread uses, owns the
 * stepFrame loop, and paints the LCD straight to a transferred OffscreenCanvas.
 *
 * The main thread talks to it over postMessage:
 *   • cold/rare calls are request/reply RPC (msg.rpc + msg.id ⇄ {rpcResult,id})
 *   • input is fire-and-forget (key / enqueue / pointer / setDeviceMode / pause)
 *   • status (backlight, orientation, simCycles, cfGap, paused) is pushed ~4x/s
 *
 * Scope (Phase 1): cold boot, render, keyboard/touch input, CF attach/detach,
 * pause/reset, and save-state (heap slice + gzip + IndexedDB, all off the main
 * thread). Audio, SSD/Datapak, serial and saved-state RESTORE are later phases.
 */
/* eslint-disable */
'use strict';

let mod = null;
let canvas = null, ctx = null;
let lcdPtr = 0, W = 0, H = 0;
let pixelBuf = null, imageData = null;
let lcdViewBuffer = null;   // ArrayBuffer the current view was built on
// LCD blit throttle during a Remote-Link transfer. readLCD + putImageData run
// on THIS worker thread, so they steal wall-time from stepFrameFull — and on a
// weak/mobile GPU (the field reports are iOS Safari) putImageData is expensive,
// starving the emulated device's serial servicing so it can't keep its RX FIFO
// drained and large uploads stall. The screen is near-static mid-transfer, so
// blit at most ~15 fps while the transfer pump is on; the reclaimed cycles go
// to the guest. Normal interactive use (pump off) is unthrottled.
let lastBlitAt = 0;
const TRANSFER_BLIT_INTERVAL_MS = 66;
let running = false;     // a device is loaded and the loop is armed
let paused = false;
let deviceMode = false;
let speakerOn = false;   // host speaker enabled → drain the DAC and ship samples
let audioScratch = 0;
const AUDIO_MAX = 4096;  // int16 samples drained per tick (≫ ~125/frame @ 8 kHz)
let micScratch = 0, micScratchCap = 0;   // mic samples from the main thread → writeAudioInput
let micOn = false;
const serialAttached = {};   // uart index → host attached? (stream UART ↔ main for PLP/IrDA)
let serialScratch = 0;
const SERIAL_CAP = 4096;
// Transfer-mode pump flag (mirrors wasmBridge.setSerialPumpEnabled, which only
// the main-thread path reads — the dialogs' flag is forwarded here in worker
// mode). When on, a host serial write pumps emulated cycles until the guest
// responds (bounded), instead of waiting for the next tick.
let serialPumpOn = false;
// Sim-time keepalive (see wasmBridge.setSimKeepAliveFrame). The main thread's
// wall-clock keepalive fires far too rarely PER SIM-FRAME at full sim speed to
// keep the EPOC RemoteLinkServer rescheduled, so a transfer stalls waiting for
// the device's reply. While the transfer pump is on we replay the latest
// keepalive Ack frame once per sim-frame (a benign duplicate Ack the device
// ignores) so reschedule kicks scale with SIM-time. lastWriteUart pins which
// UART the cable is on (the last one the host wrote to).
let simKeepAliveBytes = null;
let lastWriteUart = -1;
let simKeepAliveLogged = false;   // one-time "fix is active" marker (see tick())
// Wall-clock (performance.now) of the last DEVICE→host serial activity (bytes
// drained out of the guest). The sim-keepalive gates on DEVICE silence: it
// kicks only when the device itself has gone quiet. While the device is
// actively replying it's clearly scheduled, so an extra Ack every sim-frame
// would just flood its 4 KB RX FIFO and amplify retransmits (the field
// "keepalive ackPdu storm"). It must NOT also gate on host activity: a large
// upload deadlocks with the device idle in its HALT loop (consuming our bytes
// but never rescheduling its RemoteLinkServer to process them) WHILE the host
// is still retransmitting — gating on host writes there would suppress the very
// kick that wakes the device. Diagnosed via the browser repro
// (test/plp-browser): device silent 22 s, host still writing, pc in the kernel
// idle HALT loop. 0 means "no activity yet".
let lastSerialActivityAt = 0;
// Idle gap before the keepalive resumes (ms). Long enough that an active
// transfer's inter-frame gaps never re-arm it, short enough to still kick the
// device during a multi-second quiet reply-wait (e.g. the drive-list F32 work).
const KEEPALIVE_QUIET_MS = 120;
// Host→device backpressure queue, per UART. serialWriteFromHost only accepts
// what fits in the device's RX FIFO; under load (a starved guest, or a frame
// fat with DLE/SYN byte-stuffing) the FIFO fills mid-frame. Dropping the
// unaccepted tail truncates that frame, the device's link layer never completes
// it, and the transfer stalls even through ARQ (the retransmit hits the same
// full FIFO). Instead we KEEP the tail here and flush it on later ticks as the
// guest drains — lossless host→device delivery. Bounded by single-in-flight
// RFSV/NCP (the host won't send the next request until this one is answered),
// so a slow guest just paces us, it can't make this grow without limit.
const pendingTx = {};   // uart -> Uint8Array still awaiting the device RX FIFO

// Push as much of uart's pending-Tx queue into the device RX FIFO as it will
// accept right now; keep the remainder for the next flush. Returns bytes
// accepted this call.
function flushPendingTx(uart) {
  const buf = pendingTx[uart];
  if (!buf || !buf.length || !mod || !mod.serialWriteFromHost || !serialAttached[uart]) return 0;
  if (!serialScratch) serialScratch = mod._malloc(SERIAL_CAP);
  let off = 0;
  while (off < buf.length) {
    const n = Math.min(SERIAL_CAP, buf.length - off);
    mod.HEAPU8.set(buf.subarray(off, off + n), serialScratch);
    const acc = mod.serialWriteFromHost(uart, serialScratch, n);
    off += acc;
    if (acc < n) break;   // FIFO full — stop, keep the rest queued
  }
  pendingTx[uart] = off < buf.length ? buf.slice(off) : null;
  return off;
}
// Sparse serial tracing (capped per UART). Surfaces the device→host byte path
// in the log panel to localise remote-link / IrDA stalls. Follows the "Show
// Logs" toggle (setLoggingEnabled) — nothing is posted while logging is off.
let serialDebug = false;

// Per-UART diagnostic counters (PSION_SERIAL_DEBUG). Logged sparsely so the
// remote-link / IrDA byte path can be traced from the browser without a rebuild
// (e.g. to see whether a device is emitting on the attached UART at all).
const serialDiag = {};   // uart → { drained, posted }
let serialDiagLogged = {};
function drainSerial() {
  if (!mod || !mod.serialReadToHost) return;
  for (const u in serialAttached) {
    if (!serialAttached[u]) continue;
    if (!serialScratch) serialScratch = mod._malloc(SERIAL_CAP);
    for (;;) {
      const n = mod.serialReadToHost(+u, serialScratch, SERIAL_CAP);
      if (n <= 0) break;
      lastSerialActivityAt = performance.now();   // device→host activity (gates keepalive)
      const bytes = mod.HEAPU8.slice(serialScratch, serialScratch + n);
      // Diagnostic: log the first few non-empty drains per UART (head bytes), so
      // a "device sent nothing" symptom can be localised to the worker drain vs
      // the main-thread read. Capped at 6 logs/UART to avoid flooding.
      if (serialDebug) {
        const d = (serialDiag[u] ??= { drained: 0, posted: 0 });
        d.drained++;
        const k = 'd' + u;
        if ((serialDiagLogged[k] = (serialDiagLogged[k] || 0) + 1) <= 6) {
          const head = Array.from(bytes.subarray(0, 12)).map(b => b.toString(16).padStart(2, '0')).join(' ');
          postMessage({ type: 'log', text: '[serial] drain uart=' + u + ' n=' + n + ' head=' + head });
        }
      }
      postMessage({ type: 'serialRx', uart: +u, bytes: bytes.buffer }, [bytes.buffer]);
      if (n < SERIAL_CAP) break;
    }
  }
}
let micQueue = [];       // pending mic chunks {arr,off}; fed at the emulator's sim-rate
let micQueued = 0;
let micPerFrame = 125;   // mic samples consumed per sim-frame (audioSampleRate/64)
const MIC_QUEUE_CAP = 32000;   // ~4 s @ 8 kHz; drop oldest beyond this (bounds latency)

// Audio telemetry: real-time audio I/O (mic capture, speaker output) only maps
// cleanly to the guest if the core runs at ~1x real-time. Below 1x the mic
// queue overflows (dropped capture) and the speaker queue underflows (stretched
// playback); above 1x the mic queue starves. Sample the sim-vs-real speed and
// queue depth once/sec while audio is active so we can see which regime we're
// in instead of guessing.
let clockHz = 0;
let telLastMs = 0, telLastCycles = 0;

// Feed mic at the emulator's consumption rate, not the real-time capture rate:
// the core runs below real-time, so real mic audio arrives ~2x faster than it is
// consumed. Feeding it all immediately overflows the codec ring and floods the
// SSP IRQ recompute (which crashed the guest after ~2 s). One bounded write per
// tick, sized to the sim-frames advanced, keeps it matched and safe.
function feedMic(frames) {
  if (!micOn || !mod || !mod.writeAudioInput || micQueued === 0 || frames <= 0) return;
  const n = Math.min(frames * micPerFrame, micQueued);
  if (n <= 0) return;
  if (micScratchCap < n * 2) {
    if (micScratch) mod._free(micScratch);
    micScratch = mod._malloc(n * 2); micScratchCap = n * 2;
  }
  const dst = new Int16Array(mod.HEAPU8.buffer, micScratch, n);
  let filled = 0;
  while (filled < n && micQueue.length) {
    const head = micQueue[0];
    const take = Math.min(n - filled, head.arr.length - head.off);
    dst.set(head.arr.subarray(head.off, head.off + take), filled);
    filled += take; head.off += take; micQueued -= take;
    if (head.off >= head.arr.length) micQueue.shift();
  }
  mod.writeAudioInput(micScratch, n);
}
let baseUrl = '/';
let currentDeviceId = null;

// Frame-spaced synthetic-key queue (mirrors the main-thread render loop).
let keyQueue = [];
let keyWait = 0;

// Device-mode pixel LUTs (mirror applyDeviceModePixels in useEmulator).
const GREY = new Uint8Array(256), ALPHA = new Uint8Array(256);
for (let i = 0; i < 256; i++) {
  GREY[i]  = Math.round(0x33 * i / 255);
  ALPHA[i] = Math.round(0xFF - (0xFF - 0x05) * i / 255);
}
function applyDeviceModePixels(buf) {
  for (let i = 0; i < buf.length; i += 4) {
    const lum = (buf[i] * 77 + buf[i + 1] * 150 + buf[i + 2] * 29) >> 8;
    const g = GREY[lum];
    buf[i] = g; buf[i + 1] = g; buf[i + 2] = g; buf[i + 3] = ALPHA[lum];
  }
}

// ── IndexedDB (same store the main thread uses, so saves are interoperable) ──
const IDB_NAME = 'psion-emu', IDB_STORE = 'state';
const STATE_SCHEMA_VERSION = 11;
function openIDB() {
  return new Promise((resolve, reject) => {
    const req = indexedDB.open(IDB_NAME, 1);
    req.onupgradeneeded = e => e.target.result.createObjectStore(IDB_STORE);
    req.onsuccess = e => resolve(e.target.result);
    req.onerror = () => reject(req.error);
  });
}
async function idbPut(key, value) {
  const db = await openIDB();
  return new Promise((resolve, reject) => {
    const tx = db.transaction(IDB_STORE, 'readwrite');
    tx.objectStore(IDB_STORE).put(value, key);
    tx.oncomplete = () => resolve();
    tx.onerror = () => reject(tx.error);
  });
}
function idbGet(key) {
  return openIDB().then(db => new Promise((resolve, reject) => {
    const tx = db.transaction(IDB_STORE, 'readonly');
    const req = tx.objectStore(IDB_STORE).get(key);
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  }));
}
async function idbDelete(key) {
  const db = await openIDB();
  return new Promise((resolve, reject) => {
    const tx = db.transaction(IDB_STORE, 'readwrite');
    tx.objectStore(IDB_STORE).delete(key);
    tx.oncomplete = () => resolve();
    tx.onerror = () => reject(tx.error);
  });
}
// Factory default SSDs: devices that physically shipped with a pack inserted.
// The MC400 and MC200 both came with a ROM:: System Disk in Pack D (slot 3) —
// the window server, shell, OPL and fonts that populate the lower app bar.
// Returns the bundled image URL + pack kind, or null when the slot has no
// factory default.
function defaultSsdFor(deviceId, slot) {
  if (deviceId === 'mc200' && slot === 3) {
    // Same Pack D arrangement as the MC400 — the MC200's own
    // factory System Disk, dumped.
    return { url: 'roms/MC200_V2.12F_system.ssd', kind: 'protected' };
  }
  if (deviceId === 'mc400' && slot === 3) {
    // Strapped write-protected, like the real ROM:: System Disk.
    return { url: 'roms/MC400_V2.60F_system.ssd', kind: 'protected' };
  }
  return null;
}
// Pack kind <-> PsionSSD::Type code. 'ram' (1) and 'flash' (2) are both
// read/write drives on the Psion; 'protected' (3) is the hardware
// write-protect strap of a factory system disk. With no stored kind,
// sniff the FEFS magic.
function ssdTypeForKind(kind, bytes) {
  const k = kind ?? ((bytes.length >= 2 && bytes[0] === 0xA5 && bytes[1] === 0xF1) ? 'flash' : 'ram');
  return k === 'protected' ? 3 : k === 'flash' ? 2 : 1;
}
// Pack kind currently strapped per slot, so saveState can persist it
// alongside the (guest-modified) bytes rather than re-sniffing.
const ssdKinds = [null, null, null, null];
function ssdKindForType(t) {
  return t === 3 ? 'protected' : t === 2 ? 'flash' : 'ram';
}
async function fetchDefaultSsd(relUrl) {
  try {
    const resp = await fetch(new URL(relUrl, self.location.href).href);
    if (!resp.ok) return null;
    return new Uint8Array(await resp.arrayBuffer());
  } catch (_) { return null; }
}
async function streamThrough(stream, bytes) {
  const w = stream.writable.getWriter();
  w.write(bytes); w.close();
  const parts = [];
  const r = stream.readable.getReader();
  for (;;) { const { done, value } = await r.read(); if (done) break; parts.push(value); }
  let len = 0; for (const p of parts) len += p.byteLength;
  const out = new Uint8Array(len); let off = 0;
  for (const p of parts) { out.set(p, off); off += p.byteLength; }
  return out;
}
async function gzip(bytes) {
  if (typeof CompressionStream === 'undefined') return bytes;
  return streamThrough(new CompressionStream('gzip'), bytes);
}
async function gunzip(bytes) {
  if (typeof DecompressionStream === 'undefined') return bytes;
  return streamThrough(new DecompressionStream('gzip'), bytes);
}

// ── Heap-grow helper (mirrors growHeapToFit in useEmulator) ──
function growHeapToFit(needed) {
  if (mod.HEAPU8.byteLength >= needed) return true;
  const p = mod._malloc(needed);
  if (p) mod._free(p);
  return mod.HEAPU8.byteLength >= needed;
}

// Drop every host-side pointer into the WASM heap. Must be called the moment
// a saved snapshot overwrites linear memory (loadDevice restore / revertToSaved)
// or the module is replaced: the snapshot carries the allocator's bookkeeping
// from save time, so addresses allocated before the overwrite are not valid
// chunks in the restored heap — freeing one (ensureLcdBuffers, feedMic's
// micScratch growth) makes dlmalloc walk garbage chunk headers and trap with
// "memory access out of bounds", and writing through one silently corrupts the
// restored heap. The blocks are deliberately leaked instead, exactly like the
// main-thread path (useEmulator's "skip _free when restoring" / revertToSaved).
function invalidateHeapPointers() {
  lcdPtr = 0;
  audioScratch = 0;
  micScratch = 0; micScratchCap = 0;
  serialScratch = 0;
}

// ── The render / step loop.
//
// Unlike the main-thread RAF loop, the worker may block itself freely — the UI
// thread is never waiting on it. So it runs the emulation UNBUDGETED
// (stepFrameFull, a full 1/64 s of sim per call) instead of the 8 ms-budgeted
// stepFrame(). That is the whole point of the worker: a slow core advances sim
// at the host's FULL rate (e.g. ~0.5× real-time on the Series 7 desktop) and
// simply renders fewer fps, instead of the main thread's 8 ms-budget slow-motion
// (sim crawling while the display shows 60 fps of near-static content).
//
// Pacing: nextFrameDue tracks real-time. A fast/idle core is capped at 64
// sim-fps (don't outrun real-time); a slow core leaves nextFrameDue in the past
// and runs flat-out. A per-tick wall cap keeps input/RPC responsive, and a
// resync prevents a death-spiral of accumulated catch-up frames.
const SIM_FRAME_MS = 1000 / 64;   // 15.625 ms of sim per frame
const TICK_WALL_CAP = 12;         // max ms of emulation per tick (input latency bound)
let stepErrors = 0;
let nextFrameDue = 0;

function drainOneKey() {
  if (keyWait > 0) { keyWait--; return; }
  if (keyQueue.length > 0) {
    const ev = keyQueue.shift();
    mod.sendKey(ev.key, ev.down);
    keyWait = ev.down ? 2 : 1;
  }
}

// Zero-delay yield that the timer-nesting clamp does not apply to — see the
// scheduling note at the end of tick(). Declared here because `tick` is a
// hoisted function declaration, so the handler can reference it.
//
// Guarded: this file is also loaded into a bare vm sandbox by the worker
// restore tests, which provide no MessageChannel. Anywhere it is missing we
// simply keep the old setTimeout path.
const tickChannel = (typeof MessageChannel !== 'undefined') ? new MessageChannel() : null;
if (tickChannel) tickChannel.port1.onmessage = () => tick();

function tick() {
  const t0 = performance.now();
  if (running && !paused && mod && ctx) {
    try {
      if (nextFrameDue === 0) nextFrameDue = t0;
      let rendered = false;
      // Run all frames that are due (real-time pacing), unbudgeted, bounded by
      // the per-tick wall cap so input/RPC stay responsive within the worker.
      while (t0 >= nextFrameDue && (performance.now() - t0) < TICK_WALL_CAP) {
        drainOneKey();
        feedMic(1);                     // one sim-frame of mic, matched to consumption
        // Sim-time keepalive: re-inject the latest link Ack before this frame so
        // the device's RemoteLinkServer gets a reschedule kick proportional to
        // sim-time (the wall-clock keepalive alone is too sparse per frame at
        // full speed and stalls the transfer). Benign duplicate Ack; injected as
        // a complete frame between host writes, so it never splits a data frame.
        // ...but ONLY during a quiet reply-wait. While bytes are actively
        // flowing (either direction) the real frames keep the device scheduled,
        // and an extra Ack per sim-frame just floods its RX FIFO and amplifies
        // retransmits (the field "keepalive ackPdu storm" that stalled large
        // transfers). Resume the kick only once the link has been idle a beat.
        if (serialPumpOn && simKeepAliveBytes && lastWriteUart >= 0 &&
            serialAttached[lastWriteUart] && mod.serialWriteFromHost &&
            (performance.now() - lastSerialActivityAt) >= KEEPALIVE_QUIET_MS) {
          if (!serialScratch) serialScratch = mod._malloc(SERIAL_CAP);
          const kn = Math.min(simKeepAliveBytes.length, SERIAL_CAP);
          mod.HEAPU8.set(simKeepAliveBytes.subarray(0, kn), serialScratch);
          mod.serialWriteFromHost(lastWriteUart, serialScratch, kn);
          // Build/active marker (once): if this line is ABSENT from a Show-Logs
          // capture during a transfer, the service worker is serving a stale
          // emulator-worker.js and this fix is not actually running. (BUILD-TAG
          // worker-keepalive-arq)
          if (!simKeepAliveLogged) {
            simKeepAliveLogged = true;
            postMessage({ type: 'log', text: '[serial] BUILD worker-keepalive-arq: sim-keepalive ACTIVE uart=' + lastWriteUart });
          }
        }
        mod.stepFrameFull();
        if (mod.isCFPollGapActive && mod.isCFPollGapActive()) {
          const b0 = performance.now();
          while (performance.now() - b0 < 60 && mod.isCFPollGapActive()) mod.stepFrameFull();
        }
        // Drain the device→host serial queue after EACH sim-frame during a
        // transfer, not only once at the end of the tick. A catch-up tick runs
        // several stepFrameFull calls; during an upload the device emits a
        // link Ack per inbound host frame plus its RFSV replies, which
        // overflowed the device's 4 KB TX queue before the end-of-tick drain
        // and got SILENTLY DROPPED (hardware.h pushTxByte) — the lost reply
        // then stalled the transfer ("large app fails partway", reproduced in
        // _plp_worker_repro: 900 KB wedged once-per-tick, completed per-frame).
        // Keeping the queue drained every frame prevents the overflow.
        if (serialPumpOn) drainSerial();
        // Flush any host→device bytes the FIFO couldn't accept earlier now that
        // this frame has drained it — keeps a backpressured upload moving
        // without dropping (see pendingTx).
        if (serialPumpOn && lastWriteUart >= 0 && pendingTx[lastWriteUart]) flushPendingTx(lastWriteUart);
        nextFrameDue += SIM_FRAME_MS;
        rendered = true;
      }
      // Slow core: stop trying to catch up real-time once far behind, so we run
      // flat-out from "now" rather than spiralling on accumulated frame debt.
      if (performance.now() - nextFrameDue > 250) nextFrameDue = performance.now();
      // Audio telemetry (once/sec while mic or speaker is active).
      if ((micOn || speakerOn) && clockHz > 0) {
        const nowMs = performance.now();
        if (telLastMs === 0) { telLastMs = nowMs; telLastCycles = mod.getSimCycles(); }
        else if (nowMs - telLastMs >= 1000) {
          const cyc = mod.getSimCycles();
          const simSec = (cyc - telLastCycles) / clockHz;
          const realSec = (nowMs - telLastMs) / 1000;
          const speed = realSec > 0 ? simSec / realSec : 0;
          postMessage({ type: 'log', text:
            '[audio-tel] sim-speed=' + speed.toFixed(2) + 'x realtime  micQueue=' +
            micQueued + '/' + MIC_QUEUE_CAP + (micOn ? ' MIC' : '') + (speakerOn ? ' SPK' : '') });
          telLastMs = nowMs; telLastCycles = cyc;
        }
      }
      // Speaker: drain the DAC this tick and ship the batch to the main thread,
      // which feeds the speaker worklet (audio playback stays a main-thread/
      // AudioWorklet concern; only the drain moves here).
      if (rendered && speakerOn && mod.readAudioOutput) {
        if (!audioScratch) audioScratch = mod._malloc(AUDIO_MAX * 2);
        const got = mod.readAudioOutput(audioScratch, AUDIO_MAX);
        if (got > 0) {
          const samples = new Int16Array(got);
          samples.set(new Int16Array(mod.HEAPU8.buffer, audioScratch, got));
          postMessage({ type: 'audio', samples }, [samples.buffer]);
        }
      }
      // Serial: stream any UART output to the host (main thread), where the PLP/
      // IrDA client reads it via the sync serialReadBytes interface.
      if (rendered) drainSerial();
      // Throttle the LCD blit during an active transfer so the guest keeps the
      // CPU it needs to drain its serial FIFO (see TRANSFER_BLIT_INTERVAL_MS).
      const blitNow = rendered && (!serialPumpOn ||
        (performance.now() - lastBlitAt) >= TRANSFER_BLIT_INTERVAL_MS);
      if (blitNow) {
        lastBlitAt = performance.now();
        refreshLcdView();
        mod.readLCD(lcdPtr);
        if (deviceMode) applyDeviceModePixels(pixelBuf);
        ctx.putImageData(imageData, 0, 0);
      }
      stepErrors = 0;
    } catch (err) {
      stepErrors++;
      postMessage({ type: 'error', message: 'stepFrame threw: ' + String(err && err.message || err) });
      if (stepErrors >= 180) { running = false; paused = true; postStatus(); return; }
    }
  }
  if (running) postStatusMaybe();
  // ALWAYS reschedule while the worker lives — a device switch sets running=false
  // during the async ROM load, and an early return here would kill the loop for
  // good (it's only armed once). Idle when not running; pace to the frame when running.
  const sleep = running ? Math.max(0, Math.min(SIM_FRAME_MS, nextFrameDue - performance.now()))
                        : SIM_FRAME_MS;
  // A chained setTimeout(0) is clamped to 4 ms by the HTML timer-nesting rule
  // (level > 5 forces timeout to 4). Measured in Chromium inside a Worker:
  // 249 iterations/sec, 4.02 ms each — against 90,228/sec, 0.011 ms each, for a
  // MessageChannel port. sleep is 0 exactly when the core is behind real time
  // and wants to run again immediately, i.e. on the CPU-bound guests that can
  // least afford to hand 4 ms of every ~16 ms back to the scheduler. Yield
  // through the port in that case; keep setTimeout for genuine waits.
  //
  // Both are ordinary tasks, so incoming messages (input, RPC, device switch)
  // still interleave exactly as they did before.
  if (sleep <= 0 && tickChannel) tickChannel.port2.postMessage(0);
  else                           setTimeout(tick, sleep);
}

// A psion.wasm built before the screen-orientation binding reports
// nothing, and on the netpad that is indistinguishable from "Switch
// orientation is broken" — the desktop draws rotated and the device
// never turns. Say so once rather than silently reporting upright.
let staleOrientationWarned = false;
function readOrientation() {
  if (!mod) return 0;
  if (typeof mod.getScreenOrientation !== 'function') {
    if (!staleOrientationWarned) {
      staleOrientationWarned = true;
      console.warn('psion.wasm pre-dates the screen-orientation binding — the netpad '
        + 'will draw rotated without the device turning. Rebuild it: bash scripts/build-wasm.sh');
    }
    return 0;
  }
  return mod.getScreenOrientation();
}

let lastStatus = 0;
function postStatusMaybe() { if (performance.now() - lastStatus > 200) postStatus(); }
function postStatus() {
  lastStatus = performance.now();
  postMessage({
    type: 'status',
    paused,
    simCycles: mod ? mod.getSimCycles() : 0,
    backlight: (mod && mod.getBacklight) ? mod.getBacklight() : false,
    cfGap: (mod && mod.isCFPollGapActive) ? mod.isCFPollGapActive() : false,
    cardAttached: (mod && mod.isCFImageAttached) ? mod.isCFImageAttached() : false,
    // Quarter-turns anticlockwise the panel image has to be shown at —
    // non-zero once the netpad's "Switch orientation" has been used.
    orientation: readOrientation(),
  });
}

function ensureLcdBuffers(info) {
  W = info.lcdWidth; H = info.lcdHeight;
  if (canvas) { canvas.width = W; canvas.height = H; ctx = canvas.getContext('2d'); }
  if (lcdPtr) mod._free(lcdPtr);
  lcdPtr = mod._malloc(W * H * 4);
  lcdViewBuffer = null;            // force the view below to be rebuilt
  refreshLcdView();
}

// The frame used to be copied out of the wasm heap into a JS-owned array before
// being uploaded — 1.2 MB memcpy per painted frame at 640x480. An ImageData can
// view the heap directly instead. Measured in Chromium with an OffscreenCanvas:
// copy + putImageData 0.236 ms/frame, putImageData straight off the heap
// 0.096 ms — 0.140 ms a frame saved.
//
// The catch is ALLOW_MEMORY_GROWTH: growing the heap replaces the underlying
// ArrayBuffer and detaches every view onto the old one, so the ImageData would
// start throwing. Cheap to defend — compare buffer identity and rebuild — and
// it must be checked every frame, because growth can happen at any allocation
// (attaching a CF image, loading a device).
function refreshLcdView() {
  const buf = mod.HEAPU8.buffer;
  if (buf === lcdViewBuffer && pixelBuf) return;
  lcdViewBuffer = buf;
  pixelBuf  = new Uint8ClampedArray(buf, lcdPtr, W * H * 4);
  imageData = new ImageData(pixelBuf, W, H);
}

// Loading-progress push (loadDevice → main thread). value is 0..1 for a
// determinate bar or null for indeterminate; status is the human-readable
// phase line ("Downloading ROM… 2.1 of 8.4 MB"). Ordering with the final
// rpcResult is guaranteed — same postMessage channel.
function postLoadProgress(value, status) {
  postMessage({ type: 'loadProgress', value, status });
}
function fmtMB(bytes) { return (bytes / 1048576).toFixed(1); }

// ── EPOC Unique id (see the rpc handlers below) ──
// The id EPOC shows under System → Information → Machine: the identity chip's
// half plus the model UID the ROM supplies. The bindings landed together, so
// requiring the full set doubles as the "is this psion.wasm new enough?"
// check — an older build reports unsupported and the UI hides the control.
function machineIdReady() {
  return !!(mod && mod.hasMachineId && mod.getMachineId && mod.setMachineId
            && mod.getMachineIdPrefix && mod.canSetMachineIdPrefix
            && mod.setMachineIdPrefix && mod.hasMachineId());
}

function readMachineId() {
  if (!machineIdReady()) {
    return { supported: false, id: null, prefix: null, prefixSettable: false };
  }
  const prefix = mod.getMachineIdPrefix() >>> 0;
  return {
    supported: true,
    id: mod.getMachineId() >>> 0,
    prefix: prefix !== 0 ? prefix : null,
    prefixSettable: !!mod.canSetMachineIdPrefix(),
  };
}

// Writes `override` — the full Unique id as a 16-digit hex string, or null to
// leave the machine alone. Called from loadDevice once the ROM (or the
// restored heap) is in place and before the device starts stepping, so EPOC's
// boot-time read sees the user's value. The high half is a ROM constant and
// only lands where the emulator located it; the reply says what stuck.
function applyMachineId(override) {
  if (!machineIdReady()) {
    return { supported: false, id: null, prefix: null, prefixSettable: false };
  }
  if (override != null) {
    const v = BigInt('0x' + String(override).replace(/[^0-9a-fA-F]/g, ''));
    mod.setMachineId(Number(v & 0xFFFFFFFFn) >>> 0);
    const wantPrefix = Number((v >> 32n) & 0xFFFFFFFFn) >>> 0;
    if (wantPrefix !== 0 && mod.canSetMachineIdPrefix()) mod.setMachineIdPrefix(wantPrefix);
  }
  return readMachineId();
}

// ── ROM language variant (see the rpc handlers below) ──
// A multilingual ROM carries one set of resources per language and picks
// between them at boot. Only the Geofox One has more than one; everything
// else reports no names and the UI hides the control. Same "did the bindings
// land?" check as the machine id above.
function languageReady() {
  return !!(mod && mod.getLanguageCount && mod.getLanguageName
            && mod.getLanguage && mod.setLanguage);
}

// Selects `override` (an index, or null to leave the machine alone) and
// reports what stuck. Called from loadDevice before the device starts
// stepping, so the guest's boot-time read of its settings PROM sees it.
function applyLanguage(override) {
  if (!languageReady()) return { names: [], index: 0 };
  const count = mod.getLanguageCount() | 0;
  if (count < 2) return { names: [], index: 0 };
  if (override != null) mod.setLanguage(override | 0);
  const names = [];
  for (let i = 0; i < count; i++) names.push(mod.getLanguageName(i));
  return { names, index: mod.getLanguage() | 0 };
}

// ── RPC handlers (return a value or throw) ──
const rpc = {
  getProfiles() { return JSON.parse(mod.getAllDeviceProfilesJSON()); },
  // `machineId` (null when unset) is the user's stored Unique-id override for
  // this device, as a hex string — see the helpers above. It rides along with
  // the load because localStorage, where it is persisted, is main-thread-only.
  async loadDevice({ deviceId, romUrl, preroll, restore = true, machineId = null,
                     language = null }) {
    // Auto-save the outgoing device before switching (mirrors the main-thread
    // hook's save-on-switch). Must run BEFORE currentDeviceId is reassigned —
    // saveState() keys IndexedDB on it. Swallow errors so a bad save can't
    // wedge the switch; the device-switch must always proceed.
    if (mod && currentDeviceId && currentDeviceId !== deviceId) {
      postLoadProgress(0.02, 'Saving current session…');
      try { await rpc.saveState(); }
      catch (e) { postMessage({ type: 'log', text: 'save-on-switch failed: ' + (e && e.message || e), err: true }); }
    }
    running = false; paused = false;
    currentDeviceId = deviceId;
    // The new device instance has no host bridges. Stale attach flags from the
    // outgoing device would make drainSerial() poll a UART that was never
    // attached on the new instance — and (with the matching stale main-thread
    // flag) let serialWrite silently drop the host's frames.
    for (const u in serialAttached) serialAttached[u] = false;
    postLoadProgress(0.04, 'Downloading ROM…');
    const resp = await fetch(romUrl);
    if (!resp.ok) throw new Error('HTTP ' + resp.status + ' fetching ROM');
    // Stream the body so the bar tracks the actual download — on slow mobile
    // connections this is most of the wait. Download occupies 4%→75% of the
    // bar; posts are throttled so a fast link doesn't flood the main thread.
    // Content-Length can be the compressed size while the reader yields
    // decompressed bytes, so the fraction is clamped to 1.
    const total = Number(resp.headers.get('Content-Length') || 0);
    let rom;
    if (total > 0 && resp.body) {
      const reader = resp.body.getReader();
      const chunks = [];
      let received = 0, lastPost = 0;
      for (;;) {
        const { done, value } = await reader.read();
        if (done) break;
        chunks.push(value);
        received += value.byteLength;
        const now = performance.now();
        if (now - lastPost > 80 || received >= total) {
          lastPost = now;
          const frac = Math.min(1, received / total);
          postLoadProgress(0.04 + 0.71 * frac,
            'Downloading ROM… ' + fmtMB(received) + ' of ' + fmtMB(total) + ' MB');
        }
      }
      rom = new Uint8Array(received);
      let off = 0;
      for (const c of chunks) { rom.set(c, off); off += c.byteLength; }
    } else {
      // No usable length → indeterminate sweep while the body buffers.
      postLoadProgress(null, 'Downloading ROM…');
      rom = new Uint8Array(await resp.arrayBuffer());
    }
    postLoadProgress(0.78, 'Preparing ROM…');
    const ptr = mod.prepareROMUpload(rom.length);
    mod.HEAPU8.set(rom, ptr);
    let name = mod.loadBufferedROM(rom.length, deviceId);
    let info = mod.getDeviceInfo();
    // Auto-restore the device's saved snapshot if one exists (mirrors the main-
    // thread hook's tryRestore). Without this, returning to a device always
    // cold-boots even though its state was saved on the way out. The cold boot
    // above is the safe baseline; we then overwrite linear memory with the
    // snapshot. The version gate keeps cross-build saves (incompatible heap
    // layout) from being applied — those fall through to the cold boot.
    // restore=false forces a cold boot regardless of any saved snapshot — the
    // Reset button uses it so reset boots fresh from ROM instead of reverting
    // to the last saved session.
    let restored = false;
    if (restore) {
      // Once the snapshot has been written over linear memory, two distinct
      // hazards exist (both observed as "Error: memory access out of bounds"
      // on a device switch):
      //   1. Host-side heap pointers (lcdPtr from the OUTGOING device, the
      //      audio/mic/serial scratch buffers) are not valid allocations in
      //      the restored allocator — ensureLcdBuffers' _free(lcdPtr) below
      //      trapped on them. invalidateHeapPointers() leaks them instead.
      //   2. The snapshot itself can be incompatible (getDeviceInfo traps or
      //      returns garbage). The memory is poisoned at that point, so the
      //      recovery is a fresh module + redo of the cold boot above, and
      //      the bad snapshot is deleted so the next load doesn't re-trap —
      //      mirroring the main-thread hook's reloadPsionModule() fallback.
      let heapWritten = false;
      try {
        const stored = await idbGet('state-' + deviceId);
        if (stored && stored.version === STATE_SCHEMA_VERSION && stored.heap && stored.byteLength
            && (!stored.deviceId || stored.deviceId === deviceId)) {
          postLoadProgress(0.82, 'Restoring saved session…');
          const heap = await gunzip(stored.heap);
          if (heap.byteLength > 0 && growHeapToFit(heap.byteLength)) {
            heapWritten = true;
            mod.HEAPU8.set(heap.subarray(0, Math.min(heap.byteLength, mod.HEAPU8.byteLength)));
            invalidateHeapPointers();
            const ri = mod.getDeviceInfo();
            // Same plausibility gate as the main-thread restore: a stale heap
            // can return without trapping but with garbage geometry.
            if (!ri || !ri.deviceName
                || !(ri.lcdWidth  > 0 && ri.lcdWidth  <= 4096)
                || !(ri.lcdHeight > 0 && ri.lcdHeight <= 4096)) {
              throw new Error('restored heap returned implausible device info');
            }
            info = ri;
            restored = true;
            postLoadProgress(0.9, 'Restoring saved session…');
          }
        }
      } catch (e) {
        postMessage({ type: 'log', text: 'restore-on-load failed, cold booting: ' + (e && e.message || e), err: true });
        if (heapWritten) {
          try { await idbDelete('state-' + deviceId); } catch (_) {}
          postLoadProgress(0.84, 'Saved session incompatible — cold booting…');
          mod = await createModule();
          invalidateHeapPointers();
          const p2 = mod.prepareROMUpload(rom.length);
          mod.HEAPU8.set(rom, p2);
          name = mod.loadBufferedROM(rom.length, deviceId);
          info = mod.getDeviceInfo();
          restored = false;
        }
      }
    }
    // Re-attach the device's CF from the separately-persisted cf-<device> image
    // (mirrors the main-thread load path, which re-attaches the saved card on
    // every load regardless of heap restore). Two reasons it must run for cold
    // boots too, not just restores: (1) a Reset (restore=false) should boot
    // fresh "as if loaded from fresh" — with the card still inserted, not
    // ejected; (2) a restored heap can capture the CF mount mid-operation (the
    // save runs while switching away), which the guest reads as "Corrupt" — the
    // cf-<device> image was read consistently (readCFImage) at save time, so
    // re-staging and re-mounting it gives a clean mount. Excludes the
    // bootloader-OS-card devices (netBook / 5mxPro) whose CF is the OS image
    // owned by the attachOsCard flow.
    if (deviceId !== 'netbook' && deviceId !== '5mxpro') {
      try {
        const savedCard = await idbGet('cf-' + deviceId);
        const u8 = savedCard && savedCard.byteLength > 0
          ? (savedCard instanceof Uint8Array ? savedCard : new Uint8Array(savedCard)) : null;
        if (u8 && growHeapToFit(u8.length + (1 << 20))) {
          const ptr = mod.prepareCFImageUpload(u8.length);
          mod.HEAPU8.set(u8, ptr);
          mod.attachCFImage(u8.length);   // re-mount consistently; false just leaves it detached
        }
      } catch (e) {
        postMessage({ type: 'log', text: 'CF re-attach on load failed: ' + (e && e.message || e), err: true });
      }
    }
    // Re-attach persisted SSD packs and Datapaks BEFORE the loop starts,
    // for the same reasons as the CF card above — plus one more: a restored
    // heap captures the guest OS with the pack MOUNTED (the EPOC16 file
    // server believes drive A:/B: is live). On a fresh module with nothing
    // in the slot, the guest's next access to that ghost pack wedges its
    // slot scan — observed in the field as "SIBO devices don't boot while
    // an SSD pack is inserted" (this worker never re-attached packs on
    // load; the main-thread hook always did). A cold boot with the pack
    // present at t=0 is the harness-validated configuration
    // (scripts/test-boot.sh --ssd), so attaching here, before any stepping,
    // is the safe spot. Keys are shared with the main-thread hook:
    // ssd-<device>-<slot>, ssd-type-<device>-<slot>, datapak-<device>-<slot>.
    const ssdAttachedState = [false, false, false, false];
    const datapakAttachedState = [false, false];
    try {
      for (let slot = 0; slot < 4; slot++) {
        let img = await idbGet('ssd-' + deviceId + '-' + slot);
        let kind = await idbGet('ssd-type-' + deviceId + '-' + slot);
        // Devices that shipped with a System Disk SSD inserted (the MC400's
        // ROM:: disk in Pack D) get it pre-loaded on cold boot when the slot
        // is otherwise empty. A user-inserted pack persists to IndexedDB and
        // takes precedence; ejecting clears the slot as usual.
        if (!img || !img.byteLength) {
          const def = defaultSsdFor(deviceId, slot);
          if (def) { img = await fetchDefaultSsd(def.url); kind = def.kind; }
        }
        if (!img || !img.byteLength) continue;
        const u8 = img instanceof Uint8Array ? img : new Uint8Array(img);
        if (!growHeapToFit(u8.length + (1 << 20))) continue;
        const t = ssdTypeForKind(kind, u8);
        const p = mod.prepareSSDImageUpload(u8.length);
        mod.HEAPU8.set(u8, p);
        if (mod.attachSSDImage(slot, u8.length, t)) {
          ssdAttachedState[slot] = true;
          ssdKinds[slot] = ssdKindForType(t);
          postMessage({ type: 'log', text: 'SSD: re-attached slot ' + slot + ' on load (' +
            (u8.length >> 10) + ' KB, type ' + t + ')', err: false });
        } else {
          // Bad image (e.g. size not a power of two) — drop it so the
          // slot isn't permanently wedged on every future load.
          try { await idbDelete('ssd-' + deviceId + '-' + slot); } catch (_) {}
          try { await idbDelete('ssd-type-' + deviceId + '-' + slot); } catch (_) {}
        }
      }
      for (let slot = 0; slot < 2; slot++) {
        const img = await idbGet('datapak-' + deviceId + '-' + slot);
        if (!img || !img.byteLength) continue;
        const u8 = img instanceof Uint8Array ? img : new Uint8Array(img);
        if (!growHeapToFit(u8.length + (1 << 20))) continue;
        const p = mod.prepareDatapakUpload(u8.length);
        mod.HEAPU8.set(u8, p);
        if (mod.attachDatapakImage(slot, u8.length)) {
          datapakAttachedState[slot] = true;
        } else {
          try { await idbDelete('datapak-' + deviceId + '-' + slot); } catch (_) {}
        }
      }
    } catch (e) {
      postMessage({ type: 'log', text: 'SSD/Datapak re-attach on load failed: ' + (e && e.message || e), err: true });
    }
    postLoadProgress(0.95, 'Starting device…');
    // Machine ID before the first step, so the guest's boot-time read of the
    // identity chip sees it. Also covers the reset path (which re-enters
    // loadDevice with restore=false) and the restore path, where the stored
    // override is the more recent expression of intent than whatever ID the
    // snapshot was taken with.
    const machineIdState = applyMachineId(machineId);
    // Likewise the ROM's language variant, read out of the settings PROM
    // once and early. Covers the reset path (loadDevice again with
    // restore=false) the same way.
    const languageState = applyLanguage(language);
    micPerFrame = Math.max(1, Math.round((info.audioSampleRate || 8000) / 64));
    clockHz = (mod.getClockHz && mod.getClockHz()) || 0;
    telLastMs = 0; telLastCycles = 0;
    micQueue = []; micQueued = 0;       // fresh capture state per device
    ensureLcdBuffers(info);
    // Skip the cold-boot preroll when we restored a snapshot — it's already booted.
    if (!restored) for (let i = 0; i < (preroll | 0); i++) mod.stepFrameFull();
    nextFrameDue = 0;                    // resync pacing for the new device
    // Reconcile the host serial-bridge tracking with the WASM truth. A
    // restored heap carries the UART's hostAttached flag (and, for a
    // parked Remote Link session, a live link the device is still holding
    // open). We zeroed serialAttached before the restore (so a stale flag
    // from the OUTGOING device couldn't make drainSerial poll a UART the
    // new instance never attached); now re-derive it from the restored
    // bridge. This matters for the device-switch reconnect: if the flag
    // stays false, the main-thread hook believes the bridge is detached
    // and re-runs serialAttachHost on connect — which clears the UART
    // FIFOs and raises a cable-plug modem-status IRQ, the very edge the
    // ROM treats as session-fatal, so the parked session can't be
    // adopted. Surfacing the real flag lets the hook skip that re-attach
    // (matching the main-thread path, which reads serialIsAttached
    // directly). Also re-arms drainSerial so the restored link streams.
    const serialAttachedState = {};
    if (mod.serialIsAttached) {
      for (let u = 0; u <= 3; u++) {
        const on = !!mod.serialIsAttached(u);
        serialAttached[u] = on;
        if (on) serialAttachedState[u] = true;
      }
    }
    running = true;
    if (!tickArmed) { tickArmed = true; tick(); }
    return { deviceName: name, info,
             ssdAttached: ssdAttachedState, datapakAttached: datapakAttachedState,
             serialAttached: serialAttachedState, machineId: machineIdState,
             language: languageState };
  },
  pause() { paused = true; postStatus(); return true; },
  resume() { paused = false; postStatus(); return true; },
  reset() { paused = false; keyQueue = []; keyWait = 0; return true; },
  setDeviceMode({ on }) { deviceMode = !!on; return true; },
  async attachCard({ bytes }) {
    const u8 = new Uint8Array(bytes);
    if (!growHeapToFit(u8.length + (1 << 20))) return false;
    const ptr = mod.prepareCFImageUpload(u8.length);
    mod.HEAPU8.set(u8, ptr);
    return !!mod.attachCFImage(u8.length);
  },
  // Replace the attached CF card's bytes WITHOUT a detach/attach cycle (the
  // "Update files" action), so the guest sees the new files in place. Persist
  // the updated image under the same cf-<device> key saveState/restore use.
  async updateCard({ bytes }) {
    const u8 = new Uint8Array(bytes);
    if (!growHeapToFit(u8.length + (1 << 20))) return false;
    const ptr = mod.prepareCFImageUpload(u8.length);
    mod.HEAPU8.set(u8, ptr);
    const ok = (typeof mod.updateCFImageInPlace === 'function')
      ? !!mod.updateCFImageInPlace(u8.length)
      : !!mod.attachCFImage(u8.length);   // older core: fall back to a plain attach
    if (ok && currentDeviceId) { try { await idbPut('cf-' + currentDeviceId, u8); } catch (e) {} }
    return ok;
  },
  detachCard() { if (mod.detachCFImage) mod.detachCFImage(); return true; },
  async attachSSD({ slot, bytes, ssdType }) {
    const u8 = new Uint8Array(bytes);
    if (!growHeapToFit(u8.length + (1 << 20))) return false;
    const ptr = mod.prepareSSDImageUpload(u8.length);
    mod.HEAPU8.set(u8, ptr);
    // ssdType mirrors PsionSSD::Type: 1 = RAM, 2 = Flash (both
    // read/write), 3 = hardware write-protected. Default to sniffing the
    // FEFS magic for callers that don't say.
    const t = ssdType ?? ((u8.length >= 2 && u8[0] === 0xA5 && u8[1] === 0xF1) ? 2 : 1);
    const ok = !!mod.attachSSDImage(slot, u8.length, t);
    postMessage({ type: 'log', text: 'SSD: hot attach slot ' + slot + ' (' +
      (u8.length >> 10) + ' KB, type ' + t + ') -> ' + ok, err: !ok });
    // Persist like the main-thread hook so the pack survives reloads and
    // device switches (loadDevice re-attaches it before the loop starts).
    if (ok && currentDeviceId) {
      try {
        await idbPut('ssd-' + currentDeviceId + '-' + slot, u8);
        await idbPut('ssd-type-' + currentDeviceId + '-' + slot, ssdKindForType(t));
        ssdKinds[slot] = ssdKindForType(t);
      } catch (_) {}
    }
    return ok;
  },
  async detachSSD({ slot }) {
    // Snapshot any on-device writes before detaching so the persisted
    // image survives (RAM and Flash packs are both guest-writable) —
    // main-thread parity.
    if (mod && currentDeviceId && mod.isSSDImageAttached && mod.isSSDImageAttached(slot)) {
      try {
        const size = mod.getSSDImageSize(slot);
        if (size > 0) {
          const ptr = mod._malloc(size);
          try {
            mod.readSSDImage(slot, ptr);
            await idbPut('ssd-' + currentDeviceId + '-' + slot,
                         new Uint8Array(mod.HEAPU8.subarray(ptr, ptr + size)));
          } finally { mod._free(ptr); }
        }
      } catch (_) {}
    }
    if (mod.detachSSDImage) mod.detachSSDImage(slot);
    ssdKinds[slot] = null;
    return true;
  },
  async attachDatapak({ slot, bytes }) {
    const u8 = new Uint8Array(bytes);
    if (!growHeapToFit(u8.length + (1 << 20))) return { ok: false, kind: 0 };
    const ptr = mod.prepareDatapakUpload(u8.length);
    mod.HEAPU8.set(u8, ptr);
    const ok = !!mod.attachDatapakImage(slot, u8.length);
    if (ok && currentDeviceId) {
      try { await idbPut('datapak-' + currentDeviceId + '-' + slot, u8); } catch (_) {}
    }
    return { ok, kind: ok && mod.getDatapakKind ? mod.getDatapakKind(slot) : 0 };
  },
  async detachDatapak({ slot }) {
    if (mod && currentDeviceId && mod.isDatapakAttached && mod.isDatapakAttached(slot)) {
      try {
        const size = mod.getDatapakImageSize(slot);
        if (size > 0) {
          const ptr = mod._malloc(size);
          try {
            mod.readDatapakImage(slot, ptr);
            await idbPut('datapak-' + currentDeviceId + '-' + slot,
                         new Uint8Array(mod.HEAPU8.subarray(ptr, ptr + size)));
          } finally { mod._free(ptr); }
        }
      } catch (_) {}
    }
    if (mod.detachDatapakImage) mod.detachDatapakImage(slot);
    return true;
  },
  serialAttach({ uart }) {
    const ok = !!(mod.serialAttachHost && mod.serialAttachHost(uart));
    serialAttached[uart] = ok;
    pendingTx[uart] = null;       // drop any backpressure tail from a prior session
    simKeepAliveLogged = false;   // re-arm the active marker for this session
    // Build marker (always, low-volume): proves which emulator-worker.js the
    // service worker actually served. If this line is missing from a Show-Logs
    // capture, the browser is running a stale cached worker without the fix.
    postMessage({ type: 'log', text: '[serial] BUILD worker-keepalive-arq attach uart=' + uart + ' ok=' + ok });
    if (serialDebug) {
      serialDiagLogged = {};   // reset drain-log caps for the new session
    }
    return ok;
  },
  serialDetach({ uart }) {
    if (mod.serialDetachHost) mod.serialDetachHost(uart);
    serialAttached[uart] = false;
    pendingTx[uart] = null;       // discard any undelivered backpressure tail
    return true;
  },
  // Diagnostic snapshot of the guest CPU + serial buffers. Used by the browser
  // transfer repro (frontend/test/plp-browser) to localise a mid-transfer
  // wedge: sampling insn/wfiCount over time tells a HALTED CPU (counts flat)
  // from one SPINNING in a loop (insn climbing, pc stuck); rxPending/txAvail
  // tell whether the device has stopped draining its RX or stopped emitting.
  debugState({ uart }) {
    return {
      pc: (mod.getCpuPc ? mod.getCpuPc() : 0) >>> 0,
      realPc: (mod.getCpuRealPc ? mod.getCpuRealPc() : 0) >>> 0,
      lr: (mod.getCpuLr ? mod.getCpuLr() : 0) >>> 0,
      wfi: mod.getCpuWfi ? mod.getCpuWfi() : 0,
      insn: mod.getInsnCount ? mod.getInsnCount() : 0,
      wfiCount: mod.getWfiCount ? mod.getWfiCount() : 0,
      excCount: mod.getExcCount ? mod.getExcCount() : 0,
      rxPending: mod.serialHostRxPending ? mod.serialHostRxPending(uart) : -1,
      txAvail: mod.serialHostTxAvailable ? mod.serialHostTxAvailable(uart) : -1,
      pendingTx: pendingTx[uart] ? pendingTx[uart].length : 0,
      irq: mod.serialIrqState ? mod.serialIrqState(uart) : 0,
    };
  },
  // Read `count` MMU-translated 32-bit words from `addr` (diagnostic; see
  // debugState). Used by the browser repro to disassemble a wedged guest's
  // live code, which the ROM file isn't mapped 1:1 to.
  debugPeek({ addr, count }) {
    const out = [];
    if (mod.debugPeek32) for (let i = 0; i < count; i++) out.push(mod.debugPeek32((addr + i * 4) >>> 0) >>> 0);
    return out;
  },
  // Write 32-bit words at an MMU-translated virtual address (diagnostic; the
  // upload-wedge investigation uses it to restore a torn-down kernel serial
  // descriptor at the wedge and test whether the transfer resumes).
  debugPoke({ addr, words }) {
    if (!mod.writeVirt) return false;
    for (let i = 0; i < words.length; i++) mod.writeVirt((addr + i * 4) >>> 0, words[i] >>> 0);
    return true;
  },
  getRamSnapshot() {
    if (!mod.getRamSnapshotSize) return null;
    const size = mod.getRamSnapshotSize();
    if (!size) return null;
    const ptr = mod._malloc(size);
    mod.readRamSnapshot(ptr);
    const out = mod.HEAPU8.slice(ptr, ptr + size);
    mod._free(ptr);
    return { bytes: out.buffer, byteLength: size };
  },
  // ── EPOC machine ID ──
  // Writes the unique ID in the device's identity chip (ETNA PROM / Eiger
  // EEPROM) and reports what the chip ended up holding — devices may reserve
  // bits of the word. The persisted override is owned by the main thread
  // (localStorage isn't reachable from a worker) and arrives as a loadDevice
  // argument; a null id here means "leave the chip alone", since the main
  // thread resolves "restore the factory value" to the real default first.
  setMachineId({ id }) { return applyMachineId(id); },
  // The language index is persisted in localStorage by the main thread and
  // rides along with loadDevice the same way; this handler is what the
  // Language control calls between loads.
  setLanguage({ index }) { return applyLanguage(index); },
  getCardBytes() {
    if (!mod.isCFImageAttached || !mod.isCFImageAttached()) return null;
    const size = mod.getCFImageSize();
    const ptr = mod._malloc(size);
    mod.readCFImage(ptr);
    const out = mod.HEAPU8.slice(ptr, ptr + size);
    mod._free(ptr);
    return { bytes: out.buffer, byteLength: size };
  },
  // Live SSD pack bytes — same shape as getCardBytes; lets the SSD
  // dialog list an inserted pack's files in worker mode.
  getSSDBytes({ slot }) {
    if (!mod || !(mod.isSSDImageAttached && mod.isSSDImageAttached(slot))) return null;
    const size = mod.getSSDImageSize(slot);
    if (!size) return null;
    const ptr = mod._malloc(size);
    mod.readSSDImage(slot, ptr);
    const out = mod.HEAPU8.slice(ptr, ptr + size);
    mod._free(ptr);
    return { bytes: out.buffer, byteLength: size };
  },
  async saveState() {
    if (!mod || !currentDeviceId) return false;
    const wasPaused = paused; paused = true;          // freeze the heap
    try {
      if (mod.isCFImageAttached && mod.isCFImageAttached()) {
        const c = rpc.getCardBytes();
        if (c) await idbPut('cf-' + currentDeviceId, new Uint8Array(c.bytes));
      }
      // Persist live SSD / Datapak images alongside the heap: RAM and
      // Flash packs are both guest-writable, and the restored heap
      // expects the pack contents it last saw (loadDevice re-attaches
      // from these keys).
      for (let slot = 0; slot < 4; slot++) {
        if (!(mod.isSSDImageAttached && mod.isSSDImageAttached(slot))) continue;
        const size = mod.getSSDImageSize(slot);
        if (!size) continue;
        const ptr = mod._malloc(size);
        try {
          mod.readSSDImage(slot, ptr);
          await idbPut('ssd-' + currentDeviceId + '-' + slot,
                       new Uint8Array(mod.HEAPU8.subarray(ptr, ptr + size)));
          if (ssdKinds[slot]) {
            await idbPut('ssd-type-' + currentDeviceId + '-' + slot, ssdKinds[slot]);
          }
        } finally { mod._free(ptr); }
      }
      for (let slot = 0; slot < 2; slot++) {
        if (!(mod.isDatapakAttached && mod.isDatapakAttached(slot))) continue;
        const size = mod.getDatapakImageSize(slot);
        if (!size) continue;
        const ptr = mod._malloc(size);
        try {
          mod.readDatapakImage(slot, ptr);
          await idbPut('datapak-' + currentDeviceId + '-' + slot,
                       new Uint8Array(mod.HEAPU8.subarray(ptr, ptr + size)));
        } finally { mod._free(ptr); }
      }
      const raw = mod.HEAPU8.slice();
      const data = await gzip(raw);
      await idbPut('state-' + currentDeviceId, {
        version: STATE_SCHEMA_VERSION, deviceId: currentDeviceId,
        heap: data, byteLength: raw.byteLength,
      });
      return true;
    } finally { paused = wasPaused; }
  },
  // Restore the device's last save: overwrite the whole linear memory with the
  // snapshot (which carries the full allocator state), then re-point the LCD
  // buffer in the restored allocator. Same mechanic as the main-thread hook.
  async revertToSaved() {
    if (!mod || !currentDeviceId) return false;
    const stored = await idbGet('state-' + currentDeviceId);
    if (!stored || stored.version !== STATE_SCHEMA_VERSION || !stored.heap || !stored.byteLength) return false;
    if (stored.deviceId && stored.deviceId !== currentDeviceId) return false;
    const heap = await gunzip(stored.heap);
    if (heap.byteLength === 0) return false;
    const wasPaused = paused; paused = true;
    try {
      if (!growHeapToFit(heap.byteLength)) return false;
      mod.HEAPU8.set(heap.subarray(0, Math.min(heap.byteLength, mod.HEAPU8.byteLength)));
      // The live lcdPtr / scratch buffers are not valid allocations in the
      // restored allocator — drop them (leak) before ensureLcdBuffers would
      // _free(lcdPtr) against the restored heap and trap.
      invalidateHeapPointers();
      const info = mod.getDeviceInfo();
      ensureLcdBuffers(info);          // reallocate LCD ptr in the restored allocator
      nextFrameDue = 0;                // resync pacing after the time jump
      postStatus();
      return true;
    } finally { paused = wasPaused; }
  },
};

let tickArmed = false;

// Instantiate a fresh WASM module (importScripts(psion.js) must already have
// run). Used at init and to recover from a poisoned heap — once a snapshot
// restore has overwritten linear memory and then failed, the old instance's
// memory is garbage and the only safe path forward is a clean instance.
function createModule() {
  return self.createPsionModule({
    print:    (t) => postMessage({ type: 'log', text: String(t) }),
    printErr: (t) => postMessage({ type: 'log', text: String(t), err: true }),
  });
}

onmessage = async (e) => {
  const m = e.data;
  try {
    if (m.type === 'init') {
      baseUrl = m.baseUrl || '/';
      // The emulator's main log channel (cpu.log) is wired in C++ to
      // EM_ASM(console.log(...)) — NOT stdout — so intercept console.log/error
      // here and forward to the main thread, or worker-mode logs are invisible
      // in the page console / log panel. (stdout/stderr go through print/printErr
      // below; the [patch]-style fprintf lines arrive that way.)
      const _clog = console.log.bind(console), _cerr = console.error.bind(console);
      console.log = (...a) => { _clog(...a); postMessage({ type: 'log', text: a.join(' ') }); };
      console.error = (...a) => { _cerr(...a); postMessage({ type: 'log', text: a.join(' '), err: true }); };
      // A failed engine load (psion.js fetch, WASM compile/instantiate — e.g.
      // memory pressure on mobile, or a half-updated HTTP/SW cache) must
      // REJECT the client's init promise, not just toast an error: the app
      // otherwise waits on 'ready' forever with an empty device panel stuck
      // on "Loading…". initError is handled by EmulatorWorkerClient.init().
      try {
        importScripts(baseUrl + 'psion.js');
        mod = await createModule();
      } catch (err) {
        postMessage({ type: 'initError',
                      message: 'emulator engine failed to load: ' + String(err && err.message || err) });
        return;
      }
      postMessage({ type: 'ready' });
      return;
    }
    if (m.type === 'setCanvas') {
      // The OffscreenCanvas can arrive before OR after a device is loaded;
      // (re)bind the 2D context and, if a device is already running, reattach
      // the LCD buffers at the current geometry so painting resumes.
      canvas = m.canvas || null;
      if (canvas && W && H) {
        canvas.width = W; canvas.height = H; ctx = canvas.getContext('2d');
        // Repaint the latest frame immediately so a remounted canvas (Settings
        // ↔ device, device switch) shows content at once instead of grey until
        // the next tick — re-render from the live LCD if we can, else the buffer.
        if (mod && lcdPtr && pixelBuf && imageData) {
          try {
            refreshLcdView();
            mod.readLCD(lcdPtr);
            if (deviceMode) applyDeviceModePixels(pixelBuf);
          } catch (_) { /* fall back to the last buffer */ }
          ctx.putImageData(imageData, 0, 0);
        }
      }
      return;
    }
    if (m.type === 'setHostAudio') {
      speakerOn = !!m.spk; micOn = !!m.mic;
      if (mod && mod.setHostAudioEnabled) mod.setHostAudioEnabled(!!m.spk, !!m.mic);
      if (!micOn) { micQueue = []; micQueued = 0; }   // flush stale capture on disable
      return;
    }
    if (m.type === 'mic') {
      const s = m.samples;                       // Int16Array captured on the main thread
      micQueue.push({ arr: s, off: 0 });
      micQueued += s.length;
      while (micQueued > MIC_QUEUE_CAP && micQueue.length > 1) {
        micQueued -= micQueue[0].arr.length - micQueue[0].off;
        micQueue.shift();                        // drop oldest: bound latency under a slow core
      }
      return;
    }
    if (m.type === 'serialWrite') {
      lastWriteUart = m.uart;   // pin the cable's UART for the sim-time keepalive
      if (mod && mod.serialWriteFromHost && serialAttached[m.uart]) {
        const data = new Uint8Array(m.bytes);
        if (!serialScratch) serialScratch = mod._malloc(SERIAL_CAP);
        // Append to the per-UART backpressure queue (preserving byte order
        // across writes) and flush as much as the device RX FIFO accepts. When
        // it fills, run the guest so its serial ISR drains the FIFO, then flush
        // the rest — bounded by a short wall-clock budget so a starved guest
        // can't hang the worker. Whatever still doesn't fit STAYS QUEUED for the
        // tick loop to flush as the guest catches up; it is never dropped, so a
        // frame can't be truncated mid-stream (the cause of large-upload stalls
        // that ARQ couldn't recover, since the retransmit hit the same full
        // FIFO). See pendingTx / flushPendingTx.
        const prev = pendingTx[m.uart];
        if (prev && prev.length) {
          const merged = new Uint8Array(prev.length + data.length);
          merged.set(prev, 0); merged.set(data, prev.length);
          pendingTx[m.uart] = merged;
        } else {
          pendingTx[m.uart] = data;
        }
        const writeDeadline = performance.now() + 200;
        for (;;) {
          flushPendingTx(m.uart);
          if (!pendingTx[m.uart]) break;               // fully accepted
          if (!mod.serialPumpCycles || performance.now() > writeDeadline) break;  // leave rest for the tick loop
          mod.serialPumpCycles();                      // let the guest drain RX
          drainSerial();                               // and relieve its TX side
        }
        const accepted = data.length - (pendingTx[m.uart] ? pendingTx[m.uart].length : 0);
        // Diagnostic: the core returns the byte count it actually queued; 0
        // with bytes pending means the device-side UART rejected the write
        // (e.g. core hostAttached=false — a stale attach). Capped per UART.
        if (serialDebug) {
          const k = 'w' + m.uart;
          if ((serialDiagLogged[k] = (serialDiagLogged[k] || 0) + 1) <= 6 || accepted < data.length)
            postMessage({ type: 'log', text: '[serial] write uart=' + m.uart + ' n=' + data.length + ' accepted=' + accepted });
        }
        // Turnaround accelerator. The host just wrote a protocol frame and is
        // now polling for the guest's reply; left to the tick loop, that reply
        // waits for the next tick drain + the client's next poll (~25-50 ms per
        // exchange). PLP's second-scale timeouts shrug that off, but IrLAP's
        // slot-scale discovery/handshake windows do not — device-initiated IR
        // (beam to computer) never connected over this bridge. Pump a short
        // burst so the guest's ISR + response run NOW, and when the transfer-
        // mode flag is on keep pumping until output appears (bounded), then
        // ship it immediately — the worker-mode equivalent of the pump loop in
        // wasmBridge.serialReadBytes.
        if (mod.serialPumpCycles) {
          mod.serialPumpCycles();
          if (serialPumpOn) {
            const deadline = performance.now() + 30;
            while (performance.now() < deadline) {
              const n = mod.serialReadToHost(m.uart, serialScratch, SERIAL_CAP);
              if (n > 0) {
                const bytes = mod.HEAPU8.slice(serialScratch, serialScratch + n);
                postMessage({ type: 'serialRx', uart: m.uart, bytes: bytes.buffer }, [bytes.buffer]);
                break;
              }
              mod.serialPumpCycles();
            }
          }
        }
        drainSerial();
      } else if (serialDebug) {
        // A write arrived for a UART the worker doesn't consider attached —
        // the silent-drop case behind "host sent N frames, device saw none".
        const k = 'x' + m.uart;
        if ((serialDiagLogged[k] = (serialDiagLogged[k] || 0) + 1) <= 6)
          postMessage({ type: 'log', text: '[serial] write DROPPED uart=' + m.uart + ' (not attached in worker)' });
      }
      return;
    }
    if (m.type === 'setSerialPump') { serialPumpOn = !!m.on; return; }
    if (m.type === 'simKeepAlive') { simKeepAliveBytes = m.bytes ? new Uint8Array(m.bytes) : null; return; }
    if (m.type === 'setLoggingEnabled') {
      serialDebug = !!m.on;   // serial tracing follows the Show Logs toggle
      mod && mod.setLoggingEnabled && mod.setLoggingEnabled(!!m.on);
      return;
    }
    if (m.type === 'key')         { mod && mod.sendKey(m.key, m.down); return; }
    if (m.type === 'enqueue')     { for (const ev of m.events) keyQueue.push(ev); return; }
    if (m.type === 'touch')       { mod && mod.sendTouch(m.x, m.y, m.down); return; }
    if (m.type === 'rpc') {
      const fn = rpc[m.rpc];
      if (!fn) throw new Error('unknown rpc ' + m.rpc);
      const result = await fn(m.args || {});
      // Transfer big buffers back without a copy.
      const transfer = (result && result.bytes instanceof ArrayBuffer) ? [result.bytes] : [];
      postMessage({ type: 'rpcResult', id: m.id, result }, transfer);
      return;
    }
  } catch (err) {
    // Include the error name and the top stack frame: engine-side failures
    // can be browser-specific (e.g. Safari/JSC's generic "Type error"), and
    // a bare message gives nothing to localise them with from a field report.
    const detail = (err && err.name ? err.name + ': ' : '') +
      String(err && err.message || err) +
      (err && err.stack ? ' @ ' + String(err.stack).split('\n').slice(0, 2).join(' | ').slice(0, 300) : '');
    if (m && m.type === 'rpc') postMessage({ type: 'rpcResult', id: m.id, error: detail });
    else postMessage({ type: 'error', message: detail });
  }
};
