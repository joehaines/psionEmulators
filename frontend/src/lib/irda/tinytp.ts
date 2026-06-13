// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Tiny TP — the thin transport that EPOC requires under IrOBEX for
// reliable beaming. It adds two things on top of an LM-MUX connection:
//
//  1. Credit-based flow control. Each side advertises an initial credit
//     in the connect handshake (carried as the LM-Connect user-data), and
//     thereafter every PDU's first octet's low 7 bits carry "delta
//     credit" granted to the peer. A side may only transmit a data PDU if
//     it holds send-credit > 0; each transmitted data PDU spends one.
//
//  2. Segmentation & reassembly (SAR). An SDU larger than one PDU is split
//     across PDUs with the M (more) bit (0x80) set on every PDU but the
//     last. A PDU with no payload is a pure credit update, not an SDU.
//
// First octet of a connect PDU's TTP data: bit7 = P (params present),
// bits0-6 = initial credit. Data PDU first octet: bit7 = M, bits0-6 =
// delta credit.

import { IrLmp } from './irlmp.ts';
import { TTP_M_BIT, TTP_CREDIT_MASK, TTP_INITIAL_CREDIT } from './types.ts';

// Per-PDU payload cap. Chosen to fit the conservative 64-byte IrLAP
// data-size we negotiate (2 LM-MUX header bytes + 1 TTP byte + payload).
const MAX_SEGMENT = 60;

export class TinyTp {
  private readonly lmp: IrLmp;
  private readonly localLsap: number;
  private readonly remoteLsap: number;

  // Credit we may spend sending data PDUs to the peer.
  private sendCredit = 0;
  // Credit we have currently granted the peer (how many PDUs it may send
  // before we must extend more).
  private peerCredit = 0;

  // Reassembly buffer for inbound segmented SDUs.
  private rxSegments: Uint8Array[] = [];

  // Optional "device proof-of-life" clock (IrLAP last-inbound timestamp). When
  // set, the send-credit wait fails only after the device has been completely
  // silent — not merely slow to extend credit. Lets a slow device (e.g. the
  // Series 5, ~2.5x slower than real time) flow-control us indefinitely without
  // tripping the "device stopped acknowledging" timeout, while a genuinely dead
  // device still fails. Set via setActivityClock(); falls back to wall time.
  private activityClock: (() => number) | null = null;
  setActivityClock(fn: () => number): void {
    this.activityClock = fn;
  }

  onSdu: ((sdu: Uint8Array) => void) | null = null;
  // Set true once the peer has sent an LM-Disconnect for this channel.
  private peerDisconnected = false;
  private peerDisconnectResolvers: Array<() => void> = [];

  constructor(lmp: IrLmp, localLsap: number, remoteLsap: number) {
    this.lmp = lmp;
    this.localLsap = localLsap;
    this.remoteLsap = remoteLsap;
  }

  // Connect: open the LM-MUX channel carrying our initial-credit octet,
  // and read the peer's initial credit from the confirm. Wires our data
  // handler before returning.
  async connect(timeoutMs: number): Promise<void> {
    this.peerCredit = TTP_INITIAL_CREDIT;
    // Connect data: initial credit, no parameters (P bit clear).
    const connectData = new Uint8Array([TTP_INITIAL_CREDIT & TTP_CREDIT_MASK]);
    const confirm = await this.lmp.connect(this.localLsap, this.remoteLsap, timeoutMs, connectData);
    // Peer's initial credit is the low 7 bits of the confirm's first octet.
    this.sendCredit = confirm.length > 0 ? confirm[0] & TTP_CREDIT_MASK : 0;
    this.lmp.setDataHandler(this.localLsap, (data) => this.handleData(data));
    this.lmp.setDisconnectHandler(this.localLsap, () => {
      this.peerDisconnected = true;
      const r = this.peerDisconnectResolvers;
      this.peerDisconnectResolvers = [];
      for (const res of r) res();
    });
  }

  disconnect(): void {
    this.lmp.disconnect(this.localLsap);
  }

  // Resolve when the peer initiates a graceful LM-Disconnect (the EPOC file
  // receiver does this after it has saved the received file), or after
  // `timeoutMs` if it never does. Resolves immediately if already disconnected.
  waitForPeerDisconnect(timeoutMs: number): Promise<boolean> {
    if (this.peerDisconnected) return Promise.resolve(true);
    return new Promise<boolean>((resolve) => {
      const timer = setTimeout(() => {
        this.peerDisconnectResolvers = this.peerDisconnectResolvers.filter((r) => r !== onDc);
        resolve(false);
      }, timeoutMs);
      const onDc = () => {
        clearTimeout(timer);
        resolve(true);
      };
      this.peerDisconnectResolvers.push(onDc);
    });
  }

  get availableSendCredit(): number {
    return this.sendCredit;
  }

  // Send an SDU, segmenting as needed. Resolves once every segment has
  // been handed to the link; gates on send-credit, polling until the peer
  // extends more. Rejects if credit never arrives within `timeoutMs`.
  async send(sdu: Uint8Array, timeoutMs = 10000): Promise<void> {
    let offset = 0;
    // An empty SDU still sends one (final, empty) data PDU.
    do {
      const end = Math.min(offset + MAX_SEGMENT, sdu.length);
      const more = end < sdu.length;
      await this.waitForCredit(timeoutMs);
      this.sendPdu(sdu.subarray(offset, end), more);
      offset = end;
    } while (offset < sdu.length);
  }

  private waitForCredit(timeoutMs: number): Promise<void> {
    if (this.sendCredit > 0) return Promise.resolve();
    return new Promise<void>((resolve, reject) => {
      const start = Date.now();
      const poll = setInterval(() => {
        if (this.sendCredit > 0) {
          clearInterval(poll);
          resolve();
          return;
        }
        // Fail only when the device has gone genuinely silent. With an activity
        // clock, "silent" means no IrLAP frame received for timeoutMs — so a
        // slow device that keeps answering polls (just without new credit yet)
        // is given as long as it needs. Without one, fall back to wall-clock
        // since the send started.
        const idleSince = this.activityClock ? this.activityClock() : start;
        if (Date.now() - idleSince > timeoutMs) {
          clearInterval(poll);
          reject(new Error('Tiny TP send-credit exhausted (device stopped acknowledging)'));
        }
      }, 5);
    });
  }

  // Emit one data PDU: TTP header (M bit + delta credit we grant) + body.
  private sendPdu(payload: Uint8Array, more: boolean): void {
    // Replenish the peer back up to the initial credit window.
    const grant = Math.max(0, TTP_INITIAL_CREDIT - this.peerCredit);
    this.peerCredit += grant;
    const header = (more ? TTP_M_BIT : 0) | (grant & TTP_CREDIT_MASK);
    const pdu = new Uint8Array(1 + payload.length);
    pdu[0] = header;
    pdu.set(payload, 1);
    this.sendCredit -= 1;
    this.lmp.sendData(this.localLsap, pdu);
  }

  // Send a pure credit-update PDU (no payload) so the peer can keep
  // transmitting when we have no data of our own to piggyback on.
  private grantCredit(): void {
    const grant = Math.max(0, TTP_INITIAL_CREDIT - this.peerCredit);
    if (grant === 0) return;
    this.peerCredit += grant;
    this.lmp.sendData(this.localLsap, new Uint8Array([grant & TTP_CREDIT_MASK]));
  }

  private handleData(pdu: Uint8Array): void {
    if (pdu.length < 1) return;
    const header = pdu[0];
    const more = (header & TTP_M_BIT) !== 0;
    const delta = header & TTP_CREDIT_MASK;
    this.sendCredit += delta;
    const payload = pdu.subarray(1);

    if (payload.length === 0 && !more) {
      // Pure credit update — no SDU data.
      return;
    }

    // One credit of the peer's granted window was consumed.
    this.peerCredit = Math.max(0, this.peerCredit - 1);
    this.rxSegments.push(payload);
    if (!more) {
      const sdu = concat(this.rxSegments);
      this.rxSegments = [];
      this.onSdu?.(sdu);
    }
    // Keep the peer's window topped up so it can send the next segment /
    // response without stalling.
    if (this.peerCredit <= 1) this.grantCredit();
  }
}

function concat(parts: Uint8Array[]): Uint8Array {
  let total = 0;
  for (const p of parts) total += p.length;
  const out = new Uint8Array(total);
  let off = 0;
  for (const p of parts) {
    out.set(p, off);
    off += p.length;
  }
  return out;
}
