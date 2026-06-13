// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Mode dispatcher: routes incoming bytes between the AT command parser
// (command mode) and the HDLC framer (online / PPP mode). Also owns
// the +++ escape sequence detection that flips a connected modem back
// into command mode, and the ATH/+++ATH hangup path.
//
// Wire layout we coordinate:
//
//   RX from device  →  dispatcher.feed()
//                         │
//             ┌───────────┴───────────┐
//             ▼                       ▼
//        'command'  → AtParser    'online' → HDLC.decode
//                       │                       │
//                       ▼                       ▼
//                     AT result            PPP frames
//                       │                       │
//                  dispatcher.send()       PppClient.handle()
//
// TX bytes that need to reach the device flow back out via the
// transport.send() callback we're constructed with.

import { AtCommandParser, type AtResult } from './at.ts';
import { HdlcDecoder, encodeFrame } from '../ppp/hdlc.ts';
import { buildPpp, parsePpp, PROTO_IP } from '../ppp/demux.ts';

export type DispatcherMode = 'command' | 'online';

export interface DispatcherEvents {
  onAtLine?:   (line: string, result: AtResult) => void;
  onModeChange?: (mode: DispatcherMode) => void;
  // Called when a complete PPP frame has been received in online mode.
  // The caller routes it to LCP/PAP/IPCP/IP.
  onPppFrame?: (protocol: number, info: Uint8Array) => void;
  // Bad-FCS counter changed.
  onFcsErrors?: (count: number) => void;
}

export class ModeDispatcher {
  private mode: DispatcherMode = 'command';
  private at: AtCommandParser;
  private hdlc: HdlcDecoder;
  private send: (bytes: Uint8Array) => void;
  private events: DispatcherEvents;

  // +++ guard time + sequence detection. We need 1 s of silence both
  // sides + exactly three 0x2B bytes + 1 s of silence. We approximate
  // with timestamps.
  private lastTxTs: number = 0;
  private plusCount: number = 0;
  private plusFirstTs: number = 0;

  constructor(send: (bytes: Uint8Array) => void, events: DispatcherEvents = {}) {
    this.at = new AtCommandParser();
    this.hdlc = new HdlcDecoder();
    this.send = send;
    this.events = events;
  }

  getMode(): DispatcherMode { return this.mode; }
  getAt(): AtCommandParser { return this.at; }

  // Bytes arriving from the device.
  feed(bytes: Uint8Array): void {
    const now = performance.now();
    if (this.mode === 'command') this.feedCommand(bytes, now);
    else                          this.feedOnline(bytes, now);
  }

  // Bytes the host wants to send to the device. Routed through HDLC
  // when in online mode; raw otherwise. Tracks tx timing for the
  // +++ guard.
  sendBytes(bytes: Uint8Array): void {
    this.lastTxTs = performance.now();
    this.send(bytes);
  }

  // Convenience: build and send a PPP frame over the wire.
  sendPpp(protocol: number, info: Uint8Array): void {
    const ppp = buildPpp(protocol, info);
    const framed = encodeFrame(ppp);
    this.sendBytes(framed);
  }

  // Force-transition. Used by ModemClient when the AT layer resolves
  // ATDT to "CONNECT" and the dispatcher needs to flip to PPP framing.
  enterOnline(): void {
    if (this.mode === 'online') return;
    this.mode = 'online';
    this.hdlc.reset();
    this.events.onModeChange?.('online');
  }

  enterCommand(): void {
    if (this.mode === 'command') return;
    this.mode = 'command';
    this.events.onModeChange?.('command');
  }

  // ── Command-mode RX ────────────────────────────────────────────
  private feedCommand(bytes: Uint8Array, _now: number): void {
    // Echo first if enabled (Hayes ATE1 default).
    if (this.at.echo) this.send(bytes);
    const lines = this.at.feed(bytes);
    for (const line of lines) {
      const result = this.at.execute(line);
      this.events.onAtLine?.(line, result);
      const rendered = this.at.renderResult(result);
      if (rendered.length > 0) this.send(rendered);
      if (result.kind === 'connect') this.enterOnline();
    }
  }

  // ── Online-mode RX ─────────────────────────────────────────────
  private feedOnline(bytes: Uint8Array, now: number): void {
    // Scan for +++ escape sequence. We start counting only after
    // 1 s of silence preceding the first '+'.
    for (const b of bytes) {
      if (b === 0x2B /* '+' */) {
        const sinceTx = now - this.lastTxTs;
        if (this.plusCount === 0 && sinceTx < 1000) {
          // Not enough silence to enter escape — ignore the +.
          this.plusCount = 0;
          continue;
        }
        if (this.plusCount === 0) this.plusFirstTs = now;
        this.plusCount += 1;
      } else {
        this.plusCount = 0;
      }
    }
    // After 3 '+' bytes and another 1 s silence we'd transition back
    // to command mode. We commit on count==3; the AT parser will then
    // see ATH or ATO as appropriate. We don't enforce the trailing
    // silence (EPOC seldom uses +++ in this path anyway).
    if (this.plusCount >= 3 && (now - this.plusFirstTs) >= 800) {
      this.plusCount = 0;
      this.enterCommand();
      this.send(this.at.renderResult({ kind: 'ok' }));
      return;
    }

    // Hand the bytes (regardless of +++ state) to HDLC. If we ended
    // up flipping mode the early-return above already happened.
    const frames = this.hdlc.feed(bytes);
    for (const raw of frames) {
      const ppp = parsePpp(raw);
      if (!ppp) continue;
      this.events.onPppFrame?.(ppp.protocol, ppp.info);
      // Surface FCS error count whenever it changes.
    }
    if (this.hdlc.badFcs > 0) this.events.onFcsErrors?.(this.hdlc.badFcs);
  }

  // Convenience for callers (LCP/IPCP/IP) that want to push an IP
  // packet onto the wire as a PPP/IP frame.
  sendIp(packet: Uint8Array): void {
    this.sendPpp(PROTO_IP, packet);
  }
}
