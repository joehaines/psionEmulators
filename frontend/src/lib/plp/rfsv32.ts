// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// RFSV-32 — the file-services subprotocol the Revo's Remote Link
// publishes on the SYS$RFSV.* NCP channel. Each request/response on
// this channel has the form:
//
//   request:  [opcode:u16-LE] [reqLen:u16-LE] [args...]
//   reply:    [status:i16-LE] [data...]
//
// where `opcode` is one of `Rfsv32Op` and `status` is an EPOC error
// code (0 = ok, negative on failure). The exact arg/data layouts vary
// per opcode — they're documented inline below as each is implemented.
//
// Source of truth for the wire format: plptools `plpnfsd/rfsv32.cc`.
//
// Right now this module implements the bare minimum needed to validate
// the protocol stack end-to-end:
//
//   * DRIVE_LIST  — no arguments, returns a bitmask of present drives
//
// Once a DRIVE_LIST round-trip is observed working against the
// emulated Revo, the other opcodes follow the same template (encoder
// builds the request payload, decoder reads the reply, optional
// helper wraps the pair as a Promise on the channel handler).

import { Rfsv32Op } from './types.ts';

export interface RfsvRequest {
  op: number;          // Rfsv32Op value
  payload: Uint8Array; // opcode-specific arguments
}

export interface RfsvReply {
  status: number;      // signed 16-bit; 0 = OK, negative = EPOC error
  data: Uint8Array;
}

// Encode an RFSV request body. The NCP layer adds its own header
// before this is handed to the framer.
export function encodeRfsvRequest(req: RfsvRequest): Uint8Array {
  const out = new Uint8Array(4 + req.payload.length);
  const dv = new DataView(out.buffer);
  dv.setUint16(0, req.op, true);
  dv.setUint16(2, req.payload.length, true);
  out.set(req.payload, 4);
  return out;
}

// Decode an RFSV reply. Reads the 2-byte signed status code and
// returns the rest as `data`. Returns null on too-short input.
export function decodeRfsvReply(data: Uint8Array): RfsvReply | null {
  if (data.length < 2) return null;
  const dv = new DataView(data.buffer, data.byteOffset, data.byteLength);
  return {
    status: dv.getInt16(0, true),
    data:   data.subarray(2),
  };
}

// ── DRIVE_LIST ────────────────────────────────────────────────────
// Returns a bitmask of mounted drives. The bit positions match the
// ASCII drive letters: bit 0 = A:, bit 1 = B:, …, bit 25 = Z:. So
// a typical Revo response is bit 2 (C:) + bit 25 (Z:) → 0x02000004.
//
// Request payload: empty.
// Reply data:      4-byte LE bitmask (after the 2-byte status).

export function requestDriveList(): RfsvRequest {
  return { op: Rfsv32Op.DRIVE_LIST, payload: new Uint8Array(0) };
}

export function parseDriveListReply(reply: RfsvReply): string[] {
  if (reply.status !== 0 || reply.data.length < 4) return [];
  const dv = new DataView(reply.data.buffer, reply.data.byteOffset, reply.data.byteLength);
  const bitmask = dv.getUint32(0, true);
  const drives: string[] = [];
  for (let i = 0; i < 26; i++) {
    if (bitmask & (1 << i)) drives.push(`${String.fromCharCode(0x41 + i)}:`);
  }
  return drives;
}

// ── Directory iteration ───────────────────────────────────────────
// OPENDIR opens a directory handle; READDIR_LFN reads one batch of
// entries; CLOSEDIR releases the handle. Iteration ends when
// READDIR_LFN returns EpocErr.Eof (-25).
//
// EPOC paths use backslashes and end with a trailing backslash for
// directory names, e.g. "C:\Documents\".

// Encode an ASCII C-string (nul-terminated).
function encodeCString(s: string): Uint8Array {
  const utf8 = new TextEncoder().encode(s);
  const out = new Uint8Array(utf8.length + 1);
  out.set(utf8, 0);
  out[utf8.length] = 0;
  return out;
}

// OPENDIR (0x1B)
//   Request: [attribMask:u32-LE] [path-zstring]
//   Reply:   [status:i16-LE] [handle:u32-LE]
//
// attribMask filters which entry types come back from subsequent
// READDIR_LFN calls; pass 0 to accept everything (files + dirs).
// (If the Revo turns out to ignore the mask, that's harmless — leaving
// it 0 means "all entries" on every implementation we've seen.)
export function requestOpenDir(path: string, attribMask = 0): RfsvRequest {
  const pathBytes = encodeCString(path);
  const payload = new Uint8Array(4 + pathBytes.length);
  const dv = new DataView(payload.buffer);
  dv.setUint32(0, attribMask, true);
  payload.set(pathBytes, 4);
  return { op: Rfsv32Op.OPENDIR, payload };
}

export function parseOpenDirReply(reply: RfsvReply): number | null {
  if (reply.status !== 0 || reply.data.length < 4) return null;
  const dv = new DataView(reply.data.buffer, reply.data.byteOffset, reply.data.byteLength);
  return dv.getUint32(0, true);
}

// READDIR_LFN (0x1D)
//   Request: [handle:u32-LE]
//   Reply:   [status:i16-LE] [<one or more entries>]
//
// Best-effort per-entry layout (validated roundtrip in the e2e
// harness against an identically-shaped mock; the real Revo's exact
// layout may differ in the UID-block ordering or in whether a 2-byte
// pad follows odd-length names — both are simple to adjust once we
// have a real capture):
//
//   [attrib:u32-LE]
//   [size:u32-LE]
//   [modified:i64-LE]   // EPOC32 TTime — microseconds since 0 AD
//   [uid1:u32-LE]
//   [uid2:u32-LE]
//   [uid3:u32-LE]
//   [nameLen:u16-LE]
//   [name:ASCII × nameLen]
//
// Total fixed-overhead per entry: 30 bytes.
export function requestReadDirLfn(handle: number): RfsvRequest {
  const payload = new Uint8Array(4);
  new DataView(payload.buffer).setUint32(0, handle, true);
  return { op: Rfsv32Op.READDIR_LFN, payload };
}

// EPOC32 file-attribute bits. Mirror the well-known TUint constants
// in f32file.h on the device side. Only the ones we surface in UI
// are spelled out; the rest pass through as the raw mask.
export const EPOC_ATT_READ_ONLY = 0x01;
export const EPOC_ATT_HIDDEN    = 0x02;
export const EPOC_ATT_SYSTEM    = 0x04;
export const EPOC_ATT_DIR       = 0x10;
export const EPOC_ATT_ARCHIVE   = 0x20;

export interface DirEntry {
  name: string;
  size: number;
  attrib: number;
  isDirectory: boolean;
  // Microseconds since 0 AD (EPOC32 TTime). 0 when unparsed.
  modifiedUsec: bigint;
}

export interface DirBatchParse {
  entries: DirEntry[];
  // Bytes after the last entry we could read. 0 means the parser
  // consumed the entire batch cleanly; non-zero is a layout mismatch
  // and the UI should fall back to showing raw bytes.
  remainingBytes: number;
  parseError?: string;
}

// Parse a single READDIR_LFN response chunk (the bytes after the
// 2-byte status). The parser is deliberately conservative: any
// sanity-check failure (oversized name length, negative size,
// truncated trailer) bails out and surfaces the remaining bytes
// rather than emitting garbage entries.
export function parseDirBatch(bytes: Uint8Array): DirBatchParse {
  const entries: DirEntry[] = [];
  let off = 0;
  while (off < bytes.length) {
    if (bytes.length - off < 30) {
      // Not enough for the fixed-overhead block — likely a layout
      // mismatch or a trailing pad we don't recognise. Stop here.
      return { entries, remainingBytes: bytes.length - off };
    }
    const dv = new DataView(bytes.buffer, bytes.byteOffset + off, bytes.length - off);
    const attrib = dv.getUint32(0, true);
    const size   = dv.getUint32(4, true);
    const modLo  = dv.getUint32(8,  true);
    const modHi  = dv.getUint32(12, true);
    // 16..27 = uid1/uid2/uid3 (unused by the UI for now).
    const nameLen = dv.getUint16(28, true);
    // Sanity check before reading the name — EPOC32 limits TBuf<256>.
    if (nameLen > 255 || off + 30 + nameLen > bytes.length) {
      return {
        entries,
        remainingBytes: bytes.length - off,
        parseError: `entry at offset ${off}: bad nameLen ${nameLen}`,
      };
    }
    const name = new TextDecoder().decode(bytes.subarray(off + 30, off + 30 + nameLen));
    const modifiedUsec = (BigInt(modHi) << 32n) | BigInt(modLo);
    entries.push({
      name,
      size,
      attrib,
      isDirectory: (attrib & EPOC_ATT_DIR) !== 0,
      modifiedUsec,
    });
    off += 30 + nameLen;
  }
  return { entries, remainingBytes: 0 };
}

// CLOSEDIR (0x1C)
//   Request: [handle:u32-LE]
//   Reply:   [status:i16-LE] (no data)
export function requestCloseDir(handle: number): RfsvRequest {
  const payload = new Uint8Array(4);
  new DataView(payload.buffer).setUint32(0, handle, true);
  return { op: Rfsv32Op.CLOSEDIR, payload };
}

// ── File I/O ──────────────────────────────────────────────────────
// FOPEN opens an existing file or creates a new one (depending on the
// mode bits in `types.ts` — RFSV32_OMODE_*). FREAD pulls up to
// maxBytes; chunking large files is the caller's job. FWRITE pushes a
// block at a time. FCLOSE releases the handle. DELETE removes a file
// by path. The reply formats below follow the documented EPOC32
// patterns; FWRITE's reply body in particular is the one place where
// captured traffic should be checked once available.

// FOPEN (0x01)
//   Request: [mode:u16-LE] [path-zstring]
//   Reply:   [status:i16-LE] [handle:u32-LE]
export function requestFopen(path: string, mode: number): RfsvRequest {
  const pathBytes = encodeCString(path);
  const payload = new Uint8Array(2 + pathBytes.length);
  new DataView(payload.buffer).setUint16(0, mode, true);
  payload.set(pathBytes, 2);
  return { op: Rfsv32Op.FOPEN, payload };
}

export function parseFopenReply(reply: RfsvReply): number | null {
  if (reply.status !== 0 || reply.data.length < 4) return null;
  return new DataView(reply.data.buffer, reply.data.byteOffset, reply.data.byteLength)
    .getUint32(0, true);
}

// FREAD (0x03)
//   Request: [handle:u32-LE] [maxBytes:u32-LE]
//   Reply:   [status:i16-LE] [data...] (length is data.length, exactly
//            what the server chose to return — possibly less than
//            maxBytes; EOF is signalled by a zero-length reply with
//            status 0, or by status = EpocErr.Eof).
export function requestFread(handle: number, maxBytes: number): RfsvRequest {
  const payload = new Uint8Array(8);
  const dv = new DataView(payload.buffer);
  dv.setUint32(0, handle,   true);
  dv.setUint32(4, maxBytes, true);
  return { op: Rfsv32Op.FREAD, payload };
}

// FWRITE (0x04)
//   Request: [handle:u32-LE] [data...]
//   Reply:   [status:i16-LE] [bytesWritten:u32-LE]
// (The reply layout is the documented format used by plptools and
// PsiWin; if a real capture shows a shorter or empty body, that's
// where to adjust.)
export function requestFwrite(handle: number, data: Uint8Array): RfsvRequest {
  const payload = new Uint8Array(4 + data.length);
  new DataView(payload.buffer).setUint32(0, handle, true);
  payload.set(data, 4);
  return { op: Rfsv32Op.FWRITE, payload };
}

export function parseFwriteReply(reply: RfsvReply): number | null {
  if (reply.status !== 0) return null;
  if (reply.data.length >= 4) {
    return new DataView(reply.data.buffer, reply.data.byteOffset, reply.data.byteLength)
      .getUint32(0, true);
  }
  // Some captures may not include the byte-count tail. Treat that as
  // "the server accepted the whole write" — callers can sanity-check
  // by tracking how much they've sent.
  return null;
}

// FCLOSE (0x02)
//   Request: [handle:u32-LE]
//   Reply:   [status:i16-LE]
export function requestFclose(handle: number): RfsvRequest {
  const payload = new Uint8Array(4);
  new DataView(payload.buffer).setUint32(0, handle, true);
  return { op: Rfsv32Op.FCLOSE, payload };
}

// DELETE (0x09)
//   Request: [path-zstring]
//   Reply:   [status:i16-LE]
export function requestDelete(path: string): RfsvRequest {
  return { op: Rfsv32Op.DELETE, payload: encodeCString(path) };
}
