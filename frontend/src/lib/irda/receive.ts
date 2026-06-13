// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// IrReceiveClient — the reverse of IrSendClient. Here the EMULATED DEVICE
// beams a file to us: the user opens the device's own "Infrared → Send"
// (EPOC's Eikon IR transfer app), picks a file, and points it at the host.
// The device drives the link as the IrLAP *primary* (it runs discovery,
// sends SNRM, queries our IAS, opens the TinyTP channel and streams the
// file); we are the *secondary*/responder on every layer and hand the
// assembled bytes back for download.
//
//   device (primary) ── XID discovery ──▶ host answers (we're listening)
//                    ── SNRM ───────────▶ host UA
//                    ── IAS GetValueByClass(Epoc32:EikonIr:v2.0) ▶ host → LSAP 8
//                    ── TinyTP connect to LSAP 8 ─▶ host confirm + credit
//                    ── "FILE <size> <att> <hi> <lo> <name>" ─▶ host "ACK Y"
//                    ── raw body SDUs ──▶ host accumulates (+ credit grants)
//                    ◀─ graceful LM-Disconnect ── host (file complete)
//
// This is the mirror image of the in-process MockDevice that validates the
// send path (frontend/src/lib/__tests__/irda.e2e.test.mts): that mock IS the
// body receiver answering the real IrSendClient, so the same responder logic
// proven there is what we run as the host here. The new round-trip test
// (irda.receive.e2e.test.mts) drives the real IrSendClient straight into this
// client to prove the two production stacks interoperate end-to-end.
//
// We deliberately re-implement the IrLAP-secondary / LM-MUX-server / IAS-server
// / TinyTP-receiver / Eikon-receiver state machine inline rather than reuse the
// initiator-oriented IrLap/IrLmp/TinyTp classes (which only know how to *open*
// connections): that keeps the verified send path untouched and keeps the whole
// responder in one auditable place.

import { IrTransport, type IrTransportConfig } from './transport.ts';
import {
  IRLAP_CONN_ADDR,
  U_SNRM,
  U_UA,
  U_DISC,
  U_XID,
  U_XID_RESPONSE,
  PF_BIT,
  S_RR,
  XID_FORMAT_DISCOVERY,
  XID_CHARSET_ASCII,
  LMP_CONTROL_BIT,
  LMP_CONNECT,
  LMP_CONNECT_CNF,
  LMP_DISCONNECT,
  LSAP_IAS,
  IAS_OP_GET_VALUE_BY_CLASS,
  IAS_LAST_FRAME,
  IAS_RET_SUCCESS,
  IAS_RET_NO_SUCH_CLASS,
  IAS_TYPE_INTEGER,
  IAS_EIKONIR_V2_CLASS,
  IAS_EIKONIR_V1_CLASS,
  IAS_TINYTP_LSAPSEL_ATTR,
  EIKONIR_LSAPSEL,
  TTP_M_BIT,
  TTP_CREDIT_MASK,
  TTP_INITIAL_CREDIT,
  EIKONIR_ACK_YES,
  EIKONIR_ACK_NO,
} from './types.ts';

// Thrown to reject receive() when the user cancels an in-progress (or pending)
// infrared receive. The dialog keys on this to stay quiet instead of surfacing
// an error.
export class IrReceiveCanceledError extends Error {
  constructor() {
    super('Infrared receive canceled');
    this.name = 'IrReceiveCanceledError';
  }
}

export interface IrReceiveConfig extends IrTransportConfig {
  // Discovery nickname we announce to the beaming device (cosmetic — shown
  // in the device's "sending to …" UI on some EPOC builds).
  nickname?: string;
}

export interface ReceivedFile {
  name: string;
  data: Uint8Array;
}

export interface ReceiveCallbacks {
  // Bytes of the body received so far (fires after each chunk).
  onProgress?: (received: number) => void;
  // Fires once the "FILE …" line is parsed, before the body streams — the UI
  // uses it to show the name/size and (on the Series 7) to start the cycle
  // pump for the one-way body phase.
  onFileInfo?: (name: string, size: number) => void;
}

// Field-debug switch shared with the send path: `localStorage.irDebug = '1'`
// or `globalThis.__irDebug = true`.
function irDebugEnabled(): boolean {
  try {
    if ((globalThis as { __irDebug?: unknown }).__irDebug) return true;
    return typeof localStorage !== 'undefined' && localStorage.getItem('irDebug') === '1';
  } catch {
    return false;
  }
}

function concat(parts: Uint8Array[]): Uint8Array {
  let n = 0;
  for (const p of parts) n += p.length;
  const out = new Uint8Array(n);
  let o = 0;
  for (const p of parts) {
    out.set(p, o);
    o += p.length;
  }
  return out;
}

function asciiOf(b: Uint8Array): string {
  let s = '';
  for (let i = 0; i < b.length; i++) s += String.fromCharCode(b[i]);
  return s;
}
function asciiBytes(s: string): Uint8Array {
  const out = new Uint8Array(s.length);
  for (let i = 0; i < s.length; i++) out[i] = s.charCodeAt(i) & 0xff;
  return out;
}

export class IrReceiveClient {
  private readonly transport: IrTransport;
  private readonly nickname: string;
  private readonly debug: boolean;

  // ── IrLAP secondary state ──
  // Connection address the primary assigned in its SNRM (we parse it out and
  // echo it on every response). Defaults to the conventional 0x01.
  private connAddr = IRLAP_CONN_ADDR;
  private vs = 0;
  private vr = 0;
  // Stop-and-wait ARQ for the responses WE transmit. The single I-frame we
  // have sent but the primary has not yet acknowledged (its N(r) hasn't
  // advanced past it). Held so we can RETRANSMIT it when the primary re-polls
  // — without this the responder was fire-and-forget: if any reply we sent
  // (UA aside; the IAS reply, the TinyTP connect-confirm, an "ACK Y", a credit
  // grant, the final LM-Disconnect) was lost or simply missed in the device's
  // half-duplex turnaround window, the device re-polls, we answer a bare RR
  // that acks its command but re-delivers nothing, and its IrLAP waits forever
  // for the reply that never comes. That is the intermittent "another device
  // was found but it never started to send the file": discovery (XID) and even
  // SNRM succeed, then a single dropped response strands the handshake. This is
  // the exact mirror of the primary-side ARQ in irlap.ts (IrLap.unacked), added
  // there for the same slow-/lossy-link reason. Cleared the moment the primary
  // acks it (processAck) and on every fresh connection (SNRM / reset).
  private unacked: { ns: number; info: Uint8Array } | null = null;
  // Our randomly-chosen 32-bit station address, announced in XID.
  private readonly deviceAddr = (Math.floor(Math.random() * 0xfffffffe) + 1) >>> 0;
  // The beaming station's address, captured from its XID command so our XID
  // response can target it.
  private peerStationAddr = 0xffffffff;

  // ── Tiny TP (Eikon channel, LSAP 8) state ──
  // Credit we have granted the device so it may stream body PDUs to us.
  private creditToPeer = 0;

  // ── SIBO "Psion IRLink" receiver state ──
  // The EPOC16 beam (Series 3c / Siena "Psion IR") is NOT Eikon-IR: after
  // SNRM the sender LM-Connects to LSAP 1 with the greeting
  // "Psion IRLink 1.00" (which we must ECHO in the connect-confirm), then
  // streams u16-length-prefixed blocks on LSAP 1: first a 136-byte header
  // ([size:u32][LOC:: path NUL][padding][4-byte tail]), then body chunks,
  // then a ZERO-length block as end-of-stream, then DISC. Pinned against
  // the live 3c v5.20f from both sides — see docs/sibo-remote-link.md.
  private siboMode = false;
  private siboBuf: number[] = [];
  private siboHeaderDone = false;
  private siboSize = 0;

  // ── Eikon-IR receiver state machine ──
  private rxSegments: Uint8Array[] = [];
  private gotFileLine = false;
  private fileName = '';
  private numToRecv = 0;
  private bodyChunks: Uint8Array[] = [];
  private bodyLen = 0;
  // A device-initiated PDU (the graceful LM-Disconnect after an empty file)
  // waiting to be sent on the next poll, since a secondary can only transmit
  // when polled.
  private pendingDisconnect: Uint8Array | null = null;

  // ── receive() promise plumbing ──
  private listening = false;
  private settled = false;
  private resolveReceive: ((f: ReceivedFile) => void) | null = null;
  private rejectReceive: ((e: Error) => void) | null = null;
  private callbacks: ReceiveCallbacks = {};

  constructor(
    readBytes: () => Uint8Array,
    writeBytes: (data: Uint8Array) => number,
    config: IrReceiveConfig = {},
  ) {
    this.nickname = config.nickname ?? 'PsionWeb';
    this.debug = irDebugEnabled();
    this.transport = new IrTransport(readBytes, writeBytes, config);
  }

  start(): void {
    this.transport.start(
      (body) => this.handleFrame(body),
      () => {
        /* pure responder — no poll-driven work, the device drives the link */
      },
    );
  }

  stop(): void {
    this.transport.stop();
  }

  get badFcs() {
    return this.transport.badFcs;
  }
  get framingErrors() {
    return this.transport.framingErrors;
  }

  // Wait for the device to beam a file. Resolves with the received file once
  // the whole body has arrived; rejects with IrReceiveCanceledError if the
  // user cancels, or with an Error if the device's "FILE" line is malformed
  // (we answer "ACK N" and reject). Until the device beams, this simply keeps
  // answering discovery so the device can find us.
  receive(callbacks: ReceiveCallbacks = {}): Promise<ReceivedFile> {
    this.callbacks = callbacks;
    this.listening = true;
    this.settled = false;
    this.resetTransferState();
    return new Promise<ReceivedFile>((resolve, reject) => {
      this.resolveReceive = resolve;
      this.rejectReceive = reject;
    });
  }

  // Abort a pending or in-progress receive. We can't spontaneously transmit as
  // an IrLAP secondary, so we settle the promise immediately and let the caller
  // stop()/detach the host bridge — dropping the line, which the device's beam
  // sees as a lost link and aborts. Safe to call before any device has beamed.
  cancel(): void {
    if (this.settled) return;
    this.fail(new IrReceiveCanceledError());
  }

  private resetTransferState(): void {
    this.vs = 0;
    this.vr = 0;
    this.unacked = null;
    this.creditToPeer = 0;
    this.siboMode = false;
    this.siboBuf = [];
    this.siboHeaderDone = false;
    this.siboSize = 0;
    this.rxSegments = [];
    this.gotFileLine = false;
    this.fileName = '';
    this.numToRecv = 0;
    this.bodyChunks = [];
    this.bodyLen = 0;
    this.pendingDisconnect = null;
  }

  private complete(): void {
    if (this.settled) return;
    this.settled = true;
    this.listening = false;
    const file: ReceivedFile = { name: this.fileName, data: concat(this.bodyChunks) };
    const resolve = this.resolveReceive;
    this.resolveReceive = null;
    this.rejectReceive = null;
    resolve?.(file);
  }

  private fail(err: Error): void {
    if (this.settled) return;
    this.settled = true;
    this.listening = false;
    const reject = this.rejectReceive;
    this.resolveReceive = null;
    this.rejectReceive = null;
    reject?.(err);
  }

  // ── Inbound IrLAP frame dispatch (from the SIR decoder) ──
  // The device is the primary, so every frame we receive is a command (C/R=1);
  // we answer each one as a response (C/R=0, F=1) to return the NRM token.
  private handleFrame(body: Uint8Array): void {
    if (body.length < 2) return;
    const addr = body[0];
    const control = body[1];
    const info = body.subarray(2);
    // Only consume command frames (C/R bit set). Our own responses (C/R=0)
    // never loop back, but a stray frame might.
    if ((addr & 0x01) !== 0x01) return;
    if (this.debug) this.trace('RX', body);

    // Both I- and S-frames from the primary carry its N(r), which acknowledges
    // the response I-frame we have outstanding. Apply it before dispatching so a
    // re-poll can tell "my last reply got through" (RR) from "it was lost,
    // retransmit it".
    if ((control & 0x01) === 0) {
      this.processAck((control >> 5) & 0x07);
      this.handleI(control, info);
      return;
    }
    if ((control & 0x03) === 0x01) {
      // Supervisory poll (RR/RNR). Its N(r) may ack our outstanding reply.
      this.processAck((control >> 5) & 0x07);
      // If we have a device-initiated PDU queued (the graceful disconnect after
      // an empty file), deliver it now. Otherwise, if a reply of ours is still
      // unacknowledged the primary is polling because it never got it — so
      // retransmit it rather than hand back a bare RR (which strands the
      // handshake). With nothing outstanding, just return the token with RR.
      if (this.pendingDisconnect) {
        const pdu = this.pendingDisconnect;
        this.pendingDisconnect = null;
        this.sendI(pdu);
      } else {
        this.resendOrRR();
      }
      return;
    }
    // Unnumbered command.
    const u = control & ~PF_BIT;
    if (u === U_XID) {
      if (this.listening && !this.settled) this.handleXidCommand(info);
    } else if (u === U_SNRM) {
      this.handleSnrm(info);
    } else if (u === U_DISC) {
      this.sendUA();
      // A SIBO sender may close the link right after its last body
      // chunk; if we already hold the full advertised size, treat the
      // DISC as a successful end-of-transfer.
      if (this.siboMode && this.siboHeaderDone && !this.settled &&
          this.bodyLen >= this.siboSize) {
        this.complete();
      }
    }
  }

  private handleSnrm(info: Uint8Array): void {
    // SNRM I-field: src(4) dst(4) connAddr(1) + QoS tuples. The connection-
    // address byte is the IrLAP address-field form (connAddr << 1 | C/R); the
    // bare 7-bit value is the high 7 bits. Fall back to the default if absent.
    if (info.length >= 9) this.connAddr = (info[8] >> 1) & 0x7f;
    this.vs = 0;
    this.vr = 0;
    // Fresh connection: drop any reply left outstanding from a prior link so a
    // re-poll early in this one can't retransmit a stale frame.
    this.unacked = null;
    this.sendUA();
  }

  private handleXidCommand(info: Uint8Array): void {
    // Discovery XID I-field: format(1) src(4) dst(4) flags(1) slot(1) version(1).
    // Capture the discovering station's address (offset 1, little-endian) so
    // our response targets it, and answer ONLY the slot-0 command so the device
    // collects exactly one response per discovery round (answering every slot
    // would collide on a multi-slot discovery). The final XID carries slot
    // 0xFF, which we likewise ignore.
    if (info.length < 11 || info[0] !== XID_FORMAT_DISCOVERY) return;
    this.peerStationAddr =
      (info[1] | (info[2] << 8) | (info[3] << 16) | (info[4] << 24)) >>> 0;
    const slot = info[10];
    if (slot !== 0x00) return;
    this.sendXidResponse();
  }

  // ── LM-MUX / IAS / Tiny TP / Eikon, returning a response LM-PDU or null ──
  private processLm(pdu: Uint8Array): Uint8Array | null {
    if (this.settled) return null;
    if (pdu.length < 2) return null;
    const dst = pdu[0];
    const localLsap = dst & 0x7f; // our LSAP (the PDU's destination)
    const peerLsap = pdu[1] & 0x7f;

    if ((dst & LMP_CONTROL_BIT) !== 0) {
      // Control PDU: opcode at [2].
      if (pdu.length < 3) return null;
      const opcode = pdu[2];
      if (opcode === LMP_CONNECT) {
        // SIBO beam: LM-Connect carrying the "Psion IRLink" greeting.
        // The confirm must echo the greeting block byte-for-byte — the
        // plain 4-byte confirm leaves the 3c streaming optimistically
        // and aborting with "No Psion found".
        if (asciiOf(pdu.subarray(3)).includes('Psion IRLink')) {
          this.siboMode = true;
          this.siboBuf = [];
          this.siboHeaderDone = false;
          this.siboSize = 0;
          const out = new Uint8Array(3 + (pdu.length - 3));
          out[0] = peerLsap | LMP_CONTROL_BIT;
          out[1] = localLsap;
          out[2] = LMP_CONNECT_CNF;
          out.set(pdu.subarray(3), 3);
          return out;
        }
        if (localLsap === EIKONIR_LSAPSEL) {
          // Tiny TP connect to our Eikon receiver: confirm and grant the device
          // its initial credit so it may stream the body.
          this.creditToPeer = TTP_INITIAL_CREDIT;
          return new Uint8Array([
            peerLsap | LMP_CONTROL_BIT,
            localLsap,
            LMP_CONNECT_CNF,
            0x00,
            TTP_INITIAL_CREDIT & TTP_CREDIT_MASK,
          ]);
        }
        // IAS (LSAP 0) or any other channel: plain connect-confirm.
        return new Uint8Array([peerLsap | LMP_CONTROL_BIT, localLsap, LMP_CONNECT_CNF, 0x00]);
      }
      // The device disconnecting a channel (e.g. IAS after its query): no reply.
      return null;
    }

    // Data PDU.
    const data = pdu.subarray(2);
    if (this.siboMode) {
      this.siboFeed(data);
      return null;   // acked at IrLAP level (RR); no LM reply expected
    }
    if (localLsap === LSAP_IAS) {
      return this.iasReply(peerLsap, localLsap, data);
    }
    if (localLsap === EIKONIR_LSAPSEL) {
      return this.ttpReceive(peerLsap, localLsap, data);
    }
    return null;
  }

  private iasReply(peerLsap: number, localLsap: number, req: Uint8Array): Uint8Array {
    // GetValueByClass request: op(0x84) classLen className attrLen attrName.
    // We publish the EPOC Eikon-IR receiver LSAP under classes
    // "Epoc32:EikonIr:v2.0" / "v1.0", attribute "IrDA:TinyTP:LsapSel" → 8
    // (KEikIrDALsapSel) — exactly the classes the device's sender looks up.
    if (req.length < 2 || req[0] !== (IAS_OP_GET_VALUE_BY_CLASS | IAS_LAST_FRAME)) {
      return new Uint8Array([peerLsap, localLsap, IAS_OP_GET_VALUE_BY_CLASS | IAS_LAST_FRAME, IAS_RET_NO_SUCH_CLASS]);
    }
    let p = 1;
    const classLen = req[p++];
    const className = asciiOf(req.subarray(p, p + classLen));
    p += classLen;
    const attrLen = req[p++];
    const attrName = asciiOf(req.subarray(p, p + attrLen));
    const knownClass = className === IAS_EIKONIR_V2_CLASS || className === IAS_EIKONIR_V1_CLASS;
    const known = knownClass && attrName === IAS_TINYTP_LSAPSEL_ATTR;
    const op = IAS_OP_GET_VALUE_BY_CLASS | IAS_LAST_FRAME;
    if (!known) {
      return new Uint8Array([peerLsap, localLsap, op, IAS_RET_NO_SUCH_CLASS]);
    }
    // op ret(SUCCESS) listLen(0001) objId(0002) type(INTEGER) value(LSAP 8, 4B BE).
    return new Uint8Array([
      peerLsap,
      localLsap,
      op,
      IAS_RET_SUCCESS,
      0x00, 0x01, // list length 1
      0x00, 0x02, // object id
      IAS_TYPE_INTEGER,
      0x00, 0x00, 0x00, EIKONIR_LSAPSEL & 0xff,
    ]);
  }

  private ttpReceive(peerLsap: number, localLsap: number, data: Uint8Array): Uint8Array | null {
    if (data.length < 1) return null;
    const header = data[0];
    const more = (header & TTP_M_BIT) !== 0;
    const payload = data.subarray(1);
    // One credit of the window we granted the device was spent by this PDU.
    this.creditToPeer = Math.max(0, this.creditToPeer - 1);
    if (payload.length > 0) this.rxSegments.push(payload.slice());

    // Replenish the device's window back up to the full initial credit.
    const grant = Math.max(0, TTP_INITIAL_CREDIT - this.creditToPeer);
    this.creditToPeer += grant;
    const grantByte = grant & TTP_CREDIT_MASK;

    if (more || payload.length === 0) {
      // Mid-SDU segment or pure credit update → just hand back a credit grant.
      return new Uint8Array([peerLsap, localLsap, grantByte]);
    }
    // SDU complete — reassemble and run the Eikon receiver step.
    const sdu = concat(this.rxSegments);
    this.rxSegments = [];
    return this.processEikon(peerLsap, localLsap, sdu, grantByte);
  }

  // EPOC Eikon-IR receiver state machine on LSAP 8.
  private processEikon(
    peerLsap: number,
    localLsap: number,
    sdu: Uint8Array,
    grantByte: number,
  ): Uint8Array | null {
    if (!this.gotFileLine) {
      // First SDU: "FILE <size> <att> <hi> <lo> <name>".
      const line = asciiOf(sdu);
      const m = line.match(/^FILE (\d+) (\d+) (\d+) (\d+) (.*)$/s);
      let ackPayload: string;
      if (m) {
        this.numToRecv = Number(m[1]);
        this.fileName = m[5];
        this.gotFileLine = true;
        ackPayload = EIKONIR_ACK_YES;
        this.callbacks.onFileInfo?.(this.fileName, this.numToRecv);
        this.callbacks.onProgress?.(0);
      } else {
        ackPayload = EIKONIR_ACK_NO;
      }
      // Reply: a TinyTP data PDU carrying our credit grant + the ACK text.
      // (The real device emits the credit update and "ACK Y" as two PDUs; we
      // fold them, which the sender reads identically — it only cares about the
      // SDU payload and the delta credit.)
      const ackBytes = asciiBytes(ackPayload);
      const ttp = new Uint8Array(1 + ackBytes.length);
      ttp[0] = grantByte;
      ttp.set(ackBytes, 1);
      const reply = new Uint8Array([peerLsap, localLsap, ...ttp]);

      if (!m) {
        // We declined the file. Send "ACK N" and surface the error; the device
        // shows "transfer failed" and tears the link down.
        this.fail(new Error(`Unexpected infrared packet (not a FILE header): "${line.trim()}"`));
        return reply;
      }
      if (this.numToRecv === 0) {
        // Empty file: complete the moment FILE is ACKed. Reply "ACK Y" now and
        // queue the graceful disconnect for the next poll.
        this.queueDisconnect(peerLsap, localLsap);
        this.complete();
      }
      return reply;
    }

    // Body chunk.
    this.bodyChunks.push(sdu.slice());
    this.bodyLen += sdu.length;
    this.callbacks.onProgress?.(Math.min(this.bodyLen, this.numToRecv));
    if (this.bodyLen >= this.numToRecv) {
      // Whole file received: save it and gracefully shut the socket (control
      // PDU, opcode LM-Disconnect, reason user) — the sender's
      // waitForPeerDisconnect resolves on this and tears its side down.
      this.complete();
      return new Uint8Array([peerLsap | LMP_CONTROL_BIT, localLsap, LMP_DISCONNECT, 0x01]);
    }
    // More to come → grant credit so the device keeps streaming.
    return new Uint8Array([peerLsap, localLsap, grantByte]);
  }

  private queueDisconnect(peerLsap: number, localLsap: number): void {
    this.pendingDisconnect = new Uint8Array([
      peerLsap | LMP_CONTROL_BIT,
      localLsap,
      LMP_DISCONNECT,
      0x01,
    ]);
  }

  // ── IrLAP secondary frame builders (responses: C/R=0, F=1) ──
  // SIBO length-prefixed block stream (may split across I-frames).
  private siboFeed(data: Uint8Array): void {
    for (let i = 0; i < data.length; i++) this.siboBuf.push(data[i]);
    for (;;) {
      if (this.siboBuf.length < 2) return;
      const len = this.siboBuf[0] | (this.siboBuf[1] << 8);
      if (this.siboBuf.length < 2 + len) return;
      const block = new Uint8Array(this.siboBuf.slice(2, 2 + len));
      this.siboBuf = this.siboBuf.slice(2 + len);
      this.siboBlock(block);
      if (this.settled) return;
    }
  }

  private siboBlock(block: Uint8Array): void {
    if (!this.siboHeaderDone) {
      // Header: [size:u32][path NUL][padding][tail]. The path is the
      // sender-side spec ("LOC::M:\WRD\X.WRD") — surface just the
      // final component as the file name.
      if (block.length < 6) return;
      this.siboSize = (block[0] | (block[1] << 8) | (block[2] << 16) | (block[3] << 24)) >>> 0;
      let end = 4;
      while (end < block.length && block[end] !== 0) end++;
      const path = asciiOf(block.subarray(4, end));
      const parts = path.split('\\');
      this.fileName = parts[parts.length - 1] || path;
      this.siboHeaderDone = true;
      this.callbacks.onFileInfo?.(this.fileName, this.siboSize);
      return;
    }
    if (block.length === 0) {
      // Zero-length block = end of stream.
      this.complete();
      return;
    }
    this.bodyChunks.push(block);
    this.bodyLen += block.length;
    this.callbacks.onProgress?.(this.bodyLen);
  }

  private handleI(control: number, info: Uint8Array): void {
    const ns = (control >> 1) & 0x07;
    if (ns === this.vr) {
      // In-order command: consume it and answer.
      this.vr = (this.vr + 1) & 0x07;
      const resp = this.processLm(info);
      if (resp) this.sendI(resp);
      else this.sendRR();
      return;
    }
    // Duplicate/old command (ns has already been received): the primary re-sent
    // it because it never saw our response. Retransmit the outstanding reply —
    // a bare RR would acknowledge the command but leave the primary waiting
    // forever for the reply it is actually re-polling for.
    this.resendOrRR();
  }

  private sendBody(body: Uint8Array): void {
    if (this.debug) this.trace('TX', body);
    this.transport.sendFrame(body);
  }

  private sendUA(): void {
    this.sendBody(new Uint8Array([(this.connAddr << 1) | 0, U_UA | PF_BIT]));
  }
  private sendRR(): void {
    this.sendBody(new Uint8Array([(this.connAddr << 1) | 0, (this.vr << 5) | PF_BIT | S_RR]));
  }
  // Send a NEW response I-frame: stamp it with V(s), keep it as the single
  // outstanding (stop-and-wait) frame for retransmission until the primary's
  // N(r) acks it, then advance V(s).
  private sendI(info: Uint8Array): void {
    const ns = this.vs;
    this.unacked = { ns, info };
    this.txI(info, ns);
    this.vs = (this.vs + 1) & 0x07;
  }
  // Build + transmit an I-frame carrying `info` with the given N(s), F=1 and the
  // current V(r). Used for both first transmission and retransmission (so a
  // resend re-presents the original N(s) the primary is waiting on).
  private txI(info: Uint8Array, ns: number): void {
    const control = (this.vr << 5) | PF_BIT | (ns << 1);
    this.sendBody(new Uint8Array([(this.connAddr << 1) | 0, control, ...info]));
  }
  private retransmitUnacked(): void {
    if (this.unacked) this.txI(this.unacked.info, this.unacked.ns);
  }
  // Re-poll handler: retransmit the outstanding reply if the primary hasn't
  // acked it, otherwise hand the token back with a plain RR.
  private resendOrRR(): void {
    if (this.unacked) this.retransmitUnacked();
    else this.sendRR();
  }
  // Apply the primary's N(r) (present on every I/S command). N(r) == V(s)
  // acknowledges our outstanding reply (whose N(s) is V(s)-1); release it. A
  // lower N(r) leaves it outstanding so a re-poll retransmits it.
  private processAck(nr: number): void {
    if (this.unacked && nr === this.vs) this.unacked = null;
  }

  private sendXidResponse(): void {
    // XID discovery response: format src(4) dst(4) flags slot version, then our
    // discovery info (hint bytes + charset + nickname). The hint bytes mark us
    // as a PDA/palmtop-class station (cosmetic — the device's beam proceeds via
    // the IAS lookup regardless). The address byte is the broadcast connection
    // (0x7F) with C/R=0; control is the XID *response* form (0xAF | F).
    const info: number[] = [
      XID_FORMAT_DISCOVERY,
      this.deviceAddr & 0xff,
      (this.deviceAddr >>> 8) & 0xff,
      (this.deviceAddr >>> 16) & 0xff,
      (this.deviceAddr >>> 24) & 0xff,
      this.peerStationAddr & 0xff,
      (this.peerStationAddr >>> 8) & 0xff,
      (this.peerStationAddr >>> 16) & 0xff,
      (this.peerStationAddr >>> 24) & 0xff,
      0x00, // discovery flags
      0x00, // slot 0 (we answer the slot-0 command)
      0x00, // version
      // Discovery info — hints + charset + nickname.
      0x82, // hint byte 0: PDA/palmtop (0x02) + extension (0x80)
      0x24, // hint byte 1
      XID_CHARSET_ASCII,
    ];
    for (const c of this.nickname) info.push(c.charCodeAt(0) & 0x7f);
    this.sendBody(new Uint8Array([(0x7f << 1) | 0, U_XID_RESPONSE | PF_BIT, ...info]));
  }

  private trace(dir: 'RX' | 'TX', body: Uint8Array): void {
    const hex = Array.from(body).map((b) => b.toString(16).padStart(2, '0')).join(' ');
    const txt = Array.from(body)
      .map((b) => (b >= 0x20 && b < 0x7f ? String.fromCharCode(b) : '.'))
      .join('');
    // eslint-disable-next-line no-console
    console.debug(`[IR-RX] ${dir} ${hex}  |${txt}|`);
  }
}
