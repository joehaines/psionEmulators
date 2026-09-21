// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The two-way mirror between a host folder and the machine's own drive.
//
// Pure, with both filesystems injected, so the integration test can drive it
// against a real ROM in the native harness and this file never has to know what
// a PlpClient is.
//
// ── Why a three-way compare ──
//
// Comparing the two sides alone cannot tell a deletion from an absence. If a
// file is on the device and not on the host, either the user deleted it here or
// it has simply never been downloaded — and guessing wrong either re-downloads
// what they deleted or deletes what they had not yet received. So every
// decision is made against a third leg: what the last completed sync left.
//
// ── What this is honestly not ──
//
// Not a mounted filesystem. The transport is an emulated UART at 115200 baud,
// minus byte-stuffing and framing, with one round-trip per 1 KB (2 KB on
// SA-1100) and a full EPOC thread wake per round-trip. Expect single-digit
// KB/s. Everything here is shaped around that: the plan is ordered
// smallest-first, listings are the expensive part, and the caller is expected
// to present it as "synced a moment ago" rather than as a drive letter.

import {
  DEFAULT_MIRROR_POLICY, backoffUntil,
  type DeviceFile, type DeviceFs, type HostFile, type HostFs,
  type MirrorPolicy, type MirrorState, type MirrorStep,
} from './types.ts';

/** Join a device directory and a name the way EPOC writes paths. */
export function deviceJoin(dir: string, name: string): string {
  const base = dir.endsWith('\\') ? dir : `${dir}\\`;
  return base + name;
}

/** The device path relative to the mirror root, POSIX-separated. */
export function deviceRelative(root: string, devPath: string): string {
  const base = root.endsWith('\\') ? root : `${root}\\`;
  const tail = devPath.toUpperCase().startsWith(base.toUpperCase())
    ? devPath.slice(base.length)
    : devPath;
  return tail.replace(/\\/g, '/');
}

export interface PlanInput {
  hostFiles: HostFile[];
  deviceFiles: DeviceFile[];
  state: MirrorState;
  /** Maps a host relative path to a device filename. Supplied by names.ts. */
  toDeviceName(hostRel: string): string;
  /** Maps a device path back to a host relative path, for files we did not put there. */
  toHostName(devRel: string): string;
  policy?: MirrorPolicy;
  now?: number;
}

export interface Plan {
  steps: MirrorStep[];
  warnings: string[];
  /** True when the plan was refused. `refusedBecause` says why. */
  refused: boolean;
  refusedBecause?: 'too-many-files' | 'mass-deletion';
}

/**
 * Decide what to move, in which direction.
 *
 * The truth table, host-vs-last × device-vs-last:
 *
 *   new / absent          → upload
 *   absent / new          → download
 *   changed / unchanged   → upload
 *   unchanged / changed   → download
 *   changed / changed     → CONFLICT (never a silent overwrite)
 *   deleted / unchanged   → delete on the device
 *   unchanged / deleted   → delete on the host
 *   deleted / deleted     → forget it
 *   deleted / changed     → download — an edit outranks a delete
 *   changed / deleted     → upload  — likewise, the other way
 *
 * The last two are the ones worth stating: resurrecting beats honouring a
 * deletion, because a file that comes back is an annoyance and a file that is
 * gone is lost work.
 */
export function planMirror(input: PlanInput): Plan {
  const policy = input.policy ?? DEFAULT_MIRROR_POLICY;
  const now = input.now ?? Date.now();
  const { state } = input;

  const excluded = new Set(policy.excludeDirs.map((d) => d.toUpperCase()));

  const hostByRel = new Map<string, HostFile>();
  for (const f of input.hostFiles) hostByRel.set(f.rel, f);

  // Device files, keyed by the host path they correspond to. A file we put
  // there is found through the recorded name map; one the device created gets a
  // host name invented for it.
  const deviceByRel = new Map<string, { file: DeviceFile; devRel: string }>();
  const devPathToHost = new Map<string, string>();
  for (const [hostRel, devPath] of Object.entries(state.names)) {
    devPathToHost.set(devPath.toUpperCase(), hostRel);
  }

  for (const f of input.deviceFiles) {
    if (f.isDir) continue;
    const devRel = deviceRelative(state.root, f.path);
    const topLevel = devRel.split('/')[0]?.toUpperCase() ?? '';
    if (excluded.has(topLevel)) continue;
    const known = devPathToHost.get(f.path.toUpperCase());
    const rel = known ?? input.toHostName(devRel);
    deviceByRel.set(rel, { file: f, devRel });
  }

  const steps: MirrorStep[] = [];
  const warnings: string[] = [];
  const allRels = new Set([...hostByRel.keys(), ...deviceByRel.keys(),
                           ...Object.keys(state.entries)]);

  const backedOff: string[] = [];

  for (const rel of allRels) {
    // A file the device refuses to open — one it has open itself, typically —
    // is left alone until its backoff expires. Retrying every cycle would cost
    // a round-trip each time on a link with none to spare.
    const skip = state.skips[rel];
    if (skip && skip.until > now) { backedOff.push(rel); continue; }

    const host = hostByRel.get(rel);
    const device = deviceByRel.get(rel);
    const last = state.entries[rel];
    const devPath = state.names[rel]
      ?? (device ? device.file.path : deviceJoin(state.root, input.toDeviceName(rel)));

    // Neither side has it, and we remember it: the record is stale.
    if (!host && !device) {
      if (last) steps.push({ op: 'forget', rel, devPath, bytes: 0 });
      continue;
    }

    if (host && !device) {
      if (!last) {
        steps.push({ op: 'upload', rel, devPath, bytes: host.size });
      } else if (changedOnHost(host, last)) {
        // Edited here, deleted there: the edit wins.
        steps.push({ op: 'upload', rel, devPath, bytes: host.size });
      } else {
        // Untouched here, deleted on the device: honour it.
        steps.push({ op: 'delete-host', rel, devPath, bytes: 0 });
      }
      continue;
    }

    if (!host && device) {
      if (!last) {
        steps.push({ op: 'download', rel, devPath: device.file.path, bytes: device.file.size });
      } else if (changedOnDevice(device.file, last)) {
        // Deleted here, edited there: the edit wins.
        steps.push({ op: 'download', rel, devPath: device.file.path, bytes: device.file.size });
      } else {
        steps.push({ op: 'delete-device', rel, devPath: device.file.path, bytes: 0 });
      }
      continue;
    }

    // Both sides have it.
    if (!host || !device) continue;
    const hostChanged = !last || changedOnHost(host, last);
    const deviceChanged = !last || changedOnDevice(device.file, last);

    if (!hostChanged && !deviceChanged) continue;

    if (hostChanged && deviceChanged) {
      // Newer wins; a tie goes to the device, because the user was more likely
      // just typing on the Psion than touching the folder. The loser is kept —
      // written to the HOST side, where there is room for a long filename and
      // no transfer to pay for.
      const winner: 'host' | 'device' =
        device.file.mtime > host.mtimeMs ? 'device'
        : host.mtimeMs > device.file.mtime ? 'host'
        : 'device';
      steps.push({
        op: 'conflict', rel, devPath: device.file.path,
        bytes: winner === 'device' ? device.file.size : host.size,
        winner,
        loserAs: conflictName(rel, winner === 'device' ? 'host' : 'device', now),
      });
      continue;
    }

    if (hostChanged) steps.push({ op: 'upload', rel, devPath, bytes: host.size });
    else steps.push({ op: 'download', rel, devPath: device.file.path, bytes: device.file.size });
  }

  // Smallest transfers first, so "I dropped a 4 KB note in" completes in a
  // second rather than queueing behind half a megabyte. Metadata-only steps
  // (deletes, forgets) sort to the front — they cost nothing and finishing them
  // keeps the state tidy if a later transfer fails.
  steps.sort((a, b) => a.bytes - b.bytes);

  const transfers = steps.filter((s) => s.bytes > 0);
  const totalBytes = transfers.reduce((n, s) => n + s.bytes, 0);

  if (steps.length > policy.maxFiles) {
    return {
      steps: [],
      refused: true,
      refusedBecause: 'too-many-files',
      warnings: [
        `${steps.length} files would have to be synced, over the ${policy.maxFiles} limit. ` +
        'This is a folder sync over an emulated serial cable, not a disk — ' +
        'point it at a smaller folder.',
      ],
    };
  }

  // The mass-deletion catch. Learnt the hard way: a device listing that came
  // back empty (RFSV wants a trailing separator on the directory, and without
  // it reports nothing) read as "the user deleted everything", and the engine
  // duly deleted their host files. Nothing about the plan looked wrong.
  //
  // So bulk deletion is treated as evidence of a fault rather than intent. The
  // cost of being wrong in this direction is one extra cycle; in the other, it
  // is the user's documents.
  const tracked = Object.keys(state.entries).length;
  const deletions = steps.filter((s) => s.op === 'delete-host' || s.op === 'delete-device');
  const allowedDeletes = Math.max(policy.maxDeleteFloor,
                                  Math.floor(tracked * policy.maxDeleteFraction));
  if (tracked > 0 && deletions.length > allowedDeletes) {
    return {
      steps: [],
      refused: true,
      refusedBecause: 'mass-deletion',
      warnings: [
        `${deletions.length} of ${tracked} tracked files appear to have been deleted, ` +
        'which is more likely a link or path problem than something you did. ' +
        'Nothing has been changed — check the folder and the cable, then sync again.',
      ],
    };
  }
  if (steps.length > policy.warnFiles) {
    warnings.push(`${steps.length} files to sync; over a serial link this will take a while.`);
  }
  if (totalBytes > policy.warnBytes) {
    warnings.push(`${Math.round(totalBytes / 1024)} KB to transfer at roughly 5 KB/s.`);
  }

  if (backedOff.length) {
    warnings.push(
      `${backedOff.length} file(s) skipped for now: ` +
      backedOff.slice(0, 3).map((r) => `${r} (${state.skips[r]?.reason ?? 'unknown'})`).join(', ') +
      (backedOff.length > 3 ? `, and ${backedOff.length - 3} more` : ''));
  }

  return { steps, warnings, refused: false };
}

function changedOnHost(host: HostFile, last: { hostSize: number; hostMtimeMs: number }): boolean {
  if (host.size !== last.hostSize) return true;
  // A second of slack: host filesystems and our own write-back stamp do not
  // agree to the millisecond.
  return Math.abs(host.mtimeMs - last.hostMtimeMs) > 1000;
}

function changedOnDevice(device: DeviceFile, last: { devSize: number; devMtime: number }): boolean {
  if (device.size !== last.devSize) return true;
  // EPOC's timestamps have two-second granularity.
  return Math.abs(device.mtime - last.devMtime) > 2000;
}

/**
 * Where a conflict's loser goes.
 *
 * Always on the host side and always alongside the winner, never overwritten
 * and never left only on the device: 8.3 name space is scarce over there and a
 * second copy would cost a transfer at 5 KB/s for nothing.
 */
export function conflictName(rel: string, loser: 'host' | 'device', now: number): string {
  const stamp = new Date(now).toISOString().slice(0, 16).replace('T', ' ');
  const dot = rel.lastIndexOf('.');
  const slash = rel.lastIndexOf('/');
  const hasExt = dot > slash;
  const stem = hasExt ? rel.slice(0, dot) : rel;
  const ext = hasExt ? rel.slice(dot) : '';
  const which = loser === 'device' ? 'from device' : 'from this computer';
  return `${stem} (${which} ${stamp})${ext}`;
}

export interface ApplyHooks {
  onStep?(step: MirrorStep, index: number, total: number): void;
  onBytes?(done: number, total: number): void;
  /** Return true to stop after the current step. */
  shouldAbort?(): boolean;
  /**
   * Clock, injectable so it agrees with the one planMirror was given.
   *
   * They must be the same clock: the backoff this writes is compared against
   * the plan's `now`, and if the two disagree a skip is either ignored
   * immediately or honoured forever.
   */
  now?(): number;
}

export interface ApplyResult {
  state: MirrorState;
  failed: { step: MirrorStep; error: string }[];
  completed: number;
  aborted: boolean;
}

/**
 * Carry out a plan.
 *
 * State is advanced per step, not at the end. A transfer over this link can
 * fail halfway through a plan for reasons that have nothing to do with the
 * files — a dialog claiming the port, the device's link server wedging — and a
 * mirror that forgot everything it had just done would redo the whole folder on
 * the next attempt, at 5 KB/s.
 */
export async function applyMirror(
  plan: MirrorStep[],
  host: HostFs,
  device: DeviceFs,
  state: MirrorState,
  hooks: ApplyHooks = {},
): Promise<ApplyResult> {
  const next: MirrorState = {
    version: 1, root: state.root,
    names: { ...state.names },
    entries: { ...state.entries },
    skips: { ...state.skips },
  };
  const failed: ApplyResult['failed'] = [];
  const clock = hooks.now ?? Date.now;
  const totalBytes = plan.reduce((n, s) => n + s.bytes, 0);
  let movedBytes = 0;
  let completed = 0;
  let aborted = false;

  for (let i = 0; i < plan.length; i++) {
    if (hooks.shouldAbort?.()) { aborted = true; break; }
    const step = plan[i];
    hooks.onStep?.(step, i, plan.length);

    try {
      switch (step.op) {
        case 'upload': {
          const bytes = await host.read(step.rel);
          await ensureDeviceDir(device, step.devPath);
          await device.write(step.devPath, bytes);
          const listed = await statDevice(device, step.devPath);
          next.names[step.rel] = step.devPath;
          next.entries[step.rel] = {
            hostSize: bytes.byteLength,
            hostMtimeMs: await hostMtime(host, step.rel, clock()),
            devSize: listed?.size ?? bytes.byteLength,
            devMtime: listed?.mtime ?? clock(),
            syncedAt: clock(),
          };
          break;
        }
        case 'download': {
          const bytes = await device.read(step.devPath);
          await host.write(step.rel, bytes);
          const listed = await statDevice(device, step.devPath);
          next.names[step.rel] = step.devPath;
          next.entries[step.rel] = {
            hostSize: bytes.byteLength,
            hostMtimeMs: await hostMtime(host, step.rel, clock()),
            devSize: listed?.size ?? bytes.byteLength,
            devMtime: listed?.mtime ?? clock(),
            syncedAt: clock(),
          };
          break;
        }
        case 'delete-device': {
          await device.remove(step.devPath);
          delete next.entries[step.rel];
          delete next.names[step.rel];
          break;
        }
        case 'delete-host': {
          await host.remove(step.rel);
          delete next.entries[step.rel];
          delete next.names[step.rel];
          break;
        }
        case 'mkdir-device': {
          await device.mkdirAll(step.devPath);
          break;
        }
        case 'conflict': {
          // Keep both. The loser is written to the host under a name that says
          // where it came from and when, so nothing is destroyed and the user
          // can see at a glance which is which.
          if (step.winner === 'device') {
            const deviceBytes = await device.read(step.devPath);
            const hostBytes = await host.read(step.rel).catch(() => null);
            if (hostBytes && step.loserAs) await host.write(step.loserAs, hostBytes);
            await host.write(step.rel, deviceBytes);
          } else {
            const hostBytes = await host.read(step.rel);
            const deviceBytes = await device.read(step.devPath).catch(() => null);
            if (deviceBytes && step.loserAs) await host.write(step.loserAs, deviceBytes);
            await ensureDeviceDir(device, step.devPath);
            await device.write(step.devPath, hostBytes);
          }
          const listed = await statDevice(device, step.devPath);
          next.names[step.rel] = step.devPath;
          next.entries[step.rel] = {
            hostSize: (await host.list()).find((f) => f.rel === step.rel)?.size ?? 0,
            hostMtimeMs: await hostMtime(host, step.rel, clock()),
            devSize: listed?.size ?? 0,
            devMtime: listed?.mtime ?? clock(),
            syncedAt: clock(),
          };
          break;
        }
        case 'forget': {
          delete next.entries[step.rel];
          delete next.names[step.rel];
          break;
        }
      }
      completed++;
      movedBytes += step.bytes;
      hooks.onBytes?.(movedBytes, totalBytes);
      // A step that worked clears any backoff — the document has been closed.
      delete next.skips[step.rel];
    } catch (err) {
      const error = err instanceof Error ? err.message : String(err);
      failed.push({ step, error });
      const attempts = (next.skips[step.rel]?.attempts ?? 0) + 1;
      next.skips[step.rel] = {
        attempts,
        reason: describeFailure(error),
        until: backoffUntil(attempts, clock()),
      };
    }
  }

  return { state: next, failed, completed, aborted };
}

/**
 * Turn an RFSV error into something a person can act on.
 *
 * KErrInUse is the one worth naming: it is what a real machine answers for a
 * document it has open, which on a freshly booted 5mx includes the stock Word
 * and Sheet files sitting in C:\\Documents. Reported as "in use on the device"
 * it is obviously not a bug; reported as "status -14" it looks like one.
 */
export function describeFailure(error: string): string {
  if (/-14\b/.test(error)) return 'in use on the device';
  if (/-21\b/.test(error)) return 'access denied on the device';
  if (/-1\b/.test(error)) return 'not found on the device';
  if (/-12\b/.test(error)) return 'path not found on the device';
  if (/timed? ?out/i.test(error)) return 'the link timed out';
  return error.slice(0, 120);
}

async function ensureDeviceDir(device: DeviceFs, devPath: string): Promise<void> {
  const cut = devPath.lastIndexOf('\\');
  if (cut <= 0) return;
  const dir = devPath.slice(0, cut);
  // Cheap and idempotent on every ROM we talk to; cheaper than a listing to
  // find out whether it was needed.
  await device.mkdirAll(dir).catch(() => undefined);
}

async function statDevice(device: DeviceFs, devPath: string) {
  const cut = devPath.lastIndexOf('\\');
  const dir = cut > 0 ? devPath.slice(0, cut) : devPath;
  const name = devPath.slice(cut + 1).toUpperCase();
  const listed = await device.list(dir).catch(() => [] as DeviceFile[]);
  return listed.find((f) => f.path.slice(f.path.lastIndexOf('\\') + 1).toUpperCase() === name);
}

async function hostMtime(host: HostFs, rel: string, fallback: number): Promise<number> {
  const found = (await host.list().catch(() => [] as HostFile[])).find((f) => f.rel === rel);
  return found?.mtimeMs ?? fallback;
}
