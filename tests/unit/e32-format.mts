// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Checks tools/e32/e32link.mts against the format's own worked example.
//
// Z:\System\Samples\D_EXC.exe in the Conan ROM is a RAM-format E32Image
// stored as a plain file, written by Psion's own tools. Everything our
// packer computes for an image — the UID checksum, the code checksum, the
// import section, the relocation section — is recomputed here from what
// that binary declares and required to come out byte-identical to what it
// carries. If a field was misread, this fails rather than the machine.
//
// It then reads back the committed tools/romdump/ROMDUMP.EXE and requires
// it to be self-consistent in the same terms.
//
//   node --experimental-strip-types tests/unit/e32-format.mts

import * as fs from 'node:fs';
import * as path from 'node:path';
import { uidChecksum, wordSum, buildImportSection, buildRelocSection }
  from '../../tools/e32/e32link.mts';
import { readRom } from '../../tools/e32/romfs.mts';

const REPO = path.resolve(new URL('../..', import.meta.url).pathname);
const ROM = path.join(REPO, 'roms', 'conan_s2_2201.engbuild.IMG');
const OURS = path.join(REPO, 'tools', 'romdump', 'ROMDUMP.EXE');

let failures = 0;
const check = (name: string, ok: boolean, detail = ''): void => {
  console.log(`${ok ? 'ok  ' : 'FAIL'} ${name}${ok || !detail ? '' : ` — ${detail}`}`);
  if (!ok) failures++;
};

// ── Parsing, in the terms e32link writes ────────────────────────────

interface Parsed {
  uid1: number; uid2: number; uid3: number; uidcrc: number;
  crcCode: number; codeSize: number; codeOffset: number; textSize: number;
  dllRefCount: number; importOffset: number; relocOffset: number;
  code: Buffer;
  imports: { dll: string; ordinals: number[] }[];
  importSection: Buffer;
  relocSection: Buffer;
  relocOffsets: number[];
}

function parse(f: Buffer): Parsed {
  const u = (o: number): number => f.readUInt32LE(o);
  const p: Parsed = {
    uid1: u(0x00), uid2: u(0x04), uid3: u(0x08), uidcrc: u(0x0c),
    crcCode: u(0x18), codeSize: u(0x30), codeOffset: u(0x64), textSize: u(0x60),
    dllRefCount: u(0x54), importOffset: u(0x6c), relocOffset: u(0x70),
    code: Buffer.alloc(0), imports: [], importSection: Buffer.alloc(0),
    relocSection: Buffer.alloc(0), relocOffsets: [],
  };
  p.code = f.subarray(p.codeOffset, p.codeOffset + p.codeSize);
  p.importSection = f.subarray(p.importOffset, p.importOffset + u(p.importOffset));
  let q = p.importOffset + 4;
  for (let i = 0; i < p.dllRefCount; i++) {
    const nameOff = u(q), count = u(q + 4);
    const s = p.importOffset + nameOff;
    p.imports.push({
      dll: f.subarray(s, f.indexOf(0, s)).toString('latin1'),
      ordinals: Array.from({ length: count }, (_, j) => u(q + 8 + j * 4)),
    });
    q += 8 + count * 4;
  }
  p.relocSection = f.subarray(p.relocOffset, p.relocOffset + 8 + u(p.relocOffset));
  const nRelocs = u(p.relocOffset + 4);
  let r = p.relocOffset + 8, seen = 0;
  while (seen < nRelocs) {
    const page = u(r), blockSize = u(r + 4);
    for (let e = 8; e < blockSize && seen < nRelocs; e += 2, seen++) {
      p.relocOffsets.push(page + (f.readUInt16LE(r + e) & 0xfff));
    }
    r += blockSize;
  }
  return p;
}

function checkAgainst(label: string, f: Buffer): Parsed {
  const p = parse(f);
  check(`${label}: UID checksum`,
        uidChecksum(p.uid1, p.uid2, p.uid3) === p.uidcrc,
        `computed ${uidChecksum(p.uid1, p.uid2, p.uid3).toString(16)}, ` +
        `file says ${p.uidcrc.toString(16)}`);
  check(`${label}: iCheckSumCode is the code section's word sum`,
        wordSum(p.code) === p.crcCode,
        `computed ${wordSum(p.code).toString(16)}, file says ${p.crcCode.toString(16)}`);
  check(`${label}: import section round-trips`,
        buildImportSection(p.imports).equals(p.importSection));
  check(`${label}: relocation section round-trips`,
        buildRelocSection(p.relocOffsets).equals(p.relocSection),
        `${p.relocOffsets.length} relocations`);
  // The IAT sits at iTextSize and holds one ordinal per import, in
  // import-table order, then a terminating zero.
  const iat: number[] = [];
  for (let o = p.textSize; o + 4 <= p.codeSize; o += 4) iat.push(p.code.readUInt32LE(o));
  const wanted = p.imports.flatMap(b => b.ordinals);
  check(`${label}: IAT holds the import ordinals in order`,
        wanted.every((v, i) => iat[i] === v) && iat[wanted.length] === 0,
        `iat ${iat.slice(0, wanted.length + 1)} vs imports ${wanted}`);
  return p;
}

// ── Psion's binary ──────────────────────────────────────────────────

if (!fs.existsSync(ROM)) {
  console.log(`SKIP: ${ROM} not present`);
  process.exit(0);
}
const rom = readRom(ROM);
const dexc = rom.files.find(x => x.path.toLowerCase().endsWith('\\d_exc.exe'));
if (!dexc) {
  console.error('FAIL: no D_EXC.exe in the ROM');
  process.exit(1);
}
const at = dexc.addr - rom.base;
const theirs = checkAgainst('D_EXC.exe', rom.data.subarray(at, at + dexc.size));
check('D_EXC.exe: is a RAM-format E32Image', theirs.uid1 === 0x1000007a);

// ── Ours ────────────────────────────────────────────────────────────

if (fs.existsSync(OURS)) {
  const ours = checkAgainst('ROMDUMP.EXE', fs.readFileSync(OURS));
  const dlls = [...new Set(ours.imports.map(i => i.dll))];
  check('ROMDUMP.EXE: imports the file-server client and EUser, nothing else',
        dlls.length === 2 && dlls.some(d => d.startsWith('EFSRV[')) &&
        dlls.some(d => d.startsWith('EUSER[')),
        dlls.join(', '));
  check('ROMDUMP.EXE: UID3 is 0, so the System screen runs it',
        ours.uid3 === 0, `uid3 ${ours.uid3.toString(16)}`);
} else {
  console.log(`SKIP: ${OURS} not built`);
}

process.exit(failures ? 1 : 0);
