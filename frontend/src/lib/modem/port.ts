// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Minimal handle over the WASM serial bridge, scoped to a single UART.
// Lives in lib/modem/ to keep the modem path free of imports from
// lib/wasmBridge.ts (which is shared with the Remote Link path).
//
// The constructor takes the parts of EmulatorControls we need; we never
// hold a reference to the rest of the controls object so a stale
// reference can't drift.

export interface SerialBridge {
  serialAttachHost: (uartIndex: number) => boolean;
  serialDetachHost: (uartIndex: number) => boolean;
  serialIsAttached: (uartIndex: number) => boolean;
  serialReadBytes:  (uartIndex: number) => Uint8Array;
  serialWriteBytes: (uartIndex: number, data: Uint8Array) => number;
}

export class ModemPort {
  private readonly uart: number;
  private readonly br: SerialBridge;

  constructor(bridge: SerialBridge, uartIndex: number) {
    this.br = bridge;
    this.uart = uartIndex;
  }

  attach(): boolean {
    if (this.br.serialIsAttached(this.uart)) return false;
    return this.br.serialAttachHost(this.uart);
  }

  detach(): void {
    if (this.br.serialIsAttached(this.uart)) this.br.serialDetachHost(this.uart);
  }

  isAttached(): boolean { return this.br.serialIsAttached(this.uart); }

  // Returns whatever's queued from the device. Empty array if nothing.
  read(): Uint8Array { return this.br.serialReadBytes(this.uart); }

  // Returns the number of bytes the FIFO accepted; caller resubmits the
  // tail on the next tick if less than data.length.
  write(data: Uint8Array): number {
    if (data.length === 0) return 0;
    return this.br.serialWriteBytes(this.uart, data);
  }
}
