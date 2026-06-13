// license:BSD-3-Clause
// copyright-holders:Nigel Barnes (MAME psion_condor)
//                  + adaptation by the Psion emulator project, 2026.
//
// Interrupt model (beyond MAME, which stubs it): the Condor has its own
// interrupt block separate from the 16550-shaped UART core —
//   InterruptStatusRegister  (0x0C): b0 UINT (16550 core), b1 RHFINT
//     (RX FIFO half full), b2 RTOINT (RX timeout — data waiting below
//     the half mark), b3 THEINT (TX FIFO half empty), b4/b5 MP1/MP2.
//   InterruptMaskAndMiscRegister (0x0D): enable bits for the above in
//     the same positions, plus b6/b7 RS232PORT1/2 (port select).
//   ControlRegister2 (0x0F): b0 THECLR clears THEINT, b1 RHFCLR clears
//     the RX interrupts.
// Bit meanings from MAME's register docstrings; the dynamic behaviour
// (when each bit sets / re-asserts) is our model, tuned against the
// Series 3c v5.20f kernel's Link-cable driver: TX writes complete
// immediately (no baud pacing) so THEINT re-asserts after every TX
// write and after THECLR while TX has room — which is always. The
// kernel masks ETHEINT off when its TX queue empties, exactly like a
// 16550 driver, so this does not storm.

#include "psion_condor.h"

#include <cstdio>
#include <cstdlib>

namespace {
bool condorTraceEnabled() {
    static const char *t = std::getenv("PSION_CONDOR_TRACE");
    return t && t[0] && t[0] != '0';
}
} // namespace

void PsionCondor::reset() {
    m_ier = 0;
    m_lcr = 0;
    m_mcr = 0;
    m_divisor = 0;
    m_paraData = 0;
    m_paraDir  = 0;
    m_intMask  = 0;
    m_ctrl1    = 0;
    m_ctrl2    = 0;
    m_intStatus = 0;
    m_threPending = false;
    m_rxHead = m_rxTail = m_rxCount = 0;
    m_cts = m_dsr = m_ri = m_dcd = true;
    if (m_irqAsserted) {
        m_irqAsserted = false;
        if (m_irqOut) m_irqOut(false);
    }
    if (m_rtsOut) m_rtsOut(false);
    if (m_dtrOut) m_dtrOut(false);
    if (m_hipowerOut) m_hipowerOut(false);
}

void PsionCondor::updateIrq() {
    // Synthesize the level-derived status bits before evaluating.
    // (THEINT is edge-set in the TX write paths — a permanently-true
    // level here put the v5.20f kernel in an endless THECLR/re-read
    // loop, since our TX always has room.)
    //  RHFINT — RX FIFO at or past half (4 of 8).
    if (m_rxCount >= kFifoDepth / 2) m_intStatus |= 0x02;
    //  RTOINT — RX data waiting below the half mark ("receive
    //  timeout"). We assert immediately rather than after a real
    //  3.5-character timeout so host bytes are delivered promptly.
    if (m_rxCount > 0 && m_rxCount < kFifoDepth / 2) m_intStatus |= 0x04;
    //  UINT — the 16550 core's own summary: received-data-available
    //  (IER b0), transmit-holding-register-empty (IER b1 ETBEI — the
    //  Siena v4.20f driver paces its TX one byte per THRE interrupt),
    //  and modem-status-change (IER b3 EDSSI, latched in m_msrDelta
    //  until the kernel reads the MSR; the v5.20f link driver uses the
    //  DSR/DCD delta as its "cable plugged in" trigger).
    if (((m_ier & 0x01) && m_rxCount > 0) ||
        ((m_ier & 0x02) && m_threPending) ||
        ((m_ier & 0x08) && m_msrDelta))   m_intStatus |= 0x01;
    else                                  m_intStatus &= ~0x01;

    bool wantIrq = m_plain16550Irq
        ? (m_intStatus & 0x01) != 0
        : (m_intStatus & m_intMask & 0x3F) != 0;
    if (wantIrq != m_irqAsserted) {
        m_irqAsserted = wantIrq;
        if (m_irqOut) m_irqOut(wantIrq);
    }
}

uint8_t PsionCondor::readReg(uint8_t off) {
    // Register index 0x0..0xF. The host (core/series3c.cpp condorRead)
    // derives it from the 16-bit-bus byte address: registers live on
    // the LOW byte lane of each word in the 0x100-0x11F window (MAME
    // installs the chip with lane mask 0x00ff), so index = offset >> 1.
    off &= 0x0F;
    uint8_t data = 0x00;
    switch (off) {
    case 0x00: {
        if (m_lcr & 0x80) { data = uint8_t(m_divisor & 0xFF); break; } // div latch LSB
        // RX buffer: pop one byte if available.
        if (m_rxCount > 0) {
            data = m_rxFifo[m_rxHead];
            m_rxHead = (m_rxHead + 1) % kFifoDepth;
            --m_rxCount;
            if (m_rxCount == 0) m_intStatus &= ~0x06; // RX ints drop with the data
            updateIrq();
        }
        break;
    }
    case 0x01:
        data = (m_lcr & 0x80) ? uint8_t((m_divisor >> 8) & 0xFF) : m_ier;
        break;
    case 0x02:
        // IRQ identification (16550 IIR): b0=1 means "no interrupt
        // pending"; 0x04 = received data available, 0x02 = THRE (and
        // reading it as the highest-priority source clears the THRE
        // latch, per 16550 semantics), 0x00 = modem status.
        if ((m_ier & 0x01) && m_rxCount > 0)      data = 0x04;
        else if ((m_ier & 0x02) && m_threPending) { data = 0x02; m_threPending = false; updateIrq(); }
        else if ((m_ier & 0x08) && m_msrDelta)    data = 0x00;
        else                                      data = 0x01;
        break;
    case 0x04:
        // Modem control read-back — the 3mx kernel RMWs the MCR
        // (read, flip DTR/RTS, write) during link bring-up.
        data = m_mcr;
        break;
    case 0x05:
        // Line status: TEMT (b6) + THRE (b5) always set — TX is
        // drained immediately. DR (b0) follows RX FIFO.
        data = uint8_t(0x60 | (m_rxCount > 0 ? 0x01 : 0));
        break;
    case 0x06:
        // Modem status: b4=CTS b5=DSR b6=RI b7=DCD (asserted = 1),
        // low nibble = 16550-style change deltas (b0 dCTS, b1 dDSR,
        // b2 TERI, b3 dDCD), cleared by this read.
        data = uint8_t((m_cts ? 0x10 : 0) | (m_dsr ? 0x20 : 0) |
                       (m_ri  ? 0x40 : 0) | (m_dcd ? 0x80 : 0) |
                       (m_msrDelta & 0x0F));
        m_msrDelta = 0;
        updateIrq();
        break;
    case 0x08:
        // FIFO data register — RX dequeue (same backing FIFO as 0x00).
        if (m_rxCount > 0) {
            data = m_rxFifo[m_rxHead];
            m_rxHead = (m_rxHead + 1) % kFifoDepth;
            --m_rxCount;
            if (m_rxCount == 0) m_intStatus &= ~0x06;
            updateIrq();
        }
        break;
    case 0x09:
        data = m_ctrl1;
        break;
    case 0x0A: data = m_paraData; break;
    case 0x0C:
        // InterruptStatusRegister — raw (unmasked) so polled drivers
        // see pending conditions even with the mask off.
        updateIrq();
        data = m_intStatus;
        break;
    case 0x0D:
        data = m_intMask;
        break;
    case 0x0E:
        // Status: b0 = RXFE (RX FIFO empty). Bit 1 is documented in
        // MAME as "TXFE — Transmit FIFO Empty", but the v5.20f kernel's
        // TX burst loop (serial driver @ ROM 0x1ab6bb) writes bytes
        // WHILE the bit is clear and stops the moment it sets — i.e.
        // on real silicon it means "TX FIFO FULL, stop writing".
        // Returning it permanently set (the old TXFE reading) made the
        // kernel abort every transmission before the first byte, which
        // is why the link never answered the host. Our TX drains
        // synchronously, so the FIFO is never full: keep it 0.
        data = uint8_t(m_rxCount == 0 ? 0x01 : 0x00);
        break;
    default:
        break;
    }
    if (condorTraceEnabled() && off != 0x05 && off != 0x0E)
        std::fprintf(stderr, "[condor] r %02x => %02x\n", off, data);
    return data;
}

void PsionCondor::writeReg(uint8_t off, uint8_t v) {
    off &= 0x0F;  // 4-bit fold — see readReg
    
    if (condorTraceEnabled())
        std::fprintf(stderr, "[condor] w %02x <= %02x\n", off, v);
    switch (off) {
    case 0x00:
        if (m_lcr & 0x80) {
            m_divisor = uint16_t((m_divisor & 0xFF00) | v);
        } else {
            // TX path. With LOOPBACK (MCR b4) set the byte echoes
            // into our own RX FIFO instead of the wire. Each completed
            // TX write raises THEINT (the byte leaves the FIFO
            // immediately, so "TX half empty" becomes true again) and
            // re-latches the 16550 THRE condition for ETBEI users;
            // THECLR / an IIR read acknowledge them respectively.
            if (m_mcr & 0x10) pushRx(v);
            else if (m_txdCb) m_txdCb(v);
            m_intStatus |= 0x08;
            m_threPending = true;
            updateIrq();
        }
        break;
    case 0x01:
        if (m_lcr & 0x80) {
            m_divisor = uint16_t((m_divisor & 0x00FF) | (uint16_t(v) << 8));
        } else {
            // Enabling ETBEI with the THR already empty (always, in
            // our zero-latency model) fires THRE at once — standard
            // 16550 behaviour the Siena's one-byte-per-interrupt TX
            // loop depends on to start transmitting.
            if ((v & 0x02) && !(m_ier & 0x02)) m_threPending = true;
            m_ier = v;
            updateIrq();
        }
        break;
    case 0x03:
        m_lcr = v;
        break;
    case 0x04: {
        uint8_t prev = m_mcr;
        m_mcr = v;
        bool newDtr = (v & 0x01) != 0;
        bool newRts = (v & 0x02) != 0;
        bool oldDtr = (prev & 0x01) != 0;
        bool oldRts = (prev & 0x02) != 0;
        if (newDtr != oldDtr && m_dtrOut) m_dtrOut(newDtr);
        if (newRts != oldRts && m_rtsOut) m_rtsOut(newRts);
        break;
    }
    case 0x08:
        // FifoDataRegister — TX enqueue; drains synchronously.
        // LOOPBACK (MCR b4) echoes into our own RX FIFO (see case 0x00).
        if (m_mcr & 0x10) pushRx(v);
        else if (m_txdCb) m_txdCb(v);
        m_intStatus |= 0x08;  // TX-half-empty edge (see case 0x00)
        m_threPending = true;
        updateIrq();
        break;
    case 0x09:
        // ControlRegister1: b0 TFCLR, b1 RFCLR, b3 IRSD (IR shutdown),
        // b4/b5 ESEL1/2 (external interface select), b6 CLKEN, b7 PSRST.
        m_ctrl1 = v;
        if (v & 0x02) { // RFCLR
            m_rxHead = m_rxTail = m_rxCount = 0;
            m_intStatus &= ~0x06;
        }
        // TFCLR (b0) has nothing to clear — TX drains synchronously.
        updateIrq();
        break;
    case 0x0A: {
        bool oldHi = (m_paraData & 0x80) != 0;
        m_paraData = v;
        bool newHi = (v & 0x80) != 0;
        if (newHi != oldHi && m_hipowerOut) m_hipowerOut(newHi);
        break;
    }
    case 0x0B:
        m_paraDir = v;
        break;
    case 0x0D:
        // Enabling ETHEINT (b3) with the TX FIFO already below half —
        // which in our zero-latency model is always — raises THEINT
        // immediately, matching 16550 semantics where enabling the TX
        // interrupt with an empty FIFO fires at once. The kernel's
        // link driver relies on this to kick its transmit queue.
        if ((v & 0x08) && !(m_intMask & 0x08)) m_intStatus |= 0x08;
        m_intMask = v;
        updateIrq();
        break;
    case 0x0F:
        // ControlRegister2: b0 THECLR clears the TX interrupt, b1
        // RHFCLR clears RHFINT, b2 clears RTOINT (the v5.20f kernel
        // writes 0x04 after draining a sub-half-full RX burst). The RX
        // bits re-assert from level on the next updateIrq if data is
        // still waiting.
        m_ctrl2 = v;
        if (v & 0x01) m_intStatus &= ~0x08;
        if (v & 0x02) m_intStatus &= ~0x02;
        if (v & 0x04) m_intStatus &= ~0x04;
        updateIrq();
        break;
    default:
        break;
    }
}

void PsionCondor::step(int64_t /*cycles*/) {
    // Nothing to drain — TX bytes go through m_txdCb synchronously
    // at write time. RX is event-driven via pushRx. updateIrq is
    // called from the relevant register write paths.
}

void PsionCondor::pushRx(uint8_t b) {
    if (rxFull()) return; // overrun, drop
    m_rxFifo[m_rxTail] = b;
    m_rxTail = (m_rxTail + 1) % kFifoDepth;
    ++m_rxCount;
    updateIrq();
}

// Modem-line inputs latch a 16550-style delta bit on every change so
// the EDSSI path can interrupt the kernel (cable plug/unplug events).
void PsionCondor::setCts(bool s) {
    if (s != m_cts) { m_msrDelta |= 0x01; m_cts = s; updateIrq(); }
}
void PsionCondor::setDsr(bool s) {
    if (s != m_dsr) { m_msrDelta |= 0x02; m_dsr = s; updateIrq(); }
}
void PsionCondor::setDcd(bool s) {
    if (s != m_dcd) { m_msrDelta |= 0x08; m_dcd = s; updateIrq(); }
}
void PsionCondor::setRi (bool s) {
    if (s != m_ri)  { m_msrDelta |= 0x04; m_ri  = s; updateIrq(); }
}
