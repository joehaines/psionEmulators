// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The one window: frameless, resizable, and locked to the aspect of
// whichever machine is running.
//
// The aspect ratio is driven from the RENDERER, not computed here, because
// only EmulatorView knows the composed footprint — the device frame's own
// aspect, plus the SIBO app-button bar that sits below it, plus the
// transpose when the netpad is turned a quarter turn. Duplicating that
// arithmetic in the main process would guarantee it drifts.

import { BrowserWindow, screen, shell, app } from 'electron';
import * as path from 'node:path';
import { constrainToRatio, largestIntegerScale, type Extra, type ResizeEdge } from './aspect';
import { SCHEME, HOST, START_URL } from './protocol';
import * as windowState from './windowState';

const MIN_WIDTH = 240;
const MIN_HEIGHT = 120;

let win: BrowserWindow | null = null;
let ratio = 0;
let extra: Extra = { width: 0, height: 0 };
let currentDeviceId: string | null = null;
let dragTimer: NodeJS.Timeout | null = null;

export function getWindow(): BrowserWindow | null {
  return win && !win.isDestroyed() ? win : null;
}

export function setCurrentDeviceId(id: string | null): void {
  currentDeviceId = id;
}

export function getCurrentDeviceId(): string | null {
  return currentDeviceId;
}

export interface CreateOptions {
  /** Open with an OS title bar. Debug aid — a framed window is far easier
   *  to reason about when the question is "why is the page blank". */
  framed: boolean;
  dev: boolean;
}

export function createMainWindow(opts: CreateOptions): BrowserWindow {
  const pos = windowState.initialPosition();

  win = new BrowserWindow({
    ...(pos ?? {}),
    width: 900,
    height: 560,
    minWidth: MIN_WIDTH,
    minHeight: MIN_HEIGHT,
    // Bounds mean content, so the aspect maths never has to subtract a
    // frame it cannot measure.
    useContentSize: true,
    frame: opts.framed,
    titleBarStyle: opts.framed ? 'default' : 'hidden',
    // Windows: with frame:false this is what keeps the WM_NCHITTEST resize
    // border alive, so the edges stay grabbable.
    thickFrame: true,
    roundedCorners: true,
    // psion-mid. A frameless window paints its background during a resize,
    // and white flashing behind the device looks broken.
    backgroundColor: '#c9c9bd',
    show: false,
    webPreferences: {
      preload: path.join(__dirname, '..', 'preload', 'preload.js'),
      contextIsolation: true,
      sandbox: true,
      nodeIntegration: false,
      nodeIntegrationInWorker: false,
      webSecurity: true,
      // Chromium throttles rAF and timers in unfocused windows. That would
      // slow the guest AND clamp the PLP poll loop to about 1 Hz, which
      // kills a live Remote Link session mid-transfer.
      backgroundThrottling: false,
      additionalArguments: [
        `--psion-version=${app.getVersion()}`,
        `--psion-userdata=${app.getPath('userData')}`,
        ...(opts.dev ? ['--psion-dev'] : []),
      ],
    },
  });

  win.once('ready-to-show', () => win?.show());

  // ── The aspect fallback ────────────────────────────────────────────────
  // setAspectRatio's extraSize is documented macOS-only and its Windows
  // behaviour has been unreliable, so the ratio is enforced during the drag
  // too. Doing both is deliberate: this path also gives a better feel on
  // Windows, where it constrains live rather than snapping on release.
  win.on('will-resize', (event, newBounds, details) => {
    if (!ratio || !win) return;
    const edge = (details?.edge ?? 'unknown') as ResizeEdge;
    const want = constrainToRatio(newBounds, edge, ratio, extra,
                                  { minWidth: MIN_WIDTH, minHeight: MIN_HEIGHT });
    if (want.width === newBounds.width && want.height === newBounds.height) return;
    event.preventDefault();
    win.setContentBounds(want);
  });

  // ── Navigation lockdown ────────────────────────────────────────────────
  win.webContents.setWindowOpenHandler(({ url }) => {
    try {
      if (/^https?:$/.test(new URL(url).protocol)) void shell.openExternal(url);
    } catch { /* an unparseable URL is simply not opened */ }
    return { action: 'deny' };
  });
  win.webContents.on('will-navigate', (event, url) => {
    try {
      const u = new URL(url);
      if (u.protocol !== `${SCHEME}:` || u.hostname !== HOST) event.preventDefault();
    } catch { event.preventDefault(); }
  });

  // useEmulator installs a beforeunload confirm to catch an accidental tab
  // close. In a desktop app that dialog would block our own quit handshake,
  // and overriding it here means the frontend needs no edit for it.
  win.webContents.on('will-prevent-unload', (event) => event.preventDefault());

  windowState.watch(win, () => currentDeviceId);

  void win.loadURL(START_URL);
  if (opts.dev) win.webContents.openDevTools({ mode: 'detach' });
  return win;
}

// ── Geometry, driven from the renderer ───────────────────────────────────

/**
 * Lock the window to `nextRatio` (emulated panel width/height), excluding
 * `nextExtra` of non-scaling chrome. Reshapes the window immediately so a
 * device switch does not leave the previous machine's letterboxing behind.
 */
export function setAspect(nextRatio: number, nextExtra?: Extra): void {
  const w = getWindow();
  ratio = Number.isFinite(nextRatio) && nextRatio > 0 ? nextRatio : 0;
  extra = {
    width: Math.max(0, Math.round(nextExtra?.width ?? 0)),
    height: Math.max(0, Math.round(nextExtra?.height ?? 0)),
  };
  if (!w) return;

  w.setAspectRatio(ratio, extra);
  if (!ratio || w.isMaximized() || w.isFullScreen()) return;

  // Re-solve the current size against the new ratio, driving from width.
  const b = w.getContentBounds();
  const next = constrainToRatio(b, 'bottom-right', ratio, extra,
                               { minWidth: MIN_WIDTH, minHeight: MIN_HEIGHT });
  if (next.width !== b.width || next.height !== b.height) w.setContentBounds(next);
}

export function setContentSize(width: number, height: number): void {
  const w = getWindow();
  if (!w || w.isMaximized() || w.isFullScreen()) return;
  if (!(width > 0) || !(height > 0)) return;
  const b = w.getContentBounds();
  w.setContentBounds({
    x: b.x, y: b.y,
    width: Math.max(MIN_WIDTH, Math.round(width)),
    height: Math.max(MIN_HEIGHT, Math.round(height)),
  });
}

/**
 * Size the window to an exact integer multiple of the emulated panel, so
 * one device pixel maps to N screen pixels and the LCD stays crisp. Clamps
 * to what the current display can actually show.
 */
export function fitToScale(lcdWidth: number, lcdHeight: number, scale: number): void {
  const w = getWindow();
  if (!w || !(lcdWidth > 0) || !(lcdHeight > 0)) return;
  const b = w.getContentBounds();
  const work = screen.getDisplayNearestPoint({ x: b.x, y: b.y }).workArea;
  const cap = largestIntegerScale({ width: lcdWidth, height: lcdHeight }, work, extra);
  const use = Math.max(1, Math.min(Math.round(scale) || 1, cap));
  setContentSize(lcdWidth * use + extra.width, lcdHeight * use + extra.height);
}

export function rememberGeometry(deviceId: string): Promise<void> {
  const w = getWindow();
  return w ? windowState.rememberSizeForDevice(w, deviceId) : Promise.resolve();
}

export function restoreGeometry(deviceId: string): void {
  const w = getWindow();
  if (w) windowState.restoreSizeForDevice(w, deviceId);
}

// ── Manual drag fallback ─────────────────────────────────────────────────
//
// The renderer's primary drag is -webkit-app-region, which hands the
// gesture to the OS. This is the escape hatch for a platform where that
// proves unreliable: poll the OS cursor and move the window by the delta.
// 60 Hz is smooth enough and costs nothing next to the emulator.

export function startWindowDrag(): void {
  const w = getWindow();
  if (!w || dragTimer) return;
  let last = screen.getCursorScreenPoint();
  dragTimer = setInterval(() => {
    const cur = getWindow();
    if (!cur) { endWindowDrag(); return; }
    const p = screen.getCursorScreenPoint();
    const dx = p.x - last.x;
    const dy = p.y - last.y;
    if (dx || dy) {
      const b = cur.getBounds();
      cur.setBounds({ ...b, x: b.x + dx, y: b.y + dy });
      last = p;
    }
  }, 16);
}

export function endWindowDrag(): void {
  if (dragTimer) { clearInterval(dragTimer); dragTimer = null; }
}
