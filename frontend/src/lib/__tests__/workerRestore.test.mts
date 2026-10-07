// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Regression test for the worker-mode "Error: memory access out of bounds"
// on device switch (3a / Revo, any device with a saved snapshot).
//
// Root cause: rpc.loadDevice restored a heap snapshot — replacing the WASM
// allocator's entire bookkeeping with save-time state — and then
// ensureLcdBuffers() called _free(lcdPtr) with the OUTGOING device's LCD
// buffer pointer, which is not a valid chunk in the restored allocator.
// dlmalloc walked garbage chunk headers and trapped. The main-thread hook
// already leaked the stale pointer on purpose; the worker port didn't.
//
// Unlike the other state tests (which model the logic), this loads the REAL
// frontend/public/emulator-worker.js in a vm sandbox and drives its rpc
// surface against a stub module whose allocator throws "memory access out of
// bounds" when freeing any pointer allocated before the last full-heap
// overwrite — the same observable behaviour as the WASM trap.
//
// It also covers the engine gate (src/lib/engineId.ts): a snapshot carries
// the function-table indices of the psion.wasm that wrote it, so the worker
// must only restore saves stamped with its own engine's identity — the
// desktop app on a Raspberry Pi wedged on "Restoring saved session…" with a
// session saved by a locally built engine.
//
// Run via:
//   node --experimental-strip-types frontend/src/lib/__tests__/workerRestore.test.mts

import { readFileSync } from 'node:fs';
import vm from 'node:vm';
import { engineIdOf } from '../engineId.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}

// Big enough that the bump allocator never hands out a pointer past the end
// (each ensureLcdBuffers malloc is lcdW*lcdH*4 = 3 KB with the stub geometry).
const HEAP_SIZE = 1 << 20;

// ── Stub WASM module ─────────────────────────────────────────────────────
// Heap content drives getDeviceInfo so snapshots can select behaviour:
//   byte0 0x5A → plausible "Restored" info; 0xBA → garbage (stale snapshot);
//   anything else → plausible cold-boot info.
let moduleInstances = 0;
function makeModule() {
  moduleInstances++;
  const heapBuf = new Uint8Array(HEAP_SIZE);
  let epoch = 0;
  const allocs = new Map<number, number>();   // ptr → epoch allocated in
  let next = 1024;
  // A full-length write at offset 0 is a snapshot restore: the allocator's
  // bookkeeping is replaced wholesale, so prior allocations stop existing.
  const origSet = Uint8Array.prototype.set;
  Object.defineProperty(heapBuf, 'set', {
    value(arr: ArrayLike<number>, off?: number) {
      if (!off && arr.length >= HEAP_SIZE) epoch++;
      return origSet.call(this, arr, off ?? 0);
    },
  });
  const mod = {
    HEAPU8: heapBuf,
    _malloc(n: number) { const p = next; next += (n + 7) & ~7; allocs.set(p, epoch); return p; },
    _free(p: number) {
      if (!p) return;
      if (allocs.get(p) !== epoch) throw new Error('memory access out of bounds');
      allocs.delete(p);
    },
    prepareROMUpload(n: number) { return mod._malloc(n); },
    loadBufferedROM(_n: number, id: string) { heapBuf[0] = 0; return 'Stub ' + id; },
    getDeviceInfo() {
      if (heapBuf[0] === 0xBA) return { deviceName: '', lcdWidth: 999999, lcdHeight: 0, audioSampleRate: 8000 };
      const restored = heapBuf[0] === 0x5A;
      return { deviceName: restored ? 'Restored' : 'ColdBoot', lcdWidth: 48, lcdHeight: 16, audioSampleRate: 8000 };
    },
    getSimCycles() { return 0; },
    stepFrameFull() {},
    readLCD() {},
  };
  return mod;
}

// ── Mock IndexedDB (the few corners openIDB/idbPut/idbGet/idbDelete use) ──
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
                queueMicrotask(() => {
                  r.result = idbStore.get(k);
                  (r.onsuccess as (() => void) | undefined)?.();
                });
                return r;
              },
            }),
          };
          // Fires after the caller's synchronous put/delete + handler wiring.
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

// ── Sandbox: run the real worker source ──────────────────────────────────
const workerSrc = readFileSync(new URL('../../../public/emulator-worker.js', import.meta.url), 'utf8');

const messages: Array<Record<string, unknown>> = [];
const pendingRpc = new Map<number, (m: Record<string, unknown>) => void>();
function postMessageStub(msg: Record<string, unknown>) {
  messages.push(msg);
  if (msg.type === 'rpcResult') {
    const r = pendingRpc.get(msg.id as number);
    if (r) { pendingRpc.delete(msg.id as number); r(msg); }
  }
}

// What the worker fetches as psion.wasm (ROM fetches get zeros), and the
// options each createPsionModule call was given.
const WASM_BYTES = new Uint8Array([0x00, 0x61, 0x73, 0x6d, 1, 0, 0, 0, 0xde, 0xad, 0xbe, 0xef]);
const moduleOptions: Array<Record<string, unknown>> = [];

const sandbox: Record<string, unknown> = {
  onmessage: null,                       // predefined so strict-mode `onmessage =` resolves
  postMessage: postMessageStub,
  importScripts: () => {},               // psion.js stubbed by createPsionModule below
  createPsionModule: async (opts: Record<string, unknown>) => { moduleOptions.push(opts); return makeModule(); },
  indexedDB: indexedDBStub,
  crypto: globalThis.crypto,             // SHA-256, as in a browser worker
  fetch: async (url: string) => ({
    ok: true,
    headers: { get: () => null },        // no Content-Length → arrayBuffer path
    arrayBuffer: async () => (String(url).endsWith('psion.wasm')
      ? WASM_BYTES.slice().buffer : new Uint8Array(16).buffer),
  }),
  performance: { now: () => Date.now() },
  setTimeout: () => 0,                   // drop the tick reschedule; test drives rpc directly
  console: { log: () => {}, error: () => {}, warn: () => {} },
  ImageData: class {
    data: unknown; width: number; height: number;
    constructor(data: unknown, width: number, height: number) { this.data = data; this.width = width; this.height = height; }
  },
  // No CompressionStream/DecompressionStream → worker's gzip/gunzip are
  // identity, so snapshots below are stored raw.
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

function snapshot(firstByte: number): Uint8Array {
  const s = new Uint8Array(HEAP_SIZE);
  s[0] = firstByte;
  return s;
}

await deliver({ data: { type: 'init', baseUrl: '/' } });
check(moduleInstances === 1, 'init created the module');

// The identity the worker stamps on saves — the same function the main
// thread uses (hasStoredState, bundle import/export) must produce it.
const ENGINE = await engineIdOf(WASM_BYTES);
check(/^[0-9a-f]{64}$/.test(ENGINE), `engine id is a SHA-256 hex digest (${ENGINE})`);
check(sandbox.ENGINE_ID === undefined
      && vm.runInContext('ENGINE_ID', sandbox) === ENGINE,
      `worker's engine id matches lib/engineId (${vm.runInContext('ENGINE_ID', sandbox)})`);
{
  const opts = moduleOptions[0] ?? {};
  const bin = opts.wasmBinary as ArrayBuffer | undefined;
  check(!!bin && Buffer.from(bin).equals(Buffer.from(WASM_BYTES)),
        'the module is instantiated from the same bytes that were fingerprinted');
}
// Without SubtleCrypto (insecure origin) both sides fall back to the same
// FNV fingerprint.
{
  const workerFnv = await vm.runInContext(
    `(() => { const c = self.crypto; self.crypto = undefined;
              return engineIdOf(new Uint8Array([1, 2, 3, 250])).finally(() => { self.crypto = c; }); })()`,
    sandbox) as string;
  const desc = Object.getOwnPropertyDescriptor(globalThis, 'crypto')!;
  Object.defineProperty(globalThis, 'crypto', { value: undefined, configurable: true });
  const libFnv = await engineIdOf(new Uint8Array([1, 2, 3, 250]));
  Object.defineProperty(globalThis, 'crypto', desc);
  check(workerFnv.startsWith('fnv:') && workerFnv === libFnv,
        `FNV fallback agrees between worker and lib (${workerFnv} vs ${libFnv})`);
}

// 1. Cold-boot a first device (no snapshot) — allocates the worker's lcdPtr.
{
  const r = await callRpc('loadDevice', { deviceId: 'series5', romUrl: '/rom1', preroll: 0 });
  check(!r.error, `first cold boot has no error (got ${r.error})`);
}

// 2. THE regression: switch to a device WITH a saved snapshot. Pre-fix, the
//    restore overwrote the allocator, then ensureLcdBuffers freed the first
//    device's stale lcdPtr against it → "memory access out of bounds".
{
  idbStore.set('state-revo', { version: 14, deviceId: 'revo', engine: ENGINE, heap: snapshot(0x5A), byteLength: HEAP_SIZE });
  const r = await callRpc('loadDevice', { deviceId: 'revo', romUrl: '/rom2', preroll: 0 });
  check(!r.error, `switch-with-snapshot restores without trapping (got ${r.error})`);
  const info = (r.result as { info?: { deviceName?: string } } | undefined)?.info;
  check(info?.deviceName === 'Restored', `snapshot was actually restored (deviceName=${info?.deviceName})`);
}

// 3. revertToSaved bumps the allocator epoch again — its ensureLcdBuffers
//    call had the same stale-free hazard.
{
  idbStore.set('state-revo', { version: 14, deviceId: 'revo', engine: ENGINE, heap: snapshot(0x5A), byteLength: HEAP_SIZE });
  const r = await callRpc('revertToSaved');
  check(!r.error && r.result === true, `revertToSaved restores without trapping (error=${r.error} result=${r.result})`);
}

// 4. Implausible snapshot (stale build): memory is poisoned once written, so
//    the worker must rebuild the module, cold-boot, and delete the snapshot.
{
  idbStore.set('state-osaris', { version: 14, deviceId: 'osaris', engine: ENGINE, heap: snapshot(0xBA), byteLength: HEAP_SIZE });
  const before = moduleInstances;
  const r = await callRpc('loadDevice', { deviceId: 'osaris', romUrl: '/rom3', preroll: 0 });
  check(!r.error, `implausible snapshot falls back to cold boot without error (got ${r.error})`);
  const info = (r.result as { info?: { deviceName?: string } } | undefined)?.info;
  check(info?.deviceName === 'ColdBoot', `fallback is a cold boot (deviceName=${info?.deviceName})`);
  check(moduleInstances === before + 1, 'poisoned heap was replaced with a fresh module');
  check(!idbStore.has('state-osaris'), 'bad snapshot was deleted so the next load cold-boots cleanly');
}

// 5. THE Raspberry Pi hang: a save made by a different psion.wasm (same
//    schema version, plausible-looking heap) must not be written over this
//    engine's memory at all — load cold-boots, revert refuses, and the
//    log says why.
{
  idbStore.set('state-series7', { version: 14, deviceId: 'series7', engine: 'f'.repeat(64),
                                  heap: snapshot(0x5A), byteLength: HEAP_SIZE });
  const before = messages.length;
  const r = await callRpc('loadDevice', { deviceId: 'series7', romUrl: '/rom4', preroll: 0 });
  check(!r.error, `foreign-engine save loads without error (got ${r.error})`);
  const info = (r.result as { info?: { deviceName?: string } } | undefined)?.info;
  check(info?.deviceName === 'ColdBoot', `foreign-engine save is not restored (deviceName=${info?.deviceName})`);
  check(messages.slice(before).some(m => m.type === 'log' && /different engine build/.test(String(m.text))),
        'the cold boot is explained in the log');
  const rv = await callRpc('revertToSaved');
  check(rv.result === false, `revertToSaved refuses a foreign-engine save (result=${rv.result})`);
}

// 6. Saves from before engines were recorded carry no stamp: unknowable,
//    so they cold-boot too.
{
  idbStore.set('state-revo', { version: 14, deviceId: 'revo', heap: snapshot(0x5A), byteLength: HEAP_SIZE });
  const r = await callRpc('loadDevice', { deviceId: 'revo', romUrl: '/rom2', preroll: 0 });
  const info = (r.result as { info?: { deviceName?: string } } | undefined)?.info;
  check(info?.deviceName === 'ColdBoot', `unstamped save is not restored (deviceName=${info?.deviceName})`);
}

// 7. A save is stamped with this engine, and so restores on it.
{
  const r = await callRpc('saveState');
  check(r.result === true, `saveState succeeds (result=${r.result} error=${r.error})`);
  const stored = idbStore.get('state-revo') as { engine?: string; version?: number } | undefined;
  check(stored?.engine === ENGINE, `save is stamped with the engine (${stored?.engine})`);
  check(stored?.version === 14, `save carries the schema version (${stored?.version})`);
}

if (failures > 0) {
  console.error(`\n${failures} test(s) failed`);
  process.exit(1);
}
console.log('workerRestore tests passed');
process.exit(0);   // the worker may hold the loop open; tests are done
