// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Put LEMMINGS.EXE on a blank FAT16 card image, for the netpad (whose ROM
// has no spare file to swap the game into, so it is opened off a card
// instead — see tests/integration/test-lemmings-netpad.sh).
//
//   node --experimental-strip-types tools/lemmings/mkcard.mts OUT.img [LEMMINGS.EXE]

import * as fs from 'node:fs';
import * as path from 'node:path';
import { addFile, createBlankImage } from '../../frontend/src/lib/fat16.ts';

const out = process.argv[2];
if (!out) {
  console.error('usage: mkcard.mts OUT.img [FILE]');
  process.exit(2);
}
const exe = process.argv[3] ?? path.join(path.dirname(new URL(import.meta.url).pathname), 'LEMMINGS.EXE');
const img = createBlankImage(16 * 1024 * 1024);
const r = addFile(img, 'LEMMINGS.EXE', new Uint8Array(fs.readFileSync(exe)));
if (!r.ok) throw new Error(`could not add the file: ${JSON.stringify(r)}`);
fs.writeFileSync(out, img);
console.log(`wrote ${out}`);
