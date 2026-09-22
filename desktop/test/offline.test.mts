// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The app must work with no network connection.
//
// That is a promise about a packaged app on a Raspberry Pi with no cable in
// it, and it is easy to break by accident from the web side: index.html is
// shared with the web build, so a <link> to a font CDN added there ships in
// the desktop renderer too, and a new feature that fetches something is one
// import away. Neither shows up in a test run on a machine with working DNS.
//
// Two halves, because there are two mechanisms and only one of them is
// enforcement:
//
//   1. The CSP the protocol handler serves. This is the real guarantee — a
//      fetch-directive naming a remote origin is the only way the renderer
//      could reach the network at all, so every one of them must name
//      nothing but 'self' and the app's own scheme.
//   2. The built renderer's index.html. Nothing in it may point off-origin,
//      which the CSP would block anyway; the point is that a blocked request
//      is still a request, it still logs a console violation, and it still
//      means a DNS lookup was attempted on a machine that may have no DNS.
//
// The second half needs `npm run build:desktop` to have run. It reports what
// it checked and skips rather than fails when the renderer is absent, so the
// test is still useful in a bare checkout.
//
// Run: node --experimental-strip-types desktop/test/offline.test.mts

import { existsSync, readFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = dirname(fileURLToPath(import.meta.url));
const DESKTOP = resolve(HERE, '..');

let failures = 0;
function check(cond: unknown, msg: string) {
  console.log(`${cond ? '✓' : '✗'} ${msg}`);
  if (!cond) failures++;
}

// ── 1. No fetch directive may name a remote origin ──────────────────────
//
// Read out of the source rather than imported: protocol.ts imports electron
// at module scope, which is not available under plain node.
const source = readFileSync(join(DESKTOP, 'src', 'main', 'protocol.ts'), 'utf8');
const cspBlock = /const CSP = \[([\s\S]*?)\]\.join/.exec(source);
check(!!cspBlock, 'protocol.ts still declares the CSP as a list of directives');

// Everything that can pull bytes over a network. `style-src` and `font-src`
// are in the list precisely because of the Google Fonts <link> this test was
// written after: a stylesheet is a network fetch like any other.
const FETCH_DIRECTIVES = [
  'default-src', 'script-src', 'style-src', 'font-src', 'img-src',
  'media-src', 'connect-src', 'worker-src', 'frame-src', 'object-src',
];
// What a source expression may be. Anything else is either a remote origin
// or a wildcard broad enough to reach one.
const ALLOWED_SOURCE = /^('self'|'none'|'unsafe-inline'|'unsafe-eval'|'wasm-unsafe-eval'|app:\/\/psion|data:|blob:|'sha256-[A-Za-z0-9+/=]+')$/;

const directives = (cspBlock?.[1] ?? '')
  .split('\n')
  .map((line) => /"([^"]+)"/.exec(line)?.[1])
  .filter((d): d is string => !!d);
check(directives.length > 0, `the CSP parsed into directives (${directives.length})`);

let sawConnectSrc = false;
for (const directive of directives) {
  const [name, ...sources] = directive.split(/\s+/);
  if (!FETCH_DIRECTIVES.includes(name)) continue;
  if (name === 'connect-src') sawConnectSrc = true;
  const remote = sources.filter((s) => !ALLOWED_SOURCE.test(s));
  check(remote.length === 0,
        `${name} names no remote origin${remote.length ? ` (found ${remote.join(', ')})` : ''}`);
}
// A missing connect-src would fall back to default-src, which is checked
// too — but silently relying on that is how the directive gets dropped.
check(sawConnectSrc, 'connect-src is set explicitly, not left to default-src');

// ── 2. The built renderer asks for nothing off-origin ───────────────────
const indexHtml = join(DESKTOP, 'build', 'renderer', 'index.html');
if (!existsSync(indexHtml)) {
  console.log('· no built renderer — run `npm run build:desktop` in frontend/ to check index.html');
} else {
  const html = readFileSync(indexHtml, 'utf8');

  // Only the attributes the browser actually goes and fetches. href on
  // <link rel="canonical"> and the Open Graph URLs are metadata for a
  // crawler that will never see this build, so they are left alone — the
  // test is about requests, not about strings.
  const FETCHING = /<(?:link|script|img|iframe|source)\b[^>]*\b(?:src|href)\s*=\s*["']([^"']+)["']/gi;
  const remote: string[] = [];
  for (const m of html.matchAll(FETCHING)) {
    const url = m[1];
    const tag = m[0];
    // <link rel="canonical"> is the one non-fetching href on a <link>.
    if (/\brel\s*=\s*["']canonical["']/i.test(tag)) continue;
    if (/^(https?:)?\/\//i.test(url)) remote.push(url);
  }
  check(remote.length === 0,
        `the built index.html fetches nothing off-origin${remote.length ? ` (found ${remote.join(', ')})` : ''}`);

  // The specific regression: index.html's Google Fonts <link>s are stripped
  // by the desktop vite config (stripRemoteFonts). Named separately from the
  // check above so a failure says which mechanism broke.
  check(!/fonts\.(googleapis|gstatic)\.com/.test(html),
        'the Google Fonts links are stripped from the desktop renderer');
}

console.log('');
if (failures) {
  console.error(`✗ ${failures} assertion(s) failed`);
  process.exit(1);
}
console.log('✓ nothing in the desktop app reaches for the network\n');
