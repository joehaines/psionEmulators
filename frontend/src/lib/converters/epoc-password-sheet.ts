// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// EPOC32 password-protected Sheet workbooks (.spr) — read the cells back
// out without the password.
//
// Worked out the same way as the Word side (epoc-password.ts): a stock
// C:\Documents\Sheet on an emulated 5mx, cells typed in, "Password..."
// set, the file pulled back over the Remote Link, and compared with the
// same workbook saved without one — twice, once with a single text cell
// and once with ten cells of text and numbers.
//
// ── How Sheet differs from Word ─────────────────────────────────────
//
// Same cipher — cipher[i] = (plain[i] + key[i mod 32]) mod 256, the same
// 41-byte 0x100000CD password section spliced in at 0x14, the same 0x30
// filler padding each enciphered run out to a multiple of 32 bytes.  Two
// things make it a different job:
//
//   * Word protects one stream (its text).  Sheet protects *every* stream
//     holding user data — the workbook, worksheet list, worksheets, cell
//     lists, line sections, graph list — so a one-cell workbook grows
//     from 429 to 776 bytes, nearly all of it filler.  Only the status
//     section, the page layout, the application-ID section and the
//     section table stay in clear text.
//
//   * Each run has its own keystream phase.  Within a run the phase
//     tracks the file offset, but it steps between runs by an amount
//     that isn't the gap between them, so there is no file-wide rule to
//     apply.  Rather than guess at one, this module *measures* each
//     run's phase: 32 possibilities, and Sheet's structures are specific
//     enough (marker bytes, offsets that have to land inside the file,
//     a filler tail that has to be filler) that only the right one fits.
//
// ── Why no language model is needed ─────────────────────────────────
//
// Sheet hands over enough known plaintext to solve the key outright.  A
// workbook with no graphs has a Graph List Section of exactly three
// bytes (02 00 00) padded to 32, so 3 known bytes plus 29 filler bytes
// give all 32 key bytes directly — and any run whose filler is a whole
// period long gives them on its own.  The key that comes out is correct
// only up to rotation (we don't know the anchor's own phase), which
// costs nothing because every run's phase is searched anyway.
//
// The recovery is therefore exact or it fails: if the chain below doesn't
// validate, the caller is told to hand back the original file rather than
// a plausible-looking guess.

import { UID3_SHEET, readEpocHeader, isValidEpocHeader, readUint32LE } from './epoc-uids.ts';
import { readSectionTable, findSection, readCardinal, type SectionEntry } from './epoc-store.ts';
import { isEpocPasswordProtected } from './epoc-password.ts';

// Section UIDs (psiconv data.h; the same ones sheet.ts walks).
const UID_WORKBOOK_SECTION   = 0x1000011D;
const UID_GRAPH_LIST_SECTION = 0x10000121;
const UID_APPID_SECTION      = 0x10000089;

const KEY_LEN  = 32;
const PAD_BYTE = 0x30;

// A graph-less Graph List Section, which is what a workbook nobody has
// drawn a graph in carries.
const EMPTY_GRAPH_LIST = [0x02, 0x00, 0x00];

export function isProtectedEpocSheet(bytes: Uint8Array): boolean {
  const header = readEpocHeader(bytes);
  if (!header || !isValidEpocHeader(header) || header.uid3 !== UID3_SHEET) return false;
  return isEpocPasswordProtected(bytes);
}

export interface SheetRecovery {
  // The file with every enciphered run decrypted in place — offsets are
  // untouched, so it goes straight into convertSheetToCsv.
  file: Uint8Array;
  key: Uint8Array;                       // the 32-byte keystream (up to rotation)
  streams: { start: number; phase: number }[];
}

// Recover a password-protected EPOC Sheet workbook.  Returns null when
// the file isn't one, or when the structure doesn't validate — there is
// no partial answer here, so a caller that gets null should hand back
// the original bytes.
export function recoverEpocSheet(bytes: Uint8Array): SheetRecovery | null {
  if (!isProtectedEpocSheet(bytes)) return null;
  const table = readSectionTable(bytes);
  if (!table) return null;

  const workbook  = findSection(table, UID_WORKBOOK_SECTION);
  const graphList = findSection(table, UID_GRAPH_LIST_SECTION);
  if (!workbook || !graphList) return null;
  if ((workbook.end - workbook.start) % KEY_LEN !== 0) return null;   // not enciphered

  // Try each way of reading known plaintext out of the graph list; the
  // workbook is what says whether the key that came out is right.
  for (const key of anchorKeys(bytes, graphList)) {
    const recovered = walk(bytes, table, key, workbook, graphList);
    if (recovered) return recovered;
  }
  return null;
}

// Candidate keys from the Graph List Section's filler.  Two readings, in
// order of how much they assume: a whole period of trailing filler (no
// assumption about the section's contents at all), then the three-byte
// graph-less section.
function anchorKeys(bytes: Uint8Array, graphList: SectionEntry): Uint8Array[] {
  const out: Uint8Array[] = [];
  const len = graphList.end - graphList.start;
  if (len % KEY_LEN !== 0 || len < KEY_LEN) return out;

  if (len >= 2 * KEY_LEN) {
    // The last 32 bytes are filler whenever the section's data stops at
    // least a period short of its end, and a period of filler is the
    // whole key.
    const key = new Uint8Array(KEY_LEN);
    const from = graphList.end - KEY_LEN;
    for (let i = 0; i < KEY_LEN; i++) {
      key[(from - graphList.start + i) % KEY_LEN] = (bytes[from + i] - PAD_BYTE) & 0xff;
    }
    out.push(key);
  }
  const key = new Uint8Array(KEY_LEN);
  for (let i = 0; i < KEY_LEN; i++) {
    const plain = i < EMPTY_GRAPH_LIST.length ? EMPTY_GRAPH_LIST[i] : PAD_BYTE;
    key[i] = (bytes[graphList.start + i] - plain) & 0xff;
  }
  out.push(key);
  return out;
}

// =============================================================================
// Walking the workbook with one candidate key
// =============================================================================

function walk(
  bytes: Uint8Array, table: SectionEntry[], key: Uint8Array,
  workbook: SectionEntry, graphList: SectionEntry,
): SheetRecovery | null {
  const inFile = (offset: number) => offset >= 0x14 && offset < bytes.length;
  // start -> phase; a stream we find but can't place keeps phase null and
  // is left enciphered (nothing downstream reads it).
  const streams = new Map<number, number | null>();

  // ----- Workbook Section: 02/04, four or five offsets, filler tail -----
  const wbLen = workbook.end - workbook.start;
  const wb = placeStream(bytes, key, workbook.start, wbLen, body => {
    const offsets = workbookOffsets(body, wbLen);
    if (!offsets) return false;
    if (!offsets.every(inFile)) return false;
    const dataLen = 1 + 4 * offsets.length;
    for (let i = dataLen; i < wbLen; i++) if (body[i] !== PAD_BYTE) return false;
    return true;
  });
  if (!wb) return null;
  streams.set(workbook.start, wb.phase);
  streams.set(graphList.start, 0);              // the anchor, by construction

  const offsets = workbookOffsets(wb.body, wbLen)!;
  // Offsets are: info section, formula list, worksheet list, variable
  // list, and (with flag 0x04) the name section.  Only the worksheet
  // list leads to cells, but the others are stream starts too, and
  // knowing them keeps the decryption from running one run's phase on
  // into the next.
  for (const offset of offsets) if (inFile(offset)) streams.set(offset, null);
  const worksheetListOffset = offsets[2];
  if (!inFile(worksheetListOffset)) return null;

  // ----- Worksheet List: 0x02, count, then (number, offset) per sheet -----
  const wl = placeStream(bytes, key, worksheetListOffset, 256, body => {
    if (body[0] !== 0x02) return false;
    const sheets = worksheetOffsets(body);
    return sheets !== null && sheets.length > 0 && sheets.every(inFile);
  });
  if (!wl) return null;
  streams.set(worksheetListOffset, wl.phase);

  // ----- Each worksheet -> its Cell List -----
  let placedCellList = false;
  for (const sheetOffset of worksheetOffsets(wl.body)!) {
    const ws = placeStream(bytes, key, sheetOffset, 512, body => {
      const refs = worksheetRefs(body);
      return refs !== null && refs.every(inFile);
    });
    if (!ws) continue;
    streams.set(sheetOffset, ws.phase);
    const refs = worksheetRefs(ws.body)!;
    const [rows, cols, cells, grid] = refs;
    for (const offset of [rows, cols, grid]) {
      if (inFile(offset) && !streams.has(offset)) streams.set(offset, null);
    }
    // Cell List: 0x02 0x00, a cell count, then the cells themselves.
    const cl = placeStream(bytes, key, cells, 64, body => {
      if (body[0] !== 0x02 || body[1] !== 0x00) return false;
      const count = readCardinal(body, 2);
      return count !== null && count.value >= 1 && count.value <= 4096;
    });
    if (cl) { streams.set(cells, cl.phase); placedCellList = true; }
  }
  if (!placedCellList) return null;

  // ----- Decrypt -----
  // Every byte takes the phase of the nearest run start at or before it,
  // and the enciphered zone ends where the clear application-ID section
  // (or the section table) begins.
  const appId = findSection(table, UID_APPID_SECTION);
  const zoneEnd = appId ? appId.start : table[table.length - 1].end;
  const starts = [...streams.keys()].filter(s => s >= 0x14 && s < zoneEnd).sort((a, b) => a - b);
  const file = new Uint8Array(bytes);
  for (let i = 0; i < starts.length; i++) {
    const phase = streams.get(starts[i]);
    if (phase === null || phase === undefined) continue;
    const end = i + 1 < starts.length ? starts[i + 1] : zoneEnd;
    for (let off = starts[i]; off < end && off < file.length; off++) {
      file[off] = (bytes[off] - key[(off - starts[i] + phase) % KEY_LEN]) & 0xff;
    }
  }
  return {
    file, key,
    streams: starts
      .filter(s => streams.get(s) !== null && streams.get(s) !== undefined)
      .map(s => ({ start: s, phase: streams.get(s)! })),
  };
}

// Find the phase that makes the run at `start` decrypt to something its
// own format accepts.  There are only 32, and Sheet's structures are
// specific enough that at most one fits.
function placeStream(
  bytes: Uint8Array, key: Uint8Array, start: number, want: number,
  accepts: (body: Uint8Array) => boolean,
): { phase: number; body: Uint8Array } | null {
  if (start < 0x14 || start >= bytes.length) return null;
  const len = Math.min(want, bytes.length - start);
  for (let phase = 0; phase < KEY_LEN; phase++) {
    const body = new Uint8Array(len);
    for (let i = 0; i < len; i++) body[i] = (bytes[start + i] - key[(i + phase) % KEY_LEN]) & 0xff;
    let ok = false;
    try { ok = accepts(body); } catch { ok = false; }
    if (ok) return { phase, body };
  }
  return null;
}

// =============================================================================
// The pieces of Sheet structure this needs (spec: psiconv Sheet_*.psi,
// and the same reading sheet.ts uses to pull cells out)
// =============================================================================

// Workbook Section: a flag byte (0x02, or 0x04 when a name section
// follows), then that many section offsets.
function workbookOffsets(body: Uint8Array, len: number): number[] | null {
  const flag = body[0];
  if (flag !== 0x02 && flag !== 0x04) return null;
  const count = flag === 0x04 ? 5 : 4;
  if (1 + 4 * count > len) return null;
  const out: number[] = [];
  for (let i = 0; i < count; i++) out.push(readUint32LE(body, 1 + i * 4));
  return out;
}

// Worksheet List: 0x02, a worksheet count, then a number + offset each.
function worksheetOffsets(body: Uint8Array): number[] | null {
  const count = readCardinal(body, 1);
  if (!count || count.value < 1 || count.value > 64) return null;
  let pos = count.next;
  const out: number[] = [];
  for (let i = 0; i < count.value; i++) {
    pos += 1;                                   // worksheet number
    if (pos + 4 > body.length) return null;
    out.push(readUint32LE(body, pos));
    pos += 4;
  }
  return out;
}

// Worksheet: 0x04, a show-zeros flag, the default cell layout, then the
// row-defaults, column-defaults, cell-list and grid offsets.
function worksheetRefs(body: Uint8Array): number[] | null {
  if (body[0] !== 0x04) return null;
  let pos = 2;
  if (body[pos] !== 0x02) return null;          // cell layout marker
  pos += 1;
  const flags = body[pos];
  pos += 1;
  if ((flags & ~0x07) !== 0) return null;
  for (const bit of [0x01, 0x02]) {             // paragraph, character layout
    if (flags & bit) {
      if (pos + 4 > body.length) return null;
      const listLen = readUint32LE(body, pos);
      if (listLen > 512) return null;
      pos += 4 + listLen;
    }
  }
  if (flags & 0x04) pos += 3;                   // number format
  if (pos + 16 > body.length) return null;
  return [0, 1, 2, 3].map(i => readUint32LE(body, pos + i * 4));
}
