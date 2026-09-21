// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Renderer build for the Electron desktop app.
//
// Deliberately a separate config rather than a flag inside vite.config.ts:
// the web build's output must stay byte-for-byte unaffected by any of this,
// and a config that can't be reached from `npm run build` is the simplest
// way to guarantee that.
//
// Two choices worth explaining:
//
//   base stays '/psion/'. Every asset in the app already resolves through
//   `import.meta.env.BASE_URL`, but index.html additionally hardcodes
//   /psion/favicon.svg, /psion/icon-192.png and /psion/manifest.webmanifest.
//   Switching to './' would mean a second index.html and a permanent
//   divergence; keeping the base identical and serving the app at
//   app://psion/psion/ (see desktop/src/main/protocol.ts) means zero
//   divergence instead.
//
//   No VitePWA. A service worker inside a packaged app is all cost and no
//   benefit — it would cache the app shell against itself across upgrades.
//   main.tsx calls registerSW() unconditionally though, so rather than
//   edit it we resolve the virtual module to a no-op.

import { defineConfig, type Plugin } from 'vite';
import react from '@vitejs/plugin-react';

/**
 * vite-plugin-pwa injects `virtual:pwa-register`, which main.tsx imports.
 * With the plugin absent that import would fail to resolve and the build
 * would break, so stand in for it with a no-op module of the same shape.
 */
function stubPwaRegister(): Plugin {
  const id = 'virtual:pwa-register';
  const resolved = '\0' + id;
  return {
    name: 'psion-stub-pwa-register',
    resolveId: (source) => (source === id ? resolved : null),
    load: (loadedId) =>
      loadedId === resolved
        ? 'export function registerSW() { return () => Promise.resolve(); }'
        : null,
  };
}

export default defineConfig({
  base: '/psion/',
  plugins: [react(), stubPwaRegister()],
  define: {
    // Read by lib/desktop/host.ts and by analytics.ts, so the desktop
    // build can tree-shake the web-only paths and never phone home.
    'import.meta.env.VITE_PSION_DESKTOP': '"1"',
  },
  optimizeDeps: {
    exclude: ['psion.js'],
  },
  build: {
    outDir: '../desktop/build/renderer',
    emptyOutDir: true,
    // The desktop app is not served over a network, so a single large
    // chunk is cheaper than many round-trips. Silence the advisory.
    chunkSizeWarningLimit: 4096,
  },
  assetsInclude: ['**/*.wasm'],
});
