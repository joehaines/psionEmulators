// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Psion FEFS24 (Flash EPOC File System, 24-bit pointers) reader/writer.
//
// The on-disk layout below was reverse-engineered byte-for-byte against
// real factory Flash SSD dumps (MAME softlist `psion_ssd`, e.g.
// Games_3a.rom) and cross-checked against thelastpsion/fefstool
// (sibo.h record offsets) — both agree. The previous revision of this
// file used a home-grown record layout (flags-first entries, 24-bit
// data sizes) that EPOC16 does NOT recognise: packs written with it
// always mounted as "Unformatted!" on a real ROM. Every offset here is
// now the genuine article, and packs produced by this writer have been
// verified to mount and list files on the emulated Series 3c System
// screen (Disk → Directory).
//
// ── Image header (FEFS24, factory "ROM image" variant) ───────────────
//   0x00  2   magic 0xF1A5 (LE bytes A5 F1)
//   0x02  4   unique id — factory packs leave the first two bytes as
//             erased 0xFF and program the last two
//   0x06  4   01 00 01 00 (fixed on every sampled pack)
//   0x0A  1   pointer size: 0x00 = FEFS24, nonzero = FEFS32
//   0x0B  3   root directory entry pointer (factory: 0x000045)
//   0x0E  11  volume name, 8+3, space padded
//   0x19  4   flash/format count. 0xFFFFFFFF marks a factory "ROM
//             image"; a pack formatted in the field carries a count.
//             EPOC16 mounts either form on a Flash or write-protected
//             pack, so the packs this module builds leave it erased.
//   0x1D  28  id string "Copyright (c) Psion Plc 1991" (ROM position;
//             field-formatted Flash packs carry it at 0x21 instead,
//             after a 4-byte size field)
//   0x39  12  erased (0xFF)
//   0x45  ..  root directory entry ("ROOT", directory chain start)
//
// ── Directory entry (26 bytes) ────────────────────────────────────────
//   +0   3   next sibling entry pointer (0xFFFFFF = none)
//   +3   8   name, space padded
//   +11  3   extension, space padded
//   +14  1   flags — see ENTRY_FLAG_* (note: erased-flash friendly,
//            all mutations only clear bits 1→0)
//   +15  3   first child entry pointer (0xFFFFFF = none)
//   +18  3   alternative record pointer (0xFFFFFF = none)
//   +21  1   properties — see PROP_*
//   +22  2   timecode  (hh*0x800 + mm*0x20 + ss/2)
//   +24  2   datecode  ((yy-1980)*0x200 + month*0x20 + day)
//
// ── File entry (31 bytes) ─────────────────────────────────────────────
//   Same 26 bytes as a directory entry (with FLAG_IS_FILE clear — the
//   flag bit is active-LOW, see below), then:
//   +26  3   first data record pointer
//   +29  2   first data record length (16-bit! files > 64K chain
//            continuation records)
//
// ── File continuation record (17 bytes) ──────────────────────────────
//   +0   1   flags (same bit meanings)
//   +1   3   next record pointer
//   +4   3   alternative record pointer
//   +7   3   data record pointer
//   +10  2   data record length
//   +12  1   properties
//   +13  2   timecode
//   +15  2   datecode
//
// Flag bits are designed so every filesystem mutation on real Flash
// only programs bits 1→0 (erased = 0xFF):
//   bit 0  entry is valid        (1 = valid; DELETE clears it)
//   bit 1  properties/datetime valid
//   bit 2  0 = directory, 1 = file
//   bit 3  1 = NO child/continuation record (cleared when one is added)
//   bit 4  1 = NO alternative record
//   bit 5  1 = last sibling      (cleared when a next-pointer is set)
//   bits 6-7 reserved (left 1)

// ── Public API ────────────────────────────────────────────────────────

export interface FefsFile {
  // Path within the pack, e.g. "HELLO.TXT" or "WRD\\REPORT.WRD".
  name: string;
  size: number;
}

// The pack type presented to the Psion. It models a hardware strap on
// the pack PCB, not the contents:
//   'ram'       SRAM pack — read/write, formatted by EPOC16 as a FAT
//               volume ("PSION1.0" boot record), not FEFS.
//   'flash'     Type 1 Flash pack — read/write via the 28F0xx command
//               interface; the FEFS packs this module builds.
//   'protected' hardware write-protected flash, the strap on a factory
//               system disk (MC200/MC400 ROM:: disk). EPOC16 mounts it
//               read-only and labels the drive "Protected".
export type PackKind = 'ram' | 'flash' | 'protected';

// Pack sizes Psion shipped as branded Flash SSDs.
export const FLASH_PACK_SIZES: { label: string; bytes: number }[] = [
  { label: '128K', bytes: 0x020000 },
  { label: '256K', bytes: 0x040000 },
  { label: '512K', bytes: 0x080000 },
  { label: '1M',   bytes: 0x100000 },
  { label: '2M',   bytes: 0x200000 },
  { label: '4M',   bytes: 0x400000 },
  { label: '8M',   bytes: 0x800000 },
];

const NULL_PTR     = 0xFFFFFF;
const ROOT_PTR_OFF = 0x0B;
const VOL_NAME_OFF = 0x0E;
const ROOT_DIR_OFF = 0x45;
const DIR_ENTRY_SIZE  = 26;
const FILE_ENTRY_SIZE = 31;
const FILE_RECORD_SIZE = 17;
// 16-bit data-record length; chunk big files into 32K data records.
const DATA_CHUNK = 0x8000;

const FLAG_VALID       = 1 << 0;
const FLAG_IS_FILE     = 1 << 2;
const FLAG_NO_CHILD    = 1 << 3;
const FLAG_LAST_SIBLING = 1 << 5;

const PROP_DIRECTORY = 1 << 4;

const ID_STRING = 'Copyright (c) Psion Plc 1991';

// ── Small helpers ─────────────────────────────────────────────────────

function writeU24(buf: Uint8Array, off: number, v: number) {
  buf[off    ] =  v         & 0xFF;
  buf[off + 1] = (v >>>  8) & 0xFF;
  buf[off + 2] = (v >>> 16) & 0xFF;
}

function readU24(buf: Uint8Array, off: number): number {
  return buf[off] | (buf[off + 1] << 8) | (buf[off + 2] << 16);
}

function writeU16(buf: Uint8Array, off: number, v: number) {
  buf[off    ] =  v        & 0xFF;
  buf[off + 1] = (v >>> 8) & 0xFF;
}

function readU16(buf: Uint8Array, off: number): number {
  return buf[off] | (buf[off + 1] << 8);
}

function writeStr(buf: Uint8Array, off: number, s: string, pad: number) {
  for (let i = 0; i < pad; i++) {
    buf[off + i] = i < s.length ? s.charCodeAt(i) & 0xFF : 0x20;
  }
}

function readStr(buf: Uint8Array, off: number, len: number): string {
  let s = '';
  for (let i = 0; i < len; i++) {
    const c = buf[off + i];
    if (c === 0 || c === 0x20 || c === 0xFF) break;
    s += String.fromCharCode(c);
  }
  return s;
}

// Psion timestamp encoding (see fefstool psidateftime).
function psiDateTime(d = new Date()): { time: number; date: number } {
  return {
    time: d.getHours() * 0x800 + d.getMinutes() * 0x20 + (d.getSeconds() >> 1),
    date: (d.getFullYear() - 1980) * 0x200 + (d.getMonth() + 1) * 0x20 + d.getDate(),
  };
}

// 8.3 name validation; FEFS stores upper-case names.
function normaliseName(name: string): { stem: string; ext: string } {
  const dot  = name.lastIndexOf('.');
  const stem = (dot < 0 ? name : name.slice(0, dot)).toUpperCase();
  const ext  = (dot < 0 ? ''   : name.slice(dot + 1)).toUpperCase();
  if (stem.length === 0 || stem.length > 8) {
    throw new Error(`FEFS filename "${name}" must have 1-8 chars before the dot`);
  }
  if (ext.length > 3) {
    throw new Error(`FEFS extension "${ext}" must be 0-3 chars`);
  }
  const ok = /^[A-Z0-9_$~!#%&()@^`'{}-]+$/;
  if (!ok.test(stem) || (ext && !ok.test(ext))) {
    throw new Error(`FEFS filename "${name}" contains unsupported characters`);
  }
  return { stem, ext };
}

// Splits a directory path ("BLADES", "GAMES\\DATA", …) into validated
// 8-char components. FEFS directories nest like any filing system —
// the single-level limit was a writer limitation, not a format one.
function normaliseDirPath(dir: string | undefined): string[] {
  if (!dir) return [];
  const parts = dir.toUpperCase().split(/[\\/]+/).filter(Boolean);
  for (const d of parts) {
    if (d.length > 8 || !/^[A-Z0-9_$~!#%&()@^`'{}-]+$/.test(d)) {
      throw new Error(`FEFS directory name "${d}" is invalid`);
    }
  }
  return parts;
}

// ── createFlashPack ──────────────────────────────────────────────────
// 0xFF-filled image with the factory FEFS24 header + empty root
// directory, byte-compatible with real Psion Flash SSD dumps. Attach
// with the "flash" pack type and EPOC16 mounts it as a read/write
// drive; the rest of the image stays erased (0xFF) so the device can
// program into it without an erase cycle first.
export function createFlashPack(sizeBytes: number, volumeName = 'FLASH'): Uint8Array {
  if (!FLASH_PACK_SIZES.some(s => s.bytes === sizeBytes)) {
    throw new Error(`Unsupported pack size ${sizeBytes}`);
  }
  const buf = new Uint8Array(sizeBytes).fill(0xFF);

  buf[0] = 0xA5; buf[1] = 0xF1;
  // Unique id: factory style — first two bytes stay erased, last two
  // programmed (any value; EPOC16 uses it for media-change detection).
  buf[4] = Math.floor(Math.random() * 256);
  buf[5] = Math.floor(Math.random() * 256);
  buf[6] = 0x01; buf[7] = 0x00; buf[8] = 0x01; buf[9] = 0x00;
  buf[0x0A] = 0x00;                                   // FEFS24
  writeU24(buf, ROOT_PTR_OFF, ROOT_DIR_OFF);
  writeStr(buf, VOL_NAME_OFF, normaliseName(volumeName).stem, 8);
  writeStr(buf, VOL_NAME_OFF + 8, '', 3);
  // 0x19 flash count: left erased (0xFFFFFFFF) = ROM image.
  writeStr(buf, 0x1D, ID_STRING, ID_STRING.length);

  // Empty root directory entry.
  const { time, date } = psiDateTime();
  const o = ROOT_DIR_OFF;
  writeU24(buf, o, NULL_PTR);                         // no siblings
  writeStr(buf, o + 3, 'ROOT', 8);
  writeStr(buf, o + 11, '', 3);
  buf[o + 14] = 0xFF & ~FLAG_IS_FILE;                 // 0xFB: valid dir, no children yet
  writeU24(buf, o + 15, NULL_PTR);                    // first child
  writeU24(buf, o + 18, NULL_PTR);                    // alt record
  buf[o + 21] = PROP_DIRECTORY;
  writeU16(buf, o + 22, time);
  writeU16(buf, o + 24, date);
  return buf;
}

// ── Internal walker ───────────────────────────────────────────────────

interface WalkedEntry {
  off: number;           // entry offset in the image
  name: string;          // "STEM.EXT"
  isFile: boolean;
  valid: boolean;
  last: boolean;
  nextPtr: number;
  childPtr: number;      // first child (dir) / first continuation (file)
  size: number;          // total bytes across the data-record chain
  // highest byte used by this entry + its records/data (for allocation)
  extent: number;
}

function entryAt(img: Uint8Array, off: number): WalkedEntry {
  const flags  = img[off + 14];
  const isFile = (flags & FLAG_IS_FILE) !== 0;
  const stem   = readStr(img, off + 3, 8);
  const ext    = readStr(img, off + 11, 3);
  const e: WalkedEntry = {
    off,
    name: ext ? `${stem}.${ext}` : stem,
    isFile,
    valid: (flags & FLAG_VALID) !== 0,
    last:  (flags & FLAG_LAST_SIBLING) !== 0,
    nextPtr:  readU24(img, off),
    childPtr: (flags & FLAG_NO_CHILD) ? NULL_PTR : readU24(img, off + 15),
    size: 0,
    extent: off + (isFile ? FILE_ENTRY_SIZE : DIR_ENTRY_SIZE),
  };
  if (isFile) {
    const chain = dataChain(img, off);
    for (const c of chain.chunks) e.size += c.len;
    e.extent = Math.max(e.extent, chain.extent);
  }
  return e;
}

// Walks a file's data-record chain: the first record lives in the file
// entry itself, continuations are 17-byte FILE records. Returns the
// data runs in order plus the highest byte the chain occupies (records
// and data alike), which is what the allocator needs.
//
// A pack the *device* wrote chains records the same way but lays them
// out however its own allocator saw fit, so nothing here may assume the
// contiguous layout addFileToPack produces.
function dataChain(img: Uint8Array, off: number): {
  chunks: { ptr: number; len: number }[];
  extent: number;
} {
  const chunks: { ptr: number; len: number }[] = [];
  let extent = off + FILE_ENTRY_SIZE;
  let dataPtr = readU24(img, off + 26);
  let dataLen = readU16(img, off + 29);
  let contPtr = (img[off + 14] & FLAG_NO_CHILD) ? NULL_PTR : readU24(img, off + 15);
  for (let safety = 0; safety < 100000; safety++) {
    if (dataPtr !== NULL_PTR && dataLen !== 0xFFFF) {
      chunks.push({ ptr: dataPtr, len: dataLen });
      extent = Math.max(extent, dataPtr + dataLen);
    }
    if (contPtr === NULL_PTR || contPtr === 0) break;
    const r = contPtr;
    if (r + FILE_RECORD_SIZE > img.length) break;
    extent = Math.max(extent, r + FILE_RECORD_SIZE);
    dataPtr = readU24(img, r + 7);
    dataLen = readU16(img, r + 10);
    contPtr = (img[r] & FLAG_NO_CHILD) ? NULL_PTR : readU24(img, r + 1);
  }
  return { chunks, extent };
}

// Walks a sibling chain starting at `off`. Returns entries in order.
function walkChain(img: Uint8Array, off: number): WalkedEntry[] {
  const out: WalkedEntry[] = [];
  for (let safety = 0; safety < 10000; safety++) {
    if (off === NULL_PTR || off === 0 || off + DIR_ENTRY_SIZE > img.length) break;
    const e = entryAt(img, off);
    out.push(e);
    if (e.last) break;
    off = e.nextPtr;
  }
  return out;
}

function rootEntry(img: Uint8Array): WalkedEntry | null {
  const rootOff = readU24(img, ROOT_PTR_OFF);
  if (rootOff === NULL_PTR || rootOff + DIR_ENTRY_SIZE > img.length) return null;
  return entryAt(img, rootOff);
}

// Highest used offset across the whole filesystem (append point).
function imageExtent(img: Uint8Array): number {
  const root = rootEntry(img);
  if (!root) return ROOT_DIR_OFF + DIR_ENTRY_SIZE;
  let extent = root.extent;
  const stack: number[] = [root.childPtr];
  for (let safety = 0; safety < 10000 && stack.length; safety++) {
    const ptr = stack.pop()!;
    for (const e of walkChain(img, ptr)) {
      extent = Math.max(extent, e.extent);
      if (!e.isFile && e.childPtr !== NULL_PTR) stack.push(e.childPtr);
    }
  }
  return extent;
}

// ── listFiles ─────────────────────────────────────────────────────────
// Lists all valid files at any depth, shown as "DIR\\SUB\\NAME.EXT"
// (matching how EPOC16 displays pack paths).
export function listFiles(packBytes: Uint8Array): FefsFile[] {
  const out: FefsFile[] = [];
  if (classifyPack(packBytes) !== 'flash') return out;
  const root = rootEntry(packBytes);
  if (!root || root.childPtr === NULL_PTR) return out;
  const visit = (chainOff: number, prefix: string) => {
    for (const e of walkChain(packBytes, chainOff)) {
      if (!e.valid) continue;
      if (e.isFile) {
        out.push({ name: prefix + e.name, size: e.size });
      } else if (e.childPtr !== NULL_PTR) {
        visit(e.childPtr, `${prefix}${e.name}\\`);
      }
    }
  };
  visit(root.childPtr, '');
  return out;
}

// ── addFileToPack ─────────────────────────────────────────────────────
// Appends a file, optionally inside a directory path ("WRD" or nested
// "BLADES\\DATA"), creating each level on demand. If a valid file of
// the same name already exists at that level it is deleted first (FEFS
// logical delete), so re-adding a file replaces it. Returns a new
// image; the input is not modified. Throws on invalid names or a full
// pack.
export function addFileToPack(
  packBytes: Uint8Array,
  name: string,
  fileBytes: Uint8Array,
  dir?: string,
): Uint8Array {
  if (classifyPack(packBytes) !== 'flash') {
    throw new Error('Not a FEFS pack — create a pack before adding files');
  }
  const { stem, ext } = normaliseName(name);
  const dirParts = normaliseDirPath(dir);
  let out: Uint8Array = new Uint8Array(packBytes);

  // Replace-on-duplicate: logically delete any existing live file with
  // this name at the same level.
  const fullName = [...dirParts, `${stem}${ext ? '.' + ext : ''}`].join('\\');
  const existing = listFiles(out).find(f => f.name === fullName);
  if (existing) out = removeFileFromPack(out, existing.name);

  const root = rootEntry(out);
  if (!root) throw new Error('FEFS pack corrupt: no root directory');

  // Resolve the parent directory entry level by level, creating
  // directories as needed.
  let parentOff = root.off;
  for (const comp of dirParts) {
    const parent = entryAt(out, parentOff);
    let dirEntry = parent.childPtr === NULL_PTR
      ? undefined
      : walkChain(out, parent.childPtr).find(e => e.valid && !e.isFile && e.name === comp);
    if (!dirEntry) {
      const dirOff = allocate(out, DIR_ENTRY_SIZE);
      writeDirEntry(out, dirOff, comp);
      chainIntoParent(out, parentOff, dirOff);
      dirEntry = entryAt(out, dirOff);
    }
    parentOff = dirEntry.off;
  }

  // Lay out data records (32K chunks; the length field is 16-bit).
  const chunks: { ptr: number; len: number }[] = [];
  let fileOff: number;
  {
    const nChunks = Math.max(1, Math.ceil(fileBytes.length / DATA_CHUNK));
    const needed = FILE_ENTRY_SIZE + fileBytes.length +
                   (nChunks - 1) * FILE_RECORD_SIZE;
    const base = allocate(out, needed);
    fileOff = base;
    let dataOff = base + FILE_ENTRY_SIZE;
    for (let i = 0; i < nChunks; i++) {
      const len = Math.min(DATA_CHUNK, fileBytes.length - i * DATA_CHUNK);
      out.set(fileBytes.subarray(i * DATA_CHUNK, i * DATA_CHUNK + len), dataOff);
      chunks.push({ ptr: dataOff, len });
      dataOff += len;
      if (i < nChunks - 1) dataOff += FILE_RECORD_SIZE; // room for the next record
    }
  }

  // File entry with the first data record.
  const { time, date } = psiDateTime();
  writeU24(out, fileOff, NULL_PTR);                     // next sibling
  writeStr(out, fileOff + 3, stem, 8);
  writeStr(out, fileOff + 11, ext, 3);
  let flags = 0xFF;                                     // valid file, last sibling
  if (chunks.length === 1) {
    // single data record: no continuation
  } else {
    flags &= ~FLAG_NO_CHILD;
  }
  out[fileOff + 14] = flags;
  // First continuation record sits right after the first data chunk.
  writeU24(out, fileOff + 15, chunks.length > 1 ? chunks[0].ptr + chunks[0].len : NULL_PTR);
  writeU24(out, fileOff + 18, NULL_PTR);                // alt record
  out[fileOff + 21] = 0x00;                             // plain file properties
  writeU16(out, fileOff + 22, time);
  writeU16(out, fileOff + 24, date);
  writeU24(out, fileOff + 26, chunks[0].ptr);
  writeU16(out, fileOff + 29, chunks[0].len);

  // Continuation records for chunks 1..n.
  for (let i = 1; i < chunks.length; i++) {
    const rec = chunks[i - 1].ptr + chunks[i - 1].len;
    let rFlags = 0xFF;
    if (i < chunks.length - 1) rFlags &= ~FLAG_NO_CHILD;
    out[rec] = rFlags;
    writeU24(out, rec + 1, i < chunks.length - 1 ? chunks[i].ptr + chunks[i].len : NULL_PTR);
    writeU24(out, rec + 4, NULL_PTR);
    writeU24(out, rec + 7, chunks[i].ptr);
    writeU16(out, rec + 10, chunks[i].len);
    out[rec + 12] = 0x00;
    writeU16(out, rec + 13, time);
    writeU16(out, rec + 15, date);
  }

  chainIntoParent(out, parentOff, fileOff);
  return out;
}

// Writes an empty directory entry at `off`.
function writeDirEntry(img: Uint8Array, off: number, name: string) {
  const { time, date } = psiDateTime();
  writeU24(img, off, NULL_PTR);
  writeStr(img, off + 3, name, 8);
  writeStr(img, off + 11, '', 3);
  img[off + 14] = 0xFF & ~FLAG_IS_FILE;                 // valid dir, no children
  writeU24(img, off + 15, NULL_PTR);
  writeU24(img, off + 18, NULL_PTR);
  img[off + 21] = PROP_DIRECTORY;
  writeU16(img, off + 22, time);
  writeU16(img, off + 24, date);
}

// Links a freshly written entry into a parent directory, using only
// 1→0 bit programming (authentic Flash semantics): either the parent
// gains its first child (clear FLAG_NO_CHILD + program the pointer),
// or the current last sibling chains forward (clear FLAG_LAST_SIBLING
// + program its next-pointer).
function chainIntoParent(img: Uint8Array, parentOff: number, newOff: number) {
  const parent = entryAt(img, parentOff);
  if (parent.childPtr === NULL_PTR) {
    img[parentOff + 14] &= ~FLAG_NO_CHILD;
    writeU24(img, parentOff + 15, newOff);
    return;
  }
  const siblings = walkChain(img, parent.childPtr);
  const tail = siblings[siblings.length - 1];
  img[tail.off + 14] &= ~FLAG_LAST_SIBLING;
  writeU24(img, tail.off, newOff);
}

// Finds `size` bytes of erased space after everything currently used.
function allocate(img: Uint8Array, size: number): number {
  const off = imageExtent(img);
  if (off + size > img.length) {
    throw new Error(`FEFS pack full: need ${size} bytes, ${img.length - off} free`);
  }
  return off;
}

// ── removeFileFromPack ────────────────────────────────────────────────
// FEFS logical delete: clears the entry's valid bit (a 1→0 program on
// real Flash). Space is only reclaimed by reformatting, exactly like
// the real filesystem. Accepts "NAME.EXT" or "DIR\\NAME.EXT".
export function removeFileFromPack(packBytes: Uint8Array, name: string): Uint8Array {
  if (classifyPack(packBytes) !== 'flash') {
    throw new Error('Not a FEFS pack');
  }
  const out = new Uint8Array(packBytes);
  const target = findFile(out, name);
  if (!target) throw new Error(`FEFS file "${name}" not found`);
  out[target.off + 14] &= ~FLAG_VALID;
  return out;
}

// ── readFileFromPack ──────────────────────────────────────────────────
// Pulls one file's bytes back out of a pack, by the same path listFiles
// reports ("HELLO.TXT", "WRD\\REPORT.WRD"). This is how a document the
// Psion itself saved onto an SSD gets off the device: the pack image is
// read back out of the emulator and the file extracted here.
export function readFileFromPack(packBytes: Uint8Array, name: string): Uint8Array {
  if (classifyPack(packBytes) !== 'flash') {
    throw new Error('Not a FEFS pack');
  }
  const target = findFile(packBytes, name);
  if (!target) throw new Error(`FEFS file "${name}" not found`);

  // A file can't hold more than the pack it lives on: cap the allocation
  // so a corrupt length field can't ask for gigabytes.
  const out = new Uint8Array(Math.min(target.size, packBytes.length));
  let written = 0;
  for (const c of dataChain(packBytes, target.off).chunks) {
    // Clamp against a truncated or corrupt image rather than throwing:
    // salvaging what is readable beats refusing the download.
    const end = Math.min(c.ptr + c.len, packBytes.length);
    if (end <= c.ptr) continue;
    const take = Math.min(end - c.ptr, out.length - written);
    if (take <= 0) break;
    out.set(packBytes.subarray(c.ptr, c.ptr + take), written);
    written += take;
  }
  return written === out.length ? out : out.subarray(0, written);
}

// Resolves "NAME.EXT" or "DIR\\SUB\\NAME.EXT" to its live file entry,
// or null if no valid file sits at that path.
function findFile(img: Uint8Array, name: string): WalkedEntry | null {
  const root = rootEntry(img);
  if (!root || root.childPtr === NULL_PTR) return null;
  const parts = name.toUpperCase().split(/[\\/]/).filter(Boolean);
  if (parts.length === 0) return null;
  let chain = walkChain(img, root.childPtr);
  for (const comp of parts.slice(0, -1)) {
    const d = chain.find(e => e.valid && !e.isFile && e.name === comp);
    if (!d || d.childPtr === NULL_PTR) return null;
    chain = walkChain(img, d.childPtr);
  }
  return chain.find(e => e.valid && e.isFile && e.name === parts[parts.length - 1]) ?? null;
}

// ── classifyPack ──────────────────────────────────────────────────────
// FEFS magic check. Note this only inspects the *contents* — the pack
// TYPE (RAM / Flash / write-protected) presented to the Psion is a
// hardware strap chosen at attach time, independent of contents.
/**
 * Free bytes left in the pack's append arena.
 *
 * FEFS is append-only: every write lands past the current extent, and a
 * delete only clears a valid bit. So this shrinks monotonically as the pack is
 * used, whatever is deleted — which is why compactPack exists.
 */
export function packFreeBytes(packBytes: Uint8Array): number {
  if (classifyPack(packBytes) !== 'flash') return 0;
  return Math.max(0, packBytes.length - imageExtent(packBytes));
}

export function readVolumeFreeInfo(
  packBytes: Uint8Array,
): { free: number; used: number; total: number } {
  const total = packBytes.length;
  const free = packFreeBytes(packBytes);
  return { free, used: total - free, total };
}

/**
 * Rebuild the pack containing only its live files, reclaiming the space that
 * logically-deleted ones still occupy.
 *
 * This is not an optimisation — it is what keeps a shared folder usable. FEFS
 * models real Flash, where a delete can only clear bits and space comes back
 * solely by erasing the part: removeFileFromPack therefore grows the pack's
 * extent every time, and a host folder that changes a couple of times a day
 * would exhaust a 2 MB pack within weeks of projections. Compacting is the
 * software equivalent of reformatting the pack, which is exactly how a real
 * one is reclaimed.
 *
 * The volume name is preserved. Directory structure is rebuilt implicitly:
 * every live file is re-added at its full path, and addFileToPack creates the
 * directories it needs.
 */
export function compactPack(packBytes: Uint8Array): Uint8Array {
  if (classifyPack(packBytes) !== 'flash') {
    throw new Error('Not a FEFS pack — nothing to compact');
  }
  const live = listFiles(packBytes);
  const contents = live.map((f) => ({
    name: f.name,
    bytes: readFileFromPack(packBytes, f.name),
  }));

  let out = createFlashPack(packBytes.length, readVolumeName(packBytes) || 'FLASH');
  for (const { name, bytes } of contents) {
    // listFiles reports full paths ("WRD\REPORT.WRD"); split the directory
    // back out so it is recreated rather than becoming part of the filename.
    const cut = name.lastIndexOf('\\');
    const dir = cut >= 0 ? name.slice(0, cut) : undefined;
    const base = cut >= 0 ? name.slice(cut + 1) : name;
    out = addFileToPack(out, base, bytes, dir);
  }
  return out;
}

export function classifyPack(packBytes: Uint8Array): PackKind {
  if (packBytes.length >= 2 && packBytes[0] === 0xA5 && packBytes[1] === 0xF1) {
    return 'flash';
  }
  return 'ram';
}

export function readVolumeName(packBytes: Uint8Array): string {
  if (packBytes.length < VOL_NAME_OFF + 8) return '';
  if (classifyPack(packBytes) !== 'flash') return '';
  return readStr(packBytes, VOL_NAME_OFF, 8);
}
