// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Aspect-ratio constraint for a live resize drag.
//
// Why this exists when BrowserWindow.setAspectRatio() already does the job:
// setAspectRatio's `extraSize` argument is documented macOS-only, and its
// Windows behaviour has historically been patchy. A window that silently
// stops honouring its ratio on one platform is exactly the kind of bug that
// ships, so the ratio is also enforced from a 'will-resize' handler, which
// constrains the window DURING the drag on every platform.
//
// Split out as a pure function for the same reason deviceSizing.ts is: the
// way this breaks is silent. Every path returns a plausible-looking
// rectangle, and the only symptom of a wrong one is a window that drifts
// out of shape as you drag a corner.

/** Rectangle in the same shape Electron's bounds use. */
export interface Rect { x: number; y: number; width: number; height: number }

/**
 * Chrome inside the window that must NOT scale with the emulated panel —
 * the pinned key strip, mainly. The ratio applies to what's left after
 * subtracting it, so pinning the strip grows the window rather than
 * squeezing the screen.
 */
export interface Extra { width: number; height: number }

/**
 * Which handle the user is dragging. Mirrors Electron's
 * WillResizeDetails['edge'], including the 'unknown' it reports on
 * platforms that don't say.
 */
export type ResizeEdge =
  | 'top' | 'bottom' | 'left' | 'right'
  | 'top-left' | 'top-right' | 'bottom-left' | 'bottom-right'
  | 'unknown';

export interface Limits {
  minWidth: number;
  minHeight: number;
}

const HORIZONTAL_ONLY = new Set<ResizeEdge>(['left', 'right']);
const VERTICAL_ONLY = new Set<ResizeEdge>(['top', 'bottom']);
// Edges whose drag moves the window's left or top, meaning the OPPOSITE
// side is the one that must stay put.
const ANCHOR_RIGHT = new Set<ResizeEdge>(['left', 'top-left', 'bottom-left']);
const ANCHOR_BOTTOM = new Set<ResizeEdge>(['top', 'top-left', 'top-right']);

/**
 * Reshape `next` so its content honours `ratio`, keeping the edge opposite
 * the drag anchored so the window doesn't crawl across the screen.
 *
 * `ratio` is width/height of the EMULATED PANEL, not of the whole window;
 * `extra` is subtracted before the ratio is applied and added back after.
 * A ratio of 0 (or non-finite) means unlocked and returns `next` unchanged.
 *
 * Which axis wins:
 *   - dragging a left/right edge, width is what the user is expressing, so
 *     height follows;
 *   - dragging top/bottom, height wins and width follows;
 *   - on a corner (and when the platform won't say which edge), width wins.
 *     Corners are ambiguous by nature and picking one axis unconditionally
 *     is what keeps the drag from oscillating between two solutions.
 */
export function constrainToRatio(
  next: Rect,
  edge: ResizeEdge,
  ratio: number,
  extra: Extra = { width: 0, height: 0 },
  limits: Limits = { minWidth: 1, minHeight: 1 },
): Rect {
  if (!Number.isFinite(ratio) || ratio <= 0) return next;

  const exW = Math.max(0, extra.width);
  const exH = Math.max(0, extra.height);

  // The smallest window that satisfies both the caller's minimums and the
  // ratio. Without this a tall-thin machine could be dragged to a width
  // whose derived height is below minHeight, and the OS would then clamp
  // the height and break the ratio anyway.
  const minPanelW = Math.max(1, limits.minWidth - exW);
  const minPanelH = Math.max(1, limits.minHeight - exH);
  const floorPanelW = Math.max(minPanelW, minPanelH * ratio);
  const floorPanelH = floorPanelW / ratio;

  const vertical = VERTICAL_ONLY.has(edge);
  const horizontal = HORIZONTAL_ONLY.has(edge);

  let panelW: number;
  let panelH: number;
  if (vertical && !horizontal) {
    panelH = Math.max(floorPanelH, next.height - exH);
    panelW = panelH * ratio;
  } else {
    // left/right, every corner, and 'unknown'.
    panelW = Math.max(floorPanelW, next.width - exW);
    panelH = panelW / ratio;
  }

  const width = Math.round(panelW + exW);
  const height = Math.round(panelH + exH);

  // Keep the side the user is NOT dragging where Electron put it. During a
  // left-edge drag Electron has already moved x and grown width, leaving
  // x+width fixed — so re-deriving x from the anchored right edge is what
  // stops the window sliding sideways as we adjust its size.
  const x = ANCHOR_RIGHT.has(edge) ? next.x + next.width - width : next.x;
  const y = ANCHOR_BOTTOM.has(edge) ? next.y + next.height - height : next.y;

  return { x: Math.round(x), y: Math.round(y), width, height };
}

/**
 * The largest integer multiple of the emulated panel that fits `avail`,
 * clamped to `maxScale`. One device pixel to N screen pixels keeps the LCD
 * crisp; this is the window-level counterpart of deviceSizing.ts's
 * DeviceScale, and it's what lets a resize snap to a sharp size instead of
 * landing on a fractional one.
 */
export function largestIntegerScale(
  panel: { width: number; height: number },
  avail: { width: number; height: number },
  extra: Extra = { width: 0, height: 0 },
  maxScale = 8,
): number {
  if (panel.width <= 0 || panel.height <= 0) return 1;
  const w = Math.floor((avail.width - extra.width) / panel.width);
  const h = Math.floor((avail.height - extra.height) / panel.height);
  return Math.max(1, Math.min(maxScale, w, h));
}

/**
 * Snap a content size to an integer panel scale when it is already within
 * `tolerance` pixels of one, so a hand-dragged resize settles on a crisp
 * multiple without fighting the user who genuinely wants an odd size.
 * Returns null when nothing nearby is worth snapping to.
 */
export function snapToIntegerScale(
  content: { width: number; height: number },
  panel: { width: number; height: number },
  extra: Extra = { width: 0, height: 0 },
  tolerance = 6,
): { width: number; height: number; scale: number } | null {
  if (panel.width <= 0 || panel.height <= 0) return null;
  const panelW = content.width - extra.width;
  const exact = panelW / panel.width;
  const scale = Math.round(exact);
  if (scale < 1) return null;
  const snappedW = panel.width * scale;
  if (Math.abs(snappedW - panelW) > tolerance) return null;
  return {
    width: snappedW + extra.width,
    height: panel.height * scale + extra.height,
    scale,
  };
}
