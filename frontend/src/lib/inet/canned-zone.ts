// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Curated pages served by the in-browser HTTP proxy. Keys are
// `${host}${path}` (path always starts with /). Anything not in this
// map under a `*.psion.local` host gets a generic 404; real-world
// hosts go through window.fetch().
//
// Style note: pages target the 5mx's Web 1.0 browser which only
// renders very plain HTML. No CSS, no JS — and tables work, but big
// pages scroll poorly on the 640×240 LCD.

const home = `<html><head><title>Simulated Internet</title></head><body>
<h1>Welcome to the simulated internet</h1>
<p>You are online. This page is served by the host emulator, not the real
internet, so it works regardless of CORS.</p>
<h2>Pages on the curated zone</h2>
<ul>
<li><a href="http://canned.psion.local/about">About this simulation</a></li>
<li><a href="http://news.psion.local/">News (sample articles)</a></li>
<li><a href="http://mail.psion.local/">Mail server info (phase 3)</a></li>
</ul>
<h2>Real internet</h2>
<p>You can try any other URL. Most will hit a CORS wall and serve a
synthetic 502 — that's a browser limitation, not an EPOC one.</p>
<ul>
<li><a href="http://example.com/">example.com</a> (CORS-permissive — works)</li>
<li><a href="http://www.psion.com/">www.psion.com</a> (probably blocked)</li>
</ul>
</body></html>`;

const about = `<html><head><title>About</title></head><body>
<h1>About this simulation</h1>
<p>The emulator hosts a fake dial-up modem on UART2. When the device
dials, the host runs a Hayes AT modem, then PPP (LCP / PAP / IPCP)
to bring the link up, then answers IP traffic.</p>
<p>DNS resolves <code>*.psion.local</code> to curated pages served from
the browser. Everything else routes to an HTTP fetch proxy that asks
your browser to fetch the URL via <code>window.fetch()</code>. CORS
limits apply to that.</p>
<p>SMTP / POP3 servers for the Email app arrive in phase 3.</p>
<p><a href="http://canned.psion.local/">Back home</a></p>
</body></html>`;

const news = `<html><head><title>News</title></head><body>
<h1>Today's headlines</h1>
<dl>
<dt><b>Psion 5mx receives surprise internet upgrade in 2026</b></dt>
<dd>Decades after launch, the venerable clamshell handheld is back
online thanks to an in-browser PPP server. Reactions are mixed.</dd>
<dt><b>EPOC kernel reportedly "fine, thanks"</b></dt>
<dd>"It's been waiting," sources close to the ARM710 say.</dd>
<dt><b>IrDA still feels left out</b></dt>
<dd>"They said UART2 was easier," an anonymous infrared port commented.</dd>
</dl>
<p><a href="http://canned.psion.local/">Back home</a></p>
</body></html>`;

const mailInfo = `<html><head><title>Mail (phase 3)</title></head><body>
<h1>Mail server</h1>
<p>SMTP (TCP/25) and POP3 (TCP/110) servers arrive in phase 3.
Once they're live, configure the Email app to use
<code>mail.psion.local</code> for both incoming and outgoing.</p>
<p><a href="http://canned.psion.local/">Back home</a></p>
</body></html>`;

const CURATED: Record<string, string> = {
  'canned.psion.local/':       home,
  'canned.psion.local/index':  home,
  'canned.psion.local/about':  about,
  'news.psion.local/':         news,
  'mail.psion.local/':         mailInfo,
};

export function lookupCurated(host: string, path: string): string | null {
  const norm = (path === '' ? '/' : path).split('?')[0].split('#')[0];
  const key = `${host.toLowerCase()}${norm}`;
  return CURATED[key] ?? null;
}

export function isCuratedHost(host: string): boolean {
  return host.toLowerCase().endsWith('.psion.local');
}

export function curatedNotFound(host: string, path: string): string {
  return `<html><head><title>Not found</title></head><body>
<h1>404 Not Found</h1>
<p>The curated server at <code>${host}</code> has no page at
<code>${path}</code>.</p>
<p><a href="http://canned.psion.local/">Home</a></p>
</body></html>`;
}

export function corsErrorPage(host: string, path: string, reason: string): string {
  return `<html><head><title>502 Bad Gateway</title></head><body>
<h1>502 Bad Gateway</h1>
<p>The host emulator tried to fetch
<code>http://${host}${path}</code> on your behalf, but the browser
refused:</p>
<pre>${reason}</pre>
<p>This is almost always a CORS restriction. The remote server didn't
include an <code>Access-Control-Allow-Origin</code> header, so your
browser blocks the in-page JavaScript from reading the response.</p>
<p>For a working demo, try <a href="http://canned.psion.local/">the
curated zone</a>.</p>
</body></html>`;
}
