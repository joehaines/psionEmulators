// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Tests for the wire-correct NCP layer (frontend/src/lib/plp/ncp-spec.ts).
// Run with:
//   node --experimental-strip-types frontend/src/lib/__tests__/plp.ncp-spec.test.mts

import {
  encodeNcp, decodeNcp,
  makeConnectPacket, makeConnectResponsePacket,
  makeNcpInfoPacket, makeCompletePacket,
  Ncp,
} from '../plp/ncp-spec.ts';
import { LinkLayer } from '../plp/link.ts';
import { FrameDecoder } from '../plp/framing.ts';
import { NCP, NCP_CONTROL_CHAN, NCP_VERSION } from '../plp/types.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}
function eqBytes(a: Uint8Array, b: Uint8Array, msg: string) {
  if (a.length !== b.length) { console.error(`FAIL: ${msg}: length ${a.length} vs ${b.length}`); failures++; return; }
  for (let i = 0; i < a.length; i++) {
    if (a[i] !== b[i]) { console.error(`FAIL: ${msg}: byte ${i} is 0x${a[i].toString(16)} (want 0x${b[i].toString(16)})`); failures++; return; }
  }
}

// 1. Encode/decode round-trip with arbitrary header bytes.
{
  const p = { destChan: 0x42, srcChan: 0x11, type: NCP.COMPLETE, data: new Uint8Array([1, 2, 3]) };
  const wire = encodeNcp(p);
  eqBytes(wire, new Uint8Array([0x42, 0x11, NCP.COMPLETE, 1, 2, 3]), 'encodeNcp byte layout');
  const back = decodeNcp(wire);
  check(back !== null && back.destChan === 0x42 && back.srcChan === 0x11
        && back.type === NCP.COMPLETE && back.data.length === 3,
        'decodeNcp round-trip');
}

// 2. Connect packet: destChan=0, srcChan=clientChan, type=0x03,
//    data=name+NUL.
{
  const p = makeConnectPacket(0x05, 'LINK.*');
  const wire = encodeNcp(p);
  // 0x00 0x05 0x03 then "LINK.*" then 0x00
  check(wire[0] === 0x00 && wire[1] === 0x05 && wire[2] === 0x03, 'Connect header bytes');
  eqBytes(wire.subarray(3, 9), new TextEncoder().encode('LINK.*'), 'Connect service name');
  check(wire[9] === 0x00, 'Connect terminator');
  check(wire.length === 10, 'Connect packet total length');
}

// 3. Connect Response: destChan=0, srcChan=server, type=0x04,
//    data=[client, status].
{
  const wire = encodeNcp(makeConnectResponsePacket(0x07, 0x05, 0));
  eqBytes(wire, new Uint8Array([0x00, 0x07, 0x04, 0x05, 0x00]), 'Connect Response byte layout');
}

// 4. NCP Info: destChan=0, srcChan=0, type=0x06, data=[version, id*4].
{
  const id = new Uint8Array([0xDE, 0xAD, 0xBE, 0xEF]);
  const wire = encodeNcp(makeNcpInfoPacket(NCP_VERSION.EPOC_ER3, id));
  eqBytes(wire, new Uint8Array([0x00, 0x00, 0x06, 0x06, 0xDE, 0xAD, 0xBE, 0xEF]),
          'NCP Info byte layout');
}

// 5. Complete packet (data on an established channel).
{
  const wire = encodeNcp(makeCompletePacket(0x07, 0x05, new Uint8Array([0xAA, 0xBB])));
  eqBytes(wire, new Uint8Array([0x07, 0x05, 0x01, 0xAA, 0xBB]), 'Complete packet byte layout');
}

// 6. Full integration: two LinkLayer+Ncp pairs over an in-process
//    byte loop. A connects to a "LINK.*" server that B "provides"
//    by responding to NCP Connect with a successful Connect Response.

function makeNcpLoopback() {
  const aToB: number[] = [];
  const bToA: number[] = [];
  const decA = new FrameDecoder();
  const decB = new FrameDecoder();

  // We have to construct the LinkLayer with a placeholder onData,
  // then Ncp overrides cfg.onData via its (intentional) internal hook.
  const linkA = new LinkLayer({
    sendBytes: bytes => { for (const b of bytes) aToB.push(b); pump(); },
    onData: () => { /* replaced by Ncp */ },
  });
  const linkB = new LinkLayer({
    sendBytes: bytes => { for (const b of bytes) bToA.push(b); pump(); },
    onData: () => { /* replaced by Ncp */ },
  });
  const ncpA = new Ncp(linkA);
  const ncpB = new Ncp(linkB);

  function pump() {
    while (aToB.length > 0 || bToA.length > 0) {
      if (aToB.length > 0) {
        const chunk = new Uint8Array(aToB.splice(0));
        for (const f of decB.feed(chunk)) linkB.handleFrame(f.payload);
      }
      if (bToA.length > 0) {
        const chunk = new Uint8Array(bToA.splice(0));
        for (const f of decA.feed(chunk)) linkA.handleFrame(f.payload);
      }
    }
  }

  return { linkA, linkB, ncpA, ncpB, pump };
}

// Bring up the link, exchange NCP Info, open a "LINK.*" channel from
// A. The Ncp instance on B auto-responds with a success Connect
// Response (default behaviour for unknown server names — see the
// CONNECT case in dispatchPacket).
async function runConnectTest() {
  const lo = makeNcpLoopback();
  lo.linkA.initiate();
  check(lo.linkA.state === 'connected', 'A link connected');
  check(lo.linkB.state === 'connected', 'B link connected');

  // Each side sends NCP Information.
  lo.ncpA.sendNcpInfo();
  lo.ncpB.sendNcpInfo();
  check(lo.ncpA.peerInfoReceived, 'A received B’s NCP Info');
  check(lo.ncpB.peerInfoReceived, 'B received A’s NCP Info');

  // A asks B to open the LINK.* server. B auto-responds with success.
  const serverChan = await lo.ncpA.connectServer('LINK.*');
  check(typeof serverChan === 'number' && serverChan > 0,
        `LINK.* channel assigned (got ${serverChan})`);

  // Bonus: data exchange on the open channel.
  let aReceivedOnLink: Uint8Array | null = null;
  lo.ncpA.setHandler(2 /* first client chan */, data => { aReceivedOnLink = data; });
  // B sends a Complete packet addressed back to A's clientChan.
  lo.ncpB['link'].sendData(encodeNcp(
    makeCompletePacket(2 /* destChan=A's client */, serverChan, new Uint8Array([0x99, 0x88]))
  ));
  lo.pump();
  check(aReceivedOnLink !== null, 'A received data on LINK.* channel');
  if (aReceivedOnLink) eqBytes(aReceivedOnLink, new Uint8Array([0x99, 0x88]), 'data byte-equal');
}

await runConnectTest();

if (failures > 0) {
  console.error(`\n${failures} test failure(s)`);
  process.exit(1);
}
console.log('OK — all PLP NCP-spec tests passed');
