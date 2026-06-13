// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Decode a captured print job into plain text pages.
//
// The happy path is the device's "General (Text only)" driver, which
// emits character data with CR/LF line ends and FF between pages. The
// Epson FX-80 / ESC P 2 drivers wrap the same text in ESC/P control
// sequences (typeface, pitch, line spacing, bit-image graphics); we
// strip those so the text underneath still reads cleanly. Graphics-only
// jobs (e.g. an HP PCL Sketch print) won't decode to anything useful —
// that's what the raw .prn download is for.
//
// Character set: EPOC32 prints in the Windows-1252 superset of Latin-1,
// so the C1 range 0x80–0x9F is mapped through the CP1252 table (curly
// quotes, en/em dashes, £ and € land correctly); everything else is
// Latin-1 verbatim. SIBO's CP850 differs in the upper half, but the
// ASCII range — i.e. all of the text people actually print — is
// identical, so one decoder covers both without a per-device switch.

export interface DecodedPrintJob {
  pages: string[];   // one entry per FF-separated page, controls stripped
  text: string;      // pages joined with "\n\f\n" — '\f' marks page breaks
}

// CP1252 mappings for 0x80–0x9F (0x81/0x8D/0x8F/0x90/0x9D are unused).
// Exported for pdf.ts, which needs the inverse to emit WinAnsiEncoding.
export const CP1252_C1: Record<number, string> = {
  0x80: '€', 0x82: '‚', 0x83: 'ƒ', 0x84: '„',
  0x85: '…', 0x86: '†', 0x87: '‡', 0x88: 'ˆ',
  0x89: '‰', 0x8A: 'Š', 0x8B: '‹', 0x8C: 'Œ',
  0x8E: 'Ž', 0x91: '‘', 0x92: '’', 0x93: '“',
  0x94: '”', 0x95: '•', 0x96: '–', 0x97: '—',
  0x98: '˜', 0x99: '™', 0x9A: 'š', 0x9B: '›',
  0x9C: 'œ', 0x9E: 'ž', 0x9F: 'Ÿ',
};

// ESC/P commands with a fixed argument byte count after the command
// letter. Commands not listed (and not handled specially below) are
// skipped as zero-argument.
const ESCP_FIXED_ARGS: Record<number, number> = {
  0x21: 1, // ESC ! n   master select
  0x23: 1, // ESC # n
  0x24: 2, // ESC $ n1 n2  absolute horizontal position
  0x25: 1, // ESC % n
  0x26: 0, // ESC &      (download chars — variable; bail to 0 and resync)
  0x2B: 1, // ESC + n
  0x2D: 1, // ESC - n   underline
  0x2F: 1, // ESC / n
  0x33: 1, // ESC 3 n   line spacing n/180
  0x41: 1, // ESC A n   line spacing n/60
  0x43: 1, // ESC C n   page length (n=0 ⇒ ESC C NUL n, handled below)
  0x49: 1, // ESC I n
  0x4A: 1, // ESC J n   advance paper n/180
  0x4E: 1, // ESC N n   skip-over-perforation
  0x51: 1, // ESC Q n   right margin
  0x52: 1, // ESC R n   international char set
  0x53: 1, // ESC S n   superscript/subscript
  0x55: 1, // ESC U n   unidirectional
  0x57: 1, // ESC W n   double width
  0x5C: 2, // ESC \ n1 n2  relative horizontal position
  0x61: 1, // ESC a n   justification
  0x63: 2, // ESC c n1 n2
  0x65: 2, // ESC e n m
  0x66: 2, // ESC f n m
  0x67: 1, // ESC g n
  0x69: 1, // ESC i n
  0x6A: 1, // ESC j n
  0x6B: 1, // ESC k n   typeface
  0x6C: 1, // ESC l n   left margin
  0x70: 1, // ESC p n   proportional
  0x71: 1, // ESC q n
  0x72: 1, // ESC r n   colour
  0x73: 1, // ESC s n
  0x74: 1, // ESC t n   char table
  0x77: 1, // ESC w n   double height
  0x78: 1, // ESC x n   draft/NLQ
};

// Skip an ESC sequence starting at the ESC byte; returns the index of
// the first byte after the sequence.
function skipEscape(bytes: Uint8Array, i: number): number {
  i++;                                   // past ESC
  if (i >= bytes.length) return i;
  const cmd = bytes[i++];
  switch (cmd) {
    case 0x2A: {                         // ESC * m n1 n2 + n1+256·n2 data bytes
      if (i + 3 > bytes.length) return bytes.length;
      const n = bytes[i + 1] + 256 * bytes[i + 2];
      return Math.min(bytes.length, i + 3 + n);
    }
    case 0x4B: case 0x4C: case 0x59: case 0x5A: { // ESC K/L/Y/Z n1 n2 + data
      if (i + 2 > bytes.length) return bytes.length;
      const n = bytes[i] + 256 * bytes[i + 1];
      return Math.min(bytes.length, i + 2 + n);
    }
    case 0x28: {                         // ESC ( x n1 n2 + data (ESC P 2 extended)
      if (i + 3 > bytes.length) return bytes.length;
      const n = bytes[i + 1] + 256 * bytes[i + 2];
      return Math.min(bytes.length, i + 3 + n);
    }
    case 0x42: case 0x44: {              // ESC B / ESC D — NUL-terminated tab stops
      while (i < bytes.length && bytes[i] !== 0x00) i++;
      return Math.min(bytes.length, i + 1);
    }
    case 0x43:                           // ESC C n, or ESC C NUL n (inches)
      if (i < bytes.length && bytes[i] === 0x00) return Math.min(bytes.length, i + 2);
      return Math.min(bytes.length, i + 1);
    default:
      return Math.min(bytes.length, i + (ESCP_FIXED_ARGS[cmd] ?? 0));
  }
}

export function decodePrintJob(bytes: Uint8Array): DecodedPrintJob {
  const pages: string[] = [];
  let line = '';
  let lines: string[] = [];
  let pendingCR = false;

  const endLine = () => { lines.push(line); line = ''; };
  const endPage = () => {
    if (line.length > 0) endLine();
    // Trim trailing blank lines the driver pads the page with.
    while (lines.length > 0 && lines[lines.length - 1] === '') lines.pop();
    pages.push(lines.join('\n'));
    lines = [];
  };

  for (let i = 0; i < bytes.length; ) {
    const b = bytes[i];
    // CR alone is a newline; CR+LF is one newline, not two.
    if (pendingCR && b !== 0x0A) endLine();
    pendingCR = false;

    if (b === 0x1B) { i = skipEscape(bytes, i); continue; }
    i++;
    switch (b) {
      case 0x0D: pendingCR = true; break;
      case 0x0A: endLine(); break;
      case 0x0C: endPage(); break;
      case 0x09: line += '\t'; break;
      case 0x08: line = line.slice(0, -1); break;
      default:
        if (b < 0x20 || b === 0x7F) break;           // other controls: drop
        line += CP1252_C1[b] ?? String.fromCharCode(b);
    }
  }
  if (pendingCR) endLine();
  if (line.length > 0 || lines.length > 0) endPage();

  return { pages, text: pages.join('\n\f\n') };
}
