// license:BSD-3-Clause
// copyright-holders:Sandro Ronco (MAME psion driver)
//                  + adaptation by the Psion emulator project, 2026.
//
// Psion Organiser II driver — wires the HD6303X CPU + HD44780 LCD +
// two Datapak/Rampak slots into an EmuBase subclass.
//
// Memory map (LZ / LZ64 model, derived from MAME `psionla_mem`):
//   $0000-$001F   HD6303X internal register window (handled by CPU)
//   $0020-$00FF   HD6303X internal RAM (handled by CPU)
//   $0100-$03FF   memory-mapped I/O (LCD, beeper, keyboard, datapaks,
//                 bank-select latches)
//   $0400-$3FFF   RAM bank 1 (16 KiB window, banked)
//   $4000-$7FFF   RAM bank 2 (16 KiB window, banked)
//   $8000-$BFFF   ROM bank 2 (16 KiB window, banked into 4-page space)
//   $C000-$FFFF   ROM bank 1 (16 KiB window; reset vectors live here)
//
// LZ64 has 64 KiB of physical RAM; the bank latches choose which
// 16 KiB slice maps into the two RAM windows. The 64 KiB ROM image is
// split into four 16 KiB pages; the bank latches choose which page
// maps into the two ROM windows. Bank latch addresses live inside
// the $0100-$03FF I/O block — exact registers come from MAME's
// `psion.cpp` `io_w` body and will be filled in when the CPU port
// reaches the first OUT-to-bank instruction.

#pragma once

#include "emubase.h"
#include "hd6303.h"
#include "hd44780.h"
#include "psion_datapak.h"

#include <array>

namespace Organiser2 {

class Emulator : public EmuBase, public HD6303Bus {
public:
    Emulator();
    ~Emulator() override = default;

    HD6303 *getHd6303Cpu() override { return &cpu; }
    const HD6303 *getHd6303Cpu() const override { return &cpu; }

    // ── EmuBase overrides ───────────────────────────────────────────
    uint8_t *getROMBuffer() override { return rom.data(); }
    size_t   getROMSize()   override { return rom.size(); }
    void     loadROM(uint8_t *buffer, size_t size) override;
    void     executeUntil(int64_t cycles) override;

    uint8_t *getRamBuffer()       override { return ram.data(); }
    size_t   getRamSize()   const override { return ram.size(); }
    void     loadRamSnapshot(const uint8_t *bytes, size_t size) override;

    int32_t     getClockSpeed() const override { return E_CLOCK_HZ; }
    const char *getDeviceName() const override { return "Psion Organiser II"; }

    // The LZ panel is 120x36 active dots (4 rows × 20 cols, 6×9 per
    // cell — see HD44780.h). We report a 4x upscale so the frontend
    // has a comfortable viewing size: 480x144.
    int getDigitiserWidth()  const override { return PIXEL_W * 4; }
    int getDigitiserHeight() const override { return PIXEL_H * 4; }
    int getLCDOffsetX()      const override { return 0; }
    int getLCDOffsetY()      const override { return 0; }
    int getLCDWidth()        const override { return PIXEL_W * 4; }
    int getLCDHeight()       const override { return PIXEL_H * 4; }

    void readLCDIntoBuffer(uint8_t **lines, bool is32BitOutput) const override;
    void dumpLcdDdram() const;
    void dumpStateTable() const;
    void setKeyboardKey(EpocKey key, bool value) override;
    void updateTouchInput(int32_t /*x*/, int32_t /*y*/, bool /*down*/) override {}

    // ── Datapak slots (Pack A = slot 0, Pack B = slot 1) ────────────
    int  getDatapakSlotCount() const override { return 2; }
    bool attachDatapak(int slot, const uint8_t *bytes, size_t size) override;
    void detachDatapak(int slot) override;
    bool   isDatapakInserted(int slot)   const override;
    size_t getDatapakImageSize(int slot) const override;
    const uint8_t *getDatapakImageData(int slot) const override;
    PackKind getDatapakKind(int slot) const override;

    // ── HD6303Bus implementation ────────────────────────────────────
    uint8_t readByte(uint16_t addr) override;
    void    writeByte(uint16_t addr, uint8_t v) override;

private:
    // 3.6864 MHz crystal / 4 = 921.6 kHz E-clock (per MAME
    // HD6303X(config, m_maincpu, 3.6864_MHz_XTAL)).
    static constexpr int32_t E_CLOCK_HZ = 921'600;

    static constexpr int PIXEL_W = HD44780::PIXEL_W;   // 80
    static constexpr int PIXEL_H = HD44780::PIXEL_H;   // 32

    static constexpr size_t ROM_SIZE = 0x10000;        // 64 KiB
    static constexpr size_t RAM_SIZE = 0x10000;        // 64 KiB (LZ64)

    HD6303       cpu{*this};
    HD44780      lcd;
    PsionDatapak pack[2];

    std::array<uint8_t, ROM_SIZE> rom{};
    std::array<uint8_t, RAM_SIZE> ram{};

    // ROM bank table (LZ/LZ64 layout per MAME psion.cpp machine_start):
    //
    //   rombank1 (FIXED, never switched): CPU $C000-$FFFF →
    //     ROM image offset $4000-$7FFF (page 1; holds reset vectors).
    //   rombank2 entry 0: ROM image offset $0000-$3FFF (page 0)
    //   rombank2 entry 1: ROM image offset $8000-$BFFF (page 2)
    //   rombank2 entry 2: ROM image offset $C000-$FFFF (page 3)
    //
    // Bank-select via I/O writes inside $0100-$03FF (decoded in ioWrite):
    //   $2E0: increment rombank2 (clamped at last entry)
    //   $260: reset both rambank2 and rombank2 to entry 0
    //   $2A0: increment rambank2 (clamped at m_ramBankCount-1)
    //
    // Initial entries are 0 — the OS resets to a known state at $C000
    // and pages other ROM segments in via increment writes as needed.
    static constexpr uint16_t kRomBank1Base = 0x4000; // page 1 fixed
    static constexpr uint8_t  kRomBank2Count = 3;     // pages 0, 2, 3
    static constexpr uint8_t  kRamBank2Count = 4;     // LZ64 = 4 banks of 16 KiB

    uint8_t m_romBank2 = 0;   // ROM bank index visible at $8000-$BFFF
    uint8_t m_ramBank2 = 0;   // RAM bank index visible at $4000-$7FFF

    // Side-effect state per MAME's psion2_state::io_rw:
    //   m_kbCounter — keyboard column scan index. Reset by writes to
    //                 $200; incremented by reads/writes anywhere in
    //                 $240-$25F or $261-$27F. The keyboard data byte
    //                 read via the HD6303's port 6 is m_keyRow[m_kbCounter].
    //   m_nmiEnable — set by $280 (anywhere except $2A0); cleared by
    //                 $2C0 (anywhere except $2E0) or $0C0 (standby).
    //   m_pulseEnable — toggled by $100 (on) / $140 (off). Drives a
    //                 pulse-output pin used by the OS for timing; we
    //                 latch it but don't drive any peripheral yet.
    //   m_beep — on/off via $180 / $1C0. Latched only.
    // 11-bit counter (rolls over at 0x800). The keyboard scan in
    // port5Reader matches it against 7-bit one-hot-zero patterns;
    // the full-width counter also drives the bit-1 rollover indicator
    // when at $7FF.
    uint16_t m_kbCounter  = 0;
    bool    m_nmiEnable   = false;
    bool    m_pulseEnable = false;
    bool    m_beep        = false;

    // NMI tick scheduler. MAME wires a 1 Hz periodic timer that pulses
    // INPUT_LINE_NMI when m_nmi_enable is set; this drives the OS's
    // wake-up / keyboard-scan / time-of-day path. We track cycles
    // since the last NMI and fire on the next executeUntil tick that
    // crosses the 1-second boundary.
    int64_t m_nextNmiCycles = E_CLOCK_HZ;  // first NMI at t=1s
    // rambank1 ($0400-$3FFF) is fixed at the top of physical RAM minus
    // the 16 KiB rambank1 window. The first 1 KiB of the window ($0400)
    // is taken from RAM offset 0; rambank2 entries 0..N tile after.

    // Keyboard: 7-row matrix (K1-K7) per MAME's INPUT_PORTS_START(psion2).
    // Each row uses bit positions $04, $08, $10, $20, $40 (5 columns
    // per row, 35 total + the separate ON line = 36 keys). All bits are
    // IP_ACTIVE_LOW: default 0xFF means "no keys pressed in this row";
    // pressing a key clears its bit. The HD6303's Port 6 read returns
    // m_keyRow[m_kbCounter]; the OS increments m_kbCounter via reads in
    // the $241-$25F or $261-$27F I/O range to walk through all 7 rows.
    //
    // The ON / CLEAR key is wired to a separate input that drives NMI
    // directly on real hardware (the OS uses it to wake from power-off).
    // Stored in m_onKey; checked by the NMI scheduler.
    uint8_t m_keyRow[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    bool    m_onKey    = false;
    bool    m_onKeyEdgeFired = false;

    bool m_initialised = false;

    // Memory-mapped I/O dispatch helpers ($0100-$03FF window).
    uint8_t  ioRead(uint16_t addr);
    void     ioWrite(uint16_t addr, uint8_t v);
    void     ioSideEffect(uint16_t off);
};

} // namespace Organiser2
