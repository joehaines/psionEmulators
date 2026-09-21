// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Mount registry: which host folders are attached, and when they change.
//
// A mount root is only ever set by a native folder dialog, which is a real
// user gesture the renderer cannot fake. The root itself never crosses the
// bridge as a usable path — the renderer gets a display label and addresses
// files as (mountId, relativePath). See hostfs.ts for why.
//
// Change notification is chokidar rather than fs.watch. That is not
// preference: on macOS an application saving a document does a rename dance
// over a temp file (write NEW, rename old to ~, rename NEW to old), which
// fs.watch reports as an unhelpful scatter of events and sometimes misses
// entirely — and that is how Finder, TextEdit and Word all save. On Windows
// fs.watch cannot watch a tree recursively without re-arming per directory.
// awaitWriteFinish is the other half: without it a large file dropped into the
// folder is picked up while it is still being copied.
//
// The notification carries no path detail on purpose. The consumer re-lists.
// A per-path stream would be a lie given the rename dance above, and a
// consumer that trusted it would miss the save it most cares about.

import { BrowserWindow, dialog, shell } from 'electron';
import type { FSWatcher } from 'chokidar';
import * as chokidar from 'chokidar';
import * as path from 'node:path';
import * as os from 'node:os';
import type { MountId, MountInfo } from '../ipc/contract';
import * as hostfs from './hostfs';
import * as settings from './settings';

/** Coalescing window for change events. A save is several events. */
const DEBOUNCE_MS = 400;
/** How long a file must stop growing before it counts as written. */
const WRITE_SETTLE_MS = 600;

interface StoredMount {
  root: string;
  enabled: boolean;
}

const SETTINGS_KEY = 'mount.roots';

const watchers = new Map<MountId, FSWatcher>();
const debounceTimers = new Map<MountId, NodeJS.Timeout>();
let notify: ((id: MountId) => void) | null = null;

function readStored(): Partial<Record<MountId, StoredMount>> {
  return settings.get<Partial<Record<MountId, StoredMount>>>(SETTINGS_KEY) ?? {};
}

async function writeStored(next: Partial<Record<MountId, StoredMount>>): Promise<void> {
  await settings.set(SETTINGS_KEY, next);
}

/** Shorten an absolute path for display. Never used to build a path. */
function label(root: string): string {
  const home = os.homedir();
  return root.startsWith(home + path.sep) ? `~${root.slice(home.length)}` : root;
}

function infoFor(id: MountId): MountInfo {
  const stored = readStored()[id];
  return {
    id,
    path: stored ? label(stored.root) : null,
    enabled: stored?.enabled ?? false,
  };
}

export function snapshot(): Record<MountId, MountInfo> {
  return { internal: infoFor('internal'), external: infoFor('external') };
}

/** Called once at startup to re-attach whatever the user chose last time. */
export function restore(onChange: (id: MountId) => void): void {
  notify = onChange;
  const stored = readStored();
  for (const id of ['internal', 'external'] as MountId[]) {
    const entry = stored[id];
    if (!entry) continue;
    try {
      hostfs.setRoot(id, entry.root);
      if (entry.enabled) startWatching(id, entry.root);
    } catch (err) {
      // A folder on a drive that is no longer mounted, most likely. Keep the
      // setting so it comes back when the drive does, but do not pretend the
      // mount is live.
      console.warn(`[psion] mount '${id}' could not be restored:`, err);
    }
  }
}

function startWatching(id: MountId, root: string): void {
  stopWatching(id);
  const watcher = chokidar.watch(root, {
    ignoreInitial: true,
    // Our own bookkeeping and the OS's clutter are not the user's content, and
    // reacting to them would mean a sync cycle per Finder visit.
    ignored: (p: string) => {
      const base = path.basename(p);
      return base.startsWith('.') || base === 'Thumbs.db' || base === 'desktop.ini'
        || p.includes(`${path.sep}.psion-sync${path.sep}`)
        || base.includes('.psion-tmp-');
    },
    awaitWriteFinish: { stabilityThreshold: WRITE_SETTLE_MS, pollInterval: 100 },
    depth: hostfs.MAX_DEPTH,
    followSymlinks: false,
  });

  const fire = () => {
    const existing = debounceTimers.get(id);
    if (existing) clearTimeout(existing);
    debounceTimers.set(id, setTimeout(() => {
      debounceTimers.delete(id);
      notify?.(id);
    }, DEBOUNCE_MS));
  };

  watcher.on('add', fire);
  watcher.on('change', fire);
  watcher.on('unlink', fire);
  watcher.on('addDir', fire);
  watcher.on('unlinkDir', fire);
  watcher.on('error', (err) => console.warn(`[psion] watcher for '${id}':`, err));

  watchers.set(id, watcher);
}

function stopWatching(id: MountId): void {
  const existing = watchers.get(id);
  if (existing) { void existing.close(); watchers.delete(id); }
  const timer = debounceTimers.get(id);
  if (timer) { clearTimeout(timer); debounceTimers.delete(id); }
}

export async function pick(id: MountId): Promise<MountInfo> {
  const win = BrowserWindow.getAllWindows()[0] ?? null;
  const titles: Record<MountId, string> = {
    internal: "Choose a folder for this machine's own drive",
    external: 'Choose the shared card folder',
  };
  const result = win
    ? await dialog.showOpenDialog(win, {
        title: titles[id],
        properties: ['openDirectory', 'createDirectory'],
        buttonLabel: 'Use this folder',
      })
    : await dialog.showOpenDialog({
        title: titles[id],
        properties: ['openDirectory', 'createDirectory'],
      });

  // Cancelled: report the mount unchanged rather than clearing it, so a
  // mis-click does not detach a folder the user is relying on.
  if (result.canceled || result.filePaths.length === 0) return infoFor(id);

  const root = result.filePaths[0];
  hostfs.setRoot(id, root);
  const stored = readStored();
  stored[id] = { root, enabled: true };
  await writeStored(stored);
  startWatching(id, root);
  return infoFor(id);
}

export async function clear(id: MountId): Promise<void> {
  stopWatching(id);
  hostfs.setRoot(id, null);
  const stored = readStored();
  delete stored[id];
  await writeStored(stored);
}

export async function setEnabled(id: MountId, on: boolean): Promise<void> {
  const stored = readStored();
  const entry = stored[id];
  if (!entry) return;
  entry.enabled = on;
  await writeStored(stored);
  if (on) startWatching(id, entry.root);
  else stopWatching(id);
}

export async function reveal(id: MountId, rel?: string): Promise<void> {
  const target = await hostfs.absolutePathFor(id, rel);
  // showItemInFolder selects a file; a folder wants openPath, or it opens the
  // parent with the folder highlighted instead of opening it.
  if (rel) shell.showItemInFolder(target);
  else await shell.openPath(target);
}

/** Close every watcher. Called on the quit path so nothing keeps the loop alive. */
export async function shutdown(): Promise<void> {
  for (const id of [...watchers.keys()]) stopWatching(id);
  await Promise.resolve();
}
