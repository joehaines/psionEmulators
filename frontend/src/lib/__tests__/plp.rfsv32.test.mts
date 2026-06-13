// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// RFSV-32 encoder/decoder + DRIVE_LIST round-trip. Run via:
//   node --experimental-strip-types \
//       frontend/src/lib/__tests__/plp.rfsv32.test.mts

import {
  encodeRfsvRequest,
  decodeRfsvReply,
  requestDriveList,
  parseDriveListReply,
  requestOpenDir,
  parseOpenDirReply,
  requestReadDirLfn,
  requestCloseDir,
  requestFopen,
  parseFopenReply,
  requestFread,
  requestFwrite,
  parseFwriteReply,
  requestFclose,
  requestDelete,
  parseDirBatch,
  EPOC_ATT_DIR,
  EPOC_ATT_ARCHIVE,
} from '../plp/rfsv32.ts';
import { Rfsv32Op, RFSV32_OMODE_OPEN_EXISTING, RFSV32_OMODE_SHARE_READ } from '../plp/types.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}
function eqArr<T>(a: T[], b: T[], msg: string) {
  if (a.length !== b.length || a.some((x, i) => x !== b[i])) {
    console.error(`FAIL: ${msg}: got [${a.join(',')}], want [${b.join(',')}]`);
    failures++;
  }
}

// 1. Request encoder lays out [opcode-lo opcode-hi len-lo len-hi args...] LE.
{
  const req = encodeRfsvRequest({
    op: Rfsv32Op.FOPEN,
    payload: new Uint8Array([0xAA, 0xBB, 0xCC]),
  });
  check(req.length === 7, `request length ${req.length}, want 7`);
  check(req[0] === 0x01 && req[1] === 0x00, 'opcode LE (FOPEN = 0x0001)');
  check(req[2] === 0x03 && req[3] === 0x00, 'arg-length LE');
  check(req[4] === 0xAA && req[5] === 0xBB && req[6] === 0xCC, 'args copied through');
}

// 2. DRIVE_LIST request: opcode 0x0D, zero-length args.
{
  const req = encodeRfsvRequest(requestDriveList());
  check(req.length === 4, 'DRIVE_LIST request is 4 bytes (op + zero len)');
  check(req[0] === 0x0D && req[1] === 0x00, 'opcode == DRIVE_LIST');
  check(req[2] === 0x00 && req[3] === 0x00, 'arg-length == 0');
}

// 3. Reply decoder: signed 16-bit status, then `data` is the remainder.
{
  // status = 0 (OK), then 4 data bytes
  const raw = new Uint8Array([0x00, 0x00, 0xDE, 0xAD, 0xBE, 0xEF]);
  const r = decodeRfsvReply(raw);
  check(r !== null, 'decoder produces a reply');
  if (r) {
    check(r.status === 0, `status ${r.status}, want 0`);
    check(r.data.length === 4, `data length ${r.data.length}, want 4`);
    check(r.data[0] === 0xDE && r.data[3] === 0xEF, 'data bytes preserved');
  }
}

// 4. Negative status (EPOC error). 0xFFFF = -1 = KErrNotFound.
{
  const raw = new Uint8Array([0xFF, 0xFF]);
  const r = decodeRfsvReply(raw);
  check(r !== null && r.status === -1, `negative status decode = ${r?.status}, want -1`);
}

// 5. Too-short reply -> null.
{
  check(decodeRfsvReply(new Uint8Array(0)) === null, 'empty reply -> null');
  check(decodeRfsvReply(new Uint8Array([0x00])) === null, '1-byte reply -> null');
}

// 6. DRIVE_LIST reply parser: bitmask -> drive letter list.
//    Bit 2 (C:) + bit 25 (Z:) = 0x02000004 LE.
{
  const reply = {
    status: 0,
    data: new Uint8Array([0x04, 0x00, 0x00, 0x02]),
  };
  const drives = parseDriveListReply(reply);
  eqArr(drives, ['C:', 'Z:'], 'DRIVE_LIST decode (bit 2 + bit 25 -> C: Z:)');
}

// 7. Error reply -> empty drive list (we don't surface a half-parsed status).
{
  const reply = { status: -21, data: new Uint8Array(4) };
  const drives = parseDriveListReply(reply);
  check(drives.length === 0, 'error reply -> empty drive list');
}

// 8. Drive bitmask covering A:..Z: round-trip.
{
  // All 26 letters set.
  const mask = 0x03FFFFFF;
  const data = new Uint8Array([
    mask & 0xFF, (mask >> 8) & 0xFF, (mask >> 16) & 0xFF, (mask >> 24) & 0xFF,
  ]);
  const drives = parseDriveListReply({ status: 0, data });
  check(drives.length === 26, `all-drives bitmask -> ${drives.length}, want 26`);
  check(drives[0] === 'A:' && drives[25] === 'Z:', 'first / last drive letters');
}

// 9. OPENDIR request: [attribMask:u32-LE][path-zstring].
{
  const path = 'C:\\Documents\\';
  const req = encodeRfsvRequest(requestOpenDir(path, 0));
  check(req[0] === 0x1B && req[1] === 0x00, 'opcode == OPENDIR');
  // Length of arg = 4 (mask) + path bytes + 1 (nul) = 4 + 13 + 1 = 18
  check(req[2] === 18 && req[3] === 0x00, `OPENDIR arg-length ${req[2]} (want 18)`);
  // First 4 args bytes are the attribute mask (0).
  check(req[4] === 0 && req[5] === 0 && req[6] === 0 && req[7] === 0, 'attribMask LE = 0');
  // Then the path string, nul-terminated.
  const pathStart = 8;
  const tail = req.subarray(pathStart, pathStart + 14);
  const decoded = new TextDecoder().decode(tail.subarray(0, 13));
  check(decoded === path, `OPENDIR path "${decoded}" (want "${path}")`);
  check(tail[13] === 0, 'OPENDIR path is nul-terminated');
}

// 10. OPENDIR reply: status + 4-byte handle.
{
  const r = parseOpenDirReply({ status: 0, data: new Uint8Array([0x78, 0x56, 0x34, 0x12]) });
  check(r === 0x12345678, `OPENDIR handle 0x${r?.toString(16)} (want 0x12345678)`);
}
{
  // Error reply -> null.
  const r = parseOpenDirReply({ status: -12, data: new Uint8Array(0) });
  check(r === null, 'error OPENDIR reply -> null');
}

// 11. READDIR_LFN request: [handle:u32-LE].
{
  const req = encodeRfsvRequest(requestReadDirLfn(0xDEADBEEF));
  check(req[0] === 0x1D && req[1] === 0x00, 'opcode == READDIR_LFN');
  check(req[2] === 4 && req[3] === 0, 'arg length 4');
  check(req[4] === 0xEF && req[5] === 0xBE && req[6] === 0xAD && req[7] === 0xDE,
        'handle is u32-LE');
}

// 12. CLOSEDIR request: same shape as READDIR_LFN, different opcode.
{
  const req = encodeRfsvRequest(requestCloseDir(0x42));
  check(req[0] === 0x1C && req[1] === 0x00, 'opcode == CLOSEDIR');
  check(req[2] === 4 && req[3] === 0, 'arg length 4');
  check(req[4] === 0x42, 'handle low byte == 0x42');
}

// 13. FOPEN request: [mode:u16-LE][path-zstring].
{
  const mode = RFSV32_OMODE_OPEN_EXISTING | RFSV32_OMODE_SHARE_READ;
  const path = 'C:\\hello.txt';
  const req = encodeRfsvRequest(requestFopen(path, mode));
  check(req[0] === 0x01 && req[1] === 0x00, 'opcode == FOPEN');
  // 2 (mode) + 12 (path) + 1 (nul) = 15
  check(req[2] === 15 && req[3] === 0x00, `FOPEN arg-length ${req[2]} (want 15)`);
  check(req[4] === (mode & 0xFF) && req[5] === ((mode >> 8) & 0xFF), 'mode LE');
  const pathSlice = new TextDecoder().decode(req.subarray(6, 6 + 12));
  check(pathSlice === path, `FOPEN path "${pathSlice}" (want "${path}")`);
  check(req[6 + 12] === 0, 'FOPEN path is nul-terminated');
}

// 14. FOPEN reply: status + handle.
{
  const r = parseFopenReply({ status: 0, data: new Uint8Array([0x01, 0x02, 0x03, 0x04]) });
  check(r === 0x04030201, `FOPEN handle 0x${r?.toString(16)} (want 0x04030201)`);
}
check(parseFopenReply({ status: -1, data: new Uint8Array(4) }) === null,
      'FOPEN with error -> null');

// 15. FREAD request: [handle:u32][maxBytes:u32].
{
  const req = encodeRfsvRequest(requestFread(0x11223344, 1024));
  check(req[0] === 0x03 && req[1] === 0x00, 'opcode == FREAD');
  check(req[2] === 8 && req[3] === 0, 'arg length 8');
  check(req[4] === 0x44 && req[5] === 0x33 && req[6] === 0x22 && req[7] === 0x11,
        'handle u32-LE');
  check(req[8] === 0x00 && req[9] === 0x04 && req[10] === 0 && req[11] === 0,
        'maxBytes u32-LE (1024 = 0x400)');
}

// 16. FWRITE request: [handle:u32][data...].
{
  const data = new Uint8Array([0xAA, 0xBB, 0xCC]);
  const req = encodeRfsvRequest(requestFwrite(0xDEADBEEF, data));
  check(req[0] === 0x04 && req[1] === 0x00, 'opcode == FWRITE');
  check(req[2] === 7 && req[3] === 0, 'arg length 7 (4 handle + 3 data)');
  check(req[4] === 0xEF && req[7] === 0xDE, 'handle u32-LE');
  check(req[8] === 0xAA && req[9] === 0xBB && req[10] === 0xCC, 'data tail');
}

// 17. FWRITE reply: byte count (when present) or null.
{
  const r = parseFwriteReply({ status: 0, data: new Uint8Array([0xFF, 0x00, 0x00, 0x00]) });
  check(r === 255, `FWRITE bytesWritten ${r} (want 255)`);
  check(parseFwriteReply({ status: 0, data: new Uint8Array(0) }) === null,
        'FWRITE reply with empty body -> null (treat as "whole write accepted")');
  check(parseFwriteReply({ status: -21, data: new Uint8Array(4) }) === null,
        'FWRITE error -> null');
}

// 18. FCLOSE request: handle only.
{
  const req = encodeRfsvRequest(requestFclose(0x99));
  check(req[0] === 0x02 && req[1] === 0x00, 'opcode == FCLOSE');
  check(req[4] === 0x99, 'handle low byte');
}

// 19. DELETE request: just a nul-terminated path.
{
  const req = encodeRfsvRequest(requestDelete('D:\\file'));
  check(req[0] === 0x09 && req[1] === 0x00, 'opcode == DELETE');
  // 7-byte path + 1 nul
  check(req[2] === 8 && req[3] === 0, `DELETE arg-length ${req[2]} (want 8)`);
  const path = new TextDecoder().decode(req.subarray(4, 11));
  check(path === 'D:\\file', `DELETE path "${path}"`);
  check(req[11] === 0, 'DELETE path nul-terminated');
}

// 20. parseDirBatch: two well-formed entries, one file + one dir.
//     30-byte header + name, repeated. (The same layout the e2e
//     mock emits, kept in sync between this unit test and the mock.)
function buildEntry(name: string, size: number, attrib: number): Uint8Array {
  const nameBytes = new TextEncoder().encode(name);
  const out = new Uint8Array(30 + nameBytes.length);
  const dv = new DataView(out.buffer);
  dv.setUint32(0, attrib, true);
  dv.setUint32(4, size, true);
  dv.setUint16(28, nameBytes.length, true);
  out.set(nameBytes, 30);
  return out;
}
{
  const fileEntry = buildEntry('readme.txt', 1234, EPOC_ATT_ARCHIVE);
  const dirEntry  = buildEntry('Documents',  0,    EPOC_ATT_DIR);
  const batch = new Uint8Array(fileEntry.length + dirEntry.length);
  batch.set(fileEntry, 0);
  batch.set(dirEntry, fileEntry.length);
  const parsed = parseDirBatch(batch);
  check(parsed.entries.length === 2, `parseDirBatch entry count ${parsed.entries.length}, want 2`);
  check(parsed.remainingBytes === 0, `parseDirBatch remainingBytes ${parsed.remainingBytes}, want 0`);
  if (parsed.entries.length === 2) {
    const [a, b] = parsed.entries;
    check(a.name === 'readme.txt' && a.size === 1234 && !a.isDirectory,
          `entry 0: ${a.name} size=${a.size} dir=${a.isDirectory}`);
    check(b.name === 'Documents' && b.isDirectory,
          `entry 1: ${b.name} dir=${b.isDirectory}`);
  }
}

// 21. parseDirBatch on truncated header: bail out with remainingBytes > 0
//     rather than emitting garbage.
{
  const trunc = new Uint8Array(20); // < 30 bytes
  const parsed = parseDirBatch(trunc);
  check(parsed.entries.length === 0, 'truncated header: no entries emitted');
  check(parsed.remainingBytes === 20, `remainingBytes ${parsed.remainingBytes}, want 20`);
}

// 22. parseDirBatch with an impossible nameLen: parseError surfaced.
{
  const bad = new Uint8Array(30);
  // attrib=0, size=0, … nameLen=999 at offset 28
  new DataView(bad.buffer).setUint16(28, 999, true);
  const parsed = parseDirBatch(bad);
  check(parsed.entries.length === 0, 'oversized nameLen: no entries emitted');
  check(parsed.parseError !== undefined, 'oversized nameLen: parseError set');
}

if (failures > 0) {
  console.error(`\n${failures} test failure(s)`);
  process.exit(1);
}
console.log('OK — all PLP RFSV-32 tests passed');
