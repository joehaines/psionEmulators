// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Reads the colour screenshot tests/integration/test-lemmings-netpad.sh
// took of the game on an emulated netpad: level 1 in play, in colour —
// blue sky, brown earth, green on the lemmings' heads — with the panel
// along the bottom.
//
//   node --experimental-strip-types tests/integration/test-lemmings-netpad.mts SHOT.ppm

import * as fs from 'node:fs';

const file = fs.readFileSync(process.argv[2]);
const m = /^P6\s+(\d+)\s+(\d+)\s+255\s/.exec(file.subarray(0, 32).toString('latin1'));
if (!m) throw new Error('not a binary PPM');
const W = Number(m[1]), H = Number(m[2]);
const px = file.subarray(file.length - W * H * 3);
const at = (x: number, y: number): [number, number, number] => {
  const i = (y * W + x) * 3;
  return [px[i], px[i + 1], px[i + 2]];
};

let failures = 0;
const check = (name: string, ok: boolean, detail = ''): void => {
  console.log(`${ok ? 'ok  ' : 'FAIL'} ${name}${ok || !detail ? '' : ` — ${detail}`}`);
  if (!ok) failures++;
};

check('screenshot is 640x240', W === 640 && H === 240, `${W}x${H}`);
const sky = at(300, 20), earth = at(300, 145);
check('the sky is blue', sky[2] > sky[0] + 40 && sky[2] >= sky[1], `sky ${sky}`);
check('the earth is brown', earth[0] > earth[2] + 20, `earth ${earth}`);

// Hair is green: a lemming is some pixels well above the ground where G beats R and B.
let green = 0;
for (let y = 60; y < 129; y++) for (let x = 100; x < 640; x++) {
  const [r, g, b] = at(x, y);
  if (g > r + 60 && g > b + 60) green++;
}
check('lemmings are out on the level', green >= 15, `${green} green pixels`);

let frames = 0;
for (let i = 0; i < 12; i++) if (at(11 + i * 50, 200).every(v => v === 0)) frames++;
check('the panel shows its twelve buttons', frames === 12, `${frames} frames found`);

if (failures) {
  console.log(`\n${failures} check(s) failed`);
  process.exit(1);
}
console.log('\nall checks passed');
