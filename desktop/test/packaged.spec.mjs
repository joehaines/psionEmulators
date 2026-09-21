// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Does the PACKAGED app start?
//
// This exists because of a bug the other gates could not see. chokidar is used
// by the main process at runtime, but it was declared in devDependencies —
// electron-builder ships production dependencies only, so it was absent from
// the app archive, require() threw during startup, and the packaged app opened
// no window at all. Every other test passed throughout: they run the app from
// the source tree, where node_modules has everything.
//
// So this checks the artifact rather than the tree:
//   - every runtime dependency is actually inside the asar
//   - the app opens a window
//   - the bridge is exposed, with each capability group present
//   - the ROM tree resolves from process.resourcesPath, which is a different
//     code path from the dev one (app.isPackaged flips it)
//
// It does NOT need the WASM engine: without psion.js the renderer shows its own
// "engine failed to load" message, which still proves everything up to that
// point works. With the engine present the device boots instead, and the check
// below accepts either.
//
//   npx electron-builder --dir && node test/packaged.spec.mjs

import { createRequire } from 'node:module';
import { execFileSync } from 'node:child_process';
import { existsSync, readdirSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const DESKTOP = resolve(HERE, '..');
const require = createRequire(import.meta.url);
const { _electron } = require(join(DESKTOP, '..', 'frontend', 'node_modules', 'playwright'));

let failures = 0;
const check = (cond, msg) => {
  console.log(`${cond ? '✓' : '✗'} ${msg}`);
  if (!cond) failures++;
};

// Find the unpacked app for whichever platform this is.
const DIST = join(DESKTOP, 'dist');
const candidates = existsSync(DIST)
  ? readdirSync(DIST).filter((d) => d.endsWith('-unpacked') || d.endsWith('.app'))
  : [];
if (!candidates.length) {
  console.error('✗ no unpacked build found — run `npx electron-builder --dir` first');
  process.exit(1);
}
const unpacked = join(DIST, candidates[0]);

const BIN_BY_PLATFORM = {
  linux: join(unpacked, 'psion-emulator-desktop'),
  win32: join(unpacked, 'Psion Emulator.exe'),
  darwin: join(unpacked, 'Contents', 'MacOS', 'Psion Emulator'),
};
const bin = BIN_BY_PLATFORM[process.platform];
if (!bin || !existsSync(bin)) {
  console.error(`✗ packaged binary not found at ${bin}`);
  process.exit(1);
}

// ── Runtime dependencies must be IN the archive ──────────────────────────
// The direct guard on the bug that prompted this file.
const asar = process.platform === 'darwin'
  ? join(unpacked, 'Contents', 'Resources', 'app.asar')
  : join(unpacked, 'resources', 'app.asar');
check(existsSync(asar), 'the app archive exists');

const pkg = require(join(DESKTOP, 'package.json'));
const runtimeDeps = Object.keys(pkg.dependencies ?? {});
check(runtimeDeps.length > 0,
      'package.json declares its runtime dependencies (not all of them as dev)');
if (existsSync(asar)) {
  let listing = '';
  try {
    listing = execFileSync('npx', ['asar', 'list', asar], { encoding: 'utf8' });
  } catch {
    console.log('· could not list the asar (npx asar unavailable) — skipping that check');
  }
  if (listing) {
    for (const dep of runtimeDeps) {
      check(listing.includes(`node_modules/${dep}`),
            `${dep} is inside the app archive, so requiring it at startup works`);
    }
  }
}

// ── The app starts ──────────────────────────────────────────────────────
const app = await _electron.launch({
  executablePath: bin,
  args: ['--no-sandbox'],
  env: { ...process.env, ELECTRON_DISABLE_SECURITY_WARNINGS: '1' },
});

try {
  const page = await app.firstWindow();
  await page.waitForLoadState('domcontentloaded');
  check(true, 'the packaged app opens a window');

  const probe = await page.evaluate(async () => {
    const h = window.psionHost;
    // A ROM proves resourcesPath/roms resolved — a different branch from dev.
    let rom = null;
    try {
      const r = await fetch(new URL('roms/OrganiserII.rom', location.href).href);
      rom = { status: r.status, length: Number(r.headers.get('Content-Length') || 0) };
    } catch (e) { rom = { error: String(e) }; }
    return {
      url: location.href,
      hasBridge: !!h,
      groups: h ? {
        window: typeof h.window?.setAspectRatio === 'function',
        devices: typeof h.devices?.publish === 'function',
        state: typeof h.state?.get === 'function',
        lifecycle: typeof h.lifecycle?.onPrepareQuit === 'function',
        saves: typeof h.saves?.write === 'function',
        mounts: typeof h.mounts?.pick === 'function',
      } : null,
      version: h?.info?.appVersion ?? null,
      rom,
      body: document.body.innerText.slice(0, 160),
    };
  });

  check(probe.url.startsWith('app://psion/psion/'),
        `the renderer is served over the custom scheme (${probe.url})`);
  check(probe.hasBridge, 'the bridge is exposed in the packaged app');
  for (const [group, present] of Object.entries(probe.groups ?? {})) {
    check(present, `host.${group} is present`);
  }
  check(probe.version === pkg.version,
        `the app reports its own version (${probe.version})`);
  check(probe.rom?.status === 200 && probe.rom.length > 0,
        `a ROM serves from resourcesPath with a Content-Length (${JSON.stringify(probe.rom)})`);

  // Either the machine boots, or the renderer says the engine is missing. Both
  // prove main, the scheme, the bridge and the worker path are working; only a
  // blank page or a crash would not.
  const reachedRenderer = /Starting|Loading|engine failed to load|Checking for a saved session/i
    .test(probe.body);
  check(reachedRenderer,
        `the desktop shell rendered (${JSON.stringify(probe.body.trim().slice(0, 80))})`);

  console.log('');
  if (failures) {
    console.error(`✗ ${failures} assertion(s) failed`);
    process.exit(1);
  }
  console.log('✓ the packaged app starts and its bridge is intact\n');
} finally {
  await app.close().catch(() => {});
}
