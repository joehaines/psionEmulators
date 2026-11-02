// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The emulated Siena: the repo's Series3c::Emulator in its Siena
// configuration, plus the bits the board needs around it — the LCD as
// levels and a millisecond clock on emulated time.
//
// Portable: no ESP-IDF dependency, so host/ boots the same machine.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

class EmuBase;

class SienaMachine {
public:
    static constexpr int     kLcdW = 240, kLcdH = 160;
    static constexpr int64_t kClockHz = 7'680'000;

    // `storage`, if given, is where the emulator object is built: at least
    // emulatorSize() bytes. The firmware passes internal RAM, so the CPU
    // and ASIC state the interpreter touches on every instruction stay out
    // of PSRAM.
    explicit SienaMachine(void *storage = nullptr);
    ~SienaMachine();
    static size_t emulatorSize();

    // Load the 1 MiB ROM image and reset. `language` picks the locale the
    // v4.20f image boots in (0 UK, 1 USA, 2 Swedish, 3 Spanish).
    void loadRom(const uint8_t *rom, size_t size, int language = 0);

    // Run until emulated time reaches `ms`.
    void runUntilMs(int64_t ms);
    int64_t nowMs() const;

    // The LCD as kLcdW x kLcdH levels: 0 paper, 1 ink, 2 grey. Returns true,
    // having filled `levels`, if anything changed since the last call;
    // otherwise `levels` is left as it was.
    bool readLcd(uint8_t *levels);

    // Everything but the RAM (emu().getRamBuffer()) and ROM, between
    // slices: with the same RAM and this, a machine built by the same
    // firmware resumes exactly where this one was. Loading releases all keys.
    void saveState(std::vector<uint8_t> &out);
    bool loadState(const uint8_t *data, size_t size);

    // A Flash SSD in the Siena's slot (drive A:), read in place from
    // `pack` (which must stay mapped), before the Siena first runs. The
    // Siena's writes to it are kept apart: saveState carries them, and
    // so do these, for when only the RAM is restored.
    bool attachPack(const uint8_t *pack, size_t size);
    void savePackWrites(std::vector<uint8_t> &out);
    bool loadPackWrites(const uint8_t *data, size_t size);

    EmuBase &emu() { return *m_emu; }

private:
    EmuBase *m_emu;
    bool     m_placed;
    uint8_t *m_rows[kLcdH];
    uint8_t *m_grey;     // kLcdW * kLcdH, as readLCDIntoBuffer writes it
    uint8_t *m_last;     // previous levels, for change detection
    uint8_t *m_vram;     // the framebuffer as last decoded
    int      m_framesSinceDecode = 0;
};
