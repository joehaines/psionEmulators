// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Shared types for the inet (IP/ICMP/UDP/TCP) layer of the modem path.
// Kept in its own file so layers above and below don't end up importing
// each other just for the type names.

export const IP_PROTO_ICMP = 1;
export const IP_PROTO_TCP  = 6;
export const IP_PROTO_UDP  = 17;

export interface Ipv4Header {
  version: number;
  ihl: number;          // header length in 32-bit words
  tos: number;
  totalLength: number;
  identification: number;
  flags: number;
  fragmentOffset: number;
  ttl: number;
  protocol: number;
  headerChecksum: number;
  src: Uint8Array;      // 4 bytes
  dst: Uint8Array;      // 4 bytes
}

export interface Ipv4Packet {
  header: Ipv4Header;
  payload: Uint8Array;
}

// Callback that a layer can use to emit an outgoing IP packet back
// onto the wire (down to PPP → HDLC → UART). Phase 1 only emits
// ICMP echo replies; phase 2 will add TCP/UDP transmission.
export type EmitIpPacket = (proto: number, src: Uint8Array, dst: Uint8Array, payload: Uint8Array) => void;
