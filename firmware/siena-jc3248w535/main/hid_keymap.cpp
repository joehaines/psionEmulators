// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "hid_keymap.h"

#include "emubase.h"
#include "key_player.h"

#include <cstring>

namespace {

// HID modifier byte → the three Siena modifiers.
uint8_t sienaMods(uint8_t hid) {
    uint8_t m = 0;
    if (hid & 0x22) m |= kModShift;   // LShift / RShift
    if (hid & 0x11) m |= kModCtrl;    // LCtrl / RCtrl
    if (hid & 0xCC) m |= kModPsion;   // LAlt / LGUI / RAlt / RGUI
    return m;
}

int modKey(uint8_t mod) {
    switch (mod) {
    case kModShift: return EStdKeyLeftShift;
    case kModCtrl:  return EStdKeyLeftCtrl;
    default:        return EStdKeyLeftAlt;   // Psion
    }
}

// A key the Siena has, held for as long as the host key is. `extra` is a
// Siena modifier the host key implies (Delete is Shift+Del).
struct Direct { int key; uint8_t extra; };

bool directKey(uint8_t u, bool shifted, Direct &out) {
    out.extra = 0;
    if (u >= 0x04 && u <= 0x1D) { out.key = 'A' + (u - 0x04); return true; }
    // Digits and the punctuation keys whose unshifted legend matches a
    // Siena key; shifted, they're symbols (see hostChar).
    if (!shifted) {
        if (u >= 0x1E && u <= 0x26) { out.key = '1' + (u - 0x1E); return true; }
        switch (u) {
        case 0x27: out.key = '0'; return true;
        case 0x2D: out.key = '-'; return true;
        case 0x2E: out.key = '='; return true;
        case 0x33: out.key = ';'; return true;
        case 0x36: out.key = ','; return true;
        case 0x37: out.key = '.'; return true;
        case 0x38: out.key = '/'; return true;
        }
    }
    switch (u) {
    case 0x28: case 0x58: out.key = EStdKeyEnter;      return true;  // Enter, keypad Enter
    case 0x29: out.key = EStdKeyEscape;                return true;
    case 0x2A: out.key = EStdKeyBackspace;             return true;  // Siena Del
    case 0x2B: out.key = EStdKeyTab;                   return true;
    case 0x2C: out.key = EStdKeySpace;                 return true;
    case 0x39: out.key = EStdKeyCapsLock;              return true;
    case 0x4F: out.key = EStdKeyRightArrow;            return true;
    case 0x50: out.key = EStdKeyLeftArrow;             return true;
    case 0x51: out.key = EStdKeyDownArrow;             return true;
    case 0x52: out.key = EStdKeyUpArrow;               return true;
    case 0x4C: out.key = EStdKeyBackspace; out.extra = kModShift; return true; // Delete
    case 0x4A: out.key = EStdKeyLeftArrow;  out.extra = kModPsion; return true; // Home
    case 0x4D: out.key = EStdKeyRightArrow; out.extra = kModPsion; return true; // End
    case 0x4B: out.key = EStdKeyUpArrow;    out.extra = kModPsion; return true; // PgUp
    case 0x4E: out.key = EStdKeyDownArrow;  out.extra = kModPsion; return true; // PgDn
    case 0x42: out.key = EStdKeyMenu;                  return true;  // F9
    case 0x43: out.key = EStdKeyHelp;                  return true;  // F10
    case 0x65: out.key = EStdKeyMenu;                  return true;  // Application
    case 0x55: out.key = '*';                          return true;  // keypad *
    case 0x56: out.key = '-';                          return true;  // keypad -
    case 0x57: out.key = '+';                          return true;  // keypad +
    case 0x54: out.key = '/';                          return true;  // keypad /
    case 0x62: out.key = '0';                          return true;  // keypad 0
    case 0x63: out.key = '.';                          return true;  // keypad .
    }
    if (u >= 0x59 && u <= 0x61) { out.key = '1' + (u - 0x59); return true; } // keypad 1-9
    if (u >= 0x3A && u <= 0x41) {                      // F1..F8: app keys
        out.key = EStdKeyF1 + (u - 0x3A);
        return true;
    }
    return false;
}

// What a US-layout host key types, for the keys that aren't a Siena key
// as they stand: the shifted digit row and punctuation, and the
// punctuation the Siena has no key for at all.
char hostChar(uint8_t u, bool shifted) {
    static const char kDigitRow[] = "!@#$%^&*()";
    if (u >= 0x1E && u <= 0x27) return shifted ? kDigitRow[u - 0x1E] : 0;
    switch (u) {
    case 0x2D: return shifted ? '_' : '-';
    case 0x2E: return shifted ? '+' : '=';
    case 0x2F: return shifted ? '{' : '[';
    case 0x30: return shifted ? '}' : ']';
    case 0x31: return shifted ? '|' : '\\';
    case 0x32: return shifted ? '~' : '#';   // non-US # key
    case 0x33: return shifted ? ':' : ';';
    case 0x34: return shifted ? '"' : '\'';
    case 0x35: return shifted ? '~' : '`';
    case 0x36: return shifted ? '<' : ',';
    case 0x37: return shifted ? '>' : '.';
    case 0x38: return shifted ? '?' : '/';
    }
    return 0;
}

} // namespace

void HidKeymap::onReport(const uint8_t *report, size_t len) {
    if (len < 3) return;
    uint8_t mods = report[0];
    uint8_t raw[6] = {};
    const size_t n = len - 2 < 6 ? len - 2 : 6;
    std::memcpy(raw, report + 2, n);
    // 0x01 in every slot is the keyboard's "too many keys" rollover error.
    if (raw[0] == 0x01) return;
    // F11 is a second Psion key, held like one: fold it into the modifier
    // byte as LAlt.
    uint8_t keys[6] = {};
    for (int i = 0, k = 0; i < 6; ++i) {
        if (raw[i] == kF11) mods |= 0x04;
        else keys[k++] = raw[i];
    }

    auto contains = [](const uint8_t *set, uint8_t u) {
        for (int i = 0; i < 6; ++i) if (set[i] == u) return true;
        return false;
    };
    // Releases first, then modifier changes, then presses: Shift+A then
    // releasing Shift before A must still leave a capital A.
    for (int i = 0; i < 6; ++i)
        if (m_keys[i] && !contains(keys, m_keys[i])) keyUp(m_keys[i]);
    hostModsChanged(mods);
    for (int i = 0; i < 6; ++i)
        if (keys[i] && !contains(m_keys, keys[i])) keyDown(keys[i], mods);
    std::memcpy(m_keys, keys, sizeof m_keys);
}

void HidKeymap::releaseAll() {
    m_soloMods = 0;
    for (int i = 0; i < 6; ++i)
        if (m_keys[i]) keyUp(m_keys[i]);
    std::memset(m_keys, 0, sizeof m_keys);
    hostModsChanged(0);
}

void HidKeymap::setCaptured(bool captured) {
    if (captured && !m_captured) {
        for (Held &h : m_held) {
            if (!h.usage || h.key == kTaken) continue;
            m_player.release(h.key);
            if (h.extra) m_player.release(modKey(h.extra));
            h.key = kTaken;   // its release is no longer the Siena's
        }
        for (uint8_t m : { kModShift, kModCtrl, kModPsion })
            if (m_sienaMods & m) m_player.release(modKey(m));
        m_sienaMods = 0;
        m_soloMods = 0;
    }
    m_captured = captured;
}

void HidKeymap::hostModsChanged(uint8_t mods) {
    const uint8_t was = sienaMods(m_mods), now = sienaMods(mods);
    m_mods = mods;
    if (m_captured) return;
    for (uint8_t m : { kModShift, kModCtrl, kModPsion }) {
        if ((now & m) && !(m_sienaMods & m)) m_player.press(modKey(m));
        if (!(now & m) && (m_sienaMods & m)) m_player.release(modKey(m));
    }
    m_sienaMods = now;
    // Ctrl tapped on its own is Menu, and Psion (Alt / Win / F11) is the
    // diamond key, which the Siena's own Psion and Ctrl keys don't do
    // alone. Chords still reach the Siena as Ctrl / Psion; the tap
    // follows a solo press's release.
    for (uint8_t m : { kModCtrl, kModPsion }) {
        if ((now & m) && !(was & m)) m_soloMods |= m;
        if (!(now & m) && (was & m) && (m_soloMods & m)) {
            m_soloMods &= uint8_t(~m);
            m_player.tap(m == kModCtrl ? EStdKeyMenu : EStdKeyCapsLock);
        }
    }
}

void HidKeymap::keyDown(uint8_t usage, uint8_t mods) {
    m_soloMods = 0;
    const uint8_t held = sienaMods(mods);
    // The character it types, for text the board is taking.
    char typed = hostChar(usage, held & kModShift);
    if (usage >= 0x04 && usage <= 0x1D) typed = char((held & kModShift ? 'A' : 'a') + usage - 0x04);
    else if (usage >= 0x1E && usage <= 0x27 && !(held & kModShift)) typed = "1234567890"[usage - 0x1E];
    else if (usage == 0x2C) typed = ' ';
    if (typed && !(held & (kModCtrl | kModPsion)) && textFilter && textFilter(typed)) {
        for (Held &h : m_held) {
            if (!h.usage) { h = { usage, kTaken, 0 }; break; }
        }
        return;
    }
    Direct d;
    const bool direct = directKey(usage, held & kModShift, d);
    if ((direct && filter && filter(d.key, held | d.extra)) || m_captured) {
        for (Held &h : m_held) {
            if (!h.usage) { h = { usage, kTaken, 0 }; break; }
        }
        return;
    }
    if (direct) {
        // Only add the implied modifier if the host isn't already holding
        // it, so releasing the key can't drop a modifier the host still holds.
        const uint8_t extra = d.extra & ~held;
        if (extra) m_player.press(modKey(extra));
        m_player.press(d.key);
        for (Held &h : m_held) {
            if (!h.usage) { h = { usage, d.key, extra }; break; }
        }
        return;
    }
    // A symbol: typed as one complete character, with the Siena's own
    // Shift state for it, so host Shift is lifted for the duration.
    const char ch = hostChar(usage, held & kModShift);
    if (!ch) return;
    if (held & kModShift) m_player.release(EStdKeyLeftShift);
    m_player.typeChar(ch);
    if (held & kModShift) m_player.press(EStdKeyLeftShift);
}

void HidKeymap::keyUp(uint8_t usage) {
    for (Held &h : m_held) {
        if (h.usage != usage) continue;
        if (h.key == kTaken) { h = {}; return; }
        m_player.release(h.key);
        if (h.extra) m_player.release(modKey(h.extra));
        h = {};
        return;
    }
}
