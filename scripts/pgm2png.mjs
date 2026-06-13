#!/usr/bin/env node
// Convert a binary PGM (P5) screenshot from harness/run into a PNG.
// Zero-dependency (uses node's built-in zlib) so it works in CI and
// sandboxes where Pillow / ImageMagick aren't installed.
//
//   node scripts/pgm2png.mjs in.pgm [out.png]

import { readFileSync, writeFileSync } from 'node:fs';
import { deflateSync } from 'node:zlib';

const inPath = process.argv[2];
const outPath = process.argv[3] ?? inPath.replace(/\.pgm$/i, '') + '.png';
if (!inPath) {
  console.error('usage: pgm2png.mjs in.pgm [out.png]');
  process.exit(2);
}

const buf = readFileSync(inPath);

// Parse the P5 header: magic, whitespace/comments, width, height, maxval.
let pos = 0;
function token() {
  while (pos < buf.length) {
    const c = buf[pos];
    if (c === 0x23) { while (pos < buf.length && buf[pos] !== 0x0a) pos++; } // # comment
    else if (c === 0x20 || c === 0x09 || c === 0x0a || c === 0x0d) pos++;
    else break;
  }
  const start = pos;
  while (pos < buf.length && !/\s/.test(String.fromCharCode(buf[pos]))) pos++;
  return buf.toString('ascii', start, pos);
}
const magic = token();
if (magic !== 'P5') { console.error(`not a P5 PGM: ${inPath}`); process.exit(1); }
const width = parseInt(token(), 10);
const height = parseInt(token(), 10);
const maxval = parseInt(token(), 10);
pos++; // single whitespace after maxval
const pixels = buf.subarray(pos, pos + width * height);
if (pixels.length < width * height) { console.error('truncated PGM'); process.exit(1); }

// Build the PNG: 8-bit grayscale, filter 0 per scanline.
const raw = Buffer.alloc(height * (width + 1));
for (let y = 0; y < height; y++) {
  raw[y * (width + 1)] = 0;
  for (let x = 0; x < width; x++) {
    let v = pixels[y * width + x];
    if (maxval !== 255) v = Math.round((v * 255) / maxval);
    raw[y * (width + 1) + 1 + x] = v;
  }
}

const crcTable = Array.from({ length: 256 }, (_, n) => {
  let c = n;
  for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
  return c >>> 0;
});
function crc32(b) {
  let c = 0xffffffff;
  for (const byte of b) c = crcTable[(c ^ byte) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}
function chunk(type, data) {
  const len = Buffer.alloc(4); len.writeUInt32BE(data.length);
  const body = Buffer.concat([Buffer.from(type, 'ascii'), data]);
  const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(body));
  return Buffer.concat([len, body, crc]);
}

const ihdr = Buffer.alloc(13);
ihdr.writeUInt32BE(width, 0);
ihdr.writeUInt32BE(height, 4);
ihdr[8] = 8;  // bit depth
ihdr[9] = 0;  // grayscale
const png = Buffer.concat([
  Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
  chunk('IHDR', ihdr),
  chunk('IDAT', deflateSync(raw)),
  chunk('IEND', Buffer.alloc(0)),
]);
writeFileSync(outPath, png);
console.log(`${outPath} (${width}x${height})`);
