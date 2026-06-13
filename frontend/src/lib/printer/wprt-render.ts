// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// WPRT print-job renderer: parses the per-page primitive streams the
// device's "Printer via PC" driver produces (see the PLP spec, "Print
// Data Format") and renders them to a positioned PDF, plus a plain-text
// extraction for the live preview / .txt download.
//
// All measurements on the wire are twips (1/1440 inch), positions
// relative to the page's top-left corner; PDF wants points (1/72 inch)
// from the bottom-left, so everything maps through twips/20 and a
// vertical flip. Text is CP1252 like the rest of the EPOC world.
//
// Fidelity notes (deliberate v1 simplifications, all per-primitive
// local so they're easy to upgrade):
//   - Drawing modes other than the default 0x20 are ignored (the spec
//     itself says 0x20 is "almost always the mode used").
//   - Clipping rectangles are ignored.
//   - Hatched/patterned brush styles fill solid in the brush colour.
//   - Bitmaps are rendered greyscale; the colour display modes carry
//     no palette on the wire anyway.

import { CP1252_C1 } from './decode.ts';

// ── Primitive model ─────────────────────────────────────────────────

export interface WprtFont {
  face: string;
  screenFont: number;       // 0 Swiss, 1 Arial, 2 Courier New, 3 Times
  baseSizeTwips: number;
  style: number;            // 1 italic, 2 bold, 4 superscript, 8 subscript
  actualSizeTwips: number;
  baselineOffsetTwips: number;
}

export type WprtPrimitive =
  | { op: 'start'; section: number; pageNo: number }
  | { op: 'end' }
  | { op: 'drawMode'; mode: number }
  | { op: 'clip'; l: number; t: number; r: number; b: number }
  | { op: 'unclip' }
  | { op: 'font'; font: WprtFont }
  | { op: 'discardFont' }
  | { op: 'underline'; on: boolean }
  | { op: 'strike'; on: boolean }
  | { op: 'penColour'; r: number; g: number; b: number }
  | { op: 'penStyle'; style: number }
  | { op: 'penSize'; w: number; h: number }
  | { op: 'brushColour'; r: number; g: number; b: number }
  | { op: 'brushStyle'; style: number }
  | { op: 'line'; x1: number; y1: number; x2: number; y2: number }
  | { op: 'ellipse'; l: number; t: number; r: number; b: number }
  | { op: 'rect'; l: number; t: number; r: number; b: number }
  | { op: 'polygon'; points: { x: number; y: number }[]; fillRule: number }
  | { op: 'bitmap'; l: number; t: number; r: number; b: number;
      widthPx: number; heightPx: number; gray: Uint8Array }
  | { op: 'text'; text: string; x: number; baseline: number }
  | { op: 'textJustified'; text: string; l: number; t: number; r: number; b: number;
      baseline: number; align: number; margin: number };

export interface ParsedPage {
  primitives: WprtPrimitive[];
  errors: string[];          // parse problems (page still usable up to the error)
}

// ── Byte readers ────────────────────────────────────────────────────

class Reader {
  readonly bytes: Uint8Array;
  pos = 0;
  constructor(bytes: Uint8Array) { this.bytes = bytes; }
  get remaining(): number { return this.bytes.length - this.pos; }
  private need(n: number): void {
    if (this.pos + n > this.bytes.length) {
      throw new Error(`truncated page at offset ${this.pos} (need ${n} bytes)`);
    }
  }
  u8(): number { this.need(1); return this.bytes[this.pos++]; }
  u16(): number { this.need(2); const v = this.bytes[this.pos] | (this.bytes[this.pos+1] << 8); this.pos += 2; return v; }
  u32(): number {
    this.need(4);
    const b = this.bytes, p = this.pos; this.pos += 4;
    return ((b[p] | (b[p+1] << 8) | (b[p+2] << 16) | (b[p+3] << 24)) >>> 0);
  }
  i32(): number { return this.u32() | 0; }
  take(n: number): Uint8Array { this.need(n); const v = this.bytes.subarray(this.pos, this.pos + n); this.pos += n; return v; }
  // EPOC compact length: 1 byte when (b & 3) == 2 (value = n<<2 | 2),
  // 2 bytes LE when (b & 7) == 5 (value = n<<3 | 5).
  compactLen(): number {
    const b0 = this.u8();
    if ((b0 & 0x03) === 0x02) return b0 >> 2;
    if ((b0 & 0x07) === 0x05) return ((b0 | (this.u8() << 8)) >> 3);
    throw new Error(`bad compact length 0x${b0.toString(16)}`);
  }
}

function cp1252(bytes: Uint8Array): string {
  let s = '';
  for (const b of bytes) s += CP1252_C1[b] ?? String.fromCharCode(b);
  return s;
}

// ── Bitmap decoding ─────────────────────────────────────────────────

function rleDecode(src: Uint8Array): Uint8Array {
  const out: number[] = [];
  for (let i = 0; i < src.length; ) {
    const marker = src[i++];
    if (marker <= 0x7F) {
      const value = src[i++];
      for (let n = 0; n <= marker; n++) out.push(value);
    } else {
      const n = 0x100 - marker;
      for (let k = 0; k < n && i < src.length; k++) out.push(src[i++]);
    }
  }
  return new Uint8Array(out);
}

// Expand packed pixel rows (LSB = left-most pixel, rows padded to a
// 4-byte multiple) into one grey byte per pixel.
function toGray(pixels: Uint8Array, w: number, h: number, mode: number): Uint8Array {
  const out = new Uint8Array(w * h);
  const bpp = mode === 1 ? 1 : mode === 2 ? 2 : (mode === 3 || mode === 5) ? 4
            : (mode === 4 || mode === 6) ? 8 : mode === 7 ? 16 : mode === 8 ? 32 : 8;
  const rowBytes = (Math.ceil((w * bpp) / 8) + 3) & ~3;
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const bit = x * bpp;
      const idx = y * rowBytes + (bit >> 3);
      if (idx >= pixels.length) { out[y * w + x] = 0xFF; continue; }
      let v: number;
      switch (bpp) {
        case 1:  v = ((pixels[idx] >> (bit & 7)) & 1) ? 0xFF : 0x00; break;
        case 2:  v = ((pixels[idx] >> (bit & 7)) & 3) * 0x55; break;
        case 4:  v = ((pixels[idx] >> (bit & 4)) & 0xF) * 17; break;
        case 16: {
          const px = pixels[idx] | (pixels[idx + 1] << 8);   // RGB565
          const r = ((px >> 11) & 0x1F) << 3, g = ((px >> 5) & 0x3F) << 2, b = (px & 0x1F) << 3;
          v = Math.round((r + g + b) / 3); break;
        }
        case 32: v = Math.round((pixels[idx] + pixels[idx + 1] + pixels[idx + 2]) / 3); break;
        default: v = pixels[idx]; break;                      // 8bpp grey/colour
      }
      out[y * w + x] = v;
    }
  }
  return out;
}

// ── Page parser ─────────────────────────────────────────────────────

// Every real page begins with this 8-byte marker (the job's fake first
// page is handled by the caller via isJobSentinelPage).
const PAGE_MARKER = [0xe8, 0x03, 0x00, 0x00, 0xe8, 0x03, 0x00, 0x00];

export function parsePage(data: Uint8Array): ParsedPage {
  const prims: WprtPrimitive[] = [];
  const errors: string[] = [];
  const r = new Reader(data);

  let marked = data.length >= 8;
  for (let i = 0; marked && i < 8; i++) if (data[i] !== PAGE_MARKER[i]) marked = false;
  if (marked) r.pos = 8;

  try {
    while (r.remaining > 0) {
      const op = r.u8();
      switch (op) {
        case 0x00: {  // WPRT_START
          const section = r.u32();
          prims.push({ op: 'start', section: section & 3, pageNo: section >>> 2 });
          break;
        }
        case 0x01: prims.push({ op: 'end' }); break;
        case 0x03: prims.push({ op: 'drawMode', mode: r.u8() }); break;
        case 0x04: prims.push({ op: 'clip', l: r.i32(), t: r.i32(), r: r.i32(), b: r.i32() }); break;
        case 0x05: prims.push({ op: 'unclip' }); break;
        case 0x06: r.u8(); r.u32(); break;          // WPRT_PRIMITIVE_06 (unknown boolean)
        case 0x07: {  // WPRT_USE_FONT
          const len = r.compactLen();
          const face = cp1252(r.take(len));
          prims.push({ op: 'font', font: {
            face,
            screenFont: r.u8(),
            baseSizeTwips: r.u16(),
            style: r.u32(),
            actualSizeTwips: r.u32(),
            baselineOffsetTwips: r.i32(),
          }});
          break;
        }
        case 0x08: prims.push({ op: 'discardFont' }); break;
        case 0x09: prims.push({ op: 'underline', on: r.u8() !== 0 }); break;
        case 0x0a: prims.push({ op: 'strike', on: r.u8() !== 0 }); break;
        case 0x0b: r.u32(); r.u32(); break;          // WPRT_LINE_FEED (no rendering effect)
        case 0x0c: r.take(8); break;                 // WPRT_CARRIAGE_RETURN (no rendering effect)
        case 0x0d: prims.push({ op: 'penColour', r: r.u8(), g: r.u8(), b: r.u8() }); break;
        case 0x0e: prims.push({ op: 'penStyle', style: r.u8() }); break;
        case 0x0f: prims.push({ op: 'penSize', w: r.u32(), h: r.u32() }); break;
        case 0x10: prims.push({ op: 'brushColour', r: r.u8(), g: r.u8(), b: r.u8() }); break;
        case 0x11: prims.push({ op: 'brushStyle', style: r.u8() }); break;
        case 0x17: r.u32(); r.u32(); break;          // WPRT_PRIMITIVE_17 (unknown)
        case 0x19: prims.push({ op: 'line', x1: r.i32(), y1: r.i32(), x2: r.i32(), y2: r.i32() }); break;
        case 0x1b: r.u32(); r.u32(); break;          // WPRT_PRIMITIVE_1B (unknown)
        case 0x1f: prims.push({ op: 'ellipse', l: r.i32(), t: r.i32(), r: r.i32(), b: r.i32() }); break;
        case 0x20: prims.push({ op: 'rect', l: r.i32(), t: r.i32(), r: r.i32(), b: r.i32() }); break;
        case 0x23: {  // WPRT_DRAW_POLYGON
          const count = r.u32();
          const points: { x: number; y: number }[] = [];
          for (let i = 0; i < count; i++) points.push({ x: r.i32(), y: r.i32() });
          prims.push({ op: 'polygon', points, fillRule: r.u8() });
          break;
        }
        case 0x25: case 0x26: {  // WPRT_DRAW_BITMAP_RECT / _SRC
          const l = r.i32(), t = r.i32(), rr = r.i32(), b = r.i32();
          const dataLength = r.u32();
          const dataOffset = r.u32();
          const widthPx = r.u32(), heightPx = r.u32();
          r.u32(); r.u32();                          // raw width / raw height
          const mode = r.u32();
          r.take(8);                                 // two reserved zero words
          const encoding = r.u32();
          let pixels = r.take(Math.max(0, dataLength - dataOffset));
          if (op === 0x26) r.take(16);               // source rectangle (ignored: we draw the whole bitmap)
          if (encoding === 1) pixels = rleDecode(pixels);
          prims.push({ op: 'bitmap', l, t, r: rr, b,
                       widthPx, heightPx, gray: toGray(pixels, widthPx, heightPx, mode) });
          break;
        }
        case 0x27: {  // WPRT_DRAW_TEXT
          const len = r.compactLen();
          const text = cp1252(r.take(len));
          prims.push({ op: 'text', text, x: r.i32(), baseline: r.i32() });
          break;
        }
        case 0x28: {  // WPRT_DRAW_TEXT_JUSTIFIED
          const len = r.compactLen();
          const text = cp1252(r.take(len));
          prims.push({ op: 'textJustified', text,
                       l: r.i32(), t: r.i32(), r: r.i32(), b: r.i32(),
                       baseline: r.i32(), align: r.u8(), margin: r.u32() });
          break;
        }
        default:
          // Unknown opcode: primitives have no length prefix, so we
          // can't skip it — stop parsing and keep what we have.
          errors.push(`unknown primitive 0x${op.toString(16)} at offset ${r.pos - 1}`);
          return { primitives: prims, errors };
      }
    }
  } catch (e) {
    errors.push(e instanceof Error ? e.message : String(e));
  }
  return { primitives: prims, errors };
}

// ── Text extraction (preview / .txt) ────────────────────────────────
// Collect the text primitives in baseline order; segments on the same
// baseline are joined left-to-right with a space when there's a
// visible gap. Good enough for a faithful reading copy — the PDF is
// the positioned rendering.

export function extractPageText(page: ParsedPage): string {
  interface Seg { x: number; baseline: number; text: string; estWidth: number }
  const segs: Seg[] = [];
  let font: WprtFont | null = null;
  for (const p of page.primitives) {
    if (p.op === 'font') { font = p.font; continue; }
    if (p.op !== 'text' && p.op !== 'textJustified') continue;
    const size = font?.actualSizeTwips || 200;
    const est = p.text.length * size * 0.55;
    if (p.op === 'text') {
      segs.push({ x: p.x, baseline: p.baseline, text: p.text, estWidth: est });
    } else {
      segs.push({ x: p.l + p.margin, baseline: p.t, text: p.text, estWidth: est });
    }
  }
  // Group into lines: a new baseline more than ~half a line below the
  // previous starts a new line (twips; 100 ≈ 5 pt).
  segs.sort((a, b) => (a.baseline - b.baseline) || (a.x - b.x));
  const lines: string[] = [];
  let curLine = '';
  let curBaseline = Number.NEGATIVE_INFINITY;
  let curEnd = 0;
  for (const s of segs) {
    if (s.baseline - curBaseline > 100) {
      if (curLine !== '') lines.push(curLine);
      curLine = s.text;
    } else {
      curLine += (s.x - curEnd > 60 ? ' ' : '') + s.text;
    }
    curBaseline = s.baseline;
    curEnd = s.x + s.estWidth;
  }
  if (curLine !== '') lines.push(curLine);
  return lines.join('\n');
}

// ── PDF rendering ───────────────────────────────────────────────────

const PAGE_W = 595;   // A4 in points
const PAGE_H = 842;
const T = 20;         // twips per point

// Map (screen font / face, style) onto the PDF standard-14 set.
function pdfFontName(font: WprtFont | null): string {
  const face = (font?.face ?? '').toLowerCase();
  const sf = font?.screenFont ?? 1;
  const bold = ((font?.style ?? 0) & 0x02) !== 0;
  const italic = ((font?.style ?? 0) & 0x01) !== 0;
  if (sf === 2 || face.includes('courier')) {
    return bold && italic ? 'Courier-BoldOblique' : bold ? 'Courier-Bold'
         : italic ? 'Courier-Oblique' : 'Courier';
  }
  if (sf === 3 || face.includes('times')) {
    return bold && italic ? 'Times-BoldItalic' : bold ? 'Times-Bold'
         : italic ? 'Times-Italic' : 'Times-Roman';
  }
  return bold && italic ? 'Helvetica-BoldOblique' : bold ? 'Helvetica-Bold'
       : italic ? 'Helvetica-Oblique' : 'Helvetica';
}

// Rough per-character width as a fraction of the font size, for
// centre/right justification (exact for Courier, average elsewhere).
function avgWidthFactor(fontName: string): number {
  if (fontName.startsWith('Courier')) return 0.6;
  if (fontName.startsWith('Times')) return 0.5;
  return 0.52;
}

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

function esc(s: string): string {
  return s.replace(/\\/g, '\\\\').replace(/\(/g, '\\(').replace(/\)/g, '\\)');
}

function fmt(n: number): string {
  return Number.isInteger(n) ? String(n) : n.toFixed(2);
}

interface PdfImage { name: string; w: number; h: number; gray: Uint8Array }

// Renders parsed pages to PDF content streams + image XObjects, then
// serializes with the same byte-accurate xref scheme as pdf.ts.
export function renderJobToPdf(pages: ParsedPage[]): Uint8Array {
  const fontNames = new Map<string, string>();   // PDF base font → /F<n>
  const fontRef = (base: string): string => {
    let ref = fontNames.get(base);
    if (!ref) { ref = `F${fontNames.size + 1}`; fontNames.set(base, ref); }
    return ref;
  };

  const images: PdfImage[] = [];
  const contents: string[] = [];

  for (const page of pages) {
    const ops: string[] = [];
    let font: WprtFont | null = null;
    let underline = false, strike = false;
    let pen = { r: 0, g: 0, b: 0 };
    let penStyle = 1;
    let penWidthPt = 0.5;
    let brush = { r: 255, g: 255, b: 255 };
    let brushStyle = 0;

    const px = (twips: number) => twips / T;
    const py = (twips: number) => PAGE_H - twips / T;
    const strokeColour = () => `${fmt(pen.r / 255)} ${fmt(pen.g / 255)} ${fmt(pen.b / 255)} RG`;
    const fillPen      = () => `${fmt(pen.r / 255)} ${fmt(pen.g / 255)} ${fmt(pen.b / 255)} rg`;
    const fillBrush    = () => `${fmt(brush.r / 255)} ${fmt(brush.g / 255)} ${fmt(brush.b / 255)} rg`;

    // Shared shape finisher: brush style 0 = outline only; anything
    // else fills (hatch patterns approximated as solid).
    const paintOp = () => {
      if (penStyle === 0 && brushStyle === 0) return 'n';
      if (brushStyle === 0) return 'S';
      return penStyle === 0 ? 'f' : 'B';
    };

    const drawText = (text: string, xPt: number, yPt: number) => {
      if (text.length === 0) return;
      const base = pdfFontName(font);
      const sizePt = (font?.actualSizeTwips || 200) / T;
      ops.push('BT', `/${fontRef(base)} ${fmt(sizePt)} Tf`, fillPen(),
               `${fmt(xPt)} ${fmt(yPt)} Td`, `(${esc(text)}) Tj`, 'ET');
      const estW = text.length * sizePt * avgWidthFactor(base);
      if (underline) {
        ops.push(strokeColour(), `${fmt(Math.max(0.4, sizePt * 0.06))} w`,
                 `${fmt(xPt)} ${fmt(yPt - sizePt * 0.15)} m ${fmt(xPt + estW)} ${fmt(yPt - sizePt * 0.15)} l S`);
      }
      if (strike) {
        ops.push(strokeColour(), `${fmt(Math.max(0.4, sizePt * 0.06))} w`,
                 `${fmt(xPt)} ${fmt(yPt + sizePt * 0.27)} m ${fmt(xPt + estW)} ${fmt(yPt + sizePt * 0.27)} l S`);
      }
    };

    for (const p of page.primitives) {
      switch (p.op) {
        case 'font': font = p.font; break;
        case 'discardFont': font = null; break;
        case 'underline': underline = p.on; break;
        case 'strike': strike = p.on; break;
        case 'penColour': pen = { r: p.r, g: p.g, b: p.b }; break;
        case 'penStyle': penStyle = p.style; break;
        case 'penSize': penWidthPt = Math.max(0.5, p.w / T); break;
        case 'brushColour': brush = { r: p.r, g: p.g, b: p.b }; break;
        case 'brushStyle': brushStyle = p.style; break;
        case 'start':
          // Section start resets the graphics context (spec).
          font = null; underline = false; strike = false;
          pen = { r: 0, g: 0, b: 0 }; penStyle = 1; penWidthPt = 0.5;
          brush = { r: 255, g: 255, b: 255 }; brushStyle = 0;
          break;
        case 'line':
          if (penStyle === 0) break;
          ops.push(strokeColour(), `${fmt(penWidthPt)} w`,
                   `${fmt(px(p.x1))} ${fmt(py(p.y1))} m ${fmt(px(p.x2))} ${fmt(py(p.y2))} l S`);
          break;
        case 'rect':
          ops.push(fillBrush(), strokeColour(), `${fmt(penWidthPt)} w`,
                   `${fmt(px(p.l))} ${fmt(py(p.b))} ${fmt(px(p.r) - px(p.l))} ${fmt(py(p.t) - py(p.b))} re ${paintOp()}`);
          break;
        case 'ellipse': {
          const cx = (px(p.l) + px(p.r)) / 2, cy = (py(p.t) + py(p.b)) / 2;
          const rx = (px(p.r) - px(p.l)) / 2, ry = (py(p.t) - py(p.b)) / 2;
          const k = 0.5523;
          ops.push(fillBrush(), strokeColour(), `${fmt(penWidthPt)} w`,
            `${fmt(cx + rx)} ${fmt(cy)} m`,
            `${fmt(cx + rx)} ${fmt(cy + ry * k)} ${fmt(cx + rx * k)} ${fmt(cy + ry)} ${fmt(cx)} ${fmt(cy + ry)} c`,
            `${fmt(cx - rx * k)} ${fmt(cy + ry)} ${fmt(cx - rx)} ${fmt(cy + ry * k)} ${fmt(cx - rx)} ${fmt(cy)} c`,
            `${fmt(cx - rx)} ${fmt(cy - ry * k)} ${fmt(cx - rx * k)} ${fmt(cy - ry)} ${fmt(cx)} ${fmt(cy - ry)} c`,
            `${fmt(cx + rx * k)} ${fmt(cy - ry)} ${fmt(cx + rx)} ${fmt(cy - ry * k)} ${fmt(cx + rx)} ${fmt(cy)} c`,
            paintOp());
          break;
        }
        case 'polygon': {
          if (p.points.length === 0) break;
          const parts = [fillBrush(), strokeColour(), `${fmt(penWidthPt)} w`,
                         `${fmt(px(p.points[0].x))} ${fmt(py(p.points[0].y))} m`];
          for (let i = 1; i < p.points.length; i++) {
            parts.push(`${fmt(px(p.points[i].x))} ${fmt(py(p.points[i].y))} l`);
          }
          parts.push('h');
          // Fill rule 0 = even-odd, 1 = non-zero winding.
          const fillOp = paintOp();
          parts.push(p.fillRule === 0 ? fillOp.replace('f', 'f*').replace('B', 'B*') : fillOp);
          ops.push(...parts);
          break;
        }
        case 'bitmap': {
          if (p.widthPx <= 0 || p.heightPx <= 0) break;
          const name = `Im${images.length + 1}`;
          images.push({ name, w: p.widthPx, h: p.heightPx, gray: p.gray });
          const w = px(p.r) - px(p.l), h = py(p.t) - py(p.b);
          ops.push('q', `${fmt(w)} 0 0 ${fmt(h)} ${fmt(px(p.l))} ${fmt(py(p.b))} cm`, `/${name} Do`, 'Q');
          break;
        }
        case 'text':
          drawText(p.text, px(p.x), py(p.baseline));
          break;
        case 'textJustified': {
          // Background fill when a brush is selected (spec: white if no
          // brush colour set; null brush = alignment only, no fill).
          if (brushStyle !== 0) {
            ops.push(fillBrush(),
                     `${fmt(px(p.l))} ${fmt(py(p.b))} ${fmt(px(p.r) - px(p.l))} ${fmt(py(p.t) - py(p.b))} re f`);
          }
          const base = pdfFontName(font);
          const sizePt = (font?.actualSizeTwips || 200) / T;
          const estW = p.text.length * sizePt * avgWidthFactor(base);
          let xPt: number;
          if (p.align === 1) {        // centre
            xPt = (px(p.l) + px(p.r)) / 2 - estW / 2;
          } else if (p.align === 2) { // right
            xPt = px(p.r) - px(p.margin) - estW;
          } else {                    // left
            xPt = px(p.l) + px(p.margin);
          }
          // Spec hint: baseline at a quarter of the way up the box,
          // offset by the font's baseline field.
          const yPt = py(p.b) + (py(p.t) - py(p.b)) * 0.25
                    - (font?.baselineOffsetTwips ?? 0) / T;
          drawText(p.text, xPt, yPt);
          break;
        }
        default: break;   // start/end/clip/drawMode handled or ignored above
      }
    }
    contents.push(ops.join('\n'));
  }

  return serializePdf(contents, fontNames, images);
}

// ── PDF serialization ──────────────────────────────────────────────
// Same object/xref scheme as pdf.ts, with a shared Resources dict
// (fonts + image XObjects) referenced by every page.

function serializePdf(
  contents: string[],
  fontNames: Map<string, string>,
  images: PdfImage[],
): Uint8Array {
  if (contents.length === 0) contents = [''];
  // Object layout: 1 catalog, 2 pages root, 3 resources,
  // then fonts, then images, then per page: page obj + content stream.
  const fontEntries = [...fontNames.entries()];   // [base, /F ref]
  const firstFontObj = 4;
  const firstImageObj = firstFontObj + fontEntries.length;
  const firstPageObj = firstImageObj + images.length;
  const pageObjNum = (i: number) => firstPageObj + 2 * i;

  const objects: { num: number; body: Uint8Array }[] = [];
  const textObj = (num: number, body: string) => objects.push({ num, body: latin1(body) });
  const streamObj = (num: number, dict: string, stream: Uint8Array) => {
    const head = latin1(`<< ${dict} /Length ${stream.length} >>\nstream\n`);
    const tail = latin1(`\nendstream`);
    const body = new Uint8Array(head.length + stream.length + tail.length);
    body.set(head, 0); body.set(stream, head.length); body.set(tail, head.length + stream.length);
    objects.push({ num, body });
  };

  const kids = contents.map((_, i) => `${pageObjNum(i)} 0 R`).join(' ');
  textObj(1, `<< /Type /Catalog /Pages 2 0 R >>`);
  textObj(2, `<< /Type /Pages /Kids [${kids}] /Count ${contents.length} >>`);

  const fontDict = fontEntries
    .map(([, ref], i) => `/${ref} ${firstFontObj + i} 0 R`).join(' ');
  const imageDict = images
    .map((im, i) => `/${im.name} ${firstImageObj + i} 0 R`).join(' ');
  textObj(3, `<< /Font << ${fontDict} >> /XObject << ${imageDict} >> >>`);

  fontEntries.forEach(([base], i) => {
    textObj(firstFontObj + i,
      `<< /Type /Font /Subtype /Type1 /BaseFont /${base} /Encoding /WinAnsiEncoding >>`);
  });
  images.forEach((im, i) => {
    streamObj(firstImageObj + i,
      `/Type /XObject /Subtype /Image /Width ${im.w} /Height ${im.h} ` +
      `/ColorSpace /DeviceGray /BitsPerComponent 8`,
      im.gray);
  });
  contents.forEach((content, i) => {
    textObj(pageObjNum(i),
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ${PAGE_W} ${PAGE_H}] ` +
      `/Resources 3 0 R /Contents ${pageObjNum(i) + 1} 0 R >>`);
    streamObj(pageObjNum(i) + 1, '', latin1(content));
  });

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
