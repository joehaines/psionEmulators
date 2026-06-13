// license:BSD-3-Clause
// copyright-holders:Nigel Barnes
//
// Standalone port of MAME's psion_asic1_device. The original lives in
// src/devices/machine/psion_asic1.{cpp,h} in the MAME tree; this file
// strips out MAME's device_t / emu_timer / address_space / devcb_write_line
// machinery and exposes a plain C++17 class that the host (the V30-based
// Psion3 driver in this repo) can drive directly.
//
// Public surface (rough mapping to MAME):
//   mem_r/mem_w     -> memRead / memWrite       (16-bit memory window)
//   io_r /io_w      -> ioRead  / ioWrite        (16-bit I/O window)
//   tick(cycles)    -> drives tick / FRC / watchdog timers from the host
//                      cycle counter (no internal scheduler / event loop)
//   eintN_w/enmi_w  -> setEintN / setEnmi       (external IRQ / NMI lines)
//   inta_cb         -> inta()                   (called by the V30 on IRQ ack)
//   int_cb/nmi_cb/  -> setIrqOutCb / setNmiOutCb / setFrcOvlCb (output lines)
//     frcovl_cb
//   screen_update_* -> readLCD()                (decodes 1bpp VRAM into a
//                                                host-friendly byte buffer;
//                                                VRAM is fetched through the
//                                                mem-read callback set via
//                                                setMemReader())
//
// Cycle accounting: tick(cycles) is called by the parent each time it
// advances the CPU; the ASIC keeps an internal int64_t cycle counter and
// fires the three timers (tick @ 4 or 32 Hz, FRC @ 512 kHz, watchdog
// @ 4 Hz) when their next-due cycle is reached. clock() defaults to
// 7'680'000 / 2 = 3.84 MHz — the bus clock that matches the Series 3
// driver in MAME (V30 input is 7.68_MHz_XTAL / 2). If the host runs the
// CPU at a different clock it should call setBusClock() before reset.

#pragma once

#include <cstdint>
#include <functional>

class PsionAsic1 {
public:
    PsionAsic1();

    // ------------------------------------------------------------------
    // Configuration
    // ------------------------------------------------------------------

    // Number of host CPU cycles per second that the host will pass to
    // tick(). Defaults to 3.84 MHz to match the V30 bus clock used by
    // the Series 3 driver. Must be set before reset() if non-default.
    void setBusClock(int64_t hz);

    // Laptop mode (HC / MC) selects the alternate VRAM base and reports
    // a different LCD identifier. The Series 3 / Pocket Book leave this
    // false.
    void setLaptopMode(bool laptop) { m_laptop_mode = laptop; }

    // Selects how the LCD reports its screen ID via A1Status[14:15].
    // 0 = 640x400, 1 = 640x200 small, 2 = 640x200 big, 3 = 720x348,
    // 4 = 160x80. The MAME driver derives this from screen height in
    // laptop mode and reports 0 otherwise; we let the host set it
    // explicitly so the same chip class can serve all SIBO models.
    void setLcdId(uint8_t id) { m_lcd_id = id & 0x07; }

    // The LCD controller in MAME pulls VRAM bytes via a memory-interface
    // address space. We can't include the V30 here, so instead the host
    // installs a byte-read callback the LCD scanout calls. The callback
    // takes a 20-bit physical address (the MAME default base of 0x00400
    // for handheld / 0xb8000 for laptop is applied internally).
    void setMemReader(std::function<uint8_t(uint32_t addr)> reader);

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    void reset();

    // Advance the chip's internal scheduler by the given number of host
    // cycles. Safe to call with 0 or negative values (no-op).
    void tick(int64_t cycles);

    // ------------------------------------------------------------------
    // Bus access
    //
    // Offsets are byte offsets within the chip's I/O / memory window
    // (matching the pre-shift offsets used inside the MAME source's
    // switch statements). The MAME device's mem_r/mem_w receive an
    // already-shifted offset; here we expose the byte address directly,
    // mask it with the appropriate window, and shift internally.
    // ------------------------------------------------------------------

    uint16_t memRead(uint32_t offset, uint16_t mask = 0xFFFF);
    void     memWrite(uint32_t offset, uint16_t data, uint16_t mask = 0xFFFF);
    uint16_t ioRead(uint32_t offset, uint16_t mask = 0xFFFF);
    void     ioWrite(uint32_t offset, uint16_t data, uint16_t mask = 0xFFFF);

    // ------------------------------------------------------------------
    // Output callbacks (replace MAME's devcb_write_line)
    // ------------------------------------------------------------------

    void setIrqOutCb(std::function<void(bool)> cb)    { m_int_cb    = std::move(cb); }
    void setNmiOutCb(std::function<void(bool)> cb)    { m_nmi_cb    = std::move(cb); }
    void setFrcOvlCb(std::function<void(bool)> cb)    { m_frcovl_cb = std::move(cb); }

    // ------------------------------------------------------------------
    // External interrupt inputs (active-high; latch into A1IntStatus)
    // ------------------------------------------------------------------

    void setEint1(bool state); // ExpIntRightB
    void setEint2(bool state); // ExpIntLeftA
    void setEint3(bool state); // Asic2Int
    void setEnmi(bool state);  // ExternalNmi

    // ------------------------------------------------------------------
    // Interrupt acknowledge — V30 calls this on INTA cycles.
    // Returns the IVT vector (0x78..0x7F) for the highest-priority
    // unmasked pending interrupt, or 0x78 if nothing is pending
    // (matching MAME's fall-through behaviour).
    // ------------------------------------------------------------------

    uint8_t inta();

    // ------------------------------------------------------------------
    // LCD readout.
    //
    // Decodes the active framebuffer into dst as 8-bit pixels (0 or 1)
    // using the provided memory reader. plates = 1 for the Series 3,
    // 2 for the dual-plate (HC / MC) mode where the lower half of the
    // visible region is fetched from VRAM + 0x4000.
    //
    // width / height are the visible region in pixels. The caller is
    // responsible for sizing dst to width*height bytes.
    //
    // If the LCD is disabled (A1Status bit 5 clear) the buffer is
    // filled with zeroes. Returns true if the panel was enabled.
    // ------------------------------------------------------------------

    bool readLCD(uint8_t *dst, int width, int height, int plates) const;

    // Convenience accessors used by tests / by parent code that wants
    // to know whether the panel was programmed.
    bool     lcdEnabled() const   { return (m_a1_status & 0x0020) != 0; }
    uint16_t lcdSizeReg() const   { return m_a1_lcd_size; }
    uint16_t lcdControl() const   { return m_a1_lcd_control; }
    uint16_t status() const       { return m_a1_status; }

private:
    // ------------------------------------------------------------------
    // Internal scheduler — replaces MAME's emu_timer.
    //
    // Each timer counts down in host cycles to its next firing edge.
    // ------------------------------------------------------------------

    void rescheduleTickTimer(); // re-derives the period from A1Status
    void fireTick();
    void fireFrc();
    void fireWatchdog();

    void updateInterrupts(bool addressTrap = false);
    bool isProtected(uint32_t byteOffset);

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------

    int64_t  m_bus_clock = 3'840'000; // V30 bus clock, host cycles/sec

    // Cycles-until-next-fire counters. They start equal to the timer
    // period at reset() and decrement inside tick(). When they reach
    // zero (or below) the timer fires and is reloaded.
    int64_t  m_tick_period_cycles     = 0;
    int64_t  m_frc_period_cycles      = 0;
    int64_t  m_watchdog_period_cycles = 0;
    int64_t  m_tick_remaining         = 0;
    int64_t  m_frc_remaining          = 0;
    int64_t  m_watchdog_remaining     = 0;

    uint16_t m_a1_status              = 0;
    uint16_t m_a1_lcd_size            = 0;
    uint16_t m_a1_lcd_control         = 0;
    uint16_t m_frc_count              = 0;
    uint16_t m_frc_reload             = 0;
    int      m_frc_ovl                = 0;
    uint8_t  m_watchdog_count         = 0;
    bool     m_a1_protection_mode     = false;
    uint32_t m_a1_protection_upper    = 0;
    uint32_t m_a1_protection_lower    = 0;

    uint8_t  m_a1_interrupt_status    = 0;
    uint8_t  m_a1_interrupt_mask      = 0;

    bool     m_laptop_mode            = false;
    uint8_t  m_lcd_id                 = 0;

    // Cached state for output callbacks so we only fire on edge.
    bool     m_int_line               = false;
    bool     m_nmi_line               = false;

    std::function<void(bool)>             m_int_cb;
    std::function<void(bool)>             m_nmi_cb;
    std::function<void(bool)>             m_frcovl_cb;
    std::function<uint8_t(uint32_t)>      m_mem_reader;
};
