// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Tiny from-scratch PDF writer for captured print jobs: monospaced text,
// A4, one PDF page per printed page (long pages spill onto extra ones).
// Hand-rolled like the rest of the converters (cf. converters/png-encoder.ts)
// so we don't pull a PDF library into the bundle for what is a fixed,
// Courier-only layout.
//
// Text is encoded as WinAnsiEncoding (= CP1252), which round-trips
// everything decodePrintJob can produce; anything outside CP1252 becomes
// a question mark.

import { CP1252_C1 } from './decode.ts';

const PAGE_W = 595;   // A4 in points
const PAGE_H = 842;
const MARGIN = 50;
const FONT_SIZE = 10;
const LEADING = 12;
const COLS = 80;                                          // wrap width (Courier 10pt = 6pt/char)
const ROWS = Math.floor((PAGE_H - 2 * MARGIN) / LEADING); // 61 lines/page

// Inverse CP1252 map: characters whose WinAnsi byte differs from their
// Unicode code point.
const TO_CP1252: Record<string, number> = {};
for (const [byte, ch] of Object.entries(CP1252_C1)) TO_CP1252[ch] = Number(byte);

function latin1(s: string): Uint8Array {
  const out = new Uint8Array(s.length);
  for (let i = 0; i < s.length; i++) {
    const c = s.charCodeAt(i);
    out[i] = c <= 0xFF ? c : (TO_CP1252[s[i]] ?? 0x3F);
  }
  return out;
}

function escapePdfString(s: string): string {
  return s.replace(/\\/g, '\\\\').replace(/\(/g, '\\(').replace(/\)/g, '\\)');
}

// Split decoded pages into PDF-page-sized line blocks: hard-wrap at COLS,
// then chunk every ROWS lines, keeping printed-page boundaries.
function layoutPages(pages: string[]): string[][] {
  const out: string[][] = [];
  for (const page of pages) {
    const wrapped: string[] = [];
    for (const line of page.split('\n')) {
      // Expand tabs to the next 8-column stop before wrapping.
      let expanded = '';
      for (const ch of line) {
        if (ch === '\t') expanded += ' '.repeat(8 - (expanded.length % 8));
        else expanded += ch;
      }
      if (expanded.length === 0) { wrapped.push(''); continue; }
      for (let i = 0; i < expanded.length; i += COLS) wrapped.push(expanded.slice(i, i + COLS));
    }
    for (let i = 0; i < Math.max(1, wrapped.length); i += ROWS) {
      out.push(wrapped.slice(i, i + ROWS));
    }
  }
  return out.length > 0 ? out : [[]];
}

export function textPagesToPdf(pages: string[]): Uint8Array {
  const pdfPages = layoutPages(pages);

  // Object numbering: 1 catalog, 2 pages root, 3 font, then for page i:
  // 4+2i = page object, 5+2i = its content stream.
  const pageObjNum = (i: number) => 4 + 2 * i;
  const kids = pdfPages.map((_, i) => `${pageObjNum(i)} 0 R`).join(' ');

  const objects: { num: number; body: Uint8Array }[] = [];
  const textObj = (num: number, body: string) => objects.push({ num, body: latin1(body) });

  textObj(1, `<< /Type /Catalog /Pages 2 0 R >>`);
  textObj(2, `<< /Type /Pages /Kids [${kids}] /Count ${pdfPages.length} >>`);
  textObj(3, `<< /Type /Font /Subtype /Type1 /BaseFont /Courier /Encoding /WinAnsiEncoding >>`);

  pdfPages.forEach((lines, i) => {
    textObj(pageObjNum(i),
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ${PAGE_W} ${PAGE_H}] ` +
      `/Resources << /Font << /F1 3 0 R >> >> /Contents ${pageObjNum(i) + 1} 0 R >>`);
    const ops = [
      'BT',
      `/F1 ${FONT_SIZE} Tf`,
      `${LEADING} TL`,
      `${MARGIN} ${PAGE_H - MARGIN + LEADING - FONT_SIZE} Td`,
      ...lines.map(l => `T* (${escapePdfString(l)}) Tj`),
      'ET',
    ].join('\n');
    const stream = latin1(ops);
    const head = latin1(`<< /Length ${stream.length} >>\nstream\n`);
    const tail = latin1(`\nendstream`);
    const body = new Uint8Array(head.length + stream.length + tail.length);
    body.set(head, 0); body.set(stream, head.length); body.set(tail, head.length + stream.length);
    objects.push({ num: pageObjNum(i) + 1, body });
  });

  // Serialize with a byte-accurate xref table.
  const chunks: Uint8Array[] = [];
  let offset = 0;
  const push = (b: Uint8Array) => { chunks.push(b); offset += b.length; };

  push(latin1('%PDF-1.4\n'));
  const xrefOffsets: number[] = [];
  objects.sort((a, b) => a.num - b.num);
  for (const obj of objects) {
    xrefOffsets[obj.num] = offset;
    push(latin1(`${obj.num} 0 obj\n`));
    push(obj.body);
    push(latin1(`\nendobj\n`));
  }

  const xrefStart = offset;
  const count = objects.length + 1;
  let xref = `xref\n0 ${count}\n0000000000 65535 f \n`;
  for (let n = 1; n < count; n++) {
    xref += `${String(xrefOffsets[n]).padStart(10, '0')} 00000 n \n`;
  }
  xref += `trailer\n<< /Size ${count} /Root 1 0 R >>\nstartxref\n${xrefStart}\n%%EOF\n`;
  push(latin1(xref));

  const out = new Uint8Array(offset);
  let pos = 0;
  for (const c of chunks) { out.set(c, pos); pos += c.length; }
  return out;
}
