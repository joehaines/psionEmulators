// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// One host folder, projected into whatever card the machine in front of you
// actually takes.
//
// The point of the feature: a file dropped in the folder should follow you from
// a Series 3a to a netBook without being copied anywhere by hand. Those two
// machines have nothing in common at the media level — one takes a Psion SSD
// pack holding a FEFS24 volume, the other a CompactFlash card holding FAT16 —
// so "the same drive" means projecting the folder into each format on the way
// in and reading it back on the way out. Same suitcase, different plug.
//
// Everything here is pure: host files in, an image out (and the reverse). The
// controller owns the bridge, the emulator and the timing; this owns the
// translation, so it can be tested against the real filesystem writers in
// plain Node.

import * as fat16 from '../fat16.ts';
import * as fefs from '../fefs.ts';
import { appDirForFile, isAppDir } from '../siboAppDirs.ts';
import {
  adoptIndex, createIndex, deviceNameFor, hostNameFor, hostNameForNew, pruneIndex,
  type NameIndex,
} from './nameIndex.ts';

/**
 * What kind of removable medium a machine takes.
 *
 * Derived from the profile's capability flags rather than a list of device ids,
 * so a device added to the registry later is classified without touching this.
 */
export type CardFamily = 'fat16-cf' | 'fat16-mmc' | 'fefs-ssd' | 'datapak' | 'none';

export interface CardCapableProfile {
  hasCFSlot?: boolean;
  hasMmcSlot?: boolean;
  ssdSlotCount?: number;
  datapakSlotCount?: number;
}

export function familyFor(profile: CardCapableProfile | null | undefined): CardFamily {
  if (!profile) return 'none';
  if (profile.hasCFSlot) return 'fat16-cf';
  if (profile.hasMmcSlot) return 'fat16-mmc';
  if ((profile.ssdSlotCount ?? 0) > 0) return 'fefs-ssd';
  if ((profile.datapakSlotCount ?? 0) > 0) return 'datapak';
  return 'none';
}

/** Whether the shared drive can be offered at all on this machine. */
export function familySupportsProjection(family: CardFamily): boolean {
  return family === 'fat16-cf' || family === 'fat16-mmc' || family === 'fefs-ssd';
}

export function familyLabel(family: CardFamily): string {
  switch (family) {
    case 'fat16-cf': return 'CompactFlash card';
    case 'fat16-mmc': return 'MMC card';
    case 'fefs-ssd': return 'SSD pack';
    case 'datapak': return 'Datapak';
    default: return 'no removable slot';
  }
}

export interface HostFile {
  /** POSIX-separated, relative to the mount root. */
  rel: string;
  bytes: Uint8Array;
  mtimeMs: number;
}

export type SkipReason = 'no-space' | 'too-big' | 'rejected' | 'unsupported-family';

export interface SkippedFile {
  rel: string;
  reason: SkipReason;
  detail?: string;
}

export interface ProjectionResult {
  /** The card image, ready to attach. Null when the family cannot take one. */
  image: Uint8Array | null;
  /** The updated name index. Persist it — the round trip depends on it. */
  index: NameIndex;
  /** Files that did not make it, with why. Never silently dropped. */
  skipped: SkippedFile[];
  /** Bytes of the medium still free after projection. */
  freeBytes: number;
  family: CardFamily;
}

export interface ProjectOptions {
  /** Size of the medium to build. Defaults per family. */
  capacityBytes?: number;
  /** A previously-persisted index, so device names stay stable. */
  index?: unknown;
  volumeLabel?: string;
}

const DEFAULT_CAPACITY: Record<string, number> = {
  'fat16-cf': 16 * 1024 * 1024,
  'fat16-mmc': 16 * 1024 * 1024,
  'fefs-ssd': 2 * 1024 * 1024,
};

/** FEFS only comes in the sizes real packs came in. */
function nearestFlashPackSize(requested: number): number {
  const sizes = fefs.FLASH_PACK_SIZES.map((s) => s.bytes).sort((a, b) => a - b);
  return sizes.find((s) => s >= requested) ?? sizes[sizes.length - 1];
}

/**
 * Build a card image from a host folder.
 *
 * Files are written smallest-first. That is deliberate: when the folder does
 * not fit, the outcome most people want is "most of my documents are there"
 * rather than "one big file is there" — and the common case, a handful of small
 * documents behind one large one, then completes rather than being blocked by
 * it. Whatever does not fit is reported, never dropped in silence.
 */
export function projectFolder(
  files: HostFile[],
  family: CardFamily,
  opts: ProjectOptions = {},
): ProjectionResult {
  const index = pruneIndex(adoptIndex(opts.index ?? createIndex()),
                           files.map((f) => f.rel));

  if (!familySupportsProjection(family)) {
    return {
      image: null,
      index,
      skipped: files.map((f) => ({
        rel: f.rel,
        reason: 'unsupported-family' as SkipReason,
        detail: `this machine has ${familyLabel(family)}`,
      })),
      freeBytes: 0,
      family,
    };
  }

  const skipped: SkippedFile[] = [];
  const ordered = [...files].sort((a, b) => a.bytes.byteLength - b.bytes.byteLength);

  if (family === 'fefs-ssd') {
    const capacity = nearestFlashPackSize(opts.capacityBytes ?? DEFAULT_CAPACITY['fefs-ssd']);
    let pack = fefs.createFlashPack(capacity, opts.volumeLabel ?? 'SHARED');
    for (const file of ordered) {
      // Routed to the app folder EPOC16's System screen scans, so a .WRD
      // appears in Word's own file list rather than only under Disk →
      // Directory.
      const devicePath = deviceNameFor(index, file.rel, appDirForFile);
      const cut = devicePath.lastIndexOf('/');
      const dir = cut >= 0 ? devicePath.slice(0, cut).replace(/\//g, '\\') : undefined;
      const base = cut >= 0 ? devicePath.slice(cut + 1) : devicePath;
      try {
        pack = fefs.addFileToPack(pack, base, file.bytes, dir);
      } catch (err) {
        // FEFS throws rather than returning a reason when the append arena is
        // exhausted, which is the overwhelmingly likely cause here.
        skipped.push({
          rel: file.rel,
          reason: file.bytes.byteLength > capacity ? 'too-big' : 'no-space',
          detail: String(err instanceof Error ? err.message : err),
        });
      }
    }
    return { image: pack, index, skipped, freeBytes: fefs.packFreeBytes(pack), family };
  }

  // FAT16, for CompactFlash and the netpad's MMC alike — the same image either
  // way; only the label the UI uses differs.
  const capacity = opts.capacityBytes ?? DEFAULT_CAPACITY['fat16-cf'];
  const img = fat16.createBlankImage(capacity, opts.volumeLabel ?? 'PSION');
  for (const file of ordered) {
    const devicePath = deviceNameFor(index, file.rel);
    const result = fat16.writeFileAtPath(img, devicePath, file.bytes,
                                         { mtimeMs: file.mtimeMs });
    if (!result.ok) {
      const reason: SkipReason = /space/i.test(result.reason ?? '')
        ? (file.bytes.byteLength > capacity ? 'too-big' : 'no-space')
        : 'rejected';
      skipped.push({ rel: file.rel, reason, detail: result.reason });
    }
  }
  return { image: img, index, skipped, freeBytes: fat16.freeSpace(img).free, family };
}

export interface ReadbackChange {
  /** Host path to write to, POSIX-separated. */
  rel: string;
  bytes: Uint8Array;
  mtimeMs: number;
  /** True when the device created this file rather than editing ours. */
  isNew: boolean;
}

export interface ReadbackResult {
  /** Files the device wrote or changed, to be written into the host folder. */
  changed: ReadbackChange[];
  /** Host paths whose device copy has gone — deleted on the machine. */
  deleted: string[];
  index: NameIndex;
}

/**
 * Work out what the machine changed on the card.
 *
 * Compared against what the projection put there, not against the host folder:
 * the question is "what did the guest do", and only the projection knows what
 * the card looked like when it was handed over.
 *
 * FAT16 is diffed on size and mtime, which EPOC maintains. FEFS entries carry
 * no timestamp at all, so there the comparison is on content — packs are at
 * most a few megabytes, so that costs nothing worth avoiding.
 */
export function readbackImage(
  image: Uint8Array,
  family: CardFamily,
  storedIndex: unknown,
  projected: { rel: string; size: number; mtimeMs: number; bytes?: Uint8Array }[],
): ReadbackResult {
  const index = adoptIndex(storedIndex);
  const changed: ReadbackChange[] = [];
  const seenHostPaths = new Set<string>();
  const projectedByRel = new Map(projected.map((p) => [p.rel, p]));

  if (family === 'fefs-ssd') {
    for (const entry of fefs.listFiles(image)) {
      const devicePath = entry.name.replace(/\\/g, '/');
      const bytes = fefs.readFileFromPack(image, entry.name);
      const known = hostNameFor(index, devicePath);
      if (known) {
        seenHostPaths.add(known);
        const before = projectedByRel.get(known);
        // No timestamps in FEFS, so content is the only honest comparison.
        if (before?.bytes && sameBytes(before.bytes, bytes)) continue;
        if (before && !before.bytes && before.size === bytes.byteLength) continue;
        changed.push({ rel: known, bytes, mtimeMs: Date.now(), isNew: false });
      } else {
        const rel = hostNameForNew(index, devicePath,
                                   { lowercase: true, stripDir: isAppDir });
        seenHostPaths.add(rel);
        changed.push({ rel, bytes, mtimeMs: Date.now(), isNew: true });
      }
    }
  } else if (family === 'fat16-cf' || family === 'fat16-mmc') {
    for (const entry of fat16.walkAll(image)) {
      if (entry.isDir) continue;
      const known = hostNameFor(index, entry.path);
      const read = () => {
        const found = fat16.findByPath(image, entry.path);
        return found ? fat16.readFileBytes(image, found) : new Uint8Array(0);
      };
      if (known) {
        seenHostPaths.add(known);
        const before = projectedByRel.get(known);
        // A rewrite the guest made shows up as a different size, or a moved
        // timestamp. FAT's two-second granularity is why this is not an
        // equality test.
        const unchanged = before
          && before.size === entry.size
          && Math.abs((before.mtimeMs || 0) - entry.mtimeMs) <= 2000;
        if (unchanged) continue;
        changed.push({ rel: known, bytes: read(), mtimeMs: entry.mtimeMs || Date.now(), isNew: false });
      } else {
        const rel = hostNameForNew(index, entry.path, { lowercase: true });
        seenHostPaths.add(rel);
        changed.push({ rel, bytes: read(), mtimeMs: entry.mtimeMs || Date.now(), isNew: true });
      }
    }
  }

  // Anything we put on the card that is no longer there was deleted on the
  // machine. Reported rather than acted on — the controller decides, because
  // propagating a deletion is the one direction that destroys user data.
  const deleted = projected
    .map((p) => p.rel)
    .filter((rel) => !seenHostPaths.has(rel));

  return { changed, deleted, index };
}

function sameBytes(a: Uint8Array, b: Uint8Array): boolean {
  if (a.byteLength !== b.byteLength) return false;
  for (let i = 0; i < a.byteLength; i++) if (a[i] !== b[i]) return false;
  return true;
}

/**
 * Whether a pack should be rebuilt before the next projection.
 *
 * FEFS never reclaims space — a delete only clears a bit — so a pack that is
 * repeatedly projected onto creeps towards full regardless of what it holds.
 * Compacting is the software equivalent of reformatting it.
 */
export function shouldCompact(pack: Uint8Array, wantBytes: number): boolean {
  const free = fefs.packFreeBytes(pack);
  return free < wantBytes;
}
