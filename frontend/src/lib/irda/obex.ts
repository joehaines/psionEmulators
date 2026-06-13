// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// IrOBEX — the object-exchange protocol that actually carries the file
// into the device's Infrared inbox. Each OBEX packet is one Tiny TP SDU.
//
// A beam is: CONNECT → PUT (non-final, Name + Length + first Body) →
// PUT… (more Body, device answers Continue 0x90) → PUT-final (End-of-Body,
// device answers Success 0xA0) → DISCONNECT.
//
// Packet layout: opcode/response(1) | packetLen(2 BE) | fields…
// CONNECT additionally has version(1) flags(1) maxPacket(2) before headers.
// Header encoding is keyed by the top two bits of the header id:
//   00 → unicode text   (id, len2, UTF-16BE text incl. NUL terminator)
//   01 → byte sequence  (id, len2, bytes)
//   10 → 1-byte value   (id, value)
//   11 → 4-byte value   (id, value4 BE)

import {
  OBEX_CONNECT,
  OBEX_DISCONNECT,
  OBEX_PUT,
  OBEX_PUT_FINAL,
  OBEX_RESP_CONTINUE,
  OBEX_RESP_SUCCESS,
  OBEX_HDR_NAME,
  OBEX_HDR_LENGTH,
  OBEX_HDR_BODY,
  OBEX_HDR_END_OF_BODY,
  OBEX_VERSION,
  OBEX_DEFAULT_MTU,
} from './types.ts';

// ── Header builders ───────────────────────────────────────────────────
export function nameHeader(name: string): Uint8Array {
  // UTF-16BE text including a trailing NUL (two zero bytes).
  const chars = name.length + 1;
  const out = new Uint8Array(3 + chars * 2);
  out[0] = OBEX_HDR_NAME;
  const len = out.length;
  out[1] = (len >> 8) & 0xff;
  out[2] = len & 0xff;
  let p = 3;
  for (let i = 0; i < name.length; i++) {
    const c = name.charCodeAt(i);
    out[p++] = (c >> 8) & 0xff;
    out[p++] = c & 0xff;
  }
  out[p++] = 0;
  out[p++] = 0;
  return out;
}

export function lengthHeader(byteLen: number): Uint8Array {
  return new Uint8Array([
    OBEX_HDR_LENGTH,
    (byteLen >>> 24) & 0xff,
    (byteLen >>> 16) & 0xff,
    (byteLen >>> 8) & 0xff,
    byteLen & 0xff,
  ]);
}

export function bodyHeader(id: number, data: Uint8Array): Uint8Array {
  const len = 3 + data.length;
  const out = new Uint8Array(len);
  out[0] = id;
  out[1] = (len >> 8) & 0xff;
  out[2] = len & 0xff;
  out.set(data, 3);
  return out;
}

// ── Packet builders ───────────────────────────────────────────────────
export function encodeConnect(maxPacket = OBEX_DEFAULT_MTU): Uint8Array {
  const len = 7;
  return new Uint8Array([
    OBEX_CONNECT,
    (len >> 8) & 0xff,
    len & 0xff,
    OBEX_VERSION,
    0x00, // flags
    (maxPacket >> 8) & 0xff,
    maxPacket & 0xff,
  ]);
}

export function encodePut(final: boolean, headers: Uint8Array[]): Uint8Array {
  let bodyLen = 0;
  for (const h of headers) bodyLen += h.length;
  const len = 3 + bodyLen;
  const out = new Uint8Array(len);
  out[0] = final ? OBEX_PUT_FINAL : OBEX_PUT;
  out[1] = (len >> 8) & 0xff;
  out[2] = len & 0xff;
  let p = 3;
  for (const h of headers) {
    out.set(h, p);
    p += h.length;
  }
  return out;
}

export function encodeDisconnect(): Uint8Array {
  return new Uint8Array([OBEX_DISCONNECT, 0x00, 0x03]);
}

export interface ConnectReply {
  code: number;
  maxPacket: number;
}

export function parseConnectReply(sdu: Uint8Array): ConnectReply {
  if (sdu.length < 7) return { code: sdu[0] ?? 0, maxPacket: OBEX_DEFAULT_MTU };
  const code = sdu[0];
  const maxPacket = (sdu[5] << 8) | sdu[6];
  return { code, maxPacket: maxPacket || OBEX_DEFAULT_MTU };
}

export function responseCode(sdu: Uint8Array): number {
  return sdu.length > 0 ? sdu[0] : 0;
}

// ── High-level client ─────────────────────────────────────────────────
// `exchange` sends one OBEX packet and resolves with the device's reply
// packet. The IrSendClient binds it to Tiny TP send + next-SDU.
export class ObexClient {
  private readonly exchange: (packet: Uint8Array) => Promise<Uint8Array>;
  private maxPacket = OBEX_DEFAULT_MTU;

  constructor(exchange: (packet: Uint8Array) => Promise<Uint8Array>) {
    this.exchange = exchange;
  }

  async connect(): Promise<void> {
    const reply = await this.exchange(encodeConnect(OBEX_DEFAULT_MTU));
    const parsed = parseConnectReply(reply);
    if (parsed.code !== OBEX_RESP_SUCCESS) {
      throw new Error(`OBEX CONNECT rejected: response 0x${parsed.code.toString(16)}`);
    }
    // Honour the smaller of the two MTUs for our PUT packets.
    this.maxPacket = Math.min(OBEX_DEFAULT_MTU, parsed.maxPacket);
  }

  // PUT a file: Name + Length in the first packet, then Body chunks sized
  // to the negotiated MTU, finishing with an End-of-Body in a final
  // packet. onProgress(bytesSoFar) fires after each accepted chunk.
  async putFile(name: string, data: Uint8Array, onProgress?: (n: number) => void): Promise<void> {
    const nameH = nameHeader(name);
    const lenH = lengthHeader(data.length);
    // Bytes of Body that fit in the first packet alongside Name + Length
    // (3-byte PUT header + headers + 3-byte Body header overhead).
    let offset = 0;
    const firstBudget = this.maxPacket - 3 - nameH.length - lenH.length - 3;
    const firstChunk = data.subarray(0, Math.max(0, Math.min(firstBudget, data.length)));
    offset += firstChunk.length;
    const firstFinal = offset >= data.length;
    {
      const headers = [nameH, lenH];
      headers.push(bodyHeader(firstFinal ? OBEX_HDR_END_OF_BODY : OBEX_HDR_BODY, firstChunk));
      const reply = await this.exchange(encodePut(firstFinal, headers));
      this.expectContinueOrSuccess(reply, firstFinal);
      onProgress?.(offset);
    }

    // Remaining body in MTU-sized chunks.
    const chunkBudget = this.maxPacket - 3 - 3; // PUT header + Body header
    while (offset < data.length) {
      const end = Math.min(offset + chunkBudget, data.length);
      const chunk = data.subarray(offset, end);
      const isFinal = end >= data.length;
      const reply = await this.exchange(
        encodePut(isFinal, [bodyHeader(isFinal ? OBEX_HDR_END_OF_BODY : OBEX_HDR_BODY, chunk)]),
      );
      this.expectContinueOrSuccess(reply, isFinal);
      offset = end;
      onProgress?.(offset);
    }
    // An empty file is already fully sent by the first (final) packet
    // above, whose firstChunk is empty and firstFinal is true.
  }

  async disconnect(): Promise<void> {
    await this.exchange(encodeDisconnect());
  }

  private expectContinueOrSuccess(reply: Uint8Array, final: boolean): void {
    const code = responseCode(reply);
    const want = final ? OBEX_RESP_SUCCESS : OBEX_RESP_CONTINUE;
    if (code !== want) {
      throw new Error(
        `OBEX PUT ${final ? '(final)' : ''} got 0x${code.toString(16)}, expected 0x${want.toString(16)}`,
      );
    }
  }
}
