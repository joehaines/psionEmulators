// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// How big the device frame is drawn — the maths behind the sizing controls
// in the page header (device mode, actual size 1×–4×, fill window,
// fullscreen).
//
// The rule everything here obeys: THE DEVICE NEVER LEAVES THE WINDOW
// SIDEWAYS. A machine wider than the viewport is unusable on a phone and
// merely annoying on a desktop, and no zoom level is worth it — so a scale
// the window can't show is not drawn at that scale. The one deliberate
// exception is vertical, and it predates all of this: an upright device may
// let the CASE (never the emulated screen) run past the bottom, because the
// page scrolls down to it.
//
// Split out of EmulatorView so it can be exercised without a DOM: the way
// this breaks is silent, because every path returns a plausible-looking
// number and the only symptom of a wrong one is a device that hangs off the
// screen or a header button that stops changing anything.

import { isQuarterTurn } from './screenRotation.ts';
import type { Quadrant } from './screenRotation.ts';

export type SizingMode = 'device' | 'fill';
// Integer zoom factor for the 'device' (pixel-perfect) mode — 1 device
// pixel maps to N physical screen pixels. Selected via the hover flyout
// on the actual-size header button; persists across reloads and across
// device switches.
export type DeviceScale = 1 | 2 | 3 | 4;
export const DEVICE_SCALES: readonly DeviceScale[] = [1, 2, 3, 4];

// The part of a skin layout the sizing maths needs: the frame's aspect
// ratio and where the emulated screen sits inside it, as fractions of the
// frame. EmulatorView's SkinLayout carries all this plus the digitiser and
// silkscreen geometry, and satisfies this structurally.
export interface ScreenLayout {
  aspectRatio: number;   // frame width / frame height
  screenWidth: number;   // fraction of frame width
  screenHeight: number;  // fraction of frame height
}

// Viewport budget the device is fitted into. Fullscreen gets all of it;
// otherwise 0.98 of the width, and a vertical share that leaves room for
// the page header and the control / key-helper rows EmulatorView draws
// below the frame. Chromeless (#/embed/<id>) has neither, so it can use
// almost the whole height.
function budget(isFullscreen: boolean, chromeless: boolean) {
  return {
    maxW: window.innerWidth  * (isFullscreen ? 1 : 0.98),
    maxH: window.innerHeight * (isFullscreen ? 1 : (chromeless ? 0.98 : 0.78)),
  };
}

// Frame size that renders the emulated screen at exactly `scale` physical
// pixels per emulated pixel. The canvas backing store is lcdWidth ×
// lcdHeight device pixels and the browser draws it at cssDim ×
// devicePixelRatio physical pixels, so cssDim = lcdDim × scale / DPR; the
// frame is that divided by the screen's fraction of it.
function pixelPerfectSize(
  skinLayout: ScreenLayout, lcdWidth: number, lcdHeight: number, scale: number,
) {
  const dpr = window.devicePixelRatio || 1;
  return {
    width:  ((lcdWidth  * scale) / dpr) / skinLayout.screenWidth,
    height: ((lcdHeight * scale) / dpr) / skinLayout.screenHeight,
  };
}

/**
 * The largest whole scale up to `wanted` that this window can draw
 * pixel-perfect — every emulated pixel on an exact N×N block of physical
 * ones — or 0 when it can't manage even 1× (a handheld on a phone).
 *
 * A scale past this doesn't mean "you get the one below": it means the
 * device grows to the full width of the window instead, which is bigger
 * than the last whole scale and is what the machine should do with the
 * space. It just isn't a whole number of pixels per pixel any more. See
 * largestUsefulScale for where that stops buying anything.
 */
export function largestFittingScale(
  skinLayout: ScreenLayout,
  lcdWidth: number,
  lcdHeight: number,
  wanted: DeviceScale,
  orientation: Quadrant = 0,
  forceFS = false,
  chromeless = false,
): number {
  const isFullscreen = forceFS || !!document.fullscreenElement;
  if (isFullscreen) return 0;   // fullscreen fits the frame, scale doesn't apply
  const { maxW, maxH } = budget(false, chromeless);
  const dpr = window.devicePixelRatio || 1;
  const turned = isQuarterTurn(orientation);
  for (let scale = wanted; scale >= 1; scale--) {
    const s = pixelPerfectSize(skinLayout, lcdWidth, lcdHeight, scale);
    if (turned) {
      // Turned, the page footprint is the frame's height × its width, and
      // BOTH have to fit: a 2.8:1 machine stood on its end and budgeted the
      // upright way hides a third of itself off the bottom.
      if (s.height <= maxW && s.width <= maxH) return scale;
    } else {
      // Upright: the whole frame has to fit across, but only the emulated
      // screen has to fit down — the case may run past the bottom and the
      // page scrolls to it. (For a skinless device screenHeight is 1 and
      // the two are the same thing.)
      if (s.width <= maxW && (lcdHeight * scale) / dpr <= maxH) return scale;
    }
  }
  return 0;
}

/**
 * The highest scale in the flyout that still changes the picture — what the
 * header greys out above.
 *
 * Everything up to largestFittingScale is drawn pixel-perfect and each step
 * is visibly bigger. The FIRST scale past it is still worth having: it means
 * "as big as this window goes", which is bigger again than the last whole
 * scale. Beyond that there is nothing left to give — the device is already
 * as wide as the window — so those levels would come back the identical
 * picture, and a button that can't change anything should say so rather than
 * look inert.
 */
export function largestUsefulScale(
  skinLayout: ScreenLayout,
  lcdWidth: number,
  lcdHeight: number,
  orientation: Quadrant = 0,
  forceFS = false,
  chromeless = false,
): number {
  // Fullscreen ignores the scale entirely, and the header is covered by the
  // device anyway — don't grey anything out on the way back.
  if (forceFS || !!document.fullscreenElement) return 4;
  const fits = largestFittingScale(skinLayout, lcdWidth, lcdHeight, 4,
                                   orientation, forceFS, chromeless);
  if (fits >= 4) return 4;
  // Only offer the fit-to-window step when it is actually a bigger picture:
  // a device whose whole-scale size already fills the window exactly has
  // nothing more to grow into.
  const filled = calcContainerSize(skinLayout, lcdWidth, lcdHeight, 'fill', 1,
                                   forceFS, chromeless, orientation);
  const whole = fits > 0
    ? pixelPerfectSize(skinLayout, lcdWidth, lcdHeight, fits)
    : { width: 0, height: 0 };
  return filled.width > whole.width + 1 ? fits + 1 : fits;
}

export function calcContainerSize(
  skinLayout: ScreenLayout,
  lcdWidth: number,
  lcdHeight: number,
  mode: SizingMode,
  deviceScale: DeviceScale,
  forceFS = false,
  chromeless = false,
  orientation: Quadrant = 0,
): { width: number; height: number } {
  const isFullscreen = forceFS || !!document.fullscreenElement;

  // Pixel-perfect, when the window can draw the asked-for scale exactly.
  if (mode === 'device' && !isFullscreen &&
      largestFittingScale(skinLayout, lcdWidth, lcdHeight,
                          deviceScale, orientation, forceFS, chromeless) === deviceScale) {
    return pixelPerfectSize(skinLayout, lcdWidth, lcdHeight, deviceScale);
  }
  // It can't, so the device grows to fill the window instead — the paths
  // below. Dropping back to the largest whole scale that DOES fit would be
  // the other option and it is worse: a netpad whose 2× is a shade too wide
  // would sit at 1×, 910 px of machine in a 1482 px window, with the space
  // going to waste. Asking for more than fits means "as big as this window
  // goes", and the header greys out the levels past the first one that
  // means that (largestUsefulScale), so no button lies about what it does.

  // A turned device (netpad in portrait) has to be fitted to the viewport
  // by its ROTATED footprint — a 640×240 frame laid on its side is tall,
  // not wide. Size a virtual device whose layout is transposed, then swap
  // the result back into the frame's own unrotated space, which is what
  // every skin measurement below is expressed in.
  if (isQuarterTurn(orientation)) {
    const transposed: ScreenLayout = {
      ...skinLayout,
      aspectRatio:  1 / skinLayout.aspectRatio,
      screenWidth:  skinLayout.screenHeight,
      screenHeight: skinLayout.screenWidth,
    };
    // 'fill' rather than `mode`: getting here at all means the pixel-perfect
    // answer was already ruled out above, so this is the fit-to-window path
    // whichever button the user pressed. (Recursing with 'device' would make
    // the inner call re-decide that on the transposed layout, against the
    // upright rule, and answer a question we have already settled.)
    const r = calcContainerSize(transposed, lcdHeight, lcdWidth, 'fill', deviceScale,
                                forceFS, chromeless, 0);
    // Then make the whole footprint fit. The paths below deliberately
    // budget the vertical space against the emulated screen rather than
    // the case around it, letting a wide device fill the viewport width
    // and run its surround off the bottom (the page scrolls to it). Stood
    // on its end that rule hides a third of the machine, so a turned
    // device is clamped by its footprint instead.
    const { maxW, maxH } = budget(isFullscreen, chromeless);
    const fit = Math.min(1, maxW / r.width, maxH / r.height);
    return { width: r.height * fit, height: r.width * fit };
  }

  const aspectRatio = skinLayout.aspectRatio;
  if (isFullscreen) {
    const w = Math.min(window.innerWidth, window.innerHeight * aspectRatio);
    return { width: w, height: w / aspectRatio };
  }
  // 'fill' — and the too-small-for-1× fallback above. Width overflow is
  // prevented on the whole device surround (clamp to maxW), but height
  // overflow is prevented only on the emulated LCD screen: the maxH budget
  // applies to the screen sub-rectangle (screenHeight fraction of the
  // surround), not the surround itself. Dividing by screenHeight lets the
  // surround run past the viewport vertically so the device can fill the
  // available width; the page scrolls to reach the rest of the case. For
  // skinless devices screenHeight === 1, so the two coincide.
  const { maxW, maxH } = budget(false, chromeless);
  const w = Math.min(maxW, (maxH * aspectRatio) / skinLayout.screenHeight);
  return { width: w, height: w / aspectRatio };
}
