// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Typing from a serial terminal on the laptop (idf.py monitor, miniterm,
// screen ...) over the board's USB port. A terminal sends characters and
// VT100 escape sequences rather than key presses, so each one becomes a
// complete tap of the matching Siena key(s):
//   printable characters               the key, with Shift as the Siena needs,
//                                       or Ctrl + the character's code; UTF-8
//                                       £ and common accents too
//   Enter, Tab, Backspace, Esc         Enter, Tab, Del, Esc
//   arrow keys                          arrows
//   Home/End/PgUp/PgDn                  Psion + Left/Right/Up/Down
//   Delete                              Shift+Del
//   F1..F8                              System, Data, Word, Agenda, Time,
//                                       World, Calc, Sheet
//   F9 / F10                            Menu / Help
//   Ctrl+letter                         Ctrl+letter
//   Shift+F1..F10                       the board's shortcuts (see ui.h)
//
// Portable: no ESP-IDF dependency.

#pragma once

#include <cstdint>
#include <functional>
#include <string>

class KeyPlayer;

class TerminalKeys {
public:
    explicit TerminalKeys(KeyPlayer &player) : m_player(player) {}

    // Feed one received byte, received at wall-clock time nowMs.
    void onByte(uint8_t b, int64_t nowMs);
    // Resolve a lone Esc once no sequence has followed it for a while.
    void poll(int64_t nowMs);

    // Offered every key first, as a Siena key (letters upper case) and
    // modifiers; returning true keeps it from the Siena.
    std::function<bool(int key, uint8_t mods)> filter;
    // Offered printable characters before anything else.
    std::function<bool(char ch)> textFilter;

private:
    void sequence(const std::string &seq);
    void tap(int key, uint8_t mods = 0);

    KeyPlayer  &m_player;
    std::string m_esc;          // bytes after ESC, while a sequence is open
    bool        m_inEsc = false;
    int64_t     m_escAt = 0;
    bool        m_lastCR = false;
    uint8_t     m_utf8Lead = 0;     // pending first byte of a UTF-8 pair
};
