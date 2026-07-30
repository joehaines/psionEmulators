// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Helpers for navigating the binary body of EPOC document files.
//
// EPOC uses two store flavours:
//
// * **Direct File Store** (UID1 = 0x10000037).  Used by MBM and WVE.
//   Layout: [16-byte UID header][4-byte trailer offset][data...][trailer].
//   The trailer is a short structure (typically a count + per-element u32
//   offsets) pointing into the data section.
//
// * **Permanent File Store** (UID1 = 0x10000039).  Used by Word and Sheet
//   document files.  Vastly more complex — streams with type tags and
//   variable-length encoded lengths.  For v1 we don't fully parse it;
//   instead the per-format converters (word.ts, sheet.ts) walk the body
//   heuristically to pull out readable text.
//
// This module only provides the bits the converters actually need.

import { readUint32LE, EPOC_HEADER_BYTES } from './epoc-uids.ts';

// Reads the "trailer offset" that immediately follows the 16-byte UID
// header in a Direct File Store.  Returns null if the bytes are too
// short or the offset is implausible.
export function readDirectStoreTrailerOffset(bytes: Uint8Array): number | null {
  if (bytes.length < EPOC_HEADER_BYTES + 4) return null;
  const offset = readUint32LE(bytes, EPOC_HEADER_BYTES);
  if (offset < EPOC_HEADER_BYTES + 4 || offset >= bytes.length) return null;
  return offset;
}

// A Direct File Store's trailer is, for every document format we handle,
// a Section Table: a single-byte-cardinal count followed by that many
// (u32 UID, u32 offset) pairs pointing at the file's sections.  Sections
// are not length-tagged — a section runs to whichever section (or the
// table itself) starts next — so we sort by offset and pair each start
// with the following one.
export interface SectionEntry { uid: number; start: number; end: number; }

export function readSectionTable(bytes: Uint8Array): SectionEntry[] | null {
  const tableOffset = readDirectStoreTrailerOffset(bytes);
  if (tableOffset === null) return null;

  const countByte = bytes[tableOffset];
  if ((countByte & 1) !== 0) return null;          // not a single-byte cardinal
  const count = countByte >> 1;
  if (count === 0 || count > 32) return null;
  const entriesStart = tableOffset + 1;
  if (entriesStart + count * 8 > bytes.length) return null;

  const raw: { uid: number; start: number }[] = [];
  for (let i = 0; i < count; i++) {
    const uid   = readUint32LE(bytes, entriesStart + i * 8);
    const start = readUint32LE(bytes, entriesStart + i * 8 + 4);
    if (start < EPOC_HEADER_BYTES + 4 || start >= tableOffset) return null;
    raw.push({ uid, start });
  }
  raw.sort((a, b) => a.start - b.start);

  return raw.map((entry, i) => ({
    uid: entry.uid,
    start: entry.start,
    end: i + 1 < raw.length ? raw[i + 1].start : tableOffset,
  }));
}

export function findSection(table: SectionEntry[], uid: number): SectionEntry | null {
  return table.find(s => s.uid === uid) ?? null;
}

// EPOC stores variable-length integers (used inside Permanent File Store
// records for paragraph lengths, cell positions, etc.) using a simple
// LSB-first scheme:
//   bit 0 of byte 0 == 0: value = (byte0 >> 1)                     (0..127)
//   bit 1..0 of byte 0 == 01: value = ((byte0 | byte1<<8) >> 2)   (0..16k)
//   bit 2..0 of byte 0 == 011: value = ((b0 | b1<<8 | b2<<16 | b3<<24) >> 3)
// We return { value, next } so the caller can chain reads.  Returns null
// on out-of-bounds.
export interface CardinalRead { value: number; next: number; }
export function readCardinal(bytes: Uint8Array, offset: number): CardinalRead | null {
  if (offset >= bytes.length) return null;
  const b0 = bytes[offset];
  if ((b0 & 1) === 0) {
    return { value: b0 >> 1, next: offset + 1 };
  }
  if ((b0 & 2) === 0) {
    if (offset + 1 >= bytes.length) return null;
    return { value: (b0 | (bytes[offset + 1] << 8)) >> 2, next: offset + 2 };
  }
  if ((b0 & 4) === 0) {
    if (offset + 3 >= bytes.length) return null;
    const raw = (
      b0 |
      (bytes[offset + 1] <<  8) |
      (bytes[offset + 2] << 16) |
      (bytes[offset + 3] << 24)
    ) >>> 0;
    return { value: raw >>> 3, next: offset + 4 };
  }
  return null;
}

// Decode an EPOC document body as best we can to extract Latin-1
// printable runs.  Useful as a last-resort "give me the text" extractor
// for Permanent File Store formats whose internal stream layout we
// don't fully parse (Word, Sheet).
//
// Run boundaries: drop any byte that isn't printable Latin-1 (space..0xFF
// minus control chars), but treat tab/CR/LF as run breaks rather than
// dropping the surrounding text.  Adjacent printable bytes form one run;
// runs shorter than `minRunLen` are discarded as noise (UID values,
// stream IDs, struct fields look like 2-3 char "strings" otherwise).
export function extractPrintableRuns(bytes: Uint8Array, minRunLen = 3): string[] {
  const out: string[] = [];
  let current = '';
  for (let i = 0; i < bytes.length; i++) {
    const b = bytes[i];
    const printable =
      (b >= 0x20 && b <= 0x7E) ||           // ASCII printable
      (b >= 0xA0 && b <= 0xFF);             // Latin-1 supplement
    if (printable) {
      current += String.fromCharCode(b);
    } else if (b === 0x09 || b === 0x0A || b === 0x0D) {
      // Whitespace breaks a run but ends it cleanly.
      if (current.length >= minRunLen) out.push(current);
      current = '';
    } else {
      if (current.length >= minRunLen) out.push(current);
      current = '';
    }
  }
  if (current.length >= minRunLen) out.push(current);
  return out;
}
