// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// PLP link-layer framing.
//
// Each PLP frame on the wire looks like:
//
//   SYN | DLE STX | <DLE-stuffed payload> | DLE ETX | CRC-hi CRC-lo
//
// where SYN = 0x16, DLE = 0x10, STX = 0x02, ETX = 0x03. Inside the
// payload, every literal DLE byte is escaped as DLE DLE so the
// receiver can find the real ETX. The 16-bit CRC is computed over
// the UN-stuffed payload only (not the framing bytes), uses
// CRC-16-CCITT (poly 0x1021), seed 0x0000, no final XOR, and is
// transmitted **big-endian on the wire** (high byte first, then low
// byte).
//
// The SYN prefix is structural: every frame the real Revo emits has
// it (`16 10 02 21 10 03 34 43`), so EPOC's framer almost certainly
// hunts for SYN as the start-of-frame marker. We were initially
// treating the byte as line noise on the receive side, which works
// fine for decoding (the decoder skips up to the first DLE STX
// regardless), but it meant our outbound frames lacked the prefix
// and were silently dropped by the device's framer.
//
// Verified against a real Revo capture: payload [0x21] → wire trailer
// `34 43` (big-endian CRC of 0x3443 == crc16Ccitt([0x21])).

import { DLE, STX, ETX } from './types.ts';

// CRC-16-CCITT computed bit-serially. Slow but obvious. The frame
// sizes we send (max ~2 KiB payload) and the 50 Hz host poll mean
// this never approaches a bottleneck.
export function crc16Ccitt(data: Uint8Array, seed = 0x0000): number {
  let crc = seed;
  for (let i = 0; i < data.length; i++) {
    crc ^= (data[i] << 8) & 0xFFFF;
    for (let b = 0; b < 8; b++) {
      if (crc & 0x8000) crc = ((crc << 1) ^ 0x1021) & 0xFFFF;
      else              crc = (crc << 1) & 0xFFFF;
    }
  }
  return crc & 0xFFFF;
}

// Wraps `payload` into a complete on-the-wire frame: DLE-STX prologue,
// DLE-stuffed body, DLE-ETX, then CRC-hi / CRC-lo. Returns a fresh
// buffer; the caller hands this straight to the WASM serial bridge.
export const SYN = 0x16;
export function encodeFrame(payload: Uint8Array): Uint8Array {
  // Worst case is every byte being a DLE (which we'd double), plus
  // the 5-byte fixed framing (SYN + DLE STX + DLE ETX) and the 2-byte
  // CRC trailer.
  const out: number[] = [SYN, DLE, STX];
  for (let i = 0; i < payload.length; i++) {
    const b = payload[i];
    if (b === DLE) out.push(DLE, DLE);
    else           out.push(b);
  }
  out.push(DLE, ETX);
  const crc = crc16Ccitt(payload);
  // Big-endian CRC trailer (hi byte first).
  out.push((crc >> 8) & 0xFF, crc & 0xFF);
  return new Uint8Array(out);
}

// ── Streaming decoder ─────────────────────────────────────────────────
// PLP is a byte stream — frames may straddle WASM polls — so the
// decoder keeps state between feed() calls. Each feed() returns zero
// or more complete payloads (already un-stuffed and CRC-verified) and
// silently drops invalid framing.

// Plain numeric states (rather than a TS enum) so the test runner's
// strip-only TS mode can parse this file.
const ST_HUNTING      = 0; // looking for prologue DLE
const ST_PROLOGUE_DLE = 1; // saw DLE while hunting; expect STX next
const ST_IN_FRAME     = 2; // collecting payload bytes
const ST_ESCAPE_OR_END = 3; // saw DLE while in frame; expect DLE or ETX
const ST_GOT_ETX      = 4; // saw DLE ETX, expecting CRC-lo
const ST_GOT_CRC_LO   = 5; // have CRC-lo, expecting CRC-hi

export interface DecodedFrame {
  payload: Uint8Array;
}

export class FrameDecoder {
  // Stateful between calls.
  private state: number = ST_HUNTING;
  private payload: number[] = [];
  // First CRC byte off the wire is the high byte (big-endian
  // trailer); the second byte completes the 16-bit value.
  private crcHi = 0;
  // Tally of frames the decoder rejected since startup. Useful for
  // surfacing "we're seeing bytes but they don't parse" in the UI.
  badCrc = 0;
  framingErrors = 0;

  feed(bytes: Uint8Array): DecodedFrame[] {
    const out: DecodedFrame[] = [];
    for (let i = 0; i < bytes.length; i++) {
      const b = bytes[i];
      switch (this.state) {
        case ST_HUNTING:
          // Look for the DLE-STX prologue. Anything else is line noise
          // (the wire often has wake bytes before the first frame).
          if (b === DLE) this.state = ST_PROLOGUE_DLE;
          break;

        case ST_PROLOGUE_DLE:
          if (b === STX) {
            this.payload = [];
            this.state = ST_IN_FRAME;
          } else if (b === DLE) {
            // Two DLEs in a row while hunting — stay armed; the second
            // DLE is still a candidate prologue start.
            this.state = ST_PROLOGUE_DLE;
          } else {
            this.state = ST_HUNTING;
          }
          break;

        case ST_IN_FRAME:
          if (b === DLE) this.state = ST_ESCAPE_OR_END;
          else           this.payload.push(b);
          break;

        case ST_ESCAPE_OR_END:
          if (b === DLE) {
            this.payload.push(DLE);
            this.state = ST_IN_FRAME;
          } else if (b === ETX) {
            this.state = ST_GOT_ETX;
          } else {
            // DLE inside the payload must be followed by DLE (escape)
            // or ETX (terminator). Anything else is corruption — drop
            // the in-flight frame and resync.
            this.framingErrors++;
            this.state = ST_HUNTING;
          }
          break;

        case ST_GOT_ETX:
          this.crcHi = b;
          this.state = ST_GOT_CRC_LO;
          break;

        case ST_GOT_CRC_LO: {
          const crcReceived = (this.crcHi << 8) | b;
          const payloadBytes = new Uint8Array(this.payload);
          const crcExpected = crc16Ccitt(payloadBytes);
          if (crcReceived === crcExpected) {
            out.push({ payload: payloadBytes });
          } else {
            this.badCrc++;
          }
          this.state = ST_HUNTING;
          this.payload = [];
          break;
        }
      }
    }
    return out;
  }

  // Drops any in-flight frame state. Call after detach so the next
  // attach starts clean.
  reset(): void {
    this.state = ST_HUNTING;
    this.payload = [];
    this.crcHi = 0;
  }
}
