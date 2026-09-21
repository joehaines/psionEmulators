// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Electron main process for the Phase 0 capability probe.
//
// Deliberately separate from src/main/main.ts so the production entry point
// carries no test-only branches — but it REUSES the real compiled protocol
// handler, because that handler is the thing under test. If the probe
// passes, the app:// scheme genuinely supports everything the emulator's
// runtime needs.

const { app, BrowserWindow, session } = require('electron');
const path = require('node:path');
const proto = require('../build/main/main/protocol.js');

proto.registerScheme();

const romName = process.env.PSION_PROBE_ROM || '';
let printed = false;

app.whenReady().then(() => {
  proto.installHandler(proto.resolveRoots());
  session.defaultSession.setPermissionRequestHandler((_wc, p, cb) => cb(p === 'clipboard-read'));

  const win = new BrowserWindow({
    width: 400, height: 300, show: false,
    webPreferences: { contextIsolation: true, sandbox: true, nodeIntegration: false,
                      backgroundThrottling: false },
  });

  // The probe reports through console, so main needs no bridge for it.
  win.webContents.on('did-fail-load', (_e, code, desc, url) => {
    process.stdout.write(`PSION_PROBE_DIAG did-fail-load ${code} ${desc} ${url}\n`);
  });
  win.webContents.on('preload-error', (_e, p, err) => {
    process.stdout.write(`PSION_PROBE_DIAG preload-error ${p} ${err}\n`);
  });

  win.webContents.on('console-message', (...args) => {
    if (process.env.PSION_PROBE_VERBOSE) {
      try {
        const a0 = args[0];
        const m = (typeof a0 === 'object' && a0 && 'message' in a0) ? a0.message : args[2];
        process.stdout.write(`PSION_PROBE_DIAG console :: ${String(m).slice(0, 600)}\n`);
      } catch { /* a console message we can't read is not worth failing over */ }
    }
    // Electron 44 passes a single event object; older builds pass
    // (event, level, message). Accept both so this is not version-locked.
    const message = typeof args[0] === 'object' && args[0] && 'message' in args[0]
      ? args[0].message
      : args[2];
    if (typeof message === 'string' && message.startsWith('PSION_PROBE_RESULT ')) {
      printed = true;
      process.stdout.write(message + '\n');
      setTimeout(() => app.exit(0), 50);
    }
  });

  win.webContents.on('render-process-gone', (_e, d) => {
    process.stdout.write(`PSION_PROBE_FATAL renderer gone: ${JSON.stringify(d)}\n`);
    app.exit(3);
  });

  const url = `${proto.START_URL.replace(/index\.html$/, 'probe.html')}?rom=${encodeURIComponent(romName)}`;
  win.loadURL(url).catch((err) => {
    process.stdout.write(`PSION_PROBE_FATAL loadURL: ${String(err)}\n`);
    app.exit(4);
  });

  setTimeout(() => {
    if (!printed) { process.stdout.write('PSION_PROBE_FATAL timeout\n'); app.exit(5); }
  }, 45_000);
});

// Never let a stray window keep the probe alive.
app.on('window-all-closed', () => app.exit(printed ? 0 : 6));
