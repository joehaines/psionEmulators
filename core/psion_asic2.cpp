// license:BSD-3-Clause
// copyright-holders:Nigel Barnes
//
// Standalone port of MAME src/devices/machine/psion_asic2.cpp.
//
// What is faithfully ported:
//   - Indexed-control register file (A2Index + four A2IControl banks for
//     CLKSEL, channel-clock enables, port output data and DDR).
//   - 8-bit register window R/W behaviour for A2Status, A2InterruptStatus,
//     A2External (keyboard column poll), A2KeyData (slow-port read),
//     A2SerialData / A2SerialControl, A2ChannelControl, A2Control1/2/3.
//   - Edge-driven IRQ + NMI outputs derived from A2Status[0,5] (IRQ)
//     and A2InterruptStatus[0:1] gated by A2Control3[4:5] (NMI).
//   - Single-shot "busy" timer that holds the CBUSY output low for
//     12 SIBO clocks (~6.25 us at 1.92 MHz) after each frame transfer.
//   - SIBO 8-channel transmit/receive multiplex driven by
//     A2ChannelControl bit layout.
//   - Buzzer driver: BuzzerToggle path when BuzzerMode=0, FRC-overflow
//     path (frcOvlIn) when BuzzerMode=1, plus the volume gate.
//   - On / Reset / DoorNmi / SDS interrupt input handlers.
//
// What is stubbed / no-op:
//   - A2SlaveData (offset 0x07 read) — MAME hard-wires 0 here, we do
//     too; the real-hardware slave interface isn't used by Series 3.
//   - The detailed CLKSEL crystal-divider effect; we store the value
//     but don't reprogram the bus clock from it. MAME doesn't either.
//   - The "DTMF tone generator" referenced in psion3.cpp's TODO is on
//     a separate PCD3311 device, not ASIC2 — not our concern.

#include "psion_asic2.h"

#include <utility>

namespace {
inline uint8_t bit(uint8_t v, int b) { return (v >> b) & 1; }
inline uint8_t bits(uint8_t v, int lo, int n) { return (v >> lo) & ((1u << n) - 1u); }
} // namespace

PsionAsic2::PsionAsic2() {
    m_int_cb     = [](bool){};
    m_nmi_cb     = [](bool){};
    m_cbusy_cb   = [](bool){};
    m_buz_cb     = [](bool){};
    m_buzvol_cb  = [](bool){};
    m_dr_cb      = [](bool){};
    m_col_cb     = [](int) -> uint8_t { return 0xFF; };
    m_read_pd_cb = []() -> uint8_t { return 0; };
    m_write_pd_cb= [](uint8_t){};
    for (int i = 0; i < 8; i++) {
        m_data_r[i] = []() -> uint8_t { return 0; };
        m_data_w[i] = [](uint16_t){};
    }
}

void PsionAsic2::setBusClock(int64_t hz) {
    if (hz > 0) m_bus_clock = hz;
}

void PsionAsic2::reset() {
    m_a2_index = 0;
    m_a2_icontrol0 = 0;
    m_a2_icontrol1 = 0;
    m_a2_iddr = 0;
    m_a2_control1 = 0;
    m_a2_control2 = 0;
    m_a2_control3 = 0;
    m_a2_serial_data = 0;
    m_a2_serial_control = 0;
    m_a2_interrupt_status = 0;
    // A1ResetFlag latched on power-up so the boot ROM (Series 3 v1.91f
    // and MC400 V1.26F both rely on it) sees a fresh cold-boot when it
    // reads A2Status during early POST. MAME wires this through the
    // !RESET line via reset_w(0); we hard-set the bit at chip reset
    // because our host always cold-boots. Reading A2Status clears this
    // bit (see ioRead case 0x04), so it's a one-shot.
    m_a2_status = 0x04; // A1ResetFlag
    m_a2_channel_control = 0;

    m_int_line = false;
    m_nmi_line = false;
    m_busy_remaining = 0;
}

void PsionAsic2::tick(int64_t cycles) {
    if (cycles <= 0) return;
    if (m_busy_remaining > 0) {
        m_busy_remaining -= cycles;
        if (m_busy_remaining <= 0) {
            m_busy_remaining = 0;
            fireBusyTimer();
        }
    }
}

void PsionAsic2::fireBusyTimer() {
    m_a2_status &= ~0x10; // clear SerialBusy
    m_cbusy_cb(true);
}

void PsionAsic2::setChannelReadCb(int channel, std::function<uint8_t()> cb) {
    if (channel < 0 || channel >= 8) return;
    if (cb) m_data_r[channel] = std::move(cb);
}

void PsionAsic2::setChannelWriteCb(int channel, std::function<void(uint16_t)> cb) {
    if (channel < 0 || channel >= 8) return;
    if (cb) m_data_w[channel] = std::move(cb);
}

// ---------------------------------------------------------------------
// External input handlers
// ---------------------------------------------------------------------

void PsionAsic2::setOnClr(bool state) {
    if (state) m_a2_status |=  0x01; // A1OnKey
    else       m_a2_status &= ~0x01;
    updateInterrupts();
}

void PsionAsic2::setSdsInt(bool state) {
    if (state) m_a2_status |=  0x20; // A2Sdis
    else       m_a2_status &= ~0x20;
    updateInterrupts();
}

void PsionAsic2::setDoorNmi(bool state) {
    if (state) m_a2_interrupt_status |=  0x01; // DNMI
    else       m_a2_interrupt_status &= ~0x01;
    updateInterrupts();
}

void PsionAsic2::frcOvlIn(bool state) {
    // Only routes to the buzzer when BuzzerMode (control2 bit 4) is set.
    if (bit(m_a2_control2, 4)) {
        m_buz_cb(state);
    }
}

void PsionAsic2::setResetLine(bool state) {
    // MAME latches A1ResetFlag on the falling edge of !RESET (state=0).
    if (!state) m_a2_status |= 0x04;
}

void PsionAsic2::updateInterrupts() {
    bool irq = (m_a2_status & 0x21) != 0;
    // NMI: latched DNMI / ExpansionInterrupt bits gated by Control3
    // bits 4 (DoorEnable) and 5 (ExpansionEnable) — i.e. the same
    // two-bit AND that MAME does with BIT(...,0,2) & BIT(...,4,2).
    uint8_t nmi_bits  = bits(m_a2_interrupt_status, 0, 2);
    uint8_t nmi_gates = bits(m_a2_control3, 4, 2);
    bool nmi = (nmi_bits & nmi_gates) != 0;

    if (irq != m_int_line) {
        m_int_line = irq;
        m_int_cb(irq);
    }
    if (nmi != m_nmi_line) {
        m_nmi_line = nmi;
        m_nmi_cb(nmi);
    }
}

// ---------------------------------------------------------------------
// Register read/write
// ---------------------------------------------------------------------

uint8_t PsionAsic2::ioRead(uint32_t offset) {
    uint8_t data = 0;
    switch (offset & 7) {
    case 0x00: // A2Index
        data = m_a2_index;
        break;

    case 0x01: // A2Control - indexed
        switch (m_a2_index) {
        case 0: data = m_a2_icontrol0; break;
        case 1: data = m_a2_icontrol1; break;
        case 2: data = 0; break; // A2IWrite is write-only
        case 3: data = m_a2_iddr; break;
        }
        break;

    case 0x02: // A2External - keyboard column poll
        switch (m_a2_control1 & 0x0F) {
        case 0x0F: // poll all 10 columns
            for (int i = 0; i < 10; i++) data |= m_col_cb(i);
            break;
        case 0x00: case 0x01: case 0x02: case 0x03: case 0x04:
        case 0x05: case 0x06: case 0x07: case 0x08: case 0x09:
        case 0x0A:
            // MAME's selector is "(KeyScan & 0x0F) - 1" — yes, 0x00
            // selects column 0xFF (-1 wrapped to 0xFF), but the
            // keyboard column callback ignores that. We mirror it
            // verbatim so quirky ROM probing matches.
            data = m_col_cb(int8_t(m_a2_control1 & 0x0F) - 1);
            break;
        default:
            data = 0;
            break;
        }
        break;

    case 0x03: // A2InterruptStatus
        data = m_a2_interrupt_status;
        break;

    case 0x04: // A2Status (reading clears A1OnKey + A1ResetFlag)
        data = m_a2_status;
        m_a2_status &= ~0x01;
        m_a2_status &= ~0x04;
        // The IRQ line is derived from (m_a2_status & 0x21). Clearing
        // A1OnKey inside the kernel's EINT3 ISR needs to drop the line so
        // the V30 doesn't re-enter the ISR on every IRET — without this
        // the Series 3's cold-boot warning dialog cannot be dismissed by
        // Esc because the kernel's event loop never gets a chance to run
        // between back-to-back IRQ dispatches. MAME's asic2 device_r has
        // the same textbook omission, but its NEC/V30 core re-samples
        // the line differently around int-ack; our simpler V30 core needs
        // the ASIC to drop INT explicitly.
        updateInterrupts();
        break;

    case 0x05: // A2SerialData (reading triggers next frame in ReadMulti)
        data = m_a2_serial_data;
        if ((m_a2_serial_control & 0x10) == 0x10)
            m_a2_serial_data = receiveFrame();
        break;

    case 0x06: // A2KeyData / Port-Data input
        data = m_read_pd_cb();
        break;

    case 0x07: // A2SlaveData — MAME returns 0
        data = 0;
        break;

    default:
        break;
    }
    return data;
}

void PsionAsic2::ioWrite(uint32_t offset, uint8_t data) {
    switch (offset & 7) {
    case 0x00: // A2Index — only bottom two bits are valid
        m_a2_index = data & 3;
        break;

    case 0x01: // A2Control - indexed write
        switch (m_a2_index) {
        case 0: // A2IControl0 — CLKSEL/CHEN3/4/6/EXONOFF/INTSEL
            m_a2_icontrol0 = data;
            break;
        case 1: // A2IControl1 — CKEN1..4
            m_a2_icontrol1 = data;
            break;
        case 2: // A2IWrite — masked port output
            m_write_pd_cb(data & m_a2_iddr);
            break;
        case 3: // A2IDDR — direction register
            m_a2_iddr = data;
            break;
        }
        break;

    case 0x02: // A2Control1 — KeyScan + serial clock rate
        m_a2_control1 = data;
        break;

    case 0x03: // A2Control2 — digitiser / buzzer / channel-clock enables
        m_a2_control2 = data;
        m_dr_cb(bit(data, 1) != 0);
        // Buzzer is driven directly by BuzzerToggle bit when BuzzerMode==0;
        // otherwise it tracks frcOvlIn().
        if (!bit(data, 4)) {
            m_buz_cb(bit(data, 2) != 0);
        }
        m_buzvol_cb(bit(data, 3) != 0);
        break;

    case 0x04: // A2Control3 — door / expansion / SLD / Vpp
        m_a2_control3 = data;
        if (bit(data, 0)) {
            // SerialNull — emit a NULL frame on the next bus tick.
            transmitFrame(NULL_FRAME);
        }
        // Re-evaluate NMI gating since DoorEnable/ExpansionEnable changed.
        updateInterrupts();
        break;

    case 0x05: // A2SerialData (write)
        if ((m_a2_serial_control & 0xC0) == 0x80) {
            transmitFrame(DATA_FRAME | data);
        }
        break;

    case 0x06: // A2SerialControl — issues a control frame and may
               // immediately latch a response in ReadSingle/ReadMulti.
        m_a2_serial_control = data;
        transmitFrame(CONTROL_FRAME | data);
        if ((m_a2_serial_control & 0x40) == 0x40) {
            m_a2_serial_data = receiveFrame();
        }
        break;

    case 0x07: // A2ChannelControl — pack/peripheral channel select
        m_a2_channel_control = data;
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------------
// SIBO Serial Protocol Controller
// ---------------------------------------------------------------------

bool PsionAsic2::channelActive(int channel) const {
    switch (channel) {
    case 0:
        return bits(m_a2_channel_control, 4, 3) == 4;
    case 1: case 2: case 3: case 4:
        return bit(m_a2_channel_control, channel - 1) != 0;
    case 5: case 6: case 7:
        return bits(m_a2_channel_control, 4, 3) == channel;
    }
    return false;
}

void PsionAsic2::transmitFrame(uint16_t data) {
    m_a2_status |= 0x10; // SerialBusy
    // 12 SIBO bus ticks at clock/2 — convert to host cycles.
    if (m_bus_clock > 0) {
        // 12 ticks * (host cycles per tick at clock/2) == 24 cycles at
        // m_bus_clock; we just use 24 host cycles as a reasonable
        // approximation since bus clock == host CPU clock here.
        m_busy_remaining = 24;
    } else {
        m_busy_remaining = 24;
    }
    m_cbusy_cb(false);

    for (int ch = 0; ch < 8; ch++) {
        if (channelActive(ch)) {
            m_data_w[ch](data);
        }
    }
}

uint8_t PsionAsic2::receiveFrame() {
    uint8_t data = 0;

    m_a2_status |= 0x10;
    m_busy_remaining = 24;
    m_cbusy_cb(false);

    for (int ch = 0; ch < 8; ch++) {
        if (channelActive(ch)) {
            data |= m_data_r[ch]();
        }
    }
    return data;
}
