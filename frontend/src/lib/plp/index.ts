// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// PlpClient — top-level API for talking PsiWin / PLP to the emulated
// Revo. Constructed with a pair of byte I/O functions (read TX from
// guest, write RX to guest); owns the polling loop and the protocol
// stack.
//
// Lifecycle:
//
//   const client = new PlpClient(read, write);
//   client.start();
//   await client.connectRfsv();          // opens SYS$RFSV.* channel
//   const drives = await client.listDrives();
//   client.stop();                       // detach
//
// For now `connectRfsv()` and `listDrives()` are the only round-trips
// implemented — enough to validate the protocol stack against the
// emulated Revo before adding the heavier RFSV ops (FOPEN / FREAD /
// FWRITE / OPENDIR / READDIR_LFN / DELETE).

import { PlpTransport, type TransportConfig } from './transport.ts';
import { NcpMultiplexer, type OpenChannel } from './ncp.ts';
import { NCP_SERVICE_RFSV, EpocErr } from './types.ts';
import {
  encodeRfsvRequest,
  decodeRfsvReply,
  requestDriveList,
  parseDriveListReply,
  requestOpenDir,
  parseOpenDirReply,
  requestReadDirLfn,
  requestCloseDir,
  requestFopen,
  parseFopenReply,
  requestFread,
  requestFwrite,
  requestFclose,
  requestDelete,
  parseDirBatch,
  type RfsvReply,
  type DirEntry,
} from './rfsv32.ts';
export type { DirEntry } from './rfsv32.ts';
import {
  RFSV32_OMODE_OPEN_EXISTING,
  RFSV32_OMODE_CREATE,
  RFSV32_OMODE_REPLACE,
  RFSV32_OMODE_SHARE_READ,
  RFSV32_OMODE_SHARE_EXCL,
} from './types.ts';

export interface DirListingRaw {
  // Status returned by the final READDIR_LFN. EpocErr.Eof (-25) is the
  // normal "no more entries" terminator; anything else is a real error.
  finalStatus: number;
  // Raw response payloads from each successful READDIR_LFN call — the
  // bytes after the 2-byte status field. Surfaced so the UI's
  // diagnostic panel can show what the wire produced even when entry
  // parsing fails.
  chunks: Uint8Array[];
}

export interface DirListing {
  finalStatus: number;
  entries: DirEntry[];
  // True iff every batch parsed cleanly (no trailing unconsumed
  // bytes, no per-entry bail-outs). When false the UI should also
  // surface the raw chunks so the layout can be diagnosed.
  fullyParsed: boolean;
  parseError?: string;
  rawChunks: Uint8Array[];
}

export interface PlpClientConfig extends TransportConfig {
  // Timeout (ms) for a single RFSV request before the returned Promise
  // rejects. Defaults to 5 seconds — generous enough for the Revo's
  // typically <100 ms turnaround while leaving room for emulator hiccups.
  requestTimeoutMs?: number;
}

export class PlpClient {
  private transport: PlpTransport;
  private ncp: NcpMultiplexer;
  private rfsv: OpenChannel | null = null;
  private pendingReply: ((reply: RfsvReply) => void) | null = null;
  private readonly requestTimeoutMs: number;

  constructor(
    readBytes: () => Uint8Array,
    writeBytes: (data: Uint8Array) => number,
    config: PlpClientConfig = {},
  ) {
    this.requestTimeoutMs = config.requestTimeoutMs ?? 5000;
    this.transport = new PlpTransport(readBytes, writeBytes, config);
    this.ncp = new NcpMultiplexer(payload => this.transport.sendFrame(payload));
  }

  start(): void {
    this.transport.start(payload => this.ncp.handleFrame(payload));
  }

  stop(): void {
    this.transport.stop();
    this.rfsv = null;
    this.pendingReply = null;
  }

  get badCrc()        { return this.transport.badCrc; }
  get framingErrors() { return this.transport.framingErrors; }

  // Open the SYS$RFSV.* file-services channel on the Revo. Returns
  // when the remote sends NCON_ACK.
  async connectRfsv(): Promise<void> {
    if (this.rfsv && this.rfsv.state === 'open') return;
    this.rfsv = await this.ncp.openChannel(NCP_SERVICE_RFSV, data => this.onRfsvData(data));
  }

  async listDrives(): Promise<string[]> {
    const reply = await this.sendRfsv(requestDriveList());
    return parseDriveListReply(reply);
  }

  // Walk a directory: OPENDIR → READDIR_LFN-until-EOF → CLOSEDIR.
  // Returns the raw per-batch response payloads so the caller can hex-
  // dump them. Entry parsing is deliberately deferred until we have
  // captured real bytes to validate the layout against.
  //
  // Cap on number of READDIR_LFN calls so a misbehaving remote can't
  // wedge the UI in an infinite loop — at one batch per second this is
  // well over an hour of pulling, plenty for a real dir.
  async listDirRaw(path: string): Promise<DirListingRaw> {
    const openReply = await this.sendRfsv(requestOpenDir(path));
    if (openReply.status !== 0) {
      return { finalStatus: openReply.status, chunks: [] };
    }
    const handle = parseOpenDirReply(openReply);
    if (handle === null) {
      return { finalStatus: -1, chunks: [] };
    }

    const chunks: Uint8Array[] = [];
    let finalStatus = 0;
    try {
      for (let i = 0; i < 4096; i++) {
        const reply = await this.sendRfsv(requestReadDirLfn(handle));
        if (reply.status === 0) {
          chunks.push(reply.data);
          continue;
        }
        // EOF (KErrEof = -25) is the normal terminator; any other
        // negative status is a real error.
        finalStatus = reply.status;
        break;
      }
    } finally {
      // Always close the directory handle, even if iteration errored.
      try { await this.sendRfsv(requestCloseDir(handle)); } catch { /* ignore */ }
    }
    return { finalStatus, chunks };
  }

  // True when the final status of a listDirRaw is the normal EOF
  // terminator rather than an actual error.
  static isDirEof(status: number): boolean {
    return status === EpocErr.Eof;
  }

  // Walk a directory and parse each batch into entries. Returns
  // both the parsed entries and the raw chunks so the UI can fall
  // back to a hex view if the parser disagrees with what the device
  // sent. Errors during iteration (rejected sendRfsv) propagate.
  async listDir(path: string): Promise<DirListing> {
    const raw = await this.listDirRaw(path);
    const entries: DirEntry[] = [];
    let fullyParsed = true;
    let parseError: string | undefined;
    for (const chunk of raw.chunks) {
      const result = parseDirBatch(chunk);
      entries.push(...result.entries);
      if (result.remainingBytes !== 0 || result.parseError) {
        fullyParsed = false;
        parseError = result.parseError ?? `${result.remainingBytes} unconsumed bytes in batch`;
      }
    }
    return {
      finalStatus: raw.finalStatus,
      entries,
      fullyParsed,
      parseError,
      rawChunks: raw.chunks,
    };
  }

  // ── File I/O ─────────────────────────────────────────────────────
  // Conservative chunk size for the FREAD / FWRITE loops. A PLP frame
  // payload caps around ~1500 bytes once NCP + RFSV overhead is
  // accounted for, so a 1024-byte data chunk leaves plenty of headroom
  // for the wrappers. Easy to tune later if the round-trip cost
  // dominates throughput.
  private static readonly CHUNK_SIZE = 1024;

  // Download `path` off the device into an in-memory buffer. The
  // returned Promise resolves with the full file bytes; it rejects on
  // any RFSV error (path not found, access denied, etc.). Optional
  // onProgress fires after each chunk with (bytesSoFar) — wire it to
  // a progress bar.
  async downloadFile(path: string, onProgress?: (bytes: number) => void): Promise<Uint8Array> {
    const openMode = RFSV32_OMODE_OPEN_EXISTING | RFSV32_OMODE_SHARE_READ;
    const openReply = await this.sendRfsv(requestFopen(path, openMode));
    if (openReply.status !== 0) {
      throw new Error(`FOPEN(${path}) failed: status ${openReply.status}`);
    }
    const handle = parseFopenReply(openReply);
    if (handle === null) {
      throw new Error(`FOPEN(${path}): server returned no handle`);
    }

    const chunks: Uint8Array[] = [];
    let total = 0;
    try {
      // Cap the loop at 64 MiB worth of chunks so a misbehaving remote
      // can't OOM the browser tab.
      for (let i = 0; i < 64 * 1024; i++) {
        const reply = await this.sendRfsv(requestFread(handle, PlpClient.CHUNK_SIZE));
        if (reply.status !== 0 && reply.status !== EpocErr.Eof) {
          throw new Error(`FREAD failed: status ${reply.status}`);
        }
        if (reply.data.length === 0) break;
        chunks.push(reply.data);
        total += reply.data.length;
        onProgress?.(total);
        if (reply.status === EpocErr.Eof) break;
      }
    } finally {
      try { await this.sendRfsv(requestFclose(handle)); } catch { /* ignore */ }
    }

    const out = new Uint8Array(total);
    let offset = 0;
    for (const c of chunks) {
      out.set(c, offset);
      offset += c.length;
    }
    return out;
  }

  // Upload `data` to `path` on the device, overwriting any existing
  // file. Splits into CHUNK_SIZE chunks for FWRITE.
  async uploadFile(path: string, data: Uint8Array, onProgress?: (bytes: number) => void): Promise<void> {
    const openMode = RFSV32_OMODE_REPLACE | RFSV32_OMODE_CREATE | RFSV32_OMODE_SHARE_EXCL;
    const openReply = await this.sendRfsv(requestFopen(path, openMode));
    if (openReply.status !== 0) {
      throw new Error(`FOPEN(${path}, create) failed: status ${openReply.status}`);
    }
    const handle = parseFopenReply(openReply);
    if (handle === null) {
      throw new Error(`FOPEN(${path}, create): server returned no handle`);
    }

    try {
      for (let offset = 0; offset < data.length; offset += PlpClient.CHUNK_SIZE) {
        const chunk = data.subarray(offset, Math.min(offset + PlpClient.CHUNK_SIZE, data.length));
        const reply = await this.sendRfsv(requestFwrite(handle, chunk));
        if (reply.status !== 0) {
          throw new Error(`FWRITE at offset ${offset} failed: status ${reply.status}`);
        }
        onProgress?.(offset + chunk.length);
      }
    } finally {
      try { await this.sendRfsv(requestFclose(handle)); } catch { /* ignore */ }
    }
  }

  // Delete a file by path.
  async deleteFile(path: string): Promise<void> {
    const reply = await this.sendRfsv(requestDelete(path));
    if (reply.status !== 0) {
      throw new Error(`DELETE(${path}) failed: status ${reply.status}`);
    }
  }

  // ── internals ───────────────────────────────────────────────────
  private onRfsvData(data: Uint8Array): void {
    const reply = decodeRfsvReply(data);
    if (!reply) return;
    const resolver = this.pendingReply;
    if (resolver) {
      this.pendingReply = null;
      resolver(reply);
    }
  }

  // Sends a single RFSV request and resolves with its reply. RFSV is
  // strictly half-duplex per channel, so the simple "one outstanding
  // request" model is sufficient until we introduce streaming reads.
  private sendRfsv(req: { op: number; payload: Uint8Array }): Promise<RfsvReply> {
    if (!this.rfsv || this.rfsv.state !== 'open') {
      return Promise.reject(new Error('RFSV channel is not open — call connectRfsv() first'));
    }
    if (this.pendingReply) {
      return Promise.reject(new Error('another RFSV request is already in flight'));
    }
    const channel = this.rfsv;
    const body = encodeRfsvRequest(req);
    return new Promise<RfsvReply>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pendingReply = null;
        reject(new Error(`RFSV request 0x${req.op.toString(16)} timed out after ${this.requestTimeoutMs} ms`));
      }, this.requestTimeoutMs);
      this.pendingReply = reply => {
        clearTimeout(timer);
        resolve(reply);
      };
      this.ncp.send(channel.localChan, body);
    });
  }
}
