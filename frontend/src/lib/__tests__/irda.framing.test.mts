// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// IrDA SIR async-framing round-trip + FCS + decoder edge cases. Run via:
//   node --experimental-strip-types \
//       frontend/src/lib/__tests__/irda.framing.test.mts

import {
  encodeSirFrame,
  SirDecoder,
  fcs16,
  fcsValid,
  BOF,
  EOF,
  CE,
} from '../irda/framing.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) {
    console.error(`FAIL: ${msg}`);
    failures++;
  }
}
function eqBytes(a: Uint8Array, b: Uint8Array, msg: string) {
  if (a.length !== b.length) {
    console.error(`FAIL: ${msg}: length ${a.length} vs ${b.length}`);
    failures++;
    return;
  }
  for (let i = 0; i < a.length; i++) {
    if (a[i] !== b[i]) {
      console.error(
        `FAIL: ${msg}: byte ${i} is 0x${a[i].toString(16)}, want 0x${b[i].toString(16)}`,
      );
      failures++;
      return;
    }
  }
}

// 1. HDLC FCS "good FCS" magic. For any frame, the running CRC over the
//    data followed by its (complemented, LSB-first) FCS must equal the
//    standard 0xF0B8 residue.
{
  const body = new Uint8Array([0xff, 0x01, 0x42, 0x99]);
  const crc = fcs16(body) ^ 0xffff;
  const withFcs = new Uint8Array([...body, crc & 0xff, (crc >> 8) & 0xff]);
  check(fcs16(withFcs) === 0xf0b8, `good-FCS residue = 0x${fcs16(withFcs).toString(16)} (want 0xf0b8)`);
  check(fcsValid(withFcs), 'fcsValid accepts a correctly-checksummed frame');
}

// 2. Encoder shape — leading BOF(s), real BOF, body, FCS, trailing EOF,
//    no escaping needed for plain bytes.
{
  const body = new Uint8Array([0x01, 0x02, 0x03]);
  const frame = encodeSirFrame(body);
  check(frame[0] === BOF, 'frame begins with an (x)BOF');
  check(frame[frame.length - 1] === EOF, 'frame ends with EOF');
  // Body should appear verbatim somewhere after the BOF run.
  check(frame[2] === 0x01 && frame[3] === 0x02 && frame[4] === 0x03, 'plain body bytes intact');
}

// 3. Transparency: control bytes in the body must be escaped CE / b^0x20.
{
  const body = new Uint8Array([BOF, EOF, CE, 0x00]);
  const frame = encodeSirFrame(body);
  // First body byte is BOF(0xc0) → CE, 0xe0.
  check(frame[2] === CE && frame[3] === (BOF ^ 0x20), '0xC0 in body escaped as 7D E0');
  check(frame[4] === CE && frame[5] === (EOF ^ 0x20), '0xC1 in body escaped as 7D E1');
  check(frame[6] === CE && frame[7] === (CE ^ 0x20), '0x7D in body escaped as 7D 5D');
}

// 4. Round-trip across many bodies, including all control bytes and an
//    empty body, smashed back-to-back into one decoder feed().
{
  const cases = [
    new Uint8Array([0x00]),
    new Uint8Array([BOF]),
    new Uint8Array([EOF]),
    new Uint8Array([CE]),
    new Uint8Array([BOF, EOF, CE, BOF, EOF, CE]),
    new Uint8Array(Array.from({ length: 64 }, (_, i) => i & 0xff)),
    new Uint8Array(Array.from({ length: 300 }, () => Math.floor(Math.random() * 256))),
  ];
  const d = new SirDecoder();
  let combined = new Uint8Array(0);
  for (const p of cases) {
    const f = encodeSirFrame(p);
    const merged = new Uint8Array(combined.length + f.length);
    merged.set(combined, 0);
    merged.set(f, combined.length);
    combined = merged;
  }
  const decoded = d.feed(combined);
  check(decoded.length === cases.length, `decoded ${decoded.length} frames, want ${cases.length}`);
  check(d.badFcs === 0, 'no FCS errors in clean roundtrip');
  check(d.framingErrors === 0, 'no framing errors in clean roundtrip');
  for (let i = 0; i < Math.min(decoded.length, cases.length); i++) {
    eqBytes(decoded[i].body, cases[i], `case ${i} roundtrip`);
  }
}

// 5. Decoder survives byte-at-a-time chunking (mimics the UART poll
//    delivering bytes in unpredictable bursts).
{
  const body = new Uint8Array([0x11, BOF, 0x22, CE, 0x33]);
  const f = encodeSirFrame(body);
  const d = new SirDecoder();
  let collected: Uint8Array | null = null;
  for (let i = 0; i < f.length; i++) {
    const frames = d.feed(f.subarray(i, i + 1));
    if (frames.length === 1) collected = frames[0].body;
  }
  check(collected !== null, 'frame survives byte-at-a-time chunking');
  if (collected) eqBytes(collected, body, 'chunked frame body matches');
}

// 6. A corrupted FCS is rejected and counted, not surfaced as a frame.
{
  const body = new Uint8Array([0xde, 0xad, 0xbe, 0xef]);
  const f = encodeSirFrame(body);
  const corrupted = new Uint8Array(f);
  corrupted[corrupted.length - 2] ^= 0xff; // flip a (likely) FCS byte
  const d = new SirDecoder();
  const decoded = d.feed(corrupted);
  check(decoded.length === 0, 'corrupted FCS drops the frame');
  check(d.badFcs === 1, `badFcs=${d.badFcs}, want 1`);
}

// 7. Extra BOF / line idle before a frame is ignored.
{
  const body = new Uint8Array([0xca, 0xfe]);
  const f = encodeSirFrame(body);
  const noisy = new Uint8Array([BOF, BOF, BOF, 0xff, ...f]);
  const d = new SirDecoder();
  const decoded = d.feed(noisy);
  check(decoded.length === 1, `noisy/xBOF prefix: decoded ${decoded.length} frame(s)`);
  if (decoded.length === 1) eqBytes(decoded[0].body, body, 'frame survives xBOF prefix');
}

if (failures > 0) {
  console.error(`\n${failures} test failure(s)`);
  process.exit(1);
}
console.log('OK — all IrDA framing tests passed');
