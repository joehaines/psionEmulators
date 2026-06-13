// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// PLP transport — owns the host-side polling loop that bridges the
// WASM serial port to the PLP framing layer and the NCP multiplexer.
//
// Layering, top to bottom:
//
//   PlpClient (rfsv32 / future rpcs)
//     ↓
//   NcpMultiplexer
//     ↓
//   Transport ← (this file)
//     ↓ framing.ts (encodeFrame / FrameDecoder)
//     ↓
//   wasmBridge.serialReadBytes / serialWriteBytes
//     ↓
//   Windermere UART (C++)
//     ↓
//   Revo's Remote Link service (EPOC32)

import { encodeFrame, FrameDecoder } from './framing.ts';

export interface TransportConfig {
  // How often to poll the WASM bridge for inbound bytes. The Revo
  // typically runs PLP at 115200 baud, so 50 Hz gives ~290 B per
  // tick — comfortable headroom for the longest legal frame.
  pollHz?: number;
  // Optional hook for raw byte logging — used by the debug dialog to
  // mirror live traffic into its hex panel.
  onRawRx?: (bytes: Uint8Array) => void;
  onRawTx?: (bytes: Uint8Array) => void;
}

export class PlpTransport {
  private decoder = new FrameDecoder();
  // ReturnType so this compiles in both browser (returns number) and
  // node (returns NodeJS.Timeout) — the lib is exercised from both.
  private intervalId: ReturnType<typeof setInterval> | null = null;
  private frameHandler: ((payload: Uint8Array) => void) | null = null;
  // Explicit fields (not constructor parameter properties) so Node's
  // strip-only TS mode in the e2e harness can load this file.
  private readonly readBytes: () => Uint8Array;
  private readonly writeBytes: (data: Uint8Array) => number;
  private readonly config: TransportConfig;

  constructor(
    readBytes: () => Uint8Array,
    writeBytes: (data: Uint8Array) => number,
    config: TransportConfig = {},
  ) {
    this.readBytes = readBytes;
    this.writeBytes = writeBytes;
    this.config = config;
  }

  // Begin the polling loop. `onFrame` fires for every complete PLP
  // frame the decoder validates; the NcpMultiplexer wires that up to
  // its packet dispatcher.
  start(onFrame: (payload: Uint8Array) => void): void {
    if (this.intervalId !== null) return;
    this.frameHandler = onFrame;
    const periodMs = Math.max(1, Math.round(1000 / (this.config.pollHz ?? 50)));
    this.intervalId = setInterval(() => this.tick(), periodMs);
  }

  stop(): void {
    if (this.intervalId !== null) {
      clearInterval(this.intervalId);
      this.intervalId = null;
    }
    this.frameHandler = null;
    this.decoder.reset();
  }

  // Send a single NCP packet wrapped in a PLP frame.
  sendFrame(payload: Uint8Array): void {
    const framed = encodeFrame(payload);
    this.writeBytes(framed);
    this.config.onRawTx?.(framed);
  }

  // Decoder counters surfaced so the UI can show "we see bytes but
  // they don't parse" diagnostics.
  get badCrc()        { return this.decoder.badCrc; }
  get framingErrors() { return this.decoder.framingErrors; }

  private tick(): void {
    const chunk = this.readBytes();
    if (chunk.length === 0) return;
    this.config.onRawRx?.(chunk);
    const frames = this.decoder.feed(chunk);
    if (this.frameHandler) {
      for (const f of frames) this.frameHandler(f.payload);
    }
  }
}
