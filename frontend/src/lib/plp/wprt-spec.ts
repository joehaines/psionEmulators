// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// WPRT (print spooler) — the presentation-layer service behind the
// device's "Printer via PC" printer driver. Sits on top of an open NCP
// channel like RfsvClient, but the roles are inverted relative to a
// real printer: the DEVICE spools the rendered job and the host (us,
// playing PsiWin) pulls it down with WPRT_DATA commands.
//
// Wire format (PLP spec, "WPRT Server"):
//   WPRT_LEVEL  cmd [0x00 major minor]  → reply [status major minor]
//   WPRT_DATA   cmd [0xF0]              → reply with printer data:
//     first packet of a page:  [lastPacket][lastPage][pageLen u32-LE][data…]
//     other packets:           [lastPacket][data…]
//     lastPacket/lastPage: 0x2A = more follow, 0xFF = end of print job
//   WPRT_CANCEL cmd [0xF1]              → reply = any pending data
//   WPRT_STOP   cmd [0xFF]              → no reply
//
// The service isn't running on the device until a LINK Register
// command loads it — the spec's canonical sequence is Connect →
// failure → Register("SYS$WPRT") → Connect("SYS$WPRT.*") → success.
// PlpClient owns that dance; this file owns the channel protocol.

import { Ncp } from './ncp-spec.ts';
import { WPRT, WPRT_LAST } from './types.ts';
import { LinkResetError } from './rfsv32-spec.ts';

// ── Job assembler ───────────────────────────────────────────────────
// Pure state machine fed with successive WPRT_DATA reply payloads.
// Tracks the page framing (the long header only appears on the first
// packet of each page; continuations carry just the lastPacket byte)
// and yields completed jobs as an array of per-page primitive streams.
//
// The first "page" of every job is the 15-byte fake page the spec
// uses as a job-start sentinel; it's kept in `pages` so the renderer
// can verify it, and skipped during rendering.

export interface WprtJob {
  pages: Uint8Array[];     // raw per-page data, fake first page included
}

export class WprtJobAssembler {
  private pages: Uint8Array[] = [];
  private current: Uint8Array[] = [];
  private pageRemaining = 0;     // bytes of the current page still to come

  // Feed one WPRT_DATA reply. Returns a completed job when this packet
  // carried the end-of-job marker, else null.
  feed(reply: Uint8Array): WprtJob | null {
    if (reply.length < 1) return null;
    const lastPacket = reply[0];
    let data: Uint8Array;

    if (this.pageRemaining > 0) {
      // Continuation packet: [lastPacket][data…]
      data = reply.subarray(1);
    } else {
      // First packet of a page: [lastPacket][lastPage][pageLen u32][data…]
      if (reply.length < 6) return null;
      this.pageRemaining =
        (reply[2] | (reply[3] << 8) | (reply[4] << 16) | (reply[5] << 24)) >>> 0;
      data = reply.subarray(6);
    }

    this.current.push(data);
    this.pageRemaining = Math.max(0, this.pageRemaining - data.length);
    if (this.pageRemaining === 0) this.flushPage();

    if (lastPacket === WPRT_LAST) {
      // End of job. A short final page (device under-delivering against
      // its declared length) is still flushed rather than dropped.
      if (this.current.length > 0) this.flushPage();
      const job: WprtJob = { pages: this.pages };
      this.reset();
      return job;
    }
    return null;
  }

  reset(): void {
    this.pages = [];
    this.current = [];
    this.pageRemaining = 0;
  }

  // Completed pages of the in-progress job (for progress reporting).
  get pagesSoFar(): number { return this.pages.length; }

  private flushPage(): void {
    let total = 0;
    for (const c of this.current) total += c.length;
    const page = new Uint8Array(total);
    let off = 0;
    for (const c of this.current) { page.set(c, off); off += c.length; }
    this.current = [];
    this.pageRemaining = 0;
    this.pages.push(page);
  }
}

// ── Channel client ──────────────────────────────────────────────────

export class WprtClient {
  private ncp: Ncp;
  private clientChan: number;
  // Strictly command/reply with a single request in flight — WPRT has
  // no operation IDs, so replies match commands by order alone.
  private pendingResolve?: (data: Uint8Array) => void;
  private pendingReject?: (e: Error) => void;
  private aborted = false;

  constructor(ncp: Ncp, clientChan: number) {
    this.ncp = ncp;
    this.clientChan = clientChan;
    this.ncp.setHandler(clientChan, data => this.onData(data));
  }

  // Tear down after a link reset: rejects the in-flight command (which
  // for WPRT is usually the indefinitely-waiting WPRT_DATA poll).
  reset(): void {
    this.aborted = true;
    const reject = this.pendingReject;
    this.pendingResolve = undefined;
    this.pendingReject = undefined;
    reject?.(new LinkResetError());
  }

  // Exchange WPRT version numbers. Returns the device's status code
  // (0 = the version we offered is acceptable).
  async exchangeLevel(major = 2, minor = 0, timeoutMs = 10_000): Promise<number> {
    const reply = await this.sendOnce(new Uint8Array([WPRT.LEVEL, major, minor]), timeoutMs);
    if (reply.length < 1) throw new Error('WPRT_LEVEL: empty reply');
    return reply[0];
  }

  // Poll for printer data. The reply is delayed until the device has
  // something to print, so the default timeout is "forever" — the
  // caller cancels by tearing the client down (reset()).
  readData(timeoutMs = 0): Promise<Uint8Array> {
    return this.sendOnce(new Uint8Array([WPRT.DATA]), timeoutMs);
  }

  // Cancel the current job; the reply flushes any pending data.
  cancel(timeoutMs = 10_000): Promise<Uint8Array> {
    return this.sendOnce(new Uint8Array([WPRT.CANCEL]), timeoutMs);
  }

  // Terminate the WPRT server on the device. Fire-and-forget (no reply).
  stop(): void {
    try { this.ncp.sendOn(this.clientChan, new Uint8Array([WPRT.STOP])); }
    catch { /* channel may already be gone */ }
  }

  private sendOnce(cmd: Uint8Array, timeoutMs: number): Promise<Uint8Array> {
    return new Promise<Uint8Array>((resolve, reject) => {
      if (this.aborted) { reject(new LinkResetError()); return; }
      if (this.pendingResolve) {
        reject(new Error('WPRT: another command is already in flight'));
        return;
      }
      let timer: ReturnType<typeof setTimeout> | null = null;
      const clear = () => {
        if (timer !== null) clearTimeout(timer);
        this.pendingResolve = undefined;
        this.pendingReject = undefined;
      };
      if (timeoutMs > 0) {
        timer = setTimeout(() => {
          clear();
          reject(new Error(`WPRT command 0x${cmd[0].toString(16)} timed out`));
        }, timeoutMs);
      }
      this.pendingResolve = data => { clear(); resolve(data); };
      this.pendingReject  = err  => { clear(); reject(err); };
      this.ncp.sendOn(this.clientChan, cmd);
    });
  }

  private onData(data: Uint8Array): void {
    const resolve = this.pendingResolve;
    this.pendingResolve = undefined;
    this.pendingReject = undefined;
    // Unsolicited data with no command outstanding is dropped — the
    // spec's WPRT is strictly command/reply.
    resolve?.(data);
  }
}

// Job-start sentinel. The spec presents this as "a fake page consisting
// of the following fifteen bytes" — but on the wire (verified against
// the real 5mx v1.05 ROM) those fifteen bytes INCLUDE the WPRT_DATA
// reply header (lastPacket 0x2A, lastPage 0x2A, pageLen = 9), so the
// fake page the assembler yields is the nine-byte payload tail.
export const WPRT_JOB_SENTINEL = new Uint8Array([
  0x82, 0x2e, 0x00, 0x00, 0xc6, 0x41, 0x00, 0x00, 0x00,
]);

// The full fifteen-byte wire form (header + payload), as a device
// emits it in a single WPRT_DATA reply. Exported for tests/mocks.
export const WPRT_JOB_SENTINEL_WIRE = new Uint8Array([
  0x2a, 0x2a, 0x09, 0x00, 0x00, 0x00, 0x82, 0x2e,
  0x00, 0x00, 0xc6, 0x41, 0x00, 0x00, 0x00,
]);

function bytesEqual(a: Uint8Array, b: Uint8Array): boolean {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}

export function isJobSentinelPage(page: Uint8Array): boolean {
  // Match the assembled payload form; accept the full wire form too in
  // case a device ever frames the fake page as ordinary page data.
  return bytesEqual(page, WPRT_JOB_SENTINEL) || bytesEqual(page, WPRT_JOB_SENTINEL_WIRE);
}
