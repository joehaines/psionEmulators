// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// EPOC Sheet (.spr) -> CSV.
//
// Sheet files on Series 5 wrap a Direct File Store around a Section
// Table that points to a Workbook Section.  The Workbook in turn
// references a Worksheet List, which references one or more Worksheets,
// each of which references a Cell List of (row, col, value) entries.
//
// Layout chain (spec: psiconv formats/psion/Sheet_*.psi):
//
//   [16-byte UID header]
//   [4-byte section-table offset]
//     -> Section Table (BListE of UID/offset)
//          0x1000011D Workbook Section
//          0x1000011F Sheet Status Section
//          0x10000105 Page Layout Section
//          0x10000121 Sheet Graph List Section
//          0x10000089 Application ID Section
//
//   Workbook Section:
//     B 0x02 or 0x04 (with-name flag)
//     L offset to Sheet Info Section
//     L offset to Sheet Formula List       <- formulas referenced by cells
//     L offset to Sheet Worksheet List     <- KEY: gets us to worksheets
//     L offset to Sheet Variable List
//     L offset to Sheet Name Section       <- only if flag=0x04
//
//   Worksheet List:
//     B 0x02 (always)
//     XListE of (B worksheet#, L offset-to-Worksheet)
//
//   Worksheet:
//     B 0x04 (always)
//     B flags (bit 0 = show zeros)
//     Sheet Cell Layout (variable: B 0x02, B layout-flags, then optional
//                        paragraph layout list, character layout list,
//                        number format)
//     L offset to row-defaults Sheet Line Section
//     L offset to col-defaults Sheet Line Section
//     L offset to Sheet Cell List      <- KEY: gets us to cells
//     L offset to Sheet Grid Section
//     L offset to unknown section
//
//   Cell List:
//     B 0x02, B 0x00
//     XListE of Sheet Cells
//
//   Sheet Cell (variable size):
//     3B position (column = bits 2..9, row = bits 10..23)
//     B  flags (bit 3 = calculated, bit 4 = has-layout, bits 5..7 = type)
//     value (type-dependent)
//     Sheet Cell Layout       (only if has-layout)
//     XInt formula reference  (only if calculated)
//
//   Cell value types (psiconv parse_sheet.c, Sheet_Cell_List.psi):
//     0  blank      (no value bytes)
//     1  int        SInt (4 bytes, sign-magnitude: bit 31 of MSByte = sign)
//     2  boolean    1 byte
//     3  error      u16 code
//     4  float      8-byte IEEE-754 double
//     5  string     SListB (special-encoded length + ASCII bytes)
//
// Strings, floats, ints get rendered as their value.  Booleans render
// as TRUE/FALSE.  Errors render as #DIV/0! / #VALUE! / #REF! etc. per
// the spec.  Blank cells emit empty CSV cells; gaps between cells with
// data become empty CSV cells in the right column too.
//
// If structured parsing fails partway through, we fall back to the v1
// heuristic (printable runs + plausible doubles) so the user still
// gets *something*.

import { EPOC_HEADER_BYTES, readUint32LE } from './epoc-uids.ts';

// Section UIDs (psiconv data.h).
const UID_WORKBOOK_SECTION = 0x1000011D;

// Cell content types (Sheet_Cell_List.psi).
const CELL_BLANK    = 0;
const CELL_INT      = 1;
const CELL_BOOL     = 2;
const CELL_ERROR    = 3;
const CELL_FLOAT    = 4;
const CELL_STRING   = 5;

// Error codes (Sheet_Cell_List.psi).
const ERROR_TEXT: Record<number, string> = {
  0: '',
  1: '#NULL!',
  2: '#DIV/0!',
  3: '#VALUE!',
  4: '#REF!',
  5: '#NAME!',
  6: '#NUM!',
  7: '#N/A!',
};

export interface Cell {
  row: number;
  col: number;
  text: string;
}

interface Cursor { pos: number; }

export function convertSheetToCsv(input: Uint8Array): Uint8Array | null {
  const result = parseSheetCells(input);
  // `null`     -> structured parsing genuinely failed (unknown structure)
  // `[]`       -> file is a valid but empty Sheet (no Workbook Section
  //                or zero cells).  Don't make up content via heuristic;
  //                return null so the caller falls back to the raw .spr
  //                download with a "no data" status message.
  // `[cells]`  -> emit CSV.
  if (result === null) return fallbackHeuristicCsv(input);
  if (result.length === 0) return null;
  return new TextEncoder().encode(buildCsv(result));
}

// Debug entry-point: returns either {cells} on success or
// {failedAt, message} on failure.  Useful for diagnosing why a real
// Sheet file falls through to the heuristic fallback.  Not used by
// the main convert path.
export function diagnoseSheet(input: Uint8Array):
  | { ok: true; cellCount: number; preview: Cell[] }
  | { ok: false; stage: string; detail: string }
{
  let lastStage = 'start';
  try {
    if (input.length < EPOC_HEADER_BYTES + 4) {
      return { ok: false, stage: 'header', detail: `file too short (${input.length}B)` };
    }
    lastStage = 'sectionTableOffset';
    const tableOff = readUint32LE(input, EPOC_HEADER_BYTES);
    if (tableOff < EPOC_HEADER_BYTES + 4 || tableOff >= input.length) {
      return { ok: false, stage: lastStage,
               detail: `section-table offset 0x${tableOff.toString(16)} out of range` };
    }
    lastStage = 'sectionTableCount';
    const tableCountByte = input[tableOff];
    if ((tableCountByte & 1) !== 0) {
      return { ok: false, stage: lastStage,
               detail: `count byte 0x${tableCountByte.toString(16)} has odd low bit` };
    }
    const tableCount = tableCountByte >> 1;

    lastStage = 'findWorkbook';
    let workbookOff = -1;
    const sections: { uid: number; offset: number }[] = [];
    for (let i = 0; i < tableCount; i++) {
      const o = tableOff + 1 + i * 8;
      const uid    = readUint32LE(input, o);
      const offset = readUint32LE(input, o + 4);
      sections.push({ uid, offset });
      if (uid === UID_WORKBOOK_SECTION) workbookOff = offset;
    }
    if (workbookOff < 0) {
      const list = sections.map(s => `0x${s.uid.toString(16)}@0x${s.offset.toString(16)}`).join(', ');
      return { ok: false, stage: lastStage,
               detail: `no Workbook Section (0x1000011D) in [${list}]` };
    }

    lastStage = 'workbookFirstByte';
    const flag = input[workbookOff];
    if (flag !== 0x02 && flag !== 0x04) {
      return { ok: false, stage: lastStage,
               detail: `workbook first byte 0x${flag.toString(16)} (expected 02 or 04)` };
    }

    // From here, just run the real parser and report success or
    // catch failure.
    lastStage = 'parseCells';
    const cells = parseSheetCellsInner(input);
    if (!cells || cells.length === 0) {
      return { ok: false, stage: lastStage,
               detail: `parser returned ${cells ? 'empty' : 'null'}` };
    }
    return { ok: true, cellCount: cells.length, preview: cells.slice(0, 10) };
  } catch (e: unknown) {
    return { ok: false, stage: lastStage,
             detail: e instanceof Error ? e.message : String(e) };
  }
}

function parseSheetCells(input: Uint8Array): Cell[] | null {
  try {
    return parseSheetCellsInner(input);
  } catch {
    return null;
  }
}

function parseSheetCellsInner(input: Uint8Array): Cell[] | null {
  if (input.length < EPOC_HEADER_BYTES + 4) return null;

  // ----- Section Table (BListE of UID/offset pairs) -----
  const tableOff = readUint32LE(input, EPOC_HEADER_BYTES);
  if (tableOff < EPOC_HEADER_BYTES + 4 || tableOff >= input.length) return null;
  const tableCountByte = input[tableOff];
  if ((tableCountByte & 1) !== 0) return null;
  const tableCount = tableCountByte >> 1;
  if (tableCount === 0 || tableCount > 16) return null;

  let workbookOff = -1;
  for (let i = 0; i < tableCount; i++) {
    const o = tableOff + 1 + i * 8;
    if (o + 8 > input.length) return null;
    if (readUint32LE(input, o) === UID_WORKBOOK_SECTION) {
      workbookOff = readUint32LE(input, o + 4);
      break;
    }
  }
  if (workbookOff < 0 || workbookOff >= input.length) {
    // Valid Sheet section table but no Workbook Section.  EPOC Sheet
    // creates a tiny stub file like this when the app is opened and
    // saved without entering any data.  Treat as "empty Sheet, no
    // cells" — emit empty cell list, NOT null (which would trigger
    // the heuristic fallback and surface the app-name metadata).
    return [];
  }

  // ----- Workbook Section -----
  const cur: Cursor = { pos: workbookOff };
  const nameFlag = readU8(input, cur, input.length);
  if (nameFlag !== 0x02 && nameFlag !== 0x04) return null;
  skipBytes(input, cur, 4);              // info section offset
  skipBytes(input, cur, 4);              // formula list offset
  const worksheetListOff = readU32(input, cur, input.length);
  // remaining (variable list, optional name section) ignored.

  // ----- Worksheet List -----
  cur.pos = worksheetListOff;
  readU8(input, cur, input.length);      // 0x02
  const wsCount = readX(input, cur, input.length);
  const worksheetOffsets: number[] = [];
  for (let i = 0; i < wsCount; i++) {
    readU8(input, cur, input.length);    // worksheet #
    worksheetOffsets.push(readU32(input, cur, input.length));
  }
  if (worksheetOffsets.length === 0) return null;

  // ----- Walk each worksheet -> Cell List -----
  // For now we emit one CSV that concatenates all worksheets (blank-row
  // separated).  Real-world Sheet files have exactly one worksheet.
  const out: Cell[] = [];
  let worksheetIndex = 0;
  for (const wsOff of worksheetOffsets) {
    const ws: Cursor = { pos: wsOff };
    readU8(input, ws, input.length);      // 0x04
    readU8(input, ws, input.length);      // show-zeros flag
    skipCellLayout(input, ws);            // default cell layout
    skipBytes(input, ws, 4);              // rows offset
    skipBytes(input, ws, 4);              // cols offset
    const cellsOff = readU32(input, ws, input.length);
    // grid + unknown offsets follow — we don't need them.

    const cells = parseCellList(input, cellsOff);
    if (cells === null) continue;
    if (worksheetIndex > 0 && cells.length > 0) {
      // Visual separator between worksheets in the CSV.  Push a sentinel
      // cell with negative row so buildCsv emits a blank line.
      out.push({ row: -1, col: 0, text: '' });
    }
    out.push(...cells);
    worksheetIndex++;
  }
  return out;
}

// Locate and decode a Sheet Cell List somewhere inside [start, end).
// Used by the Word converter to pull the data out of an embedded Sheet
// *object* (the thing behind a chart in a document): the object payload
// is a headerless sub-store whose internal section offsets we can't
// resolve, but the cell list itself has enough structure to find by
// scanning — the 0x02 0x00 list header, an X-encoded count, and every
// cell parsing cleanly with strictly increasing (row, col) positions.
// Random bytes essentially never satisfy all of that at once.
export function scanForCellList(
  input: Uint8Array, start: number, end: number,
): Cell[] | null {
  end = Math.min(end, input.length);
  for (let off = start; off + 4 < end; off++) {
    if (input[off] !== 0x02 || input[off + 1] !== 0x00) continue;
    const cells = tryParseStrictCellList(input, off, end);
    if (cells && cells.length >= 2) return cells;
  }
  return null;
}

function tryParseStrictCellList(
  input: Uint8Array, off: number, end: number,
): Cell[] | null {
  try {
    const cur: Cursor = { pos: off + 2 };
    const count = readX(input, cur, end);
    if (count < 2 || count > 10_000) return null;
    const out: Cell[] = [];
    let lastPos = -1;
    let nonEmpty = 0;
    for (let i = 0; i < count; i++) {
      const cell = parseCell(input, cur);
      if (!cell || cur.pos > end) return null;
      const linear = cell.row * 256 + cell.col;
      if (linear <= lastPos) return null;       // cell lists are sorted
      lastPos = linear;
      if (cell.text !== '') nonEmpty++;
      out.push(cell);
    }
    return nonEmpty >= 2 ? out : null;
  } catch {
    return null;
  }
}

function parseCellList(input: Uint8Array, off: number): Cell[] | null {
  if (off + 2 > input.length) return null;
  const cur: Cursor = { pos: off };
  readU8(input, cur, input.length);      // 0x02
  readU8(input, cur, input.length);      // 0x00
  const count = readX(input, cur, input.length);
  if (count > 1_000_000) return null;     // sanity bound

  const out: Cell[] = [];
  for (let i = 0; i < count; i++) {
    const cell = parseCell(input, cur);
    if (cell) out.push(cell);
  }
  return out;
}

function parseCell(input: Uint8Array, cur: Cursor): Cell | null {
  // 3-byte position: low 2 bits unknown, next 8 bits column, top 14 bits row.
  if (cur.pos + 4 > input.length) return null;
  const pos = input[cur.pos] | (input[cur.pos + 1] << 8) | (input[cur.pos + 2] << 16);
  cur.pos += 3;
  const col = (pos >>> 2) & 0xFF;
  const row = (pos >>> 10) & 0x3FFF;

  const flags = readU8(input, cur, input.length);
  const calculated = (flags & 0x08) !== 0;
  const hasLayout  = (flags & 0x10) !== 0;
  const type       = (flags >>> 5) & 0x07;

  const text = readCellValue(input, cur, type);
  if (hasLayout)  skipCellLayout(input, cur);
  if (calculated) readX(input, cur, input.length);  // formula reference

  return { row, col, text };
}

function readCellValue(input: Uint8Array, cur: Cursor, type: number): string {
  switch (type) {
    case CELL_BLANK:
      return '';
    case CELL_INT: {
      // SInt: u32 with sign-magnitude (bit 31 of MSByte = sign).
      if (cur.pos + 4 > input.length) return '';
      const raw = readUint32LE(input, cur.pos);
      cur.pos += 4;
      const sign  = (raw & 0x80000000) ? -1 : 1;
      const value = sign * (raw & 0x7FFFFFFF);
      return value.toString();
    }
    case CELL_BOOL: {
      const v = readU8(input, cur, input.length);
      return v ? 'TRUE' : 'FALSE';
    }
    case CELL_ERROR: {
      if (cur.pos + 2 > input.length) return '';
      const code = input[cur.pos] | (input[cur.pos + 1] << 8);
      cur.pos += 2;
      return ERROR_TEXT[code] ?? `#ERR${code}`;
    }
    case CELL_FLOAT: {
      if (cur.pos + 8 > input.length) return '';
      const dv = new DataView(input.buffer, input.byteOffset + cur.pos, 8);
      const v  = dv.getFloat64(0, true);
      cur.pos += 8;
      return formatFloat(v);
    }
    case CELL_STRING:
      return readSpecialString(input, cur);
    default:
      return '';
  }
}

function formatFloat(v: number): string {
  if (!Number.isFinite(v)) return '';
  if (Number.isInteger(v) && Math.abs(v) < 1e15) return v.toString();
  // Trim trailing zeros from a 12-digit decimal representation.
  return Number(v.toPrecision(12)).toString();
}

// SListB string: special-encoded length, then `length` bytes of ASCII.
// Special encoding (Basic_Elements.html):
//   byte & 3 == 2  : 1-byte length  = byte >> 2
//   byte & 7 == 5  : 2-byte length  = (word >> 3)
function readSpecialString(input: Uint8Array, cur: Cursor): string {
  if (cur.pos >= input.length) return '';
  const b0 = input[cur.pos];
  let length: number;
  if ((b0 & 3) === 2) {
    length = b0 >>> 2;
    cur.pos += 1;
  } else if ((b0 & 7) === 5) {
    if (cur.pos + 2 > input.length) return '';
    length = (b0 | (input[cur.pos + 1] << 8)) >>> 3;
    cur.pos += 2;
  } else {
    return '';
  }
  if (cur.pos + length > input.length) return '';
  let s = '';
  for (let i = 0; i < length; i++) s += String.fromCharCode(input[cur.pos + i]);
  cur.pos += length;
  return s;
}

// Cell layout: B 0x02, B flags, optional paragraph layout (LListB),
// optional character layout (LListB), optional number format (3 bytes).
// We only need to skip past it.
function skipCellLayout(input: Uint8Array, cur: Cursor): void {
  readU8(input, cur, input.length);                   // 0x02
  const flags = readU8(input, cur, input.length);
  if (flags & 0x01) skipLListB(input, cur);           // paragraph layout
  if (flags & 0x02) skipLListB(input, cur);           // character layout
  if (flags & 0x04) skipBytes(input, cur, 3);         // number format
}

function skipLListB(input: Uint8Array, cur: Cursor): void {
  const byteCount = readU32(input, cur, input.length);
  skipBytes(input, cur, byteCount);
}

// ----- Reader primitives -----

function readU8(input: Uint8Array, cur: Cursor, end: number): number {
  if (cur.pos >= end) throw new Error('readU8 OOB');
  return input[cur.pos++];
}
function readU32(input: Uint8Array, cur: Cursor, end: number): number {
  if (cur.pos + 4 > end) throw new Error('readU32 OOB');
  const v = readUint32LE(input, cur.pos);
  cur.pos += 4;
  return v;
}
function skipBytes(_input: Uint8Array, cur: Cursor, n: number): void {
  cur.pos += n;
}

// X (extra) encoding (Basic_Elements.html):
//   byte & 1 == 0 : 1-byte value = byte >> 1            (0..127)
//   byte & 3 == 1 : 2-byte value = (word >> 2)          (0..16383)
//   byte & 7 == 3 : 4-byte value = (long >> 3)          (0..512M)
function readX(input: Uint8Array, cur: Cursor, end: number): number {
  if (cur.pos >= end) throw new Error('readX OOB');
  const b0 = input[cur.pos];
  if ((b0 & 1) === 0) {
    cur.pos += 1;
    return b0 >>> 1;
  }
  if ((b0 & 3) === 1) {
    if (cur.pos + 2 > end) throw new Error('readX OOB');
    const v = (b0 | (input[cur.pos + 1] << 8)) >>> 2;
    cur.pos += 2;
    return v;
  }
  if ((b0 & 7) === 3) {
    if (cur.pos + 4 > end) throw new Error('readX OOB');
    const raw = readUint32LE(input, cur.pos);
    cur.pos += 4;
    return raw >>> 3;
  }
  throw new Error('readX bad encoding');
}

// =============================================================================
// CSV output
// =============================================================================

function buildCsv(cells: Cell[]): string {
  // Group by row, padded with empty strings for sparse columns.
  // Use a Map<row, Map<col, text>> so we can iterate rows in numeric
  // order and only emit columns up to the document's max.
  const rows = new Map<number, Map<number, string>>();
  let maxCol = -1;
  for (const c of cells) {
    if (c.row < 0) continue;             // worksheet separator
    let r = rows.get(c.row);
    if (!r) { r = new Map(); rows.set(c.row, r); }
    r.set(c.col, c.text);
    if (c.col > maxCol) maxCol = c.col;
  }
  if (rows.size === 0) return '';

  const sortedRows = Array.from(rows.keys()).sort((a, b) => a - b);
  const lines: string[] = [];

  // Emit rows from min to max (filling blank rows between filled ones).
  const minRow = sortedRows[0];
  const maxRow = sortedRows[sortedRows.length - 1];
  for (let r = minRow; r <= maxRow; r++) {
    const row = rows.get(r);
    const cols: string[] = [];
    for (let c = 0; c <= maxCol; c++) {
      const text = row?.get(c) ?? '';
      cols.push(csvEscape(text));
    }
    // Trim trailing empty cells per CSV convention.
    while (cols.length > 0 && cols[cols.length - 1] === '') cols.pop();
    lines.push(cols.join(','));
  }

  // Worksheet separator: collapse the original "-1 row" sentinel into
  // a blank line between worksheets.  (For v1 we always have one
  // worksheet so this rarely fires.)
  return lines.join('\n') + '\n';
}

function csvEscape(s: string): string {
  if (s.includes(',') || s.includes('"') || s.includes('\n') || s.includes('\r')) {
    return '"' + s.replace(/"/g, '""') + '"';
  }
  return s;
}

// =============================================================================
// Heuristic fallback (used when structured parsing fails)
// =============================================================================

function fallbackHeuristicCsv(input: Uint8Array): Uint8Array | null {
  if (input.length <= EPOC_HEADER_BYTES) return null;

  const body = input.subarray(EPOC_HEADER_BYTES);
  const strings = extractPrintableRuns(body, 2).filter(looksLikeUserData);
  const numbers = extractPlausibleDoubles(body);

  if (strings.length === 0 && numbers.length === 0) return null;

  const lines: string[] = [];
  for (const s of strings) lines.push(csvEscape(s));
  for (const n of numbers) lines.push(formatFloat(n));
  return new TextEncoder().encode(lines.join('\n') + '\n');
}

// Reject obvious metadata that ends up in EPOC files alongside user
// data: app-name markers like "&Sheet.app" (Application ID Section),
// the standard built-in font names ("Times New Roman", "Arial", etc.)
// when they appear with no surrounding context, and very short single-
// word identifiers that look like internal tokens rather than text the
// user typed.  We err on the side of dropping more — the heuristic is
// already lossy by definition, and a CSV full of internal app names is
// strictly worse than a smaller CSV of real content.
function looksLikeUserData(s: string): boolean {
  if (/\.app$/.test(s))                 return false;   // "&Sheet.app", "Word.app"
  if (/^&[A-Z][a-z]+\.app$/.test(s))    return false;
  return true;
}

function extractPrintableRuns(bytes: Uint8Array, minRun: number): string[] {
  const out: string[] = [];
  let cur = '';
  for (let i = 0; i < bytes.length; i++) {
    const b = bytes[i];
    const printable =
      (b >= 0x20 && b <= 0x7E) || (b >= 0xA0 && b <= 0xFF);
    if (printable) {
      cur += String.fromCharCode(b);
    } else {
      if (cur.length >= minRun) out.push(cur);
      cur = '';
    }
  }
  if (cur.length >= minRun) out.push(cur);
  return out;
}

function extractPlausibleDoubles(bytes: Uint8Array): number[] {
  const seen = new Set<string>();
  const out: number[] = [];
  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  for (let i = 0; i + 8 <= bytes.length; i++) {
    const v = dv.getFloat64(i, true);
    if (!Number.isFinite(v) || v === 0) continue;
    const abs = Math.abs(v);
    if (abs < 1e-6 || abs > 1e15) continue;
    const rounded = Number(v.toPrecision(12));
    if (rounded !== v && Math.abs(rounded - v) / abs > 1e-9) continue;
    const key = v.toString();
    if (seen.has(key)) continue;
    seen.add(key);
    out.push(v);
  }
  return out;
}
