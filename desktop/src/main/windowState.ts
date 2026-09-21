// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Window geometry that survives a restart, remembered per device.
//
// Per device, not globally, because every machine has a different aspect:
// restoring a netpad's 910x360 onto a Revo would look broken, and the user
// who sized their Series 5 just so does not want it undone by having
// glanced at an Organiser II.
//
// Everything here validates against the CURRENT displays before trusting
// it. A saved rectangle is a statement about a monitor arrangement that may
// no longer exist — an unplugged second screen is the usual way a window
// restores itself somewhere the user cannot reach it.

import { screen, type BrowserWindow, type Rectangle } from 'electron';
import * as settings from './settings';

const KEY = 'windowState';
/** How much of the window must be on a work area for the position to be usable. */
const MIN_VISIBLE_PX = 64;

interface Size { width: number; height: number }
interface Stored {
  last?: Rectangle & { maximized?: boolean };
  perDevice?: Record<string, Size>;
}

function read(): Stored {
  return settings.get<Stored>(KEY) ?? {};
}

/**
 * True when enough of `rect` overlaps some display's work area to be
 * grabbable. Checked against work areas rather than full bounds so a window
 * restored entirely behind the macOS menu bar or a Windows taskbar counts
 * as lost.
 */
export function isReachable(rect: Rectangle): boolean {
  return screen.getAllDisplays().some((d) => {
    const w = d.workArea;
    const overlapX = Math.min(rect.x + rect.width, w.x + w.width) - Math.max(rect.x, w.x);
    const overlapY = Math.min(rect.y + rect.height, w.y + w.height) - Math.max(rect.y, w.y);
    return overlapX >= MIN_VISIBLE_PX && overlapY >= MIN_VISIBLE_PX;
  });
}

/**
 * The position to open at, or null to let Electron centre the window.
 * Only ever returns a position — the SIZE comes from the device, because
 * the renderer will immediately reshape the window to the machine's aspect
 * anyway and opening at the wrong shape produces a visible jump.
 */
export function initialPosition(): { x: number; y: number } | null {
  const last = read().last;
  if (!last) return null;
  if (!isReachable(last)) return null;
  return { x: last.x, y: last.y };
}

/** The remembered content size for a device, if we have one. */
export function sizeForDevice(deviceId: string): Size | null {
  const sizes = read().perDevice ?? {};
  const s = sizes[deviceId];
  if (!s || !(s.width > 0) || !(s.height > 0)) return null;
  return s;
}

export function rememberPosition(win: BrowserWindow): Promise<void> {
  if (win.isDestroyed()) return Promise.resolve();
  // A maximized or full-screen window's bounds are the screen's, not the
  // user's chosen size, so recording them would lose their real geometry.
  if (win.isMaximized() || win.isFullScreen() || win.isMinimized()) return Promise.resolve();
  const stored = read();
  stored.last = { ...win.getContentBounds(), maximized: false };
  return settings.set(KEY, stored);
}

export function rememberSizeForDevice(win: BrowserWindow, deviceId: string): Promise<void> {
  if (win.isDestroyed() || !deviceId) return Promise.resolve();
  if (win.isMaximized() || win.isFullScreen() || win.isMinimized()) return Promise.resolve();
  const stored = read();
  const { width, height } = win.getContentBounds();
  stored.perDevice = { ...(stored.perDevice ?? {}), [deviceId]: { width, height } };
  stored.last = { ...win.getContentBounds(), maximized: false };
  return settings.set(KEY, stored);
}

/**
 * Apply a device's remembered size, keeping the window where it is and
 * refusing a size that no longer fits any display (a window sized on a 4K
 * monitor, reopened on a laptop).
 */
export function restoreSizeForDevice(win: BrowserWindow, deviceId: string): boolean {
  const size = sizeForDevice(deviceId);
  if (!size || win.isDestroyed()) return false;
  if (win.isMaximized() || win.isFullScreen()) return false;
  const current = win.getContentBounds();
  const work = screen.getDisplayNearestPoint({ x: current.x, y: current.y }).workArea;
  if (size.width > work.width || size.height > work.height) return false;
  win.setContentBounds({ x: current.x, y: current.y, ...size });
  return true;
}

/**
 * Debounce geometry writes: a drag emits a resize event per frame, and
 * settings.json does not need to be rewritten sixty times a second.
 */
export function watch(win: BrowserWindow, currentDeviceId: () => string | null): void {
  let timer: NodeJS.Timeout | null = null;
  const save = () => {
    if (timer) clearTimeout(timer);
    timer = setTimeout(() => {
      timer = null;
      const id = currentDeviceId();
      void (id ? rememberSizeForDevice(win, id) : rememberPosition(win));
    }, 400);
  };
  win.on('resize', save);
  win.on('move', save);
  win.on('close', () => {
    // Synchronous-ish final write: cancel the pending debounce and take the
    // geometry now, before the window is gone and getContentBounds throws.
    if (timer) { clearTimeout(timer); timer = null; }
    const id = currentDeviceId();
    void (id ? rememberSizeForDevice(win, id) : rememberPosition(win));
  });
}
