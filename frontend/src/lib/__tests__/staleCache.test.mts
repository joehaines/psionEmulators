// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Stale-cache recovery tests. Run via:
//   node --experimental-strip-types frontend/src/lib/__tests__/staleCache.test.mts
// or `npm run test:stale-cache` from frontend/.
//
// Drives recoverFromStaleCache()/purgeAndReload() against stubbed browser
// globals and checks the properties that matter: it reloads at most once
// per tab (a real bug must not become a reload loop), never purges offline
// (the caches are the app there), keeps ROMs, and re-arms after a good load.

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}

// ── Browser stubs ─────────────────────────────────────────────────────
const session = new Map<string, string>();
let online = true;
let serverUp = true;
let reloads = 0;
let unregistered = 0;
let cacheNames: string[] = [];
const fetched: { url: string; cache?: string }[] = [];

function reset(names = ['workbox-precache-v2-https://x/psion/', 'psion-roms', 'psion-skins']) {
  session.clear(); online = true; serverUp = true; reloads = 0; unregistered = 0;
  cacheNames = [...names]; fetched.length = 0;
}

const g = globalThis as Record<string, unknown>;
g.sessionStorage = {
  getItem: (k: string) => session.get(k) ?? null,
  setItem: (k: string, v: string) => { session.set(k, v); },
  removeItem: (k: string) => { session.delete(k); },
};
Object.defineProperty(globalThis, 'navigator', {
  configurable: true,
  value: {
    get onLine() { return online; },
    serviceWorker: { getRegistrations: async () => [{ unregister: async () => { unregistered++; return true; } }] },
  },
});
g.caches = {
  keys: async () => [...cacheNames],
  delete: async (n: string) => { cacheNames = cacheNames.filter(c => c !== n); return true; },
};
g.fetch = async (url: string, init?: { cache?: string }) => {
  fetched.push({ url, cache: init?.cache });
  if (!serverUp) throw new TypeError('Failed to fetch');
  return { ok: true, arrayBuffer: async () => new ArrayBuffer(0) };
};
g.window = { location: { reload: () => { reloads++; } } };
const quietWarn = console.warn;
console.warn = () => {};

const { recoverFromStaleCache, purgeAndReload, markLoadHealthy } = await import('../staleCache.ts');
const BASE = '/psion/';

// ── First failure: purge and reload ───────────────────────────────────
reset();
check(await recoverFromStaleCache(BASE, 'TextDecoder') === true, 'first failure recovers');
check(reloads === 1, 'first failure reloads once');
check(unregistered === 1, 'service worker unregistered');
check(cacheNames.length === 1 && cacheNames[0] === 'psion-roms', `only ROM cache kept (left: ${cacheNames})`);
for (const f of ['psion.js', 'psion.wasm', 'emulator-worker.js']) {
  check(fetched.some(x => x.url === BASE + f && x.cache === 'reload'), `${f} refreshed past the HTTP cache`);
}

// ── Second failure in the same tab: no loop ───────────────────────────
reloads = 0;
check(await recoverFromStaleCache(BASE, 'TextDecoder') === false, 'second failure falls through to the error');
check(reloads === 0, 'second failure does not reload');

// ── A good load re-arms it ────────────────────────────────────────────
markLoadHealthy();
check(await recoverFromStaleCache(BASE) === true, 'recovers again after a healthy load');

// ── Offline: never purge ──────────────────────────────────────────────
reset();
online = false;
check(await recoverFromStaleCache(BASE) === false, 'navigator offline: no recovery');
check(cacheNames.length === 3 && reloads === 0, 'navigator offline: caches untouched, no reload');
online = true; serverUp = false;
check(await recoverFromStaleCache(BASE) === false, 'server unreachable: no recovery');
check(cacheNames.length === 3 && unregistered === 0, 'server unreachable: caches and SW untouched');
serverUp = true;
check(await recoverFromStaleCache(BASE) === true, 'an unreachable server does not use up the one attempt');

// The reachability probe must not be answerable from the SW precache.
check(fetched.some(x => /manifest\.webmanifest\?/.test(x.url) && x.cache === 'no-store'), 'probe is cache-busted');

// ── No sessionStorage: can't guard a loop, so never auto-reload ───────
reset();
const realGet = (g.sessionStorage as { getItem: unknown }).getItem;
(g.sessionStorage as { getItem: unknown }).getItem = () => { throw new Error('SecurityError'); };
check(await recoverFromStaleCache(BASE) === false, 'no sessionStorage: no automatic reload');
(g.sessionStorage as { getItem: unknown }).getItem = realGet;

// ── Reload button: always reloads; purges only when online ────────────
reset();
session.set('psion-stale-cache-recovery', String(Date.now()));   // auto attempt already spent
await purgeAndReload(BASE);
check(reloads === 1 && cacheNames.length === 1, 'button purges and reloads even after the auto attempt');
reset();
serverUp = false;
await purgeAndReload(BASE);
check(reloads === 1 && cacheNames.length === 3, 'button offline: plain reload, caches kept');

console.warn = quietWarn;
if (failures) { console.error(`${failures} failure(s)`); process.exit(1); }
console.log('staleCache: all checks passed');
