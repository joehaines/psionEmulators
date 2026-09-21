// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Tests for UART arbitration.
//
// The failure this prevents is subtle and nasty: PlpClient.start() installs its
// keepalive into a GLOBAL single slot, so two clients on one port do not just
// compete for bytes — the second replaces the first's keepalive and the first's
// session starves and dies. A lease that overlaps two owners by even a moment
// reintroduces exactly that, so the ordering assertions here matter as much as
// the exclusion ones.
//
// Run: node --experimental-strip-types frontend/src/lib/__tests__/uartLease.test.mts

import {
  acquireUart, onUartFree, resetUartLeases, uartHolder, LEASE_PRIORITY,
} from '../plp/uartLease.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}
function eq(actual: unknown, expected: unknown, msg: string) {
  if (actual !== expected) {
    console.error(`FAIL: ${msg} (got ${JSON.stringify(actual)}, want ${JSON.stringify(expected)})`);
    failures++;
  }
}

// ── Basic exclusion ──────────────────────────────────────────────────────
{
  resetUartLeases();
  eq(uartHolder(2), null, 'a free port has no holder');
  const a = await acquireUart(2, 'remote-link');
  check(a !== null, 'an uncontended port is granted');
  eq(uartHolder(2), 'remote-link', 'and the holder is reported');
  check(a?.active, 'the lease starts active');

  // Equal standing must NOT displace: two dialogs both at 10 means whoever got
  // there first keeps it, rather than them taking turns stealing the port.
  const b = await acquireUart(2, 'printer');
  eq(b, null, 'an equal-priority claim is refused');
  eq(uartHolder(2), 'remote-link', 'and the original holder is untouched');

  // A different port is independent.
  const other = await acquireUart(3, 'printer');
  check(other !== null, 'a different UART is unaffected');
  eq(uartHolder(3), 'printer', 'with its own holder');

  a?.release();
  eq(uartHolder(2), null, 'releasing frees the port');
  check(!a?.active, 'and the lease goes inactive');
  eq(uartHolder(3), 'printer', 'the other port is still held');
  other?.release();
}

// ── Preemption ───────────────────────────────────────────────────────────
{
  resetUartLeases();
  const order: string[] = [];
  const background = await acquireUart(2, 'desktop-mount', {
    onPreempt: async () => {
      // A real holder quiesces here, which takes a moment.
      await new Promise((r) => setTimeout(r, 10));
      order.push('background-yielded');
    },
  });
  check(background !== null, 'the background mount takes an idle port');

  order.push('dialog-asks');
  const dialog = await acquireUart(2, 'remote-link');
  order.push('dialog-granted');

  check(dialog !== null, 'a dialog preempts the background mount');
  eq(uartHolder(2), 'remote-link', 'and becomes the holder');
  check(!background?.active, "the preempted lease is no longer active");

  // The ordering is the point: the old owner must have finished yielding BEFORE
  // the new one is granted, or both would be driving the UART at once.
  eq(order.join(','), 'dialog-asks,background-yielded,dialog-granted',
     'the old owner finishes yielding before the new one is granted');

  dialog?.release();
}

// A yield that throws must not prevent the handover — a wedged background job
// cannot be allowed to lock the user out of the Remote Link dialog.
{
  resetUartLeases();
  const bad = await acquireUart(2, 'desktop-mount', {
    onPreempt: () => { throw new Error('deliberately broken'); },
  });
  check(bad !== null, 'the background holder acquired');
  const dialog = await acquireUart(2, 'printer');
  check(dialog !== null, 'a holder whose yield throws is still preempted');
  eq(uartHolder(2), 'printer', 'and the port changes hands');
  dialog?.release();
}

// Background work must not displace other background work of equal standing.
{
  resetUartLeases();
  const first = await acquireUart(2, 'desktop-mount');
  const second = await acquireUart(2, 'desktop-mount');
  check(first !== null, 'the first background holder is granted');
  eq(second, null, 'a second claim at the same priority is refused');
  first?.release();
}

// App delivery sits between the two: it yields to a dialog, outranks the mount.
{
  resetUartLeases();
  eq(LEASE_PRIORITY['app-delivery'] > LEASE_PRIORITY['desktop-mount'], true,
     'app delivery outranks the background mount');
  eq(LEASE_PRIORITY['app-delivery'] < LEASE_PRIORITY['remote-link'], true,
     'and yields to a dialog');

  const mount = await acquireUart(2, 'desktop-mount');
  const delivery = await acquireUart(2, 'app-delivery');
  check(delivery !== null, 'app delivery preempts the mount');
  check(!mount?.active, 'the mount yielded');
  const dialog = await acquireUart(2, 'remote-link');
  check(dialog !== null, 'and a dialog preempts app delivery in turn');
  check(!delivery?.active, 'delivery yielded');
  dialog?.release();
}

// ── Taking the port back ─────────────────────────────────────────────────
// A preempted background holder needs to know when the dialog has gone, or the
// sync would stay paused until the app restarted.
{
  resetUartLeases();
  const freed: number[] = [];
  const stop = onUartFree((uart) => freed.push(uart));

  const mount = await acquireUart(2, 'desktop-mount', { onPreempt: () => {} });
  const dialog = await acquireUart(2, 'remote-link');
  check(dialog !== null, 'the dialog took the port');
  eq(freed.length, 0, 'preemption is not a free event — the port changed hands');

  dialog?.release();
  eq(freed.join(','), '2', 'releasing announces the port is free');

  const again = await acquireUart(2, 'desktop-mount');
  check(again !== null, 'the background holder can take it back');
  eq(uartHolder(2), 'desktop-mount', 'and is the holder again');
  check(!mount?.active, 'the original preempted lease stays dead');
  stop();
  again?.release();
}

// Releasing twice, or after preemption, must be harmless — teardown paths run
// in every order and a double release must not free someone else's port.
{
  resetUartLeases();
  const mount = await acquireUart(2, 'desktop-mount', { onPreempt: () => {} });
  const dialog = await acquireUart(2, 'remote-link');
  mount?.release();          // already preempted
  eq(uartHolder(2), 'remote-link',
     "a preempted holder's late release does not free the new owner's port");
  dialog?.release();
  dialog?.release();
  eq(uartHolder(2), null, 'a double release is harmless');
}

if (failures) {
  console.error(`\n${failures} assertion(s) failed`);
  process.exit(1);
}
console.log('uartLease.test.mts: all assertions passed');
