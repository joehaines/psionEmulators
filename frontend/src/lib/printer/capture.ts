// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Serial printer capture. The user points the device's printer setup at
// the serial port ("General (Text only)" / Epson / HP driver, print via
// Serial) and prints; we attach the host bridge to the cable UART and
// accumulate every byte the device sends. No bytes ever flow host→device:
// the bridge's attach already raises CTS/DSR/DCD so the driver sees a
// ready printer, and the device only pauses output if it *receives* XOFF
// — which we never send.
//
// Parallel construction to lib/modem/port.ts + transport.ts; no shared
// imports (same convention as the modem and PLP paths).

export interface PrinterBridge {
  serialAttachHost: (uartIndex: number) => boolean;
  serialDetachHost: (uartIndex: number) => boolean;
  serialIsAttached: (uartIndex: number) => boolean;
  serialReadBytes:  (uartIndex: number) => Uint8Array;
}

export interface PrinterCaptureOptions {
  pollHz?: number;                       // default 50
  onData: (chunk: Uint8Array) => void;   // every non-empty RX drain
  onError: (msg: string) => void;
}

export class PrinterCapture {
  private readonly uart: number;
  private readonly br: PrinterBridge;
  private readonly onData: (chunk: Uint8Array) => void;
  private readonly onError: (msg: string) => void;
  private readonly periodMs: number;
  private timer: ReturnType<typeof setInterval> | null = null;
  private attached = false;

  constructor(bridge: PrinterBridge, uartIndex: number, opts: PrinterCaptureOptions) {
    this.br = bridge;
    this.uart = uartIndex;
    this.onData = opts.onData;
    this.onError = opts.onError;
    this.periodMs = 1000 / (opts.pollHz ?? 50);
  }

  isRunning(): boolean { return this.timer !== null; }

  start(): boolean {
    if (this.timer !== null) return true;
    if (this.br.serialIsAttached(this.uart)) {
      this.onError('Serial port is in use — disconnect Remote Link / Modem first.');
      return false;
    }
    if (!this.br.serialAttachHost(this.uart)) {
      this.onError('Could not attach to the serial port.');
      return false;
    }
    this.attached = true;
    this.timer = setInterval(() => this.tick(), this.periodMs);
    return true;
  }

  stop(): void {
    if (this.timer !== null) { clearInterval(this.timer); this.timer = null; }
    if (this.attached) {
      // Final drain so bytes the device pushed between the last tick and
      // Stop aren't lost mid-page.
      this.tick();
      if (this.br.serialIsAttached(this.uart)) this.br.serialDetachHost(this.uart);
      this.attached = false;
    }
  }

  // Public for tests: pump one poll iteration synchronously. Production
  // code uses start() which schedules this on a setInterval.
  tick(): void {
    const rx = this.br.serialReadBytes(this.uart);
    if (rx.length > 0) this.onData(rx);
  }
}
