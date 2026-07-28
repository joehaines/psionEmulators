// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Converter tests. Run via:
//   node --experimental-strip-types frontend/src/lib/__tests__/converters.test.mts
// or `npm run test:converters` from frontend/.
//
// Covers: UID checksum, the public `tryConvert` dispatch, and each
// per-format converter against synthetic fixtures built in-test (no
// binary blobs needed).

import { readFileSync, existsSync } from 'node:fs';

import { tryConvert } from '../converters/index.ts';
import {
  computeUidChecksum, isValidEpocHeader, readEpocHeader, readUint32LE,
  UID3_WORD, UID3_SHEET, UID3_RECORD, UID3_MBM,
  UID1_DIRECT_FILE_STORE, UID2_MBM,
} from '../converters/epoc-uids.ts';
import { convertRecordToWav } from '../converters/record.ts';
import { convertSketchToPng } from '../converters/sketch.ts';
import { convertWordToRtf } from '../converters/word.ts';
import { convertSheetToCsv } from '../converters/sheet.ts';
import {
  isSiboWord, isSiboWordEncrypted, extractSiboWordText, decryptSiboWordFile,
  recoverBodyKey, scanSiboWord,
} from '../converters/psion-word-sibo.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}
function eq<T>(actual: T, expected: T, msg: string) {
  if (actual !== expected) {
    console.error(`FAIL: ${msg}: got ${String(actual)}, want ${String(expected)}`);
    failures++;
  }
}

// ---------- helpers ----------

function buildEpocHeader(uid1: number, uid2: number, uid3: number): Uint8Array {
  const checksum = computeUidChecksum(uid1, uid2, uid3);
  const header = new Uint8Array(16);
  const dv = new DataView(header.buffer);
  dv.setUint32(0,  uid1,     true);
  dv.setUint32(4,  uid2,     true);
  dv.setUint32(8,  uid3,     true);
  dv.setUint32(12, checksum, true);
  return header;
}

function concat(...parts: (Uint8Array | number[])[]): Uint8Array {
  const arrs = parts.map(p => p instanceof Uint8Array ? p : new Uint8Array(p));
  const total = arrs.reduce((n, a) => n + a.length, 0);
  const out = new Uint8Array(total);
  let off = 0;
  for (const a of arrs) { out.set(a, off); off += a.length; }
  return out;
}

function u32le(v: number): Uint8Array {
  const a = new Uint8Array(4);
  new DataView(a.buffer).setUint32(0, v >>> 0, true);
  return a;
}

// ---------- UID checksum ----------

const validHeader = buildEpocHeader(0x10000037, 0x10000042, 0x10000040);
const parsed = readEpocHeader(validHeader)!;
check(parsed !== null, 'header parses');
check(isValidEpocHeader(parsed), 'checksum verifies for a header we built ourselves');

// Same UIDs but corrupted checksum -> rejected.
const tampered = new Uint8Array(validHeader);
tampered[12] ^= 0xFF;
check(!isValidEpocHeader(readEpocHeader(tampered)!), 'corrupt checksum rejected');

// ---------- tryConvert dispatch ----------

eq(tryConvert(new Uint8Array(0), 'empty.x'), null, 'empty input -> null');
eq(tryConvert(new TextEncoder().encode('hello world, this is plain text'), 'note.txt'), null,
   'ASCII text -> null (no EPOC header)');

// EPOC header with an unregistered UID3 -> null
const unknownEpoc = buildEpocHeader(0x10000037, 0x10000000, 0xDEADBEEF);
eq(tryConvert(unknownEpoc, 'mystery.x'), null, 'unregistered UID3 -> null');

// ---------- Sketch / MBM round-trip ----------
// Build a 2x2 1bpp MBM (checkerboard) and verify it converts to a PNG
// whose first 8 bytes are the PNG signature.
{
  // Bitmap header (40 bytes, all u32 LE)
  const widthPx = 2, heightPx = 2, bpp = 1;
  // 1bpp packed LSB-first; 2 pixels = 2 bits in one byte.  Pixel layout
  // is checkerboard: row0 = [black, white], row1 = [white, black].
  // Encoding: bit0 = pixel 0, bit1 = pixel 1.  row0 = 0b10 = 0x02,
  // row1 = 0b01 = 0x01.  Each scanline padded to 4 bytes.
  const lineBytes = 4; // ceil(ceil(2*1/8)/4)*4 = 4
  const bitmapData = new Uint8Array(lineBytes * heightPx);
  bitmapData[0] = 0x02;
  bitmapData[lineBytes] = 0x01;
  const structSize = 40;
  const bitmapSize = structSize + bitmapData.length;

  const bmHeader = concat(
    u32le(bitmapSize),     // bitmapSize
    u32le(structSize),     // structSize
    u32le(widthPx),        // widthPixels
    u32le(heightPx),       // heightPixels
    u32le(widthPx * 15),   // widthTwips
    u32le(heightPx * 15),  // heightTwips
    u32le(bpp),            // bitsPerPixel
    u32le(0),              // isColor (grayscale)
    u32le(0),              // paletteEntries
    u32le(0),              // compression (none)
  );

  // Body layout (offsets relative to start of file):
  //   [0..16)   UID header
  //   [16..20)  trailer offset
  //   [20..)    bitmap header + data
  //   trailer:  count(1), header_offset(20)
  const header = buildEpocHeader(UID1_DIRECT_FILE_STORE, UID2_MBM, UID3_MBM);
  const bitmapSection = concat(bmHeader, bitmapData);
  const trailerOffset = header.length + 4 + bitmapSection.length;
  const trailer = concat(u32le(1), u32le(20));
  const file = concat(header, u32le(trailerOffset), bitmapSection, trailer);

  const png = convertSketchToPng(file);
  check(png !== null, 'Sketch: convertSketchToPng returned bytes');
  if (png) {
    const sig = [0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A];
    let sigOk = png.length >= 8;
    for (let i = 0; i < 8 && sigOk; i++) sigOk = png[i] === sig[i];
    check(sigOk, 'Sketch: output starts with PNG signature');
  }

  // End-to-end via tryConvert.
  const conv = tryConvert(file, 'Doodle.mbm');
  check(conv !== null, 'tryConvert: MBM matched');
  if (conv) {
    eq(conv.filename, 'Doodle.png', 'tryConvert: MBM renamed to .png');
    eq(conv.mime, 'image/png', 'tryConvert: MBM mime is image/png');
  }
}

// ---------- Record / WVE -> WAV ----------
// Synthetic fixture matching the real EPOC Record layout:
//   [16-byte UID header]
//   [4-byte section-table offset]
//   [Record Section: u32 len, u32 compression=0, u16 reps, u8 vol, u8 _,
//                    u32 reptime, u32 dataLen, ...samples...]
//   [section table: u8 count=1, (u32 record-sec UID, u32 record-sec offset)]
{
  const sampleCount = 16;
  const samples = new Uint8Array(sampleCount);
  for (let i = 0; i < sampleCount; i++) samples[i] = 0x80 + (i << 2); // walking PCM

  const header = buildEpocHeader(UID1_DIRECT_FILE_STORE, 0x1000006D, UID3_RECORD);
  const recordSectionBytes = concat(
    u32le(sampleCount),  // uncompressed length (samples)
    u32le(0),            // compression: 0 = PCM
    new Uint8Array([0, 0, 1, 0]),  // u16 reps + u8 volume + u8 reserved
    u32le(1_000_000),    // time between repeats (1s, unused)
    u32le(sampleCount),  // sound data length
    samples,
  );
  const recordSectionOffset = header.length + 4;                                // right after section-table-offset
  const sectionTableOffset  = recordSectionOffset + recordSectionBytes.length;
  const sectionTable = concat(
    new Uint8Array([2 /* count << 1 */]),
    u32le(0x10000052),                         // Record Section UID
    u32le(recordSectionOffset),
  );
  const file = concat(header, u32le(sectionTableOffset), recordSectionBytes, sectionTable);

  const wav = convertRecordToWav(file);
  check(wav !== null, 'Record: convertRecordToWav returned bytes');
  if (wav) {
    eq(String.fromCharCode(wav[0], wav[1], wav[2], wav[3]), 'RIFF', 'Record: starts with RIFF');
    eq(String.fromCharCode(wav[8], wav[9], wav[10], wav[11]), 'WAVE', 'Record: format is WAVE');
    eq(wav.length, 44 + sampleCount * 2, 'Record: WAV size matches sample count');
  }

  const conv = tryConvert(file, 'Memo.wve');
  check(conv !== null, 'tryConvert: WVE matched');
  if (conv) eq(conv.filename, 'Memo.wav', 'tryConvert: WVE renamed to .wav');
}

// ---------- Word -> RTF ----------
{
  const header = buildEpocHeader(0x10000039, 0x1000006A, UID3_WORD);
  // Body: short binary preamble + a paragraph + EPOC paragraph delimiter
  // (0x06) + a second paragraph.  The preamble bytes don't form a real
  // paragraph (no letters) so trimToBody drops them.
  const body = concat(
    [0x00, 0x00, 0x00, 0x00],
    new TextEncoder().encode('First paragraph of the doc'),
    [0x06],
    new TextEncoder().encode('Second paragraph here'),
    [0x06],
  );
  const file = concat(header, body);

  const rtf = convertWordToRtf(file);
  check(rtf !== null, 'Word: convertWordToRtf returned bytes');
  if (rtf) {
    const s = new TextDecoder().decode(rtf);
    check(s.startsWith('{\\rtf1'), 'Word: output starts with RTF magic');
    check(s.includes('First paragraph of the doc'), 'Word: first paragraph present');
    check(s.includes('Second paragraph here'),     'Word: second paragraph present');
    check(s.includes('\\par'),                     'Word: paragraph break emitted');
    check(s.endsWith('}\n') || s.endsWith('}'),    'Word: RTF document closes properly');
  }

  const conv = tryConvert(file, 'Letter.wrd');
  check(conv !== null, 'tryConvert: Word matched');
  if (conv) {
    eq(conv.filename, 'Letter.rtf',     'tryConvert: Word renamed to .rtf');
    eq(conv.mime,     'application/rtf','tryConvert: Word mime is application/rtf');
  }
}

// ---------- Sheet -> CSV ----------
{
  const header = buildEpocHeader(0x10000037, 0x1000006D, UID3_SHEET);
  // Build a minimal but structurally correct Sheet file:
  //   [16-byte UID header]
  //   [4-byte section-table offset]
  //   Workbook Section
  //   Worksheet List (1 worksheet)
  //   Worksheet (default layout flags = 0, so no embedded layout body)
  //   Cell List (3 cells: A1="Quarter", B1="Revenue", B2=42.5)
  //   Section Table

  // Cell list helpers ------------------------------------------------
  function packCellPos(row: number, col: number): Uint8Array {
    const v = (col << 2) | (row << 10);
    return new Uint8Array([v & 0xFF, (v >>> 8) & 0xFF, (v >>> 16) & 0xFF]);
  }
  function stringCell(row: number, col: number, s: string): Uint8Array {
    const flagsByte = 5 << 5;
    const lenByte = (s.length << 2) | 2;
    const strBody = new TextEncoder().encode(s);
    return concat(packCellPos(row, col), [flagsByte], [lenByte], strBody);
  }
  function floatCell(row: number, col: number, v: number): Uint8Array {
    const flagsByte = 4 << 5;
    const f = new Uint8Array(8);
    new DataView(f.buffer).setFloat64(0, v, true);
    return concat(packCellPos(row, col), [flagsByte], f);
  }
  function intCell(row: number, col: number, v: number): Uint8Array {
    const flagsByte = 1 << 5;
    const sign = v < 0 ? 0x80000000 : 0;
    const mag  = Math.abs(v) & 0x7FFFFFFF;
    return concat(packCellPos(row, col), [flagsByte], u32le((sign | mag) >>> 0));
  }
  function xInt(n: number): Uint8Array {
    return new Uint8Array([(n << 1) & 0xFE]);
  }

  // Build a complete Sheet file containing one worksheet with the
  // given cell records.
  function buildSheet(cells: Uint8Array[]): Uint8Array {
    const header = buildEpocHeader(0x10000037, 0x1000006D, UID3_SHEET);
    const cellsBody = concat([0x02, 0x00], xInt(cells.length), ...cells);

    const headerLen = 16 + 4;
    let off = headerLen;
    const workbookOff = off;       off += 17;
    const worksheetListOff = off;  off += 1 + 1 + 1 + 4;
    const worksheetOff = off;      off += 1 + 1 + 2 + 5 * 4;
    const cellListOff = off;       off += cellsBody.length;
    const sectionTableOff = off;

    const workbook = concat(
      [0x02],
      u32le(0), u32le(0), u32le(worksheetListOff), u32le(0),
    );
    const worksheetList = concat([0x02], xInt(1), [0x00], u32le(worksheetOff));
    const worksheet = concat(
      [0x04], [0x00], [0x02, 0x00],
      u32le(0), u32le(0), u32le(cellListOff), u32le(0), u32le(0),
    );
    const sectionTable = concat(
      [2 /* count = 1 << 1 */],
      u32le(0x1000011D), u32le(workbookOff),
    );
    return concat(header, u32le(sectionTableOff),
                  workbook, worksheetList, worksheet, cellsBody, sectionTable);
  }

  const file = buildSheet([
    stringCell(0, 0, 'Quarter'),
    stringCell(0, 1, 'Revenue'),
    floatCell (1, 1, 42.5),
  ]);

  const csv = convertSheetToCsv(file);
  check(csv !== null, 'Sheet: convertSheetToCsv returned bytes');
  if (csv) {
    const s = new TextDecoder().decode(csv).trim();
    const rows = s.split('\n');
    eq(rows.length, 2, 'Sheet: two rows emitted');
    eq(rows[0],     'Quarter,Revenue', 'Sheet: header row');
    eq(rows[1],     ',42.5',           'Sheet: data row with empty col A');
  }

  const negFile = buildSheet([intCell(0, 0, -42)]);
  const negCsv = convertSheetToCsv(negFile);
  check(negCsv !== null, 'Sheet: int cell file converts');
  if (negCsv) {
    const s = new TextDecoder().decode(negCsv).trim();
    eq(s, '-42', 'Sheet: signed int decodes as -42');
  }

  const conv = tryConvert(file, 'Budget.spr');
  check(conv !== null, 'tryConvert: Sheet matched');
  if (conv) eq(conv.filename, 'Budget.csv', 'tryConvert: Sheet renamed to .csv');
}

// ---------- Real-file regression: tests/fixtures/Sketch ----------
// A Sketch file saved on a 5mx (245x126, 2bpp grayscale, byte-RLE,
// wrapped in the Sketch app envelope with UID3 = 0x1000007D).
// Exercises the bitmap-header scan + RLE expansion together — the
// pieces that v1's synthetic-only tests missed.
{
  const refPath = new URL('../../../../tests/fixtures/Sketch', import.meta.url).pathname;
  if (existsSync(refPath)) {
    const bytes = new Uint8Array(readFileSync(refPath));
    const png = convertSketchToPng(bytes);
    check(png !== null, 'tests/fixtures/Sketch: convertSketchToPng returned bytes');
    if (png) {
      // PNG signature
      const sig = [0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A];
      let sigOk = png.length >= 8;
      for (let i = 0; i < 8 && sigOk; i++) sigOk = png[i] === sig[i];
      check(sigOk, 'tests/fixtures/Sketch: output is a real PNG');

      // IHDR comes right after signature: 4-byte length, "IHDR", then
      // width (BE u32), height (BE u32).  Sample is 245 x 126.
      const dv = new DataView(png.buffer, png.byteOffset, png.byteLength);
      const width  = dv.getUint32(16, false);
      const height = dv.getUint32(20, false);
      eq(width,  245, 'tests/fixtures/Sketch: PNG width');
      eq(height, 126, 'tests/fixtures/Sketch: PNG height');
    }

    const conv = tryConvert(bytes, 'Sketch');
    check(conv !== null, 'tests/fixtures/Sketch: tryConvert dispatched');
    if (conv) eq(conv.filename, 'Sketch.png', 'tests/fixtures/Sketch: renamed to .png');
  } else {
    console.log('SKIP tests/fixtures/Sketch: file not present');
  }
}

// ---------- Real-file regression: tests/fixtures/Word2 ----------
// Four paragraphs, each labelled with its formatting:
//   1. "this whole line is heading 1"  -> Heading 1 style
//   2. "...alignment left"             -> Normal, left-aligned
//   3. "...alignment center"           -> Normal, center-aligned
//   4. "...alignment right"            -> Normal, right-aligned
// Exercises paragraph-level style + alignment decoding.
{
  const refPath = new URL('../../../../tests/fixtures/Word2', import.meta.url).pathname;
  if (existsSync(refPath)) {
    const bytes = new Uint8Array(readFileSync(refPath));
    const rtf = convertWordToRtf(bytes);
    check(rtf !== null, 'tests/fixtures/Word2: convertWordToRtf returned bytes');
    if (rtf) {
      const s = new TextDecoder().decode(rtf);
      // Paragraph 1: Heading 1 - assert \fs28 + \b within its paragraph properties.
      const idxHeading = s.indexOf('this whole line is heading 1');
      check(idxHeading > 0, 'tests/fixtures/Word2: heading paragraph present');
      // Look at the slice between the previous \pard and the heading text.
      const beforeHeading = s.substring(0, idxHeading);
      const lastPard = beforeHeading.lastIndexOf('\\pard');
      const headingProps = beforeHeading.substring(lastPard);
      check(headingProps.includes('\\fs28'),
            'tests/fixtures/Word2: Heading 1 carries \\fs28');
      check(headingProps.includes('\\b'),
            'tests/fixtures/Word2: Heading 1 carries \\b');
      // Paragraph 3: center-aligned.
      const idxCenter = s.indexOf('alignment center');
      check(idxCenter > 0, 'tests/fixtures/Word2: center paragraph present');
      check(s.substring(0, idxCenter).lastIndexOf('\\qc')
              > s.substring(0, idxCenter).lastIndexOf('\\qr'),
            'tests/fixtures/Word2: center paragraph uses \\qc');
      // Paragraph 4: right-aligned.
      const idxRight = s.indexOf('alignment right');
      check(idxRight > 0, 'tests/fixtures/Word2: right paragraph present');
      check(s.substring(0, idxRight).lastIndexOf('\\qr')
              > s.substring(0, idxRight).lastIndexOf('\\qc'),
            'tests/fixtures/Word2: right paragraph uses \\qr');
    }
  } else {
    console.log('SKIP tests/fixtures/Word2: file not present');
  }
}

// ---------- Real-file regression: tests/fixtures/WordFormatting ----------
// Updated for the full layout parser: now expects paragraph styles
// (Heading 1/2/3 with their fs/bold), font sizes (4pt -> \fs8, 50pt
// -> \fs100), character runs (bold/italic/underline), and alignment
// keywords.  Each paragraph in the source document is labelled with
// its formatting, so the assertions read directly off the labels.
{
  const refPath = new URL('../../../../tests/fixtures/WordFormatting', import.meta.url).pathname;
  if (existsSync(refPath)) {
    const bytes = new Uint8Array(readFileSync(refPath));
    const rtf = convertWordToRtf(bytes);
    check(rtf !== null, 'tests/fixtures/WordFormatting: convertWordToRtf returned bytes');
    if (rtf) {
      const s = new TextDecoder().decode(rtf);
      check(s.startsWith('{\\rtf1'), 'tests/fixtures/WordFormatting: valid RTF preamble');
      // Helper: assert the paragraph-properties block immediately
      // preceding `label` contains every fragment in `props`.
      const propsBefore = (label: string): string => {
        const idx = s.indexOf(label);
        if (idx < 0) return '';
        const slice = s.substring(0, idx);
        const lastPard = slice.lastIndexOf('\\pard');
        return slice.substring(lastPard);
      };
      const heading1Props = propsBefore('Heading 1');
      check(heading1Props.includes('\\fs28') && heading1Props.includes('\\b'),
            'WordFormatting: Heading 1 styled (\\fs28 + \\b)');
      const heading2Props = propsBefore('Heading 2');
      check(heading2Props.includes('\\fs24') && heading2Props.includes('\\b'),
            'WordFormatting: Heading 2 styled (\\fs24 + \\b)');
      // Heading 3 is user-customised in this sample (not the default
      // italic+ul) — we just assert SOMETHING is set for it.
      const heading3Props = propsBefore('Heading 3');
      check(heading3Props.length > '\\pard\\ql'.length,
            'WordFormatting: Heading 3 has paragraph properties beyond default');
      // Font sizes (inline runs override paragraph default).
      check(s.includes('{\\fs8 4pt}'),    'WordFormatting: 4pt -> \\fs8');
      check(s.includes('{\\fs100 50pt}'), 'WordFormatting: 50pt -> \\fs100');
      // Font typeface switches.
      check(/\{\\f[12] Arial\}/.test(s),       'WordFormatting: Arial via \\fN');
      check(/\{\\f[12] Courier New\}/.test(s), 'WordFormatting: Courier New via \\fN');
      // Character-run formatting.
      check(s.includes('{\\b Bold}'),        'WordFormatting: bold run');
      check(s.includes('{\\i Italic}'),      'WordFormatting: italic run');
      check(s.includes('{\\ul Underlined}'), 'WordFormatting: underline run');
      // Alignment (paragraph-properties block ends with text).
      check(propsBefore('Align right').includes('\\qr'),
            'WordFormatting: right alignment');
      check(propsBefore('Align Center').includes('\\qc'),
            'WordFormatting: center alignment');
      check(propsBefore('Align spaced').includes('\\qj'),
            'WordFormatting: justified alignment');
      // Bullet.
      check(s.includes('\\bullet'), 'WordFormatting: bullet point');
    }
  } else {
    console.log('SKIP tests/fixtures/WordFormatting: file not present');
  }
}

// ---------- Real-file regression: tests/fixtures/Word1 ----------
// One paragraph mixing bold/italic/underlined character runs:
//   "This is bold and this is italic and this is underlined"
// Exercises the character-format-run parser (markup stream).
{
  const refPath = new URL('../../../../tests/fixtures/Word1', import.meta.url).pathname;
  if (existsSync(refPath)) {
    const bytes = new Uint8Array(readFileSync(refPath));
    const rtf = convertWordToRtf(bytes);
    check(rtf !== null, 'tests/fixtures/Word1: convertWordToRtf returned bytes');
    if (rtf) {
      const s = new TextDecoder().decode(rtf);
      // Body text in full
      check(s.includes('This is'),                        'tests/fixtures/Word1: body present');
      check(s.includes('and this is'),                    'tests/fixtures/Word1: body continuation present');
      // Each formatting group should appear, wrapping the right word.
      check(s.includes('{\\b bold}'),                     'tests/fixtures/Word1: bold run wraps "bold"');
      check(s.includes('{\\i italic}'),                   'tests/fixtures/Word1: italic run wraps "italic"');
      check(s.includes('{\\ul underlined}'),              'tests/fixtures/Word1: underline run wraps "underlined"');
    }
  } else {
    console.log('SKIP tests/fixtures/Word1: file not present');
  }
}

// ---------- Real-file regression: tests/fixtures/123 ----------
// A real Record (.wve) voice file: 2.4s of "one, two, three" at 8 kHz.
// Locks in the audio decode against the user-provided reference WAVs
// (tests/fixtures/123_as_*khz.wav): samples are SIGNED 8-bit PCM expanded
// to 16-bit via `signed8 << 8`.  Previously decoded as unsigned PCM
// which produced wildly-swinging samples and inaudible static.
{
  const refPath = new URL('../../../../tests/fixtures/123', import.meta.url).pathname;
  if (existsSync(refPath)) {
    const bytes = new Uint8Array(readFileSync(refPath));
    const wav = convertRecordToWav(bytes);
    check(wav !== null, 'tests/fixtures/123: convertRecordToWav returned bytes');
    if (wav) {
      const dv = new DataView(wav.buffer, wav.byteOffset, wav.byteLength);
      eq(String.fromCharCode(wav[0], wav[1], wav[2], wav[3]), 'RIFF', 'tests/fixtures/123: RIFF magic');
      eq(dv.getUint32(24, true), 8000, 'tests/fixtures/123: 8 kHz sample rate');
      eq(dv.getUint16(34, true), 16, 'tests/fixtures/123: 16-bit output');
      // Source byte 0xFF (-1 signed) -> sample -256 (= -1 << 8).
      // Source byte 0x00         -> sample 0.  The voice file's first
      // audio bytes start with a 0x00 followed by several 0xFFs — check
      // that the first ~5 output samples follow that pattern, which
      // proves SIGNED interpretation (unsigned PCM would give +32512 / -32768).
      const s0 = dv.getInt16(44 + 0 * 2, true);
      const s1 = dv.getInt16(44 + 1 * 2, true);
      const s2 = dv.getInt16(44 + 2 * 2, true);
      check(s0 === 0,    `tests/fixtures/123: sample 0 is 0 (got ${s0})`);
      check(s1 === -256, `tests/fixtures/123: sample 1 is -256 (got ${s1})`);
      check(s2 === -256, `tests/fixtures/123: sample 2 is -256 (got ${s2})`);
    }
  } else {
    console.log('SKIP tests/fixtures/123: file not present');
  }
}

// ---------- Real-file regression: tests/fixtures/Sheet ----------
// A 43-byte "stub" Sheet file (EPOC creates this when the app saves
// without any cell data: just the UID header + Application ID Section).
// Asserts that the converter recognises this as an empty Sheet and
// returns null — NOT a CSV with "&Sheet.app" pulled from the app-id
// metadata via the heuristic fallback.
{
  const refPath = new URL('../../../../tests/fixtures/Sheet', import.meta.url).pathname;
  if (existsSync(refPath)) {
    const bytes = new Uint8Array(readFileSync(refPath));
    const csv = convertSheetToCsv(bytes);
    check(csv === null, 'tests/fixtures/Sheet: empty Sheet returns null (no fabricated content)');
    const conv = tryConvert(bytes, 'Sheet');
    check(conv === null, 'tests/fixtures/Sheet: tryConvert returns null so dialog downloads original');
  } else {
    console.log('SKIP tests/fixtures/Sheet: file not present');
  }
}

// ---------- Real-file regression: tests/fixtures/TestSheet ----------
// A populated Sheet: 3 rows x 2 cols with header row + integer cells.
//   A1: Year     B1: Revenue
//   A2: 2024     B2: 100
//   A3: 2025     B3: 150
// Exercises the full Workbook -> Worksheet -> Cell List -> Cell
// decode chain (string cells AND integer cells, at non-zero rows).
{
  const refPath = new URL('../../../../tests/fixtures/TestSheet', import.meta.url).pathname;
  if (existsSync(refPath)) {
    const bytes = new Uint8Array(readFileSync(refPath));
    const csv = convertSheetToCsv(bytes);
    check(csv !== null, 'tests/fixtures/TestSheet: convertSheetToCsv returned bytes');
    if (csv) {
      const s = new TextDecoder().decode(csv).trim();
      const rows = s.split('\n');
      eq(rows.length, 3, 'tests/fixtures/TestSheet: three rows emitted');
      eq(rows[0], 'Year,Revenue', 'tests/fixtures/TestSheet: header row');
      eq(rows[1], '2024,100',     'tests/fixtures/TestSheet: first data row');
      eq(rows[2], '2025,150',     'tests/fixtures/TestSheet: second data row');
    }
    const conv = tryConvert(bytes, 'TestSheet');
    check(conv !== null, 'tests/fixtures/TestSheet: tryConvert dispatched');
    if (conv) eq(conv.filename, 'TestSheet.csv', 'tests/fixtures/TestSheet: renamed to .csv');
  } else {
    console.log('SKIP tests/fixtures/TestSheet: file not present');
  }
}

// ---------- ROM regressions: the "Welcome to ..." documents ----------
// Every EPOC device ROM ships sample Word files under Z:\System\Samples,
// including a "Welcome to <device>" document.  They make ideal converter
// tests: real documents with headings, bullet lists, embedded Sketch
// pictures AND embedded Sheet charts.  We carve them straight out of the
// ROM images (EPOC ROM file systems store file contents contiguously;
// a Word file's length is recoverable from its stream dictionary).
//
// The Series 7 file is the richest: an 8bpp-colour EPOC-logo picture
// whose bitmap header sits at an ODD file offset (regression for the
// even-stride scan bug that dropped every such image), a Sheet bar
// chart, and bullet points.

function carveEpocFile(rom: Uint8Array, offset: number): Uint8Array | null {
  const dictOff = readUint32LE(rom, offset + 16);
  if (dictOff < 20 || offset + dictOff + 1 > rom.length) return null;
  const countByte = rom[offset + dictOff];
  if ((countByte & 1) !== 0 || countByte === 0) return null;
  const fileLen = dictOff + 1 + (countByte >> 1) * 8;
  if (offset + fileLen > rom.length) return null;
  return rom.subarray(offset, offset + fileLen);
}

{
  const romPath = new URL('../../../../roms/S7_v1.05(254)_b754_eng.bin', import.meta.url).pathname;
  if (existsSync(romPath)) {
    const rom = new Uint8Array(readFileSync(romPath));
    const file = carveEpocFile(rom, 0xa777e0)!;   // Z:\System\Samples\Welcome to Series 7
    check(file !== null, 'S7 Welcome: carved from ROM');
    const conv = tryConvert(file, 'Welcome to Series 7');
    check(conv !== null, 'S7 Welcome: tryConvert dispatched');
    if (conv) {
      eq(conv.filename, 'Welcome to Series 7.rtf', 'S7 Welcome: renamed to .rtf');
      const s = new TextDecoder().decode(conv.bytes);
      check(s.startsWith('{\\rtf1'), 'S7 Welcome: valid RTF preamble');
      check(s.includes('Welcome to Series 7'), 'S7 Welcome: body text present');
      // The EPOC-logo picture: 245x296 8bpp colour, bitmap header at an
      // odd offset, displayed at the document's own 693x829 twips.
      check(s.includes('\\pict\\pngblip\\picw245\\pich296\\picwgoal693\\pichgoal829'),
            'S7 Welcome: colour picture embedded at document display size');
      // The Sheet chart can't be rasterised, but its data comes through.
      check(s.includes('[Sheet chart'), 'S7 Welcome: chart marker present');
      check(s.includes('Jan\\tab 12\\tab 6\\line Feb\\tab 18\\tab 12'),
            'S7 Welcome: chart cell data extracted');
      check(s.includes('\\bullet'), 'S7 Welcome: bullet points present');
    }

    // Same ROM carries non-document data under Word's UID3 (carved at
    // 0x944650).  The heuristic body finder used to emit pages of
    // escaped Latin-1 garbage for these; they must be rejected so the
    // CF dialog falls back to a raw download.
    const junk = carveEpocFile(rom, 0x944650);
    if (junk) {
      eq(tryConvert(junk, 'NotReallyWord'), null,
         'S7 non-document with Word UID: rejected, no garbage RTF');
    }
  } else {
    console.log('SKIP roms/S7: file not present');
  }
}

{
  const romPath = new URL('../../../../roms/5mx_v1.05(260)_eng.bin', import.meta.url).pathname;
  if (existsSync(romPath)) {
    const rom = new Uint8Array(readFileSync(romPath));
    const file = carveEpocFile(rom, 0x3d3d90)!;   // Z:\System\Samples\Welcome to Series 5mx
    check(file !== null, '5mx Welcome: carved from ROM');
    const conv = tryConvert(file, 'Welcome');
    check(conv !== null, '5mx Welcome: tryConvert dispatched');
    if (conv) {
      const s = new TextDecoder().decode(conv.bytes);
      // The heading bolds the leading "W" as a separate inline run:
      // "{\b\fs28 W}elcome to the {\b Series 5mx }- powerful..."
      check(s.includes('elcome to the ') && s.includes('Series 5mx'),
            '5mx Welcome: body text present');
      check(s.includes('powerful portable computing'), '5mx Welcome: prose present');
      // Greyscale 190x86 picture (even offset, uncompressed).
      check(s.includes('\\pict\\pngblip\\picw190\\pich86'),
            '5mx Welcome: picture embedded');
      check(s.includes('Jan\\tab 12\\tab 6'), '5mx Welcome: chart data extracted');
    }
  } else {
    console.log('SKIP roms/5mx: file not present');
  }
}

// ---------- SIBO / EPOC16 Word (.WRD), incl. password recovery ----------
//
// The plain document (applib DOCPAD.WRD) and an encrypted copy of it
// (tests/fixtures/WRD-ENCRYPTED-DOCPAD.WRD, made by password-protecting
// the same file on the emulated Series 3a) let us assert exact recovery:
// the body decrypted without the password must equal the plaintext body.
{
  const plainPath = new URL('../../../../applib/s3util/1k-pad/DOCPAD.WRD', import.meta.url).pathname;
  const encPath   = new URL('../../../../tests/fixtures/WRD-ENCRYPTED-DOCPAD.WRD', import.meta.url).pathname;
  if (existsSync(plainPath) && existsSync(encPath)) {
    const plain = new Uint8Array(readFileSync(plainPath));
    const enc   = new Uint8Array(readFileSync(encPath));

    check(isSiboWord(plain),  'SIBO Word: plain file detected by magic');
    check(isSiboWord(enc),    'SIBO Word: encrypted file detected by magic');
    eq(isSiboWordEncrypted(plain), false, 'SIBO Word: plain file not flagged encrypted');
    eq(isSiboWordEncrypted(enc),   true,  'SIBO Word: encrypted file flagged encrypted');

    // Same record layout in both — encryption only rewrites the body.
    const sp = scanSiboWord(plain)!, se = scanSiboWord(enc)!;
    eq(se.records.length, sp.records.length, 'SIBO Word: record count unchanged by encryption');
    check(sp.body !== null && se.body !== null, 'SIBO Word: body record located in both');

    const plainText = extractSiboWordText(plain)!;
    const autoText  = extractSiboWordText(enc)!;
    check(plainText.text.startsWith('1k-pad\nVersion 1.0'), 'SIBO Word: plain body text extracted');
    // Automatic (no-password, no-crib) recovery must reproduce the body exactly.
    eq(autoText.text, plainText.text, 'SIBO Word: password recovered automatically (exact)');
    check(autoText.wasEncrypted, 'SIBO Word: recovery reports the file was encrypted');
    check(autoText.confidence > 0.95, 'SIBO Word: high recovery confidence');

    // Direct key recovery API: recovered key decrypts the raw body.
    const rawBody = enc.subarray(se.body!.start, se.body!.end);
    const rec = recoverBodyKey(rawBody);
    eq(rec.key.length, 9, 'SIBO Word: 9-byte keystream recovered');
    check(rec.plainBody.every((b, i) => b === plain[sp.body!.start + i]),
          'SIBO Word: recovered body equals plaintext body byte-for-byte');

    // A correct crib pins the key exactly even with no statistics. The
    // crib is literal body plaintext (paragraph breaks are 0x00), so use
    // the true first nine body bytes — one per key position.
    const crib = plain.subarray(sp.body!.start, sp.body!.start + 9);
    const cribbed = recoverBodyKey(rawBody, crib);
    check(cribbed.plainBody.every((b, i) => b === plain[sp.body!.start + i]),
          'SIBO Word: crib recovery equals plaintext body');

    // "Remove password" produces a plain, self-consistent file.
    const decFile = decryptSiboWordFile(enc)!;
    check(decFile !== null, 'SIBO Word: decryptSiboWordFile produced output');
    eq(isSiboWordEncrypted(decFile), false, 'SIBO Word: decrypted file no longer flagged encrypted');
    eq(extractSiboWordText(decFile)!.text, plainText.text,
       'SIBO Word: decrypted file extracts identical text');

    // Dispatch through the public tryConvert path.
    const conv = tryConvert(enc, 'JOESDOC.WRD');
    check(conv !== null, 'SIBO Word: tryConvert handles encrypted .WRD');
    if (conv) {
      eq(conv.filename, 'JOESDOC.txt', 'SIBO Word: converted to .txt');
      eq(conv.recoveredPassword, true, 'SIBO Word: tryConvert flags password recovery');
      eq(new TextDecoder().decode(conv.bytes), plainText.text,
         'SIBO Word: tryConvert output equals plaintext');
    }
  } else {
    console.log('SKIP SIBO Word: DOCPAD/encrypted fixture not present');
  }
}

if (failures === 0) {
  console.log('PASS converters (UID + Sketch + Record + Word + Sheet + SIBO Word)');
  process.exit(0);
} else {
  console.error(`FAIL: ${failures} assertion(s)`);
  process.exit(1);
}
