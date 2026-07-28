// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// FEFS24 round-trip test. Run via:
//   node --experimental-strip-types frontend/src/lib/__tests__/fefs.test.mts
// or `bash scripts/test-fefs.sh` which forwards the right flags.
//
// The layout under test is the real Psion FEFS24 format (validated
// against factory Flash SSD dumps + thelastpsion/fefstool). If a MAME
// softlist dump has been extracted to /tmp, the reader is additionally
// validated against it.

import {
  createFlashPack,
  addFileToPack,
  removeFileFromPack,
  readFileFromPack,
  listFiles,
  classifyPack,
  readVolumeName,
  FLASH_PACK_SIZES,
} from '../fefs.ts';
import { existsSync, readFileSync } from 'node:fs';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}
function eq<T>(actual: T, expected: T, msg: string) {
  if (actual !== expected) {
    console.error(`FAIL: ${msg}: got ${String(actual)}, want ${String(expected)}`);
    failures++;
  }
}

// 1. Size presets.
check(FLASH_PACK_SIZES.length === 7, '7 Flash presets');
check(FLASH_PACK_SIZES.every(s => (s.bytes & (s.bytes - 1)) === 0), 'sizes are powers of two');

// 2. Blank pack: factory-compatible header.
const pack0 = createFlashPack(0x20000, 'TESTPACK');
eq(pack0.length, 0x20000, 'pack size');
eq(pack0[0], 0xA5, 'magic LO');
eq(pack0[1], 0xF1, 'magic HI');
eq(pack0[0x0A], 0x00, 'FEFS24 pointer-size byte');
eq(pack0[0x0B], 0x45, 'root pointer LSB');
// Flash count left erased = factory "ROM image" style; EPOC16 mounts
// it either way, and as a Flash pack the drive is read/write.
check([0x19, 0x1A, 0x1B, 0x1C].every(o => pack0[o] === 0xFF), 'flash count erased');
eq(readVolumeName(pack0), 'TESTPACK', 'volume name round-trip');
eq(classifyPack(pack0), 'flash', 'classified as flash');
eq(listFiles(pack0).length, 0, 'blank pack lists no files');
// Root entry shape: "ROOT" name at 0x48, valid-directory flags.
eq(String.fromCharCode(pack0[0x48], pack0[0x49], pack0[0x4A], pack0[0x4B]), 'ROOT', 'root entry name');
eq(pack0[0x45 + 14], 0xFB, 'root flags: valid dir, no children, last');

// 3. Add files: root level + auto subdirectory.
const enc = new TextEncoder();
const helloBytes = enc.encode('Hello, Psion!');
const wrdBytes   = new Uint8Array(3000).fill(0x41);
let pack = addFileToPack(pack0, 'HELLO.TXT', helloBytes);
pack = addFileToPack(pack, 'REPORT.WRD', wrdBytes, 'WRD');

let files = listFiles(pack);
eq(files.length, 2, '2 files listed');
eq(files[0].name, 'HELLO.TXT', 'root file name');
eq(files[0].size, helloBytes.length, 'root file size');
eq(files[1].name, 'WRD\\REPORT.WRD', 'subdir file path');
eq(files[1].size, wrdBytes.length, 'subdir file size');

// 4. Second file in the same subdirectory reuses the directory entry.
pack = addFileToPack(pack, 'NOTES.WRD', enc.encode('n'), 'WRD');
files = listFiles(pack);
eq(files.length, 3, '3 files listed');
check(files.some(f => f.name === 'WRD\\NOTES.WRD'), 'second subdir file listed');

// 5. Replace-on-duplicate: re-adding a name supersedes the old copy.
pack = addFileToPack(pack, 'HELLO.TXT', enc.encode('replaced'));
files = listFiles(pack);
eq(files.length, 3, 'still 3 files after replace');
eq(files.find(f => f.name === 'HELLO.TXT')?.size, 8, 'replacement size wins');

// 6. Remove (logical delete, including subdir paths).
pack = removeFileFromPack(pack, 'WRD\\NOTES.WRD');
files = listFiles(pack);
eq(files.length, 2, '2 files after delete');
check(!files.some(f => f.name === 'WRD\\NOTES.WRD'), 'deleted file gone');

// 7. Large file chunks across continuation records (data length field
//    is 16-bit; the writer uses 32K data records).
const big = new Uint8Array(200000);
for (let i = 0; i < big.length; i++) big[i] = i & 0xFF;
let bigPack = createFlashPack(0x80000, 'BIG');
bigPack = addFileToPack(bigPack, 'BIG.BIN', big);
eq(listFiles(bigPack)[0]?.size, big.length, 'chunked file size survives');

// 8. Bad inputs throw.
let threw = false;
try { addFileToPack(pack, 'TOOOOLONG.TXT', new Uint8Array(0)); }
catch { threw = true; }
check(threw, 'reject 9-char stem');

threw = false;
try { addFileToPack(pack, 'A.TOOOLONG', new Uint8Array(0)); }
catch { threw = true; }
check(threw, 'reject 7-char extension');

threw = false;
try { removeFileFromPack(pack, 'NOSUCH.TXT'); }
catch { threw = true; }
check(threw, 'remove unknown file throws');

// 9. Pack-full detection.
threw = false;
try {
  let tiny = createFlashPack(0x20000, 'TINY');
  tiny = addFileToPack(tiny, 'A.BIN', new Uint8Array(0x16000));
  tiny = addFileToPack(tiny, 'B.BIN', new Uint8Array(0x16000));
} catch { threw = true; }
check(threw, 'pack full detection');

// 10. Nested directories (the app-library packs mirror each app's
//     folder tree, e.g. BLADES\DATA\LEVEL1.DAT).
{
  let nested = createFlashPack(0x20000, 'NEST');
  nested = addFileToPack(nested, 'BLADES.OPA', new Uint8Array([1, 2, 3]), 'BLADES');
  nested = addFileToPack(nested, 'LEVEL1.DAT', new Uint8Array([4, 5]), 'BLADES\\DATA');
  nested = addFileToPack(nested, 'LEVEL2.DAT', new Uint8Array([6]), 'BLADES\\DATA');
  const names = listFiles(nested).map(f => f.name);
  check(names.includes('BLADES\\BLADES.OPA'), `nested: program at one level (${names.join(', ')})`);
  check(names.includes('BLADES\\DATA\\LEVEL1.DAT') && names.includes('BLADES\\DATA\\LEVEL2.DAT'),
        'nested: two-level files listed with full paths');
  // Replace + remove work through nested paths too.
  nested = addFileToPack(nested, 'LEVEL1.DAT', new Uint8Array([9, 9, 9]), 'BLADES\\DATA');
  eq(listFiles(nested).filter(f => f.name === 'BLADES\\DATA\\LEVEL1.DAT').length, 1,
     'nested: replace-on-duplicate keeps one live copy');
  eq(listFiles(nested).find(f => f.name === 'BLADES\\DATA\\LEVEL1.DAT')?.size, 3,
     'nested: replacement carries the new size');
  nested = removeFileFromPack(nested, 'BLADES\\DATA\\LEVEL2.DAT');
  check(!listFiles(nested).some(f => f.name === 'BLADES\\DATA\\LEVEL2.DAT'),
        'nested: remove by full path');
}

// 11. readFileFromPack: getting files back OFF a pack — how a document
//     the Psion saved to an SSD reaches the host.
{
  const bytesEq = (a: Uint8Array, b: Uint8Array) =>
    a.length === b.length && a.every((v, i) => v === b[i]);

  // Root file, after the step-5 replacement.
  eq(new TextDecoder().decode(readFileFromPack(pack, 'HELLO.TXT')), 'replaced',
     'read back a root file');
  // Subdirectory file, by the same path listFiles reports.
  check(bytesEq(readFileFromPack(pack, 'WRD\\REPORT.WRD'), wrdBytes),
        'read back a subdirectory file byte-exact');
  // Forward slashes and lower case work too — the paths users paste.
  check(bytesEq(readFileFromPack(pack, 'wrd/report.wrd'), wrdBytes),
        'path matching is case- and separator-insensitive');
  // Chunked file: the data-record chain has to be followed and joined
  // in order.
  check(bytesEq(readFileFromPack(bigPack, 'BIG.BIN'), big),
        'read back a chunked file across continuation records');
  // Nested two-level path.
  {
    let nested = createFlashPack(0x20000, 'NEST');
    nested = addFileToPack(nested, 'LEVEL1.DAT', new Uint8Array([4, 5]), 'BLADES\\DATA');
    check(bytesEq(readFileFromPack(nested, 'BLADES\\DATA\\LEVEL1.DAT'), new Uint8Array([4, 5])),
          'read back a nested file');
  }
  // Empty file: a valid entry with a zero-length data record.
  {
    let empty = createFlashPack(0x20000, 'EMPTY');
    empty = addFileToPack(empty, 'EMPTY.BIN', new Uint8Array(0));
    eq(readFileFromPack(empty, 'EMPTY.BIN').length, 0, 'read back an empty file');
  }

  // Deleted, unknown and non-FEFS all throw rather than return junk.
  let threwRead = false;
  try { readFileFromPack(pack, 'WRD\\NOTES.WRD'); } catch { threwRead = true; }
  check(threwRead, 'reading a deleted file throws');

  threwRead = false;
  try { readFileFromPack(pack, 'NOSUCH.TXT'); } catch { threwRead = true; }
  check(threwRead, 'reading an unknown file throws');

  threwRead = false;
  try { readFileFromPack(new Uint8Array(0x20000).fill(0xFF), 'HELLO.TXT'); }
  catch { threwRead = true; }
  check(threwRead, 'reading from a non-FEFS (RAM) image throws');

  // A directory is not a file.
  threwRead = false;
  try { readFileFromPack(pack, 'WRD'); } catch { threwRead = true; }
  check(threwRead, 'reading a directory throws');

  // A truncated image must not throw or over-read: the reader clamps to
  // whatever data survives (salvage beats refusing the download). The
  // entry survives the cut here, its data record doesn't.
  {
    const whole = addFileToPack(createFlashPack(0x20000, 'TRUNC'),
                                'A.BIN', new Uint8Array(64).fill(7));
    const cut = 0xA0;
    const salvaged = readFileFromPack(whole.slice(0, cut), 'A.BIN');
    check(salvaged.length > 0 && salvaged.length < 64,
          `truncated image salvages a partial file (got ${salvaged.length} of 64)`);
    check(salvaged.every(v => v === 7), 'salvaged bytes are the file\'s own');
  }
}

// 12. Reader vs a real factory dump, when available (validates our
//     entry-walk against bytes EPOC16 itself ships). Extract
//     tests/fixtures/games3a.zip to /tmp to enable.
const realDump = '/tmp/Games_3a.rom';
if (existsSync(realDump)) {
  const real = new Uint8Array(readFileSync(realDump));
  eq(readVolumeName(real), 'GAMES3A', 'factory dump volume name');
  const rf = listFiles(real);
  check(rf.some(f => f.name === 'APP\\BOMZ3A.OPA' && f.size === 40776),
        'factory dump file walk (APP\\BOMZ3A.OPA, 40776 bytes)');
  // Extraction off a pack we didn't write: the file must come out at
  // its full length, carrying the signature EPOC16 stamps on a compiled
  // OPL application.
  const opa = readFileFromPack(real, 'APP\\BOMZ3A.OPA');
  eq(opa.length, 40776, 'factory dump file extracted at full length');
  eq(new TextDecoder('latin1').decode(opa.subarray(0, 13)), 'OPLObjectFile',
     'extracted OPA carries its signature');
}

if (failures === 0) {
  console.log('PASS fefs round-trip (FEFS24 create/add/remove/replace, subdirs, chunking)');
  process.exit(0);
} else {
  console.error(`FAIL: ${failures} assertion(s)`);
  process.exit(1);
}
