// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// PLP session layer (NCP) — wire-correct rewrite of the legacy ncp.ts.
//
// Built on top of LinkLayer. Owns channel multiplexing, the Connect
// handshake for higher-level servers (SYS$RFSV.*, LINK.*, etc.), and
// fragmentation/reassembly of large packets across Complete/Partial
// frame pairs.
//
// Verified end-to-end against the real Revo: see harness/run.cpp's
// --serial-auto-rule transcripts in commit history. The frame types
// here are the spec values (NCP.CONNECT=0x03 etc), not the legacy
// (wrong) NcpType.* aliases.

import { LinkLayer } from './link.ts';
import {
  NCP, NCP_CONTROL_CHAN, NCP_LINK_CHAN, NCP_VERSION,
  NCP_SERVICE_LINK, NCP_SERVICE_RFSV,
} from './types.ts';

// Re-export common constants so importers don't need both modules.
export { NCP_SERVICE_LINK, NCP_SERVICE_RFSV };

// ── NCP packet codec ──────────────────────────────────────────────
export interface NcpPacket {
  destChan: number;
  srcChan: number;
  type: number;        // NCP.* value
  data: Uint8Array;
}

export function encodeNcp(p: NcpPacket): Uint8Array {
  const out = new Uint8Array(3 + p.data.length);
  out[0] = p.destChan & 0xFF;
  out[1] = p.srcChan & 0xFF;
  out[2] = p.type & 0xFF;
  out.set(p.data, 3);
  return out;
}

export function decodeNcp(payload: Uint8Array): NcpPacket | null {
  if (payload.length < 3) return null;
  return {
    destChan: payload[0],
    srcChan:  payload[1],
    type:     payload[2],
    data:     payload.subarray(3),
  };
}

// ── Specific NCP packet types ────────────────────────────────────
// Connect frame: `0x00 ClientChan 0x03 ServerName\0`. The destChan is
// always 0x00 (the control channel routes Connect requests to the
// LINK service / NCP itself).
export function makeConnectPacket(clientChan: number, serverName: string): NcpPacket {
  const name = new TextEncoder().encode(serverName);
  const data = new Uint8Array(name.length + 1);
  data.set(name, 0);
  data[name.length] = 0;       // NUL-terminated per the spec
  return {
    destChan: NCP_CONTROL_CHAN,
    srcChan: clientChan,
    type: NCP.CONNECT,
    data,
  };
}

// Connect Response: `0x00 ServerChan 0x04 ClientChan StatusCode-byte`.
// SrcChan is the server's chosen channel for the connection when
// status == 0 (success); the spec says it's set to 0x00 on failure.
// StatusCode is 1 byte (E_SIBO_NONE = 0, non-zero = error).
export function makeConnectResponsePacket(
  serverChan: number,
  clientChan: number,
  status: number,
): NcpPacket {
  return {
    destChan: NCP_CONTROL_CHAN,
    srcChan: serverChan,
    type: NCP.CONNECT_RESPONSE,
    data: new Uint8Array([clientChan & 0xFF, status & 0xFF]),
  };
}

// NCP Information frame: `0x00 0x00 0x06 Version-byte ID-4bytes`.
// Sent as the first packet after a fresh link comes up; both ends do
// it. Use NCP_VERSION.EPOC_ER3 (0x06) on EPOC variant.
export function makeNcpInfoPacket(version: number, id: Uint8Array): NcpPacket {
  if (id.length !== 4) throw new Error('NCP Info ID must be 4 bytes');
  const data = new Uint8Array(5);
  data[0] = version & 0xFF;
  data.set(id, 1);
  return {
    destChan: NCP_CONTROL_CHAN,
    srcChan: NCP_CONTROL_CHAN,
    type: NCP.INFO,
    data,
  };
}

// Complete frame: payload-bearing data for an open channel.
// `DestChan SrcChan 0x01 Data`. Used to deliver RFSV-32 commands and
// replies once a channel has been opened.
export function makeCompletePacket(destChan: number, srcChan: number, data: Uint8Array): NcpPacket {
  return { destChan, srcChan, type: NCP.COMPLETE, data };
}
// Partial frame: a non-final fragment of a larger message. Receivers
// queue Partial frames keyed by (destChan, srcChan) and concatenate
// them onto the eventual Complete frame's payload to reconstruct the
// full RFSV message.
export function makePartialPacket(destChan: number, srcChan: number, data: Uint8Array): NcpPacket {
  return { destChan, srcChan, type: NCP.PARTIAL, data };
}

// Maximum NCP user-data bytes per data-link frame. The spec caps the
// frame's Data field at 300 bytes before stuffing; subtract the 3-byte
// NCP header (destChan + srcChan + type) and we have 297 bytes for
// the payload. Verified on the real Revo: a WRITE_FILE message split
// at this boundary into Partial + Complete frames goes through, while
// the same message sent as a single oversized Complete is silently
// dropped (no reply).
export const NCP_MAX_PAYLOAD = 297;

// ── Channel manager ─────────────────────────────────────────────────
// Tracks open NCP channels, dispatches incoming packets to the right
// handler, and provides openServer() for the RFSV layer above.

export type NcpDataHandler = (data: Uint8Array) => void;

interface OpenChannelInfo {
  serverChan: number;       // assigned by the remote on Connect Response
  clientChan: number;       // our local channel ID
  serverName: string;
  state: 'pending' | 'open' | 'closed';
  handler: NcpDataHandler;
  // Promise resolver for the in-flight Connect Request.
  resolveConnect?: (success: boolean) => void;
}

// Result of a LINK Register command (see linkRegister below).
export interface LinkRegisterResult {
  status: number;        // u16 status code from the reply; 0 = success
  serverName: string;    // name to use in the Connect frame ('' if absent)
}

export class Ncp {
  private link: LinkLayer;
  private nextClientChan = 2;  // 0 = control, 1 = LINK; client side picks 2+
  // Lowest channel the allocator may hand out. Raised (randomised)
  // after a link adoption — the device still holds the dead session's
  // channels, and a stale retransmitted reply on a reused channel id
  // would be matched against our own in-flight request.
  private clientChanFloor = 2;
  private byClient: Map<number, OpenChannelInfo> = new Map();
  // In-flight LINK-channel Register commands, keyed by Operation ID.
  private pendingLinkOps: Map<number, {
    resolve: (r: LinkRegisterResult) => void;
    reject: (e: Error) => void;
  }> = new Map();
  private nextLinkOpId = 1;
  // Reassembly buffer for incoming Partial-frame sequences, keyed by
  // (destChan,srcChan) pair. We currently never see fragmentation in
  // captures, but the spec allows it so wire it up.
  private partialBuf: Map<string, Uint8Array[]> = new Map();
  // Set after the first NCP Information frame arrives from the device.
  private peerInfoSeen = false;
  // The four-byte NCP ID we advertise.
  private localNcpId: Uint8Array;

  // NCP Information version byte we advertise: EPOC_ER3 (0x06) on the
  // EPOC variant, SIBO_NEW (0x03) on EPOC16 SIBO devices (matches what
  // the 3c v5.20f kernel itself announces).
  private infoVersion: number;

  constructor(link: LinkLayer, infoVersion: number = NCP_VERSION.EPOC_ER3) {
    this.link = link;
    this.infoVersion = infoVersion;
    this.localNcpId = new Uint8Array(4);
    for (let i = 0; i < 4; i++) this.localNcpId[i] = Math.floor(Math.random() * 256);
    // Plug into the LinkLayer's onData stream.
    // We do this by replacing the LinkLayer's onData callback —
    // the original handler stays in cfg.onData and is forwarded
    // for any non-NCP traffic, but PLP only has NCP above the link.
    (this.link as unknown as { cfg: { onData: (p: Uint8Array) => void } })
      .cfg.onData = p => this.dispatchPacket(p);
  }

  // Open a connection to the named server. Resolves with the assigned
  // server channel on success, rejects on failure.
  async connectServer(serverName: string): Promise<number> {
    const clientChan = this.nextClientChan++;
    return new Promise<number>((resolve, reject) => {
      const info: OpenChannelInfo = {
        serverChan: 0,
        clientChan,
        serverName,
        state: 'pending',
        handler: () => { /* set later */ },
        resolveConnect: ok => {
          if (ok) resolve(info.serverChan);
          else reject(new Error(`NCP Connect for ${serverName} rejected`));
        },
      };
      this.byClient.set(clientChan, info);
      this.send(makeConnectPacket(clientChan, serverName));
    });
  }

  // Set the data handler for an established channel.
  setHandler(clientChan: number, handler: NcpDataHandler): void {
    const info = this.byClient.get(clientChan);
    if (!info) throw new Error(`setHandler: unknown channel ${clientChan}`);
    info.handler = handler;
  }

  // Send data on an open channel. Messages up to NCP_MAX_PAYLOAD (297
  // bytes) go as a single Complete frame; larger messages are
  // automatically fragmented into Partial frames followed by a final
  // Complete carrying the tail. The receiver's queuePartial /
  // dispatchPacket pair reassembles them.
  sendOn(clientChan: number, data: Uint8Array): void {
    const info = this.byClient.get(clientChan);
    if (!info || info.state !== 'open') {
      throw new Error(`sendOn: channel ${clientChan} not open`);
    }
    if (data.length <= NCP_MAX_PAYLOAD) {
      this.send(makeCompletePacket(info.serverChan, clientChan, data));
      return;
    }
    let off = 0;
    while (off + NCP_MAX_PAYLOAD < data.length) {
      const chunk = data.subarray(off, off + NCP_MAX_PAYLOAD);
      this.send(makePartialPacket(info.serverChan, clientChan, chunk));
      off += NCP_MAX_PAYLOAD;
    }
    this.send(makeCompletePacket(info.serverChan, clientChan, data.subarray(off)));
  }

  // LINK Register command (PLP spec "Link Register Command"): ask the
  // device's LINK server (always channel 1) to load/register a named
  // server so a subsequent Connect succeeds. Wire format:
  //   command: [0x00] [OpId u16-LE] [server name, no extension]
  //   reply:   [0x01] [OpId u16-LE] [status u16-LE] [?? u16] [name]
  // Used by the WPRT print path: SYS$WPRT isn't running until
  // registered (the spec's typical sequence is Connect → failure →
  // Register → Connect → success).
  linkRegister(serverName: string, timeoutMs = 10_000): Promise<LinkRegisterResult> {
    const opId = this.nextLinkOpId++ & 0xFFFF;
    const name = new TextEncoder().encode(serverName);
    const cmd = new Uint8Array(3 + name.length);
    cmd[0] = 0x00;
    cmd[1] = opId & 0xFF;
    cmd[2] = (opId >> 8) & 0xFF;
    cmd.set(name, 3);
    return new Promise<LinkRegisterResult>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pendingLinkOps.delete(opId);
        reject(new Error(`LINK Register ${serverName} timed out`));
      }, timeoutMs);
      this.pendingLinkOps.set(opId, {
        resolve: r => { clearTimeout(timer); resolve(r); },
        reject:  e => { clearTimeout(timer); reject(e); },
      });
      this.send(makeCompletePacket(NCP_LINK_CHAN, NCP_LINK_CHAN, cmd));
    });
  }

  // Send our NCP Information frame (host-side announcement).
  sendNcpInfo(): void {
    this.send(makeNcpInfoPacket(this.infoVersion, this.localNcpId));
  }

  // Called after a link adoption (LinkLayer.probeAdopt): shift channel
  // allocation to a random higher base, clear of whatever ids the dead
  // session's channels occupy on the device.
  randomizeClientChanBase(): void {
    this.clientChanFloor = 16 + Math.floor(Math.random() * 180);
    if (this.nextClientChan < this.clientChanFloor) {
      this.nextClientChan = this.clientChanFloor;
    }
  }

  // Returns true once we've received the peer's NCP Info.
  get peerInfoReceived(): boolean { return this.peerInfoSeen; }

  // Drop all channel state after a data-link reset. The peer's NCP sits
  // on the same link, so it reaps every channel when the link
  // re-handshakes; if we kept addressing servers by the old channel IDs
  // our packets would route nowhere. Any pending Connect is rejected so
  // its awaiter unblocks, and peerInfoSeen is cleared because the device
  // re-announces its NCP Info after the reset. PlpClient re-opens
  // SYS$RFSV.* afterwards.
  resetChannels(): void {
    for (const info of this.byClient.values()) {
      info.state = 'closed';
      const r = info.resolveConnect;
      info.resolveConnect = undefined;
      r?.(false);
    }
    this.byClient.clear();
    this.partialBuf.clear();
    this.nextClientChan = this.clientChanFloor;
    this.peerInfoSeen = false;
    for (const op of this.pendingLinkOps.values()) {
      op.reject(new Error('link reset'));
    }
    this.pendingLinkOps.clear();
  }

  // Parse a LINK-channel reply and resolve the matching Register op.
  // Reply layout: [0x01][OpId u16-LE][status u16-LE][?? u16][name...].
  // The spec says a returned name is only usable when it's at least 4
  // chars and free of control characters; otherwise the caller falls
  // back to appending ".*" to the name it registered.
  private handleLinkReply(data: Uint8Array): void {
    if (data.length < 5 || data[0] !== 0x01) return;
    const opId = data[1] | (data[2] << 8);
    const op = this.pendingLinkOps.get(opId);
    if (!op) return;
    this.pendingLinkOps.delete(opId);
    const status = data[3] | (data[4] << 8);
    let serverName = '';
    if (data.length > 7) {
      const raw = data.subarray(7);
      let ok = raw.length >= 4;
      for (const b of raw) if (b < 0x20 || b > 0x7E) { ok = false; break; }
      if (ok) serverName = new TextDecoder().decode(raw);
    }
    op.resolve({ status, serverName });
  }

  // ── internals ────────────────────────────────────────────────────
  private send(packet: NcpPacket): void {
    this.link.sendData(encodeNcp(packet));
  }

  private dispatchPacket(payload: Uint8Array): void {
    const p = decodeNcp(payload);
    if (!p) return;

    // Reassemble fragmented packets.
    if (p.type === NCP.PARTIAL) {
      this.queuePartial(p);
      return;
    }
    let data = p.data;
    const key = `${p.destChan}/${p.srcChan}`;
    if (this.partialBuf.has(key)) {
      const parts = this.partialBuf.get(key)!;
      parts.push(data);
      let total = 0;
      for (const c of parts) total += c.length;
      const merged = new Uint8Array(total);
      let off = 0;
      for (const c of parts) { merged.set(c, off); off += c.length; }
      data = merged;
      this.partialBuf.delete(key);
    }

    switch (p.type) {
      case NCP.INFO:
        this.peerInfoSeen = true;
        // Optionally check version, ID — for now just note receipt.
        return;
      case NCP.CONNECT: {
        // The peer (Revo) is requesting a connect to a server WE
        // provide. We don't currently run any host-side servers, but
        // the LINK.* announcement is harmless to acknowledge — reply
        // with a successful Connect Response so the device stops
        // retransmitting. ServerChan in the response identifies our
        // chosen channel for THEIR Connect (we just mirror their
        // ClientChan since we don't actually have a server backing
        // the channel).
        const serverName = new TextDecoder().decode(
          p.data[p.data.length - 1] === 0
            ? p.data.subarray(0, -1)
            : p.data,
        );
        // p.srcChan = the remote's client channel for this Connect.
        void serverName;  // we don't yet act on the requested service.
        this.send(makeConnectResponsePacket(p.srcChan, p.srcChan, 0));
        return;
      }
      case NCP.CONNECT_RESPONSE: {
        // Reply to our own Connect Request. p.destChan = 0x00 (control)
        // and the first byte of data is the ClientChan we used, the
        // second is the status. SrcChan is the server's chosen channel
        // if successful.
        if (p.data.length < 2) return;
        const clientChan = p.data[0];
        const status = p.data[1];
        const info = this.byClient.get(clientChan);
        if (!info) return;
        if (status === 0) {
          info.serverChan = p.srcChan;
          info.state = 'open';
          info.resolveConnect?.(true);
        } else {
          info.state = 'closed';
          info.resolveConnect?.(false);
        }
        info.resolveConnect = undefined;
        return;
      }
      case NCP.COMPLETE: {
        // LINK-channel traffic: replies to our Register commands.
        if (p.destChan === NCP_LINK_CHAN) {
          this.handleLinkReply(data);
          return;
        }
        // Data for an established channel: route to its handler.
        // The packet's destChan is OUR clientChan from the original
        // Connect (since the peer addresses us by our chosen ID).
        const info = this.byClient.get(p.destChan);
        if (info && info.state === 'open') {
          info.handler(data);
        }
        return;
      }
      case NCP.DISCONNECT:
      case NCP.CONN_TERM: {
        const info = this.byClient.get(p.destChan);
        if (info) info.state = 'closed';
        return;
      }
      case NCP.NCP_TERM:
        // Peer is shutting NCP down. Mark all channels closed.
        for (const c of this.byClient.values()) c.state = 'closed';
        return;
    }
  }

  private queuePartial(p: NcpPacket): void {
    const key = `${p.destChan}/${p.srcChan}`;
    let parts = this.partialBuf.get(key);
    if (!parts) { parts = []; this.partialBuf.set(key, parts); }
    parts.push(p.data);
  }
}
