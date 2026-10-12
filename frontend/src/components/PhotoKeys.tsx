// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useEffect, useRef, useState } from 'react';
import type { PhotoKeyLayout } from '../lib/photoKeyLayouts';

// The keys on a device-skin photo, made pressable.
//
// One invisible zone per key, placed from lib/photoKeyLayouts.ts (measured
// off the photo by tools/photo-keys/build.py), lit while it is down. Mouse,
// pen and touch all arrive as pointer events, and each zone follows the
// press / release contract the machine's keyboard scanner expects: the key
// goes down with the pointer and comes up with it, so holding a key on the
// photo auto-repeats exactly as holding the real one does.
//
// Modifiers (Shift, Ctrl, Fn, Psion) behave like a phone keyboard's Shift:
// a tap holds the modifier for the next key, which is the only way to
// chord on a single-pointer mouse. A second tap lets it go. Held down
// while another finger presses a key, a modifier is simply a held key and
// comes up when lifted.

// Shortest a key is held down in the machine. EPOC scans the keyboard
// matrix on a timer, so a click whose up follows its down within the same
// millisecond is never seen. Same figure as the SIBO app-button zones and
// the 3D view's keys.
const MIN_KEY_HOLD_MS = 120;

interface Props {
  layout: PhotoKeyLayout;
  /** CSS size of the photo as drawn: the zones scale with it. */
  width: number;
  height: number;
  sendKey(code: number, down: boolean): void;
}

interface Held {
  pointerId: number;
  at: number;
  /** A modifier that another key was pressed under: lifting it releases
   *  it rather than latching it. */
  used: boolean;
}

export default function PhotoKeys({ layout, width, height, sendKey }: Props) {
  const wrapRef = useRef<HTMLDivElement>(null);
  const sendRef = useRef(sendKey);
  sendRef.current = sendKey;

  // Keys currently lit — held by a pointer or latched — by index.
  const [lit, setLit] = useState<ReadonlySet<number>>(() => new Set());
  const held = useRef(new Map<number, Held>());
  const latched = useRef(new Map<number, number>());     // index → down time
  const pendingUp = useRef(new Map<number, number>());   // index → timer id

  const refresh = () => setLit(new Set([...held.current.keys(), ...latched.current.keys()]));

  const up = (i: number) => {
    pendingUp.current.delete(i);
    sendRef.current(layout.keys[i].code, false);
  };
  // Lets a key go, but not sooner than MIN_KEY_HOLD_MS after it went down.
  const release = (i: number, downAt: number) => {
    const wait = MIN_KEY_HOLD_MS - (performance.now() - downAt);
    if (wait <= 0) { up(i); return; }
    pendingUp.current.set(i, window.setTimeout(() => up(i), wait));
  };
  // A key pressed again before its last release went out: send that
  // release now, so the new press is a fresh edge the scanner can see.
  const flushPending = (i: number) => {
    const t = pendingUp.current.get(i);
    if (t === undefined) return;
    window.clearTimeout(t);
    up(i);
  };

  // Everything still down goes up when the photo goes away (device switch,
  // lid shut, device mode off) — no pointerup is coming for it.
  useEffect(() => {
    const h = held.current, l = latched.current, p = pendingUp.current;
    return () => {
      for (const t of p.values()) window.clearTimeout(t);
      const codes = new Set([...h.keys(), ...l.keys(), ...p.keys()]);
      for (const i of codes) sendRef.current(layout.keys[i].code, false);
      h.clear(); l.clear(); p.clear();
    };
  }, [layout]);

  // While the on-screen keyboard's hidden textarea has focus (a phone with
  // its soft keyboard up), a tap on a key must not blur it — iOS would
  // dismiss the keyboard. Only a non-passive touchstart listener can stop
  // that, which React's onTouchStart is not.
  useEffect(() => {
    const el = wrapRef.current;
    if (!el) return;
    const onTouchStart = (e: TouchEvent) => {
      if ((document.activeElement as HTMLElement | null)?.dataset?.psionInput) e.preventDefault();
    };
    el.addEventListener('touchstart', onTouchStart, { passive: false });
    return () => el.removeEventListener('touchstart', onTouchStart);
  }, []);

  const onDown = (i: number, e: React.PointerEvent<HTMLButtonElement>) => {
    if (e.pointerType === 'mouse' && e.button !== 0) return;
    // Keeps focus where it was, so the next host keystroke still reaches
    // the machine.
    e.preventDefault();
    const key = layout.keys[i];
    const wasLatched = latched.current.get(i);
    if (wasLatched !== undefined) {
      // Second tap on a latched modifier lets it go.
      latched.current.delete(i);
      release(i, wasLatched);
      refresh();
      return;
    }
    if (held.current.has(i)) return;            // a second finger on the same key
    e.currentTarget.setPointerCapture(e.pointerId);
    flushPending(i);
    if (!key.modifier) {
      for (const [j, h] of held.current) if (layout.keys[j].modifier) h.used = true;
    }
    sendRef.current(key.code, true);
    held.current.set(i, { pointerId: e.pointerId, at: performance.now(), used: false });
    refresh();
  };

  const onUp = (i: number, e: React.PointerEvent<HTMLButtonElement>, cancelled: boolean) => {
    const h = held.current.get(i);
    if (!h || h.pointerId !== e.pointerId) return;
    held.current.delete(i);
    const key = layout.keys[i];
    if (key.modifier && !h.used && !cancelled) {
      // Tapped: hold it for the next key.
      latched.current.set(i, h.at);
      refresh();
      return;
    }
    release(i, h.at);
    if (!key.modifier) {
      // Latched modifiers go up with the key they modified, never before
      // it, or the machine would see that key unmodified.
      for (const j of latched.current.keys()) release(j, h.at);
      latched.current.clear();
    }
    refresh();
  };

  const sx = width / layout.width;
  const sy = height / layout.height;

  return (
    <div ref={wrapRef} className="absolute inset-0" style={{ pointerEvents: 'none', zIndex: 25 }}>
      {layout.keys.map((k, i) => {
        const [x0, y0, x1, y1] = k.rect;
        return (
          <button
            key={i}
            type="button"
            // Sixty invisible buttons have no place in the tab order: the
            // host keyboard already reaches every one of these keys.
            tabIndex={-1}
            title={k.modifier ? `${k.label} — tap to hold it for the next key` : k.label}
            aria-label={`${k.label} key`}
            aria-pressed={k.modifier ? lit.has(i) : undefined}
            className={[
              'absolute select-none rounded transition-opacity',
              lit.has(i)
                ? 'opacity-40 bg-psion-highlight'
                : 'opacity-0 hover:opacity-10 hover:bg-white',
            ].join(' ')}
            style={{
              left:   x0 * sx,
              top:    y0 * sy,
              width:  (x1 - x0) * sx,
              height: (y1 - y0) * sy,
              pointerEvents: 'auto',
              touchAction: 'none',
              cursor: 'pointer',
            }}
            onPointerDown={e => onDown(i, e)}
            onPointerUp={e => onUp(i, e, false)}
            onPointerCancel={e => onUp(i, e, true)}
            onLostPointerCapture={e => onUp(i, e, true)}
            onContextMenu={e => e.preventDefault()}
          />
        );
      })}
    </div>
  );
}
