// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Minimal TCP per RFC 793 — enough to host the HTTP fetch proxy on
// port 80 (and later SMTP/POP3 on 25/110). The wire underneath is
// PPP-over-UART loopback, so we skip the bits TCP needs for real
// networks: no retransmit timer (no packet loss), no congestion
// control (no shared link), no TIME_WAIT (no port-reuse pressure),
// no out-of-order reassembly (PPP delivers in order).
//
// State machine implemented: LISTEN ⇒ SYN_RCVD ⇒ ESTABLISHED ⇒
// CLOSE_WAIT ⇒ LAST_ACK ⇒ CLOSED, plus FIN_WAIT_1 ⇒ FIN_WAIT_2 ⇒
// CLOSED for the case where we send FIN first.

import { buildIpv4 } from './ip.ts';
import { pseudoChecksum } from './udp.ts';
import { IP_PROTO_TCP } from './types.ts';

const FLAG_FIN = 0x01;
const FLAG_SYN = 0x02;
const FLAG_RST = 0x04;
const FLAG_PSH = 0x08;
const FLAG_ACK = 0x10;

export type TcpState =
  | 'syn-rcvd' | 'established'
  | 'close-wait' | 'last-ack'
  | 'fin-wait-1' | 'fin-wait-2'
  | 'closed';

// Segments delivered to the per-connection handler.
export interface TcpListenerHandler {
  onConnect(conn: TcpConn): void;
}

export class TcpConn {
  state: TcpState = 'syn-rcvd';
  // Sequence space — RFC 793 §3.2.
  private iss: number;       // our initial send seq
  private sndUna: number;    // oldest unacked
  private sndNxt: number;    // next to send
  private rcvNxt: number;    // expected from peer
  // Application-level buffering
  private sendBuf: Uint8Array = new Uint8Array(0);
  private sendClosed: boolean = false;   // app called close()
  private finSent: boolean = false;

  // App-set callbacks
  onData?: (bytes: Uint8Array) => void;
  onClose?: () => void;

  readonly remoteIp:   Uint8Array;
  readonly remotePort: number;
  readonly localIp:    Uint8Array;
  readonly localPort:  number;
  private readonly tcp: Tcp;

  constructor(
    remoteIp: Uint8Array,
    remotePort: number,
    localIp: Uint8Array,
    localPort: number,
    tcp: Tcp,
    initialPeerSeq: number,
    _initialPeerWnd: number,
  ) {
    this.remoteIp = remoteIp;
    this.remotePort = remotePort;
    this.localIp = localIp;
    this.localPort = localPort;
    this.tcp = tcp;
    this.iss = (Math.random() * 0xFFFFFFFF) >>> 0;
    this.sndUna = this.iss;
    this.sndNxt = (this.iss + 1) >>> 0;       // SYN consumes 1
    this.rcvNxt = (initialPeerSeq + 1) >>> 0; // peer's SYN consumes 1
    // Peer window isn't currently honoured — the wire is lossless and
    // EPOC's window is generous enough that we never fill it.
  }

  // ── App-facing API ──────────────────────────────────────────────
  // Buffers data regardless of state — flushTx() guards the actual
  // transmit. Server handlers can therefore write a greeting in
  // onConnect() while we're still in syn-rcvd; it'll go out as soon
  // as the peer's final ACK promotes us to established.
  send(bytes: Uint8Array): void {
    if (bytes.length === 0) return;
    const out = new Uint8Array(this.sendBuf.length + bytes.length);
    out.set(this.sendBuf, 0); out.set(bytes, this.sendBuf.length);
    this.sendBuf = out;
    this.flushTx();
  }

  close(): void {
    if (this.sendClosed) return;
    this.sendClosed = true;
    this.flushTx();
  }

  // ── Send the initial SYN+ACK in response to peer's SYN ─────────
  sendSynAck(): void {
    // MSS option — 4 bytes: kind 2, len 4, value (1400 to stay safely
    // under PPP MRU 1500 minus IP+TCP headers).
    const mss = 1400;
    const opts = new Uint8Array([0x02, 0x04, (mss >>> 8) & 0xFF, mss & 0xFF]);
    this.tcp.emit(this, this.iss, this.rcvNxt, FLAG_SYN | FLAG_ACK, new Uint8Array(0), opts);
    // sndNxt already at iss+1.
  }

  // ── Inbound segment processing ──────────────────────────────────
  recv(seq: number, ack: number, flags: number, _window: number, payload: Uint8Array): void {
    if ((flags & FLAG_RST) !== 0) { this.cleanup(); return; }

    // Process ACK first — may advance sndUna. We don't retransmit
    // (the wire is lossless), so sendBuf isn't a "retransmit buffer" —
    // it holds application-queued bytes waiting to be emitted by
    // flushTx(). ACKs only update sndUna for sequence-tracking and
    // state-transition purposes; they don't touch sendBuf.
    if ((flags & FLAG_ACK) !== 0) {
      if (seqGt(ack, this.sndUna)) {
        this.sndUna = ack >>> 0;
        if (this.state === 'syn-rcvd') {
          this.state = 'established';
        }
        if (this.state === 'fin-wait-1' && seqGe(this.sndUna, this.sndNxt)) {
          this.state = 'fin-wait-2';
        }
        if (this.state === 'last-ack' && seqGe(this.sndUna, this.sndNxt)) {
          this.cleanup(); return;
        }
      }
    }

    // Process payload (if it's in-order and we're in a state that
    // can receive data).
    if (payload.length > 0 && (this.state === 'established' || this.state === 'fin-wait-1' || this.state === 'fin-wait-2')) {
      if (seq === this.rcvNxt) {
        this.rcvNxt = (this.rcvNxt + payload.length) >>> 0;
        if (this.onData) this.onData(payload);
        // Ack the new data.
        this.sendAckOnly();
      } else if (seqLt(seq, this.rcvNxt)) {
        // Already-seen data — just re-ack.
        this.sendAckOnly();
      }
      // Future data (seqGt) is dropped silently. PPP delivers in order
      // so this branch is unreachable in practice.
    }

    // Process FIN flag — must be after data (FIN is logically after
    // the last data byte; per RFC 793 a FIN segment may carry data).
    if ((flags & FLAG_FIN) !== 0 && seq <= this.rcvNxt) {
      // Consumes 1 seq.
      this.rcvNxt = (this.rcvNxt + 1) >>> 0;
      this.sendAckOnly();
      if (this.state === 'established' || this.state === 'syn-rcvd') {
        this.state = 'close-wait';
        if (this.onClose) this.onClose();
      } else if (this.state === 'fin-wait-2') {
        this.cleanup();
      } else if (this.state === 'fin-wait-1') {
        // Simultaneous close — treat as closed once peer ACKs our FIN.
        this.state = 'last-ack';
      }
    }

    // Drain any pending TX (e.g. peer's ACK opened the window).
    this.flushTx();
  }

  // ── Internal helpers ────────────────────────────────────────────
  private flushTx(): void {
    if (this.state !== 'established' && this.state !== 'close-wait') {
      // After we've sent FIN we no longer push data.
      return;
    }
    // Send data in MSS-sized chunks.
    const MSS = 1400;
    while (this.sendBuf.length > 0) {
      const chunkLen = Math.min(MSS, this.sendBuf.length);
      const chunk = this.sendBuf.subarray(0, chunkLen);
      // Note: we don't drop the bytes from sendBuf until they're ACKed
      // (recv() handles that). But we *do* advance sndNxt for retx
      // accounting — and since we don't retransmit, we just advance and
      // drop the bytes from sendBuf now.
      this.tcp.emit(this, this.sndNxt, this.rcvNxt, FLAG_ACK | FLAG_PSH, chunk);
      this.sndNxt = (this.sndNxt + chunkLen) >>> 0;
      this.sendBuf = this.sendBuf.subarray(chunkLen);
    }
    // Send FIN if the app asked to close.
    if (this.sendClosed && !this.finSent) {
      this.finSent = true;
      this.tcp.emit(this, this.sndNxt, this.rcvNxt, FLAG_ACK | FLAG_FIN, new Uint8Array(0));
      this.sndNxt = (this.sndNxt + 1) >>> 0;
      this.state = (this.state === 'close-wait') ? 'last-ack' : 'fin-wait-1';
    }
  }

  private sendAckOnly(): void {
    this.tcp.emit(this, this.sndNxt, this.rcvNxt, FLAG_ACK, new Uint8Array(0));
  }

  private cleanup(): void {
    this.state = 'closed';
    this.tcp.dropConn(this);
  }
}

// Module-level Tcp instance. Holds connection table + per-port
// listener map.
export class Tcp {
  private conns = new Map<string, TcpConn>();
  private listeners = new Map<number, TcpListenerHandler>();
  private readonly localIp: Uint8Array;
  private readonly emitIp: (packet: Uint8Array) => void;

  constructor(localIp: Uint8Array, emitIp: (packet: Uint8Array) => void) {
    this.localIp = localIp;
    this.emitIp = emitIp;
  }

  listen(port: number, handler: TcpListenerHandler): void {
    this.listeners.set(port, handler);
  }

  // Called by the IP demux when a TCP segment arrives.
  handleSegment(srcIp: Uint8Array, dstIp: Uint8Array, raw: Uint8Array): void {
    if (raw.length < 20) return;
    const srcPort = (raw[0] << 8) | raw[1];
    const dstPort = (raw[2] << 8) | raw[3];
    const seq     = ((raw[4] << 24) | (raw[5] << 16) | (raw[6] << 8) | raw[7]) >>> 0;
    const ack     = ((raw[8] << 24) | (raw[9] << 16) | (raw[10] << 8) | raw[11]) >>> 0;
    const dataOff = (raw[12] >>> 4) & 0xF;
    const flags   = raw[13];
    const window  = (raw[14] << 8) | raw[15];
    const headerLen = dataOff * 4;
    if (headerLen < 20 || headerLen > raw.length) return;
    const payload = raw.subarray(headerLen);

    const key = tupleKey(srcIp, srcPort, dstIp, dstPort);
    const existing = this.conns.get(key);

    if (existing) {
      existing.recv(seq, ack, flags, window, payload);
      return;
    }

    // No connection — only SYN to a listening port is interesting.
    if ((flags & FLAG_SYN) === 0) {
      // Stray segment — RST it.
      this.sendRst(dstIp, srcIp, dstPort, srcPort, ack, seq, flags, payload.length);
      return;
    }
    const listener = this.listeners.get(dstPort);
    if (!listener) {
      // No listener — RST.
      this.sendRst(dstIp, srcIp, dstPort, srcPort, ack, seq, flags, payload.length);
      return;
    }
    const conn = new TcpConn(srcIp, srcPort, dstIp, dstPort, this, seq, window);
    this.conns.set(key, conn);
    conn.sendSynAck();
    listener.onConnect(conn);
  }

  // Connections call this to push a segment back through us → IP layer.
  emit(conn: TcpConn, seq: number, ack: number, flags: number,
       payload: Uint8Array, options: Uint8Array = new Uint8Array(0)): void {
    const optLen = options.length;
    // Pad options to a multiple of 4.
    const padLen = (4 - (optLen % 4)) % 4;
    const fullOpts = new Uint8Array(optLen + padLen);
    fullOpts.set(options);
    // padding bytes left zero (0 == End-of-options)
    const headerLen = 20 + fullOpts.length;
    const segLen = headerLen + payload.length;
    const seg = new Uint8Array(segLen);
    seg[0] = (conn.localPort >>> 8) & 0xFF;
    seg[1] = conn.localPort & 0xFF;
    seg[2] = (conn.remotePort >>> 8) & 0xFF;
    seg[3] = conn.remotePort & 0xFF;
    seg[4] = (seq >>> 24) & 0xFF; seg[5] = (seq >>> 16) & 0xFF;
    seg[6] = (seq >>> 8) & 0xFF;  seg[7] = seq & 0xFF;
    seg[8] = (ack >>> 24) & 0xFF; seg[9] = (ack >>> 16) & 0xFF;
    seg[10] = (ack >>> 8) & 0xFF; seg[11] = ack & 0xFF;
    seg[12] = ((headerLen / 4) << 4) & 0xFF;   // data offset (words), reserved bits 0
    seg[13] = flags;
    seg[14] = 0x20; seg[15] = 0x00;            // window = 8192
    seg[16] = 0; seg[17] = 0;                  // checksum 0 for computation
    seg[18] = 0; seg[19] = 0;                  // urgent pointer
    seg.set(fullOpts, 20);
    seg.set(payload, headerLen);
    const csum = pseudoChecksum(conn.localIp, conn.remoteIp, IP_PROTO_TCP, seg);
    seg[16] = (csum >>> 8) & 0xFF;
    seg[17] = csum & 0xFF;
    const ipPkt = buildIpv4(IP_PROTO_TCP, conn.localIp, conn.remoteIp, seg);
    this.emitIp(ipPkt);
  }

  dropConn(conn: TcpConn): void {
    const key = tupleKey(conn.remoteIp, conn.remotePort, conn.localIp, conn.localPort);
    this.conns.delete(key);
  }

  getLocalIp(): Uint8Array { return this.localIp; }
  connectionCount(): number { return this.conns.size; }

  // Send a RST in response to a stray segment.
  private sendRst(srcIp: Uint8Array, dstIp: Uint8Array,
                  srcPort: number, dstPort: number,
                  ack: number, seq: number, flags: number, payloadLen: number): void {
    // RFC 793: if the inbound segment had ACK, use its ack as our seq
    // and zero our ack; otherwise seq=0 and ack=(seq+segment-length).
    let rstSeq = 0;
    let rstAck = 0;
    let rstFlags = FLAG_RST;
    if ((flags & FLAG_ACK) !== 0) {
      rstSeq = ack;
    } else {
      rstFlags |= FLAG_ACK;
      const synLen = ((flags & FLAG_SYN) !== 0 ? 1 : 0) + ((flags & FLAG_FIN) !== 0 ? 1 : 0);
      rstAck = (seq + payloadLen + synLen) >>> 0;
    }
    // Fake a conn-shaped object just for the emit helper.
    const fakeConn = {
      localIp: srcIp, remoteIp: dstIp, localPort: srcPort, remotePort: dstPort,
    } as unknown as TcpConn;
    this.emit(fakeConn, rstSeq, rstAck, rstFlags, new Uint8Array(0));
  }
}

function tupleKey(remoteIp: Uint8Array, remotePort: number, localIp: Uint8Array, localPort: number): string {
  return `${remoteIp[0]}.${remoteIp[1]}.${remoteIp[2]}.${remoteIp[3]}:${remotePort}` +
         `→${localIp[0]}.${localIp[1]}.${localIp[2]}.${localIp[3]}:${localPort}`;
}

// 32-bit sequence-number arithmetic per RFC 793 (modulo 2^32 "less
// than" comparisons).
function seqLt(a: number, b: number): boolean { return ((a - b) | 0) < 0; }
function seqGt(a: number, b: number): boolean { return ((a - b) | 0) > 0; }
function seqGe(a: number, b: number): boolean { return ((a - b) | 0) >= 0; }
