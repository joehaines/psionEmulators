// license:BSD-3-Clause
// copyright-holders:Nigel Barnes (MAME psion_condor — Siena's UART)
//                  + adaptation by the Psion emulator project, 2026.
//
// Psion Condor UART. The Siena (and Series 3c) maps this device into
// V30 I/O space at 0x0100..0x011F per reference/mame-psion/psion/siena.cpp:74
// and reference/mame-psion/psion/psion3a.cpp::psion3c_state.
//
// Condor is a Psion-custom serial controller modeled after a 16550-shape
// UART (DLAB / FIFO / modem-status / interrupt registers) plus an
// auxiliary parallel-data byte the kernel uses to switch the Honda
// expansion port between high-power (SSD pack present) and low-power
// (modem / cable) modes. It is NOT the SIBO frame channel — SSD data
// frames still travel through ASIC9 SIBO channel 4 on the parallel
// half of the Honda slot.
//
// Register layout per reference/mame-psion/machine/psion_condor.cpp (lines
// 82-278). Only the bits the v4.20f Siena ROM actually touches need to
// behave; the rest can return reset-state values.
//
//   off  r/w  name
//   ---  ---  ------------------------------------------------------------
//   00   RW   UART receive buffer (R) / div-latch LSB (W if DLAB)
//   01   RW   UART IRQ enable      (R) / div-latch MSB (W if DLAB)
//   02   R    UART IRQ identification (priority-encoded; b0=1 -> none)
//   03   W    UART line control (b7 = DLAB)
//   04   W    UART modem control (b0=DTR, b1=RTS)
//   05   R    UART line status (b6 = TEMT transmitter empty)
//   06   R    UART modem status (b4=CTS b5=DSR b6=RI b7=DCD)
//   08   RW   FIFO data
//   0A   RW   Parallel data (b7 = high-power enable)
//   0B   W   Parallel direction
//   0C   R    Interrupt status
//   0D   W    Interrupt mask + misc
//   0E   R    Status (b0 = RXFE, b1 = TXFE)
//   0F   W    Control 2

#pragma once

#include <cstdint>
#include <functional>

class PsionCondor {
public:
    PsionCondor() = default;

    // Hard reset — mirrors MAME's device_reset(). After reset the modem
    // status lines read "no carrier, no clear-to-send, no data-set-ready
    // (line-active-low so 1s); FIFOs empty; transmitter empty". This is
    // the same posture the previous stub faked, so the kernel's UART
    // probe sees identical bytes.
    void reset();

    // Register-file accessors (8-bit). Out-of-range writes are ignored;
    // out-of-range reads return 0.
    uint8_t  readReg(uint8_t off);
    void     writeReg(uint8_t off, uint8_t v);

    // Cycle tick: drain TX FIFO bytes (one per ~120 cycles, fast enough
    // to never back up) via m_txdCb, and age IRQ assertion. Called from
    // the host's per-step scheduler.
    void step(int64_t cycles);

    // ------------------------------------------------------------------
    // Wiring callbacks
    // ------------------------------------------------------------------

    // IRQ output — host wires to ASIC9 eint1.
    void setIrqOutCb(std::function<void(bool)> cb) { m_irqOut = std::move(cb); }
    // Serial output bytes (TXD) drained from the TX FIFO. Host wires
    // to the Honda slot's write_txd handler.
    void setTxdCb(std::function<void(uint8_t)> cb) { m_txdCb = std::move(cb); }
    // Modem-control output edges. Host wires to the Honda slot's
    // write_rts / write_dtr handlers.
    void setRtsOutCb(std::function<void(bool)> cb) { m_rtsOut = std::move(cb); }
    void setDtrOutCb(std::function<void(bool)> cb) { m_dtrOut = std::move(cb); }
    // Parallel-data output edge — fires on writes to register 0x0A.
    // Host wires to the Honda slot's high-power-enable input.
    void setHighPowerOutCb(std::function<void(bool)> cb) { m_hipowerOut = std::move(cb); }

    // ------------------------------------------------------------------
    // Inputs from the host / Honda slot
    // ------------------------------------------------------------------

    // Push a received byte into the RX FIFO. Dropped if the FIFO is
    // full (matches a real UART overrun, which the Siena kernel won't
    // hit in normal use because nothing connects RX in the default
    // Honda-empty carrier).
    void pushRx(uint8_t b);
    // True when the RX FIFO can accept another byte. The host serial
    // bridge paces its staged bytes on this (a real cable peer paces on
    // the wire's baud rate + RTS/CTS; the bridge has neither, and
    // pushing a whole PLP frame at once would drop everything past
    // byte 8).
    bool rxHasRoom() const { return !rxFull(); }
    // True while received bytes are waiting — the MX-UART host wiring
    // re-asserts the shared interrupt latch on this (see series3c.cpp).
    bool rxPending() const { return m_rxCount > 0; }

    // Drive modem-status inputs. The line is active-low on the wire,
    // but Condor inverts internally so `true` here = "asserted at the
    // device" (DCD present, CTS present, etc.).
    void setCts(bool state);
    void setDsr(bool state);
    void setDcd(bool state);
    void setRi (bool state);

    // Plain-16550 IRQ mode: the ASIC9MX's integrated UART exposes only
    // the 16550 half of this register file — its driver programs the
    // IER and never touches the Condor-specific InterruptMask (0x0D),
    // so gating the IRQ line on that mask (correct for the discrete
    // Condor) would mute it forever. In this mode the IRQ line follows
    // UINT (the IER-driven 16550 summary) alone.
    void setPlain16550IrqMode(bool on) { m_plain16550Irq = on; }

    // ControlRegister1 state (0x09): b3 IRSD (IR shutdown), b4/b5
    // ESEL1/2 (external-interface select). Exposed so the host bridge
    // can tell which medium (cable vs infrared) the kernel selected.
    // The exact bit decode is pinned empirically per kernel — see
    // core/series3c_serial_bridge.cpp.
    uint8_t control1() const { return m_ctrl1; }
    // InterruptMaskAndMiscRegister bits b6/b7 (RS232PORT1/2) — the
    // other half of the media-select picture.
    uint8_t intMaskMisc() const { return m_intMask; }

private:
    // Register file (only the cells the kernel actually pokes).
    uint8_t m_ier  = 0;   // 0x01 IRQ enable (when DLAB=0)
    uint8_t m_lcr  = 0;   // 0x03 line control
    uint8_t m_mcr  = 0;   // 0x04 modem control (b0=DTR, b1=RTS)
    uint16_t m_divisor = 0; // baud-rate divisor (DLAB latches)

    // Modem-status input lines (true = asserted) + 16550-style change
    // deltas (MSR low nibble), cleared when the kernel reads the MSR.
    bool m_cts = true, m_dsr = true, m_ri = true, m_dcd = true;
    uint8_t m_msrDelta = 0;

    // FIFO state. 8 entries each; we treat TX as "always empty" by
    // draining a byte per write through the TXD callback once enabled,
    // so the kernel never sees TX-full and never blocks.
    static constexpr int kFifoDepth = 8;
    uint8_t m_rxFifo[kFifoDepth]{};
    int     m_rxHead = 0, m_rxTail = 0, m_rxCount = 0;

    // Auxiliary parallel-data register (0x0A) and direction (0x0B).
    uint8_t m_paraData = 0;
    uint8_t m_paraDir  = 0;

    // Interrupt block (see the .cpp header comment): live status bits
    // (0x0C layout), enable mask (0x0D), control 1 (0x09 — FIFO clears,
    // IRSD, ESEL1/2) and control 2 (0x0F — THECLR/RHFCLR).
    uint8_t m_intStatus = 0;
    // 16550 THRE latch: set when a TX write completes (or ETBEI is
    // enabled with an empty THR), cleared when the kernel reads the
    // IIR with THRE as the highest-priority source.
    bool m_threPending = false;
    uint8_t m_intMask = 0;
    uint8_t m_ctrl1   = 0;
    uint8_t m_ctrl2   = 0;

    bool m_irqAsserted = false;
    bool m_plain16550Irq = false;

    // Callbacks
    std::function<void(bool)>    m_irqOut;
    std::function<void(uint8_t)> m_txdCb;
    std::function<void(bool)>    m_rtsOut;
    std::function<void(bool)>    m_dtrOut;
    std::function<void(bool)>    m_hipowerOut;

    // Helpers
    void updateIrq();
    bool rxEmpty() const { return m_rxCount == 0; }
    bool rxFull()  const { return m_rxCount >= kFifoDepth; }
};
