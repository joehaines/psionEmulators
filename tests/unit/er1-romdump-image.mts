// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The part of tools/romdump-er1/ROMDUMP.EXE that only a machine's own
// ROM can settle: that it names a DLL the machine has, by the UID the
// machine has it under, and imports the ordinals that are the calls it
// means to make — and that its container passes the checks the R1
// loader itself applies.
//
// Every EPOC Release 1 ROM in the repository is asked the same
// questions independently, and has to give the same answers. That is
// the evidence for one binary covering the Series 5 prototype, the
// shipping Series 5 and the Geofox: their EFSrv.dll is the same build,
// and nothing here takes that on trust.
//
//   node --experimental-strip-types tests/unit/er1-romdump-image.mts

import * as fs from 'node:fs';
import * as path from 'node:path';
import { readRom1 } from '../../tools/e32/romfs1.mts';
import { fileServerOrdinals, checkImage, findDll } from '../../tools/e32/er1check.mts';
import { scanArmV3 } from '../../tools/e32/armv3check.mts';

const REPO = path.resolve(new URL('../..', import.meta.url).pathname);
const EXE = path.join(REPO, 'tools', 'romdump-er1', 'ROMDUMP.EXE');
const EXE0 = path.join(REPO, 'tools', 'romdump-er1', 'ROMDUMP0.EXE');
const APP = path.join(REPO, 'tools', 'romdump-er1', 'ROMDUMP.APP');

// Every R1 machine this repository has a ROM for.
const ROMS = [
  'Series5/S5_v1.00(113)_eng/S5_v1.00(113)_eng.bin',      // the Series 5 prototype — what the tool is for
  'Series5/S5_v1.01(145)_eng/S5_v1.01(145)_eng.bin', // the shipping Series 5
  'Geofox/Geofox_v1.01(146)_eng/Geofox_v1.01(146)_eng.bin',  // the Geofox One, same EPOC generation
];

// What tools/romdump-er1 is built against.
const EXPECTED = {
  close: 14, connect: 17, open: 105, create: 23,
  replace: 133, temp: 165, read: 119, write: 170,
};
const KEfsrvUid3 = 0x100000bd;

let failures = 0;
const check = (name: string, ok: boolean, detail = ''): void => {
  console.log(`${ok ? 'ok  ' : 'FAIL'} ${name}${ok || !detail ? '' : ` — ${detail}`}`);
  if (!ok) failures++;
};

if (!fs.existsSync(EXE)) {
  console.error(`${EXE} is missing — run bash tools/romdump-er1/build.sh`);
  process.exit(2);
}

// The CPU, before any of the ROMs: a Series 5's ARM710a is ARMv3, and
// the instructions ARMv4 added are not there — nor does the machine
// refuse them, it executes something else instead (see
// docs/series5-prototype-rom-dumping.md). The image's own relocation
// table says which words in the code section are addresses rather than
// instructions, so they are left out of the scan.
{
  const f = fs.readFileSync(EXE);
  const codeOffset = f.readUInt32LE(0x64), textSize = f.readUInt32LE(0x60);
  const skip = new Set<number>();
  const rel = f.readUInt32LE(0x70);
  if (rel) {
    const size = f.readUInt32LE(rel);
    let q = rel + 8;
    while (q < rel + 8 + size) {
      const page = f.readUInt32LE(q), blk = f.readUInt32LE(q + 4);
      if (blk < 8) break;
      for (let k = 8; k < blk; k += 2) {
        const ent = f.readUInt16LE(q + k);
        if (ent) skip.add(page + (ent & 0xfff));
      }
      q += blk;
    }
  }
  const bad = scanArmV3(f.subarray(codeOffset, codeOffset + textSize), 0, skip);
  check('ROMDUMP.EXE holds no instruction an ARM710a cannot execute',
        bad.length === 0,
        bad.slice(0, 4).map(b => `+0x${b.offset.toString(16)} ${b.what}`).join(', '));
}

// The second build is the one for a machine that will not bind the
// first, so the thing to check about it is that there is nothing to
// bind — and that it is ARMv3-clean too.
{
  const f = fs.readFileSync(EXE0);
  check('ROMDUMP0.EXE imports nothing at all',
        f.readUInt32LE(0x54) === 0, `iDllRefTableCount=${f.readUInt32LE(0x54)}`);
  const codeOffset = f.readUInt32LE(0x64), textSize = f.readUInt32LE(0x60);
  const skip = new Set<number>();
  const rel = f.readUInt32LE(0x70);
  if (rel) {
    const size = f.readUInt32LE(rel);
    let q = rel + 8;
    while (q < rel + 8 + size) {
      const page = f.readUInt32LE(q), blk = f.readUInt32LE(q + 4);
      if (blk < 8) break;
      for (let k = 8; k < blk; k += 2) {
        const ent = f.readUInt16LE(q + k);
        if (ent) skip.add(page + (ent & 0xfff));
      }
      q += blk;
    }
  }
  const bad = scanArmV3(f.subarray(codeOffset, codeOffset + textSize), 0, skip);
  check('ROMDUMP0.EXE holds no instruction an ARM710a cannot execute', bad.length === 0);
}

// The third is ROMDUMP0.EXE's code as an application DLL, for a machine
// whose System screen will not open an EXE. What makes it one is its
// header, laid out as Psion's own R1 applications are (applib's
// BOXES.APP and eptable.app): UID1 KDynamicLibraryUid, UID2 KUidApp, no
// data and no bss — the R1 loader gives a RAM-loaded DLL no static data
// — and an export directory of one word at the end of the code section,
// holding NewApplication's offset from the start of the code.
{
  const f = fs.readFileSync(APP);
  const u = (o: number): number => f.readUInt32LE(o);
  check('ROMDUMP.APP is a DLL (UID1 0x10000079, iFlags 3)', u(0) === 0x10000079 && u(0x2c) === 3);
  check('ROMDUMP.APP is an application (UID2 KUidApp 0x1000006c)', u(4) === 0x1000006c);
  check('ROMDUMP.APP has a UID3 from the development range',
        u(8) >= 0x01000000 && u(8) <= 0x0fffffff, `0x${u(8).toString(16)}`);
  check('ROMDUMP.APP has no static data (iDataSize and iBssSize 0)', u(0x34) === 0 && u(0x44) === 0);
  check('ROMDUMP.APP imports nothing at all', u(0x54) === 0, `iDllRefTableCount=${u(0x54)}`);
  const codeOffset = u(0x64), codeSize = u(0x30), textSize = u(0x60);
  const expOff = u(0x58), expCount = u(0x5c);
  check('ROMDUMP.APP exports one function, from the last word of its code section',
        expCount === 1 && expOff === codeOffset + codeSize - 4);
  const newApp = u(expOff);
  check('and that function (NewApplication) is in its text', newApp > 0 && newApp < textSize,
        `0x${newApp.toString(16)}`);
  const skip = new Set<number>();
  const rel = u(0x70);
  if (rel) {
    const size = u(rel);
    let q = rel + 8;
    while (q < rel + 8 + size) {
      const page = u(q), blk = u(q + 4);
      if (blk < 8) break;
      for (let k = 8; k < blk; k += 2) {
        const ent = f.readUInt16LE(q + k);
        if (ent) skip.add(page + (ent & 0xfff));
      }
      q += blk;
    }
  }
  const bad = scanArmV3(f.subarray(codeOffset, codeOffset + textSize), 0, skip);
  check('ROMDUMP.APP holds no instruction an ARM710a cannot execute', bad.length === 0);
}

for (const name of ROMS) {
  const romPath = path.join(REPO, 'roms', name);
  if (!fs.existsSync(romPath)) {
    console.log(`skip ${name} — not present`);
    continue;
  }
  const rom = readRom1(romPath);
  console.log(`# ${name}`);

  const dll = findDll(rom, 'efsrv.dll');
  check(`${name}: EFSrv.dll has UID3 0x${KEfsrvUid3.toString(16)}`,
        dll.image.uid3 === KEfsrvUid3, `0x${dll.image.uid3.toString(16)}`);

  const ordinals = fileServerOrdinals(rom);
  for (const [what, want] of Object.entries(EXPECTED)) {
    const got = (ordinals as unknown as Record<string, number>)[what];
    check(`${name}: ${what} is ordinal ${want}`, got === want, `found ${got}`);
  }

  for (const r of checkImage(rom, EXE)) check(`${name}: ${r.text}`, r.ok);
  // The loader's own checks apply to the application too; the ones that
  // describe ROMDUMP.EXE (its UID1, the zero UID3 the Shell runs an
  // executable by, and its six imports — the application has none) are
  // the ones checked differently above.
  for (const r of checkImage(rom, APP)) {
    if (/^iUid1 is |^iUid3 is 0|^the image imports every call/.test(r.text)) continue;
    check(`${name}: ROMDUMP.APP ${r.text}`, r.ok);
  }
}

console.log(failures ? `${failures} FAILURES` : 'all checks pass');
process.exit(failures ? 1 : 0);
