// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// RFC 1662 HDLC-like framing for PPP-in-async-serial.
//
// Frame on the wire (bytes):
//   0x7E  <payload-byte-stuffed>  FCS-low  FCS-high  0x7E
// The two FCS bytes are themselves part of the byte-stuffing region.
//
// Byte-stuffing rules:
//   - 0x7E (flag) and 0x7D (escape) must always be stuffed.
//   - Bytes whose code is < 32 are stuffed if their bit in the
//     "send ACCM" / "receive ACCM" mask is set. Default ACCM at
//     LCP start is 0xFFFFFFFF (stuff every control byte).
//   - Stuffing: emit 0x7D, then (byte XOR 0x20).
//
// FCS-16: RFC 1662 §C.2. Polynomial x^16+x^12+x^5+1 (0x1021), but
// bit-reflected — initial value 0xFFFF, final XOR 0xFFFF, LSB-first.
// This is the standard "FCS-16" / "CRC-16/X-25" variant. Distinct
// from the CRC-CCITT (init 0x0000, non-reflected) used by the PLP
// framing in lib/plp/framing.ts — we deliberately don't share code.

const FLAG    = 0x7E;
const ESC     = 0x7D;
const ESC_XOR = 0x20;

// FCS-16 table (RFC 1662 §C.2). Built once at module load.
const FCS_TAB: Uint16Array = (() => {
  const t = new Uint16Array(256);
  for (let b = 0; b < 256; b++) {
    let v = b;
    for (let i = 0; i < 8; i++) {
      v = (v & 1) ? (v >>> 1) ^ 0x8408 : (v >>> 1);
    }
    t[b] = v;
  }
  return t;
})();

export function fcs16(data: Uint8Array, init: number = 0xFFFF): number {
  let fcs = init;
  for (let i = 0; i < data.length; i++) {
    fcs = (fcs >>> 8) ^ FCS_TAB[(fcs ^ data[i]) & 0xFF];
  }
  return fcs;
}

// Default ACCM: stuff every control byte (0x00..0x1F). LCP can
// negotiate a tighter mask, but we keep the safe default.
export const DEFAULT_ACCM = 0xFFFFFFFF >>> 0;

// Per-direction state — each side has its own ACCM after LCP.
export class HdlcAccm {
  send: number = DEFAULT_ACCM;
  recv: number = DEFAULT_ACCM;
}

// Encode a single PPP payload (address + control + protocol + info
// fields are caller-supplied as a single Uint8Array) into a framed
// byte stream with byte-stuffing and FCS-16.
export function encodeFrame(payload: Uint8Array, sendAccm: number = DEFAULT_ACCM): Uint8Array {
  // Compute FCS over the unstuffed payload.
  const fcs = fcs16(payload);
  const fcsLo = fcs & 0xFF;
  const fcsHi = (fcs >>> 8) & 0xFF;
  const compliment = (fcs ^ 0xFFFF) & 0xFFFF;
  const fcsXorLo = compliment & 0xFF;
  const fcsXorHi = (compliment >>> 8) & 0xFF;

  // Worst case: every byte stuffed → 2x. Plus 4 framing bytes + flag.
  const out: number[] = [FLAG];
  const needsStuff = (b: number) => b === FLAG || b === ESC || (b < 0x20 && ((sendAccm >>> b) & 1) === 1);
  for (let i = 0; i < payload.length; i++) {
    const b = payload[i];
    if (needsStuff(b)) { out.push(ESC); out.push(b ^ ESC_XOR); }
    else out.push(b);
  }
  // Per RFC 1662 the transmitted FCS is the one's-complement of the
  // computed CRC. We compute via the reflected table directly, then
  // XOR; matches the spec's worked examples.
  for (const b of [fcsXorLo, fcsXorHi]) {
    if (needsStuff(b)) { out.push(ESC); out.push(b ^ ESC_XOR); }
    else out.push(b);
  }
  // Suppress unused-var warning by referencing fcsLo/fcsHi:
  void fcsLo; void fcsHi;
  out.push(FLAG);
  return new Uint8Array(out);
}

// Stream decoder. Feed bytes; get a list of well-formed frames.
export class HdlcDecoder {
  private buf: number[] = [];
  private inFrame: boolean = false;
  private escaped: boolean = false;
  badFcs: number = 0;
  framingErrors: number = 0;

  feed(bytes: Uint8Array): Uint8Array[] {
    const out: Uint8Array[] = [];
    for (const b of bytes) {
      if (b === FLAG) {
        if (this.inFrame && this.buf.length >= 2) {
          // Trailing flag — end of frame. The last 2 bytes are FCS.
          const raw = new Uint8Array(this.buf);
          const payload = raw.subarray(0, raw.length - 2);
          const recvFcs = raw[raw.length - 2] | (raw[raw.length - 1] << 8);
          const wantFcs = fcs16(payload) ^ 0xFFFF;
          if ((recvFcs & 0xFFFF) === (wantFcs & 0xFFFF)) {
            out.push(payload);
          } else {
            this.badFcs++;
          }
        } else if (this.inFrame && this.buf.length > 0) {
          // Too-short frame between flags — discard.
          this.framingErrors++;
        }
        this.buf = [];
        this.inFrame = true;
        this.escaped = false;
        continue;
      }
      if (!this.inFrame) continue; // Pre-stream garbage; drop.
      if (b === ESC) { this.escaped = true; continue; }
      if (this.escaped) { this.buf.push(b ^ ESC_XOR); this.escaped = false; continue; }
      this.buf.push(b);
    }
    return out;
  }

  reset(): void { this.buf = []; this.inFrame = false; this.escaped = false; }
}
