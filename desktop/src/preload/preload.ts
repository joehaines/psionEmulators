// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The only bridge between the renderer and the host.
//
// This runs in a SANDBOXED preload: it may use contextBridge and
// ipcRenderer and nothing else. There is deliberately no require('fs') and
// no require('path') here — every filesystem operation lives in
// main/hostfs.ts, addressed by (mountId, relativePath), so the renderer can
// neither see nor name an absolute path.
//
// Capability groups are exposed only once their main-side handlers exist.
// `host.mounts === undefined` in the renderer therefore means the feature
// genuinely is not there, which is a better contract than a method that
// rejects when called.

import { contextBridge, ipcRenderer, type IpcRendererEvent } from 'electron';
import {
  CHANNELS,
  type AspectExtra, type DeviceMenuEntry, type HostCommand,
  type HostDirent, type MountId, type MountInfo,
  type PsionHost, type PsionHostInfo, type SaveGeneration, type WindowStateEvent,
} from '../ipc/contract';

// Passed via additionalArguments at window creation so `info` is available
// to the renderer synchronously, rather than behind an await before its
// first render.
function readArg(name: string): string {
  const prefix = `--psion-${name}=`;
  const found = process.argv.find((a) => a.startsWith(prefix));
  return found ? found.slice(prefix.length) : '';
}

const info: PsionHostInfo = {
  platform: process.platform as PsionHostInfo['platform'],
  appVersion: readArg('version'),
  userDataDir: readArg('userdata'),
  dev: process.argv.includes('--psion-dev'),
};

/**
 * Subscribe to a main→renderer channel, returning an unsubscribe.
 *
 * The listener is wrapped so the raw IpcRendererEvent — which carries a
 * `sender` the renderer has no business touching — never reaches caller
 * code. Only the payload crosses.
 */
function subscribe<A extends unknown[]>(
  channel: string,
  cb: (...args: A) => void,
): () => void {
  const handler = (_e: IpcRendererEvent, ...args: unknown[]) => cb(...(args as A));
  ipcRenderer.on(channel, handler);
  return () => { ipcRenderer.off(channel, handler); };
}

// ── The quit handshake ───────────────────────────────────────────────────
//
// Several parts of the renderer want a say before the app exits — the shell's
// final save, and later the drive sync's read-back. Main blocks on a single
// `quitReady`, so these MUST be aggregated: if each subscriber reported for
// itself, the fastest one would release the quit while the others were still
// working, and the slow one (the one that matters — a multi-megabyte save)
// would be cut off. That is exactly the bug the window gate caught.
const prepareQuitCallbacks = new Set<() => Promise<void>>();
let prepareQuitWired = false;
let prepareQuitRunning = false;

function registerPrepareQuit(cb: () => Promise<void>): () => void {
  prepareQuitCallbacks.add(cb);

  if (!prepareQuitWired) {
    prepareQuitWired = true;
    ipcRenderer.on(CHANNELS.prepareQuit, () => {
      // A second quit request while the first is still saving must not start
      // the work again; main's watchdog covers a callback that never settles.
      if (prepareQuitRunning) return;
      prepareQuitRunning = true;
      void (async () => {
        // allSettled, not all: one subscriber throwing must not strand the
        // others or leave the app in a state where quit does nothing and the
        // user has to kill it.
        const results = await Promise.allSettled(
          [...prepareQuitCallbacks].map((fn) => fn()));
        for (const r of results) {
          if (r.status === 'rejected') {
            console.error('[psion] a pre-quit task failed:', r.reason);
          }
        }
        ipcRenderer.send(CHANNELS.quitReady);
      })();
    });
  }

  return () => { prepareQuitCallbacks.delete(cb); };
}

const host: PsionHost = {
  info,

  window: {
    setAspectRatio: (ratio: number, extra?: AspectExtra) =>
      ipcRenderer.invoke(CHANNELS.windowSetAspect, ratio, extra),
    setContentSize: (width: number, height: number) =>
      ipcRenderer.invoke(CHANNELS.windowSetContent, width, height),
    fitToScale: (lcdWidth: number, lcdHeight: number, scale: number) =>
      ipcRenderer.invoke(CHANNELS.windowFitToScale, lcdWidth, lcdHeight, scale),
    rememberGeometry: (deviceId: string) =>
      ipcRenderer.invoke(CHANNELS.windowRemember, deviceId),
    restoreGeometry: (deviceId: string) =>
      ipcRenderer.invoke(CHANNELS.windowRestore, deviceId),
    minimize: () => ipcRenderer.send(CHANNELS.windowMinimize),
    toggleMaximize: () => ipcRenderer.send(CHANNELS.windowMaximize),
    close: () => ipcRenderer.send(CHANNELS.windowClose),
    startDrag: () => ipcRenderer.send(CHANNELS.windowStartDrag),
    endDrag: () => ipcRenderer.send(CHANNELS.windowEndDrag),
    onStateChange: (cb: (s: WindowStateEvent) => void) =>
      subscribe<[WindowStateEvent]>(CHANNELS.windowState, cb),
  },

  devices: {
    publish: (list: DeviceMenuEntry[]) => ipcRenderer.send(CHANNELS.devicesPublish, list),
    setCurrent: (id: string | null) => ipcRenderer.send(CHANNELS.deviceCurrent, id),
    onCommand: (cb: (c: HostCommand) => void) =>
      subscribe<[HostCommand]>(CHANNELS.command, cb),
  },

  state: {
    get: <T,>(key: string) => ipcRenderer.invoke(CHANNELS.stateGet, key) as Promise<T | null>,
    set: (key: string, value: unknown) => ipcRenderer.invoke(CHANNELS.stateSet, key, value),
  },

  lifecycle: {
    onPrepareQuit: registerPrepareQuit,
    setBusy: (reason: string | null) => ipcRenderer.send(CHANNELS.setBusy, reason),
    notify: (title: string, body: string) => ipcRenderer.send(CHANNELS.notify, title, body),
  },

  saves: {
    write: (deviceId: string, bundle: Uint8Array,
            meta: { savedAt: number; schemaVersion: number }) =>
      ipcRenderer.invoke(CHANNELS.saveWrite, deviceId, bundle, meta),
    list: (deviceId: string) =>
      ipcRenderer.invoke(CHANNELS.saveList, deviceId) as Promise<SaveGeneration[]>,
    read: (deviceId: string, file: string) =>
      ipcRenderer.invoke(CHANNELS.saveRead, deviceId, file) as Promise<Uint8Array>,
    prune: (deviceId: string, files: string[]) =>
      ipcRenderer.invoke(CHANNELS.savePrune, deviceId, files),
  },

  mounts: {
    get: () => ipcRenderer.invoke(CHANNELS.mountsGet) as Promise<Record<MountId, MountInfo>>,
    pick: (id: MountId) => ipcRenderer.invoke(CHANNELS.mountsPick, id) as Promise<MountInfo>,
    clear: (id: MountId) => ipcRenderer.invoke(CHANNELS.mountsClear, id),
    setEnabled: (id: MountId, on: boolean) =>
      ipcRenderer.invoke(CHANNELS.mountsSetEnabled, id, on),
    reveal: (id: MountId, rel?: string) => ipcRenderer.invoke(CHANNELS.mountsReveal, id, rel),
    list: (id: MountId) => ipcRenderer.invoke(CHANNELS.fsList, id) as Promise<HostDirent[]>,
    read: (id: MountId, rel: string) =>
      ipcRenderer.invoke(CHANNELS.fsRead, id, rel) as Promise<Uint8Array>,
    write: (id: MountId, rel: string, data: Uint8Array, mtimeMs?: number) =>
      ipcRenderer.invoke(CHANNELS.fsWrite, id, rel, data, mtimeMs),
    remove: (id: MountId, rel: string) => ipcRenderer.invoke(CHANNELS.fsDelete, id, rel),
    mkdirp: (id: MountId, rel: string) => ipcRenderer.invoke(CHANNELS.fsMkdirp, id, rel),
    onChange: (cb: (id: MountId) => void) => subscribe<[MountId]>(CHANNELS.fsChanged, cb),
  },
};

contextBridge.exposeInMainWorld('psionHost', host);
