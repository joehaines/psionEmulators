// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Tests for the internal-drive mirror.
//
// The three-way compare is the whole reason this engine can propagate deletions
// at all, and every cell of its truth table is a decision about someone's
// files. Two of those cells destroy data if they are wrong — deleting because a
// file was merely never downloaded, or overwriting because two edits looked like
// one — so all ten are asserted individually rather than through a couple of
// happy paths.
//
// Run: node --experimental-strip-types frontend/src/lib/__tests__/hostsync.mirror.test.mts

import {
  applyMirror, conflictName, deviceJoin, deviceRelative, planMirror,
} from '../hostsync/mirror.ts';
import {
  createMirrorState, adoptMirrorState, backoffUntil, DEFAULT_MIRROR_POLICY,
  type DeviceFile, type DeviceFs, type HostFile, type HostFs, type MirrorState,
} from '../hostsync/types.ts';
import { describeFailure } from '../hostsync/mirror.ts';

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

const ROOT = 'C:\\Documents';
const T0 = 1_800_000_000_000;
const enc = (s: string) => new TextEncoder().encode(s);
const dec = (b: Uint8Array) => new TextDecoder().decode(b);

// Simple, deterministic name mapping — nameIndex is tested separately.
const toDeviceName = (rel: string) => rel.toUpperCase().replace(/\//g, '\\');
const toHostName = (devRel: string) => devRel.toLowerCase();

function hostFile(rel: string, size: number, mtimeMs: number): HostFile {
  return { rel, size, mtimeMs };
}
function devFile(name: string, size: number, mtime: number): DeviceFile {
  return { path: deviceJoin(ROOT, name), size, mtime, isDir: false };
}

/** State describing one file that was synced cleanly at T0. */
function syncedState(rel: string, size: number, mtime: number): MirrorState {
  const state = createMirrorState(ROOT);
  state.names[rel] = deviceJoin(ROOT, toDeviceName(rel));
  state.entries[rel] = {
    hostSize: size, hostMtimeMs: mtime,
    devSize: size, devMtime: mtime,
    syncedAt: T0,
  };
  return state;
}

function plan(hostFiles: HostFile[], deviceFiles: DeviceFile[], state: MirrorState) {
  return planMirror({ hostFiles, deviceFiles, state, toDeviceName, toHostName, now: T0 });
}
function opsOf(p: { steps: { op: string; rel: string }[] }) {
  return p.steps.map((s) => `${s.op}:${s.rel}`).sort().join(',');
}

// ── Path helpers ─────────────────────────────────────────────────────────
{
  eq(deviceJoin('C:\\Documents', 'A.TXT'), 'C:\\Documents\\A.TXT', 'joins with a backslash');
  eq(deviceJoin('C:\\Documents\\', 'A.TXT'), 'C:\\Documents\\A.TXT',
     'does not double the separator');
  eq(deviceRelative(ROOT, 'C:\\Documents\\SUB\\A.TXT'), 'SUB/A.TXT',
     'relative paths come back POSIX-separated');
  eq(deviceRelative(ROOT, 'C:\\DOCUMENTS\\A.TXT'), 'A.TXT',
     'the root match is case-insensitive, as the filesystem is');
}

// ── The truth table ──────────────────────────────────────────────────────

// new / absent → upload
{
  const p = plan([hostFile('a.txt', 10, T0)], [], createMirrorState(ROOT));
  eq(opsOf(p), 'upload:a.txt', 'a file only on the host is uploaded');
}

// absent / new → download
{
  const p = plan([], [devFile('A.TXT', 10, T0)], createMirrorState(ROOT));
  eq(opsOf(p), 'download:a.txt', 'a file only on the device is downloaded');
}

// unchanged / unchanged → nothing
{
  const p = plan([hostFile('a.txt', 10, T0)], [devFile('A.TXT', 10, T0)],
                 syncedState('a.txt', 10, T0));
  eq(p.steps.length, 0, 'an untouched pair produces no work');
}

// changed / unchanged → upload
{
  const p = plan([hostFile('a.txt', 20, T0 + 60_000)], [devFile('A.TXT', 10, T0)],
                 syncedState('a.txt', 10, T0));
  eq(opsOf(p), 'upload:a.txt', 'a host-side edit uploads');
}

// unchanged / changed → download
{
  const p = plan([hostFile('a.txt', 10, T0)], [devFile('A.TXT', 30, T0 + 60_000)],
                 syncedState('a.txt', 10, T0));
  eq(opsOf(p), 'download:a.txt', 'a device-side edit downloads');
}

// changed / changed → conflict, never a silent overwrite
{
  const p = plan([hostFile('a.txt', 20, T0 + 30_000)],
                 [devFile('A.TXT', 30, T0 + 60_000)],
                 syncedState('a.txt', 10, T0));
  eq(opsOf(p), 'conflict:a.txt', 'two edits are a conflict, not an overwrite');
  eq(p.steps[0].winner, 'device', 'the newer side wins');
  check(!!p.steps[0].loserAs, 'and the loser is given somewhere to go');
  check(p.steps[0].loserAs?.includes('from this computer'),
        `the loser's name says where it came from (${p.steps[0].loserAs})`);
}
{
  const p = plan([hostFile('a.txt', 20, T0 + 90_000)],
                 [devFile('A.TXT', 30, T0 + 60_000)],
                 syncedState('a.txt', 10, T0));
  eq(p.steps[0].winner, 'host', 'a newer host copy wins instead');
  check(p.steps[0].loserAs?.includes('from device'), 'and the device copy is kept');
}
{
  // A tie goes to the device: the user was more likely typing on the Psion.
  const p = plan([hostFile('a.txt', 20, T0 + 60_000)],
                 [devFile('A.TXT', 30, T0 + 60_000)],
                 syncedState('a.txt', 10, T0));
  eq(p.steps[0].winner, 'device', 'a tie goes to the device');
}

// deleted / unchanged → delete on the device
{
  const p = plan([], [devFile('A.TXT', 10, T0)], syncedState('a.txt', 10, T0));
  eq(opsOf(p), 'delete-device:a.txt', 'a host deletion propagates to the device');
}

// unchanged / deleted → delete on the host
{
  const p = plan([hostFile('a.txt', 10, T0)], [], syncedState('a.txt', 10, T0));
  eq(opsOf(p), 'delete-host:a.txt', 'a device deletion propagates to the host');
}

// deleted / deleted → forget
{
  const p = plan([], [], syncedState('a.txt', 10, T0));
  eq(opsOf(p), 'forget:a.txt', 'gone from both sides, the record is dropped');
}

// deleted / changed → download. An edit outranks a deletion, both ways: a file
// that reappears is an annoyance, a file that is gone is lost work.
{
  const p = plan([], [devFile('A.TXT', 30, T0 + 60_000)], syncedState('a.txt', 10, T0));
  eq(opsOf(p), 'download:a.txt',
     'a device edit resurrects a file deleted on the host');
}

// changed / deleted → upload
{
  const p = plan([hostFile('a.txt', 20, T0 + 60_000)], [], syncedState('a.txt', 10, T0));
  eq(opsOf(p), 'upload:a.txt',
     'a host edit resurrects a file deleted on the device');
}

// ── Change detection tolerances ──────────────────────────────────────────
{
  // EPOC's timestamps have two-second granularity, so a small difference is not
  // a change — otherwise every cycle would re-transfer every file forever.
  const p = plan([hostFile('a.txt', 10, T0)], [devFile('A.TXT', 10, T0 + 1500)],
                 syncedState('a.txt', 10, T0));
  eq(p.steps.length, 0, 'a sub-granularity timestamp difference is not a change');

  const beyond = plan([hostFile('a.txt', 10, T0)], [devFile('A.TXT', 10, T0 + 5000)],
                      syncedState('a.txt', 10, T0));
  eq(opsOf(beyond), 'download:a.txt', 'but a real timestamp move is');

  // Size always wins, whatever the timestamps say.
  const resized = plan([hostFile('a.txt', 11, T0)], [devFile('A.TXT', 10, T0)],
                       syncedState('a.txt', 10, T0));
  eq(opsOf(resized), 'upload:a.txt', 'a size change is always a change');
}

// ── Exclusions ───────────────────────────────────────────────────────────
{
  // C:\Documents\System would be the OS. Never mirrored, never deletable from
  // the host side, not configurable.
  const p = plan([], [
    devFile('SYSTEM\\APPS\\WORD.APP', 100, T0),
    devFile('SYS\\BIN\\THING', 100, T0),
    devFile('A.TXT', 10, T0),
  ], createMirrorState(ROOT));
  eq(opsOf(p), 'download:a.txt', 'excluded device directories are ignored entirely');
}

// ── Size limits ──────────────────────────────────────────────────────────
{
  const many = Array.from({ length: DEFAULT_MIRROR_POLICY.maxFiles + 10 },
                          (_, i) => hostFile(`f${i}.txt`, 10, T0));
  const p = plan(many, [], createMirrorState(ROOT));
  check(p.refused, 'an oversized plan is refused rather than started');
  eq(p.steps.length, 0, 'and nothing is attempted');
  check(p.warnings[0]?.includes('serial'),
        'the refusal explains this is a serial link, not a disk');
}
{
  const some = Array.from({ length: DEFAULT_MIRROR_POLICY.warnFiles + 5 },
                          (_, i) => hostFile(`f${i}.txt`, 10, T0));
  const p = plan(some, [], createMirrorState(ROOT));
  check(!p.refused, 'a merely large plan is allowed');
  check(p.warnings.length > 0, 'but warns');
}
{
  const p = plan([hostFile('big.bin', 4_000_000, T0)], [], createMirrorState(ROOT));
  check(p.warnings.some((w) => w.includes('KB')), 'a large transfer warns about the time');
}

// ── The mass-deletion catch ──────────────────────────────────────────────
//
// This exists because of a real failure. RFSV's OPEN_DIR wants a trailing
// separator on the directory; without one it reports an empty folder. The
// engine read that as "the user deleted everything" and deleted their host
// files. Nothing about the plan looked wrong at any point.
//
// So a plan that deletes most of what the mirror tracks is now treated as
// evidence of a fault rather than intent. Being wrong this way costs one extra
// cycle; being wrong the other way costs the user's documents.
{
  const state = createMirrorState(ROOT);
  const names = ['a.txt', 'b.txt', 'c.txt', 'd.txt', 'e.txt', 'f.txt'];
  for (const rel of names) {
    state.names[rel] = deviceJoin(ROOT, toDeviceName(rel));
    state.entries[rel] = {
      hostSize: 10, hostMtimeMs: T0, devSize: 10, devMtime: T0, syncedAt: T0,
    };
  }
  const hostFiles = names.map((rel) => hostFile(rel, 10, T0));

  // The exact failure: the device listing comes back empty.
  const emptied = plan(hostFiles, [], state);
  check(emptied.refused, 'an empty device listing is refused, not obeyed');
  eq(emptied.refusedBecause, 'mass-deletion', 'and says why');
  eq(emptied.steps.length, 0, 'nothing is attempted');
  check(emptied.warnings[0]?.includes('link or path'),
        'the warning points at the likely cause rather than blaming the user');

  // The same in the other direction: the HOST folder reads as empty, which is
  // what an unmounted drive or a renamed folder looks like.
  const hostGone = plan([], names.map((rel) => devFile(toDeviceName(rel), 10, T0)), state);
  check(hostGone.refused, 'an empty host listing is refused too');
  eq(hostGone.refusedBecause, 'mass-deletion', 'for the same reason');

  // A genuine tidy-up of a few files must still go through, or the guard would
  // make ordinary use impossible. Two removed from the host while the device
  // still has all six: that is a real deletion to propagate.
  const allOnDevice = names.map((rel) => devFile(toDeviceName(rel), 10, T0));
  const twoGone = plan(hostFiles.slice(0, 4), allOnDevice, state);
  check(!twoGone.refused, 'deleting two of six is ordinary and allowed');
  eq(twoGone.steps.filter((s) => s.op === 'delete-device').length, 2,
     'and both deletions are planned');

  // Files absent from BOTH sides are simply forgotten — not deletions, and so
  // not counted against the guard.
  const bothGone = plan(hostFiles.slice(0, 4), allOnDevice.slice(0, 4), state);
  check(!bothGone.refused, 'records for files gone from both sides do not trip the guard');
  eq(bothGone.steps.filter((s) => s.op === 'forget').length, 2,
     'they are forgotten instead');
  eq(bothGone.steps.filter((s) => s.op.startsWith('delete')).length, 0,
     'with nothing deleted anywhere');
}
{
  // On a small mirror the floor applies rather than the fraction: deleting two
  // of three files is half the mirror but is obviously fine.
  const state = createMirrorState(ROOT);
  for (const rel of ['a.txt', 'b.txt', 'c.txt']) {
    state.names[rel] = deviceJoin(ROOT, toDeviceName(rel));
    state.entries[rel] = {
      hostSize: 10, hostMtimeMs: T0, devSize: 10, devMtime: T0, syncedAt: T0,
    };
  }
  const p = plan([hostFile('a.txt', 10, T0)], [devFile('A.TXT', 10, T0)], state);
  check(!p.refused, 'the floor lets a small mirror lose a couple of files');
}
{
  // A fresh mirror tracks nothing, so the guard must not fire on a first pull —
  // there is nothing to protect and everything to download.
  const p = plan([], [devFile('A.TXT', 10, T0), devFile('B.TXT', 10, T0)],
                 createMirrorState(ROOT));
  check(!p.refused, 'a first sync with an empty state is never refused');
  eq(p.steps.length, 2, 'and downloads both files');
}

// ── Ordering ─────────────────────────────────────────────────────────────
{
  // Smallest first, so a small note dropped in does not queue behind half a
  // megabyte — which at 5 KB/s is two minutes of waiting.
  const p = plan([
    hostFile('huge.bin', 500_000, T0),
    hostFile('note.txt', 100, T0),
    hostFile('medium.txt', 5_000, T0),
  ], [], createMirrorState(ROOT));
  eq(p.steps.map((s) => s.rel).join(','), 'note.txt,medium.txt,huge.bin',
     'transfers are ordered smallest-first');
}

// ── adoptMirrorState ─────────────────────────────────────────────────────
{
  const original = syncedState('a.txt', 10, T0);
  const adopted = adoptMirrorState(JSON.parse(JSON.stringify(original)), ROOT);
  eq(Object.keys(adopted.entries).length, 1, 'a stored state round-trips');
  // A different root is a different mirror. Reusing the state would read as
  // "everything was deleted on both sides" and trigger a mass deletion.
  const different = adoptMirrorState(original, 'C:\\Other');
  eq(Object.keys(different.entries).length, 0,
     'state from a different root is discarded, not reinterpreted');
  eq(Object.keys(adoptMirrorState(null, ROOT).entries).length, 0, 'null gives a fresh state');
  eq(Object.keys(adoptMirrorState({ version: 9 }, ROOT).entries).length, 0,
     'a future version gives a fresh state');
}

// ── conflictName ─────────────────────────────────────────────────────────
{
  const n = conflictName('notes/report.wrd', 'device', Date.UTC(2026, 8, 19, 14, 2));
  check(n.startsWith('notes/report ('), `the folder and stem are kept (${n})`);
  check(n.endsWith('.wrd'), 'and the extension stays last, so it still opens');
  check(n.includes('from device'), 'the name says which side it came from');
  const noExt = conflictName('README', 'host', Date.UTC(2026, 8, 19, 14, 2));
  check(!noExt.includes('.'), `a file with no extension gains none (${noExt})`);
  // A dot in a directory name must not be mistaken for an extension.
  const dotted = conflictName('my.folder/file', 'host', Date.UTC(2026, 8, 19));
  check(dotted.startsWith('my.folder/file ('),
        `a dot in a folder name is not treated as an extension (${dotted})`);
}

// ── applyMirror against in-memory ports ──────────────────────────────────

function makeHostFs(initial: Record<string, string> = {}) {
  const files = new Map<string, { bytes: Uint8Array; mtimeMs: number }>();
  for (const [rel, body] of Object.entries(initial)) {
    files.set(rel, { bytes: enc(body), mtimeMs: T0 });
  }
  const fs: HostFs & { files: typeof files } = {
    files,
    list: async () => [...files.entries()].map(([rel, v]) =>
      ({ rel, size: v.bytes.byteLength, mtimeMs: v.mtimeMs })),
    read: async (rel) => {
      const f = files.get(rel);
      if (!f) throw new Error(`no such host file ${rel}`);
      return f.bytes;
    },
    write: async (rel, data, mtimeMs) => {
      files.set(rel, { bytes: data, mtimeMs: mtimeMs ?? Date.now() });
    },
    remove: async (rel) => { files.delete(rel); },
    mkdirp: async () => {},
  };
  return fs;
}

function makeDeviceFs(initial: Record<string, string> = {}) {
  const files = new Map<string, { bytes: Uint8Array; mtime: number }>();
  for (const [p, body] of Object.entries(initial)) {
    files.set(p.toUpperCase(), { bytes: enc(body), mtime: T0 });
  }
  const calls: string[] = [];
  const fs: DeviceFs & { files: typeof files; calls: string[] } = {
    files, calls,
    list: async (dir) => {
      calls.push(`list ${dir}`);
      const prefix = (dir.endsWith('\\') ? dir : `${dir}\\`).toUpperCase();
      return [...files.entries()]
        .filter(([p]) => p.startsWith(prefix) && !p.slice(prefix.length).includes('\\'))
        .map(([p, v]) => ({ path: p, size: v.bytes.byteLength, mtime: v.mtime, isDir: false }));
    },
    read: async (p) => {
      calls.push(`read ${p}`);
      const f = files.get(p.toUpperCase());
      if (!f) throw new Error(`no such device file ${p}`);
      return f.bytes;
    },
    write: async (p, data) => {
      calls.push(`write ${p}`);
      files.set(p.toUpperCase(), { bytes: data, mtime: Date.now() });
    },
    remove: async (p) => { calls.push(`remove ${p}`); files.delete(p.toUpperCase()); },
    mkdirAll: async (p) => { calls.push(`mkdir ${p}`); },
  };
  return fs;
}

// A fresh push, end to end.
{
  const host = makeHostFs({ 'note.txt': 'hello psion' });
  const device = makeDeviceFs();
  const state = createMirrorState(ROOT);
  const p = plan(await host.list(), [], state);
  const result = await applyMirror(p.steps, host, device, state);
  eq(result.failed.length, 0, 'a fresh push has no failures');
  eq(result.completed, 1, 'one step completed');
  eq(dec(device.files.get('C:\\DOCUMENTS\\NOTE.TXT')?.bytes ?? new Uint8Array()),
     'hello psion', 'the file arrived on the device');
  check(!!result.state.entries['note.txt'], 'and the state records it');
  eq(result.state.names['note.txt'], 'C:\\Documents\\NOTE.TXT', 'with its device path');

  // Immediately re-planning against the new state must produce nothing. If it
  // does not, the mirror would loop forever re-uploading the same file.
  const again = plan(await host.list(), await device.list(ROOT), result.state);
  eq(again.steps.length, 0, 'the plan is empty straight after applying it');
}

// A fresh pull.
{
  const host = makeHostFs();
  const device = makeDeviceFs({ 'C:\\Documents\\LETTER.WRD': 'dear sir' });
  const state = createMirrorState(ROOT);
  const p = plan([], await device.list(ROOT), state);
  const result = await applyMirror(p.steps, host, device, state);
  eq(result.failed.length, 0, 'a fresh pull has no failures');
  eq(dec(host.files.get('letter.wrd')?.bytes ?? new Uint8Array()), 'dear sir',
     'the file arrived on the host, under a lower-cased name');
}

// A conflict keeps BOTH copies. This is the assertion that matters most in the
// whole engine: nothing the user wrote may be destroyed.
{
  const host = makeHostFs({ 'a.txt': 'edited here' });
  host.files.get('a.txt')!.mtimeMs = T0 + 30_000;
  const device = makeDeviceFs({ 'C:\\Documents\\A.TXT': 'edited on the psion' });
  device.files.get('C:\\DOCUMENTS\\A.TXT')!.mtime = T0 + 60_000;

  const state = syncedState('a.txt', 10, T0);
  const p = plan(await host.list(), await device.list(ROOT), state);
  eq(p.steps[0].op, 'conflict', 'the plan says conflict');
  const result = await applyMirror(p.steps, host, device, state);
  eq(result.failed.length, 0, 'the conflict was applied without failing');
  eq(dec(host.files.get('a.txt')?.bytes ?? new Uint8Array()), 'edited on the psion',
     'the winner (the device copy) is in place');
  const loser = [...host.files.keys()].find((k) => k.includes('from this computer'));
  check(!!loser, `the losing copy was kept (${[...host.files.keys()].join(', ')})`);
  eq(dec(host.files.get(loser ?? '')?.bytes ?? new Uint8Array()), 'edited here',
     'with its original contents');
}

// Deletions, both directions.
{
  const host = makeHostFs();
  const device = makeDeviceFs({ 'C:\\Documents\\A.TXT': 'x' });
  const state = syncedState('a.txt', 1, T0);
  const p = plan([], await device.list(ROOT), state);
  const result = await applyMirror(p.steps, host, device, state);
  eq(device.files.size, 0, 'a host deletion removed the device copy');
  check(!result.state.entries['a.txt'], 'and the record is dropped');
}
{
  const host = makeHostFs({ 'a.txt': 'x' });
  const device = makeDeviceFs();
  const state = syncedState('a.txt', 1, T0);
  const p = plan(await host.list(), [], state);
  const result = await applyMirror(p.steps, host, device, state);
  eq(host.files.size, 0, 'a device deletion removed the host copy');
  check(!result.state.entries['a.txt'], 'and the record is dropped');
}

// A mid-plan failure must leave the successful steps recorded. Otherwise the
// next attempt redoes the whole folder over a 5 KB/s link.
{
  const host = makeHostFs({ 'ok.txt': 'fine', 'bad.txt': 'doomed', 'also-ok.txt': 'fine too' });
  const device = makeDeviceFs();
  // Fail exactly one write.
  const realWrite = device.write;
  device.write = async (p, data) => {
    if (p.toUpperCase().includes('BAD')) throw new Error('device refused the write');
    return realWrite(p, data);
  };
  const state = createMirrorState(ROOT);
  const p = plan(await host.list(), [], state);
  const result = await applyMirror(p.steps, host, device, state);
  eq(result.failed.length, 1, 'the one failing step is reported');
  eq(result.completed, 2, 'the other two completed');
  check(!!result.state.entries['ok.txt'] && !!result.state.entries['also-ok.txt'],
        'the successful steps are recorded');
  check(!result.state.entries['bad.txt'], 'the failed one is not');

  // And the retry only redoes the failure.
  const retry = plan(await host.list(), await device.list(ROOT), result.state);
  eq(opsOf(retry), 'upload:bad.txt', 'a retry moves only what failed');
}

// Aborting stops cleanly, keeping what was done.
{
  const host = makeHostFs({ 'a.txt': '1', 'b.txt': '2', 'c.txt': '3' });
  const device = makeDeviceFs();
  const state = createMirrorState(ROOT);
  const p = plan(await host.list(), [], state);
  let seen = 0;
  const result = await applyMirror(p.steps, host, device, state, {
    shouldAbort: () => seen++ >= 1,
  });
  check(result.aborted, 'the abort is reported');
  check(result.completed >= 1 && result.completed < 3, `partial progress (${result.completed})`);
  eq(Object.keys(result.state.entries).length, result.completed,
     'exactly the completed steps are recorded');
}

// ── Backing off a file the device will not open ───────────────────────────
//
// Also learnt from a real machine. A freshly booted 5mx holds its stock Word
// and Sheet documents OPEN, and RFSV answers KErrInUse to any attempt to read
// them. Without a backoff the mirror would spend a round-trip per file per
// cycle, forever, on a link that manages a couple of KB/s.
{
  eq(describeFailure('OPEN_FILE(C:\\Documents\\Word) failed: -14'), 'in use on the device',
     'KErrInUse is reported in words, not as a number');
  eq(describeFailure('OPEN_FILE(x) failed: -21'), 'access denied on the device',
     'and so is access denied');
  check(describeFailure('something else entirely').includes('something else'),
        'an unrecognised error is passed through');
}
{
  const host = makeHostFs();
  const device = makeDeviceFs({ 'C:\\Documents\\LOCKED.TXT': 'cannot be read' });
  device.read = async () => { throw new Error('OPEN_FILE(C:\\Documents\\LOCKED.TXT) failed: -14'); };

  const state = createMirrorState(ROOT);
  const first = plan([], await device.list(ROOT), state);
  eq(first.steps.length, 1, 'the unreadable file is attempted once');
  const applied = await applyMirror(first.steps, host, device, state, { now: () => T0 });
  eq(applied.failed.length, 1, 'and fails');
  const skip = applied.state.skips['locked.txt'];
  check(!!skip, 'a backoff record is written');
  eq(skip?.reason, 'in use on the device', 'with a reason a person can act on');
  check(skip!.until > T0, 'and a time to wait until');

  // The next cycle must not touch it at all — that is the whole point.
  const second = plan([], await device.list(ROOT), applied.state);
  eq(second.steps.length, 0, 'the next cycle skips it entirely');
  check(second.warnings.some((w) => w.includes('in use on the device')),
        'while still saying so, so it is visible rather than silently dropped');

  // Once the user closes the document the mirror picks it up again, without
  // being told.
  const expired: MirrorState = {
    ...applied.state,
    skips: { 'locked.txt': { ...skip!, until: T0 - 1000 } },
  };
  const third = plan([], await device.list(ROOT), expired);
  eq(third.steps.length, 1, 'once the backoff expires it is retried');

  // And a success clears the record rather than leaving it to expire.
  device.read = async () => new TextEncoder().encode('now readable');
  const recovered = await applyMirror(third.steps, host, device, expired, { now: () => T0 });
  check(!recovered.state.skips['locked.txt'], 'a successful transfer clears the backoff');
  eq(dec(host.files.get('locked.txt')?.bytes ?? new Uint8Array()), 'now readable',
     'and the file arrives');
}
{
  // The wait grows, but not without limit: a document left open all day should
  // still be picked up within the hour of being closed.
  const now = 1_000_000;
  const first = backoffUntil(1, now) - now;
  const later = backoffUntil(4, now) - now;
  const capped = backoffUntil(50, now) - now;
  check(later > first, 'the wait grows with repeated failures');
  eq(capped, 60 * 60_000, 'and is capped at an hour');
}

if (failures) {
  console.error(`\n${failures} assertion(s) failed`);
  process.exit(1);
}
console.log('hostsync.mirror.test.mts: all assertions passed');
