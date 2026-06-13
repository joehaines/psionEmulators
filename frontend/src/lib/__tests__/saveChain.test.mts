// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Regression test for the state-save chain pattern used in useEmulator.ts.
//
// The hook's previous implementation used a `savingRef.current` boolean
// to drop concurrent saves; if a fire-and-forget save was in flight when
// the user clicked "Save State", the user's save was silently swallowed
// and IDB only ever contained the older snapshot. The replacement uses a
// Promise chain so every call runs to completion sequentially. This file
// pins that behaviour with a small simulator.
//
// Run via:
//   node --experimental-strip-types frontend/src/lib/__tests__/saveChain.test.mts

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}

// Mirrors useEmulator.ts's triggerSave: each call appends to a single
// chain, and errors are caught between segments so they don't poison
// downstream saves.
function makeChain() {
  let chain: Promise<void> | null = null;
  return {
    enqueue(work: () => Promise<void>): Promise<void> {
      const next = (chain ?? Promise.resolve())
        .catch(() => undefined)
        .then(() => work());
      chain = next;
      return next;
    },
  };
}

// ── all queued saves run, sequentially, in order ───────────────────────
{
  const { enqueue } = makeChain();
  const order: number[] = [];
  let inflight = 0;
  let maxInflight = 0;

  async function fakeSave(id: number, delay: number): Promise<void> {
    inflight++;
    maxInflight = Math.max(maxInflight, inflight);
    await new Promise(r => setTimeout(r, delay));
    order.push(id);
    inflight--;
  }

  // Fire 5 saves rapid-fire (the scenario where a cold-boot save is
  // already in flight when the user mashes Save State).
  const promises = [
    enqueue(() => fakeSave(1, 30)),
    enqueue(() => fakeSave(2, 5)),
    enqueue(() => fakeSave(3, 20)),
    enqueue(() => fakeSave(4, 10)),
    enqueue(() => fakeSave(5, 0)),
  ];
  await Promise.all(promises);

  check(order.length === 5, 'all 5 saves completed');
  check(JSON.stringify(order) === '[1,2,3,4,5]', `saves executed in submission order, got ${JSON.stringify(order)}`);
  check(maxInflight === 1, `serial execution (maxInflight=${maxInflight})`);
}

// ── one failing save does not poison the rest ─────────────────────────
{
  const { enqueue } = makeChain();
  const completed: string[] = [];
  let rejected = 0;

  async function fakeOk(label: string): Promise<void> {
    await new Promise(r => setTimeout(r, 5));
    completed.push(label);
  }
  async function fakeFail(): Promise<void> {
    await new Promise(r => setTimeout(r, 5));
    throw new Error('synthetic failure');
  }

  const ok1 = enqueue(() => fakeOk('a')).catch(() => { rejected++; });
  const bad = enqueue(() => fakeFail()).catch(() => { rejected++; });
  const ok2 = enqueue(() => fakeOk('b')).catch(() => { rejected++; });
  const ok3 = enqueue(() => fakeOk('c')).catch(() => { rejected++; });
  await Promise.all([ok1, bad, ok2, ok3]);

  check(completed.length === 3, '3 ok saves completed');
  check(JSON.stringify(completed) === '["a","b","c"]', `ok saves in order, got ${JSON.stringify(completed)}`);
  check(rejected === 1, `exactly one rejection surfaced, got ${rejected}`);
}

// ── callers that await get a promise resolving for THEIR save ─────────
{
  const { enqueue } = makeChain();
  const order: string[] = [];

  async function step(label: string, delay: number): Promise<void> {
    await new Promise(r => setTimeout(r, delay));
    order.push(label);
  }

  const p1 = enqueue(() => step('first', 30));
  const p2 = enqueue(() => step('second', 5));
  // p1 should resolve before "second" runs.
  await p1;
  check(order.length === 1 && order[0] === 'first', 'first save resolves before second begins');
  await p2;
  check(order.length === 2 && order[1] === 'second', 'second save resolves after first');
}

if (failures > 0) {
  console.error(`\n${failures} test(s) failed`);
  process.exit(1);
}
console.log('saveChain tests passed');
