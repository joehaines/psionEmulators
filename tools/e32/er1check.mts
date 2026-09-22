// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Two things about an EPOC Release 1 machine that a ROM dumper has to
// get right, both answered out of the machine's own ROM.
//
//   ordinals ROM
//     Which EFSRV export is which. The file server client has 205
//     exports and no names, so each one this repo uses is identified by
//     something only it does — the function code it sends to the file
//     server, or the literal it loads — rather than by an SDK header
//     nobody has. Printing the same six numbers for the Series 5
//     prototype, the shipping Series 5 and the Geofox is the evidence
//     that one binary fits all three.
//
//   check ROM EXE
//     Whether that ROM's loader will load this E32Image. The rules are
//     not guessed: they are the ones EFile.exe's own validator applies,
//     read out of the ROM (the 'EPOC' signature, then every size and
//     offset in the header range-checked), plus the two things that
//     actually stop the ER5 dumpers — a DLL named by a UID the ROM does
//     not have, and an ordinal past the end of a DLL's export table.
//
// Run (node 22+):
//   node --experimental-strip-types tools/e32/er1check.mts ordinals 'roms/S5_v1.00(113)_eng.bin'
//   node --experimental-strip-types tools/e32/er1check.mts check ROM tools/romdump-er1/ROMDUMP.EXE

import * as fs from 'node:fs';
import { readRom1, readRomImage, find, type Rom1, type RomImage } from './romfs1.mts';
import { uidChecksum, wordSum } from './e32link.mts';

// ── Finding a DLL in the ROM ────────────────────────────────────────

export interface RomDll { path: string; addr: number; image: RomImage; }

export function findDll(rom: Rom1, leaf: string): RomDll {
  const want = `\\${leaf.toLowerCase()}`;
  const hit = rom.files.find(f => f.path.toLowerCase().endsWith(want));
  if (!hit) throw new Error(`no ${leaf} in this ROM`);
  return { path: hit.path, addr: hit.addr, image: readRomImage(rom, hit.addr) };
}

// ── Identifying the file server's exports ───────────────────────────

// Every identification below is a fingerprint of what the export does
// on the wire, not of where it sits. The file server's own function
// codes are the strongest of them: four exports create a subsession
// with the consecutive codes 0x1b, 0x1c, 0x1d, 0x1e, which are
// RFile::Open, Create, Replace and Temp in that order — Temp is the one
// with the extra out-parameter, which is how the run is anchored.
export interface FsOrdinals {
  close: number;      // RFile::Close()
  connect: number;    // RFs::Connect(TInt)
  open: number;       // RFile::Open(RFs&, const TDesC8&, TUint)
  create: number;     // RFile::Create(RFs&, const TDesC8&, TUint)
  replace: number;    // RFile::Replace(RFs&, const TDesC8&, TUint)
  temp: number;       // RFile::Temp(...)
  read: number;       // RFile::Read(TDes8&)
  write: number;      // RFile::Write(const TDesC8&)
}

const MOV_R1_1A = 0xe3a0101a;   // mov r1, #26      RSubSessionBase::CloseSubSession's code
const MOV_R3_SP = 0xe1a0300d;   // mov r3, sp       the argument block, for CreateSubSession
const MOV_R1_1F = 0xe3a0101f;   // mov r1, #31      EFsFileRead
const MOV_R1_20 = 0xe3a01020;   // mov r1, #32      EFsFileWrite
const MOV_R3_80 = 0xe3a03480;   // mov r3, #0x80000000   "at the current position"
const LDR_R3_R1_4 = 0xe5913004; // ldr r3, [r1, #4]      TDes8::iMaxLength
const BIC_TOP_NIB = 0xe3c334f0; // bic r3, r3, #0xf0000000   the descriptor's type nibble
const MOV_R3_R2 = 0xe1a03002;   // mov r3, r2       an asynchronous call's TRequestStatus&

export function fileServerOrdinals(rom: Rom1): FsOrdinals {
  const dll = findDll(rom, 'efsrv.dll');
  const { codeAddress, codeSize, exportDirCount } = dll.image;
  const rd = (a: number): number => rom.data.readUInt32LE(a - rom.base);

  const addr: number[] = [];
  for (let i = 1; i <= exportDirCount; i++) addr.push(rd(dll.image.exportDir + (i - 1) * 4));
  // Each export runs to the start of the next one in address order.
  const sorted = [...addr].map((a, i) => ({ a, ord: i + 1 })).sort((x, y) => x.a - y.a);
  const endOf = new Map<number, number>();
  sorted.forEach((e, i) => endOf.set(e.ord, i + 1 < sorted.length ? sorted[i + 1].a : codeAddress + codeSize));
  const body = (ord: number): number[] => {
    const words: number[] = [];
    for (let a = addr[ord - 1]; a < Math.min(endOf.get(ord)!, addr[ord - 1] + 0x100); a += 4) words.push(rd(a));
    return words;
  };

  const only = (what: string, pred: (w: number[], ord: number) => boolean): number => {
    const hits: number[] = [];
    for (let ord = 1; ord <= exportDirCount; ord++) if (pred(body(ord), ord)) hits.push(ord);
    if (hits.length !== 1) throw new Error(`${what}: ${hits.length} exports match (${hits.join(',')}), wanted exactly one`);
    return hits[0];
  };

  // RFile::Close: the whole body is "mov r1, #0x1a" and a branch to
  // CloseSubSession, which clears the two words of the handle. Two
  // exports have exactly that body — the other belongs to one of the
  // file server's other subsession classes — so the shape alone does
  // not say which, and the machine is asked instead: whichever one the
  // ROM's own binaries close their files with (below).
  const closes: number[] = [];
  for (let ord = 1; ord <= exportDirCount; ord++) {
    const w = body(ord);
    if (w.length >= 2 && w[0] === MOV_R1_1A && (w[1] >>> 24) === 0xea) closes.push(ord);
  }
  if (closes.length === 0) throw new Error('no export closes a subsession with code 0x1a');

  // RFs::Connect: the only export that loads the address of the
  // "FileServer" literal, which is in EFSRV's own constants and is
  // 8-bit here because R1 is a non-Unicode build.
  const nameAt = rom.data.indexOf(Buffer.from('FileServer\0', 'latin1'), codeAddress - rom.base);
  if (nameAt < 0 || nameAt >= codeAddress - rom.base + codeSize) {
    throw new Error('no "FileServer" literal inside EFSRV');
  }
  const nameAddr = rom.base + nameAt;
  const connect = only('RFs::Connect', w => w.includes(nameAddr));

  // The subsession four, by their consecutive function codes.
  const subsession = (code: number, what: string): number =>
    only(what, w => w.includes(MOV_R3_SP) && w.includes(((0xe3a02000 | code) >>> 0)));
  const open = subsession(0x1b, 'RFile::Open');
  const create = subsession(0x1c, 'RFile::Create');
  const replace = subsession(0x1d, 'RFile::Replace');
  const temp = subsession(0x1e, 'RFile::Temp');

  // RFile::Read(TDes8&): function code 0x1f with {aDes, aDes.iMaxLength,
  // 0x80000000} — the last being "at the current position", which is
  // what separates it from the overloads that take one. Its
  // asynchronous twin builds the same argument block and then puts the
  // caller's TRequestStatus& in r3 as a fourth argument, so "no
  // mov r3, r2" is what is left of the eight overloads.
  const read = only('RFile::Read(TDes8&)',
    w => w.includes(MOV_R1_1F) && w.includes(MOV_R3_80) && w.includes(LDR_R3_R1_4) &&
         !w.includes(MOV_R3_R2));

  // RFile::Write(const TDesC8&): function code 0x20 with the length
  // taken from the descriptor with its type nibble masked off — and the
  // early return when that length is zero, which is the masking done
  // twice and is what separates it from the overload taking a length.
  const write = only('RFile::Write(const TDesC8&)',
    w => w.includes(MOV_R1_20) && w.includes(MOV_R3_80) &&
         w.filter(x => x === BIC_TOP_NIB).length >= 2 && !w.includes(MOV_R3_R2));

  // Which close goes with RFile is settled by what the ROM does with
  // them: every binary in the ROM that opens, replaces, reads or writes
  // a file imports one of the candidates, and nothing imports the
  // other. An execute-in-place binary reaches an import through
  // "ldr r12,[pc]; ldr pc,[r12]" and a pointer to a table slot the ROM
  // builder has already filled in with the target's address, so the set
  // of EFSRV exports a ROM binary uses can simply be read off.
  const users = new Map<number, number>();      // ordinal -> binaries using it
  for (const ord of [...closes, open, replace, write]) users.set(ord, 0);
  const byAddress = new Map<number, number>();
  for (let i = 1; i <= exportDirCount; i++) {
    const a = rd(dll.image.exportDir + (i - 1) * 4);
    if (!byAddress.has(a)) byAddress.set(a, i);
  }
  for (const file of rom.files) {
    const at = file.addr - rom.base;
    if (at < 0 || at + 0x58 > rom.data.length) continue;
    const uid1 = rom.data.readUInt32LE(at);
    if (uid1 !== 0x10000079 && uid1 !== 0x1000007a) continue;   // not a DLL or EXE
    const img = readRomImage(rom, file.addr);
    if (img.codeAddress < rom.base || img.codeSize <= 0) continue;
    if (img.codeAddress - rom.base + img.codeSize > rom.data.length) continue;
    const used = new Set<number>();
    for (let a = img.codeAddress; a + 12 <= img.codeAddress + img.codeSize; a += 4) {
      if (rd(a) !== 0xe59fc000 || rd(a + 4) !== 0xe59cf000) continue;   // ldr r12,[pc]; ldr pc,[r12]
      const slot = rd(a + 8);
      if (slot < rom.base || slot - rom.base + 4 > rom.data.length) continue;
      const ord = byAddress.get(rd(slot));
      if (ord !== undefined && users.has(ord)) used.add(ord);
    }
    // Only binaries that do something with a file get a vote.
    if (!used.has(open) && !used.has(replace) && !used.has(write)) continue;
    for (const ord of used) if (closes.includes(ord)) users.set(ord, users.get(ord)! + 1);
  }
  const ranked = closes.map(ord => ({ ord, n: users.get(ord)! })).sort((a, b) => b.n - a.n);
  if (ranked.length > 1 && ranked[1].n !== 0) {
    throw new Error(`RFile::Close is ambiguous: ${ranked.map(r => `${r.ord} used by ${r.n}`).join(', ')}`);
  }
  if (ranked[0].n === 0) throw new Error('no ROM binary closes a file, so RFile::Close cannot be told apart');
  const close = ranked[0].ord;

  return { close, connect, open, create, replace, temp, read, write };
}

// ── Will this image load on this ROM? ───────────────────────────────

interface Check { ok: boolean; text: string; }

export function checkImage(rom: Rom1, exePath: string): Check[] {
  const f = fs.readFileSync(exePath);
  const u = (o: number): number => f.readUInt32LE(o);
  const out: Check[] = [];
  const say = (ok: boolean, text: string): void => { out.push({ ok, text }); };

  // 1. What EFile.exe's own E32Image validator checks, in its order.
  say(f.subarray(0x10, 0x14).toString('latin1') === 'EPOC', `signature 'EPOC' at +0x10`);
  const nonNeg: [string, number][] = [
    ['iCodeSize', 0x30], ['iDataSize', 0x34], ['iHeapSizeMin', 0x38], ['iStackSize', 0x40],
    ['iBssSize', 0x44], ['iDllRefTableCount', 0x54], ['iExportDirOffset', 0x58],
    ['iExportDirCount', 0x5c], ['iTextSize', 0x60], ['iCodeOffset', 0x64],
    ['iDataOffset', 0x68], ['iImportOffset', 0x6c], ['iCodeRelocOffset', 0x70],
    ['iDataRelocOffset', 0x74],
  ];
  const bad = nonNeg.filter(([, o]) => (u(o) | 0) < 0).map(([n]) => n);
  say(bad.length === 0, `every size and offset is non-negative${bad.length ? `: ${bad.join(', ')} are not` : ''}`);
  say(u(0x3c) >= u(0x38), 'iHeapSizeMax >= iHeapSizeMin');
  say(u(0x30) >= u(0x60), 'iCodeSize >= iTextSize');
  const within = [0x58, 0x64, 0x68, 0x6c, 0x70, 0x74].every(o => u(o) <= f.length);
  say(within && u(0x64) + u(0x30) + u(0x34) <= f.length,
      `every offset is inside the ${f.length}-byte file`);

  // 2. The header's own arithmetic.
  say(u(0x0c) === uidChecksum(u(0), u(4), u(8)),
      `iUidChecksum 0x${u(0x0c).toString(16)} for UIDs (${[0, 4, 8].map(o => '0x' + u(o).toString(16)).join(', ')})`);
  const code = f.subarray(u(0x64), u(0x64) + u(0x30));
  say(u(0x18) === wordSum(code), `iCheckSumCode 0x${u(0x18).toString(16)} is the code's word sum`);
  say(u(0x14) === 0x2000, 'iCpuIdentifier is ARM (0x2000)');
  say(u(0) === 0x1000007a, 'iUid1 is KExecutableImageUid');
  say(u(8) === 0, 'iUid3 is 0 — the Shell runs an executable with no application UID');
  say(u(0x48) === 0, 'iEntryPoint is the start of the code section');

  // 3. The imports, against this ROM's own DLLs.
  const iat: number[] = [];
  for (let o = u(0x64) + u(0x60); o < u(0x64) + u(0x30); o += 4) {
    const v = u(o);
    iat.push(v);
    if (v === 0) break;
  }
  const wanted: number[] = [];
  let p = u(0x6c) + 4;
  for (let i = 0; i < u(0x54); i++) {
    const nameOff = u(p), count = u(p + 4);
    let e = u(0x6c) + nameOff;
    while (f[e]) e++;
    const name = f.subarray(u(0x6c) + nameOff, e).toString('latin1');
    const ordinals: number[] = [];
    for (let j = 0; j < count; j++) ordinals.push(u(p + 8 + j * 4));
    wanted.push(...ordinals);
    p += 8 + count * 4;

    const m = name.match(/^([^[]+)\[([0-9a-f]{8})\]\.DLL$/i);
    if (!m) { say(false, `import name "${name}" is not NAME[uid3].DLL`); continue; }
    const leaf = `${m[1]}.dll`.toLowerCase(), uid3 = parseInt(m[2], 16);
    let dll: RomDll | undefined;
    try { dll = findDll(rom, leaf); } catch { /* reported below */ }
    say(!!dll, `${name}: ${dll ? `found in the ROM as ${dll.path}` : 'IS NOT IN THIS ROM'}`);
    if (!dll) continue;
    say(dll.image.uid3 === uid3,
        `${name}: the ROM's copy has UID3 0x${dll.image.uid3.toString(16)}`);
    const over = ordinals.filter(o2 => o2 < 1 || o2 > dll!.image.exportDirCount);
    say(over.length === 0,
        `${name}: all ${ordinals.length} ordinals are within its ${dll.image.exportDirCount} exports` +
        `${over.length ? ` — ${over.join(', ')} are not` : ''}`);
  }
  say(iat.length === wanted.length + 1 && iat[iat.length - 1] === 0 &&
      wanted.every((v, i) => iat[i] === v),
      'the import address table holds the ordinals in import order, zero-terminated');

  // 3b. And what those EFSRV ordinals actually are, identified from
  //     this ROM's own copy of the DLL rather than taken on trust.
  try {
    const fsOrd = fileServerOrdinals(rom);
    const name = new Map<number, string>(Object.entries(fsOrd).map(([k, v]) => [v as number, k]));
    for (const ord of wanted) {
      const what = name.get(ord);
      if (what) say(true, `EFSRV ordinal ${ord} is this ROM's ${what}`);
    }
    const missing = ['close', 'connect', 'open', 'read', 'replace', 'write']
      .filter(k => !wanted.includes((fsOrd as unknown as Record<string, number>)[k]));
    say(missing.length === 0,
        `the image imports every call a dump needs${missing.length ? ` — ${missing.join(', ')} missing` : ''}`);
  } catch (e) {
    say(false, `identifying EFSRV's exports in this ROM: ${(e as Error).message}`);
  }

  // 4. Relocations: type 3 ("inferred"), inside the code, the count the
  //    section's header claims.
  const rel = u(0x70);
  if (rel) {
    const size = u(rel), count = u(rel + 4);
    let q = rel + 8, n = 0, types = new Set<number>(), outside = 0;
    while (q < rel + 8 + size) {
      const page = u(q), blk = u(q + 4);
      if (blk < 8) break;
      for (let k = 8; k < blk; k += 2) {
        const ent = f.readUInt16LE(q + k);
        if (!ent) continue;
        types.add(ent >> 12);
        if (page + (ent & 0xfff) >= u(0x30)) outside++;
        n++;
      }
      q += blk;
    }
    say(n === count, `${n} relocation entries, as the section says`);
    say([...types].every(t => t === 3), `every relocation is type 3 (inferred): ${[...types].join(',')}`);
    say(outside === 0, 'every relocation is inside the code section');
  }

  return out;
}

// ── CLI ─────────────────────────────────────────────────────────────

function main(): void {
  const [cmd, romPath, exePath] = process.argv.slice(2);
  if (!cmd || !romPath) {
    console.error('usage: er1check.mts ordinals ROM | check ROM EXE');
    process.exit(2);
  }
  const rom = readRom1(romPath);

  if (cmd === 'ordinals') {
    const o = fileServerOrdinals(rom);
    const dll = findDll(rom, 'efsrv.dll');
    console.log(`# ${romPath}`);
    console.log(`# ${dll.path}: UID3 0x${dll.image.uid3.toString(16)}, ${dll.image.exportDirCount} exports`);
    for (const [name, ord] of Object.entries(o)) console.log(`${name.padEnd(8)} ${ord}`);
    return;
  }

  if (cmd === 'check') {
    if (!exePath) throw new Error('check needs ROM EXE');
    const results = checkImage(rom, exePath);
    for (const r of results) console.log(`${r.ok ? 'ok  ' : 'FAIL'} ${r.text}`);
    const failed = results.filter(r => !r.ok).length;
    console.log(`# ${results.length - failed} of ${results.length} checks pass`);
    process.exit(failed ? 1 : 0);
  }

  throw new Error(`unknown command ${cmd}`);
}

if (process.argv[1] && process.argv[1].endsWith('er1check.mts')) main();
