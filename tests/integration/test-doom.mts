// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Reads a screenshot taken by tests/integration/test-doom.sh of DOOM.EXE running on an
// emulated machine, and checks it shows Doom playing: a 3D view of many shades
// (or colours), the status bar's red numerals on a colour machine, and the
// frame-rate readout the port draws in the top margin.
//
//   node --experimental-strip-types tests/integration/test-doom.mts SHOT.{pgm,ppm} WIDTH HEIGHT [grey|colour]

import * as fs from 'node:fs';

const [file, ws, hs, kind = 'grey'] = process.argv.slice(2);
const data = fs.readFileSync(file);
const rgb = data.subarray(0, 2).toString('latin1') === 'P6';
const m = /^P[56]\s+(\d+)\s+(\d+)\s+255\s/.exec(data.subarray(0, 32).toString('latin1'));
if (!m) throw new Error('not a PGM/PPM');
const W = Number(m[1]), H = Number(m[2]);
const bpp = rgb ? 3 : 1;
const px = data.subarray(data.length - W * H * bpp);
const at = (x: number, y: number): [number, number, number] => {
  const i = (y * W + x) * bpp;
  return rgb ? [px[i], px[i + 1], px[i + 2]] : [px[i], px[i], px[i]];
};
const lum = (x: number, y: number): number => { const [r, g, b] = at(x, y); return (r + g + b) / 3; };

let failures = 0;
const check = (name: string, ok: boolean, detail = ''): void => {
  console.log(`${ok ? 'ok  ' : 'FAIL'} ${name}${ok || !detail ? '' : ` — ${detail}`}`);
  if (!ok) failures++;
};

check(`screenshot is ${ws}x${hs}`, W === Number(ws) && H === Number(hs), `${W}x${H}`);

// the picture: Doom's frame is 640 x 200 (or 400) centred on the panel
const top = Math.floor(H / 2) - (H >= 400 ? 200 : 100);
const bottom = H - top;
const shades = new Set<string>();
for (let y = top; y < bottom; y += 3) for (let x = 0; x < W; x += 5) shades.add(at(x, y).join(','));
check('the picture has many shades', shades.size >= (kind === 'colour' ? 50 : 8), `${shades.size} distinct`);

// the status bar: the bottom 32 rows of Doom's frame, busy with detail
const barTop = bottom - (H >= 400 ? 64 : 32), barBottom = bottom - 1;
let edges = 0;
for (let y = barTop; y < barBottom; y += 2) for (let x = 2; x < W; x += 2) if (Math.abs(lum(x, y) - lum(x - 1, y)) > 40) edges++;
check('the status bar is drawn', edges > 400, `${edges} edges`);

if (kind === 'colour') {
  let red = 0;
  for (let y = barTop; y < barBottom; y++) for (let x = 0; x < W; x++) { const [r, g, b] = at(x, y); if (r > 150 && g < 60 && b < 60) red++; }
  check('the status bar has its red numerals', red > 100, `${red} red pixels`);
}

// the frame-rate readout, black on white, top left of the panel
let dark = 0, bright = 0;
for (let y = 2; y < 16; y++) for (let x = 2; x < 100; x++) { const l = lum(x, y); if (l < 60) dark++; else if (l > 150) bright++; }
check('the frame-rate readout is there', dark > 40 && bright > 100, `${dark} dark, ${bright} bright`);

if (failures) { console.log(`\n${failures} check(s) failed`); process.exit(1); }
console.log('\nall checks passed');
