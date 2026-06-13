// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// NCP (Network Control Protocol) packet layer — sits on top of the
// PLP link-layer framing in framing.ts.
//
// Each NCP packet (i.e. the payload of a single PLP frame) starts with
// a 3-byte header:
//
//   +-------------+-------------+--------+---------+
//   | remoteChan  | localChan   | type   | data... |
//   +-------------+-------------+--------+---------+
//
// `remoteChan` is the channel ID on the Revo's side, `localChan` is the
// channel ID we picked on the host side. `type` is one of `NcpType`.
//
// Channel 0 is the control channel used for the initial CONNECT
// handshake. To open a service, the host sends NCON_REQ with
// localChan=N (a host-assigned channel number) and the ASCII service
// name as the data payload. The Revo replies with NCON_ACK echoing
// our localChan and assigning its own remote channel number. After
// that the two ends address each other by the pair (theirChan, ourChan)
// and exchange NDATA messages.

import { NcpType } from './types.ts';

export interface NcpPacket {
  remoteChan: number;
  localChan: number;
  type: number;
  data: Uint8Array;
}

export function encodeNcp(p: NcpPacket): Uint8Array {
  const out = new Uint8Array(3 + p.data.length);
  out[0] = p.remoteChan & 0xFF;
  out[1] = p.localChan & 0xFF;
  out[2] = p.type & 0xFF;
  out.set(p.data, 3);
  return out;
}

export function decodeNcp(payload: Uint8Array): NcpPacket | null {
  if (payload.length < 3) return null;
  return {
    remoteChan: payload[0],
    localChan:  payload[1],
    type:       payload[2],
    data:       payload.subarray(3),
  };
}

// ── Connection helpers ─────────────────────────────────────────────
// The CONNECT exchange asks the remote side to open a named service.
// Service names are ASCII strings (e.g. "SYS$RFSV.*"), null-terminated
// in the on-the-wire payload.

export function makeConnectRequest(localChan: number, serviceName: string): NcpPacket {
  const name = new TextEncoder().encode(serviceName);
  const data = new Uint8Array(name.length + 1);
  data.set(name, 0);
  data[name.length] = 0; // C-string terminator
  return {
    remoteChan: 0,   // address control channel on the Revo
    localChan,
    type: NcpType.NCON_REQ,
    data,
  };
}

export function makeDataPacket(remoteChan: number, localChan: number, data: Uint8Array): NcpPacket {
  return { remoteChan, localChan, type: NcpType.NDATA, data };
}

export function makeDisconnect(remoteChan: number, localChan: number): NcpPacket {
  return {
    remoteChan,
    localChan,
    type: NcpType.NDIS_REQ,
    data: new Uint8Array(0),
  };
}

// ── Channel manager ────────────────────────────────────────────────
// Tracks open channels and dispatches inbound packets to their
// listeners. Keeps the NCP layer protocol-aware without coupling to
// the higher layers (RFSV, RPCS) — those plug in via openChannel().

export type ChannelHandler = (data: Uint8Array) => void;

export interface OpenChannel {
  localChan: number;
  remoteChan: number;     // assigned by the remote side on NCON_ACK
  serviceName: string;
  state: 'pending' | 'open' | 'closed';
}

export class NcpMultiplexer {
  private nextLocalChan = 1; // 0 reserved for the control channel
  private channels = new Map<number, OpenChannel>(); // keyed by localChan
  private handlers = new Map<number, ChannelHandler>(); // keyed by localChan
  private pendingConnects = new Map<number, (chan: OpenChannel) => void>();

  // Caller wires this up to send a frame onto the wire.
  // Explicit field assignment (rather than a parameter property) so
  // the file loads under Node's strip-only TS mode in the e2e harness.
  private readonly sendFrame: (payload: Uint8Array) => void;
  constructor(sendFrame: (payload: Uint8Array) => void) {
    this.sendFrame = sendFrame;
  }

  // Initiate a CONNECT for `serviceName`. Resolves once the remote
  // sends NCON_ACK with the channel pair fully populated. Caller can
  // then register a handler via setHandler() and start exchanging
  // NDATA messages.
  openChannel(serviceName: string, handler: ChannelHandler): Promise<OpenChannel> {
    const localChan = this.nextLocalChan++;
    const chan: OpenChannel = {
      localChan,
      remoteChan: 0,
      serviceName,
      state: 'pending',
    };
    this.channels.set(localChan, chan);
    this.handlers.set(localChan, handler);
    this.sendFrame(encodeNcp(makeConnectRequest(localChan, serviceName)));
    return new Promise(resolve => {
      this.pendingConnects.set(localChan, resolve);
    });
  }

  // Send NDATA for an already-open channel.
  send(localChan: number, data: Uint8Array): boolean {
    const chan = this.channels.get(localChan);
    if (!chan || chan.state !== 'open') return false;
    this.sendFrame(encodeNcp(makeDataPacket(chan.remoteChan, chan.localChan, data)));
    return true;
  }

  // Close a channel. The remote may also close it independently.
  close(localChan: number): void {
    const chan = this.channels.get(localChan);
    if (!chan || chan.state === 'closed') return;
    this.sendFrame(encodeNcp(makeDisconnect(chan.remoteChan, chan.localChan)));
    chan.state = 'closed';
  }

  // Feed an inbound frame payload (post-framing) into the multiplexer.
  // Returns false if the packet is malformed.
  //
  // Header POV note: every NCP packet is [destChan, srcChan, type].
  // The sender fills the destination into the `remoteChan` slot and
  // its own ID into `localChan`. On receive those labels swap meaning
  // — pkt.remoteChan names a channel on the *receiver's* side (us),
  // and pkt.localChan names a channel on the *sender's* side (them).
  // So our own-channel lookups go through pkt.remoteChan, and the
  // sender's ID lives in pkt.localChan. This caught a bug end-to-end
  // in plp.e2e.test.mts before any real Revo capture was needed.
  handleFrame(payload: Uint8Array): boolean {
    const pkt = decodeNcp(payload);
    if (!pkt) return false;
    const ourChan = pkt.remoteChan;   // "destination" from the wire = us
    const theirChan = pkt.localChan;  // "source" from the wire = them
    switch (pkt.type) {
      case NcpType.NCON_ACK: {
        // The remote echoes our channel in the destination slot and
        // names its own chosen channel in the source slot. Some
        // captures also place the remote channel in the first byte of
        // `data`; prefer that when present (covers either layout).
        const chan = this.channels.get(ourChan);
        if (!chan) return false;
        chan.remoteChan = pkt.data.length > 0 ? pkt.data[0] : theirChan;
        chan.state = 'open';
        const resolver = this.pendingConnects.get(ourChan);
        if (resolver) {
          this.pendingConnects.delete(ourChan);
          resolver(chan);
        }
        return true;
      }
      case NcpType.NDATA: {
        const handler = this.handlers.get(ourChan);
        if (handler) handler(pkt.data);
        return true;
      }
      case NcpType.NDIS_REQ:
      case NcpType.NDIS_ACK: {
        const chan = this.channels.get(ourChan);
        if (chan) chan.state = 'closed';
        return true;
      }
      default:
        return true; // ignore unknown types for now
    }
  }

  openChannels(): OpenChannel[] {
    return [...this.channels.values()].filter(c => c.state !== 'closed');
  }
}
