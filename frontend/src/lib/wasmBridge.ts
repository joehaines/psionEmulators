// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import type { PsionModule } from '../types/emulator';

let modulePromise: Promise<PsionModule> | null = null;

export async function loadPsionModule(): Promise<PsionModule> {
  if (!modulePromise) {
    modulePromise = (async () => {
      // psion.js is served as a static asset from public/
      // It exports a factory function createPsionModule()
      const script = document.createElement('script');
      script.src = import.meta.env.BASE_URL + 'psion.js';
      script.type = 'text/javascript';
      await new Promise<void>((resolve, reject) => {
        script.onload = () => resolve();
        script.onerror = () => reject(new Error('Failed to load psion.js'));
        document.head.appendChild(script);
      });

      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      const factory = (window as any).createPsionModule;
      if (typeof factory !== 'function') {
        throw new Error('createPsionModule not found on window');
      }

      const mod = await (factory() as Promise<PsionModule>);
      // Test hook: expose the module on window for the audio harness
      // (scripts/test-audio-browser.mjs) so it can read codec counters
      // without scraping the DOM. No-op for prod usage.
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      (window as any).__psionMod = mod;
      return mod;
    })();
  }
  return modulePromise;
}

// ── Host serial bridge helpers ───────────────────────────────────────

// When true, serialReadBytes pumps extra emulated cycles if the device
// hasn't responded yet.  Set by PlpClient AFTER the RFSV channel is
// open (so the timing-sensitive link handshake runs at normal speed).
let serialPumpEnabled = false;
// In worker mode the module (and therefore the pump) lives in the worker;
// useEmulatorWorker registers a forwarder so the dialogs'/PlpClient's existing
// setSerialPumpEnabled calls reach it. Main-thread mode leaves this null and
// the flag is consumed by the pump loop in serialReadBytes below.
let serialPumpForward: ((on: boolean) => void) | null = null;
export function setSerialPumpForward(fn: ((on: boolean) => void) | null) { serialPumpForward = fn; }
export function setSerialPumpEnabled(v: boolean) { serialPumpEnabled = v; serialPumpForward?.(v); }

// Sim-time keepalive hook, invoked from inside the pump loop below.
//
// The pump fast-forwards device SIM-time synchronously (a tight loop of
// serialPumpCycles, up to 200ms WALL). Because it blocks the JS event loop,
// PlpClient's wall-clock keepalive setInterval CANNOT fire during it — so
// across a multi-second drive-list wait the device sees a large SIM-time gap
// with zero host traffic. The EPOC RemoteLinkServer's link-inactivity timer
// (SIM-time based) then fires, the device re-handshakes (Req_Req_Pdu), and the
// in-flight SYS$RFSV channel is reaped → "NCP Connect … rejected". (This is
// the browser-only bounce in docs/series7-plp-drivelist-2026-06-06.md: the
// native socket repro never blocks the keepalive, so it never bounces.)
//
// PlpClient registers link.keepAlive here so a duplicate Ack is emitted
// between pump bursts, holding the device's timer alive in SIM-time
// proportion rather than wall-time. Cleared on disconnect.
let pumpKeepAlive: (() => void) | null = null;
export function setPumpKeepAlive(fn: (() => void) | null) { pumpKeepAlive = fn; }

// Worker-mode sim-time keepalive. Main-thread mode services the device's
// scheduler via the in-pump keepAlive() above, but in worker mode the pump
// loop lives in the worker and never runs here — the only keepalive is
// PlpClient's wall-clock setInterval, which at full sim speed fires far too
// rarely PER SIM-FRAME to keep the EPOC RemoteLinkServer rescheduled (the
// device stalls mid-transfer; enabling Show Logs slows wall-time per frame
// and accidentally masks it). PlpClient pushes the current keepalive frame
// here; useEmulatorWorker forwards it to the worker, which re-injects it once
// per sim-frame so kicks scale with sim-time. Null in main-thread mode (the
// forward is never registered there) and cleared on disconnect.
let simKeepAliveForward: ((bytes: Uint8Array | null) => void) | null = null;
export function setSimKeepAliveForward(fn: ((bytes: Uint8Array | null) => void) | null) { simKeepAliveForward = fn; }
export function setSimKeepAliveFrame(bytes: Uint8Array | null) { simKeepAliveForward?.(bytes); }

export function serialReadBytes(mod: PsionModule, uartIndex: number, cap = 4096): Uint8Array {
  if (!mod.serialIsAttached(uartIndex)) return new Uint8Array(0);
  const ptr = mod._malloc(cap);
  try {
    let n = mod.serialReadToHost(uartIndex, ptr, cap);
    // If we're in transfer mode and no data is ready, pump extra
    // cycles so the kernel processes the pending request NOW instead
    // of waiting for the next requestAnimationFrame.
    // If we're in transfer mode and no data is ready, pump emulated
    // cycles until the kernel produces a response OR 200ms wall-time
    // elapses.  This adapts to any machine speed: fast machines pump
    // many cycles quickly, slow machines pump fewer but still avoid
    // the rAF frame-boundary stall.
    if (n === 0 && serialPumpEnabled && mod.serialPumpCycles) {
      // Shorter block (was 200ms): the pump monopolises the JS event loop, so a
      // long budget starves the RAF step loop AND the wall-clock keepalive. A
      // small budget still fast-forwards the device enough to answer while
      // yielding often so those timers keep running.
      const deadline = performance.now() + 50;
      while (n === 0 && performance.now() < deadline) {
        // Reschedule kick BEFORE every pump step: each keepalive Ack lands as a
        // UART3 RX → synthetic OSMR1 → Reschedule, waking the Series 7's slow
        // EKA1 F32/RemoteLinkServer thread. Doing it every step (not every Nth)
        // matters in the browser, where slow wasm fits only a few pump steps in
        // the budget — so "every 5th" could fire zero kicks in a whole block.
        pumpKeepAlive?.();
        mod.serialPumpCycles();
        n = mod.serialReadToHost(uartIndex, ptr, cap);
      }
    }
    if (n === 0) return new Uint8Array(0);
    return mod.HEAPU8.slice(ptr, ptr + n);
  } finally {
    mod._free(ptr);
  }
}

export function serialWriteBytes(mod: PsionModule, uartIndex: number, data: Uint8Array): number {
  if (data.length === 0 || !mod.serialIsAttached(uartIndex)) return 0;
  const ptr = mod._malloc(data.length);
  try {
    mod.HEAPU8.set(data, ptr);
    return mod.serialWriteFromHost(uartIndex, ptr, data.length);
  } finally {
    mod._free(ptr);
  }
}

// Instantiates a brand-new WASM module with a fresh linear memory.
// Used as a recovery path when a saved-state restore corrupts the
// running module's heap (e.g. snapshot from a previous build whose
// vtable indices no longer match the current indirect-call table).
// The script tag from the original load is reused; only the factory
// call repeats, so this is cheap.
export async function reloadPsionModule(): Promise<PsionModule> {
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const factory = (window as any).createPsionModule;
  if (typeof factory !== 'function') {
    throw new Error('createPsionModule not found on window');
  }
  const mod = await (factory() as Promise<PsionModule>);
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  (window as any).__psionMod = mod;
  modulePromise = Promise.resolve(mod);
  return mod;
}
