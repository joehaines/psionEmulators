// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Tests for the desktop autosave policy.
//
// Both functions fail quietly when wrong, which is why they are pure and
// why this exists. An over-eager shouldAutoSave freezes the emulated machine
// under the user's fingers every minute to write a snapshot identical to the
// last one; an over-keen planRetention deletes the generation they wanted
// back. Neither shows up as an error anywhere.
//
// Run: node --experimental-strip-types frontend/src/lib/__tests__/desktopAutoSave.test.mts

import {
  shouldAutoSave, planRetention, generationFilename,
  DEFAULT_AUTOSAVE_POLICY, DEFAULT_RETENTION,
  type AutoSaveInputs, type Generation,
} from '../desktop/autosave.ts';

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
function eqArr(actual: string[], expected: string[], msg: string) {
  const a = [...actual].sort().join(',');
  const b = [...expected].sort().join(',');
  if (a !== b) {
    console.error(`FAIL: ${msg}\n  got:  [${a}]\n  want: [${b}]`);
    failures++;
  }
}

const NOW = 1_800_000_000_000;
const MIN = 60_000;

// A machine that has been running a while, idle, with work done since the
// last save: the case that SHOULD save.
function ready(over: Partial<AutoSaveInputs> = {}): AutoSaveInputs {
  return {
    now: NOW,
    lastSaveAt: NOW - 90_000,
    lastInputAt: NOW - 10_000,
    running: true,
    loading: false,
    paused: false,
    syncInFlight: false,
    simCyclesDelta: 5_000_000,
    ...over,
  };
}

// ── shouldAutoSave ───────────────────────────────────────────────────────
{
  const d = shouldAutoSave(ready());
  eq(d.save, true, 'an idle running machine with progress saves');
  eq(d.reason, 'idle', 'and reports why');
}

// Each blocker, checked individually, so a regression names itself.
{
  eq(shouldAutoSave(ready({ running: false })).reason, 'not-running',
     'a machine that is not running does not save');
  eq(shouldAutoSave(ready({ loading: true })).reason, 'loading',
     'mid-load does not save');
  eq(shouldAutoSave(ready({ paused: true })).reason, 'paused',
     'an already-paused machine does not save');
  eq(shouldAutoSave(ready({ syncInFlight: true })).reason, 'sync-in-flight',
     'a save is deferred while a drive sync is transferring');
  for (const r of ['not-running', 'loading', 'paused', 'sync-in-flight'] as const) {
    const inputs = ready({
      running: r !== 'not-running',
      loading: r === 'loading',
      paused: r === 'paused',
      syncInFlight: r === 'sync-in-flight',
    });
    eq(shouldAutoSave(inputs).save, false, `${r} blocks the save`);
  }
}

// The interval.
{
  eq(shouldAutoSave(ready({ lastSaveAt: NOW - 59_000 })).reason, 'too-soon',
     'just under the interval does not save');
  eq(shouldAutoSave(ready({ lastSaveAt: NOW - 60_000 })).save, true,
     'exactly at the interval does save');
}

// No emulated progress: the snapshot would be identical, and a freeze for
// nothing is the worst possible trade.
{
  const d = shouldAutoSave(ready({ simCyclesDelta: 0 }));
  eq(d.reason, 'no-progress', 'a machine that has not advanced does not save');
  eq(shouldAutoSave(ready({ simCyclesDelta: 0, lastSaveAt: NOW - 10 * MIN })).reason,
     'no-progress',
     'and no-progress outranks the deferral ceiling — an identical snapshot is never worth a freeze');
}

// Idleness, and the ceiling that overrides it.
{
  eq(shouldAutoSave(ready({ lastInputAt: NOW - 500 })).reason, 'user-active',
     'mid-keystroke the save waits');
  eq(shouldAutoSave(ready({ lastInputAt: NOW - 2_500 })).save, true,
     'exactly at the idle threshold counts as idle');
  const deferred = shouldAutoSave(ready({
    lastInputAt: NOW - 100,            // still typing
    lastSaveAt: NOW - 6 * MIN,         // past the 5 min ceiling
  }));
  eq(deferred.save, true, 'past the deferral ceiling it saves even while the user types');
  eq(deferred.reason, 'max-defer', 'and says that is why');
}

// The policy is a parameter, not a constant.
{
  const brisk = { intervalMs: 1_000, idleMs: 0, maxDeferMs: 10_000 };
  eq(shouldAutoSave(ready({ lastSaveAt: NOW - 2_000, lastInputAt: NOW }), brisk).save, true,
     'a custom policy is honoured');
  eq(DEFAULT_AUTOSAVE_POLICY.intervalMs, 60_000, 'the documented default interval is 60s');
}

// ── planRetention ────────────────────────────────────────────────────────
const DAY = 24 * 3600 * 1000;

function gen(file: string, agoMs: number, bytes = 4_000_000): Generation {
  return { file, savedAt: NOW - agoMs, bytes };
}

// Nothing to do with nothing, and never delete the only copy.
{
  eqArr(planRetention([], DEFAULT_RETENTION, NOW), [], 'no generations, nothing to prune');
  eqArr(planRetention([gen('a', 0)], DEFAULT_RETENTION, NOW), [],
        'a single generation is never pruned');
  eqArr(planRetention([gen('a', 100 * DAY)], DEFAULT_RETENTION, NOW), [],
        'even a very old single generation survives — something must be restorable');
}

// The newest N are unconditional.
{
  const gens = [gen('n1', 1), gen('n2', 2), gen('n3', 3), gen('n4', 4), gen('n5', 5),
                gen('old', 400 * DAY)];
  eqArr(planRetention(gens, DEFAULT_RETENTION, NOW), ['old'],
        'the five newest are kept and the ancient one goes');
}

// Daily buckets: the newest of each day, not all of them.
{
  const gens = [
    gen('d0a', 1 * 3600_000), gen('d0b', 2 * 3600_000), gen('d0c', 3 * 3600_000),
    gen('d1a', 1 * DAY), gen('d1b', 1 * DAY + 3600_000),
    gen('d30', 30 * DAY),
  ];
  const policy = { ...DEFAULT_RETENTION, keepNewest: 1, keepWeeklyWeeks: 0 };
  const deleted = new Set(planRetention(gens, policy, NOW));
  check(!deleted.has('d0a'), 'the newest of today is kept');
  check(deleted.has('d0b') && deleted.has('d0c'), 'older same-day generations are pruned');
  check(!deleted.has('d1a'), "the newest of yesterday is kept");
  check(deleted.has('d1b'), 'the older of yesterday is pruned');
  check(deleted.has('d30'), 'beyond the daily window it is pruned');
}

// Weekly buckets extend the reach beyond the daily window.
{
  const gens = [gen('now', 1), gen('w2', 14 * DAY), gen('w10', 70 * DAY)];
  const policy = { ...DEFAULT_RETENTION, keepNewest: 1, keepDailyDays: 1, keepWeeklyWeeks: 4 };
  const deleted = new Set(planRetention(gens, policy, NOW));
  check(!deleted.has('w2'), 'a two-week-old generation is kept by the weekly rule');
  check(deleted.has('w10'), 'a ten-week-old one is outside every window');
}

// The byte cap, oldest-first — and never the newest.
{
  const big = 400_000_000;
  const gens = [gen('n1', 1, big), gen('n2', 2, big), gen('n3', 3, big),
                gen('n4', 4, big), gen('n5', 5, big)];
  // 5 x 400 MB = 2 GB against a 1 GB cap: three must go.
  const policy = { ...DEFAULT_RETENTION, maxBytes: 1_000_000_000 };
  const deleted = planRetention(gens, policy, NOW);
  check(!deleted.includes('n1'), 'the byte cap never deletes the newest');
  check(deleted.includes('n5'), 'the byte cap drops the oldest first');
  eq(deleted.length, 3, `three generations dropped to get under the cap (got ${deleted.length})`);
  const remaining = gens.filter((g) => !deleted.includes(g.file))
    .reduce((n, g) => n + g.bytes, 0);
  check(remaining <= policy.maxBytes,
        `what remains fits the cap (${remaining} <= ${policy.maxBytes})`);
}

// Input order must not matter.
{
  const gens = [gen('old', 400 * DAY), gen('new', 1), gen('mid', 2 * DAY)];
  const a = planRetention(gens, { ...DEFAULT_RETENTION, keepNewest: 1 }, NOW);
  const b = planRetention([...gens].reverse(), { ...DEFAULT_RETENTION, keepNewest: 1 }, NOW);
  eqArr(a, b, 'the result is independent of input order');
}

// ── generationFilename ───────────────────────────────────────────────────
{
  const name = generationFilename(Date.UTC(2026, 8, 19, 14, 2, 33));
  eq(name, '20260919-140233.psionst1', 'filename is UTC and compact');
  // Lexical order must match chronological order, since the manifest and the
  // directory listing are both read as sorted strings.
  const earlier = generationFilename(Date.UTC(2026, 8, 19, 9, 0, 0));
  const later = generationFilename(Date.UTC(2026, 8, 19, 14, 0, 0));
  check(earlier < later, 'filenames sort lexically in chronological order');
  const nextYear = generationFilename(Date.UTC(2027, 0, 1, 0, 0, 0));
  check(later < nextYear, 'and across a year boundary');
}

if (failures) {
  console.error(`\n${failures} assertion(s) failed`);
  process.exit(1);
}
console.log('desktopAutoSave.test.mts: all assertions passed');
