// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// IrDA SIR (Serial Infrared) async framing — the lowest layer of the
// infrared stack. At SIR speeds the IrLAP frame is wrapped in an
// asynchronous "wrapping layer" identical in spirit to HDLC:
//
//   xBOF… | BOF 0xC0 | <escaped data> | <escaped FCS-lo FCS-hi> | EOF 0xC1
//
// Transparency (byte stuffing): any 0xC0 / 0xC1 / 0x7D appearing in the
// data or the FCS is replaced by CE (0x7D) followed by that byte XOR
// 0x20. The FCS is the 16-bit HDLC frame-check sequence (CRC-16 with the
// reflected CCITT polynomial 0x8408, seeded 0xFFFF, then ones-
// complemented) computed over the UN-escaped frame data, transmitted
// least-significant byte first.
//
// The host (us) is the only station on the wire feeding the emulated
// device's UART1 in SIR mode, so a single BOF is sufficient; the EOF is
// what the device's framer keys on. Real hardware prepends additional
// "extra BOF" (xBOF) bytes for transceiver wake-up — harmless here since
// the bytes go straight into the emulated UART rxFifo, but we emit one so
// captures look conventional.
//
// FCS reference: the "good FCS" magic computed over data+FCS at the
// receiver is 0xF0B8 — exercised by the framing test.

// Async-wrapper control bytes.
export const BOF = 0xc0; // begin-of-frame flag
export const EOF = 0xc1; // end-of-frame flag
export const CE = 0x7d; // control-escape
export const ESC_XOR = 0x20; // transparency XOR mask

// HDLC FCS-16 computed bit-serially over `data` with the reflected
// CCITT polynomial. Returns the running (NON-complemented) value; pass
// the result of a previous call as `seed` to continue a computation.
// The bit-serial form mirrors lib/plp/framing.ts — the frame sizes here
// (≤ a couple KiB) mean speed is irrelevant.
export function fcs16(data: Uint8Array, seed = 0xffff): number {
  let crc = seed;
  for (let i = 0; i < data.length; i++) {
    crc ^= data[i];
    for (let b = 0; b < 8; b++) {
      if (crc & 1) crc = (crc >>> 1) ^ 0x8408;
      else crc = crc >>> 1;
    }
  }
  return crc & 0xffff;
}

// True iff `frame` (the un-escaped data followed by its two FCS bytes)
// carries a valid FCS. A correct frame yields the HDLC "good FCS" magic
// 0xF0B8 when the running CRC is taken over data+FCS.
export function fcsValid(dataPlusFcs: Uint8Array): boolean {
  return fcs16(dataPlusFcs) === 0xf0b8;
}

// Append `b` to `out`, escaping it if it collides with a control byte.
function pushEscaped(out: number[], b: number): void {
  if (b === BOF || b === EOF || b === CE) {
    out.push(CE, b ^ ESC_XOR);
  } else {
    out.push(b);
  }
}

// Wrap an IrLAP frame body into a complete on-the-wire SIR frame:
// xBOF + BOF, escaped body, escaped FCS (lo, hi), EOF. Returns a fresh
// buffer the caller hands straight to the UART bridge.
export function encodeSirFrame(body: Uint8Array): Uint8Array {
  const out: number[] = [BOF, BOF]; // one xBOF + the real BOF
  for (let i = 0; i < body.length; i++) pushEscaped(out, body[i]);
  // FCS is the complement of the running CRC over the body, LSB first.
  const crc = fcs16(body) ^ 0xffff;
  pushEscaped(out, crc & 0xff);
  pushEscaped(out, (crc >> 8) & 0xff);
  out.push(EOF);
  return new Uint8Array(out);
}

// ── Streaming decoder ─────────────────────────────────────────────────
// SIR is a byte stream — frames may straddle UART polls — so the decoder
// keeps state across feed() calls. Each feed() returns zero or more
// complete IrLAP frame bodies (un-escaped, FCS-verified and stripped).

const ST_HUNTING = 0; // outside a frame, waiting for BOF
const ST_IN_FRAME = 1; // collecting (possibly escaped) bytes
const ST_ESCAPE = 2; // saw CE, next byte is XORed back

export interface DecodedFrame {
  // Frame body with the two trailing FCS bytes already stripped.
  body: Uint8Array;
}

export class SirDecoder {
  private state = ST_HUNTING;
  private buf: number[] = [];
  // Diagnostics surfaced to the UI: bytes seen but rejected.
  badFcs = 0;
  framingErrors = 0;

  feed(bytes: Uint8Array): DecodedFrame[] {
    const out: DecodedFrame[] = [];
    for (let i = 0; i < bytes.length; i++) {
      const b = bytes[i];
      switch (this.state) {
        case ST_HUNTING:
          if (b === BOF) {
            this.buf = [];
            this.state = ST_IN_FRAME;
          }
          // Anything else (xBOF runs, line idle) is skipped.
          break;

        case ST_IN_FRAME:
          if (b === BOF) {
            // A fresh BOF restarts the frame (treat the partial as a
            // wake-up run, not data).
            this.buf = [];
          } else if (b === EOF) {
            this.finishFrame(out);
            this.state = ST_HUNTING;
          } else if (b === CE) {
            this.state = ST_ESCAPE;
          } else {
            this.buf.push(b);
          }
          break;

        case ST_ESCAPE:
          // CE followed by EOF/BOF is illegal — resync.
          if (b === EOF || b === BOF) {
            this.framingErrors++;
            this.buf = [];
            this.state = b === BOF ? ST_IN_FRAME : ST_HUNTING;
          } else {
            this.buf.push(b ^ ESC_XOR);
            this.state = ST_IN_FRAME;
          }
          break;
      }
    }
    return out;
  }

  private finishFrame(out: DecodedFrame[]): void {
    // Need at least the 2-byte FCS.
    if (this.buf.length < 2) {
      if (this.buf.length > 0) this.framingErrors++;
      this.buf = [];
      return;
    }
    const full = new Uint8Array(this.buf);
    if (!fcsValid(full)) {
      this.badFcs++;
      this.buf = [];
      return;
    }
    out.push({ body: full.subarray(0, full.length - 2) });
    this.buf = [];
  }

  reset(): void {
    this.state = ST_HUNTING;
    this.buf = [];
  }
}
