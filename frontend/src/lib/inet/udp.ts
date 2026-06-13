// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// UDP per RFC 768.
//
// 8-byte header: src port, dst port, length, checksum.
// Checksum uses the IPv4 pseudo-header (src IP + dst IP + 0 + proto +
// UDP length). An all-zeros checksum on the wire means "not computed";
// we accept incoming with checksum 0 and don't validate (the link is
// loopback over PPP, no corruption risk).

import { onesComplementChecksum } from './ip.ts';

export interface UdpSegment {
  srcPort: number;
  dstPort: number;
  payload: Uint8Array;
}

export function parseUdp(raw: Uint8Array): UdpSegment | null {
  if (raw.length < 8) return null;
  const srcPort = (raw[0] << 8) | raw[1];
  const dstPort = (raw[2] << 8) | raw[3];
  const length  = (raw[4] << 8) | raw[5];
  if (length < 8 || length > raw.length) return null;
  return { srcPort, dstPort, payload: raw.subarray(8, length) };
}

export function buildUdp(srcIp: Uint8Array, dstIp: Uint8Array, seg: UdpSegment): Uint8Array {
  const len = 8 + seg.payload.length;
  const out = new Uint8Array(len);
  out[0] = (seg.srcPort >>> 8) & 0xFF;
  out[1] = seg.srcPort & 0xFF;
  out[2] = (seg.dstPort >>> 8) & 0xFF;
  out[3] = seg.dstPort & 0xFF;
  out[4] = (len >>> 8) & 0xFF;
  out[5] = len & 0xFF;
  // Checksum field at [6..7] left zero for the computation.
  out.set(seg.payload, 8);
  const csum = pseudoChecksum(srcIp, dstIp, 17, out);
  out[6] = (csum >>> 8) & 0xFF;
  out[7] = csum & 0xFF;
  return out;
}

// Shared with TCP — the pseudo-header construction.
export function pseudoChecksum(srcIp: Uint8Array, dstIp: Uint8Array, proto: number, segment: Uint8Array): number {
  const pseudo = new Uint8Array(12 + segment.length);
  pseudo.set(srcIp, 0);
  pseudo.set(dstIp, 4);
  pseudo[8] = 0;
  pseudo[9] = proto;
  pseudo[10] = (segment.length >>> 8) & 0xFF;
  pseudo[11] = segment.length & 0xFF;
  pseudo.set(segment, 12);
  return onesComplementChecksum(pseudo);
}
