// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Regression test for the device-load generation guard in useEmulator.ts.
//
// The hook's doLoadDevice() runs a multi-await sequence (decompress, ROM
// fetch, IDB reads, yields to the UI) before it commits a device as the
// running one. With nothing serialising those runs, picking device B while
// device A was still mid-load left two doLoadDevice invocations interleaving
// their writes to the shared module / lcdPtr / currentDeviceId refs. The
// corruption could make every subsequent stepFrame() throw, which — with the
// render loop's "keep alive on throw" guard — presented as the site wedged
// in a permanent loading/error loop.
//
// The fix gives every load a monotonic token (loadGenerationRef): after each
// await the run checks the token is still current and bails if a newer load
// has started. This file models that pattern and pins the invariants:
//   1. Only the most-recent load commits a running device.
//   2. A superseded load performs NO writes after the await on which it was
//      overtaken (it must not clobber the winner's committed state).
//   3. A lone load (no contention) still commits normally.
//
// Run via:
//   node --experimental-strip-types frontend/src/lib/__tests__/loadGeneration.test.mts

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}

const yield_ = () => new Promise<void>(r => setTimeout(r, 0));

// ── Model of the production guard ───────────────────────────────────────
// Mirrors useEmulator.ts: a shared generation ref, a "module" the load
// writes into across await boundaries, and a committed-device slot that
// only the winning load is allowed to set.
function makeLoader() {
  const loadGenerationRef = { current: 0 };
  // The single shared resource both loads would race to write. We record
  // every write so the test can assert a superseded load wrote nothing
  // after it was overtaken.
  const writeLog: string[] = [];
  let committedDevice: string | null = null;

  async function doLoadDevice(deviceId: string): Promise<void> {
    const myGen = ++loadGenerationRef.current;
    const superseded = () => loadGenerationRef.current !== myGen;

    // Phase 1: simulate decompress / ROM fetch.
    await yield_();
    if (superseded()) return;
    writeLog.push(`${deviceId}:phase1`);

    // Phase 2: simulate the IDB reads + yields right before commit.
    await yield_();
    if (superseded()) return;
    writeLog.push(`${deviceId}:phase2`);

    // Commit — only the winner should reach here.
    committedDevice = deviceId;
    writeLog.push(`${deviceId}:commit`);

    // Phase 3: post-commit card/SSD restore awaits.
    await yield_();
    if (superseded()) return;
    writeLog.push(`${deviceId}:postcommit`);
  }

  return {
    doLoadDevice,
    writeLog,
    get committedDevice() { return committedDevice; },
  };
}

// ── Test 1: overlapping loads — only the latest commits ─────────────────
{
  const L = makeLoader();
  // Start A, then B before A has resolved its first await. B claims the
  // newer generation; A must bail at its very next checkpoint.
  const pA = L.doLoadDevice('A');
  const pB = L.doLoadDevice('B');
  await Promise.all([pA, pB]);

  check(L.committedDevice === 'B', `expected B to win, got ${L.committedDevice}`);
  check(!L.writeLog.includes('A:commit'), 'superseded load A must not commit');
  check(!L.writeLog.includes('A:phase1'), 'superseded load A must not write after being overtaken');
  check(L.writeLog.includes('B:commit'), 'winning load B must commit');
  check(L.writeLog.includes('B:postcommit'), 'winning load B must finish post-commit work');
}

// ── Test 2: three rapid switches — only the final one commits ───────────
{
  const L = makeLoader();
  const ps = [L.doLoadDevice('A'), L.doLoadDevice('B'), L.doLoadDevice('C')];
  await Promise.all(ps);

  check(L.committedDevice === 'C', `expected C to win, got ${L.committedDevice}`);
  check(
    !L.writeLog.some(w => w.endsWith(':commit') && !w.startsWith('C')),
    `only C may commit; log was ${JSON.stringify(L.writeLog)}`,
  );
}

// ── Test 3: a load that supersedes mid-flight, then runs alone ──────────
{
  const L = makeLoader();
  // First load runs to completion uncontended.
  await L.doLoadDevice('A');
  check(L.committedDevice === 'A', 'lone load A should commit');

  // A later, separate switch also commits cleanly.
  await L.doLoadDevice('B');
  check(L.committedDevice === 'B', 'subsequent lone load B should commit');
}

if (failures === 0) {
  console.log('loadGeneration tests passed');
} else {
  console.error(`${failures} loadGeneration test(s) failed`);
  process.exit(1);
}
