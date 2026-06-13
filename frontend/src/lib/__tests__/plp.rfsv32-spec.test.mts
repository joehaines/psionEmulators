// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Tests for the wire-correct RFSV-32 layer
// (frontend/src/lib/plp/rfsv32-spec.ts).
//
// Run with:
//   node --experimental-strip-types frontend/src/lib/__tests__/plp.rfsv32-spec.test.mts

import {
  buildGetDriveList,
  buildOpenDir,
  buildReadDir,
  buildCloseHandle,
  buildOpenFile,
  buildReadFile,
  buildDelete,
  decodeReply,
  parseDriveList,
  parseDirEntries,
} from '../plp/rfsv32-spec.ts';
import { RFSV32, RFSV32_REPLY_MARKER, ATTR } from '../plp/types.ts';

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

// 1. Request encoder shape — reason code + opId in u16-LE.
{
  const r = buildGetDriveList(0x1234);
  eqBytes(r, new Uint8Array([RFSV32.GET_DRIVE_LIST & 0xFF, (RFSV32.GET_DRIVE_LIST >> 8) & 0xFF, 0x34, 0x12]),
          'GET_DRIVE_LIST request');
}

// 2. OPEN_DIR has a 4-byte attribute mask + length-prefixed path
//    (no NUL terminator).
{
  const r = buildOpenDir(0x0042, 0x0010, 'C:\\*');
  // reason(0x10 0x00) opId(0x42 0x00) attrs(0x10 0x00 0x00 0x00) len(0x04 0x00) "C:\*"
  eqBytes(r, new Uint8Array([
    0x10, 0x00,
    0x42, 0x00,
    0x10, 0x00, 0x00, 0x00,
    0x04, 0x00,
    0x43, 0x3A, 0x5C, 0x2A,
  ]), 'OPEN_DIR request bytes');
}

// 3. READ_DIR / CLOSE_HANDLE / OPEN_FILE / READ_FILE / DELETE shape.
{
  const r = buildReadDir(0x0001, 0x12345678);
  // Handle is u32-LE: 0x78 0x56 0x34 0x12
  eqBytes(r.subarray(0, 4), new Uint8Array([RFSV32.READ_DIR & 0xFF, 0x00, 0x01, 0x00]), 'READ_DIR header');
  eqBytes(r.subarray(4), new Uint8Array([0x78, 0x56, 0x34, 0x12]), 'READ_DIR handle');
}
{
  const r = buildCloseHandle(0x00FF, 0x401);
  eqBytes(r, new Uint8Array([RFSV32.CLOSE_HANDLE & 0xFF, 0x00, 0xFF, 0x00, 0x01, 0x04, 0x00, 0x00]),
          'CLOSE_HANDLE request');
}
{
  const r = buildOpenFile(0x0007, 0x0001 /* share-read */, 'hello.txt');
  // reason(0x16 0x00) opId(0x07 0x00) mode(0x01 0x00 0x00 0x00) len(0x09 0x00) "hello.txt"
  eqBytes(r.subarray(0, 10), new Uint8Array([
    0x16, 0x00, 0x07, 0x00,
    0x01, 0x00, 0x00, 0x00,
    0x09, 0x00,
  ]), 'OPEN_FILE header');
}
{
  const r = buildReadFile(0x0010, 0x401, 0x800);
  eqBytes(r, new Uint8Array([
    0x18, 0x00, 0x10, 0x00,
    0x01, 0x04, 0x00, 0x00,
    0x00, 0x08, 0x00, 0x00,
  ]), 'READ_FILE request');
}
{
  const r = buildDelete(0x0020, 'C:\\junk.bin');
  // reason(0x1B 0x00) opId(0x20 0x00) len(0x0B 0x00) "C:\junk.bin"
  eqBytes(r.subarray(0, 6), new Uint8Array([0x1B, 0x00, 0x20, 0x00, 0x0B, 0x00]),
          'DELETE header');
  eqBytes(r.subarray(6), new TextEncoder().encode('C:\\junk.bin'), 'DELETE path');
}

// 4. Reply decoder rejects non-0x11 markers; accepts valid frames.
{
  const bad = new Uint8Array([0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00]);
  check(decodeReply(bad) === null, 'reply with bad marker rejected');

  const ok = new Uint8Array([
    0x11, 0x00,             // reply marker
    0x42, 0x00,             // opId = 0x0042
    0x00, 0x00, 0x00, 0x00, // status = 0
    0xAB, 0xCD,             // reply data
  ]);
  const r = decodeReply(ok);
  check(r !== null && r.opId === 0x42 && r.status === 0 && r.data.length === 2,
        'reply with correct marker decoded');
  if (r) eqBytes(r.data, new Uint8Array([0xAB, 0xCD]), 'reply data bytes');
}

// 5. Negative status (e.g. KErrEof = -25) preserved.
{
  const buf = new Uint8Array([0x11, 0x00, 0x01, 0x00,
                              0xE7, 0xFF, 0xFF, 0xFF /* -25 in two's complement */]);
  const r = decodeReply(buf);
  check(r !== null && r.status === -25, `signed status decode: got ${r?.status}, want -25`);
}

// 6. Drive list parser: 26-byte bitmap, non-zero entries mark
//    present drives. Captured from real Revo had C: at byte 2 (0x11)
//    and Z: at byte 25 (0x12).
{
  const data = new Uint8Array(26);
  data[2] = 0x11;
  data[25] = 0x12;
  const r = parseDriveList({ opId: 1, status: 0, data });
  check(r.length === 2 && r[0] === 'C:' && r[1] === 'Z:',
        `drive list = [${r.join(',')}], want [C:, Z:]`);
}

// 7. Dir entries parser against the actual captured bytes from
//    harness/run --serial-tx-framed READ_DIR on C:\.
//    Real response after the 8-byte reply header:
{
  const entryBytes = new Uint8Array([
    // Entry 1: "System" (directory, no short name)
    0x00, 0x00, 0x00, 0x00,                         // short_name_length = 0
    0x10, 0x00, 0x00, 0x00,                         // attributes = DIR
    0x00, 0x00, 0x00, 0x00,                         // size = 0
    0x80, 0xbf, 0x2b, 0x5f,                         // modified low
    0xa9, 0x2f, 0xe3, 0x00,                         // modified high
    0x00, 0x00, 0x00, 0x00,                         // uid1
    0x00, 0x00, 0x00, 0x00,                         // uid2
    0x00, 0x00, 0x00, 0x00,                         // uid3
    0x06, 0x00, 0x00, 0x00,                         // long_name_length = 6
    0x53, 0x79, 0x73, 0x74, 0x65, 0x6d,             // "System"
    0x00, 0x30,                                      // pad to 4-byte boundary

    // Entry 2: "Documents" with short name "DOCUMENT"
    0x08, 0x00, 0x00, 0x00,                         // short_name_length = 8
    0x10, 0x00, 0x00, 0x00,                         // attributes = DIR
    0x00, 0x00, 0x00, 0x00,                         // size = 0
    0x80, 0xbf, 0x2b, 0x5f,
    0xa9, 0x2f, 0xe3, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x00, 0x00,                         // long_name_length = 9
    0x44, 0x6f, 0x63, 0x75, 0x6d, 0x65, 0x6e, 0x74, 0x73, // "Documents"
    0xe3, 0x05, 0x50,                                // pad bytes (random)
    0x44, 0x4f, 0x43, 0x55, 0x4d, 0x45, 0x4e, 0x54, // "DOCUMENT"
  ]);
  const { entries, truncated } = parseDirEntries({ opId: 1, status: 0, data: entryBytes });
  check(!truncated, 'no truncation on real-capture bytes');
  check(entries.length === 2, `entry count = ${entries.length}, want 2`);
  if (entries.length === 2) {
    check(entries[0].longName === 'System' && entries[0].isDirectory && entries[0].shortName === '',
          `entry 0: name=${entries[0].longName} dir=${entries[0].isDirectory} short=${entries[0].shortName}`);
    check(entries[1].longName === 'Documents' && entries[1].isDirectory
          && entries[1].shortName === 'DOCUMENT'
          && (entries[1].attributes & ATTR.DIRECTORY) !== 0,
          `entry 1: name=${entries[1].longName} dir=${entries[1].isDirectory} short=${entries[1].shortName}`);
  }
}

if (failures > 0) {
  console.error(`\n${failures} test failure(s)`);
  process.exit(1);
}
console.log('OK — all RFSV-32-spec tests passed');
