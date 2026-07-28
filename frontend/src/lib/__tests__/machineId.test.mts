// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Worker-mode Unique-id plumbing (the "Show debugging" → Unique id panel).
//
// The contract the debug panel depends on, and what breaks if it slips:
//   • The stored override is written into the identity chip DURING the load,
//     BEFORE the device is stepped. EPOC reads the chip at boot, so an ID
//     applied after the first frame would be missed by the boot it was
//     supposed to affect.
//   • A reset (loadDevice with restore=false) re-applies it — the reset
//     rebuilds the emulator from ROM, which puts the stock id back.
//   • A restored snapshot re-applies it too.
//   • setMachineId reports the EFFECTIVE id, which differs from the request
//     where the machine won't take all of it — the ROM-side half only
//     changes where the emulator could locate the model UID
//     (canSetMachineIdPrefix), and this stub's chip word accepts only some
//     bits so the read-back path stays covered.
//   • The id crosses the postMessage boundary as a hex string, because a
//     bigint can't be structured-cloned through the RPC layer.
//   • A psion.wasm without the bindings reports unsupported, so the UI hides
//     the control instead of throwing.
//
// Like workerRestore.test.mts, this runs the REAL frontend/public/
// emulator-worker.js in a vm sandbox against a stub module.
//
// Run via:
//   node --experimental-strip-types frontend/src/lib/__tests__/machineId.test.mts

import { readFileSync } from 'node:fs';
import vm from 'node:vm';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}

const HEAP_SIZE = 1 << 20;
const FACTORY_ID = 0x12345678;
// The stub chip only accepts some bits of the word, standing in for any
// machine that won't take an id verbatim — the panel has to show what stuck
// rather than what was asked for.
const WRITABLE_MASK = 0x3fffff00;
// The fixed high half EPOC prints in front of the settable word (ROM-sourced).
const PREFIX = 0x1000118a;
// What the stub chip ends up holding when `req` is written over `prev`.
const effective = (prev: number, req: number) =>
  ((prev & ~WRITABLE_MASK) | (req & WRITABLE_MASK)) >>> 0;

interface StubModule {
  machineIdWrites: number[];
  steps: number;
  // Index into `events` for ordering assertions.
  events: string[];
  [k: string]: unknown;
}

// `withMachineId` false models an older psion.wasm that predates the bindings.
function makeModule(withMachineId = true): StubModule {
  const heapBuf = new Uint8Array(HEAP_SIZE);
  let next = 1024;
  let machineId = FACTORY_ID;
  let prefix = PREFIX;
  const mod: StubModule = {
    machineIdWrites: [],
    steps: 0,
    events: [],
    HEAPU8: heapBuf,
    _malloc(n: number) { const p = next; next += (n + 7) & ~7; return p; },
    _free() {},
    prepareROMUpload(n: number) { return mod._malloc(n); },
    loadBufferedROM(_n: number, id: string) {
      // A fresh emulator: the identity chip is back to its factory value,
      // which is exactly why the override has to be re-applied per load.
      machineId = FACTORY_ID;
      prefix = PREFIX;
      heapBuf[0] = 0;
      return 'Stub ' + id;
    },
    getDeviceInfo() {
      // 0xBA marks a snapshot from an incompatible build: implausible
      // geometry makes the worker discard it and rebuild the module, which
      // is how case 7 swaps in a binding-less one mid-session.
      if (heapBuf[0] === 0xba) {
        return { deviceName: '', lcdWidth: 999999, lcdHeight: 0, audioSampleRate: 8000 };
      }
      return {
        deviceName: heapBuf[0] === 0x5a ? 'Restored' : 'ColdBoot',
        lcdWidth: 48, lcdHeight: 16, audioSampleRate: 8000,
      };
    },
    getSimCycles() { return 0; },
    stepFrameFull() { mod.steps++; mod.events.push('step'); },
    readLCD() {},
  };
  if (withMachineId) {
    Object.assign(mod, {
      hasMachineId() { return true; },
      getMachineId() { return machineId >>> 0; },
      getMachineIdPrefix() { return prefix; },
      canSetMachineIdPrefix() { return true; },
      setMachineIdPrefix(v: number) { prefix = v >>> 0; return true; },
      setMachineId(id: number) {
        // Reserved bits stay as they are, like the SA-1100 EEPROM word.
        machineId = ((machineId & ~WRITABLE_MASK) | (id & WRITABLE_MASK)) >>> 0;
        mod.machineIdWrites.push(id >>> 0);
        mod.events.push('setMachineId');
        return true;
      },
    });
  }
  return mod;
}

// ── Mock IndexedDB (same shape as workerRestore.test.mts) ──
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

// ── Sandbox: run the real worker source ──
const workerSrc = readFileSync(new URL('../../../public/emulator-worker.js', import.meta.url), 'utf8');

const pendingRpc = new Map<number, (m: Record<string, unknown>) => void>();
function postMessageStub(msg: Record<string, unknown>) {
  if (msg.type === 'rpcResult') {
    const r = pendingRpc.get(msg.id as number);
    if (r) { pendingRpc.delete(msg.id as number); r(msg); }
  }
}

let currentModule: StubModule = makeModule();
let nextModuleHasBindings = true;

const sandbox: Record<string, unknown> = {
  onmessage: null,
  postMessage: postMessageStub,
  importScripts: () => {},
  createPsionModule: async () => {
    currentModule = makeModule(nextModuleHasBindings);
    return currentModule;
  },
  indexedDB: indexedDBStub,
  fetch: async () => ({
    ok: true,
    headers: { get: () => null },
    arrayBuffer: async () => new Uint8Array(16).buffer,
  }),
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

interface MachineIdReply {
  supported: boolean; id: number | null; prefix: number | null; prefixSettable: boolean;
}
function machineIdOf(r: Record<string, unknown>): MachineIdReply | undefined {
  return (r.result as { machineId?: MachineIdReply } | undefined)?.machineId;
}
const hex = (v: number | null | undefined) =>
  v == null ? String(v) : (v >>> 0).toString(16).padStart(8, '0');

await deliver({ data: { type: 'init', baseUrl: '/' } });

// 1. No override: the chip is left alone and the factory ID is reported.
{
  const r = await callRpc('loadDevice', { deviceId: '5mx', romUrl: '/rom1', preroll: 0, machineId: null });
  const m = machineIdOf(r);
  check(m?.supported === true, 'load reports machine-ID support');
  check(m?.id === FACTORY_ID, `no override → factory ID (got ${hex(m?.id)})`);
  check(m?.prefix === PREFIX,
        `ROM-side prefix reported so the panel can show all 16 digits (got ${hex(m?.prefix)})`);
  check(currentModule.machineIdWrites.length === 0, 'no override → no write to the chip');
}

// 2. Stored override, cold boot with a preroll: written before ANY stepping,
//    because EPOC reads the identity chip during boot.
{
  const r = await callRpc('loadDevice', {
    deviceId: '5mx', romUrl: '/rom1', preroll: 3, machineId: '1000118acafe1200',
  });
  const m = machineIdOf(r);
  const want2 = effective(FACTORY_ID, 0xcafe1200);
  check(m?.id === want2, `override applied (got ${hex(m?.id)}, want ${hex(want2)})`);
  check(currentModule.machineIdWrites.length === 1, 'chip written exactly once per load');
  const firstWrite = currentModule.events.indexOf('setMachineId');
  const firstStep  = currentModule.events.indexOf('step');
  check(firstWrite >= 0 && firstStep >= 0 && firstWrite < firstStep,
        `ID programmed before the first step (events: ${currentModule.events.join(',')})`);
}

// 3. Reset path: loadDevice with restore=false rebuilds from ROM (which puts
//    the factory ID back), so the override has to be re-applied.
{
  const r = await callRpc('loadDevice', {
    deviceId: '5mx', romUrl: '/rom1', preroll: 0, restore: false, machineId: '1000118a00abcd00',
  });
  const m = machineIdOf(r);
  const want3 = effective(FACTORY_ID, 0x00abcd00);
  check(m?.id === want3, `reset re-applies the override (got ${hex(m?.id)}, want ${hex(want3)})`);
}

// 4. Restored snapshot: the override is re-applied over the snapshot's ID, and
//    the factory value is still reported (it comes from the emulator, not from
//    sampling the chip before the write).
{
  const snap = new Uint8Array(HEAP_SIZE);
  snap[0] = 0x5a;
  idbStore.set('state-revo', { version: 11, deviceId: 'revo', heap: snap, byteLength: HEAP_SIZE });
  const r = await callRpc('loadDevice', {
    deviceId: 'revo', romUrl: '/rom2', preroll: 0, machineId: '1000118a11223300',
  });
  const info = (r.result as { info?: { deviceName?: string } } | undefined)?.info;
  check(info?.deviceName === 'Restored', 'snapshot really was restored');
  const m = machineIdOf(r);
  const want4 = effective(FACTORY_ID, 0x11223300);
  check(m?.id === want4, `override applied over a restored heap (got ${hex(m?.id)}, want ${hex(want4)})`);
  check(m?.prefix === PREFIX,
        `ROM-side half still reported after a restore (got ${hex(m?.prefix)})`);
}

// 5. Live write via the panel: the reply carries the EFFECTIVE id, so a device
//    that reserves bits of the word reports what it actually stored.
{
  const r = await callRpc('setMachineId', { id: 'aabbccddffffffff' });
  const m = r.result as MachineIdReply;
  check(m.supported === true, 'setMachineId reports support');
  // Reserved bits keep the value the chip already held; the ROM-side half
  // takes the request verbatim because this stub can patch it.
  const expected = effective(effective(FACTORY_ID, 0x11223300), 0xffffffff);
  check(m.id === expected,
        `effective chip half reflects reserved bits (got ${hex(m.id)}, want ${hex(expected)})`);
  check(m.prefix === 0xaabbccdd, `ROM-side half was patched (got ${hex(m.prefix)})`);
  check(m.prefixSettable === true, 'reports the ROM-side half as settable');
}

// 6. A machine that can't patch its model UID keeps the ROM's value and says
//    so, instead of silently pretending the write landed.
{
  currentModule.canSetMachineIdPrefix = () => false;
  const r = await callRpc('setMachineId', { id: '99887766aabbccdd' });
  const m = r.result as MachineIdReply;
  check(m.prefixSettable === false, 'reports the ROM-side half as fixed');
  check(m.prefix !== 0x99887766, 'the ROM-side half was NOT changed');
  check(m.id === effective(m.id ?? 0, 0xaabbccdd), 'the chip half still took the write');
  currentModule.canSetMachineIdPrefix = () => true;
}

// 7. Older psion.wasm without the bindings: unsupported, no throw.
{
  nextModuleHasBindings = false;
  // A poisoned snapshot forces the worker to build a fresh module, which is
  // how we swap in the binding-less one mid-session.
  const bad = new Uint8Array(HEAP_SIZE);
  bad[0] = 0xba;
  idbStore.set('state-osaris', { version: 11, deviceId: 'osaris', heap: bad, byteLength: HEAP_SIZE });
  const r = await callRpc('loadDevice', { deviceId: 'osaris', romUrl: '/rom3', preroll: 0, machineId: '1000118adead0000' });
  check(!r.error, `binding-less module loads without error (got ${r.error})`);
  const m = machineIdOf(r);
  check(m?.supported === false, 'binding-less module reports unsupported');
  check(m?.id === null && m?.prefix === null && m?.prefixSettable === false,
        'unsupported reply carries no ids');
  const w = await callRpc('setMachineId', { id: '0000000000001234' });
  check(!w.error, `setMachineId on a binding-less module does not throw (got ${w.error})`);
  check((w.result as MachineIdReply).supported === false, 'setMachineId reports unsupported');
}

if (failures > 0) {
  console.error(`\n${failures} test(s) failed`);
  process.exit(1);
}
console.log('machineId tests passed');
process.exit(0);   // the worker may hold the loop open; tests are done
