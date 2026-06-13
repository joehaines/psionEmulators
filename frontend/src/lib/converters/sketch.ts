// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Sketch / MBM (.mbm) -> PNG.
//
// Decoder modelled directly on Symbian's `bmconv` (Symbian SDK source,
// reference/psion_cpp_sdk_linux-master/install/sdk2unix-1.9.tar.gz,
// helpers/bmconv-1.1.0-2/src/pbmtobm.cpp).  In particular:
//   * the byte/12-bit/16-bit RLE expansion algorithms — needed because
//     Sketch always saves compressed (the v1 implementation that only
//     handled uncompressed bitmaps returned null for every real Sketch
//     file);
//   * BitmapUtils::ByteWidth() for the unpacked scan-line stride;
//   * the LSB-first pixel packing convention used by GetPixel().
//
// File layout:
//   [16-byte UID header]
//   [4-byte trailer offset]                <- points to bitmap-table
//   [bitmap1 header][bitmap1 (compressed) data]
//   [bitmap2 header][bitmap2 data]
//   ...
//   [u32 count][u32 hdr1Off][u32 hdr2Off]...   <- the trailer
//
// Bitmap header (40 bytes, all u32 LE):
//   bitmapSize, structSize, widthPx, heightPx, widthTwips, heightTwips,
//   bitsPerPixel, isColor, paletteEntries, compression
//
// Compression codes:  0 = none, 1 = byte RLE, 2 = 12-bit RLE,
//                     3 = 16-bit RLE, 4 = 24-bit RLE.
// Sketch in the wild: 2 bpp / 4 bpp grayscale + byte RLE.  We support
// every grayscale bit-depth bmconv emits (1/2/4/8) and the common color
// modes (4/8 paletted, 12 RGB444, 16 RGB565, 24 RGB888) so that Sketch
// files made on a 5mx or Revo also convert cleanly.

import { readDirectStoreTrailerOffset } from './epoc-store.ts';
import { readUint32LE } from './epoc-uids.ts';
import { encodePng } from './png-encoder.ts';

interface BitmapHeader {
  bitmapSize: number;
  structSize: number;
  widthPixels: number;
  heightPixels: number;
  widthTwips: number;
  heightTwips: number;
  bitsPerPixel: number;
  isColor: number;
  paletteEntries: number;
  compression: number;
  pixelDataOffset: number;
}

const COMP_NONE        = 0;
const COMP_BYTE_RLE    = 1;
const COMP_12BIT_RLE   = 2;
const COMP_16BIT_RLE   = 3;
const COMP_24BIT_RLE   = 4;

export function convertSketchToPng(input: Uint8Array): Uint8Array | null {
  // Two file layouts in the wild:
  //
  //  (a) Raw multi-bitmap container (UID3 = 0x10000040): the trailer at
  //      bytes[16..20] points to a (count, hdr1Off, hdr2Off, ...) table.
  //      System icons, bmconv output.
  //
  //  (b) Sketch picture file (UID3 = 0x1000007D): an outer envelope
  //      with the bitmap header embedded inside.  The "trailer offset"
  //      at bytes[16..20] points to a Sketch-specific TOC at the end
  //      that talks about Paint.app, NOT to a bitmap-header table.
  //
  // Strategy: try (a) first; if it doesn't land on a sane bitmap header,
  // fall back to scanning the file body for one.  The scan is cheap and
  // unambiguous because a real bitmap header has structSize == 40, a
  // bpp in {1,2,4,8,12,16,24,32}, a known compression code, and
  // plausible dimensions — random bytes essentially never satisfy all
  // four at once.
  const hdr = readMbmTrailerHeader(input) ?? scanForBitmapHeader(input);
  if (!hdr) return null;

  return renderHeaderToPng(input, hdr);
}

// Try the standard MBM trailer layout: u32 trailerOffset at byte 16,
// then at that offset a (count, hdr1Off, ...) table.  Returns null if
// any field is implausible or the table doesn't point at a valid
// bitmap header — the caller then falls back to scanning.
function readMbmTrailerHeader(input: Uint8Array): BitmapHeader | null {
  const trailerOffset = readDirectStoreTrailerOffset(input);
  if (trailerOffset === null) return null;
  if (trailerOffset + 8 > input.length) return null;
  const count = readUint32LE(input, trailerOffset);
  if (count < 1 || count > 256) return null;
  const firstHdrOffset = readUint32LE(input, trailerOffset + 4);
  if (firstHdrOffset < 20 || firstHdrOffset >= trailerOffset) return null;
  return readBitmapHeader(input, firstHdrOffset);
}

// Last-resort: scan the file body for the first byte position where a
// plausible bitmap header sits.  Used for Sketch envelope files (UID3
// 0x1000007D) where the bitmap header is embedded inside an outer
// container and the "trailer" pointer goes to a Sketch TOC instead of a
// bitmap-header table.
//
// The header can land at ANY byte alignment: the Sketch envelope on a
// 5mx puts it at offset 38, while Word documents embed Sketch payloads
// behind variable-length text and routinely leave the header at an odd
// offset (every ROM "Welcome to" file does).  So we stride by 1.  The
// strict bitmap-header validator (`readBitmapHeader`) rejects every
// non-header position by requiring structSize, bpp, compression code
// and dimensions to all be sane simultaneously, so false positives
// essentially can't happen.
function scanForBitmapHeader(input: Uint8Array): BitmapHeader | null {
  const maxStart = Math.min(input.length - 40, 32768);
  for (let i = 16; i <= maxStart; i++) {
    const hdr = readBitmapHeader(input, i);
    if (hdr) return hdr;
  }
  return null;
}

// Render a single already-located bitmap header to PNG.  Used by the
// Word converter to convert each embedded image it finds inside a
// document body.
export function renderBitmapAt(input: Uint8Array, offset: number): Uint8Array | null {
  const hdr = readBitmapHeader(input, offset);
  if (!hdr) return null;
  return renderHeaderToPng(input, hdr);
}

// Find every embedded bitmap in a document body (Word inserts MBM
// payloads inline; each one starts with an sbm header).  Returns an
// ordered list of offsets.  Skips past each bitmap's data before
// continuing so we don't see the same image twice.
//
// Strides by 1: embedded bitmap headers sit behind variable-length text
// and land at odd offsets all the time (see scanForBitmapHeader).  An
// optional `endOffset` restricts the scan to a sub-range — used by the
// Word converter to look inside a single embedded-object payload.
export function findEmbeddedBitmaps(
  input: Uint8Array, startOffset = 16, endOffset = input.length,
): number[] {
  const offsets: number[] = [];
  const maxStart = Math.min(endOffset, input.length) - 40;
  for (let i = startOffset; i <= maxStart; i++) {
    const hdr = readBitmapHeader(input, i);
    if (!hdr) continue;
    offsets.push(i);
    // Jump past this bitmap's data — bitmapSize covers header + (possibly
    // compressed) data, so the next bitmap can't start before then.
    i = i + hdr.bitmapSize - 1; // -1 because the loop adds +1
  }
  return offsets;
}

function renderHeaderToPng(input: Uint8Array, hdr: BitmapHeader): Uint8Array | null {
  const pixelDataLen = hdr.bitmapSize - hdr.structSize;
  if (pixelDataLen < 0) return null;
  if (hdr.pixelDataOffset + pixelDataLen > input.length) return null;
  const pixelData = input.subarray(
    hdr.pixelDataOffset, hdr.pixelDataOffset + pixelDataLen,
  );
  const byteWidth = computeByteWidth(hdr.widthPixels, hdr.bitsPerPixel);
  if (byteWidth === 0) return null;
  const unpackedSize = byteWidth * hdr.heightPixels;
  const unpacked = expandPixelData(pixelData, unpackedSize, hdr.compression);
  if (!unpacked) return null;
  const rgba = decodeBitmap(unpacked, hdr, byteWidth);
  if (!rgba) return null;
  return encodePng(rgba, hdr.widthPixels, hdr.heightPixels);
}

const VALID_COMPRESSIONS = new Set([
  COMP_NONE, COMP_BYTE_RLE, COMP_12BIT_RLE, COMP_16BIT_RLE, COMP_24BIT_RLE,
]);
const VALID_BPPS = new Set([1, 2, 4, 8, 12, 16, 24, 32]);

function readBitmapHeader(bytes: Uint8Array, offset: number): BitmapHeader | null {
  if (offset + 40 > bytes.length) return null;
  const structSize = readUint32LE(bytes, offset + 4);
  // Real-world MBMs have structSize == 40.  Accept anything in
  // [40, 256] in case Symbian extended the header in some build we
  // haven't seen — the pixel data still starts at offset + structSize.
  if (structSize < 40 || structSize > 256) return null;
  if (offset + structSize > bytes.length) return null;

  const bitmapSize     = readUint32LE(bytes, offset +  0);
  const widthPixels    = readUint32LE(bytes, offset +  8);
  const heightPixels   = readUint32LE(bytes, offset + 12);
  const bitsPerPixel   = readUint32LE(bytes, offset + 24);
  const compression    = readUint32LE(bytes, offset + 36);

  // Tight validators — every random offset that happens to have
  // structSize==40 in its second word would otherwise produce a header
  // here.  These four checks together make false positives ~impossible.
  if (!VALID_BPPS.has(bitsPerPixel))         return null;
  if (!VALID_COMPRESSIONS.has(compression))  return null;
  if (widthPixels  < 1 || widthPixels  > 8192) return null;
  if (heightPixels < 1 || heightPixels > 8192) return null;
  if (bitmapSize   < structSize || offset + bitmapSize > bytes.length) return null;

  return {
    bitmapSize,
    structSize,
    widthPixels,
    heightPixels,
    widthTwips:     readUint32LE(bytes, offset + 16),
    heightTwips:    readUint32LE(bytes, offset + 20),
    bitsPerPixel,
    isColor:        readUint32LE(bytes, offset + 28),
    paletteEntries: readUint32LE(bytes, offset + 32),
    compression,
    pixelDataOffset: offset + structSize,
  };
}

// BitmapUtils::ByteWidth from bmconv/utils.cpp — the number of bytes
// per scan-line in the *unpacked* bitmap, including padding to a 32-bit
// word boundary.
function computeByteWidth(widthPixels: number, bpp: number): number {
  let wordWidth = 0;
  switch (bpp) {
    case  1: wordWidth = Math.floor((widthPixels + 31) / 32); break;
    case  2: wordWidth = Math.floor((widthPixels + 15) / 16); break;
    case  4: wordWidth = Math.floor((widthPixels +  7) /  8); break;
    case  8: wordWidth = Math.floor((widthPixels +  3) /  4); break;
    case 12:
    case 16: wordWidth = Math.floor((widthPixels +  1) /  2); break;
    case 24: wordWidth = Math.floor(((widthPixels * 3) + 11) / 12) * 3; break;
    case 32: wordWidth = widthPixels; break;
    default: return 0;
  }
  return wordWidth * 4;
}

function expandPixelData(
  src: Uint8Array, expandedSize: number, compression: number,
): Uint8Array | null {
  if (compression === COMP_NONE) {
    // Uncompressed: data is already byte-for-byte the unpacked bitmap.
    // If the file is short we return null; if it's long (rare) we just
    // take the prefix we need.
    if (src.length < expandedSize) return null;
    return src.subarray(0, expandedSize);
  }
  const dst = new Uint8Array(expandedSize);
  switch (compression) {
    case COMP_BYTE_RLE:   return expandByteRle(src, dst)  ? dst : null;
    case COMP_12BIT_RLE:  return expand12BitRle(src, dst) ? dst : null;
    case COMP_16BIT_RLE:  return expand16BitRle(src, dst) ? dst : null;
    case COMP_24BIT_RLE:  return expand24BitRle(src, dst) ? dst : null;
    default: return null;
  }
}

// EpocLoader::ExpandByteRLEData — Symbian's byte RLE.
// Reads a *signed* control byte:
//   count < 0  : literal run of (-count) bytes
//   count >= 0 : (count+1) copies of the following byte
function expandByteRle(src: Uint8Array, dst: Uint8Array): boolean {
  let si = 0, di = 0;
  while (si < src.length && di < dst.length) {
    // JS-only: convert unsigned byte to signed.
    const count = (src[si++] << 24) >> 24;
    if (count < 0) {
      const runLen = -count;
      if (si + runLen > src.length || di + runLen > dst.length) return false;
      dst.set(src.subarray(si, si + runLen), di);
      si += runLen;
      di += runLen;
    } else {
      if (si >= src.length) return false;
      const value = src[si++];
      const runLen = count + 1;
      if (di + runLen > dst.length) return false;
      for (let i = 0; i < runLen; i++) dst[di++] = value;
    }
  }
  // bmconv treats "didn't consume all source / produce all destination"
  // as a decode error; we relax this on the source side (some files have
  // trailing padding) but still require the destination to be filled.
  return di === dst.length;
}

// EpocLoader::ExpandTwelveBitRLEData.
// Each compressed unit is a single 16-bit LE word:
//   top 4 bits  = run length - 1  (so 1..16 copies)
//   bottom 12   = pixel value
function expand12BitRle(src: Uint8Array, dst: Uint8Array): boolean {
  if ((src.length & 1) !== 0 || (dst.length & 1) !== 0) return false;
  const dv = new DataView(src.buffer, src.byteOffset, src.byteLength);
  const dst16 = new DataView(dst.buffer, dst.byteOffset, dst.byteLength);
  let si = 0, di = 0;
  while (si + 2 <= src.length && di + 2 <= dst.length) {
    const v = dv.getUint16(si, true); si += 2;
    const runLen = (v >>> 12) + 1;
    const pixel  = v & 0x0FFF;
    if (di + runLen * 2 > dst.length) return false;
    for (let i = 0; i < runLen; i++) {
      dst16.setUint16(di, pixel, true);
      di += 2;
    }
  }
  return di === dst.length;
}

// EpocLoader::ExpandSixteenBitRLEData.
// Signed byte control, then either:
//   count >= 0 : (count+1) copies of next 16-bit LE word
//   count <  0 : (-count) literal 16-bit words
function expand16BitRle(src: Uint8Array, dst: Uint8Array): boolean {
  if ((dst.length & 1) !== 0) return false;
  let si = 0, di = 0;
  while (si < src.length && di < dst.length) {
    const count = (src[si++] << 24) >> 24;
    if (count >= 0) {
      if (si + 2 > src.length) return false;
      const lo = src[si++], hi = src[si++];
      const runLen = count + 1;
      if (di + runLen * 2 > dst.length) return false;
      for (let i = 0; i < runLen; i++) { dst[di++] = lo; dst[di++] = hi; }
    } else {
      const runLen = -count;
      const byteLen = runLen * 2;
      if (si + byteLen > src.length || di + byteLen > dst.length) return false;
      dst.set(src.subarray(si, si + byteLen), di);
      si += byteLen;
      di += byteLen;
    }
  }
  return di === dst.length;
}

// EpocLoader::ExpandTwentyFourBitRLEData. Same pattern as 16-bit but
// with 3-byte (RGB) pixel units.
function expand24BitRle(src: Uint8Array, dst: Uint8Array): boolean {
  let si = 0, di = 0;
  while (si < src.length && di < dst.length) {
    const count = (src[si++] << 24) >> 24;
    if (count >= 0) {
      if (si + 3 > src.length) return false;
      const b = src[si++], g = src[si++], r = src[si++];
      const runLen = count + 1;
      if (di + runLen * 3 > dst.length) return false;
      for (let i = 0; i < runLen; i++) {
        dst[di++] = b; dst[di++] = g; dst[di++] = r;
      }
    } else {
      const runLen = -count;
      const byteLen = runLen * 3;
      if (si + byteLen > src.length || di + byteLen > dst.length) return false;
      dst.set(src.subarray(si, si + byteLen), di);
      si += byteLen;
      di += byteLen;
    }
  }
  return di === dst.length;
}

function decodeBitmap(
  unpacked: Uint8Array, hdr: BitmapHeader, byteWidth: number,
): Uint8Array | null {
  const w = hdr.widthPixels, h = hdr.heightPixels;
  const rgba = new Uint8Array(w * h * 4);

  // Modelled on EpocLoader::GetPixel from bmconv/pbmtobm.cpp.
  switch (hdr.bitsPerPixel) {
    case  1: decode1bpp(unpacked, hdr, byteWidth, rgba); break;
    case  2: decodeNbppGray(unpacked, hdr, byteWidth, rgba, 2); break;
    case  4: hdr.isColor ? decode4bppColor(unpacked, hdr, byteWidth, rgba)
                         : decodeNbppGray(unpacked, hdr, byteWidth, rgba, 4); break;
    case  8: hdr.isColor ? decode8bppColor(unpacked, hdr, byteWidth, rgba)
                         : decode8bppGray(unpacked, hdr, byteWidth, rgba);   break;
    case 12: decode12bppRgb444(unpacked, hdr, byteWidth, rgba); break;
    case 16: decode16bppRgb565(unpacked, hdr, byteWidth, rgba); break;
    case 24: decode24bppBgr(unpacked, hdr, byteWidth, rgba);    break;
    default: return null;
  }
  return rgba;
}

// ----- pixel decoders (all bit-orderings LSB-first per bmconv) -----

function decode1bpp(src: Uint8Array, hdr: BitmapHeader, byteWidth: number, dst: Uint8Array): void {
  for (let y = 0; y < hdr.heightPixels; y++) {
    const lineStart = y * byteWidth;
    for (let x = 0; x < hdr.widthPixels; x++) {
      const byte = src[lineStart + (x >> 3)];
      const bit = (byte >> (x & 7)) & 1;
      const g = bit ? 255 : 0;
      const o = (y * hdr.widthPixels + x) * 4;
      dst[o] = g; dst[o + 1] = g; dst[o + 2] = g; dst[o + 3] = 255;
    }
  }
}

function decodeNbppGray(
  src: Uint8Array, hdr: BitmapHeader, byteWidth: number, dst: Uint8Array,
  bpp: 2 | 4,
): void {
  const pxPerByte = 8 / bpp;
  const mask = (1 << bpp) - 1;
  const scale = 255 / mask;
  for (let y = 0; y < hdr.heightPixels; y++) {
    const lineStart = y * byteWidth;
    for (let x = 0; x < hdr.widthPixels; x++) {
      const byte = src[lineStart + Math.floor(x / pxPerByte)];
      const shift = (x % pxPerByte) * bpp;
      const v = (byte >> shift) & mask;
      const g = Math.round(v * scale);
      const o = (y * hdr.widthPixels + x) * 4;
      dst[o] = g; dst[o + 1] = g; dst[o + 2] = g; dst[o + 3] = 255;
    }
  }
}

function decode8bppGray(src: Uint8Array, hdr: BitmapHeader, byteWidth: number, dst: Uint8Array): void {
  for (let y = 0; y < hdr.heightPixels; y++) {
    const lineStart = y * byteWidth;
    for (let x = 0; x < hdr.widthPixels; x++) {
      const g = src[lineStart + x];
      const o = (y * hdr.widthPixels + x) * 4;
      dst[o] = g; dst[o + 1] = g; dst[o + 2] = g; dst[o + 3] = 255;
    }
  }
}

// Symbian's standard 16-colour EPOC palette (TRgb::Color16 maps indices
// 0..15 to these RGB triples).  Taken from the Symbian source comment
// block — same ordering bmconv uses for /c4 paletted output.
const PALETTE_16: ReadonlyArray<readonly [number, number, number]> = [
  [0x00, 0x00, 0x00],  [0x55, 0x55, 0x55],  [0xAA, 0xAA, 0xAA],  [0xFF, 0xFF, 0xFF],
  [0x55, 0x00, 0x00],  [0x00, 0x55, 0x00],  [0x00, 0x00, 0x55],  [0x55, 0x55, 0x00],
  [0x55, 0x00, 0x55],  [0x00, 0x55, 0x55],  [0xFF, 0x00, 0x00],  [0x00, 0xFF, 0x00],
  [0x00, 0x00, 0xFF],  [0xFF, 0xFF, 0x00],  [0xFF, 0x00, 0xFF],  [0x00, 0xFF, 0xFF],
];

function decode4bppColor(src: Uint8Array, hdr: BitmapHeader, byteWidth: number, dst: Uint8Array): void {
  for (let y = 0; y < hdr.heightPixels; y++) {
    const lineStart = y * byteWidth;
    for (let x = 0; x < hdr.widthPixels; x++) {
      const byte = src[lineStart + (x >> 1)];
      const v = (x & 1) ? (byte >> 4) & 0xF : byte & 0xF;
      const [r, g, b] = PALETTE_16[v];
      const o = (y * hdr.widthPixels + x) * 4;
      dst[o] = r; dst[o + 1] = g; dst[o + 2] = b; dst[o + 3] = 255;
    }
  }
}

// Symbian's standard EColor256 palette, transcribed from bmconv's
// `color256array` (rgb.h).  Each entry is a TRgb long: red in the low
// byte, then green, then blue (TRgb::TRgb(long) in rgb.cpp).  Layout:
// three 36-entry blocks of a 6x6x6 colour cube (values 00/33/66), 40
// extra greys + primary ramps, then the remaining three cube blocks
// (99/cc/ff).  Used by 8bpp colour bitmaps — e.g. the picture in the
// Series 7 ROM's "Welcome to Series 7" document.
const PALETTE_256: ReadonlyArray<number> = [
  0x000000, 0x000033, 0x000066, 0x000099, 0x0000cc, 0x0000ff,
  0x003300, 0x003333, 0x003366, 0x003399, 0x0033cc, 0x0033ff,
  0x006600, 0x006633, 0x006666, 0x006699, 0x0066cc, 0x0066ff,
  0x009900, 0x009933, 0x009966, 0x009999, 0x0099cc, 0x0099ff,
  0x00cc00, 0x00cc33, 0x00cc66, 0x00cc99, 0x00cccc, 0x00ccff,
  0x00ff00, 0x00ff33, 0x00ff66, 0x00ff99, 0x00ffcc, 0x00ffff,
  0x330000, 0x330033, 0x330066, 0x330099, 0x3300cc, 0x3300ff,
  0x333300, 0x333333, 0x333366, 0x333399, 0x3333cc, 0x3333ff,
  0x336600, 0x336633, 0x336666, 0x336699, 0x3366cc, 0x3366ff,
  0x339900, 0x339933, 0x339966, 0x339999, 0x3399cc, 0x3399ff,
  0x33cc00, 0x33cc33, 0x33cc66, 0x33cc99, 0x33cccc, 0x33ccff,
  0x33ff00, 0x33ff33, 0x33ff66, 0x33ff99, 0x33ffcc, 0x33ffff,
  0x660000, 0x660033, 0x660066, 0x660099, 0x6600cc, 0x6600ff,
  0x663300, 0x663333, 0x663366, 0x663399, 0x6633cc, 0x6633ff,
  0x666600, 0x666633, 0x666666, 0x666699, 0x6666cc, 0x6666ff,
  0x669900, 0x669933, 0x669966, 0x669999, 0x6699cc, 0x6699ff,
  0x66cc00, 0x66cc33, 0x66cc66, 0x66cc99, 0x66cccc, 0x66ccff,
  0x66ff00, 0x66ff33, 0x66ff66, 0x66ff99, 0x66ffcc, 0x66ffff,
  0x111111, 0x222222, 0x444444, 0x555555, 0x777777,
  0x000011, 0x000022, 0x000044, 0x000055, 0x000077,
  0x001100, 0x002200, 0x004400, 0x005500, 0x007700,
  0x110000, 0x220000, 0x440000, 0x550000, 0x770000,
  0x880000, 0xaa0000, 0xbb0000, 0xdd0000, 0xee0000,
  0x008800, 0x00aa00, 0x00bb00, 0x00dd00, 0x00ee00,
  0x000088, 0x0000aa, 0x0000bb, 0x0000dd, 0x0000ee,
  0x888888, 0xaaaaaa, 0xbbbbbb, 0xdddddd, 0xeeeeee,
  0x990000, 0x990033, 0x990066, 0x990099, 0x9900cc, 0x9900ff,
  0x993300, 0x993333, 0x993366, 0x993399, 0x9933cc, 0x9933ff,
  0x996600, 0x996633, 0x996666, 0x996699, 0x9966cc, 0x9966ff,
  0x999900, 0x999933, 0x999966, 0x999999, 0x9999cc, 0x9999ff,
  0x99cc00, 0x99cc33, 0x99cc66, 0x99cc99, 0x99cccc, 0x99ccff,
  0x99ff00, 0x99ff33, 0x99ff66, 0x99ff99, 0x99ffcc, 0x99ffff,
  0xcc0000, 0xcc0033, 0xcc0066, 0xcc0099, 0xcc00cc, 0xcc00ff,
  0xcc3300, 0xcc3333, 0xcc3366, 0xcc3399, 0xcc33cc, 0xcc33ff,
  0xcc6600, 0xcc6633, 0xcc6666, 0xcc6699, 0xcc66cc, 0xcc66ff,
  0xcc9900, 0xcc9933, 0xcc9966, 0xcc9999, 0xcc99cc, 0xcc99ff,
  0xcccc00, 0xcccc33, 0xcccc66, 0xcccc99, 0xcccccc, 0xccccff,
  0xccff00, 0xccff33, 0xccff66, 0xccff99, 0xccffcc, 0xccffff,
  0xff0000, 0xff0033, 0xff0066, 0xff0099, 0xff00cc, 0xff00ff,
  0xff3300, 0xff3333, 0xff3366, 0xff3399, 0xff33cc, 0xff33ff,
  0xff6600, 0xff6633, 0xff6666, 0xff6699, 0xff66cc, 0xff66ff,
  0xff9900, 0xff9933, 0xff9966, 0xff9999, 0xff99cc, 0xff99ff,
  0xffcc00, 0xffcc33, 0xffcc66, 0xffcc99, 0xffcccc, 0xffccff,
  0xffff00, 0xffff33, 0xffff66, 0xffff99, 0xffffcc, 0xffffff,
];

function decode8bppColor(src: Uint8Array, hdr: BitmapHeader, byteWidth: number, dst: Uint8Array): void {
  for (let y = 0; y < hdr.heightPixels; y++) {
    const lineStart = y * byteWidth;
    for (let x = 0; x < hdr.widthPixels; x++) {
      const v = PALETTE_256[src[lineStart + x]];
      const o = (y * hdr.widthPixels + x) * 4;
      dst[o    ] =  v         & 0xFF;   // TRgb low byte = red
      dst[o + 1] = (v >>>  8) & 0xFF;
      dst[o + 2] = (v >>> 16) & 0xFF;
      dst[o + 3] = 255;
    }
  }
}

// 12bpp: packed as one 16-bit LE word per pixel, RGB444 in low 12 bits
// (0x0RGB).  Top 4 bits ignored.
function decode12bppRgb444(src: Uint8Array, hdr: BitmapHeader, byteWidth: number, dst: Uint8Array): void {
  for (let y = 0; y < hdr.heightPixels; y++) {
    const lineStart = y * byteWidth;
    for (let x = 0; x < hdr.widthPixels; x++) {
      const p = src[lineStart + x * 2] | (src[lineStart + x * 2 + 1] << 8);
      const r4 =  p        & 0xF;
      const g4 = (p >>  4) & 0xF;
      const b4 = (p >>  8) & 0xF;
      const o = (y * hdr.widthPixels + x) * 4;
      // Expand 4-bit -> 8-bit by repeating the nibble (standard upsample).
      dst[o    ] = (r4 << 4) | r4;
      dst[o + 1] = (g4 << 4) | g4;
      dst[o + 2] = (b4 << 4) | b4;
      dst[o + 3] = 255;
    }
  }
}

// 16bpp: RGB565, one 16-bit LE word per pixel.
function decode16bppRgb565(src: Uint8Array, hdr: BitmapHeader, byteWidth: number, dst: Uint8Array): void {
  for (let y = 0; y < hdr.heightPixels; y++) {
    const lineStart = y * byteWidth;
    for (let x = 0; x < hdr.widthPixels; x++) {
      const p = src[lineStart + x * 2] | (src[lineStart + x * 2 + 1] << 8);
      const r5 = (p >> 11) & 0x1F;
      const g6 = (p >>  5) & 0x3F;
      const b5 =  p        & 0x1F;
      const o = (y * hdr.widthPixels + x) * 4;
      dst[o    ] = (r5 << 3) | (r5 >> 2);
      dst[o + 1] = (g6 << 2) | (g6 >> 4);
      dst[o + 2] = (b5 << 3) | (b5 >> 2);
      dst[o + 3] = 255;
    }
  }
}

// 24bpp: BGR triplets per pixel (bmconv stores as B, G, R).
function decode24bppBgr(src: Uint8Array, hdr: BitmapHeader, byteWidth: number, dst: Uint8Array): void {
  for (let y = 0; y < hdr.heightPixels; y++) {
    const lineStart = y * byteWidth;
    for (let x = 0; x < hdr.widthPixels; x++) {
      const off = lineStart + x * 3;
      const b = src[off], g = src[off + 1], r = src[off + 2];
      const o = (y * hdr.widthPixels + x) * 4;
      dst[o] = r; dst[o + 1] = g; dst[o + 2] = b; dst[o + 3] = 255;
    }
  }
}
