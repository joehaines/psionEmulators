// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Copy the ROM images the website serves into a staging directory for the
// deploy (.github/workflows/deploy.yml mirrors it to the server's roms/).
//
// roms/ in the repository is the whole Psion-ROM archive plus this site's
// own images (roms/README.md). The site loads only some of them, and the
// web host's disk quota does not stretch to the rest, so the deploy
// uploads exactly these:
//
//   - every device's default ROM (core/device_registry.cpp romFilename);
//   - every ROM choice Settings offers (frontend/src/lib/romCatalog.ts);
//   - every image the frontend loads by a fixed roms/ path (the factory
//     SSD packs, the ESHELL and Quartz OS images the easter-egg boots use).
//
// The same three lists tests/unit/rom-catalog-sync.mts checks, so a path
// the site asks for and the deploy leaves out fails there first.
//
// Run (node 22+):
//   node --experimental-strip-types scripts/stage-deploy-roms.mts <out-dir>

import { copyFileSync, existsSync, mkdirSync, readFileSync, rmSync, statSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { ROM_CATALOG } from '../frontend/src/lib/romCatalog.ts';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const out = process.argv[2];
if (!out) { console.error('usage: stage-deploy-roms.mts <out-dir>'); process.exit(2); }
const read = (p: string) => readFileSync(join(ROOT, p), 'utf8');

const served = new Set<string>();
for (const m of read('core/device_registry.cpp')
  .matchAll(/^\s+"([a-z0-9_]+)",\n\s+"[^"\n]*",\n(?:\s*\/\/.*\n)*\s+"([^"\n]+)",/gm))
  served.add(m[2]);
for (const set of Object.values(ROM_CATALOG)) for (const o of set.options) served.add(o.path);
for (const f of ['frontend/src/hooks/useEmulator.ts', 'frontend/public/emulator-worker.js'])
  for (const m of read(f).matchAll(/roms\/([A-Za-z0-9_./()$-]+\.(?:bin|BIN|img|IMG|ssd|rom))/g))
    served.add(m[1]);

rmSync(out, { recursive: true, force: true });
let bytes = 0;
for (const p of [...served].sort()) {
  const src = join(ROOT, 'roms', p);
  if (!existsSync(src)) { console.error(`missing: roms/${p}`); process.exit(1); }
  mkdirSync(dirname(join(out, p)), { recursive: true });
  copyFileSync(src, join(out, p));
  bytes += statSync(src).size;
}
console.log(`staged ${served.size} ROM images, ${(bytes / 1048576).toFixed(0)} MB, in ${out}`);
