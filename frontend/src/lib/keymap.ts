// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Maps browser KeyboardEvent.code strings to EPOC key codes.
// EPOC key values are from EpocKey enum in core/emubase.h.
//
// IMPORTANT: the Psion 5mx (and Osaris) physical keyboard matrix only contains
// a limited set of keys (see core/windermere.cpp Emulator::setKeyboardKey).
// Letters, digits, Comma, FullStop, SingleQuote, modifiers, arrows, Tab,
// Enter, Esc, Space, Backspace are present.  Other symbols (/, \, ;, =, -, +,
// [, ], {, }, :, <, >, ?, #, @, *, &, |, ^, ~) are NOT on the matrix and must
// be produced via Fn+letter or Shift+number combinations.  Mapping a host key
// to an EPOC code that isn't in the matrix has no effect — so we deliberately
// omit those keys from the physical-key map and let them fall through to the
// character-based fallback (charToEpocChord) which sends the right chord.

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
  // Symbol keys present in the Psion 5mx matrix.
  Comma:          121, // EStdKeyComma
  Period:         122, // EStdKeyFullStop
  Quote:          126, // EStdKeySingleQuote
  // NOTE: Slash, Backslash, Semicolon, Backquote, BracketLeft, BracketRight,
  // Minus, Equal are intentionally NOT mapped — those EPOC codes don't exist
  // in the 5mx hardware matrix.  Pressing those keys is handled by the
  // character-based fallback (charToEpocChord) which sends the appropriate
  // Fn+letter or Shift+key combination instead.
};

// Fn-key combinations on the Psion 5mx (UK) keyboard for symbols that lack a
// dedicated key.  EStdKeyLeftFunc = 24.  These mirror the symbols printed in
// blue on the alphabetic keys of a real Psion 5mx.  The value is the EPOC key
// code of the letter to combine with Fn.
//
// If a particular symbol comes out wrong on the device, edit the entry here.
const FN = 24;
const fnSymbols: Record<string, number> = {
  ';':  /* S */ 83,
  ':':  /* L */ 76,
  '<':  /* , */ 121,
  '>':  /* . */ 122,
  '/':  /* H */ 72,
  '\\': /* W */ 87,
  '?':  /* Q */ 81,
  '=':  /* E */ 69,
  '+':  /* R */ 82,
  '*':  /* T */ 84,
  '-':  /* M */ 77,
  '[':  /* I */ 73,
  ']':  /* O */ 79,
};

// Holds an EPOC key sequence that should be sent to produce a single character.
export interface KeyChord {
  // Modifier keys to hold down across the chord (Shift=18, Fn=24, etc).
  modifiers: number[];
  // The main key to press once.
  key: number;
}

// Returns the EPOC key chord (modifiers + key) needed to produce `char`,
// or null if no mapping exists.  Used for paste and mobile input injection.
export function charToEpocChord(char: string): KeyChord | null {
  if (char.length !== 1) return null;

  // Direct (no modifier) keys present in the matrix.
  const direct: Record<string, number> = {
    ' ': 5, ',': 121, '.': 122, "'": 126,
    '\n': 3, '\r': 3, '\t': 2,
  };
  if (char in direct) return { modifiers: [], key: direct[char] };

  // Shift+number → punctuation (Psion 5mx UK shifted-digit row).
  // EPOC codes for digits are their ASCII values.
  const shiftedNumber: Record<string, number> = {
    '!': 49,  // Shift+1
    '"': 50,  // Shift+2
    '$': 52,  // Shift+4
    '%': 53,  // Shift+5
    '^': 54,  // Shift+6
    '&': 55,  // Shift+7
    '(': 57,  // Shift+9
    ')': 48,  // Shift+0
    '@': 126, // Shift+' (UK Psion layout)
  };
  if (char in shiftedNumber) return { modifiers: [18], key: shiftedNumber[char] };

  // Fn+letter → symbol (Psion 5mx UK Fn-secondary layer).
  if (char in fnSymbols) return { modifiers: [FN], key: fnSymbols[char] };

  // Letters — EPOC key code == uppercase ASCII; uppercase needs Shift.
  const upper = char.toUpperCase();
  const code  = upper.charCodeAt(0);
  if (code >= 65 && code <= 90) {
    const isUpper = char >= 'A' && char <= 'Z';
    return { modifiers: isUpper ? [18] : [], key: code };
  }

  // Digits — EPOC key code == ASCII.
  if (code >= 48 && code <= 57) return { modifiers: [], key: code };

  return null;
}

// Returns the EPOC chord for a browser KeyboardEvent.  Code-based entries
// (physical keys) carry no modifier — the OS tracks the physical Shift state.
// Character-based fallback returns the full chord including modifiers.
export function browserKeyToEpocChord(event: KeyboardEvent): KeyChord | null {
  if (event.code in keymap) return { modifiers: [], key: keymap[event.code] };
  return charToEpocChord(event.key);
}
