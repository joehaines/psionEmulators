// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Simulates the useEmulator save chain to verify the "auto-save on device
// switch" sequence preserves the per-device heap snapshot correctly. The
// real hook can't run in Node (it needs a WASM module + DOM), so we model
// the parts that matter: a mock IDB, a "WASM heap" that mutates while the
// emulator runs, the cold-boot fire-and-forget save, and the explicit
// auto-save call sequenced inside loadDevice.
//
// Run via:
//   node --experimental-strip-types frontend/src/lib/__tests__/autoSave.test.mts

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}

// ── Mock IDB ────────────────────────────────────────────────────────────
const idb = new Map<string, unknown>();

async function idbPut(key: string, value: unknown): Promise<void> {
  // Simulate IDB by yielding to the microtask queue once.
  await Promise.resolve();
  idb.set(key, value);
}

// ── Mock "WASM heap" + device state ────────────────────────────────────
// The "heap" is a single byte: the device id char's char-code + however
// many edits the user has made. Each device's heap mutates independently
// inside its render loop.
const heap = { byteCode: 0 };

const currentDeviceIdRef = { current: null as string | null };
const sessionActiveRef = { current: false };
const moduleRef = { current: { HEAPU8: heap } as { HEAPU8: typeof heap } | null };

// ── Save chain (copy of the production logic) ──────────────────────────
const saveChainRef: { current: Promise<void> | null } = { current: null };

function triggerSave(): Promise<void> {
  const next = (saveChainRef.current ?? Promise.resolve())
    .catch(() => undefined)
    .then(() => doActualSave());
  saveChainRef.current = next;
  return next;
}

async function doActualSave(): Promise<void> {
  const mod = moduleRef.current;
  const deviceId = currentDeviceIdRef.current;
  if (!mod || !sessionActiveRef.current || !deviceId) return;
  // Capture the heap "byteCode" synchronously, then write to IDB
  // asynchronously — mirrors the real slice+compress+idbPut sequence.
  const captured = mod.HEAPU8.byteCode;
  await idbPut(`state-${deviceId}`, { deviceId, byteCode: captured });
}

// ── Mock loadDevice (mirrors the real flow) ────────────────────────────
async function loadDevice(deviceId: string): Promise<void> {
  if (currentDeviceIdRef.current && currentDeviceIdRef.current !== deviceId) {
    try { await triggerSave(); }
    catch (err) { console.warn('save-on-switch failed:', err); }
  }
  // doLoadDevice equivalent: cold-boot the new device.
  // 1. Try to restore (no-op if none exists).
  const stored = idb.get(`state-${deviceId}`) as { byteCode: number } | undefined;
  if (stored) {
    heap.byteCode = stored.byteCode;
    currentDeviceIdRef.current = deviceId;
    sessionActiveRef.current = true;
    return;
  }
  // 2. Cold boot: stamp the heap with the device's id-char code, then
  //    fire-and-forget the initial save.
  heap.byteCode = deviceId.charCodeAt(0);
  currentDeviceIdRef.current = deviceId;
  sessionActiveRef.current = true;
  void triggerSave();
}

// ── Tests ──────────────────────────────────────────────────────────────

// 1. Load A → type → load B → load A back → typed state survives.
{
  idb.clear();
  saveChainRef.current = null;
  currentDeviceIdRef.current = null;
  sessionActiveRef.current = false;
  heap.byteCode = 0;

  await loadDevice('A');
  // Let the cold-boot save's chain drain so state-A is in IDB.
  await saveChainRef.current;
  check(idb.has('state-A'), 'cold-boot save of A landed in IDB');

  // User "types" — mutate the heap.
  heap.byteCode = 0xAA;

  await loadDevice('B');
  // Auto-save of A awaited inside loadDevice; chain must include it.
  await saveChainRef.current;
  const savedA = idb.get('state-A') as { byteCode: number };
  check(savedA?.byteCode === 0xAA, `auto-save preserved typed state for A (got ${savedA?.byteCode.toString(16)})`);

  // Switch back to A; heap should now reflect the typed snapshot.
  await loadDevice('A');
  check(heap.byteCode === 0xAA, `restore-on-switch-back loaded typed A (heap=${heap.byteCode.toString(16)})`);
}

// 2. Auto-save runs to completion BEFORE cold-boot of the new device,
//    so an "in-flight cold-boot save" doesn't swallow it. The previous
//    `savingRef` boolean had this bug.
{
  idb.clear();
  saveChainRef.current = null;
  currentDeviceIdRef.current = null;
  sessionActiveRef.current = false;
  heap.byteCode = 0;

  await loadDevice('A');
  // Don't drain the cold-boot save — leave it in flight.
  // Type immediately. Then switch.
  heap.byteCode = 0xBB;
  await loadDevice('C');
  await saveChainRef.current;

  const savedA = idb.get('state-A') as { byteCode: number };
  check(savedA?.byteCode === 0xBB,
    `with cold-boot still in flight, auto-save still preserved typed A (got ${savedA?.byteCode.toString(16)})`);
}

// 3. Three-device round trip — each device's state preserved.
{
  idb.clear();
  saveChainRef.current = null;
  currentDeviceIdRef.current = null;
  sessionActiveRef.current = false;
  heap.byteCode = 0;

  await loadDevice('X');
  heap.byteCode = 0x11;
  await loadDevice('Y');
  heap.byteCode = 0x22;
  await loadDevice('Z');
  heap.byteCode = 0x33;
  // Now switch back through them and verify each.
  await loadDevice('X');
  check(heap.byteCode === 0x11, `X retained typed (got ${heap.byteCode.toString(16)})`);
  heap.byteCode = 0x44;
  await loadDevice('Y');
  check(heap.byteCode === 0x22, `Y retained typed (got ${heap.byteCode.toString(16)})`);
  await loadDevice('X');
  check(heap.byteCode === 0x44, `X retained second edit (got ${heap.byteCode.toString(16)})`);
}

if (failures > 0) {
  console.error(`\n${failures} test(s) failed`);
  process.exit(1);
}
console.log('autoSave tests passed');
