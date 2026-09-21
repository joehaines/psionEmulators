// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The ONLY filesystem code in the app.
//
// Everything the renderer does to a host folder comes through here, and the
// rule that makes it safe is that the renderer never names a location: it
// names a MOUNT and a path RELATIVE to it. Mount roots are established solely
// by a native folder dialog — a real user gesture — and held in the main
// process. The renderer cannot introduce one, cannot read one back as a usable
// path, and cannot reach outside one.
//
// Validation is belt and braces on purpose, because this is the one place
// where a mistake reaches the user's whole disk:
//
//   1. The relative path is rejected on its own terms — no NUL, no absolute
//      form, no drive letter, no '.' or '..' segment, no backslash (the
//      contract says POSIX separators, so a backslash means someone is
//      guessing).
//   2. It is resolved against the root and prefix-checked.
//   3. The result is realpath'd and prefix-checked AGAIN, which is what
//      catches a symlink inside the mount pointing out of it. Step 1 cannot
//      see that; only the filesystem can.
//
// Caps exist so a mistake is bounded rather than fatal: a single file, the
// number of entries a listing returns, and how deep a walk goes.

import * as fsp from 'node:fs/promises';
import * as path from 'node:path';
import type { HostDirent, MountId } from '../ipc/contract';

/** Refuse a single file larger than this. Documents, not disk images. */
export const MAX_FILE_BYTES = 32 * 1024 * 1024;
/** A listing beyond this is a mistake — the user pointed at their home dir. */
export const MAX_ENTRIES = 5_000;
/** How deep to walk. Psion media is shallow; this is generous. */
export const MAX_DEPTH = 8;

const MOUNT_IDS: readonly MountId[] = ['internal', 'external'];
/** Bookkeeping the sync engines keep beside a mount, never mirrored. */
const HIDDEN_ENTRIES = new Set(['.psion-sync', '.DS_Store', 'Thumbs.db', 'desktop.ini']);

const roots = new Map<MountId, string>();

export function isMountId(v: unknown): v is MountId {
  return typeof v === 'string' && (MOUNT_IDS as readonly string[]).includes(v);
}

export function assertMountId(v: unknown): MountId {
  if (!isMountId(v)) throw new Error(`unknown mount: ${String(v)}`);
  return v;
}

export function setRoot(id: MountId, absolutePath: string | null): void {
  if (absolutePath === null) { roots.delete(id); return; }
  if (!path.isAbsolute(absolutePath)) {
    throw new Error('a mount root must be an absolute path');
  }
  roots.set(id, path.normalize(absolutePath));
}

export function getRoot(id: MountId): string | null {
  return roots.get(id) ?? null;
}

function requireRoot(id: MountId): string {
  const root = roots.get(id);
  if (!root) throw new Error(`mount '${id}' has no folder chosen`);
  return root;
}

/**
 * Validate a renderer-supplied relative path on its own terms.
 *
 * Deliberately strict and deliberately NOT clever: no attempt to sanitise a
 * bad path into a good one. Anything questionable is refused, because the
 * cost of refusing a legitimate filename is a puzzled user and the cost of
 * accepting a malicious one is the user's home directory.
 */
export function validateRelPath(rel: unknown): string {
  if (typeof rel !== 'string') throw new Error('path must be a string');
  if (rel.length === 0 || rel.length > 1024) throw new Error('path length out of range');
  if (rel.includes('\0')) throw new Error('path contains a NUL');
  // The contract is POSIX separators. A backslash means the caller is
  // guessing at the platform, and on Windows it would be a second separator
  // the checks below would not see.
  if (rel.includes('\\')) throw new Error('path must use / as its separator');
  if (rel.startsWith('/')) throw new Error('path must be relative');
  if (/^[A-Za-z]:/.test(rel)) throw new Error('path must not name a drive');

  const segments = rel.split('/');
  for (const seg of segments) {
    if (seg === '') throw new Error('path has an empty segment');
    if (seg === '.' || seg === '..') throw new Error('path has a . or .. segment');
    // Trailing dots and spaces are stripped by Windows, which would make two
    // distinct relative paths resolve to one file.
    if (/[. ]$/.test(seg)) throw new Error('path segment ends with a dot or space');
  }
  return segments.join(path.sep);
}

/**
 * The deepest ancestor of `p` (including `p`) that exists on disk.
 *
 * Needed because a containment check can only interrogate something real. To
 * create `a/b/c` safely we must first establish that the deepest part of that
 * chain which already exists is inside the mount — if `a` is a symlink out,
 * `mkdir -p` would follow it and build `b/c` outside before any check ran.
 */
async function deepestExisting(p: string): Promise<string> {
  let cur = p;
  for (;;) {
    try {
      await fsp.lstat(cur);
      return cur;
    } catch {
      const parent = path.dirname(cur);
      if (parent === cur) return cur;   // reached the filesystem root
      cur = parent;
    }
  }
}

/**
 * Validate `rel`, and prove that creating it cannot write outside the mount.
 *
 * This runs BEFORE anything is created. The earlier version checked only
 * after mkdir, which meant a symlinked directory inside the mount let
 * `mkdir -p` build a tree outside it and then refused — having already done
 * the damage. Caught by desktop/test/hostfs.test.mts.
 */
async function prepareForCreate(id: MountId, rel: unknown): Promise<string> {
  const root = requireRoot(id);
  const native = validateRelPath(rel);
  const candidate = path.resolve(root, native);
  if (!candidate.startsWith(root + path.sep)) throw new Error('path escapes the mount');

  const realRoot = await fsp.realpath(root).catch(() => {
    throw new Error(`mount '${id}' is no longer reachable`);
  });
  const ancestor = await deepestExisting(candidate);
  const realAncestor = await fsp.realpath(ancestor).catch(() => {
    throw new Error('the containing folder cannot be resolved');
  });
  if (realAncestor !== realRoot && !realAncestor.startsWith(realRoot + path.sep)) {
    throw new Error('path resolves outside the mount');
  }
  return candidate;
}

/**
 * Resolve to an absolute path inside the mount, or throw.
 *
 * `mustExist` picks which link the realpath check applies to: the file itself
 * for a read, its parent directory for a write (where the file may not be
 * there yet). Either way something real is checked — a prefix test on an
 * unresolved string cannot see a symlink.
 */
export async function resolveIn(
  id: MountId, rel: unknown, mustExist: boolean,
): Promise<string> {
  const root = requireRoot(id);
  const native = validateRelPath(rel);
  const candidate = path.resolve(root, native);

  if (candidate !== root && !candidate.startsWith(root + path.sep)) {
    throw new Error('path escapes the mount');
  }

  const realRoot = await fsp.realpath(root).catch(() => {
    throw new Error(`mount '${id}' is no longer reachable`);
  });

  const toCheck = mustExist ? candidate : path.dirname(candidate);
  const real = await fsp.realpath(toCheck).catch(() => {
    if (mustExist) throw new Error('no such file in the mount');
    throw new Error('the containing folder does not exist');
  });
  if (real !== realRoot && !real.startsWith(realRoot + path.sep)) {
    // The usual cause is a symlink planted inside the mount. Refusing is the
    // only safe answer: following it would write outside the folder the user
    // actually chose.
    throw new Error('path resolves outside the mount');
  }
  return candidate;
}

/**
 * Every file under the mount, relative and POSIX-separated.
 *
 * Directories are reported too, so a projection can recreate an empty one.
 * Symlinks are skipped rather than followed: a loop would hang the walk and a
 * link out of the mount is not the user's content.
 */
export async function list(id: MountId): Promise<HostDirent[]> {
  const root = requireRoot(id);
  const out: HostDirent[] = [];

  async function walk(dir: string, prefix: string, depth: number): Promise<void> {
    if (depth > MAX_DEPTH || out.length >= MAX_ENTRIES) return;
    const entries = await fsp.readdir(dir, { withFileTypes: true }).catch(() => []);
    // Sorted so a listing is stable between calls — the diff on top of it
    // compares sequences, and an arbitrary readdir order would churn.
    entries.sort((a, b) => a.name.localeCompare(b.name));
    for (const entry of entries) {
      if (out.length >= MAX_ENTRIES) return;
      if (HIDDEN_ENTRIES.has(entry.name) || entry.name.startsWith('.')) continue;
      const full = path.join(dir, entry.name);
      const rel = prefix ? `${prefix}/${entry.name}` : entry.name;
      if (entry.isSymbolicLink()) continue;
      if (entry.isDirectory()) {
        out.push({ rel, size: 0, mtimeMs: 0, isDir: true });
        await walk(full, rel, depth + 1);
      } else if (entry.isFile()) {
        const st = await fsp.stat(full).catch(() => null);
        if (!st) continue;
        out.push({ rel, size: st.size, mtimeMs: st.mtimeMs, isDir: false });
      }
    }
  }

  await walk(root, '', 0);
  return out;
}

export async function read(id: MountId, rel: unknown): Promise<Uint8Array> {
  const file = await resolveIn(id, rel, true);
  const st = await fsp.stat(file);
  if (!st.isFile()) throw new Error('not a file');
  if (st.size > MAX_FILE_BYTES) {
    throw new Error(`file is larger than the ${MAX_FILE_BYTES} byte limit`);
  }
  const buf = await fsp.readFile(file);
  return new Uint8Array(buf.buffer, buf.byteOffset, buf.byteLength);
}

export async function write(
  id: MountId, rel: unknown, data: unknown, mtimeMs?: unknown,
): Promise<void> {
  if (!(data instanceof Uint8Array)) throw new Error('data must be a Uint8Array');
  if (data.byteLength > MAX_FILE_BYTES) {
    throw new Error(`refusing to write more than ${MAX_FILE_BYTES} bytes`);
  }
  // Proven safe before anything is created — see prepareForCreate.
  const file = await prepareForCreate(id, rel);
  await fsp.mkdir(path.dirname(file), { recursive: true });
  // Temp-plus-rename, so a reader (the user's own file manager, or a watcher)
  // never sees a half-written document.
  const tmp = `${file}.psion-tmp-${process.pid}`;
  await fsp.writeFile(tmp, data);
  await fsp.rename(tmp, file);
  if (typeof mtimeMs === 'number' && Number.isFinite(mtimeMs) && mtimeMs > 0) {
    const when = new Date(mtimeMs);
    await fsp.utimes(file, when, when).catch(() => undefined);
  }
}

export async function remove(id: MountId, rel: unknown): Promise<void> {
  const target = await resolveIn(id, rel, true);
  const st = await fsp.lstat(target);
  if (st.isDirectory()) {
    // Non-recursive on purpose: deleting a tree is not something the renderer
    // should be able to ask for in one call.
    await fsp.rmdir(target);
    return;
  }
  await fsp.rm(target, { force: true });
}

export async function mkdirp(id: MountId, rel: unknown): Promise<void> {
  const target = await prepareForCreate(id, rel);
  await fsp.mkdir(target, { recursive: true });
}

/** Absolute path for shell reveal. Main-process use only. */
export async function absolutePathFor(id: MountId, rel?: unknown): Promise<string> {
  if (rel === undefined || rel === null || rel === '') return requireRoot(id);
  return resolveIn(id, rel, true);
}
