// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// IrSiboSendClient — host-side SENDER for the SIBO "Psion IRLink" beam
// (Series 3c / 3mx / Siena "Psion IR"). The user arms the receiving
// screen on the device (Psion+Tab then ↓ on the 3c; the dedicated IR
// Receive key on the Siena) and the host plays the sending Psion.
//
// Protocol (pinned from the 3c v5.20f and Siena v4.20f native sender
// captures and verified on-device in both receivers — the beamed file
// appears on the System screen; see docs/sibo-remote-link.md):
//
//   host  XID discovery (6 slots + final w/ hints+nickname)
//   dev   XID response (carries its station address)
//   host  SNRM (dst = device addr, conn addr 0x1c) ── dev UA
//   host  LM-Connect LSAP 1→1: 01 00 66 "Psion IRLink 1.00" + pad
//   dev   LM-Connect-Confirm echoing the block (opcode byte 0x81)
//   host  length-prefixed stream on LSAP 1 (u16le len + payload):
//           block 0: header [bodyLen:u32le]["LOC::"+dest path NUL][pad]
//                    (136 bytes on the 3c, 132 on the Siena)
//           block 1: the file body (single block when it fits u16)
//         Every block is framed independently — its length prefix
//         STARTS a fresh I-frame and its last frame runs short, the
//         way both native senders lay the stream out. No end marker:
//         the receiver completes on byte count.
//   host  DISC ── dev UA  (receiver saves the file and closes its UI)
//
// IrLAP runs at window 1 (stop-and-wait): every I-frame we send carries
// P=1 and we wait for the secondary's RR (its N(r) acking us) before the
// next. Transmissions honor a minimum turnaround after the device's
// last frame (IR is half-duplex; the kernels flush their RX around
// their own TX — see SiboSendOptions.turnaroundMs).

import { encodeSirFrame, SirDecoder } from './framing.ts';
import { U_SNRM, U_UA, U_DISC, U_XID, PF_BIT } from './types.ts';

export interface SiboSendCallbacks {
  onProgress?: (sent: number) => void;
  onPhase?: (phase: string) => void;
}

export class IrSiboBeamError extends Error {
  constructor(msg: string) {
    super(msg);
    this.name = 'IrSiboBeamError';
  }
}

const CONN_ADDR = 0x1c;                  // matches the 3c's own sender
const GREETING = 'Psion IRLink 1.00';
// Body bytes per stream block. The native senders put the WHOLE body
// in one u16le-prefixed block; cap ours below 64 KiB so the prefix
// can always express the block length.
const CHUNK = 0x8000;

export interface SiboSendOptions {
  // Size of the fixed header block the receiver expects. Pinned from
  // each device's OWN sender: 136 on the Series 3c (v5.20f), 132 on
  // the Siena (v4.20f). A mismatched length leaves the receiver
  // waiting for more header bytes and the transfer never engages.
  headerLen?: number;
  // Whether to append a zero-length end-of-stream block. NEITHER
  // device's own sender emits one (the receivers end on byte count +
  // DISC) and the 3c receiver actively rejects it ("Receiving
  // interrupted" at save time), so it defaults OFF; kept as an option
  // for protocol experiments only.
  eofChunk?: boolean;
  // Minimum turnaround: how long to hold off transmitting after the
  // device's last frame. IrDA is half-duplex and IrLAP requires the
  // station to honor the peer's minimum-turnaround-time parameter —
  // the SIBO kernels service their IR UART around their own TX and
  // an instant reply lands while the receiver is still in its
  // transmit window, where it is flushed unseen (the v4.20f Siena
  // aborts "Receiving interrupted" mid-transfer). 100 ms is safely
  // above every SIBO ROM's window and adds <1 s to a beam.
  turnaroundMs?: number;
}

function ascii(s: string): number[] {
  return Array.from(s).map(c => c.charCodeAt(0) & 0xff);
}

export class IrSiboSendClient {
  private readBytes: () => Uint8Array;
  private writeBytes: (data: Uint8Array) => number;
  private decoder = new SirDecoder();
  private rxQueue: Uint8Array[] = [];
  private stationAddr: number[];
  private canceled = false;
  private headerLen: number;
  private eofChunk: boolean;
  private turnaroundMs: number;
  // IrLAP receive sequence V(r): advanced for each in-order I-frame the
  // device sends (see sendFile), reflected as N(r) on our frames.
  private vr = 0;
  // When the device's last frame arrived — transmissions hold off for
  // turnaroundMs after this (see SiboSendOptions.turnaroundMs).
  private lastRxAt = 0;

  constructor(readBytes: () => Uint8Array, writeBytes: (data: Uint8Array) => number,
              opts: SiboSendOptions = {}) {
    this.readBytes = readBytes;
    this.writeBytes = writeBytes;
    this.headerLen = opts.headerLen ?? 136;
    this.eofChunk = opts.eofChunk ?? false;
    this.turnaroundMs = opts.turnaroundMs ?? 100;
    this.stationAddr = Array.from({ length: 4 }, () => Math.floor(Math.random() * 255) + 1);
  }

  cancel(): void { this.canceled = true; }
  // Uniform-teardown alias: the Infrared dialog stops whichever client
  // type is active on unmount; for this sender stopping IS cancelling
  // (there is no polling interval to clear — sendFile drives its own
  // poll loop and aborts on the canceled flag).
  stop(): void { this.cancel(); }

  // Beam `data` to the armed device receiver, asking it to save the
  // file at `destPath` (a full SIBO path, e.g. "M:\\WRD\\NOTES.WRD" —
  // the receiver creates it verbatim from the header).
  async sendFile(destPath: string, data: Uint8Array, cb: SiboSendCallbacks = {},
                 timeoutMs = 30_000): Promise<void> {
    const deadline = Date.now() + timeoutMs;
    this.canceled = false;
    this.rxQueue = [];
    this.decoder.reset?.();

    // ── Discovery ────────────────────────────────────────────────────
    cb.onPhase?.('discovery');
    let devAddr: number[] | null = null;
    for (let round = 0; round < 8 && !devAddr; round++) {
      this.checkAbort(deadline);
      for (let slot = 0; slot < 6; slot++) {
        this.tx([0xff, U_XID | PF_BIT, 0x01, ...this.stationAddr,
                 0xff, 0xff, 0xff, 0xff, 0x01, slot, 0x00]);
        const fs = await this.collect(250);
        devAddr = devAddr ?? this.sniffXidResponse(fs);
        if (devAddr) break;
      }
      this.tx([0xff, U_XID | PF_BIT, 0x01, ...this.stationAddr,
               0xff, 0xff, 0xff, 0xff, 0x01, 0xff, 0x00,
               0x02, 0x00, ...ascii('PsionWeb'), 0x00]);
      const fs = await this.collect(600);
      devAddr = devAddr ?? this.sniffXidResponse(fs);
    }
    if (!devAddr) throw new IrSiboBeamError(
      'No Psion found — open the Infrared screen on the device (Psion+Tab) and choose Receive');

    // ── Connect ──────────────────────────────────────────────────────
    cb.onPhase?.('connect');
    this.tx([0xff, U_SNRM | PF_BIT, ...this.stationAddr, ...devAddr, (CONN_ADDR << 1),
             0x01, 0x01, 0x3f, 0x82, 0x01, 0x01, 0x83, 0x01, 0x0f,
             0x84, 0x01, 0x01, 0x85, 0x01, 0x80, 0x86, 0x01, 0x80, 0x08, 0x01, 0x03]);
    if (!await this.waitFor(deadline, f => (f[1] & ~PF_BIT) === U_UA)) {
      throw new IrSiboBeamError('device did not accept the IR connection (no UA)');
    }

    let vs = 0;  // our send sequence; receiver acks with N(r) = vs+1
    this.vr = 0; // receive sequence: every I-frame the device sends us
                 // (its LM-Connect-Confirm, possibly retransmitted) must
                 // be acknowledged via the N(r) we stamp on OUR frames —
                 // the Siena retransmits its confirm forever and flags
                 // the whole transfer "interrupted" if N(r) never moves.
    const sendIAwaitAck = async (payload: number[]): Promise<void> => {
      const next = (vs + 1) & 7;
      for (let attempt = 0; attempt < 5; attempt++) {
        this.checkAbort(deadline);
        await this.turnaround();
        const ctrl = ((this.vr & 7) << 5) | ((vs & 7) << 1) | PF_BIT;
        this.tx([(CONN_ADDR << 1) | 1, ctrl, ...payload]);
        const acked = await this.waitFor(deadline, f => {
          const c = f[1];
          if ((c & 0x03) === 0x01 || (c & 0x01) === 0) {   // S- or I-frame
            return ((c >> 5) & 7) === next;
          }
          return false;
        }, 1_500);
        if (acked) { vs = next; return; }
      }
      throw new IrSiboBeamError('device stopped acknowledging the IR transfer');
    };

    // LM-Connect with the IRLink greeting (echo-confirmed by the device).
    const lm = [0x81, 0x01, 0x01, 0x00, 0x66, ...ascii(GREETING), 0x00];
    while (lm.length < 64) lm.push(0x00);
    await sendIAwaitAck(lm);

    // ── Stream: header + body chunks ─────────────────────────────────
    cb.onPhase?.('transfer');
    // Header block, mirroring the devices' own senders byte-for-byte
    // (native 3c and Siena captures, docs/sibo-remote-link.md):
    //   [bodyLen:u32le] ["LOC::" + dest path, NUL] [zero pad to headerLen]
    // The "LOC::" scheme prefix is how EPOC16's link layer names file
    // locations; the receiver parses it before creating the file, and
    // a bare path makes it abort the transfer at save time (the UI
    // reports "Receiving interrupted" even though every IrLAP frame
    // was acknowledged).
    const block: number[] = [
      data.length & 0xff, (data.length >> 8) & 0xff,
      (data.length >> 16) & 0xff, (data.length >> 24) & 0xff,
      ...ascii('LOC::'), ...ascii(destPath), 0x00,
    ];
    if (block.length > this.headerLen) throw new IrSiboBeamError('destination path too long');
    while (block.length < this.headerLen) block.push(0x00);

    // Each [u16le len][block] is framed INDEPENDENTLY: the prefix
    // always starts a fresh I-frame and the block's final frame runs
    // short — exactly how both devices' own senders lay the stream out
    // (native captures: the Siena closes its 132-byte header with a
    // 10-byte frame, the 3c its 136-byte header with a 14-byte frame,
    // and the body prefix opens the next frame). Concatenating blocks
    // into one flat byte stream and slicing at 62 puts a block length
    // mid-frame, which the receivers cannot parse.
    let sentPayload = 0;
    const sendBlock = async (payload: number[]): Promise<void> => {
      const withLen = [payload.length & 0xff, (payload.length >> 8) & 0xff, ...payload];
      for (let off = 0; off < withLen.length; off += 62) {
        const part = withLen.slice(off, Math.min(off + 62, withLen.length));
        await sendIAwaitAck([0x01, 0x01, ...part]);
      }
    };
    await sendBlock(block);
    // Body: a single block, like the native senders (their u16 prefix
    // is the full body length). Split only when u16 can't express it.
    for (let off = 0; off < data.length; off += CHUNK) {
      const c = data.subarray(off, Math.min(off + CHUNK, data.length));
      await sendBlock(Array.from(c));
      sentPayload += c.length;
      cb.onProgress?.(sentPayload);
    }
    if (this.eofChunk) await sendBlock([]);   // zero-length end marker

    // ── Close ────────────────────────────────────────────────────────
    cb.onPhase?.('closing');
    // Keep polling RR (with our final N(r)) for a few rounds before
    // disconnecting: the receiver writes the file and may want to send
    // a closing PDU of its own (it can only transmit when polled). The
    // device's RD (request-disconnect, 0x53 response form) or repeated
    // silence both mean it is done.
    for (let i = 0; i < 10; i++) {
      await this.turnaround();
      this.tx([(CONN_ADDR << 1) | 1, ((this.vr & 7) << 5) | PF_BIT | 0x01]);
      const fs = await this.collect(400);
      let rd = false;
      for (const f of fs) {
        if (f.length >= 2 && (f[1] & ~PF_BIT) === 0x43) rd = true;   // RD/DISC response form
        if (f.length >= 2 && (f[1] & 0x01) === 0) i = 0;             // it sent data: keep going
      }
      if (rd) break;
    }
    await this.turnaround();
    this.tx([(CONN_ADDR << 1) | 1, U_DISC | PF_BIT]);
    await this.waitFor(deadline, f => (f[1] & ~PF_BIT) === U_UA, 2_000);
  }

  // ── internals ──────────────────────────────────────────────────────
  private checkAbort(deadline: number): void {
    if (this.canceled) throw new IrSiboBeamError('beam canceled');
    if (Date.now() > deadline) throw new IrSiboBeamError('beam timed out');
  }

  private tx(body: number[]): void {
    // The 3c's own sender precedes every frame with ~10 0xFF preamble
    // bytes; mirror that so slower receive loops have sync time.
    const pre = new Uint8Array(10).fill(0xff);
    this.writeBytes(pre);
    this.writeBytes(encodeSirFrame(new Uint8Array(body)));
  }

  private pump(): void {
    const chunk = this.readBytes();
    if (chunk.length === 0) return;
    this.lastRxAt = Date.now();
    for (const f of this.decoder.feed(chunk)) {
      const b = f.body;
      if (b.length >= 2 && (b[1] & 0x01) === 0) {
        // Device I-frame: advance V(r) when in order (retransmissions
        // of an already-seen N(s) leave it alone).
        const ns = (b[1] >> 1) & 7;
        if (ns === this.vr) this.vr = (this.vr + 1) & 7;
      }
      this.rxQueue.push(b);
    }
  }

  // Hold off until turnaroundMs has elapsed since the device's last
  // bytes arrived (IrLAP minimum-turnaround — see SiboSendOptions).
  private async turnaround(): Promise<void> {
    for (;;) {
      this.pump();
      const wait = this.lastRxAt + this.turnaroundMs - Date.now();
      if (wait <= 0) return;
      await new Promise(r => setTimeout(r, Math.min(wait, 25)));
    }
  }

  private async collect(ms: number): Promise<Uint8Array[]> {
    const until = Date.now() + ms;
    while (Date.now() < until) {
      this.pump();
      if (this.rxQueue.length > 0) break;
      await new Promise(r => setTimeout(r, 20));
    }
    this.pump();
    return this.rxQueue.splice(0);
  }

  private async waitFor(deadline: number, pred: (f: Uint8Array) => boolean,
                        ms = 3_000): Promise<boolean> {
    const until = Math.min(Date.now() + ms, deadline);
    while (Date.now() < until) {
      this.pump();
      while (this.rxQueue.length > 0) {
        const f = this.rxQueue.shift()!;
        if (f.length >= 2 && pred(f)) return true;
      }
      await new Promise(r => setTimeout(r, 20));
    }
    return false;
  }

  private sniffXidResponse(frames: Uint8Array[]): number[] | null {
    for (const f of frames) {
      // XID response: ctrl 0xAF (|P/F), format 1, src addr at [3..6].
      if (f.length >= 11 && (f[1] & ~PF_BIT) === 0xaf && f[2] === 0x01) {
        return Array.from(f.subarray(3, 7));
      }
    }
    return null;
  }
}
