// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Screen-orientation geometry.
//
// The netpad's Tools menu carries "Switch orientation", and choosing it
// turns the machine from a landscape slab into a portrait one. Nothing
// moves in hardware: EPOC keeps the same 640×240 framebuffer and simply
// draws the whole desktop rotated inside it (see netpadScreenOrientation
// in core/sa1100.cpp), so it falls to the frontend to turn the device to
// match — rotate the frame 90° anticlockwise and the UI reads upright
// again, exactly as it would if you picked the real machine up and
// turned it.
//
// The emulator reports the state as a quadrant count: the number of
// quarter-turns ANTICLOCKWISE the panel image has to be shown at. Every
// device other than a rotated netpad reports 0, and then every helper
// here is an identity.

export type Quadrant = 0 | 1 | 2 | 3;

/** Clamps whatever the emulator reported to a 0..3 quarter-turn count. */
export function normaliseQuadrant(value: number | undefined | null): Quadrant {
  if (!Number.isFinite(value as number)) return 0;
  const q = ((Math.round(value as number) % 4) + 4) % 4;
  return q as Quadrant;
}

/** True when the quadrant swaps the device's width and height. */
export function isQuarterTurn(q: Quadrant): boolean {
  return q === 1 || q === 3;
}

/**
 * Footprint the rotated device frame occupies on the page. The frame
 * itself keeps its own unrotated width × height (the skin layout is
 * expressed in that space); only the box it sits in swaps.
 */
export function rotatedFootprint(q: Quadrant, width: number, height: number) {
  return isQuarterTurn(q) ? { width: height, height: width } : { width, height };
}

/**
 * Inline style that places the device frame inside that footprint: the
 * frame is centred in the box and then spun about its own centre, so a
 * quarter turn lands its (swapped) edges exactly on the box's.
 */
export function rotationStyle(q: Quadrant, width: number, height: number) {
  const box = rotatedFootprint(q, width, height);
  return {
    left: (box.width - width) / 2,
    top: (box.height - height) / 2,
    width,
    height,
    transform: q === 0 ? undefined : `rotate(${-90 * q}deg)`,
  };
}

/**
 * Maps a pointer event to coordinates in an element's own, unrotated CSS
 * box.
 *
 * `getBoundingClientRect()` on a rotated element reports the axis-aligned
 * box around it, so the usual `clientX - rect.left` is wrong the moment
 * the device is turned — a tap near the top of a portrait netpad would
 * land near the left edge of the panel. Rotation is about the element's
 * centre, which the bounding box shares, so undoing it is a rotation of
 * the offset from that centre by the opposite angle.
 *
 * `width` / `height` are the element's untransformed CSS size, which is
 * what the caller's own layout maths is expressed in.
 */
export function localPointerPos(
  q: Quadrant,
  rect: { left: number; top: number; width: number; height: number },
  clientX: number,
  clientY: number,
  width: number,
  height: number,
): { x: number; y: number } {
  const dx = clientX - (rect.left + rect.width / 2);
  const dy = clientY - (rect.top + rect.height / 2);
  // Inverse of rotate(-90q deg), i.e. rotate(+90q deg) applied to the
  // centre-relative offset.
  let lx: number, ly: number;
  switch (q) {
    case 1:  lx = -dy; ly =  dx; break;
    case 2:  lx = -dx; ly = -dy; break;
    case 3:  lx =  dy; ly = -dx; break;
    default: lx =  dx; ly =  dy; break;
  }
  return { x: width / 2 + lx, y: height / 2 + ly };
}
