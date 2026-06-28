// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Device-aware keymap tests. Run via:
//   node --experimental-strip-types frontend/src/lib/__tests__/keymap.test.mts
// or `npm run test:keymap` from frontend/.
//
// Verifies that each device family produces the right EPOC chord for symbols.
// Expected chords are transcribed from the authoritative key legends: MAME's
// INPUT_PORTS (psion2/psion3/psion3a/siena/workabout/mc400) and NetBSD's
// epoc32 epockbdmap.h (Series 7 / netBook UK map).

import {
  charToEpocChord, keyboardLayoutForDevice, type KeyboardLayout,
} from '../keymap.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}

const SHIFT = 18, PSION = 20, FN = 24;

function chord(layout: KeyboardLayout, char: string, modifiers: number[], key: number, note = '') {
  const c = charToEpocChord(char, layout);
  const want = JSON.stringify({ modifiers, key });
  const got = JSON.stringify(c && { modifiers: c.modifiers, key: c.key });
  check(got === want, `${layout} '${char}'${note ? ` (${note})` : ''}: got ${got}, want ${want}`);
}
function isNull(layout: KeyboardLayout, char: string, note = '') {
  const c = charToEpocChord(char, layout);
  check(c === null, `${layout} '${char}'${note ? ` (${note})` : ''}: expected null, got ${JSON.stringify(c)}`);
}

// ── Device id → layout resolution ─────────────────────────────────────────
function eqLayout(id: string, want: KeyboardLayout) {
  check(keyboardLayoutForDevice(id) === want, `layout(${id}) == ${want}, got ${keyboardLayoutForDevice(id)}`);
}
eqLayout('5mx', 'epoc32');     eqLayout('revo', 'epoc32');    eqLayout('osaris', 'epoc32');
eqLayout('series7', 'sa1100'); eqLayout('netbook', 'sa1100');
eqLayout('mc400', 'mc400');    eqLayout('mc400v126', 'mc400');
eqLayout('series3', 'sibo-classic');
eqLayout('series3a', 'sibo-3a'); eqLayout('series3c', 'sibo-3a'); eqLayout('series3mx', 'sibo-3a');
eqLayout('pocketbk', 'sibo-3a'); eqLayout('pocketbk2', 'sibo-3a');
eqLayout('siena', 'sibo-siena');
eqLayout('workabout', 'sibo-workabout'); eqLayout('workaboutmx', 'sibo-workabout');
eqLayout('organiser2', 'organiser2');
check(keyboardLayoutForDevice(null) === 'epoc32', 'layout(null) defaults epoc32');

// ── Letters, digits, whitespace (every non-Organiser layout) ──────────────
for (const layout of ['epoc32', 'sa1100', 'mc400', 'sibo-classic', 'sibo-3a', 'sibo-siena', 'sibo-workabout'] as KeyboardLayout[]) {
  chord(layout, 'a', [], 65, 'lowercase');
  chord(layout, 'A', [SHIFT], 65, 'uppercase');
  chord(layout, ' ', [], 5, 'space');
  chord(layout, '\n', [], 3, 'newline');
  chord(layout, '\t', [], 2, 'tab');
  chord(layout, '7', [], 55, 'digit 7 unshifted');
}

// ── EPOC32 (Series 5 family) ──────────────────────────────────────────────
// Full reconciliation against MAME's psion5mx INPUT_PORTS (UK keyboard,
// PORT_CHAR order = unshifted / Shift / Fn). Each matrix key bears up to three
// legends; we send (modifier + that key's EStdKey).
// Dedicated punctuation keys, unshifted.
chord('epoc32', ',', [], 121, 'comma key');
chord('epoc32', '.', [], 122, 'full-stop key');
chord('epoc32', "'", [], 126, 'quote key');
// Shift + digit row (1..0 → ! " £ $ % ^ & * ( )).
chord('epoc32', '!', [SHIFT], 49); chord('epoc32', '"', [SHIFT], 50);
chord('epoc32', '£', [SHIFT], 51); chord('epoc32', '$', [SHIFT], 52);
chord('epoc32', '%', [SHIFT], 53); chord('epoc32', '^', [SHIFT], 54);
chord('epoc32', '&', [SHIFT], 55); chord('epoc32', '*', [SHIFT], 56, 'Shift+8');
chord('epoc32', '(', [SHIFT], 57); chord('epoc32', ')', [SHIFT], 48);
// Shift + dedicated punctuation keys.
chord('epoc32', '@', [SHIFT], 126, 'Shift+quote (UK)');
chord('epoc32', '?', [SHIFT], 122, 'Shift+full-stop');
chord('epoc32', '/', [SHIFT], 121, 'Shift+comma');
// Fn + digit row (1..0 → _ # \\ ~ < > [ ] { }).
chord('epoc32', '_', [FN], 49, 'Fn+1'); chord('epoc32', '#', [FN], 50, 'Fn+2');
chord('epoc32', '\\', [FN], 51, 'Fn+3'); chord('epoc32', '~', [FN], 52, 'Fn+4 (revo)');
chord('epoc32', '<', [FN], 53, 'Fn+5'); chord('epoc32', '>', [FN], 54, 'Fn+6');
chord('epoc32', '[', [FN], 55, 'Fn+7'); chord('epoc32', ']', [FN], 56, 'Fn+8');
chord('epoc32', '{', [FN], 57, 'Fn+9'); chord('epoc32', '}', [FN], 48, 'Fn+0');
// Fn + letter / quote keys.
chord('epoc32', ':', [FN], 126, 'Fn+quote key');
chord('epoc32', ';', [FN], 76, 'Fn+L');
chord('epoc32', '=', [FN], 80, 'Fn+P');
chord('epoc32', '-', [FN], 79, 'Fn+O');
chord('epoc32', '+', [FN], 73, 'Fn+I');

// ── Series 7 / netBook (sa1100, UK): distinct Fn(AltGr) layer ─────────────
chord('sa1100', "'", [], 125, 'apostrophe = bare ;-position key');
chord('sa1100', '~', [SHIFT], 125, 'Shift on ;-position key');
chord('sa1100', ':', [FN], 125, 'Fn on ;-position key');
chord('sa1100', '=', [FN], 80, 'Fn+P');
chord('sa1100', '-', [FN], 79, 'Fn+O');
chord('sa1100', ';', [FN], 76, 'Fn+L');
chord('sa1100', '@', [FN], 52, 'Fn+4 (UK)');
chord('sa1100', '#', [FN], 50, 'Fn+2 (UK)');
chord('sa1100', '[', [FN], 55, 'Fn+7');
chord('sa1100', '?', [SHIFT], 122, 'Shift+.');
chord('sa1100', '/', [SHIFT], 121, 'Shift+,');
chord('sa1100', '£', [SHIFT], 51, 'Shift+3');

// ── Series 3 classic: 3 layers; '+' unshifted, '=' Shift; no ';' ──────────
chord('sibo-classic', '+', [], 43, 'unshifted plus key');
chord('sibo-classic', '=', [SHIFT], 43, 'Shift+plus key');
chord('sibo-classic', '?', [SHIFT], 56, 'Shift+8');
chord('sibo-classic', '#', [PSION], 50, 'Psion+2');
chord('sibo-classic', "'", [PSION], 53, 'Psion+5');
chord('sibo-classic', '[', [PSION], 57, 'Psion+9');
chord('sibo-classic', '<', [SHIFT], 44, 'Shift+comma');
isNull('sibo-classic', ';', 'Series 3 classic has no semicolon key');

// ── Series 3a: as classic, plus ';' = Shift + slash key ───────────────────
chord('sibo-3a', ';', [SHIFT], 47, 'Shift+slash');
chord('sibo-3a', '+', [], 43, 'unshifted plus key');
chord('sibo-3a', '=', [SHIFT], 43, 'Shift+plus key');
chord('sibo-3a', '@', [PSION], 54, 'Psion+6');
chord('sibo-3a', '}', [PSION], 56, 'Psion+8');

// ── Siena: dedicated ; / - = + *, Shift adds : ? _ < > ────────────────────
chord('sibo-siena', ';', [], 59, 'dedicated semicolon');
chord('sibo-siena', ':', [SHIFT], 59);
chord('sibo-siena', '+', [], 43, 'dedicated plus');
chord('sibo-siena', '?', [SHIFT], 47, 'Shift+slash');

// ── Workabout: ',' is Shift+'.', '+' unshifted/'=' Shift, full Psion layer ─
chord('sibo-workabout', '.', [], 46, 'unshifted stop');
chord('sibo-workabout', ',', [SHIFT], 46, 'Shift+stop');
chord('sibo-workabout', '+', [], 43, 'unshifted plus');
chord('sibo-workabout', '=', [SHIFT], 43, 'Shift+plus');
chord('sibo-workabout', '#', [PSION], 50, 'Psion+2');
chord('sibo-workabout', "'", [PSION], 55, 'Psion+7');
chord('sibo-workabout', ';', [SHIFT], 48, 'Shift+0');

// ── MC400: full UK Shift legends ──────────────────────────────────────────
chord('mc400', '-', [], 45);    chord('mc400', '_', [SHIFT], 45);
chord('mc400', '[', [], 91);    chord('mc400', '{', [SHIFT], 91);
chord('mc400', '#', [], 35);    chord('mc400', '~', [SHIFT], 35);
chord('mc400', '\\', [], 92);   chord('mc400', '|', [SHIFT], 92);
chord('mc400', '@', [SHIFT], 126, 'Shift+quote');
chord('mc400', '*', [SHIFT], 56, 'Shift+8');

// ── Organiser II: letters NOT Shift-wrapped; digits/symbols are Shift+letter
chord('organiser2', 'a', [], 65, 'lowercase letter, no shift');
chord('organiser2', 'A', [], 65, 'uppercase still no shift (CAP mode handles case)');
chord('organiser2', '0', [SHIFT], 89, 'Shift+Y');
chord('organiser2', '1', [SHIFT], 85, 'Shift+U');
chord('organiser2', ';', [SHIFT], 83, 'Shift+S');
chord('organiser2', '/', [SHIFT], 70, 'Shift+F');
chord('organiser2', '+', [SHIFT], 88, 'Shift+X');
chord('organiser2', ' ', [], 5, 'space');
isNull('organiser2', '#', 'no hash on the Organiser keypad');
isNull('organiser2', '[', 'no bracket on the Organiser keypad');

if (failures === 0) {
  console.log('PASS keymap (device-aware symbol coverage)');
  process.exit(0);
} else {
  console.error(`FAIL: ${failures} assertion(s)`);
  process.exit(1);
}
