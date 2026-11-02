// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "key_player.h"

#include "emubase.h"
#include "v30.h"

#include "siena_chars.h"

void KeyPlayer::press(int key)   { m_steps.push_back({ key & 0xFF, true, 0 }); }
void KeyPlayer::release(int key) { m_steps.push_back({ key & 0xFF, false, 0 }); }
void KeyPlayer::wait(int ms)     { m_steps.push_back({ kWait, false, ms }); }
void KeyPlayer::waitIdle(int maxMs) { m_steps.push_back({ kWaitIdle, false, maxMs }); }

void KeyPlayer::tap(int key, uint8_t mods) {
    if (mods & kModShift) press(EStdKeyLeftShift);
    if (mods & kModCtrl)  press(EStdKeyLeftCtrl);
    if (mods & kModPsion) press(EStdKeyLeftAlt);
    press(key);
    release(key);
    if (mods & kModPsion) release(EStdKeyLeftAlt);
    if (mods & kModCtrl)  release(EStdKeyLeftCtrl);
    if (mods & kModShift) release(EStdKeyLeftShift);
}

void KeyPlayer::typeChar(int ch) {
    int key; bool shift;
    if (ch >= 'a' && ch <= 'z')      tap(ch - 'a' + 'A');
    else if (ch >= 'A' && ch <= 'Z') tap(ch, kModShift);
    else if (ch >= '0' && ch <= '9') tap(ch);
    else if (ch == ' ')              tap(EStdKeySpace);
    else if (sienaChar(char(ch), key, shift)) tap(key, shift ? kModShift : 0);
    else if (ch > ' ' && ch < 256) {
        press(EStdKeyLeftCtrl);
        for (int div = 100; div; div /= 10) {
            const int digit = '0' + (ch / div) % 10;
            press(digit);
            release(digit);
        }
        release(EStdKeyLeftCtrl);
    }
}

void KeyPlayer::update(EmuBase &emu, int64_t nowMs) {
    while (!m_steps.empty()) {
        const Step s = m_steps.front();
        if (s.key == kWait || s.key == kWaitIdle) {
            if (m_waitUntil < 0) {
                m_waitUntil = nowMs + s.waitMs;
                m_idleHistory = 0;
                m_idleSeen = 0;
            }
            if (s.key == kWaitIdle) {
                m_idleHistory = (m_idleHistory << 1) | (emu.getV30Cpu()->halted ? 1 : 0);
                ++m_idleSeen;
                const uint64_t recent = m_idleHistory & ((uint64_t(1) << kIdleWindow) - 1);
                const bool idle = m_idleSeen >= kIdleWindow &&
                                  __builtin_popcountll(recent) >= kIdleHalted;
                if (!idle && nowMs < m_waitUntil) return;
            } else if (nowMs < m_waitUntil) {
                return;
            }
            m_waitUntil = -1;
            m_steps.pop_front();
            continue;
        }
        if (s.down) {
            // A new press waits out the gap after the previous release, so
            // two taps of the same key read as two keystrokes.
            if (nowMs < m_lastReleaseMs + kGapMs) return;
            m_lastPressMs[s.key] = nowMs;
        } else {
            if (nowMs < m_lastPressMs[s.key] + kMinHoldMs) return;
            m_lastReleaseMs = nowMs;
        }
        emu.setKeyboardKey(static_cast<EpocKey>(s.key), s.down);
        m_steps.pop_front();
    }
}
