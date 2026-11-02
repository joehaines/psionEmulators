// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Perspective-mapping tests. Run via:
//   node --experimental-strip-types frontend/src/lib/__tests__/perspective.test.mts
// or `npm run test:perspective` from frontend/.
//
// The Psion LX's photo skin shows its screen as a trapezoid, so the LCD
// is warped into it and taps are mapped back out of it. What matters is
// that the corners land exactly, that straight lines stay straight, and
// that a tap round-trips to the panel pixel it was drawn from.

import { squareToQuad, project, invert, quadMatrix3d, type Quad } from '../perspective.ts';

let failures = 0;
function check(cond: unknown, msg: string) {
  if (!cond) { console.error(`FAIL: ${msg}`); failures++; }
}
function near(a: number, b: number, msg: string, eps = 1e-9) {
  check(Math.abs(a - b) <= eps, `${msg}: got ${a}, want ${b}`);
}

// The LX photo's screen, in its own pixels.
const lx: Quad = [[143, 67.5], [720.5, 64.5], [686, 466.5], [180.5, 469.5]];
const square: Quad = [[0, 0], [1, 0], [1, 1], [0, 1]];

for (const [name, quad] of [['LX trapezoid', lx], ['rectangle', [[10, 20], [110, 20], [110, 70], [10, 70]] as Quad]] as const) {
  const m = squareToQuad(quad);
  // Corners map onto corners.
  square.forEach(([u, v], i) => {
    const [x, y] = project(m, u, v);
    near(x, quad[i][0], `${name} corner ${i} x`, 1e-9);
    near(y, quad[i][1], `${name} corner ${i} y`, 1e-9);
  });
  // The inverse takes each corner back, and an interior point round-trips.
  const inv = invert(m);
  square.forEach(([u, v], i) => {
    const [ru, rv] = project(inv, quad[i][0], quad[i][1]);
    near(ru, u, `${name} inverse corner ${i} u`, 1e-9);
    near(rv, v, `${name} inverse corner ${i} v`, 1e-9);
  });
  for (const [u, v] of [[0.5, 0.5], [0.1, 0.9], [0.73, 0.21]]) {
    const [x, y] = project(m, u, v);
    const [ru, rv] = project(inv, x, y);
    near(ru, u, `${name} round trip u at (${u}, ${v})`, 1e-9);
    near(rv, v, `${name} round trip v at (${u}, ${v})`, 1e-9);
  }
}

// A projective map keeps lines straight: points along a panel row stay
// collinear on the photo.
{
  const m = squareToQuad(lx);
  const [ax, ay] = project(m, 0, 0.4);
  const [bx, by] = project(m, 0.5, 0.4);
  const [cx, cy] = project(m, 1, 0.4);
  near((bx - ax) * (cy - ay) - (by - ay) * (cx - ax), 0, 'row stays straight', 1e-6);
}

// Foreshortening: the camera looks down on the open lid, so the top edge is
// the nearer one (which is why it is the wider one) and the far, bottom half
// of the panel is squeezed. The panel's midline therefore sits below the
// point halfway between the trapezoid's top and bottom edges.
{
  const m = squareToQuad(lx);
  const [, yMid] = project(m, 0.5, 0.5);
  const topY = (lx[0][1] + lx[1][1]) / 2, botY = (lx[2][1] + lx[3][1]) / 2;
  check(yMid > (topY + botY) / 2 + 5, `midline is pushed towards the far edge (got ${yMid})`);
}

// matrix3d: decode the CSS string back into the map it applies to the
// element box and check it lands the box's corners on the quad.
{
  const w = 400, h = 300;
  const args = quadMatrix3d(lx, w, h).replace(/^matrix3d\(|\)$/g, '').split(',').map(Number);
  check(args.length === 16, 'matrix3d has 16 arguments');
  const apply = (x: number, y: number): [number, number] => {
    const X = args[0] * x + args[4] * y + args[12];
    const Y = args[1] * x + args[5] * y + args[13];
    const W = args[3] * x + args[7] * y + args[15];
    return [X / W, Y / W];
  };
  ([[0, 0], [w, 0], [w, h], [0, h]] as const).forEach(([x, y], i) => {
    const [px, py] = apply(x, y);
    near(px, lx[i][0], `matrix3d corner ${i} x`, 1e-6);
    near(py, lx[i][1], `matrix3d corner ${i} y`, 1e-6);
  });
}

if (failures) {
  console.error(`\n${failures} perspective check(s) failed`);
  process.exit(1);
}
console.log('perspective: all checks passed');
