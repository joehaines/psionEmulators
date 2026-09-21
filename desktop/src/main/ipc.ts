// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Every ipcMain handler, in one place.
//
// Phase 1 registers the window, device, small-state and lifecycle groups.
// The mount and save groups are registered by their own phases; the preload
// only exposes a group once its handlers exist, so `host.mounts` being
// undefined in the renderer is the truth rather than a method that rejects.

import { BrowserWindow, ipcMain, Notification } from 'electron';
import { CHANNELS, type AspectExtra, type DeviceMenuEntry } from '../ipc/contract';
import * as hostfs from './hostfs';
import * as mounts from './mounts';
import * as saves from './saves';
import * as settings from './settings';
import * as win from './window';

/** Set by main.ts so the menu can rebuild when the renderer publishes. */
export interface Hooks {
  onDevicesPublished(list: DeviceMenuEntry[]): void;
  onCurrentDeviceChanged(id: string | null): void;
  onQuitReady(): void;
  onBusyChanged(reason: string | null): void;
}

/**
 * Keys the renderer may read and write through the small-state channel.
 *
 * An allowlist rather than free-form access: settings.json also holds the
 * mount roots and the window geometry, and the renderer has no business
 * rewriting either. Sync maps and view preferences are namespaced under
 * `ui.` and `sync.` so the list stays a prefix check rather than a growing
 * enumeration.
 */
const STATE_PREFIXES = ['ui.', 'sync.', 'mount.'] as const;

function stateKeyAllowed(key: unknown): key is string {
  return typeof key === 'string'
    && key.length > 0 && key.length <= 200
    && STATE_PREFIXES.some((p) => key.startsWith(p));
}

function asExtra(v: unknown): AspectExtra | undefined {
  if (!v || typeof v !== 'object') return undefined;
  const o = v as Partial<AspectExtra>;
  const width = Number(o.width);
  const height = Number(o.height);
  if (!Number.isFinite(width) || !Number.isFinite(height)) return undefined;
  return { width, height };
}

function num(v: unknown): number {
  const n = Number(v);
  return Number.isFinite(n) ? n : 0;
}

export function registerCore(hooks: Hooks): void {
  // ── window geometry ────────────────────────────────────────────────────
  ipcMain.handle(CHANNELS.windowSetAspect, (_e, ratio: unknown, extra: unknown) => {
    win.setAspect(num(ratio), asExtra(extra));
  });
  ipcMain.handle(CHANNELS.windowSetContent, (_e, w: unknown, h: unknown) => {
    win.setContentSize(num(w), num(h));
  });
  ipcMain.handle(CHANNELS.windowFitToScale, (_e, w: unknown, h: unknown, scale: unknown) => {
    win.fitToScale(num(w), num(h), num(scale));
  });
  ipcMain.handle(CHANNELS.windowRemember, async (_e, deviceId: unknown) => {
    if (typeof deviceId === 'string' && deviceId) await win.rememberGeometry(deviceId);
  });
  ipcMain.handle(CHANNELS.windowRestore, (_e, deviceId: unknown) => {
    if (typeof deviceId === 'string' && deviceId) win.restoreGeometry(deviceId);
  });

  ipcMain.on(CHANNELS.windowMinimize, () => win.getWindow()?.minimize());
  ipcMain.on(CHANNELS.windowMaximize, () => {
    const w = win.getWindow();
    if (!w) return;
    // Unmaximizing restores the pre-maximize bounds, which the aspect lock
    // already vetted, so there is nothing to re-solve here.
    if (w.isMaximized()) w.unmaximize(); else w.maximize();
  });
  ipcMain.on(CHANNELS.windowClose, () => win.getWindow()?.close());
  ipcMain.on(CHANNELS.windowStartDrag, () => win.startWindowDrag());
  ipcMain.on(CHANNELS.windowEndDrag, () => win.endWindowDrag());

  // ── devices ────────────────────────────────────────────────────────────
  ipcMain.on(CHANNELS.devicesPublish, (_e, list: unknown) => {
    if (!Array.isArray(list)) return;
    const clean: DeviceMenuEntry[] = [];
    for (const raw of list) {
      if (!raw || typeof raw !== 'object') continue;
      const r = raw as Partial<DeviceMenuEntry>;
      if (typeof r.id !== 'string' || !r.id) continue;
      clean.push({
        id: r.id,
        name: typeof r.name === 'string' ? r.name : r.id,
        supported: r.supported !== false,
        hidden: r.hidden === true,
      });
    }
    hooks.onDevicesPublished(clean);
  });
  ipcMain.on(CHANNELS.deviceCurrent, (_e, id: unknown) => {
    const deviceId = typeof id === 'string' && id ? id : null;
    win.setCurrentDeviceId(deviceId);
    hooks.onCurrentDeviceChanged(deviceId);
  });

  // ── small persisted state ──────────────────────────────────────────────
  ipcMain.handle(CHANNELS.stateGet, (_e, key: unknown) => {
    if (!stateKeyAllowed(key)) return null;
    return settings.get(key);
  });
  ipcMain.handle(CHANNELS.stateSet, async (_e, key: unknown, value: unknown) => {
    if (!stateKeyAllowed(key)) {
      throw new Error(`state key not writable from the renderer: ${String(key)}`);
    }
    await settings.set(key, value);
  });

  // ── lifecycle ──────────────────────────────────────────────────────────
  ipcMain.on(CHANNELS.quitReady, () => hooks.onQuitReady());
  ipcMain.on(CHANNELS.setBusy, (_e, reason: unknown) => {
    hooks.onBusyChanged(typeof reason === 'string' && reason ? reason : null);
  });
  ipcMain.on(CHANNELS.notify, (_e, title: unknown, body: unknown) => {
    if (!Notification.isSupported()) return;
    new Notification({
      title: typeof title === 'string' ? title : 'Psion Emulator',
      body: typeof body === 'string' ? body : '',
    }).show();
  });
}

/**
 * Phase 2: save generations on disk.
 *
 * Registered separately from registerCore so the preload can expose the
 * `saves` group only when these handlers exist — a renderer that feature-
 * detects `host.saves` gets the truth rather than a method that rejects.
 */
export function registerSaves(): void {
  ipcMain.handle(CHANNELS.saveWrite, (_e, deviceId, bundle, meta) =>
    saves.write(deviceId, bundle, meta));
  ipcMain.handle(CHANNELS.saveList, (_e, deviceId) => saves.list(deviceId));
  ipcMain.handle(CHANNELS.saveRead, (_e, deviceId, file) => saves.read(deviceId, file));
  ipcMain.handle(CHANNELS.savePrune, (_e, deviceId, files) => saves.prune(deviceId, files));
}

/**
 * Phases 3 and 4: host folder mounts.
 *
 * Every handler resolves its mount id first and hands the relative path to
 * hostfs, which is the only code that touches the filesystem and the only
 * place a path is validated. Nothing here builds a path itself.
 */
export function registerMounts(): void {
  ipcMain.handle(CHANNELS.mountsGet, () => mounts.snapshot());
  ipcMain.handle(CHANNELS.mountsPick, (_e, id) => mounts.pick(hostfs.assertMountId(id)));
  ipcMain.handle(CHANNELS.mountsClear, (_e, id) => mounts.clear(hostfs.assertMountId(id)));
  ipcMain.handle(CHANNELS.mountsSetEnabled, (_e, id, on) =>
    mounts.setEnabled(hostfs.assertMountId(id), on === true));
  ipcMain.handle(CHANNELS.mountsReveal, (_e, id, rel) =>
    mounts.reveal(hostfs.assertMountId(id),
                  typeof rel === 'string' && rel ? rel : undefined));

  ipcMain.handle(CHANNELS.fsList, (_e, id) => hostfs.list(hostfs.assertMountId(id)));
  ipcMain.handle(CHANNELS.fsRead, (_e, id, rel) =>
    hostfs.read(hostfs.assertMountId(id), rel));
  ipcMain.handle(CHANNELS.fsWrite, (_e, id, rel, data, mtimeMs) =>
    hostfs.write(hostfs.assertMountId(id), rel, data, mtimeMs));
  ipcMain.handle(CHANNELS.fsDelete, (_e, id, rel) =>
    hostfs.remove(hostfs.assertMountId(id), rel));
  ipcMain.handle(CHANNELS.fsMkdirp, (_e, id, rel) =>
    hostfs.mkdirp(hostfs.assertMountId(id), rel));
}

/** Tell the renderer a mount's contents changed, so it can re-list. */
export function notifyMountChanged(w: BrowserWindow, id: string): void {
  if (!w.isDestroyed()) w.webContents.send(CHANNELS.fsChanged, id);
}

/** Push window state to the renderer so its chrome can track maximize/focus. */
export function forwardWindowState(w: BrowserWindow): void {
  const send = () => {
    if (w.isDestroyed()) return;
    w.webContents.send(CHANNELS.windowState, {
      maximized: w.isMaximized(),
      focused: w.isFocused(),
      fullScreen: w.isFullScreen(),
    });
  };
  // Listed individually rather than looped: BrowserWindow.on is overloaded
  // per event name, so a union loses the overload resolution.
  w.on('maximize', send);
  w.on('unmaximize', send);
  w.on('focus', send);
  w.on('blur', send);
  w.on('enter-full-screen', send);
  w.on('leave-full-screen', send);
  w.webContents.on('did-finish-load', send);
}
