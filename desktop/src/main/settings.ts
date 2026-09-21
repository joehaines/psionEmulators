// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// A small persisted key/value store in the app's userData directory.
//
// Hand-rolled rather than electron-store because it is forty lines and the
// only behaviour that matters here is the one a dependency wouldn't
// necessarily give us: a write that cannot leave a truncated file behind.
// The renderer reaches this through stateGet/stateSet on the bridge, and
// the mount roots live here too — so a corrupted settings file would mean
// forgetting where the user's folders are.

import { app } from 'electron';
import * as fs from 'node:fs';
import * as fsp from 'node:fs/promises';
import * as path from 'node:path';

type Json = unknown;

let cache: Record<string, Json> | null = null;
let writeChain: Promise<void> = Promise.resolve();

function file(): string {
  return path.join(app.getPath('userData'), 'settings.json');
}

function load(): Record<string, Json> {
  if (cache) return cache;
  try {
    cache = JSON.parse(fs.readFileSync(file(), 'utf8')) as Record<string, Json>;
  } catch {
    // Missing on first run, and unparseable if a previous version wrote
    // something we can't read. Either way an empty store is the right
    // answer — losing preferences is survivable, refusing to start is not.
    cache = {};
  }
  return cache;
}

/**
 * Writes are serialised through one chain and land via a temp file plus
 * rename, so a crash mid-write leaves the previous settings intact rather
 * than a half-written file that reads as "no mounts configured".
 */
function flush(): Promise<void> {
  const snapshot = JSON.stringify(cache ?? {}, null, 2);
  writeChain = writeChain.catch(() => undefined).then(async () => {
    const target = file();
    const tmp = `${target}.${process.pid}.tmp`;
    await fsp.mkdir(path.dirname(target), { recursive: true });
    await fsp.writeFile(tmp, snapshot, 'utf8');
    await fsp.rename(tmp, target);
  });
  return writeChain;
}

export function get<T = Json>(key: string): T | null {
  const v = load()[key];
  return v === undefined ? null : (v as T);
}

export function set(key: string, value: Json): Promise<void> {
  const store = load();
  if (value === null || value === undefined) delete store[key];
  else store[key] = value;
  return flush();
}

/** Await any in-flight write. Used on the quit path. */
export function settled(): Promise<void> {
  return writeChain.catch(() => undefined);
}
