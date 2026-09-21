// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Electron main process for the Psion desktop app.
//
// Orchestration only: the window lives in window.ts, the app:// scheme in
// protocol.ts, the handlers in ipc.ts, the menu in menu.ts. What is left
// here is lifecycle — and the one genuinely delicate piece of it, the quit
// handshake.

import { app, BrowserWindow, powerMonitor, session } from 'electron';
import { CHANNELS } from '../ipc/contract';
import { installHandler, registerScheme, resolveRoots } from './protocol';
import * as ipc from './ipc';
import * as menu from './menu';
import * as mounts from './mounts';
import * as saves from './saves';
import * as settings from './settings';
import * as win from './window';

// Must run at module scope: registerSchemesAsPrivileged is only honoured
// before the app is ready.
registerScheme();

const DEV = process.argv.includes('--dev');
const FRAMED = process.argv.includes('--framed');

/**
 * How long the renderer gets to finish its final save before we exit
 * anyway. A post-boot heap gzips to single-digit megabytes, so this is
 * generous — but it is a watchdog, not a budget: an app that will not quit
 * is worse than one that loses the last minute of a session.
 */
const QUIT_SAVE_BUDGET_MS = 25_000;

let quitting = false;
let quitAllowed = false;
let quitTimer: NodeJS.Timeout | null = null;
let busyReason: string | null = null;

function forceQuit(): void {
  if (quitTimer) { clearTimeout(quitTimer); quitTimer = null; }
  quitAllowed = true;
  // Close the folder watchers and let the settings write that the save path
  // just queued reach disk.
  void mounts.shutdown()
    .catch(() => undefined)
    .then(() => settings.settled())
    .finally(() => app.exit(0));
}

/**
 * Quit is deferred, not cancelled: the renderer owns the heap and is the
 * only thing that can snapshot it, so main asks it to save and waits.
 * Without this the process would exit while a multi-megabyte gzip was still
 * in flight and the session would be lost.
 */
function beginQuit(event: Electron.Event): void {
  if (quitAllowed) return;
  event.preventDefault();
  // A second Cmd+Q while the save is already running must not restart it.
  if (quitting) return;
  quitting = true;

  const w = win.getWindow();
  if (!w) { forceQuit(); return; }

  w.webContents.send(CHANNELS.prepareQuit);
  quitTimer = setTimeout(() => {
    console.warn(`[psion] final save exceeded ${QUIT_SAVE_BUDGET_MS} ms — exiting anyway`);
    forceQuit();
  }, QUIT_SAVE_BUDGET_MS);
}

/**
 * Keep the window alive long enough to save.
 *
 * Without this, closing the window destroys the renderer, and only then does
 * window-all-closed reach app.quit() — so before-quit finds no window to ask
 * for a final save and exits immediately. Routing the close through app.quit()
 * instead means the ONE quit path always has a live renderer to talk to.
 */
function holdOpenUntilSaved(w: BrowserWindow): void {
  w.on('close', (event) => {
    if (quitAllowed) return;
    event.preventDefault();
    app.quit();
  });
}

if (!app.requestSingleInstanceLock()) {
  // Two processes autosaving the same device would race each other's
  // generation files, so a second instance hands focus over and leaves.
  app.quit();
} else {
  app.on('second-instance', () => {
    const w = win.getWindow();
    if (!w) return;
    if (w.isMinimized()) w.restore();
    w.focus();
  });

  void app.whenReady().then(() => {
    installHandler(resolveRoots());

    // The emulator needs no web permissions beyond reading the clipboard for
    // paste-into-device. Deny the rest outright rather than trusting that
    // nothing ever asks.
    session.defaultSession.setPermissionRequestHandler((_wc, permission, callback) => {
      callback(permission === 'clipboard-read');
    });

    ipc.registerSaves();
    ipc.registerMounts();
    // Clear temp files from an interrupted write and re-derive each manifest
    // from its directory, so a crashed session does not leave the store
    // offering a restore that cannot happen.
    void saves.sweep().catch((err) => console.warn('[psion] save sweep failed:', err));

    ipc.registerCore({
      onDevicesPublished: (list) => menu.setDevices(list),
      onCurrentDeviceChanged: (id) => menu.setCurrentDevice(id),
      onQuitReady: () => { if (quitting) forceQuit(); },
      onBusyChanged: (reason) => { busyReason = reason; },
    });

    const w = win.createMainWindow({ framed: FRAMED, dev: DEV });
    ipc.forwardWindowState(w);
    menu.attach(w);
    holdOpenUntilSaved(w);

    // Re-attach whatever folders the user chose last time, and route their
    // change events to whichever window is current.
    mounts.restore((id) => {
      const live = win.getWindow();
      if (live) ipc.notifyMountChanged(live, id);
    });

    app.on('activate', () => {
      if (BrowserWindow.getAllWindows().length === 0) {
        const next = win.createMainWindow({ framed: FRAMED, dev: DEV });
        ipc.forwardWindowState(next);
        menu.attach(next);
        holdOpenUntilSaved(next);
      }
    });
  });

  app.on('before-quit', beginQuit);

  app.on('window-all-closed', () => {
    // Single-window app, so closing the window means quitting — on macOS
    // too. The close itself already routed through before-quit.
    if (!quitting) app.quit();
  });

  // Sleep and shutdown get the same treatment as a quit: Windows gives a
  // short window before it kills the process, and the watchdog keeps us
  // from hanging a logoff. Subscribed after ready — powerMonitor throws if
  // touched before it.
  void app.whenReady().then(() => {
    powerMonitor.on('suspend', () => {
      const w = win.getWindow();
      if (w && !quitting) w.webContents.send(CHANNELS.prepareQuit);
    });
    powerMonitor.on('shutdown', (event?: Electron.Event) => {
      if (event) beginQuit(event);
    });
  });
}

/** Exposed for the smoke test, which asserts the app reports its busy state. */
export function currentBusyReason(): string | null {
  return busyReason;
}
