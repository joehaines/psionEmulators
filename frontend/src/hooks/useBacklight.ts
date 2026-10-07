// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useCallback, useEffect, useRef, useState } from 'react';
import type { EmulatorControls } from './useEmulator';
import { isHostTextEntry } from '../lib/keymap';
import {
  BACKLIGHT_FADE_MS, BACKLIGHT_SHORTCUT_LABEL, describeBacklightKey, elColourFor, isBacklightShortcut,
} from '../lib/backlight';

export interface BacklightState {
  // The running machine has a backlight its OS switches. Every control
  // (header button, control-bar button, host shortcut) hides when false.
  available: boolean;
  // The level is a brightness the key steps through (Series 7 / netBook),
  // not on/off. The core renders that brightness into the framebuffer, so
  // there is no tint or overlay for these.
  dimmable: boolean;
  // Live level, 0 (dark) to 100, as the machine's own output reports it.
  level: number;
  on: boolean;
  // The colour a lit EL panel glows; undefined unless the machine has an
  // EL panel (available && !dimmable). Used by the device-mode overlay and
  // the screenshot; the live canvas is tinted in the blit path.
  color: string | undefined;
  // Tooltip text naming the machine's own key and the host shortcut.
  hint: string;
  // Presses the machine's own backlight key, exactly as its keyboard would.
  toggle: () => void;
}

export const NO_BACKLIGHT: BacklightState = {
  available: false, dimmable: false, level: 0, on: false, color: undefined, hint: '', toggle: () => {},
};

// Drives the controls for the emulated LCD backlight. The machine decides
// everything — whether it has a backlight, which key switches it, and
// whether it is lit (getBacklightLevel reads the very output its OS
// drives, so its own key handling, auto-off timer and power-down all show
// through). This hook only presses that key and shows the result.
//
// `lightsOut` dims the rest of the page while an EL panel is lit, so the
// glow reads the way it does in a dark room. Off in embed mode, where the
// page around the machine is not ours to dim.
export function useBacklight(
  controls: EmulatorControls,
  { lightsOut = true }: { lightsOut?: boolean } = {},
): BacklightState {
  const {
    deviceInfo, currentDeviceId, getBacklightLevel, setBacklightTint,
    pressEpocKey, pressEpocChord, releaseHeldKeys,
  } = controls;
  const key = deviceInfo?.backlightKey ?? 0;
  const modifier = deviceInfo?.backlightModifier ?? 0;
  const available = !!deviceInfo?.hasBacklight && key > 0;
  const dimmable = available && !!deviceInfo?.backlightDimmable;
  const color = available && !dimmable ? elColourFor(currentDeviceId) : undefined;

  // Poll the live level. 10 Hz is well inside what a light switch needs;
  // in worker mode it reads a pushed status field, so it costs nothing.
  const [level, setLevel] = useState(0);
  useEffect(() => {
    setLevel(0);
    if (!available) return;
    const read = () => setLevel(getBacklightLevel());
    read();
    const id = window.setInterval(read, 100);
    return () => window.clearInterval(id);
  }, [available, getBacklightLevel, currentDeviceId]);

  const toggle = useCallback(() => {
    if (!available) return;
    if (modifier) pressEpocChord([modifier], key);
    else pressEpocKey(key);
  }, [available, modifier, key, pressEpocChord, pressEpocKey]);

  // Host shortcut (see isBacklightShortcut). Capture phase on window, so
  // it runs ahead of the emulator's own key handler and the L never
  // reaches the machine.
  //
  // The shortcut's own Ctrl and Shift went down to the machine on the way
  // to the L, and they matter: the Series 7 reads Ctrl+Fn+Space as
  // "dimmer" and Ctrl+Shift+Fn+Space as "full". So let go of everything
  // the host holds, then give the machine time to see the release before
  // pressing its key — the Series 7's keyboard driver takes ~0.1 s of
  // machine time to register a modifier going up (measured: a 20 ms gap
  // still dims, 120 ms steps up as it should). 250 ms covers that with
  // margin for a machine running slower than real time.
  useEffect(() => {
    if (!available) return;
    let pending = 0;
    const onKey = (e: KeyboardEvent) => {
      if (!isBacklightShortcut(e) || isHostTextEntry(e.target)) return;
      e.preventDefault();
      e.stopImmediatePropagation();
      if (e.type !== 'keydown' || e.repeat) return;
      releaseHeldKeys();
      window.clearTimeout(pending);
      pending = window.setTimeout(toggle, 250);
    };
    window.addEventListener('keydown', onKey, true);
    window.addEventListener('keyup', onKey, true);
    return () => {
      window.clearTimeout(pending);
      window.removeEventListener('keydown', onKey, true);
      window.removeEventListener('keyup', onKey, true);
    };
  }, [available, toggle, releaseHeldKeys]);

  const on = available && level > 0;

  // Everything that changes with the light changes from here, on the same
  // render and over the same BACKLIGHT_FADE_MS, so the switch reads as one
  // light coming up rather than parts of the page jumping at different
  // moments: the LCD (faded per frame in the blit path), the glow on the
  // case and the header bulb (both transition off `on` in this render), and
  // the page going "lights out".
  useEffect(() => {
    setBacklightTint(color ?? null, on);
  }, [color, on, setBacklightTint]);
  useEffect(() => () => setBacklightTint(null, false), [setBacklightTint]);

  // "Lights out" on the rest of the page while an EL panel is lit (CSS in
  // index.css dims the page chrome and the device skin photo). The page's
  // colours are plain utility classes with no transitions of their own,
  // so .psion-lights-fading lends every element one for the length of the
  // switch, then goes again so it can't slow anything else down.
  const fadeTimer = useRef(0);
  useEffect(() => {
    const want = on && !dimmable && lightsOut;
    const body = document.body;
    if (body.classList.contains('psion-lights-out') === want) return;
    body.classList.add('psion-lights-fading');
    body.classList.toggle('psion-lights-out', want);
    window.clearTimeout(fadeTimer.current);
    fadeTimer.current = window.setTimeout(
      () => body.classList.remove('psion-lights-fading'), BACKLIGHT_FADE_MS + 100);
  }, [on, dimmable, lightsOut]);
  useEffect(() => () => {
    window.clearTimeout(fadeTimer.current);
    document.body.classList.remove('psion-lights-out', 'psion-lights-fading');
  }, []);

  const own = available ? describeBacklightKey(modifier, key) : '';
  const hint = available ? `${own} on the machine, or ${BACKLIGHT_SHORTCUT_LABEL}` : '';

  if (!available) return NO_BACKLIGHT;
  return { available, dimmable, level, on, color, hint, toggle };
}
