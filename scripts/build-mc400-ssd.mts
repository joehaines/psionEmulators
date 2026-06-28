// Build the Psion MC400 "System Disk" SSD image from the ROM:: disk files
// shipped in roms/MC400_V2.60F_ROM_disk/.
//
// The MC400 (1989 SIBO laptop) shipped with a System Disk SSD inserted in
// Pack D containing the window server, shell, OPL, fonts and the rest of the
// ROM:: filing system. The emulator ships the same files as a directory; this
// script packs them into a factory-style FEFS24 Flash image (the format real
// Psion Flash SSDs used) so the frontend can pre-insert it on cold boot.
//
// The pack is attached as the write-protected ("Protected") SSD type — EPOC16
// only mounts a ROM-image FEFS pack (erased flash count) when the hardware
// info byte declares write-protection, exactly like the genuine System Disk.
//
//   node --experimental-strip-types scripts/build-mc400-ssd.mts \
//        [roms/MC400_V2.60F_ROM_disk] [roms/MC400_V2.60F_system.ssd]
import { createFlashPack, addFileToPack, listFiles } from '../frontend/src/lib/fefs.ts';
import { readFileSync, writeFileSync, readdirSync, statSync } from 'fs';
import { join } from 'path';

const [
  srcDir = 'roms/MC400_V2.60F_ROM_disk',
  out    = 'roms/MC400_V2.60F_system.ssd',
] = process.argv.slice(2);

// 256K Flash pack — the ~170 KB of ROM:: files fit with room to spare, and a
// power-of-two size is required by the SSD attach path.
const PACK_SIZE = 0x40000;

// Stable, deterministic order — readdir order is platform-dependent.
const files = readdirSync(srcDir)
  .filter((name) => name !== 'README.md' && statSync(join(srcDir, name)).isFile())
  .sort();

let pack = createFlashPack(PACK_SIZE, 'MC400');
for (const name of files) {
  const bytes = new Uint8Array(readFileSync(join(srcDir, name)));
  pack = addFileToPack(pack, name, bytes);
}

writeFileSync(out, Buffer.from(pack.buffer, pack.byteOffset, pack.byteLength));

const listed = listFiles(pack);
console.log(`wrote ${out} (${pack.length} bytes), ${listed.length} files:`);
for (const f of listed) console.log(`  ${f.name} (${f.size} bytes)`);
