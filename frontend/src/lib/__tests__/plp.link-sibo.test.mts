// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// SIBO-variant link layer tests. The handshake and sequence-number
// vectors are pinned from live exchanges with the Series 3c v5.20f
// ROM via frontend/src/lib/__tests__/_plp_repro.mts:
//   host Req_Pdu  16 10 02 21 10 03 34 43
//   dev  Req_Pdu  16 10 02 20 10 03 24 62
//   host Ack      16 10 02 00 10 03 00 00
//   dev  Ack + Data_Pdu(seq=1) carrying NCP Info 00 00 06 03 …
//
// Run: node --experimental-strip-types frontend/src/lib/__tests__/plp.link-sibo.test.mts

import { LinkLayer, encodePdu, decodePdu, dataPdu } from '../plp/link.ts';
import { FrameDecoder } from '../plp/framing.ts';

let failures = 0;
function check(name: string, cond: boolean, extra?: string) {
  if (cond) console.log(`ok   ${name}`);
  else { console.error(`FAIL ${name}${extra ? ` — ${extra}` : ''}`); failures++; }
}

const hex = (b: Uint8Array) => Array.from(b).map(x => x.toString(16).padStart(2, '0')).join(' ');

// ── Wire bytes of the host Req_Pdu must match the live capture ──────
{
  const req = encodePdu({ cont: 2, seq: 1, data: new Uint8Array(0) });
  check('SIBO Req_Pdu wire bytes', hex(req) === '16 10 02 21 10 03 34 43', hex(req));
}

// ── Handshake: initiate → device Req → Ack out, connected ───────────
{
  const sent: Uint8Array[] = [];
  const states: string[] = [];
  const link = new LinkLayer({
    variant: 'sibo',
    sendBytes: b => sent.push(b),
    onData: () => {},
    onStateChange: s => states.push(s),
  });
  link.initiate();
  check('initiate sends Req', sent.length === 1 && hex(sent[0]) === '16 10 02 21 10 03 34 43');
  // Device replies with its own Req_Pdu (cont=2, seq=0).
  link.handleFrame(new Uint8Array([0x20]));
  check('device Req → connected', link.state === 'connected');
  check('device Req → Ack(0) out',
    sent.length === 2 && hex(sent[1]) === '16 10 02 00 10 03 00 00', hex(sent[1] ?? new Uint8Array(0)));
}

// ── Sequence numbers count plain mod 8 (… 6, 7, 0, 1 …) ─────────────
{
  const sent: Uint8Array[] = [];
  const link = new LinkLayer({
    variant: 'sibo',
    sendBytes: b => sent.push(b),
    onData: () => {},
  });
  link.initiate();
  link.handleFrame(new Uint8Array([0x20]));   // connected
  const seqs: number[] = [];
  for (let i = 0; i < 10; i++) seqs.push(link.sendData(new Uint8Array([0x42])));
  check('SIBO Data_Pdu seq wraps through 0',
    seqs.join(',') === '1,2,3,4,5,6,7,0,1,2', seqs.join(','));
}

// ── Duplicate Data_Pdu (device retransmission) is re-acked, not
//    redelivered ──────────────────────────────────────────────────────
{
  const delivered: string[] = [];
  let acks = 0;
  const decoder = new FrameDecoder();
  void decoder;
  const link = new LinkLayer({
    variant: 'sibo',
    sendBytes: b => {
      const f = decodePdu(stripFrame(b));
      if (f && f.cont === 0) acks++;
    },
    onData: d => delivered.push(hex(d)),
  });
  link.initiate();
  link.handleFrame(new Uint8Array([0x20]));   // connected
  const frame = encodePdu(dataPdu(1, new Uint8Array([0xAA, 0xBB])));
  void frame;
  link.handleFrame(new Uint8Array([0x31, 0xAA, 0xBB]));   // Data seq 1
  link.handleFrame(new Uint8Array([0x31, 0xAA, 0xBB]));   // retransmit
  link.handleFrame(new Uint8Array([0x31, 0xAA, 0xBB]));   // retransmit again, same poll batch
  link.handleFrame(new Uint8Array([0x31, 0xAA, 0xBB]));   // …and again
  check('duplicate delivered once', delivered.length === 1, String(delivered.length));
  // Handshake Ack + the new frame's Ack went out at once; the three copies'
  // re-acks are held for the end of the poll batch rather than sent per copy.
  check('new frame acked at once', acks === 2, String(acks));
  link.flushAck();
  check('duplicates re-acked once per batch', acks === 3, String(acks));
  link.flushAck();
  check('nothing left to flush', acks === 3, String(acks));
}

// ── A peer Disc_Pdu on an established link is reported as a reset ──────
// The peer's follow-up Req_Pdu finds the link idle, so without this the upper
// layers never learn their NCP channels are gone.
{
  let resets = 0;
  const link = new LinkLayer({
    variant: 'sibo',
    sendBytes: () => { /* discard */ },
    onData: () => { /* unused */ },
    onReset: () => { resets++; },
  });
  link.initiate();
  link.handleFrame(new Uint8Array([0x20]));   // connected
  link.handleFrame(new Uint8Array([0x10]));   // peer Disc_Pdu
  check('Disc on a live link reports a reset', resets === 1, String(resets));
  link.handleFrame(new Uint8Array([0x20]));   // peer re-handshakes (link idle → connected)
  check('the re-handshake that follows is not a second reset', resets === 1, String(resets));
  link.handleFrame(new Uint8Array([0x10]));
  link.handleFrame(new Uint8Array([0x10]));   // Disc on an already-idle link
  check('Disc on an idle link reports nothing', resets === 2, String(resets));
}

function stripFrame(wire: Uint8Array): Uint8Array {
  // SYN DLE STX <payload, DLE-stuffed> DLE ETX CRC2 — tests only feed
  // frames without DLE bytes in the payload, so simple slicing works.
  return wire.subarray(3, wire.length - 4);
}

process.exit(failures === 0 ? 0 : 1);
