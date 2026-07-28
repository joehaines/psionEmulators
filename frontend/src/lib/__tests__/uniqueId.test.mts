// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Formatting / parsing for the Unique id panel.
//
// EPOC prints the id as four hex groups (1000-118A-CAFE-BABE), assembled from
// two halves the panel sets separately: the identity chip's low 32 bits and
// the model UID the ROM supplies. The field carries all 16 digits where the
// machine's ROM half is known, and just the 8 it owns where it isn't. These
// are the rules that keep that honest.
//
// Run via:
//   node --experimental-strip-types frontend/src/lib/__tests__/uniqueId.test.mts

import {
  hex8, hex16, group4, composeUniqueId, formatUniqueId, sanitiseUniqueId, parseUniqueId,
} from '../uniqueId.ts';

let failures = 0;
function eq(actual: unknown, expected: unknown, msg: string) {
  if (actual !== expected) {
    console.error(`FAIL: ${msg}\n  expected ${String(expected)}\n  actual   ${String(actual)}`);
    failures++;
  }
}

// ── Display ──
eq(hex8(0x12345678), '12345678', 'hex8 pads and upper-cases');
eq(hex8(0xcafe), '0000CAFE', 'hex8 zero-pads short values');
eq(group4('1000118ACAFEBABE'), '1000-118A-CAFE-BABE', 'group4 splits into four groups');
eq(group4('CAFE'), 'CAFE', 'group4 leaves a single group alone');

// The whole value the OS shows, ROM half first.
eq(formatUniqueId(0x1000118a, 0xcafebabe), '1000-118A-CAFE-BABE',
   '5mx: full 16-digit id');
eq(formatUniqueId(0x09080001, 0x4afeba01), '0908-0001-4AFE-BA01',
   'Series 7: full 16-digit id');
// Machines whose ROM half we haven't read show only the half they own.
eq(formatUniqueId(null, 0xcafebabe), 'CAFE-BABE', 'unknown prefix: settable half only');

// ── Input normalisation ──
eq(sanitiseUniqueId('1000118acafebabe', 16), '1000-118A-CAFE-BABE', 'regroups as typed');
eq(sanitiseUniqueId('1000', 16), '1000', 'no trailing separator mid-type');
eq(sanitiseUniqueId('10001', 16), '1000-1', 'separator appears at the fifth digit');
eq(sanitiseUniqueId('1000-118A-CAFE-BABE9', 16), '1000-118A-CAFE-BABE',
   'a 17th digit is dropped rather than invalidating the field');
eq(sanitiseUniqueId('0x1000118A', 16), '1000-118A', 'an 0x prefix is dropped');
eq(sanitiseUniqueId('  1000 118a ', 16), '1000-118A', 'spaces are dropped');
eq(sanitiseUniqueId('zz12zz34', 16), '1234', 'non-hex characters are dropped');
eq(sanitiseUniqueId('', 16), '', 'empty stays empty');
eq(sanitiseUniqueId('cafebabe99', 8), 'CAFE-BABE',
   'prefix-less machines cap at the 8 digits they own');
eq(sanitiseUniqueId('DEADBEEFCAFEBABE', 16), 'DEAD-BEEF-CAFE-BABE',
   'a full 16-digit id passes through');

// ── Parsing: the whole 64-bit id ──
eq(hex16(0xdeadbeefcafebaben), 'DEADBEEFCAFEBABE', 'hex16 pads to 16 digits');
eq(composeUniqueId(0x1000118a, 0xcafebabe), 0x1000118acafebaben, 'halves compose');
eq(composeUniqueId(null, 0xcafebabe), 0xcafebaben, 'unknown prefix composes as zero');

eq(parseUniqueId('DEAD-BEEF-CAFE-BABE', 0x1000118a), 0xdeadbeefcafebaben,
   'a full id is taken as typed, replacing the prefix');
eq(parseUniqueId('deadbeefcafebabe', null), 0xdeadbeefcafebaben, 'case-insensitive');
// Short input is the chip half; the known prefix fills the rest so callers
// always send a complete id.
eq(parseUniqueId('CAFE-BABE', 0x1000118a), 0x1000118acafebaben,
   'short input keeps the machine prefix');
eq(parseUniqueId('5678', 0x1000118a), 0x1000118a00005678n,
   'short input is right-aligned in the low half, not shifted');
eq(parseUniqueId('CAFEBABE', null), 0xcafebaben, 'no prefix known: low half only');
eq(parseUniqueId('FFFFFFFFFFFFFFFF', null), 0xffffffffffffffffn, 'all ones survives');
eq(parseUniqueId('', 0x1000118a), null, 'empty parses as null so Set stays disabled');

// A value typed in, normalised, parsed and re-displayed must come back
// unchanged — the round trip the panel performs on every Set.
for (const [prefix, id] of [[0x1000118a, 0xcafebabe], [0x09080001, 0x00000001],
                            [null, 0xffffffff], [0x1000118a, 0]] as const) {
  const shown = formatUniqueId(prefix, id);
  const width = prefix != null ? 16 : 8;
  eq(sanitiseUniqueId(shown, width), shown, `round trip: sanitise(${shown}) is stable`);
  eq(parseUniqueId(shown, prefix), composeUniqueId(prefix, id),
     `round trip: parse(${shown}) recovers the whole id`);
}

if (failures > 0) {
  console.error(`\n${failures} test(s) failed`);
  process.exit(1);
}
console.log('uniqueId tests passed');
