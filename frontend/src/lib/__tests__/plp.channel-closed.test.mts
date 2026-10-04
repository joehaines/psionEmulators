// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// A request sent on a channel the device has already closed must fail as a
// LOST CHANNEL (LinkResetError — the signal PlpClient.withRfsv answers by
// re-opening SYS$RFSV.*) and must not leave the in-flight slot occupied.
// Previously Ncp.sendOn's "channel not open" throw escaped the Promise
// executor with the request's timers and pending slot still armed, so every
// later request failed "another request is already in flight" until the
// 30-120 s timeout expired.
//
// Run with:
//   node --experimental-strip-types frontend/src/lib/__tests__/plp.channel-closed.test.mts

import { RfsvClient, LinkResetError, buildGetDriveList } from '../plp/rfsv32-spec.ts';
import { Rfsv16Client } from '../plp/rfsv16-spec.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}

// Just the two Ncp methods the clients touch.
const closedNcp = {
  setHandler: () => { /* handler is never invoked */ },
  sendOn: () => { throw new Error('sendOn: channel 2 not open'); },
};

{
  const c = new RfsvClient(closedNcp as never, 2);
  const t0 = Date.now();
  // idempotent=false: a single attempt, so the test doesn't sit in send()'s
  // 1 s retry back-off (LinkResetError is never retried anyway).
  let first: unknown = null;
  try { await c.send(buildGetDriveList(c.nextOpId()), false); } catch (e) { first = e; }
  check(first instanceof LinkResetError, `rfsv32: closed channel rejects with LinkResetError (got ${first})`);
  let second: unknown = null;
  try { await c.send(buildGetDriveList(c.nextOpId()), false); } catch (e) { second = e; }
  check(second instanceof LinkResetError,
    `rfsv32: next request is not wedged "already in flight" (got ${second})`);
  check(Date.now() - t0 < 1000, 'rfsv32: both failures were immediate');
}

{
  const c = new Rfsv16Client(closedNcp as never, 2);
  let err: unknown = null;
  try { await c.getDriveList(); } catch (e) { err = e; }
  check(err instanceof LinkResetError, `rfsv16: closed channel rejects with LinkResetError (got ${err})`);
}

// A SIBO device that acks frames but never answers requests must surface as an
// error, not as an empty drive list. (Timers are shortened so the three 5 s
// probes don't make the test take 15 s.)
{
  const realSetTimeout = globalThis.setTimeout;
  globalThis.setTimeout = ((fn: () => void, ms?: number) =>
    realSetTimeout(fn, Math.min(ms ?? 0, 5))) as typeof setTimeout;
  try {
    const silentNcp = { setHandler: () => { /* no replies ever */ }, sendOn: () => { /* swallowed */ } };
    const c = new Rfsv16Client(silentNcp as never, 2);
    let err: unknown = null;
    let drives: string[] | null = null;
    try { drives = await c.getDriveList(); } catch (e) { err = e; }
    check(err instanceof Error && /timed out/.test(err.message),
      `rfsv16: no answer to any probe is an error, not "no drives" (got err=${err}, drives=${drives})`);
  } finally {
    globalThis.setTimeout = realSetTimeout;
  }
}

if (failures > 0) {
  console.error(`\n${failures} test failure(s)`);
  process.exit(1);
}
console.log('OK — channel-closed tests passed');
