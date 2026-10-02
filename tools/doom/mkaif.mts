// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Writes DOOM.AIF, the application information file that gives the
// game its name and icon: a direct file store (UID1 0x10000037, UID2
// 0x1000006a) holding the caption and two 48 x 48 four-grey bitmaps, the
// icon and its mask, in the layout real ER5 AIFs have (this one was made
// to match applib's HEXCALC.AIF byte for byte in structure, and is read
// back through the same decoder the app library uses to pull icons out of
// AIFs, to prove it).
//
// The icon is a lemming walking, from the game's own sprite, at 4x with a
// black outline — drawn here in four greys: hair dark, robe light, skin
// white.
//
//   node --experimental-strip-types tools/lemmings/mkaif.mts [OUT.AIF]

import * as fs from 'node:fs';
import * as path from 'node:path';
import { findEmbeddedBitmaps, renderBitmapAt } from '../../frontend/src/lib/converters/sketch.ts';

const UID3 = 0x0d0c5e03;        // from the range EPOC sets aside for development
const CAPTION = 'Doom';
const W = 48, H = 48;

// The icon: a red-eyed skull-ish "face" is more than four greys can carry, so it is the
// name, DOOM, in the game's own blocky letters on a dark plate with a lighter frame.
const font = (await import('node:fs')).readFileSync(
  path.join(path.dirname(new URL(import.meta.url).pathname), '../lemmings/src/font.c'), 'utf8');
const glyph = (ch: string): number[] => {
  const m = new RegExp('\\{ ((?:0x[0-9a-f]+(?:, )?){7}) \\},\\s+/\\* ' + ch.replace(/[^A-Za-z0-9]/g, '\\$&') + ' \\*/').exec(font);
  if (!m) throw new Error('no glyph ' + ch);
  return m[1].split(', ').map(x => parseInt(x, 16));
};
const grey = new Uint8Array(W * H).fill(3);
const shape = new Uint8Array(W * H);
// plate with a frame
for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
  const edge = x < 2 || y < 2 || x >= W - 2 || y >= H - 2;
  grey[y * W + x] = edge ? 3 : 1; shape[y * W + x] = 1;
}
for (let x = 2; x < W - 2; x++) { grey[2 * W + x] = 2; grey[(H - 3) * W + x] = 0; }
// DOOM twice as tall as wide: 4 letters of 5x7 at 2x across, 3x down is 8+... fits as 4 x (5*2+2) = 48 wide
const word = 'DOOM';
for (let k = 0; k < 4; k++) {
  const g = glyph(word[k]);
  for (let r = 0; r < 7; r++) for (let c = 0; c < 5; c++) {
    if (!(g[r] & (0x10 >> c))) continue;
    for (let dy = 0; dy < 3; dy++) for (let dx = 0; dx < 2; dx++) {
      const x = 3 + k * 11 + c * 2 + dx, y = 13 + r * 3 + dy;
      if (x < W - 2) { grey[y * W + x] = 3; }
    }
  }
}

// Two bits a pixel, leftmost pixel in the low bits, rows padded to a word (48 px is 12 bytes: no padding).
function pack(px: (i: number) => number): Uint8Array {
  const out = new Uint8Array((W / 4) * H);
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
    out[y * (W / 4) + (x >> 2)] |= px(y * W + x) << ((x & 3) * 2);
  }
  return out;
}

// EPOC byte RLE: a control byte n >= 0 repeats the next byte n+1 times, n < 0 copies -n bytes.
function rle(data: Uint8Array): number[] {
  const out: number[] = [];
  let i = 0;
  while (i < data.length) {
    let run = 1;
    while (i + run < data.length && run < 128 && data[i + run] === data[i]) run++;
    if (run >= 2) { out.push(run - 1, data[i]); i += run; continue; }
    let lit = 1;
    while (i + lit < data.length && lit < 128 &&
           !(i + lit + 1 < data.length && data[i + lit] === data[i + lit + 1])) lit++;
    out.push((-lit) & 0xff, ...data.subarray(i, i + lit));
    i += lit;
  }
  return out;
}

const le32 = (v: number): number[] => [v & 255, (v >>> 8) & 255, (v >>> 16) & 255, (v >>> 24) & 255];

function bitmap(data: Uint8Array): number[] {
  const body = rle(data);
  const size = 40 + body.length;
  // iBitmapSize, iStructSize, width, height, twips w and h, bpp, colour, palette entries, compression
  return [...le32(size), ...le32(40), ...le32(W), ...le32(H), ...le32(0), ...le32(0),
          ...le32(2), ...le32(0), ...le32(0), ...le32(1), ...body];
}

// The same checksum EPOC puts after a file's three UIDs: a CRC-CCITT of the
// odd bytes in the high half and of the even bytes in the low.
function crc16(bytes: number[]): number {
  let crc = 0;
  for (const b of bytes) {
    crc ^= b << 8;
    for (let k = 0; k < 8; k++) crc = crc & 0x8000 ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff;
  }
  return crc;
}

const icon = bitmap(pack(i => grey[i]));
const mask = bitmap(pack(i => shape[i] ? 0 : 3));        // black where the icon shows

const caption = [(CAPTION.length << 2) | 2, ...Buffer.from(CAPTION, 'latin1')];
const head = [...le32(0x10000037), ...le32(0x1000006a), ...le32(UID3)];
const capRef = 0x14;
const bmpRef = capRef + caption.length;
const toc = bmpRef + icon.length + mask.length;
const tail = [0x02, ...le32(capRef), 0x01, 0x00, 0x02, ...le32(bmpRef), W & 255, W >> 8, 0x01,
              ...new Array(15).fill(0)];
const crc = (crc16(head.filter((_, i) => i % 2 === 1)) << 16) | crc16(head.filter((_, i) => i % 2 === 0));
const file = Uint8Array.from([...head, ...le32(crc >>> 0), ...le32(toc), ...caption, ...icon, ...mask, ...tail]);

const out = process.argv[2] ?? path.join(path.dirname(new URL(import.meta.url).pathname), 'DOOM.AIF');
fs.writeFileSync(out, file);

// Read it back the way the app library does.
const found = findEmbeddedBitmaps(file, 16, file.length);
const png = found.length ? renderBitmapAt(file, found[0]) : null;
if (found.length !== 2 || !png || png.length < 100) throw new Error(`the AIF does not read back (${found.length} bitmaps)`);
fs.writeFileSync(out.replace(/\.AIF$/i, '') + '-icon.png', png);
console.log(`wrote ${out}: ${file.length} bytes, 2 bitmaps; preview ${out.replace(/\.AIF$/i, '')}-icon.png`);
