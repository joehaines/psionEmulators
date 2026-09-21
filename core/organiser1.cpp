// license:BSD-3-Clause
// copyright-holders:Sandro Ronco (MAME psion driver)
//                  + adaptation by the Psion emulator project, 2026.
//
// Psion Organiser I driver. See organiser1.h for the memory map, the
// port wiring and the porting source (MAME src/mame/psion/psion.cpp,
// `psion1_state`).

#include "organiser1.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Organiser1 {

namespace {
// ORG1_TRACE=1 — one line per half sim-second with everything needed to
// see what the machine is doing: where it is executing, whether it has
// switched itself off, where the keyboard scan counter has got to, and
// what the panel currently reads.
bool traceOn() {
    static int cached = -1;
    if (cached < 0) {
        const char *e = std::getenv("ORG1_TRACE");
        cached = (e && *e && *e != '0') ? 1 : 0;
    }
    return cached != 0;
}
}  // namespace

Emulator::Emulator() {
    rom.fill(0xFF);
    ram.fill(0x00);
    lcd.setLayout(HD44780::Layout::Organiser1);

    // ── Port 5 read: keyboard + battery + ON key ────────────────────
    // psion_state::port5_r:
    //   b7    ON key, active high
    //   b6-b2 keyboard row return, active low
    //   b1    pulse / keyboard-counter rollover
    //   b0    battery status, 0 = good
    // The Organiser I never sets the pulse latch (that is the
    // Organiser II's $100/$140 I/O pair, which this machine has no
    // decode for), so bit 1 is only the counter's own 0x7FF rollover.
    cpu.port5Reader = [this]() -> uint8_t {
        uint8_t data = kbRead();
        if (m_onKey) data |= 0x80;
        if (m_kbCounter == 0x7FF) data |= 0x02;
        return data;                      // b0 left clear: battery good
    };

    // ── Port 6: datapak control lines ───────────────────────────────
    // psion_state::port6_w fans one control byte out to both slots,
    // moving each slot's own select bit down into the shared b4
    // position: slot 1 is b4 already, slot 2 is b5 >> 1.
    cpu.port6Writer = [this](uint8_t v) {
        pack[0].writeControl((v & 0x8F) | (v & 0x10));
        pack[1].writeControl((v & 0x8F) | ((v & 0x20) >> 1));
    };
    // port6_r ORs the slots' control status back together. Our
    // PsionDatapak models the image and the data bus but not the
    // control read-back (it is a stub shared with the Organiser II —
    // see psion_datapak.h), so this reads as "no pack driving the
    // control lines", which is also what MAME returns with both slots
    // empty.
    cpu.port6Reader = []() -> uint8_t { return 0; };

    // ── Port 2: datapak data bus ────────────────────────────────────
    // Wired to both slots in parallel; an empty slot drives nothing,
    // so the bus reads as the OR of whatever is inserted.
    cpu.port2Reader = [this]() -> uint8_t {
        uint8_t data = 0;
        for (auto &p : pack)
            if (p.isInserted()) data |= p.readData();
        return data;
    };
    cpu.port2Writer = [this](uint8_t v) {
        for (auto &p : pack)
            if (p.isInserted()) p.writeData(v);
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
    while (passedCycles < cycles) {
        if (traceOn() && passedCycles >= m_nextTraceCycles) {
            m_nextTraceCycles += E_CLOCK_HZ / 2;
            char line[17] = {};
            const uint8_t *d = lcd.debugDdram();
            for (int i = 0; i < 16; i++) {
                uint8_t b = d[(i < 8) ? i : 0x40 + (i - 8)];
                line[i] = (b >= 0x20 && b < 0x7F) ? char(b) : '.';
            }
            std::fprintf(stderr,
                "[org1] t=%.1fs pc=%04x sp=%04x standby=%d kbCounter=%04x "
                "on=%d display=%d nmi=%lld/%d |%s|\n",
                double(passedCycles) / E_CLOCK_HZ, cpu.pc, cpu.sp,
                int(m_standby), m_kbCounter, int(m_onKey),
                int(lcd.debugDisplayOn()),
                (long long)m_nmiCount, int(cpu.nmiLine), line);
        }

        // Cold boot ends with the ROM switching the machine off, so
        // synthesise the ON press that a user would make (see the
        // m_autoOn* comments in organiser1.h). Armed by the switch-off
        // itself, so it lands on a machine that is actually asleep and
        // waiting for it; skipped entirely once the user has pressed ON
        // themselves.
        if (m_autoOnArmed && !m_autoOnFired && !m_userTouchedOn) {
            if (!m_onKey && passedCycles >= m_autoOnAssertAt) {
                pressOn(true);
            } else if (m_onKey && passedCycles >= m_autoOnAssertAt + ON_HOLD_CYCLES) {
                pressOn(false);
                m_autoOnFired = true;
            }
        }

        if (m_standby) {
            // The CPU is parked on its backup supply: no instructions,
            // no interrupts, just time passing until ON resets it.
            passedCycles = cycles;
            break;
        }

        // 500 ms periodic NMI — the OS's clock and keyboard-scan tick.
        if (passedCycles >= m_nextNmiCycles) {
            cpu.setNmiLine(true);
            m_nmiCount++;
            m_nextNmiCycles += NMI_PERIOD_CYCLES;
        }

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

// ── Keyboard scan ─────────────────────────────────────────────────────

uint8_t Emulator::kbRead() const {
    // psion_state::kb_read. The scan counter selects one row by holding
    // exactly one of the seven low bits low; a counter of 0 reads every
    // row at once (the OS uses that as a "any key down?" probe), and any
    // other value selects nothing and reads the idle 0x7C.
    uint8_t data = 0x7C;
    if (m_kbCounter == 0) {
        uint8_t all = 0xFF;
        for (int line = 0; line < 7; line++) all &= m_keyRow[line];
        data = all;
    } else {
        for (int line = 0; line < 7; line++) {
            if (m_kbCounter == uint16_t(0x7F & ~(1 << line))) {
                data = m_keyRow[line];
                break;
            }
        }
    }
    return data & 0x7C;
}

void Emulator::pressOn(bool down) {
    m_onKey = down;
    // A press on ON pulls the CPU's reset line, which is how the
    // machine comes back from the standby the OFF path put it in
    // (MAME: INPUT_CHANGED_MEMBER(psion_state::psion_on)).
    if (down && m_standby) {
        if (traceOn())
            std::fprintf(stderr, "[org1] ON pressed — waking from standby\n");
        m_standby = false;
        cpu.resumeFromStandby();
    }
}

// ── HD6303Bus dispatch ────────────────────────────────────────────────

uint8_t Emulator::readByte(uint16_t addr) {
    // $0000-$00FF is the CPU's own register file + internal RAM; the
    // core answers those before the bus sees them.
    if (addr < 0x0100) return 0x00;

    if (addr >= 0x2000 && addr < 0x2800) {
        // HD44780, mirrored every two bytes: RS = A0.
        return (addr & 1) ? lcd.readData() : lcd.readStatus();
    }
    if (addr == 0x2800) {           // reset_kb_counter_r
        m_kbCounter = 0;
        return 0;
    }
    if (addr == 0x2E00) {           // switchoff_r
        if (traceOn() && !m_standby)
            std::fprintf(stderr, "[org1] switch-off (pc=%04x, t=%.2fs)\n",
                         cpu.pc, double(passedCycles) / E_CLOCK_HZ);
        if (!m_standby && !m_autoOnArmed) {
            // First switch-off of the session: arm the synthetic ON
            // press a fraction of a second later.
            m_autoOnArmed    = true;
            m_autoOnAssertAt = passedCycles + E_CLOCK_HZ / 10;   // +100 ms
        }
        m_standby = true;
        return 0;
    }
    if (addr == 0x3000) {           // inc_kb_counter_r
        m_kbCounter++;
        return 0;
    }
    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE) {
        return ram[addr - RAM_BASE];
    }
    if (addr >= ROM_BASE) {
        return rom[addr - ROM_BASE];
    }
    return 0x00;                    // unmapped, as in MAME
}

void Emulator::writeByte(uint16_t addr, uint8_t v) {
    if (addr < 0x0100) return;      // CPU-internal range

    if (addr >= 0x2000 && addr < 0x2800) {
        if (std::getenv("ORG1_LCD_TRACE")) {
            static int n = 0;
            if (n < 200) {
                std::fprintf(stderr, "[lcd-w] %s $%02x (pc=%04x t=%.2fs)\n",
                             (addr & 1) ? "data" : "cmd ", v, cpu.pc,
                             double(passedCycles) / E_CLOCK_HZ);
                n++;
            }
        }
        if (addr & 1) lcd.writeData(v);
        else          lcd.writeCommand(v);
        return;
    }
    if (addr >= RAM_BASE && addr < RAM_BASE + RAM_SIZE) {
        ram[addr - RAM_BASE] = v;
        return;
    }
    // ROM and the read-only strobes above ignore writes.
}

// ── LCD readout ───────────────────────────────────────────────────────

void Emulator::readLCDIntoBuffer(uint8_t **lines, bool is32BitOutput) const {
    if (std::getenv("ORG1_LCD_DUMP")) {
        static int dumped = 0;
        if (dumped++ < 5) {
            const uint8_t *d = lcd.debugDdram();
            // The 16 visible cells are DDRAM $00-$07 then $40-$47.
            std::fprintf(stderr, "[lcd] |");
            for (int i = 0; i < 16; i++) {
                uint8_t b = d[(i < 8 ? i : 0x40 + (i - 8))];
                std::fprintf(stderr, "%c", (b >= 0x20 && b < 0x7F) ? char(b) : '.');
            }
            std::fprintf(stderr, "|  display_on=%d\n", (int)lcd.debugDisplayOn());
        }
    }
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
                // Same olive-on-grey LCD tint as the Organiser II.
                line[x] = bit ? 0xFF202018u : 0xFF8C9670u;
            } else {
                lines[y][x] = bit ? 0x00 : 0xC8;
            }
        }
    }
}

// ── Keyboard ──────────────────────────────────────────────────────────

void Emulator::setKeyboardKey(EpocKey key, bool value) {
    // INPUT_PORTS_START(psion1): seven rows of five keys, bits $04,
    // $08, $10, $20, $40, all active low. The Organiser I's keypad is
    // alphabetical rather than QWERTY, which is why the rows look
    // nothing like the Organiser II's.
    //
    //   K1  →      ←      ↓[FIND] ↑[SAVE] MODE
    //   K2  A      G      M       S       SHIFT
    //   K3  B      H      N       T       Y
    //   K4  C      I      O       U       Z
    //   K5  E      K      Q       W       DELETE
    //   K6  F      L      R       X       EXECUTE
    //   K7  D      J      P       V       SPACE
    int row = -1;
    uint8_t bit = 0;
    switch (static_cast<int>(key)) {
    // ON / CLEAR wakes the machine; it is not on the scan matrix.
    case EStdKeyEscape:     m_userTouchedOn = true; pressOn(value); return;

    case EStdKeyRightArrow: row = 0; bit = 0x04; break;
    case EStdKeyLeftArrow:  row = 0; bit = 0x08; break;
    case EStdKeyDownArrow:  row = 0; bit = 0x10; break;   // FIND
    case EStdKeyUpArrow:    row = 0; bit = 0x20; break;   // SAVE
    case EStdKeyMenu:
    case EStdKeyTab:        row = 0; bit = 0x40; break;   // MODE / HOME

    case EStdKeyLeftShift:
    case EStdKeyRightShift: row = 1; bit = 0x40; break;
    case EStdKeyDelete:
    case EStdKeyBackspace:  row = 4; bit = 0x40; break;
    case EStdKeyEnter:      row = 5; bit = 0x40; break;   // EXECUTE
    case EStdKeySpace:      row = 6; bit = 0x40; break;

    // Letters. EPOC uses the ASCII codes for A-Z.
    case (EpocKey)65: row = 1; bit = 0x04; break;  // A
    case (EpocKey)71: row = 1; bit = 0x08; break;  // G
    case (EpocKey)77: row = 1; bit = 0x10; break;  // M
    case (EpocKey)83: row = 1; bit = 0x20; break;  // S

    case (EpocKey)66: row = 2; bit = 0x04; break;  // B
    case (EpocKey)72: row = 2; bit = 0x08; break;  // H
    case (EpocKey)78: row = 2; bit = 0x10; break;  // N
    case (EpocKey)84: row = 2; bit = 0x20; break;  // T
    case (EpocKey)89: row = 2; bit = 0x40; break;  // Y

    case (EpocKey)67: row = 3; bit = 0x04; break;  // C
    case (EpocKey)73: row = 3; bit = 0x08; break;  // I
    case (EpocKey)79: row = 3; bit = 0x10; break;  // O
    case (EpocKey)85: row = 3; bit = 0x20; break;  // U
    case (EpocKey)90: row = 3; bit = 0x40; break;  // Z

    case (EpocKey)69: row = 4; bit = 0x04; break;  // E
    case (EpocKey)75: row = 4; bit = 0x08; break;  // K
    case (EpocKey)81: row = 4; bit = 0x10; break;  // Q
    case (EpocKey)87: row = 4; bit = 0x20; break;  // W

    case (EpocKey)70: row = 5; bit = 0x04; break;  // F
    case (EpocKey)76: row = 5; bit = 0x08; break;  // L
    case (EpocKey)82: row = 5; bit = 0x10; break;  // R
    case (EpocKey)88: row = 5; bit = 0x20; break;  // X

    case (EpocKey)68: row = 6; bit = 0x04; break;  // D
    case (EpocKey)74: row = 6; bit = 0x08; break;  // J
    case (EpocKey)80: row = 6; bit = 0x10; break;  // P
    case (EpocKey)86: row = 6; bit = 0x20; break;  // V
    default: return;
    }
    if (row < 0) return;
    if (value) m_keyRow[row] &= ~bit;   // active low: press clears
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

} // namespace Organiser1
