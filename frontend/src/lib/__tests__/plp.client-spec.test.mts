// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Integration test: PlpClient (frontend/src/lib/plp/client-spec.ts)
// against an in-process mock Revo. The mock implements the link
// handshake exactly as we observed the real Revo behave (Req_Req →
// Req_Con + magic → Ack), then announces LINK.* and responds to a
// SYS$RFSV.* Connect with success, and handles GET_DRIVE_LIST.
//
// Run with:
//   node --experimental-strip-types frontend/src/lib/__tests__/plp.client-spec.test.mts

import { PlpClient } from '../plp/client-spec.ts';
import { encodePdu, decodePdu, reqReqPdu, reqConPdu, ackPdu, dataPdu } from '../plp/link.ts';
import { FrameDecoder } from '../plp/framing.ts';
import { encodeNcp, decodeNcp, makeConnectPacket, makeConnectResponsePacket, makeNcpInfoPacket, makeCompletePacket } from '../plp/ncp-spec.ts';
import { NCP, NCP_VERSION, NCP_SERVICE_LINK, PDU_CONT_REQ, PDU_CONT_ACK, PDU_CONT_DISC, PDU_CONT_DATA, RFSV32, RFSV32_REPLY_MARKER } from '../plp/types.ts';

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
  // Used by the mock to push device-side bytes back to the client.
  deviceTx(d: Uint8Array): void {
    for (const b of d) this.fromDevice.push(b);
  }
  // Drain anything the client has sent so the mock can react.
  deviceRx(): Uint8Array {
    if (this.fromClient.length === 0) return new Uint8Array(0);
    const out = new Uint8Array(this.fromClient); this.fromClient = []; return out;
  }
}

// ── Mock Revo ───────────────────────────────────────────────────────
// Implements just enough of the protocol to make a PlpClient see a
// working device. Pumped at 1 ms via setInterval; expects the
// PlpClient on the other side.

class MockRevo {
  private bridge: Bridge;
  private decoder = new FrameDecoder();
  private myMagic = new Uint8Array([0x11, 0x22, 0x33, 0x44]);
  // Link state.
  private linkUp = false;
  // Our (device-side) sequence numbers.
  private seqTx = 0;
  // Has the client sent its NCP Info yet? Mirrors the real device's
  // requirement (refusing Connect until both sides have exchanged info).
  private clientInfoSeen = false;
  // SYS$RFSV channel — assigned when the client opens it.
  private rfsvServerChan = 0;
  private rfsvClientChan = 0;
  private timer: ReturnType<typeof setInterval> | null = null;
  // Reset-recovery hooks. When armBounce is set, the mock drops and
  // re-handshakes its link (forgetting every channel) the first time it
  // receives a GET_DRIVE_LIST — exactly what the netBook / Series 7
  // RemoteLinkServer does when the kernel is slow to reschedule its
  // serial thread mid-session. firstDataSeqAfterBounce captures the
  // host's first post-bounce Data_Pdu Seq so the test can assert the
  // link layer reset its sequence counter (the core fix).
  armBounce = false;
  private bounced = false;
  firstDataSeqAfterBounce: number | null = null;

  constructor(bridge: Bridge) {
    this.bridge = bridge;
  }
  start(): void {
    // Spontaneously send the Req_Req_Pdu the way the real device does
    // when DSR goes high.
    this.sendPdu(reqReqPdu(1));
    this.timer = setInterval(() => this.tick(), 1);
  }
  stop(): void {
    if (this.timer !== null) { clearInterval(this.timer); this.timer = null; }
  }

  private tick(): void {
    const bytes = this.bridge.deviceRx();
    if (bytes.length === 0) return;
    const frames = this.decoder.feed(bytes);
    for (const f of frames) this.handleFrame(f.payload);
  }

  private sendPdu(pdu: { cont: number; seq: number; data: Uint8Array }): void {
    this.bridge.deviceTx(encodePdu(pdu));
  }
  private sendDataPdu(data: Uint8Array): number {
    this.seqTx = (this.seqTx + 1) & 0x7;
    if (this.seqTx === 0) this.seqTx = 1;
    this.sendPdu(dataPdu(this.seqTx, data));
    return this.seqTx;
  }
  // Tear our link down and re-initiate the handshake, forgetting every
  // channel — the EPOC R5 mid-session link bounce we reproduce.
  private bounceLink(): void {
    this.linkUp = false;
    this.seqTx = 0;
    this.rfsvServerChan = 0;
    this.rfsvClientChan = 0;
    this.clientInfoSeen = false;
    this.sendPdu(reqReqPdu(1));
  }

  private handleFrame(payload: Uint8Array): void {
    const pdu = decodePdu(payload);
    if (!pdu) return;
    if (pdu.cont === PDU_CONT_REQ) {
      if (pdu.data.length === 0) {
        // Client's Req_Req_Pdu reply — confirm with our magic.
        this.sendPdu(reqConPdu(this.myMagic, 4));
      } else if (pdu.data.length === 4) {
        // Client's Req_Con_Pdu with its magic. Ack and consider link up.
        this.sendPdu(ackPdu(0));
        this.linkUp = true;
        // Send our initial Data_Pdus: NCP Info then Connect for LINK.*.
        this.sendDataPdu(encodeNcp(makeNcpInfoPacket(NCP_VERSION.EPOC_ER3,
          new Uint8Array([0xAA, 0xBB, 0xCC, 0xDD]))));
        this.sendDataPdu(encodeNcp(makeConnectPacket(1, NCP_SERVICE_LINK)));
      }
      return;
    }
    if (pdu.cont === PDU_CONT_ACK) {
      if (!this.linkUp) {
        // Client's Ack of our Req_Con — link is up.
        this.linkUp = true;
      }
      return;
    }
    if (pdu.cont === PDU_CONT_DATA) {
      if (this.bounced && this.firstDataSeqAfterBounce === null) {
        this.firstDataSeqAfterBounce = pdu.seq;
      }
      // Auto-ack at the link layer (just by sending back an Ack with
      // our `seqRx`; we don't track strict sequence here).
      this.sendPdu(ackPdu(pdu.seq));
      this.handleNcpPayload(pdu.data);
      return;
    }
    if (pdu.cont === PDU_CONT_DISC) {
      // Peer told us to drop the link. Reset session state so a
      // subsequent reconnect from the same client works cleanly.
      this.linkUp = false;
      this.seqTx = 0;
      this.rfsvServerChan = 0;
      this.rfsvClientChan = 0;
      this.clientInfoSeen = false;
      return;
    }
  }

  private handleNcpPayload(payload: Uint8Array): void {
    const p = decodeNcp(payload);
    if (!p) return;
    if (p.type === NCP.INFO) {
      this.clientInfoSeen = true;
      return;
    }
    if (p.type === NCP.CONNECT) {
      // Client wants a server. The PDF says we refuse with non-zero
      // status if Info hasn't been exchanged.
      const name = new TextDecoder().decode(
        p.data[p.data.length - 1] === 0 ? p.data.subarray(0, -1) : p.data,
      );
      if (!this.clientInfoSeen) {
        // Per real-Revo behaviour: send Connect Response + immediate
        // Disconnect notification.
        this.sendDataPdu(encodeNcp(makeConnectResponsePacket(p.srcChan, p.srcChan, 1)));
        // (Skip the spec's Disconnection frame for this mock — the
        // PlpClient should already see status != 0 and fail.)
        return;
      }
      if (name === 'SYS$RFSV.*' || name === 'SYS$RFSV') {
        this.rfsvServerChan = 10;
        this.rfsvClientChan = p.srcChan;
        this.sendDataPdu(encodeNcp(makeConnectResponsePacket(
          this.rfsvServerChan, this.rfsvClientChan, 0,
        )));
      } else {
        // Reject unknown services.
        this.sendDataPdu(encodeNcp(makeConnectResponsePacket(0, p.srcChan, 1)));
      }
      return;
    }
    if (p.type === NCP.CONNECT_RESPONSE) {
      // We don't currently send Connect requests from this mock —
      // ignore for now.
      return;
    }
    if (p.type === NCP.COMPLETE) {
      // RFSV command on our SYS$RFSV channel.
      if (p.destChan === this.rfsvServerChan && p.srcChan === this.rfsvClientChan) {
        this.handleRfsv(p.data);
      }
      return;
    }
  }

  // Per-mock virtual filesystem keyed by absolute path.
  files: Map<string, Uint8Array> = new Map();
  // Open file/dir handles. Each handle keeps its kind + path + cursor.
  private nextHandle = 0x401;
  private handles: Map<number, { path: string; kind: 'read' | 'write'; offset: number }> = new Map();

  private sendReply(opId: number, status: number, body: Uint8Array = new Uint8Array(0)): void {
    const reply = new Uint8Array(8 + body.length);
    reply[0] = RFSV32_REPLY_MARKER & 0xFF;
    reply[1] = (RFSV32_REPLY_MARKER >> 8) & 0xFF;
    reply[2] = opId & 0xFF;
    reply[3] = (opId >> 8) & 0xFF;
    // status as 32-bit LE signed
    const s = status | 0;
    reply[4] = s & 0xFF;
    reply[5] = (s >> 8) & 0xFF;
    reply[6] = (s >> 16) & 0xFF;
    reply[7] = (s >> 24) & 0xFF;
    reply.set(body, 8);
    this.sendDataPdu(encodeNcp(makeCompletePacket(
      this.rfsvClientChan, this.rfsvServerChan, reply,
    )));
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
    const opId   = data[2] | (data[3] << 8);
    const args   = data.subarray(4);

    if (reason === RFSV32.GET_DRIVE_LIST) {
      if (this.armBounce && !this.bounced) {
        // Simulate the device dropping its link mid-session: forget the
        // channel and re-initiate the handshake instead of replying. A
        // correct host resets its sequence counters, re-opens SYS$RFSV,
        // and resends — converging on a successful drive list.
        this.bounced = true;
        this.bounceLink();
        return;
      }
      const body = new Uint8Array(26);
      body[2]  = 0x11;  // C: present
      body[25] = 0x12;  // Z: present
      this.sendReply(opId, 0, body);
      return;
    }
    if (reason === RFSV32.REPLACE_FILE) {
      // [mode:u32][len:u16][path]
      const path = this.readLenStr(args, 4);
      this.files.set(path, new Uint8Array(0));
      const h = this.nextHandle++;
      this.handles.set(h, { path, kind: 'write', offset: 0 });
      this.sendReply(opId, 0, this.u32(h));
      return;
    }
    if (reason === RFSV32.OPEN_FILE) {
      const path = this.readLenStr(args, 4);
      const existing = this.files.get(path);
      if (!existing) { this.sendReply(opId, -1 /* NotFound */); return; }
      const h = this.nextHandle++;
      this.handles.set(h, { path, kind: 'read', offset: 0 });
      this.sendReply(opId, 0, this.u32(h));
      return;
    }
    if (reason === RFSV32.WRITE_FILE) {
      const handle = this.readU32(args, 0);
      const data = args.subarray(4);
      const slot = this.handles.get(handle);
      if (!slot || slot.kind !== 'write') { this.sendReply(opId, -8 /* BadHandle */); return; }
      const existing = this.files.get(slot.path) || new Uint8Array(0);
      const merged = new Uint8Array(existing.length + data.length);
      merged.set(existing, 0);
      merged.set(data, existing.length);
      this.files.set(slot.path, merged);
      this.sendReply(opId, 0);
      return;
    }
    if (reason === RFSV32.READ_FILE) {
      const handle = this.readU32(args, 0);
      const want   = this.readU32(args, 4);
      const slot = this.handles.get(handle);
      if (!slot || slot.kind !== 'read') { this.sendReply(opId, -8); return; }
      const file = this.files.get(slot.path);
      if (!file) { this.sendReply(opId, -1); return; }
      const chunk = file.subarray(slot.offset, slot.offset + Math.min(want, file.length - slot.offset));
      slot.offset += chunk.length;
      this.sendReply(opId, 0, chunk);
      return;
    }
    if (reason === RFSV32.CLOSE_HANDLE) {
      const handle = this.readU32(args, 0);
      this.handles.delete(handle);
      this.sendReply(opId, 0);
      return;
    }
    if (reason === RFSV32.DELETE) {
      const path = this.readLenStr(args, 0);
      const had = this.files.delete(path);
      this.sendReply(opId, had ? 0 : -1);
      return;
    }
    // Unknown opcode → bad-name-ish error so the client sees something
    // useful instead of hanging.
    this.sendReply(opId, -5 /* NotSupported */);
  }
}

// ── Test driver ─────────────────────────────────────────────────────
async function run() {
  const bridge = new Bridge();
  const mock = new MockRevo(bridge);
  mock.start();

  const client = new PlpClient(
    () => bridge.read(),
    d => bridge.write(d),
    { pollHz: 1000 },
  );
  client.setChunkSize(256);  // mock doesn't support NCP fragmentation
  client.start();

  try {
    await client.connect(2000);
    check(client.state === 'connected',
          `client.state = ${client.state} (want connected)`);
    const drives = await client.listDrives();
    check(drives.length === 2 && drives[0] === 'C:' && drives[1] === 'Z:',
          `listDrives → [${drives.join(',')}], want [C:, Z:]`);

    // Upload → download round-trip. Progress callback must fire at
    // least once per chunk (256 bytes), so for 2500 bytes we expect
    // roughly 10 progress reports culminating in 2500.
    const payload = new Uint8Array(2500);
    for (let i = 0; i < payload.length; i++) payload[i] = (i * 17) & 0xFF;
    const progressReports: number[] = [];
    await client.uploadFile('C:\\test.bin', payload, n => progressReports.push(n));
    check(progressReports.length >= 5,
          `upload progress fired ${progressReports.length} times (want ≥ 5 chunks)`);
    check(progressReports.at(-1) === 2500,
          `final progress report = ${progressReports.at(-1)}, want 2500`);
    check(mock.files.get('C:\\test.bin')?.length === 2500,
          `mock fs has uploaded file (length=${mock.files.get('C:\\test.bin')?.length ?? 0}, want 2500)`);
    const dlProgress: number[] = [];
    const dl = await client.downloadFile('C:\\test.bin', n => dlProgress.push(n));
    check(dlProgress.length >= 5,
          `download progress fired ${dlProgress.length} times (want ≥ 5 chunks)`);
    check(dl.length === payload.length,
          `download length ${dl.length}, want ${payload.length}`);
    let mismatch = -1;
    for (let i = 0; i < dl.length; i++) {
      if (dl[i] !== payload[i]) { mismatch = i; break; }
    }
    check(mismatch === -1, `byte-for-byte match (first mismatch at index ${mismatch})`);

    // Delete: file goes away, next download fails.
    await client.deleteFile('C:\\test.bin');
    check(!mock.files.has('C:\\test.bin'), 'mock fs no longer has file after delete');
    let threw = false;
    try { await client.downloadFile('C:\\test.bin'); } catch { threw = true; }
    check(threw, 'download of deleted file rejects');

    // Reconnect: disconnect cleanly, stand up a fresh client against
    // the same (live) mock, connect again, do another file op.
    // Without LinkLayer.disconnect() sending a Disc_Pdu and the
    // connect()-side initiate() that sends Req_Req_Pdu, this would
    // hang because the mock still thinks the link is up from the
    // first session.
    client.disconnect();
  } finally {
    client.stop();
  }

  // Second session: brand new PlpClient against the same MockRevo.
  const client2 = new PlpClient(
    () => bridge.read(),
    d => bridge.write(d),
    { pollHz: 1000 },
  );
  client2.start();
  try {
    await client2.connect(2000);
    check(client2.state === 'connected',
          `reconnect: client2.state = ${client2.state} (want connected)`);
    const drives = await client2.listDrives();
    check(drives.length === 2 && drives[0] === 'C:' && drives[1] === 'Z:',
          `reconnect: listDrives → [${drives.join(',')}], want [C:, Z:]`);
  } finally {
    client2.disconnect();
    mock.stop();
  }
}

// Regression: the device bounces its data-link mid-session (the exact
// Series 7 field failure — "DRIVE_LIST failed: … timed out after 10s").
// The host must reset its link sequence counters, tear down the stale
// NCP channel, re-open SYS$RFSV.* on the fresh link, and resend — so
// listDrives() still resolves instead of timing out.
async function runResetRecovery(): Promise<void> {
  const bridge = new Bridge();
  const mock = new MockRevo(bridge);
  mock.armBounce = true;
  mock.start();

  const client = new PlpClient(
    () => bridge.read(),
    d => bridge.write(d),
    { pollHz: 1000 },
  );
  client.setChunkSize(256);
  client.start();
  try {
    await client.connect(2000);
    check(client.state === 'connected',
          `reset-recovery: state after connect = ${client.state} (want connected)`);
    // This listDrives triggers the device's mid-session link bounce; the
    // client must transparently recover and still return the drive list.
    const drives = await client.listDrives();
    check(drives.length === 2 && drives[0] === 'C:' && drives[1] === 'Z:',
          `reset-recovery: listDrives → [${drives.join(',')}], want [C:, Z:]`);
    check(mock.firstDataSeqAfterBounce === 1,
          `reset-recovery: host first Data Seq after bounce = ${mock.firstDataSeqAfterBounce}, want 1 (counter reset)`);
    check(client.state === 'connected',
          `reset-recovery: state after recovery = ${client.state} (want connected)`);
  } finally {
    client.disconnect();
    mock.stop();
  }
}

await run();
await sleep(10); // settle
await runResetRecovery();
await sleep(10); // settle

if (failures > 0) {
  console.error(`\n${failures} test failure(s)`);
  process.exit(1);
}
console.log('OK — PlpClient integration test passed');
