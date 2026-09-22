// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Maps browser KeyboardEvent input to EPOC key codes, per device.
// EPOC key values are from the EpocKey enum in core/emubase.h.
//
// ── Why this is device-aware ─────────────────────────────────────────────
// Every Psion family has a different keyboard, and the same printed symbol is
// typed with a different key + modifier on each.  Crucially, almost no symbol
// has a dedicated matrix slot: it's the Shift, Psion (◆/AltGr) or Fn legend
// of a key that IS on the matrix.  The device's own keyboard ROM turns
// (base key + modifier) into the character, so the frontend's whole job is to
// send the right base key with the right modifier(s) held.
//
// The per-layout tables below are transcribed from MAME's INPUT_PORTS for
// each machine (PORT_CHAR order = unshifted, shifted, AltGr/Psion) and, for
// the Series 7 / netBook, from NetBSD's epoc32 epockbdmap.h (the UK keymap).
// They are the authoritative key legends, and they match what each emulated
// device's ROM produces — so coverage is as complete as the real hardware:
//
//   • epoc32  — Series 5 / 5mx / 5mxPro / MC218 / Osaris / Revo.  Symbols are
//     the blue Fn legends on the alphabetic keys, plus Shift+digit.
//   • sa1100  — Series 7 / netBook.  A near-full UK laptop keyboard: digits
//     unshifted, the usual Shift legends, and an Fn (AltGr) layer for
//     brackets, =, -, etc.  Different layout from the 5mx.
//   • sibo-classic / sibo-3a — Series 3 and Series 3a/3c/3mx/Pocket Book.
//     Three layers: unshifted, Shift, and the Psion (◆) key for the bracket
//     / brace / hash / tilde family on the digit row.
//   • sibo-siena — Siena.  A reduced SIBO layout (no Psion symbol layer).
//   • sibo-workabout — Workabout / WorkaboutMX.  Compact membrane keyboard;
//     ','/'.'+ '+'/'=' are reversed relative to the other SIBO machines, and
//     it has a full Psion layer.
//   • hc120  — HC120.  An industrial keypad: A-Z in rows of six, a
//     numeric keypad down the right, and no comma or semicolon at all.
//     Digits and the arithmetic symbols are unshifted; Shift gives the
//     second legend printed above each key.
//   • mc400  — MC400 / MC200.  A full laptop keyboard with standard UK Shift
//     legends (no Psion symbol layer).
//   • organiser2 — Organiser II.  A modal alpha keypad with NO dedicated
//     digit or symbol keys: every digit and symbol is Shift + a letter key
//     (Shift+S = ';', Shift+Y = '0', …).  Letters therefore must NOT be
//     Shift-wrapped for upper-case (Shift selects the symbol layer); case is
//     governed by the device's own CAP mode.
//   • organiser1 — Organiser I.  Also a modal alpha keypad, but the layer
//     is picked by the device's MODE key rather than by Shift: in CALC
//     mode the O key types '1', in ENTER mode it types 'O'.  So digits and
//     symbols map to the BARE key that carries the legend (no Shift), and
//     the machine decides which it is.

// ── Modifier EPOC codes used when building chords ─────────────────────────
const SHIFT = 18; // EStdKeyLeftShift
const PSION = 20; // EStdKeyLeftAlt — the Psion / ◆ "AltGr" modifier on SIBO
const FN    = 24; // EStdKeyLeftFunc — the Fn / Mode_switch (AltGr) modifier

// Maps browser KeyboardEvent.code strings to EPOC key codes for keys that
// are the SAME EPOC code on every device — navigation, editing, modifiers
// and the function row.  These EStdKey constants are accepted (or harmlessly
// ignored) by every device matrix, so they don't need to be device-aware.
//
// Symbol-bearing codes (Comma, Period, Quote, Semicolon, Slash, Minus, …)
// are deliberately NOT here: they fall through to the device-aware
// character path (charToEpocChord) so each family gets the right chord.
export const keymap: Record<string, number> = {
  Backspace:      1,
  Tab:            2,
  Enter:          3,
  Escape:         4,
  Space:          5,
  Home:           8,
  End:            9,
  PageUp:         10,
  PageDown:       11,
  Insert:         12,
  Delete:         13,
  ArrowLeft:      14,
  ArrowRight:     15,
  ArrowUp:        16,
  ArrowDown:      17,
  ShiftLeft:      18,
  ShiftRight:     19,
  // Host Alt sends EStdKeyLeftAlt / EStdKeyRightAlt rather than Menu.
  // SIBO devices (Series 3 / 3a / 3c / 3mx / Siena / MC400) treat Alt as
  // their hardware "Psion" modifier (used with a letter to launch apps).
  // Windermere/Series 5mx has no Psion key — it routes EStdKeyLeftAlt
  // through to the same matrix slot as its dedicated Menu key, so host
  // Alt still opens the Menu there. The dedicated Menu host key (where
  // present) still maps via EStdKeyMenu = 148 below if needed.
  AltLeft:        20,  // EStdKeyLeftAlt → Psion modifier on SIBO; Menu on Series 5mx
  AltRight:       21,  // EStdKeyRightAlt — same routing
  ControlLeft:    22,
  ControlRight:   23,
  MetaLeft:       24,  // Fn key
  MetaRight:      25,
  CapsLock:       26,
  NumLock:        27,
  ScrollLock:     28,
  F1:             96,
  F2:             97,
  F3:             98,
  F4:             99,
  F5:             100,
  F6:             101,
  F7:             102,
  F8:             103,
  F9:             104,
  F10:            105,
  F11:            106,
  F12:            107,
};

// ── Keyboard layout selection ─────────────────────────────────────────────
export type KeyboardLayout =
  | 'epoc32'         // Series 5 / 5mx / 5mxPro / MC218 / Osaris / Revo
  | 'sa1100'         // Series 7 / netBook
  | 'mc400'          // MC400
  | 'sibo-classic'   // Series 3
  | 'sibo-3a'        // Series 3a / 3c / 3mx / Pocket Book / Pocket Book II
  | 'sibo-siena'     // Siena
  | 'sibo-workabout' // Workabout / WorkaboutMX
  | 'hc120'          // HC120
  | 'organiser1'     // Organiser I
  | 'organiser2';    // Organiser II

const SIBO_3A_IDS = new Set([
  'series3a', 'series3c', 'series3mx', 'pocketbk', 'pocketbk2',
]);

// Resolves a device id (as used by core/device_registry.cpp) to its layout.
export function keyboardLayoutForDevice(deviceId: string | null | undefined): KeyboardLayout {
  switch (deviceId) {
    case 'organiser1':            return 'organiser1';
    case 'organiser2':            return 'organiser2';
    case 'hc120':                 return 'hc120';
    case 'mc200':
    case 'mc400':
    case 'mc400v126':             return 'mc400';
    case 'series3':               return 'sibo-classic';
    case 'siena':                 return 'sibo-siena';
    case 'workabout':
    case 'workaboutmx':           return 'sibo-workabout';
    case 'series7':
    case 'netbook':               return 'sa1100';
    default:
      return deviceId && SIBO_3A_IDS.has(deviceId) ? 'sibo-3a' : 'epoc32';
  }
}

// ── Per-layout symbol tables ──────────────────────────────────────────────
// Each entry is [modifiers, baseKey]: hold `modifiers`, press `baseKey` (the
// EPOC code of the matrix key the symbol lives on).  Letters, digits and
// whitespace are handled generically in charToEpocChord and are not repeated
// here (except on the Organiser, whose digits ARE shifted letters).
type SymEntry = readonly [number[], number];
// Modifier shorthands (shared array refs — never mutated).
const _N: number[] = [];
const _S = [SHIFT];
const _P = [PSION];
const _F = [FN];

// EPOC codes for the three keys SIBO/MC400 matrices expose for , . and digits.
const C_COMMA = 44, C_STOP = 46;
const D = { '0': 48, '1': 49, '2': 50, '3': 51, '4': 52, '5': 53, '6': 54, '7': 55, '8': 56, '9': 57 };

// Series 5 / 5mx family (Windermere & CL-PS7111).  Blue Fn legends + Shift.
//
// Transcribed from MAME's psion5mx INPUT_PORTS (the UK keyboard), where each
// matrix key carries up to three legends in PORT_CHAR order: unshifted, Shift,
// Fn.  The base key code we send is the EStdKey of that matrix key — for
// letters that's the uppercase ASCII code, for digits the ASCII digit, and for
// the three dedicated punctuation keys it's EStdKeySingleQuote(126),
// EStdKeyFullStop(122) and EStdKeyComma(121).  The device ROM turns
// (base key + modifier) into the printed character, so every symbol below is
// "hold <modifier>, press <the matrix key that bears the legend>".
//
// The quote key follows MAME's UK `revo` variant ' / @ / : (so @ stays on
// Shift+quote, the standard UK position, matching " on Shift+2); ~ is therefore
// the Fn legend of the 4 key.
const EPOC32: Record<string, SymEntry> = {
  // Dedicated punctuation keys (unshifted).
  ',': [_N, 121], '.': [_N, 122], "'": [_N, 126],
  // Shift + digit row.
  '!': [_S, 49], '"': [_S, 50], '£': [_S, 51], '$': [_S, 52], '%': [_S, 53],
  '^': [_S, 54], '&': [_S, 55], '*': [_S, 56], '(': [_S, 57], ')': [_S, 48],
  // Shift + dedicated punctuation keys: @ on the quote key, ? on '.', / on ','.
  '@': [_S, 126], '?': [_S, 122], '/': [_S, 121],
  // Fn + digit row.
  '_': [_F, 49], '#': [_F, 50], '\\': [_F, 51], '~': [_F, 52], '<': [_F, 53],
  '>': [_F, 54], '[': [_F, 55], ']': [_F, 56], '{': [_F, 57], '}': [_F, 48],
  // Fn + letter / quote keys: : on the quote key, ; = - + on L P O I.
  ':': [_F, 126], ';': [_F, 76], '=': [_F, 80], '-': [_F, 79], '+': [_F, 73],
};

// Series 7 / netBook (UK), from NetBSD epockbdmap.h epockbd_keysym_uk over _us.
// Base keys: digits, letters P/O/I/L, plus EStdKeySemiColon(125)=apostrophe
// key, EStdKeyFullStop(122), EStdKeyComma(121).  Fn = AltGr (Mode_switch).
const SA1100: Record<string, SymEntry> = {
  ',': [_N, 121], '.': [_N, 122], "'": [_N, 125],
  '!': [_S, 49], '"': [_S, 50], '£': [_S, 51], '$': [_S, 52], '%': [_S, 53],
  '^': [_S, 54], '&': [_S, 55], '*': [_S, 56], '(': [_S, 57], ')': [_S, 48],
  '~': [_S, 125], '?': [_S, 122], '/': [_S, 121],
  '>': [_F, 54], '<': [_F, 53], '@': [_F, 52], '\\': [_F, 51], '#': [_F, 50],
  '_': [_F, 49], '}': [_F, 48], '{': [_F, 57], ']': [_F, 56], '[': [_F, 55],
  '=': [_F, 80], '-': [_F, 79], '+': [_F, 122], ':': [_F, 125], ';': [_F, 76],
};

// Shared Series 3 / 3a digit-row layers (psion3 & psion3a are identical here).
const SIBO3_DIGIT_SHIFT: Record<string, SymEntry> = {
  '!': [_S, D['1']], '"': [_S, D['2']], '£': [_S, D['3']], '$': [_S, D['4']],
  '%': [_S, D['5']], '^': [_S, D['6']], '&': [_S, D['7']], '?': [_S, D['8']],
  '(': [_S, D['9']], ')': [_S, D['0']],
};
const SIBO3_DIGIT_PSION: Record<string, SymEntry> = {
  '#': [_P, D['2']], '\\': [_P, D['3']], '~': [_P, D['4']], "'": [_P, D['5']],
  '@': [_P, D['6']], '{': [_P, D['7']], '}': [_P, D['8']], '[': [_P, D['9']],
  ']': [_P, D['0']],
};
// Shared Series 3 / 3a punctuation keys ('+' is unshifted, '=' is Shift).
const SIBO3_PUNCT: Record<string, SymEntry> = {
  '/': [_N, 47], '-': [_N, 45], '+': [_N, 43], '*': [_N, 42],
  ',': [_N, C_COMMA], '.': [_N, C_STOP],
  '_': [_S, 45], '=': [_S, 43], ':': [_S, 42], '<': [_S, C_COMMA], '>': [_S, C_STOP],
};

// Series 3 (classic).  Slash's Shift legend is '?', and there is no ';' key.
const SIBO_CLASSIC: Record<string, SymEntry> = {
  ...SIBO3_DIGIT_SHIFT, ...SIBO3_DIGIT_PSION, ...SIBO3_PUNCT,
};
// Series 3a / 3c / 3mx / Pocket Book.  Adds ';' as Shift + the slash key.
const SIBO_3A: Record<string, SymEntry> = {
  ...SIBO3_DIGIT_SHIFT, ...SIBO3_DIGIT_PSION, ...SIBO3_PUNCT,
  ';': [_S, 47],
};

// Siena.  Dedicated ; / - = + * keys; Shift adds : ? _ < >.  No Psion layer;
// the Shift+digit symbols are the standard UK legends (best-effort — MAME's
// natural-keyboard map leaves the Siena digit row unshifted).
const SIBO_SIENA: Record<string, SymEntry> = {
  ';': [_N, 59], '/': [_N, 47], '-': [_N, 45], '=': [_N, 61], '+': [_N, 43],
  '*': [_N, 42], ',': [_N, C_COMMA], '.': [_N, C_STOP],
  ':': [_S, 59], '?': [_S, 47], '_': [_S, 45], '<': [_S, C_COMMA], '>': [_S, C_STOP],
  '!': [_S, D['1']], '"': [_S, D['2']], '£': [_S, D['3']], '$': [_S, D['4']],
  '%': [_S, D['5']], '^': [_S, D['6']], '&': [_S, D['7']], '(': [_S, D['9']], ')': [_S, D['0']],
};

// Workabout / WorkaboutMX.  '.' is unshifted and ',' is Shift+'.'; '+' is
// unshifted and '=' is Shift; full Psion layer on the digit row.
const SIBO_WORKABOUT: Record<string, SymEntry> = {
  '/': [_N, 47], '-': [_N, 45], '+': [_N, 43], '*': [_N, 42], '.': [_N, C_STOP],
  '?': [_S, 47], '_': [_S, 45], '=': [_S, 43], ':': [_S, 42], ',': [_S, C_STOP],
  ';': [_S, D['0']], '!': [_S, D['1']], '"': [_S, D['2']], '£': [_S, D['3']],
  '$': [_S, D['4']], '%': [_S, D['5']], '^': [_S, D['6']], '&': [_S, D['7']],
  '(': [_S, D['8']], ')': [_S, D['9']],
  '<': [_P, D['0']], '#': [_P, D['2']], '\\': [_P, D['3']], '~': [_P, D['4']],
  '{': [_P, D['5']], '}': [_P, D['6']], "'": [_P, D['7']], '[': [_P, D['8']], ']': [_P, D['9']],
};

// MC400.  Full laptop keyboard, standard UK Shift legends, no Psion layer.
const MC400: Record<string, SymEntry> = {
  ';': [_N, 59], "'": [_N, 126], ',': [_N, C_COMMA], '.': [_N, C_STOP],
  '/': [_N, 47], '-': [_N, 45], '=': [_N, 61], '[': [_N, 91], ']': [_N, 93],
  '#': [_N, 35], '\\': [_N, 92],
  ':': [_S, 59], '@': [_S, 126], '<': [_S, C_COMMA], '>': [_S, C_STOP], '?': [_S, 47],
  '_': [_S, 45], '+': [_S, 61], '{': [_S, 91], '}': [_S, 93], '~': [_S, 35], '|': [_S, 92],
  '!': [_S, D['1']], '"': [_S, D['2']], '£': [_S, D['3']], '$': [_S, D['4']], '%': [_S, D['5']],
  '^': [_S, D['6']], '&': [_S, D['7']], '*': [_S, D['8']], '(': [_S, D['9']], ')': [_S, D['0']],
};

// Organiser II.  Digits AND symbols are the Shift legend of a letter key
// (from MAME psion2 INPUT_PORTS).  Letters are handled separately (no Shift).
const ORGANISER2: Record<string, SymEntry> = {
  '0': [_S, 89 /*Y*/], '1': [_S, 85 /*U*/], '2': [_S, 86 /*V*/], '3': [_S, 87 /*W*/],
  '4': [_S, 79 /*O*/], '5': [_S, 80 /*P*/], '6': [_S, 81 /*Q*/], '7': [_S, 73 /*I*/],
  '8': [_S, 74 /*J*/], '9': [_S, 75 /*K*/],
  ';': [_S, 83 /*S*/], ',': [_S, 77 /*M*/], '=': [_S, 71 /*G*/], '<': [_S, 65 /*A*/],
  ':': [_S, 84 /*T*/], '$': [_S, 78 /*N*/], '"': [_S, 72 /*H*/], '>': [_S, 66 /*B*/],
  '(': [_S, 67 /*C*/], '%': [_S, 69 /*E*/], '+': [_S, 88 /*X*/], '-': [_S, 82 /*R*/],
  '*': [_S, 76 /*L*/], '/': [_S, 70 /*F*/], '.': [_S, 90 /*Z*/], ')': [_S, 68 /*D*/],
};

// HC120.  The keypad carries the digits and the four arithmetic symbols
// as unshifted keys (7 8 9 /, 4 5 6 *, 1 2 3 -, 0 . +), with %, @ and \
// as the Shift legends of / * and -.  There is no comma, semicolon or
// quote key on the machine — those characters cannot be typed, so they
// are absent here rather than mapped to something that would send the
// wrong key.  Read off the machine: see core/series3.cpp::hc120KeyMatrix.
const HC120: Record<string, SymEntry> = {
  '.': [_N, C_STOP], '/': [_N, 47], '*': [_N, 42], '-': [_N, 45], '+': [_N, 43],
  '%': [_S, 47], '@': [_S, 42], '\\': [_S, 45],
};

// Organiser I.  Its digits and symbols are the second legend on the letter
// keys, as on the Organiser II — but this machine picks the layer by MODE,
// not by Shift.  In CALC mode the same key that types O types 1; holding
// Shift over it types nothing at all (the ROM discards the combination),
// which is why these entries carry NO modifier: the host's '1' presses the
// key that bears the 1, and the device's own mode decides what it means.
// Verified against the ROM: MODE MODE MODE (to CALC) then O X P EXECUTE
// leaves "CALC:1+2=3" on the panel.  Key positions from MAME psion1
// INPUT_PORTS.
const ORGANISER1: Record<string, SymEntry> = {
  '0': [_N, 85 /*U*/], '1': [_N, 79 /*O*/], '2': [_N, 80 /*P*/], '3': [_N, 81 /*Q*/],
  '4': [_N, 73 /*I*/], '5': [_N, 74 /*J*/], '6': [_N, 75 /*K*/], '7': [_N, 67 /*C*/],
  '8': [_N, 68 /*D*/], '9': [_N, 69 /*E*/],
  ',': [_N, 83 /*S*/], '%': [_N, 77 /*M*/], '=': [_N, 71 /*G*/], '<': [_N, 65 /*A*/],
  '(': [_N, 89 /*Y*/], ':': [_N, 84 /*T*/], '$': [_N, 78 /*N*/], '"': [_N, 72 /*H*/],
  '>': [_N, 66 /*B*/], ')': [_N, 90 /*Z*/], '+': [_N, 88 /*X*/], '-': [_N, 82 /*R*/],
  '*': [_N, 76 /*L*/], '/': [_N, 70 /*F*/], '.': [_N, 86 /*V*/],
};

const LAYOUT_SYMBOLS: Record<KeyboardLayout, Record<string, SymEntry>> = {
  'epoc32':         EPOC32,
  'sa1100':         SA1100,
  'mc400':          MC400,
  'sibo-classic':   SIBO_CLASSIC,
  'sibo-3a':        SIBO_3A,
  'sibo-siena':     SIBO_SIENA,
  'sibo-workabout': SIBO_WORKABOUT,
  'hc120':          HC120,
  'organiser1':     ORGANISER1,
  'organiser2':     ORGANISER2,
};

// Holds an EPOC key sequence that should be sent to produce a single character.
export interface KeyChord {
  // Modifier keys to hold down across the chord (Shift=18, Fn=24, etc).
  modifiers: number[];
  // The main key to press once.
  key: number;
}

// True when a keystroke belongs to a text field of the HOST UI rather than to
// the emulated machine.
//
// EmulatorView listens for keydown on `window` and calls preventDefault() for
// every key the device's keymap can produce, so without this check a keystroke
// typed into a dialog field is cancelled before the browser inserts the
// character — the field looks frozen and the letter goes to EPOC instead.
// (Verified in Chromium: typing "CAFE" into an unguarded dialog input leaves
// the field empty and delivers CAFE to the guest.)
//
// The emulator's own hidden textarea — the one mobile soft keyboards type
// into, marked data-psion-input — is deliberately NOT host UI: its keystrokes
// must keep reaching EPOC, so it is excluded from the match.
export function isHostTextEntry(target: EventTarget | null): boolean {
  const el = target as HTMLElement | null;
  if (!el || typeof el.closest !== 'function') return false;
  const field = el.closest<HTMLElement>(
    'input, textarea, select, [contenteditable=""], [contenteditable="true"]');
  return field != null && field.dataset.psionInput == null;
}

// Returns the EPOC key chord (modifiers + key) needed to produce `char` on the
// given device layout, or null if the device's keyboard can't produce it.
// Used by physical-key fallback, paste, and mobile soft-keyboard input.
export function charToEpocChord(char: string, layout: KeyboardLayout = 'epoc32'): KeyChord | null {
  if (char.length !== 1) return null;

  // Whitespace / control — same EPOC codes on every device.
  if (char === ' ')                   return { modifiers: [], key: 5 }; // EStdKeySpace
  if (char === '\n' || char === '\r') return { modifiers: [], key: 3 }; // EStdKeyEnter
  if (char === '\t')                  return { modifiers: [], key: 2 }; // EStdKeyTab

  const upper = char.toUpperCase();
  const code  = upper.charCodeAt(0);

  if (layout === 'organiser1' || layout === 'organiser2') {
    // Letters: NO Shift — Shift selects the digit/symbol layer on this
    // keyboard, and upper/lower case is governed by the device's CAP mode.
    if (code >= 65 && code <= 90) return { modifiers: [], key: code };
    const e = LAYOUT_SYMBOLS[layout][char];
    return e ? { modifiers: e[0], key: e[1] } : null; // digits live here too
  }

  // Letters — EPOC key code == uppercase ASCII; uppercase needs Shift.
  if (code >= 65 && code <= 90) {
    const isUpper = char >= 'A' && char <= 'Z';
    return { modifiers: isUpper ? [SHIFT] : [], key: code };
  }
  // Digits — unshifted on every (non-Organiser) keyboard.
  if (code >= 48 && code <= 57) return { modifiers: [], key: code };

  const e = LAYOUT_SYMBOLS[layout][char];
  return e ? { modifiers: e[0], key: e[1] } : null;
}

// Returns the EPOC chord for a browser KeyboardEvent on the given device.
// Code-based entries (physical nav/modifier/function keys) carry no modifier —
// the OS tracks the physical Shift state.  Everything else (letters and all
// symbols) goes through the device-aware character path.
export function browserKeyToEpocChord(
  event: KeyboardEvent,
  layout: KeyboardLayout = 'epoc32',
): KeyChord | null {
  if (event.code in keymap) return { modifiers: [], key: keymap[event.code] };
  return charToEpocChord(event.key, layout);
}
