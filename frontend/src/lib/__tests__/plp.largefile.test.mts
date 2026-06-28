// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Regression: large-file transfer over Remote Link.
//
// Reproduces the field report "large file transfer fails halfway".
// A real EPOC32 RFSV server is free to satisfy an FREAD with FEWER
// bytes than requested without being at end-of-file — F32 fills a read
// up to whatever the server's internal buffer boundary is, which can be
// well under the host's CHUNK size. The download loop must only stop
// when the server returns ZERO bytes (or an explicit EOF status), never
// on a short read; otherwise the downloaded file is silently truncated
// at the first short read.
//
// The mock here returns at most `maxReadFill` bytes per FREAD reply
// (simulating that buffer boundary) and zero bytes once the cursor
// reaches EOF — exactly the shape that broke the short-read terminator.
//
// Run with:
//   node --experimental-strip-types frontend/src/lib/__tests__/plp.largefile.test.mts

import { PlpClient } from '../plp/client-spec.ts';
import { encodePdu, decodePdu, reqConPdu, ackPdu, dataPdu, reqReqPdu } from '../plp/link.ts';
import { FrameDecoder } from '../plp/framing.ts';
import {
  encodeNcp, decodeNcp, makeConnectPacket, makeConnectResponsePacket,
  makeNcpInfoPacket, makeCompletePacket,
} from '../plp/ncp-spec.ts';
import {
  NCP, NCP_VERSION, NCP_SERVICE_LINK, RFSV32, RFSV32_REPLY_MARKER, EpocErr,
} from '../plp/types.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}
const sleep = (ms: number) => new Promise<void>(r => setTimeout(r, ms));

// ── In-memory byte bridge ───────────────────────────────────────────
class Bridge {
  private fromDevice: number[] = [];
  private fromClient: number[] = [];
  read(): Uint8Array {
    if (this.fromDevice.length === 0) return new Uint8Array(0);
    const out = new Uint8Array(this.fromDevice); this.fromDevice = []; return out;
  }
  write(d: Uint8Array): number {
    for (const b of d) this.fromClient.push(b);
    return d.length;
  }
  deviceTx(d: Uint8Array): void { for (const b of d) this.fromDevice.push(b); }
  deviceRx(): Uint8Array {
    if (this.fromClient.length === 0) return new Uint8Array(0);
    const out = new Uint8Array(this.fromClient); this.fromClient = []; return out;
  }
}

// ── Mock Revo with a short-read RFSV server ──────────────────────────
class MockRevo {
  private bridge: Bridge;
  private decoder = new FrameDecoder();
  private myMagic = new Uint8Array([0x11, 0x22, 0x33, 0x44]);
  private linkUp = false;
  private seqTx = 0;
  private clientInfoSeen = false;
  private rfsvServerChan = 0;
  private rfsvClientChan = 0;
  private timer: ReturnType<typeof setInterval> | null = null;

  files = new Map<string, Uint8Array>();
  // The most bytes the server will hand back in one FREAD reply. Real
  // F32 short-reads at its buffer boundary; set well under the client's
  // CHUNK to exercise the mid-file short read.
  maxReadFill = 300;
  // When true, signal end-of-file with an EpocErr.Eof status reply
  // instead of a zero-length status-0 reply (both are legal; the loop
  // must handle either).
  eofViaStatus = false;

  private nextHandle = 0x401;
  private handles = new Map<number, { path: string; kind: 'read' | 'write'; offset: number }>();

  constructor(bridge: Bridge) { this.bridge = bridge; }
  start(): void {
    this.bridge.deviceTx(encodePdu(reqReqPdu(1)));
    this.timer = setInterval(() => this.tick(), 1);
  }
  stop(): void { if (this.timer !== null) { clearInterval(this.timer); this.timer = null; } }

  private tick(): void {
    const bytes = this.bridge.deviceRx();
    if (bytes.length === 0) return;
    for (const f of this.decoder.feed(bytes)) this.handleFrame(f.payload);
  }
  private sendPdu(pdu: { cont: number; seq: number; data: Uint8Array }): void {
    this.bridge.deviceTx(encodePdu(pdu));
  }
  private sendDataPdu(data: Uint8Array): void {
    // Mod-2048, matching the real EPOC R5 device. The host enforces a strict
    // in-order receive window on this Seq space (see LinkLayer's PDU_CONT_DATA
    // handler), so a mod-8 wrap here would look out-of-order and be dropped.
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
        this.linkUp = true;
        this.sendDataPdu(encodeNcp(makeNcpInfoPacket(NCP_VERSION.EPOC_ER3,
          new Uint8Array([0xAA, 0xBB, 0xCC, 0xDD]))));
        this.sendDataPdu(encodeNcp(makeConnectPacket(1, NCP_SERVICE_LINK)));
      }
      return;
    }
    if (pdu.cont === 0 /* ACK */) { if (!this.linkUp) this.linkUp = true; return; }
    if (pdu.cont === 3 /* DATA */) {
      this.sendPdu(ackPdu(pdu.seq));
      this.handleNcpPayload(pdu.data);
      return;
    }
  }

  // Reassembly buffer for inbound PARTIAL fragment runs, keyed by the
  // (destChan,srcChan) pair — mirrors the real Ncp so a fragmented
  // WRITE_FILE request (>297 NCP payload bytes) is rebuilt before
  // dispatch instead of seeing only the trailing COMPLETE fragment.
  private partialBuf = new Map<string, Uint8Array[]>();

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
    reply[2] = opId & 0xFF;
    reply[3] = (opId >> 8) & 0xFF;
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
      if (remaining <= 0) {
        if (this.eofViaStatus) this.sendReply(opId, EpocErr.Eof);
        else this.sendReply(opId, 0);   // zero-length read = EOF
        return;
      }
      // Short read: hand back at most maxReadFill bytes, like real F32.
      const n = Math.min(want, remaining, this.maxReadFill);
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

async function downloadWithFill(opts: { fill: number; eofViaStatus: boolean }): Promise<void> {
  const bridge = new Bridge();
  const mock = new MockRevo(bridge);
  mock.maxReadFill = opts.fill;
  mock.eofViaStatus = opts.eofViaStatus;
  mock.start();

  const client = new PlpClient(() => bridge.read(), d => bridge.write(d), { pollHz: 1000 });
  client.setChunkSize(1024);   // CHUNK deliberately larger than the fill
  client.start();
  try {
    await client.connect(2000);
    // A file several CHUNKs long so a short-read terminator would stop
    // well before the end.
    const payload = new Uint8Array(5000);
    for (let i = 0; i < payload.length; i++) payload[i] = (i * 37 + 5) & 0xFF;
    mock.files.set('C:\\big.bin', payload);

    const got = await client.downloadFile('C:\\big.bin');
    check(got.length === payload.length,
      `download (fill=${opts.fill}, eofViaStatus=${opts.eofViaStatus}) length ${got.length}, want ${payload.length}`);
    let mismatch = -1;
    for (let i = 0; i < Math.min(got.length, payload.length); i++) {
      if (got[i] !== payload[i]) { mismatch = i; break; }
    }
    check(mismatch === -1,
      `download (fill=${opts.fill}) byte match (first mismatch at ${mismatch})`);
  } finally {
    client.disconnect();
    mock.stop();
  }
}

// Upload a large file, then read it back, asserting an exact round-trip.
async function uploadRoundtrip(): Promise<void> {
  const bridge = new Bridge();
  const mock = new MockRevo(bridge);
  mock.maxReadFill = 300;
  mock.start();
  const client = new PlpClient(() => bridge.read(), d => bridge.write(d), { pollHz: 1000 });
  client.setChunkSize(1024);
  client.start();
  try {
    await client.connect(2000);
    const payload = new Uint8Array(4096);
    for (let i = 0; i < payload.length; i++) payload[i] = (i * 13 + 7) & 0xFF;
    await client.uploadFile('C:\\up.bin', payload);
    const stored = mock.files.get('C:\\up.bin');
    check(stored?.length === payload.length,
      `upload stored length ${stored?.length ?? 0}, want ${payload.length}`);
    const got = await client.downloadFile('C:\\up.bin');
    check(got.length === payload.length,
      `upload→download length ${got.length}, want ${payload.length}`);
    let mismatch = -1;
    for (let i = 0; i < Math.min(got.length, payload.length); i++) {
      if (got[i] !== payload[i]) { mismatch = i; break; }
    }
    check(mismatch === -1, `upload→download byte match (first mismatch at ${mismatch})`);
  } finally {
    client.disconnect();
    mock.stop();
  }
}

await downloadWithFill({ fill: 300, eofViaStatus: false });
await sleep(10);
await downloadWithFill({ fill: 300, eofViaStatus: true });
await sleep(10);
await uploadRoundtrip();
await sleep(10);

if (failures > 0) {
  console.error(`\n${failures} large-file test failure(s)`);
  process.exit(1);
}
console.log('OK — large-file transfer tests passed');
