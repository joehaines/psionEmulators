// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// When to auto-save, and which generations to keep.
//
// Pure functions, separated from the controller that drives them, because
// both decisions are easy to get subtly wrong in ways nothing reports: a
// predicate that is slightly too eager rewrites an identical snapshot every
// minute and freezes the guest each time, and a retention rule that is
// slightly too keen deletes the generation the user wanted back.
//
// The costs these are balancing, from frontend/public/emulator-worker.js:
// saveState pauses the emulated machine for the whole snapshot-and-gzip.
// Compression is already off the render thread, so the thing to avoid is not
// stutter — it is freezing the guest under the user's fingers.

export interface AutoSaveInputs {
  now: number;
  /** Epoch ms of the last completed save. 0 when none this session. */
  lastSaveAt: number;
  /** Epoch ms of the last keystroke or pointer press. */
  lastInputAt: number;
  /** False while loading, erroring, or between devices. */
  running: boolean;
  loading: boolean;
  /** The machine is already paused — a save now would be redundant. */
  paused: boolean;
  /** A drive sync is mid-transfer; saving would quiesce it needlessly. */
  syncInFlight: boolean;
  /**
   * Emulated cycles executed since the last save. Zero means the guest has
   * not advanced — a switched-off machine, or one sitting at a prompt — and
   * an identical snapshot is not worth a freeze.
   */
  simCyclesDelta: number;
}

export interface AutoSavePolicy {
  /** Minimum gap between saves. */
  intervalMs: number;
  /** How long the user must have been idle before we freeze the machine. */
  idleMs: number;
  /**
   * Upper bound on deferral. Someone typing continuously never goes idle,
   * and would otherwise never be saved at all.
   */
  maxDeferMs: number;
}

export const DEFAULT_AUTOSAVE_POLICY: AutoSavePolicy = {
  intervalMs: 60_000,
  idleMs: 2_500,
  maxDeferMs: 300_000,
};

/** Why a save was or was not taken. Surfaced in the status UI and in tests. */
export type AutoSaveDecision =
  | { save: false; reason: 'not-running' | 'loading' | 'paused' | 'sync-in-flight'
      | 'too-soon' | 'no-progress' | 'user-active' }
  | { save: true; reason: 'idle' | 'max-defer' };

export function shouldAutoSave(
  i: AutoSaveInputs,
  policy: AutoSavePolicy = DEFAULT_AUTOSAVE_POLICY,
): AutoSaveDecision {
  if (!i.running) return { save: false, reason: 'not-running' };
  if (i.loading) return { save: false, reason: 'loading' };
  if (i.paused) return { save: false, reason: 'paused' };
  if (i.syncInFlight) return { save: false, reason: 'sync-in-flight' };

  const sinceSave = i.now - i.lastSaveAt;
  if (sinceSave < policy.intervalMs) return { save: false, reason: 'too-soon' };

  // Nothing has happened in the emulated machine, so the snapshot would be
  // the one we already have.
  if (i.simCyclesDelta <= 0) return { save: false, reason: 'no-progress' };

  // Past the deferral ceiling, save regardless of idleness — a long spell of
  // continuous typing is exactly the session worth not losing.
  if (sinceSave >= policy.maxDeferMs) return { save: true, reason: 'max-defer' };

  const sinceInput = i.now - i.lastInputAt;
  if (sinceInput < policy.idleMs) return { save: false, reason: 'user-active' };

  return { save: true, reason: 'idle' };
}

// ── Retention ────────────────────────────────────────────────────────────

export interface RetentionPolicy {
  /** Always kept, however old. */
  keepNewest: number;
  /** Keep the newest generation of each of the last N days. */
  keepDailyDays: number;
  /** Keep the newest generation of each of the last N weeks. */
  keepWeeklyWeeks: number;
  /** Hard cap on total bytes per device, dropping oldest-first. */
  maxBytes: number;
}

export const DEFAULT_RETENTION: RetentionPolicy = {
  keepNewest: 5,
  keepDailyDays: 7,
  keepWeeklyWeeks: 4,
  // A post-boot heap gzips to single-digit megabytes, so five generations is
  // roughly 40 MB. The cap is for the outliers — a netpad or 5mx Pro with a
  // large CF card attached.
  maxBytes: 1_500_000_000,
};

export interface Generation {
  file: string;
  savedAt: number;
  bytes: number;
}

function dayKey(ms: number): string {
  return new Date(ms).toISOString().slice(0, 10);
}

/** ISO-ish week bucket. Only needs to be stable, not calendar-perfect. */
function weekKey(ms: number): number {
  return Math.floor(ms / (7 * 24 * 3600 * 1000));
}

/**
 * Which generations to delete, newest-first input or not.
 *
 * The newest is never returned, under any policy: whatever else happens, the
 * user must be left with something to restore.
 */
export function planRetention(
  generations: Generation[],
  policy: RetentionPolicy = DEFAULT_RETENTION,
  now = Date.now(),
): string[] {
  if (generations.length <= 1) return [];

  const sorted = [...generations].sort((a, b) => b.savedAt - a.savedAt);
  const keep = new Set<string>();

  // 1. The newest few, unconditionally.
  for (const g of sorted.slice(0, Math.max(1, policy.keepNewest))) keep.add(g.file);

  // 2. The newest of each recent day, then of each recent week. Walking the
  //    sorted list and taking the first hit per bucket is what makes these
  //    "newest per period" rather than "any per period".
  const dayCutoff = now - policy.keepDailyDays * 24 * 3600 * 1000;
  const seenDays = new Set<string>();
  for (const g of sorted) {
    if (g.savedAt < dayCutoff) break;
    const k = dayKey(g.savedAt);
    if (!seenDays.has(k)) { seenDays.add(k); keep.add(g.file); }
  }

  const weekCutoff = now - policy.keepWeeklyWeeks * 7 * 24 * 3600 * 1000;
  const seenWeeks = new Set<number>();
  for (const g of sorted) {
    if (g.savedAt < weekCutoff) break;
    const k = weekKey(g.savedAt);
    if (!seenWeeks.has(k)) { seenWeeks.add(k); keep.add(g.file); }
  }

  // 3. The byte cap, applied to what survived, dropping oldest-first. The
  //    newest is exempt even here.
  let total = 0;
  const overCap = new Set<string>();
  for (const g of sorted) {
    if (!keep.has(g.file)) continue;
    total += g.bytes;
  }
  if (total > policy.maxBytes) {
    const kept = sorted.filter((g) => keep.has(g.file));
    // Oldest first, never touching index 0 of `sorted`.
    for (let i = kept.length - 1; i >= 1 && total > policy.maxBytes; i--) {
      overCap.add(kept[i].file);
      total -= kept[i].bytes;
    }
  }

  return sorted
    .filter((g, index) => index !== 0 && (!keep.has(g.file) || overCap.has(g.file)))
    .map((g) => g.file);
}

/** Generation filename for a moment in time: UTC, so it sorts lexically. */
export function generationFilename(savedAt: number): string {
  const iso = new Date(savedAt).toISOString();       // 2026-09-19T14:02:33.123Z
  return `${iso.slice(0, 10).replace(/-/g, '')}-${iso.slice(11, 19).replace(/:/g, '')}.psionst1`;
}
