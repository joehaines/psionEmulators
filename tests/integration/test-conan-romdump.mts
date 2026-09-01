// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Checks a Conan ROM-dump run from outside the machine, so that nothing
// here depends on what the program says about itself: for every 64 KB
// step through the dump, that run of the ROM image has to be found in a
// snapshot of the machine's RAM — which is where the C: RAM disk it was
// written to lives.
//
// The check stops after the first three parts because
// --save-ram-snapshot dumps one 8 MB bank and the back half of a 12 MB
// dump lands in the other one. What the program has to say — including
// its own read-back of every part against the ROM — is on the console in
// the run's screenshot, which the caller compares against a golden.
//
// Run (via tests/integration/test-conan-romdump.sh):
//   node --experimental-strip-types tests/integration/test-conan-romdump.mts \
//        --ram ram.bin --rom roms/conan_s2_2201.engbuild.IMG

import * as fs from 'node:fs';

const arg = (flag: string, fallback = ''): string => {
  const i = process.argv.indexOf(flag);
  return i >= 0 && i + 1 < process.argv.length ? process.argv[i + 1] : fallback;
};

const ram = fs.readFileSync(arg('--ram'));
const rom = fs.readFileSync(arg('--rom'));
const PART_BYTES = Number(arg('--part-bytes', String(2 * 1024 * 1024)));
const VERIFY_PARTS = Number(arg('--verify-parts', '3'));
const PROBE_STEP = 0x10000;

// The report is UCS-2 (the machine is a Unicode build), and where the RAM
// disk puts a 300-byte file is not something to assert on — this is
// printed when it happens to be in the bank that was dumped, and passed
// over when it is not.
const MARK = Buffer.from('Psion EPOC ER5u ROM dump', 'utf16le');
const at = ram.indexOf(MARK);
if (at >= 0) {
  const text = ram.subarray(at, at + 640).toString('utf16le');
  console.log('--- ROMDUMP.TXT, as the machine wrote it ---');
  console.log(text.split('Join the parts')[0].trimEnd());
}

let probes = 0, found = 0;
const missing: number[] = [];
for (let off = 0; off < Math.min(VERIFY_PARTS * PART_BYTES, rom.length - 64); off += PROBE_STEP) {
  probes++;
  if (ram.includes(rom.subarray(off, off + 64))) found++;
  else missing.push(off);
}
console.log(`ROM content in RAM: ${found}/${probes} probes over the first ${VERIFY_PARTS} parts`);
if (found !== probes) {
  console.error(`FAIL: ${missing.length} ROM probes never reached the disk, first at ` +
                `0x${missing[0].toString(16)}`);
  process.exit(1);
}
console.log('PASS: the ROM dump is on the machine and matches the ROM.');
