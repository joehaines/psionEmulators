// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Refuse an ARM binary that a Psion Series 5 cannot execute.
//
// The Series 5 and the Geofox run an **ARM710a**, which is ARMv3. The
// instructions that arrived with ARMv4 and ARMv4T are not there:
//
//   * BX / BLX — the interworking branch. ARMv3 has no Thumb to
//     interwork with. The ROM's own import stubs load PC directly
//     ("ldr r12,[pc]; ldr pc,[r12]") for exactly this reason, and a
//     thunk that ends in BX takes an undefined-instruction exception
//     the first time an import is called.
//   * UMULL / SMULL / UMLAL / SMLAL — the 64-bit multiplies. A compiler
//     emits one of these for every division by a constant, so an
//     innocent `n / 10` in C is enough to produce a binary that cannot
//     run. (This is not theoretical: it is what made the first build of
//     tools/romdump-er1 panic with KERN-EXEC 3 on the machine.)
//   * LDRH / STRH / LDRSB / LDRSH — the halfword and signed loads.
//   * CLZ, and the ARMv5 and later additions.
//
// clang has no armv3 target, so the binary is checked rather than
// declared: every word of the code section is decoded and anything
// outside ARMv3 is named, with its offset, and the build stops.
//
// A code section holds constants as well as instructions, and a
// constant can happen to look like an instruction. The ones that do are
// almost always addresses — and an E32Image lists every address in it,
// because every one of them needs relocating. So with --e32 the
// relocation section is read first and those words are left out of the
// scan, which is exact rather than a heuristic.
//
// Run (node 22+):
//   node --experimental-strip-types tools/e32/armv3check.mts FILE.bin
//   node --experimental-strip-types tools/e32/armv3check.mts --e32 ROMDUMP.EXE

import * as fs from 'node:fs';

export interface Offence { offset: number; word: number; what: string; }

export function scanArmV3(code: Buffer, base = 0, skip = new Set<number>()): Offence[] {
  const out: Offence[] = [];
  for (let o = 0; o + 4 <= code.length; o += 4) {
    if (skip.has(o)) continue;              // a relocated word is an address, not an instruction
    const w = code.readUInt32LE(o);
    const say = (what: string): void => { out.push({ offset: base + o, word: w, what }); };

    // BX / BLX(register): cond 0001 0010 1111 1111 1111 0001 Rm (BX)
    //                     ...                          0011 Rm (BLX)
    if ((w & 0x0ffffff0) === 0x012fff10) { say('BX (ARMv4T)'); continue; }
    if ((w & 0x0ffffff0) === 0x012fff30) { say('BLX register (ARMv5)'); continue; }
    if ((w & 0xfe000000) === 0xfa000000) { say('BLX immediate (ARMv5)'); continue; }

    // Multiply long: cond 0000 1UAS RdHi RdLo Rs 1001 Rm
    if ((w & 0x0f8000f0) === 0x00800090) { say('UMULL/SMULL/UMLAL/SMLAL (ARMv3M/v4)'); continue; }

    // Halfword and signed transfers: cond 000P U0WL Rn Rd 0000 1SH1 Rm
    // and the immediate form with bit 22 set. Both have bit 7 and bit 4
    // set inside a data-processing-shaped opcode, with SH != 0.
    if ((w & 0x0e000090) === 0x00000090 && (w & 0x60) !== 0) {
      say('LDRH/STRH/LDRSB/LDRSH (ARMv4)'); continue;
    }

    // CLZ: cond 0001 0110 1111 Rd 1111 0001 Rm
    if ((w & 0x0fff0ff0) === 0x016f0f10) { say('CLZ (ARMv5)'); continue; }
  }
  return out;
}

function main(): void {
  const args = process.argv.slice(2);
  const isE32 = args[0] === '--e32';
  const path = isE32 ? args[1] : args[0];
  if (!path) {
    console.error('usage: armv3check.mts [--e32] FILE');
    process.exit(2);
  }
  const file = fs.readFileSync(path);
  // An E32Image's code section is iCodeSize bytes at iCodeOffset; a flat
  // binary is all code. The import address table at the end is data (it
  // holds ordinals), so it is left out of the scan.
  let code = file, base = 0, note = `${file.length} bytes`;
  const skip = new Set<number>();
  if (isE32) {
    const codeOffset = file.readUInt32LE(0x64);
    const textSize = file.readUInt32LE(0x60);
    code = file.subarray(codeOffset, codeOffset + textSize);
    base = 0;
    // The relocation section: 16-bit (type << 12) | offset-in-page
    // entries, in per-page blocks of { page, block size }.
    const rel = file.readUInt32LE(0x70);
    if (rel) {
      const size = file.readUInt32LE(rel);
      let q = rel + 8;
      while (q < rel + 8 + size) {
        const page = file.readUInt32LE(q), blk = file.readUInt32LE(q + 4);
        if (blk < 8) break;
        for (let k = 8; k < blk; k += 2) {
          const ent = file.readUInt16LE(q + k);
          if (ent) skip.add(page + (ent & 0xfff));
        }
        q += blk;
      }
    }
    note = `text 0x${textSize.toString(16)} at 0x${codeOffset.toString(16)}, ` +
           `${skip.size} relocated words skipped`;
  }
  const bad = scanArmV3(code, base, skip);
  for (const b of bad) {
    console.error(`${path}+0x${b.offset.toString(16)}: 0x${b.word.toString(16).padStart(8, '0')} — ${b.what}`);
  }
  if (bad.length) {
    console.error(`${path}: ${bad.length} instruction(s) an ARM710a cannot execute`);
    process.exit(1);
  }
  console.log(`${path}: ARMv3-clean (${note})`);
}

if (process.argv[1] && process.argv[1].endsWith('armv3check.mts')) main();
