// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Tests for the live-resize aspect constraint.
//
// The reason these are worth writing: every branch in constrainToRatio
// returns a plausible-looking rectangle, so a wrong one shows up only as a
// window that slowly drifts out of shape or crawls sideways while you drag
// it — a bug that is easy to ship and tedious to bisect by hand.
//
// Run: node --experimental-strip-types desktop/test/aspect.test.mts

import {
  constrainToRatio, largestIntegerScale, snapToIntegerScale,
  type Rect, type ResizeEdge,
} from '../src/main/aspect.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}
function eq(actual: unknown, expected: unknown, msg: string) {
  if (actual !== expected) {
    console.error(`FAIL: ${msg} (got ${String(actual)}, want ${String(expected)})`);
    failures++;
  }
}
/** Content ratio, so assertions read in the same terms as the argument. */
function ratioOf(r: Rect, exW = 0, exH = 0): number {
  return (r.width - exW) / (r.height - exH);
}

// A Series 5-ish 640x240 panel: ratio 8/3, wide enough that a width-driven
// and a height-driven solve give visibly different answers.
const RATIO = 640 / 240;
const NO_EXTRA = { width: 0, height: 0 };
const LIMITS = { minWidth: 1, minHeight: 1 };

// ── Unlocked passthrough ────────────────────────────────────────────────
{
  const r: Rect = { x: 10, y: 20, width: 333, height: 111 };
  const out = constrainToRatio(r, 'right', 0, NO_EXTRA, LIMITS);
  eq(out, r, 'ratio 0 returns the input untouched (same object is fine)');
  const nan = constrainToRatio(r, 'right', Number.NaN, NO_EXTRA, LIMITS);
  eq(nan, r, 'non-finite ratio returns the input untouched');
}

// ── Which axis is authoritative ─────────────────────────────────────────
{
  // Dragging the right edge: the user is expressing WIDTH, so width must
  // survive exactly and height must follow it.
  const out = constrainToRatio({ x: 0, y: 0, width: 800, height: 999 },
                               'right', RATIO, NO_EXTRA, LIMITS);
  eq(out.width, 800, 'right drag preserves the dragged width');
  eq(out.height, 300, 'right drag derives height from width');
  check(Math.abs(ratioOf(out) - RATIO) < 1e-9, 'right drag lands on the ratio');
}
{
  // Dragging the bottom edge: HEIGHT is authoritative.
  const out = constrainToRatio({ x: 0, y: 0, width: 999, height: 300 },
                               'bottom', RATIO, NO_EXTRA, LIMITS);
  eq(out.height, 300, 'bottom drag preserves the dragged height');
  eq(out.width, 800, 'bottom drag derives width from height');
}
{
  // Corners are ambiguous; width wins, unconditionally, so the drag cannot
  // oscillate between two valid solutions.
  for (const edge of ['top-left', 'top-right', 'bottom-left', 'bottom-right'] as ResizeEdge[]) {
    const out = constrainToRatio({ x: 100, y: 100, width: 800, height: 999 },
                                 edge, RATIO, NO_EXTRA, LIMITS);
    eq(out.width, 800, `${edge} takes width as authoritative`);
    eq(out.height, 300, `${edge} derives height`);
  }
}
{
  // Platforms that don't report an edge must still be constrained.
  const out = constrainToRatio({ x: 0, y: 0, width: 800, height: 50 },
                               'unknown', RATIO, NO_EXTRA, LIMITS);
  eq(out.width, 800, "'unknown' edge still constrains");
  eq(out.height, 300, "'unknown' edge derives height from width");
}

// ── Anchoring: the far edge must not move ───────────────────────────────
{
  // A left-edge drag: Electron has moved x and grown width, leaving the
  // right edge fixed at 900. Adjusting the size must not move it.
  const next: Rect = { x: 100, y: 50, width: 800, height: 400 };
  const out = constrainToRatio(next, 'left', RATIO, NO_EXTRA, LIMITS);
  eq(out.x + out.width, 900, 'left drag keeps the right edge anchored');
  eq(out.y, 50, 'left drag leaves the top where it was');
}
{
  const next: Rect = { x: 100, y: 50, width: 800, height: 400 };
  const out = constrainToRatio(next, 'top', RATIO, NO_EXTRA, LIMITS);
  eq(out.y + out.height, 450, 'top drag keeps the bottom edge anchored');
  eq(out.x, 100, 'top drag leaves the left where it was');
}
{
  // top-left must pin the bottom-right corner on BOTH axes.
  const next: Rect = { x: 100, y: 50, width: 800, height: 400 };
  const out = constrainToRatio(next, 'top-left', RATIO, NO_EXTRA, LIMITS);
  eq(out.x + out.width, 900, 'top-left keeps the right edge anchored');
  eq(out.y + out.height, 450, 'top-left keeps the bottom edge anchored');
}
{
  // bottom-right pins the origin — the common case, and the one where a
  // sign error would be least noticeable.
  const next: Rect = { x: 100, y: 50, width: 800, height: 999 };
  const out = constrainToRatio(next, 'bottom-right', RATIO, NO_EXTRA, LIMITS);
  eq(out.x, 100, 'bottom-right keeps x');
  eq(out.y, 50, 'bottom-right keeps y');
}
{
  // top-right: x anchored (dragging the right edge), bottom anchored.
  const next: Rect = { x: 100, y: 50, width: 800, height: 400 };
  const out = constrainToRatio(next, 'top-right', RATIO, NO_EXTRA, LIMITS);
  eq(out.x, 100, 'top-right keeps the left edge');
  eq(out.y + out.height, 450, 'top-right keeps the bottom edge anchored');
}

// ── `extra` is excluded from the ratio ──────────────────────────────────
{
  // A pinned 40px key strip. The RATIO applies to the panel above it, so
  // the window must end up 40px taller than an unadorned one.
  const extra = { width: 0, height: 40 };
  const out = constrainToRatio({ x: 0, y: 0, width: 800, height: 0 },
                               'right', RATIO, extra, LIMITS);
  eq(out.width, 800, 'extra height does not change the width');
  eq(out.height, 340, 'extra height is added on top of the derived panel height');
  check(Math.abs(ratioOf(out, 0, 40) - RATIO) < 1e-9,
        'the panel inside the window still honours the ratio');
}
{
  // Extra on the width axis, for a side strip.
  const extra = { width: 28, height: 0 };
  const out = constrainToRatio({ x: 0, y: 0, width: 828, height: 0 },
                               'right', RATIO, extra, LIMITS);
  eq(out.height, 300, 'extra width is excluded before the ratio is applied');
}
{
  // Driving from height with extra present must round-trip too.
  const extra = { width: 0, height: 40 };
  const out = constrainToRatio({ x: 0, y: 0, width: 0, height: 340 },
                               'bottom', RATIO, extra, LIMITS);
  eq(out.height, 340, 'bottom drag with extra preserves the dragged height');
  eq(out.width, 800, 'bottom drag with extra derives the panel width');
}

// ── Minimums, honoured without breaking the ratio ───────────────────────
{
  // Dragged far too small. The result must respect BOTH minimums and still
  // sit exactly on the ratio — the case where a naive clamp would satisfy
  // the minimum and quietly leave the window the wrong shape.
  const limits = { minWidth: 320, minHeight: 200 };
  const out = constrainToRatio({ x: 0, y: 0, width: 10, height: 10 },
                               'bottom-right', RATIO, NO_EXTRA, limits);
  check(out.width >= limits.minWidth, `min width respected (got ${out.width})`);
  check(out.height >= limits.minHeight, `min height respected (got ${out.height})`);
  check(Math.abs(ratioOf(out) - RATIO) < 0.01,
        `ratio survives the minimum clamp (got ${ratioOf(out).toFixed(4)})`);
  // minHeight 200 at 8:3 needs 533px of width, so minWidth 320 is not the
  // binding constraint here — the height one is.
  eq(out.height, 200, 'the binding minimum is the height one');
  eq(out.width, 533, 'width comes from the binding height minimum');
}
{
  // A tall-thin panel, where the width minimum binds instead.
  const tall = 240 / 640;
  const limits = { minWidth: 320, minHeight: 100 };
  const out = constrainToRatio({ x: 0, y: 0, width: 1, height: 1 },
                               'bottom-right', tall, NO_EXTRA, limits);
  check(out.width >= 320, `tall panel respects min width (got ${out.width})`);
  check(out.height >= 100, `tall panel respects min height (got ${out.height})`);
  check(Math.abs(ratioOf(out) - tall) < 0.01, 'tall panel keeps its ratio at the minimum');
}

// ── Rounding: integers out, always ──────────────────────────────────────
{
  // 1.5-ish ratios are where a float would leak into bounds and make
  // Electron round unpredictably.
  const out = constrainToRatio({ x: 0, y: 0, width: 777, height: 500 },
                               'right', 1.37, NO_EXTRA, LIMITS);
  for (const k of ['x', 'y', 'width', 'height'] as const) {
    eq(Number.isInteger(out[k]), true, `${k} is an integer`);
  }
}

// ── largestIntegerScale ─────────────────────────────────────────────────
{
  const panel = { width: 640, height: 240 };
  eq(largestIntegerScale(panel, { width: 1920, height: 1080 }), 3,
     '1920x1080 fits 3x of a 640x240 panel (width binds)');
  eq(largestIntegerScale(panel, { width: 1280, height: 400 }), 1,
     '400px of height only fits 1x');
  eq(largestIntegerScale(panel, { width: 100, height: 100 }), 1,
     'never returns less than 1, even when nothing fits');
  eq(largestIntegerScale(panel, { width: 99999, height: 99999 }, NO_EXTRA, 4), 4,
     'clamps to maxScale');
  eq(largestIntegerScale(panel, { width: 1920, height: 1080 }, { width: 0, height: 700 }), 1,
     'extra chrome eats into the available space');
  eq(largestIntegerScale({ width: 0, height: 0 }, { width: 800, height: 600 }), 1,
     'a degenerate panel does not divide by zero');
}

// ── snapToIntegerScale ──────────────────────────────────────────────────
{
  const panel = { width: 640, height: 240 };
  const near = snapToIntegerScale({ width: 1283, height: 470 }, panel);
  check(near !== null, 'a size 3px off 2x snaps');
  eq(near?.width, 1280, 'snaps width to the exact multiple');
  eq(near?.height, 480, 'snaps height to the matching multiple');
  eq(near?.scale, 2, 'reports the scale it snapped to');

  eq(snapToIntegerScale({ width: 1400, height: 500 }, panel), null,
     'a size far from any multiple is left alone');
  eq(snapToIntegerScale({ width: 200, height: 80 }, panel), null,
     'below 1x there is nothing to snap to');

  const withExtra = snapToIntegerScale({ width: 1282, height: 520 }, panel,
                                       { width: 0, height: 40 });
  eq(withExtra?.width, 1280, 'snapping ignores extra on the width axis when it is zero');
  eq(withExtra?.height, 520, 'snapped height includes the extra chrome');
}

if (failures) {
  console.error(`\n${failures} assertion(s) failed`);
  process.exit(1);
}
console.log('aspect.test.mts: all assertions passed');
