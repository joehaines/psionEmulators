// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Put a WAD (and optionally other files) on a blank FAT16 card image, for
// the emulated machine's card slot.
//
//   node --experimental-strip-types tools/doom/mkcard.mts OUT.img [WAD] [NAME=FILE ...]
//
// WAD defaults to tools/Doom1.WAD and is stored as DOOM1.WAD.

import * as fs from 'node:fs';
import * as path from 'node:path';
import { addFile, createBlankImage } from '../../frontend/src/lib/fat16.ts';

const here = path.dirname(new URL(import.meta.url).pathname);
const out = process.argv[2];
if (!out) {
  console.error('usage: mkcard.mts OUT.img [WAD] [NAME=FILE ...]');
  process.exit(2);
}
const wad = process.argv[3] && !process.argv[3].includes('=') ? process.argv[3] : path.join(here, '..', 'Doom1.WAD');
const img = createBlankImage(16 * 1024 * 1024);
const put = (name: string, file: string): void => {
  const r = addFile(img, name, new Uint8Array(fs.readFileSync(file)));
  if (!r.ok) throw new Error(`could not add ${name}: ${JSON.stringify(r)}`);
};
put('DOOM1.WAD', wad);
for (const a of process.argv.slice(3)) {
  const [n, f] = a.split('=');
  if (f) put(n, f);
}
fs.writeFileSync(out, img);
console.log(`wrote ${out}`);
