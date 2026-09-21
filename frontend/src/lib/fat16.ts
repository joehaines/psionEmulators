// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Minimal in-memory FAT16 driver used by the CF card dialog to create blank
// images, list root-directory files, add new files, and delete files.
// Subdirectories and long filenames are read for display but not written; new
// files get 8.3 short names. This is enough for staging files onto the virtual
// CF card — EPOC happily mounts the resulting image as drive D:.
//
// Image layout: the image is always a partitioned disk. Sector 0 is an MBR
// with a single FAT16 partition starting at PART_FIRST_SEC; the BPB lives at
// PART_FIRST_SEC. EPOC's pccd_ata driver (PartitionInfo) rejects bare-BPB
// images with KErrCorrupt, so partitioning is non-optional.

const SECTOR_SIZE = 512;
// First sector of the partition. Psion's own CF formatter leaves the MBR
// at LBA 0 and starts the FAT16 partition at LBA 32 (CHS 0,1,1 with 32
// sectors/track, 2 heads). Matching that layout is important: EPOC's
// pccd_ata driver appears to have a fast-path for "card looks like it was
// formatted by EPOC" and a slow-path otherwise.
const PART_FIRST_SEC = 32;
// Bits in the directory-entry attribute byte.
const ATTR_VOLUME_ID = 0x08;
const ATTR_DIRECTORY = 0x10;
const ATTR_LFN       = 0x0F;
// MBR partition-type byte for FAT16 (>=32 MB). Matches EPOC's
// KPartitionTypeFAT16 in eka/include/partitions.h.
const MBR_PART_TYPE_FAT16 = 0x06;
const MBR_PART_TYPE_FAT16_SMALL = 0x04;

export interface Fat16Entry {
  name: string;          // display name (short 8.3, upper-cased)
  size: number;          // bytes
  isDirectory: boolean;
  attr: number;
  firstCluster: number;  // 0 if empty file
  // Byte offset of this entry within the on-disk root directory, so callers
  // can invalidate it when deleting.
  entryOffset: number;
  // Last-write time as epoch ms, from the packed +22/+24 fields the writer
  // already stamps. Read back so a host-folder projection can tell which
  // files the guest has rewritten without hashing the whole card.
  //
  // FAT stores local time with no zone, and its seconds field has two-second
  // granularity, so this is approximate by design — it is a change detector,
  // not a clock.
  mtimeMs: number;
}

export interface Fat16Info {
  bytesPerSector: number;
  sectorsPerCluster: number;
  reservedSectorCount: number;
  numFats: number;
  rootEntryCount: number;
  // Sectors inside the partition (NOT the whole image). Matches BPB fields.
  totalSectors: number;
  sectorsPerFat: number;
  // Absolute LBAs on the image. fatStartSector = partitionStartSector +
  // reservedSectorCount, etc. — callers can byte-offset into the image with
  // `lba * bytesPerSector` without needing to add the partition base.
  fatStartSector: number;
  rootDirStartSector: number;
  dataStartSector: number;
  totalClusters: number;
  clusterBytes: number;
  rootDirSectors: number;
  // Where the partition begins on the raw image (0 for legacy bare-BPB images,
  // PART_FIRST_SEC for new MBR-partitioned images produced here).
  partitionStartSector: number;
  // Label from the boot sector (extended BPB), trimmed.
  volumeLabel: string;
}

function readU8(img: Uint8Array, off: number): number { return img[off]; }
function readU16(img: Uint8Array, off: number): number {
  return img[off] | (img[off + 1] << 8);
}
function readU32(img: Uint8Array, off: number): number {
  return (img[off] | (img[off + 1] << 8) | (img[off + 2] << 16) | (img[off + 3] * 0x1000000)) >>> 0;
}
function writeU16(img: Uint8Array, off: number, value: number): void {
  img[off]     = value & 0xFF;
  img[off + 1] = (value >> 8) & 0xFF;
}
function writeU32(img: Uint8Array, off: number, value: number): void {
  img[off]     = value & 0xFF;
  img[off + 1] = (value >> 8) & 0xFF;
  img[off + 2] = (value >> 16) & 0xFF;
  img[off + 3] = (value >> 24) & 0xFF;
}

// Determine where the BPB lives on the image: sector 0 for a bare VBR, or
// the first-sector field of a primary MBR partition. Returns the absolute
// sector offset, or -1 if no valid FAT16 BPB can be located.
function findBpbSector(img: Uint8Array): number {
  if (img.length < SECTOR_SIZE) return -1;
  if (img[510] !== 0x55 || img[511] !== 0xAA) return -1;

  // Does sector 0 directly look like a BPB?
  const bpsSector0 = readU16(img, 11);
  if (bpsSector0 === 512 || bpsSector0 === 1024 || bpsSector0 === 2048 || bpsSector0 === 4096) {
    const spc = readU8(img, 13);
    const label = String.fromCharCode(...img.subarray(54, 59));
    if (spc !== 0 && (spc & (spc - 1)) === 0 && (label === 'FAT16' || label === 'FAT12')) {
      return 0;
    }
  }

  // Otherwise, scan the four MBR partition entries for a FAT16 partition.
  for (let i = 0; i < 4; i++) {
    const off = 0x1BE + i * 16;
    const type = img[off + 4];
    if (type !== MBR_PART_TYPE_FAT16 && type !== MBR_PART_TYPE_FAT16_SMALL) continue;
    const firstSec = readU32(img, off + 8);
    const numSecs  = readU32(img, off + 12);
    if (firstSec === 0 || numSecs === 0) continue;
    const bpbOff = firstSec * SECTOR_SIZE;
    if (bpbOff + SECTOR_SIZE > img.length) continue;
    if (img[bpbOff + 510] !== 0x55 || img[bpbOff + 511] !== 0xAA) continue;
    const bps = readU16(img, bpbOff + 11);
    if (bps !== 512 && bps !== 1024 && bps !== 2048 && bps !== 4096) continue;
    return firstSec;
  }
  return -1;
}

export function isFat16(img: Uint8Array): boolean {
  return findBpbSector(img) >= 0;
}

export function readInfo(img: Uint8Array): Fat16Info {
  const partitionStartSector = Math.max(0, findBpbSector(img));
  const bpb = partitionStartSector * SECTOR_SIZE;
  const bytesPerSector     = readU16(img, bpb + 11);
  const sectorsPerCluster  = readU8(img, bpb + 13);
  const reservedSectorCount = readU16(img, bpb + 14);
  const numFats            = readU8(img, bpb + 16);
  const rootEntryCount     = readU16(img, bpb + 17);
  const totalSectors16     = readU16(img, bpb + 19);
  const sectorsPerFat      = readU16(img, bpb + 22);
  const totalSectors32     = readU32(img, bpb + 32);
  const totalSectors       = totalSectors16 !== 0 ? totalSectors16 : totalSectors32;
  const rootDirSectors     = Math.ceil((rootEntryCount * 32) / bytesPerSector);
  // All derived sector numbers are absolute image LBAs so callers can index
  // directly into the image buffer.
  const fatStartSector     = partitionStartSector + reservedSectorCount;
  const rootDirStartSector = fatStartSector + numFats * sectorsPerFat;
  const dataStartSector    = rootDirStartSector + rootDirSectors;
  const dataSectors        = totalSectors - (reservedSectorCount + numFats * sectorsPerFat + rootDirSectors);
  const totalClusters      = Math.floor(dataSectors / sectorsPerCluster);
  const clusterBytes       = sectorsPerCluster * bytesPerSector;
  const volumeLabel        = String.fromCharCode(...img.subarray(bpb + 43, bpb + 54)).trim();

  return {
    bytesPerSector, sectorsPerCluster, reservedSectorCount, numFats,
    rootEntryCount, totalSectors, sectorsPerFat, fatStartSector,
    rootDirStartSector, dataStartSector, totalClusters, clusterBytes,
    rootDirSectors, partitionStartSector, volumeLabel,
  };
}

function readFatEntry(img: Uint8Array, info: Fat16Info, cluster: number): number {
  const fatOff = info.fatStartSector * info.bytesPerSector + cluster * 2;
  return readU16(img, fatOff);
}

function writeFatEntry(img: Uint8Array, info: Fat16Info, cluster: number, value: number): void {
  // Write to every FAT copy to keep them in sync (EPOC verifies this).
  for (let f = 0; f < info.numFats; f++) {
    const fatOff = (info.fatStartSector + f * info.sectorsPerFat) * info.bytesPerSector + cluster * 2;
    writeU16(img, fatOff, value);
  }
}

function clusterOffset(info: Fat16Info, cluster: number): number {
  return (info.dataStartSector + (cluster - 2) * info.sectorsPerCluster) * info.bytesPerSector;
}

function parseShortName(raw: Uint8Array): string {
  const name = String.fromCharCode(...raw.subarray(0, 8)).trimEnd();
  const ext  = String.fromCharCode(...raw.subarray(8, 11)).trimEnd();
  return ext.length > 0 ? `${name}.${ext}` : name;
}

// Cluster id that means "the root directory" in this module's directory-
// addressing scheme. Root has no cluster of its own (it's a contiguous,
// fixed-size area on FAT12/16) and cluster 0 is reserved on FAT, so it's a
// safe sentinel.
export const ROOT_DIR_CLUSTER = 0;

// Yields absolute byte offsets of every 32-byte entry slot in a directory,
// in scan order. For the root directory (cluster 0) it walks the fixed-size
// area; for sub-directories it follows the cluster chain. Callers are
// responsible for honouring the 0x00 terminator (entries past it are
// guaranteed-free but conventionally not scanned).
function* iterateDirOffsets(
  img: Uint8Array,
  info: Fat16Info,
  dirCluster: number,
): Generator<number> {
  if (dirCluster === ROOT_DIR_CLUSTER) {
    const rootOffset = info.rootDirStartSector * info.bytesPerSector;
    for (let i = 0; i < info.rootEntryCount; i++) {
      yield rootOffset + i * 32;
    }
    return;
  }
  let cluster = dirCluster;
  const entriesPerCluster = info.clusterBytes / 32;
  // Guard against pathological FAT corruption — a circular chain would loop
  // forever. The chain can't be longer than `totalClusters`.
  let safety = info.totalClusters + 2;
  while (cluster >= 2 && cluster < 0xFFF8 && safety-- > 0) {
    const base = clusterOffset(info, cluster);
    for (let i = 0; i < entriesPerCluster; i++) {
      yield base + i * 32;
    }
    cluster = readFatEntry(img, info, cluster);
  }
}

// Walk a directory's entry slots and decode short-name + LFN entries into
// a Fat16Entry[]. Shared between root and sub-directory listing.
function readDirEntries(
  img: Uint8Array,
  info: Fat16Info,
  dirCluster: number,
): Fat16Entry[] {
  const entries: Fat16Entry[] = [];
  let lfn = '';
  for (const off of iterateDirOffsets(img, info, dirCluster)) {
    const first = img[off];
    if (first === 0x00) break;        // no further entries past this slot
    if (first === 0xE5) { lfn = ''; continue; }  // deleted entry
    const attr = img[off + 11];
    if ((attr & ATTR_LFN) === ATTR_LFN) {
      // LFN fragment — 13 UCS-2 chars spread across the entry.
      const frag = decodeLFNFragment(img, off);
      lfn = frag + lfn;               // fragments come in reverse order
      continue;
    }
    if (attr & ATTR_VOLUME_ID) { lfn = ''; continue; }

    const shortName = parseShortName(img.subarray(off, off + 11));
    const name = lfn.length > 0 ? lfn.replace(/\0.*$/, '') : shortName;
    lfn = '';
    entries.push({
      name,
      size: readU32(img, off + 28),
      isDirectory: (attr & ATTR_DIRECTORY) !== 0,
      attr,
      firstCluster: readU16(img, off + 26),
      entryOffset: off,
      mtimeMs: decodeFatTimestamp(readU16(img, off + 24), readU16(img, off + 22)),
    });
  }
  return entries;
}

// List the contents of an arbitrary directory. `dirCluster === 0` lists the
// root; any other value is interpreted as the first cluster of a sub-
// directory chain. The "." and ".." entries that sub-directories carry are
// filtered out so callers see only real children.
export function listDirectory(img: Uint8Array, dirCluster: number): Fat16Entry[] {
  const info = readInfo(img);
  const entries = readDirEntries(img, info, dirCluster);
  if (dirCluster === ROOT_DIR_CLUSTER) return entries;
  return entries.filter(e => e.name !== '.' && e.name !== '..');
}

export function listRoot(img: Uint8Array): Fat16Entry[] {
  return listDirectory(img, ROOT_DIR_CLUSTER);
}

function decodeLFNFragment(img: Uint8Array, off: number): string {
  // LFN entry: 5 chars at 1, 6 chars at 14, 2 chars at 28, each UCS-2LE.
  const chars: number[] = [];
  const ranges: [number, number][] = [[1, 5], [14, 6], [28, 2]];
  for (const [start, count] of ranges) {
    for (let i = 0; i < count; i++) {
      const c = readU16(img, off + start + i * 2);
      if (c === 0xFFFF || c === 0x0000) return String.fromCharCode(...chars);
      chars.push(c);
    }
  }
  return String.fromCharCode(...chars);
}

export function readFileBytes(img: Uint8Array, entry: Fat16Entry): Uint8Array {
  const info = readInfo(img);
  const out = new Uint8Array(entry.size);
  let written = 0;
  let cluster = entry.firstCluster;
  while (cluster >= 2 && cluster < 0xFFF8 && written < entry.size) {
    const start = clusterOffset(info, cluster);
    const take = Math.min(info.clusterBytes, entry.size - written);
    out.set(img.subarray(start, start + take), written);
    written += take;
    cluster = readFatEntry(img, info, cluster);
  }
  return out;
}

function toShortName(name: string): { name: string; ext: string } | null {
  const dot = name.lastIndexOf('.');
  let base = dot >= 0 ? name.slice(0, dot) : name;
  let ext  = dot >= 0 ? name.slice(dot + 1) : '';
  base = base.toUpperCase().replace(/[^A-Z0-9_~\-!@#$%^&()]/g, '_').slice(0, 8);
  ext  = ext.toUpperCase().replace(/[^A-Z0-9_~\-!@#$%^&()]/g, '_').slice(0, 3);
  if (base.length === 0) return null;
  return { name: base, ext };
}

function findFreeClusters(img: Uint8Array, info: Fat16Info, needed: number): number[] {
  const free: number[] = [];
  for (let c = 2; c < info.totalClusters + 2 && free.length < needed; c++) {
    if (readFatEntry(img, info, c) === 0) free.push(c);
  }
  return free.length === needed ? free : [];
}

function findFreeRootEntry(img: Uint8Array, info: Fat16Info): number {
  const rootOff = info.rootDirStartSector * info.bytesPerSector;
  for (let i = 0; i < info.rootEntryCount; i++) {
    const off = rootOff + i * 32;
    const first = img[off];
    if (first === 0x00 || first === 0xE5) return off;
  }
  return -1;
}

// Find a free 32-byte entry slot in `dirCluster`. For the root directory we
// just scan the fixed slot table; for a sub-directory we scan the cluster
// chain and grow it by one cluster if every existing slot is taken. Returns
// -1 if the directory can't be extended (no free clusters / root is full).
function findOrGrowFreeDirEntry(
  img: Uint8Array,
  info: Fat16Info,
  dirCluster: number,
): number {
  if (dirCluster === ROOT_DIR_CLUSTER) {
    return findFreeRootEntry(img, info);
  }
  let cluster = dirCluster;
  let lastCluster = cluster;
  const entriesPerCluster = info.clusterBytes / 32;
  let safety = info.totalClusters + 2;
  while (cluster >= 2 && cluster < 0xFFF8 && safety-- > 0) {
    const base = clusterOffset(info, cluster);
    for (let i = 0; i < entriesPerCluster; i++) {
      const off = base + i * 32;
      const first = img[off];
      if (first === 0x00 || first === 0xE5) return off;
    }
    lastCluster = cluster;
    cluster = readFatEntry(img, info, cluster);
  }
  // Sub-directory is full — extend by one cluster, zero-fill it (so the 0x00
  // terminator is at the head and `iterateDirOffsets` sees fresh slots), and
  // link it onto the tail of the chain.
  const grown = findFreeClusters(img, info, 1);
  if (grown.length === 0) return -1;
  const newCluster = grown[0];
  const newOff = clusterOffset(info, newCluster);
  img.fill(0x00, newOff, newOff + info.clusterBytes);
  writeFatEntry(img, info, lastCluster, newCluster);
  writeFatEntry(img, info, newCluster, 0xFFFF);
  return newOff;
}

// Write a short 8.3 directory entry. `firstCluster` is the entry's data
// start (0 for empty files / empty dirs), `size` is the 32-bit byte length
// (0 for directories, by FAT convention).
function writeShortDirEntry(
  img: Uint8Array,
  entryOff: number,
  shortName: string,
  shortExt: string,
  attributes: number,
  firstCluster: number,
  size: number,
): void {
  const nameField = (shortName.padEnd(8, ' ') + shortExt.padEnd(3, ' '));
  for (let i = 0; i < 11; i++) img[entryOff + i] = nameField.charCodeAt(i);
  img[entryOff + 11] = attributes & 0xFF;
  img[entryOff + 12] = 0;
  img[entryOff + 13] = 0;
  const { date, time } = nowFields();
  writeU16(img, entryOff + 14, time); // create time
  writeU16(img, entryOff + 16, date); // create date
  writeU16(img, entryOff + 18, date); // last-access date
  writeU16(img, entryOff + 20, 0);    // first-cluster-high (FAT16: always 0)
  writeU16(img, entryOff + 22, time); // last-write time
  writeU16(img, entryOff + 24, date); // last-write date
  writeU16(img, entryOff + 26, firstCluster & 0xFFFF);
  writeU32(img, entryOff + 28, size >>> 0);
}

// Unpack FAT's date/time pair into epoch ms. Returns 0 for an unset stamp so
// callers can tell "no timestamp" from "the epoch".
function decodeFatTimestamp(date: number, time: number): number {
  if (!date) return 0;
  const year = 1980 + ((date >> 9) & 0x7F);
  const month = (date >> 5) & 0x0F;
  const day = date & 0x1F;
  if (month < 1 || month > 12 || day < 1 || day > 31) return 0;
  const hours = (time >> 11) & 0x1F;
  const minutes = (time >> 5) & 0x3F;
  const seconds = (time & 0x1F) * 2;
  // Local time, because that is what FAT records — no zone is stored.
  return new Date(year, month - 1, day, hours, minutes, seconds).getTime();
}

// Pack epoch ms into FAT's date/time pair.
function encodeFatTimestamp(ms: number): { date: number; time: number } {
  const d = new Date(ms);
  const date = ((d.getFullYear() - 1980) << 9) | ((d.getMonth() + 1) << 5) | d.getDate();
  const time = (d.getHours() << 11) | (d.getMinutes() << 5) | Math.floor(d.getSeconds() / 2);
  return { date, time };
}

/**
 * Overwrite an entry's last-write stamp.
 *
 * Used when projecting a host folder onto a card: carrying the host file's
 * mtime across means the read-back diff can compare timestamps rather than
 * content, and the file shows a sensible date on the device.
 */
export function setEntryMtime(img: Uint8Array, entry: Fat16Entry, ms: number): void {
  const { date, time } = encodeFatTimestamp(ms);
  writeU16(img, entry.entryOffset + 22, time);
  writeU16(img, entry.entryOffset + 24, date);
}

// Packed FAT timestamp for "now".
function nowFields(): { date: number; time: number } {
  const d = new Date();
  const date = ((d.getFullYear() - 1980) << 9) | ((d.getMonth() + 1) << 5) | d.getDate();
  const time = (d.getHours() << 11) | (d.getMinutes() << 5) | Math.floor(d.getSeconds() / 2);
  return { date, time };
}

// FAT directory-entry attribute byte values, OR-able. The 5mx Pro
// bootloader's payload (SYS$ROM.BIN on its hand-built default card)
// uses Hidden | System so EPOC's mount path skips it instead of
// trying to interpret it as a user data file and getting wedged.
export const FAT_ATTR_ARCHIVE = 0x20;
export const FAT_ATTR_HIDDEN  = 0x02;
export const FAT_ATTR_SYSTEM  = 0x04;

export interface AddResult {
  ok: boolean;
  reason?: string;
}

// Add a file to an arbitrary directory. `dirCluster === 0` writes to the
// root (same semantics as the legacy `addFile`); any other value points at
// the first cluster of a sub-directory.
export function addFileToDirectory(
  img: Uint8Array,
  dirCluster: number,
  name: string,
  data: Uint8Array,
  attributes: number = FAT_ATTR_ARCHIVE,
): AddResult {
  const info = readInfo(img);
  const short = toShortName(name);
  if (!short) return { ok: false, reason: 'Invalid filename' };

  // Collision check — FAT lookup is case-insensitive on short names.
  for (const e of listDirectory(img, dirCluster)) {
    if (e.name.toUpperCase() === name.toUpperCase()) {
      return { ok: false, reason: 'A file with that name already exists' };
    }
  }

  const clustersNeeded = data.byteLength === 0
    ? 0
    : Math.ceil(data.byteLength / info.clusterBytes);
  const clusters = findFreeClusters(img, info, clustersNeeded);
  if (clustersNeeded > 0 && clusters.length === 0) {
    return { ok: false, reason: 'Not enough free space on card' };
  }
  const entryOff = findOrGrowFreeDirEntry(img, info, dirCluster);
  if (entryOff < 0) {
    return {
      ok: false,
      reason: dirCluster === ROOT_DIR_CLUSTER
        ? 'Root directory is full'
        : 'Directory is full and no space to extend it',
    };
  }

  // Write file data across allocated clusters.
  for (let i = 0; i < clusters.length; i++) {
    const off = clusterOffset(info, clusters[i]);
    const chunkStart = i * info.clusterBytes;
    const chunk = data.subarray(chunkStart, chunkStart + info.clusterBytes);
    img.set(chunk, off);
    // Zero-fill the rest of the cluster for a clean image.
    if (chunk.length < info.clusterBytes) {
      img.fill(0, off + chunk.length, off + info.clusterBytes);
    }
  }
  // Chain the cluster list in the FAT, terminating with 0xFFFF.
  for (let i = 0; i < clusters.length; i++) {
    const next = (i + 1 < clusters.length) ? clusters[i + 1] : 0xFFFF;
    writeFatEntry(img, info, clusters[i], next);
  }

  writeShortDirEntry(
    img,
    entryOff,
    short.name,
    short.ext,
    attributes,
    clusters.length > 0 ? clusters[0] : 0,
    data.byteLength,
  );
  return { ok: true };
}

export function addFile(
  img: Uint8Array,
  name: string,
  data: Uint8Array,
  attributes: number = FAT_ATTR_ARCHIVE,
): AddResult {
  return addFileToDirectory(img, ROOT_DIR_CLUSTER, name, data, attributes);
}

export interface CreateDirResult extends AddResult {
  cluster?: number;
}

// Create an empty sub-directory inside `parentCluster` (0 = root). Allocates
// one cluster for the new directory, seeds it with the canonical "." / ".."
// entries, and writes a directory entry into the parent. Returns the new
// directory's first cluster so callers can immediately navigate into it.
export function createDirectory(
  img: Uint8Array,
  parentCluster: number,
  name: string,
): CreateDirResult {
  const info = readInfo(img);
  const short = toShortName(name);
  if (!short) return { ok: false, reason: 'Invalid directory name' };

  for (const e of listDirectory(img, parentCluster)) {
    if (e.name.toUpperCase() === name.toUpperCase()) {
      return { ok: false, reason: 'A file or folder with that name already exists' };
    }
  }

  const grown = findFreeClusters(img, info, 1);
  if (grown.length === 0) return { ok: false, reason: 'Not enough free space on card' };
  const dirCluster = grown[0];

  const entryOff = findOrGrowFreeDirEntry(img, info, parentCluster);
  if (entryOff < 0) {
    return {
      ok: false,
      reason: parentCluster === ROOT_DIR_CLUSTER
        ? 'Root directory is full'
        : 'Parent directory is full and no space to extend it',
    };
  }

  // Zero the new cluster (so iteration sees a clean terminator at the end)
  // and seed "."/".." before linking the FAT chain — keeps the on-disk view
  // consistent even if a later step were to fail.
  const dirOff = clusterOffset(info, dirCluster);
  img.fill(0x00, dirOff, dirOff + info.clusterBytes);
  // "." points to the new dir itself.
  writeShortDirEntry(img, dirOff,      '.',  '', ATTR_DIRECTORY, dirCluster, 0);
  // ".." points to the parent. By FAT convention, ".." in a top-level dir
  // (parent == root) carries cluster 0.
  writeShortDirEntry(
    img, dirOff + 32, '..', '', ATTR_DIRECTORY,
    parentCluster === ROOT_DIR_CLUSTER ? 0 : parentCluster, 0,
  );
  writeFatEntry(img, info, dirCluster, 0xFFFF);

  writeShortDirEntry(
    img, entryOff, short.name, short.ext, ATTR_DIRECTORY, dirCluster, 0,
  );
  return { ok: true, cluster: dirCluster };
}

export function deleteEntry(img: Uint8Array, entry: Fat16Entry): void {
  const info = readInfo(img);
  // Free the cluster chain.
  let cluster = entry.firstCluster;
  while (cluster >= 2 && cluster < 0xFFF8) {
    const next = readFatEntry(img, info, cluster);
    writeFatEntry(img, info, cluster, 0);
    cluster = next;
  }
  // Mark the directory entry deleted (0xE5 in the name field's first byte).
  img[entry.entryOffset] = 0xE5;
}

// Recursively delete a directory entry and everything underneath it. For a
// file this is identical to `deleteEntry`; for a directory it first walks
// the tree and frees every descendant so no clusters are orphaned.
export function deleteEntryRecursive(img: Uint8Array, entry: Fat16Entry): void {
  if (entry.isDirectory && entry.firstCluster >= 2) {
    for (const child of listDirectory(img, entry.firstCluster)) {
      deleteEntryRecursive(img, child);
    }
  }
  deleteEntry(img, entry);
}

// Maximum FAT volume-label length — 11 bytes, the size of the 8.3 name field.
export const MAX_VOLUME_LABEL_LEN = 11;

// Characters disallowed in a FAT volume label. Mirrors the FAT short-name
// rules EPOC's formatter enforces: no control chars and none of these
// punctuation symbols (spaces are allowed, including internal ones).
const ILLEGAL_LABEL_CHARS = /[\x00-\x1F"*+,./:;<=>?[\\\]|]/;

export interface SetLabelResult { ok: boolean; reason?: string }

// Normalise a user-typed label into the form FAT stores: trimmed, upper-cased
// and space-padded to 11 bytes. Returns null if it's too long or contains an
// illegal character.
function normaliseLabel(label: string): string | null {
  const up = label.trim().toUpperCase();
  if (up.length > MAX_VOLUME_LABEL_LEN) return null;
  if (ILLEGAL_LABEL_CHARS.test(up)) return null;
  return up.padEnd(MAX_VOLUME_LABEL_LEN, ' ');
}

// Find the root-directory volume-label entry (ATTR_VOLUME_ID set, not an LFN
// fragment or sub-directory). Returns its byte offset, or -1 if absent.
function findVolumeLabelEntry(img: Uint8Array, info: Fat16Info): number {
  for (const off of iterateDirOffsets(img, info, ROOT_DIR_CLUSTER)) {
    const first = img[off];
    if (first === 0x00) break;
    if (first === 0xE5) continue;
    const attr = img[off + 11];
    if ((attr & ATTR_LFN) === ATTR_LFN) continue;
    if ((attr & ATTR_VOLUME_ID) && !(attr & ATTR_DIRECTORY)) return off;
  }
  return -1;
}

// Rename the volume (the CF "partition name"). Writes the label into the
// boot-sector BPB field AND, to mirror EPOC's CFatMountCB::SetVolumeL, the
// root-directory volume-label entry — EPOC reads that entry first at mount and
// only falls back to the BPB label when it's absent, so the two must agree.
// A blank label clears the name (BPB filled with spaces, root entry removed).
// Returns ok:false with a reason for an over-long / illegal label or full root.
export function setVolumeLabel(img: Uint8Array, label: string): SetLabelResult {
  const normalised = normaliseLabel(label);
  if (normalised === null) {
    return {
      ok: false,
      reason: `Label must be ≤ ${MAX_VOLUME_LABEL_LEN} characters and avoid " * / : < > ? \\ |`,
    };
  }
  if (!isFat16(img)) return { ok: false, reason: 'Image is not a FAT16 volume' };

  const info = readInfo(img);
  // BPB extended-boot-record label field: offset 43, 11 bytes.
  const bpb = info.partitionStartSector * SECTOR_SIZE;
  for (let i = 0; i < MAX_VOLUME_LABEL_LEN; i++) {
    img[bpb + 43 + i] = normalised.charCodeAt(i);
  }

  const existing = findVolumeLabelEntry(img, info);
  if (normalised.trim().length === 0) {
    // Clearing the label — drop any existing root volume-label entry.
    if (existing >= 0) img[existing] = 0xE5;
    return { ok: true };
  }
  if (existing >= 0) {
    // Overwrite the 11-byte name field in place; leave attrs/timestamps.
    for (let i = 0; i < MAX_VOLUME_LABEL_LEN; i++) {
      img[existing + i] = normalised.charCodeAt(i);
    }
    return { ok: true };
  }
  // No existing entry — create one in the first free root slot. The volume-
  // label entry carries the raw 11-byte name (not split into 8.3), cluster 0
  // and size 0, so it's written directly rather than via writeShortDirEntry.
  const entryOff = findFreeRootEntry(img, info);
  if (entryOff < 0) return { ok: false, reason: 'Root directory is full' };
  img.fill(0, entryOff, entryOff + 32);
  for (let i = 0; i < MAX_VOLUME_LABEL_LEN; i++) {
    img[entryOff + i] = normalised.charCodeAt(i);
  }
  img[entryOff + 11] = ATTR_VOLUME_ID;
  const { date, time } = nowFields();
  writeU16(img, entryOff + 14, time); // create time
  writeU16(img, entryOff + 16, date); // create date
  writeU16(img, entryOff + 18, date); // last-access date
  writeU16(img, entryOff + 22, time); // last-write time
  writeU16(img, entryOff + 24, date); // last-write date
  return { ok: true };
}

// Creates an empty partitioned FAT16 image of `sizeBytes`, formatted to match
// what the Psion's own CF formatter emits (verified against
// reference/psion-formatted-cf.img):
//   - MBR partition 1: bootable, type 0x04 for <32MB or 0x06 otherwise,
//     starts at LBA 32 (matches PART_FIRST_SEC), CHS 0/1/1
//   - BPB at LBA 32 with OEM "EPOC", drive byte 0x80, sectors/track 16,
//     heads 2, volume label "NO NAME   " — EPOC's fingerprint
//   - Cluster size 1 sector (512 B) whenever that keeps clusters in the FAT16
//     band, otherwise bumped to 2/4/8 — Psion defaults to 1 on 20 MB cards
//   - FAT[0..1] = {0xFFF8, 0xFFFF}, FAT[2..3] = {0xFFFF, 0xFFFF} reserved
//   - Empty data/FAT area filled with 0xFF, not 0x00 (flash-erased semantics)
//   - No volume-label root-dir entry (Psion's formatter doesn't write one)
// The `label` parameter is kept for call-site compatibility but ignored:
// Psion always writes "NO NAME   " and changing it breaks the EPOC fast-path.
export function createBlankImage(sizeBytes: number, _label = 'PSION CF'): Uint8Array {
  void _label;
  if (sizeBytes < 4 * 1024 * 1024 || sizeBytes > 2 * 1024 * 1024 * 1024) {
    throw new Error('Image size must be between 4 MB and 2 GB');
  }
  // Psion fills unallocated sectors with 0xFF (CF flash-erased state). Start
  // with 0xFF everywhere and overwrite the MBR/BPB/FAT regions below.
  const img = new Uint8Array(sizeBytes).fill(0xFF);
  const bytesPerSector = 512;
  const totalSectorsOnImage = Math.floor(sizeBytes / bytesPerSector);
  const partTotalSectors = totalSectorsOnImage - PART_FIRST_SEC;

  const reservedSectorCount = 1;
  const numFats = 2;
  const rootEntryCount = 512;
  const rootDirSectors = Math.ceil((rootEntryCount * 32) / bytesPerSector);

  // Pick the smallest cluster size that produces both (a) a FAT16
  // cluster count in-range [4085, 65524] AND (b) sectorsPerFat ≤ 255.
  // EPOC's FAT16 mounter fails to allocate the FAT cache when
  // BPB_FATSz16 > ~256 sectors — the symptom we see is EPOC re-reading
  // the BPB in a tight retry loop and reporting the card as corrupt
  // in the file manager. Empirically 255-sector FATs mount cleanly;
  // 508 does not. For a 32 MB card this bumps the cluster size from
  // 1 sector (spf=508) to 2 sectors (spf=255); for 64 MB, 8 sectors
  // (spf=128); for 128 MB, 16 sectors (spf=128).
  let sectorsPerCluster = 0;
  let sectorsPerFat = 0;
  for (const candidate of [1, 2, 4, 8, 16, 32, 64]) {
    const tmp1 = partTotalSectors - reservedSectorCount - rootDirSectors;
    const tmp2 = Math.floor((256 * candidate + numFats) / 2);
    const spf = Math.max(1, Math.ceil(tmp1 / tmp2));
    const dataSectors = partTotalSectors - reservedSectorCount - numFats * spf - rootDirSectors;
    const clusters = Math.floor(dataSectors / candidate);
    if (clusters >= 4085 && clusters <= 65524 && spf <= 255) {
      sectorsPerCluster = candidate;
      sectorsPerFat = spf;
      break;
    }
  }
  if (sectorsPerCluster === 0) {
    throw new Error('Image size cannot be formatted as FAT16');
  }

  // Zero the MBR up to the partition table, signature and reserved tail.
  // The partition-table area and the data/FAT region get their own writes.
  img.fill(0x00, 0, 0x1BE);
  img.fill(0x00, 0x1BE, 0x200);

  // ---- MBR at sector 0 --------------------------------------------------
  const peOff = 0x1BE;
  const partType = partTotalSectors <= 0xFFFF
    ? MBR_PART_TYPE_FAT16_SMALL
    : MBR_PART_TYPE_FAT16;
  img[peOff + 0] = 0x80;                              // bootable flag
  img[peOff + 1] = 0x01;                              // start head
  img[peOff + 2] = 0x01;                              // start sector
  img[peOff + 3] = 0x00;                              // start cylinder
  img[peOff + 4] = partType;                          // partition type
  // Fake CHS end that's accepted by Psion + Windows. Psion's own image
  // encodes end-CHS exactly, but EPOC only reads LBA fields.
  img[peOff + 5] = 0x01;                              // end head
  img[peOff + 6] = 0xA0;                              // end sector (CHS-packed)
  img[peOff + 7] = 0x62;                              // end cylinder
  writeU32(img, peOff + 8,  PART_FIRST_SEC);          // first LBA
  writeU32(img, peOff + 12, partTotalSectors);        // length in sectors
  img[510] = 0x55; img[511] = 0xAA;

  // ---- BPB / boot sector at PART_FIRST_SEC ------------------------------
  const bpb = PART_FIRST_SEC * bytesPerSector;
  img.fill(0x00, bpb, bpb + 0x3E);  // zero BPB-covered area; boot code stays 0xFF
  // Psion's 3-byte jump is E9 00 90 (long jump + NOP pad).
  img[bpb + 0] = 0xE9; img[bpb + 1] = 0x00; img[bpb + 2] = 0x90;
  // OEM ID: "EPOC" padded with NULs — Psion's formatter signature.
  const oemName = 'EPOC';
  for (let i = 0; i < oemName.length; i++) img[bpb + 3 + i] = oemName.charCodeAt(i);
  writeU16(img, bpb + 11, bytesPerSector);
  img[bpb + 13] = sectorsPerCluster;
  writeU16(img, bpb + 14, reservedSectorCount);
  img[bpb + 16] = numFats;
  writeU16(img, bpb + 17, rootEntryCount);
  writeU16(img, bpb + 19, partTotalSectors <= 0xFFFF ? partTotalSectors : 0);
  img[bpb + 21] = 0xF8;                                        // fixed media
  writeU16(img, bpb + 22, sectorsPerFat);
  writeU16(img, bpb + 24, 32);                                 // sectors/track
  writeU16(img, bpb + 26, 2);                                  // heads
  writeU32(img, bpb + 28, PART_FIRST_SEC);                     // hidden sectors
  writeU32(img, bpb + 32, partTotalSectors > 0xFFFF ? partTotalSectors : 0);
  img[bpb + 36] = 0x80;                                        // drive number (matches Psion)
  img[bpb + 37] = 0;
  img[bpb + 38] = 0x29;                                        // extended boot sig
  writeU32(img, bpb + 39, Math.floor(Math.random() * 0xFFFFFFFF));
  const labelField = 'NO NAME    ';                            // Psion always writes this
  for (let i = 0; i < 11; i++) img[bpb + 43 + i] = labelField.charCodeAt(i);
  const fsType = 'FAT16   ';
  for (let i = 0; i < 8; i++) img[bpb + 54 + i] = fsType.charCodeAt(i);
  img[bpb + 510] = 0x55; img[bpb + 511] = 0xAA;

  // ---- FATs -------------------------------------------------------------
  // Zero both FAT copies (the data area keeps its 0xFF fill). Write
  // entries 0-1 (media + EOC markers) and reserve entries 2-3 as EOC, to
  // match Psion's on-disk FAT layout.
  const info = readInfo(img);
  for (let f = 0; f < numFats; f++) {
    const fatOff = (info.fatStartSector + f * info.sectorsPerFat) * info.bytesPerSector;
    const fatBytes = info.sectorsPerFat * info.bytesPerSector;
    img.fill(0x00, fatOff, fatOff + fatBytes);
    img[fatOff + 0] = 0xF8; img[fatOff + 1] = 0xFF;  // entry 0
    img[fatOff + 2] = 0xFF; img[fatOff + 3] = 0xFF;  // entry 1 (EOC)
    img[fatOff + 4] = 0xFF; img[fatOff + 5] = 0xFF;  // entry 2 (reserved, EOC)
    img[fatOff + 6] = 0xFF; img[fatOff + 7] = 0xFF;  // entry 3 (reserved, EOC)
  }

  // ---- Root directory ---------------------------------------------------
  // Psion's formatter doesn't leave any volume-label entry; EPOC uses the
  // BPB label directly. Zero the whole root so `listRoot` stops at the
  // first slot instead of wandering into the 0xFF data fill.
  const rootOff = info.rootDirStartSector * info.bytesPerSector;
  img.fill(0x00, rootOff, rootOff + rootDirSectors * bytesPerSector);

  return img;
}

// ── Path-addressed helpers ───────────────────────────────────────────────
//
// Everything above is cluster-addressed, which is the right primitive but a
// poor fit for projecting a host folder: that arrives as a list of relative
// paths, and turning each into a cluster walk at every call site would be the
// same loop written many times.
//
// Paths here are POSIX-separated and relative to the card root ('DOCS/A.TXT'),
// case-insensitive as FAT is.

function splitPath(pathStr: string): string[] {
  return pathStr.split(/[/\\]+/).filter((seg) => seg.length > 0);
}

/** Resolve a directory path to its first cluster, or null if absent. */
export function findDirCluster(img: Uint8Array, dirPath: string): number | null {
  let cluster = ROOT_DIR_CLUSTER;
  for (const seg of splitPath(dirPath)) {
    const match = listDirectory(img, cluster)
      .find((e) => e.isDirectory && e.name.toUpperCase() === seg.toUpperCase());
    if (!match) return null;
    cluster = match.firstCluster;
  }
  return cluster;
}

/** The entry at `filePath`, or null. */
export function findByPath(img: Uint8Array, filePath: string): Fat16Entry | null {
  const segs = splitPath(filePath);
  if (segs.length === 0) return null;
  const name = segs.pop() as string;
  const dir = findDirCluster(img, segs.join('/'));
  if (dir === null) return null;
  return listDirectory(img, dir).find((e) => e.name.toUpperCase() === name.toUpperCase()) ?? null;
}

/** List a directory by path. Returns null when the directory is not there. */
export function listPath(img: Uint8Array, dirPath: string): Fat16Entry[] | null {
  const cluster = findDirCluster(img, dirPath);
  return cluster === null ? null : listDirectory(img, cluster);
}

export interface EnsureDirResult extends AddResult {
  cluster?: number;
}

/**
 * Create every missing level of `dirPath`, returning the deepest cluster.
 *
 * Stops at the first failure and reports it, rather than leaving the caller to
 * discover a half-made tree — a card that has run out of directory entries
 * partway is exactly when a projection needs to say so.
 */
export function ensureDirPath(img: Uint8Array, dirPath: string): EnsureDirResult {
  let cluster = ROOT_DIR_CLUSTER;
  for (const seg of splitPath(dirPath)) {
    const existing = listDirectory(img, cluster)
      .find((e) => e.name.toUpperCase() === seg.toUpperCase());
    if (existing) {
      if (!existing.isDirectory) {
        return { ok: false, reason: `${seg} exists as a file, not a folder` };
      }
      cluster = existing.firstCluster;
      continue;
    }
    const made = createDirectory(img, cluster, seg);
    if (!made.ok || made.cluster === undefined) {
      return { ok: false, reason: made.reason ?? `could not create ${seg}` };
    }
    cluster = made.cluster;
  }
  return { ok: true, cluster };
}

/** Remove the entry at `filePath`, recursively if it is a directory. */
export function deleteAtPath(img: Uint8Array, filePath: string): boolean {
  const entry = findByPath(img, filePath);
  if (!entry) return false;
  if (entry.isDirectory) deleteEntryRecursive(img, entry);
  else deleteEntry(img, entry);
  return true;
}

export interface WriteFileResult extends AddResult {
  /** True when an existing file of the same name was replaced. */
  replaced?: boolean;
}

/**
 * Write `data` to `filePath`, creating directories and replacing any existing
 * file of that name.
 *
 * Replace-in-place is what addFileToDirectory deliberately does not do — it
 * refuses a collision — but a projection re-runs over the same folder
 * constantly, so "the file is already there" is the normal case rather than an
 * error. Deleting first also reclaims the old file's clusters, so rewriting a
 * file repeatedly does not consume the card.
 */
export function writeFileAtPath(
  img: Uint8Array,
  filePath: string,
  data: Uint8Array,
  opts: { mtimeMs?: number; attributes?: number } = {},
): WriteFileResult {
  const segs = splitPath(filePath);
  if (segs.length === 0) return { ok: false, reason: 'empty path' };
  const name = segs.pop() as string;

  const dir = ensureDirPath(img, segs.join('/'));
  if (!dir.ok || dir.cluster === undefined) {
    return { ok: false, reason: dir.reason ?? 'could not create the folder' };
  }

  const existing = listDirectory(img, dir.cluster)
    .find((e) => e.name.toUpperCase() === name.toUpperCase());
  if (existing) {
    if (existing.isDirectory) {
      return { ok: false, reason: `${name} exists as a folder` };
    }
    deleteEntry(img, existing);
  }

  const added = addFileToDirectory(img, dir.cluster, name, data,
                                   opts.attributes ?? FAT_ATTR_ARCHIVE);
  if (!added.ok) return { ...added, replaced: !!existing };

  if (opts.mtimeMs !== undefined && opts.mtimeMs > 0) {
    const written = listDirectory(img, dir.cluster)
      .find((e) => e.name.toUpperCase() === name.toUpperCase());
    if (written) setEntryMtime(img, written, opts.mtimeMs);
  }
  return { ok: true, replaced: !!existing };
}

/**
 * Every file on the card, as POSIX-separated paths relative to the root.
 *
 * Directories are reported too (isDir), so an empty one can be recreated on
 * the host side of a round trip.
 */
export function walkAll(
  img: Uint8Array,
  maxDepth = 8,
): { path: string; size: number; mtimeMs: number; isDir: boolean }[] {
  const out: { path: string; size: number; mtimeMs: number; isDir: boolean }[] = [];
  const visit = (cluster: number, prefix: string, depth: number): void => {
    if (depth > maxDepth) return;
    for (const e of listDirectory(img, cluster)) {
      // The volume label lives in the root directory as an entry; it is
      // metadata, not a file.
      if (e.attr & ATTR_VOLUME_ID) continue;
      const rel = prefix ? `${prefix}/${e.name}` : e.name;
      if (e.isDirectory) {
        out.push({ path: rel, size: 0, mtimeMs: e.mtimeMs, isDir: true });
        visit(e.firstCluster, rel, depth + 1);
      } else {
        out.push({ path: rel, size: e.size, mtimeMs: e.mtimeMs, isDir: false });
      }
    }
  };
  visit(ROOT_DIR_CLUSTER, '', 0);
  return out;
}

export function freeSpace(img: Uint8Array): { free: number; used: number; total: number } {
  const info = readInfo(img);
  let freeClusters = 0;
  for (let c = 2; c < info.totalClusters + 2; c++) {
    if (readFatEntry(img, info, c) === 0) freeClusters++;
  }
  const total = info.totalClusters * info.clusterBytes;
  const free = freeClusters * info.clusterBytes;
  return { free, used: total - free, total };
}
