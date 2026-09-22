// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Checks what the EPOC Release 1 ROM dumper left in the machine's RAM
// after tests/integration/test-er1-romdump.sh ran it on an emulated
// Series 5.
//
// Two questions, and the second is the one that matters:
//
//   Did it finish?      C:\ROMDUMP.TXT is written to the RAM disk after
//                       every part, so the report is in the snapshot.
//                       It has to say the whole ROM is written, and to
//                       account for every part.
//   Is it the ROM?      The report only says what the program believes.
//                       So the run is checked from outside as well: a
//                       64-byte run of the real ROM image, taken every
//                       64 KB, has to be findable in the machine's RAM —
//                       which it can only be if the parts on the RAM
//                       disk really are the ROM.
//
//   node --experimental-strip-types tests/integration/test-er1-romdump.mts \
//        --ram RAM --rom ROM

import * as fs from 'node:fs';

const arg = (flag: string): string => {
  const i = process.argv.indexOf(flag);
  if (i < 0 || i + 1 >= process.argv.length) throw new Error(`missing ${flag}`);
  return process.argv[i + 1];
};

const noImports = process.argv.includes('--no-imports');
const ram = fs.readFileSync(arg('--ram'));
const rom = fs.readFileSync(arg('--rom'));
let failures = 0;
const check = (name: string, ok: boolean, detail = ''): void => {
  console.log(`${ok ? 'ok  ' : 'FAIL'} ${name}${ok || !detail ? '' : ` — ${detail}`}`);
  if (!ok) failures++;
};

// ── The report the machine wrote ────────────────────────────────────

const at = ram.indexOf(Buffer.from('ROM base    0x50000000', 'latin1'));
check('the machine wrote a report with its ROM base filled in', at >= 0);
let report = '';
if (at >= 0) {
  const start = ram.lastIndexOf(Buffer.from('Psion EPOC R1 ROM dump', 'latin1'), at);
  report = ram.subarray(start, start + 1400).toString('latin1');
  console.log(report.split('\r\n\r\nTo finish')[0].split('\0')[0]);
}

const romSize = rom.readUInt32LE(0x90);
const romKb = romSize >> 10;
check(`the report names the ROM's own size (${romKb} KB)`,
      report.includes(`(${romKb} KB)`));
check('it dumped the whole ROM',
      report.includes(`Done so far ${romKb} KB of ${romKb} KB`));
check('and says there is nothing left for a next run',
      report.includes('Next run    nothing - the whole ROM is written'));

// Each part it wrote has a row naming the bytes of the ROM it holds.
// Only the rows in the report's first cluster are contiguous in RAM —
// the RAM disk scatters the rest — so this checks the rows that are
// there, and leaves "did it finish" to the lines above, which are at
// the top of the report for that reason.
const parts = Math.ceil(romSize / (1024 * 1024));
const rows = [...report.matchAll(/ {2}ROMDUMP\.(\d{3}) {2}ROM 0x([0-9A-F]{6})\.\.0x([0-9A-F]{6}) {2}(\d+) KB {2}checked/g)];
check('the report rows name each part and the ROM bytes it holds',
      rows.length >= 3 && rows[0][1] === '001' && rows[0][2] === '000000',
      `${rows.length} rows readable of ${parts}`);
check('the rows run consecutively through the ROM',
      rows.every((m, k) => parseInt(m[2], 16) === k * 0x100000 &&
                           parseInt(m[3], 16) === k * 0x100000 + 0xfffff));

// ── The same thing, from outside the machine ────────────────────────

// Only the first 3 MB: the snapshot is one bank, and a 6 MB dump on a
// machine this size does not keep every part in it at once.
let found = 0, sampled = 0;
const missing: string[] = [];
for (let off = 0; off + 64 <= Math.min(rom.length, 0x300000); off += 0x10000) {
  sampled++;
  if (ram.indexOf(rom.subarray(off, off + 64)) >= 0) found++;
  else missing.push(`0x${off.toString(16)}`);
}
check(`every 64-byte run of the ROM sampled every 64 KB is in the machine's RAM (${found}/${sampled})`,
      found === sampled, missing.slice(0, 6).join(' '));

// ── The log it kept while it worked ─────────────────────────────────

// ROMDUMP.LOG is rewritten after every line, so on a machine that fell
// over it is the record of how far it got. Here it is the record of the
// startup self-check: the program reads this machine's own EFSrv.dll
// out of the ROM and confirms, one by one, that the calls it makes are
// the ones it means to make.
//
// The RAM disk scatters a file across clusters, so the log is looked
// for line by line across the whole snapshot rather than as one run of
// text — and every line looked for carries a value, which the constants
// in the program's own code do not.
const whole = ram.toString('latin1');

const logAt = whole.indexOf('rom hdr base=0x50000000 size=');
check('the machine kept a log', logAt >= 0);
if (logAt >= 0) {
  const from = Math.max(0, whole.lastIndexOf('ROMDUMP for EPOC R1', logAt));
  console.log(whole.slice(from, from + 700).split('\r\n\r\n')[0]);
}
// The two builds reach the file server different ways, and the log says
// which: the ordinary one through the library the loader bound, the
// no-import one through the client written out over the kernel's own
// executive calls. Either way the line carries rc=0.
check('the log records the connect that everything else depends on',
      whole.includes(noImports
                     ? 'connect with the kernel calls in this program, rc=0'
                     : 'connect to the file server rc=0'));
if (noImports) {
  check('and says the calls it went on to use are its own',
        whole.includes("the calls in use are this program's own, over the kernel"));
}
check("the log names this ROM's own EFSrv.dll",
      /EFSrv\.dll at 0x[0-9A-F]{8} uid1=0x10000079 uid3=0x100000BD exports=205/.test(whole));

const confirmed = whole.match(/ (iClose|iConnect|iOpen|iReplace|iRead|iWrite) linked=\d+ found=\d+ matches=\d+ confirmed at 0x[0-9A-F]{8}/g) ?? [];
// (The log turns up more than once in a RAM snapshot — the disk's copy
// and the file server's — so it is the set of calls that matters.)
const named = new Set(confirmed.map(l => l.trim().split(' ')[0]));
for (const call of named) {
  console.log(` ${confirmed.find(l => l.trim().startsWith(call))!.trim()}`);
}
check('the self-check confirms all six calls against the machine itself',
      named.size === 6, `${[...named].join(' ')}`);
check('and nothing it was linked against turned out to be wrong',
      !/NOT THE LINKED ORDINAL \*\* using 0x[0-9A-F]{8}/.test(whole));

// For the no-import build, what matters is the image itself: an empty
// import table is what makes it impossible for the loader to refuse.
if (noImports) {
  const exe = fs.readFileSync(arg('--exe'));
  check('this build has no import table for the loader to bind',
        exe.readUInt32LE(0x54) === 0, `iDllRefTableCount=${exe.readUInt32LE(0x54)}`);
}

// ── And that it did not fall over ───────────────────────────────────

// The first build of this program panicked with KERN-EXEC 3 the moment
// it tried to divide (an ARM710a has no long multiply, and executes the
// encoding as something else). A panic stops the run where it happens,
// so a report that says "complete" is the proof that none happened —
// and a regression of that kind shows up here as this line missing.
check('the run reached the end (no KERN-EXEC 3 part way through)',
      report.includes('complete - the dump is the whole ROM'));

console.log(failures ? `${failures} FAILURES` : 'all checks pass');
process.exit(failures ? 1 : 0);
