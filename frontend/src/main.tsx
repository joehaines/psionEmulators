// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import { registerSW } from 'virtual:pwa-register';
import './index.css';
import App from './App';
import { recoverFromStaleCache } from './lib/staleCache';

// Offline support: precaches the app shell + WASM engine and runtime-caches
// ROMs/skins (see the VitePWA block in vite.config.ts). autoUpdate mode — a
// new deploy's service worker takes over as soon as it has installed, which
// is usually a few seconds into the visit that finds it. The page reloads
// onto it then, unless the visitor has already started using it: a reload
// would throw away whatever they've done since the last autosave, so they
// get the new version on their next visit instead.
let userActive = false;
for (const type of ['pointerdown', 'keydown']) {
  window.addEventListener(type, () => { userActive = true; }, { once: true, capture: true });
}
registerSW({
  immediate: true,
  onNeedReload() {
    if (!userActive) window.location.reload();
  },
});

// A lazily-loaded chunk named by an older index.html that this deploy no
// longer serves: same remedy as a stale engine (lib/staleCache.ts) — purge
// and reload once onto the current deploy.
window.addEventListener('vite:preloadError', (e) => {
  void recoverFromStaleCache(import.meta.env.BASE_URL, String(e.payload));
});

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <App />
  </StrictMode>
);
