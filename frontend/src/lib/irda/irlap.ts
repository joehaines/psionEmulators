// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// IrLAP — the IrDA link-access layer. We act solely as the primary
// station (the side initiating the beam): we run discovery to find the
// listening device, send SNRM to open a Normal-Response-Mode connection,
// then exchange information frames stop-and-wait (window size 1) and tear
// the link down with DISC.
//
// Frame body handed to/from the SIR framing layer:
//   Address(1) | Control(1) | Information(0..N)
// Address = (connAddr << 1) | C/R. Control values live in types.ts.
//
// NRM token passing: the primary "polls" by setting the P bit on a
// command frame; the secondary replies with frames carrying the F bit and
// returns the token on its final frame. With window 1 this reduces to:
// send one command with P=1, wait for the secondary's F=1 reply, repeat.
// Every upper-layer exchange (IAS query, TinyTP/OBEX request) is itself
// request→response, so stop-and-wait is the natural fit.

import {
  IRLAP_CR_COMMAND,
  IRLAP_CR_RESPONSE,
  IRLAP_ADDR_BROADCAST,
  IRLAP_CONN_ADDR,
  U_SNRM,
  U_UA,
  U_DISC,
  U_DM,
  U_XID,
  U_XID_RESPONSE,
  PF_BIT,
  S_RR,
  XID_FORMAT_DISCOVERY,
  XID_SLOTS_1,
  XID_SLOT_FINAL,
  XID_CHARSET_ASCII,
} from './types.ts';

export interface DiscoveredDevice {
  // 32-bit device address the secondary reported in its XID response.
  deviceAddr: number;
  // Hint bytes (service hints) — informational.
  hints: Uint8Array;
  // Discovery nickname, decoded as ASCII (best effort).
  nickname: string;
}

// addr/control helpers.
function addrByte(connAddr: number, command: boolean): number {
  return ((connAddr & 0x7f) << 1) | (command ? IRLAP_CR_COMMAND : IRLAP_CR_RESPONSE);
}

// SNRM QoS negotiation parameters we offer. Conservative, single-window
// values — see types.ts notes. Each tuple is PI, PL=1, PV.
function snrmQosParams(): number[] {
  return [
    0x01, 1, 0x22, // baud: 9600 | 115200
    0x82, 1, 0x01, // max turnaround time: 500 ms
    0x83, 1, 0x01, // data size: 64 bytes
    0x84, 1, 0x01, // window size: 1
    0x85, 1, 0x01, // additional BOFs
    0x86, 1, 0x80, // min turnaround time: 0 ms
    0x08, 1, 0xff, // link disconnect/threshold time
  ];
}

const ST_IDLE = 0;
const ST_DISCOVERING = 1;
const ST_CONNECTING = 2;
const ST_CONNECTED = 3;
const ST_DISCONNECTING = 4;

export class IrLap {
  private readonly sendBody: (body: Uint8Array) => void;
  // Our randomly-chosen 32-bit source device address.
  private readonly srcAddr: number;
  private connAddr = IRLAP_CONN_ADDR;
  private peerAddr = 0;

  private state = ST_IDLE;
  // Send/receive sequence numbers (mod 8).
  private vs = 0;
  private vr = 0;
  // True while we hold the NRM token (may transmit a poll).
  private tokenHeld = false;
  // A command poll is outstanding; we are waiting for the secondary's
  // F-bit reply before polling again.
  private pollOutstanding = false;
  // F-timer (poll/final recovery). IrLAP NRM is stop-and-wait: the primary
  // sends a command with P=1 and may not poll again until the secondary
  // answers with F=1. If that answer never arrives — on a device slower than
  // real-time the secondary occasionally fails to turn the half-duplex line
  // around within one host poll window — a primary with no recovery waits
  // forever and the whole beam deadlocks (TinyTP credit then starves out,
  // surfacing as "device stopped acknowledging"). The spec's recovery is to
  // re-issue the poll when the F-timer expires. We re-poll with a FRESH RR
  // command carrying the CURRENT V(r): RR is a pure supervisory poll, so it is
  // idempotent and never perturbs the I-frame sequence (re-sending a stale
  // I-frame instead would present an out-of-date N(s)/N(r) the secondary can
  // reject with FRMR). The secondary answers with its outstanding I-frame or
  // an RR, handing the token back and unblocking the link. Bounded so a truly
  // dead peer still surfaces as the upper-layer timeout.
  private pollSentAt = 0;
  private pollRetries = 0;
  private static readonly POLL_TIMEOUT_MS = 500;
  private static readonly MAX_POLL_RETRIES = 30;
  // Wall-clock time of the most recent valid frame received from the secondary.
  // Upper layers (Tiny TP flow control) use this to tell "the device is slow /
  // flow-controlling us" (it still answers every poll with RR/RNR, so this keeps
  // advancing) from "the device has genuinely stopped acknowledging" (no frames
  // at all). Initialised to construction time so a never-connected link still
  // times out. See handleFrame.
  lastInboundAt = Date.now();
  // Queue of upper-layer info fields awaiting transmission as I-frames.
  private txQueue: Uint8Array[] = [];
  // When true and we have nothing to send, we still poll (RR P=1) so the
  // secondary can deliver a pending reply.
  private expectingReply = false;
  // Secondary is in RNR (receive-not-ready) flow-control: its receive buffer is
  // full and it cannot accept I-frames. We hold transmission (but keep polling)
  // until it answers RR again. Cleared on any RR; set on any RNR. See
  // handleSFrame / pump.
  private peerBusy = false;
  // Stop-and-wait ARQ. The single I-frame we have transmitted but the secondary
  // has not yet acknowledged (via its N(r) advancing past it). Held so we can
  // retransmit it if it (or its ack) was lost — without this the primary was
  // fire-and-forget: any dropped I-frame desynced the link (V(S) ran ahead of
  // what the device received → "host bar ahead of the device" → wedge on a
  // sustained beam). Cleared the moment the secondary acks it. See sendI /
  // processAck / pump.
  private unacked: { ns: number; info: Uint8Array } | null = null;

  // ── Discovery bookkeeping ──
  private discovered: DiscoveredDevice[] = [];
  private discoveryResolve: ((d: DiscoveredDevice[]) => void) | null = null;
  private discoveryTimer: ReturnType<typeof setTimeout> | null = null;

  // ── Connect bookkeeping ──
  private connectResolve: (() => void) | null = null;
  private connectReject: ((e: Error) => void) | null = null;
  private connectTimer: ReturnType<typeof setTimeout> | null = null;
  // SNRM retransmission. IrLAP setup is itself stop-and-wait: the primary
  // sends SNRM (P=1) and waits for the secondary's UA. The spec recovers a
  // lost SNRM/UA by retransmitting SNRM on the F-timer. We send SNRM once and
  // then re-send it from tick() until UA arrives or the overall connect
  // window expires. This matters for the Psion netBook (EPOC R5, v450): right
  // after answering XID discovery it ignores the *first* SNRM (its IrLAP
  // hasn't returned to NDM yet) but answers a second SNRM ~1 s later with UA.
  // Without retransmission discoverAndConnect() sent a single SNRM into that
  // window and timed out. Devices that answer the first SNRM (5mx, Osaris,
  // Series 5, Series 7) reach ST_CONNECTED before the first retry fires, so
  // this is a no-op for them.
  private snrmBody: Uint8Array | null = null;
  private snrmSentAt = 0;
  private static readonly SNRM_RETRY_MS = 600;

  // Delivered to the upper layer (IrLMP) for each received info field.
  onInfo: ((info: Uint8Array) => void) | null = null;
  // Fired if the link drops unexpectedly (DM/DISC from peer).
  onDisconnect: (() => void) | null = null;

  constructor(sendBody: (body: Uint8Array) => void) {
    this.sendBody = sendBody;
    this.srcAddr = (Math.floor(Math.random() * 0xfffffffe) + 1) >>> 0;
  }

  get connected(): boolean {
    return this.state === ST_CONNECTED;
  }

  // ── Discovery ────────────────────────────────────────────────────────
  // Sends a single-slot discovery exchange and resolves with whatever
  // devices respond within `timeoutMs`. Empty array = nobody listening
  // (i.e. the device is not in Infrared receive mode).
  discover(timeoutMs: number): Promise<DiscoveredDevice[]> {
    this.state = ST_DISCOVERING;
    this.discovered = [];
    return new Promise<DiscoveredDevice[]>((resolve) => {
      this.discoveryResolve = resolve;
      // Slot 0 command, then the final (slot 0xFF) command carrying our
      // own discovery info. A device in receive mode answers in slot 0.
      this.sendXidCommand(0, XID_SLOTS_1, false);
      this.sendXidCommand(XID_SLOT_FINAL, XID_SLOTS_1, true);
      this.discoveryTimer = setTimeout(() => this.finishDiscovery(), timeoutMs);
    });
  }

  private finishDiscovery(): void {
    if (this.discoveryTimer) {
      clearTimeout(this.discoveryTimer);
      this.discoveryTimer = null;
    }
    const resolve = this.discoveryResolve;
    this.discoveryResolve = null;
    if (this.state === ST_DISCOVERING) this.state = ST_IDLE;
    resolve?.(this.discovered);
  }

  private sendXidCommand(slot: number, slotFlags: number, isFinal: boolean): void {
    const info: number[] = [XID_FORMAT_DISCOVERY];
    // Source device address (little-endian), destination = broadcast.
    pushU32LE(info, this.srcAddr);
    pushU32LE(info, 0xffffffff);
    info.push(slotFlags & 0x03); // discovery flags: slot count
    info.push(slot & 0xff); // slot number
    info.push(0x00); // version
    if (isFinal) {
      // Our discovery info: hint byte(s) + charset + nickname.
      info.push(0x00); // hint: no special services
      info.push(XID_CHARSET_ASCII);
      for (const c of 'PsionWeb') info.push(c.charCodeAt(0) & 0x7f);
    }
    this.sendU(IRLAP_ADDR_BROADCAST, U_XID, true, true, new Uint8Array(info));
  }

  private parseXidResponse(info: Uint8Array): void {
    // info: format(1) src(4) dst(4) flags(1) slot(1) version(1) [hints… charset nick]
    // The discovery-info (hint/charset/nickname) begins at offset 12, AFTER
    // the version byte at offset 11. Verified against a captured 5mx XID
    // response: ...version=00 | hints=82 24 | charset=00 | "Symbian EPOC".
    if (info.length < 12 || info[0] !== XID_FORMAT_DISCOVERY) return;
    const deviceAddr = readU32LE(info, 1);
    let p = 12;
    const hintStart = p;
    // Hint bytes use the high bit as an "extension" continuation flag.
    while (p < info.length && (info[p] & 0x80) !== 0) p++;
    if (p < info.length) p++; // final hint byte
    const hints = info.subarray(hintStart, p);
    let nickname = '';
    if (p < info.length) {
      p++; // charset byte
      nickname = asciiDecode(info.subarray(p));
    }
    // Avoid duplicate entries when the secondary answers our slot-0 and
    // final XID commands (or we re-send during a retry).
    if (this.discovered.some((d) => d.deviceAddr === deviceAddr)) return;
    this.discovered.push({ deviceAddr, hints, nickname });
    // Resolve discovery promptly once a device answers rather than burning
    // the whole discovery window. The EPOC IrLAP secondary only stays in
    // the discovery-response state briefly after replying, so connecting
    // (SNRM) immediately — within its turnaround window — is what lets the
    // link actually come up. We still allow a short settle to collect a
    // second device if one is present.
    if (this.state === ST_DISCOVERING && this.discoveryResolve) {
      if (this.discoveryTimer) clearTimeout(this.discoveryTimer);
      this.discoveryTimer = setTimeout(() => this.finishDiscovery(), 5);
    }
  }

  // ── Connect ──────────────────────────────────────────────────────────
  connect(deviceAddr: number, timeoutMs: number): Promise<void> {
    this.peerAddr = deviceAddr;
    this.state = ST_CONNECTING;
    this.vs = 0;
    this.vr = 0;
    this.unacked = null;
    return new Promise<void>((resolve, reject) => {
      this.connectResolve = resolve;
      this.connectReject = reject;
      // SNRM I-field: src(4) dst(4) connAddr(1) + QoS PV tuples.
      // The connection-address byte uses the IrLAP address-field encoding
      // `(connAddr << 1) | C/R`, NOT the bare 7-bit value. Sending the raw
      // 0x01 made the real 5mx silently drop the SNRM (no UA, link never
      // came up) because connection 0 is reserved.
      //
      // We send the EVEN form `(connAddr << 1) | 0` = 0x02 (C/R bit clear),
      // not 0x03. EPOC R1 (Series 5) stores this byte verbatim as the link's
      // expected connection address and validates every inbound frame with
      // `(frameAddr & 0xFE) == expected`; an odd stored value (0x03) can never
      // match that even-masked comparison, so the R1 link comes up (UA) but
      // then silently drops every numbered I/S frame. Storing the even 0x02
      // makes the comparison succeed (our command frames at 0x03 mask to 0x02)
      // and unblocks the data phase. The 5mx/Osaris (EPOC R5) normalise the
      // address internally and answer UA to either form, so this is safe for
      // them too. (Verified by ROM disassembly of the R1 IrLAP dispatch at
      // 0x50096200 — see docs.)
      const info: number[] = [];
      pushU32LE(info, this.srcAddr);
      pushU32LE(info, this.peerAddr);
      info.push(((this.connAddr & 0x7f) << 1) | IRLAP_CR_RESPONSE);
      info.push(...snrmQosParams());
      // Cache the framed SNRM so tick() can retransmit it (see snrmBody notes).
      const body = new Uint8Array(2 + info.length);
      body[0] = addrByte(IRLAP_ADDR_BROADCAST, true);
      body[1] = U_SNRM | PF_BIT;
      body.set(new Uint8Array(info), 2);
      this.snrmBody = body;
      this.snrmSentAt = Date.now();
      this.sendBody(body);
      this.connectTimer = setTimeout(() => {
        this.failConnect(new Error('SNRM connect timed out'));
      }, timeoutMs);
    });
  }

  private failConnect(err: Error): void {
    if (this.connectTimer) {
      clearTimeout(this.connectTimer);
      this.connectTimer = null;
    }
    this.snrmBody = null;
    const reject = this.connectReject;
    this.connectResolve = null;
    this.connectReject = null;
    this.state = ST_IDLE;
    reject?.(err);
  }

  private completeConnect(): void {
    if (this.connectTimer) {
      clearTimeout(this.connectTimer);
      this.connectTimer = null;
    }
    this.snrmBody = null;
    const resolve = this.connectResolve;
    this.connectResolve = null;
    this.connectReject = null;
    this.state = ST_CONNECTED;
    this.tokenHeld = true; // primary owns the token after connect
    this.pollOutstanding = false;
    this.peerBusy = false;
    resolve?.();
  }

  // ── Upper-layer data ─────────────────────────────────────────────────
  // Queue an info field for transmission as an I-frame. Set expecting=true
  // when a reply is anticipated so the link keeps polling.
  sendInfo(info: Uint8Array, expectReply = true): void {
    this.txQueue.push(info);
    this.expectingReply = this.expectingReply || expectReply;
    this.pump();
  }

  setExpectingReply(v: boolean): void {
    this.expectingReply = v;
    this.pump();
  }

  // ── Disconnect ───────────────────────────────────────────────────────
  disconnect(): void {
    if (this.state === ST_CONNECTED) {
      this.state = ST_DISCONNECTING;
      this.sendU(this.connAddr, U_DISC, true, true, new Uint8Array(0));
    }
    this.state = ST_IDLE;
    this.txQueue = [];
    this.tokenHeld = false;
    this.pollOutstanding = false;
    this.unacked = null;
  }

  // ── Tick: drive polling when connected ───────────────────────────────
  tick(): void {
    this.checkSnrmTimeout();
    this.checkPollTimeout();
    this.pump();
  }

  // SNRM retransmission while waiting for UA. Re-sends the cached SNRM every
  // SNRM_RETRY_MS until the secondary answers (completeConnect clears
  // snrmBody) or the overall connect window expires (failConnect clears it).
  private checkSnrmTimeout(): void {
    if (this.state !== ST_CONNECTING || !this.snrmBody) return;
    if (Date.now() - this.snrmSentAt < IrLap.SNRM_RETRY_MS) return;
    this.snrmSentAt = Date.now();
    this.sendBody(this.snrmBody);
  }

  private pump(): void {
    if (this.state !== ST_CONNECTED) return;
    if (!this.tokenHeld || this.pollOutstanding) return;
    if (this.unacked) {
      // A previously-sent I-frame is still unacknowledged: don't advance to new
      // data. Retransmit it (stop-and-wait ARQ) so a lost frame/ack recovers —
      // unless the secondary is busy (RNR), in which case just probe with RR
      // until it can receive again.
      if (this.peerBusy) this.sendS(S_RR, true);
      else this.retransmitUnacked();
      this.markPolled();
    } else if (this.txQueue.length > 0 && !this.peerBusy) {
      // Send the next queued I-frame — but only while the secondary is ready.
      const info = this.txQueue.shift()!;
      this.sendI(info);
      this.markPolled();
    } else if (this.expectingReply || this.txQueue.length > 0) {
      // Poll with RR so the secondary can deliver its reply — or, when it is
      // busy (RNR) and we still hold queued I-frames, to probe for it clearing
      // so we resume sending without overrunning it.
      this.sendS(S_RR, true);
      this.markPolled();
    }
  }

  // Record that a poll (P=1) is now outstanding and (re)start the F-timer.
  private markPolled(): void {
    this.pollOutstanding = true;
    this.tokenHeld = false;
    this.pollSentAt = Date.now();
    this.pollRetries = 0;
  }

  // Token returned (secondary answered with F=1). Stop the F-timer.
  private clearPolled(): void {
    this.pollOutstanding = false;
    this.tokenHeld = true;
    this.pollRetries = 0;
  }

  // F-timer expiry: the secondary never answered our poll. Re-poll so it
  // re-sends its pending response and returns the token. If an I-frame is
  // outstanding, retransmit *that* (recovers a lost frame or a lost ack);
  // otherwise a fresh RR (current V(r)), which leaves the I-sequence untouched.
  private checkPollTimeout(): void {
    if (this.state !== ST_CONNECTED || !this.pollOutstanding) return;
    if (Date.now() - this.pollSentAt < IrLap.POLL_TIMEOUT_MS) return;
    if (this.pollRetries >= IrLap.MAX_POLL_RETRIES) return; // give up; upper-layer timeout surfaces it
    this.pollRetries++;
    this.pollSentAt = Date.now();
    if (this.unacked && !this.peerBusy) this.retransmitUnacked();
    else this.sendS(S_RR, true);
  }

  // ── Inbound frame dispatch (from the SIR decoder) ────────────────────
  handleFrame(body: Uint8Array): void {
    if (body.length < 2) return;
    // Any valid frame from the secondary is proof of life — record it so
    // Tiny TP can distinguish a slow-but-responsive device from a dead one.
    this.lastInboundAt = Date.now();
    const addr = body[0];
    const control = body[1];
    const info = body.subarray(2);
    // As IrLAP primary, every frame we receive is from the secondary, so the
    // direction is implied — match on the connection address (bits 1..7) and
    // ignore the C/R bit. Most EPOC builds (5mx, Osaris) clear C/R in their
    // responses (UA on connection 1 = addr 0x02), but EPOC R1 on the Psion
    // Series 5 (CL-PS7110) *sets* it (addr 0x03) for its UA and subsequent
    // I/S-frames. Filtering on C/R alone discarded the Series 5's UA and the
    // link never came up; matching the address byte regardless of C/R fixes
    // it without affecting the 5mx/Osaris (whose addresses still match).
    const connAddr7 = (addr >> 1) & 0x7f;
    if (connAddr7 !== this.connAddr && connAddr7 !== IRLAP_ADDR_BROADCAST) {
      return; // not for our connection / not a broadcast discovery slot
    }

    // Frame type from the low control bits: ......0 = I, ....01 = S,
    // ....11 = U.
    if ((control & 0x01) === 0) {
      this.handleIFrame(control, info);
      return;
    }
    const final = (control & PF_BIT) !== 0;
    if ((control & 0x03) === 0x01) {
      this.handleSFrame(control, final);
      return;
    }

    // Unnumbered frames.
    const uMasked = control & ~PF_BIT;
    switch (uMasked) {
      case U_UA:
        if (this.state === ST_CONNECTING) this.completeConnect();
        else if (this.state === ST_DISCONNECTING) this.state = ST_IDLE;
        break;
      case U_XID:
      case U_XID_RESPONSE:
        // The 5mx/EPOC secondary answers discovery with the XID *response*
        // control 0xAF; we also tolerate the command-form 0x2F for symmetry
        // with simpler peers.
        if (this.state === ST_DISCOVERING) this.parseXidResponse(info);
        break;
      case U_DM:
        // Disconnected mode — peer refuses / dropped.
        if (this.state === ST_CONNECTING) this.failConnect(new Error('device replied DM (busy)'));
        else this.dropLink();
        break;
      case U_DISC:
        this.dropLink();
        break;
      default:
        break;
    }
  }

  private handleIFrame(control: number, info: Uint8Array): void {
    if (this.state !== ST_CONNECTED) return;
    const ns = (control >> 1) & 0x07;
    const nr = (control >> 5) & 0x07;
    const final = (control & PF_BIT) !== 0;
    // The secondary's N(r) acknowledges our outstanding I-frame.
    this.processAck(nr);
    if (ns === this.vr) {
      this.vr = (this.vr + 1) & 0x07;
      this.onInfo?.(info);
    }
    if (final) {
      // Token returns to us; stop the F-timer.
      this.clearPolled();
    }
    this.pump();
  }

  private handleSFrame(control: number, final: boolean): void {
    if (this.state !== ST_CONNECTED) return;
    // Distinguish RR (Receive Ready) from RNR (Receive Not Ready). The S-frame
    // type is in control bits 3:2 — RR=00, RNR=01. RNR means the secondary's
    // receive buffer is full (e.g. a slow Series 5 buffering body segments while
    // it writes the file to disk): we MUST stop sending I-frames until it clears
    // (answers RR again), or we overrun it — the host races ahead, the device
    // drops frames and wedges. Keep polling while busy so we resume the instant
    // it can receive again. (We never treated RNR specially before, which is why
    // a sustained beam to the Series 5 hung partway with the host bar ahead of
    // the device.)
    this.peerBusy = (control & 0x0c) === 0x04;
    // An RR/RNR also carries N(r), acknowledging our outstanding I-frame.
    this.processAck((control >> 5) & 0x07);
    // RR/RNR response with F=1 hands the token back.
    if (final) {
      this.clearPolled();
    }
    this.pump();
  }

  private dropLink(): void {
    if (this.state === ST_CONNECTED) {
      this.state = ST_IDLE;
      this.onDisconnect?.();
    }
    this.tokenHeld = false;
    this.pollOutstanding = false;
  }

  // ── Frame builders ───────────────────────────────────────────────────
  private sendU(connAddr: number, control: number, command: boolean, pf: boolean, info: Uint8Array): void {
    const body = new Uint8Array(2 + info.length);
    body[0] = addrByte(connAddr, command);
    body[1] = control | (pf ? PF_BIT : 0);
    body.set(info, 2);
    this.sendBody(body);
  }

  // Build + transmit an I-frame carrying `info` with sequence number `ns`,
  // P=1. Used for both first transmission and retransmission.
  private txIFrame(info: Uint8Array, ns: number): void {
    const control = (this.vr << 5) | PF_BIT | (ns << 1);
    const body = new Uint8Array(2 + info.length);
    body[0] = addrByte(this.connAddr, true);
    body[1] = control;
    body.set(info, 2);
    this.sendBody(body);
  }

  // Send a NEW I-frame: assign it V(S), keep it as the single outstanding
  // (stop-and-wait) frame for retransmission until acknowledged, then advance
  // V(S).
  private sendI(info: Uint8Array): void {
    const ns = this.vs;
    this.unacked = { ns, info };
    this.txIFrame(info, ns);
    this.vs = (this.vs + 1) & 0x07;
  }

  // Stop-and-wait retransmission of the outstanding I-frame (lost frame or lost
  // ack — the secondary discards a duplicate via its own N(s) check).
  private retransmitUnacked(): void {
    if (this.unacked) this.txIFrame(this.unacked.info, this.unacked.ns);
  }

  // Apply the secondary's N(r) (present on every I/S response). N(r) == V(S)
  // acknowledges our outstanding frame (whose ns is V(S)-1); release it so the
  // next can go. A lower N(r) means it was not received — leave it outstanding
  // so pump()/the F-timer retransmit it.
  private processAck(nr: number): void {
    if (this.unacked && nr === this.vs) this.unacked = null;
  }

  private sendS(code: number, pf: boolean): void {
    const control = (this.vr << 5) | (pf ? PF_BIT : 0) | code;
    const body = new Uint8Array([addrByte(this.connAddr, true), control]);
    this.sendBody(body);
  }
}

// ── little-endian / ASCII helpers ──────────────────────────────────────
function pushU32LE(out: number[], v: number): void {
  out.push(v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff);
}
function readU32LE(b: Uint8Array, off: number): number {
  return ((b[off] | (b[off + 1] << 8) | (b[off + 2] << 16) | (b[off + 3] << 24)) >>> 0);
}
function asciiDecode(b: Uint8Array): string {
  let s = '';
  for (let i = 0; i < b.length; i++) {
    const c = b[i];
    if (c === 0) break;
    if (c >= 0x20 && c < 0x7f) s += String.fromCharCode(c);
  }
  return s;
}
