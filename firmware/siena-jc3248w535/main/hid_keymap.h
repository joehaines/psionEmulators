// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Turns USB/Bluetooth HID keyboard reports into Siena key presses.
//
// Keys that exist on the Siena (letters, digits, arrows, Enter, Esc, Tab,
// Del, Space, Shift, Ctrl, Psion) are held for as long as the host key is,
// so auto-repeat and Shift/Ctrl/Psion chords behave as on the real
// keyboard. Symbols are translated by character, because the Siena's
// legends differ from a PC's: host Shift+8 is '*', which on the Siena is
// its own unshifted key. Those are sent as one complete tap with whatever
// Shift state the Siena needs, whatever the host Shift is doing.
//
// The host layout is assumed to be US, the usual for Bluetooth keyboards.
//
// Host → Siena extras:
//   F1..F8      System, Data, Word, Agenda, Time, World, Calc, Sheet
//   F9          Menu         F10  Help
//   Ctrl        Menu when tapped on its own, Ctrl in a chord
//   Alt / Win   Psion in a chord, diamond when tapped on its own
//   F11         the same as Alt
//   Caps Lock   diamond
//   Delete      Shift+Del (delete right)
//   Home/End    Psion+Left / Psion+Right
//   PgUp/PgDn   Psion+Up / Psion+Down
//
// Portable: no ESP-IDF dependency.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

class KeyPlayer;

class HidKeymap {
public:
    explicit HidKeymap(KeyPlayer &player) : m_player(player) {}

    // A boot-protocol keyboard report: modifier byte, reserved byte, up to
    // six usage codes. `len` may be 8 (boot) or longer (report protocol
    // keyboards append nothing the Siena could use).
    void onReport(const uint8_t *report, size_t len);

    // Release everything — on disconnect, so no key sticks down.
    void releaseAll();

    // Offered every key press first, as the Siena key and modifiers it
    // would be; returning true keeps it from the Siena.
    std::function<bool(int key, uint8_t mods)> filter;
    // Offered first the character a key types (US layout), if it types
    // one: returning true keeps the key from the filter and the Siena.
    std::function<bool(char ch)> textFilter;
    // While captured, nothing reaches the Siena (keys still go to the
    // filter); capturing releases whatever the Siena had held.
    void setCaptured(bool captured);

private:
    void hostModsChanged(uint8_t mods);
    void keyDown(uint8_t usage, uint8_t mods);
    void keyUp(uint8_t usage);

    KeyPlayer &m_player;
    static constexpr uint8_t kF11 = 0x44;
    static constexpr int kTaken = -1;   // a Held key the filter took
    uint8_t m_mods = 0;
    uint8_t m_sienaMods = 0;  // the modifiers pressed on the Siena
    bool    m_captured = false;
    uint8_t m_soloMods = 0;   // Ctrl / Psion down with no key joining them
    uint8_t m_keys[6] = {};
    // Siena key (and any Psion/Shift it was chorded with) each held usage
    // pressed, so its release undoes exactly what its press did.
    struct Held { uint8_t usage; int key; uint8_t extra; };
    Held m_held[6] = {};
};
