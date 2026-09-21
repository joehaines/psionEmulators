// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Phase 1 gate: the borderless window and the bridge.
//
// What this can and cannot prove, stated plainly:
//
//   CAN — the window is frameless and resizable, the renderer reaches the
//   desktop shell rather than a web route, the bridge is exposed with its
//   info populated, setAspectRatio reshapes the window to the machine's
//   ratio, and geometry survives a relaunch.
//
//   CANNOT — the live resize-DRAG constraint. Electron only emits
//   'will-resize' for user-initiated resizes, and no automation can produce
//   a native window-manager drag. The maths behind it is covered by
//   test/aspect.test.mts instead, and the real gesture needs a human on each
//   OS — which is exactly why the plan lists frameless behaviour as a risk
//   that cannot be closed from a Linux container.
//
// Also cannot boot a device: that needs the Emscripten build, which is not
// available here. The renderer therefore sits on its loading screen, which
// is itself the assertion that the desktop shell mounted.
//
//   node test/window.spec.mjs

import { createRequire } from 'node:module';
import { existsSync, rmSync, mkdtempSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const DESKTOP = resolve(HERE, '..');
const MAIN = join(DESKTOP, 'build', 'main', 'main', 'main.js');

// playwright lives in frontend/'s dev dependencies; _electron drives our own
// Electron binary, so there is nothing extra to install here.
const require = createRequire(import.meta.url);
let _electron;
try {
  ({ _electron } = require(join(DESKTOP, '..', 'frontend', 'node_modules', 'playwright')));
} catch {
  console.error('✗ playwright not found — run `npm install` in frontend/ first');
  process.exit(1);
}

const APP_VERSION = JSON.parse(
  require('node:fs').readFileSync(join(DESKTOP, 'package.json'), 'utf8')).version;

let failures = 0;
const check = (cond, msg) => {
  console.log(`${cond ? '✓' : '✗'} ${msg}`);
  if (!cond) failures++;
};
const near = (a, b, tol, msg) => check(Math.abs(a - b) <= tol,
  `${msg} (got ${typeof a === 'number' ? a.toFixed(4) : a}, want ~${b})`);

if (!existsSync(MAIN)) {
  console.error('✗ main not built — run `npm run build:main`');
  process.exit(1);
}
if (!existsSync(join(DESKTOP, 'build', 'renderer', 'index.html'))) {
  console.error('✗ renderer not built — run `npm run build:desktop` in frontend/');
  process.exit(1);
}

// An isolated userData dir, so the test neither reads nor writes the real
// profile — geometry persistence is one of the things under test.
const userData = mkdtempSync(join(tmpdir(), 'psion-desktop-test-'));

// Launch by APP DIRECTORY rather than by script path, so Electron reads
// desktop/package.json and app.getVersion() reports our version instead of
// its own — the same resolution the packaged app uses.
const launchArgs = [DESKTOP, '--no-sandbox', `--user-data-dir=${userData}`];
// playwright resolves `electron` relative to its OWN install, which is
// frontend/node_modules — and Electron lives in desktop/. Point it at ours.
const ELECTRON_BIN = join(DESKTOP, 'node_modules', 'electron', 'dist',
                          process.platform === 'win32' ? 'electron.exe' : 'electron');
if (!existsSync(ELECTRON_BIN)) {
  console.error(`\u2717 Electron binary missing at ${ELECTRON_BIN} — run \`npm install\` in desktop/`);
  process.exit(1);
}
const launchOpts = {
  executablePath: ELECTRON_BIN,
  args: launchArgs,
  env: { ...process.env, ELECTRON_DISABLE_SECURITY_WARNINGS: '1' },
};

async function launch() {
  const app = await _electron.launch(launchOpts);
  const page = await app.firstWindow();
  await page.waitForLoadState('domcontentloaded');
  return { app, page };
}

/** Read main-process facts about the one window. */
function windowFacts(app) {
  return app.evaluate(({ BrowserWindow }) => {
    const w = BrowserWindow.getAllWindows()[0];
    const c = w.getContentBounds();
    return {
      resizable: w.isResizable(),
      // With frame:false the window has no frame inset, so the outer bounds
      // and the content bounds are the same rectangle.
      frameless: w.getBounds().height === c.height && w.getBounds().width === c.width,
      content: c,
    };
  });
}

try {
  // ── Launch, frameless, bridge present ────────────────────────────────
  let { app, page } = await launch();

  const facts = await windowFacts(app);
  check(facts.resizable, 'the window is resizable');
  check(facts.frameless, 'the window has no frame inset (frame: false)');

  const info = await page.evaluate(() => {
    const h = window.psionHost;
    if (!h) return null;
    return {
      hasWindow: typeof h.window?.setAspectRatio === 'function',
      hasDevices: typeof h.devices?.publish === 'function',
      hasState: typeof h.state?.get === 'function',
      hasLifecycle: typeof h.lifecycle?.onPrepareQuit === 'function',
      // Both optional groups have landed. They are asserted by a real method
      // rather than by the object existing, because the point of the grouping
      // is that a renderer can feature-detect a capability and get the truth —
      // an empty object would pass a bare existence check and then fail at the
      // call site.
      hasSaves: typeof h.saves?.write === 'function',
      hasMounts: typeof h.mounts?.pick === 'function'
        && typeof h.mounts?.list === 'function'
        && typeof h.mounts?.onChange === 'function',
      platform: h.info?.platform,
      version: h.info?.appVersion,
      userDataSet: typeof h.info?.userDataDir === 'string' && h.info.userDataDir.length > 0,
    };
  });
  check(info !== null, 'window.psionHost is exposed to the renderer');
  check(info?.hasWindow && info?.hasDevices && info?.hasState && info?.hasLifecycle,
        'the phase-1 capability groups are all present');
  check(info?.hasSaves, 'host.saves is present (phase 2)');
  check(info?.hasMounts, 'host.mounts is present with real methods (phase 3)');
  check(info?.version === APP_VERSION,
        `app version is ours, not Electron's (got ${info?.version}, want ${APP_VERSION})`);
  check(info?.userDataSet, 'userData path reached the renderer');

  // ── The desktop shell mounted, not a web route ───────────────────────
  // No WASM here, so the shell sits on its own loading screen. Its presence
  // (and the absence of the web header) is the assertion.
  const shell = await page.evaluate(() => ({
    text: document.body.innerText.slice(0, 200),
    // The web app renders a header with the device panel button; the
    // chromeless desktop shell renders none.
    headers: document.querySelectorAll('header').length,
  }));
  check(shell.headers === 0, 'no web page header is rendered');
  check(/Starting|Loading|emulator/i.test(shell.text),
        `the shell is on its loading screen (${JSON.stringify(shell.text.trim().slice(0, 60))})`);

  // ── Aspect lock reshapes the window ─────────────────────────────────
  // This is the device-load path: EmulatorView measures the composed
  // content, DesktopApp forwards the ratio, main re-solves the bounds.
  const RATIO = 640 / 240;
  await page.evaluate((r) => window.psionHost.window.setAspectRatio(r), RATIO);
  await page.waitForTimeout(300);
  let c = (await windowFacts(app)).content;
  near(c.width / c.height, RATIO, 0.02, 'the window took the panel ratio');

  // A second machine with a different shape must reshape it again — the
  // device-switch case, where a stale ratio would letterbox the new machine.
  const REVO_RATIO = 480 / 160;
  await page.evaluate((r) => window.psionHost.window.setAspectRatio(r), REVO_RATIO);
  await page.waitForTimeout(300);
  c = (await windowFacts(app)).content;
  near(c.width / c.height, REVO_RATIO, 0.02, 'switching ratio reshapes the window again');

  // Pinned chrome must grow the window, not squeeze the panel.
  await page.evaluate((r) => window.psionHost.window.setAspectRatio(r, { width: 0, height: 40 }),
                      RATIO);
  await page.waitForTimeout(300);
  c = (await windowFacts(app)).content;
  near((c.width) / (c.height - 40), RATIO, 0.03,
       'with 40px of pinned chrome the PANEL keeps the ratio');

  // ── fitToScale gives an exact integer multiple ───────────────────────
  await page.evaluate(() => window.psionHost.window.setAspectRatio(640 / 240));
  await page.evaluate(() => window.psionHost.window.fitToScale(640, 240, 2));
  await page.waitForTimeout(300);
  c = (await windowFacts(app)).content;
  check(c.width === 1280 && c.height === 480,
        `fitToScale(2) sized the window to exactly 2x the panel (got ${c.width}x${c.height})`);

  // ── Geometry persists across a relaunch ─────────────────────────────
  // Recorded against a device id, which is how the real app keys it.
  await page.evaluate(() => window.psionHost.devices.setCurrent('testdev'));
  await page.evaluate(() => window.psionHost.window.setContentSize(900, 338));
  await page.waitForTimeout(200);
  await page.evaluate(() => window.psionHost.window.rememberGeometry('testdev'));
  await page.waitForTimeout(300);
  const saved = (await windowFacts(app)).content;

  await app.close();

  ({ app, page } = await launch());
  await page.evaluate(() => window.psionHost.devices.setCurrent('testdev'));
  await page.evaluate(() => window.psionHost.window.restoreGeometry('testdev'));
  await page.waitForTimeout(400);
  const restored = (await windowFacts(app)).content;
  near(restored.width, saved.width, 2, 'width restored for the device');
  near(restored.height, saved.height, 2, 'height restored for the device');

  // ── The quit handshake is honoured ───────────────────────────────────
  // Main must wait for the renderer's final save instead of exiting under
  // it. A deliberately slow callback proves the deferral is real.
  await page.evaluate(() => {
    window.__quitObserved = false;
    window.psionHost.lifecycle.onPrepareQuit(async () => {
      window.__quitObserved = true;
      await new Promise((r) => setTimeout(r, 1200));
    });
  });
  // Quit from inside the app, which is what Cmd+Q and the window's close
  // button both reach. (playwright's app.close() tears the window down first,
  // so it cannot exercise this path.)
  const startedAt = Date.now();
  await app.evaluate(({ app: electronApp }) => { electronApp.quit(); });
  await app.waitForEvent('close');
  const elapsed = Date.now() - startedAt;
  check(elapsed >= 1000,
        `quit waited for the final save (${elapsed} ms elapsed, callback slept 1200 ms)`);

  // The same again, but triggered by closing the WINDOW rather than quitting
  // the app — the path a user actually takes, and the one that previously
  // destroyed the renderer before it could save.
  ({ app, page } = await launch());
  await page.evaluate(() => {
    window.psionHost.lifecycle.onPrepareQuit(async () => {
      await new Promise((r) => setTimeout(r, 1200));
    });
  });
  const closeStartedAt = Date.now();
  await app.evaluate(({ BrowserWindow }) => { BrowserWindow.getAllWindows()[0].close(); });
  await app.waitForEvent('close');
  const closeElapsed = Date.now() - closeStartedAt;
  check(closeElapsed >= 1000,
        `closing the window also waits for the final save (${closeElapsed} ms elapsed)`);

  console.log('');
  if (failures) {
    console.error(`✗ ${failures} assertion(s) failed`);
    process.exit(1);
  }
  console.log('✓ Phase 1 window gate passed\n');
} finally {
  rmSync(userData, { recursive: true, force: true });
}
