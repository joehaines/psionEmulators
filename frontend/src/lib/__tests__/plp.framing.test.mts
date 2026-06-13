// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// PLP framing round-trip + decoder edge cases. Run via:
//   node --experimental-strip-types \
//       frontend/src/lib/__tests__/plp.framing.test.mts

import { encodeFrame, FrameDecoder, crc16Ccitt } from '../plp/framing.ts';
import { DLE, STX, ETX } from '../plp/types.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}
function eqBytes(a: Uint8Array, b: Uint8Array, msg: string) {
  if (a.length !== b.length) {
    console.error(`FAIL: ${msg}: length ${a.length} vs ${b.length}`);
    failures++;
    return;
  }
  for (let i = 0; i < a.length; i++) {
    if (a[i] !== b[i]) {
      console.error(`FAIL: ${msg}: byte ${i} is 0x${a[i].toString(16)}, want 0x${b[i].toString(16)}`);
      failures++;
      return;
    }
  }
}

// 1. CRC-16-CCITT spot-check. The classic "123456789" reference vector
//    with seed 0x0000 produces 0x31C3 — anyone tweaking the CRC routine
//    will see this test trip immediately.
{
  const v = new TextEncoder().encode('123456789');
  const crc = crc16Ccitt(v);
  check(crc === 0x31C3, `CRC-16-CCITT("123456789") = 0x${crc.toString(16)} (want 0x31C3)`);
}

// 2. Encoder shape — SYN prefix + DLE STX prologue + DLE ETX terminator
//    + big-endian CRC trailer, no escaping needed for simple bytes.
{
  const payload = new Uint8Array([0x01, 0x02, 0x03]);
  const frame = encodeFrame(payload);
  check(frame[0] === 0x16, 'frame begins with SYN (0x16) preamble');
  check(frame[1] === DLE && frame[2] === STX, 'DLE STX follows SYN');
  check(frame[3] === 0x01 && frame[4] === 0x02 && frame[5] === 0x03, 'payload bytes intact');
  check(frame[6] === DLE && frame[7] === ETX, 'frame ends DLE ETX before CRC');
  const crc = crc16Ccitt(payload);
  // Wire order is big-endian (CRC-hi, CRC-lo) — locked in against a
  // real Revo capture showing payload [0x21] producing trailer 34 43
  // (== 0x3443, which is crc16Ccitt([0x21])).
  check(frame[8] === ((crc >> 8) & 0xFF) && frame[9] === (crc & 0xFF), 'CRC trailer is big-endian');
  check(frame.length === 10, 'no spurious padding (length 10 for 3-byte payload incl. SYN prefix)');
}

// 2b. Captured-from-real-Revo regression vector. The device repeatedly
//     emitted `16 10 02 21 10 03 34 43` waiting for a link-layer ACK;
//     decoding it must produce a single 1-byte frame [0x21] with zero
//     CRC errors. (The leading 0x16 is the SYN line-noise byte the
//     decoder skips while hunting for DLE STX.)
{
  const captured = new Uint8Array([0x16, 0x10, 0x02, 0x21, 0x10, 0x03, 0x34, 0x43]);
  const d = new FrameDecoder();
  const decoded = d.feed(captured);
  check(decoded.length === 1, `captured Revo frame: decoded ${decoded.length} frame(s), want 1`);
  check(d.badCrc === 0, `captured Revo frame: badCrc=${d.badCrc}, want 0`);
  if (decoded.length === 1) {
    check(decoded[0].payload.length === 1 && decoded[0].payload[0] === 0x21,
          `captured Revo frame payload: got [${Array.from(decoded[0].payload).map(b => b.toString(16)).join(',')}], want [21]`);
  }
}

// 3. DLE inside the payload must be doubled.
{
  const payload = new Uint8Array([0xAA, DLE, 0xBB]);
  const frame = encodeFrame(payload);
  // Expect SYN | DLE STX | AA DLE DLE BB | DLE ETX | crc-hi crc-lo → 11 bytes
  check(frame.length === 11, 'stuffed frame is exactly one byte longer than the bare-byte version');
  check(frame[4] === DLE && frame[5] === DLE, 'literal DLE is escaped DLE DLE');
}

// 4. Roundtrip across many payloads, including DLE/STX/ETX boundary
//    bytes and empty payloads.
{
  const cases = [
    new Uint8Array(0),
    new Uint8Array([0x00]),
    new Uint8Array([DLE]),
    new Uint8Array([DLE, DLE]),
    new Uint8Array([DLE, STX, DLE, ETX]),
    new Uint8Array([0x16, 0x10, 0x02, 0x10, 0x03]),
    new Uint8Array(Array.from({ length: 64 }, (_, i) => i & 0xFF)),
    new Uint8Array(Array.from({ length: 512 }, () => Math.floor(Math.random() * 256))),
  ];
  const d = new FrameDecoder();
  // Smash them all into the decoder back-to-back to verify it can
  // handle multiple frames per feed() call without bleeding state.
  let combined = new Uint8Array(0);
  for (const p of cases) {
    const f = encodeFrame(p);
    const merged = new Uint8Array(combined.length + f.length);
    merged.set(combined, 0);
    merged.set(f, combined.length);
    combined = merged;
  }
  const decoded = d.feed(combined);
  check(decoded.length === cases.length, `decoded ${decoded.length} frames, want ${cases.length}`);
  check(d.badCrc === 0, 'no CRC errors in clean roundtrip');
  check(d.framingErrors === 0, 'no framing errors in clean roundtrip');
  for (let i = 0; i < Math.min(decoded.length, cases.length); i++) {
    eqBytes(decoded[i].payload, cases[i], `case ${i} roundtrip`);
  }
}

// 5. The decoder ignores line noise before the prologue.
{
  const payload = new Uint8Array([0xCA, 0xFE]);
  const f = encodeFrame(payload);
  const noisy = new Uint8Array([0x00, 0xFF, 0x55, ...f]);
  const d = new FrameDecoder();
  const decoded = d.feed(noisy);
  check(decoded.length === 1, `noisy prefix: decoded ${decoded.length} frame(s)`);
  if (decoded.length === 1) eqBytes(decoded[0].payload, payload, 'frame survives noisy prefix');
}

// 6. Decoder handles a frame fed in arbitrary chunk sizes (mimics the
//    real WASM bridge poll behaviour: bytes arrive in unpredictable
//    bursts).
{
  const payload = new Uint8Array([0x11, DLE, 0x22, 0x33, DLE, 0x44]);
  const f = encodeFrame(payload);
  const d = new FrameDecoder();
  let collected: Uint8Array | null = null;
  for (let i = 0; i < f.length; i++) {
    const frames = d.feed(f.subarray(i, i + 1));
    if (frames.length === 1) collected = frames[0].payload;
  }
  check(collected !== null, 'frame survives byte-at-a-time chunking');
  if (collected) eqBytes(collected, payload, 'chunked frame payload matches');
}

// 7. Corrupted CRC is rejected, not surfaced as a frame.
{
  const payload = new Uint8Array([0xDE, 0xAD]);
  const f = encodeFrame(payload);
  const corrupted = new Uint8Array(f);
  corrupted[corrupted.length - 1] ^= 0xFF; // flip the CRC high byte
  const d = new FrameDecoder();
  const decoded = d.feed(corrupted);
  check(decoded.length === 0, 'corrupted CRC drops frame');
  check(d.badCrc === 1, 'badCrc counter incremented');
}

// 8. Spurious DLE-followed-by-non-DLE-non-ETX inside payload is a
//    framing error and the decoder resyncs to the next prologue.
{
  // Manually craft: DLE STX | 0xAA | DLE 0x05 (bad escape) — should
  // resync, then a valid frame follows.
  const bad = new Uint8Array([DLE, STX, 0xAA, DLE, 0x05]);
  const good = encodeFrame(new Uint8Array([0x42]));
  const combined = new Uint8Array(bad.length + good.length);
  combined.set(bad, 0);
  combined.set(good, bad.length);
  const d = new FrameDecoder();
  const decoded = d.feed(combined);
  check(d.framingErrors === 1, `framingErrors=${d.framingErrors}, want 1`);
  check(decoded.length === 1, 'decoder recovers and emits the following good frame');
  if (decoded.length === 1) eqBytes(decoded[0].payload, new Uint8Array([0x42]), 'post-error frame payload');
}

if (failures > 0) {
  console.error(`\n${failures} test failure(s)`);
  process.exit(1);
}
console.log('OK — all PLP framing tests passed');
