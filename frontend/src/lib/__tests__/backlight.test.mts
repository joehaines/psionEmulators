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
  BACKLIGHT_FADE_MS, BacklightFade, blendBacklitPixels,
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

// The fade. Partway between unlit and lit the LCD is a blend of the two,
// and it gets there over BACKLIGHT_FADE_MS on an eased curve.

test('the fade blends from the unlit frame to the lit one', () => {
  const lut = buildBacklightLut(parseHexColour('#bcd47a'));
  const at = (mix: number) => {
    const buf = new Uint8ClampedArray([...SIBO_PAPER, 0xFF]);
    blendBacklitPixels(buf, lut, mix);
    return [...buf];
  };
  assert.deepEqual(at(0), [...SIBO_PAPER, 0xFF]);
  assert.deepEqual(at(1), lit([...SIBO_PAPER], '#bcd47a'));
  const half = at(0.5);
  for (let c = 0; c < 3; c++) {
    const lo = Math.min(SIBO_PAPER[c], at(1)[c]), hi = Math.max(SIBO_PAPER[c], at(1)[c]);
    assert.ok(half[c] >= lo && half[c] <= hi, `channel ${c} ${half[c]} not between ${lo} and ${hi}`);
  }
});

test('the fade takes BACKLIGHT_FADE_MS, eases, and turns back mid-way', () => {
  const f = new BacklightFade();
  assert.equal(f.step(true, 1000), 0);                       // first frame: no time has passed
  const quarter = f.step(true, 1000 + BACKLIGHT_FADE_MS / 4);
  assert.ok(quarter > 0 && quarter < 0.25, `eased start, got ${quarter}`);
  assert.equal(f.step(true, 1000 + BACKLIGHT_FADE_MS), 1);
  assert.equal(f.step(true, 5000), 1);                        // stays lit
  const back = f.step(false, 5000 + BACKLIGHT_FADE_MS / 2);   // going out
  assert.ok(back > 0 && back < 1, `half-way out, got ${back}`);
  const again = f.step(true, 5000 + BACKLIGHT_FADE_MS * 3 / 4);  // and back on, from where it was
  assert.ok(again > back && again < 1, `turned back, got ${again}`);
  f.reset(false);
  assert.equal(f.step(true, 9000), 0);
});

test('the worker blends and fades the same way', () => {
  const src = readFileSync(fileURLToPath(new URL('../../../public/emulator-worker.js', import.meta.url)), 'utf8');
  const start = src.indexOf('const LIT_INK');
  const end = src.indexOf('// Partway through the fade');
  assert.ok(start > 0 && end > start, 'worker backlight block not found');
  const ctx = vm.createContext({ Math });
  vm.runInContext(src.slice(start, end) + `
    this.blend = blendBacklitPixels;
    this.build = buildBacklightLut;
    this.fade = (lit, now) => { backlightLit = lit; return stepBacklightFade(now); };`, ctx);
  const w = ctx as unknown as {
    blend: (b: Uint8ClampedArray, l: Uint8Array, m: number, g: Uint8Array | null, a: Uint8Array | null) => void;
    build: (h: string) => Uint8Array;
    fade: (lit: boolean, now: number) => number;
  };
  const grey = new Uint8Array(256).map((_, i) => Math.round(0x33 * i / 255));
  const alpha = new Uint8Array(256).map((_, i) => Math.round(0xFF - (0xFF - 0x05) * i / 255));
  const px = [...SIBO_PAPER, 0xFF, ...SIBO_GREY, 0xFF, ...SIBO_INK, 0xFF, ...EPOC_PAPER, 0xFF];
  for (const mix of [0.2, 0.5, 0.9]) {
    for (const dev of [false, true]) {
      const a = new Uint8ClampedArray(px), b = new Uint8ClampedArray(px);
      blendBacklitPixels(a, buildBacklightLut(parseHexColour('#8cc4f2')), mix, dev ? grey : null, dev ? alpha : null);
      w.blend(b, w.build('#8cc4f2'), mix, dev ? grey : null, dev ? alpha : null);
      assert.deepEqual([...b], [...a], `mix ${mix}${dev ? ' device mode' : ''}`);
    }
  }
  const f = new BacklightFade();
  for (const [lit, t] of [[true, 100], [true, 180], [true, 333], [false, 400], [false, 650], [true, 700], [true, 2000]] as const)
    assert.equal(w.fade(lit, t), f.step(lit, t), `fade at ${t} ms`);
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
