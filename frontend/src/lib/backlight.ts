// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// LCD backlight: the presentation half.
//
// Whether a machine has a backlight, the key its keyboard switches it
// with, and whether it is lit right now all come from the emulated
// hardware (EmuBase's backlight section in core/emubase.h, exposed through
// getDeviceInfo and getBacklightLevel). Nothing here decides any of that.
// What lives here is how a lit panel looks, and the host shortcut that
// reaches the machine's own key.
//
// Two kinds of panel:
//   • EL (electroluminescent) behind a reflective mono LCD — every backlit
//     Psion bar the Series 7 / netBook. Lit, the "paper" glows the EL
//     colour and the ink stays dark. The core renders the panel as it
//     looks unlit; tintBacklitPixels turns that into the lit look.
//   • A lamp behind the Series 7 / netBook's colour panel, whose level is
//     a brightness. The core already renders that brightness into the
//     framebuffer (it is the only light the panel has), so the frontend
//     only reports it.

/** The colour an EL panel's unlit background takes on while lit, as
 *  eyeballed from photos of each machine lit. The Series 5 family's EL is
 *  the pale blue-green "indiglo" of the period's watches; the SIBO
 *  machines' a yellower green; the Conan's a clear blue. Machines not
 *  listed take the default. */
const EL_COLOURS: Record<string, string> = {
  series5:     '#a8d8c4',
  '5mx':       '#a8d8c4',
  '5mxpro':    '#a8d8c4',
  mc218:       '#a8d8c4',
  osaris:      '#a8d8c4',
  geofox:      '#a8d8c4',
  conan:       '#8cc4f2',
  conanv001:   '#8cc4f2',
  series3mx:   '#bcd47a',
  workabout:   '#bcd47a',
  workaboutmx: '#bcd47a',
  hc120:       '#bcd47a',
};
const DEFAULT_EL_COLOUR = '#a8d8c4';

export function elColourFor(deviceId: string | null | undefined): string {
  return (deviceId && EL_COLOURS[deviceId]) || DEFAULT_EL_COLOUR;
}

export type Rgb = readonly [number, number, number];

export function parseHexColour(hex: string): Rgb {
  const v = parseInt(hex.replace('#', ''), 16);
  return [(v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff];
}

/** The colour the darkest ink settles at on a lit panel: the EL light
 *  leaks a little through even fully driven pixels. */
const LIT_INK: Rgb = [0x1c, 0x24, 0x22];

/**
 * Per-luminance lookup for a lit EL panel: 256 RGB triples, from ink
 * (luminance 0) to the EL colour (the brightest paper). Build once per
 * colour and pass to tintBacklitPixels.
 */
export function buildBacklightLut(el: Rgb): Uint8Array {
  const lut = new Uint8Array(256 * 3);
  for (let l = 0; l < 256; l++) {
    // The cores render an unlit panel's paper as a light grey-green
    // (luminance ~161 on the EPOC32 panels, ~172 on the SIBO ones) and its
    // ink above black (~40 on the SIBO panels), so stretch that span onto
    // ink..EL: paper lands on the full EL colour and ink stays dark.
    const t = Math.min(1, Math.max(0, (l - 36) / (158 - 36)));
    for (let c = 0; c < 3; c++) lut[l * 3 + c] = Math.round(LIT_INK[c] + (el[c] - LIT_INK[c]) * t);
  }
  return lut;
}

/** Re-light an RGBA frame in place: each pixel's luminance picks its lit
 *  colour from the lookup. Alpha is left alone. */
export function tintBacklitPixels(buf: Uint8ClampedArray, lut: Uint8Array): void {
  for (let i = 0; i < buf.length; i += 4) {
    const l = ((buf[i] * 77 + buf[i + 1] * 150 + buf[i + 2] * 29) >> 8) * 3;
    buf[i]     = lut[l];
    buf[i + 1] = lut[l + 1];
    buf[i + 2] = lut[l + 2];
  }
}

/** How long the page takes to go "lights out" (and come back) when the
 *  backlight switches — the page chrome, the skin photo and the header
 *  bulb. The machine's own light — the LCD and the glow on the case —
 *  switches at once, as an EL panel does. (index.css's
 *  .psion-lights-fading carries the same figure.) */
export const BACKLIGHT_FADE_MS = 400;

/**
 * The host shortcut for the backlight: Ctrl+Shift+L, on every machine.
 *
 * Most backlit Psions switch theirs with Fn+Space, and the host key for
 * Fn is Meta — but Cmd+Space (Spotlight on macOS) and Win/Super+Space
 * (input-source switching on Windows and most Linux desktops) never reach
 * the page. Ctrl and Shift alone do nothing on any of the machines, so
 * holding them on the way to L sends the guest nothing it acts on. The
 * machines' own chords still work wherever the host passes them through:
 * Meta+Space for Fn+Space, Alt+Space for the 3mx's Psion+Space.
 */
export function isBacklightShortcut(e: Pick<KeyboardEvent, 'code' | 'ctrlKey' | 'shiftKey' | 'altKey' | 'metaKey'>): boolean {
  return e.code === 'KeyL' && e.ctrlKey && e.shiftKey && !e.altKey && !e.metaKey;
}
export const BACKLIGHT_SHORTCUT_LABEL = 'Ctrl+Shift+L';

const KEY_NAMES: Record<number, string> = {
  5: 'Space',
  20: 'Psion',
  21: 'Psion',
  24: 'Fn',
  151: 'Backlight key',
};

/** "Fn+Space", "Psion+Space", "Backlight key" — the machine's own key,
 *  for tooltips. */
export function describeBacklightKey(modifier: number, key: number): string {
  const k = KEY_NAMES[key] ?? `key ${key}`;
  return modifier ? `${KEY_NAMES[modifier] ?? `key ${modifier}`}+${k}` : k;
}
