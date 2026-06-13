// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// PPP protocol-field demux on top of the HDLC frame decoder.
//
// A PPP info-frame on the wire (already de-stuffed and FCS-checked
// by HdlcDecoder) looks like:
//   Address(1) Control(1) Protocol(1 or 2) <info bytes…>
//
// With Address-and-Control Field Compression (ACFC), Address and
// Control may be omitted. With Protocol Field Compression (PFC),
// Protocol may be one byte when its high byte is zero. Both are
// negotiated via LCP options 7 and 8 respectively; we accept
// (Configure-Ack) both when the peer requests them.
//
// We don't *send* compressed forms — every packet we emit uses the
// full 0xFF 0x03 + 16-bit Protocol. This is always legal.

export const PROTO_LCP  = 0xC021;
export const PROTO_PAP  = 0xC023;
export const PROTO_CHAP = 0xC223;
export const PROTO_IPCP = 0x8021;
export const PROTO_IP   = 0x0021;

export interface PppFrame {
  protocol: number;
  info: Uint8Array;
}

// Parse a de-stuffed HDLC payload. Returns null on malformed input.
export function parsePpp(raw: Uint8Array): PppFrame | null {
  let i = 0;
  if (raw.length < 2) return null;
  // ACFC: skip Address(0xFF) Control(0x03) if present.
  if (raw.length >= 2 && raw[0] === 0xFF && raw[1] === 0x03) i = 2;
  if (i >= raw.length) return null;
  let proto: number;
  // PFC: low bit of first protocol byte == 1 means single-byte protocol.
  if ((raw[i] & 0x01) === 0x01) {
    proto = raw[i];
    i += 1;
  } else {
    if (i + 1 >= raw.length) return null;
    proto = (raw[i] << 8) | raw[i + 1];
    i += 2;
  }
  return { protocol: proto, info: raw.subarray(i) };
}

// Build a PPP frame ready for HDLC encoding. Always uses the full
// address/control header and 16-bit protocol field.
export function buildPpp(protocol: number, info: Uint8Array): Uint8Array {
  const out = new Uint8Array(4 + info.length);
  out[0] = 0xFF;
  out[1] = 0x03;
  out[2] = (protocol >>> 8) & 0xFF;
  out[3] = protocol & 0xFF;
  out.set(info, 4);
  return out;
}
