// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Reads the screenshot tests/integration/test-lemmings-series5.sh took of
// the game running on an emulated Series 5, and checks that what is on
// the screen is level 1 in play: a light sky over darker ground, the
// panel's twelve buttons along the bottom, and lemmings between the
// trapdoor and the floor.
//
//   node --experimental-strip-types tests/integration/test-lemmings-series5.mts SHOT.pgm

import * as fs from 'node:fs';

const file = fs.readFileSync(process.argv[2]);
const m = /^P5\s+(\d+)\s+(\d+)\s+255\s/.exec(file.subarray(0, 32).toString('latin1'));
if (!m) throw new Error('not a binary PGM');
const W = Number(m[1]), H = Number(m[2]);
const px = file.subarray(file.length - W * H);
const at = (x: number, y: number): number => px[y * W + x];

let failures = 0;
const check = (name: string, ok: boolean, detail = ''): void => {
  console.log(`${ok ? 'ok  ' : 'FAIL'} ${name}${ok || !detail ? '' : ` — ${detail}`}`);
  if (!ok) failures++;
};

check('screenshot is 640x240', W === 640 && H === 240, `${W}x${H}`);
check('sky is lighter than the ground', at(300, 20) > at(300, 145) + 40,
  `sky ${at(300, 20)}, ground ${at(300, 145)}`);

// Each of the twelve buttons has a black frame one pixel outside it; the
// first button starts at x=12 and they are 50 apart.
let frames = 0;
for (let i = 0; i < 12; i++) if (at(11 + i * 50, 200) === 0) frames++;
check('the panel shows its twelve buttons', frames === 12, `${frames} frames found`);

// Lemmings are dark specks above the ground, below the trapdoor.
let dark = 0;
for (let y = 60; y < 129; y++) for (let x = 100; x < 640; x++) if (at(x, y) <= 30) dark++;
check('lemmings are out on the level', dark >= 30, `${dark} dark pixels`);

if (failures) {
  console.log(`\n${failures} check(s) failed`);
  process.exit(1);
}
console.log('\nall checks passed');
