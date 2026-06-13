// Build a FAT16 CompactFlash disk image containing OS.IMG, the way the
// frontend's "Insert CF card containing OS" dialog does — so the bootloader's
// FAITHFUL CF path (PSION_NB_NATIVE_CF[_NO_HANDOFF]) can mount the volume and
// read D:\OS.IMG through its own driver.  The raw netbook_os.img is NOT a FAT
// volume (it starts with "EPOCARM ROM"), so the faithful path can't mount it;
// only the synthetic netBookLoadOsFromCard handoff handles the raw image.
//
//   node --experimental-strip-types scripts/build-cf-fat16-image.mts \
//        roms/netbook_os.img /tmp/cf_fat16.img [sizeMB]
import { createBlankImage, addFile, isFat16, listRoot } from '../frontend/src/lib/fat16.ts';
import { readFileSync, writeFileSync } from 'fs';
const [src = 'roms/netbook_os.img', out = '/tmp/cf_fat16.img', mb = '24'] = process.argv.slice(2);
const os = new Uint8Array(readFileSync(src));
const img = createBlankImage(parseInt(mb, 10) * 1024 * 1024);
const r = addFile(img, 'OS.IMG', os);
if (!r.ok) { console.error('addFile failed:', r.reason); process.exit(1); }
if (!isFat16(img)) { console.error('not FAT16'); process.exit(1); }
writeFileSync(out, Buffer.from(img.buffer, img.byteOffset, img.byteLength));
console.log(`wrote ${out} (${img.length} bytes), root=${listRoot(img).map(e => e.name + '/' + e.size)}`);
