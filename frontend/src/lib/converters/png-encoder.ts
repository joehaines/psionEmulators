// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Minimal dependency-free PNG encoder for RGBA8 images.
//
// Why hand-rolled: the converters need to produce PNG bytes in both the
// browser AND under Node (for tests).  Browser Canvas would be enough in
// production but isn't available in our test runner.  A full PNG
// encoder in pure JS is ~120 lines once you commit to "stored" (i.e.
// uncompressed) DEFLATE blocks — the output is larger than a real
// deflate would give, but Sketch images are small (a few KB) so it's
// fine.

const PNG_SIGNATURE = new Uint8Array([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]);

export function encodePng(rgba: Uint8Array, width: number, height: number): Uint8Array {
  if (rgba.length !== width * height * 4) {
    throw new Error(`encodePng: rgba length ${rgba.length} != ${width}*${height}*4`);
  }

  // Build the raw scanline buffer: each row prefixed with a filter byte
  // (0 = None — every byte goes through unchanged).
  const stride = width * 4;
  const raw = new Uint8Array(height * (stride + 1));
  for (let y = 0; y < height; y++) {
    const dst = y * (stride + 1);
    raw[dst] = 0; // filter: none
    raw.set(rgba.subarray(y * stride, (y + 1) * stride), dst + 1);
  }

  const compressed = zlibDeflateStored(raw);

  return concat([
    PNG_SIGNATURE,
    pngChunk('IHDR', buildIHDR(width, height)),
    pngChunk('IDAT', compressed),
    pngChunk('IEND', new Uint8Array(0)),
  ]);
}

function buildIHDR(width: number, height: number): Uint8Array {
  const ihdr = new Uint8Array(13);
  const dv = new DataView(ihdr.buffer);
  dv.setUint32(0, width,  false);
  dv.setUint32(4, height, false);
  ihdr[8]  = 8;  // bit depth
  ihdr[9]  = 6;  // colour type: RGBA
  ihdr[10] = 0;  // compression: deflate
  ihdr[11] = 0;  // filter: standard
  ihdr[12] = 0;  // interlace: none
  return ihdr;
}

function pngChunk(type: string, data: Uint8Array): Uint8Array {
  const chunk = new Uint8Array(4 + 4 + data.length + 4);
  const dv = new DataView(chunk.buffer);
  dv.setUint32(0, data.length, false);
  for (let i = 0; i < 4; i++) chunk[4 + i] = type.charCodeAt(i);
  chunk.set(data, 8);
  const crc = crc32(chunk.subarray(4, 8 + data.length));
  dv.setUint32(8 + data.length, crc, false);
  return chunk;
}

// Wrap raw bytes as a zlib stream using stored (uncompressed) DEFLATE
// blocks.  Per RFC 1950: 2-byte zlib header + DEFLATE data + 4-byte
// adler-32 of the uncompressed input.
function zlibDeflateStored(input: Uint8Array): Uint8Array {
  // Stored DEFLATE block: 1-byte header (BFINAL + BTYPE=00) followed by
  // 2-byte LEN, 2-byte ~LEN, then LEN bytes.  Max LEN per block = 65535.
  const blocks: Uint8Array[] = [];
  let pos = 0;
  while (pos < input.length || (pos === 0 && input.length === 0)) {
    const remaining = input.length - pos;
    const len = Math.min(remaining, 0xFFFF);
    const isLast = pos + len >= input.length;
    const header = new Uint8Array(5);
    header[0] = isLast ? 0x01 : 0x00;  // BFINAL bit, BTYPE=00 (stored)
    header[1] =  len        & 0xFF;
    header[2] = (len >>  8) & 0xFF;
    header[3] = (~len)      & 0xFF;
    header[4] = (~len >> 8) & 0xFF;
    blocks.push(header);
    blocks.push(input.subarray(pos, pos + len));
    pos += len;
    if (input.length === 0) break;
  }
  const deflate = concat(blocks);

  const adler = adler32(input);
  const out = new Uint8Array(2 + deflate.length + 4);
  // zlib header: CMF=0x78 (deflate, 32K window), FLG chosen so
  // (CMF*256 + FLG) % 31 == 0 — 0x01 satisfies this with FLEVEL=0.
  out[0] = 0x78;
  out[1] = 0x01;
  out.set(deflate, 2);
  const dv = new DataView(out.buffer);
  dv.setUint32(2 + deflate.length, adler, false);
  return out;
}

function adler32(bytes: Uint8Array): number {
  const MOD = 65521;
  let a = 1, b = 0;
  for (let i = 0; i < bytes.length; i++) {
    a = (a + bytes[i]) % MOD;
    b = (b + a)        % MOD;
  }
  return ((b << 16) | a) >>> 0;
}

// CRC32 used by PNG chunks — standard IEEE 802.3 polynomial, table-based.
let crcTable: Uint32Array | null = null;
function crc32(bytes: Uint8Array): number {
  if (!crcTable) {
    crcTable = new Uint32Array(256);
    for (let n = 0; n < 256; n++) {
      let c = n;
      for (let k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1);
      crcTable[n] = c >>> 0;
    }
  }
  let crc = 0xFFFFFFFF;
  for (let i = 0; i < bytes.length; i++) {
    crc = (crcTable[(crc ^ bytes[i]) & 0xFF] ^ (crc >>> 8)) >>> 0;
  }
  return (crc ^ 0xFFFFFFFF) >>> 0;
}

function concat(parts: Uint8Array[]): Uint8Array {
  let total = 0;
  for (const p of parts) total += p.length;
  const out = new Uint8Array(total);
  let off = 0;
  for (const p of parts) { out.set(p, off); off += p.length; }
  return out;
}
