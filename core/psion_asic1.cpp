// license:BSD-3-Clause
// copyright-holders:Nigel Barnes
//
// Standalone port of MAME src/devices/machine/psion_asic1.cpp.
//
// What is faithfully ported:
//   - I/O register map (A1Status / A1Control, A1LcdSize, A1LcdControl,
//     A1InterruptStatus / A1InterruptMask, A1NonSpecificEoi,
//     A1TimerEoi, A1FrcEoi, A1ResetWatchDog, A1FrcControl,
//     A1ProtectionOn / Off / Upper / Lower, A1Sound* placeholders).
//   - Eight-source PIC with vectors 0x78..0x7F and the highest-priority
//     scan that MAME does in its IRQ_CALLBACK_MEMBER.
//   - External interrupt inputs (eint1/eint2/eint3) and external NMI line.
//   - Watchdog NMI generation (3 unacknowledged 4 Hz ticks -> NMI).
//   - Tick interrupt, switchable between 4 Hz and ~32 Hz (the MAME source
//     uses 32.768 Hz = the RTC tap).
//   - Free-Running Counter: counts down at 512 kHz, raises FrcExpired on
//     underflow, toggles the FrcOvl output line, optionally auto-reloads
//     when in periodic mode (A1Status bit 0).
//   - Memory protection trap (A1ProtectionOn/Off/Upper/Lower) — writes
//     outside the protected window are dropped and an "address trap"
//     NMI is asserted, just like MAME.
//   - LCD scanout into a host buffer; reads VRAM through a host-installed
//     std::function rather than a MAME address_space.
//
// What is stubbed / no-op:
//   - A1Sound* registers (Lsw / Msw / Control). MAME just LOG()s them
//     and they have no audible effect on the Series 3 (the speaker is
//     driven through ASIC2's BuzzerToggle). We log nothing and store
//     nothing.
//   - The "side effects disabled" guard around A1ProtectionOff and
//     A1Status. We always run the side effect; the guard only matters
//     for MAME's debugger, not for execution.
//   - mem_r/mem_w in MAME bounce through a per-device 1 MB address space
//     that the Psion3 driver populates (RAM at 0x00000-, ROM at
//     0x80000-). Here we don't own RAM/ROM and just delegate to the
//     same memory reader the LCD uses. The parent driver should route
//     CPU memory accesses through its own bus and only call
//     PsionAsic1::ioRead/ioWrite for the chip's I/O window.
//   - readLCD currently always uses the MAME default VRAM bases
//     (0x00400 handheld, 0xb8000 laptop). The Psion 3 framebuffer lives
//     at the start of RAM; this matches MAME exactly.

#include "psion_asic1.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
constexpr uint16_t A1_STATUS_FRC_MODE     = 0x0001;
constexpr uint16_t A1_STATUS_TICK_RATE    = 0x0002;
constexpr uint16_t A1_STATUS_LCD_ENABLE   = 0x0020;
constexpr uint16_t A1_STATUS_WATCHDOG_NMI = 0x0100;
constexpr uint16_t A1_STATUS_EXTERNAL_NMI = 0x0200;

constexpr uint8_t  IRQ_TIMER         = 0x01;
constexpr uint8_t  IRQ_EXP_RIGHT_B   = 0x04;
constexpr uint8_t  IRQ_EXP_LEFT_A    = 0x08;
constexpr uint8_t  IRQ_ASIC2         = 0x10;
constexpr uint8_t  IRQ_FRC_EXPIRED   = 0x20;
} // namespace

PsionAsic1::PsionAsic1() {
    // No callbacks bound by default. Output callbacks default to no-op
    // lambdas so the chip is safe to drive even before the host wires
    // anything up.
    m_int_cb    = [](bool){};
    m_nmi_cb    = [](bool){};
    m_frcovl_cb = [](bool){};
    m_mem_reader = [](uint32_t) -> uint8_t { return 0; };
}

void PsionAsic1::setBusClock(int64_t hz) {
    if (hz > 0) m_bus_clock = hz;
}

void PsionAsic1::setMemReader(std::function<uint8_t(uint32_t)> reader) {
    if (reader) m_mem_reader = std::move(reader);
}

void PsionAsic1::reset() {
    // Default tick = 4 Hz; FRC = 512 kHz; watchdog = 4 Hz, matching
    // MAME's device_reset() defaults.
    rescheduleTickTimer();

    m_frc_period_cycles      = m_bus_clock / 512000;
    if (m_frc_period_cycles < 1) m_frc_period_cycles = 1;

    m_watchdog_period_cycles = m_bus_clock / 4;
    if (m_watchdog_period_cycles < 1) m_watchdog_period_cycles = 1;

    m_tick_remaining     = m_tick_period_cycles;
    m_frc_remaining      = m_frc_period_cycles;
    m_watchdog_remaining = m_watchdog_period_cycles;

    m_a1_status = 0x00;
    m_frc_count = 0;
    m_frc_reload = 0;
    m_frc_ovl = 0;
    m_watchdog_count = 0;

    m_a1_interrupt_status = 0x00;
    m_a1_interrupt_mask = 0x00;

    m_a1_protection_mode = false;
    m_a1_protection_lower = 0;
    m_a1_protection_upper = 0;

    m_int_line = false;
    m_nmi_line = false;
}

void PsionAsic1::rescheduleTickTimer() {
    // A1Status bit 1 (TickRate): 0 = 4 Hz, 1 = ~32 Hz (32.768 Hz on
    // real silicon, fed from the PS34 RTC tap).
    int64_t hz = (m_a1_status & A1_STATUS_TICK_RATE) ? 33 : 4;
    m_tick_period_cycles = m_bus_clock / hz;
    if (m_tick_period_cycles < 1) m_tick_period_cycles = 1;
    m_tick_remaining = m_tick_period_cycles;
}

void PsionAsic1::tick(int64_t cycles) {
    if (cycles <= 0) return;

    // Process each timer in a "while underflow" loop so that hosts
    // that pass us large chunks (e.g. an instruction that took more
    // than one timer period) don't lose events.

    m_tick_remaining -= cycles;
    while (m_tick_remaining <= 0) {
        m_tick_remaining += m_tick_period_cycles;
        fireTick();
    }

    m_frc_remaining -= cycles;
    while (m_frc_remaining <= 0) {
        m_frc_remaining += m_frc_period_cycles;
        fireFrc();
    }

    m_watchdog_remaining -= cycles;
    while (m_watchdog_remaining <= 0) {
        m_watchdog_remaining += m_watchdog_period_cycles;
        fireWatchdog();
    }
}

void PsionAsic1::fireTick() {
    m_a1_interrupt_status |= IRQ_TIMER;
    updateInterrupts();
}

void PsionAsic1::fireFrc() {
    // Decrement, then check the new value against the same two cases
    // MAME inspects in its TIMER_CALLBACK_MEMBER(frc).
    --m_frc_count;
    if (m_frc_count == 0x0000) {
        m_frc_ovl ^= 1;
        m_frcovl_cb(m_frc_ovl != 0);
        m_a1_interrupt_status |= IRQ_FRC_EXPIRED;
        updateInterrupts();
    } else if (m_frc_count == 0xffff) {
        if (m_a1_status & A1_STATUS_FRC_MODE) {
            m_frc_count = m_frc_reload;
        }
    }
}

void PsionAsic1::fireWatchdog() {
    m_watchdog_count = (m_watchdog_count + 1) & 3;
    if (m_watchdog_count == 3) {
        m_a1_status |= A1_STATUS_WATCHDOG_NMI;
        updateInterrupts();
    }
}

void PsionAsic1::setEint1(bool state) {
    if (state) m_a1_interrupt_status |=  IRQ_EXP_RIGHT_B;
    else       m_a1_interrupt_status &= ~IRQ_EXP_RIGHT_B;
    updateInterrupts();
}

void PsionAsic1::setEint2(bool state) {
    if (state) m_a1_interrupt_status |=  IRQ_EXP_LEFT_A;
    else       m_a1_interrupt_status &= ~IRQ_EXP_LEFT_A;
    updateInterrupts();
}

void PsionAsic1::setEint3(bool state) {
    if (state) m_a1_interrupt_status |=  IRQ_ASIC2;
    else       m_a1_interrupt_status &= ~IRQ_ASIC2;
    updateInterrupts();
}

void PsionAsic1::setEnmi(bool state) {
    if (state) m_a1_status |=  A1_STATUS_EXTERNAL_NMI;
    else       m_a1_status &= ~A1_STATUS_EXTERNAL_NMI;
    updateInterrupts();
}

void PsionAsic1::updateInterrupts(bool addressTrap) {
    bool irq = (m_a1_interrupt_status & m_a1_interrupt_mask) != 0;
    bool nmi = (m_a1_status & (A1_STATUS_WATCHDOG_NMI | A1_STATUS_EXTERNAL_NMI)) != 0
            || addressTrap;

    if (irq != m_int_line) {
        m_int_line = irq;
        m_int_cb(irq);
    }
    if (nmi != m_nmi_line) {
        m_nmi_line = nmi;
        m_nmi_cb(nmi);
    }
}

uint8_t PsionAsic1::inta() {
    // IRQ  Vector
    //  0    0x78  TINT
    //  1    0x79  EINT0  (mains detect)
    //  2    0x7A  EINT1
    //  3    0x7B  EINT2
    //  4    0x7C  EINT3  (ASIC2)
    //  5    0x7D  OVINT  (FRC overflow)
    //  6    0x7E  SRXI   (SLD receive)
    //  7    0x7F  STXI   (SLD transmit)
    uint8_t pending = m_a1_interrupt_status & m_a1_interrupt_mask;
    uint8_t vector = 0x78;
    for (int irq = 0; irq < 8; irq++) {
        if (pending & (1u << irq)) {
            vector = 0x78 + irq;
            break;
        }
    }
    return vector;
}

bool PsionAsic1::isProtected(uint32_t byteOffset) {
    if (m_a1_protection_mode &&
        (byteOffset <= m_a1_protection_lower || byteOffset > m_a1_protection_upper)) {
        updateInterrupts(true);
        return true;
    }
    return false;
}

uint16_t PsionAsic1::memRead(uint32_t offset, uint16_t mask) {
    // We don't own RAM/ROM; instead defer to the host-supplied byte
    // reader (used for both the LCD scanner and any direct mem_r the
    // host wants to route through us).
    uint32_t addr = offset & 0x000FFFFE;
    uint16_t lo = m_mem_reader(addr);
    uint16_t hi = m_mem_reader(addr + 1);
    return ((hi << 8) | lo) & mask;
}

void PsionAsic1::memWrite(uint32_t offset, uint16_t /*data*/, uint16_t /*mask*/) {
    // Host should not normally route memWrite through us — it owns the
    // bus. We retain the entry point so address-trap behaviour can still
    // be exercised. If the host does call us, drop the write when the
    // protection trap fires (matching MAME) and otherwise quietly ignore
    // it (we have nowhere to put the bytes).
    (void)isProtected(offset & 0x000FFFFE);
}

uint16_t PsionAsic1::ioRead(uint32_t offset, uint16_t /*mask*/) {
    uint16_t data = 0;
    switch (offset & 0x1E) {
    case 0x02: // A1Status
        data = m_a1_status;
        data |= uint16_t(m_lcd_id & 0x07) << 14;
        break;

    case 0x06: // A1InterruptStatus
        data = m_a1_interrupt_status & m_a1_interrupt_mask;
        break;

    case 0x08: // A1InterruptMask
        data = m_a1_interrupt_mask;
        break;

    case 0x12: // A1FrcControl (read of the live counter)
        data = m_frc_count;
        break;

    case 0x14: // A1ProtectionOff (read disables protection)
        m_a1_protection_mode = false;
        data = 0;
        break;

    default:
        data = 0xFFFF;
        break;
    }
    return data;
}

void PsionAsic1::ioWrite(uint32_t offset, uint16_t data, uint16_t /*mask*/) {
    switch (offset & 0x1E) {
    case 0x02: // A1Control (lower byte mirrors A1Status[7:0])
        if ((data & A1_STATUS_TICK_RATE) != (m_a1_status & A1_STATUS_TICK_RATE)) {
            // Update tick rate immediately and restart the countdown,
            // matching MAME (which adjusts the timer to attotime::zero).
            m_a1_status = (m_a1_status & 0xFF00) | (data & 0xFF);
            rescheduleTickTimer();
        } else {
            m_a1_status = (m_a1_status & 0xFF00) | (data & 0xFF);
        }
        break;

    case 0x04: // A1LcdSize
        m_a1_lcd_size = data;
        break;

    case 0x06: // A1LcdControl
        m_a1_lcd_control = data;
        break;

    case 0x08: // A1InterruptMask
        m_a1_interrupt_mask = data & 0xFF;
        updateInterrupts();
        {
            const char *e = std::getenv("PSION3_TRACE_KERNEL");
            if (e && *e && *e != '0') {
                std::fprintf(stderr,
                    "[asic1] A1InterruptMask <= %02x\n",
                    m_a1_interrupt_mask);
            }
        }
        break;

    case 0x0A: // A1NonSpecificEoi — MAME just logs; no state change
        break;

    case 0x0C: // A1TimerEoi
        m_a1_interrupt_status &= ~IRQ_TIMER;
        updateInterrupts();
        break;

    case 0x0E: // A1FrcEoi
        m_a1_interrupt_status &= ~IRQ_FRC_EXPIRED;
        updateInterrupts();
        break;

    case 0x10: // A1ResetWatchDog
        m_watchdog_count = 0;
        break;

    case 0x12: // A1FrcControl (write reload + load counter)
        m_frc_reload = data;
        m_frc_count  = data;
        break;

    case 0x14: // A1ProtectionOn
        m_a1_protection_mode = true;
        break;

    case 0x16: // A1ProtectionUpper
        m_a1_protection_upper = (uint32_t(data) << 4) | 0x0F;
        break;

    case 0x18: // A1ProtectionLower
        m_a1_protection_lower = uint32_t(data) << 4;
        break;

    case 0x1A: // A1SoundLsw  — stubbed (no audio path here)
    case 0x1C: // A1SoundMsw  — stubbed
    case 0x1E: // A1SoundControl — stubbed
        break;

    default:
        break;
    }
}

bool PsionAsic1::readLCD(uint8_t *dst, int width, int height, int plates) const {
    if (!dst || width <= 0 || height <= 0 || plates <= 0) return false;

    if (!(m_a1_status & A1_STATUS_LCD_ENABLE)) {
        std::memset(dst, 0, size_t(width) * size_t(height));
        return false;
    }

    // VRAM base mirrors the MAME source (it's a fixed offset in the
    // V30's address space — the parent driver maps RAM at 0 and the
    // PS3 video buffer always lives at 0x400; HC/MC put it at 0xB8000).
    uint32_t videoram = m_laptop_mode ? 0xB8000u : 0x00400u;

    // The row stride in VRAM comes from A1LcdSize bits[14:10]:
    //   pixels_per_row = ((bits[14:10]) + 1) * 32
    // For the Series 3 the kernel programs this to 256 pixels = 32 bytes
    // even though only the first 240 pixels are visible. Using the caller's
    // `width` as the stride caused a 2-byte-per-row offset that smeared a
    // vertical dotted artefact across the whole image.
    int stride_pixels = ((m_a1_lcd_size >> 10) & 0x1F) + 1;
    stride_pixels *= 32;
    if (stride_pixels < width) stride_pixels = width; // paranoia
    int src_byte_stride = stride_pixels / 8;

    int per_plate_height = height / plates;

    for (int plate = 0; plate < plates; plate++) {
        uint32_t plate_base = videoram + (uint32_t(plate) << 14);
        for (int y = 0; y < per_plate_height; y++) {
            uint32_t row_base = plate_base + uint32_t(y) * uint32_t(src_byte_stride);
            int dst_y = plate * per_plate_height + y;
            uint8_t *row_dst = dst + size_t(dst_y) * size_t(width);
            // Walk source bytes; only emit the first `width` pixels.
            for (int x = 0; x < width; x++) {
                int byte_idx = x >> 3;
                int bit = x & 7;
                uint8_t pixels = m_mem_reader(row_base + uint32_t(byte_idx));
                // MAME's pen lookup uses BIT(pixels, i) for i=0..7 —
                // bit 0 is the leftmost pixel of the byte.
                row_dst[x] = (pixels >> bit) & 1;
            }
        }
    }
    return true;
}
