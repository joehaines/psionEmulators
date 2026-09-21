// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Ports and state for the internal-drive mirror.
//
// Both sides are interfaces rather than concrete classes for one specific
// reason: tests/integration/ drives the real PlpClient against a real ROM in
// the native harness, in plain Node, with no DOM and no Electron. The engine has
// to be reachable from there, so it can know nothing about either.

/** A file in the host folder. */
export interface HostFile {
  /** POSIX-separated, relative to the mount root. */
  rel: string;
  size: number;
  mtimeMs: number;
}

export interface HostFs {
  list(): Promise<HostFile[]>;
  read(rel: string): Promise<Uint8Array>;
  write(rel: string, data: Uint8Array, mtimeMs?: number): Promise<void>;
  remove(rel: string): Promise<void>;
  mkdirp(rel: string): Promise<void>;
}

/** A file on the emulated machine's own drive. */
export interface DeviceFile {
  /** Full device path, backslash-separated as EPOC reports it. */
  path: string;
  size: number;
  /** Epoch ms. EPOC maintains this on save, which is what makes the diff work. */
  mtime: number;
  isDir: boolean;
}

export interface DeviceFs {
  list(dir: string): Promise<DeviceFile[]>;
  read(path: string, onProgress?: (bytes: number) => void): Promise<Uint8Array>;
  write(path: string, data: Uint8Array, onProgress?: (bytes: number) => void): Promise<void>;
  remove(path: string): Promise<void>;
  mkdirAll(path: string): Promise<void>;
}

/**
 * What the last completed sync left behind.
 *
 * The third leg of the three-way compare, and the reason deletions can be
 * propagated safely: without a record of what was synced, "absent on the host"
 * is indistinguishable from "never been there", and the engine would either
 * re-download everything the user deleted or delete everything it had not yet
 * uploaded.
 */
export interface MirrorState {
  version: 1;
  /** The device directory being mirrored, e.g. 'C:\\Documents\\'. */
  root: string;
  /** Host relative path → device path. */
  names: Record<string, string>;
  entries: Record<string, MirrorEntry>;
  /**
   * Files to leave alone for a while, and why.
   *
   * Not every file on a Psion can be read. A real 5mx boots with its stock
   * Word and Sheet documents OPEN, and RFSV answers KErrInUse (-14) to any
   * attempt to open them — so a mirror that simply retried would burn a
   * round-trip per file per cycle, forever, on a link that manages a couple of
   * KB/s. Backing off keeps the failure visible without making it expensive.
   */
  skips: Record<string, SkipRecord>;
}

export interface SkipRecord {
  /** Epoch ms; the file is left alone until then. */
  until: number;
  reason: string;
  attempts: number;
}

export interface MirrorEntry {
  hostSize: number;
  hostMtimeMs: number;
  devSize: number;
  devMtime: number;
  syncedAt: number;
}

export function createMirrorState(root: string): MirrorState {
  return { version: 1, root, names: {}, entries: {}, skips: {} };
}

/**
 * How long to leave a file alone after a failure.
 *
 * Doubling from a minute to an hour: a document the user has open on the Psion
 * will still be open in a minute, and probably in ten, but they will close it
 * eventually and the mirror should notice without being told.
 */
export function backoffUntil(attempts: number, now: number): number {
  const minutes = Math.min(60, 2 ** Math.max(0, attempts - 1));
  return now + minutes * 60_000;
}

export function adoptMirrorState(stored: unknown, root: string): MirrorState {
  if (!stored || typeof stored !== 'object') return createMirrorState(root);
  const s = stored as Partial<MirrorState>;
  if (s.version !== 1 || !s.entries || !s.names) return createMirrorState(root);
  // A different root means a different mirror; reusing the state would read as
  // "everything was deleted on both sides".
  if (s.root !== root) return createMirrorState(root);
  return {
    version: 1, root,
    names: { ...s.names },
    entries: { ...s.entries },
    // Absent in a state written before backoff existed.
    skips: { ...(s.skips ?? {}) },
  };
}

export type MirrorOp =
  | 'upload' | 'download'
  | 'delete-device' | 'delete-host'
  | 'mkdir-device'
  | 'conflict'
  | 'forget';

export interface MirrorStep {
  op: MirrorOp;
  /** Host relative path this step concerns. */
  rel: string;
  /** Device path this step concerns. */
  devPath: string;
  /** Bytes to move, for progress and for ordering. 0 for metadata-only steps. */
  bytes: number;
  /** For a conflict: which side won, and where the loser was put. */
  winner?: 'host' | 'device';
  loserAs?: string;
}

export interface MirrorPolicy {
  /** Reject a plan that would touch more than this many files. */
  maxFiles: number;
  /** Warn beyond this many. */
  warnFiles: number;
  /** Warn beyond this many total bytes. */
  warnBytes: number;
  /** Device subdirectories never touched, whatever they contain. */
  excludeDirs: string[];
  /**
   * Refuse a plan that would delete more than this fraction of the files the
   * mirror is tracking.
   *
   * A safety catch, not a preference. A device listing that comes back empty
   * for a reason that has nothing to do with the user — a glitched link, a
   * renamed folder, a path the ROM rejects — is indistinguishable from "they
   * deleted everything", and acting on it destroys their documents. Bulk
   * deletion is therefore treated as evidence of a fault rather than intent.
   * A real spring-clean just takes two cycles.
   */
  maxDeleteFraction: number;
  /** Deletions are always allowed up to this many, however small the mirror. */
  maxDeleteFloor: number;
}

export const DEFAULT_MIRROR_POLICY: MirrorPolicy = {
  maxFiles: 512,
  warnFiles: 64,
  warnBytes: 1024 * 1024,
  // Never mirror the OS. It churns, it is large, and a host-side delete in
  // there would wreck the session. Not configurable on purpose.
  excludeDirs: ['SYSTEM', 'SYS', '$RECYCLE.BIN'],
  maxDeleteFraction: 0.5,
  maxDeleteFloor: 3,
};
