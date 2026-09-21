// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useLayoutEffect, useRef } from 'react';
import type { EmulatorControls } from '../../hooks/useEmulator';
import { keyGroupsFor } from '../../lib/deviceKeys';

// The keys the case no longer shows.
//
// A screen-only window draws no bezel, so the Menu key, the SIBO ⊔ and ◆,
// the Organiser II command pad and the Series 5 recorder buttons have
// nowhere to live. This strip is that home. It is deliberately a
// discoverability aid rather than the only route: keymap.ts already binds
// most of these to host keys, and each chip's tooltip says which — so the
// strip teaches itself out of a job.
//
// When visible its height is reported to the parent, which adds it to the
// window's aspect `extra`. That makes showing the strip GROW the window
// rather than squash the emulated screen, which is what stops the picture
// jumping every time you glance at the keys.

interface Props {
  controls: EmulatorControls;
  visible: boolean;
  /** Reports the strip's rendered height so the window can grow by it. */
  onHeightChange(px: number): void;
}

export default function DeviceKeyStrip({ controls, visible, onHeightChange }: Props) {
  const { deviceInfo, currentDeviceId, pressEpocKey } = controls;
  const ref = useRef<HTMLDivElement>(null);

  const groups = keyGroupsFor({
    deviceId: currentDeviceId,
    deviceName: deviceInfo?.deviceName ?? null,
  });

  // Measured, not assumed: the chip row wraps on a narrow window, so its
  // height depends on the window width and the device's key count.
  useLayoutEffect(() => {
    if (!visible) { onHeightChange(0); return; }
    const el = ref.current;
    if (!el) { onHeightChange(0); return; }
    const report = () => onHeightChange(el.getBoundingClientRect().height);
    report();
    const ro = new ResizeObserver(report);
    ro.observe(el);
    return () => ro.disconnect();
  }, [visible, onHeightChange, groups.length, currentDeviceId]);

  if (!visible || groups.length === 0) return null;

  return (
    <div
      ref={ref}
      style={{
        position: 'fixed', bottom: 0, left: 0, right: 0, zIndex: 450,
        display: 'flex', flexWrap: 'wrap', alignItems: 'center',
        gap: 10, padding: '6px 10px',
        background: 'rgba(201, 201, 189, 0.96)',
        borderTop: '1px solid rgba(0,0,0,0.12)',
        // Must opt out: the modifier-drag overlay sits above everything, and
        // without this the strip's own buttons would be undraggable-but-dead
        // while the modifier is held.
        WebkitAppRegion: 'no-drag',
      } as React.CSSProperties}
    >
      {groups.map((group) => (
        <div key={group.id} style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
          <span
            style={{ fontSize: 10, opacity: 0.5, fontFamily: 'ui-monospace, monospace',
                     userSelect: 'none' }}
          >
            {group.label}
          </span>
          {group.keys.map((k) => (
            <button
              key={k.label}
              type="button"
              title={k.hostHint ? `${k.title} — or press ${k.hostHint}` : k.title}
              aria-label={k.title}
              onClick={() => pressEpocKey(k.code)}
              // The device must keep the keyboard: a focused chip would
              // swallow the next keystroke the user meant for EPOC.
              onMouseDown={(e) => e.preventDefault()}
              style={{
                padding: '3px 9px', borderRadius: 4,
                border: '1px solid rgba(0,0,0,0.25)',
                background: 'rgba(255,255,255,0.55)',
                font: '11px ui-monospace, monospace',
                cursor: 'pointer', userSelect: 'none', whiteSpace: 'nowrap',
              }}
            >
              {k.label}
            </button>
          ))}
        </div>
      ))}
    </div>
  );
}
