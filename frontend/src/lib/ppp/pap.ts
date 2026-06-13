// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// PAP (RFC 1334) — accept-all server.
//
// PAP runs after LCP comes up if the peer agreed to OPT_AUTH_PROTOCOL =
// 0xC023 during LCP. The peer sends Authenticate-Request with a
// peer-id/password tuple; we respond with Authenticate-Ack.
//
// This implementation accepts any credentials.

export const PAP_AUTH_REQ = 1;
export const PAP_AUTH_ACK = 2;
export const PAP_AUTH_NAK = 3;

export interface PapEvent {
  send?: Uint8Array;
  authenticated?: boolean;
  peerId?: string;
}

export function recvPap(info: Uint8Array): PapEvent[] {
  if (info.length < 4) return [];
  const code = info[0];
  const id   = info[1];
  // length field at info[2..3] — not needed since we trust the FCS-
  // validated frame size.
  if (code !== PAP_AUTH_REQ) return [];
  let peerId = '';
  // Body: peer-id-len(1) peer-id(N) passwd-len(1) passwd(M)
  let p = 4;
  if (p < info.length) {
    const plen = info[p]; p += 1;
    if (p + plen <= info.length) {
      let s = '';
      for (let i = 0; i < plen; i++) s += String.fromCharCode(info[p + i]);
      peerId = s;
      p += plen;
    }
  }

  // Build Authenticate-Ack with a friendly message.
  const msg = 'Login OK';
  const body = new Uint8Array(1 + msg.length);
  body[0] = msg.length;
  for (let i = 0; i < msg.length; i++) body[1 + i] = msg.charCodeAt(i) & 0xFF;

  const out = new Uint8Array(4 + body.length);
  out[0] = PAP_AUTH_ACK;
  out[1] = id;
  out[2] = ((4 + body.length) >>> 8) & 0xFF;
  out[3] = (4 + body.length) & 0xFF;
  out.set(body, 4);

  return [{ send: out, authenticated: true, peerId }];
}
