// license:BSD-3-Clause
// copyright-holders:Sandro Ronco (MAME psion driver)
//                  + adaptation by the Psion emulator project, 2026.
//
// Psion Organiser I (1984) driver — the machine that started the line:
// a Hitachi HD6301X0 single-chip micro with its program in the CPU's own
// 4 KiB mask ROM, 2 KiB of external RAM, one row of 16 characters on an
// HD44780 character LCD, and the two Datapak slots the Organiser II later
// inherited.
//
// Memory map (derived from MAME `psion1_state::psion1_mem`):
//   $0000-$001F   HD6301X0 internal register window (handled by the CPU)
//   $0040-$00FF   HD6301X0 internal RAM (192 bytes, handled by the CPU)
//   $2000-$2001   HD44780: RS = A0, so $2000 = command, $2001 = data.
//                 Mirrored every 2 bytes up to $27FF.
//   $2800         read: reset the keyboard scan counter
//   $2E00         read: switch off — the CPU goes into standby until the
//                 ON key resets it
//   $3000         read: increment the keyboard scan counter
//   $4000-$47FF   2 KiB external RAM (the whole of the machine's RAM)
//   $F000-$FFFF   the HD6301X0's internal mask ROM — roms/Organiser1.rom,
//                 which is MAME's `psion1` ROM byte for byte. The 6800-
//                 family interrupt vectors live in its last 18 bytes.
//
// Everything else reads as 0, the way an unmapped address does in MAME.
//
// Off-CPU-bus wiring (shared with the Organiser II, `psion_state`):
//   Port 2  datapak data bus (read + write)
//   Port 5  b7 ON key (active high), b6-b2 keyboard rows (active low),
//           b1 pulse / scan-counter rollover, b0 battery (0 = good)
//   Port 6  datapak control lines (slot select, OE, PGM, RES, CLK)
//
// Two timers drive the machine: the CPU's own on-chip counter, and a
// 500 ms periodic NMI (MAME wires it as a discrete timer_device with a
// 1 s start delay) which the OS uses for its clock and its keyboard
// scan. Unlike the Organiser II, the Organiser I has no software switch
// for that NMI — `psion1_state::machine_reset` enables it and nothing
// ever turns it off — so it runs from reset until the machine is
// switched off.

#pragma once

#include "emubase.h"
#include "hd6303.h"
#include "hd44780.h"
#include "psion_datapak.h"

#include <array>

namespace Organiser1 {

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
    const char *getDeviceName() const override { return "Psion Organiser I"; }

    // The panel is one row of 16 characters — 96x8 active dots. That is
    // too small to look at on a modern display, so we report a 6x
    // upscale (576x48), the same idea as the Organiser II's 4x.
    int getDigitiserWidth()  const override { return PIXEL_W * SCALE; }
    int getDigitiserHeight() const override { return PIXEL_H * SCALE; }
    int getLCDOffsetX()      const override { return 0; }
    int getLCDOffsetY()      const override { return 0; }
    int getLCDWidth()        const override { return PIXEL_W * SCALE; }
    int getLCDHeight()       const override { return PIXEL_H * SCALE; }

    void readLCDIntoBuffer(uint8_t **lines, bool is32BitOutput) const override;
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
    // 3.6864 MHz crystal / 4 = 921.6 kHz E-clock, per MAME
    // HD6301X0(config, m_maincpu, 3.6864_MHz_XTAL).
    static constexpr int32_t E_CLOCK_HZ = 921'600;

    static constexpr int PIXEL_W = HD44780::ORG1_PIXEL_W;   // 96
    static constexpr int PIXEL_H = HD44780::ORG1_PIXEL_H;   // 8
    static constexpr int SCALE   = 6;

    // The HD6301X0's mask ROM: 4 KiB at $F000-$FFFF.
    static constexpr size_t ROM_SIZE = 0x1000;
    static constexpr uint16_t ROM_BASE = 0xF000;
    // External RAM: 2 KiB at $4000-$47FF, the machine's entire store.
    static constexpr size_t RAM_SIZE = 0x0800;
    static constexpr uint16_t RAM_BASE = 0x4000;

    HD6303       cpu{*this};
    HD44780      lcd;
    PsionDatapak pack[2];

    std::array<uint8_t, ROM_SIZE> rom{};
    std::array<uint8_t, RAM_SIZE> ram{};

    // Keyboard scan counter, driven by reads of $2800 (reset) and $3000
    // (increment) — MAME's psion1_state::reset_kb_counter_r and
    // inc_kb_counter_r. The OS walks it until the pattern selects the
    // row it wants; `kb_read` matches the counter against the seven
    // one-hot-zero values 0x7E, 0x7D, 0x7B, 0x77, 0x6F, 0x5F, 0x3F.
    uint16_t m_kbCounter = 0;

    // 500 ms periodic NMI (MAME: nmi_timer, 1 s start delay). Always
    // enabled on this machine — see the file header.
    int64_t m_nextNmiCycles = E_CLOCK_HZ;   // first NMI one second in
    static constexpr int64_t NMI_PERIOD_CYCLES = E_CLOCK_HZ / 2;  // 500 ms

    // Standby. A read of $2E00 is the OS switching the machine off: the
    // real HD6301X0 drops into standby with RAM kept alive, and stays
    // there until the ON key pulls its reset line. We model that by
    // parking the CPU (no stepping, no interrupts) and resetting it when
    // ON is pressed, which is what INPUT_CHANGED_MEMBER(psion_on) does.
    bool m_standby = false;

    // Cold boot ends in standby: about a second in, the ROM has set the
    // panel up and switches the machine off again to wait to be woken,
    // which is why MAME's own notes say "psion1 goes into standby right
    // after a cold boot, so press the ON button". A machine that shows
    // nothing until the user finds that key reads as broken, so we
    // synthesise the press — the same approach as the Series 3's
    // auto-wake in series3.cpp. Arming it from the switch-off itself
    // (rather than at a fixed time from reset) keeps it tied to the
    // thing it is answering. It fires once, and not at all if the user
    // has already worked the key themselves.
    bool    m_autoOnArmed     = false;
    int64_t m_autoOnAssertAt  = 0;
    static constexpr int64_t ON_HOLD_CYCLES = E_CLOCK_HZ / 20;   // 50 ms
    bool    m_autoOnFired     = false;
    bool    m_userTouchedOn   = false;

    // Keyboard: 7 rows (K1-K7) of 5 columns at bit positions $04, $08,
    // $10, $20, $40, all active low — INPUT_PORTS_START(psion1). 0xFF
    // means "nothing pressed in this row".
    uint8_t m_keyRow[7] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    bool    m_onKey = false;

    bool m_initialised = false;

    // ORG1_TRACE cadence — see the trace block in executeUntil.
    int64_t m_nextTraceCycles = 0;
    int64_t m_nmiCount = 0;

    // ON-key edge handling: pressing ON resets the CPU out of standby.
    void pressOn(bool down);
    uint8_t kbRead() const;
};

} // namespace Organiser1
