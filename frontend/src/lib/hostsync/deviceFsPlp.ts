// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The mirror's DeviceFs, implemented over the Remote Link.
//
// A thin adapter on purpose: PlpClient already handles the hard parts (the
// link handshake, NCP channels, RFSV chunking, retransmission, the parkable
// session), and appLibrary.ts already proves the transfer flow works against
// real ROMs. This just presents that as the four operations the mirror needs.
//
// The only real work here is timestamps. EPOC reports modification time as a
// 64-bit count of microseconds since 1 January 0 AD — not a Unix epoch — so it
// has to be converted, and the conversion has to be stable, because the
// mirror's whole change-detection story rests on comparing it.

import type { DirEntry } from './../plp/rfsv32-spec.ts';
import { ATTR } from './../plp/types.ts';
import type { DeviceFile, DeviceFs } from './types.ts';

/**
 * The subset of PlpClient the mirror uses.
 *
 * Declared structurally rather than importing the class so this module (and so
 * the engine above it) stays testable with a stub, and so the integration test
 * can pass a real client without a cast.
 */
export interface PlpLike {
  listDirectory(path: string): Promise<DirEntry[]>;
  downloadFile(path: string, onProgress?: (bytes: number) => void): Promise<Uint8Array>;
  uploadFile(path: string, data: Uint8Array, onProgress?: (bytes: number) => void): Promise<void>;
  deleteFile(path: string): Promise<void>;
  makeDirAll(path: string): Promise<void>;
}

// Microseconds from 1 Jan 0 AD (EPOC's epoch) to 1 Jan 1970.
//
// 1970 years, of which the leap years are those divisible by 4, less those
// divisible by 100, plus those divisible by 400 — on the proleptic Gregorian
// calendar EPOC's TTime uses.
const LEAPS_TO_1970 = Math.floor(1969 / 4) - Math.floor(1969 / 100) + Math.floor(1969 / 400);
const DAYS_TO_1970 = 1970 * 365 + LEAPS_TO_1970;
const EPOC_TO_UNIX_US = DAYS_TO_1970 * 86_400 * 1_000_000;

/** EPOC's 64-bit microsecond time, as a pair of 32-bit words, to epoch ms. */
export function epocTimeToUnixMs(low: number, high: number): number {
  // Reassembled in floating point: the value exceeds 2^53 only beyond the year
  // 285 million, so a double holds it with microsecond precision for any date a
  // Psion can produce.
  const micros = (high >>> 0) * 4_294_967_296 + (low >>> 0);
  const ms = (micros - EPOC_TO_UNIX_US) / 1000;
  // A file with no timestamp, or one whose conversion lands before the Psion
  // existed, is reported as 0 so the mirror treats it as "unknown" rather than
  // as 1970 — which would look like an edit on every single cycle.
  return Number.isFinite(ms) && ms > 0 ? Math.round(ms) : 0;
}

export function dirEntryToDeviceFile(dir: string, entry: DirEntry): DeviceFile {
  const base = dir.endsWith('\\') ? dir : `${dir}\\`;
  const name = entry.longName || entry.shortName;
  return {
    path: base + name,
    size: entry.size,
    mtime: epocTimeToUnixMs(entry.modifiedLow, entry.modifiedHigh),
    // Both signals, because a ROM that sets one and not the other would
    // otherwise have its directories mirrored as zero-byte files.
    isDir: (entry.attributes & ATTR.DIRECTORY) !== 0 || entry.isDirectory,
  };
}

export interface DeviceFsOptions {
  /**
   * Recursion depth for a listing. One by default: every extra level is
   * another round-trip per directory over a link that manages single-digit
   * KB/s, and Psion document folders are flat in practice.
   */
  maxDepth?: number;
  /** Directory names never descended into. */
  excludeDirs?: string[];
  onProgress?(path: string, bytes: number): void;
}

/**
 * Wrap a connected PlpClient as the mirror's device side.
 *
 * `list` recurses to maxDepth, which is why it takes a directory rather than
 * returning everything: each level costs an OPEN_DIR plus READ_DIR round trip,
 * so the caller decides how much of the tree it is willing to pay for.
 */
export function createDeviceFs(client: PlpLike, opts: DeviceFsOptions = {}): DeviceFs {
  const maxDepth = opts.maxDepth ?? 1;
  const excluded = new Set((opts.excludeDirs ?? []).map((d) => d.toUpperCase()));

  const listRecursive = async (dir: string, depth: number): Promise<DeviceFile[]> => {
    // RFSV's OPEN_DIR wants the trailing separator — 'C:\\Documents' lists
    // empty where 'C:\\Documents\\' lists the folder. Every existing caller
    // (RemoteLinkDialog, appLibrary) happens to pass one already, so the
    // requirement is easy to miss; the mirror passes a root that does not, and
    // an empty listing read as "everything was deleted".
    const entries = await client.listDirectory(dir.endsWith('\\') ? dir : `${dir}\\`);
    const out: DeviceFile[] = [];
    for (const entry of entries) {
      const file = dirEntryToDeviceFile(dir, entry);
      const name = (entry.longName || entry.shortName).toUpperCase();
      if (file.isDir) {
        if (excluded.has(name)) continue;
        out.push(file);
        if (depth < maxDepth) {
          // A directory that cannot be listed is reported as itself and skipped
          // rather than failing the whole cycle — one unreadable folder must not
          // stop the rest of the mirror.
          out.push(...await listRecursive(file.path, depth + 1).catch(() => []));
        }
      } else {
        out.push(file);
      }
    }
    return out;
  };

  return {
    list: (dir) => listRecursive(dir, 1),
    read: (path, onProgress) =>
      client.downloadFile(path, (n) => { onProgress?.(n); opts.onProgress?.(path, n); }),
    write: (path, data, onProgress) =>
      client.uploadFile(path, data, (n) => { onProgress?.(n); opts.onProgress?.(path, n); }),
    remove: (path) => client.deleteFile(path),
    mkdirAll: (path) => client.makeDirAll(path),
  };
}

/**
 * Where to mirror on a given machine.
 *
 * EPOC32 keeps user documents in C:\Documents, which is what the System screen
 * shows. EPOC16 has no such folder: its internal RAM disk is M:, and the System
 * screen finds documents by scanning app folders, so \WRD\ is where a word
 * processor document has to be to appear at all.
 */
export function defaultMirrorRoot(linkProtocol: number | undefined): string {
  return linkProtocol === 2 ? 'M:\\WRD' : 'C:\\Documents';
}
