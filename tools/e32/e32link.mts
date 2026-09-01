// Turn a flat ARM binary into an EPOC32 E32Image (`.exe`) that an EPOC
// Release 5 loader will load, relocate and run.
//
// EPOC's own tools (petran) are not available and would not run here, so
// this emits the container by hand.  Every field below was read out of a
// real ER5u binary rather than assumed: Z:\System\Samples\D_EXC.exe in
// roms/conan_s2_2201.engbuild.IMG is a RAM-format E32Image sitting in the
// ROM as a plain file, so it doubles as the format's worked example (see
// docs/conan-rom-dumping.md, "The container").
//
//   0x00  iUid1 iUid2 iUid3 iUidChecksum   KExecutableImageUid + CCITT pair
//   0x10  'EPOC'  iCpuIdentifier(0x2000 = ARM)
//   0x18  iCheckSumCode  — plain 32-bit word sum of the code section
//   0x20  iVersion(tools) iTime(TInt64) iFlags(2 = EXE, 3 = DLL)
//   0x30  iCodeSize iDataSize iHeapSizeMin iHeapSizeMax iStackSize iBssSize
//   0x48  iEntryPoint iCodeBase iDataBase iDllRefTableCount
//   0x58  iExportDirOffset iExportDirCount iTextSize
//   0x64  iCodeOffset iDataOffset iImportOffset iCodeRelocOffset
//   0x74  iDataRelocOffset iPriority                        (header = 0x7c)
//
// The code section is laid out text | import address table | (const data),
// with iTextSize giving the IAT's offset: the loader walks the import
// blocks in order and overwrites IAT slot k — which holds the ordinal
// until then — with the address that ordinal resolves to.  Relocations are
// 16-bit (type << 12) | offset-in-page entries, and this tool finds them
// the way a flat-binary toolchain has to: the same objects are linked
// twice at two different bases and every word that moved by the delta is
// an absolute address that needs one.
//
// Run (node 22+):
//   node --experimental-strip-types tools/e32/e32link.mts \
//        --bin build/a.bin --bin2 build/b.bin --syms build/a.nm \
//        --base 0x400000 --base2 0x500000 \
//        --import 'EFSRV[100039e4].DLL:18,151,194,15' \
//        --uid3 0x10004242 --out romdump.exe
//
// tools/romdump/build.sh drives it; see tools/e32/README.md.

import * as fs from 'node:fs';

export interface ImportBlock { dll: string; ordinals: number[]; }

const KExecutableImageUid = 0x1000007a;
const KDynamicLibraryUid = 0x10000079;
const HEADER_SIZE = 0x7c;
// The tools version and CPU id the ER5u ROM's own binaries carry. The
// loader compares iVersion against what it can handle, so it is copied
// rather than invented.
const TOOLS_VERSION = 0x00ad0001;
const CPU_ARM = 0x2000;
const KInferredRelocType = 0x3000;
const RELOC_PAGE = 0x1000;

// ── CLI ─────────────────────────────────────────────────────────────

function arg(flag: string, fallback?: string): string {
  const i = process.argv.indexOf(flag);
  if (i >= 0 && i + 1 < process.argv.length) return process.argv[i + 1];
  if (fallback !== undefined) return fallback;
  throw new Error(`missing ${flag}`);
}
function argAll(flag: string): string[] {
  const out: string[] = [];
  process.argv.forEach((a, i) => { if (a === flag) out.push(process.argv[i + 1]); });
  return out;
}
const num = (s: string): number => {
  const v = Number(s);
  if (!Number.isFinite(v)) throw new Error(`not a number: ${s}`);
  return v >>> 0;
};

// ── Checksums ───────────────────────────────────────────────────────

// CCITT CRC-16, poly 0x1021, MSB first, zero seed — EPOC's Mem::Crc.
function crc16(data: Uint8Array): number {
  let c = 0;
  for (const b of data) {
    c ^= b << 8;
    for (let i = 0; i < 8; i++) c = (c & 0x8000) ? ((c << 1) ^ 0x1021) & 0xffff : (c << 1) & 0xffff;
  }
  return c & 0xffff;
}

// TCheckedUid: CRC of the even-numbered bytes of the three UIDs in the low
// half, of the odd-numbered bytes in the high half.  Verified against the
// Conan ROM: (1000007a,0,0) -> 045ac39e, (10000079,1000008d,100039e4) ->
// ea0535b6.
export function uidChecksum(uid1: number, uid2: number, uid3: number): number {
  const b = Buffer.alloc(12);
  b.writeUInt32LE(uid1 >>> 0, 0); b.writeUInt32LE(uid2 >>> 0, 4); b.writeUInt32LE(uid3 >>> 0, 8);
  const even = Uint8Array.from([0, 2, 4, 6, 8, 10].map(i => b[i]));
  const odd = Uint8Array.from([1, 3, 5, 7, 9, 11].map(i => b[i]));
  return ((crc16(odd) << 16) | crc16(even)) >>> 0;
}

// iCheckSumCode / iCheckSumData: a plain 32-bit sum of the section's words.
export function wordSum(b: Buffer): number {
  let s = 0;
  for (let o = 0; o + 4 <= b.length; o += 4) s = (s + b.readUInt32LE(o)) >>> 0;
  return s >>> 0;
}

// ── Relocations by double-link diff ─────────────────────────────────

// Two links of the same objects, `delta` apart. Any word that moved by
// exactly delta held an absolute address and needs a relocation entry; a
// word that moved by anything else means the two links are not the same
// program, which would silently corrupt the image, so it is fatal.
export function findRelocs(a: Buffer, b: Buffer, delta: number): number[] {
  if (a.length !== b.length) throw new Error(`link sizes differ: ${a.length} vs ${b.length}`);
  const out: number[] = [];
  for (let o = 0; o + 4 <= a.length; o += 4) {
    const va = a.readUInt32LE(o), vb = b.readUInt32LE(o);
    if (va === vb) continue;
    if (((vb - va) >>> 0) !== (delta >>> 0)) {
      throw new Error(`word at code+0x${o.toString(16)} differs by 0x${((vb - va) >>> 0).toString(16)}, ` +
                      `not the link delta 0x${delta.toString(16)} — the two links are not equivalent`);
    }
    out.push(o);
  }
  return out;
}

export function buildRelocSection(offsets: number[]): Buffer {
  const pages = new Map<number, number[]>();
  for (const o of offsets) {
    const page = o & ~(RELOC_PAGE - 1);
    if (!pages.has(page)) pages.set(page, []);
    pages.get(page)!.push(o - page);
  }
  const blocks: Buffer[] = [];
  for (const page of [...pages.keys()].sort((x, y) => x - y)) {
    const ents = pages.get(page)!.sort((x, y) => x - y);
    // Entries are padded to a whole number of words, as petran does.
    const count = ents.length + (ents.length & 1);
    const blk = Buffer.alloc(8 + count * 2);
    blk.writeUInt32LE(page, 0);
    blk.writeUInt32LE(blk.length, 4);
    ents.forEach((e, i) => blk.writeUInt16LE(KInferredRelocType | e, 8 + i * 2));
    blocks.push(blk);
  }
  const body = Buffer.concat(blocks);
  const sec = Buffer.alloc(8 + body.length);
  // iSize counts the relocation data only — the 8-byte section header is
  // not included (it is in the import section; the two differ).
  sec.writeUInt32LE(body.length, 0);
  sec.writeUInt32LE(offsets.length, 4);
  body.copy(sec, 8);
  return sec;
}

// ── Import section ──────────────────────────────────────────────────

export function buildImportSection(blocks: ImportBlock[]): Buffer {
  const names = blocks.map(b => Buffer.from(b.dll + '\0', 'latin1'));
  let size = 4;
  for (const b of blocks) size += 8 + b.ordinals.length * 4;
  const namesAt = size;
  for (const n of names) size += n.length;
  const sec = Buffer.alloc(size);
  sec.writeUInt32LE(size, 0);          // iSize covers the whole section
  let p = 4, nameOff = namesAt;
  blocks.forEach((b, i) => {
    sec.writeUInt32LE(nameOff, p);     // iOffsetOfDllName, from section start
    sec.writeUInt32LE(b.ordinals.length, p + 4);
    b.ordinals.forEach((o, j) => sec.writeUInt32LE(o >>> 0, p + 8 + j * 4));
    p += 8 + b.ordinals.length * 4;
    names[i].copy(sec, nameOff);
    nameOff += names[i].length;
  });
  return sec;
}

// ── Symbols ─────────────────────────────────────────────────────────

// `llvm-nm` output: "00400f2c T __iat_start".
function readSyms(path: string): Map<string, number> {
  const m = new Map<string, number>();
  for (const line of fs.readFileSync(path, 'utf8').split('\n')) {
    const parts = line.trim().split(/\s+/);
    if (parts.length >= 3 && /^[0-9a-f]+$/i.test(parts[0])) m.set(parts[2], parseInt(parts[0], 16));
  }
  return m;
}

// ── Main ────────────────────────────────────────────────────────────

function main(): void {
  const base = num(arg('--base', '0x400000'));
  const base2 = num(arg('--base2', '0x500000'));
  const bin = fs.readFileSync(arg('--bin'));
  const bin2 = fs.readFileSync(arg('--bin2'));
  const syms = readSyms(arg('--syms'));
  const out = arg('--out');
  const isDll = process.argv.includes('--dll');

  const blocks: ImportBlock[] = argAll('--import').map(spec => {
    const [dll, ords] = spec.split(':');
    return { dll, ordinals: ords.split(',').filter(s => s.length).map(num) };
  });

  const need = (n: string): number => {
    const v = syms.get(n);
    if (v === undefined) throw new Error(`linker script did not define ${n}`);
    return v;
  };
  const iatStart = need('__iat_start') - base;
  const iatEnd = need('__iat_end') - base;
  const codeEnd = need('__code_end') - base;
  const bssSize = need('__bss_end') - need('__bss_start');
  const entry = need('_entry') - base;

  if (codeEnd !== bin.length) {
    throw new Error(`__code_end 0x${codeEnd.toString(16)} != linked image length 0x${bin.length.toString(16)}`);
  }
  if (entry !== 0) throw new Error('_entry must be the first thing in the code section');

  // The IAT the linker laid down must hold exactly the ordinals of the
  // import table, in import-table order, plus the terminating zero the
  // ROM's own binaries carry: the loader fills slot k from import k.
  const wanted = blocks.flatMap(b => b.ordinals).concat([0]);
  const got: number[] = [];
  for (let o = iatStart; o < iatEnd; o += 4) got.push(bin.readUInt32LE(o));
  if (got.length !== wanted.length || got.some((v, i) => v !== wanted[i])) {
    throw new Error(`.iat section [${got}] does not match --import ordinals [${wanted}]`);
  }

  const relocs = findRelocs(bin, bin2, base2 - base);
  const importSec = buildImportSection(blocks);
  const relocSec = buildRelocSection(relocs);

  const codeSize = bin.length;
  const textSize = iatStart;
  const importOffset = HEADER_SIZE + codeSize;
  const relocOffset = importOffset + importSec.length;

  const h = Buffer.alloc(HEADER_SIZE);
  const uid1 = isDll ? KDynamicLibraryUid : KExecutableImageUid;
  const uid2 = num(arg('--uid2', '0'));
  const uid3 = num(arg('--uid3', '0'));
  h.writeUInt32LE(uid1, 0x00);
  h.writeUInt32LE(uid2, 0x04);
  h.writeUInt32LE(uid3, 0x08);
  h.writeUInt32LE(uidChecksum(uid1, uid2, uid3), 0x0c);
  h.write('EPOC', 0x10, 'latin1');
  h.writeUInt32LE(CPU_ARM, 0x14);
  h.writeUInt32LE(wordSum(bin), 0x18);          // iCheckSumCode
  h.writeUInt32LE(0, 0x1c);                     // iCheckSumData (no data section)
  h.writeUInt32LE(TOOLS_VERSION, 0x20);
  // iTime, a TInt64 of microseconds since year 0 — the same epoch EPOC's
  // TTime uses. Fixed by default so builds are reproducible.
  const t = BigInt(arg('--time', '63183916800000000'));
  h.writeUInt32LE(Number(t & 0xffffffffn), 0x24);
  h.writeUInt32LE(Number(t >> 32n), 0x28);
  h.writeUInt32LE(isDll ? 3 : 2, 0x2c);         // iFlags
  h.writeUInt32LE(codeSize, 0x30);
  h.writeUInt32LE(0, 0x34);                     // iDataSize
  h.writeUInt32LE(num(arg('--heap-min', '0x1000')), 0x38);
  h.writeUInt32LE(num(arg('--heap-max', '0x100000')), 0x3c);
  h.writeUInt32LE(num(arg('--stack', '0x2000')), 0x40);
  h.writeUInt32LE(bssSize, 0x44);
  h.writeUInt32LE(entry, 0x48);
  h.writeUInt32LE(base, 0x4c);                  // iCodeBase
  h.writeUInt32LE(base + codeSize, 0x50);       // iDataBase — .bss follows the code
  h.writeUInt32LE(blocks.length, 0x54);         // iDllRefTableCount
  h.writeUInt32LE(0, 0x58);                     // iExportDirOffset
  h.writeUInt32LE(0, 0x5c);                     // iExportDirCount
  h.writeUInt32LE(textSize, 0x60);
  h.writeUInt32LE(HEADER_SIZE, 0x64);           // iCodeOffset
  h.writeUInt32LE(0, 0x68);                     // iDataOffset
  h.writeUInt32LE(importOffset, 0x6c);
  h.writeUInt32LE(relocOffset, 0x70);           // iCodeRelocOffset
  h.writeUInt32LE(0, 0x74);                     // iDataRelocOffset
  h.writeUInt32LE(num(arg('--priority', '0x15e')), 0x78);  // EPriorityForeground

  fs.writeFileSync(out, Buffer.concat([h, bin, importSec, relocSec]));
  console.log(`${out}: ${HEADER_SIZE + codeSize + importSec.length + relocSec.length} bytes ` +
              `(text 0x${textSize.toString(16)}, iat 0x${(iatEnd - iatStart).toString(16)}, ` +
              `bss 0x${bssSize.toString(16)}, ${relocs.length} relocs, ` +
              `${blocks.reduce((n, b) => n + b.ordinals.length, 0)} imports)`);
}

if (process.argv[1] && process.argv[1].endsWith('e32link.mts')) main();
