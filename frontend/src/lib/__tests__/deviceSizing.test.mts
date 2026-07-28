// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Device-frame sizing tests. Run via:
//   node --experimental-strip-types frontend/src/lib/__tests__/deviceSizing.test.mts
// or `npm run test:sizing` from frontend/.
//
// Two properties, and they pull against each other — which is how this got
// broken twice.
//
//   1. THE DEVICE NEVER LEAVES THE WINDOW SIDEWAYS. Not at any zoom level,
//      in either orientation, on any viewport. A machine hanging off the
//      side is unusable on a phone, and the default scale is 2×, so it is
//      what a phone gets first.
//   2. THE ZOOM LEVELS DO SOMETHING, AND THE DEVICE USES THE WINDOW. Every
//      level up to the point where the machine is as wide as the window
//      must come back a different size; past that there is nothing left to
//      give and the header greys it out (largestUsefulScale) rather than
//      leaving a button that looks inert.
//
// The netpad is the machine that finds every edge here: it is the widest
// panel in the lineup (2.8:1 with its silkscreen strip) and the only one
// that can be turned, and stood on its end its long edge won't fit any
// landscape window.

import {
  calcContainerSize, largestFittingScale, largestUsefulScale, DEVICE_SCALES,
  type ScreenLayout,
} from '../deviceSizing.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}
function near(a: number, b: number, msg: string, eps = 1e-6) {
  check(Math.abs(a - b) <= eps, `${msg}: got ${a}, want ${b}`);
}

// Minimal window/document so the module can read the viewport. Node has no
// DOM; these three fields are all the sizing maths touches.
const g = globalThis as unknown as {
  window: { innerWidth: number; innerHeight: number; devicePixelRatio: number };
  document: { fullscreenElement: unknown };
};
g.window = { innerWidth: 1512, innerHeight: 860, devicePixelRatio: 1 };
g.document = { fullscreenElement: null };
function viewport(w: number, h: number, dpr = 1) {
  g.window.innerWidth = w; g.window.innerHeight = h; g.window.devicePixelRatio = dpr;
}

// The devices as the app draws them. Numbers copied from EmulatorView's
// layout tables so a tweak there can't quietly weaken the test.
const NETPAD_SILKSCREEN_WIDTH = 240 * (41 / 350);
const NETPAD_PANEL_WIDTH = 640 + NETPAD_SILKSCREEN_WIDTH;
const LAYOUTS: { name: string; layout: ScreenLayout; lcd: [number, number]; turns: boolean }[] = [
  { name: 'netpad bare',  turns: true,  lcd: [640, 240],
    layout: { aspectRatio: NETPAD_PANEL_WIDTH / 240,
              screenWidth: 640 / NETPAD_PANEL_WIDTH, screenHeight: 1 } },
  { name: 'netpad photo', turns: true,  lcd: [640, 240],
    layout: { aspectRatio: 1200 / 520,
              screenWidth: 844 / 1200, screenHeight: 314 / 520 } },
  { name: '5mx photo',    turns: false, lcd: [640, 240],
    layout: { aspectRatio: 815 / 678, screenWidth: 0.777, screenHeight: 0.351 } },
  { name: 'Series 7',     turns: false, lcd: [640, 480],
    layout: { aspectRatio: 846 / 814, screenWidth: 0.582, screenHeight: 0.450 } },
];

// Phone, phone at DPR 1, tablet, small laptop, laptop, desktop.
const VIEWPORTS: [number, number, number][] = [
  [390, 844, 3], [390, 844, 1], [820, 1180, 2],
  [1280, 800, 1], [1512, 860, 2], [1920, 1080, 1],
];

// Page footprint: a turned frame keeps its own unrotated width/height and
// the caller rotates it, so on the page the two are swapped.
function footprint(r: { width: number; height: number }, turned: boolean) {
  return turned ? { width: r.height, height: r.width } : r;
}

// ── 1. Nothing ever hangs off the side ────────────────────────────────
//
// Every device, every scale, every mode, every viewport, both orientations.
// This is the check that would have caught letting a picked scale overflow:
// on a phone at DPR 3 the netpad's 1× fits, so the default 2× was honoured
// at 607 px on a 390 px screen.
for (const [vw, vh, dpr] of VIEWPORTS) {
  viewport(vw, vh, dpr);
  for (const { name, layout, lcd, turns } of LAYOUTS) {
    for (const orientation of (turns ? [0, 1] : [0]) as (0 | 1)[]) {
      for (const scale of DEVICE_SCALES) {
        const f = footprint(
          calcContainerSize(layout, lcd[0], lcd[1], 'device', scale, false, false, orientation),
          orientation === 1);
        check(f.width <= vw + 1e-6,
              `${name} o${orientation} ${scale}x fits ${vw}x${vh}@${dpr}: ${f.width} > ${vw}`);
      }
      const f = footprint(
        calcContainerSize(layout, lcd[0], lcd[1], 'fill', 1, false, false, orientation),
        orientation === 1);
      check(f.width <= vw + 1e-6,
            `${name} o${orientation} fill fits ${vw}x${vh}@${dpr}: ${f.width} > ${vw}`);
    }
  }
}

// Fullscreen fits both ways, on both axes — there is no page to scroll.
for (const [vw, vh, dpr] of VIEWPORTS) {
  viewport(vw, vh, dpr);
  for (const { name, layout, lcd, turns } of LAYOUTS) {
    for (const orientation of (turns ? [0, 1] : [0]) as (0 | 1)[]) {
      const f = footprint(
        calcContainerSize(layout, lcd[0], lcd[1], 'device', 4, true, false, orientation),
        orientation === 1);
      check(f.width <= vw + 1e-6 && f.height <= vh + 1e-6,
            `${name} o${orientation} fullscreen fits ${vw}x${vh}@${dpr}`);
    }
  }
}

// ── 2. A scale that fits is exact; one that doesn't fills the window ──
//
// "2×" means two physical pixels per emulated pixel: the panel's CSS size
// is lcd × scale / devicePixelRatio, and the frame is that divided by the
// panel's fraction of it. Where the window can't manage that, the device
// grows to the full width of the window — NOT back down to the last whole
// scale that fits, which would park a netpad at 910 px in a 1482 px window
// and waste the space.
for (const [vw, vh, dpr] of VIEWPORTS) {
  viewport(vw, vh, dpr);
  for (const { name, layout, lcd, turns } of LAYOUTS) {
    for (const orientation of (turns ? [0, 1] : [0]) as (0 | 1)[]) {
      const filled = calcContainerSize(layout, lcd[0], lcd[1], 'fill', 1, false, false, orientation);
      for (const scale of DEVICE_SCALES) {
        const where = `${name} o${orientation} at ${scale}x on ${vw}x${vh}@${dpr}`;
        const exact = largestFittingScale(layout, lcd[0], lcd[1], scale, orientation) === scale;
        const r = calcContainerSize(layout, lcd[0], lcd[1], 'device', scale, false, false, orientation);
        if (exact) {
          near(r.width * layout.screenWidth, lcd[0] * scale / dpr, `${where} is exact across`);
          near(r.height * layout.screenHeight, lcd[1] * scale / dpr, `${where} is exact down`);
        } else {
          near(r.width, filled.width, `${where} doesn't fit, so it fills the window`);
        }
      }
    }
  }
}

// ── 3. Every level up to the useful one changes the picture ──────────
//
// largestUsefulScale is what the header greys out by, so it has to agree
// with what gets drawn: each level up to it must be visibly bigger than the
// last, and everything past it must come back the same size as it (the
// device is already as wide as the window by then).
for (const [vw, vh, dpr] of VIEWPORTS) {
  viewport(vw, vh, dpr);
  for (const { name, layout, lcd, turns } of LAYOUTS) {
    for (const orientation of (turns ? [0, 1] : [0]) as (0 | 1)[]) {
      const max = Math.max(1, largestUsefulScale(layout, lcd[0], lcd[1], orientation));
      const widths = DEVICE_SCALES.map(s =>
        calcContainerSize(layout, lcd[0], lcd[1], 'device', s, false, false, orientation).width);
      const where = `${name} o${orientation} on ${vw}x${vh}@${dpr} (useful ≤ ${max})`;
      for (let s = 2; s <= max; s++)
        check(widths[s - 1] > widths[s - 2] + 1, `${where}: ${s}x is bigger than ${s - 1}x`);
      for (let s = max + 1; s <= 4; s++)
        near(widths[s - 1], widths[max - 1], `${where}: ${s}x draws the same as ${max}x`);
    }
  }
}

// ── 4. A landscape device reaches the full width of the window ───────
//
// The netpad is no more special than the rest: on any window that can show
// it at all, some level fills the width, and it is never pinned to 1×.
for (const [vw, vh, dpr] of VIEWPORTS) {
  viewport(vw, vh, dpr);
  const filledWidths = LAYOUTS.map(({ layout, lcd }) =>
    calcContainerSize(layout, lcd[0], lcd[1], 'fill', 1, false, false, 0).width);
  LAYOUTS.forEach(({ name, layout, lcd }, i) => {
    const where = `${name} on ${vw}x${vh}@${dpr}`;
    const widths = DEVICE_SCALES.map(s =>
      calcContainerSize(layout, lcd[0], lcd[1], 'device', s, false, false, 0).width);
    // Either some level fills the width, or the 1×–4× range ran out before
    // the window did (a small panel on a big display at 4× can be smaller
    // than the window and still be the biggest honest thing to draw).
    check(widths.some(w => Math.abs(w - filledWidths[i]) < 1e-6)
            || largestFittingScale(layout, lcd[0], lcd[1], 4, 0) === 4,
          `${where}: the scales reach the full window width, or run out at a pixel-perfect 4×`);
    check(largestUsefulScale(layout, lcd[0], lcd[1], 0) >= 1, `${where}: at least one level`);
  });
}
// On a laptop that means more than one level, for every landscape device.
viewport(1512, 860, 1);
for (const { name, layout, lcd } of LAYOUTS) {
  check(largestUsefulScale(layout, lcd[0], lcd[1], 0) >= 2,
        `${name} offers more than one zoom level on a 1512@1 laptop`);
}
check(largestUsefulScale(LAYOUTS[1].layout, 640, 240, 0) === 2,
      'netpad photo: 1x is pixel-perfect and 2x fills the window');
viewport(1512, 860, 2);
check(largestUsefulScale(LAYOUTS[0].layout, 640, 240, 0) === 4,
      'netpad bare reaches a pixel-perfect 4x on a 1512@2 laptop');

// ── 5. A machine too big for 1× is fitted, not overflowed ─────────────
//
// A phone at DPR 1: even the smallest scale is wider than the screen, so
// there is nothing pixel-perfect to draw and the device is fitted to the
// window exactly as 'fill' does. largestFittingScale says 0, and the header
// offers 1× alone.
viewport(390, 844, 1);
for (const { name, layout, lcd } of LAYOUTS) {
  check(largestFittingScale(layout, lcd[0], lcd[1], 4, 0) === 0,
        `${name} cannot be drawn pixel-perfect on a 390px phone`);
  check(largestUsefulScale(layout, lcd[0], lcd[1], 0) === 1,
        `${name} offers only 1x on a 390px phone`);
  const r = calcContainerSize(layout, lcd[0], lcd[1], 'device', 2, false, false, 0);
  const f = calcContainerSize(layout, lcd[0], lcd[1], 'fill', 1, false, false, 0);
  near(r.width, f.width, `${name} falls back to the fitted size on a phone`);
}

viewport(1512, 860);
if (failures) {
  console.error(`\n${failures} check(s) FAILED`);
  process.exit(1);
}
console.log('PASS deviceSizing (stays in the window, fills it, levels declared)');
