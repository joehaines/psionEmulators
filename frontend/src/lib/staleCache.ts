// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Self-healing for a browser that is still holding pieces of an older deploy.
//
// psion.js (Emscripten's glue) and psion.wasm only work as a matched pair,
// and so do index.html and the hashed chunks it names. A visitor who last
// came by several deploys ago can end up with a mix: an old psion.js out of
// the service worker's precache or the HTTP cache, run against a newer
// psion.wasm, or the reverse. The engine then fails at its first string call
// — getAllDeviceProfilesJSON — with something like "Failed to execute
// 'decode' on 'TextDecoder': The provided ArrayBuffer value must not be
// resizable", and the device list never appears. A plain reload doesn't help
// (the service worker serves the same stale copy again); clearing site data
// does, and visitors shouldn't have to know that.
//
// So when the engine or the device list fails to load, the app calls
// recoverFromStaleCache(): it unregisters the service worker, drops its
// caches, refreshes the engine files in the HTTP cache and reloads the page,
// which then comes up entirely on the current deploy. It does this at most
// once per tab every few minutes, so a failure that is NOT a stale cache
// (a real bug, memory pressure) falls through to the error message after one
// try instead of reloading forever. It also does nothing offline: there the
// caches are the app, and throwing them away would leave nothing to load.
//
// Saved sessions live in IndexedDB/localStorage and are never touched. ROM
// images are kept too: they are immutable once shipped (CacheFirst in
// vite.config.ts), so they can't be stale, and they're the costliest thing
// to download again.

const GUARD_KEY = 'psion-stale-cache-recovery';
/** A second failure within this window is a real failure, not a stale cache. */
const GUARD_MS = 5 * 60 * 1000;
/** Caches that can't go stale and are worth keeping across a purge. */
const KEEP_CACHES = new Set(['psion-roms']);
/** Fetched past every cache before reloading, so the reload can't see old copies. */
const ENGINE_FILES = ['psion.js', 'psion.wasm', 'emulator-worker.js'];

/**
 * Whether the server is reachable. The query string keeps the request clear
 * of the service worker's precache route, so this asks the network, not the
 * cache. navigator.onLine alone can't be trusted (it's true on a captive
 * portal or a dead Wi-Fi link).
 */
async function serverReachable(base: string): Promise<boolean> {
  if (typeof navigator !== 'undefined' && navigator.onLine === false) return false;
  try {
    const resp = await fetch(`${base}manifest.webmanifest?reachable=${Date.now()}`, { cache: 'no-store' });
    return resp.ok;
  } catch {
    return false;
  }
}

/** Unregister the service worker and delete its caches (bar KEEP_CACHES). */
async function purgeCaches(): Promise<void> {
  try {
    const regs = await navigator.serviceWorker?.getRegistrations?.() ?? [];
    await Promise.all(regs.map(r => r.unregister()));
  } catch { /* no SW support, or not allowed: nothing to unregister */ }
  try {
    if (typeof caches !== 'undefined') {
      const names = await caches.keys();
      await Promise.all(names.filter(n => !KEEP_CACHES.has(n)).map(n => caches.delete(n)));
    }
  } catch { /* CacheStorage unavailable */ }
}

/**
 * Re-fetch the engine files with cache: 'reload', which goes to the network
 * and overwrites whatever the HTTP cache held, so the reloaded page can't
 * pick up an old psion.js from there either. Best effort.
 */
async function refreshEngineFiles(base: string): Promise<void> {
  await Promise.all(ENGINE_FILES.map(f =>
    fetch(base + f, { cache: 'reload' }).then(r => r.arrayBuffer(), () => undefined).catch(() => undefined),
  ));
}

function guardTripped(now: number): boolean {
  try {
    const last = Number(sessionStorage.getItem(GUARD_KEY));
    return Number.isFinite(last) && last > 0 && now - last < GUARD_MS;
  } catch {
    // Without sessionStorage there is no way to tell a second attempt from
    // the first, so never reload automatically.
    return true;
  }
}

function setGuard(now: number): void {
  try { sessionStorage.setItem(GUARD_KEY, String(now)); } catch { /* see guardTripped */ }
}

/**
 * The device list loaded, so whatever caches this page came from are good.
 * Clears the guard so a stale cache met later in the same tab (after
 * another deploy) can be recovered from too.
 */
export function markLoadHealthy(): void {
  try { sessionStorage.removeItem(GUARD_KEY); } catch { /* nothing to clear */ }
}

/**
 * Purge stale caches and reload. Returns true when a reload is under way
 * (the caller should keep showing its loading state, not an error), false
 * when recovery was skipped: already tried in this tab recently, offline,
 * or the server can't be reached.
 */
export async function recoverFromStaleCache(base: string, reason?: string): Promise<boolean> {
  const now = Date.now();
  if (guardTripped(now)) return false;
  if (!(await serverReachable(base))) return false;
  setGuard(now);
  console.warn('[stale-cache] load failed, refreshing cached files and reloading', reason ?? '');
  await purgeCaches();
  await refreshEngineFiles(base);
  window.location.reload();
  return true;
}

/**
 * The Reload button behind a load error: the same purge, without the
 * once-per-tab guard because the user asked. Offline it's a plain reload,
 * so the cached app is still there to come back to.
 */
export async function purgeAndReload(base: string): Promise<void> {
  if (await serverReachable(base)) {
    setGuard(Date.now());
    await purgeCaches();
    await refreshEngineFiles(base);
  }
  window.location.reload();
}
