// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Long host names ⇄ 8.3 device names.
//
// This is the most important bookkeeping in the shared-drive feature, and the
// place where getting it wrong is worst: both media the projection writes are
// 8.3-only (FAT16's short entries, and FEFS by construction), so
// `Quarterly Report.wrd` has to become something like `QUARTE~1.WRD` on the
// card. If the mapping back is lost or ambiguous, the read-back writes the
// user's document under the mangled name and their file has silently been
// renamed.
//
// So the mapping is recorded, not recomputed. Once a host file has been given
// a device name it keeps it for as long as the index survives, which also means
// a projection re-run produces a byte-identical card rather than shuffling
// names around.
//
// The index lives in the app's own storage, never as a dotfile in the user's
// folder — the folder is meant to be an ordinary folder.

/** 8 + 3, uppercase, from the character set both filesystems accept. */
const SAFE_CHARS = /[^A-Z0-9_~\-!@#$%^&()]/g;

export interface NameIndex {
  version: 1;
  /** Host relative path (POSIX, original case) → device path (8.3, upper). */
  toDevice: Record<string, string>;
  /** Device path → host relative path. The reverse of the above. */
  toHost: Record<string, string>;
}

export function createIndex(): NameIndex {
  return { version: 1, toDevice: {}, toHost: {} };
}

/** Accept a stored index, or start fresh if it is missing or a future shape. */
export function adoptIndex(stored: unknown): NameIndex {
  if (!stored || typeof stored !== 'object') return createIndex();
  const s = stored as Partial<NameIndex>;
  if (s.version !== 1 || !s.toDevice || !s.toHost) return createIndex();
  return { version: 1, toDevice: { ...s.toDevice }, toHost: { ...s.toHost } };
}

/** One path segment squashed into 8.3, without collision handling. */
export function toShort83(segment: string): { stem: string; ext: string } {
  const dot = segment.lastIndexOf('.');
  const rawStem = dot > 0 ? segment.slice(0, dot) : segment;
  const rawExt = dot > 0 ? segment.slice(dot + 1) : '';
  let stem = rawStem.toUpperCase().replace(SAFE_CHARS, '_').slice(0, 8);
  const ext = rawExt.toUpperCase().replace(SAFE_CHARS, '_').slice(0, 3);
  // A name made entirely of rejected characters must still produce something
  // openable rather than an empty stem the filesystem would refuse.
  if (stem.length === 0) stem = 'FILE';
  return { stem, ext };
}

function join83(stem: string, ext: string): string {
  return ext ? `${stem}.${ext}` : stem;
}

/**
 * A `~N` variant that fits 8.3.
 *
 * The suffix eats into the stem rather than extending it, because eight
 * characters is the hard limit: `QUARTERL` + `~1` would be nine.
 */
function withDisambiguator(stem: string, ext: string, n: number): string {
  const suffix = `~${n}`;
  const keep = Math.max(1, 8 - suffix.length);
  return join83(stem.slice(0, keep) + suffix, ext);
}

/**
 * The device path for a host file, allocating one on first sight.
 *
 * `dirFor` optionally relocates the file — the SIBO app folders need a
 * root-level `.WRD` to land in `\WRD\`. It is applied to the DEVICE side only,
 * so the host folder keeps its own shape.
 *
 * Mutates `index`; the caller persists it.
 */
export function deviceNameFor(
  index: NameIndex,
  hostRel: string,
  dirFor?: (filename: string) => string | undefined,
): string {
  const existing = index.toDevice[hostRel];
  if (existing) return existing;

  const segments = hostRel.split('/').filter(Boolean);
  const filename = segments.pop() ?? 'FILE';
  const hostDirs = segments.map((seg) => toShort83(seg)).map((s) => join83(s.stem, s.ext));

  const routed = dirFor?.(filename);
  // A routing rule only applies at the top level: a file the user filed in
  // their own subfolder stays where they put it.
  const deviceDirs = routed && hostDirs.length === 0 ? [routed.toUpperCase()] : hostDirs;

  const { stem, ext } = toShort83(filename);
  const prefix = deviceDirs.length ? `${deviceDirs.join('/')}/` : '';

  // First choice is the plain mangled name; then ~1, ~2, … until one is free.
  // Bounded so a pathological folder cannot spin here.
  let candidate = prefix + join83(stem, ext);
  for (let n = 1; index.toHost[candidate] !== undefined && n < 1000; n++) {
    candidate = prefix + withDisambiguator(stem, ext, n);
  }
  if (index.toHost[candidate] !== undefined) {
    // A thousand files mangling to the same 8.3 stem. Fall back to something
    // certainly unique rather than overwriting one of them.
    candidate = `${prefix}F${Date.now().toString(36).slice(-7).toUpperCase()}${ext ? '.' + ext : ''}`;
  }

  index.toDevice[hostRel] = candidate;
  index.toHost[candidate] = hostRel;
  return candidate;
}

/**
 * The host path a device file belongs at.
 *
 * Returns null when the index has never seen it, which means the DEVICE
 * created the file. hostNameForNew decides what to call it in that case.
 */
export function hostNameFor(index: NameIndex, devicePath: string): string | null {
  return index.toHost[devicePath] ?? null;
}

/**
 * A host name for a file the device created.
 *
 * Lower-cased, because an EPOC16 volume stores everything in capitals and a
 * folder full of `REPORT.WRD` looks like something has gone wrong rather than
 * like a document. `stripDir` unwinds the app-folder routing: a file found in
 * \WRD\ came from the host root and goes back there, or the folder gradually
 * accretes EPOC's directory layout.
 */
export function hostNameForNew(
  index: NameIndex,
  devicePath: string,
  opts: { lowercase?: boolean; stripDir?: (dir: string) => boolean } = {},
): string {
  const segments = devicePath.split('/').filter(Boolean);
  const filename = segments.pop() ?? 'file';
  const dirs = opts.stripDir
    ? segments.filter((seg) => !opts.stripDir!(seg))
    : segments;
  const name = opts.lowercase ? filename.toLowerCase() : filename;
  const dirPart = dirs.map((d) => (opts.lowercase ? d.toLowerCase() : d));
  let candidate = [...dirPart, name].join('/');

  // Two device files can legitimately map to one host name once the app
  // folders are stripped (\WRD\REPORT.WRD and \SPR\REPORT.WRD both become
  // report.wrd if their extensions matched). Keep both.
  if (index.toDevice[candidate] !== undefined
      && index.toDevice[candidate] !== devicePath) {
    const dot = name.lastIndexOf('.');
    const stem = dot > 0 ? name.slice(0, dot) : name;
    const ext = dot > 0 ? name.slice(dot) : '';
    for (let n = 2; n < 1000; n++) {
      candidate = [...dirPart, `${stem} (${n})${ext}`].join('/');
      if (index.toDevice[candidate] === undefined) break;
    }
  }

  index.toDevice[candidate] = devicePath;
  index.toHost[devicePath] = candidate;
  return candidate;
}

/**
 * Drop entries for host files that no longer exist.
 *
 * Without this the index grows forever and, worse, keeps reserving 8.3 names
 * for deleted files, so a folder edited over months slowly pushes new files
 * into `~7` territory for no reason.
 */
export function pruneIndex(index: NameIndex, liveHostPaths: Iterable<string>): NameIndex {
  const live = new Set(liveHostPaths);
  const next = createIndex();
  for (const [hostRel, devicePath] of Object.entries(index.toDevice)) {
    if (!live.has(hostRel)) continue;
    next.toDevice[hostRel] = devicePath;
    next.toHost[devicePath] = hostRel;
  }
  return next;
}
