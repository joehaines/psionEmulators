// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Printer-capture tests. Run via:
//   node --experimental-strip-types frontend/src/lib/__tests__/printer.test.mts
// or `npm run test:printer` from frontend/.
//
// Covers: the ESC/P-stripping text decoder, the from-scratch PDF writer
// (structure + byte-accurate xref), and the PrinterCapture poll/attach
// lifecycle against a fake serial bridge.

import { decodePrintJob } from '../printer/decode.ts';
import { textPagesToPdf } from '../printer/pdf.ts';
import { PrinterCapture } from '../printer/capture.ts';

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

function bytes(...parts: (string | number[])[]): Uint8Array {
  const out: number[] = [];
  for (const p of parts) {
    if (typeof p === 'string') for (const ch of p) out.push(ch.charCodeAt(0));
    else out.push(...p);
  }
  return new Uint8Array(out);
}

// ---------- decodePrintJob ----------

{
  // Plain text, CRLF line ends, FF page break.
  const job = decodePrintJob(bytes('Hello\r\nWorld\r\n', [0x0C], 'Page two\r\n'));
  eq(job.pages.length, 2, 'CRLF/FF: two pages');
  eq(job.pages[0], 'Hello\nWorld', 'CRLF collapses to one newline');
  eq(job.pages[1], 'Page two', 'text after FF lands on page 2');
  eq(job.text, 'Hello\nWorld\n\f\nPage two', 'combined text keeps the \\f marker');
}

{
  // Lone CR is a newline; trailing blank lines are trimmed per page.
  const job = decodePrintJob(bytes('a\rb\r\n\r\n\r\n'));
  eq(job.pages[0], 'a\nb', 'lone CR breaks the line, padding trimmed');
}

{
  // ESC/P sequences are stripped without eating the text around them.
  const job = decodePrintJob(bytes(
    [0x1B], '@',                 // ESC @  reset
    [0x1B], 'x', [0x01],         // ESC x 1  NLQ
    'bold:',
    [0x1B], 'E', 'yes', [0x1B], 'F',   // ESC E / ESC F  bold on/off
    '\r\n',
    [0x1B], '*', [0x00, 0x03, 0x00, 0xAA, 0xBB, 0xCC],  // bit image, 3 data bytes
    'after-graphics\r\n',
    [0x1B], 'D', [0x08, 0x10, 0x00],   // tab stops, NUL-terminated
    'tabs\r\n',
  ));
  eq(job.pages[0], 'bold:yes\nafter-graphics\ntabs', 'ESC/P stripped cleanly');
}

{
  // CP1252 C1 range maps to the right glyphs; other controls drop.
  const job = decodePrintJob(bytes([0x93], 'hi', [0x94, 0x07]));
  eq(job.pages[0], '“hi”', 'CP1252 curly quotes decoded, BEL dropped');
}

{
  eq(decodePrintJob(new Uint8Array(0)).pages.length, 0, 'empty capture has no pages');
}

// ---------- textPagesToPdf ----------

function latin1Str(b: Uint8Array): string {
  let s = '';
  for (const x of b) s += String.fromCharCode(x);
  return s;
}

{
  const pdf = textPagesToPdf(['Hello (PDF) \\world', 'second page']);
  const s = latin1Str(pdf);
  check(s.startsWith('%PDF-1.4\n'), 'PDF header present');
  check(s.endsWith('%%EOF\n'), 'PDF trailer present');
  eq((s.match(/\/Type \/Page[^s]/g) ?? []).length, 2, 'one PDF page per printed page');
  check(s.includes('/Count 2'), 'pages root counts 2');
  check(s.includes('\\(PDF\\)') && s.includes('\\\\world'), 'parens and backslash escaped');
  check(s.includes('/BaseFont /Courier'), 'Courier font object present');

  // startxref points at the xref table, and each xref offset points at
  // the right "N 0 obj" line.
  const sx = /startxref\n(\d+)\n%%EOF\n$/.exec(s);
  check(sx !== null, 'startxref parsable');
  if (sx) {
    eq(s.slice(Number(sx[1]), Number(sx[1]) + 4), 'xref', 'startxref points at xref');
    const table = /xref\n0 (\d+)\n0000000000 65535 f \n((?:\d{10} 00000 n \n)+)/.exec(s);
    check(table !== null, 'xref table parsable');
    if (table) {
      const offsets = table[2].trim().split('\n').map(l => parseInt(l.slice(0, 10), 10));
      eq(offsets.length, Number(table[1]) - 1, 'xref row per object');
      offsets.forEach((off, idx) => {
        check(s.slice(off).startsWith(`${idx + 1} 0 obj`), `xref offset ${idx + 1} lands on its object`);
      });
    }
  }
}

{
  // A printed page longer than one A4 page of lines spills onto a second
  // PDF page; an empty job still yields one (blank) page.
  const long = Array.from({ length: 70 }, (_, i) => `line ${i}`).join('\n');
  const s = latin1Str(textPagesToPdf([long]));
  check(s.includes('/Count 2'), 'long page spills onto a second PDF page');
  const empty = latin1Str(textPagesToPdf([]));
  check(empty.includes('/Count 1'), 'empty job still produces one page');
}

{
  // CP1252 round trip: decoded “ ” re-encode to WinAnsi bytes 0x93/0x94.
  const pdf = textPagesToPdf(['“q”']);
  let found = false;
  for (let i = 0; i + 2 < pdf.length; i++) {
    if (pdf[i] === 0x93 && pdf[i + 1] === 0x71 && pdf[i + 2] === 0x94) found = true;
  }
  check(found, 'curly quotes encoded as WinAnsi 0x93/0x94');
}

// ---------- PrinterCapture ----------

{
  // Fake bridge: a queue of chunks the device "prints".
  let attached = false;
  const queue: Uint8Array[] = [];
  let attachCalls = 0;
  const bridge = {
    serialAttachHost: (_u: number) => { attachCalls++; attached = true; return true; },
    serialDetachHost: (_u: number) => { attached = false; return true; },
    serialIsAttached: (_u: number) => attached,
    serialReadBytes:  (_u: number) => queue.shift() ?? new Uint8Array(0),
  };

  const got: number[] = [];
  let err = '';
  const cap = new PrinterCapture(bridge, 2, {
    onData: c => got.push(...c),
    onError: m => { err = m; },
  });

  check(cap.start(), 'capture starts on a free port');
  eq(attachCalls, 1, 'attached exactly once');
  queue.push(bytes('abc'));
  cap.tick();
  queue.push(bytes('def'));
  cap.stop();           // stop() does a final drain before detaching
  eq(String.fromCharCode(...got), 'abcdef', 'all chunks delivered, including the final drain');
  check(!attached, 'detached after stop');
  eq(err, '', 'no errors on the happy path');

  // A busy port is refused with a readable error and no attach.
  attached = true;
  const cap2 = new PrinterCapture(bridge, 2, { onData: () => {}, onError: m => { err = m; } });
  check(!cap2.start(), 'start refused while port is busy');
  check(err.includes('in use'), 'busy error mentions the port is in use');
  eq(attachCalls, 1, 'no second attach attempted');
}

if (failures > 0) {
  console.error(`\n${failures} failure(s)`);
  process.exit(1);
}
console.log('printer.test.mts: all tests passed');
