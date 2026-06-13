import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { VitePWA } from 'vite-plugin-pwa';

export default defineConfig({
  base: '/psion/',
  plugins: [
    react(),
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
        globPatterns: ['**/*.{js,css,html,svg,png,webmanifest,wasm}'],
        globIgnores: [
          'skins/**', 'device-skins/**', 'device-logos/**',
          'mame-3a/**', 'roms/**', 'api/**', 'apps/**',
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
              sameOrigin && /\/(skins|device-skins|device-logos|mame-3a)\//.test(url.pathname),
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
