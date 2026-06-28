// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// RFSV16 — EPOC16 (SIBO) remote file services, the 16-bit counterpart
// of rfsv32-spec.ts. Sits on top of an open NCP channel (the same
// "SYS$RFSV.*" server name; the NCP Information version byte — 0x03 on
// the 3c v5.20f — selects the 16-bit dialect on the device side).
//
// Wire format (PLP spec "RFSV Server — SIBO Command Frames"):
//   command: [reason:u16le] [n:u16le]     [request data… (n bytes)]
//   reply:   [0x2a:u16le]   [n+2:u16le]   [status:i16le] [reply data…]
//
// Strings are NUL-terminated in the SIBO character set (ASCII for our
// purposes). The Status code is a SIBO status code (0 = E_SIBO_NONE,
// negative = error; -36 = end-of-file terminates FDIRREAD loops).
//
// Drive discovery: the SIBO dialect has no GET_DRIVE_LIST; we probe the
// fixed SIBO drive set (M: internal RAM drive, A:/B: SSD packs, ROM::)
// with RFSV16_STATUSDEVICE and report the ones that answer status 0.

import { Ncp } from './ncp-spec.ts';
import type { DirEntry } from './rfsv32-spec.ts';
import { LinkResetError } from './rfsv32-spec.ts';

export const RFSV16 = {
  FOPEN:        0x00,
  FCLOSE:       0x02,
  FREAD:        0x04,
  FDIRREAD:     0x06,
  FDEVICEREAD:  0x08,
  FWRITE:       0x0a,
  FSEEK:        0x0c,
  FFLUSH:       0x0e,
  FSETEOF:      0x10,
  RENAME:       0x12,
  DELETE:       0x14,
  FINFO:        0x16,
  SFSTAT:       0x18,
  PARSE:        0x1a,
  MKDIR:        0x1c,
  OPENUNIQUE:   0x1e,
  STATUSDEVICE: 0x20,
  PATHTEST:     0x22,
  STATUSSYSTEM: 0x24,
  CHANGEDIR:    0x26,
  SFDATE:       0x28,
  RESPONSE:     0x2a,
} as const;

// FOPEN mode word: low nibble = operation, 0x00n0 = stream type,
// upper bits = flags (PLP spec RFSV16_FOPEN).
export const FOPEN_MODE = {
  OPEN_EXISTING: 0x0000,
  CREATE:        0x0001,
  REPLACE:       0x0002,
  APPEND:        0x0003,
  UNIQUE:        0x0004,
  STREAM_BINARY: 0x0000,
  STREAM_TEXT:   0x0010,
  RECORD_BINARY: 0x0020,
  RECORD_DIR:    0x0030,
  FORMAT:        0x0040,
  DEVICE_LIST:   0x0050,
  NODE_LIST:     0x0060,
  READ_WRITE:    0x0100,
  RANDOM_ACCESS: 0x0200,
  SHARE:         0x0400,
} as const;

// SIBO status code that terminates an FDIRREAD loop.
export const E_SIBO_FILE_EOF = -36;

// FDIRREAD entry attribute bits (subset the dialog cares about).
const ATTR16_DIRECTORY = 0x0010;

const enc = new TextEncoder();
const dec = new TextDecoder('latin1');

const u16 = (v: number) => [v & 0xFF, (v >> 8) & 0xFF];

// Build a command frame: reason code, payload length, payload.
export function buildRfsv16(reason: number, payload: Uint8Array): Uint8Array {
  const out = new Uint8Array(4 + payload.length);
  out[0] = reason & 0xFF;
  out[1] = (reason >> 8) & 0xFF;
  out[2] = payload.length & 0xFF;
  out[3] = (payload.length >> 8) & 0xFF;
  out.set(payload, 4);
  return out;
}

function zstr(s: string): number[] {
  return [...enc.encode(s), 0];
}

export function buildFOpen(mode: number, name: string): Uint8Array {
  return buildRfsv16(RFSV16.FOPEN, new Uint8Array([...u16(mode), ...zstr(name)]));
}
export function buildFClose(handle: number): Uint8Array {
  return buildRfsv16(RFSV16.FCLOSE, new Uint8Array(u16(handle)));
}
export function buildFRead(handle: number, length: number): Uint8Array {
  return buildRfsv16(RFSV16.FREAD, new Uint8Array([...u16(handle), ...u16(length)]));
}
export function buildFDirRead(handle: number): Uint8Array {
  return buildRfsv16(RFSV16.FDIRREAD, new Uint8Array(u16(handle)));
}
export function buildFWrite(handle: number, data: Uint8Array): Uint8Array {
  return buildRfsv16(RFSV16.FWRITE, new Uint8Array([...u16(handle), ...data]));
}
export function buildDelete(name: string): Uint8Array {
  return buildRfsv16(RFSV16.DELETE, new Uint8Array(zstr(name)));
}
export function buildMkDir(name: string): Uint8Array {
  return buildRfsv16(RFSV16.MKDIR, new Uint8Array(zstr(name)));
}
export function buildStatusDevice(name: string): Uint8Array {
  return buildRfsv16(RFSV16.STATUSDEVICE, new Uint8Array(zstr(name)));
}

export interface Rfsv16Reply {
  status: number;       // signed 16-bit SIBO status
  data: Uint8Array;     // reply data after the status word
}

// Parse a reply frame. Returns null when the frame isn't an RFSV16
// response (wrong reason code or truncated).
export function decodeRfsv16Reply(frame: Uint8Array): Rfsv16Reply | null {
  if (frame.length < 6) return null;
  const reason = frame[0] | (frame[1] << 8);
  if (reason !== RFSV16.RESPONSE) return null;
  const status16 = frame[4] | (frame[5] << 8);
  const status = status16 >= 0x8000 ? status16 - 0x10000 : status16;
  return { status, data: frame.subarray(6) };
}

// Decode an FDIRREAD reply: a single u16 buffer length ("the length of
// the repeated fields") followed by repeated records of
//   [version:u16][attr:u16][size:u32][modified:u32][reserved:u32][name… NUL]
// Pinned against the live 3c v5.20f: an M:\ listing returns one reply
// with bufLen=100 carrying five 20-byte records (16 fixed + "WLD\0"
// etc.).
export function decodeDirEntries(data: Uint8Array): DirEntry[] {
  const out: DirEntry[] = [];
  if (data.length < 2) return out;
  const bufLen = data[0] | (data[1] << 8);
  const end = Math.min(2 + bufLen, data.length);
  let off = 2;
  while (off + 16 < end) {
    const rec = data.subarray(off);
    const attributes = rec[2] | (rec[3] << 8);
    const size = (rec[4] | (rec[5] << 8) | (rec[6] << 16) | (rec[7] << 24)) >>> 0;
    let nameEnd = 16;
    while (off + nameEnd < end && rec[nameEnd] !== 0) nameEnd++;
    const name = dec.decode(rec.subarray(16, nameEnd));
    if (name.length === 0) break;
    out.push({
      longName: name,
      shortName: '',
      size,
      attributes,
      modifiedLow: 0,
      modifiedHigh: 0,
      uid1: 0, uid2: 0, uid3: 0,
      isDirectory: (attributes & ATTR16_DIRECTORY) !== 0,
    });
    off += nameEnd + 1;  // past the NUL
  }
  return out;
}

// Human-readable text for the EPOC16 file-server status codes the
// Remote Link dialog surfaces. The starred codes are pinned live
// against the 3c v5.20f (-38 from a malformed/long name, -40 from
// FOPENing the file the Agenda app holds open, -44 from FWRITE on a
// read-only handle, -62 from STATUSDEVICE on an empty SSD slot, -36
// from FDIRREAD at end-of-directory); the rest follow the PLP spec's
// EPOC16 error table.
export function rfsv16ErrorText(status: number): string {
  switch (status) {
    case -33: return 'file not found on the device';
    case -34: return 'directory not found on the device';
    case -36: return 'end of file';                                    // *
    case -38: return 'name not valid on the device (EPOC16 names are 8.3, e.g. NOTES.TXT)';  // *
    case -39: return 'a file with that name already exists on the device';
    case -40: return 'the file is in use on the device — close the app that has it open';     // *
    case -43: return 'the device’s disk is full';
    case -44: return 'the file is read-only on the device';            // *
    case -62: return 'no disk in that drive';                          // *
    default:  return `device error ${status}`;
  }
}
function rfsvErr(op: string, status: number): Error {
  return new Error(`${op}: ${rfsv16ErrorText(status)} (${status})`);
}

// ── High-level client over an open NCP channel ──────────────────────
// Mirrors the slice of rfsv32-spec.ts's RfsvClient that PlpClient
// consumes, but speaks the 16-bit dialect. One request in flight at a
// time (the SIBO server is strictly request/reply).
export class Rfsv16Client {
  private ncp: Ncp;
  private chan: number;
  private pending: {
    resolve: (r: Rfsv16Reply) => void;
    reject: (e: Error) => void;
    timer: ReturnType<typeof setTimeout>;
    rearm: () => void;
  } | null = null;

  // Inactivity timeout window (ms): a request fails only after this long
  // with no inbound device bytes. Same rationale as RfsvClient — keeps a
  // transfer alive while the user keeps the device busy. Overridable per
  // send() call; configurable default via setRequestTimeout.
  private inactivityMs = 30_000;
  setRequestTimeout(ms: number): void { this.inactivityMs = Math.max(1_000, ms); }

  constructor(ncp: Ncp, clientChan: number) {
    this.ncp = ncp;
    this.chan = clientChan;
    this.ncp.setHandler(clientChan, data => this.onReply(data));
  }

  // Push back the in-flight request's inactivity timer on any sign of life
  // from the device (called by PlpClient when inbound bytes arrive).
  noteActivity(): void { this.pending?.rearm(); }

  // Abort the in-flight request after a data-link reset, so the caller
  // can re-open the channel without waiting out the timeout.
  reset(): void {
    const p = this.pending;
    this.pending = null;
    if (p) {
      clearTimeout(p.timer);
      p.reject(new LinkResetError());
    }
  }

  send(frame: Uint8Array, timeoutMs?: number, op = 'request'): Promise<Rfsv16Reply> {
    if (this.pending) return Promise.reject(new Error(`RFSV16 ${op}: another request is already in flight`));
    const windowMs = timeoutMs ?? this.inactivityMs;
    return new Promise<Rfsv16Reply>((resolve, reject) => {
      const onTimeout = () => {
        if (this.pending) {
          this.pending = null;
          reject(new Error(`RFSV16 ${op} timed out — the device did not answer`));
        }
      };
      const rearm = () => {
        if (this.pending) { clearTimeout(this.pending.timer); this.pending.timer = setTimeout(onTimeout, windowMs); }
      };
      const timer = setTimeout(onTimeout, windowMs);
      this.pending = { resolve, reject, timer, rearm };
      try {
        this.ncp.sendOn(this.chan, frame);
      } catch (e) {
        clearTimeout(timer);
        this.pending = null;
        reject(e instanceof Error ? e : new Error(String(e)));
      }
    });
  }

  private onReply(data: Uint8Array): void {
    const reply = decodeRfsv16Reply(data);
    if (!reply) return;
    const p = this.pending;
    if (!p) return;
    this.pending = null;
    clearTimeout(p.timer);
    p.resolve(reply);
  }

  // ── File operations the Remote Link dialog uses ────────────────────

  // SIBO has no drive-list op; probe the fixed drive set. M: is the
  // internal RAM drive; A:/B: are the SSD pack slots. A drive answers
  // STATUSDEVICE with status 0 when present. Entries carry the colon
  // ("M:") to match the RFSV32 convention — RemoteLinkDialog builds
  // the root path as `${drive}\`, so a bare letter would produce the
  // malformed "M\" (FOPEN error -38, E_GEN_BADNAME).
  async getDriveList(): Promise<string[]> {
    const drives: string[] = [];
    for (const d of ['M', 'A', 'B']) {
      try {
        const r = await this.send(buildStatusDevice(`${d}:`), 5_000, `STATUSDEVICE ${d}:`);
        if (r.status === 0) drives.push(`${d}:`);
      } catch (e) {
        if (e instanceof LinkResetError) throw e;
        // Treat per-drive failures (timeouts on absent packs) as absent.
      }
    }
    return drives;
  }

  // List a directory: FOPEN the path as a directory record stream and
  // FDIRREAD until E_SIBO_FILE_EOF. SIBO wants the directory path with
  // a trailing backslash (e.g. "M:\" or "M:\WRD\").
  async listDirectory(path: string): Promise<DirEntry[]> {
    let dirPath = path;
    if (!dirPath.endsWith('\\')) dirPath += '\\';
    const open = await this.send(
      buildFOpen(FOPEN_MODE.OPEN_EXISTING | FOPEN_MODE.RECORD_DIR | FOPEN_MODE.SHARE, dirPath),
      10_000, `FOPEN dir ${dirPath}`);
    if (open.status !== 0) throw rfsvErr(`Listing ${dirPath}`, open.status);
    if (open.data.length < 2) throw new Error('FOPEN(dir): no handle');
    const handle = open.data[0] | (open.data[1] << 8);
    const entries: DirEntry[] = [];
    try {
      for (let i = 0; i < 4096; i++) {
        const r = await this.send(buildFDirRead(handle), 10_000, 'FDIRREAD');
        if (r.status !== 0) break;            // E_SIBO_FILE_EOF ends the listing
        entries.push(...decodeDirEntries(r.data));
        if (r.data.length === 0) break;
      }
    } finally {
      try { await this.send(buildFClose(handle)); } catch { /* ignore */ }
    }
    return entries;
  }

  async downloadFile(path: string, onProgress?: (bytes: number) => void): Promise<Uint8Array> {
    const open = await this.send(
      buildFOpen(FOPEN_MODE.OPEN_EXISTING | FOPEN_MODE.STREAM_BINARY | FOPEN_MODE.SHARE, path),
      10_000, `FOPEN ${path}`);
    if (open.status !== 0) throw rfsvErr(`Opening ${path}`, open.status);
    if (open.data.length < 2) throw new Error('FOPEN: no handle');
    const handle = open.data[0] | (open.data[1] << 8);
    const chunks: Uint8Array[] = [];
    let total = 0;
    try {
      // ≤640 bytes per read, per the spec's "buggy implementations"
      // guidance; keeps each reply within a couple of NCP fragments.
      const CHUNK = 256;
      for (let i = 0; i < 64 * 1024; i++) {
        const r = await this.send(buildFRead(handle, CHUNK), 10_000, 'FREAD');
        if (r.status !== 0 && r.data.length === 0) break;  // EOF
        if (r.data.length === 0) break;
        chunks.push(new Uint8Array(r.data));
        total += r.data.length;
        onProgress?.(total);
        // Do NOT stop on a short read: a SIBO FREAD may return fewer than
        // CHUNK bytes without being at end-of-file (it fills only to its
        // own buffer boundary). EOF is the zero-length read handled above;
        // treating a short read as EOF truncated large downloads mid-file.
      }
    } finally {
      try { await this.send(buildFClose(handle)); } catch { /* ignore */ }
    }
    const out = new Uint8Array(total);
    let off = 0;
    for (const c of chunks) { out.set(c, off); off += c.length; }
    return out;
  }

  async uploadFile(path: string, data: Uint8Array, onProgress?: (bytes: number) => void): Promise<void> {
    // READ_WRITE (0x0100) is required for FWRITE to accept the handle —
    // a plain REPLACE open returns a read-only handle and every write
    // fails with -44 (verified live against the 3c v5.20f).
    const open = await this.send(
      buildFOpen(FOPEN_MODE.REPLACE | FOPEN_MODE.STREAM_BINARY | FOPEN_MODE.READ_WRITE, path),
      10_000, `FOPEN replace ${path}`);
    if (open.status !== 0) throw rfsvErr(`Creating ${path}`, open.status);
    if (open.data.length < 2) throw new Error('FOPEN: no handle');
    const handle = open.data[0] | (open.data[1] << 8);
    try {
      const CHUNK = 256;
      for (let off = 0; off < data.length; off += CHUNK) {
        const chunk = data.subarray(off, Math.min(off + CHUNK, data.length));
        const r = await this.send(buildFWrite(handle, chunk), 10_000, `FWRITE @${off}`);
        if (r.status !== 0) throw rfsvErr(`Writing ${path} at offset ${off}`, r.status);
        onProgress?.(off + chunk.length);
      }
    } finally {
      try { await this.send(buildFClose(handle)); } catch { /* ignore */ }
    }
  }

  async deleteFile(path: string): Promise<void> {
    const r = await this.send(buildDelete(path), 10_000, `DELETE ${path}`);
    if (r.status !== 0) throw rfsvErr(`Deleting ${path}`, r.status);
  }
}
