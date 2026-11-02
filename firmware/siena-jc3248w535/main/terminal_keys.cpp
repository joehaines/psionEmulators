// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "terminal_keys.h"

#include "emubase.h"
#include "key_player.h"
#include "siena_chars.h"

namespace {

constexpr int64_t kLoneEscMs = 60;

// The accented letters and symbols a UK/European terminal is likely to
// send, from Latin-1 to code page 850 (the Siena's character set).
int latin1ToCp850(int c) {
    switch (c) {
    case 0xA3: return 156;  // £
    case 0xE9: return 130;  // é
    case 0xE8: return 138;  // è
    case 0xEA: return 136;  // ê
    case 0xE0: return 133;  // à
    case 0xE2: return 131;  // â
    case 0xE4: return 132;  // ä
    case 0xE7: return 135;  // ç
    case 0xF6: return 148;  // ö
    case 0xFC: return 129;  // ü
    case 0xF1: return 164;  // ñ
    case 0xDF: return 225;  // ß
    case 0xE1: return 160;  // á
    case 0xED: return 161;  // í
    case 0xF3: return 162;  // ó
    case 0xFA: return 163;  // ú
    case 0xC4: return 142;  // Ä
    case 0xD6: return 153;  // Ö
    case 0xDC: return 154;  // Ü
    case 0xC9: return 144;  // É
    }
    return 0;
}

} // namespace

void TerminalKeys::onByte(uint8_t b, int64_t nowMs) {
    if (m_inEsc) {
        m_esc.push_back(char(b));
        // CSI/SS3 sequences end on a letter or '~'; give up on anything long.
        const bool done = m_esc.size() >= 2 &&
            ((b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z') || b == '~');
        if (m_esc.size() == 1 && b != '[' && b != 'O') {
            // ESC + key: Alt+key in most terminals → Psion+key.
            m_inEsc = false;
            int key; bool shift;
            if (b >= 'a' && b <= 'z')      tap(b - 'a' + 'A', kModPsion);
            else if (b >= 'A' && b <= 'Z') tap(b, kModPsion | kModShift);
            else if (sienaChar(char(b), key, shift))
                tap(key, kModPsion | (shift ? kModShift : 0));
            m_esc.clear();
        } else if (done || m_esc.size() > 6) {
            m_inEsc = false;
            sequence(m_esc);
            m_esc.clear();
        }
        return;
    }

    const bool wasCR = m_lastCR;
    m_lastCR = (b == '\r');
    if (b == 0x1B) {
        m_inEsc = true;
        m_escAt = nowMs;
    } else if (b == '\r' || (b == '\n' && !wasCR)) {
        tap(EStdKeyEnter);
    } else if (b == '\n') {
        // The LF of a CR LF pair.
    } else if (b == '\t') {
        tap(EStdKeyTab);
    } else if (b == 0x7F || b == 0x08) {
        tap(EStdKeyBackspace);
    } else if (b >= 1 && b <= 26) {
        tap('A' + b - 1, kModCtrl);
    } else if (b >= 0xC2 && b <= 0xC3) {
        m_utf8Lead = b;             // first byte of a Latin-1 character
        return;
    } else if (b >= 0x80 && b <= 0xBF && m_utf8Lead) {
        const int cp850 = latin1ToCp850(((m_utf8Lead & 0x1F) << 6) | (b & 0x3F));
        if (cp850) m_player.typeChar(cp850);
    } else if (b >= ' ' && b < 0x7F) {
        const int key = (b >= 'a' && b <= 'z') ? b - 'a' + 'A' : b;
        if (textFilter && textFilter(char(b))) {
        } else if (!(filter && filter(key, 0))) {
            m_player.typeChar(b);
        }
    }
    m_utf8Lead = 0;
}

void TerminalKeys::poll(int64_t nowMs) {
    if (m_inEsc && m_esc.empty() && nowMs - m_escAt >= kLoneEscMs) {
        m_inEsc = false;
        tap(EStdKeyEscape);
    }
}

void TerminalKeys::sequence(const std::string &s) {
    struct Seq { const char *text; int key; uint8_t mods; };
    static const Seq kSeqs[] = {
        { "[A", EStdKeyUpArrow, 0 },     { "OA", EStdKeyUpArrow, 0 },
        { "[B", EStdKeyDownArrow, 0 },   { "OB", EStdKeyDownArrow, 0 },
        { "[C", EStdKeyRightArrow, 0 },  { "OC", EStdKeyRightArrow, 0 },
        { "[D", EStdKeyLeftArrow, 0 },   { "OD", EStdKeyLeftArrow, 0 },
        { "[H", EStdKeyLeftArrow, kModPsion },  { "OH", EStdKeyLeftArrow, kModPsion },
        { "[1~", EStdKeyLeftArrow, kModPsion },
        { "[F", EStdKeyRightArrow, kModPsion }, { "OF", EStdKeyRightArrow, kModPsion },
        { "[4~", EStdKeyRightArrow, kModPsion },
        { "[5~", EStdKeyUpArrow, kModPsion },   { "[6~", EStdKeyDownArrow, kModPsion },
        { "[3~", EStdKeyBackspace, kModShift },
        { "OP", EStdKeyF1, 0 }, { "OQ", EStdKeyF2, 0 }, { "OR", EStdKeyF3, 0 },
        { "OS", EStdKeyF4, 0 },
        { "[11~", EStdKeyF1, 0 }, { "[12~", EStdKeyF2, 0 }, { "[13~", EStdKeyF3, 0 },
        { "[14~", EStdKeyF4, 0 }, { "[15~", EStdKeyF5, 0 }, { "[17~", EStdKeyF6, 0 },
        { "[18~", EStdKeyF7, 0 }, { "[19~", EStdKeyF8, 0 }, { "[20~", EStdKeyMenu, 0 },
        { "[21~", EStdKeyHelp, 0 },
        // Shifted, as xterm-style terminals send them: the board's shortcuts.
        { "[1;2P", EStdKeyF1, kModShift }, { "[1;2Q", EStdKeyF2, kModShift },
        { "[1;2R", EStdKeyF3, kModShift }, { "[1;2S", EStdKeyF4, kModShift },
        { "[15;2~", EStdKeyF5, kModShift }, { "[17;2~", EStdKeyF6, kModShift },
        { "[18;2~", EStdKeyF7, kModShift }, { "[19;2~", EStdKeyF8, kModShift },
        { "[21;2~", EStdKeyHelp, kModShift },
    };
    for (const Seq &q : kSeqs) {
        if (s == q.text) { tap(q.key, q.mods); return; }
    }
}

void TerminalKeys::tap(int key, uint8_t mods) {
    if (filter && filter(key, mods)) return;
    m_player.tap(key, mods);
}
