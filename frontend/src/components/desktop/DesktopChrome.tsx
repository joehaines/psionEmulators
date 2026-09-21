// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useEffect, useRef, useState } from 'react';
import { requireHost } from '../../lib/desktop/host';

// The only chrome a borderless window gets: a strip that appears when you
// reach for the top edge, and a way to drag the window.
//
// ── Why the drag handling is this careful ──
//
// `-webkit-app-region: drag` hands its region to the window manager, and
// Chromium then delivers NO pointer events to the page inside it — not
// pointerdown, not anything. Over a machine with a digitiser (every ARM
// Psion) a permanently-dragging strip would silently eat stylus taps that
// landed in it, and "the pen stopped working near the top of the screen" is
// a bug that is easy to ship and miserable to diagnose.
//
// So there are two layers, and neither is armed while you are using the
// device:
//
//   1. Hold the platform's window modifier (Alt on Windows, Cmd on macOS)
//      and the whole window becomes draggable. Release and it stops.
//   2. Bring the pointer within a few pixels of the top edge and a thin bar
//      fades in, carrying the window buttons and a drag handle.
//
// The proximity check is a pointermove listener rather than CSS :hover. A
// stylus press that lands on the LCD's first row would satisfy :hover on a
// strip overlapping it; requiring the pointer to actually be near the edge
// does not.

/** How close to the top edge the pointer must come to reveal the strip. */
const REVEAL_WITHIN_PX = 6;
/** Strip height. Kept under the smallest bezel so it never covers the LCD. */
const STRIP_HEIGHT_PX = 24;
/** Grace period before the strip hides again, so it can be moved onto. */
const HIDE_DELAY_MS = 600;

interface Props {
  deviceName: string | null;
  /** Opens the device switcher overlay. */
  onOpenSwitcher(): void;
  /** Opens the drives panel. Absent when the host cannot mount folders. */
  onOpenDrives?(): void;
  /** A short note about the shared card, when there is something to say. */
  driveSummary?: string | null;
  /** The drive sync's one-line status. */
  syncSummary?: string | null;
  /** True when the sync needs the user to do something on the device. */
  syncActionable?: boolean;
  /** Shows or hides the device key strip. */
  onToggleKeys(): void;
  keysVisible: boolean;
  /** Set while a blocking operation (the final save) is running. */
  busy: string | null;
}

export default function DesktopChrome({
  deviceName, onOpenSwitcher, onOpenDrives, driveSummary, syncSummary,
  syncActionable, onToggleKeys, keysVisible, busy,
}: Props) {
  const host = requireHost();
  const isMac = host.info.platform === 'darwin';

  const [stripVisible, setStripVisible] = useState(false);
  const [dragArmed, setDragArmed] = useState(false);
  const hideTimer = useRef<number | null>(null);

  // ── Modifier-held whole-window drag ────────────────────────────────────
  // Tracked on keydown/keyup rather than from a pointer event's modifier
  // flags, so the region is already armed by the time the user presses the
  // button — Chromium only re-reads drag regions on layout, not per event.
  useEffect(() => {
    const held = (e: KeyboardEvent) => (isMac ? e.metaKey : e.altKey);
    const onKeyDown = (e: KeyboardEvent) => { if (held(e)) setDragArmed(true); };
    const onKeyUp = (e: KeyboardEvent) => { if (!held(e)) setDragArmed(false); };
    // Losing focus mid-chord would otherwise leave the window permanently
    // draggable, which swallows every subsequent tap.
    const disarm = () => setDragArmed(false);
    window.addEventListener('keydown', onKeyDown);
    window.addEventListener('keyup', onKeyUp);
    window.addEventListener('blur', disarm);
    return () => {
      window.removeEventListener('keydown', onKeyDown);
      window.removeEventListener('keyup', onKeyUp);
      window.removeEventListener('blur', disarm);
    };
  }, [isMac]);

  // ── Edge proximity reveals the strip ──────────────────────────────────
  useEffect(() => {
    const onMove = (e: PointerEvent) => {
      // Only a hovering pointer counts. A pressed stylus dragging near the
      // top of the screen is drawing, not reaching for the window controls.
      if (e.pressure > 0 || e.buttons !== 0) return;
      const near = e.clientY <= REVEAL_WITHIN_PX;
      if (near) {
        if (hideTimer.current) { clearTimeout(hideTimer.current); hideTimer.current = null; }
        setStripVisible(true);
      } else if (e.clientY > STRIP_HEIGHT_PX && !hideTimer.current) {
        hideTimer.current = window.setTimeout(() => {
          hideTimer.current = null;
          setStripVisible(false);
        }, HIDE_DELAY_MS);
      }
    };
    window.addEventListener('pointermove', onMove);
    return () => {
      window.removeEventListener('pointermove', onMove);
      if (hideTimer.current) clearTimeout(hideTimer.current);
    };
  }, []);

  // The OS default on a drag region is maximise (Windows) / zoom (macOS),
  // both of which fight the aspect lock. Handle it ourselves instead.
  const onStripDoubleClick = (e: React.MouseEvent) => {
    e.preventDefault();
    e.stopPropagation();
    host.window.toggleMaximize();
  };

  const btn = 'px-2 h-full text-[11px] font-mono leading-none text-psion-charcoal/70 '
            + 'hover:text-psion-charcoal hover:bg-black/10 transition-colors';

  return (
    <>
      {/* Modifier-held drag surface. `display` is toggled rather than just
          the app-region property: Chromium caches drag regions per layout,
          and flipping a property on a still-present element does not always
          make it recompute them. */}
      <div
        aria-hidden
        onDoubleClick={(e) => e.preventDefault()}
        style={{
          position: 'fixed', inset: 0, zIndex: 400,
          display: dragArmed ? 'block' : 'none',
          WebkitAppRegion: 'drag',
          cursor: 'grab',
        } as React.CSSProperties}
      />

      {/* The reach-for-it strip. */}
      <div
        onDoubleClick={onStripDoubleClick}
        style={{
          position: 'fixed', top: 0, left: 0, right: 0, height: STRIP_HEIGHT_PX,
          zIndex: 500,
          display: 'flex', alignItems: 'center', gap: 4,
          background: 'rgba(201, 201, 189, 0.92)',
          borderBottom: '1px solid rgba(0,0,0,0.12)',
          opacity: stripVisible ? 1 : 0,
          pointerEvents: stripVisible ? 'auto' : 'none',
          transition: 'opacity 140ms ease',
          WebkitAppRegion: 'drag',
        } as React.CSSProperties}
      >
        {/* Everything interactive must opt OUT of the drag region, or the
            window manager takes the click before the button sees it. */}
        <div
          style={{ display: 'flex', alignItems: 'center', gap: 2, height: '100%',
                   WebkitAppRegion: 'no-drag' } as React.CSSProperties}
        >
          <button type="button" className={btn} onClick={onOpenSwitcher}
                  title="Switch device (Ctrl/Cmd+K)">⋯ Device</button>
          <button type="button" className={btn} onClick={onToggleKeys}
                  title="Show device keys (F1)">
            {keysVisible ? '▾ Keys' : '▸ Keys'}
          </button>
          {onOpenDrives && (
            <button type="button" className={btn} onClick={onOpenDrives}
                    title="Shared card folder (Ctrl/Cmd+D)">
              ▤ Card{driveSummary ? ` · ${driveSummary}` : ''}
            </button>
          )}
        </div>

        <span
          style={{ flex: 1, textAlign: 'center', fontSize: 11, opacity: 0.65,
                   fontFamily: 'ui-monospace, monospace', userSelect: 'none' }}
        >
          {deviceName ?? 'Psion'}
          {/* The sync's own words — "synced 40s ago", never "mounted". At under
              a KB/s it is not a drive and must not read like one. Something the
              user has to act on gets highlighted rather than blending in. */}
          {syncSummary && (
            <span style={{
              marginLeft: 10,
              opacity: syncActionable ? 1 : 0.7,
              color: syncActionable ? '#8a3a1a' : undefined,
              fontWeight: syncActionable ? 600 : undefined,
            }}>
              · {syncSummary}
            </span>
          )}
        </span>

        <div
          style={{ display: 'flex', alignItems: 'center', gap: 2, height: '100%',
                   WebkitAppRegion: 'no-drag' } as React.CSSProperties}
        >
          <button type="button" className={btn} title="Minimise"
                  onClick={() => host.window.minimize()}>–</button>
          <button type="button" className={btn} title="Maximise"
                  onClick={() => host.window.toggleMaximize()}>▢</button>
          <button
            type="button"
            className={btn + ' hover:!bg-red-500 hover:!text-white'}
            title="Close"
            onClick={() => host.window.close()}
          >✕</button>
        </div>
      </div>

      {/* Blocking overlay while the final save runs, so the window does not
          look frozen when quit is deliberately waiting on it. */}
      {busy && (
        <div
          style={{
            position: 'fixed', inset: 0, zIndex: 600,
            display: 'flex', alignItems: 'center', justifyContent: 'center',
            background: 'rgba(0,0,0,0.55)', color: 'white',
            font: '13px ui-monospace, monospace', userSelect: 'none',
          }}
        >
          {busy}
        </div>
      )}
    </>
  );
}
