// Read and edit the file system inside an EPOC32 ROM image.
//
// An EPOC32 ROM carries its own read-only directory tree: TRomHeader at
// the image's base gives iRomBase (+0x8c), iRomSize (+0x90) and
// iRomRootDirectoryList (+0x94); each TRomDir is a length followed by
// TRomEntry records { TInt iSize; TLinAddr iAddressLin; TUint8 iAtt;
// TUint8 iNameLength; TText iName[] }, 4-byte aligned, with 0x10 in iAtt
// marking a directory. On a Unicode (ER5u) build the names are UCS-2,
// which is what tells the two generations apart.
//
//   list     — print the tree (path, linear address, size)
//   extract  — write one file's bytes out
//   replace  — swap one file's bytes for another's, fixing up its entry
//
// `replace` is how tests/integration/test-conan-romdump.sh puts a build
// of tools/romdump on the machine: the Conan's engineering ROM shows
// Z:\System\Samples\D_EXC.exe on its desktop, so a binary swapped in
// there is one keypress from running. A replacement that fits goes in
// place; one that does not is appended past the end of the image and the
// entry pointed at it. Either way nothing else in the image moves.
//
// Run (node 22+):
//   node --experimental-strip-types tools/e32/romfs.mts list roms/conan_s2_2201.engbuild.IMG
//   node --experimental-strip-types tools/e32/romfs.mts extract ROM 'Z:\System\Samples\D_EXC.exe' out.exe
//   node --experimental-strip-types tools/e32/romfs.mts replace ROM 'Z:\...\D_EXC.exe' new.exe out.img

import * as fs from 'node:fs';

export interface RomFile { path: string; addr: number; size: number; entryOffset: number; }

export interface Rom { data: Buffer; base: number; size: number; files: RomFile[]; }

export function readRom(path: string): Rom {
  const data = fs.readFileSync(path);
  const base = data.readUInt32LE(0x8c);
  const size = data.readUInt32LE(0x90);
  const rootList = data.readUInt32LE(0x94);
  const off = (addr: number): number => addr - base;
  const files: RomFile[] = [];

  const walk = (dirAddr: number, prefix: string): void => {
    const start = off(dirAddr);
    const end = start + data.readUInt32LE(start);
    let p = start + 4;
    while (p < end) {
      const entrySize = data.readUInt32LE(p);
      const addr = data.readUInt32LE(p + 4);
      const att = data[p + 8];
      const nameLen = data[p + 9];
      const name = data.subarray(p + 10, p + 10 + nameLen * 2).toString('utf16le');
      if (att & 0x10) walk(addr, `${prefix}\\${name}`);
      else files.push({ path: `${prefix}\\${name}`, addr, size: entrySize, entryOffset: p });
      p += (10 + nameLen * 2 + 3) & ~3;
    }
  };

  const roots = data.readUInt32LE(off(rootList));
  for (let i = 0; i < roots; i++) walk(data.readUInt32LE(off(rootList) + 8 + i * 8), 'Z:');
  return { data, base, size, files };
}

function find(rom: Rom, path: string): RomFile {
  const want = path.toLowerCase();
  const hit = rom.files.find(f => f.path.toLowerCase() === want);
  if (!hit) throw new Error(`no such file in the ROM: ${path}`);
  return hit;
}

function main(): void {
  const [cmd, romPath, ...rest] = process.argv.slice(2);
  if (!cmd || !romPath) {
    console.error('usage: romfs.mts list|extract|replace ROM [args]');
    process.exit(2);
  }
  const rom = readRom(romPath);

  if (cmd === 'list') {
    console.log(`# ${romPath}: base 0x${rom.base.toString(16)}, ` +
                `declared 0x${rom.size.toString(16)}, image ${rom.data.length} bytes, ` +
                `${rom.files.length} files`);
    for (const f of rom.files) {
      console.log(`${f.path}\t0x${f.addr.toString(16)}\t${f.size}`);
    }
    return;
  }

  if (cmd === 'extract') {
    const [path, out] = rest;
    const f = find(rom, path);
    const at = f.addr - rom.base;
    fs.writeFileSync(out, rom.data.subarray(at, at + f.size));
    console.log(`${out}: ${f.size} bytes from ${f.path}`);
    return;
  }

  if (cmd === 'replace') {
    const [path, srcPath, out] = rest;
    if (!out) throw new Error('replace needs PATH SRC OUT');
    const f = find(rom, path);
    const src = fs.readFileSync(srcPath);
    // The next file starts where it started, so only what the original
    // occupied may be written over. A replacement too big for that is
    // put after the end of the image instead and the entry is pointed at
    // it — a ROM whose header declares more than the image delivers (the
    // Conan's declares 12 MB and stops at 0xBB2000) has room there, and
    // the loader only ever follows iAddressLin.
    const room = (f.size + 3) & ~3;
    const at = f.addr - rom.base;
    let data: Buffer;
    let where: string;
    if (src.length <= room) {
      data = Buffer.from(rom.data);
      data.fill(0, at, at + room);
      src.copy(data, at);
      where = `in place, ${src.length} of ${room} bytes`;
    } else {
      const appendAt = (rom.data.length + 3) & ~3;
      if (appendAt + src.length > rom.size) {
        throw new Error(`${srcPath} is ${src.length} bytes: too big for ${f.path}'s ` +
                        `${room} and past the ROM's declared end ` +
                        `(0x${rom.size.toString(16)})`);
      }
      data = Buffer.alloc(appendAt + src.length);
      rom.data.copy(data);
      src.copy(data, appendAt);
      data.fill(0, at, at + room);          // the old bytes are dead now
      data.writeUInt32LE(rom.base + appendAt, f.entryOffset + 4);  // iAddressLin
      where = `appended at 0x${(rom.base + appendAt).toString(16)}, ${src.length} bytes`;
    }
    data.writeUInt32LE(src.length, f.entryOffset);   // TRomEntry::iSize
    fs.writeFileSync(out, data);
    console.log(`${out}: ${f.path} <- ${srcPath} (${where})`);
    return;
  }

  throw new Error(`unknown command ${cmd}`);
}

if (process.argv[1] && process.argv[1].endsWith('romfs.mts')) main();
