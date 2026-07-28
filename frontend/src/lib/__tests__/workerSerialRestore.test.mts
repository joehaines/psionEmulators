// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Regression test for the device-switch Remote Link wedge.
//
// Repro (browser, worker mode — the default): connect Remote Link on an
// EPOC device, switch to another device, switch back, reconnect fails.
//
// Root cause: switching reuses the worker/module — the outgoing device's
// heap (UART hostAttached flag + the live, parked link session) is
// autosaved and restored byte-for-byte on return, so the session itself
// survives. But the worker zeroed its serial-bridge tracking on load and
// never re-derived it from the restored heap, so the main-thread hook
// believed the bridge was DETACHED and re-ran serialAttachHost() on the
// reconnect. serialAttachHost() clears the UART FIFOs and raises a
// cable-plug modem-status IRQ — the unplug/replug edge the EPOC R5 ROM
// treats as session-fatal — so the parked session could never be adopted
// and the reconnect timed out. (Proven against the live 5mx ROM in
// frontend/src/lib/__tests__/_plp_switch_repro.mts: a redundant
// serialAttachHost during the switch gap turns a clean adoption into a
// "link handshake timed out".)
//
// Fix: after a restore, loadDevice re-derives serialAttached from the
// module's real per-UART hostAttached flags and reports them, so the hook
// can skip the re-attach (matching the main-thread path).
//
// Like workerRestore.test.mts this loads the REAL emulator-worker.js in a
// vm sandbox against a stub module — here one that models the UART
// hostAttached flags (persisted in the heap snapshot) and counts
// serialAttachHost() "plug edges".
//
// Run via:
//   node --experimental-strip-types frontend/src/lib/__tests__/workerSerialRestore.test.mts

import { readFileSync } from 'node:fs';
import vm from 'node:vm';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
  else console.error(`ok: ${msg}`);
}

const HEAP_SIZE = 1 << 20;

// Heap layout the stub cares about:
//   byte 0 → 0x5A restored / else cold boot (drives getDeviceInfo geometry)
//   byte 1 → bitmask of UARTs whose hostAttached flag is set (the bridge
//            state a restored snapshot carries). serialIsAttached reads it
//            live, so writing the snapshot over linear memory restores it.
let plugEdges = 0;        // serialAttachHost() calls (each clears FIFOs + plug IRQ)
function makeModule() {
  const heapBuf = new Uint8Array(HEAP_SIZE);
  let next = 1024;
  const mod = {
    HEAPU8: heapBuf,
    _malloc(n: number) { const p = next; next += (n + 7) & ~7; return p; },
    _free(_p: number) {},
    prepareROMUpload(n: number) { return mod._malloc(n); },
    loadBufferedROM(_n: number, id: string) { heapBuf[0] = 0; heapBuf[1] = 0; return 'Stub ' + id; },
    getDeviceInfo() {
      const restored = heapBuf[0] === 0x5A;
      return { deviceName: restored ? 'Restored' : 'ColdBoot', lcdWidth: 48, lcdHeight: 16, audioSampleRate: 8000 };
    },
    getSimCycles() { return 0; },
    stepFrameFull() {},
    readLCD() {},
    // ── Serial bridge — the surface under test ──
    serialIsAttached(u: number) { return (heapBuf[1] & (1 << u)) !== 0; },
    serialAttachHost(u: number) { plugEdges++; heapBuf[1] |= (1 << u); return true; },
    serialDetachHost(u: number) { heapBuf[1] &= ~(1 << u); return true; },
    serialReadToHost() { return 0; },
  };
  return mod;
}

// ── Mock IndexedDB (mirrors workerRestore.test.mts) ──
const idbStore = new Map<string, unknown>();
const indexedDBStub = {
  open() {
    const req: Record<string, unknown> = {};
    queueMicrotask(() => {
      const db = {
        transaction() {
          const tx: Record<string, unknown> = {
            objectStore: () => ({
              put(v: unknown, k: string) { idbStore.set(k, v); },
              delete(k: string) { idbStore.delete(k); },
              get(k: string) {
                const r: Record<string, unknown> = {};
                queueMicrotask(() => { r.result = idbStore.get(k); (r.onsuccess as (() => void) | undefined)?.(); });
                return r;
              },
            }),
          };
          queueMicrotask(() => (tx.oncomplete as (() => void) | undefined)?.());
          return tx;
        },
      };
      req.result = db;
      (req.onsuccess as ((e: unknown) => void) | undefined)?.({ target: req });
    });
    return req;
  },
};

const workerSrc = readFileSync(new URL('../../../public/emulator-worker.js', import.meta.url), 'utf8');
const pendingRpc = new Map<number, (m: Record<string, unknown>) => void>();
function postMessageStub(msg: Record<string, unknown>) {
  if (msg.type === 'rpcResult') {
    const r = pendingRpc.get(msg.id as number);
    if (r) { pendingRpc.delete(msg.id as number); r(msg); }
  }
}

const sandbox: Record<string, unknown> = {
  onmessage: null,
  postMessage: postMessageStub,
  importScripts: () => {},
  createPsionModule: async () => makeModule(),
  indexedDB: indexedDBStub,
  fetch: async () => ({ ok: true, headers: { get: () => null }, arrayBuffer: async () => new Uint8Array(16).buffer }),
  performance: { now: () => Date.now() },
  setTimeout: () => 0,
  console: { log: () => {}, error: () => {}, warn: () => {} },
  ImageData: class {
    data: unknown; width: number; height: number;
    constructor(data: unknown, width: number, height: number) { this.data = data; this.width = width; this.height = height; }
  },
};
sandbox.self = sandbox;
vm.createContext(sandbox);
vm.runInContext(workerSrc, sandbox, { filename: 'emulator-worker.js' });

type OnMessage = (e: { data: Record<string, unknown> }) => Promise<void>;
const deliver = sandbox.onmessage as unknown as OnMessage;
let nextId = 1;
async function callRpc(rpc: string, args: Record<string, unknown> = {}) {
  const id = nextId++;
  const done = new Promise<Record<string, unknown>>(res => pendingRpc.set(id, res));
  await deliver({ data: { type: 'rpc', rpc, id, args } });
  return done;
}
type LoadResult = { info?: { deviceName?: string }; serialAttached?: Record<number, boolean> };

await deliver({ data: { type: 'init', baseUrl: '/' } });

// 1. Cold-boot device A (a 5mx — Remote Link cable on UART2) and attach
//    the host bridge, as RemoteLinkDialog's Connect does. The hostAttached
//    flag now lives in the heap (where a parked link session would too).
{
  const r = await callRpc('loadDevice', { deviceId: '5mx', romUrl: '/rom1', preroll: 0 });
  const res = r.result as LoadResult | undefined;
  check(!r.error, `cold boot has no error (got ${r.error})`);
  check(!res?.serialAttached || Object.keys(res.serialAttached).length === 0,
        'cold boot reports no attached UARTs');
  const a = await callRpc('serialAttach', { uart: 2 });
  check(a.result === true, 'host attaches to UART2 (Remote Link connect)');
  check(plugEdges === 1, 'connect fired exactly one serialAttachHost plug edge');
}

// 2. Switch to device B. The worker auto-saves the OUTGOING 5mx heap
//    (carrying hostAttached(UART2)=true) and cold-boots B, which has no
//    host bridge — so B must report UART2 detached.
{
  const r = await callRpc('loadDevice', { deviceId: 'revo', romUrl: '/rom2', preroll: 0 });
  const res = r.result as LoadResult | undefined;
  check(!r.error, `switch to B has no error (got ${r.error})`);
  check(!res?.serialAttached || !res.serialAttached[2], 'device B reports UART2 detached');
}

// 3. Switch BACK to device A (restore). THE regression: the restored heap
//    still has hostAttached(UART2)=true (it round-tripped through the
//    real save/restore), and the worker must report it so the hook skips
//    the session-fatal re-attach on the reconnect. A cold boot would
//    instead clear the flag (loadBufferedROM) → UART2 would read detached.
{
  const edgesBefore = plugEdges;
  const r = await callRpc('loadDevice', { deviceId: '5mx', romUrl: '/rom1', preroll: 0 });
  const res = r.result as LoadResult | undefined;
  check(!r.error, `switch back to A restores without error (got ${r.error})`);
  check(res?.serialAttached?.[2] === true,
        'restored device A reports UART2 still attached (so connect skips the re-attach)');
  check(plugEdges === edgesBefore,
        'restore-on-load fired no serialAttachHost plug edge (parked session preserved)');
}

if (failures === 0) console.error('\nworkerSerialRestore: ALL PASS');
else console.error(`\nworkerSerialRestore: ${failures} FAILURE(S)`);
process.exit(failures === 0 ? 0 : 1);
