// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// IrDA end-to-end test: drive the real IrSendClient against an in-process
// mock EPOC device that implements the device side of every layer
// (SIR framing, IrLAP secondary, IrLMP, IAS, Tiny TP with proactive
// credit, and the EPOC Eikon-IR file-receive protocol). Verifies a file
// beams through intact, and that a silent device (not in receive mode)
// surfaces IrNotListeningError.
//
// IMPORTANT: this mock mirrors the REAL Psion 5mx "Infrared receive" dialog
// (EPOC's Eikon IR transfer app, reference/Epoc SDK/.../Eikon/src/EIKIRDA.CPP),
// NOT a generic IrOBEX inbox. The wire behaviour here was reconstructed from
// LM-PDUs captured live off the harnessed 5mx (see scripts/test-infrared.sh
// trace). The device:
//   • registers IAS class "Epoc32:EikonIr:v2.0", attr "IrDA:TinyTP:LsapSel" → 8
//     (querying "OBEX" returns NO_SUCH_CLASS — the prior assumption was wrong);
//   • accepts a TinyTP connection on LSAP 8;
//   • reads one SDU "FILE <size> <att> <hi> <lo> <name>", replies "ACK Y";
//   • reads <size> raw body bytes, then gracefully LM-Disconnects (= file saved).
//
// Run via:
//   node --experimental-strip-types \
//       frontend/src/lib/__tests__/irda.e2e.test.mts
//
// Async tests run inside main() with an explicit process.exit — top-level
// await of timer-based promises deadlocks the strip-types loader.

import { IrSendClient, IrNotListeningError } from '../irda/index.ts';
import { encodeSirFrame, SirDecoder } from '../irda/framing.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) {
    console.error(`FAIL: ${msg}`);
    failures++;
  }
}

// ── Mock device ────────────────────────────────────────────────────────
const PF = 0x10;
const U_SNRM = 0x83;
const U_UA = 0x63;
const U_DISC = 0x43;
// IrLAP XID: the discovering primary SENDS the command control 0x2F; the
// EPOC/5mx secondary REPLIES with the response control 0xAF. Verified
// against a captured real 5mx XID response (control 0xBF = 0xAF | F). The
// mock mirrors that asymmetry so the test exercises the same wire format
// the real device uses, not the prior (wrong) 0xAF-command assumption.
const U_XID_CMD = 0x2f;
const U_XID_RESP = 0xaf;
const S_RR = 0x01;
const CONTROL_BIT = 0x80;
const INITIAL_CREDIT = 7;
// The EPOC Eikon-IR receiver binds LSAP-SEL 8 (KEikIrDALsapSel). The real 5mx
// returned this exact value from the IAS query.
const EIKONIR_LSAP = 0x08;
const EIKONIR_CLASS = 'Epoc32:EikonIr:v2.0';
const EIKONIR_ATTR = 'IrDA:TinyTP:LsapSel';
// IAS return codes.
const IAS_RET_SUCCESS = 0x00;
const IAS_RET_NO_SUCH_CLASS = 0x01;

function concat(parts: Uint8Array[]): Uint8Array {
  let n = 0;
  for (const p of parts) n += p.length;
  const out = new Uint8Array(n);
  let o = 0;
  for (const p of parts) {
    out.set(p, o);
    o += p.length;
  }
  return out;
}

class MockDevice {
  private decoder = new SirDecoder();
  private txQueue: number[] = [];
  // IrLAP secondary state.
  private connAddr = 1;
  private vs = 0;
  private vr = 0;
  private readonly deviceAddr = 0x1a2b3c4d;
  // Tiny TP / Eikon-IR state.
  private creditToHost = 0;
  private rxSegs: Uint8Array[] = [];
  // Eikon-IR file-receive state machine on LSAP 8.
  private gotFileLine = false;
  private fileName = '';
  private numToRecv = 0;
  private bodyChunks: Uint8Array[] = [];
  private bodyLen = 0;
  // Result.
  received: { name: string; body: Uint8Array } | null = null;
  // Set false to simulate a device NOT in Infrared receive mode.
  listening = true;
  // Set false to make the IAS server NOT know the EikonIr class (it then
  // replies NO_SUCH_CLASS, exactly like the real device does for "OBEX").
  registersEikonIr = true;

  // Host→device bytes arrive here; we decode and react synchronously,
  // enqueuing any response into txQueue.
  feed(bytes: Uint8Array): void {
    const frames = this.decoder.feed(bytes);
    for (const f of frames) this.handleFrame(f.body);
  }

  // Device→host: drain queued response bytes.
  drain(): Uint8Array {
    if (this.txQueue.length === 0) return new Uint8Array(0);
    const out = new Uint8Array(this.txQueue);
    this.txQueue = [];
    return out;
  }

  private send(body: Uint8Array): void {
    const framed = encodeSirFrame(body);
    for (let i = 0; i < framed.length; i++) this.txQueue.push(framed[i]);
  }

  private handleFrame(body: Uint8Array): void {
    if (body.length < 2) return;
    const addr = body[0];
    const control = body[1];
    const info = body.subarray(2);
    if ((addr & 0x01) !== 0x01) return; // device only consumes commands

    if ((control & 0x01) === 0) {
      this.handleI(control, info);
      return;
    }
    if ((control & 0x03) === 0x01) {
      // Supervisory poll (RR). If we have a pending device-initiated PDU (the
      // graceful disconnect after an empty file), deliver it now; otherwise
      // just return the token with an RR final.
      if (this.pendingDisconnect) {
        const pdu = this.pendingDisconnect;
        this.pendingDisconnect = null;
        this.sendI(pdu);
      } else {
        this.sendRR();
      }
      return;
    }
    // Unnumbered.
    const u = control & ~PF;
    if (u === U_XID_CMD) {
      if (this.listening) this.sendXidResponse();
    } else if (u === U_SNRM) {
      this.vs = 0;
      this.vr = 0;
      this.sendUA();
    } else if (u === U_DISC) {
      this.sendUA();
    }
  }

  private handleI(control: number, info: Uint8Array): void {
    const ns = (control >> 1) & 0x07;
    let resp: Uint8Array | null = null;
    if (ns === this.vr) {
      this.vr = (this.vr + 1) & 0x07;
      resp = this.processLm(info);
    }
    if (resp) this.sendI(resp);
    else this.sendRR();
  }

  // Returns a response LM-PDU, or null (→ device answers with RR).
  private processLm(pdu: Uint8Array): Uint8Array | null {
    if (pdu.length < 2) return null;
    const dst = pdu[0];
    const localLsap = dst & 0x7f;
    const peerLsap = pdu[1] & 0x7f;

    if ((dst & CONTROL_BIT) !== 0) {
      // Control PDU.
      const opcode = pdu[2];
      if (opcode === 0x01) {
        // LM-Connect → confirm.
        if (localLsap === EIKONIR_LSAP) {
          // Tiny TP connect: grant the host our initial credit.
          this.creditToHost = INITIAL_CREDIT;
          return new Uint8Array([peerLsap | CONTROL_BIT, localLsap, 0x81, 0x00, INITIAL_CREDIT]);
        }
        // IAS (LSAP 0) or other: plain confirm.
        return new Uint8Array([peerLsap | CONTROL_BIT, localLsap, 0x81, 0x00]);
      }
      // Host LM-Disconnect (e.g. of the IAS channel): no reply.
      return null;
    }

    // Data PDU.
    const data = pdu.subarray(2);
    if (localLsap === 0) {
      // IAS query → reply.
      return this.iasReply(peerLsap, localLsap, data);
    }
    if (localLsap === EIKONIR_LSAP) {
      return this.ttpReceive(peerLsap, localLsap, data);
    }
    return null;
  }

  private iasReply(peerLsap: number, localLsap: number, req: Uint8Array): Uint8Array {
    // Parse the GetValueByClass request: op(0x84) classLen className attrLen
    // attrName. Match the EXACT class/attr the EPOC Eikon receiver registers;
    // anything else (e.g. "OBEX") → NO_SUCH_CLASS, like the real device.
    let p = 1;
    const classLen = req[p++];
    const className = asciiOf(req.subarray(p, p + classLen));
    p += classLen;
    const attrLen = req[p++];
    const attrName = asciiOf(req.subarray(p, p + attrLen));
    const known =
      this.registersEikonIr && className === EIKONIR_CLASS && attrName === EIKONIR_ATTR;
    if (!known) {
      // op(84) retCode(NO_SUCH_CLASS) — failure replies stop after the code.
      return new Uint8Array([peerLsap, localLsap, 0x84, IAS_RET_NO_SUCH_CLASS]);
    }
    // op(84) ret(00) listLen(0001) objId(0002) type(01 integer) value(LSAP 8).
    const reply = [0x84, IAS_RET_SUCCESS, 0x00, 0x01, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, EIKONIR_LSAP];
    return new Uint8Array([peerLsap, localLsap, ...reply]);
  }

  private ttpReceive(peerLsap: number, localLsap: number, data: Uint8Array): Uint8Array | null {
    const header = data[0];
    const more = (header & 0x80) !== 0;
    const payload = data.subarray(1);
    this.creditToHost = Math.max(0, this.creditToHost - 1);
    if (payload.length > 0) this.rxSegs.push(payload);

    const grant = INITIAL_CREDIT - this.creditToHost;
    this.creditToHost += grant;
    const grantByte = grant & 0x7f;

    if (more || payload.length === 0) {
      // Mid-SDU segment (or pure credit update) → just a credit grant.
      return new Uint8Array([peerLsap, localLsap, grantByte]);
    }
    // SDU complete.
    const sdu = concat(this.rxSegs);
    this.rxSegs = [];
    return this.processEikon(peerLsap, localLsap, sdu, grantByte);
  }

  // EPOC Eikon-IR receiver state machine on LSAP 8.
  private processEikon(
    peerLsap: number,
    localLsap: number,
    sdu: Uint8Array,
    grantByte: number,
  ): Uint8Array | null {
    if (!this.gotFileLine) {
      // First SDU must be "FILE <size> <att> <hi> <lo> <name>".
      const line = asciiOf(sdu);
      const m = line.match(/^FILE (\d+) (\d+) (\d+) (\d+) (.*)$/s);
      let ackPayload: string;
      if (m) {
        this.numToRecv = Number(m[1]);
        this.fileName = m[5];
        this.gotFileLine = true;
        ackPayload = 'ACK Y';
      } else {
        ackPayload = 'ACK N';
      }
      // The real device answers with a pure credit-update PDU, then "ACK Y"
      // in a second data PDU. Both ride back as two LM-PDUs; in this
      // synchronous mock we fold the credit into the ACK PDU (functionally
      // identical to the client, which only reads the SDU payload).
      const ackBytes = asciiBytes(ackPayload);
      const ttp = new Uint8Array(1 + ackBytes.length);
      ttp[0] = grantByte;
      ttp.set(ackBytes, 1);
      // An empty file (numToRecv 0) is complete the moment FILE is ACKed.
      if (this.gotFileLine && this.numToRecv === 0) {
        this.completeReceive();
        // Reply ACK Y first; the graceful disconnect follows on the next poll.
        this.queueDisconnect(peerLsap, localLsap);
      }
      return new Uint8Array([peerLsap, localLsap, ...ttp]);
    }

    // Body chunk.
    this.bodyChunks.push(sdu.slice());
    this.bodyLen += sdu.length;
    if (this.bodyLen >= this.numToRecv) {
      this.completeReceive();
      // Receiver saves the file then gracefully shuts the socket: emit the
      // LM-Disconnect on LSAP 8 (control PDU, opcode 0x02, reason user).
      return new Uint8Array([peerLsap | CONTROL_BIT, localLsap, 0x02, 0x01]);
    }
    // More to come → grant credit so the host keeps streaming.
    return new Uint8Array([peerLsap, localLsap, grantByte]);
  }

  private completeReceive(): void {
    this.received = { name: this.fileName, body: concat(this.bodyChunks) };
  }

  private queueDisconnect(peerLsap: number, localLsap: number): void {
    // For the empty-file case: send the disconnect as a follow-up I-frame on
    // the next host poll.
    this.pendingDisconnect = new Uint8Array([peerLsap | CONTROL_BIT, localLsap, 0x02, 0x01]);
  }
  private pendingDisconnect: Uint8Array | null = null;

  // ── IrLAP secondary frame builders (responses: C/R=0, F=1) ──
  private sendUA(): void {
    this.send(new Uint8Array([(this.connAddr << 1) | 0, U_UA | PF]));
  }
  private sendRR(): void {
    this.send(new Uint8Array([(this.connAddr << 1) | 0, (this.vr << 5) | PF | S_RR]));
  }
  private sendI(info: Uint8Array): void {
    const control = (this.vr << 5) | PF | (this.vs << 1);
    this.vs = (this.vs + 1) & 0x07;
    this.send(new Uint8Array([(this.connAddr << 1) | 0, control, ...info]));
  }
  private sendXidResponse(): void {
    const info = [
      0x01, // format
      this.deviceAddr & 0xff,
      (this.deviceAddr >>> 8) & 0xff,
      (this.deviceAddr >>> 16) & 0xff,
      (this.deviceAddr >>> 24) & 0xff,
      0xff, 0xff, 0xff, 0xff, // dest (the discovering host)
      0x00, // flags
      0x00, // slot
      0x00, // version
      0x00, // hint (discovery info starts here, AFTER the version byte)
      0x00, // charset ASCII
    ];
    for (const c of 'MockPsion') info.push(c.charCodeAt(0));
    this.send(new Uint8Array([(0x7f << 1) | 0, U_XID_RESP | PF, ...info]));
  }
}

function asciiOf(b: Uint8Array): string {
  let s = '';
  for (let i = 0; i < b.length; i++) s += String.fromCharCode(b[i]);
  return s;
}
function asciiBytes(s: string): Uint8Array {
  const out = new Uint8Array(s.length);
  for (let i = 0; i < s.length; i++) out[i] = s.charCodeAt(i) & 0xff;
  return out;
}

// ── Wire a client to a mock device over two byte queues ────────────────
function makeClientFor(device: MockDevice, cfg = {}): IrSendClient {
  const writeBytes = (data: Uint8Array): number => {
    device.feed(data);
    return data.length;
  };
  const readBytes = (): Uint8Array => device.drain();
  return new IrSendClient(readBytes, writeBytes, {
    pollHz: 1000,
    discoveryTimeoutMs: 250,
    requestTimeoutMs: 4000,
    ...cfg,
  });
}

async function main() {
  // 1. Happy path: a 500-byte file beams into the device's Eikon-IR inbox.
  {
    const device = new MockDevice();
    const client = makeClientFor(device);
    client.start();
    const payload = new Uint8Array(500).map((_, i) => (i * 7) & 0xff);
    try {
      await client.discoverAndConnect();
      await client.sendFile('Memo.txt', payload);
    } finally {
      client.stop();
    }
    check(device.received !== null, 'device received a file');
    if (device.received) {
      check(device.received.name === 'Memo.txt', `file name = "${device.received.name}" (want Memo.txt)`);
      check(device.received.body.length === payload.length, `file length ${device.received.body.length} (want ${payload.length})`);
      let same = device.received.body.length === payload.length;
      for (let i = 0; same && i < payload.length; i++) same = device.received.body[i] === payload[i];
      check(same, 'received bytes match the sent bytes');
    }
  }

  // 2. Empty file still delivers.
  {
    const device = new MockDevice();
    const client = makeClientFor(device);
    client.start();
    try {
      await client.discoverAndConnect();
      await client.sendFile('Empty.dat', new Uint8Array(0));
    } finally {
      client.stop();
    }
    check(device.received !== null && device.received.body.length === 0, 'empty file delivered with zero-length body');
    check(device.received?.name === 'Empty.dat', 'empty file name preserved');
  }

  // 3. Device not in receive mode → IrNotListeningError.
  {
    const device = new MockDevice();
    device.listening = false;
    const client = makeClientFor(device, { discoveryTimeoutMs: 150 });
    client.start();
    let err: unknown = null;
    try {
      await client.discoverAndConnect();
    } catch (e) {
      err = e;
    } finally {
      client.stop();
    }
    check(err instanceof IrNotListeningError, `silent device throws IrNotListeningError (got ${err})`);
  }

  // 4. IAS NO_SUCH_CLASS: a device that does not register the EikonIr class
  //    (mirrors the real device's reply to the old "OBEX" query) → sendFile
  //    fails with an IAS error, and nothing is "received". This guards against
  //    regressing to a wrong class/attribute: the mock fails exactly when the
  //    real device would.
  {
    const device = new MockDevice();
    device.registersEikonIr = false;
    const client = makeClientFor(device);
    client.start();
    let err: unknown = null;
    try {
      await client.discoverAndConnect();
      await client.sendFile('Nope.txt', new Uint8Array([1, 2, 3]));
    } catch (e) {
      err = e;
    } finally {
      client.stop();
    }
    check(err instanceof Error && /ret 1|IAS|NO_SUCH/i.test((err as Error).message),
      `unregistered class surfaces IAS failure (got ${err})`);
    check(device.received === null, 'no file received when IAS class is unknown');
  }
}

main()
  .then(() => {
    if (failures > 0) {
      console.error(`\n${failures} test failure(s)`);
      process.exit(1);
    }
    console.log('OK — all IrDA end-to-end tests passed');
    process.exit(0);
  })
  .catch((e) => {
    console.error('FAIL: unexpected error', e);
    process.exit(1);
  });
