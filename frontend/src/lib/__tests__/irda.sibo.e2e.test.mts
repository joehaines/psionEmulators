// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// SIBO "Psion IRLink" beam round-trip: drives the production
// IrSiboSendClient (host sender) straight into the production
// IrReceiveClient (host responder, SIBO branch) over an in-process byte
// pipe — proving the two ends of our beam implementation interoperate.
// The wire dialect both sides speak is pinned against the live Series
// 3c v5.20f (see docs/sibo-remote-link.md): XID discovery → SNRM/UA →
// LM-Connect "Psion IRLink 1.00" (echo-confirmed) → length-prefixed
// header/body/zero-EOF stream on LSAP 1 → DISC.
//
// Run: node --experimental-strip-types frontend/src/lib/__tests__/irda.sibo.e2e.test.mts

import { IrSiboSendClient } from '../irda/sibosend.ts';
import { IrReceiveClient } from '../irda/receive.ts';

let failures = 0;
function check(name: string, cond: boolean, extra?: string) {
  if (cond) console.log(`ok   ${name}`);
  else { console.error(`FAIL ${name}${extra ? ` — ${extra}` : ''}`); failures++; }
}

// Byte pipes: sender → receiver and back.
let toReceiver: number[] = [];
let toSender: number[] = [];
const drain = (buf: number[]): Uint8Array => {
  if (buf.length === 0) return new Uint8Array(0);
  const out = new Uint8Array(buf);
  buf.length = 0;
  return out;
};

const receiver = new IrReceiveClient(
  () => drain(toReceiver),
  d => { for (const b of d) toSender.push(b); return d.length; },
  { pollHz: 200, nickname: 'PsionWeb' },
);
receiver.start();

const sender = new IrSiboSendClient(
  () => drain(toSender),
  d => { for (const b of d) toReceiver.push(b); return d.length; },
);

const BODY = new TextEncoder().encode(
  'SIBO beam round-trip payload — long enough to span several 62-byte '
  + 'IrLAP I-frames and multiple 200-byte stream chunks. '.repeat(8));

async function main() {
  const recvP = receiver.receive({
    onFileInfo: (name, size) => check('header parsed',
      name === 'NOTES.WRD' && size === BODY.length, `${name} ${size}`),
  });

  await sender.sendFile('M:\\WRD\\NOTES.WRD', BODY);
  check('sender completed', true);

  const f = await Promise.race([
    recvP,
    new Promise<never>((_, rej) => setTimeout(() => rej(new Error('receive timed out')), 5_000)),
  ]);
  check('file name', f.name === 'NOTES.WRD', f.name);
  check('file size', f.data.length === BODY.length, String(f.data.length));
  let same = f.data.length === BODY.length;
  for (let i = 0; same && i < BODY.length; i++) same = f.data[i] === BODY[i];
  check('payload bytes identical', same);
  process.exit(failures === 0 ? 0 : 1);
}

main().catch(e => { console.error('FAIL', e); process.exit(1); });
