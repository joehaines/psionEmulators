// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Put a WAD (and optionally other files) on a blank FAT16 card image, for
// the emulated machine's card slot.
//
//   node --experimental-strip-types tools/doom/mkcard.mts OUT.img [WAD] [NAME=FILE ...] [--base IN.img]
//   node --experimental-strip-types tools/doom/mkcard.mts OUT.img --only NAME=FILE ...
//
// WAD defaults to tools/Doom1.WAD and is stored as DOOM1.WAD. --base adds
// the files to a copy of IN.img instead of a blank card; --only leaves the
// WAD off.

import * as fs from 'node:fs';
import * as path from 'node:path';
import { addFile, createBlankImage } from '../../frontend/src/lib/fat16.ts';

const here = path.dirname(new URL(import.meta.url).pathname);
const args = process.argv.slice(2);
const flag = (name: string): string | null => {
  const i = args.indexOf(name);
  if (i < 0) return null;
  const [, v] = args.splice(i, 2);
  return v ?? '';
};
const base = flag('--base');
const only = args.includes('--only');
if (only) args.splice(args.indexOf('--only'), 1);
const out = args[0];
if (!out) {
  console.error('usage: mkcard.mts OUT.img [WAD] [NAME=FILE ...] [--base IN.img] [--only]');
  process.exit(2);
}
const wad = args[1] && !args[1].includes('=') ? args[1] : path.join(here, '..', 'Doom1.WAD');
const img = base ? new Uint8Array(fs.readFileSync(base)) : createBlankImage(16 * 1024 * 1024);
const put = (name: string, file: string): void => {
  const r = addFile(img, name, new Uint8Array(fs.readFileSync(file)));
  if (!r.ok) throw new Error(`could not add ${name}: ${JSON.stringify(r)}`);
};
if (!only) put('DOOM1.WAD', wad);
for (const a of args.slice(1)) {
  const [n, f] = a.split('=');
  if (f) put(n, f);
}
fs.writeFileSync(out, img);
console.log(`wrote ${out}`);
