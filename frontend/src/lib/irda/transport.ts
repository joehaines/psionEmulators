// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// IrDA transport — the host-side polling loop bridging the WASM UART1
// (SIR/IrDA port on Windermere) to the IrLAP layer. Mirrors
// lib/plp/transport.ts.
//
//   IrSendClient → IrLmp → IrLap → Transport (this) → SIR framing →
//   serialReadBytes/serialWriteBytes → Windermere UART1 → EPOC IrDA stack
//
// Each tick drains inbound UART bytes through the SIR decoder, dispatches
// complete IrLAP frames, then ticks IrLAP so its NRM polling / Tiny TP
// credit flow makes progress.

import { encodeSirFrame, SirDecoder } from './framing.ts';

export interface IrTransportConfig {
  // Poll cadence. IrLAP turnaround is timing-sensitive; the default
  // matches the PLP transport's 50 Hz, raised by callers if needed.
  pollHz?: number;
  onRawRx?: (bytes: Uint8Array) => void;
  onRawTx?: (bytes: Uint8Array) => void;
}

export class IrTransport {
  private readonly decoder = new SirDecoder();
  private intervalId: ReturnType<typeof setInterval> | null = null;
  private readonly readBytes: () => Uint8Array;
  private readonly writeBytes: (data: Uint8Array) => number;
  private readonly config: IrTransportConfig;
  private frameHandler: ((body: Uint8Array) => void) | null = null;
  private tickHandler: (() => void) | null = null;

  constructor(
    readBytes: () => Uint8Array,
    writeBytes: (data: Uint8Array) => number,
    config: IrTransportConfig = {},
  ) {
    this.readBytes = readBytes;
    this.writeBytes = writeBytes;
    this.config = config;
  }

  start(onFrame: (body: Uint8Array) => void, onTick: () => void): void {
    if (this.intervalId !== null) return;
    this.frameHandler = onFrame;
    this.tickHandler = onTick;
    const periodMs = Math.max(1, Math.round(1000 / (this.config.pollHz ?? 50)));
    this.intervalId = setInterval(() => this.tick(), periodMs);
  }

  stop(): void {
    if (this.intervalId !== null) {
      clearInterval(this.intervalId);
      this.intervalId = null;
    }
    this.frameHandler = null;
    this.tickHandler = null;
    this.decoder.reset();
  }

  // Send a single IrLAP frame body, SIR-wrapped.
  sendFrame(body: Uint8Array): void {
    const framed = encodeSirFrame(body);
    this.writeBytes(framed);
    this.config.onRawTx?.(framed);
  }

  get badFcs() {
    return this.decoder.badFcs;
  }
  get framingErrors() {
    return this.decoder.framingErrors;
  }

  private tick(): void {
    const chunk = this.readBytes();
    if (chunk.length > 0) {
      this.config.onRawRx?.(chunk);
      const frames = this.decoder.feed(chunk);
      if (this.frameHandler) for (const f of frames) this.frameHandler(f.body);
    }
    this.tickHandler?.();
  }
}
