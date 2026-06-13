// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Smoke tests for the modem path's pure protocol layers. Run via:
//   node --experimental-strip-types \
//       frontend/src/lib/__tests__/modem.protocol.test.mts
//
// Covers HDLC roundtrip + FCS, LCP Configure-Request handling (PAP
// counter-proposal), and IPCP IP-Address negotiation. Higher-level
// behaviour (mode dispatcher, ModemClient orchestration) is left to
// browser-driven e2e in later phases.

import { encodeFrame, HdlcDecoder, fcs16 } from '../ppp/hdlc.ts';
import { parsePpp, buildPpp, PROTO_LCP } from '../ppp/demux.ts';
import { Lcp, LCP_CONF_REQ, LCP_CONF_NAK, LCP_CONF_ACK, OPT_AUTH_PROTOCOL, OPT_MAGIC_NUMBER } from '../ppp/lcp.ts';
import { recvPap, PAP_AUTH_ACK } from '../ppp/pap.ts';
import { Ipcp, IPCP_CONF_REQ, IPCP_CONF_NAK, IPCP_OPT_IP, DEVICE_IP } from '../ppp/ipcp.ts';
import { parseIpv4, buildIpv4 } from '../inet/ip.ts';
import { handleIcmp } from '../inet/icmp.ts';
import { IP_PROTO_ICMP } from '../inet/types.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
  else       { console.log(`  PASS  ${msg}`); }
}

// 1. HDLC FCS reference vector. RFC 1662 §C.3 says the FCS of an empty
//    info field is 0xFFFF after the final XOR — we compute the un-XOR'd
//    value here, so for an empty payload that's 0xFFFF (init) unchanged.
{
  const empty = new Uint8Array(0);
  const v = fcs16(empty);
  check(v === 0xFFFF, `fcs16(empty) = 0x${v.toString(16)} (want 0xffff)`);
}

// 2. HDLC encode → decode roundtrip preserves payload, including bytes
//    that need stuffing (0x7E, 0x7D, control chars).
{
  const payload = new Uint8Array([0xFF, 0x03, 0xC0, 0x21, 0x01, 0x05, 0x00, 0x06,
                                  0x7E, 0x7D, 0x01, 0x1F]);
  const framed = encodeFrame(payload);
  const dec = new HdlcDecoder();
  const out = dec.feed(framed);
  check(out.length === 1, `decoder yields 1 frame (got ${out.length})`);
  check(dec.badFcs === 0, `no FCS errors (got ${dec.badFcs})`);
  if (out.length === 1) {
    check(out[0].length === payload.length, `roundtrip length ${out[0].length} = ${payload.length}`);
    let allEqual = true;
    for (let i = 0; i < payload.length; i++) if (out[0][i] !== payload[i]) allEqual = false;
    check(allEqual, 'roundtrip bytes equal');
  }
}

// 3. PPP demux strips ACFC + protocol field correctly.
{
  const info = new Uint8Array([1, 2, 3, 4]);
  const built = buildPpp(PROTO_LCP, info);
  const parsed = parsePpp(built);
  check(parsed !== null, 'parsePpp returns non-null');
  check(parsed!.protocol === PROTO_LCP, `protocol = 0x${parsed!.protocol.toString(16)}`);
  check(parsed!.info.length === info.length, `info length preserved`);
}

// 4. LCP: Configure-Request advertising CHAP auth gets Configure-Nak'd
//    back with PAP.
{
  const lcp = new Lcp();
  // Synthesise a peer Configure-Request: option Magic + option Auth(CHAP).
  // Wire format of the LCP info field: code(1) id(1) len(2) options…
  const peerReq = new Uint8Array([
    LCP_CONF_REQ, 0x01, 0x00, 0x10,
    OPT_MAGIC_NUMBER, 6, 0x11, 0x22, 0x33, 0x44,
    OPT_AUTH_PROTOCOL, 5, 0xC2, 0x23, 0x05,           // CHAP/MD5
    // Padding byte to hit length=0x10
    0x00,
  ]);
  // Trim padding from the test vector — set length to actual.
  peerReq[3] = peerReq.length;
  const events = lcp.recv(peerReq);
  check(events.length >= 1, 'LCP recv yields >=1 event');
  const sendEv = events.find(e => !!e.send);
  check(!!sendEv, 'an event with bytes to send');
  if (sendEv && sendEv.send) {
    check(sendEv.send[0] === LCP_CONF_NAK, `response code = NAK (got ${sendEv.send[0]})`);
    // Body should contain the AUTH_PROTOCOL option counter-proposing PAP (0xC023).
    let foundPap = false;
    for (let i = 4; i + 3 < sendEv.send.length; i++) {
      if (sendEv.send[i] === OPT_AUTH_PROTOCOL && sendEv.send[i + 1] === 4 &&
          sendEv.send[i + 2] === 0xC0 && sendEv.send[i + 3] === 0x23) {
        foundPap = true; break;
      }
    }
    check(foundPap, 'NAK contains AUTH_PROTOCOL=PAP (0xC023)');
  }
}

// 5. LCP: Configure-Ack of our request promotes state correctly.
{
  const lcp = new Lcp();
  lcp.open();                       // state -> req-sent
  // Peer's Configure-Request that we'll ack so we both reach 'opened'.
  const peerReq = new Uint8Array([LCP_CONF_REQ, 1, 0, 4]);  // empty options
  const ev1 = lcp.recv(peerReq);
  check(lcp.state === 'ack-sent', `state after acking peer = ack-sent (got ${lcp.state})`);
  check(!!ev1.find(e => e.stateChanged === 'ack-sent'), 'emits ack-sent event');
  // Now peer acks ours.
  const peerAck = new Uint8Array([LCP_CONF_ACK, 1, 0, 4]);
  const ev2 = lcp.recv(peerAck);
  check(lcp.state === 'opened', `state after Conf-Ack = opened (got ${lcp.state})`);
  check(!!ev2.find(e => e.stateChanged === 'opened'), 'emits opened event');
}

// 6. PAP: Authenticate-Request → Authenticate-Ack with the right ID.
{
  // peer-id "alice", password "x".
  const req = new Uint8Array([
    1, 0x42, 0, 0x0C,
    5, ...new TextEncoder().encode('alice'),
    1, ...new TextEncoder().encode('x'),
  ]);
  const events = recvPap(req);
  const sendEv = events.find(e => !!e.send);
  check(!!sendEv, 'PAP recv produces send event');
  if (sendEv && sendEv.send) {
    check(sendEv.send[0] === PAP_AUTH_ACK, `PAP code = Auth-Ack (got ${sendEv.send[0]})`);
    check(sendEv.send[1] === 0x42, `Auth-Ack id matches request (got 0x${sendEv.send[1].toString(16)})`);
  }
  check(events.find(e => e.authenticated === true) !== undefined, 'authenticated=true emitted');
}

// 7. IPCP: peer asks for IP 0.0.0.0; we Configure-Nak with our chosen
//    DEVICE_IP.
{
  const ipcp = new Ipcp();
  const peerReq = new Uint8Array([
    IPCP_CONF_REQ, 1, 0, 10,
    IPCP_OPT_IP, 6, 0, 0, 0, 0,
  ]);
  const events = ipcp.recv(peerReq);
  const sendEv = events.find(e => !!e.send);
  check(!!sendEv, 'IPCP recv produces send event');
  if (sendEv && sendEv.send) {
    check(sendEv.send[0] === IPCP_CONF_NAK, `IPCP code = NAK (got ${sendEv.send[0]})`);
    // Body offset 4..: should contain IPCP_OPT_IP, 6, 192, 168, 42, 2.
    const body = sendEv.send.subarray(4);
    check(body[0] === IPCP_OPT_IP && body[1] === 6 &&
          body[2] === DEVICE_IP[0] && body[3] === DEVICE_IP[1] &&
          body[4] === DEVICE_IP[2] && body[5] === DEVICE_IP[3],
          `NAK proposes ${DEVICE_IP.join('.')}`);
  }
}

// 8. IPv4 build → parse roundtrip with correct header checksum.
{
  const src = new Uint8Array([192, 168, 42, 1]);
  const dst = new Uint8Array([192, 168, 42, 2]);
  const payload = new Uint8Array([0xDE, 0xAD, 0xBE, 0xEF]);
  const pkt = buildIpv4(IP_PROTO_ICMP, src, dst, payload, 0x1234);
  const parsed = parseIpv4(pkt);
  check(parsed !== null, 'IPv4 parse returns non-null');
  if (parsed) {
    check(parsed.header.protocol === IP_PROTO_ICMP, 'protocol = ICMP');
    check(parsed.header.totalLength === 24, `totalLength = 24 (got ${parsed.header.totalLength})`);
    check(parsed.payload.length === 4, 'payload length = 4');
  }
}

// 9. ICMP echo request → echo reply, type/code/checksum.
{
  const src = new Uint8Array([192, 168, 42, 2]);
  const dst = new Uint8Array([192, 168, 42, 1]);
  // ICMP echo request: type=8 code=0 csum=? id=1 seq=1 + 4 bytes payload.
  const icmp = new Uint8Array([8, 0, 0, 0, 0, 1, 0, 1, 0xCA, 0xFE, 0xBA, 0xBE]);
  // We don't compute the inbound checksum here; handleIcmp doesn't
  // verify it, only flips the type and recomputes.
  const pkt = parseIpv4(buildIpv4(IP_PROTO_ICMP, src, dst, icmp))!;
  const reply = handleIcmp(pkt);
  check(reply !== null, 'ICMP handler returns reply');
  if (reply) {
    check(reply[0] === 0, 'reply type = 0 (echo reply)');
    check(reply[1] === 0, 'reply code = 0');
    check(reply.length === icmp.length, 'reply length matches');
  }
}

// ── Phase 2: UDP / DNS / TCP / HTTP proxy ─────────────────────────

// Imports for phase-2 modules.
import { parseUdp, buildUdp } from '../inet/udp.ts';
import { handleDnsQuery, FAKE_HOST_HTTP } from '../inet/dns.ts';
import { Tcp } from '../inet/tcp.ts';
import { createHttpProxyListener } from '../inet/http-proxy.ts';
import { IP_PROTO_UDP, IP_PROTO_TCP } from '../inet/types.ts';

// 10. UDP roundtrip + pseudo-header checksum.
{
  const src = new Uint8Array([192, 168, 42, 1]);
  const dst = new Uint8Array([192, 168, 42, 2]);
  const seg = { srcPort: 53, dstPort: 53000, payload: new Uint8Array([1, 2, 3, 4, 5]) };
  const built = buildUdp(src, dst, seg);
  check(built.length === 8 + seg.payload.length, `UDP length = ${built.length}`);
  const parsed = parseUdp(built);
  check(parsed !== null, 'UDP parse returns non-null');
  check(parsed!.srcPort === 53 && parsed!.dstPort === 53000, 'ports preserved');
  check(parsed!.payload.length === 5, 'payload length preserved');
}

// 11. DNS A-query for canned.psion.local resolves to 192.168.42.10.
{
  // Build a wire-format DNS query for canned.psion.local A IN.
  const name = 'canned.psion.local';
  const labels = name.split('.');
  let nameLen = 0;
  for (const l of labels) nameLen += 1 + l.length;
  nameLen += 1; // final root label
  const query = new Uint8Array(12 + nameLen + 4);
  query[0] = 0x12; query[1] = 0x34;     // ID
  query[2] = 0x01; query[3] = 0x00;     // flags: RD
  query[4] = 0; query[5] = 1;            // QDCOUNT
  let p = 12;
  for (const l of labels) {
    query[p++] = l.length;
    for (let i = 0; i < l.length; i++) query[p++] = l.charCodeAt(i);
  }
  query[p++] = 0;                        // root
  query[p++] = 0; query[p++] = 1;        // QTYPE A
  query[p++] = 0; query[p++] = 1;        // QCLASS IN

  const reply = handleDnsQuery(query);
  check(reply !== null, 'DNS handler returns non-null');
  if (reply) {
    check(reply[0] === 0x12 && reply[1] === 0x34, 'reply ID matches query');
    check((reply[2] & 0x80) !== 0, 'QR bit set');
    check(reply[7] === 1, 'ANCOUNT = 1');
    // Last 4 bytes of the reply are the A-record IP.
    const ip = reply.subarray(reply.length - 4);
    const ok = ip[0] === FAKE_HOST_HTTP[0] && ip[1] === FAKE_HOST_HTTP[1] &&
               ip[2] === FAKE_HOST_HTTP[2] && ip[3] === FAKE_HOST_HTTP[3];
    check(ok, `A-record IP = ${FAKE_HOST_HTTP.join('.')} (got ${ip.join('.')})`);
  }
}

// 12. TCP + HTTP-proxy end-to-end loopback. We drive both sides
//     of the wire by hand: client (us, on port 12345) sends SYN →
//     server (HTTP proxy on port 80) responds. We verify the full
//     dance produces a 200 HTML page from the curated zone.
await (async () => {
  // The "wire" — captures IP packets emitted by the TCP engine.
  const wire: Uint8Array[] = [];
  const HOST = new Uint8Array([192, 168, 42, 10]);
  const CLIENT = new Uint8Array([192, 168, 42, 2]);
  const tcp = new Tcp(HOST, pkt => wire.push(pkt));
  tcp.listen(80, createHttpProxyListener({
    // Provide a fetch stub so curated-zone hits don't need it; only
    // real-host hits would call it.
    fetch: async () => { throw new Error('fetch not expected in this test'); },
  }));

  // Manually build a SYN segment from CLIENT:12345 → HOST:80.
  function buildSeg(srcPort: number, dstPort: number, seq: number, ack: number,
                    flags: number, payload: Uint8Array, srcIp: Uint8Array, dstIp: Uint8Array): Uint8Array {
    const seg = new Uint8Array(20 + payload.length);
    seg[0] = (srcPort >>> 8) & 0xFF; seg[1] = srcPort & 0xFF;
    seg[2] = (dstPort >>> 8) & 0xFF; seg[3] = dstPort & 0xFF;
    seg[4] = (seq >>> 24) & 0xFF; seg[5] = (seq >>> 16) & 0xFF;
    seg[6] = (seq >>> 8) & 0xFF;  seg[7] = seq & 0xFF;
    seg[8] = (ack >>> 24) & 0xFF; seg[9] = (ack >>> 16) & 0xFF;
    seg[10] = (ack >>> 8) & 0xFF; seg[11] = ack & 0xFF;
    seg[12] = (5 << 4);                          // data offset = 5 words
    seg[13] = flags;
    seg[14] = 0x40; seg[15] = 0x00;              // window = 16 KiB
    seg.set(payload, 20);
    // Checksum (recomputed from pseudo-header).
    const csum = pseudoChecksumLocal(srcIp, dstIp, IP_PROTO_TCP, seg);
    seg[16] = (csum >>> 8) & 0xFF;
    seg[17] = csum & 0xFF;
    return seg;
  }

  // Need a pseudo-checksum here too. Replicate to avoid coupling tests
  // to an internal helper.
  function pseudoChecksumLocal(srcIp: Uint8Array, dstIp: Uint8Array, proto: number, seg: Uint8Array): number {
    const buf = new Uint8Array(12 + seg.length);
    buf.set(srcIp, 0); buf.set(dstIp, 4);
    buf[9] = proto;
    buf[10] = (seg.length >>> 8) & 0xFF; buf[11] = seg.length & 0xFF;
    buf.set(seg, 12);
    let sum = 0;
    for (let i = 0; i + 1 < buf.length; i += 2) {
      sum += (buf[i] << 8) | buf[i + 1];
      if (sum > 0xFFFF) sum = (sum & 0xFFFF) + 1;
    }
    return (~sum) & 0xFFFF;
  }

  // ── Step 1: SYN
  const clientSeq = 0x10000000;
  const syn = buildSeg(12345, 80, clientSeq, 0, 0x02 /* SYN */, new Uint8Array(0), CLIENT, HOST);
  tcp.handleSegment(CLIENT, HOST, syn);
  check(wire.length === 1, `wire has 1 packet after SYN (got ${wire.length})`);
  // Parse the SYN+ACK to learn the server's ISS.
  const synAckIp = wire.shift()!;
  const synAckSeg = synAckIp.subarray(20);
  const serverSeq = ((synAckSeg[4] << 24) | (synAckSeg[5] << 16) | (synAckSeg[6] << 8) | synAckSeg[7]) >>> 0;
  const serverAck = ((synAckSeg[8] << 24) | (synAckSeg[9] << 16) | (synAckSeg[10] << 8) | synAckSeg[11]) >>> 0;
  const flagsSa = synAckSeg[13];
  check((flagsSa & 0x12) === 0x12, `SYN+ACK flags set (got 0x${flagsSa.toString(16)})`);
  check(serverAck === ((clientSeq + 1) >>> 0), 'SYN+ACK ack = clientSeq+1');

  // ── Step 2: client ACKs the SYN+ACK.
  const ack1 = buildSeg(12345, 80, (clientSeq + 1) >>> 0, (serverSeq + 1) >>> 0, 0x10 /* ACK */, new Uint8Array(0), CLIENT, HOST);
  tcp.handleSegment(CLIENT, HOST, ack1);
  // Server has nothing to send yet — wire should be empty.
  check(wire.length === 0, `no extra wire packet after handshake ACK (got ${wire.length})`);

  // ── Step 3: client sends GET / HTTP/1.0 + Host header.
  const reqStr = 'GET / HTTP/1.0\r\nHost: canned.psion.local\r\n\r\n';
  const req = new Uint8Array(reqStr.length);
  for (let i = 0; i < reqStr.length; i++) req[i] = reqStr.charCodeAt(i);
  const dataSeg = buildSeg(12345, 80, (clientSeq + 1) >>> 0, (serverSeq + 1) >>> 0, 0x18 /* PSH+ACK */, req, CLIENT, HOST);
  tcp.handleSegment(CLIENT, HOST, dataSeg);

  // The HTTP proxy is async — let microtasks drain.
  await new Promise(r => setTimeout(r, 10));

  // ── Step 4: wire should now have: ACK of request, then HTTP response
  //     segments (PSH+ACK), then FIN+ACK.
  check(wire.length >= 2, `server emitted >=2 packets after request (got ${wire.length})`);
  // Concatenate all data-bearing segments into a response body buffer.
  let responseBytes = new Uint8Array(0);
  let sawFin = false;
  for (const ip of wire) {
    const seg = ip.subarray(20);
    const flags = seg[13];
    const dataOff = (seg[12] >>> 4) & 0xF;
    const payload = seg.subarray(dataOff * 4);
    if (payload.length > 0) {
      const combined = new Uint8Array(responseBytes.length + payload.length);
      combined.set(responseBytes, 0); combined.set(payload, responseBytes.length);
      responseBytes = combined;
    }
    if ((flags & 0x01) !== 0) sawFin = true;
  }
  check(sawFin, 'server sent FIN');
  let respStr = '';
  for (let i = 0; i < responseBytes.length; i++) respStr += String.fromCharCode(responseBytes[i]);
  check(respStr.startsWith('HTTP/1.0 200 OK'), `response starts with 200 OK (got ${respStr.slice(0, 40)})`);
  check(respStr.includes('Welcome to the simulated internet'), 'response body contains curated home page text');
})();

// 13. HTTP proxy serves the synthetic 502 page when fetch rejects.
await (async () => {
  const wire: Uint8Array[] = [];
  const HOST = new Uint8Array([192, 168, 42, 10]);
  const CLIENT = new Uint8Array([192, 168, 42, 2]);
  const tcp = new Tcp(HOST, pkt => wire.push(pkt));
  tcp.listen(80, createHttpProxyListener({
    fetch: async () => { throw new TypeError('Failed to fetch (CORS)'); },
  }));

  function buildSeg(srcPort: number, dstPort: number, seq: number, ack: number, flags: number, payload: Uint8Array): Uint8Array {
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
    return seg;
  }

  const clientSeq = 0x20000000;
  tcp.handleSegment(CLIENT, HOST, buildSeg(54321, 80, clientSeq, 0, 0x02, new Uint8Array(0)));
  const synAck = wire.shift()!;
  const serverSeq = ((synAck[20 + 4] << 24) | (synAck[20 + 5] << 16) | (synAck[20 + 6] << 8) | synAck[20 + 7]) >>> 0;
  tcp.handleSegment(CLIENT, HOST, buildSeg(54321, 80, (clientSeq + 1) >>> 0, (serverSeq + 1) >>> 0, 0x10, new Uint8Array(0)));

  const reqStr = 'GET / HTTP/1.0\r\nHost: example.com\r\n\r\n';
  const req = new Uint8Array(reqStr.length);
  for (let i = 0; i < reqStr.length; i++) req[i] = reqStr.charCodeAt(i);
  tcp.handleSegment(CLIENT, HOST, buildSeg(54321, 80, (clientSeq + 1) >>> 0, (serverSeq + 1) >>> 0, 0x18, req));

  await new Promise(r => setTimeout(r, 10));
  let responseBytes = new Uint8Array(0);
  for (const ip of wire) {
    const seg = ip.subarray(20);
    const payload = seg.subarray(((seg[12] >>> 4) & 0xF) * 4);
    if (payload.length > 0) {
      const combined = new Uint8Array(responseBytes.length + payload.length);
      combined.set(responseBytes, 0); combined.set(payload, responseBytes.length);
      responseBytes = combined;
    }
  }
  let respStr = '';
  for (let i = 0; i < responseBytes.length; i++) respStr += String.fromCharCode(responseBytes[i]);
  check(respStr.startsWith('HTTP/1.0 502'), `CORS failure → 502 (got ${respStr.slice(0, 40)})`);
  check(respStr.includes('Bad Gateway'), 'body mentions Bad Gateway');
})();

// Suppress unused-import warnings for the phase-2 imports referenced
// only inside the per-test scopes above.
void parseUdp; void IP_PROTO_UDP;

// ── Phase 3: SMTP / POP3 mailbox round-trip ───────────────────────

import { Mailbox, MemoryBackend, parseHeaders } from '../inet/mailbox.ts';
import { createSmtpListener } from '../inet/smtp.ts';
import { createPop3Listener } from '../inet/pop3.ts';

// Reusable helper: drive a TCP listener as a synthetic client, return
// all bytes received from the server until the server FINs (or we hit
// `timeoutMs`). The client sends `script` as a list of byte-string
// chunks, each preceded by a short delay so the server has a chance
// to respond.
async function tcpRoundTrip(
  listener: ReturnType<typeof createSmtpListener> | ReturnType<typeof createPop3Listener>,
  serverPort: number,
  script: string[],
): Promise<string> {
  const HOST   = new Uint8Array([192, 168, 42, 10]);
  const CLIENT = new Uint8Array([192, 168, 42, 99]);
  const wire: Uint8Array[] = [];
  const tcp = new Tcp(HOST, pkt => wire.push(pkt));
  tcp.listen(serverPort, listener);

  function buildSeg(srcPort: number, dstPort: number, seq: number, ack: number, flags: number, payload: Uint8Array): Uint8Array {
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
    return seg;
  }
  function parseHeader(ipPkt: Uint8Array): { seq: number; ack: number; flags: number; payload: Uint8Array } {
    const seg = ipPkt.subarray(20);
    return {
      seq:   ((seg[4] << 24) | (seg[5] << 16) | (seg[6] << 8) | seg[7]) >>> 0,
      ack:   ((seg[8] << 24) | (seg[9] << 16) | (seg[10] << 8) | seg[11]) >>> 0,
      flags: seg[13],
      payload: seg.subarray(((seg[12] >>> 4) & 0xF) * 4),
    };
  }

  const clientSeq0 = 0x40000000;
  let clientSeq = clientSeq0;
  // SYN
  tcp.handleSegment(CLIENT, HOST, buildSeg(33333, serverPort, clientSeq, 0, 0x02, new Uint8Array(0)));
  const synAck = parseHeader(wire.shift()!);
  // ACK
  clientSeq = (clientSeq + 1) >>> 0;
  const serverNext = (synAck.seq + 1) >>> 0;
  tcp.handleSegment(CLIENT, HOST, buildSeg(33333, serverPort, clientSeq, serverNext, 0x10, new Uint8Array(0)));

  // Collect anything the server sent after the SYN+ACK (e.g. the SMTP greeting).
  let received = '';
  const drainReceived = () => {
    while (wire.length > 0) {
      const p = parseHeader(wire.shift()!);
      if (p.payload.length > 0) {
        for (let i = 0; i < p.payload.length; i++) received += String.fromCharCode(p.payload[i]);
      }
    }
  };
  drainReceived();

  let serverAck = serverNext;
  for (const chunk of script) {
    const bytes = new Uint8Array(chunk.length);
    for (let i = 0; i < chunk.length; i++) bytes[i] = chunk.charCodeAt(i);
    tcp.handleSegment(CLIENT, HOST, buildSeg(33333, serverPort, clientSeq, serverAck, 0x18, bytes));
    clientSeq = (clientSeq + bytes.length) >>> 0;
    // Yield to microtasks for any async server work (SMTP/POP3 are
    // synchronous; HTTP proxy isn't but we don't go through it here).
    await new Promise(r => setTimeout(r, 5));
    // Drain server's responses and update serverAck so we keep moving.
    while (wire.length > 0) {
      const p = parseHeader(wire.shift()!);
      // Track largest seq+payload as serverAck (so our next ack covers it).
      const end = (p.seq + p.payload.length + (((p.flags & 0x02) !== 0 || (p.flags & 0x01) !== 0) ? 1 : 0)) >>> 0;
      if (((end - serverAck) | 0) > 0) serverAck = end;
      if (p.payload.length > 0) {
        for (let i = 0; i < p.payload.length; i++) received += String.fromCharCode(p.payload[i]);
      }
    }
  }
  // Final yield to allow any tail-end response.
  await new Promise(r => setTimeout(r, 5));
  drainReceived();
  return received;
}

// 14. Mailbox parse-headers reads From / To / Subject.
{
  const raw = 'From: alice@example.com\r\nTo: bob@example.com\r\nSubject: Hi\r\nDate: Fri, 1 Jan 2010 12:00:00 GMT\r\n\r\nbody';
  const h = parseHeaders(raw);
  check(h.from === 'alice@example.com', `parseHeaders.from = ${h.from}`);
  check(h.subject === 'Hi', `parseHeaders.subject = ${h.subject}`);
  check(Array.isArray(h.to) && h.to![0] === 'bob@example.com', 'parseHeaders.to[0] = bob');
}

// 15. SMTP captures a complete message into the outbox.
await (async () => {
  const mb = new Mailbox(new MemoryBackend());
  await mb.init();
  const listener = createSmtpListener({ mailbox: mb });
  const greetingAndBody = await tcpRoundTrip(listener, 25, [
    'EHLO test.example\r\n',
    'AUTH PLAIN AGFsaWNlAHNlY3JldA==\r\n',
    'MAIL FROM:<alice@example.com>\r\n',
    'RCPT TO:<bob@example.com>\r\n',
    'DATA\r\n',
    'From: alice@example.com\r\nTo: bob@example.com\r\nSubject: Hello\r\n\r\nThis is the body.\r\n.\r\n',
    'QUIT\r\n',
  ]);
  check(greetingAndBody.startsWith('220 mail.psion.local'), `SMTP greeting present (got ${greetingAndBody.slice(0, 30)})`);
  check(greetingAndBody.includes('250-AUTH PLAIN LOGIN'), 'EHLO advertises AUTH');
  check(greetingAndBody.includes('235 Authentication successful'), 'AUTH accepted');
  check(greetingAndBody.includes('354 End data'), '354 prompt sent');
  check(greetingAndBody.includes('250 OK message queued'), 'message queued');
  check(greetingAndBody.includes('221 mail.psion.local closing connection'), 'QUIT bye');
  const stored = mb.listOutbox();
  check(stored.length === 1, `outbox count = 1 (got ${stored.length})`);
  if (stored.length === 1) {
    check(stored[0].from === 'alice@example.com', `stored.from = ${stored[0].from}`);
    check(stored[0].subject === 'Hello', `stored.subject = ${stored[0].subject}`);
    check(stored[0].raw.includes('This is the body.'), 'stored.raw contains body');
  }
})();

// 16. SMTP dot-stuffing: ".." at start of a line in DATA must be
//     converted back to ".".
await (async () => {
  const mb = new Mailbox(new MemoryBackend());
  await mb.init();
  const listener = createSmtpListener({ mailbox: mb });
  await tcpRoundTrip(listener, 25, [
    'HELO test\r\n',
    'MAIL FROM:<a@b>\r\n',
    'RCPT TO:<c@d>\r\n',
    'DATA\r\n',
    'From: a@b\r\nTo: c@d\r\nSubject: Dots\r\n\r\n..A line starting with dot.\r\n.\r\n',
    'QUIT\r\n',
  ]);
  const stored = mb.listOutbox();
  check(stored.length === 1, 'outbox got the dotted message');
  if (stored.length === 1) {
    check(stored[0].raw.includes('\n.A line starting with dot.'), 'dot-stuffing reversed');
  }
})();

// 17. POP3 STAT / LIST / RETR / DELE / QUIT round-trip.
await (async () => {
  const mb = new Mailbox(new MemoryBackend());
  await mb.init();
  // Seed two messages.
  mb.appendInbox({ from: 'a@x', to: ['user@p'], subject: 'one', raw: 'From: a@x\r\nSubject: one\r\n\r\nbody1\r\n' });
  mb.appendInbox({ from: 'b@y', to: ['user@p'], subject: 'two', raw: 'From: b@y\r\nSubject: two\r\n\r\nbody2\r\n' });
  const listener = createPop3Listener({ mailbox: mb });
  const got = await tcpRoundTrip(listener, 110, [
    'USER alice\r\n',
    'PASS secret\r\n',
    'STAT\r\n',
    'LIST\r\n',
    'RETR 1\r\n',
    'DELE 1\r\n',
    'QUIT\r\n',
  ]);
  check(got.startsWith('+OK psion-emu POP3'), 'POP3 greeting');
  check(got.includes('+OK mailbox has 2 messages'), 'PASS reports message count');
  check(/\+OK 2 \d+/.test(got), 'STAT reports 2 and total octets');
  check(got.includes('1 ') && got.includes('2 '), 'LIST shows both messages');
  check(got.includes('+OK ') && got.includes('body1'), 'RETR 1 returns body');
  check(got.includes('+OK message 1 marked for deletion'), 'DELE 1 ack');
  check(got.endsWith('+OK bye\r\n'), 'QUIT bye');
  const remaining = mb.listInbox();
  check(remaining.length === 1 && remaining[0].subject === 'two', `inbox after DELE has 1 message "two" (got ${remaining.map(m => m.subject).join(',')})`);
})();

// 18. POP3 dot-stuffs on the way out: a body line starting with "."
//     becomes "..".
await (async () => {
  const mb = new Mailbox(new MemoryBackend());
  await mb.init();
  mb.appendInbox({ from: 'x@y', to: ['z@w'], subject: 'dots', raw: 'From: x@y\r\nSubject: dots\r\n\r\n.starts with dot\r\n' });
  const listener = createPop3Listener({ mailbox: mb });
  const got = await tcpRoundTrip(listener, 110, [
    'USER u\r\nPASS p\r\nRETR 1\r\nQUIT\r\n',
  ]);
  check(got.includes('\r\n..starts with dot\r\n'), 'POP3 RETR dot-stuffs the leading dot');
})();

if (failures > 0) {
  console.error(`\n${failures} test failure(s)`);
  process.exit(1);
}
console.log('\nOK — all modem-protocol tests passed');
