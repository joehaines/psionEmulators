// license:BSD-3-Clause
// copyright-holders:Nigel Barnes
//
// Standalone port of MAME src/devices/machine/psion_asic2.{cpp,h}.
//
// ASIC2 is the SIBO peripheral controller: keyboard scan, slow port I/O,
// SIBO serial channels (SSDs and the SIBO expansion port), buzzer
// driver, NMI generation for pack-door / low-battery events, and the
// power-on / reset latches.
//
// As with PsionAsic1, all MAME machinery (device_t, emu_timer,
// devcb_*, address_space) is stripped out and replaced with std::function
// callbacks plus a cycle-counted tick().
//
// Surface mapping vs MAME:
//   io_r / io_w           -> ioRead / ioWrite
//   on_clr_w              -> setOnClr     (the ESC/On key)
//   sds_int_w             -> setSdsInt    (SIBO slave-data interrupt)
//   dnmi_w                -> setDoorNmi   (pack-door open NMI)
//   frcovl_w              -> frcOvlIn     (FRC overflow input from ASIC1,
//                                           used to drive the buzzer in
//                                           BuzzerMode == 1)
//   reset_w               -> setResetLine (latches A1ResetFlag)
//   int_cb / nmi_cb       -> setIrqOutCb / setNmiOutCb
//   cbusy_cb              -> setCBusyCb   (clears the V30 POLL line while
//                                           the data serial controller is
//                                           busy)
//   buz_cb / buzvol_cb    -> setBuzCb / setBuzVolCb
//   dr_cb                 -> setDigitiserDirCb (XySwitch line)
//   col_cb / read_pd_cb / -> setKeyboardColumnCb / setReadPortDataCb /
//   write_pd_cb              setWritePortDataCb
//   data_r<N>/data_w<N>   -> setChannelReadCb(N, ...) / setChannelWriteCb(N, ...)
//                            for N in 0..7

#pragma once

#include <array>
#include <cstdint>
#include <functional>

class PsionAsic2 {
public:
    PsionAsic2();

    // ------------------------------------------------------------------
    // Configuration
    // ------------------------------------------------------------------

    // Host CPU cycles/sec — used to compute the busy-timer period
    // (12 ticks at clock/2 in MAME). Defaults to the Series 3 V30 bus
    // clock (3.84 MHz).
    void setBusClock(int64_t hz);

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    void reset();
    void tick(int64_t cycles);

    // ------------------------------------------------------------------
    // Bus access — ASIC2 has an 8-bit register window; MAME maps it at
    // 0x80..0x8F on the byte port. Offsets here are the byte address
    // within the window; we mask to 0..7 internally as the chip does.
    // ------------------------------------------------------------------

    uint8_t ioRead(uint32_t offset);
    void    ioWrite(uint32_t offset, uint8_t data);

    // ------------------------------------------------------------------
    // External inputs
    // ------------------------------------------------------------------

    void setOnClr(bool state);     // ON/CLR (ESC On) key
    void setSdsInt(bool state);    // SIBO slave data signal
    void setDoorNmi(bool state);   // pack-door open
    void frcOvlIn(bool state);     // FRC overflow from ASIC1 (buzzer drive)
    void setResetLine(bool state); // POR / system reset

    // ------------------------------------------------------------------
    // Output callbacks
    // ------------------------------------------------------------------

    void setIrqOutCb(std::function<void(bool)> cb)         { m_int_cb     = std::move(cb); }
    void setNmiOutCb(std::function<void(bool)> cb)         { m_nmi_cb     = std::move(cb); }
    void setCBusyCb(std::function<void(bool)> cb)          { m_cbusy_cb   = std::move(cb); }
    void setBuzCb(std::function<void(bool)> cb)            { m_buz_cb     = std::move(cb); }
    void setBuzVolCb(std::function<void(bool)> cb)         { m_buzvol_cb  = std::move(cb); }
    void setDigitiserDirCb(std::function<void(bool)> cb)   { m_dr_cb      = std::move(cb); }

    // Keyboard column read: invoked with column index 0..9, returns the
    // 8-bit row mask for that column (active high).
    void setKeyboardColumnCb(std::function<uint8_t(int col)> cb)
        { m_col_cb = std::move(cb); }

    // 8-bit slow-port (PD) data direction is owned by the chip; the
    // host just supplies a read callback for input bits and a write
    // callback for the masked output bits.
    void setReadPortDataCb(std::function<uint8_t()> cb)      { m_read_pd_cb  = std::move(cb); }
    void setWritePortDataCb(std::function<void(uint8_t)> cb) { m_write_pd_cb = std::move(cb); }

    // SIBO serial channels 0..7. Reads return a single byte from the
    // selected slave; writes consume the 16-bit frame already prefixed
    // with NULL_FRAME / CONTROL_FRAME / DATA_FRAME so the slave can
    // disambiguate frame types (matching MAME's data_w<N> contract).
    void setChannelReadCb(int channel, std::function<uint8_t()> cb);
    void setChannelWriteCb(int channel, std::function<void(uint16_t)> cb);

    // ------------------------------------------------------------------
    // Frame-type tags (mirrors MAME's psion_asic2_device:: constants).
    // ------------------------------------------------------------------

    static constexpr uint16_t NULL_FRAME    = 0x000;
    static constexpr uint16_t CONTROL_FRAME = 0x100;
    static constexpr uint16_t DATA_FRAME    = 0x200;

private:
    void updateInterrupts();
    bool channelActive(int channel) const;
    void transmitFrame(uint16_t data);
    uint8_t receiveFrame();
    void fireBusyTimer();

    // ------------------------------------------------------------------
    // State (1:1 with MAME's m_a2_*)
    // ------------------------------------------------------------------

    int64_t m_bus_clock = 3'840'000;

    uint8_t m_a2_index            = 0;
    uint8_t m_a2_icontrol0        = 0;
    uint8_t m_a2_icontrol1        = 0;
    uint8_t m_a2_iddr             = 0;
    uint8_t m_a2_control1         = 0;
    uint8_t m_a2_control2         = 0;
    uint8_t m_a2_control3         = 0;
    uint8_t m_a2_serial_data      = 0;
    uint8_t m_a2_serial_control   = 0;
    uint8_t m_a2_interrupt_status = 0;
    uint8_t m_a2_status           = 0;
    uint8_t m_a2_channel_control  = 0;

    // Cached output line states (so we only fire callbacks on edge).
    bool m_int_line               = false;
    bool m_nmi_line               = false;

    // Busy timer — when nonzero, counts down in host cycles. On reaching
    // zero the SerialBusy bit clears and CBUSY rises (active-high).
    int64_t m_busy_remaining      = 0;

    std::function<void(bool)>           m_int_cb;
    std::function<void(bool)>           m_nmi_cb;
    std::function<void(bool)>           m_cbusy_cb;
    std::function<void(bool)>           m_buz_cb;
    std::function<void(bool)>           m_buzvol_cb;
    std::function<void(bool)>           m_dr_cb;
    std::function<uint8_t(int)>         m_col_cb;
    std::function<uint8_t()>            m_read_pd_cb;
    std::function<void(uint8_t)>        m_write_pd_cb;

    std::array<std::function<uint8_t()>,        8> m_data_r{};
    std::array<std::function<void(uint16_t)>,   8> m_data_w{};
};
