// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// IPv4 header parse/build + checksum + protocol demux.
//
// Phase 1 scope: parse incoming packets, validate checksum, dispatch
// by protocol; build outgoing packets for ICMP responses. TCP/UDP
// listeners will plug into the same demux table in phase 2.

import { type Ipv4Packet, IP_PROTO_ICMP } from './types.ts';

export function parseIpv4(raw: Uint8Array): Ipv4Packet | null {
  if (raw.length < 20) return null;
  const verIhl = raw[0];
  const version = (verIhl >>> 4) & 0xF;
  if (version !== 4) return null;
  const ihl = verIhl & 0xF;
  if (ihl < 5) return null;
  const headerLen = ihl * 4;
  if (raw.length < headerLen) return null;
  const totalLength = (raw[2] << 8) | raw[3];
  if (totalLength < headerLen || totalLength > raw.length) return null;
  // Verify header checksum.
  if (!verifyChecksum(raw.subarray(0, headerLen))) return null;
  const flagsFrag = (raw[6] << 8) | raw[7];
  return {
    header: {
      version,
      ihl,
      tos: raw[1],
      totalLength,
      identification: (raw[4] << 8) | raw[5],
      flags: (flagsFrag >>> 13) & 0x7,
      fragmentOffset: flagsFrag & 0x1FFF,
      ttl: raw[8],
      protocol: raw[9],
      headerChecksum: (raw[10] << 8) | raw[11],
      src: raw.subarray(12, 16),
      dst: raw.subarray(16, 20),
    },
    payload: raw.subarray(headerLen, totalLength),
  };
}

export function buildIpv4(
  protocol: number,
  src: Uint8Array,
  dst: Uint8Array,
  payload: Uint8Array,
  ident: number = (Math.random() * 0xFFFF) | 0,
): Uint8Array {
  const totalLen = 20 + payload.length;
  const out = new Uint8Array(totalLen);
  out[0] = 0x45; // version=4, IHL=5
  out[1] = 0;
  out[2] = (totalLen >>> 8) & 0xFF;
  out[3] = totalLen & 0xFF;
  out[4] = (ident >>> 8) & 0xFF;
  out[5] = ident & 0xFF;
  out[6] = 0x40; // Don't Fragment
  out[7] = 0;
  out[8] = 64;   // TTL
  out[9] = protocol;
  // header checksum at [10..11] — leave 0 for computation
  out.set(src, 12);
  out.set(dst, 16);
  const csum = onesComplementChecksum(out.subarray(0, 20));
  out[10] = (csum >>> 8) & 0xFF;
  out[11] = csum & 0xFF;
  out.set(payload, 20);
  return out;
}

// One's-complement 16-bit checksum over a byte buffer. Used for the
// IPv4 header and (with a pseudo-header) for ICMP/TCP/UDP.
export function onesComplementChecksum(buf: Uint8Array): number {
  let sum = 0;
  let i = 0;
  while (i + 1 < buf.length) {
    sum += (buf[i] << 8) | buf[i + 1];
    if (sum > 0xFFFF) sum = (sum & 0xFFFF) + 1;
    i += 2;
  }
  if (i < buf.length) {
    sum += buf[i] << 8;
    if (sum > 0xFFFF) sum = (sum & 0xFFFF) + 1;
  }
  return (~sum) & 0xFFFF;
}

function verifyChecksum(header: Uint8Array): boolean {
  // The header already contains its checksum; computing again should
  // yield 0.
  let sum = 0;
  for (let i = 0; i + 1 < header.length; i += 2) {
    sum += (header[i] << 8) | header[i + 1];
    if (sum > 0xFFFF) sum = (sum & 0xFFFF) + 1;
  }
  return sum === 0xFFFF;
}

// Demux dispatcher. Phase 1 hands ICMP off; everything else returns
// null (TCP/UDP arrive in phase 2).
export interface IpListeners {
  onIcmp?: (pkt: Ipv4Packet) => Uint8Array | null;
}
export function demuxIp(pkt: Ipv4Packet, listeners: IpListeners): Uint8Array | null {
  if (pkt.header.protocol === IP_PROTO_ICMP && listeners.onIcmp) {
    return listeners.onIcmp(pkt);
  }
  return null;
}
