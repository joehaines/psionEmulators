// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useEffect, useRef, useState, useCallback } from 'react';
import type { EmulatorControls } from '../hooks/useEmulator';
import type { BacklightState } from '../hooks/useBacklight';
import CFCardDialog from './CFCardDialog';
import SSDDialog from './SSDDialog';
import DatapakDialog from './DatapakDialog';
import RemoteLinkDialog from './RemoteLinkDialog';
import ModemDialog from './ModemDialog';
import InfraredDialog from './InfraredDialog';
import PrinterDialog from './PrinterDialog';

export type SizingMode = 'device' | 'fill';
// Integer zoom factor for the 'device' (pixel-perfect) mode — 1 device
// pixel maps to N physical screen pixels. Selected via the hover flyout
// on the actual-size header button; persists across reloads and across
// device switches.
export type DeviceScale = 1 | 2 | 3 | 4;

interface Props {
  controls: EmulatorControls;
  sizingMode: SizingMode;
  // Pixel-perfect zoom factor applied when sizingMode === 'device'.
  // Ignored by 'fill' and fullscreen; kept in state so toggling sizing
  // mode preserves the user's last choice. Range enforced by the type.
  deviceScale: DeviceScale;
  // Bumped by the parent every time the user clicks the Fullscreen icon
  // in the header. EmulatorView watches this counter to enter fullscreen
  // (native API where available, CSS overlay fallback on iOS Safari).
  fullscreenRequest: number;
  // Settings checkboxes from the side panel.
  // `showDebugging` reveals the Logs + Download RAM controls.
  // `experimentalFeatures` reveals the simulated Modem.
  showDebugging: boolean;
  experimentalFeatures: boolean;
  lcdAccuracyMode?: boolean;
  // Live EL-backlight state, owned by App.tsx (see useBacklight) so the
  // header can render its own always-visible toggle in parallel with
  // the in-view control-bar button this component still draws.
  backlight: BacklightState;
  // Standalone-iframe (`#/embed/<id>`) mode. When true the component
  // renders ONLY the device frame + LCD canvas + touch overlay — no
  // control bar, no debug bar, no inline dialogs, no log panel, no
  // on-screen key helper rows. The outer wrapper also drops the
  // `minHeight: calc(100vh - 49px)` reservation for the page header
  // (which doesn't exist in the embed route) so 'fill' mode can use the
  // full viewport height. Defaults to false so the main app is
  // unaffected.
  chromeless?: boolean;
  // When true, render photo-realistic device images from the `device-skins/`
  // public folder instead of the minimal SVG/plain-LCD view. Each skin image
  // has its own layout entry in DEVICE_SKIN_LAYOUTS that positions the LCD
  // canvas and (for SIBO devices) the button-bar click zones correctly
  // within the photo.
  deviceMode?: boolean;
}

// Per-skin layout: aspect ratio of the full SVG and where the LCD screen
// (and, optionally, the touch-active digitiser pad) sits within it.
// All "screen*" / "digitiser*" values are SVG-fraction coordinates so the
// layout stays correct regardless of how big the container ends up being.
interface SkinLayout {
  aspectRatio: number;   // SVG width / SVG height
  screenLeft: number;    // fraction of SVG width
  screenTop: number;     // fraction of SVG height
  screenWidth: number;   // fraction of SVG width
  screenHeight: number;  // fraction of SVG height
  // Optional digitiser pad placement within the SVG. Lets a skin draw a
  // device body that surrounds the digitiser pad on every side (Revo) —
  // without this override the touch overlay would extend across the full
  // SVG width and start at SVG y=0, which only matches skins where the
  // digitiser is anchored at the top-left of the artwork (5mx).
  // Defaults: pad starts at (0, 0), pad width = full SVG width, pad height
  // = full SVG width × (digitiserHeight/digitiserWidth) as before.
  digitiserPadLeft?:   number;  // fraction of SVG width
  digitiserPadTop?:    number;  // fraction of SVG height
  digitiserPadWidth?:  number;  // fraction of SVG width
  digitiserPadHeight?: number;  // fraction of SVG height
  // Optional digitiser-coordinate-system override. Used when the WASM
  // module hasn't reported, or shouldn't be trusted for, the digitiser
  // size — the touch overlay uses these to map cssX/cssY to digitiser
  // coordinates instead of deviceInfo's values.
  digitiserWidth?:  number;
  digitiserHeight?: number;
  // Opt-in: use LCD-anchored mapping (Series 7 / netBook style) where the
  // touch overlay's LCD-region maps to LCD-space coords (0..digitiserWidth)
  // and silkscreen taps either side of the LCD map to negative / >width
  // coords for the backend's centering inset to handle.  Default OFF —
  // most skins (5mx, Revo, Osaris, …) use the simpler pad-rect-linear
  // mapping where the whole pad CSS rect maps linearly to digitiser space.
  lcdAnchoredTouchMapping?: boolean;
  // SIBO device-skin button bar. In device mode these replace the separate
  // siboBarImage strip that normally sits below the LCD frame. The values
  // are fractions of the full image height; the bar is divided evenly into
  // N equal-width columns, one per button.
  buttonBarTop?:    number;  // fraction of image height where button row starts
  buttonBarHeight?: number;  // fraction of image height the button row occupies
  buttonBarLeft?:   number;  // fraction of image width, left edge of button row
  buttonBarWidth?:  number;  // fraction of image width the button row spans
}

// 5mx SVG: viewBox="0 0 2895.83 1270.83"
// Screen cutout: x=187.5, y=20.83, w=2666.67, h=1000
const SKIN_LAYOUTS: Record<string, SkinLayout> = {
  '5mx.svg': {
    aspectRatio:  2895.83 / 1270.83,
    screenLeft:   187.5   / 2895.83,
    screenTop:    20.83   / 1270.83,
    screenWidth:  2666.67 / 2895.83,
    screenHeight: 1000    / 1270.83,
  },
  // Revo skin: the SVG renders only the 527 × 208 digitiser pad — no
  // surrounding device body. The viewBox is in digitiser pixels so the
  // touch overlay maps cssX/cssY straight onto the EPOC pen driver's
  // expected coordinates. When this skin is replaced with artwork that
  // includes the case, the SVG viewBox AND the digitiserPad{Left,Top,
  // Width,Height} fractions below need to be updated together so the
  // touch overlay still covers exactly the 527 × 208 pad sub-rectangle.
  'revo.svg': {
    aspectRatio:           527 / 208,
    screenLeft:             47 / 527,
    screenTop:               0 / 208,
    screenWidth:           480 / 527,
    screenHeight:          160 / 208,
    digitiserPadLeft:        0,
    digitiserPadTop:         0,
    digitiserPadWidth:       1,
    digitiserPadHeight:      1,
    digitiserWidth:        527,
    digitiserHeight:       208,
  },
  // Oregon Scientific Osaris photo skin (3326×1248). The full image is the
  // 440×200 digitiser face: left silkscreen (WORD/SHEET/DATA/AGENDA/EXTRAS),
  // central 320×200 LCD, and right silkscreen (EPOC/MAIN MENU/COPY·PASTE/
  // ZOOM IN/ZOOM OUT). Touch overlay covers the whole image so silkscreen
  // buttons register correctly via digitiser coordinates.
  // screenLeft/Width measured by pixel-sampling the JPEG: the dark separator
  // line sits at x=479–491; screen colour starts at x=492 and ends at x=2892
  // (width 2400 px out of 3326 total).
  'OregonScientific_Osaris.jpg': {
    aspectRatio:           3326 / 1248,
    screenLeft:             492 / 3326,
    screenTop:               0,
    screenWidth:           2400 / 3326,
    screenHeight:            1,
    digitiserPadLeft:        0,
    digitiserPadTop:         0,
    digitiserPadWidth:       1,
    digitiserPadHeight:      1,
    digitiserWidth:        440,
    digitiserHeight:       200,
  },
  // Psion Series 7 / netBook shared skin (1386×896 JPEG). The two devices
  // look identical bar a paint colour, so both use the same image. The
  // dark screen-surround in the artwork runs x=101..1287, y=0..895 — a
  // 1187×896 rectangle slightly off 4:3 (1.324:1). Real hardware has a
  // small consistent black bezel around the LCD, so we sit the 640×480
  // canvas inside the dark area with a 12-px margin on all sides. That
  // gives a 1163×872 LCD-rectangle (≈4:3, 1.334:1, no perceptible stretch).
  // The Series 7 / netBook digitiser EXTENDS beyond the LCD: the
  // silkscreen icon columns on either side of the LCD (Cut/Print/Zoom/
  // 1-4 on the left, World/Calc/Print/Sheet/Time/Word/Notes/Folders
  // on the right) are physically part of the same touch sensor.  The
  // pad extends to the full skin width so the crosshair-cursor reaches
  // them and taps register.
  //
  // Coordinate mapping is LCD-anchored:
  //   skin-X 113 (LCD left)  → touchX = 0          (LCD-coord 0)
  //   skin-X 1276 (LCD right) → touchX = 640       (LCD-coord 640)
  //   skin-X 0 (skin left)   → touchX = ~-62       (negative = left silkscreen)
  //   skin-X 1386 (skin right) → touchX = ~700     (>640 = right silkscreen)
  // The WASM-side touchAdcXY (which knows about the +71 px silkscreen
  // inset) adds the inset uniformly so all coords land in the right
  // ADC slice.
  //
  // The frontend computes touchX from cssX using the LCD bounds within
  // the pad, NOT the pad bounds — see toDigitiserCoords below for the
  // mapping logic.  digitiserWidth/Height = LCD size so the LCD bounds
  // arithmetic stays simple.
  '7_netbook.jpeg': {
    aspectRatio:        1386 / 896,
    screenLeft:          113 / 1386,
    screenTop:            12 / 896,
    screenWidth:        1163 / 1386,
    screenHeight:        872 / 896,
    digitiserPadLeft:      0 / 1386,
    digitiserPadTop:      12 / 896,
    digitiserPadWidth:  1386 / 1386,
    digitiserPadHeight:  872 / 896,
    digitiserWidth:        640,
    digitiserHeight:       480,
    lcdAnchoredTouchMapping: true,
  },
  // Psion MC400 placeholder (800 x 600 viewBox, 640x400 LCD centred at
  // 80, 60 inside the lid bezel). The MC400 has no digitiser — input
  // is keyboard-only — so no digitiserPad* overrides are needed.
  'mc400.svg': {
    aspectRatio:  800 / 600,
    screenLeft:    80 / 800,
    screenTop:     60 / 600,
    screenWidth:  640 / 800,
    screenHeight: 400 / 600,
  },
  // Psion Organiser II placeholder skin (viewBox 700x420; LCD pane
  // at x=110, y=50, w=480, h=140 in SVG coordinates). The canvas's
  // internal pixel resolution is reported by the WASM module as a
  // 6x upscale of the real 80x32 panel — the SVG cutout dimensions
  // here are purely about where the canvas gets drawn within the
  // device body, independent of the panel's internal resolution.
  'organiser2.svg': {
    aspectRatio:  700 / 420,
    screenLeft:   110 / 700,
    screenTop:     50 / 420,
    screenWidth:  480 / 700,
    screenHeight: 140 / 420,
  },

  // ── PNG photo skins ──────────────────────────────────────────────

  // Psion 5mx photo (1725×758). The LCD cutout sits at the same
  // fractional position as the 5mx.svg artwork it replaces — the
  // PNG was shot from the same angle so the proportions match.
  '5mx.png': {
    aspectRatio:  1725 / 758,
    screenLeft:   187.5   / 2895.83,
    screenTop:     20.83  / 1270.83,
    screenWidth:  2666.67 / 2895.83,
    screenHeight: 1000    / 1270.83,
  },

  // Psion Series 5 photo (1498×662). Same form-factor as the 5mx so
  // the LCD occupies the same fractional position within the lid.
  '5.png': {
    aspectRatio:  1498 / 662,
    screenLeft:   187.5   / 2895.83,
    screenTop:     20.83  / 1270.83,
    screenWidth:  2666.67 / 2895.83,
    screenHeight: 1000    / 1270.83,
  },

  // Psion Revo photo (789×331). The full image corresponds to the
  // 527×208 digitiser coordinate space. The LCD (480×160) sits at
  // digitiser offset (47, 0) — flush to the top of the digitiser —
  // with a 48-px silkscreen shortcut bar below it. No top margin is
  // needed; the entire image is the touch-active area.
  // screenLeft/screenWidth nudged from the raw hardware values (47/527,
  // 480/527) to compensate for the photo angle: ~10 image-px left shift
  // and ~5 image-px wider so the canvas lines up with the screen aperture.
  'revo.png': {
    aspectRatio:        789 / 331,
    screenLeft:          37 / 527,
    screenTop:            0,
    screenWidth:        490 / 527,
    screenHeight:       160 / 208,
    digitiserPadLeft:   0,
    digitiserPadTop:    0,
    digitiserPadWidth:  1,
    digitiserPadHeight: 1,
    digitiserWidth:     527,
    digitiserHeight:    208,
  },

  // Ericsson MC218 photo (2411×1087). Rebadged Psion 5mx — same
  // Windermere SoC, same 695×280 digitiser, same 640×240 LCD, same
  // silkscreen icon strip below the LCD.  The LCD cutout sits at the
  // same fractional position within the lid as the 5mx, so it reuses
  // the 5mx screen-position fractions.  The PNG is a close-up of just
  // the screen + silkscreen area, not the full clamshell.
  'mc218.png': {
    aspectRatio:  2411 / 1087,
    screenLeft:   187.5   / 2895.83,
    screenTop:     20.83  / 1270.83,
    screenWidth:  2666.67 / 2895.83,
    screenHeight: 1000    / 1270.83,
  },
};

// ── Photo-skin layouts for the `device-skins/` folder ───────────────────
//
// Each entry matches the aspect ratio of the corresponding PNG/JPEG in
// public/device-skins/ and gives the fractional position of the LCD screen
// within that image.
//
// For SIBO devices, `buttonBarTop` / `buttonBarHeight` define the fraction
// of image height occupied by the app-launch button row so the invisible
// click zones land over the correct part of the device photo.
const DEVICE_SKIN_LAYOUTS: Record<string, SkinLayout> = {
  // ── SIBO devices ─────────────────────────────────────────────────────
  // Screen + button-bar positions measured from docs/Psion_deviceSkins.csv.
  // buttonBarLeft/Width constrain click zones to the measured bar extent.
  '3.png': {
    aspectRatio:  1194 / 880,
    screenLeft:   0.243, screenTop:  0.207,
    screenWidth:  0.531, screenHeight: 0.236,
    buttonBarTop: 0.515, buttonBarHeight: 0.070,
    buttonBarLeft: 0.165, buttonBarWidth: 0.670,
  },
  '3a.png': {
    aspectRatio:  811 / 682,
    screenLeft:   0.151, screenTop:  0.150,
    screenWidth:  0.703, screenHeight: 0.276,
    buttonBarTop: 0.480, buttonBarHeight: 0.071,
    buttonBarLeft: 0.172, buttonBarWidth: 0.662,
  },
  '3c.png': {
    aspectRatio:  946 / 817,
    screenLeft:   0.140, screenTop:  0.145,
    screenWidth:  0.720, screenHeight: 0.267,
    buttonBarTop: 0.476, buttonBarHeight: 0.080,
    buttonBarLeft: 0.157, buttonBarWidth: 0.692,
  },
  '3mx.png': {
    aspectRatio:  1128 / 944,
    screenLeft:   0.180, screenTop:  0.160,
    screenWidth:  0.635, screenHeight: 0.245,
    buttonBarTop: 0.466, buttonBarHeight: 0.070,
    buttonBarLeft: 0.194, buttonBarWidth: 0.617,
  },
  'acornPB.png': {
    aspectRatio:  1125 / 944,
    screenLeft:   0.239, screenTop:  0.171,
    screenWidth:  0.539, screenHeight: 0.239,
    buttonBarTop: 0.484, buttonBarHeight: 0.067,
    buttonBarLeft: 0.161, buttonBarWidth: 0.680,
  },
  'acornPB2.png': {
    aspectRatio:  1100 / 903,
    screenLeft:   0.170, screenTop:  0.102,
    screenWidth:  0.664, screenHeight: 0.272,
    buttonBarTop: 0.445, buttonBarHeight: 0.065,
    buttonBarLeft: 0.185, buttonBarWidth: 0.631,
  },
  'siena.png': {
    aspectRatio:  1064 / 899,
    screenLeft:   0.214, screenTop:  0.166,
    screenWidth:  0.336, screenHeight: 0.266,
    buttonBarTop: 0.503, buttonBarHeight: 0.072,
    buttonBarLeft: 0.200, buttonBarWidth: 0.614,
  },
  'organiser2.png': {
    aspectRatio:  562 / 1473,
    screenLeft:   0.165, screenTop:  0.069,
    screenWidth:  0.696, screenHeight: 0.070,
  },
  'workabout.png': {
    aspectRatio:  421 / 985,
    screenLeft:   0.223, screenTop:  0.232,
    screenWidth:  0.676, screenHeight: 0.135,
  },
  'workaboutMX.png': {
    aspectRatio:  382 / 784,
    screenLeft:   0.229, screenTop:  0.120,
    screenWidth:  0.628, screenHeight: 0.153,
  },

  // ── EPOC devices ─────────────────────────────────────────────────────
  // Screen positions measured from docs/Psion_deviceSkins.csv.
  // digitiserPad* bounds measured from silkscreen extents in the same file.
  '5.png': {
    aspectRatio:  1179 / 912,
    screenLeft:   0.193, screenTop:  0.094,
    screenWidth:  0.681, screenHeight: 0.325,
    digitiserPadLeft:   0.156, digitiserPadTop:    0.086,
    digitiserPadWidth:  0.722, digitiserPadHeight: 0.380,
    digitiserWidth: 695, digitiserHeight: 280,
  },
  '5mx.png': {
    aspectRatio:  1179 / 912,
    screenLeft:   0.174, screenTop:  0.064,
    screenWidth:  0.702, screenHeight: 0.340,
    digitiserPadLeft:   0.132, digitiserPadTop:    0.054,
    digitiserPadWidth:  0.752, digitiserPadHeight: 0.396,
    digitiserWidth: 695, digitiserHeight: 280,
  },
  '5mxPro.png': {
    aspectRatio:  815 / 678,
    screenLeft:   0.140, screenTop:  0.061,
    screenWidth:  0.777, screenHeight: 0.351,
    digitiserPadLeft:   0.102, digitiserPadTop:    0.059,
    digitiserPadWidth:  0.816, digitiserPadHeight: 0.397,
    digitiserWidth: 695, digitiserHeight: 280,
  },
  'MC218.png': {
    aspectRatio:  1015 / 772,
    screenLeft:   0.167, screenTop:  0.070,
    screenWidth:  0.722, screenHeight: 0.354,
    digitiserPadLeft:   0.126, digitiserPadTop:    0.064,
    digitiserPadWidth:  0.771, digitiserPadHeight: 0.411,
    digitiserWidth: 695, digitiserHeight: 280,
  },
  // MC400: Psion clamshell laptop (V30 CPU, 640×400 LCD, trackpad — no digitiser).
  'MC400.png': {
    aspectRatio:  976 / 1093,
    screenLeft:   0.246, screenTop:  0.104,
    screenWidth:  0.513, screenHeight: 0.286,
  },
  // Series 7 / netBook: 640×480 TFT. Silkscreen columns are part of digitiser;
  // LCD-anchored mapping handles them.
  '7_netbook.png': {
    aspectRatio:  846 / 814,
    screenLeft:   0.213, screenTop:  0.118,
    screenWidth:  0.582, screenHeight: 0.450,
    digitiserPadLeft:   0.165, digitiserPadTop:    0.119,
    digitiserPadWidth:  0.680, digitiserPadHeight: 0.453,
    digitiserWidth: 640, digitiserHeight: 480,
    lcdAnchoredTouchMapping: true,
  },
  // Revo: 480×160 LCD. Digitiser 527×208 includes left silkscreen column and
  // shortcut strip below LCD.
  'revo.png': {
    aspectRatio:  986 / 802,
    screenLeft:   0.212, screenTop:  0.101,
    screenWidth:  0.635, screenHeight: 0.267,
    digitiserPadLeft:   0.163, digitiserPadTop:    0.094,
    digitiserPadWidth:  0.688, digitiserPadHeight: 0.321,
    digitiserWidth: 527, digitiserHeight: 208,
  },
  // Osaris: 320×200 LCD. Digitiser 440×200 with 60px silkscreen each side.
  'osaris.png': {
    aspectRatio:  849 / 793,
    screenLeft:   0.221, screenTop:  0.104,
    screenWidth:  0.562, screenHeight: 0.301,
    digitiserPadLeft:   0.098, digitiserPadTop:    0.097,
    digitiserPadWidth:  0.795, digitiserPadHeight: 0.320,
    digitiserWidth: 440, digitiserHeight: 200,
  },
};

// Map a device ID to its photo filename in the `device-skins/` folder.
// Returns null if no photo skin is available for this device.
export function getDeviceSkinPhotoFilename(deviceId: string | null): string | null {
  switch (deviceId) {
    case 'series3':     return '3.png';
    case 'series3a':    return '3a.png';
    case 'series3c':    return '3c.png';
    case 'series3mx':   return '3mx.png';
    case 'series5':     return '5.png';
    case '5mx':         return '5mx.png';
    case '5mxpro':      return '5mxPro.png';
    case 'series7':
    case 'netbook':     return '7_netbook.png';
    case 'mc218':       return 'MC218.png';
    case 'mc400':       return 'MC400.png';
    case 'pocketbk':    return 'acornPB.png';
    case 'pocketbk2':   return 'acornPB2.png';
    case 'organiser2':  return 'organiser2.png';
    case 'osaris':      return 'osaris.png';
    case 'revo':        return 'revo.png';
    case 'siena':       return 'siena.png';
    case 'workabout':   return 'workabout.png';
    case 'workaboutmx': return 'workaboutMX.png';
    default:            return null;
  }
}

// deviceId: the profile ID string from useEmulator (e.g. 'mc218', 'revo').
// deviceName: the string returned by the WASM getDeviceName() call.
// The MC218 and Revo both extend Windermere whose getDeviceName() returns
// "Series 5mx", so we must disambiguate by profile ID first.
// Exported so App.tsx can call it with an empty deviceName during loading
// (before WASM reports a name) to get the skin for the loading preview.
export function getSkinFilename(deviceName: string, deviceId: string | null): string | null {
  // deviceId checks first — some IDs are ambiguous from deviceName alone.
  if (deviceId === 'mc218')    return 'mc218.png';
  if (deviceId === 'revo')     return 'revo.png';
  if (deviceId === '5mx' || deviceId === '5mxpro') return '5mx.png';
  if (deviceId === 'series5')  return '5.png';
  if (deviceId === 'osaris')   return 'OregonScientific_Osaris.jpg';
  if (deviceId === 'series7')  return '7_netbook.jpeg';
  if (deviceId === 'netbook')  return '7_netbook.jpeg';
  if (deviceId === 'mc400')    return 'mc400.svg';
  if (deviceId === 'organiser2') return 'organiser2.svg';
  // deviceName fallbacks (only reached once WASM has reported the name).
  if (deviceName.includes('5mx') || deviceName.includes('5MX')) return '5mx.png';
  if (deviceName.includes('Series 5') || deviceName.includes('Psion 5')) return '5.png';
  if (deviceName.includes('Osaris')) return 'OregonScientific_Osaris.jpg';
  if (deviceName.includes('Series 7') || deviceName.includes('Series7')) return '7_netbook.jpeg';
  if (deviceName.includes('netBook') || deviceName.includes('netbook') || deviceName.includes('NetBook')) return '7_netbook.jpeg';
  if (deviceName.includes('MC400')) return 'mc400.svg';
  if (deviceName.includes('Organiser')) return 'organiser2.svg';
  // Series 3 / 3a / 3c / 3mx / Siena: plain-LCD render (no device chrome).
  // These devices show a hardware button bar below the LCD instead.
  return null;
}

// SIBO app-launch button bars. Each entry maps to one physical button;
// pressing it sends the corresponding EStdKeyF1-F8 scancode directly
// (codes 96-103). No Psion modifier is required — the SIBO kernel maps
// each F-key straight to its application in the keyboard matrix.
const SIBO_BUTTONS_8 = [
  { label: 'System', code:  96 }, // EStdKeyF1
  { label: 'Data',   code:  97 }, // EStdKeyF2
  { label: 'Word',   code:  98 }, // EStdKeyF3
  { label: 'Agenda', code:  99 }, // EStdKeyF4
  { label: 'Time',   code: 100 }, // EStdKeyF5
  { label: 'World',  code: 101 }, // EStdKeyF6
  { label: 'Calc',   code: 102 }, // EStdKeyF7
  { label: 'Sheet',  code: 103 }, // EStdKeyF8
] as const;
const SIBO_BUTTONS_9 = [
  ...SIBO_BUTTONS_8,
  { label: 'Jotter', code: 104 }, // EStdKeyF9 (Series 3c/3mx only)
] as const;
const ACORN_BP_BUTTONS = [
  { label: 'Desktop', code:  96 }, // EStdKeyF1
  { label: 'Cards',   code:  97 }, // EStdKeyF2
  { label: 'Write',   code:  98 }, // EStdKeyF3
  { label: 'Abacus',  code:  99 }, // EStdKeyF4
  { label: 'Time',    code: 100 }, // EStdKeyF5
  { label: 'Calc',    code: 101 }, // EStdKeyF6
  { label: 'Key 1',   code: 102 }, // EStdKeyF7
  { label: 'Key 2',   code: 103 }, // EStdKeyF8
] as const;
const ACORN_PB2_BUTTONS = [
  { label: 'Desktop',  code:  96 }, // EStdKeyF1
  { label: 'Cards',    code:  97 }, // EStdKeyF2
  { label: 'Write',    code:  98 }, // EStdKeyF3
  { label: 'Schedule', code:  99 }, // EStdKeyF4
  { label: 'Time',     code: 100 }, // EStdKeyF5
  { label: 'World',    code: 101 }, // EStdKeyF6
  { label: 'Calc',     code: 102 }, // EStdKeyF7
  { label: 'Abacus',   code: 103 }, // EStdKeyF8
] as const;

function calcContainerSize(
  skinLayout: SkinLayout,
  lcdWidth: number,
  lcdHeight: number,
  mode: SizingMode,
  deviceScale: DeviceScale,
  forceFS = false,
  chromeless = false,
) {
  const aspectRatio = skinLayout.aspectRatio;
  const isFS = forceFS || !!document.fullscreenElement;
  if (isFS) {
    const w = Math.min(window.innerWidth, window.innerHeight * aspectRatio);
    return { width: w, height: w / aspectRatio };
  }
  // Vertical headroom budget. The default 0.78 leaves room for the page
  // header AND the in-view control / key-helper rows EmulatorView draws
  // below the device frame. In chromeless mode (#/embed/<id>) there is
  // neither a header nor a bottom bar, so we can use almost the whole
  // viewport.
  const fillMaxHFrac = chromeless ? 0.98 : 0.78;
  if (mode === 'fill') {
    const maxW = window.innerWidth  * 0.98;
    const maxH = window.innerHeight * fillMaxHFrac;
    // Width overflow is prevented on the whole device surround (clamp to
    // maxW), but height overflow is prevented only on the emulated LCD
    // screen — the maxH budget applies to the screen sub-rectangle
    // (screenHeight fraction of the surround), not the surround itself.
    // Dividing by screenHeight lets the surround run past the viewport
    // vertically so the device can fill the available width; the page
    // scrolls to reach the rest of the case. For skinless devices
    // screenHeight === 1, so this is identical to the old behaviour.
    const w = Math.min(maxW, (maxH * aspectRatio) / skinLayout.screenHeight);
    return { width: w, height: w / aspectRatio };
  }
  // 'device' — pixel-perfect at the physical-pixel level *when it fits*.
  // We want N physical screen pixels per device pixel (N = deviceScale,
  // selected by the user from the actual-size hover flyout: 1/2/3/4).
  // The canvas backing store is already lcdWidth × lcdHeight (device
  // pixels); the browser renders it at cssDim × devicePixelRatio
  // physical pixels, so cssDim = lcdDim × N / DPR gives the
  // 1-device-pixel-to-N-physical-pixels mapping. Then we scale the
  // container so the canvas sub-rectangle lands on that target.
  //
  // The ideal size can exceed the viewport (4× on a small laptop, any
  // scale on a phone). When it does, we fall back to the same viewport
  // clamp 'fill' uses — the device shrinks to fit and we sacrifice the
  // pixel-perfect property rather than overflowing. The user's chosen
  // scale is preserved in state, so resizing the window larger snaps
  // back to pixel-perfect rendering at that scale.
  const dpr = window.devicePixelRatio || 1;
  const canvasCssW = (lcdWidth  * deviceScale) / dpr;
  const canvasCssH = (lcdHeight * deviceScale) / dpr;
  const idealW = canvasCssW / skinLayout.screenWidth;
  const idealH = canvasCssH / skinLayout.screenHeight;
  const maxW = window.innerWidth  * 0.98;
  const maxH = window.innerHeight * fillMaxHFrac;
  // Width overflow is checked against the whole device surround (idealW),
  // but height overflow is checked only against the emulated LCD screen
  // (canvasCssH) rather than the full surround (idealH) — the surround is
  // allowed to extend past the viewport vertically so the chosen scale
  // can fill the available width. The page scrolls to reach the rest of
  // the case.
  if (idealW <= maxW && canvasCssH <= maxH) {
    return { width: idealW, height: idealH };
  }
  // Fall back to the viewport clamp, again budgeting maxH against the LCD
  // screen sub-rectangle (screenHeight) instead of the whole surround.
  const w = Math.min(maxW, (maxH * aspectRatio) / skinLayout.screenHeight);
  return { width: w, height: w / aspectRatio };
}

export default function EmulatorView({
  controls,
  sizingMode,
  deviceScale,
  fullscreenRequest,
  showDebugging,
  experimentalFeatures,
  lcdAccuracyMode = false,
  backlight,
  chromeless = false,
  deviceMode = false,
}: Props) {
  const { color: backlightColor, on: backlightOn, toggle: toggleBacklight } = backlight;
  const isColourDevice = controls.currentDeviceId === 'series7' || controls.currentDeviceId === 'netbook';
  controls.deviceModeRef.current = deviceMode && lcdAccuracyMode && !isColourDevice;
  const {
    deviceInfo,
    canvasRef,
    handleKeyDown,
    handleKeyUp,
    handleInput,
    handlePasteText,
    pasteFromClipboard,
    sendEpocKey,
    handlePointerDown,
    handlePointerMove,
    handlePointerUp,
    logs,
    clearLogs,
    setLoggingEnabled,
    paused,
    powerOff,
    powerOn,
    resetDevice,
    saveState,
    getRamSnapshot,
    profiles,
    currentDeviceId,
    savedDevices,
    revertToSaved,
  } = controls;

  // Does the currently-loaded device have a CF slot? Drives visibility
  // of the "CF Card" control below — we hide it on SIBO machines
  // (Series 3 family, Siena, Workabout) and on the Revo, which either
  // used Psion SSD packs or had no external-storage slot at all.
  const hasCFSlot = profiles.find(p => p.id === currentDeviceId)?.hasCFSlot ?? false;
  // SSD pack count (SIBO devices only). 2 for 3/3a/3c/3mx, 1 for Siena,
  // 0 elsewhere — drives visibility of the "SSD Pack" button below.
  const ssdSlotCount = profiles.find(p => p.id === currentDeviceId)?.ssdSlotCount ?? 0;
  const anySsdAttached = controls.ssdAttached.slice(0, ssdSlotCount).some(Boolean);
  // Datapak / Rampak slot count (Psion Organiser II only). 2 for the
  // Organiser II, 0 elsewhere — drives visibility of the "Datapak"
  // button below. Distinct surface from SSD because the per-slot kind
  // discriminant matters in the UI.
  const datapakSlotCount = profiles.find(p => p.id === currentDeviceId)?.datapakSlotCount ?? 0;
  const anyDatapakAttached = controls.datapakAttached.slice(0, datapakSlotCount).some(Boolean);

  const logEndRef       = useRef<HTMLDivElement>(null);
  const logBodyRef      = useRef<HTMLDivElement>(null);
  const hiddenInputRef  = useRef<HTMLTextAreaElement>(null);
  const fsContainerRef  = useRef<HTMLDivElement>(null);
  const overlayRef      = useRef<HTMLDivElement>(null);
  const skinImgRef      = useRef<HTMLImageElement>(null);
  const siboBarImgRef   = useRef<HTMLImageElement>(null);
  const [showLogs,    setShowLogs]    = useState(false);
  // "Include surround" — when true, the Screenshot button captures the
  // device skin and (where present) the SIBO app-button bar in addition
  // to the LCD canvas. Default off so the legacy behaviour (LCD only) is
  // preserved for users who just want a clean panel image.
  const [includeSurround, setIncludeSurround] = useState(false);
  const [saving,      setSaving]      = useState(false);
  // Most recent save outcome surfaced briefly next to the Save State button.
  // Null = no recent save; otherwise 'ok' (transient green tick) or an error
  // message (longer-lived, red). Cleared by a setTimeout below.
  const [saveStatus,  setSaveStatus]  = useState<'ok' | string | null>(null);
  const [showCardDlg, setShowCardDlg] = useState(false);
  const [showSsdDlg,  setShowSsdDlg]  = useState(false);
  const [showDatapakDlg, setShowDatapakDlg] = useState(false);
  const [showRemoteLinkDlg, setShowRemoteLinkDlg] = useState(false);
  const [showIrDlg, setShowIrDlg] = useState(false);
  const [showModemDlg, setShowModemDlg] = useState(false);
  const [showPrinterDlg, setShowPrinterDlg] = useState(false);
  // Remote Link / Infrared capability comes from the device profile
  // (core/device_registry.cpp), the same way hasCFSlot / ssdSlotCount
  // already do: remoteLinkUart / infraredUart carry the SoC UART each
  // port is wired to (-1 = hardware absent or bridge not validated),
  // and linkProtocol / irProtocol say which wire protocol the ROM
  // speaks (1 = EPOC32 PLP+RFSV32 / IrDA+EikonIR, 2 = the EPOC16 SIBO
  // variants). Per-device values and the empirical verification notes
  // live next to each profile entry in the registry.
  const linkProfile    = profiles.find(p => p.id === currentDeviceId);
  const remoteLinkUart = linkProfile?.remoteLinkUart ?? -1;
  const linkProtocol   = linkProfile?.linkProtocol ?? 0;
  const showRemoteLink = remoteLinkUart >= 0 && linkProtocol > 0;
  const irSendUart     = linkProfile?.infraredUart ?? -1;
  const irProtocol     = linkProfile?.irProtocol ?? 0;
  const showInfrared   = irSendUart >= 0 && irProtocol > 0;
  // Printer capture rides the same cable UART as Remote Link, so it's
  // available wherever the serial bridge is validated. The two contend
  // for the port at attach time (refused with a readable error), same
  // as the Modem path.
  const showPrinter    = remoteLinkUart >= 0;
  // Link-handshake Req_Con flavour: the ER3/ER4 CL-PS711x ROMs silently drop
  // the Revo-style 0x24 Req_Con (and stop retrying their Req_Req, wedging the
  // link) but complete the handshake with 0x22 — they reply Ack_Pdu and
  // proceed to NCP Info. Harness-verified reply matrix; see
  // scripts/test-remote-link.sh.
  const isClps711x = currentDeviceId === 'osaris' || currentDeviceId === 'series5';
  const remoteLinkConSeq = isClps711x ? 2 : 4;
  // Which SIBO app-button is currently held (for press highlight on mobile)
  const [pressedSiboBtn, setPressedSiboBtn] = useState<string | null>(null);
  // Last-clicked position on the device frame, captured when showDebugging is on.
  // Stores image-fraction coords (fx/fy) and the computed digitiser coords (dx/dy)
  // so the user can read exact values to feed back as corrected layout entries.
  const [lastClick, setLastClick] = useState<{ fx: number; fy: number; dx: number; dy: number } | null>(null);
  const [coordsCopied, setCoordsCopied] = useState(false);
  // cssFS: fullscreen via CSS overlay (fallback for browsers without the API, e.g. iPhone)
  // nativeFS: fullscreen via the browser Fullscreen API
  const [cssFS,    setCssFS]    = useState(false);
  const [nativeFS, setNativeFS] = useState(false);
  const inFullscreen = cssFS || nativeFS;

  const handleSave = useCallback(async () => {
    setSaving(true);
    setSaveStatus(null);
    try {
      await saveState();
      setSaveStatus('ok');
      // Clear the tick after 2 seconds — long enough for the user to
      // see it without the indicator camping on the toolbar.
      window.setTimeout(() => setSaveStatus(s => (s === 'ok' ? null : s)), 2000);
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      setSaveStatus(`Save failed: ${msg}`);
      // Error stays for 8 seconds so the user has time to read it.
      window.setTimeout(() => setSaveStatus(s => (typeof s === 'string' && s.startsWith('Save failed') ? null : s)), 8000);
    } finally {
      setSaving(false);
    }
  }, [saveState]);

  const showKeyboard = useCallback(() => {
    hiddenInputRef.current?.focus();
  }, []);

  const toggleFullscreen = useCallback(async () => {
    // Exit
    if (nativeFS) { await document.exitFullscreen().catch(() => {}); return; }
    if (cssFS)    { setCssFS(false); return; }
    // Enter — try native Fullscreen API on the container element first
    try {
      await (fsContainerRef.current ?? document.documentElement).requestFullscreen();
      // setNativeFS is handled by the fullscreenchange listener below
    } catch {
      // API unavailable (e.g. iPhone Safari) — CSS overlay fallback
      setCssFS(true);
    }
  }, [nativeFS, cssFS]);

  // In device mode, use the photo-realistic skin from `device-skins/`.
  const deviceSkinPhotoFile = deviceMode ? getDeviceSkinPhotoFilename(currentDeviceId) : null;
  const deviceSkinLayout: SkinLayout | undefined = deviceSkinPhotoFile
    ? DEVICE_SKIN_LAYOUTS[deviceSkinPhotoFile]
    : undefined;

  const skinFile = deviceInfo ? getSkinFilename(deviceInfo.deviceName, currentDeviceId) : null;
  // Active skin: device-skin photo takes precedence when device mode is on.
  const activeSkinFile   = deviceSkinPhotoFile ?? skinFile;
  const activeSkinFolder = deviceSkinPhotoFile ? 'device-skins' : 'skins';
  const skinLayout: SkinLayout = deviceSkinLayout ?? (skinFile ? SKIN_LAYOUTS[skinFile] : undefined) ?? (deviceInfo ? {
    // No skin: render the LCD as the entire frame. Digitiser area == LCD area.
    aspectRatio:  deviceInfo.lcdWidth / deviceInfo.lcdHeight,
    screenLeft:   0,
    screenTop:    0,
    screenWidth:  1,
    screenHeight: 1,
  } : { aspectRatio: 2.28, screenLeft: 0, screenTop: 0, screenWidth: 1, screenHeight: 1 });

  // Initialise from window dimensions immediately so first render is correct.
  // Before deviceInfo arrives lcdWidth/Height are 0; the component returns
  // null anyway (see guard below) and the effect re-runs once deviceInfo lands.
  const [containerSize, setContainerSize] = useState(() =>
    calcContainerSize(skinLayout, deviceInfo?.lcdWidth ?? 0, deviceInfo?.lcdHeight ?? 0, sizingMode, deviceScale, false, chromeless)
  );

  // Global keyboard listeners
  useEffect(() => {
    window.addEventListener('keydown', handleKeyDown);
    window.addEventListener('keyup', handleKeyUp);
    return () => {
      window.removeEventListener('keydown', handleKeyDown);
      window.removeEventListener('keyup', handleKeyUp);
    };
  }, [handleKeyDown, handleKeyUp]);

  // Dev-only automation hook: lets the Playwright browser tests inject
  // EpocKey chords deterministically (synthesised KeyboardEvents are
  // fragile across focus/overlay states). Stripped from production
  // builds by the DEV gate.
  useEffect(() => {
    if (!import.meta.env.DEV) return;
    (window as unknown as Record<string, unknown>).__pressEpocChord =
      (modifiers: number[], key: number) => controls.pressEpocChord(modifiers, key);
    return () => { delete (window as unknown as Record<string, unknown>).__pressEpocChord; };
  }, [controls]);

  // Track native fullscreen state (handles Escape key exit too)
  useEffect(() => {
    const onChange = () => setNativeFS(!!document.fullscreenElement);
    document.addEventListener('fullscreenchange', onChange);
    return () => document.removeEventListener('fullscreenchange', onChange);
  }, []);

  // Resize + fullscreen — recalculate whenever window size, sizing mode,
  // or FS state changes.
  useEffect(() => {
    const lcdW = deviceInfo?.lcdWidth ?? 0;
    const lcdH = deviceInfo?.lcdHeight ?? 0;
    const update = () => setContainerSize(calcContainerSize(skinLayout, lcdW, lcdH, sizingMode, deviceScale, cssFS, chromeless));
    update();
    window.addEventListener('resize', update);
    document.addEventListener('fullscreenchange', update);
    // devicePixelRatio can change without resize/fullscreen events when the
    // window is dragged between displays of different DPI. matchMedia fires
    // 'change' when the current DPR no longer matches the captured one;
    // we re-subscribe inside the handler because the matching query also
    // changes with the new ratio.
    let mq = window.matchMedia(`(resolution: ${window.devicePixelRatio}dppx)`);
    const onDprChange = () => {
      update();
      mq.removeEventListener('change', onDprChange);
      mq = window.matchMedia(`(resolution: ${window.devicePixelRatio}dppx)`);
      mq.addEventListener('change', onDprChange);
    };
    mq.addEventListener('change', onDprChange);
    return () => {
      window.removeEventListener('resize', update);
      document.removeEventListener('fullscreenchange', update);
      mq.removeEventListener('change', onDprChange);
    };
  }, [skinLayout.aspectRatio, skinLayout.screenWidth, skinLayout.screenHeight, deviceInfo?.lcdWidth, deviceInfo?.lcdHeight, sizingMode, deviceScale, cssFS, chromeless]);

  // Header Fullscreen icon: parent bumps `fullscreenRequest` each click,
  // we trigger the same enter path we used to wire to the in-control-bar
  // Fullscreen button. The exit path (Escape, the cssFS X button, the
  // in-control-bar Exit Fullscreen button) is unchanged.
  const fullscreenRequestRef = useRef(fullscreenRequest);
  useEffect(() => {
    if (fullscreenRequest === fullscreenRequestRef.current) return;
    fullscreenRequestRef.current = fullscreenRequest;
    void toggleFullscreen();
  }, [fullscreenRequest, toggleFullscreen]);

  // Non-passive touchstart on the overlay: prevents iOS Safari from blurring
  // the hidden textarea (and dismissing the on-screen keyboard) when the user
  // taps the emulator screen while typing.
  useEffect(() => {
    const el = overlayRef.current;
    if (!el) return;
    const handler = (e: TouchEvent) => {
      if (document.activeElement === hiddenInputRef.current) e.preventDefault();
    };
    el.addEventListener('touchstart', handler, { passive: false });
    return () => el.removeEventListener('touchstart', handler);
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  // Auto-scroll log panel to bottom when new lines arrive
  useEffect(() => {
    if (showLogs) logEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [logs, showLogs]);


  if (!deviceInfo) return null;

  const { lcdWidth, lcdHeight } = deviceInfo;
  const { width: cw, height: ch } = containerSize;

  // The digitiser dimensions used for both overlay sizing AND touch
  // coordinate mapping. Prefer the skin's hardcoded values when given:
  // those match the SVG viewBox the artwork was drawn against, so the
  // overlay always lines up with the visible silkscreen even if the
  // WASM module reports a different (e.g. stale) digitiser size.
  const digitiserWidth  = skinLayout.digitiserWidth  ?? deviceInfo.digitiserWidth;
  const digitiserHeight = skinLayout.digitiserHeight ?? deviceInfo.digitiserHeight;

  // Canvas position derived from skin layout (display only — no pointer events).
  const canvasLeft   = skinLayout.screenLeft   * cw;
  const canvasTop    = skinLayout.screenTop    * ch;
  const canvasWidth  = skinLayout.screenWidth  * cw;
  const canvasHeight = skinLayout.screenHeight * ch;

  // The touch overlay covers exactly the digitiser pad — the area a user
  // can physically tap on the real device, including bezel/silkscreen zones
  // around the LCD. A skin can position the pad anywhere within the SVG
  // via digitiserPadLeft/Top/Width/Height (Revo); without those, the pad
  // defaults to anchored at SVG (0, 0), spanning the full SVG width, with
  // its height set by the digitiser's own aspect ratio (5mx behaviour).
  const padLeftFrac   = skinLayout.digitiserPadLeft  ?? 0;
  const padTopFrac    = skinLayout.digitiserPadTop   ?? 0;
  const padWidthFrac  = skinLayout.digitiserPadWidth ?? 1;
  const padHeightFrac =
    skinLayout.digitiserPadHeight
    ?? (digitiserHeight / digitiserWidth) * skinLayout.aspectRatio;
  const overlayLeft   = padLeftFrac   * cw;
  const overlayTop    = padTopFrac    * ch;
  const overlayWidth  = padWidthFrac  * cw;
  const overlayHeight = padHeightFrac * ch;

  // Map a pointer event on the overlay to digitiser coordinates.
  //
  // For most devices the digitiser-pad area coincides with the screen
  // area, so cssX/rect.width maps linearly to 0..digitiserWidth.
  //
  // For the Series 7 / netBook (and other devices whose skin opts in via
  // lcdAnchoredTouchMapping), we want LCD touches to land at LCD-X
  // 0..640 and silkscreen touches to land OUTSIDE that range (negative
  // for left silkscreen, > 640 for right silkscreen).  The WASM-side
  // touchAdcXY adds the silkscreen inset uniformly so all values end
  // up in the right ADC slice.
  //
  // Other skins (5mx / Revo / Osaris / …) use the pad-rect-linear
  // mapping where the whole pad CSS rect maps linearly onto digitiser
  // space — the pad covers the full digitiser including any silkscreen
  // regions below or beside the LCD, and the digitiser-coord output
  // already encodes which region was tapped.  Auto-triggering
  // LCD-anchored mapping for these would mis-map silkscreen taps
  // (5mx silkscreen sits BELOW the LCD at Y > 240, not beside it).
  const screenLeftFrac  = skinLayout.screenLeft  ?? padLeftFrac;
  const screenWidthFrac = skinLayout.screenWidth ?? padWidthFrac;
  const screenTopFrac   = skinLayout.screenTop   ?? padTopFrac;
  const screenHeightFrac = skinLayout.screenHeight
    ?? skinLayout.digitiserPadHeight ?? padHeightFrac;
  const useLcdAnchoredMapping = skinLayout.lcdAnchoredTouchMapping === true;
  const toDigitiserCoords = (e: React.PointerEvent<HTMLDivElement>) => {
    const rect = e.currentTarget.getBoundingClientRect();
    const cssX = e.clientX - rect.left;
    const cssY = e.clientY - rect.top;
    if (useLcdAnchoredMapping) {
      // LCD-anchored: figure out the LCD's position WITHIN the pad
      // in CSS pixels, then map cssX (which is relative to the pad
      // bounding rect) into LCD-X coords.  Outside the LCD area, the
      // result is negative or > LCD-width — i.e. silkscreen coords.
      const lcdLeftCss  = ((screenLeftFrac   - padLeftFrac) / padWidthFrac)  * rect.width;
      const lcdTopCss   = ((screenTopFrac    - padTopFrac)  / padHeightFrac) * rect.height;
      const lcdWCss     = (screenWidthFrac  / padWidthFrac)  * rect.width;
      const lcdHCss     = (screenHeightFrac / padHeightFrac) * rect.height;
      const dx = Math.round(((cssX - lcdLeftCss) / lcdWCss) * digitiserWidth);
      const dy = Math.round(((cssY - lcdTopCss)  / lcdHCss) * digitiserHeight);
      return { x: dx, y: dy };
    }
    const dx = Math.round((cssX / rect.width)  * digitiserWidth);
    const dy = Math.round((cssY / rect.height) * digitiserHeight);
    return { x: dx, y: dy };
  };

  // Base button class — light Psion theme
  const btn = [
    'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap select-none cursor-pointer',
    'bg-psion-mid border border-psion-accent/50 text-psion-charcoal',
    'hover:bg-psion-accent hover:text-white hover:border-psion-accent',
    'active:bg-psion-charcoal active:text-white active:border-psion-charcoal',
    'transition-colors',
  ].join(' ');

  // Highlighted variant (used for Power On when device is off, active storage buttons)
  const btnActive = [
    'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap select-none cursor-pointer',
    'bg-psion-highlight border border-psion-accent/50 text-psion-charcoal',
    'hover:bg-psion-accent hover:text-white hover:border-psion-accent',
    'active:bg-psion-charcoal active:text-white',
    'transition-colors',
  ].join(' ');

  // Experimental-feature variants — same shape as btn / btnActive but
  // with a saturated red border so the user can tell at a glance which
  // controls are gated by the Experimental features setting (currently
  // the Backlight toggle and the simulated Modem). Hover / active states
  // keep the red border so the signal doesn't disappear on interaction.
  const expBtn = [
    'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap select-none cursor-pointer',
    'bg-psion-mid border border-red-600 text-psion-charcoal',
    'hover:bg-psion-accent hover:text-white hover:border-red-600',
    'active:bg-psion-charcoal active:text-white active:border-red-600',
    'transition-colors',
  ].join(' ');
  const expBtnActive = [
    'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap select-none cursor-pointer',
    'bg-psion-highlight border border-red-600 text-psion-charcoal',
    'hover:bg-psion-accent hover:text-white hover:border-red-600',
    'active:bg-psion-charcoal active:text-white active:border-red-600',
    'transition-colors',
  ].join(' ');

  // EPOC key codes for the on-screen mobile keys. Mirrors the entries in
  // frontend/src/lib/keymap.ts so the on-screen buttons emit the same EPOC
  // events the matching physical keys would. Mobile soft keyboards omit
  // Esc / Menu / arrows, so these are the keys you can't otherwise reach.
  const EPOC_ESCAPE    = 4;
  const EPOC_TAB       = 2;
  const EPOC_MENU      = 148;
  const EPOC_LEFT      = 14;
  const EPOC_RIGHT     = 15;
  const EPOC_UP        = 16;
  const EPOC_DOWN      = 17;
  // Psion Organiser II: ON / MODE / EXE / DEL / SHIFT have no natural
  // mapping on a modern keyboard — surface them as dedicated on-screen
  // buttons (in addition to the Esc/Tab/Enter/Backspace/Shift physical
  // shortcuts the driver maps internally in core/organiser2.cpp).
  const EPOC_ENTER     = 3;
  const EPOC_BACKSPACE = 1;
  const EPOC_SHIFT     = 18;
  // Psion 5mx / 5mxpro / Series 5 / MC218 / Osaris / Revo: the Fn key
  // (bottom-left of the physical keyboard, blue-labelled symbols on the
  // alphabet keys). Maps to EStdKeyLeftFunc which the Windermere matrix
  // wires at COL5/ROW6 (see core/windermere.cpp::setKeyboardKey). There
  // is no widely-known PC keyboard equivalent — host MetaLeft routes to
  // EStdKeyLeftFunc via keymap.ts — so we surface it as a button.
  const EPOC_FN        = 24;
  // Psion MC400 trackpad click — repurposes EStdKeyXXX (120) as the
  // dedicated click bit (COL8 / 0x10 in the keyboard matrix). Pointer
  // events on the trackpad area only update the analog X/Y position;
  // the click button is fired separately so a mobile user can position
  // the cursor before tapping click.
  const EPOC_TRACKPAD  = 120;
  // SIBO devices (Series 3 / 3a / 3c / 3mx / Pocket Book): the Psion key
  // (⊔, the logo key left of Menu) maps to EStdKeyRightAlt (21); the
  // diamond key (◆, right of Menu) maps to EStdKeyCapsLock (26).
  const EPOC_PSION     = 21;
  const EPOC_DIAMOND   = 26;
  // Psion Series 5 / 5mx / 5mxpro / MC218: dedicated voice-recorder buttons
  // on the case (not on the keyboard). Already wired into the keyboard matrix
  // by core/series5.h and core/windermere.cpp::setKeyboardKey.
  const EPOC_DICT_RECORD = 158; // EStdKeyDictaphoneRecord
  const EPOC_DICT_PLAY   = 156; // EStdKeyDictaphonePlay
  const EPOC_DICT_STOP   = 157; // EStdKeyDictaphoneStop
  const isOrganiser2 = deviceInfo?.deviceName?.includes('Organiser') ?? false;
  const isMc400      = deviceInfo?.deviceName?.includes('MC400') ?? false;
  const isSibo       = (deviceInfo?.deviceName?.includes('Series 3') ||
                        deviceInfo?.deviceName?.includes('Pocket Book') ||
                        deviceInfo?.deviceName?.includes('Siena')) ?? false;
  // Windermere-family clamshell devices with a physical Fn key. Series 7 /
  // netBook is intentionally excluded — its keyboard has a full F1-F10 row
  // and no Fn modifier.
  const hasFnKey = currentDeviceId === '5mx'    || currentDeviceId === '5mxpro' ||
                   currentDeviceId === 'series5' || currentDeviceId === 'mc218'  ||
                   currentDeviceId === 'osaris'  || currentDeviceId === 'revo';

  // EPOC clamshells with physical voice-recorder buttons on the case
  // (Record / Play / Stop), separate from the keyboard. Excludes Revo and
  // Osaris, which have no recorder hardware (even though Revo's matrix would
  // accept the codes).
  const hasDictaphone = currentDeviceId === 'series5' || currentDeviceId === '5mx' ||
                        currentDeviceId === '5mxpro'  || currentDeviceId === 'mc218';

  // SIBO button bar — shown below the LCD for Series 3 family and Siena.
  const siboBarImage: string | null =
    currentDeviceId === 'series3'                                        ? '3_buttons.png'       :
    currentDeviceId === 'series3a'                                       ? '3a_buttons.png'      :
    (currentDeviceId === 'series3c' || currentDeviceId === 'series3mx') ? '3c_3mx_buttons.svg'  :
    currentDeviceId === 'siena'                                          ? 'siena_buttons.svg'   :
    currentDeviceId === 'pocketbk'                                       ? 'AcornBP_buttons.png' :
    currentDeviceId === 'pocketbk2'                                      ? 'AcornPB2_buttons.png':
    null;
  const siboBarBtns: ReadonlyArray<{ label: string; code: number }> =
    (currentDeviceId === 'series3c' || currentDeviceId === 'series3mx') ? SIBO_BUTTONS_9    :
    currentDeviceId === 'pocketbk'                                       ? ACORN_BP_BUTTONS  :
    currentDeviceId === 'pocketbk2'                                      ? ACORN_PB2_BUTTONS :
    SIBO_BUTTONS_8;

  // Button factory for keys that need to follow the press/release contract
  // EPOC's keyboard scanner expects (a single down event without a matching
  // up event leaves the key stuck). Pointer events cover both touch+mouse
  // and the cancel/leave handlers ensure we always release even if the
  // user drags their finger off the button.
  const keyBtnClass = [
    'w-12 h-12 flex items-center justify-center rounded-lg select-none cursor-pointer',
    'bg-psion-mid border border-psion-accent/50 text-psion-charcoal text-base font-mono',
    'active:bg-psion-highlight active:text-psion-charcoal active:border-psion-accent',
    'transition-colors',
  ].join(' ');
  const renderKeyBtn = (label: string, code: number, ariaLabel: string) => (
    <button
      key={ariaLabel}
      type="button"
      aria-label={ariaLabel}
      className={keyBtnClass}
      onPointerDown={e => {
        e.preventDefault();
        e.currentTarget.setPointerCapture(e.pointerId);
        sendEpocKey(code, true);
      }}
      onPointerUp={e => {
        e.preventDefault();
        sendEpocKey(code, false);
      }}
      onPointerCancel={() => sendEpocKey(code, false)}
      onPointerLeave={e => {
        // Fires after pointer capture release; only emit the up if the user
        // dragged off the button without lifting (rare but keeps state sane).
        if (e.buttons !== 0) sendEpocKey(code, false);
      }}
      onContextMenu={e => e.preventDefault()}
    >
      {label}
    </button>
  );

  return (
    <div
      ref={fsContainerRef}
      className={[
        'flex flex-col items-center bg-psion-mid',
        chromeless ? 'gap-0' : 'gap-4',
        cssFS
          // CSS fullscreen: fixed overlay covering the whole viewport (including header)
          ? 'fixed inset-0 z-50 justify-center overflow-hidden'
          // Chromeless (#/embed/<id>) drops the page-header reservation —
          // the iframe route renders no header so we can centre vertically
          // across the full viewport height instead.
          : chromeless ? 'absolute inset-0 justify-center overflow-hidden' : 'py-6',
      ].join(' ')}
      style={!cssFS && !chromeless ? { minHeight: 'calc(100vh - 49px)' } : undefined}
    >

      {/* CSS-fullscreen exit button — no Escape key on mobile */}
      {cssFS && (
        <button
          onClick={() => setCssFS(false)}
          style={{ position: 'absolute', top: 8, right: 8, zIndex: 60 }}
          className={btn}
        >
          ✕ Exit
        </button>
      )}

      {/* Device frame + (optional) SIBO app-button bar.
          Wrapped in a zero-gap flex container so the button bar sits flush
          against the bottom of the LCD frame on every SIBO machine. The
          outer container's gap-4 only kicks in for the next sibling. */}
      <div className="flex flex-col items-center flex-shrink-0">
      {/* ── Device frame ── */}
      <div
        className="relative flex-shrink-0"
        style={{ width: cw, height: ch }}
        onClick={showDebugging ? e => {
          const r = e.currentTarget.getBoundingClientRect();
          const fx = (e.clientX - r.left) / r.width;
          const fy = (e.clientY - r.top) / r.height;
          const cpx = fx * cw - overlayLeft;
          const cpy = fy * ch - overlayTop;
          let dx: number, dy: number;
          if (useLcdAnchoredMapping && overlayWidth > 0 && overlayHeight > 0) {
            const lx = ((screenLeftFrac  - padLeftFrac)  / padWidthFrac)  * overlayWidth;
            const ly = ((screenTopFrac   - padTopFrac)   / padHeightFrac) * overlayHeight;
            const lw = (screenWidthFrac  / padWidthFrac)  * overlayWidth;
            const lh = (screenHeightFrac / padHeightFrac) * overlayHeight;
            dx = Math.round(((cpx - lx) / lw) * digitiserWidth);
            dy = Math.round(((cpy - ly) / lh) * digitiserHeight);
          } else {
            dx = overlayWidth  > 0 ? Math.round((cpx / overlayWidth)  * digitiserWidth)  : 0;
            dy = overlayHeight > 0 ? Math.round((cpy / overlayHeight) * digitiserHeight) : 0;
          }
          setLastClick({ fx, fy, dx, dy });
        } : undefined}
      >
        {/* Device skin image (SVG or photo). In device mode, `activeSkinFile`
            resolves to the photo-realistic image from `device-skins/`; otherwise
            it falls back to the minimal SVG from `skins/`. Tagged `psion-skin`
            so the lights-out CSS rule dims the photo while EL backlight is on. */}
        {activeSkinFile && (
          <img
            ref={skinImgRef}
            src={`${import.meta.env.BASE_URL}${activeSkinFolder}/${activeSkinFile}`}
            alt="Device skin"
            className="psion-skin absolute inset-0 w-full h-full pointer-events-none select-none"
            draggable={false}
            onError={e => { (e.currentTarget as HTMLImageElement).style.display = 'none'; }}
          />
        )}

        {/* Simulated EL backlight — opaque coloured layer over the LCD
            cutout, sitting BEHIND the canvas so the canvas's transparent
            "unlit" pixels reveal the backlight colour while its opaque
            "lit" pixels remain as dark text on the glowing background.
            The box-shadow projects a soft tinted glow out onto the rest
            of the device frame so the keyboard / case isn't pitch black
            in lights-out mode — matches what happens in a real dark room
            when the EL panel lights up. Only shown for devices that
            actually had a backlit panel. */}
        {backlightOn && backlightColor && (
          <div
            aria-hidden="true"
            className="absolute pointer-events-none"
            style={{
              left:   canvasLeft,
              top:    canvasTop,
              width:  canvasWidth,
              height: canvasHeight,
              backgroundColor: backlightColor,
              boxShadow: `0 0 ${canvasHeight * 0.6}px ${canvasHeight * 0.25}px ${backlightColor}`,
              zIndex: 5,
            }}
          />
        )}

        {/* LCD canvas — display only; pointer events are handled by the
            touch overlay below so the full digitiser area is responsive.
            The canvas itself is transparent so the SVG skin's LCD-area
            colour (a silvery-green for reflective-LCD devices like Series 5
            / 5mx / MC218 / Osaris) shows through on pixels the emulator
            renders with alpha < 255 — i.e. the unlit "background". Drive
            levels are conveyed via the RGBA alpha channel; see the per-
            device readLCDIntoBuffer implementations. */}
        <canvas
          ref={canvasRef}
          width={lcdWidth}
          height={lcdHeight}
          className="absolute"
          style={{
            left:   canvasLeft,
            top:    canvasTop,
            width:  canvasWidth,
            height: canvasHeight,
            imageRendering: 'pixelated',
            pointerEvents: 'none',
            zIndex: 10,
          }}
        />

        {/* Transparent touch overlay covering the digitiser pad — the
            sub-rectangle of the SVG that's touch-sensitive on the real
            device (LCD + bezel/silkscreen). Position/size come from the
            skin's digitiserPad* fields (defaulting to "anchored at the SVG
            top-left, full width" for skins like 5mx that draw the pad at
            the top of the artwork). Pointer events on this overlay map
            (cssX/rect.width, cssY/rect.height) directly onto digitiser
            coordinates [0..digitiserWidth, 0..digitiserHeight]. */}
        {/* Touch overlay — hidden for SIBO devices in device mode (they have
            no touchscreen; button zones below handle the button bar clicks
            instead). */}
        {!(deviceMode && deviceSkinPhotoFile && isSibo) && (
        <div
          ref={overlayRef}
          className="absolute"
          style={{
            left:        overlayLeft,
            top:         overlayTop,
            width:       overlayWidth,
            height:      overlayHeight,
            touchAction: 'none',
            zIndex:      20,
            cursor:      'crosshair',
          }}
          onPointerDown={e => {
            e.preventDefault();
            e.currentTarget.setPointerCapture(e.pointerId);
            const { x, y } = toDigitiserCoords(e);
            handlePointerDown(x, y);
            // MC400 trackpad — desktop mouse preserves the click-on-press
            // ergonomic. Touch users get a dedicated on-screen click button
            // below so they can position before clicking.
            if (isMc400 && e.pointerType === 'mouse') sendEpocKey(EPOC_TRACKPAD, true);
          }}
          onPointerMove={e => {
            e.preventDefault();
            if (e.buttons === 0) return;
            const { x, y } = toDigitiserCoords(e);
            handlePointerMove(x, y);
          }}
          onPointerUp={e => {
            e.preventDefault();
            handlePointerUp();
            if (isMc400 && e.pointerType === 'mouse') sendEpocKey(EPOC_TRACKPAD, false);
          }}
          onPointerCancel={e => {
            handlePointerUp();
            if (isMc400 && e.pointerType === 'mouse') sendEpocKey(EPOC_TRACKPAD, false);
          }}
          onContextMenu={e => e.preventDefault()}
        />
        )}

        {/* SIBO button-bar click zones in device mode. Positioned within the
            device photo frame instead of showing the separate button-bar
            image below. The button row occupies buttonBarTop..buttonBarTop+
            buttonBarHeight of the image height; it is divided into N equal
            columns, one per app-launch button. */}
        {deviceMode && deviceSkinPhotoFile && isSibo && skinLayout.buttonBarTop !== undefined && (
          <div
            className="absolute flex"
            style={{
              left:   (skinLayout.buttonBarLeft ?? 0) * cw,
              top:    skinLayout.buttonBarTop * ch,
              width:  (skinLayout.buttonBarWidth ?? 1) * cw,
              height: (skinLayout.buttonBarHeight ?? 0.10) * ch,
              zIndex: 20,
            }}
          >
            {siboBarBtns.map(({ label, code }) => (
              <button
                key={label}
                type="button"
                title={label}
                className={[
                  'flex-1 h-full select-none transition-opacity',
                  pressedSiboBtn === label
                    ? 'opacity-25 bg-psion-highlight'
                    : 'opacity-0 hover:opacity-10 hover:bg-white',
                ].join(' ')}
                onPointerDown={e => {
                  e.preventDefault();
                  e.currentTarget.setPointerCapture(e.pointerId);
                  setPressedSiboBtn(label);
                  sendEpocKey(code, true);
                  setTimeout(() => {
                    sendEpocKey(code, false);
                    setPressedSiboBtn(null);
                  }, 120);
                }}
                onPointerUp={() => setPressedSiboBtn(null)}
                onPointerCancel={() => setPressedSiboBtn(null)}
                onContextMenu={e => e.preventDefault()}
              />
            ))}
          </div>
        )}

        {/* Powered-off overlay sits on top of the touch overlay */}
        {paused && (
          <div
            className="absolute flex items-center justify-center bg-black/70 rounded"
            style={{
              left:   canvasLeft,
              top:    canvasTop,
              width:  canvasWidth,
              height: canvasHeight,
              zIndex: 30,
            }}
          >
            <span className="text-white font-mono text-sm tracking-widest opacity-80">
              POWERED OFF
            </span>
          </div>
        )}

      </div>

      {/* ── SIBO app-button bar ──
          Displayed immediately below the LCD frame for Series 3 / 3a /
          3c / 3mx / Siena. Each invisible zone maps to one hardware button.
          In device mode the button zones are overlaid on the photo-realistic
          skin inside the frame above instead of being a separate element here
          (the buttons are visually part of the device photo). */}
      {siboBarImage && !(deviceMode && deviceSkinPhotoFile) && (
        <div className="relative flex-shrink-0" style={{ width: cw }}>
          <img
            ref={siboBarImgRef}
            src={`${import.meta.env.BASE_URL}skins/${siboBarImage}`}
            alt="App buttons"
            className="psion-skin w-full h-auto block pointer-events-none select-none"
            draggable={false}
          />
          <div className="absolute inset-0 flex">
            {siboBarBtns.map(({ label, code }) => (
              <button
                key={label}
                type="button"
                title={label}
                className={[
                  'flex-1 h-full select-none transition-opacity',
                  pressedSiboBtn === label
                    ? 'opacity-25 bg-psion-highlight'
                    : 'opacity-0 hover:opacity-10 hover:bg-white',
                ].join(' ')}
                onPointerDown={e => {
                  e.preventDefault();
                  e.currentTarget.setPointerCapture(e.pointerId);
                  setPressedSiboBtn(label);
                  sendEpocKey(code, true);
                  setTimeout(() => {
                    sendEpocKey(code, false);
                    setPressedSiboBtn(null);
                  }, 120);
                }}
                onPointerUp={() => setPressedSiboBtn(null)}
                onPointerCancel={() => setPressedSiboBtn(null)}
                onContextMenu={e => e.preventDefault()}
              />
            ))}
          </div>
        </div>
      )}
      </div>{/* /device-frame + sibo-bar wrapper */}

      {/* Chrome below the device frame — key helper rows, control bar,
          debug bar, inline dialogs, log panel — is hidden in chromeless
          (#/embed/<id>) mode. The standalone iframe route renders just
          the device; embedders can layer their own controls if they need
          them. The hidden-textarea mobile keyboard path lives inside this
          gate too because there's no on-screen "Keyboard" button without
          the control bar to focus it. */}
      {!chromeless && <>

      {/* Mobile keyboard capture textarea.
          Fixed to the top of the viewport, 1 px tall, fully transparent.
          position:fixed keeps it out of the document flow so it can never
          overlap the control bar buttons regardless of keyboard visibility. */}
      <textarea
        ref={hiddenInputRef}
        aria-hidden="true"
        tabIndex={-1}
        data-psion-input="1"
        style={{
          position: 'fixed',
          top: 0,
          left: 0,
          width: '100%',
          height: 1,
          opacity: 0,
          zIndex: -1,
          resize: 'none',
          border: 'none',
          padding: 0,
          pointerEvents: 'none',
        }}
        autoComplete="off"
        autoCorrect="off"
        autoCapitalize="off"
        spellCheck={false}
        onInput={e => handleInput(e.nativeEvent)}
        onPaste={e => {
          e.preventDefault();
          e.stopPropagation();
          const text = e.clipboardData.getData('text');
          if (text) handlePasteText(text);
        }}
      />

      {/* ── Esc / Tab / Shift / Menu row ──
          Esc, Tab and Shift are mobile-only (soft keyboards don't expose
          them — or in Shift's case, don't fire it as a discrete key event);
          Menu has no physical-keyboard equivalent so it's always shown.
          All four live in a single no-wrap row so they never split across
          lines on narrow screens. SIBO devices have their own flanked Menu
          row below; Organiser II surfaces Menu as MODE. */}
      {!isSibo && !isOrganiser2 && (
        <div className="flex flex-nowrap gap-2 items-center justify-center px-4">
          <span className="contents md:hidden">
            {renderKeyBtn('Esc',   EPOC_ESCAPE, 'Escape key')}
            {renderKeyBtn('Tab',   EPOC_TAB,    'Tab key')}
            {renderKeyBtn('Shift', EPOC_SHIFT,  'Shift key')}
          </span>
          {renderKeyBtn('Menu', EPOC_MENU, 'Menu key')}
          {hasFnKey && renderKeyBtn('Fn', EPOC_FN, 'Fn key')}
        </div>
      )}
      <div className="flex flex-wrap gap-2 items-center justify-center px-4 md:hidden">
        {renderKeyBtn('←', EPOC_LEFT,  'Arrow left')}
        {renderKeyBtn('↑', EPOC_UP,    'Arrow up')}
        {renderKeyBtn('↓', EPOC_DOWN,  'Arrow down')}
        {renderKeyBtn('→', EPOC_RIGHT, 'Arrow right')}
      </div>

      {/* ── Organiser II key pad ──
          ON / MODE / EXE / DEL / SHIFT are Organiser II hardware keys
          with no natural label on a modern keyboard. We always render
          this row (desktop AND mobile) for the Organiser so users can
          press them by name; the underlying EPOC codes are also bound
          to physical Esc/Tab/Enter/Backspace/Shift (see keymap.ts and
          core/organiser2.cpp::setKeyboardKey for the matrix mapping).
          OFF is omitted because on real hardware OFF is SHIFT+ON, not
          a discrete key. */}
      {isOrganiser2 && (
        <div className="flex flex-wrap gap-2 items-center justify-center px-4">
          {renderKeyBtn('ON',    EPOC_ESCAPE,    'On / Clear')}
          {renderKeyBtn('MODE',  EPOC_MENU,      'Mode')}
          {renderKeyBtn('EXE',   EPOC_ENTER,     'Execute')}
          {renderKeyBtn('DEL',   EPOC_BACKSPACE, 'Delete')}
          {renderKeyBtn('SHIFT', EPOC_SHIFT,     'Shift')}
        </div>
      )}

      {/* ── MC400 trackpad click ──
          The MC400 has a dedicated click switch alongside its trackpad.
          Touch users have no way to position-then-click without it (every
          tap on the trackpad area would otherwise both move and click in
          one event), so we surface the click bit as a dedicated button.
          Desktop mouse users get the ergonomic auto-click on mouse-down
          (handled in the overlay's onPointerDown above) but the button
          remains visible as a discoverable alternative. */}
      {isMc400 && (
        <div className="flex flex-wrap gap-2 items-center justify-center px-4">
          {renderKeyBtn('Click', EPOC_TRACKPAD, 'Trackpad click')}
        </div>
      )}

      {/* ── Voice-recorder buttons ──
          Series 5 / 5mx / 5mxpro / MC218 have three dedicated record buttons
          on the case (Record / Play / Stop), separate from the keyboard. The
          matrix wiring lives in core/series5.h and core/windermere.cpp; there
          is no PC key for them, so we always surface them as on-screen
          buttons (desktop AND mobile). */}
      {hasDictaphone && (
        <div className="flex flex-wrap gap-2 items-center justify-center px-4">
          {renderKeyBtn('● Rec',  EPOC_DICT_RECORD, 'Record')}
          {renderKeyBtn('▶ Play', EPOC_DICT_PLAY,   'Play')}
          {renderKeyBtn('■ Stop', EPOC_DICT_STOP,   'Stop')}
        </div>
      )}

      {/* ── SIBO special keys ──
          Series 3 / 3a / 3c / 3mx / Pocket Book devices have two keys
          flanking the Menu button that have no direct equivalent on a
          modern keyboard: the Psion logo key (⊔, left of Menu) and the
          diamond key (◆, right of Menu). Render them in physical order so
          mobile users can access them; desktop users can press Alt / CapsLock
          for the same effect. Esc, Tab and Shift are mobile-only (soft
          keyboards don't expose them). */}
      {isSibo && (
        <div className="flex flex-wrap gap-2 items-center justify-center px-4">
          <span className="contents md:hidden">
            {renderKeyBtn('Esc',   EPOC_ESCAPE, 'Escape key')}
            {renderKeyBtn('Tab',   EPOC_TAB,    'Tab key')}
            {renderKeyBtn('Shift', EPOC_SHIFT,  'Shift key')}
          </span>
          {renderKeyBtn('⊔',    EPOC_PSION,   'Psion key')}
          {renderKeyBtn('Menu', EPOC_MENU,    'Menu key')}
          {renderKeyBtn('◆',    EPOC_DIAMOND, 'Diamond key')}
        </div>
      )}

      {/* ── Control bar ── */}
      <div className="flex flex-wrap gap-2 items-center justify-center px-4">

        {/* Power group */}
        <button onClick={paused ? powerOn : powerOff} className={paused ? btnActive : btn}>
          {paused ? 'Power On' : 'Power Off'}
        </button>
        <button onClick={resetDevice} className={btn}>Reset</button>

        {/* The "Enter fullscreen" affordance has moved to the header — see the
            Fullscreen sizing icon next to the device-name label. We keep the
            Exit button here because the header is hidden by the fullscreen
            overlay, so without this (and the corner X on iOS) there'd be no
            way to leave a CSS fullscreen on a touch device with no Escape. */}
        {inFullscreen && (
          <button onClick={() => void toggleFullscreen()} className={btn}>
            Exit Fullscreen
          </button>
        )}

        <button onClick={handleSave} className={btn} disabled={saving}>
          {saving ? 'Saving…' : 'Save State'}
        </button>
        {saveStatus === 'ok' && (
          <span className="text-xs font-mono text-green-600 self-center" aria-live="polite">
            ✓ Saved
          </span>
        )}
        {saveStatus && saveStatus !== 'ok' && (
          <span className="text-xs font-mono text-red-600 self-center" aria-live="polite" title={saveStatus}>
            {saveStatus.length > 60 ? `${saveStatus.slice(0, 57)}…` : saveStatus}
          </span>
        )}

        {savedDevices.includes(currentDeviceId ?? '') && (
          <button
            onClick={() => void revertToSaved()}
            className={btn}
            title="Reload the last saved state, discarding changes since then"
          >
            Revert to saved state
          </button>
        )}

        <button onClick={showKeyboard} className={`${btn} show-on-touch`}>
          Keyboard
        </button>

        {/* On-screen Backlight button. Mirrors the real Fn+Space (Psion+Space
            on the 3mx) keystroke — many host OSes swallow that combo before
            the browser sees it (Cmd+Space → Spotlight on macOS, Super+Space
            → input-method switcher on Linux/Windows), so a button is the only
            reliable trigger. State is the live EPOC backlight pin on
            Windermere; the 3mx falls back to a local toggle because its port
            pin isn't modelled yet. */}
        {backlightColor && (
          <button
            onClick={toggleBacklight}
            className={backlightOn ? expBtnActive : expBtn}
            title={backlightOn
              ? 'Backlight on — click to switch off (also: Fn+Space)'
              : 'Backlight off — click to switch on (also: Fn+Space)'}
            aria-pressed={backlightOn}
          >
            {backlightOn ? 'Light ●' : 'Light'}
          </button>
        )}

        <button
          onClick={() => void pasteFromClipboard()}
          className={btn}
          title="Paste clipboard text into device"
        >
          Paste
        </button>

        {hasCFSlot && <button
          onClick={() => setShowCardDlg(v => !v)}
          className={showCardDlg ? btnActive : (controls.cardAttached ? btnActive : btn)}
          title="Manage the emulated CompactFlash card"
        >
          CF Card{controls.cardAttached ? ' ●' : ''}
        </button>}

        {/* Bootloader-style "insert the OS card" button for 5mx Pro.
            On real hardware the bootloader paints its splash before
            checking for a card; the user inserts their OS-bearing CF
            card when prompted. We mirror that by leaving the slot
            empty at boot and exposing this button to attach the
            bundled OS card on demand. The card stays attached for
            the rest of the session.

            Pulses yellow-to-white via animate-attention-yellow (defined
            in tailwind.config.ts) so it's obvious to the user that
            they need to act on it.

            netBook is NOT included here — useEmulator.ts auto-attaches
            the OS card 1 sim-second after the bootloader splash paints,
            mirroring the real-hardware experience of a 7book booting
            directly into EPOC R5 from internal storage. */}
        {(currentDeviceId === '5mxpro' || currentDeviceId === 'netbook') &&
         !controls.cardAttached &&
         !controls.osCardConsumed && <button
          onClick={() => { if (!controls.osDownloading) void controls.attachOsCard(); }}
          disabled={controls.osDownloading}
          className={[
            'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap',
            'select-none',
            'border border-psion-accent/50 text-psion-charcoal',
            // While the OS image is still downloading, drop the pulsing
            // attention animation and show a wait cursor so the button
            // clearly reads as "working" rather than "ready to click".
            controls.osDownloading
              ? 'cursor-wait opacity-70'
              : ['cursor-pointer animate-attention-yellow',
                 'hover:!bg-psion-accent hover:text-white hover:border-psion-accent',
                 'active:!bg-psion-charcoal active:text-white'].join(' '),
          ].join(' ')}
          title={controls.osDownloading
            ? "Downloading the OS image — the card will be inserted automatically when it finishes"
            : currentDeviceId === 'netbook'
              ? "Insert the OS card so the bootloader can load OS.IMG"
              : "Insert the OS card so the bootloader can load SYS$ROM.BIN"}
        >
          {controls.osDownloading ? 'Downloading OS…' : 'Insert CF card containing OS'}
        </button>}

        {ssdSlotCount > 0 && <button
          onClick={() => setShowSsdDlg(true)}
          className={anySsdAttached ? btnActive : btn}
          title="Manage the emulated SSD pack(s)"
        >
          SSD Pack{anySsdAttached ? ' ●' : ''}
        </button>}

        {datapakSlotCount > 0 && <button
          onClick={() => setShowDatapakDlg(true)}
          className={anyDatapakAttached ? btnActive : btn}
          title="Manage the emulated Datapak / Rampak slots"
        >
          Datapak{anyDatapakAttached ? ' ●' : ''}
        </button>}

        {showRemoteLink && <button
          onClick={() => setShowRemoteLinkDlg(v => !v)}
          className={showRemoteLinkDlg ? btnActive : btn}
          title="Open the host serial bridge to talk PsiWin / PLP"
        >
          Remote Link{showRemoteLinkDlg ? ' ●' : ''}
        </button>}
        {showInfrared && <button
          onClick={() => setShowIrDlg(v => !v)}
          className={showIrDlg ? btnActive : btn}
          title="Send a file to, or receive a file from, the device over emulated infrared"
        >
          Infrared{showIrDlg ? ' ●' : ''}
        </button>}
        {showPrinter && <button
          onClick={() => setShowPrinterDlg(v => !v)}
          className={showPrinterDlg ? btnActive : btn}
          title="Capture what the device prints to its serial port and save it as text, PDF or raw bytes"
        >
          Printer{showPrinterDlg ? ' ●' : ''}
        </button>}

        {/* Modem (simulated internet) — Windermere devices that aren't the
            Revo. Revo currently uses UART2 exclusively for Remote Link;
            keeping the buttons on disjoint devices means the two paths
            never compete for the cable at the UI level. Renders as an
            inline panel below the control bar (alongside the logs
            panel) rather than a modal overlay. Gated by the
            Experimental features setting in the side panel —
            simulated Internet is the roughest-edged path on the device. */}
        {experimentalFeatures &&
         (currentDeviceId === '5mx' || currentDeviceId === '5mxpro' ||
          currentDeviceId === 'mc218' || currentDeviceId === 'series5') && <button
          onClick={() => setShowModemDlg(s => !s)}
          className={showModemDlg ? expBtnActive : expBtn}
          title="Open a simulated dial-up modem on the serial port so the device can connect to a fake internet"
        >
          {showModemDlg ? 'Hide Modem' : 'Show Modem'}
        </button>}

        {/* Speaker / Mic controls have been promoted to the global
            header (App.tsx → AudioControls). Keeping audio errors
            visible inside the view still — they're contextual to the
            currently-running device. */}
      </div>

      {/* Easter-egg hint for the 5mx Pro bootloader. Its bootloader hides a
          credits page: typing "about" at the bootloader prompt lists the team
          that built it. We surface that below the control bar while the
          bootloader is still up, and hide it once the user inserts the OS card
          (osCardConsumed) — by then the bootloader has handed off to the OS and
          the command is gone. Scoped to 5mx Pro; the netBook bootloader has no
          equivalent easter egg. */}
      {currentDeviceId === '5mxpro' &&
       !controls.cardAttached &&
       !controls.osCardConsumed && (
        <div className="px-4 pt-1 text-center text-[11px] font-mono text-psion-charcoal/70">
          There is a bootloader easter egg, type{' '}
          <span className="font-semibold text-psion-charcoal">"about"</span>{' '}
          to see it
        </div>
      )}

      {/* OS-image download progress. Shown only after the user clicks
          "Insert CF card containing OS" while the background fetch of the
          multi-MB OS payload (OS.IMG / SYS$ROM.BIN) is still in flight —
          gives clear feedback on a slow connection instead of an inert
          button. Determinate when the server sends Content-Length;
          otherwise an indeterminate pulsing bar. */}
      {controls.osDownloading && (
        <div className="px-4 pt-1">
          <div className="text-[11px] font-mono text-psion-charcoal/80">
            {currentDeviceId === 'netbook'
              ? 'Downloading OS.IMG'
              : 'Downloading SYS$ROM.BIN'}
            {controls.osDownloadProgress != null
              ? ` — ${Math.round(controls.osDownloadProgress * 100)}%`
              : '…'}
          </div>
          <div className="mt-1 h-1.5 w-full max-w-md overflow-hidden rounded bg-psion-accent/20">
            {controls.osDownloadProgress != null ? (
              <div
                className="h-full bg-psion-accent transition-[width] duration-150 ease-out"
                style={{ width: `${Math.round(controls.osDownloadProgress * 100)}%` }}
              />
            ) : (
              <div className="h-full w-1/3 animate-pulse bg-psion-accent" />
            )}
          </div>
        </div>
      )}

      {deviceInfo?.hasAudio && controls.audioError && (
        <div className="text-xs font-mono text-red-600 px-4">
          Audio: {controls.audioError}
        </div>
      )}

      {/* ── Debug bar ──
          Hidden by default; surfaces only when the user ticks
          "Show debugging" in the side panel's Settings section. Lives
          below the main control bar so the standard buttons stay tidy. */}
      {showDebugging && (
        <div className="flex flex-wrap gap-2 items-center justify-center px-4 pt-1 border-t border-psion-accent/20">
          <span className="text-[10px] font-mono text-gray-400 uppercase tracking-wide mr-1">
            Debug
          </span>
          {lastClick ? (
            <span
              className="font-mono text-[11px] text-lime-400 bg-black/70 border border-psion-accent/30 rounded px-2 py-0.5 cursor-pointer hover:bg-black/90 transition-colors"
              title="Click to copy coordinates to clipboard"
              onClick={() => {
                const text = `img ${lastClick.fx.toFixed(3)} ${lastClick.fy.toFixed(3)} · dig ${lastClick.dx} ${lastClick.dy}`;
                void navigator.clipboard.writeText(text).then(() => {
                  setCoordsCopied(true);
                  window.setTimeout(() => setCoordsCopied(false), 1500);
                });
              }}
            >
              {coordsCopied ? 'Copied!' : (
                <>img&nbsp;{lastClick.fx.toFixed(3)}&nbsp;{lastClick.fy.toFixed(3)}
                &nbsp;·&nbsp;
                dig&nbsp;{lastClick.dx}&nbsp;{lastClick.dy}</>
              )}
            </span>
          ) : (
            <span className="font-mono text-[11px] text-gray-500 italic">
              click device to sample
            </span>
          )}
          <button
            onClick={async () => {
              const canvas = canvasRef.current;
              if (!canvas) {
                alert('No device screen available.');
                return;
              }

              const downloadPng = (source: HTMLCanvasElement) => {
                source.toBlob(blob => {
                  if (!blob) {
                    alert('Failed to capture screenshot.');
                    return;
                  }
                  const url = URL.createObjectURL(blob);
                  const a = document.createElement('a');
                  a.href = url;
                  const ts = new Date().toISOString().replace(/[:.]/g, '-').replace('T', '_').slice(0, 19);
                  a.download = `${currentDeviceId ?? 'psion'}-screenshot-${ts}.png`;
                  document.body.appendChild(a);
                  a.click();
                  document.body.removeChild(a);
                  URL.revokeObjectURL(url);
                }, 'image/png');
              };

              // LCD-only — legacy path.
              if (!includeSurround) {
                downloadPng(canvas);
                return;
              }

              // Surround path: composite skin (+ optional SIBO button bar)
              // onto an offscreen canvas, with the LCD pasted in at the
              // skin layout's screen rectangle. SVG/PNG/JPEG skins are
              // same-origin so the canvas does not become tainted.
              const skinImg = skinImgRef.current;
              const barImg  = siboBarImgRef.current;

              const ensureLoaded = (img: HTMLImageElement | null) =>
                !img || img.complete
                  ? Promise.resolve()
                  : new Promise<void>(res => {
                      const done = () => res();
                      img.addEventListener('load',  done, { once: true });
                      img.addEventListener('error', done, { once: true });
                    });
              await Promise.all([ensureLoaded(skinImg), ensureLoaded(barImg)]);

              // Output width: prefer the skin's natural pixel width so
              // the PNG is at native resolution. Some SVGs report 0 for
              // naturalWidth — fall back to the on-screen container width.
              const skinNaturalW = skinImg?.naturalWidth  ?? 0;
              const skinNaturalH = skinImg?.naturalHeight ?? 0;
              const outW  = skinNaturalW > 0 ? skinNaturalW : cw;
              const skinH = skinNaturalW > 0 && skinNaturalH > 0
                ? skinNaturalH * (outW / skinNaturalW)
                : outW / skinLayout.aspectRatio;

              const barNaturalW = barImg?.naturalWidth  ?? 0;
              const barNaturalH = barImg?.naturalHeight ?? 0;
              const barH = barImg && barNaturalW > 0 && barNaturalH > 0
                ? barNaturalH * (outW / barNaturalW)
                : 0;

              const outCanvas = document.createElement('canvas');
              outCanvas.width  = Math.round(outW);
              outCanvas.height = Math.round(skinH + barH);
              const ctx = outCanvas.getContext('2d');
              if (!ctx) {
                alert('Failed to allocate screenshot canvas.');
                return;
              }

              if (skinImg && skinImg.complete && skinNaturalW > 0) {
                ctx.drawImage(skinImg, 0, 0, outW, skinH);
              }

              // LCD rectangle within the skin.
              const lcdX = skinLayout.screenLeft   * outW;
              const lcdY = skinLayout.screenTop    * skinH;
              const lcdW = skinLayout.screenWidth  * outW;
              const lcdH = skinLayout.screenHeight * skinH;

              if (backlightOn && backlightColor) {
                ctx.fillStyle = backlightColor;
                ctx.fillRect(lcdX, lcdY, lcdW, lcdH);
              }

              ctx.imageSmoothingEnabled = false;
              ctx.drawImage(canvas, lcdX, lcdY, lcdW, lcdH);

              if (barImg && barImg.complete && barNaturalW > 0) {
                ctx.drawImage(barImg, 0, skinH, outW, barH);
              }

              downloadPng(outCanvas);
            }}
            className={btn}
            title="Download a PNG screenshot of the device's LCD"
          >
            Screenshot
          </button>
          {/* Inline "Include surround" modifier — sits as a sibling flex
              item so the Screenshot button itself stays on the same row
              baseline as Logs and Download RAM, and the row's items-center
              vertical centring still works. */}
          <label
            className="flex items-center gap-1 text-[10px] font-mono text-gray-500 select-none cursor-pointer"
            title="Capture the device skin and button bar around the LCD too"
          >
            <input
              type="checkbox"
              checked={includeSurround}
              onChange={e => setIncludeSurround(e.target.checked)}
              className="cursor-pointer"
            />
            Include surround
          </label>
          <button
            onClick={() => {
              const next = !showLogs;
              setShowLogs(next);
              setLoggingEnabled(next);
            }}
            className={showLogs ? btnActive : btn}
          >
            {showLogs ? 'Hide Logs' : 'Show Logs'}
          </button>
          <button
            onClick={async () => {
              const bytes = await getRamSnapshot();
              if (!bytes || bytes.byteLength === 0) {
                alert('No RAM snapshot available for this device.');
                return;
              }
              // Copy into a fresh ArrayBuffer so the Blob constructor sees
              // Uint8Array<ArrayBuffer> rather than Uint8Array<ArrayBufferLike>
              // (the SharedArrayBuffer-aware widening that emscripten's
              // HEAPU8 carries through getRamSnapshot rejects the BlobPart
              // type-check under strict TS).
              const buf = new ArrayBuffer(bytes.byteLength);
              new Uint8Array(buf).set(bytes);
              const url = URL.createObjectURL(new Blob([buf], { type: 'application/octet-stream' }));
              const a = document.createElement('a');
              a.href = url;
              const ts = new Date().toISOString().replace(/[:.]/g, '-').replace('T', '_').slice(0, 19);
              a.download = `${currentDeviceId ?? 'psion'}-ram-${ts}.bin`;
              document.body.appendChild(a);
              a.click();
              document.body.removeChild(a);
              URL.revokeObjectURL(url);
            }}
            className={btn}
            title="Download a binary dump of the emulator's RAM at this moment — useful for sharing a corruption bug"
          >
            Download RAM
          </button>
        </div>
      )}

      {/* ── Inline expandable panels ──
          Ordered top→bottom: CF Card, Remote Link, Modem, Logs. The CF
          card panel used to be a modal dialog; it's now an inline
          collapsible alongside the others so the user can browse the
          card filesystem without losing sight of the device. */}
      {showCardDlg && hasCFSlot && (
        <CFCardDialog controls={controls} onClose={() => setShowCardDlg(false)} />
      )}

      {showSsdDlg && ssdSlotCount > 0 && (
        <SSDDialog controls={controls} slotCount={ssdSlotCount}
                   onClose={() => setShowSsdDlg(false)} />
      )}

      {showDatapakDlg && datapakSlotCount > 0 && (
        <DatapakDialog controls={controls} slotCount={datapakSlotCount}
                       onClose={() => setShowDatapakDlg(false)} />
      )}

      {showRemoteLinkDlg && showRemoteLink && (
        <RemoteLinkDialog controls={controls}
                          uartIndex={remoteLinkUart}
                          protocol={linkProtocol === 2 ? 'rfsv16' : 'rfsv32'}
                          conSeq={remoteLinkConSeq}
                          deviceId={currentDeviceId}
                          onClose={() => setShowRemoteLinkDlg(false)} />
      )}
      {showIrDlg && showInfrared && (
        <InfraredDialog controls={controls}
                        uartIndex={irSendUart}
                        protocol={irProtocol === 2 ? 'sibo' : 'eikon'}
                        onClose={() => setShowIrDlg(false)} />
      )}

      {showPrinterDlg && showPrinter && (
        <PrinterDialog controls={controls}
                       uartIndex={remoteLinkUart}
                       sibo={linkProtocol === 2}
                       viaPcAvailable={linkProtocol === 1}
                       conSeq={remoteLinkConSeq}
                       onClose={() => setShowPrinterDlg(false)} />
      )}

      {showModemDlg && experimentalFeatures &&
       (currentDeviceId === '5mx' || currentDeviceId === '5mxpro' ||
        currentDeviceId === 'mc218' || currentDeviceId === 'series5') && (
        <ModemDialog controls={controls}
                     onClose={() => setShowModemDlg(false)} />
      )}

      {/* ── Log panel ──
          Gated on showDebugging so toggling the setting off hides the
          whole panel even if the user had previously enabled logs. */}
      {showLogs && showDebugging && (
        <div className="w-full max-w-3xl bg-psion-dark border border-psion-accent/40 rounded-lg overflow-hidden flex-shrink-0 mx-4 shadow-sm">
          <div className="flex items-center justify-between px-3 py-2 border-b border-psion-accent/40 bg-psion-mid">
            <span className="text-xs font-mono text-gray-600">
              Debug Log{logs.length > 0 ? ` (${logs.length})` : ''}
            </span>
            <span className="flex items-center gap-3">
              <button
                onClick={() => {
                  if (logs.length === 0) return;
                  const node = logBodyRef.current;
                  if (!node) return;
                  const sel = window.getSelection();
                  if (!sel) return;
                  sel.removeAllRanges();
                  const range = document.createRange();
                  range.selectNodeContents(node);
                  sel.addRange(range);
                }}
                disabled={logs.length === 0}
                className="text-xs font-mono text-gray-400 hover:text-psion-charcoal disabled:opacity-40 disabled:hover:text-gray-400 transition-colors"
                title="Select every log line so you can copy them in one go"
              >
                Select all
              </button>
              <button
                onClick={() => {
                  if (logs.length === 0) return;
                  const text = logs.join('\n') + '\n';
                  const blob = new Blob([text], { type: 'text/plain;charset=utf-8' });
                  const url = URL.createObjectURL(blob);
                  const a = document.createElement('a');
                  a.href = url;
                  const ts = new Date().toISOString().replace(/[:.]/g, '-').replace('T', '_').slice(0, 19);
                  a.download = `${currentDeviceId ?? 'psion'}-log-${ts}.txt`;
                  document.body.appendChild(a);
                  a.click();
                  document.body.removeChild(a);
                  URL.revokeObjectURL(url);
                }}
                disabled={logs.length === 0}
                className="text-xs font-mono text-gray-400 hover:text-psion-charcoal disabled:opacity-40 disabled:hover:text-gray-400 transition-colors"
                title="Download the current log buffer as a text file"
              >
                Download
              </button>
              <button
                onClick={clearLogs}
                className="text-xs font-mono text-gray-400 hover:text-amber-700 transition-colors"
              >
                Clear
              </button>
            </span>
          </div>
          <div ref={logBodyRef} className="h-48 overflow-y-auto p-2 space-y-px bg-gray-50">
            {logs.length === 0 ? (
              <p className="text-gray-400 font-mono text-xs italic">No log output yet…</p>
            ) : (
              logs.map((line, i) => (
                <div
                  key={i}
                  className="font-mono text-xs text-green-700 whitespace-pre-wrap break-all leading-4"
                >
                  {line}
                </div>
              ))
            )}
            <div ref={logEndRef} />
          </div>
        </div>
      )}
      </>}{/* /chromeless gate */}
    </div>
  );
}
