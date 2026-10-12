import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { VitePWA } from 'vite-plugin-pwa';
import { createHash } from 'node:crypto';
import { readFile } from 'node:fs/promises';
import path from 'node:path';

// The resolved build output directory, for hashing precache entries (below).
let outDir = '';

export default defineConfig({
  base: '/psion/',
  plugins: [
    react(),
    { name: 'psion-out-dir', configResolved(c) { outDir = path.resolve(c.root, c.build.outDir); } },
    // Offline support. The generated service worker precaches the app
    // shell (HTML/JS/CSS, the WASM engine, the emulator worker, icons)
    // so the emulator loads with no network; the big variable-size
    // assets are runtime-cached instead:
    //   - ROMs are copied into dist/roms/ by CI *after* this build, so
    //     they can't be precached — they're cached on first use
    //     (CacheFirst: a ROM image never changes once shipped).
    //   - Device skins / logos are ~37 MB of images; precaching them
    //     would bloat every visitor's first load, so they're cached as
    //     the user visits each device (StaleWhileRevalidate so skin
    //     fixes still propagate).
    // The handcrafted public/manifest.webmanifest is kept as-is
    // (manifest: false) — index.html already links it.
    VitePWA({
      registerType: 'autoUpdate',
      injectRegister: false,           // registered manually in main.tsx
      manifest: false,
      workbox: {
        // A new deploy's service worker takes over as soon as it has
        // installed. vite-plugin-pwa only sets these for autoUpdate when it
        // injects the registration itself; with injectRegister: false they
        // were off, so every new version sat "waiting" until all of the
        // site's tabs were closed — in practice never, and visitors kept
        // the old version until they cleared site data. main.tsx decides
        // whether the open page reloads onto it.
        skipWaiting: true,
        clientsClaim: true,
        // Names the precache psion-precache-v2-…; the old
        // workbox-precache-v2-… one is deleted when this worker activates
        // (cleanupOutdatedCaches). The rename is a one-off flush: precaches
        // filled before the integrity hashes below can hold another deploy's
        // files under this deploy's revisions, and Workbox never re-fetches
        // an entry it already has.
        cacheId: 'psion',
        // Subresource-integrity hash on every precache entry. The FTP deploy
        // uploads file by file, so a visitor mid-deploy can install the new
        // sw.js while the old index.html / psion.js / psion.wasm are still on
        // the server. Without a hash those old bytes were cached under the
        // new revisions — permanently, as above: the old version (or a
        // mismatched psion.js/psion.wasm pair) for good. With one, that
        // install fails instead and is retried on a later visit, by when the
        // upload has finished.
        manifestTransforms: [async (entries) => ({
          manifest: await Promise.all(entries.map(async (e) => ({
            ...e,
            integrity: 'sha384-' + createHash('sha384')
              .update(await readFile(path.join(outDir, e.url))).digest('base64'),
          }))),
          warnings: [],
        })],
        globPatterns: ['**/*.{js,css,html,svg,png,webmanifest,wasm}'],
        globIgnores: [
          'skins/**', 'device-skins/**', 'device-logos/**', 'device-3d/**',
          'mame-3a/**', 'roms/**', 'api/**', 'apps/**',
          // The experimental 3D view's chunks (three.js is ~0.5 MB): left
          // to the browser cache until the view is more than an opt-in.
          'assets/three.module-*.js', 'assets/Device3DView-*.js', 'assets/psion5mx-*.js',
        ],
        // psion.wasm is well over Workbox's 2 MiB default.
        maximumFileSizeToCacheInBytes: 64 * 1024 * 1024,
        navigateFallback: '/psion/index.html',
        navigateFallbackDenylist: [/\/api\//],
        runtimeCaching: [
          {
            urlPattern: ({ url, sameOrigin }) => sameOrigin && /\/roms\//.test(url.pathname),
            handler: 'CacheFirst',
            options: {
              cacheName: 'psion-roms',
              expiration: { maxEntries: 60, maxAgeSeconds: 365 * 24 * 3600 },
              cacheableResponse: { statuses: [0, 200] },
            },
          },
          {
            urlPattern: ({ url, sameOrigin }) =>
              sameOrigin && /\/(skins|device-skins|device-3d|device-logos|mame-3a)\//.test(url.pathname),
            handler: 'StaleWhileRevalidate',
            options: {
              cacheName: 'psion-skins',
              expiration: { maxEntries: 300, maxAgeSeconds: 365 * 24 * 3600 },
              cacheableResponse: { statuses: [0, 200] },
            },
          },
          // App library: the manifest is network-first (fresh catalogue
          // when online, last-seen copy offline); icons and app bundles
          // are immutable per deploy, so cache-first with an entry cap
          // keeps revisited apps available offline without hoarding.
          {
            urlPattern: ({ url, sameOrigin }) =>
              sameOrigin && /\/apps\/manifest\.json$/.test(url.pathname),
            handler: 'NetworkFirst',
            options: {
              cacheName: 'psion-apps-manifest',
              cacheableResponse: { statuses: [0, 200] },
            },
          },
          {
            urlPattern: ({ url, sameOrigin }) =>
              sameOrigin && /\/apps\/(files|icons)\//.test(url.pathname),
            handler: 'CacheFirst',
            options: {
              cacheName: 'psion-apps',
              expiration: { maxEntries: 400, maxAgeSeconds: 90 * 24 * 3600 },
              cacheableResponse: { statuses: [0, 200] },
            },
          },
          {
            urlPattern: ({ url }) => url.origin === 'https://fonts.googleapis.com',
            handler: 'StaleWhileRevalidate',
            options: { cacheName: 'google-fonts-css' },
          },
          {
            urlPattern: ({ url }) => url.origin === 'https://fonts.gstatic.com',
            handler: 'CacheFirst',
            options: {
              cacheName: 'google-fonts-woff',
              expiration: { maxEntries: 12, maxAgeSeconds: 365 * 24 * 3600 },
              cacheableResponse: { statuses: [0, 200] },
            },
          },
        ],
      },
    }),
  ],
  server: {
    headers: {
      'Cross-Origin-Embedder-Policy': 'require-corp',
      'Cross-Origin-Opener-Policy': 'same-origin',
    },
  },
  optimizeDeps: {
    exclude: ['psion.js'],
  },
  build: {
    outDir: '../dist',
    emptyOutDir: true,
  },
  assetsInclude: ['**/*.wasm'],
});
