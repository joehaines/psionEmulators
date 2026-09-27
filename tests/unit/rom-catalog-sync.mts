// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The ROM a device boots is named in three places that nothing ties
// together at build time:
//
//   core/device_registry.cpp       — each profile's romFilename, the
//     default image, relative to roms/;
//   frontend/src/lib/romCatalog.ts — the ROM choices Settings offers,
//     default first, with each image's size;
//   desktop/electron-builder.yml   — which of roms/ the desktop app
//     bundles (it has no Settings, so only what the defaults need).
//
// A path that drifts in any of them fails quietly — a 404 on the ROM
// fetch, a Settings entry that can't load, a desktop build missing its
// ROM — so they are checked against each other and against the files.
//
// Run:
//   node --experimental-strip-types tests/unit/rom-catalog-sync.mts

import { existsSync, readFileSync, statSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import { ROM_CATALOG } from '../../frontend/src/lib/romCatalog.ts';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const read = (p: string) => readFileSync(join(ROOT, p), 'utf8');

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}

// kProfiles entries open with three quoted strings on lines of their own —
// id, displayName, romFilename — with only comment lines between them.
const registry = new Map<string, string>(
  [...read('core/device_registry.cpp')
    .matchAll(/^\s+"([a-z0-9_]+)",\n\s+"[^"\n]*",\n(?:\s*\/\/.*\n)*\s+"([^"\n]+)",/gm)]
    .map(m => [m[1], m[2]]));
check(registry.size > 15,
      `parsed ${registry.size} profiles out of the registry — the shape of kProfiles ` +
      'must have changed, so this test is no longer checking anything');

for (const [id, rom] of registry)
  check(existsSync(join(ROOT, 'roms', rom)), `${id}: registry romFilename roms/${rom} does not exist`);

let options = 0;
for (const [deviceId, set] of Object.entries(ROM_CATALOG)) {
  check(registry.has(deviceId), `romCatalog lists '${deviceId}', which is not a device in the registry`);
  check(set.options.length > 1, `${deviceId}: a catalogue entry with one option offers no choice`);
  if (set.kind === 'rom')
    check(set.options[0].path === registry.get(deviceId),
          `${deviceId}: the first (default) option is roms/${set.options[0].path} but the registry ` +
          `boots roms/${registry.get(deviceId)} — the default has to be the ROM the device always booted`);
  const ids = new Set<string>();
  for (const o of set.options) {
    options++;
    check(!ids.has(o.id), `${deviceId}: duplicate option id ${o.id}`);
    ids.add(o.id);
    const file = join(ROOT, 'roms', o.path);
    if (!existsSync(file)) { check(false, `${deviceId}/${o.id}: roms/${o.path} does not exist`); continue; }
    const size = statSync(file).size;
    check(size === o.size, `${deviceId}/${o.id}: roms/${o.path} is ${size} bytes, catalogue says ${o.size}`);
    check(o.languages === undefined || o.languages.length !== 1,
          `${deviceId}/${o.id}: a one-name language list is no choice — use [] instead`);
  }
}

// Everything the frontend loads by a literal roms/ path (factory SSD packs,
// the alternate OS images the easter-egg boots use) must exist too.
const literals = new Set<string>();
for (const f of ['frontend/src/hooks/useEmulator.ts', 'frontend/public/emulator-worker.js'])
  for (const m of read(f).matchAll(/roms\/([A-Za-z0-9_./()$-]+\.(?:bin|BIN|img|IMG|ssd|rom))/g))
    literals.add(m[1]);
for (const p of literals) check(existsSync(join(ROOT, 'roms', p)), `frontend loads roms/${p}, which does not exist`);

// The desktop app bundles only what the defaults need: every registry ROM,
// every default OS image, and every literal above must fall under one of
// electron-builder.yml's roms filter folders.
const yml = read('desktop/electron-builder.yml');
const filterBlock = yml.split(/from: \.\.\/roms\n\s+to: roms\n\s+filter:\n/)[1] ?? '';
const folders = [...filterBlock.matchAll(/^\s+- '([^']+)\/\*\*'$/gm)].map(m => m[1]);
check(folders.length > 10, `parsed ${folders.length} folders from electron-builder.yml's roms filter`);
const bundled = (p: string) => folders.some(f => p.startsWith(f + '/'));
const needed = new Set<string>([...registry.values(), ...literals]);
for (const set of Object.values(ROM_CATALOG)) if (set.kind === 'os') needed.add(set.options[0].path);
for (const p of needed) check(bundled(p), `the desktop app needs roms/${p}, which electron-builder.yml does not bundle`);
for (const f of folders) check(existsSync(join(ROOT, 'roms', f)), `electron-builder.yml bundles roms/${f}, which does not exist`);

if (failures) {
  console.error(`\n${failures} check(s) FAILED`);
  process.exit(1);
}
console.log(`PASS rom-catalog-sync (${registry.size} registry ROMs, ${options} catalogue options, ` +
            `${folders.length} bundled folders)`);
