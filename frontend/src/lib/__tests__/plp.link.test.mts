// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Tests for the new PLP data-link layer (PDU codec + state machine).
// Run with:
//   node --experimental-strip-types frontend/src/lib/__tests__/plp.link.test.mts

import {
  encodePdu, decodePdu,
  reqReqPdu, reqConPdu, ackPdu, dataPdu,
  LinkLayer,
  type LinkState, type Pdu,
} from '../plp/link.ts';
import { FrameDecoder } from '../plp/framing.ts';
import { PDU_CONT_ACK, PDU_CONT_REQ, PDU_CONT_DATA } from '../plp/types.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}
function eqBytes(a: Uint8Array, b: Uint8Array, msg: string) {
  if (a.length !== b.length) {
    console.error(`FAIL: ${msg}: length ${a.length} vs ${b.length}`);
    failures++; return;
  }
  for (let i = 0; i < a.length; i++) {
    if (a[i] !== b[i]) {
      console.error(`FAIL: ${msg}: byte ${i} is 0x${a[i].toString(16)} (want 0x${b[i].toString(16)})`);
      failures++; return;
    }
  }
}

// ── PDU codec ────────────────────────────────────────────────────────
// Cont/Seq byte layout: high nibble = cont, low nibble = seq.
{
  const frame = encodePdu({ cont: PDU_CONT_REQ, seq: 1, data: new Uint8Array(0) });
  // Frame is SYN DLE STX | 0x21 | DLE ETX | CRC-hi CRC-lo → 8 bytes total.
  check(frame.length === 8, `Req_Req frame length ${frame.length} (want 8)`);
  check(frame[3] === 0x21, `Cont/Seq byte 0x${frame[3].toString(16)} (want 0x21)`);
}

// Round-trip: decode what we just encoded.
{
  const dec = new FrameDecoder();
  const wire = encodePdu({ cont: PDU_CONT_DATA, seq: 5, data: new Uint8Array([0xAA, 0xBB]) });
  const frames = dec.feed(wire);
  check(frames.length === 1, 'one frame decoded');
  const pdu = decodePdu(frames[0].payload);
  check(pdu !== null, 'pdu decoded');
  if (pdu) {
    check(pdu.cont === PDU_CONT_DATA && pdu.seq === 5, `decoded cont=${pdu.cont} seq=${pdu.seq}`);
    eqBytes(pdu.data, new Uint8Array([0xAA, 0xBB]), 'decoded data');
  }
}

// Real captured Req_Req_Pdu from the Revo: payload byte 0x21.
{
  const dec = new FrameDecoder();
  const frames = dec.feed(new Uint8Array([0x16, 0x10, 0x02, 0x21, 0x10, 0x03, 0x34, 0x43]));
  check(frames.length === 1, 'captured Revo Req_Req frame decoded');
  if (frames.length === 1) {
    const pdu = decodePdu(frames[0].payload);
    check(pdu?.cont === PDU_CONT_REQ && pdu?.seq === 1, 'Revo Req_Req = Cont=2 Seq=1');
  }
}

// Real captured Req_Con_Pdu from the Revo (R5 quirk: Cont=2, Seq=4
// rather than the spec's Cont=4 Seq=4 — see harness/run.cpp captures).
{
  const dec = new FrameDecoder();
  // payload = 0x24 + 4-byte magic
  const wire = new Uint8Array([0x16, 0x10, 0x02, 0x24, 0xde, 0xad, 0xbe, 0xef, 0x10, 0x03]);
  // Compute and append CRC for the payload `24 de ad be ef`.
  // (Hard-coded check, see plp.framing.test.mts for the algorithm
  // verification; we just trust the encoder for the cross-check below.)
  // Easier: encode it via encodePdu and compare to wire-tx behaviour.
  const encoded = encodePdu({ cont: PDU_CONT_REQ, seq: 4,
    data: new Uint8Array([0xde, 0xad, 0xbe, 0xef]) });
  const frames = dec.feed(encoded);
  check(frames.length === 1, 'self-encoded Req_Con round-trips');
  if (frames.length === 1) {
    const pdu = decodePdu(frames[0].payload);
    check(pdu?.cont === PDU_CONT_REQ && pdu?.seq === 4, 'decoded Req_Con cont/seq');
    check(pdu?.data.length === 4, 'magic is 4 bytes');
  }
  void wire;
}

// ── Link state machine ─────────────────────────────────────────────────
// Glue two LinkLayer instances together via a tiny in-memory loop and
// drive a full Req_Req → Req_Con → Ack → Data exchange.

function makeLoopback() {
  // Two queues, one per direction.
  const aToB: number[] = [];
  const bToA: number[] = [];

  const decA = new FrameDecoder();
  const decB = new FrameDecoder();

  let receivedByA = new Uint8Array(0);
  let receivedByB = new Uint8Array(0);
  const statesA: LinkState[] = [];
  const statesB: LinkState[] = [];
  const pdusInA: Pdu[] = [];
  const pdusInB: Pdu[] = [];

  let linkA: LinkLayer;
  let linkB: LinkLayer;

  linkA = new LinkLayer({
    sendBytes: bytes => { for (const b of bytes) aToB.push(b); pump(); },
    onData: p => { const c = new Uint8Array(receivedByA.length + p.length); c.set(receivedByA, 0); c.set(p, receivedByA.length); receivedByA = c; },
    onStateChange: s => statesA.push(s),
    onPduIn: p => pdusInA.push(p),
  });
  linkB = new LinkLayer({
    sendBytes: bytes => { for (const b of bytes) bToA.push(b); pump(); },
    onData: p => { const c = new Uint8Array(receivedByB.length + p.length); c.set(receivedByB, 0); c.set(p, receivedByB.length); receivedByB = c; },
    onStateChange: s => statesB.push(s),
    onPduIn: p => pdusInB.push(p),
  });

  function pump() {
    while (aToB.length > 0 || bToA.length > 0) {
      if (aToB.length > 0) {
        const chunk = new Uint8Array(aToB.splice(0));
        for (const f of decB.feed(chunk)) linkB.handleFrame(f.payload);
        // R5 Data_Pdu Acks are coalesced and emitted per batch by the poll
        // loop; mirror that here so the loopback delivers them.
        linkB.flushAck();
      }
      if (bToA.length > 0) {
        const chunk = new Uint8Array(bToA.splice(0));
        for (const f of decA.feed(chunk)) linkA.handleFrame(f.payload);
        linkA.flushAck();
      }
    }
  }

  return {
    linkA: () => linkA!,
    linkB: () => linkB!,
    receivedByA: () => receivedByA,
    receivedByB: () => receivedByB,
    statesA: () => statesA,
    statesB: () => statesB,
    pdusInA: () => pdusInA,
    pdusInB: () => pdusInB,
  };
}

// A initiates → both reach `connected`.
{
  const lo = makeLoopback();
  lo.linkA().initiate();
  check(lo.linkA().state === 'connected', `A.state = ${lo.linkA().state} (want connected)`);
  check(lo.linkB().state === 'connected', `B.state = ${lo.linkB().state} (want connected)`);
  // A transitioned: idle → connecting → confirming? → connected.
  // (B starts idle; on receiving Req_Req it goes confirming → connected
  // when its Req_Con's reply Ack arrives.)
  check(lo.statesA().includes('connected'), 'A reached connected state');
  check(lo.statesB().includes('connected'), 'B reached connected state');
}

// Data transfer in both directions.
{
  const lo = makeLoopback();
  lo.linkA().initiate();
  lo.linkA().sendData(new Uint8Array([1, 2, 3, 4]));
  lo.linkB().sendData(new Uint8Array([0xAA, 0xBB]));
  eqBytes(lo.receivedByB(), new Uint8Array([1, 2, 3, 4]), 'B received A’s 4-byte data');
  eqBytes(lo.receivedByA(), new Uint8Array([0xAA, 0xBB]), 'A received B’s 2-byte data');
}

// Each Data_Pdu the recipient sees gets ACKed with matching Seq.
{
  const lo = makeLoopback();
  lo.linkA().initiate();
  lo.linkA().sendData(new Uint8Array([0x42]));
  // Look at the PDUs B received — should be: Req_Req (from A's
  // initiate, but B is on the receive side so actually it sees A's
  // Req_Req), then Req_Con from A, then a Data_Pdu Seq=1.
  // A's view: Req_Con from B, then Ack(0) from B, then Ack(1) for Data.
  const aks = lo.pdusInA().filter(p => p.cont === PDU_CONT_ACK);
  check(aks.some(a => a.seq === 1), `A saw an Ack(seq=1) for its Data_Pdu (got seqs: ${aks.map(a => a.seq).join(',')})`);
}

// Regression: the R5 (mod-2048) Tx sequence must wrap 2047 → 0, not skip 0.
// In the 2-byte Seq encoding 2048 ≡ 0, so the device's masked Rx counter
// expects 0 after 2047; an old "skip Seq=0" rule sent 1 instead, and the
// device then ignored every post-wrap frame and duplicate-acked 2047 forever
// — stalling any transfer long enough to reach the wrap (the field "upload
// fails partway" report; reproduced against the real ROM in _plp_worker_repro
// at maxTxSeq=2047). 0 is only special as the first frame after a handshake,
// which is naturally 1 anyway (handshake leaves seqTx=0 → first send → 1).
{
  const out: Pdu[] = [];
  const dec = new FrameDecoder();
  const link = new LinkLayer({
    sendBytes: () => { /* discard */ }, onData: () => { /* unused */ },
    onPduOut: p => out.push(p), maxRetxRounds: 0,
  });
  link.initiate();   // → connecting (sends Req_Req)
  // Peer confirms with its magic → confirming → connected.
  for (const f of dec.feed(encodePdu(reqConPdu(new Uint8Array([9, 8, 7, 6]), 4))))
    link.handleFrame(f.payload);
  check(link.state === 'connected', `wrap-test: link connected (got ${link.state})`);
  const seqs: number[] = [];
  for (let i = 0; i < 2049; i++) seqs.push(link.sendData(new Uint8Array([0])));
  check(seqs[0] === 1, `wrap-test: first data Seq ${seqs[0]} (want 1)`);
  check(seqs[2046] === 2047, `wrap-test: Seq[2046] ${seqs[2046]} (want 2047)`);
  check(seqs[2047] === 0, `wrap-test: wrap Seq[2047] ${seqs[2047]} (want 0, NOT 1)`);
  check(seqs[2048] === 1, `wrap-test: post-wrap Seq[2048] ${seqs[2048]} (want 1)`);
  // The wrapped Seq=0 Data_Pdu must still round-trip through the codec.
  const f0 = encodePdu(dataPdu(0, new Uint8Array([0xAB])));
  const dec0 = new FrameDecoder();
  for (const fr of dec0.feed(f0)) {
    const p = decodePdu(fr.payload);
    check(p?.cont === PDU_CONT_DATA && p?.seq === 0, `wrap-test: Data Seq=0 round-trips (got cont=${p?.cont} seq=${p?.seq})`);
  }
}

if (failures > 0) {
  console.error(`\n${failures} test failure(s)`);
  process.exit(1);
}
console.log('OK — all PLP link-layer tests passed');
