// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Minimal DNS server — A-record queries only, returns curated zone or
// wildcard catch-all.
//
// We only parse enough of the query to extract the questioned name and
// echo the question section back into the response. QTYPE/QCLASS are
// trusted (we always answer with A/IN regardless).
//
// Curated zone:
//   canned.psion.local → 192.168.42.10
//   news.psion.local   → 192.168.42.11
//   mail.psion.local   → 192.168.42.12
//   *  (catch-all)     → 192.168.42.10   (the HTTP fetch-proxy host)
//
// All curated IPs route to the HTTP proxy at the TCP layer — the
// per-name IPs are cosmetic so the device's connection log shows a
// stable-looking remote.

export const FAKE_HOST_HTTP = [192, 168, 42, 10] as const;
export const FAKE_HOST_NEWS = [192, 168, 42, 11] as const;
export const FAKE_HOST_MAIL = [192, 168, 42, 12] as const;

const CURATED: Record<string, readonly number[]> = {
  'canned.psion.local': FAKE_HOST_HTTP,
  'news.psion.local':   FAKE_HOST_NEWS,
  'mail.psion.local':   FAKE_HOST_MAIL,
};

// Returns a DNS response payload for the given DNS query payload,
// or null if the query is malformed. Anything that isn't a single
// A/IN question for a known or wildcarded name still gets answered —
// real DNS would NXDOMAIN, but we route everything to the fetch proxy
// so the HTTP layer can decide what to do.
export function handleDnsQuery(query: Uint8Array): Uint8Array | null {
  if (query.length < 12) return null;
  const id = (query[0] << 8) | query[1];
  // We don't validate flags — just echo the question back and set our
  // own response flags.
  const qdcount = (query[4] << 8) | query[5];
  if (qdcount !== 1) return null;

  // Parse the question's name.
  const name = parseName(query, 12);
  if (!name) return null;
  const qend = name.endOffset;
  if (qend + 4 > query.length) return null;
  // Skip QTYPE (2) + QCLASS (2). We don't check; we always answer A/IN.
  const questionEnd = qend + 4;

  const ip = CURATED[name.name.toLowerCase()] ?? FAKE_HOST_HTTP;

  // Build response: header (12) + question (echoed) + answer (RR).
  // Answer RR layout (with name compression via 0xC00C pointer to the
  // question's name):
  //   pointer(2) TYPE(2) CLASS(2) TTL(4) RDLENGTH(2) RDATA(4)
  const answerLen = 2 + 2 + 2 + 4 + 2 + 4;
  const out = new Uint8Array(questionEnd + answerLen);

  // Header
  out[0] = (id >>> 8) & 0xFF; out[1] = id & 0xFF;
  out[2] = 0x81; out[3] = 0x80;   // QR=1, AA=0, RD=1, RA=1, RCODE=0
  out[4] = 0; out[5] = 1;          // QDCOUNT=1
  out[6] = 0; out[7] = 1;          // ANCOUNT=1
  out[8] = 0; out[9] = 0;          // NSCOUNT=0
  out[10] = 0; out[11] = 0;        // ARCOUNT=0

  // Question: copy bytes 12..questionEnd verbatim.
  out.set(query.subarray(12, questionEnd), 12);

  // Answer
  let p = questionEnd;
  out[p++] = 0xC0; out[p++] = 0x0C;        // pointer to question name (offset 12)
  out[p++] = 0; out[p++] = 1;              // TYPE A
  out[p++] = 0; out[p++] = 1;              // CLASS IN
  out[p++] = 0; out[p++] = 0; out[p++] = 1; out[p++] = 0x2C;  // TTL 300
  out[p++] = 0; out[p++] = 4;              // RDLENGTH 4
  out[p++] = ip[0]; out[p++] = ip[1]; out[p++] = ip[2]; out[p++] = ip[3];

  return out;
}

interface ParsedName { name: string; endOffset: number; }

function parseName(buf: Uint8Array, start: number): ParsedName | null {
  const labels: string[] = [];
  let p = start;
  const guard = start + 256;  // safety
  while (p < buf.length && p < guard) {
    const len = buf[p];
    if (len === 0) { p += 1; return { name: labels.join('.'), endOffset: p }; }
    // Compression pointer — phase 2 doesn't need to follow it for the
    // question-section parse (questions don't use compression per RFC
    // 1035). Reject for safety.
    if ((len & 0xC0) !== 0) return null;
    if (p + 1 + len > buf.length) return null;
    let label = '';
    for (let i = 0; i < len; i++) label += String.fromCharCode(buf[p + 1 + i]);
    labels.push(label);
    p += 1 + len;
  }
  return null;
}
