// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useCallback, useEffect, useState } from 'react';
import type { EmulatorControls } from './useEmulator';

// Devices with an EL-panel backlight, keyed by the colour of the lit panel.
// The Series 5 / 5mx / 5mx Pro all use the same pale blue-green "indiglo"
// EL (Timex-watch family); the 3mx is a more saturated yellow-green. Values
// were eyeballed from reference photos of each lit screen; they are the
// colour the LCD's *unlit background* takes on while the backlight is on.
const BACKLIGHT_COLORS: Record<string, string> = {
  '5mx':       '#a8d8c4',
  '5mxpro':    '#a8d8c4',
  'series5':   '#a8d8c4',
  'series3mx': '#bcd47a',
};

export interface BacklightState {
  // The backlight colour for the running device, or undefined when the
  // device doesn't have a backlit panel — drives both the overlay tint
  // and the visibility of the header toggle button.
  color: string | undefined;
  // Whether the simulated backlight is currently on. Driven by the live
  // PRT pin on Windermere (so EPOC's own auto-off timeout is reflected
  // automatically); driven by a local toggle on the 3mx because its port
  // pin isn't wired through ASIC9 in our core.
  on: boolean;
  // Toggles the backlight. Sends the same EPOC chord a real keyboard
  // would (Fn+Space on Windermere, Psion+Space on SIBO) so the kernel
  // sees the press exactly as it would on hardware.
  toggle: () => void;
}

// Drives the simulated EL backlight. Lifted out of EmulatorView so the
// page header (App.tsx) can render its own always-visible toggle for
// mobile users whose soft keyboard might hide the in-view control bar.
//
// Gated by the side-panel "Experimental features" flag — the feature is
// purely cosmetic and the 3mx fallback in particular is a known partial
// (no auto-off), so it stays opt-in alongside the simulated Modem.
// `enabled` false collapses the hook to a no-op so every entry point
// (header button, control-bar button, overlay, key listener, body
// class) folds away from a single source of truth.
export function useBacklight(
  controls: EmulatorControls,
  enabled: boolean,
): BacklightState {
  const { currentDeviceId, pressEpocChord, getBacklight } = controls;
  const color = enabled && currentDeviceId ? BACKLIGHT_COLORS[currentDeviceId] : undefined;
  const isSibo = currentDeviceId === 'series3mx';

  // Two sources of truth, picked at read time. Windermere has an
  // observable PRT pin (windermere.h:getBacklight); the 3mx falls back
  // to a local JS toggle because MAME hasn't wired its port pin either
  // (reference/psion3a.cpp:12 still lists it as TODO).
  const [hwOn, setHwOn] = useState(false);
  const [jsOn, setJsOn] = useState(false);
  // Force off whenever the device doesn't qualify or the experimental
  // flag is cleared — otherwise a stale hwOn/jsOn from before the flag
  // flipped would keep the overlay drawn after the entry points are
  // hidden.
  const on = !!color && (isSibo ? jsOn : hwOn);

  // Reset both sources when the user switches devices — otherwise leaving
  // a backlit device with the light on would land on the next device
  // with the page still dimmed.
  useEffect(() => { setHwOn(false); setJsOn(false); }, [currentDeviceId]);

  // Poll the live backlight pin. 10 Hz is well under the perceptual
  // threshold for a light-switch press and avoids piggy-backing on the
  // LCD render loop's hot path. Skipped on devices without a modelled
  // pin (3mx falls through to the JS toggle below).
  useEffect(() => {
    if (!color || isSibo) return;
    const id = window.setInterval(() => setHwOn(getBacklight()), 100);
    return () => window.clearInterval(id);
  }, [color, isSibo, getBacklight]);

  // Host Psion+Space (3mx only). Windermere is covered by polling — the
  // user's Fn+Space already flows through useEmulator's handleKeyDown
  // to the EPOC kernel, which flips the PRT bit we observe. SIBO has no
  // observable hardware path, so we keep a local listener for it. Gated
  // on `color` so a user with the experimental flag off doesn't get the
  // overlay flipped on by an accidental Alt+Space.
  useEffect(() => {
    if (!isSibo || !color) return;
    const handler = (e: KeyboardEvent) => {
      if (e.code !== 'Space' || !e.altKey) return;
      setJsOn(v => !v);
    };
    window.addEventListener('keydown', handler);
    return () => window.removeEventListener('keydown', handler);
  }, [isSibo, color]);

  // Toggle entry point for on-screen buttons (in-view + header icon).
  // Sends the real keyboard chord so the kernel handles the press; for
  // SIBO we also flip the local JS state directly.
  const toggle = useCallback(() => {
    if (!color) return;
    // Fn = 24 (EStdKeyLeftFunc) on Windermere; Psion = 21
    // (EStdKeyRightAlt) on SIBO. Space = 5 (EStdKeySpace) on both.
    const modifier = isSibo ? 21 : 24;
    pressEpocChord([modifier], 5);
    if (isSibo) setJsOn(v => !v);
  }, [color, isSibo, pressEpocChord]);

  // "Turn the lights out" on the rest of the page while the backlight is
  // on, so the glow stands out against a dim room. CSS rules in
  // index.css darken the page chrome and dim the device skin photo when
  // this class is present on <body>.
  useEffect(() => {
    if (!on) return;
    document.body.classList.add('psion-lights-out');
    return () => document.body.classList.remove('psion-lights-out');
  }, [on]);

  return { color, on, toggle };
}
