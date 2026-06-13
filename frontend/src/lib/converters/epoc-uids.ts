// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// EPOC file header detection.
//
// Every EPOC (Series 5 / 5mx / Revo) file starts with a 16-byte header:
//   bytes 0-3   UID1   (file class, e.g. 0x10000037 = "direct file store")
//   bytes 4-7   UID2   (sub-class)
//   bytes 8-11  UID3   (application / specific type)
//   bytes 12-15 UID checksum
//
// The checksum lets us cheaply distinguish a real EPOC file from anything
// else (plain text, a foreign binary, a truncated file).  We refuse to
// convert anything whose checksum doesn't verify.

export const EPOC_HEADER_BYTES = 16;

// UID1 values that wrap a "real" EPOC document store.
export const UID1_DIRECT_FILE_STORE   = 0x10000037;
export const UID1_PERMANENT_FILE_STORE = 0x10000039;

// UID3 values for the apps we convert.  UID3 is what differentiates a Word
// document from a Sheet from a Sketch — UID1/UID2 are usually the generic
// store wrapper, with the app-specific identity sitting in UID3.
export const UID3_WORD          = 0x1000007F;
export const UID3_SHEET         = 0x10000088;
export const UID3_SKETCH_APP    = 0x1000006D;  // the Sketch app's own UID
export const UID3_SKETCH_FILE   = 0x1000007D;  // saved Sketch *picture* files
export const UID3_RECORD        = 0x1000007E;

// MBM (Multi-BitMap) files — Sketch saves these as its native picture
// format.  An MBM has its own UID3 distinct from the Sketch app UID.
export const UID2_MBM = 0x10000042;
export const UID3_MBM = 0x10000040;

export interface EpocHeader {
  uid1: number;
  uid2: number;
  uid3: number;
  checksum: number;
}

export function readUint32LE(bytes: Uint8Array, offset: number): number {
  return (
    (bytes[offset]       |
     bytes[offset + 1] <<  8 |
     bytes[offset + 2] << 16 |
     bytes[offset + 3] << 24) >>> 0
  );
}

export function readEpocHeader(bytes: Uint8Array): EpocHeader | null {
  if (bytes.length < EPOC_HEADER_BYTES) return null;
  return {
    uid1:     readUint32LE(bytes, 0),
    uid2:     readUint32LE(bytes, 4),
    uid3:     readUint32LE(bytes, 8),
    checksum: readUint32LE(bytes, 12),
  };
}

// EPOC UID checksum: two CCITT-16 CRCs (polynomial 0x1021) computed
// over the even-indexed and odd-indexed bytes of the 12-byte UID block,
// combined as (odd_crc << 16) | even_crc.  This is the standard Symbian
// "UID checksum" algorithm — same one psiconv uses to sniff EPOC files.
export function computeUidChecksum(uid1: number, uid2: number, uid3: number): number {
  const bytes = new Uint8Array(12);
  const dv = new DataView(bytes.buffer);
  dv.setUint32(0, uid1, true);
  dv.setUint32(4, uid2, true);
  dv.setUint32(8, uid3, true);

  let evenCrc = 0;
  let oddCrc  = 0;
  for (let i = 0; i < 12; i++) {
    if ((i & 1) === 0) evenCrc = crc16Step(evenCrc, bytes[i]);
    else               oddCrc  = crc16Step(oddCrc,  bytes[i]);
  }
  return ((oddCrc << 16) | evenCrc) >>> 0;
}

function crc16Step(crc: number, byte: number): number {
  // Standard CCITT-16 (poly 0x1021), MSB-first, no reflection, no final XOR.
  crc = (crc ^ (byte << 8)) & 0xFFFF;
  for (let i = 0; i < 8; i++) {
    crc = ((crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1)) & 0xFFFF;
  }
  return crc;
}

export function isValidEpocHeader(h: EpocHeader): boolean {
  return computeUidChecksum(h.uid1, h.uid2, h.uid3) === h.checksum;
}
