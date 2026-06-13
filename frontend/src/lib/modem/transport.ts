// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// 50 Hz UART poll loop for the modem path. Drains RX, fans bytes out
// to a sink, and lets callers push TX bytes that the loop will write
// to the UART (handling back-pressure for us).
//
// Parallel construction to lib/plp/transport.ts; no shared imports.

import { ModemPort } from './port.ts';

export interface ModemTransportOptions {
  pollHz?: number;            // default 50
  onRawRx?: (bytes: Uint8Array) => void;
  onRawTx?: (bytes: Uint8Array) => void;
}

export class ModemTransport {
  private readonly port: ModemPort;
  private readonly onRawRx?: (bytes: Uint8Array) => void;
  private readonly onRawTx?: (bytes: Uint8Array) => void;
  private readonly periodMs: number;
  private timer: ReturnType<typeof setInterval> | null = null;
  private txQueue: Uint8Array[] = [];
  private rxSink: ((b: Uint8Array) => void) | null = null;

  constructor(port: ModemPort, opts: ModemTransportOptions = {}) {
    this.port = port;
    this.onRawRx = opts.onRawRx;
    this.onRawTx = opts.onRawTx;
    this.periodMs = 1000 / (opts.pollHz ?? 50);
  }

  setRxSink(sink: (b: Uint8Array) => void): void {
    this.rxSink = sink;
  }

  start(): void {
    if (this.timer !== null) return;
    this.timer = setInterval(() => this.tick(), this.periodMs);
  }

  stop(): void {
    if (this.timer !== null) { clearInterval(this.timer); this.timer = null; }
    this.txQueue = [];
  }

  // Enqueue bytes for the next tick to drain into the UART.
  send(bytes: Uint8Array): void {
    if (bytes.length === 0) return;
    this.txQueue.push(bytes);
  }

  // Public for tests: pump one poll iteration synchronously. Production
  // code uses start() which schedules this on a setInterval.
  tick(): void {
    // Drain RX first so the sink reacts to incoming bytes before we
    // potentially write a response.
    const rx = this.port.read();
    if (rx.length > 0) {
      this.onRawRx?.(rx);
      if (this.rxSink) this.rxSink(rx);
    }

    // Drain TX queue with back-pressure handling. The C++ FIFO is
    // ~4 KiB, so on a typical 50 Hz tick we won't see partial writes;
    // when we do, push the tail back on the front of the queue.
    while (this.txQueue.length > 0) {
      const head = this.txQueue[0];
      const accepted = this.port.write(head);
      if (accepted === 0) break;
      this.onRawTx?.(head.subarray(0, accepted));
      if (accepted < head.length) {
        this.txQueue[0] = head.subarray(accepted);
        break;
      }
      this.txQueue.shift();
    }
  }
}
