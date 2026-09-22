// Join the two Psion HC120 ROM chip dumps in roms/hc120/ into the single
// 256 KiB image the emulator loads.
//
// The HC120's ROM is two 128 KiB flash chips on the V30H's bus, dumped
// one chip per file. Unlike the MC's pair (even and odd bytes of a 16-bit
// bus — see build-mc200-rom.mts) these are linear halves, so the join is
// a concatenation: v172f_1.bin is the lower half and carries the SIBO
// boot block, v172f_2.bin the upper half and carries the reset vector.
//
// Where the halves land in the address space is not a straight run: the
// chips answer at 0xA0000-0xDFFFF, and the top 128 KiB window
// (0xE0000-0xFFFFF) — which has to answer something, because the CPU
// fetches its reset vector from 0xFFFF0 — reads the upper chip a second
// time. The emulator models that alias (Series3::Config::romAliasSize),
// so the image here stays exactly what the two chips hold. See
// docs/hc120-rom.md.
//
//   node --experimental-strip-types scripts/build-hc120-rom.mts \
//        [roms/hc120] [roms/hc120_v1.72F.bin]
//
// The joined image is committed, so this script only needs re-running if
// the chip dumps are ever replaced.
import { readFileSync, writeFileSync } from 'fs';
import { join } from 'path';

const [
  srcDir = 'roms/hc120',
  out    = 'roms/hc120_v1.72F.bin',
] = process.argv.slice(2);

const lower = new Uint8Array(readFileSync(join(srcDir, 'v172f_1.bin')));
const upper = new Uint8Array(readFileSync(join(srcDir, 'v172f_2.bin')));

if (lower.length !== upper.length) {
  throw new Error(`chip dumps differ in size: ${lower.length} vs ${upper.length}`);
}

const rom = new Uint8Array(lower.length + upper.length);
rom.set(lower, 0);
rom.set(upper, lower.length);

// Sanity-check both ends rather than writing a silently wrong image: the
// lower chip opens with the SIBO boot block's three near jumps, and the
// upper one closes with the reset vector that jumps to it.
if (!(rom[0] === 0xE9 && rom[3] === 0xE9 && rom[6] === 0xE9)) {
  throw new Error(
    `${srcDir}/v172f_1.bin does not start with a SIBO boot block ` +
    `(${[...rom.subarray(0, 9)].map((b) => b.toString(16).padStart(2, '0')).join(' ')}) ` +
    '— are the halves the wrong way round?');
}
const resetVector = rom.subarray(rom.length - 16, rom.length - 11);
const expected    = [0xEA, 0x00, 0x00, 0x00, 0xA0];
if (!expected.every((b, i) => resetVector[i] === b)) {
  throw new Error(
    `reset vector at 0x${(rom.length - 16).toString(16)} reads ` +
    `${[...resetVector].map((b) => b.toString(16).padStart(2, '0')).join(' ')}, ` +
    'expected ea 00 00 00 a0');
}

writeFileSync(out, Buffer.from(rom.buffer, rom.byteOffset, rom.byteLength));

const date = Buffer.from(rom.subarray(rom.length - 10, rom.length - 4)).toString('ascii');
console.log(`wrote ${out} (${rom.length} bytes), build date ${date}`);
