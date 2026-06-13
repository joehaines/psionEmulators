// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// State-bundle round-trip tests.
// Run via: node --experimental-strip-types frontend/src/lib/__tests__/stateBundle.test.mts

import {
  encodeBundle,
  decodeBundle,
  compressChunk,
  decompressChunk,
  _internal,
} from '../stateBundle.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}
function eqBytes(actual: Uint8Array | undefined, expected: Uint8Array, msg: string) {
  if (!actual) { console.error(`FAIL: ${msg}: actual is undefined`); failures++; return; }
  if (actual.length !== expected.length) {
    console.error(`FAIL: ${msg}: length ${actual.length} != ${expected.length}`);
    failures++;
    return;
  }
  for (let i = 0; i < actual.length; i++) {
    if (actual[i] !== expected[i]) {
      console.error(`FAIL: ${msg}: differ at byte ${i} (got ${actual[i]}, want ${expected[i]})`);
      failures++;
      return;
    }
  }
}

async function readBlob(b: Blob): Promise<Uint8Array> {
  return new Uint8Array(await b.arrayBuffer());
}

// ── round-trip: single device with heap only ───────────────────────────
{
  const heapRaw = new Uint8Array(64 * 1024);
  for (let i = 0; i < heapRaw.length; i++) heapRaw[i] = (i * 31 + 7) & 0xff;
  const heapCompressed = await compressChunk(heapRaw);

  const blob = await encodeBundle([
    { id: '5mx', displayName: 'Psion Series 5mx', schemaVersion: 5, heap: heapCompressed },
  ]);

  const bytes = await readBlob(blob);
  // First 8 bytes should match the magic.
  for (let i = 0; i < _internal.MAGIC.length; i++) {
    check(bytes[i] === _internal.MAGIC[i], `magic byte ${i}`);
  }

  const decoded = await decodeBundle(bytes);
  check(decoded.header.bundleVersion === _internal.BUNDLE_VERSION, 'bundleVersion');
  check(decoded.header.devices.length === 1, 'device count');
  check(decoded.header.devices[0].id === '5mx', 'device id');
  check(decoded.header.devices[0].displayName === 'Psion Series 5mx', 'displayName');

  const chunks = decoded.devices.get('5mx');
  check(!!chunks, 'chunks present for 5mx');
  // The decoded heap is still gzip-compressed; decompress and compare.
  const heapBack = await decompressChunk(chunks!.heap!);
  eqBytes(heapBack, heapRaw, 'heap round-trip');
}

// ── round-trip: multiple devices with mixed chunk types ────────────────
{
  // Heaps must be passed already gzipped to mirror what IDB stores.
  const heap3a = await compressChunk(makePattern(0xa1, 1024));
  const heap5mx = await compressChunk(makePattern(0x5e, 2048));
  const cfImg = makePattern(0xcf, 4096);
  const ssdImg = makePattern(0x55, 1024);
  const dpImg = makePattern(0xdd, 512);

  const blob = await encodeBundle([
    { id: 'series3a', schemaVersion: 5, heap: heap3a, ssd0: ssdImg, datapak0: dpImg },
    { id: '5mx', schemaVersion: 5, heap: heap5mx, cf: cfImg },
  ]);

  const bytes = await readBlob(blob);
  const decoded = await decodeBundle(bytes);
  check(decoded.header.devices.length === 2, '2 devices in header');

  const c1 = decoded.devices.get('series3a')!;
  check(!!c1.heap, 'series3a heap');
  check(!!c1.ssd0, 'series3a ssd0');
  check(!!c1.datapak0, 'series3a datapak0');
  check(!c1.cf, 'series3a no cf');
  check(!c1.ssd1, 'series3a no ssd1');

  eqBytes(await decompressChunk(c1.heap!), await decompressChunk(heap3a), 'series3a heap value');
  eqBytes(await decompressChunk(c1.ssd0!), ssdImg, 'series3a ssd0 value');
  eqBytes(await decompressChunk(c1.datapak0!), dpImg, 'series3a datapak0 value');

  const c2 = decoded.devices.get('5mx')!;
  check(!!c2.heap, '5mx heap');
  check(!!c2.cf, '5mx cf');
  eqBytes(await decompressChunk(c2.cf!), cfImg, '5mx cf value');
}

// ── empty chunks are skipped ───────────────────────────────────────────
{
  const blob = await encodeBundle([
    {
      id: 'revo', schemaVersion: 5,
      heap: await compressChunk(new Uint8Array([1, 2, 3])),
      cf: new Uint8Array(0),       // empty: must NOT appear in bundle
      ssd0: undefined,
    },
  ]);
  const bytes = await readBlob(blob);
  const decoded = await decodeBundle(bytes);
  const c = decoded.devices.get('revo')!;
  check(!!c.heap, 'revo heap kept');
  check(!c.cf, 'empty cf dropped');
  check(!c.ssd0, 'absent ssd0 dropped');
}

// ── legacy v1 JSON bundle is still readable and tagged v4 ──────────────
// The OLD build (with the save-race bug) emitted bundles in this shape.
// Users who downloaded one before the fix landed must still be able to
// re-import it on the current build — the heap layout hasn't changed
// across v4 → v5 (the v5 bump was an IDB-shape change only).
{
  const heapRaw = makePattern(0x42, 8192);
  const heapGz = await compressChunk(heapRaw);
  const legacy = {
    bundleVersion: 1,
    schemaVersion: 4,
    savedAt: '2026-01-01T00:00:00.000Z',
    devices: {
      revo: {
        displayName: 'Psion Revo',
        heap: u8ToBase64(heapGz),
      },
    },
  };
  const text = new TextEncoder().encode(JSON.stringify(legacy));
  const decoded = await decodeBundle(text);
  check(decoded.header.devices.length === 1, 'legacy: one device');
  check(decoded.header.devices[0].id === 'revo', 'legacy: id parsed');
  // schemaVersion must come through as 4 so the caller's upgrade-in-
  // place logic can recognise it.
  check(decoded.header.devices[0].schemaVersion === 4, 'legacy: schemaVersion=4 preserved');
  const c = decoded.devices.get('revo')!;
  eqBytes(await decompressChunk(c.heap!), heapRaw, 'legacy heap round-trip');
}

// ── decoder rejects truncated files ────────────────────────────────────
{
  // Build a valid bundle, then chop the last few bytes to simulate
  // truncation. The decoder must throw rather than return garbage.
  const blob = await encodeBundle([
    { id: 'x', schemaVersion: 5, heap: await compressChunk(makePattern(1, 512)) },
  ]);
  const full = await readBlob(blob);
  const chopped = full.subarray(0, full.length - 8);
  let threw = false;
  try { await decodeBundle(chopped); }
  catch { threw = true; }
  check(threw, 'truncated bundle should throw');
}

// ── decoder rejects unknown bundle version ─────────────────────────────
{
  // Build a valid bundle, then poke a bad version into the header JSON.
  const blob = await encodeBundle([
    { id: 'x', schemaVersion: 5, heap: await compressChunk(makePattern(2, 64)) },
  ]);
  const bytes = await readBlob(blob);
  // Locate the JSON header inline.
  const headerLen = new DataView(bytes.buffer, bytes.byteOffset + _internal.MAGIC.length, _internal.HEADER_LEN_BYTES).getUint32(0, true);
  const start = _internal.MAGIC.length + _internal.HEADER_LEN_BYTES;
  const headerText = new TextDecoder().decode(bytes.subarray(start, start + headerLen));
  const tampered = headerText.replace('"bundleVersion":2', '"bundleVersion":99');
  const newHeader = new TextEncoder().encode(tampered);
  // For simplicity, only proceed if the patched JSON is exactly the same
  // size (otherwise offsets in the payload would shift). The replacement
  // adds two bytes, so we also have to bump the header length field.
  const modified = new Uint8Array(start + newHeader.length + (bytes.length - start - headerLen));
  modified.set(bytes.subarray(0, _internal.MAGIC.length), 0);
  new DataView(modified.buffer, _internal.MAGIC.length, _internal.HEADER_LEN_BYTES).setUint32(0, newHeader.length, true);
  modified.set(newHeader, start);
  modified.set(bytes.subarray(start + headerLen), start + newHeader.length);
  let threw = false;
  try { await decodeBundle(modified); }
  catch { threw = true; }
  check(threw, 'unknown bundleVersion should throw');
}

// ── encoder does not OOM on a "many devices" workload ─────────────────
// Synthesise 17 devices × 256 KiB heap (~4 MiB compressed worst case).
// The original JSON exporter would have allocated a >5 MiB base64 string
// per device for this; the new path keeps the data as Blob parts and
// the test passes with bounded memory.
{
  const devs = [];
  for (let i = 0; i < 17; i++) {
    const heap = await compressChunk(makePattern((i * 17) & 0xff, 256 * 1024));
    devs.push({ id: `dev${i}`, schemaVersion: 5, heap });
  }
  const blob = await encodeBundle(devs);
  check(blob.size > 0, 'multi-device bundle is non-empty');
  const decoded = await decodeBundle(await readBlob(blob));
  check(decoded.header.devices.length === 17, '17 devices round-trip');
  for (let i = 0; i < 17; i++) {
    const c = decoded.devices.get(`dev${i}`);
    check(!!c?.heap, `dev${i} heap present`);
  }
}

// ── decoded chunks own independent ArrayBuffers (no subarray sharing) ─
// Regression test: the decoder previously used bytes.subarray() which
// returned views into the file's ArrayBuffer. When stored in IndexedDB
// via structured clone, each entry would duplicate the entire file,
// causing multi-GB storage bloat and failed restores.
{
  const heap1 = await compressChunk(makePattern(0x11, 2048));
  const cf1 = makePattern(0xcc, 4096);
  const heap2 = await compressChunk(makePattern(0x22, 1024));

  const blob = await encodeBundle([
    { id: 'dev1', schemaVersion: 5, heap: heap1, cf: cf1 },
    { id: 'dev2', schemaVersion: 5, heap: heap2 },
  ]);

  const fileBytes = await readBlob(blob);
  const decoded = await decodeBundle(fileBytes);

  const c1 = decoded.devices.get('dev1')!;
  const c2 = decoded.devices.get('dev2')!;

  // Each chunk's backing ArrayBuffer must be exactly the chunk's size,
  // not the entire file's size.
  check(
    c1.heap!.buffer.byteLength === c1.heap!.byteLength,
    `dev1 heap buffer should be ${c1.heap!.byteLength}, got ${c1.heap!.buffer.byteLength}`,
  );
  check(
    c1.cf!.buffer.byteLength === c1.cf!.byteLength,
    `dev1 cf buffer should be ${c1.cf!.byteLength}, got ${c1.cf!.buffer.byteLength}`,
  );
  check(
    c2.heap!.buffer.byteLength === c2.heap!.byteLength,
    `dev2 heap buffer should be ${c2.heap!.byteLength}, got ${c2.heap!.buffer.byteLength}`,
  );

  // None of the chunks should share a buffer with the original file.
  check(c1.heap!.buffer !== fileBytes.buffer, 'dev1 heap must not alias file buffer');
  check(c1.cf!.buffer !== fileBytes.buffer, 'dev1 cf must not alias file buffer');
  check(c2.heap!.buffer !== fileBytes.buffer, 'dev2 heap must not alias file buffer');
}

// ── helpers ────────────────────────────────────────────────────────────
function makePattern(seed: number, length: number): Uint8Array {
  const out = new Uint8Array(length);
  let s = seed | 1;
  for (let i = 0; i < length; i++) {
    s = (s * 1103515245 + 12345) & 0x7fffffff;
    out[i] = s & 0xff;
  }
  return out;
}

function u8ToBase64(data: Uint8Array): string {
  let bin = '';
  const chunk = 8192;
  for (let i = 0; i < data.length; i += chunk) {
    bin += String.fromCharCode(...data.subarray(i, i + chunk));
  }
  return btoa(bin);
}

if (failures > 0) {
  console.error(`\n${failures} test(s) failed`);
  process.exit(1);
}
console.log('stateBundle tests passed');
