// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Phase 0 gate: does the emulator's runtime substrate work inside Electron
// over the app:// scheme?
//
// Answers it without needing the 201 MB Emscripten build, by probing the
// exact primitives the real app depends on — a classic Worker started from
// the custom scheme, importScripts() off it, a transferred OffscreenCanvas,
// WebAssembly, IndexedDB, and ROM fetches with Range. Under file:// the
// first three fail, which is why this test exists at all.
//
//   node test/capabilities.mjs
//
// Needs a display; the runner adds xvfb-run itself on Linux when DISPLAY
// is unset. Run `npm run build:main` and the renderer build first.

import { spawn } from 'node:child_process';
import { existsSync, mkdirSync, copyFileSync, readdirSync, statSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const DESKTOP = resolve(HERE, '..');
const REPO = resolve(DESKTOP, '..');
const RENDERER = join(DESKTOP, 'build', 'renderer');
const ELECTRON = join(DESKTOP, 'node_modules', 'electron', 'dist', 'electron');

// Every capability the probe reports, with why it matters. A missing key is
// as much a failure as a false one — it means the probe died partway.
const REQUIRED = {
  secureContext:            'app:// must be a secure context (crypto.randomUUID, async clipboard)',
  randomUUID:               'uniqueId.ts and analytics.ts call crypto.randomUUID()',
  workerModeSupported:      "App.tsx's WORKER_MODE probe must pass, or we fall back to the slow path",
  wasm:                     'the emulator core is WebAssembly',
  indexedDB:                'save states live in IndexedDB',
  workerStarts:             'emulator-worker.js is a classic Worker — blocked under file://',
  workerImportScripts:      'the worker importScripts() psion.js — blocked under file://',
  offscreenCanvasInWorker:  'the worker paints into a transferred OffscreenCanvas',
  romFetch:                 'ROMs are fetched over the scheme',
  romContentLength:         'the ROM progress UI reads Content-Length to compute its fraction',
  romStreams:               'the ROM progress UI streams resp.body.getReader() as it downloads',
  traversalRefused:         'the protocol handler must not serve files outside its roots',
};

function fail(msg) { console.error(`\n✗ ${msg}\n`); process.exit(1); }

if (!existsSync(ELECTRON)) fail(`Electron binary missing at ${ELECTRON} — run npm install in desktop/`);
if (!existsSync(join(RENDERER, 'index.html')))
  fail('renderer not built — run `npm run build:desktop` in frontend/ first');

// Stage the probe beside the renderer so it is served by the real handler
// from the real root, rather than through some test-only special case.
for (const f of ['probe.html', 'probe.js', 'probe-worker.js', 'probe-import.js']) {
  copyFileSync(join(HERE, 'probe', f), join(RENDERER, f));
}
mkdirSync(RENDERER, { recursive: true });

// Use the smallest ROM available so the whole-file fetch stays quick.
const romsDir = join(REPO, 'roms');
const roms = readdirSync(romsDir)
  .filter((f) => /\.(rom|bin|img)$/i.test(f))
  .map((f) => ({ f, size: statSync(join(romsDir, f)).size }))
  .sort((a, b) => a.size - b.size);
if (!roms.length) fail(`no ROM images found in ${romsDir}`);
const rom = roms[0].f;
console.log(`probing with ROM: ${rom} (${roms[0].size} bytes)`);

const args = [join(HERE, 'probe-main.cjs'), '--no-sandbox'];
const useXvfb = process.platform === 'linux' && !process.env.DISPLAY;
const cmd = useXvfb ? 'xvfb-run' : ELECTRON;
const argv = useXvfb ? ['-a', ELECTRON, ...args] : args;

const child = spawn(cmd, argv, {
  cwd: DESKTOP,
  env: { ...process.env, PSION_PROBE_ROM: rom, ELECTRON_DISABLE_SECURITY_WARNINGS: '1' },
  stdio: ['ignore', 'pipe', 'pipe'],
});

let out = '';
let err = '';
child.stdout.on('data', (d) => { out += d; });
child.stderr.on('data', (d) => { err += d; });

child.on('close', (code) => {
  const line = out.split('\n').find((l) => l.startsWith('PSION_PROBE_RESULT '));
  if (!line) {
    console.error(out.trim());
    console.error(err.trim().split('\n').slice(-25).join('\n'));
    fail(`probe produced no result (electron exit ${code})`);
  }

  const results = JSON.parse(line.slice('PSION_PROBE_RESULT '.length));
  let bad = 0;
  console.log('');
  for (const [key, why] of Object.entries(REQUIRED)) {
    const r = results[key];
    const ok = r && r.ok;
    if (!ok) bad++;
    const detail = r && r.detail ? ` — ${r.detail}` : '';
    console.log(`${ok ? '✓' : '✗'} ${key}${detail}`);
    if (!ok) console.log(`    needed because: ${why}`);
  }
  for (const key of Object.keys(results)) {
    if (!(key in REQUIRED)) console.log(`· ${key} (not gated)`);
  }

  console.log('');
  if (bad) fail(`${bad} capability check(s) failed — Phase 0 is not clear`);
  console.log('✓ Phase 0 clear: the emulator runtime works over app:// under Electron\n');
});
