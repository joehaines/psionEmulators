// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Minimal ICMP — echo-request handler only.
//
// ER5's Web app pings the gateway before opening sockets; without this
// the first HTTP request will time out. Once phase 2 brings up TCP,
// many apps don't ping at all, but it's cheap to support.

import { onesComplementChecksum } from './ip.ts';
import type { Ipv4Packet } from './types.ts';

const ICMP_ECHO_REQUEST = 8;
const ICMP_ECHO_REPLY   = 0;

// Returns an ICMP payload (to be wrapped in IPv4) for the echo
// reply, or null if the inbound packet isn't an echo request.
export function handleIcmp(pkt: Ipv4Packet): Uint8Array | null {
  const p = pkt.payload;
  if (p.length < 8) return null;
  if (p[0] !== ICMP_ECHO_REQUEST) return null;
  const reply = new Uint8Array(p.length);
  reply.set(p);
  reply[0] = ICMP_ECHO_REPLY;
  reply[1] = 0;
  reply[2] = 0; reply[3] = 0;   // checksum = 0 for recompute
  const csum = onesComplementChecksum(reply);
  reply[2] = (csum >>> 8) & 0xFF;
  reply[3] = csum & 0xFF;
  return reply;
}
