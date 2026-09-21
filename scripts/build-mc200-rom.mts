// Join the two Psion MC200 boot-ROM dumps in roms/MC200_V2.12F_ROM_disk/
// into the single 256 KiB image the emulator loads.
//
// The MC200 (like the MC400 and MC Word) carries its boot ROM in two
// 28F010 128 KiB flash chips wired across the V30's 16-bit bus: chip 0
// holds the even bytes, chip 1 the odd ones. The dump in this tree keeps
// the chips as they were read — v212f_0.bin and v212f_1.bin, byte-for-byte
// identical to MAME's `mc200` ROM set (CRC32 ff346271 / 4f266410) — so
// neither file on its own contains runnable code. Interleaving them
// reconstructs the image the CPU actually sees at 0xC0000-0xFFFFF, ending
// in the reset vector `EA 00 00 00 C0` (far jump to C000:0000, the start
// of the ROM) at 0xFFFF0 and the build date "081090".
//
//   node --experimental-strip-types scripts/build-mc200-rom.mts \
//        [roms/MC200_V2.12F_ROM_disk] [roms/MC200_v2.12F.bin]
//
// The joined image is committed, so this script only needs re-running if
// the chip dumps are ever replaced.
import { readFileSync, writeFileSync } from 'fs';
import { join } from 'path';

const [
  srcDir = 'roms/MC200_V2.12F_ROM_disk',
  out    = 'roms/MC200_v2.12F.bin',
] = process.argv.slice(2);

const even = new Uint8Array(readFileSync(join(srcDir, 'v212f_0.bin')));
const odd  = new Uint8Array(readFileSync(join(srcDir, 'v212f_1.bin')));

if (even.length !== odd.length) {
  throw new Error(`chip dumps differ in size: ${even.length} vs ${odd.length}`);
}

const rom = new Uint8Array(even.length * 2);
for (let i = 0; i < even.length; i++) {
  rom[i * 2]     = even[i];
  rom[i * 2 + 1] = odd[i];
}

// Sanity-check the reconstruction rather than writing a silently wrong
// image: the last paragraph of a SIBO1 laptop ROM is the CPU's reset
// vector, and getting the two chips the wrong way round scrambles it.
const resetVector = rom.subarray(rom.length - 16, rom.length - 11);
const expected    = [0xEA, 0x00, 0x00, 0x00, 0xC0];
if (!expected.every((b, i) => resetVector[i] === b)) {
  throw new Error(
    `reset vector at 0x${(rom.length - 16).toString(16)} reads ` +
    `${[...resetVector].map((b) => b.toString(16).padStart(2, '0')).join(' ')}, ` +
    'expected ea 00 00 00 c0 — are the even/odd chips swapped?');
}

writeFileSync(out, Buffer.from(rom.buffer, rom.byteOffset, rom.byteLength));

const date = Buffer.from(rom.subarray(rom.length - 10, rom.length - 4)).toString('ascii');
console.log(`wrote ${out} (${rom.length} bytes), build date ${date}`);
