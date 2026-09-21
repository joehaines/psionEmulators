// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Save generations on disk.
//
// IndexedDB remains the live store — the emulator's save path writes it from
// inside the worker, already gzipped, and rewiring that is the one change in
// this codebase most likely to silently corrupt a session. What lands here is
// a MIRROR: a versioned history the user can roll back through, and the only
// copy that survives a wiped browser profile or an origin eviction.
//
// The format is the app's existing PSIONST1 bundle, one device per file, which
// means a desktop generation is exactly what the web app's "Upload" button
// already accepts. Portability for free.
//
// Layout:
//   <userData>/saves/<deviceId>/manifest.json
//   <userData>/saves/<deviceId>/20260919-140233.psionst1
//
// Durability rule: the generation file is written and fsynced BEFORE the
// manifest is updated, and the manifest itself lands via temp-plus-rename.
// So a crash mid-write leaves an orphan file, never a manifest pointing at a
// truncated one — and orphans are swept on startup. The failure that matters
// is a manifest that claims a save exists when it does not.

import { app } from 'electron';
import * as fsp from 'node:fs/promises';
import * as path from 'node:path';
import type { SaveGeneration } from '../ipc/contract';

/** Device ids come from the WASM registry, but they still reach a path. */
const DEVICE_ID_RE = /^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$/;
/** Generation filenames are ours; anything else in the directory is foreign. */
const GENERATION_RE = /^\d{8}-\d{6}\.psionst1$/;
/** A bundle far larger than this is not a save; refuse rather than fill a disk. */
const MAX_BUNDLE_BYTES = 512 * 1024 * 1024;

interface Manifest {
  version: 1;
  generations: SaveGeneration[];
}

function assertDeviceId(deviceId: unknown): string {
  if (typeof deviceId !== 'string' || !DEVICE_ID_RE.test(deviceId)) {
    throw new Error(`invalid device id: ${String(deviceId)}`);
  }
  return deviceId;
}

function assertGenerationFile(file: unknown): string {
  if (typeof file !== 'string' || !GENERATION_RE.test(file)) {
    throw new Error(`invalid generation filename: ${String(file)}`);
  }
  return file;
}

function deviceDir(deviceId: string): string {
  return path.join(app.getPath('userData'), 'saves', deviceId);
}

function manifestPath(deviceId: string): string {
  return path.join(deviceDir(deviceId), 'manifest.json');
}

async function writeJsonAtomic(target: string, value: unknown): Promise<void> {
  const tmp = `${target}.${process.pid}.tmp`;
  await fsp.mkdir(path.dirname(target), { recursive: true });
  await fsp.writeFile(tmp, JSON.stringify(value, null, 2), 'utf8');
  await fsp.rename(tmp, target);
}

async function readManifest(deviceId: string): Promise<Manifest> {
  try {
    const raw = JSON.parse(await fsp.readFile(manifestPath(deviceId), 'utf8')) as Manifest;
    if (raw && raw.version === 1 && Array.isArray(raw.generations)) return raw;
  } catch {
    // Missing on first save, or unreadable after an interrupted write. Either
    // way the directory is the ground truth — see reconcile().
  }
  return { version: 1, generations: [] };
}

/**
 * Trust the directory over the manifest.
 *
 * Returns the generations that actually exist on disk, dropping manifest
 * entries whose file is gone and adopting files the manifest never recorded
 * (the crash-mid-write case). A manifest entry without a file is the
 * dangerous direction: it would offer the user a restore that cannot happen.
 */
async function reconcile(deviceId: string): Promise<SaveGeneration[]> {
  const dir = deviceDir(deviceId);
  let entries: string[];
  try {
    entries = await fsp.readdir(dir);
  } catch {
    return [];
  }
  const onDisk = entries.filter((f) => GENERATION_RE.test(f));
  const manifest = await readManifest(deviceId);
  const byFile = new Map(manifest.generations.map((g) => [g.file, g]));

  const out: SaveGeneration[] = [];
  for (const file of onDisk) {
    const stat = await fsp.stat(path.join(dir, file)).catch(() => null);
    if (!stat || !stat.isFile() || stat.size === 0) continue;
    const known = byFile.get(file);
    out.push({
      file,
      // An adopted orphan has no recorded metadata; its filename is a UTC
      // timestamp, so mtime is a faithful enough stand-in.
      savedAt: known?.savedAt ?? stat.mtimeMs,
      bytes: stat.size,
      schemaVersion: known?.schemaVersion ?? 0,
    });
  }
  out.sort((a, b) => b.savedAt - a.savedAt);
  return out;
}

export async function list(deviceId: unknown): Promise<SaveGeneration[]> {
  return reconcile(assertDeviceId(deviceId));
}

export async function write(
  deviceIdRaw: unknown,
  bundle: unknown,
  metaRaw: unknown,
): Promise<void> {
  const deviceId = assertDeviceId(deviceIdRaw);
  if (!(bundle instanceof Uint8Array)) throw new Error('bundle must be a Uint8Array');
  if (bundle.byteLength === 0) throw new Error('refusing to write an empty bundle');
  if (bundle.byteLength > MAX_BUNDLE_BYTES) {
    throw new Error(`bundle is implausibly large (${bundle.byteLength} bytes)`);
  }
  const meta = (metaRaw ?? {}) as { savedAt?: unknown; schemaVersion?: unknown };
  const savedAt = Number(meta.savedAt);
  const schemaVersion = Number(meta.schemaVersion);
  if (!Number.isFinite(savedAt) || savedAt <= 0) throw new Error('meta.savedAt is required');

  // Sanity-check the magic before this becomes something the user is offered
  // as restorable. Cheap, and a mangled bundle is worse than no bundle.
  const magic = Buffer.from(bundle.subarray(0, 8)).toString('latin1');
  if (magic !== 'PSIONST1') throw new Error(`not a PSIONST1 bundle (magic ${JSON.stringify(magic)})`);

  const dir = deviceDir(deviceId);
  await fsp.mkdir(dir, { recursive: true });

  const iso = new Date(savedAt).toISOString();
  const file = `${iso.slice(0, 10).replace(/-/g, '')}-${iso.slice(11, 19).replace(/:/g, '')}.psionst1`;
  const target = path.join(dir, file);
  const tmp = `${target}.${process.pid}.tmp`;

  // Write, fsync, THEN rename: the manifest must never be able to reference a
  // file whose bytes are still in the page cache when the power goes.
  const handle = await fsp.open(tmp, 'w');
  try {
    await handle.write(bundle);
    await handle.sync();
  } finally {
    await handle.close();
  }
  await fsp.rename(tmp, target);

  const generations = await reconcile(deviceId);
  const next = generations.filter((g) => g.file !== file);
  next.unshift({ file, savedAt, bytes: bundle.byteLength, schemaVersion });
  next.sort((a, b) => b.savedAt - a.savedAt);
  await writeJsonAtomic(manifestPath(deviceId), { version: 1, generations: next } as Manifest);
}

export async function read(deviceIdRaw: unknown, fileRaw: unknown): Promise<Uint8Array> {
  const deviceId = assertDeviceId(deviceIdRaw);
  const file = assertGenerationFile(fileRaw);
  const buf = await fsp.readFile(path.join(deviceDir(deviceId), file));
  return new Uint8Array(buf.buffer, buf.byteOffset, buf.byteLength);
}

/**
 * Delete the named generations. The caller decides which — planRetention in
 * frontend/src/lib/desktop/autosave.ts — but the newest is protected here as
 * well, because a bug on the renderer side must not be able to leave a device
 * with nothing to restore.
 */
export async function prune(deviceIdRaw: unknown, filesRaw: unknown): Promise<void> {
  const deviceId = assertDeviceId(deviceIdRaw);
  if (!Array.isArray(filesRaw)) throw new Error('files must be an array');
  const requested = new Set(filesRaw.map(assertGenerationFile));

  const generations = await reconcile(deviceId);
  if (generations.length === 0) return;
  const newest = generations[0].file;
  if (requested.has(newest)) {
    console.warn(`[psion] refusing to prune the newest generation for ${deviceId}`);
    requested.delete(newest);
  }
  if (requested.size === 0) return;

  const dir = deviceDir(deviceId);
  for (const file of requested) {
    await fsp.rm(path.join(dir, file), { force: true });
  }
  const remaining = generations.filter((g) => !requested.has(g.file));
  await writeJsonAtomic(manifestPath(deviceId), { version: 1, generations: remaining } as Manifest);
}

/**
 * Sweep leftovers on startup: temp files from an interrupted write, and
 * manifests that have drifted from their directory. Cheap, and it keeps a
 * crashed session from leaving the store permanently inconsistent.
 */
export async function sweep(): Promise<void> {
  const root = path.join(app.getPath('userData'), 'saves');
  let devices: string[];
  try {
    devices = await fsp.readdir(root);
  } catch {
    return;
  }
  for (const deviceId of devices) {
    if (!DEVICE_ID_RE.test(deviceId)) continue;
    const dir = path.join(root, deviceId);
    const entries = await fsp.readdir(dir).catch(() => [] as string[]);
    for (const f of entries) {
      if (f.endsWith('.tmp')) await fsp.rm(path.join(dir, f), { force: true });
    }
    const generations = await reconcile(deviceId);
    await writeJsonAtomic(manifestPath(deviceId), { version: 1, generations } as Manifest)
      .catch(() => undefined);
  }
}
