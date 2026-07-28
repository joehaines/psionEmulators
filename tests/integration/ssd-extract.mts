// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Drives the frontend's FEFS reader over an SSD pack image from the
// shell, so tests/integration/test-ssd-write.sh can check that files
// the *device* wrote can be listed and pulled off the pack — the same
// code path the SSD dialog's per-file download uses.
//
//   node --experimental-strip-types ssd-extract.mts IMAGE
//        → one "SIZE\tPATH" line per file
//   node --experimental-strip-types ssd-extract.mts IMAGE PACKPATH OUT
//        → writes the file's bytes to OUT

import { readFileSync, writeFileSync } from 'node:fs';
import { listFiles, readFileFromPack } from '../../frontend/src/lib/fefs.ts';

const [image, packPath, out] = process.argv.slice(2);
if (!image) {
  console.error('usage: ssd-extract.mts IMAGE [PACKPATH OUT]');
  process.exit(2);
}

const bytes = new Uint8Array(readFileSync(image));

if (!packPath) {
  for (const f of listFiles(bytes)) console.log(`${f.size}\t${f.name}`);
  process.exit(0);
}

if (!out) {
  console.error('usage: ssd-extract.mts IMAGE [PACKPATH OUT]');
  process.exit(2);
}
writeFileSync(out, readFileFromPack(bytes, packPath));
