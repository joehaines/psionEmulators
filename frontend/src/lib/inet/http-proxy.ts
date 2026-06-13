// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// HTTP/1.0 server on top of the TCP listener API. For each connection
// we collect request bytes until \r\n\r\n, parse the request line +
// Host header, then either serve curated content or forward to
// window.fetch() and stream the response back.
//
// Method scope: GET/HEAD only. POST etc. respond with 501 — Web 1.0
// on the 5mx is a pure GET browser anyway.

import type { TcpListenerHandler, TcpConn } from './tcp.ts';
import { isCuratedHost, lookupCurated, curatedNotFound, corsErrorPage } from './canned-zone.ts';

// Allow tests to stub fetch. Default to the global.
export type FetchFn = (input: string, init?: { method?: string; mode?: RequestMode }) => Promise<Response>;

export interface HttpProxyOptions {
  fetch?: FetchFn;
  onRequest?: (host: string, method: string, path: string, source: 'curated' | 'fetch') => void;
  onResponse?: (host: string, status: number, length: number) => void;
}

export function createHttpProxyListener(opts: HttpProxyOptions = {}): TcpListenerHandler {
  const fetchImpl: FetchFn = opts.fetch ?? ((typeof window !== 'undefined' ? window.fetch.bind(window) : (() => Promise.reject(new Error('fetch unavailable')))));
  return {
    onConnect(conn: TcpConn): void {
      let buf = new Uint8Array(0);
      let handled = false;
      conn.onData = (chunk: Uint8Array) => {
        if (handled) return;
        const next = new Uint8Array(buf.length + chunk.length);
        next.set(buf, 0); next.set(chunk, buf.length);
        buf = next;
        // Look for end-of-headers.
        const idx = findHeaderEnd(buf);
        if (idx < 0) {
          // Cap pathological request growth.
          if (buf.length > 32 * 1024) {
            writeStatus(conn, 413, 'Request Too Large', 'Header bigger than 32 KiB.');
            conn.close();
            handled = true;
          }
          return;
        }
        handled = true;
        const headerStr = decodeAscii(buf.subarray(0, idx));
        void handleRequest(conn, headerStr, fetchImpl, opts);
      };
      conn.onClose = () => {
        // Peer hung up before we responded — nothing to do.
      };
    },
  };
}

async function handleRequest(conn: TcpConn, header: string, fetchImpl: FetchFn, opts: HttpProxyOptions): Promise<void> {
  const lines = header.split(/\r?\n/);
  const requestLine = lines[0] ?? '';
  const m = /^(\S+)\s+(\S+)\s+HTTP\/(\d)\.(\d)/.exec(requestLine);
  if (!m) {
    writeStatus(conn, 400, 'Bad Request', 'Could not parse request line.');
    conn.close(); return;
  }
  const method = m[1].toUpperCase();
  const path   = m[2];
  // Parse Host header (case-insensitive).
  let host = '';
  for (let i = 1; i < lines.length; i++) {
    const line = lines[i];
    const colon = line.indexOf(':');
    if (colon < 0) continue;
    const key = line.slice(0, colon).trim().toLowerCase();
    const val = line.slice(colon + 1).trim();
    if (key === 'host') { host = val.toLowerCase(); break; }
  }
  // If no Host (HTTP/0.9-style) and the path is absolute-URI,
  // pull host from it.
  if (!host && /^https?:\/\//i.test(path)) {
    const u = path.replace(/^https?:\/\//i, '');
    const slash = u.indexOf('/');
    host = (slash < 0 ? u : u.slice(0, slash)).toLowerCase();
  }
  if (!host) host = 'canned.psion.local';

  if (method !== 'GET' && method !== 'HEAD') {
    writeStatus(conn, 501, 'Not Implemented', `Method ${method} not supported by the simulator proxy.`);
    conn.close(); return;
  }

  // Curated zone short-circuit — no fetch, no CORS.
  if (isCuratedHost(host)) {
    const body = lookupCurated(host, path);
    if (body !== null) {
      opts.onRequest?.(host, method, path, 'curated');
      writeHtml(conn, 200, 'OK', body, method);
      opts.onResponse?.(host, 200, body.length);
      conn.close(); return;
    }
    const notFound = curatedNotFound(host, path);
    opts.onRequest?.(host, method, path, 'curated');
    writeHtml(conn, 404, 'Not Found', notFound, method);
    opts.onResponse?.(host, 404, notFound.length);
    conn.close(); return;
  }

  // Forward to the browser's fetch — best-effort, will hit CORS on
  // most real hosts.
  opts.onRequest?.(host, method, path, 'fetch');
  const url = `http://${host}${path.startsWith('/') ? path : '/' + path}`;
  try {
    const resp = await fetchImpl(url, { method, mode: 'cors' });
    // Read body as Uint8Array.
    const buf = new Uint8Array(await resp.arrayBuffer());
    const status = resp.status;
    const statusText = resp.statusText || statusReason(status);
    const contentType = resp.headers.get('content-type') ?? 'application/octet-stream';
    writeResponse(conn, status, statusText, buf, contentType, method);
    opts.onResponse?.(host, status, buf.length);
  } catch (e) {
    const reason = e instanceof Error ? e.message : String(e);
    const body = corsErrorPage(host, path, reason);
    writeHtml(conn, 502, 'Bad Gateway', body, method);
    opts.onResponse?.(host, 502, body.length);
  }
  conn.close();
}

function writeHtml(conn: TcpConn, status: number, reason: string, body: string, method: string): void {
  const bytes = encodeUtf8(body);
  writeResponse(conn, status, reason, bytes, 'text/html; charset=utf-8', method);
}

function writeStatus(conn: TcpConn, status: number, reason: string, message: string): void {
  const body = `<html><body><h1>${status} ${reason}</h1><p>${message}</p></body></html>`;
  writeHtml(conn, status, reason, body, 'GET');
}

function writeResponse(conn: TcpConn, status: number, reason: string, body: Uint8Array, contentType: string, method: string): void {
  const headers =
    `HTTP/1.0 ${status} ${reason}\r\n` +
    `Content-Type: ${contentType}\r\n` +
    `Content-Length: ${body.length}\r\n` +
    `Connection: close\r\n` +
    `Server: psion-emu-modem/1\r\n` +
    `\r\n`;
  conn.send(encodeAscii(headers));
  if (method !== 'HEAD' && body.length > 0) conn.send(body);
}

function findHeaderEnd(buf: Uint8Array): number {
  // Look for \r\n\r\n; tolerate \n\n.
  for (let i = 0; i + 3 < buf.length; i++) {
    if (buf[i] === 0x0D && buf[i + 1] === 0x0A && buf[i + 2] === 0x0D && buf[i + 3] === 0x0A) return i;
  }
  for (let i = 0; i + 1 < buf.length; i++) {
    if (buf[i] === 0x0A && buf[i + 1] === 0x0A) return i;
  }
  return -1;
}

function decodeAscii(buf: Uint8Array): string {
  let s = '';
  for (let i = 0; i < buf.length; i++) s += String.fromCharCode(buf[i]);
  return s;
}
function encodeAscii(s: string): Uint8Array {
  const out = new Uint8Array(s.length);
  for (let i = 0; i < s.length; i++) out[i] = s.charCodeAt(i) & 0xFF;
  return out;
}
function encodeUtf8(s: string): Uint8Array {
  return (typeof TextEncoder !== 'undefined') ? new TextEncoder().encode(s) : encodeAscii(s);
}

function statusReason(status: number): string {
  switch (status) {
    case 200: return 'OK';
    case 301: return 'Moved Permanently';
    case 302: return 'Found';
    case 304: return 'Not Modified';
    case 400: return 'Bad Request';
    case 401: return 'Unauthorized';
    case 403: return 'Forbidden';
    case 404: return 'Not Found';
    case 500: return 'Internal Server Error';
    case 502: return 'Bad Gateway';
    case 503: return 'Service Unavailable';
    default:  return '';
  }
}
