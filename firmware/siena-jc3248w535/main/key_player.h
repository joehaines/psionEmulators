// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Plays key presses into the emulated keyboard matrix on the emulator's own
// clock. Touch and the USB terminal both produce events far faster (or far
// shorter) than the Siena's keyboard scan can see, so every press is held
// for at least kMinHoldMs of *emulated* time and taps are spaced apart —
// the same thing a finger on the real keys does.
//
// Portable: no ESP-IDF dependency, so host/ can drive it too.

#pragma once

#include <cstdint>
#include <deque>

class EmuBase;

// Modifier bits for KeyPlayer::tap().
enum : uint8_t {
    kModShift = 1,
    kModCtrl  = 2,
    kModPsion = 4,
};

class KeyPlayer {
public:
    static constexpr int64_t kMinHoldMs = 60;   // press → release
    static constexpr int64_t kGapMs     = 40;   // release → next press

    // Queue a press or release of one matrix key (an EpocKey code).
    void press(int key);
    void release(int key);
    // Queue a complete tap: modifiers down, key down, hold, key up,
    // modifiers up.
    void tap(int key, uint8_t mods = 0);
    // Type one character (code page 850, the Siena's character set): its
    // own key where the Siena has one, otherwise Ctrl + its three-digit
    // code, which EPOC16 accepts anywhere text can be typed.
    void typeChar(int ch);
    // Hold back whatever is queued after this for `ms` of emulated time:
    // a macro's pause while a dialog opens.
    void wait(int ms);
    // Hold back what follows until the Siena has gone idle (its CPU halted
    // at the end of nearly every update for half a second; an idle app
    // still wakes now and then to blink its cursor), or `maxMs` passes:
    // a macro's pause while the Siena works, however long that takes.
    void waitIdle(int maxMs = 60000);

    // Apply every step that is due at emulated time nowMs.
    void update(EmuBase &emu, int64_t nowMs);

    bool idle() const { return m_steps.empty(); }

private:
    static constexpr int kWait = -1;           // Steps that are pauses
    static constexpr int kWaitIdle = -2;
    static constexpr int kIdleWindow = 50;     // idle: halted at this many recent updates
    static constexpr int kIdleHalted = 45;     // ... at least this many
    struct Step { int key; bool down; int waitMs; };
    std::deque<Step> m_steps;
    int64_t m_waitUntil = -1;
    uint64_t m_idleHistory = 0;                // halted at each recent update, newest bit 0
    int      m_idleSeen = 0;
    int64_t m_lastPressMs[256] = {};
    int64_t m_lastReleaseMs = -1000;
};
