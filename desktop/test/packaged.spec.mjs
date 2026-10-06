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
import { existsSync, readdirSync, statSync } from 'node:fs';
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
//
// Two layouts, because electron-builder writes one per platform: Windows and
// Linux leave the tree at the top of dist/ (`win-unpacked`, `linux-unpacked`),
// while macOS puts the .app one level down, inside the arch folder it packaged
// into (`mac-arm64/Psion Emulator.app`). Looking only at the top level finds
// nothing on macOS, which reads as "no build" directly after a build.
//
// The arch filter matters on Linux, where one run builds both: a dist/ with
// `linux-arm64-unpacked` beside `linux-unpacked` would otherwise hand the
// arm64 binary to an x64 runner on readdir order alone, and "cannot execute
// binary file" is a confusing way to learn that.
const DIST = join(DESKTOP, 'dist');
const isApp = (name) => name.endsWith('-unpacked') || name.endsWith('.app');

// Every arch electron-builder names a directory after, minus this one. An
// unpacked tree naming any of these is somebody else's build.
const FOREIGN_ARCHES = ['x64', 'arm64', 'armv7l', 'ia32']
  .filter((a) => a !== process.arch)
  .map((a) => `-${a}-`);
const isForeign = (name) => FOREIGN_ARCHES.some((a) => `${name}-`.includes(a));

function findUnpacked() {
  if (!existsSync(DIST)) return null;
  const top = readdirSync(DIST);
  const direct = top.find((n) => isApp(n) && !isForeign(n));
  if (direct) return join(DIST, direct);
  for (const entry of top) {
    if (isForeign(entry)) continue;
    const dir = join(DIST, entry);
    if (!statSync(dir).isDirectory()) continue;
    const nested = readdirSync(dir).find(isApp);
    if (nested) return join(dir, nested);
  }
  return null;
}

const unpacked = findUnpacked();
if (!unpacked) {
  console.error('✗ no unpacked build found — run `npx electron-builder --dir` first');
  process.exit(1);
}
console.log(`· testing ${unpacked}`);

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
  // Narrated, so a failure below says what the window was doing at the time.
  page.on('framenavigated', (f) => {
    if (f === page.mainFrame()) console.log(`· the window navigated to ${f.url()}`);
  });
  page.on('crash', () => console.log('· the renderer crashed'));

  // firstWindow() resolves as soon as the window EXISTS, which can be while it
  // still shows Chromium's initial empty document, before loadURL(START_URL)
  // has committed. A load-state wait there resolves against that document, and
  // the probe then dies with "Execution context was destroyed" when the real
  // page replaces it. So wait for the app's own URL first.
  await page.waitForURL((u) => u.protocol === 'app:',
                         { timeout: 120_000, waitUntil: 'domcontentloaded' });
  check(true, `the packaged app opens a window (${page.url()})`);

  // Retried ONCE if a navigation lands mid-probe — what the Windows runner hit
  // (its first launch of a fresh build is slow enough to stall ~25 s). The
  // retry runs every check again on whatever page is there now, so a window
  // that keeps reloading still fails; the narration above says why.
  const runProbe = () => page.evaluate(async () => {
    const h = window.psionHost;
    // A ROM proves resourcesPath/roms resolved — a different branch from dev.
    let rom = null;
    try {
      const r = await fetch(new URL('roms/OrganiserII/OrganiserII_LZ64_eng/OrganiserII.rom', location.href).href);
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
  let probe;
  try {
    probe = await runProbe();
  } catch (e) {
    if (!/Execution context was destroyed/.test(String(e))) throw e;
    console.log('· a navigation interrupted the probe — waiting for the page and retrying once');
    // The rejection can arrive before Playwright has seen the new document,
    // when a load-state wait would still resolve against the old one. So wait
    // for the navigation itself (bounded: it may already have been reported).
    await page.waitForEvent('framenavigated', { timeout: 10_000 }).catch(() => {});
    await page.waitForLoadState('domcontentloaded');
    probe = await runProbe();
  }

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

  // Either the machine boots, or the renderer says the engine is missing, or —
  // on the fresh profile a packaged run always has — it offers the device
  // picker. All three prove main, the scheme, the bridge and the worker path
  // are working; only a blank page or a crash would not.
  const reachedRenderer =
    /Starting|Loading|Choose a device|engine failed to load|Checking for a saved session/i
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
