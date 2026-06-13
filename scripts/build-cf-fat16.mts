// Build a proper FAT16 CF disk image containing OS.IMG, exactly as the
// frontend's "Insert CF card containing OS" dialog does.  The faithful
// bootloader CF path needs a real FAT16 volume (the raw EPOCARM ROM image
// roms/netbook_os.img is NOT a FAT volume — it's the bare OS.IMG content,
// which the bootloader can't mount).
//
//   node --experimental-strip-types scripts/build-cf-fat16.mts \
//        roms/netbook_os.img /tmp/cf_fat16.img [sizeMB]
import { createBlankImage, addFile, isFat16, listRoot } from '../frontend/src/lib/fat16.ts';
import { readFileSync, writeFileSync } from 'fs';
const [osPath, outPath, sizeMB] = process.argv.slice(2);
if (!osPath || !outPath) { console.error('usage: build-cf-fat16.mts <os.img> <out.img> [sizeMB]'); process.exit(2); }
const os = new Uint8Array(readFileSync(osPath));
const bytes = (Number(sizeMB) || 24) * 1024 * 1024;
const img = createBlankImage(bytes);
const r = addFile(img, 'OS.IMG', os);
if (!r.ok) { console.error('addFile failed:', r.reason); process.exit(1); }
writeFileSync(outPath, Buffer.from(img.buffer, img.byteOffset, img.byteLength));
console.log(`wrote ${outPath}: ${img.length} bytes, isFat16=${isFat16(img)}, root=${listRoot(img).map(e=>e.name+'('+e.size+')').join(',')}`);
