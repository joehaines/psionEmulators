// license:BSD-3-Clause
// copyright-holders:Nigel Barnes
//
// Standalone port of MAME's psion_asic9_device. The original lives in
// src/devices/machine/psion_asic9.{cpp,h} in the MAME tree; this file
// strips out MAME's device_t / emu_timer / address_space / devcb_write_line
// machinery and exposes a plain C++17 class that the host (the V30-based
// Psion Series 3a / 3c / 3mx / Siena / Workabout driver in this repo) can
// drive directly.
//
// ASIC9 is a composite chip that integrates ASIC1, ASIC2 and extra I/O
// (codec sound, extra FRC, page-select mapper, RTC, serial SIBO master,
// keyboard COL decode) on one die alongside the V30H core. Here we port
// only the peripheral half of the chip; the V30H lives in core/v30.* and
// the host driver is responsible for gluing the two together through the
// memRead/memWrite/ioRead/ioWrite/tick/inta/callback surface.
//
// Public surface (rough mapping to MAME):
//   mem_r / mem_w         -> memRead / memWrite  (page-translated 20-bit window)
//   io_r  / io_w          -> ioRead  / ioWrite   (0x00-0xFF even-byte register file)
//   tick(cycles)          -> drives internal timers from a host cycle counter
//                            (no MAME emu_timer / scheduler)
//   sds_int_w, eintN_w,   -> setSdsInt / setEint0 / setEint1 / setEint2 /
//   medchng_w                setMedChng
//   inta_cb               -> inta()
//   buz_cb / col_cb /     -> setBuzCb / setColCb / setPortAbReader /
//   port_ab_r/w / pcm_*      setPortAbWriter / setPcmIn / setPcmOut
//   data_r<N>, data_w<N>  -> setSibChannelReader(n, ...) / setSibChannelWriter
//   screen_update         -> readLCD() into a caller-owned 8-bit buffer;
//                            VRAM is fetched through the memReader callback
//
// What's implemented vs stubbed is documented at the top of the .cpp.

#pragma once

#include <array>
#include <cstdint>
#include <functional>

class PsionAsic9 {
public:
    PsionAsic9();

    // ------------------------------------------------------------------
    // Configuration
    // ------------------------------------------------------------------

    // Host CPU cycles per second that the host will pass to tick().
    // Defaults to 15'360'000 / 2 = 7.68 MHz — the V30H bus clock used by
    // the MAME psion3a driver (the crystal is 15.36 MHz; the V30 divides
    // by 2 internally to produce its bus clock). Call before reset() if
    // non-default. (The 3c uses 3.6864 MHz * some multiplier; the 3mx
    // runs faster — host picks.)
    void setBusClock(int64_t hz);

    // Use MAME-accurate 32.768 Hz tick rate (bus_clock*1000/32768) instead
    // of the legacy 32 Hz approximation. Disabled by default so existing
    // Series 3c / Siena drivers are not affected; enabled for Series 3a.
    void setAccurateTick(bool enabled);

    // Periodically set A9MNoBattery (A9WStatus bit 12 = 0x1000) so older
    // kernels that poll that bit for a "SIB scan ready" signal see it fire.
    // Enabled by default (preserves Series 3c behaviour); disabled for 3a
    // where MAME never sets the bit and it triggers a "no battery" path.
    void enableSibHeartbeat(bool enabled);

    // The LCD controller in MAME pulls VRAM bytes via an internal
    // address space; we can't embed the V30's bus here, so the host
    // installs a byte-read callback. Address is a 24-bit byte offset in
    // the V30's post-page-select physical space. The scanout reads
    // starting from 0x0400 (the Series 3a/3c framebuffer base in RAM).
    void setMemReader(std::function<uint8_t(uint32_t addr)> reader);

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    void reset();

    // Advance the chip's internal scheduler by the given number of host
    // cycles. Safe to call with 0 or negative values (no-op).
    void tick(int64_t cycles);

    // ------------------------------------------------------------------
    // Bus access. Offsets are byte offsets within the relevant window:
    //   memRead/memWrite: 20-bit offset into the V30's 1 MiB memory
    //     window. ASIC9 translates 0x60000..0x9FFFF through the four
    //     8-bit page-select registers (psel_6000..psel_9000), leaves
    //     0x00000..0x5FFFF alone (RAM), and forces 0xA0000..0xFFFFF to
    //     the top of ROM. Memory protection is enforced on writes.
    //   ioRead/ioWrite: 8-bit register index * 2; passing 0x02 gets
    //     A9WControl etc. Internally we OR the offset with its natural
    //     mask.
    // Both interfaces accept an optional byte-lane mask matching MAME's
    // mem_mask semantics.
    // ------------------------------------------------------------------

    uint16_t memRead(uint32_t offset, uint16_t mask = 0xFFFF);
    void     memWrite(uint32_t offset, uint16_t data, uint16_t mask = 0xFFFF);
    uint16_t ioRead(uint32_t offset, uint16_t mask = 0xFFFF);
    void     ioWrite(uint32_t offset, uint16_t data, uint16_t mask = 0xFFFF);

    // ------------------------------------------------------------------
    // Output callbacks (replace MAME's devcb_write_line)
    // ------------------------------------------------------------------

    void setIrqOutCb(std::function<void(bool)> cb) { m_int_cb = std::move(cb); }
    void setNmiOutCb(std::function<void(bool)> cb) { m_nmi_cb = std::move(cb); }

    // Buzzer/piezo output: toggles on FRC1 expire (if BuzzFromFrc1) or
    // mirrors BuzzTog bit in A9WControlExtra otherwise.
    // POLL line callback — matches MAME's NEC_INPUT_LINE_POLL.
    // Called with false when an SIB frame transfer starts (CPU should stall
    // on WAIT), and with true 24 bus cycles later when the transfer ends.
    void setPollCb(std::function<void(bool)> cb) { m_poll_cb = std::move(cb); }

    void setBuzCb(std::function<void(bool)> cb) { m_buz_cb = std::move(cb); }

    // Keyboard column strobe: called on writes to A9WControlExtra to
    // assert one of eight KBDCOL lines (active-low in MAME, expressed
    // here as the byte mask MAME writes to the col callback).
    void setColCb(std::function<void(uint8_t)> cb) { m_col_cb = std::move(cb); }

    // Port A/B GPIOs. The host supplies a 16-bit reader and writer.
    void setPortAbReader(std::function<uint16_t()> r) { m_port_ab_r = std::move(r); }
    void setPortAbWriter(std::function<void(uint16_t)> w) { m_port_ab_w = std::move(w); }

    // PCM codec sample IO (8 kHz nominal). in() is called when
    // A9WControl's SoundDir bit selects capture; out() when playing.
    void setPcmIn(std::function<uint8_t()> r) { m_pcm_in = std::move(r); }
    void setPcmOut(std::function<void(uint8_t)> w) { m_pcm_out = std::move(w); }

    // SIBO slave serial channels. There are 8 channels; channel N is
    // selected by A9BChannelSelect bit N. reader returns the next byte
    // of a received frame; writer receives a 16-bit framed value
    // (NULL_FRAME / CONTROL_FRAME | ctrl / DATA_FRAME | data).
    void setSibChannelReader(int ch, std::function<uint8_t()> r);
    void setSibChannelWriter(int ch, std::function<void(uint16_t)> w);

    // ------------------------------------------------------------------
    // External interrupt inputs
    // ------------------------------------------------------------------

    void setSdsInt(bool state);   // serial slave device interrupt
    // ASIC9MX integrated-UART interrupt inputs (Series 3mx /
    // Workabout MX). The MX die widens the interrupt status/mask
    // registers to 16 bits: UART0 (I/O 0x40-0x4E) interrupts on bit 8
    // and UART1 (I/O 0x50-0x5E, the Remote Link port) on bit 9 —
    // pinned from the v6.16f ROM, which stores the per-UART mask bits
    // 0x0100/0x0200 in its driver structs at 0x1ab052/0x1ab058 and
    // RMWs the A9BInterruptMask word with them. Level inputs like the
    // EINT lines.
    void setMxUartInt(int uart, bool state);
    void setEint0(bool state);    // external interrupt 0 (ExpIntC)
    void setEint1(bool state);    // external interrupt 1 (ExpIntA)
    void setEint2(bool state);    // external interrupt 2 (ExpIntB)
    void setMedChng(bool state);  // media-change door switch (door NMI)

    // ------------------------------------------------------------------
    // Interrupt acknowledge — V30 calls this on INTA cycles. Returns
    // the IVT vector (0x78 + bit number) for the highest-priority
    // unmasked pending interrupt, or 0x78 if nothing is pending. On
    // the ASIC9MX the encoder extends past the classic 8 sources to
    // the integrated UARTs (bit 8 -> 0x80, bit 9 -> 0x81).
    // ------------------------------------------------------------------

    uint8_t inta();

    // Keyboard COL readback (A9WControlExtra & 0x0f, for the keyboard
    // scanner on the driver side to mirror MAME's col_r()).
    uint8_t colR() const { return m_a9_control_extra & 0x0f; }

    // ------------------------------------------------------------------
    // LCD readout.
    //
    // Decodes the active framebuffer into dst as 8-bit pixels using the
    // memory reader. The Series 3a/3c/3mx panel is 480x160; the MAME
    // driver uses a dual-plane display (black + grey) where black wins.
    // Pixel values in the output buffer:
    //   0 = off / white, 1 = black, 2 = grey.
    //
    // If the LCD is disabled (A9WControl bit 10 clear) the buffer is
    // filled with zeroes. Returns true if the panel was enabled.
    // ------------------------------------------------------------------

    bool readLCD(uint8_t *dst, int width, int height) const;

    // Convenience accessors (tests / debugging).
    bool     lcdEnabled() const { return (m_a9_control & 0x0400) != 0; }
    uint16_t lcdSizeReg() const { return m_a9_lcd_size; }
    uint16_t control()    const { return m_a9_control; }
    uint16_t controlExtra() const { return m_a9_control_extra; }
    uint16_t status()     const { return m_a9_status; }
    uint16_t intMask()    const { return m_a9_interrupt_mask; }
    uint16_t intStatus()  const { return m_a9_interrupt_status; }
    int      sndFifoFill() const { return m_snd_fifo_size; }
    // Cumulative codec service counters (tests / diagnostics only).
    uint64_t sndDataReads()  const { return m_dbg_snd_data_reads; }
    uint64_t sndDataWrites() const { return m_dbg_snd_data_writes; }
    uint64_t sndEoiWrites()  const { return m_dbg_snd_eoi_writes; }

    // Override the initial A9Status value used on each reset(). Default is
    // 0xE020 (MainsPresent|Reset|PowerFail|Cold) matching MAME device_reset().
    // For models where MainsPresent causes "Media is corrupt" (Series 3a,
    // Workabout, WorkaboutMX) the caller passes 0x2000|0x8000 (Reset|Cold,
    // no mains, no PowerFail). PowerFail without MainsPresent is a MAME-
    // divergent combination that may take an unexpected recovery path.
    void setBootStatus(uint16_t status) { m_boot_status = status; }

    // The currently-configured RAM device type, updated by mirroring
    // MAME's configure_ram-on-bits-3-4-change behaviour: starts at the
    // host-supplied default (set via setRamTypeDefault) to match MAME's
    // configure_ram(m_ram_type) at machine_start, and only flips when a
    // write to A9WControl actually *changes* bits 3-4 (psion_asic9.cpp
    // line 830). The kernel's very first A9WControl write often has
    // bits 3-4 = 0 just because the rest of the bits the kernel cares
    // about are in the low nibble; MAME treats that as a no-op for the
    // RAM layout, and so do we.
    uint8_t  ramType()   const { return m_a9_ram_type; }
    void     setRamTypeDefault(uint8_t t) { m_a9_ram_type = t & 0x3; }

    // Page-select registers for the four 64 KiB windows at V30 segments
    // 0x6000/0x7000/0x8000/0x9000. Returned value is an 8-bit bank index
    // in the ASIC9 internal 24-bit address space: 0x00..0x7F selects a
    // RAM 64 KiB page, 0x80..0xFF selects a ROM 64 KiB page
    // (page 0x80 = ROM offset 0x000000, page 0xFF = ROM offset 0x7F0000).
    // The Series 3c host driver uses these to translate accesses in the
    // 0x60000..0x9FFFF V30 range.
    uint8_t  psel6000() const { return m_a9_psel_6000; }
    uint8_t  psel7000() const { return m_a9_psel_7000; }
    uint8_t  psel8000() const { return m_a9_psel_8000; }
    uint8_t  psel9000() const { return m_a9_psel_9000; }
    uint16_t interruptMask()   const { return m_a9_interrupt_mask; }
    uint16_t interruptStatus() const { return m_a9_interrupt_status; }

    // Frame constants used by the SIBO serial protocol.
    static constexpr uint16_t NULL_FRAME    = 0x000;
    static constexpr uint16_t CONTROL_FRAME = 0x100;
    static constexpr uint16_t DATA_FRAME    = 0x200;

private:
    // ------------------------------------------------------------------
    // Internal scheduler (replaces MAME's emu_timer). Each timer keeps
    // a "cycles until next fire" counter that decrements in tick(); on
    // or below zero it fires and reloads.
    // ------------------------------------------------------------------

    void reloadTickTimer();
    void reloadFrc1Timer();
    void reloadFrc2Timer();
    void reloadWatchdogTimer();
    void reloadRtcTimer();
    void reloadSndTimer();

    void fireTick();
    void fireFrc1();
    void fireFrc2();
    void fireWatchdog();
    void fireRtc();
    void fireSnd();

    void updateInterrupts();
    bool isProtected(uint32_t offset);
    uint32_t translateAddress(uint32_t offset) const;

    // SIBO frame helpers.
    bool    channelActive(int channel) const;
    void    transmitFrame(uint16_t data);
    uint8_t receiveFrame();

    // ------------------------------------------------------------------
    // Timing state
    // ------------------------------------------------------------------

    int64_t m_bus_clock = 7'680'000; // V30H bus clock, host cycles/sec
    bool    m_accurate_tick      = false; // 32.768 Hz (MAME); false = 32 Hz legacy
    bool    m_sib_heartbeat      = true;  // periodic bit-12 pulse; false = MAME-accurate

    int64_t m_tick_period     = 0;
    int64_t m_frc1_period     = 0;
    int64_t m_frc2_period     = 0;
    int64_t m_watchdog_period = 0;
    int64_t m_rtc_period      = 0;
    int64_t m_snd_period      = 0;

    int64_t m_tick_remaining     = 0;
    int64_t m_frc1_remaining     = 0;
    int64_t m_frc2_remaining     = 0;
    int64_t m_watchdog_remaining = 0;
    int64_t m_rtc_remaining      = 0;
    int64_t m_snd_remaining      = 0;
    int64_t m_sib_scan_remaining = 75'000; // only used when m_sib_heartbeat == true

    // ------------------------------------------------------------------
    // Register file state (mirrors MAME's psion_asic9_device members)
    // ------------------------------------------------------------------

    uint8_t  m_post                  = 0;
    uint16_t m_a9_control            = 0;
    uint8_t  m_a9_ram_type           = 2;  // default to 2 MiB / 1 MiB-device — matches the most-common SIBO2 install
    uint16_t m_a9_status             = 0;
    uint16_t m_boot_status           = 0x0020 | 0x2000 | 0x4000 | 0x8000; // MainsPresent|Reset|PowerFail|Cold = 0xE020, matching MAME device_reset()
    uint16_t m_a9_lcd_size           = 0;
    // 16-bit: classic ASIC9 sources in the low byte, ASIC9MX
    // integrated-UART sources at bits 8/9 (see setMxUartInt). Classic
    // kernels only ever drive the low byte, so the width is
    // behaviour-neutral for them.
    uint16_t m_a9_interrupt_status   = 0;
    uint16_t m_a9_interrupt_mask     = 0;

    uint16_t m_frc1_count            = 0;
    uint16_t m_frc1_reload           = 0;
    uint16_t m_frc2_count            = 0;
    uint16_t m_frc2_reload           = 0;
    int      m_buz_toggle            = 0;
    uint8_t  m_watchdog_count        = 0;

    bool     m_a9_protection_mode    = false;
    uint32_t m_a9_protection_upper   = 0;
    uint32_t m_a9_protection_lower   = 0;

    uint16_t m_a9_port_ab_ddr        = 0;
    uint8_t  m_a9_port_c_ddr         = 0;
    uint8_t  m_a9_port_d_ddr         = 0;

    uint8_t  m_a9_psel_6000          = 0;
    uint8_t  m_a9_psel_7000          = 0;
    uint8_t  m_a9_psel_8000          = 0;
    uint8_t  m_a9_psel_9000          = 0;

    uint16_t m_a9_control_extra      = 0;
    uint32_t m_rtc                   = 0;

    // Sound FIFO. MAME uses util::fifo<uint8_t,16>; we mirror with a
    // simple ring buffer. m_snd_missed counts capture-direction codec
    // fires that arrived while the FIFO was full; they are replayed as
    // the kernel drains so emulated CSINT latency never loses samples
    // (see fireSnd / ioRead 0x1a in the .cpp).
    std::array<uint8_t, 16> m_snd_fifo{};
    int                     m_snd_fifo_head = 0;
    int                     m_snd_fifo_tail = 0;
    int                     m_snd_fifo_size = 0;
    int                     m_snd_missed    = 0;

    uint8_t  m_a9_serial_data        = 0;
    uint8_t  m_a9_serial_control     = 0;
    uint8_t  m_a9_channel_select     = 0;

    // Codec service counters (diagnostics only; no behavioural effect).
    uint64_t m_dbg_snd_data_reads  = 0;
    uint64_t m_dbg_snd_data_writes = 0;
    uint64_t m_dbg_snd_eoi_writes  = 0;

    // Cached line states for edge-triggered output callbacks.
    bool     m_int_line              = false;
    bool     m_nmi_line              = false;

    // Callbacks.
    std::function<void(bool)>           m_int_cb;
    std::function<void(bool)>           m_nmi_cb;
    std::function<void(bool)>           m_poll_cb;
    std::function<void(bool)>           m_buz_cb;
    std::function<void(uint8_t)>        m_col_cb;
    std::function<uint16_t()>           m_port_ab_r;
    std::function<void(uint16_t)>       m_port_ab_w;
    std::function<uint8_t()>            m_pcm_in;
    std::function<void(uint8_t)>        m_pcm_out;
    std::function<uint8_t(uint32_t)>    m_mem_reader;

    std::array<std::function<uint8_t()>, 8>        m_data_r;
    std::array<std::function<void(uint16_t)>, 8>   m_data_w;

    // FIFO helpers.
    bool    sndFifoFull()  const { return m_snd_fifo_size >= 16; }
    bool    sndFifoEmpty() const { return m_snd_fifo_size == 0; }
    void    sndFifoEnqueue(uint8_t v);
    uint8_t sndFifoDequeue();
};
