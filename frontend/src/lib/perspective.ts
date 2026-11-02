// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Perspective (projective) mapping between the unit square and an
// arbitrary convex quadrilateral.
//
// Most device photos are shot square-on, so their LCD is an axis-aligned
// rectangle and the emulated screen is simply stretched over it. A photo
// of an open clamshell with its lid tipped back (the Psion LX) shows the
// screen as a trapezoid instead — wider at the top than the bottom, its
// sides leaning in. Painting the LCD there means warping it with a
// homography, and turning a tap on the photo back into a panel pixel
// means running the same homography backwards.
//
// The forward map is the classic square-to-quad construction (Heckbert,
// "Fundamentals of Texture Mapping and Image Warping", 1989): with u, v in
// [0, 1] along the panel's width and height,
//
//     x = (a·u + b·v + c) / (g·u + h·v + 1)
//     y = (d·u + e·v + f) / (g·u + h·v + 1)

export type Point = readonly [number, number];
/** Corners in order: top-left, top-right, bottom-right, bottom-left. */
export type Quad = readonly [Point, Point, Point, Point];
/** Row-major 3×3 with the bottom-right element fixed at 1. */
export type Homography = readonly [number, number, number, number, number, number, number, number, number];

/** Homography taking the unit square onto `quad` (corner for corner). */
export function squareToQuad(quad: Quad): Homography {
  const [[x0, y0], [x1, y1], [x2, y2], [x3, y3]] = quad;
  const dx3 = x0 - x1 + x2 - x3;
  const dy3 = y0 - y1 + y2 - y3;
  if (dx3 === 0 && dy3 === 0) {
    // A parallelogram: the map is affine.
    return [x1 - x0, x3 - x0, x0, y1 - y0, y3 - y0, y0, 0, 0, 1];
  }
  const dx1 = x1 - x2, dx2 = x3 - x2;
  const dy1 = y1 - y2, dy2 = y3 - y2;
  const det = dx1 * dy2 - dx2 * dy1;
  const g = (dx3 * dy2 - dx2 * dy3) / det;
  const h = (dx1 * dy3 - dx3 * dy1) / det;
  return [
    x1 - x0 + g * x1, x3 - x0 + h * x3, x0,
    y1 - y0 + g * y1, y3 - y0 + h * y3, y0,
    g, h, 1,
  ];
}

/** Apply `m` to the point (x, y). */
export function project(m: Homography, x: number, y: number): [number, number] {
  const w = m[6] * x + m[7] * y + m[8];
  return [(m[0] * x + m[1] * y + m[2]) / w, (m[3] * x + m[4] * y + m[5]) / w];
}

/** The inverse map, normalised so its last element is 1 again. */
export function invert(m: Homography): Homography {
  const [a, b, c, d, e, f, g, h, i] = m;
  const A = e * i - f * h, B = c * h - b * i, C = b * f - c * e;
  const D = f * g - d * i, E = a * i - c * g, F = c * d - a * f;
  const G = d * h - e * g, H = b * g - a * h, I = a * e - b * d;
  return [A / I, B / I, C / I, D / I, E / I, F / I, G / I, H / I, 1];
}

/**
 * A CSS `matrix3d()` that warps a `width` × `height` element onto `quad`
 * (coordinates in the element's own parent space, relative to its
 * top-left). Use with `transform-origin: 0 0`.
 */
export function quadMatrix3d(quad: Quad, width: number, height: number): string {
  const [a, b, c, d, e, f, g, h] = squareToQuad(quad);
  // Scale the element's box down to the unit square first.
  const sx = 1 / width, sy = 1 / height;
  // CSS lists the matrix column by column.
  return `matrix3d(${[
    a * sx, d * sx, 0, g * sx,
    b * sy, e * sy, 0, h * sy,
    0, 0, 1, 0,
    c, f, 0, 1,
  ].join(',')})`;
}

/**
 * Draw `src` (sw × sh pixels) onto a 2D context warped into `quad`.
 * Canvas 2D can only do affine transforms, so the image is cut into a
 * grid of cells and each is drawn with the affine map that agrees with
 * the homography at its corners. At this grid size the error is far below
 * a pixel; each cell is drawn one source pixel over-size so neighbouring
 * cells overlap rather than leave hairline seams between them.
 */
export function drawImageToQuad(
  ctx: CanvasRenderingContext2D,
  src: CanvasImageSource,
  sw: number, sh: number,
  quad: Quad,
  cols = 32, rows = 24,
): void {
  const m = squareToQuad(quad);
  const cw = sw / cols, ch = sh / rows;
  for (let r = 0; r < rows; r++) {
    for (let k = 0; k < cols; k++) {
      const sx = k * cw, sy = r * ch;
      const [x00, y00] = project(m, k / cols, r / rows);
      const [x10, y10] = project(m, (k + 1) / cols, r / rows);
      const [x01, y01] = project(m, k / cols, (r + 1) / rows);
      const a = (x10 - x00) / cw, b = (y10 - y00) / cw;
      const c = (x01 - x00) / ch, d = (y01 - y00) / ch;
      ctx.save();
      ctx.transform(a, b, c, d, x00 - a * sx - c * sy, y00 - b * sx - d * sy);
      const w = Math.min(cw + 1, sw - sx), h = Math.min(ch + 1, sh - sy);
      ctx.drawImage(src, sx, sy, w, h, sx, sy, w, h);
      ctx.restore();
    }
  }
}
