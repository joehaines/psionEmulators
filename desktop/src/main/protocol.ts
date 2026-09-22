// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Serves the app over a registered privileged `app://` scheme.
//
// This is NOT a nicety — file:// is a hard non-starter. The default
// emulation path runs in a Web Worker (frontend/src/App.tsx WORKER_MODE),
// and `new Worker('emulator-worker.js')` is blocked from a file origin;
// so is the `importScripts(baseUrl + 'psion.js')` the worker does to load
// the WASM module. A custom standard scheme gives us a real origin, which
// makes Worker, fetch and IndexedDB all behave exactly as they do on the
// web build.
//
// The renderer keeps the web build's `base: '/psion/'` (index.html
// hardcodes /psion/favicon.svg, /psion/manifest.webmanifest and friends),
// so the app lives at app://psion/psion/. Two roots are served under it:
//
//   app://psion/psion/…        → the built renderer
//   app://psion/psion/roms/…   → the ROM tree, which never enters the asar
//
// ROMs are served from a directory rather than bundled into the app
// archive because they are 201 MB and there is no reason to pay asar
// indirection on them.
//
// The ROM loader (useEmulator.ts ~1612 and emulator-worker.js ~632) reads
// Content-Length and then streams resp.body.getReader() to drive its
// "2.1 of 8.4 MB" progress line. So what the handler must guarantee is a
// correct Content-Length and a streaming body — NOT byte-range support.
// net.fetch over file:// gives us both; it ignores a Range header, which
// costs us nothing because nothing asks for one.

import { app, net, protocol } from 'electron';
import * as path from 'node:path';
import * as fs from 'node:fs/promises';
import { pathToFileURL } from 'node:url';

export const SCHEME = 'app';
export const HOST = 'psion';
/** Where the renderer thinks it lives; must match vite's `base`. */
export const BASE_PATH = '/psion/';
export const START_URL = `${SCHEME}://${HOST}${BASE_PATH}index.html`;

// Only these are served. Anything else 404s rather than leaking a file
// type we haven't thought about.
const CONTENT_TYPES: Record<string, string> = {
  '.html': 'text/html; charset=utf-8',
  '.js':   'text/javascript; charset=utf-8',
  '.mjs':  'text/javascript; charset=utf-8',
  '.css':  'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.webmanifest': 'application/manifest+json; charset=utf-8',
  '.wasm': 'application/wasm',
  '.png':  'image/png',
  '.jpg':  'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.gif':  'image/gif',
  '.svg':  'image/svg+xml',
  '.ico':  'image/x-icon',
  '.woff':  'font/woff',
  '.woff2': 'font/woff2',
  '.txt':  'text/plain; charset=utf-8',
  // ROM images and the SIBO pack / OS payloads beside them.
  '.bin': 'application/octet-stream',
  '.rom': 'application/octet-stream',
  '.img': 'application/octet-stream',
  '.ssd': 'application/octet-stream',
  '':     'application/octet-stream',
};

// `wasm-unsafe-eval` is required — Emscripten compiles the module at
// runtime. `unsafe-eval` is ALSO required, separately: the engine is built
// with embind (--bind, wasm/main.cpp's EMSCRIPTEN_BINDINGS), whose JS glue
// crafts per-signature invoker functions with `new Function(...)` for speed
// on hot calls like stepFrame/tickCpu — that's plain JS codegen, not wasm
// compilation, so wasm-unsafe-eval alone does not cover it (the module
// throws "Evaluating a string as JavaScript violates CSP" without this).
// The alternative, linking with -s DYNAMIC_EXECUTION=0, drops embind to a
// generic apply()-based invoker on every bound call — not worth it on this
// app's per-instruction hot path. script-src stays 'self' app://psion
// otherwise (no remote origins), which is what actually keeps arbitrary
// script out.
// Google Fonts is deliberately NOT allowed: the desktop app
// ships no webfont, so index.html's preconnect is a dead no-op here.
const CSP = [
  "default-src 'self' app://psion",
  "script-src 'self' app://psion 'wasm-unsafe-eval' 'unsafe-eval'",
  "worker-src 'self' app://psion blob:",
  "connect-src 'self' app://psion",
  "img-src 'self' app://psion data: blob:",
  "media-src 'self' app://psion blob:",
  "style-src 'self' app://psion 'unsafe-inline'",
  "font-src 'self' app://psion",
  "object-src 'none'",
  "base-uri 'none'",
  "form-action 'none'",
].join('; ');

export interface Roots {
  /** The built renderer (desktop/build/renderer). */
  renderer: string;
  /** The ROM tree (repo roms/ in dev, resourcesPath/roms when packaged). */
  roms: string;
}

/**
 * Must be called BEFORE app.whenReady(). Registering the scheme as
 * `standard` is what gives it an origin (and therefore Worker, fetch and
 * IndexedDB); `secure` keeps it out of Chromium's mixed-content and
 * "not a secure context" paths, which crypto.randomUUID and the async
 * clipboard both check.
 */
export function registerScheme(): void {
  protocol.registerSchemesAsPrivileged([{
    scheme: SCHEME,
    privileges: {
      standard: true,
      secure: true,
      supportFetchAPI: true,
      stream: true,
      corsEnabled: true,
    },
  }]);
}

export function resolveRoots(): Roots {
  // main.ts compiles to build/main/main/main.js, so the renderer sits two
  // levels up at build/renderer — true both in the dev tree and inside
  // the packaged asar, since electron-builder preserves the layout.
  const packagedRenderer = path.join(__dirname, '..', '..', 'renderer');
  const roms = app.isPackaged
    ? path.join(process.resourcesPath, 'roms')
    // In dev, desktop/ sits beside roms/ at the repo root.
    : path.resolve(__dirname, '..', '..', '..', '..', 'roms');
  return { renderer: packagedRenderer, roms };
}

/**
 * Resolve a request path to a real file inside one of the roots, or null.
 * Rejects traversal on the resolved-and-realpath'd result rather than by
 * pattern-matching the input, so `%2e%2e`, a doubled separator or a
 * symlink planted inside the tree are all caught the same way.
 */
interface Resolved { file: string; size: number }

async function resolveWithin(root: string, rel: string): Promise<Resolved | null> {
  const candidate = path.resolve(root, rel);
  // Prefix check before touching the disk, so an obvious escape costs nothing.
  if (candidate !== root && !candidate.startsWith(root + path.sep)) return null;
  let real: string;
  try {
    real = await fs.realpath(candidate);
  } catch {
    return null;
  }
  const realRoot = await fs.realpath(root).catch(() => root);
  if (real !== realRoot && !real.startsWith(realRoot + path.sep)) return null;
  const st = await fs.stat(real).catch(() => null);
  if (!st || !st.isFile()) return null;
  return { file: real, size: st.size };
}

function notFound(): Response {
  return new Response('Not found', { status: 404, headers: { 'content-type': 'text/plain' } });
}

export function installHandler(roots: Roots): void {
  protocol.handle(SCHEME, async (request) => {
    const url = new URL(request.url);
    if (url.hostname !== HOST) return notFound();

    let rel = decodeURIComponent(url.pathname);
    if (!rel.startsWith(BASE_PATH)) return notFound();
    rel = rel.slice(BASE_PATH.length);
    if (rel === '' || rel.endsWith('/')) rel += 'index.html';

    // ROMs come from their own root; everything else from the renderer.
    const fromRoms = rel.startsWith('roms/');
    const root = fromRoms ? roots.roms : roots.renderer;
    const resolved = await resolveWithin(root, fromRoms ? rel.slice('roms/'.length) : rel);
    if (!resolved) return notFound();
    const { file, size } = resolved;

    const ext = path.extname(file).toLowerCase();
    const type = CONTENT_TYPES[ext];
    if (type === undefined) return notFound();

    // net.fetch off the real file streams the body and sets Content-Length,
    // which together are what keep an 8 MB ROM's progress bar honest.
    const res = await net.fetch(pathToFileURL(file).toString(), {
      bypassCustomProtocolHandlers: true,
    });

    const headers = new Headers(res.headers);
    headers.set('content-type', type);
    // net.fetch over file:// does not set Content-Length, and the ROM
    // loader divides by it to drive its progress line — without this the
    // bar sits frozen for the whole download, which is precisely the bug
    // the "hardcoded 30%" comment in useEmulatorWorker records fixing.
    headers.set('content-length', String(size));
    headers.set('content-security-policy', CSP);
    headers.set('x-content-type-options', 'nosniff');
    // The renderer and ROMs are immutable for the life of an installed
    // build, and a stale cache across an upgrade would be a nasty bug, so
    // cache within the session only.
    headers.set('cache-control', 'no-cache');
    return new Response(res.body, { status: res.status, headers });
  });
}
