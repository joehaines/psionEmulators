// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Regression: Remote Link UPLOAD must survive a dropped serial frame.
//
// Field report: "Remote Link upload fails partway, at some point every
// time" — only on the browser/web-worker bridge, never on the lossless
// native socket. Root cause: the worker serial bridge drops bytes when the
// device RX FIFO overflows faster than the guest drains it; a dropped frame
// fails CRC on the device, which then sends no Ack. DOWNLOADS survive this
// because the device retransmits its own data until acked — but the host's
// outbound (upload) Data_PDUs had NO retransmission, so one dropped frame
// stalled the whole transfer forever.
//
// LinkLayer now tracks unacked host→device Data_PDUs and resends them when
// the device's echoing Ack doesn't arrive in time (PlpClient.tick() drives
// LinkLayer.pumpRetransmit()). This test models a LOSSY bridge two ways and
// asserts the upload still completes with byte-exact contents:
//   (a) drop a host Data_Pdu outright (device never sees it → no Ack)
//   (b) drop the device's Ack of a delivered frame (host resends a frame the
//       device already has → the device must dedup it, not double-apply it)
//
// Run with:
//   node --experimental-strip-types frontend/src/lib/__tests__/plp.retransmit.test.mts

import { PlpClient } from '../plp/client-spec.ts';
import { encodePdu, decodePdu, reqConPdu, ackPdu, dataPdu, reqReqPdu } from '../plp/link.ts';
import { FrameDecoder } from '../plp/framing.ts';
import {
  encodeNcp, decodeNcp, makeConnectResponsePacket,
  makeNcpInfoPacket, makeConnectPacket, makeCompletePacket,
} from '../plp/ncp-spec.ts';
import {
  NCP, NCP_VERSION, NCP_SERVICE_LINK, RFSV32, RFSV32_REPLY_MARKER, EpocErr,
} from '../plp/types.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
  else console.log(`ok   ${msg}`);
}
const sleep = (ms: number) => new Promise<void>(r => setTimeout(r, ms));

// ── Lossy byte bridge ───────────────────────────────────────────────
// Forwards bytes both ways, but can drop a chosen host→device Data_Pdu
// (the worker-FIFO-overflow case) or a chosen device→host Ack (the
// ack-loss case). Each is dropped only on first occurrence, so the
// retransmit recovers.
class LossyBridge {
  private fromDevice: number[] = [];
  private fromClient: number[] = [];

  // Drop the host Data_Pdu whose 1-based data-frame index matches this.
  dropHostDataIndex = 0;
  // Drop the device Ack whose Seq matches this (first occurrence only).
  dropDeviceAckSeq = -1;

  private hostDataSeen = 0;
  private droppedHostData = false;
  private droppedDeviceAck = false;
  private hostDec = new FrameDecoder();

  read(): Uint8Array {
    if (this.fromDevice.length === 0) return new Uint8Array(0);
    const out = new Uint8Array(this.fromDevice); this.fromDevice = []; return out;
  }
  // Host → device. Decode to spot (and maybe drop) a Data_Pdu.
  write(d: Uint8Array): number {
    for (const f of this.hostDec.feed(d)) {
      const pdu = decodePdu(f.payload);
      if (pdu && pdu.cont === 3 /* DATA */) {
        this.hostDataSeen++;
        if (!this.droppedHostData && this.hostDataSeen === this.dropHostDataIndex) {
          this.droppedHostData = true;
          continue;   // swallow this frame — the device never sees it
        }
      }
      // Re-frame the surviving payload onto the device's inbound queue.
      for (const b of encodePdu(pdu!)) this.fromClient.push(b);
    }
    return d.length;
  }
  // Device → host. Drop a chosen Ack once.
  deviceTx(d: Uint8Array): void {
    const dec = new FrameDecoder();
    for (const f of dec.feed(d)) {
      const pdu = decodePdu(f.payload);
      if (pdu && pdu.cont === 0 /* ACK */ && pdu.seq === this.dropDeviceAckSeq &&
          !this.droppedDeviceAck) {
        this.droppedDeviceAck = true;
        continue;   // swallow this Ack — host will resend the frame
      }
      for (const b of encodePdu(pdu!)) this.fromDevice.push(b);
    }
  }
  deviceRx(): Uint8Array {
    if (this.fromClient.length === 0) return new Uint8Array(0);
    const out = new Uint8Array(this.fromClient); this.fromClient = []; return out;
  }
}

// ── Mock R5 device with in-order (go-back-N) delivery ───────────────
// Models the real Windermere reliable link: it accepts Data_PDUs strictly
// in Seq order and Acks the last IN-ORDER Seq it has accepted. A frame that
// arrives out of order (because an earlier one was lost) is DISCARDED and
// the last good Seq re-acked, so the host's go-back-N retransmit redelivers
// the whole tail in order. A duplicate of an already-accepted frame (Seq
// below the expected one — e.g. a host resend after a lost Ack) is likewise
// re-acked but NOT re-applied. This is the behaviour the adoption path's
// "duplicate-ack of the last Seq the device accepted" comment describes.
class DedupRevo {
  private bridge: LossyBridge;
  private decoder = new FrameDecoder();
  private myMagic = new Uint8Array([0x11, 0x22, 0x33, 0x44]);
  private seqTx = 0;
  private clientInfoSeen = false;
  private rfsvServerChan = 0;
  private rfsvClientChan = 0;
  private timer: ReturnType<typeof setInterval> | null = null;
  private expectedSeq = 1;   // next in-order host Data_Pdu Seq we'll accept
  private lastGoodSeq = 0;    // last in-order Seq accepted (what we Ack)
  private nextHandle = 0x401;
  private handles = new Map<number, { path: string; kind: 'read' | 'write'; offset: number }>();
  files = new Map<string, Uint8Array>();
  private partialBuf = new Map<string, Uint8Array[]>();

  constructor(bridge: LossyBridge) { this.bridge = bridge; }

  start(): void {
    this.bridge.deviceTx(encodePdu(reqReqPdu(1)));
    this.timer = setInterval(() => this.pump(), 1);
  }
  stop(): void { if (this.timer) clearInterval(this.timer); this.timer = null; }
  private pump(): void {
    const bytes = this.bridge.deviceRx();
    if (bytes.length) for (const f of this.decoder.feed(bytes)) this.handleFrame(f.payload);
  }
  private sendPdu(pdu: { cont: number; seq: number; data: Uint8Array }): void {
    this.bridge.deviceTx(encodePdu(pdu));
  }
  private sendDataPdu(data: Uint8Array): void {
    this.seqTx = (this.seqTx + 1) & 0x7FF;
    if (this.seqTx === 0) this.seqTx = 1;
    this.sendPdu(dataPdu(this.seqTx, data));
  }
  private handleFrame(payload: Uint8Array): void {
    const pdu = decodePdu(payload);
    if (!pdu) return;
    if (pdu.cont === 2 /* REQ */) {
      if (pdu.data.length === 0) this.sendPdu(reqConPdu(this.myMagic, 4));
      else if (pdu.data.length === 4) {
        this.sendPdu(ackPdu(0));
        this.sendDataPdu(encodeNcp(makeNcpInfoPacket(NCP_VERSION.EPOC_ER3,
          new Uint8Array([0xAA, 0xBB, 0xCC, 0xDD]))));
        this.sendDataPdu(encodeNcp(makeConnectPacket(1, NCP_SERVICE_LINK)));
      }
      return;
    }
    if (pdu.cont === 0 /* ACK */) return;
    if (pdu.cont === 3 /* DATA */) {
      // Go-back-N receiver: accept only the next in-order Seq; otherwise
      // discard and re-ack the last good one (forcing the host to resend
      // the tail). Either way Ack the last IN-ORDER Seq we hold.
      if (pdu.seq === this.expectedSeq) {
        this.lastGoodSeq = pdu.seq;
        this.expectedSeq = (this.expectedSeq + 1) & 0x7FF;
        if (this.expectedSeq === 0) this.expectedSeq = 1;
        this.sendPdu(ackPdu(this.lastGoodSeq));
        this.handleNcpPayload(pdu.data);
      } else {
        this.sendPdu(ackPdu(this.lastGoodSeq));   // dup / out-of-order: re-ack only
      }
      return;
    }
  }
  private handleNcpPayload(payload: Uint8Array): void {
    const p = decodeNcp(payload);
    if (!p) return;
    const key = `${p.destChan}/${p.srcChan}`;
    if (p.type === NCP.PARTIAL) {
      let parts = this.partialBuf.get(key);
      if (!parts) { parts = []; this.partialBuf.set(key, parts); }
      parts.push(p.data);
      return;
    }
    if (this.partialBuf.has(key)) {
      const parts = this.partialBuf.get(key)!;
      parts.push(p.data);
      let total = 0; for (const c of parts) total += c.length;
      const merged = new Uint8Array(total);
      let off = 0; for (const c of parts) { merged.set(c, off); off += c.length; }
      this.partialBuf.delete(key);
      p.data = merged;
    }
    if (p.type === NCP.INFO) { this.clientInfoSeen = true; return; }
    if (p.type === NCP.CONNECT) {
      const name = new TextDecoder().decode(
        p.data[p.data.length - 1] === 0 ? p.data.subarray(0, -1) : p.data);
      if (!this.clientInfoSeen) {
        this.sendDataPdu(encodeNcp(makeConnectResponsePacket(p.srcChan, p.srcChan, 1)));
        return;
      }
      if (name === 'SYS$RFSV.*' || name === 'SYS$RFSV') {
        this.rfsvServerChan = 10;
        this.rfsvClientChan = p.srcChan;
        this.sendDataPdu(encodeNcp(makeConnectResponsePacket(
          this.rfsvServerChan, this.rfsvClientChan, 0)));
      } else {
        this.sendDataPdu(encodeNcp(makeConnectResponsePacket(0, p.srcChan, 1)));
      }
      return;
    }
    if (p.type === NCP.COMPLETE) {
      if (p.destChan === this.rfsvServerChan && p.srcChan === this.rfsvClientChan) {
        this.handleRfsv(p.data);
      }
      return;
    }
  }
  private sendReply(opId: number, status: number, body = new Uint8Array(0)): void {
    const reply = new Uint8Array(8 + body.length);
    reply[0] = RFSV32_REPLY_MARKER & 0xFF;
    reply[1] = (RFSV32_REPLY_MARKER >> 8) & 0xFF;
    reply[2] = opId & 0xFF; reply[3] = (opId >> 8) & 0xFF;
    const s = status | 0;
    reply[4] = s & 0xFF; reply[5] = (s >> 8) & 0xFF;
    reply[6] = (s >> 16) & 0xFF; reply[7] = (s >> 24) & 0xFF;
    reply.set(body, 8);
    this.sendDataPdu(encodeNcp(makeCompletePacket(
      this.rfsvClientChan, this.rfsvServerChan, reply)));
  }
  private u32(n: number): Uint8Array {
    return new Uint8Array([n & 0xFF, (n >> 8) & 0xFF, (n >> 16) & 0xFF, (n >>> 24) & 0xFF]);
  }
  private readU32(b: Uint8Array, off: number): number {
    return ((b[off] | (b[off+1] << 8) | (b[off+2] << 16) | (b[off+3] << 24)) >>> 0);
  }
  private readLenStr(b: Uint8Array, off: number): string {
    const len = b[off] | ((b[off+1] & 0x7F) << 8);
    return new TextDecoder().decode(b.subarray(off + 2, off + 2 + len));
  }
  private handleRfsv(data: Uint8Array): void {
    if (data.length < 4) return;
    const reason = data[0] | (data[1] << 8);
    const opId = data[2] | (data[3] << 8);
    const args = data.subarray(4);
    if (reason === RFSV32.REPLACE_FILE) {
      const path = this.readLenStr(args, 4);
      this.files.set(path, new Uint8Array(0));
      const h = this.nextHandle++;
      this.handles.set(h, { path, kind: 'write', offset: 0 });
      this.sendReply(opId, 0, this.u32(h));
      return;
    }
    if (reason === RFSV32.OPEN_FILE) {
      const path = this.readLenStr(args, 4);
      if (!this.files.has(path)) { this.sendReply(opId, EpocErr.NotFound ?? -1); return; }
      const h = this.nextHandle++;
      this.handles.set(h, { path, kind: 'read', offset: 0 });
      this.sendReply(opId, 0, this.u32(h));
      return;
    }
    if (reason === RFSV32.WRITE_FILE) {
      const handle = this.readU32(args, 0);
      const chunk = args.subarray(4);
      const slot = this.handles.get(handle);
      if (!slot || slot.kind !== 'write') { this.sendReply(opId, -8); return; }
      const existing = this.files.get(slot.path) || new Uint8Array(0);
      const merged = new Uint8Array(existing.length + chunk.length);
      merged.set(existing, 0); merged.set(chunk, existing.length);
      this.files.set(slot.path, merged);
      this.sendReply(opId, 0);
      return;
    }
    if (reason === RFSV32.READ_FILE) {
      const handle = this.readU32(args, 0);
      const want = this.readU32(args, 4);
      const slot = this.handles.get(handle);
      if (!slot || slot.kind !== 'read') { this.sendReply(opId, -8); return; }
      const file = this.files.get(slot.path);
      if (!file) { this.sendReply(opId, -1); return; }
      const remaining = file.length - slot.offset;
      if (remaining <= 0) { this.sendReply(opId, 0); return; }
      const n = Math.min(want, remaining, 300);
      const chunk = file.subarray(slot.offset, slot.offset + n);
      slot.offset += n;
      this.sendReply(opId, 0, chunk);
      return;
    }
    if (reason === RFSV32.CLOSE_HANDLE) {
      this.handles.delete(this.readU32(args, 0));
      this.sendReply(opId, 0);
      return;
    }
    this.sendReply(opId, -5);
  }
}

async function uploadWithLoss(opts: { dropHostDataIndex?: number; dropDeviceAckSeq?: number; label: string }): Promise<void> {
  const bridge = new LossyBridge();
  if (opts.dropHostDataIndex) bridge.dropHostDataIndex = opts.dropHostDataIndex;
  if (opts.dropDeviceAckSeq !== undefined) bridge.dropDeviceAckSeq = opts.dropDeviceAckSeq;
  const mock = new DedupRevo(bridge);
  mock.start();
  // Short retransmit interval so the test runs quickly; default 600ms would
  // make the harness sleeps long. Real client uses the 600ms default.
  const client = new PlpClient(() => bridge.read(), d => bridge.write(d),
    { pollHz: 1000, retxIntervalMs: 80, maxRetxRounds: 20 });
  client.setChunkSize(1024);
  client.start();
  try {
    await client.connect(3000);
    const payload = new Uint8Array(4096);
    for (let i = 0; i < payload.length; i++) payload[i] = (i * 13 + 7) & 0xFF;
    await client.uploadFile('C:\\up.bin', payload);
    const stored = mock.files.get('C:\\up.bin');
    check(stored?.length === payload.length,
      `${opts.label}: stored length ${stored?.length ?? 0}, want ${payload.length}`);
    let mismatch = -1;
    if (stored) for (let i = 0; i < Math.min(stored.length, payload.length); i++) {
      if (stored[i] !== payload[i]) { mismatch = i; break; }
    }
    check(mismatch === -1, `${opts.label}: stored bytes exact (first mismatch at ${mismatch})`);
  } finally {
    client.disconnect();
    mock.stop();
  }
}

// (a) A host upload Data_Pdu is dropped outright — device never acks it.
await uploadWithLoss({ dropHostDataIndex: 6, label: 'drop host Data_Pdu' });
await sleep(20);
// (b) A device Ack is lost — host resends an already-delivered frame; the
// device must dedup it so the file isn't corrupted by a double-applied chunk.
await uploadWithLoss({ dropDeviceAckSeq: 6, label: 'drop device Ack (dedup)' });
await sleep(20);

if (failures > 0) {
  console.error(`\n${failures} retransmit test failure(s)`);
  process.exit(1);
}
console.log('OK — upload retransmission tests passed');
