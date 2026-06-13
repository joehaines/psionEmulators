// license:BSD-3-Clause
// copyright-holders:Sandro Ronco (MAME psion driver)
//                  + adaptation by the Psion emulator project, 2026.
//
// Psion Organiser II driver — scaffold implementation.
// See organiser2.h for the memory map and the porting source.

#include "organiser2.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Organiser2 {

Emulator::Emulator() {
    rom.fill(0xFF);
    ram.fill(0x00);
    // ── Port 5 read: keyboard + battery + ON key ────────────────────
    // Per MAME's psion_state::port5_r, the OS reads:
    //   bits 2-6: keyboard column return (kb_read())
    //   bit 0:    battery good (m_battery)
    //   bit 1:    pulse_enable / kb_counter==0x7FF
    //   bit 7:    ON / CLEAR key
    // kb_read selects a row by m_kbCounter matching `0x7F & ~(1<<line)`
    // — a one-hot-zero pattern. With no key pressed, all rows are 0xFF
    // (IP_ACTIVE_LOW); the ANDed scan returns 0x7C (no keys in any row).
    cpu.port5Reader = [this]() -> uint8_t {
        // MAME's psion_state::kb_read: matches the FULL m_kb_counter
        // (NOT just the low 7 bits) against each one-hot-zero pattern.
        // So patterns are exactly 0x7E, 0x7D, 0x7B, 0x77, 0x6F, 0x5F,
        // 0x3F (low 7 bits = pattern, high 4 bits = 0). Counter values
        // outside that set return the default (0x7C, no row).
        uint8_t kb = 0x7C;
        if (m_kbCounter == 0) {
            // All rows ANDed (no specific row selected)
            uint8_t all = 0xFF;
            for (int line = 0; line < 7; line++) all &= m_keyRow[line];
            kb = all & 0x7C;
        } else {
            for (int line = 0; line < 7; line++) {
                if (m_kbCounter == uint16_t(0x7F & ~(1 << line))) {
                    kb = m_keyRow[line] & 0x7C;
                    break;
                }
            }
        }
        // Battery: bit 0 = 0 means GOOD (per MAME PORT_CONFSETTING).
        // Bit 0 = 1 means LOW battery, which makes the OS show
        // "BATTERY TOO LOW" instead of advancing to the menu.
        uint8_t battery = 0x00;
        // ON key on bit 7 (active high — m_onKey is "is pressed")
        uint8_t onKey = m_onKey ? 0x80 : 0x00;
        // Pulse enable / kb_counter rollover indicator
        uint8_t pulseBit = (m_pulseEnable || m_kbCounter == 0x7FF) ? 0x02 : 0x00;
        return kb | battery | pulseBit | onKey;
    };
    // ── Port 6 read: datapak control OR-bus ────────────────────────
    // With no datapaks inserted, the OR of pack control bytes is 0.
    // Once we wire pack[].control_r() this returns the live pack
    // status; for now return 0 so the OS sees "no pack present".
    cpu.port6Reader = [this]() -> uint8_t {
        return 0;
    };
    // ── Port 2 read/write: datapak data bus ─────────────────────────
    // Reads return the OR of pack data (0 with no packs).
    // Writes broadcast to all packs (no-op without inserted packs).
    cpu.port2Reader = [this]() -> uint8_t {
        return 0;
    };
    cpu.port2Writer = [this](uint8_t /*v*/) {
        // No packs attached — drop.
    };
    // ── Port 6 write: datapak control broadcast ─────────────────────
    cpu.port6Writer = [this](uint8_t /*v*/) {
        // No packs attached — drop.
    };
}

void Emulator::loadROM(uint8_t *buffer, size_t size) {
    std::memset(rom.data(), 0xFF, rom.size());
    std::memcpy(rom.data(), buffer, std::min(size, rom.size()));
    if (!m_initialised) {
        cpu.reset();
        lcd.reset();
        pack[0].reset();
        pack[1].reset();
        m_initialised = true;
    }
}

void Emulator::loadRamSnapshot(const uint8_t *bytes, size_t size) {
    std::memcpy(ram.data(), bytes, std::min(size, ram.size()));
}

void Emulator::executeUntil(int64_t cycles) {
    // Step the CPU until the cycle budget is exhausted, ticking the
    // on-chip timer once per instruction so OCF / TOF interrupts fire
    // at the right cadence. Also pulses NMI at 1 Hz (per MAME's
    // psion2_state::nmi_timer) when the OS has enabled m_nmi_enable
    // — the OS uses this for its wake-up / keyboard-scan / time-of-
    // day path and stalls forever without it.
    while (passedCycles < cycles) {
        // 1 Hz periodic NMI when enabled (drives the OS's wake /
        // keyboard-scan / time-of-day path).
        if (m_nmiEnable && passedCycles >= m_nextNmiCycles) {
            cpu.setNmiLine(true);
            m_nextNmiCycles += E_CLOCK_HZ;
        }
        // ON / CLEAR key on real hardware pulses NMI on press,
        // independent of the periodic timer — the OS uses it to wake
        // from power-off. We pulse on the leading edge.
        if (m_onKey && !m_onKeyEdgeFired) {
            cpu.setNmiLine(true);
            m_onKeyEdgeFired = true;
        }
        if (!m_onKey) m_onKeyEdgeFired = false;
        int64_t consumed = cpu.step();
        if (consumed <= 0) consumed = 1;
        cpu.tickTimer(consumed);
        passedCycles += consumed;
        if (cpu.halted) {
            passedCycles = cycles;
            break;
        }
    }
    if (passedCycles < cycles) passedCycles = cycles;
}

// ── HD6303Bus dispatch ────────────────────────────────────────────────

uint8_t Emulator::readByte(uint16_t addr) {
    // The HD6303X reserves $0000-$001F for its on-chip register file
    // and $0020-$00FF for on-chip RAM. The CPU intercepts those before
    // the bus sees them, so any access here means the CPU forwarded a
    // mirror access — return 0 as a defensive default.
    if (addr < 0x0100) return 0x00;

    if (addr < 0x0400) {
        return ioRead(addr);
    }
    if (addr < 0x4000) {
        // rambank1: fixed window from RAM offset 0. The 1 KiB at the
        // bottom of the window ($0400) starts from RAM offset 0; that's
        // a hardware quirk where the I/O block "steals" RAM offset
        // $0000-$03FF from CPU view but they're still part of the same
        // physical RAM chip.
        size_t off = addr;            // CPU $0400 → RAM offset $0400
        return (off < ram.size()) ? ram[off] : 0xFF;
    }
    if (addr < 0x8000) {
        // rambank2: 16 KiB banked window. Entry N maps to physical RAM
        // starting at offset $4000 + N*0x4000.
        size_t off = 0x4000 + size_t(m_ramBank2) * 0x4000 + (addr - 0x4000);
        return (off < ram.size()) ? ram[off] : 0xFF;
    }
    if (addr < 0xC000) {
        // rombank2: ROM image offset depends on entry.
        //   entry 0 → ROM[$0000]   (page 0)
        //   entry 1 → ROM[$8000]   (page 2)
        //   entry 2 → ROM[$C000]   (page 3)
        // (page 1 is reserved for the fixed rombank1 below.)
        size_t base = (m_romBank2 == 0) ? 0x0000
                                        : 0x8000 + size_t(m_romBank2 - 1) * 0x4000;
        return rom[base + (addr - 0x8000)];
    }
    // $C000-$FFFF: rombank1 — fixed at ROM page 1 ($4000-$7FFF). The
    // reset vectors live at the very top of this page.
    return rom[kRomBank1Base + (addr - 0xC000)];
}

void Emulator::writeByte(uint16_t addr, uint8_t v) {
    if (addr < 0x0100) return;            // CPU-internal range
    if (addr < 0x0400) { ioWrite(addr, v); return; }
    if (addr < 0x4000) {
        size_t off = addr;                // rambank1 fixed (mirrors readByte)
        if (off < ram.size()) ram[off] = v;
        return;
    }
    if (addr < 0x8000) {
        size_t off = 0x4000 + size_t(m_ramBank2) * 0x4000 + (addr - 0x4000);
        if (off < ram.size()) ram[off] = v;
        return;
    }
    // ROM windows are read-only; writes are dropped (intentional —
    // MAME's psion.cpp wires .bankr (read-only) for both ROM windows).
}

// ── MMIO ($0100-$03FF) ────────────────────────────────────────────────
//
// The Organiser II decodes a small set of latches inside this window:
// LCD command/data (HD44780 RS = address-bit), beeper enable, keyboard
// row strobe, datapak control / data, and the ROM/RAM bank-select
// latches. Concrete register addresses are inside MAME's psion.cpp
// io_r/io_w body — TBD during the CPU port. For now we route nothing,
// returning 0xFF on read and dropping writes.

// I/O side effects shared between read and write paths — mirrors
// MAME's psion2_state::io_rw. The LCD case is the only one that
// actually returns data on read; everything else returns 0 (matching
// MAME's io_r default of "return 0").
void Emulator::ioSideEffect(uint16_t off) {
    switch (off & 0xFFC0) {
    case 0x0C0:
        // Standby — switch device off. Drives the M6801 STBY input on
        // real hardware. We just clear NMI for now and don't model the
        // standby state; future work would put the CPU in low-power
        // wait until a wake-up event.
        m_nmiEnable = false;
        return;
    case 0x100:
        m_pulseEnable = true;
        return;
    case 0x140:
        m_pulseEnable = false;
        return;
    case 0x180:
        m_beep = true;
        return;
    case 0x1C0:
        m_beep = false;
        return;
    case 0x200:
        m_kbCounter = 0;
        return;
    case 0x240:
        if (off == 0x260) {
            m_ramBank2 = 0;
            m_romBank2 = 0;
        } else {
            m_kbCounter = (m_kbCounter + 1) & 0x7FF;
        }
        return;
    case 0x280:
        if (off == 0x2A0) {
            if (m_ramBank2 + 1 < kRamBank2Count) m_ramBank2++;
        } else {
            m_nmiEnable = true;
        }
        return;
    case 0x2C0:
        if (off == 0x2E0) {
            if (m_romBank2 + 1 < kRomBank2Count) m_romBank2++;
        } else {
            m_nmiEnable = false;
        }
        return;
    default:
        return;
    }
}

uint8_t Emulator::ioRead(uint16_t addr) {
    // MAME's io_r dispatches LCD on (offset & 0xFFC0) == 0x80, else
    // delegates to io_rw which returns 0. Mirror that.
    uint16_t off = uint16_t(addr - 0x0100);
    if ((off & 0xFFC0) == 0x80) {
        return (off & 1) ? lcd.readData() : lcd.readStatus();
    }
    ioSideEffect(off);
    return 0;
}

void Emulator::ioWrite(uint16_t addr, uint8_t v) {
    uint16_t off = uint16_t(addr - 0x0100);
    if ((off & 0xFFC0) == 0x80) {
        if (std::getenv("ORG2_LCD_TRACE")) {
            static int n = 0;
            if (n < 200) {
                std::fprintf(stderr, "[lcd-w] %s $%02x\n",
                    (off & 1) ? "data" : "cmd ", v);
                n++;
            }
        }
        if (off & 1) lcd.writeData(v);
        else         lcd.writeCommand(v);
        return;
    }
    ioSideEffect(off);
}

// ── LCD readout ───────────────────────────────────────────────────────

// Debug helper for harness — dump the HD44780 DDRAM as ASCII.
// Triggered by ORG2_LCD_DUMP=1 at process exit (called from any
// readLCDIntoBuffer that runs late in a session).
void Emulator::dumpStateTable() const {
    if (!std::getenv("ORG2_STATE_DUMP")) return;
    static int dumped = 0;
    if (dumped > 5) return;
    dumped++;
    std::fprintf(stderr, "[state] RAM $2040-$2060:\n  ");
    for (int i = 0; i < 0x20; i++) {
        std::fprintf(stderr, "%02x ", ram[0x2040 + i]);
        if ((i & 7) == 7 && i != 0x1F) std::fprintf(stderr, "\n  ");
    }
    std::fprintf(stderr, "\n[state] RAM $20A0-$20C0:\n  ");
    for (int i = 0; i < 0x20; i++) {
        std::fprintf(stderr, "%02x ", ram[0x20A0 + i]);
        if ((i & 7) == 7 && i != 0x1F) std::fprintf(stderr, "\n  ");
    }
    std::fprintf(stderr, "\n[state] RAM $2180-$21A0 (where $2184 lives):\n  ");
    for (int i = 0; i < 0x20; i++) {
        std::fprintf(stderr, "%02x ", ram[0x2180 + i]);
        if ((i & 7) == 7 && i != 0x1F) std::fprintf(stderr, "\n  ");
    }
    std::fprintf(stderr, "\n");
}

void Emulator::dumpLcdDdram() const {
    if (!std::getenv("ORG2_LCD_DUMP")) return;
    static int dumped = 0;
    if (dumped > 5) return;
    dumped++;
    const uint8_t *d = lcd.debugDdram();
    // Dump the full $00-$7F DDRAM range — 8 rows of 16 bytes each.
    std::fprintf(stderr, "[lcd-ddram] full 128-byte dump:\n");
    for (int r = 0; r < 8; r++) {
        std::fprintf(stderr, "  $%02x: |", r * 16);
        for (int c = 0; c < 16; c++) {
            uint8_t b = d[r * 16 + c];
            char ch = (b >= 0x20 && b < 0x7F) ? char(b) : '.';
            std::fprintf(stderr, "%c", ch);
        }
        std::fprintf(stderr, "|  ");
        for (int c = 0; c < 16; c++) {
            uint8_t b = d[r * 16 + c];
            std::fprintf(stderr, "%02x ", b);
        }
        std::fprintf(stderr, "\n");
    }
    std::fprintf(stderr, "  display_on=%d\n", (int)lcd.debugDisplayOn());
}

void Emulator::readLCDIntoBuffer(uint8_t **lines, bool is32BitOutput) const {
    dumpLcdDdram();
    dumpStateTable();
    const int outW = getLCDWidth();
    const int outH = getLCDHeight();
    uint8_t fb[PIXEL_W * PIXEL_H] = {};
    lcd.renderFramebuffer(fb, PIXEL_W, PIXEL_H);
    for (int y = 0; y < outH; y++) {
        const int srcY = (y * PIXEL_H) / outH;
        for (int x = 0; x < outW; x++) {
            const int srcX = (x * PIXEL_W) / outW;
            const uint8_t bit = fb[srcY * PIXEL_W + srcX];
            if (is32BitOutput) {
                auto line = reinterpret_cast<uint32_t *>(lines[y]);
                // Olive/grey LCD aesthetic similar to a real Psion II.
                line[x] = bit ? 0xFF202018u : 0xFF8C9670u;
            } else {
                lines[y][x] = bit ? 0x00 : 0xC8;
            }
        }
    }
}

// ── Keyboard ──────────────────────────────────────────────────────────

void Emulator::setKeyboardKey(EpocKey key, bool value) {
    // Map EpocKey → Organiser II 7-row matrix per MAME's
    // INPUT_PORTS_START(psion2). Bits are IP_ACTIVE_LOW so a press
    // CLEARS the bit, a release SETS it. The matrix has 5 columns per
    // row (bits $04, $08, $10, $20, $40); other bit positions read 1.
    //
    // Row index is the kb_counter value the OS reads: K1=0, K2=1, ...
    // K7=6. Index 7 is unused (returns $FF).
    //
    // ON / CLEAR is special — it goes through a dedicated NMI line on
    // real hardware. We surface it via m_onKey; the NMI scheduler can
    // pulse on the rising edge to wake the device.
    int row = -1;
    uint8_t bit = 0;
    switch (static_cast<int>(key)) {  // ASCII char-literal cases below are intentional
    // Special keys
    case EStdKeyEscape:    m_onKey = value; return;       // ON / Clear
    case EStdKeyMenu:      row = 0; bit = 0x04; break;    // K1: MODE
    case EStdKeyTab:       row = 0; bit = 0x04; break;    // alias for MODE
    case EStdKeyUpArrow:   row = 0; bit = 0x08; break;    // K1: ↑ / CAP
    case EStdKeyDownArrow: row = 0; bit = 0x10; break;    // K1: ↓ / NUM
    case EStdKeyLeftArrow: row = 0; bit = 0x20; break;    // K1: ←
    case EStdKeyRightArrow:row = 0; bit = 0x40; break;    // K1: →
    case EStdKeyLeftShift:
    case EStdKeyRightShift: row = 1; bit = 0x04; break;   // K2: SHIFT
    case EStdKeyDelete:
    case EStdKeyBackspace: row = 2; bit = 0x04; break;    // K3: DEL
    case EStdKeySpace:     row = 4; bit = 0x04; break;    // K5: SPACE
    case EStdKeyEnter:     row = 5; bit = 0x04; break;    // K6: EXE

    // Letter keys A-Z. Row/bit per MAME's INPUT_PORTS_START(psion2).
    // EStdKeyA = 65, B = 66, ... Z = 90 (ASCII codes also used by EPOC).
    case (EpocKey)65: row = 1; bit = 0x40; break;  // A
    case (EpocKey)66: row = 2; bit = 0x40; break;  // B
    case (EpocKey)67: row = 3; bit = 0x40; break;  // C
    case (EpocKey)68: row = 6; bit = 0x40; break;  // D
    case (EpocKey)69: row = 4; bit = 0x40; break;  // E
    case (EpocKey)70: row = 5; bit = 0x40; break;  // F
    case (EpocKey)71: row = 1; bit = 0x20; break;  // G
    case (EpocKey)72: row = 2; bit = 0x20; break;  // H
    case (EpocKey)73: row = 3; bit = 0x20; break;  // I
    case (EpocKey)74: row = 6; bit = 0x20; break;  // J
    case (EpocKey)75: row = 4; bit = 0x20; break;  // K
    case (EpocKey)76: row = 5; bit = 0x20; break;  // L
    case (EpocKey)77: row = 1; bit = 0x10; break;  // M
    case (EpocKey)78: row = 2; bit = 0x10; break;  // N
    case (EpocKey)79: row = 3; bit = 0x10; break;  // O
    case (EpocKey)80: row = 6; bit = 0x10; break;  // P
    case (EpocKey)81: row = 4; bit = 0x10; break;  // Q
    case (EpocKey)82: row = 5; bit = 0x10; break;  // R
    case (EpocKey)83: row = 1; bit = 0x08; break;  // S
    case (EpocKey)84: row = 2; bit = 0x08; break;  // T
    case (EpocKey)85: row = 3; bit = 0x08; break;  // U
    case (EpocKey)86: row = 6; bit = 0x08; break;  // V
    case (EpocKey)87: row = 4; bit = 0x08; break;  // W
    case (EpocKey)88: row = 5; bit = 0x08; break;  // X
    case (EpocKey)89: row = 3; bit = 0x04; break;  // Y
    case (EpocKey)90: row = 6; bit = 0x04; break;  // Z
    default:               return;
    }
    if (row < 0) return;
    // Active LOW: press clears the bit, release sets it.
    if (value) m_keyRow[row] &= ~bit;
    else       m_keyRow[row] |=  bit;
}

// ── Datapak slots ─────────────────────────────────────────────────────

bool Emulator::attachDatapak(int slot, const uint8_t *bytes, size_t size) {
    if (slot < 0 || slot >= 2) return false;
    return pack[slot].attach(bytes, size);
}
void Emulator::detachDatapak(int slot) {
    if (slot < 0 || slot >= 2) return;
    pack[slot].detach();
}
bool Emulator::isDatapakInserted(int slot) const {
    if (slot < 0 || slot >= 2) return false;
    return pack[slot].isInserted();
}
size_t Emulator::getDatapakImageSize(int slot) const {
    if (slot < 0 || slot >= 2) return 0;
    return pack[slot].imageSize();
}
const uint8_t *Emulator::getDatapakImageData(int slot) const {
    if (slot < 0 || slot >= 2) return nullptr;
    return pack[slot].imageData();
}
EmuBase::PackKind Emulator::getDatapakKind(int slot) const {
    if (slot < 0 || slot >= 2) return PackKind::None;
    return pack[slot].kind();
}

} // namespace Organiser2
