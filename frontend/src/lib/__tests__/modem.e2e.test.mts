// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// End-to-end test of the modem path: drives a synthetic EPOC peer
// against the real ModemClient. No mocking of internal modules — only
// the WASM SerialBridge is stubbed (which is unavoidable, since the
// modem code is host-side TypeScript and the only thing on the other
// side of UART2 in production is the C++ emulator).
//
// What the synthetic peer simulates:
//   1. AT-mode dial: ATZ then ATDT0
//   2. PPP LCP option negotiation (initiated by peer, completed by us)
//   3. PAP authenticate
//   4. PPP IPCP option negotiation (initiated by peer, completed by us)
//   5. DNS A-query for canned.psion.local
//   6. TCP/80 handshake → HTTP GET → response decode → FIN
//   7. SMTP send round-trip
//   8. POP3 retrieve round-trip
//
// Each step asserts on what the modem actually sent on the wire.
// The test exercises the *real* AtCommandParser, ModeDispatcher, HDLC
// framer, LCP / PAP / IPCP FSMs, IPv4 demux, TCP state machine, and
// HTTP/SMTP/POP3 servers, all wired together exactly as in production.
//
// Run via:
//   node --experimental-strip-types \
//       frontend/src/lib/__tests__/modem.e2e.test.mts

import { ModemClient } from '../modem/client.ts';
import type { SerialBridge } from '../modem/port.ts';
import { encodeFrame, HdlcDecoder } from '../ppp/hdlc.ts';
import { buildPpp, parsePpp, PROTO_LCP, PROTO_PAP, PROTO_IPCP, PROTO_IP } from '../ppp/demux.ts';
import { buildIpv4, parseIpv4, onesComplementChecksum } from '../inet/ip.ts';
import { buildUdp, parseUdp, pseudoChecksum } from '../inet/udp.ts';
import { Mailbox, MemoryBackend } from '../inet/mailbox.ts';

// ── Bridge stub — pure FIFOs, no WASM ───────────────────────────────

class MockBridge implements SerialBridge {
  // Bytes the device sent that the host hasn't read yet:
  private hostRxQueue: Uint8Array = new Uint8Array(0);
  // Bytes the host wrote that the device hasn't read yet:
  private hostTxQueue: Uint8Array = new Uint8Array(0);
  private attached: boolean = false;

  serialAttachHost(_u: number): boolean { this.attached = true; return true; }
  serialDetachHost(_u: number): boolean { this.attached = false; return true; }
  serialIsAttached(_u: number): boolean { return this.attached; }
  serialReadBytes(_u: number): Uint8Array {
    const out = this.hostRxQueue;
    this.hostRxQueue = new Uint8Array(0);
    return out;
  }
  serialWriteBytes(_u: number, data: Uint8Array): number {
    const out = new Uint8Array(this.hostTxQueue.length + data.length);
    out.set(this.hostTxQueue, 0); out.set(data, this.hostTxQueue.length);
    this.hostTxQueue = out;
    return data.length;
  }

  // Test driver: "device sent these bytes to the host".
  deviceSends(bytes: Uint8Array): void {
    const out = new Uint8Array(this.hostRxQueue.length + bytes.length);
    out.set(this.hostRxQueue, 0); out.set(bytes, this.hostRxQueue.length);
    this.hostRxQueue = out;
  }
  // Test driver: drain everything the host wrote (so the device can see it).
  drainHostBytes(): Uint8Array {
    const out = this.hostTxQueue;
    this.hostTxQueue = new Uint8Array(0);
    return out;
  }
}

// ── Reporting helpers ───────────────────────────────────────────────

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
  else       { console.log(`  PASS  ${msg}`); }
}
function bytesToStr(b: Uint8Array): string {
  let s = ''; for (let i = 0; i < b.length; i++) s += String.fromCharCode(b[i]); return s;
}
function strToBytes(s: string): Uint8Array {
  const out = new Uint8Array(s.length);
  for (let i = 0; i < s.length; i++) out[i] = s.charCodeAt(i) & 0xFF;
  return out;
}

// Pump the modem N times so any in-flight work (transport poll +
// reaction to received bytes) completes.
function pump(client: ModemClient, n = 4): void {
  for (let i = 0; i < n; i++) client.pumpForTest();
}

// ── Main test ───────────────────────────────────────────────────────

const bridge = new MockBridge();
const mailbox = new Mailbox(new MemoryBackend());
await mailbox.init();

let phaseHistory: string[] = [];
const client = new ModemClient({
  uartIndex: 2,
  bridge,
  mailbox,
  fetchImpl: async (url: string) => {
    // Real network from a test would be flaky; redirect any "real
    // host" fetch to a synthetic 200 response.
    void url;
    return new Response('hello from the synthetic fetch', {
      status: 200,
      headers: { 'content-type': 'text/plain' },
    });
  },
  onPhaseChange: p => phaseHistory.push(p),
});

const startOk = client.start();
check(startOk, 'ModemClient.start() returns true');
check(bridge.serialIsAttached(2), 'bridge is attached after start');
check(phaseHistory.includes('command'), 'phase reached "command" on start');

// ── 1. AT layer: ATZ → OK, ATDT → CONNECT 115200 + enter online ────

// One persistent HDLC decoder + text accumulator. flush() pumps the
// modem until quiescent, accumulating every emitted byte into both
// the decoder (for HDLC frames) and the text buffer (for AT-mode
// response matching). The same byte stream legitimately contains
// both AT text and HDLC frames around the CONNECT transition, so
// draining for one mustn't starve the other.
const dec = new HdlcDecoder();
let textAccum = '';
const frameAccum: Uint8Array[] = [];
function flush(maxIters = 32): Uint8Array[] {
  const before = frameAccum.length;
  for (let i = 0; i < maxIters; i++) {
    pump(client, 1);
    const bytes = bridge.drainHostBytes();
    if (bytes.length === 0) {
      if (i > 0) break;
      continue;
    }
    for (let j = 0; j < bytes.length; j++) textAccum += String.fromCharCode(bytes[j]);
    for (const f of dec.feed(bytes)) frameAccum.push(f);
  }
  return frameAccum.slice(before);
}

bridge.deviceSends(strToBytes('ATZ\r'));
flush();
check(textAccum.includes('ATZ'), `ATZ echoed (textAccum head: "${textAccum.slice(0, 40).replace(/\r/g, '\\r').replace(/\n/g, '\\n')}")`);
check(textAccum.includes('OK'),  'ATZ → OK');

const textBeforeDial = textAccum.length;
bridge.deviceSends(strToBytes('ATDT0\r'));
const newFramesAfterDial = flush();
const textAfterDial = textAccum.slice(textBeforeDial);
check(textAfterDial.includes('CONNECT'),
      `ATDT → CONNECT (got "${textAfterDial.slice(0, 40).replace(/\r/g, '\\r').replace(/\n/g, '\\n')}")`);
check(phaseHistory.includes('lcp'), 'phase advanced to "lcp" after ATDT');

// ── 2. PPP: drain the modem's initial LCP Configure-Request, then
//    send our own and ACK theirs. ───────────────────────────────────

check(newFramesAfterDial.length >= 1, `modem emitted >=1 LCP frame post-CONNECT (got ${newFramesAfterDial.length})`);
let modemLcpReqId: number | null = null;
for (const raw of newFramesAfterDial) {
  const f = parsePpp(raw);
  if (f && f.protocol === PROTO_LCP && f.info[0] === 1 /* Configure-Request */) {
    modemLcpReqId = f.info[1];
  }
}
check(modemLcpReqId !== null, 'modem sent LCP Configure-Request');

// Synthetic peer sends its own LCP Configure-Request (MRU 1500 +
// auth-protocol CHAP-MD5 — we expect the modem to nak this and counter
// with PAP).
function sendPpp(protocol: number, info: Uint8Array): void {
  bridge.deviceSends(encodeFrame(buildPpp(protocol, info)));
}
const peerLcpReq = new Uint8Array([
  1, 0x42, 0x00, 0x0F,
  1, 4, 0x05, 0xDC,                        // MRU = 1500
  3, 5, 0xC2, 0x23, 0x05,                  // Auth: CHAP-MD5
]);
sendPpp(PROTO_LCP, peerLcpReq);
const frames2 = flush();
let sawNak = false;
let sawAck = false;
for (const raw of frames2) {
  const f = parsePpp(raw);
  if (f && f.protocol === PROTO_LCP) {
    if (f.info[0] === 3) sawNak = true;  // Configure-Nak
    if (f.info[0] === 2) sawAck = true;  // Configure-Ack
  }
}
check(sawNak, 'modem Nakked our CHAP proposal');
// Send a corrected request — same MRU, but now PAP.
const peerLcpReq2 = new Uint8Array([
  1, 0x43, 0x00, 0x0E,
  1, 4, 0x05, 0xDC,
  3, 4, 0xC0, 0x23,                        // Auth: PAP
]);
sendPpp(PROTO_LCP, peerLcpReq2);
const frames3 = flush();
for (const raw of frames3) {
  const f = parsePpp(raw);
  if (f && f.protocol === PROTO_LCP && f.info[0] === 2) sawAck = true;
}
check(sawAck, 'modem Acked our PAP-only LCP req');

// Reply to the modem's Configure-Request with our Configure-Ack.
check(modemLcpReqId !== null, '(precondition) modem-LCP-req id captured before sending our Ack');
if (modemLcpReqId !== null) {
  const ackInfo = new Uint8Array([2, modemLcpReqId, 0x00, 0x0A,
                                  5, 6, 0xDE, 0xAD, 0xBE, 0xEF]);  // dummy magic
  sendPpp(PROTO_LCP, ackInfo);
  flush();
}
check(phaseHistory.includes('authenticating'), `phase advanced to authenticating (history: ${phaseHistory.join(',')})`);

// ── 3. PAP: send Auth-Request, expect Auth-Ack ─────────────────────

const papReq = new Uint8Array([
  1, 0x10, 0x00, 0x09,
  3, 'a'.charCodeAt(0), 'l'.charCodeAt(0), 'i',
  // simpler: empty fields
].length === 9 ? new Uint8Array(0) : new Uint8Array(0));
// Build a clean PAP req: peer-id "ali" (3 bytes) + password "p" (1 byte).
const papPeerId = strToBytes('ali');
const papPassword = strToBytes('p');
const papLen = 4 + 1 + papPeerId.length + 1 + papPassword.length;
const papInfo = new Uint8Array(papLen);
papInfo[0] = 1;             // Auth-Request
papInfo[1] = 0x77;          // id
papInfo[2] = (papLen >>> 8) & 0xFF;
papInfo[3] = papLen & 0xFF;
papInfo[4] = papPeerId.length;
papInfo.set(papPeerId, 5);
papInfo[5 + papPeerId.length] = papPassword.length;
papInfo.set(papPassword, 6 + papPeerId.length);
void papReq;
sendPpp(PROTO_PAP, papInfo);
const papFrames = flush();
let sawPapAck = false;
let modemIpcpReqId: number | null = null;
for (const raw of papFrames) {
  const f = parsePpp(raw);
  if (!f) continue;
  if (f.protocol === PROTO_PAP && f.info[0] === 2 && f.info[1] === 0x77) sawPapAck = true;
  // PAP success triggers startIpcp() which emits the IPCP CR in the
  // same pump cycle — capture it here rather than in a second drain.
  if (f.protocol === PROTO_IPCP && f.info[0] === 1) modemIpcpReqId = f.info[1];
}
check(sawPapAck, 'modem replied PAP Auth-Ack with matching id');
check(phaseHistory.includes('ipcp'), 'phase advanced to ipcp after PAP success');

// ── 4. IPCP: send our request to elicit modem's NAK; ack their CR ──

check(modemIpcpReqId !== null, 'modem sent IPCP Configure-Request');

// Send our IPCP request asking for 0.0.0.0 (so we'd get nak'd back
// with 192.168.42.2) plus DNS-Primary request.
const peerIpcpReq = new Uint8Array([
  1, 0x20, 0x00, 0x10,
  3, 6, 0, 0, 0, 0,                  // IP-Address = 0.0.0.0
  0x81, 6, 0, 0, 0, 0,               // DNS-Primary
]);
sendPpp(PROTO_IPCP, peerIpcpReq);
const frames6 = flush();
let assignedIp: number[] | null = null;
let assignedDns: number[] | null = null;
for (const raw of frames6) {
  const f = parsePpp(raw);
  if (f && f.protocol === PROTO_IPCP && f.info[0] === 3 /* Nak */) {
    // Walk options.
    let i = 4;
    while (i + 1 < f.info.length) {
      const t = f.info[i], l = f.info[i + 1];
      if (t === 3 && l === 6)        assignedIp  = [f.info[i + 2], f.info[i + 3], f.info[i + 4], f.info[i + 5]];
      if (t === 0x81 && l === 6)     assignedDns = [f.info[i + 2], f.info[i + 3], f.info[i + 4], f.info[i + 5]];
      i += l;
    }
  }
}
check(assignedIp !== null && assignedIp![0] === 192 && assignedIp![3] === 2,
      `IPCP NAK assigned 192.168.42.2 (got ${assignedIp?.join('.')})`);
check(assignedDns !== null && assignedDns![3] === 1,
      `IPCP NAK assigned DNS 192.168.42.1 (got ${assignedDns?.join('.')})`);

// Ack the modem's IPCP request to complete the FSM.
if (modemIpcpReqId !== null) {
  const ipcpAck = new Uint8Array([2, modemIpcpReqId, 0x00, 0x0A,
                                  3, 6, 192, 168, 42, 1]);   // ack host's request for .1
  sendPpp(PROTO_IPCP, ipcpAck);
  flush();
}
// Also send our final IPCP request now matching what they want (so
// we move past 'req-sent').
const peerIpcpReq2 = new Uint8Array([
  1, 0x21, 0x00, 0x10,
  3, 6, 192, 168, 42, 2,
  0x81, 6, 192, 168, 42, 1,
]);
sendPpp(PROTO_IPCP, peerIpcpReq2);
flush();
check(phaseHistory.includes('online'), `phase reached 'online' (history: ${phaseHistory.join(',')})`);

// ── 5. DNS query for canned.psion.local → 192.168.42.10 ────────────

function sendIp(payload: Uint8Array): void {
  bridge.deviceSends(encodeFrame(buildPpp(PROTO_IP, payload)));
}
function extractIp(frame: Uint8Array): { proto: number; src: Uint8Array; dst: Uint8Array; payload: Uint8Array } | null {
  const f = parsePpp(frame);
  if (!f || f.protocol !== PROTO_IP) return null;
  const pkt = parseIpv4(f.info);
  if (!pkt) return null;
  return { proto: pkt.header.protocol, src: pkt.header.src, dst: pkt.header.dst, payload: pkt.payload };
}

// Build a DNS query.
const dnsId = 0xBEEF;
const qname = 'canned.psion.local';
const labels = qname.split('.');
let qLen = 0;
for (const l of labels) qLen += 1 + l.length;
qLen += 1;
const dnsQuery = new Uint8Array(12 + qLen + 4);
dnsQuery[0] = (dnsId >>> 8) & 0xFF; dnsQuery[1] = dnsId & 0xFF;
dnsQuery[2] = 0x01; dnsQuery[3] = 0x00;
dnsQuery[4] = 0; dnsQuery[5] = 1;
let qp = 12;
for (const l of labels) {
  dnsQuery[qp++] = l.length;
  for (let i = 0; i < l.length; i++) dnsQuery[qp++] = l.charCodeAt(i);
}
dnsQuery[qp++] = 0;
dnsQuery[qp++] = 0; dnsQuery[qp++] = 1;
dnsQuery[qp++] = 0; dnsQuery[qp++] = 1;

const deviceIp = new Uint8Array([192, 168, 42, 2]);
const hostIp   = new Uint8Array([192, 168, 42, 1]);
const udpSeg = buildUdp(deviceIp, hostIp, { srcPort: 53000, dstPort: 53, payload: dnsQuery });
sendIp(buildIpv4(17 /* UDP */, deviceIp, hostIp, udpSeg));
pump(client, 6);

const dnsFrames = dec.feed(bridge.drainHostBytes());
let dnsReply: Uint8Array | null = null;
for (const raw of dnsFrames) {
  const ip = extractIp(raw);
  if (ip && ip.proto === 17) {
    const udp = parseUdp(ip.payload);
    if (udp && udp.srcPort === 53) dnsReply = udp.payload;
  }
}
check(dnsReply !== null, 'received DNS reply');
if (dnsReply) {
  const lastIp = dnsReply.subarray(dnsReply.length - 4);
  check(lastIp[0] === 192 && lastIp[1] === 168 && lastIp[2] === 42 && lastIp[3] === 10,
        `DNS A-record = 192.168.42.10 (got ${lastIp.join('.')})`);
}

// ── 6. TCP/80 handshake → HTTP GET → response ──────────────────────

const httpHost = new Uint8Array([192, 168, 42, 10]);

// Helper to build a TCP segment with correct pseudo-header checksum.
function buildTcpSeg(srcPort: number, dstPort: number, seq: number, ack: number,
                     flags: number, payload: Uint8Array,
                     srcIp: Uint8Array, dstIp: Uint8Array): Uint8Array {
  const seg = new Uint8Array(20 + payload.length);
  seg[0] = (srcPort >>> 8) & 0xFF; seg[1] = srcPort & 0xFF;
  seg[2] = (dstPort >>> 8) & 0xFF; seg[3] = dstPort & 0xFF;
  seg[4] = (seq >>> 24) & 0xFF; seg[5] = (seq >>> 16) & 0xFF;
  seg[6] = (seq >>> 8) & 0xFF;  seg[7] = seq & 0xFF;
  seg[8] = (ack >>> 24) & 0xFF; seg[9] = (ack >>> 16) & 0xFF;
  seg[10] = (ack >>> 8) & 0xFF; seg[11] = ack & 0xFF;
  seg[12] = (5 << 4); seg[13] = flags;
  seg[14] = 0x40; seg[15] = 0x00;
  seg.set(payload, 20);
  const csum = pseudoChecksum(srcIp, dstIp, 6, seg);
  seg[16] = (csum >>> 8) & 0xFF;
  seg[17] = csum & 0xFF;
  return seg;
}
function parseTcp(payload: Uint8Array): { srcPort: number; dstPort: number; seq: number; ack: number; flags: number; data: Uint8Array } {
  const off = (payload[12] >>> 4) & 0xF;
  return {
    srcPort: (payload[0] << 8) | payload[1],
    dstPort: (payload[2] << 8) | payload[3],
    seq:   ((payload[4] << 24) | (payload[5] << 16) | (payload[6] << 8) | payload[7]) >>> 0,
    ack:   ((payload[8] << 24) | (payload[9] << 16) | (payload[10] << 8) | payload[11]) >>> 0,
    flags: payload[13],
    data:  payload.subarray(off * 4),
  };
}

let cseq = 0x80000000;
sendIp(buildIpv4(6, deviceIp, httpHost,
  buildTcpSeg(12345, 80, cseq, 0, 0x02 /* SYN */, new Uint8Array(0), deviceIp, httpHost)));
pump(client, 4);
const synAckFrames = dec.feed(bridge.drainHostBytes());
let synAck: ReturnType<typeof parseTcp> | null = null;
for (const raw of synAckFrames) {
  const ip = extractIp(raw);
  if (ip && ip.proto === 6) synAck = parseTcp(ip.payload);
}
check(synAck !== null && (synAck!.flags & 0x12) === 0x12, 'received SYN+ACK from modem');
const serverSeq = synAck!.seq;
cseq = (cseq + 1) >>> 0;
sendIp(buildIpv4(6, deviceIp, httpHost,
  buildTcpSeg(12345, 80, cseq, (serverSeq + 1) >>> 0, 0x10, new Uint8Array(0), deviceIp, httpHost)));
pump(client, 4);
bridge.drainHostBytes();    // drain anything pending

// Send HTTP GET for canned home.
const httpReq = strToBytes('GET / HTTP/1.0\r\nHost: canned.psion.local\r\n\r\n');
sendIp(buildIpv4(6, deviceIp, httpHost,
  buildTcpSeg(12345, 80, cseq, (serverSeq + 1) >>> 0, 0x18 /* PSH+ACK */, httpReq, deviceIp, httpHost)));
pump(client, 8);
// HTTP proxy is async — let microtasks drain.
await new Promise(r => setTimeout(r, 50));
pump(client, 8);

const httpFrames = dec.feed(bridge.drainHostBytes());
let httpResp = '';
let httpFin = false;
for (const raw of httpFrames) {
  const ip = extractIp(raw);
  if (!ip || ip.proto !== 6) continue;
  const seg = parseTcp(ip.payload);
  if (seg.data.length > 0) httpResp += bytesToStr(seg.data);
  if ((seg.flags & 0x01) !== 0) httpFin = true;
}
check(httpResp.startsWith('HTTP/1.0 200 OK'), `HTTP 200 received (got ${httpResp.slice(0, 30)})`);
check(httpResp.includes('Welcome to the simulated internet'), 'curated home page text in response');
check(httpFin, 'server sent FIN after HTTP response');

// ── 7. SMTP send through full PPP stack ────────────────────────────

// New connection on port 25.
const smtpClientPort = 23456;
let scseq = 0x90000000;
sendIp(buildIpv4(6, deviceIp, httpHost,
  buildTcpSeg(smtpClientPort, 25, scseq, 0, 0x02, new Uint8Array(0), deviceIp, httpHost)));
pump(client, 4);
let smtpSynAck: ReturnType<typeof parseTcp> | null = null;
for (const raw of dec.feed(bridge.drainHostBytes())) {
  const ip = extractIp(raw);
  if (ip && ip.proto === 6) smtpSynAck = parseTcp(ip.payload);
}
check(smtpSynAck !== null && (smtpSynAck!.flags & 0x12) === 0x12, 'SMTP SYN+ACK received');
const smtpServerSeq = smtpSynAck!.seq;
scseq = (scseq + 1) >>> 0;
sendIp(buildIpv4(6, deviceIp, httpHost,
  buildTcpSeg(smtpClientPort, 25, scseq, (smtpServerSeq + 1) >>> 0, 0x10, new Uint8Array(0), deviceIp, httpHost)));
pump(client, 4);
// Walk the SMTP dialogue. We'll be lazy with seq numbers — re-ack
// whatever the server tells us each round.
let smtpServerNext = (smtpServerSeq + 1) >>> 0;
let smtpReceived = '';
function smtpSend(s: string): void {
  const data = strToBytes(s);
  sendIp(buildIpv4(6, deviceIp, httpHost,
    buildTcpSeg(smtpClientPort, 25, scseq, smtpServerNext, 0x18, data, deviceIp, httpHost)));
  scseq = (scseq + data.length) >>> 0;
}
function smtpDrain(): void {
  pump(client, 4);
  for (const raw of dec.feed(bridge.drainHostBytes())) {
    const ip = extractIp(raw);
    if (!ip || ip.proto !== 6) continue;
    const seg = parseTcp(ip.payload);
    if (seg.data.length > 0) smtpReceived += bytesToStr(seg.data);
    const end = (seg.seq + seg.data.length + (((seg.flags & 0x02) !== 0 || (seg.flags & 0x01) !== 0) ? 1 : 0)) >>> 0;
    if (((end - smtpServerNext) | 0) > 0) smtpServerNext = end;
  }
}
smtpDrain();          // collect the 220 greeting
smtpSend('EHLO test\r\n');                                        smtpDrain();
smtpSend('MAIL FROM:<alice@example.com>\r\n');                    smtpDrain();
smtpSend('RCPT TO:<bob@example.com>\r\n');                        smtpDrain();
smtpSend('DATA\r\n');                                             smtpDrain();
smtpSend('From: alice@example.com\r\nTo: bob@example.com\r\nSubject: Greetings\r\n\r\nHello over PPP.\r\n.\r\n');  smtpDrain();
smtpSend('QUIT\r\n');                                             smtpDrain();
check(smtpReceived.startsWith('220 mail.psion.local'), `SMTP greeting (got ${smtpReceived.slice(0, 30)})`);
check(smtpReceived.includes('250 OK message queued'), 'SMTP message queued');
const captured = mailbox.listOutbox();
check(captured.length === 1, `outbox count = 1 (got ${captured.length})`);
if (captured.length === 1) {
  check(captured[0].subject === 'Greetings', `outbox subject = "Greetings"`);
  check(captured[0].raw.includes('Hello over PPP.'), 'outbox body preserved through PPP');
}

// ── 8. POP3 retrieve through full PPP stack ────────────────────────

// Seed an inbox message.
mailbox.appendInbox({
  from: 'carol@example.com',
  to: ['user@psion.local'],
  subject: 'You\'ve got mail',
  raw: 'From: carol@example.com\r\nTo: user@psion.local\r\nSubject: You\'ve got mail\r\n\r\nBody here.\r\n',
});

const popClientPort = 23457;
let pseq = 0xA0000000;
sendIp(buildIpv4(6, deviceIp, httpHost,
  buildTcpSeg(popClientPort, 110, pseq, 0, 0x02, new Uint8Array(0), deviceIp, httpHost)));
pump(client, 4);
let popSynAck: ReturnType<typeof parseTcp> | null = null;
for (const raw of dec.feed(bridge.drainHostBytes())) {
  const ip = extractIp(raw);
  if (ip && ip.proto === 6) popSynAck = parseTcp(ip.payload);
}
check(popSynAck !== null, 'POP3 SYN+ACK received');
const popServerSeq = popSynAck!.seq;
pseq = (pseq + 1) >>> 0;
sendIp(buildIpv4(6, deviceIp, httpHost,
  buildTcpSeg(popClientPort, 110, pseq, (popServerSeq + 1) >>> 0, 0x10, new Uint8Array(0), deviceIp, httpHost)));
pump(client, 4);
let popServerNext = (popServerSeq + 1) >>> 0;
let popReceived = '';
function popSend(s: string): void {
  const data = strToBytes(s);
  sendIp(buildIpv4(6, deviceIp, httpHost,
    buildTcpSeg(popClientPort, 110, pseq, popServerNext, 0x18, data, deviceIp, httpHost)));
  pseq = (pseq + data.length) >>> 0;
}
function popDrain(): void {
  pump(client, 4);
  for (const raw of dec.feed(bridge.drainHostBytes())) {
    const ip = extractIp(raw);
    if (!ip || ip.proto !== 6) continue;
    const seg = parseTcp(ip.payload);
    if (seg.data.length > 0) popReceived += bytesToStr(seg.data);
    const end = (seg.seq + seg.data.length + (((seg.flags & 0x02) !== 0 || (seg.flags & 0x01) !== 0) ? 1 : 0)) >>> 0;
    if (((end - popServerNext) | 0) > 0) popServerNext = end;
  }
}
popDrain();                                  // collect +OK greeting
popSend('USER alice\r\n');                   popDrain();
popSend('PASS x\r\n');                       popDrain();
popSend('RETR 1\r\n');                       popDrain();
popSend('DELE 1\r\n');                       popDrain();
popSend('QUIT\r\n');                         popDrain();
check(popReceived.startsWith('+OK psion-emu POP3'), `POP3 greeting (got ${popReceived.slice(0, 30)})`);
check(popReceived.includes('Body here.'), 'POP3 RETR delivered body');
check(mailbox.listInbox().length === 0, 'inbox empty after DELE+QUIT');

// Use onesComplementChecksum so the import isn't dead-stripped.
void onesComplementChecksum;

client.stop();

if (failures > 0) {
  console.error(`\n${failures} test failure(s)`);
  process.exit(1);
}
console.log('\nOK — modem e2e (synthetic EPOC peer) passed');
