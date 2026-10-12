// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// LCD backlight presentation tests. Run via:
//   node --experimental-strip-types frontend/src/lib/__tests__/backlight.test.mts
// or `npm run test:backlight` from frontend/.
//
// Whether a machine HAS a lit backlight is the core's business and is
// tested against the real ROMs by tests/integration/test-backlight.sh.
// This covers the frontend half:
//
//   1. A lit EL panel keeps its text: paper turns the EL colour, ink stays
//      dark, on the panel colours the cores actually render.
//   2. The worker (public/emulator-worker.js, plain JS) carries its own
//      copy of the lit-panel lookup; it must build the same table, or the
//      panel would look different depending on which thread runs the
//      machine.
//   3. The host shortcut is Ctrl+Shift+L and nothing else — in particular
//      not plain L, and not the Meta / Alt chords that ARE the machines'
//      own keys (Fn+Space, Psion+Space) and must reach them untouched.

import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import vm from 'node:vm';
import {
  buildBacklightLut, describeBacklightKey, elColourFor, isBacklightShortcut,
  parseHexColour, tintBacklitPixels,
} from '../backlight.ts';

let failures = 0;
function test(name: string, fn: () => void) {
  try { fn(); console.log(`  ok  ${name}`); }
  catch (e) { failures++; console.log(`FAIL  ${name}\n      ${(e as Error).message}`); }
}

const luminance = (r: number, g: number, b: number) => (r * 77 + g * 150 + b * 29) >> 8;

// The unlit panel colours the cores emit, as canvas RGBA bytes.
const SIBO_PAPER = [0xA0, 0xB4, 0xAA];   // core/series3c.cpp: 0xFFAAB4A0
const SIBO_GREY  = [0x68, 0x70, 0x60];   //                   0xFF607068
const SIBO_INK   = [0x28, 0x28, 0x20];   //                   0xFF202828
const EPOC_PAPER = [0x99, 0xAA, 0x88];   // core/clps7111.cpp 4 bpp paper
const EPOC_INK   = [0x00, 0x00, 0x00];

function lit(px: number[], hex: string): number[] {
  const buf = new Uint8ClampedArray([...px, 0xFF]);
  tintBacklitPixels(buf, buildBacklightLut(parseHexColour(hex)));
  return [...buf];
}

console.log('backlight');

test('paper takes on the EL colour', () => {
  for (const [paper, hex] of [[SIBO_PAPER, '#bcd47a'], [EPOC_PAPER, '#a8d8c4'], [EPOC_PAPER, '#8cc4f2']] as const) {
    const want = parseHexColour(hex);
    const got = lit([...paper], hex);
    for (let c = 0; c < 3; c++)
      assert.ok(Math.abs(got[c] - want[c]) <= 6, `paper ${paper} channel ${c}: ${got[c]} vs ${want[c]}`);
  }
});

test('ink stays dark and grey stays between', () => {
  for (const ink of [SIBO_INK, EPOC_INK]) {
    const got = lit([...ink], '#bcd47a');
    assert.ok(luminance(got[0], got[1], got[2]) < 48, `ink ${ink} came out ${got}`);
  }
  const grey = lit([...SIBO_GREY], '#bcd47a');
  const g = luminance(grey[0], grey[1], grey[2]);
  const p = lit([...SIBO_PAPER], '#bcd47a');
  assert.ok(g > 60 && g < luminance(p[0], p[1], p[2]), `grey came out ${grey}`);
});

test('alpha is left alone', () => {
  const buf = new Uint8ClampedArray([0xA0, 0xB4, 0xAA, 0x40]);
  tintBacklitPixels(buf, buildBacklightLut(parseHexColour('#a8d8c4')));
  assert.equal(buf[3], 0x40);
});

// The worker carries its own copy of the frame post-processing; lit, it
// must tint exactly as the library does, and it switches at once — no
// fade, as on a real EL panel.

test('the worker lights the panel the same way, with no fade', () => {
  const src = readFileSync(fileURLToPath(new URL('../../../public/emulator-worker.js', import.meta.url)), 'utf8');
  const start = src.indexOf('const LIT_INK');
  const end = src.indexOf('// ── IndexedDB');
  assert.ok(start > 0 && end > start, 'worker backlight block not found');
  const ctx = vm.createContext({});
  vm.runInContext(src.slice(start, end) + `
    var pixelBuf, deviceMode = false;
    function applyDeviceModePixels(buf) { for (let i = 3; i < buf.length; i += 4) buf[i] = 0x11; }
    this.run = (buf, colour, lit, dev) => {
      pixelBuf = buf; deviceMode = dev;
      backlightLut = colour ? buildBacklightLut(colour) : null; backlightLit = lit;
      postProcessLcd();
    };`, ctx);
  const w = ctx as unknown as { run: (b: Uint8ClampedArray, c: string | null, lit: boolean, dev: boolean) => void };
  const px = [...SIBO_PAPER, 0xFF, ...SIBO_GREY, 0xFF, ...SIBO_INK, 0xFF, ...EPOC_PAPER, 0xFF];
  for (const dev of [false, true]) {
    // Lit: fully tinted on the very first frame, device mode or not.
    const a = new Uint8ClampedArray(px), b = new Uint8ClampedArray(px);
    tintBacklitPixels(a, buildBacklightLut(parseHexColour('#8cc4f2')));
    w.run(b, '#8cc4f2', true, dev);
    assert.deepEqual([...b], [...a], `lit${dev ? ' device mode' : ''}`);
    // Off: straight back to the unlit rendering on the very next frame.
    const c = new Uint8ClampedArray(px);
    w.run(c, '#8cc4f2', false, dev);
    if (dev) assert.ok([...c].filter((_, i) => i % 4 === 3).every(v => v === 0x11), 'device mode, off');
    else assert.deepEqual([...c], px, 'off');
  }
});

test('the worker builds the same lit-panel lookup', () => {
  const src = readFileSync(fileURLToPath(new URL('../../../public/emulator-worker.js', import.meta.url)), 'utf8');
  const start = src.indexOf('const LIT_INK');
  const end = src.indexOf('function tintBacklitPixels');
  assert.ok(start > 0 && end > start, 'worker lookup builder not found');
  const ctx = vm.createContext({});
  vm.runInContext(src.slice(start, end) + '\nthis.build = buildBacklightLut;', ctx);
  for (const hex of ['#a8d8c4', '#bcd47a']) {
    const worker = (ctx as { build: (h: string) => Uint8Array }).build(hex);
    assert.deepEqual([...worker], [...buildBacklightLut(parseHexColour(hex))], hex);
  }
});

test('every machine gets a colour', () => {
  assert.equal(elColourFor('series3mx'), '#bcd47a');
  assert.equal(elColourFor('5mx'), '#a8d8c4');
  assert.equal(elColourFor('conan'), '#8cc4f2');      // blue
  assert.equal(elColourFor('conanv001'), '#8cc4f2');
  assert.match(elColourFor('some-future-machine'), /^#[0-9a-f]{6}$/);
  assert.match(elColourFor(null), /^#[0-9a-f]{6}$/);
});

const key = (code: string, mods: Partial<Record<'ctrlKey' | 'shiftKey' | 'altKey' | 'metaKey', boolean>> = {}) =>
  ({ code, ctrlKey: false, shiftKey: false, altKey: false, metaKey: false, ...mods });

test('the host shortcut is Ctrl+Shift+L only', () => {
  assert.ok(isBacklightShortcut(key('KeyL', { ctrlKey: true, shiftKey: true })));
  assert.ok(!isBacklightShortcut(key('KeyL')));
  assert.ok(!isBacklightShortcut(key('KeyL', { ctrlKey: true })));
  assert.ok(!isBacklightShortcut(key('KeyL', { shiftKey: true })));
  assert.ok(!isBacklightShortcut(key('KeyL', { ctrlKey: true, shiftKey: true, altKey: true })));
  // The machines' own chords pass through to the machine.
  assert.ok(!isBacklightShortcut(key('Space', { metaKey: true })));   // Fn+Space
  assert.ok(!isBacklightShortcut(key('Space', { altKey: true })));    // Psion+Space
});

test('the machine key reads as the keyboard legend', () => {
  assert.equal(describeBacklightKey(24, 5), 'Fn+Space');
  assert.equal(describeBacklightKey(20, 5), 'Psion+Space');
  assert.equal(describeBacklightKey(0, 151), 'Backlight key');
});

if (failures) { console.log(`\n${failures} failure(s)`); process.exit(1); }
console.log('\nall passed');
