// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Read and edit the file system inside an EPOC *Release 1* ROM — the
// Psion Series 5 (prototype and shipping), the Series 5's siblings and
// the Geofox One.
//
// It is the same shape as tools/e32/romfs.mts, which reads the ER5u
// (Conan) ROMs, and differs in the two ways R1 differs:
//
//   * names are 8-bit. R1 is a non-Unicode build, so a TRomEntry's
//     iName is iNameLength *bytes*, not that many 16-bit characters.
//   * iRomRootDirectoryList (TRomHeader +0x94) points straight at the
//     TRomDir for Z:\, where the ER5u ROMs put a root-directory *list*
//     there — a count, a hardware variant and the directory's address.
//     Reading an R1 ROM the ER5u way loses the "System" level of every
//     path, which the ROM's own strings (ETest.exe asks the loader for
//     "Z:\system\libs\elink.exe") say is really there.
//
// A TRomEntry is { TInt iSize; TLinAddr iAddressLin; TUint8 iAtt;
// TUint8 iNameLength; TText iName[] }, padded to a word, with 0x10 in
// iAtt marking a directory — that part is the same in both.
//
// A file in the ROM that is an E32Image rather than an execute-in-place
// ROM image is loaded exactly as a copy on C: would be, which is what
// `replace` is for: tests/integration/test-er1-romdump.sh swaps a build
// of tools/romdump-er1 in over Z:\System\Libs\Elink.exe — a program the
// ROM's own ETest.exe starts by name at boot — so the machine loads and
// runs it for real.
//
// Run (node 22+):
//   node --experimental-strip-types tools/e32/romfs1.mts list 'roms/S5_v1.00(113)_eng.bin'
//   node --experimental-strip-types tools/e32/romfs1.mts extract ROM 'Z:\System\Libs\EFSrv.dll' out.dll
//   node --experimental-strip-types tools/e32/romfs1.mts replace ROM 'Z:\System\Libs\Elink.exe' new.exe out.img

import * as fs from 'node:fs';

export interface Rom1File { path: string; addr: number; size: number; entryOffset: number; }

export interface Rom1 { data: Buffer; base: number; size: number; files: Rom1File[]; }

// TRomImageHeader, R1's 0x58-byte one. The later releases grew it; the
// fields below are the ones this repo has confirmed against the Series 5
// and Geofox ROMs (core/arm710.cpp records the same 0x58 size, and that
// +0x38 is iDllRefTable rather than a data/bss base).
export interface RomImage {
  uid1: number; uid2: number; uid3: number;
  entryPoint: number; codeAddress: number; dataAddress: number;
  codeSize: number; textSize: number; dataSize: number; bssSize: number;
  exportDirCount: number; exportDir: number; flags: number;
}

export function readRom1(path: string): Rom1 {
  const data = fs.readFileSync(path);
  const base = data.readUInt32LE(0x8c);
  const size = data.readUInt32LE(0x90);
  const rootDir = data.readUInt32LE(0x94);
  const off = (addr: number): number => addr - base;
  const files: Rom1File[] = [];

  const walk = (dirAddr: number, prefix: string, depth: number): void => {
    if (depth > 8) return;                       // a cycle would otherwise not end
    const start = off(dirAddr);
    if (start < 0 || start + 4 > data.length) return;
    const end = Math.min(start + data.readUInt32LE(start), data.length);
    let p = start + 4;
    while (p + 10 <= end) {
      const entrySize = data.readUInt32LE(p);
      const addr = data.readUInt32LE(p + 4);
      const att = data[p + 8];
      const nameLen = data[p + 9];
      if (nameLen === 0) break;                  // padding, not an entry
      const name = data.subarray(p + 10, p + 10 + nameLen).toString('latin1');
      const path = `${prefix}\\${name}`;
      if (att & 0x10) walk(addr, path, depth + 1);
      else files.push({ path, addr, size: entrySize, entryOffset: p });
      p += (10 + nameLen + 3) & ~3;
    }
  };

  walk(rootDir, 'Z:', 0);
  return { data, base, size, files };
}

export function readRomImage(rom: Rom1, addr: number): RomImage {
  const u = (o: number): number => rom.data.readUInt32LE(addr - rom.base + o);
  return {
    uid1: u(0x00), uid2: u(0x04), uid3: u(0x08),
    entryPoint: u(0x10), codeAddress: u(0x14), dataAddress: u(0x18),
    codeSize: u(0x1c), textSize: u(0x20), dataSize: u(0x24), bssSize: u(0x28),
    exportDirCount: u(0x3c), exportDir: u(0x40), flags: u(0x50),
  };
}

// The address a DLL's ordinal resolves to. The export directory is one
// word per ordinal, ordinal 1 first.
export function exportAddress(rom: Rom1, image: RomImage, ordinal: number): number {
  if (ordinal < 1 || ordinal > image.exportDirCount) {
    throw new Error(`ordinal ${ordinal} is outside the export directory (1..${image.exportDirCount})`);
  }
  return rom.data.readUInt32LE(image.exportDir - rom.base + (ordinal - 1) * 4);
}

export function find(rom: Rom1, path: string): Rom1File {
  const want = path.toLowerCase();
  const hit = rom.files.find(f => f.path.toLowerCase() === want);
  if (!hit) throw new Error(`no such file in the ROM: ${path}`);
  return hit;
}

function main(): void {
  const [cmd, romPath, ...rest] = process.argv.slice(2);
  if (!cmd || !romPath) {
    console.error('usage: romfs1.mts list|extract|replace ROM [args]');
    process.exit(2);
  }
  const rom = readRom1(romPath);

  if (cmd === 'list') {
    console.log(`# ${romPath}: base 0x${rom.base.toString(16)}, ` +
                `declared 0x${rom.size.toString(16)}, image ${rom.data.length} bytes, ` +
                `${rom.files.length} files`);
    for (const f of rom.files) {
      let extra = '';
      const at = f.addr - rom.base;
      if (at >= 0 && at + 0x58 <= rom.data.length) {
        const uid1 = rom.data.readUInt32LE(at);
        if (uid1 === 0x10000079 || uid1 === 0x1000007a) {
          const img = readRomImage(rom, f.addr);
          extra = `\tuid3=0x${img.uid3.toString(16)} exports=${img.exportDirCount}`;
        }
      }
      console.log(`${f.path}\t0x${f.addr.toString(16)}\t${f.size}${extra}`);
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
    if (!out) throw new Error('replace needs PATH SRC OUT [--donor PATH]');
    // --donor names a file whose bytes may be written over when the
    // replacement does not fit where the original was: the entry for
    // PATH is pointed into the donor's space instead. An R1 ROM image
    // is exactly as long as its header declares — there is no slack
    // after the end to grow into, and a machine's ROM window stops
    // where the image does — so taking the room from a file that is not
    // wanted is the way to put a bigger program inside one.
    const donorAt = process.argv.indexOf('--donor');
    const donorPath = donorAt > 0 ? process.argv[donorAt + 1] : undefined;
    // --rename gives the entry a new name of exactly the same length.
    // A directory's entries are packed one after another with no slack,
    // so the length is the one thing that cannot change; what the name
    // says can, and it matters — the Shell decides what a file is from
    // its extension as well as from its UID.
    const renameAt = process.argv.indexOf('--rename');
    const newName = renameAt > 0 ? process.argv[renameAt + 1] : undefined;
    const f = find(rom, path);
    const src = fs.readFileSync(srcPath);
    // The next file starts where it started, so only what the original
    // occupied may be written over. A replacement too big for that goes
    // after the end of the image and the entry is pointed at it; the
    // loader only ever follows iAddressLin. An R1 ROM image is normally
    // exactly as long as its header declares, so that tail has to be
    // made — which is fine for an emulator, whose ROM window is as long
    // as the file it is given, and is not something to do to a real
    // machine's ROM.
    const room = (f.size + 3) & ~3;
    const at = f.addr - rom.base;
    let data: Buffer;
    let where: string;
    if (donorPath && src.length > room) {
      const donor = find(rom, donorPath);
      if (src.length > donor.size) {
        throw new Error(`${srcPath} is ${src.length} bytes: too big for ${donor.path}'s ${donor.size}`);
      }
      data = Buffer.from(rom.data);
      const donorAtOff = donor.addr - rom.base;
      data.fill(0, donorAtOff, donorAtOff + donor.size);
      src.copy(data, donorAtOff);
      data.fill(0, at, at + room);                                  // the old bytes are dead now
      data.writeUInt32LE(donor.addr, f.entryOffset + 4);            // iAddressLin
      where = `over ${donor.path} at 0x${donor.addr.toString(16)}, ` +
              `${src.length} of ${donor.size} bytes`;
    } else if (src.length <= room) {
      data = Buffer.from(rom.data);
      data.fill(0, at, at + room);
      src.copy(data, at);
      where = `in place, ${src.length} of ${room} bytes`;
    } else {
      const appendAt = (rom.data.length + 0xfff) & ~0xfff;
      data = Buffer.alloc(appendAt + ((src.length + 0xfff) & ~0xfff));
      rom.data.copy(data);
      src.copy(data, appendAt);
      data.fill(0, at, at + room);                                  // the old bytes are dead now
      data.writeUInt32LE(rom.base + appendAt, f.entryOffset + 4);   // iAddressLin
      where = `appended at 0x${(rom.base + appendAt).toString(16)}, ${src.length} bytes, ` +
              `image grown to ${data.length}`;
    }
    data.writeUInt32LE(src.length, f.entryOffset);                  // TRomEntry::iSize
    if (newName !== undefined) {
      const oldLen = data[f.entryOffset + 9];
      if (newName.length !== oldLen) {
        throw new Error(`--rename "${newName}" is ${newName.length} characters, ` +
                        `and the entry's name is ${oldLen} — they must match`);
      }
      data.write(newName, f.entryOffset + 10, 'latin1');
      console.log(`${out}: renamed to ${newName}`);
    }
    fs.writeFileSync(out, data);
    console.log(`${out}: ${f.path} <- ${srcPath} (${where})`);
    return;
  }

  throw new Error(`unknown command ${cmd}`);
}

if (process.argv[1] && process.argv[1].endsWith('romfs1.mts')) main();
