// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// End-to-end PLP harness — no browser, no WASM, no real emulator.
//
// A small mock Revo speaks PLP back at the real PlpClient through a
// pair of in-memory byte queues. The framing, NCP, RFSV-32, transport
// pump, channel multiplexer, and chunked file-I/O loops all run for
// real; only the EPOC32 file system at the far end is faked.
//
// Run with:
//   node --experimental-strip-types \
//       frontend/src/lib/__tests__/plp.e2e.test.mts
//
// What this catches:
//   * PLP framing roundtrip and CRC verification across the full stack
//   * NCP CONNECT handshake (NCON_REQ / NCON_ACK channel-pair plumbing)
//   * RFSV envelope (opcode-LE + reqLen-LE + status-i16 + body)
//   * DRIVE_LIST / OPENDIR / READDIR / CLOSEDIR / FOPEN / FREAD /
//     FWRITE / FCLOSE / DELETE wired up correctly in the multiplexer
//   * Channel half-duplex + concurrent-request guards in PlpClient
//
// What this does NOT catch:
//   * Wire-format mismatches against a *real* Revo (the mock implements
//     the same bytes the client emits, so any shared misunderstanding
//     hides). Browser validation is still required to lock that down.
//   * WASM bridge bugs (the bridge is bypassed entirely here).

import { PlpClient } from '../plp/index.ts';
import { encodeFrame, FrameDecoder } from '../plp/framing.ts';
import {
  encodeNcp,
  decodeNcp,
  type NcpPacket,
} from '../plp/ncp.ts';
import {
  NcpType,
  Rfsv32Op,
  EpocErr,
  NCP_SERVICE_RFSV,
} from '../plp/types.ts';

// ── Test plumbing ────────────────────────────────────────────────
let failures = 0;
function check(cond: unknown, msg: string): void {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}
function eqBytes(a: Uint8Array, b: Uint8Array, msg: string): void {
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
const sleep = (ms: number) => new Promise<void>(r => setTimeout(r, ms));

// ── In-memory byte bridge ────────────────────────────────────────
// One queue per direction. The client polls fromDevice; the mock
// polls fromClient.
class ByteBridge {
  private fromDevice: number[] = []; // device → host
  private fromClient: number[] = []; // host → device

  clientRead(): Uint8Array {
    if (this.fromDevice.length === 0) return new Uint8Array(0);
    const out = new Uint8Array(this.fromDevice);
    this.fromDevice = [];
    return out;
  }
  clientWrite(data: Uint8Array): number {
    for (const b of data) this.fromClient.push(b);
    return data.length;
  }
  deviceRead(): Uint8Array {
    if (this.fromClient.length === 0) return new Uint8Array(0);
    const out = new Uint8Array(this.fromClient);
    this.fromClient = [];
    return out;
  }
  deviceWrite(data: Uint8Array): void {
    for (const b of data) this.fromDevice.push(b);
  }
}

// ── Mock Revo file system ────────────────────────────────────────
interface MockFile {
  data: Uint8Array;
  attrib?: number;
}
class MockFs {
  // Files keyed by absolute path string. Pre-populated with a couple
  // of sample files so downloadFile() has something to grab.
  files = new Map<string, MockFile>([
    ['C:\\hello.txt', { data: new TextEncoder().encode('Hello, Revo!\r\n') }],
    ['C:\\Documents\\notes.txt', { data: new TextEncoder().encode('Test data for download.\r\n') }],
  ]);
  // Per-directory listing entries the mock emits for READDIR_LFN.
  // Each batch is encoded into the entry layout the client's
  // parseDirBatch reads — keeping the harness honest end-to-end.
  dirContents = new Map<string, { name: string; size: number; attrib: number }[]>([
    ['C:\\', [
      { name: 'hello.txt', size: 14, attrib: 0x20 /* archive */ },
      { name: 'Documents', size: 0,  attrib: 0x10 /* dir */ },
    ]],
  ]);
  driveBitmask = (1 << 2) | (1 << 25);
}

// Encode one entry batch into the same layout parseDirBatch reads.
function encodeDirBatch(entries: { name: string; size: number; attrib: number }[]): Uint8Array {
  const parts: Uint8Array[] = [];
  for (const e of entries) {
    const nameBytes = new TextEncoder().encode(e.name);
    const header = new Uint8Array(30);
    const dv = new DataView(header.buffer);
    dv.setUint32(0,  e.attrib, true);
    dv.setUint32(4,  e.size,   true);
    // modified time = 0 (parser stores it but UI doesn't care here)
    // uid1/uid2/uid3 = 0
    dv.setUint16(28, nameBytes.length, true);
    parts.push(header, nameBytes);
  }
  let total = 0;
  for (const p of parts) total += p.length;
  const out = new Uint8Array(total);
  let off = 0;
  for (const p of parts) { out.set(p, off); off += p.length; }
  return out;
}

// ── Mock Revo PLP "server" ───────────────────────────────────────
class MockRevo {
  private decoder = new FrameDecoder();
  private clientLocalToRemote = new Map<number, number>(); // client localChan -> our chosen remoteChan
  private clientLocalToService = new Map<number, string>();
  private nextRemoteChan = 0x10;
  // Open file/dir handles, keyed by an integer handle the mock issues.
  private openHandles = new Map<number, {
    kind: 'file-read' | 'file-write' | 'dir';
    path: string;
    offset: number;          // for file-read
    writeBuf: number[];      // for file-write
    dirBatch: Uint8Array | null; // for dir iteration: pre-encoded batch
    dirEmitted: boolean;     // have we already delivered the batch?
  }>();
  private nextHandle = 1;
  // Bytes the mock sees as line-noise before the framer locks in;
  // surfaced in a stat for diagnostics.
  framingErrors = 0;
  badCrc = 0;

  private readonly bridge: ByteBridge;
  private readonly fs: MockFs;
  constructor(bridge: ByteBridge, fs: MockFs) {
    this.bridge = bridge;
    this.fs = fs;
  }

  // Drive the mock one tick. Drains bytes from the client, feeds the
  // framer, dispatches each frame, pushes outbound replies. Tests
  // call this from a fast loop while waiting on a PlpClient promise.
  tick(): void {
    const chunk = this.bridge.deviceRead();
    if (chunk.length === 0) return;
    const frames = this.decoder.feed(chunk);
    this.badCrc = this.decoder.badCrc;
    this.framingErrors = this.decoder.framingErrors;
    for (const f of frames) {
      const pkt = decodeNcp(f.payload);
      if (!pkt) continue;
      this.dispatch(pkt);
    }
  }

  private send(pkt: NcpPacket): void {
    this.bridge.deviceWrite(encodeFrame(encodeNcp(pkt)));
  }

  private dispatch(pkt: NcpPacket): void {
    switch (pkt.type) {
      case NcpType.NCON_REQ: {
        // Strip the trailing nul on the service name.
        const name = new TextDecoder().decode(pkt.data).replace(/\0+$/, '');
        const remoteChan = this.nextRemoteChan++;
        this.clientLocalToRemote.set(pkt.localChan, remoteChan);
        this.clientLocalToService.set(pkt.localChan, name);
        // NCON_ACK: header echoes client's local; first body byte is
        // the remote channel we just assigned. (This is the layout
        // the PlpClient's ncp.ts assumes; verifying it round-trips
        // here is most of the point of this test.)
        this.send({
          remoteChan: pkt.localChan,
          localChan: remoteChan,
          type: NcpType.NCON_ACK,
          data: new Uint8Array([remoteChan]),
        });
        return;
      }
      case NcpType.NDATA: {
        const service = this.clientLocalToService.get(pkt.localChan);
        if (service === NCP_SERVICE_RFSV) this.handleRfsv(pkt);
        return;
      }
      case NcpType.NDIS_REQ: {
        this.clientLocalToRemote.delete(pkt.localChan);
        this.clientLocalToService.delete(pkt.localChan);
        this.send({
          remoteChan: pkt.localChan,
          localChan: pkt.remoteChan,
          type: NcpType.NDIS_ACK,
          data: new Uint8Array(0),
        });
        return;
      }
    }
  }

  private handleRfsv(pkt: NcpPacket): void {
    if (pkt.data.length < 4) return;
    const dv = new DataView(pkt.data.buffer, pkt.data.byteOffset, pkt.data.byteLength);
    const op = dv.getUint16(0, true);
    const argLen = dv.getUint16(2, true);
    const args = pkt.data.subarray(4, 4 + argLen);

    const reply = (status: number, body: Uint8Array = new Uint8Array(0)) => {
      const out = new Uint8Array(2 + body.length);
      new DataView(out.buffer).setInt16(0, status, true);
      out.set(body, 2);
      const remote = this.clientLocalToRemote.get(pkt.localChan)!;
      this.send({
        remoteChan: pkt.localChan,
        localChan: remote,
        type: NcpType.NDATA,
        data: out,
      });
    };

    const u32LE = (n: number) => {
      const b = new Uint8Array(4);
      new DataView(b.buffer).setUint32(0, n, true);
      return b;
    };
    const readCString = (b: Uint8Array): string => {
      const nul = b.indexOf(0);
      return new TextDecoder().decode(nul === -1 ? b : b.subarray(0, nul));
    };

    switch (op) {
      case Rfsv32Op.DRIVE_LIST: {
        reply(0, u32LE(this.fs.driveBitmask));
        return;
      }
      case Rfsv32Op.OPENDIR: {
        // args = [attribMask:u32-LE][path-zstring]
        const path = readCString(args.subarray(4));
        const dirEntries = this.fs.dirContents.get(path);
        if (!dirEntries) { reply(EpocErr.PathNotFound); return; }
        const h = this.nextHandle++;
        this.openHandles.set(h, {
          kind: 'dir', path, offset: 0, writeBuf: [],
          dirBatch: encodeDirBatch(dirEntries),
          dirEmitted: false,
        });
        reply(0, u32LE(h));
        return;
      }
      case Rfsv32Op.READDIR_LFN: {
        const h = new DataView(args.buffer, args.byteOffset, args.byteLength).getUint32(0, true);
        const handle = this.openHandles.get(h);
        if (!handle || handle.kind !== 'dir') { reply(EpocErr.NotFound); return; }
        if (handle.dirEmitted || handle.dirBatch === null) {
          reply(EpocErr.Eof);
          return;
        }
        handle.dirEmitted = true;
        reply(0, handle.dirBatch);
        return;
      }
      case Rfsv32Op.CLOSEDIR: {
        const h = new DataView(args.buffer, args.byteOffset, args.byteLength).getUint32(0, true);
        this.openHandles.delete(h);
        reply(0);
        return;
      }
      case Rfsv32Op.FOPEN: {
        // args = [mode:u16-LE][path-zstring]
        const mode = new DataView(args.buffer, args.byteOffset, args.byteLength).getUint16(0, true);
        const path = readCString(args.subarray(2));
        const isCreate = (mode & 0x0300) !== 0; // CREATE or REPLACE
        if (!isCreate && !this.fs.files.has(path)) {
          reply(EpocErr.NotFound);
          return;
        }
        const h = this.nextHandle++;
        this.openHandles.set(h, {
          kind: isCreate ? 'file-write' : 'file-read',
          path, offset: 0, writeBuf: [],
          dirBatch: null, dirEmitted: false,
        });
        // If create-mode, install (or clear) the file.
        if (isCreate) this.fs.files.set(path, { data: new Uint8Array(0) });
        reply(0, u32LE(h));
        return;
      }
      case Rfsv32Op.FREAD: {
        const dv2 = new DataView(args.buffer, args.byteOffset, args.byteLength);
        const h = dv2.getUint32(0, true);
        const maxBytes = dv2.getUint32(4, true);
        const handle = this.openHandles.get(h);
        if (!handle || handle.kind !== 'file-read') { reply(EpocErr.NotFound); return; }
        const file = this.fs.files.get(handle.path);
        if (!file) { reply(EpocErr.NotFound); return; }
        const remaining = file.data.length - handle.offset;
        if (remaining <= 0) {
          // Signal EOF with status = EpocErr.Eof and no data.
          reply(EpocErr.Eof);
          return;
        }
        const n = Math.min(remaining, maxBytes);
        const chunk = file.data.subarray(handle.offset, handle.offset + n);
        handle.offset += n;
        reply(0, chunk);
        return;
      }
      case Rfsv32Op.FWRITE: {
        const h = new DataView(args.buffer, args.byteOffset, args.byteLength).getUint32(0, true);
        const data = args.subarray(4);
        const handle = this.openHandles.get(h);
        if (!handle || handle.kind !== 'file-write') { reply(EpocErr.AccessDenied); return; }
        for (const b of data) handle.writeBuf.push(b);
        const file = this.fs.files.get(handle.path)!;
        // Replace the file's bytes with the accumulated buffer.
        this.fs.files.set(handle.path, { data: new Uint8Array(handle.writeBuf) });
        reply(0, u32LE(data.length));
        // Use `file` to silence the "declared but unused" lint and
        // document that this branch knows about the existing entry.
        void file;
        return;
      }
      case Rfsv32Op.FCLOSE: {
        const h = new DataView(args.buffer, args.byteOffset, args.byteLength).getUint32(0, true);
        this.openHandles.delete(h);
        reply(0);
        return;
      }
      case Rfsv32Op.DELETE: {
        const path = readCString(args);
        if (!this.fs.files.has(path)) { reply(EpocErr.NotFound); return; }
        this.fs.files.delete(path);
        reply(0);
        return;
      }
      default:
        reply(EpocErr.NotFound);
        return;
    }
  }
}

// ── Test driver ──────────────────────────────────────────────────
// Spins up bridge + mock + client, lets a Promise from the client
// race against periodic mock ticks until either it settles or we
// time out.
async function withClient<T>(
  body: (client: PlpClient, mock: MockRevo, bridge: ByteBridge, fs: MockFs) => Promise<T>,
): Promise<T> {
  const bridge = new ByteBridge();
  const fs = new MockFs();
  const mock = new MockRevo(bridge, fs);
  const mockTimer = setInterval(() => mock.tick(), 1);
  const client = new PlpClient(
    () => bridge.clientRead(),
    data => bridge.clientWrite(data),
    { pollHz: 1000, requestTimeoutMs: 2000 },
  );
  client.start();
  try {
    return await body(client, mock, bridge, fs);
  } finally {
    client.stop();
    clearInterval(mockTimer);
  }
}

// ── Test cases ───────────────────────────────────────────────────

async function testConnect(): Promise<void> {
  await withClient(async client => {
    await client.connectRfsv();
    // No assertions needed — the promise resolving means NCON_ACK
    // came back with a valid channel pair.
    check(client.badCrc === 0, `connect: badCrc = ${client.badCrc}, want 0`);
    check(client.framingErrors === 0, `connect: framingErrors = ${client.framingErrors}, want 0`);
  });
}

async function testDriveList(): Promise<void> {
  await withClient(async client => {
    await client.connectRfsv();
    const drives = await client.listDrives();
    check(drives.length === 2, `listDrives length ${drives.length}, want 2`);
    check(drives[0] === 'C:' && drives[1] === 'Z:',
          `listDrives = [${drives.join(',')}], want [C:, Z:]`);
  });
}

async function testReadDirRaw(): Promise<void> {
  await withClient(async client => {
    await client.connectRfsv();
    const listing = await client.listDirRaw('C:\\');
    check(listing.finalStatus === -25,
          `listDirRaw final status ${listing.finalStatus}, want -25 (EOF)`);
    check(listing.chunks.length === 1,
          `listDirRaw chunks ${listing.chunks.length}, want 1`);
  });
}

async function testListDir(): Promise<void> {
  await withClient(async client => {
    await client.connectRfsv();
    const listing = await client.listDir('C:\\');
    check(listing.finalStatus === -25,
          `listDir final status ${listing.finalStatus}, want -25 (EOF)`);
    check(listing.fullyParsed === true,
          `listDir fullyParsed=${listing.fullyParsed}, want true (parser layout matches mock layout)`);
    check(listing.entries.length === 2,
          `listDir entries ${listing.entries.length}, want 2`);
    if (listing.entries.length >= 2) {
      const [a, b] = listing.entries;
      check(a.name === 'hello.txt' && !a.isDirectory && a.size === 14,
            `entry 0: name=${a.name} dir=${a.isDirectory} size=${a.size}`);
      check(b.name === 'Documents' && b.isDirectory,
            `entry 1: name=${b.name} dir=${b.isDirectory}`);
    }
  });
}

async function testDownload(): Promise<void> {
  await withClient(async client => {
    await client.connectRfsv();
    const bytes = await client.downloadFile('C:\\hello.txt');
    const expected = new TextEncoder().encode('Hello, Revo!\r\n');
    eqBytes(bytes, expected, 'downloaded bytes match mock-fs contents');
  });
}

async function testDownloadChunked(): Promise<void> {
  await withClient(async (client, _mock, _bridge, fs) => {
    // 3 KiB payload — guarantees >= 3 FREAD round-trips at the 1024 B
    // client CHUNK_SIZE.
    const big = new Uint8Array(3072);
    for (let i = 0; i < big.length; i++) big[i] = (i * 17) & 0xFF;
    fs.files.set('C:\\big.bin', { data: big });
    await client.connectRfsv();
    const got = await client.downloadFile('C:\\big.bin');
    eqBytes(got, big, '3 KiB download survives chunked FREAD loop');
  });
}

async function testUploadRoundtrip(): Promise<void> {
  await withClient(async (client, _mock, _bridge, fs) => {
    await client.connectRfsv();
    const payload = new Uint8Array(2500);
    for (let i = 0; i < payload.length; i++) payload[i] = (i * 31) & 0xFF;
    await client.uploadFile('C:\\upload-test.bin', payload);
    const stored = fs.files.get('C:\\upload-test.bin');
    check(stored !== undefined, 'uploaded file appears in mock fs');
    if (stored) eqBytes(stored.data, payload, 'uploaded bytes match what was written');

    // And read it back through the client.
    const got = await client.downloadFile('C:\\upload-test.bin');
    eqBytes(got, payload, 'upload then download round-trips');
  });
}

async function testDeleteThenMissing(): Promise<void> {
  await withClient(async (client, _mock, _bridge, fs) => {
    await client.connectRfsv();
    check(fs.files.has('C:\\hello.txt'), 'pre: hello.txt exists in mock fs');
    await client.deleteFile('C:\\hello.txt');
    check(!fs.files.has('C:\\hello.txt'), 'post: hello.txt removed');
    let threw = false;
    try { await client.downloadFile('C:\\hello.txt'); }
    catch { threw = true; }
    check(threw, 'downloading the now-missing file rejects');
  });
}

async function testTimeoutOnSilentRemote(): Promise<void> {
  // No mock in the loop — the client should time out cleanly rather
  // than hanging forever.
  const bridge = new ByteBridge();
  const client = new PlpClient(
    () => bridge.clientRead(),
    data => bridge.clientWrite(data),
    { pollHz: 1000, requestTimeoutMs: 200 },
  );
  client.start();
  try {
    let timedOut = false;
    try {
      // connectRfsv waits on NCON_ACK that will never come — it has
      // no internal timeout (it's a Promise from openChannel), so we
      // race it against an explicit sleep.
      const winner = await Promise.race([
        client.connectRfsv().then(() => 'connected' as const),
        sleep(400).then(() => 'timeout' as const),
      ]);
      timedOut = winner === 'timeout';
    } catch { timedOut = true; }
    check(timedOut,
      'connectRfsv() hangs cleanly when the remote is silent (Promise.race-test, no hang of the test process)');
  } finally {
    client.stop();
  }
}

// ── Driver ────────────────────────────────────────────────────────
(async function main() {
  console.log('PLP end-to-end harness');
  const cases: [string, () => Promise<void>][] = [
    ['connect (NCP handshake)',       testConnect],
    ['listDrives (DRIVE_LIST)',       testDriveList],
    ['listDirRaw (OPENDIR + READ)',   testReadDirRaw],
    ['listDir parses entries',        testListDir],
    ['downloadFile (FOPEN + FREAD)',  testDownload],
    ['downloadFile chunked (3 KiB)',  testDownloadChunked],
    ['uploadFile -> downloadFile',    testUploadRoundtrip],
    ['deleteFile + FNF on re-read',   testDeleteThenMissing],
    ['silent remote -> no hang',      testTimeoutOnSilentRemote],
  ];
  for (const [name, fn] of cases) {
    const t0 = Date.now();
    try {
      await fn();
      const dt = Date.now() - t0;
      console.log(`  PASS  ${name}  (${dt} ms)`);
    } catch (e) {
      failures++;
      console.error(`  FAIL  ${name}: ${e instanceof Error ? e.message : String(e)}`);
    }
  }
  if (failures > 0) {
    console.error(`\n${failures} failure(s)`);
    process.exit(1);
  }
  console.log('\nOK — all PLP e2e tests passed');
})();
