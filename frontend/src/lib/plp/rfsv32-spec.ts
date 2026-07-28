// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// RFSV-32 — wire-correct rewrite. Sits on top of an open NCP channel
// (typically the SYS$RFSV.* server, opened via Ncp.connectServer).
//
// Wire format (EPOC variant, per the PDF spec + harness captures):
//   request: [reasonCode:u16-LE] [opId:u16-LE] [requestData...]
//   reply:   [0x0011:u16-LE]      [opId:u16-LE] [statusCode:i32-LE] [replyData...]
//
// Status codes are 32-bit signed. 0 = success; negative codes follow
// the EPOC error table (E_EPOC_EOF == -25 for end-of-dir, etc.).
//
// Strings in request payloads are 2-byte LE length followed by raw
// bytes (NOT NUL-terminated). The MSB of the length field is a
// Unicode flag — for ASCII paths we leave it clear.

import { Ncp } from './ncp-spec.ts';
import { RFSV32, RFSV32_REPLY_MARKER, ATTR, EpocErr } from './types.ts';

// ── Reply struct ────────────────────────────────────────────────────
export interface RfsvReply {
  opId: number;
  status: number;       // signed i32; 0 == success
  data: Uint8Array;     // bytes after the 8-byte header
}

// ── Directory entry struct (from RFSV32_READ_DIR) ───────────────────
export interface DirEntry {
  longName: string;
  shortName: string;    // empty when no DOS alias
  size: number;
  attributes: number;
  modifiedLow: number;
  modifiedHigh: number;
  uid1: number;
  uid2: number;
  uid3: number;
  isDirectory: boolean;
}

// ── Helpers ─────────────────────────────────────────────────────────
function u16le(n: number): Uint8Array {
  return new Uint8Array([n & 0xFF, (n >> 8) & 0xFF]);
}
function u32le(n: number): Uint8Array {
  return new Uint8Array([n & 0xFF, (n >> 8) & 0xFF, (n >> 16) & 0xFF, (n >>> 24) & 0xFF]);
}
function i32leFromBytes(b: Uint8Array, off: number): number {
  // Signed 32-bit little-endian.
  const u = (b[off] | (b[off+1] << 8) | (b[off+2] << 16) | (b[off+3] << 24)) >>> 0;
  return u | 0;  // bitwise OR with 0 reinterprets as signed.
}
function u32leFromBytes(b: Uint8Array, off: number): number {
  return ((b[off] | (b[off+1] << 8) | (b[off+2] << 16) | (b[off+3] << 24)) >>> 0);
}
function u16leFromBytes(b: Uint8Array, off: number): number {
  return (b[off] | (b[off+1] << 8)) & 0xFFFF;
}

// Encode a 2-byte-length + raw-bytes string (RFSV-32 convention).
function lenStr(s: string): Uint8Array {
  const bytes = new TextEncoder().encode(s);
  if (bytes.length > 0x7FFF) throw new Error('RFSV string too long');
  const out = new Uint8Array(2 + bytes.length);
  out[0] = bytes.length & 0xFF;
  out[1] = (bytes.length >> 8) & 0xFF;  // top bit = Unicode flag, kept 0
  out.set(bytes, 2);
  return out;
}

// Concatenate byte arrays into one.
function concat(...parts: Uint8Array[]): Uint8Array {
  let total = 0;
  for (const p of parts) total += p.length;
  const out = new Uint8Array(total);
  let off = 0;
  for (const p of parts) { out.set(p, off); off += p.length; }
  return out;
}

// ── Request encoders ────────────────────────────────────────────────
// Build the wire bytes for an RFSV request — reason code + opId
// header, then opcode-specific args. The opId is the caller's
// choice; the RfsvClient below allocates one per call.
function encodeRequest(reason: number, opId: number, args: Uint8Array = new Uint8Array(0)): Uint8Array {
  const out = new Uint8Array(4 + args.length);
  out.set(u16le(reason), 0);
  out.set(u16le(opId), 2);
  out.set(args, 4);
  return out;
}

export function buildGetDriveList(opId: number): Uint8Array {
  return encodeRequest(RFSV32.GET_DRIVE_LIST, opId);
}
export function buildOpenDir(opId: number, attributes: number, wildcard: string): Uint8Array {
  return encodeRequest(RFSV32.OPEN_DIR, opId, concat(u32le(attributes), lenStr(wildcard)));
}
export function buildReadDir(opId: number, handle: number): Uint8Array {
  return encodeRequest(RFSV32.READ_DIR, opId, u32le(handle));
}
export function buildCloseHandle(opId: number, handle: number): Uint8Array {
  return encodeRequest(RFSV32.CLOSE_HANDLE, opId, u32le(handle));
}
export function buildOpenFile(opId: number, mode: number, path: string): Uint8Array {
  return encodeRequest(RFSV32.OPEN_FILE, opId, concat(u32le(mode), lenStr(path)));
}
// REPLACE_FILE creates a new file, overwriting any existing file of the
// same name. Mode flags follow OPEN_FILE's; you almost always want the
// 0x0200 (READ_WRITE) flag set or WRITE_FILE will refuse the handle.
// Verified against the real Revo via harness — opcode 0x2A, with
// mode 0x0200, opens C:\x.txt cleanly and returns a writable handle.
export function buildReplaceFile(opId: number, mode: number, path: string): Uint8Array {
  return encodeRequest(RFSV32.REPLACE_FILE, opId, concat(u32le(mode), lenStr(path)));
}
// CREATE_FILE creates a new file and fails if a file with that name
// already exists (status -11 KErrAlreadyExists). Same mode flags as
// OPEN_FILE / REPLACE_FILE.
export function buildCreateFile(opId: number, mode: number, path: string): Uint8Array {
  return encodeRequest(RFSV32.CREATE_FILE, opId, concat(u32le(mode), lenStr(path)));
}
export function buildReadFile(opId: number, handle: number, length: number): Uint8Array {
  return encodeRequest(RFSV32.READ_FILE, opId, concat(u32le(handle), u32le(length)));
}
export function buildWriteFile(opId: number, handle: number, data: Uint8Array): Uint8Array {
  return encodeRequest(RFSV32.WRITE_FILE, opId, concat(u32le(handle), data));
}
export function buildDelete(opId: number, path: string): Uint8Array {
  return encodeRequest(RFSV32.DELETE, opId, lenStr(path));
}
// MK_DIR_ALL is RFs::MkDirAll — it creates every missing component of
// the path, so one call makes C:\System\Apps\<App>\ whether or not
// \System\Apps exists yet. EPOC parses the argument as a filename in a
// directory, so the path MUST end in a backslash or the last component
// is taken for a file and not created. Returns -11 (KErrAlreadyExists)
// when the leaf is already there, which callers treat as success.
export function buildMkDirAll(opId: number, path: string): Uint8Array {
  return encodeRequest(RFSV32.MK_DIR_ALL, opId,
                       lenStr(path.endsWith('\\') ? path : path + '\\'));
}

// ── Reply decoder ───────────────────────────────────────────────────
export function decodeReply(bytes: Uint8Array): RfsvReply | null {
  if (bytes.length < 8) return null;
  const marker = u16leFromBytes(bytes, 0);
  if (marker !== RFSV32_REPLY_MARKER) return null;
  return {
    opId:   u16leFromBytes(bytes, 2),
    status: i32leFromBytes(bytes, 4),
    data:   bytes.subarray(8),
  };
}

// ── Domain-specific reply parsers ───────────────────────────────────
// Parse the 26-byte drive bitmap from GET_DRIVE_LIST. Each byte 0..25
// corresponds to drives A:..Z:; non-zero values indicate presence.
// (The exact byte value distinguishes media types: 0x11 / 0x12 etc.,
// but we only surface presence/name here.)
export function parseDriveList(reply: RfsvReply): string[] {
  if (reply.status !== 0 || reply.data.length < 26) return [];
  const out: string[] = [];
  for (let i = 0; i < 26; i++) {
    if (reply.data[i] !== 0) out.push(String.fromCharCode('A'.charCodeAt(0) + i) + ':');
  }
  return out;
}

// Parse a READ_DIR reply payload into an array of DirEntry.
// Per-entry layout (PDF spec + harness-verified):
//   u32 short_name_length
//   u32 attributes
//   u32 size
//   u32 modified_low
//   u32 modified_high
//   u32 uid1
//   u32 uid2
//   u32 uid3
//   u32 long_name_length
//   long_name_length bytes  long_name (ASCII)
//   0-3 bytes               alignment to 4-byte boundary (only if
//                           short_name_length > 0)
//   short_name_length bytes short_name (ASCII)
//   0-3 bytes               alignment to next entry (4-byte boundary)
// Returns the entries plus a `consumed` count so the caller can tell
// when one batch is parseable. Doesn't throw on malformed input —
// stops at the first byte it can't read.
export function parseDirEntries(reply: RfsvReply): { entries: DirEntry[]; truncated: boolean } {
  const entries: DirEntry[] = [];
  if (reply.status !== 0 && reply.status !== EpocErr.Eof) return { entries, truncated: false };
  const buf = reply.data;
  let off = 0;
  while (off < buf.length) {
    if (off + 36 > buf.length) return { entries, truncated: true };
    const shortLen = u32leFromBytes(buf, off);
    const attributes = u32leFromBytes(buf, off + 4);
    const size = u32leFromBytes(buf, off + 8);
    const modLo = u32leFromBytes(buf, off + 12);
    const modHi = u32leFromBytes(buf, off + 16);
    const uid1 = u32leFromBytes(buf, off + 20);
    const uid2 = u32leFromBytes(buf, off + 24);
    const uid3 = u32leFromBytes(buf, off + 28);
    const longLen = u32leFromBytes(buf, off + 32);
    if (off + 36 + longLen > buf.length) return { entries, truncated: true };
    const longName = new TextDecoder().decode(buf.subarray(off + 36, off + 36 + longLen));
    let cursor = off + 36 + longLen;
    let shortName = '';
    if (shortLen > 0) {
      // Pad to next 4-byte boundary (relative to start of entry).
      const padded = (cursor - off + 3) & ~3;
      cursor = off + padded;
      if (cursor + shortLen > buf.length) return { entries, truncated: true };
      shortName = new TextDecoder().decode(buf.subarray(cursor, cursor + shortLen));
      cursor += shortLen;
    }
    // Pad to next entry's 4-byte boundary.
    const total = (cursor - off + 3) & ~3;
    off += total;
    entries.push({
      longName, shortName, size, attributes,
      modifiedLow: modLo, modifiedHigh: modHi,
      uid1, uid2, uid3,
      isDirectory: (attributes & ATTR.DIRECTORY) !== 0,
    });
  }
  return { entries, truncated: false };
}

// ── High-level client wrapping an open NCP channel ─────────────────
// Driven by the dialog / PlpClient: opens the SYS$RFSV.* channel and
// exposes typed helpers for the file operations.

// Thrown by an in-flight request when the underlying data-link resets
// (the device re-handshakes and forgets our channel). Distinct from a
// plain timeout so PlpClient.withRfsv knows to re-open the channel on
// the fresh link and retry, rather than waiting out the 10 s timeout.
export class LinkResetError extends Error {
  constructor() {
    super('RFSV request aborted: data-link reset');
    this.name = 'LinkResetError';
  }
}

export class RfsvClient {
  private ncp: Ncp;
  private clientChan: number;
  private opIdCounter = 1;
  // Reassembly: a single in-flight request per client chan. RFSV is
  // strictly command/reply, so we never need a queue. pendingReject
  // tears the in-flight request down on timeout or a link reset.
  private pendingResolve?: (r: RfsvReply) => void;
  private pendingReject?: (e: Error) => void;
  // Re-arms the in-flight request's inactivity timer. noteActivity() calls
  // this whenever the device sends anything, so a slow-but-alive peer never
  // trips the timeout (see noteActivity / the inactivity rationale below).
  private pendingRearm?: () => void;
  private pendingOpId = 0;
  private aborted = false;
  private rxBuf: Uint8Array = new Uint8Array(0);

  // The request timeout is an INACTIVITY window, not a fixed deadline: it
  // fires only after this many ms with NO inbound bytes from the device at
  // all. A fixed per-request deadline (the old hard 10 s) breaks large
  // transfers the moment the user interacts with the device — tapping icons
  // keeps WServ + the digitiser busy, starving the RemoteLinkServer serial
  // thread so a single READ/WRITE round-trip can take well over 10 s even
  // though the link is perfectly healthy (verified on the live 5mx ROM: a
  // 2 MB upload under continuous taps timed a WRITE out at exactly 10 s with
  // no link bounce). Resetting on any device activity lets such a transfer
  // crawl to completion instead of failing, while a genuinely dead link
  // still trips the window. Configurable via PlpClient's requestTimeoutMs.
  private inactivityMs = 30_000;
  // Absolute per-request ceiling, regardless of activity. The inactivity
  // window alone can livelock: when the device's RemoteLinkServer thread
  // wedges under sustained interaction it still dribbles link-level Acks,
  // which keep re-arming the inactivity timer forever while the actual
  // reply never comes (observed on the 5mx near the end of a large upload
  // under continuous taps — the transfer hung indefinitely). The ceiling
  // guarantees the request eventually fails so the whole-transfer restart
  // can re-drive it; once interaction pauses and the device un-wedges, the
  // restart completes.
  private maxRequestMs = 120_000;
  setRequestTimeout(ms: number): void {
    this.inactivityMs = Math.max(1_000, ms);
    this.maxRequestMs = Math.max(this.inactivityMs * 3, 90_000);
  }

  constructor(ncp: Ncp, clientChan: number) {
    this.ncp = ncp;
    this.clientChan = clientChan;
    this.ncp.setHandler(clientChan, data => this.onData(data));
  }

  // Called by PlpClient whenever inbound bytes arrive from the device, so
  // the in-flight request's inactivity timer is pushed back: any sign of
  // life (a reply fragment, a link-level Ack of our keepalive, anything)
  // proves the peer is still talking to us and the request isn't lost.
  noteActivity(): void { this.pendingRearm?.(); }

  // Send a pre-encoded request and resolve with the reply once it
  // arrives (matched by opId). Caller picks an op id via nextOpId().
  private sendOnce(request: Uint8Array): Promise<RfsvReply> {
    const opId = u16leFromBytes(request, 2);
    return new Promise<RfsvReply>((resolve, reject) => {
      if (this.aborted) { reject(new LinkResetError()); return; }
      if (this.pendingResolve) {
        reject(new Error('RFSV: another request is already in flight'));
        return;
      }
      let settled = false;
      let timer: ReturnType<typeof setTimeout>;
      const ceiling = setTimeout(() => {
        if (!settled) {
          clear();
          this.pendingOpId = -1;
          reject(new Error(`RFSV request opId=${opId} timed out after ${Math.round(this.maxRequestMs / 1000)}s`));
        }
      }, this.maxRequestMs);
      const clear = () => {
        settled = true;
        clearTimeout(timer);
        clearTimeout(ceiling);
        this.pendingResolve = undefined;
        this.pendingReject = undefined;
        this.pendingRearm = undefined;
      };
      const onTimeout = () => {
        if (!settled) {
          clear();
          this.pendingOpId = -1;
          reject(new Error(`RFSV request opId=${opId} timed out after ${Math.round(this.inactivityMs / 1000)}s of silence`));
        }
      };
      const rearm = () => { if (!settled) { clearTimeout(timer); timer = setTimeout(onTimeout, this.inactivityMs); } };
      this.pendingResolve = (reply: RfsvReply) => {
        if (!settled) { clear(); resolve(reply); }
      };
      this.pendingReject = (err: Error) => {
        if (!settled) { clear(); this.pendingOpId = -1; reject(err); }
      };
      this.pendingRearm = rearm;
      this.pendingOpId = opId;
      rearm();
      this.ncp.sendOn(this.clientChan, request);
    });
  }

  // Send a request and await its reply. `idempotent` (default true)
  // controls whether a timed-out request may be blindly resent on the
  // SAME handle: safe for stateless ops (DRIVE_LIST, OPEN, REPLACE,
  // CLOSE, DELETE, READ_DIR) but NOT for the file data ops READ_FILE /
  // WRITE_FILE, whose result depends on the device's current file
  // pointer. A resend of one of those after a reply was merely lost
  // would re-read past already-delivered bytes (truncating a download)
  // or re-append a chunk (corrupting an upload), so those callers pass
  // idempotent:false and restart the whole transfer from a known offset
  // instead (see doDownload / doUpload).
  async send(request: Uint8Array, idempotent = true): Promise<RfsvReply> {
    // Retry: each resend triggers a fresh IRQ→DFC→scheduler cycle on
    // the device, giving the kernel another chance to wake the RFSV
    // thread.  Without retries the first request can sit in the NCP
    // buffer forever if the scheduler doesn't pick the RFSV thread
    // on the initial delivery.
    const attempts = idempotent ? 4 : 1;
    for (let attempt = 0; attempt < attempts; attempt++) {
      try {
        return await this.sendOnce(request);
      } catch (e) {
        // A link reset means our channel is gone — resending here would
        // just hit a closed channel. Propagate so PlpClient.withRfsv can
        // re-open SYS$RFSV on the fresh link and retry the whole op.
        if (e instanceof LinkResetError) throw e;
        if (attempt === attempts - 1) throw e;
        await new Promise(r => setTimeout(r, 1000));
      }
    }
    throw new Error('unreachable');
  }

  // Abort any in-flight request because the underlying data-link reset.
  // The device forgets every NCP channel when it re-handshakes, so the
  // PlpClient discards this RfsvClient and opens a fresh one; reset()
  // makes the outstanding promise reject immediately (with LinkResetError)
  // instead of stalling the recovery for up to 10 s.
  reset(): void {
    this.aborted = true;
    this.pendingReject?.(new LinkResetError());
  }

  nextOpId(): number {
    const id = this.opIdCounter++;
    if (this.opIdCounter > 0xFFFE) this.opIdCounter = 1;
    return id;
  }

  // High-level helpers.
  async getDriveList(): Promise<string[]> {
    const reply = await this.send(buildGetDriveList(this.nextOpId()));
    return parseDriveList(reply);
  }

  async openDir(wildcard: string, attributes = ATTR.DIRECTORY | ATTR.ARCHIVE | ATTR.READ_ONLY | ATTR.HIDDEN | ATTR.SYSTEM): Promise<number> {
    const reply = await this.send(buildOpenDir(this.nextOpId(), attributes, wildcard));
    if (reply.status !== 0) throw new Error(`OPEN_DIR(${wildcard}) failed: ${reply.status}`);
    if (reply.data.length < 4) throw new Error('OPEN_DIR: no handle in reply');
    return u32leFromBytes(reply.data, 0);
  }

  async readDir(handle: number): Promise<{ entries: DirEntry[]; done: boolean }> {
    const reply = await this.send(buildReadDir(this.nextOpId(), handle));
    if (reply.status !== 0 && reply.status !== EpocErr.Eof) {
      throw new Error(`READ_DIR failed: ${reply.status}`);
    }
    const { entries } = parseDirEntries(reply);
    return { entries, done: reply.status === EpocErr.Eof };
  }

  async closeHandle(handle: number): Promise<void> {
    await this.send(buildCloseHandle(this.nextOpId(), handle));
  }

  async listDirectory(path: string): Promise<DirEntry[]> {
    // Append `*` if the path ends in `\` (or doesn't already include
    // a wildcard pattern).
    const wild = path.endsWith('\\') ? path + '*' : path;
    const handle = await this.openDir(wild);
    const all: DirEntry[] = [];
    try {
      for (let i = 0; i < 4096; i++) {
        const { entries, done } = await this.readDir(handle);
        all.push(...entries);
        if (done) break;
      }
    } finally {
      try { await this.closeHandle(handle); } catch { /* ignore */ }
    }
    return all;
  }

  // ── internals ──────────────────────────────────────────────────
  private onData(data: Uint8Array): void {
    // Accumulate incoming bytes — the NCP layer hands us each Complete
    // frame's payload separately, which already contains the full
    // RFSV reply for our use cases. Just decode and dispatch.
    this.rxBuf = data;  // overwrite; one reply per request
    const reply = decodeReply(this.rxBuf);
    if (!reply) return;
    if (reply.opId !== this.pendingOpId) {
      // Out-of-band reply (e.g. an unsolicited error notification).
      // Drop for now.
      return;
    }
    const r = this.pendingResolve;
    this.pendingResolve = undefined;
    this.pendingReject = undefined;
    if (r) r(reply);
  }
}
