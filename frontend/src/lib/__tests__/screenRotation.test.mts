// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Screen-rotation geometry tests. Run via:
//   node --experimental-strip-types frontend/src/lib/__tests__/screenRotation.test.mts
// or `npm run test:rotation` from frontend/.
//
// Covers the maths behind the netpad's "Switch orientation": the device
// frame is turned a quarter-turn anticlockwise, and pointer positions
// have to be turned back before they can be mapped onto the (never
// rotated) digitiser. The interesting property is the round trip — a
// tap on a given spot of the turned device must reach the same panel
// pixel it would have reached with the device the right way up.

import {
  normaliseQuadrant, isQuarterTurn, rotatedFootprint, rotationStyle, localPointerPos,
  type Quadrant,
} from '../screenRotation.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}
function near(a: number, b: number, msg: string, eps = 1e-9) {
  check(Math.abs(a - b) <= eps, `${msg}: got ${a}, want ${b}`);
}

// ── Quadrant normalisation ────────────────────────────────────────────
check(normaliseQuadrant(0) === 0, 'quadrant 0');
check(normaliseQuadrant(1) === 1, 'quadrant 1');
check(normaliseQuadrant(4) === 0, 'quadrant wraps at 4');
check(normaliseQuadrant(-1) === 3, 'negative quadrant wraps up');
check(normaliseQuadrant(undefined) === 0, 'missing orientation reads as upright');
check(normaliseQuadrant(NaN) === 0, 'NaN orientation reads as upright');
check(isQuarterTurn(1) && isQuarterTurn(3), 'odd quadrants swap the axes');
check(!isQuarterTurn(0) && !isQuarterTurn(2), 'even quadrants keep the axes');

// ── Footprint ─────────────────────────────────────────────────────────
// The netpad frame is landscape; turned, it stands the other way up.
check(JSON.stringify(rotatedFootprint(0, 800, 300)) === JSON.stringify({ width: 800, height: 300 }),
      'upright footprint is the frame itself');
check(JSON.stringify(rotatedFootprint(1, 800, 300)) === JSON.stringify({ width: 300, height: 800 }),
      'quarter-turn footprint swaps width and height');
check(JSON.stringify(rotatedFootprint(2, 800, 300)) === JSON.stringify({ width: 800, height: 300 }),
      'half-turn footprint is unswapped');

// ── Frame placement ───────────────────────────────────────────────────
const upright = rotationStyle(0, 800, 300);
check(upright.transform === undefined, 'upright frame carries no transform');
near(upright.left, 0, 'upright frame is flush left');
near(upright.top, 0, 'upright frame is flush top');

const turned = rotationStyle(1, 800, 300);
check(turned.transform === 'rotate(-90deg)', 'a quarter-turn is anticlockwise');
check(turned.width === 800 && turned.height === 300, 'the frame keeps its own box');
// Centred in the 300×800 footprint before being spun about its centre.
near(turned.left, (300 - 800) / 2, 'turned frame is centred horizontally');
near(turned.top,  (800 - 300) / 2, 'turned frame is centred vertically');
check(rotationStyle(3, 800, 300).transform === 'rotate(-270deg)', 'three quarter-turns');

// ── Pointer mapping ───────────────────────────────────────────────────
//
// A netpad-sized overlay: 640×240 panel drawn at 2 CSS px per pixel, so
// the element's own box is 1280×480. Turned, it occupies a 480×1280
// bounding rect on the page; place it at (100, 50).
const W = 1280, H = 480;
const uprightRect = { left: 100, top: 50, width: W, height: H };
const turnedRect  = { left: 100, top: 50, width: H, height: W };

// Upright: the mapping is the plain "offset from the top-left" it has
// always been.
const u = localPointerPos(0, uprightRect, 100 + 320, 50 + 120, W, H);
near(u.x, 320, 'upright pointer x');
near(u.y, 120, 'upright pointer y');

// Turned anticlockwise, the panel's right-hand edge ends up along the
// top of the page footprint: the top-left corner of the panel (local
// 0,0) is drawn at the bottom-left, its top-right corner at the top-left
// and its bottom-left corner at the bottom-right.
const corners: Array<[number, number, number, number, string]> = [
  // clientX, clientY, expected local x, expected local y, label
  [100,       50 + W,   0,   0, 'local top-left sits at the footprint bottom-left'],
  [100,       50,       W,   0, 'local top-right sits at the footprint top-left'],
  [100 + H,   50 + W,   0,   H, 'local bottom-left sits at the footprint bottom-right'],
  [100 + H/2, 50 + W/2, W/2, H/2, 'centre maps to centre'],
];
for (const [cx, cy, wantX, wantY, label] of corners) {
  const p = localPointerPos(1, turnedRect, cx, cy, W, H);
  near(p.x, wantX, `${label} (x)`);
  near(p.y, wantY, `${label} (y)`);
}

// Round trip: for every quadrant, un-rotating the screen position of a
// known local point must give that local point back. This is the
// property the touch path depends on — the digitiser never turns, so a
// tap has to land on the same panel pixel however the frame is drawn.
function screenPosOfLocal(q: Quadrant, rect: { left: number; top: number; width: number; height: number },
                          lx: number, ly: number) {
  const cx = rect.left + rect.width / 2, cy = rect.top + rect.height / 2;
  const dx = lx - W / 2, dy = ly - H / 2;
  switch (q) {
    case 1:  return { x: cx + dy, y: cy - dx };   // rotate(-90deg)
    case 2:  return { x: cx - dx, y: cy - dy };
    case 3:  return { x: cx - dy, y: cy + dx };
    default: return { x: cx + dx, y: cy + dy };
  }
}
for (const q of [0, 1, 2, 3] as Quadrant[]) {
  const rect = isQuarterTurn(q) ? turnedRect : uprightRect;
  for (const [lx, ly] of [[0, 0], [W, 0], [0, H], [W, H], [17, 431], [W / 2, H / 2]]) {
    const s = screenPosOfLocal(q, rect, lx, ly);
    const back = localPointerPos(q, rect, s.x, s.y, W, H);
    near(back.x, lx, `q=${q} round-trip x for (${lx},${ly})`, 1e-9);
    near(back.y, ly, `q=${q} round-trip y for (${lx},${ly})`, 1e-9);
  }
}

// Worked example against the real ROM. Driving the booted netpad through
// the native harness, the portrait "Switch orientation" menu entry sits
// at panel pixel (331, 122) — the point that re-selects it and turns the
// machine back (tests/stress/netpad_orientation.sh). With the device
// turned on screen, that entry is drawn 122 CSS-px-per-panel-pixel-pair
// from the left of the footprint and 331 up from its bottom; clicking
// there must produce panel (331, 122) again.
const scale = 2;   // CSS px per panel pixel, matching W/H above
const clickX = turnedRect.left + 122 * scale;
const clickY = turnedRect.top + (640 - 331) * scale;
const hit = localPointerPos(1, turnedRect, clickX, clickY, W, H);
near(Math.round(hit.x / scale), 331, 'menu entry maps back to panel x');
near(Math.round(hit.y / scale), 122, 'menu entry maps back to panel y');

if (failures === 0) {
  console.log('PASS screenRotation (footprint, frame placement, pointer round trip)');
  process.exit(0);
} else {
  console.error(`FAIL: ${failures} assertion(s)`);
  process.exit(1);
}
