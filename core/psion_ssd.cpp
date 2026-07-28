// license:BSD-3-Clause
// copyright-holders:Nigel Barnes
//
// PsionSSD — see psion_ssd.h for the design rationale (collapsed
// psion_ssd_device + psion_asic5_device PACK_MODE branches into one).
//
// Two upstream MAME files form the reference:
//   reference/mame-psion/machine/psion_ssd.cpp   (info byte + door)
//   /tmp/psion_asic5.cpp from upstream MAME      (PACK_MODE protocol)
//
// Key behaviour ported from MAME ASIC5:
//   * Port B has four modes (counter / latch / baud / test) selected
//     via control 0x82 + DATA byte. SSD uses counter mode for
//     auto-incrementing sequential reads and latch mode for setting
//     the address LSB by hand.
//   * In counter mode, a Port A read or write whose control byte has
//     bit 4 set increments port_b_counter and the result is "sent" out
//     Port B — for an SSD that means the address LSB auto-advances
//     after every byte of a multi-byte transfer.
//   * Port D / Port C accept the address mid-byte and high-byte; the
//     first DATA_FRAME after CONTROL 0x93 hits D, subsequent frames
//     hit C. A Port D write in counter mode resets port_b_counter
//     to 0 so a new address sequence starts at boundary zero.

#include "psion_ssd.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace {
// PSION_SSD_TRACE=1 logs every SIBO frame to stderr. Used during
// driver bring-up to understand which sub-controls EPOC16 issues
// when probing a freshly-attached pack; off by default.
inline bool ssdTrace() {
    static const char *e = std::getenv("PSION_SSD_TRACE");
    return e && e[0] && e[0] != '0';
}
}

namespace {
// Extract bits [lo, lo+n) from v.
inline uint8_t bits(uint8_t v, int lo, int n) {
    return uint8_t((v >> lo) & ((1u << n) - 1u));
}
} // namespace

bool PsionSSD::attach(const uint8_t *bytes, size_t size, Type type) {
    if (size < 0x10000 || size > 0x800000) return false;
    if ((size & (size - 1)) != 0)          return false;  // not power of two
    if (!bytes)                            return false;

    m_data.assign(bytes, bytes + size);

    // Explicit type wins (it models the pack's hardware straps, which
    // exist independently of the contents); Auto falls back to sniffing
    // the FEFS magic, which is right for Flash dumps but misreads a
    // FEFS-formatted RAM pack as Flash.
    Type effective = type;
    if (effective == Type::Auto) {
        const bool magic = m_data.size() >= 2 &&
            uint16_t(m_data[0] | (m_data[1] << 8)) == 0xF1A5;
        effective = magic ? Type::Flash : Type::Ram;
    }

    computeInfo(size, effective);

    m_isFlash        = (effective == Type::Flash || effective == Type::Protected);
    m_writeProtected = (effective == Type::Protected);
    m_flashState     = FlashState::ReadArray;
    m_eraseArmed     = false;

    // Reset the per-frame latches so the next CPU access starts from a
    // known state (matches MAME's device_reset on call_load).
    m_siboControl  = 0;
    m_portLatch    = 0;
    m_portDcSelect = false;
    m_portBLatch   = 0;
    m_portBMode    = 0;
    m_portBCounter = 0;

    // Door-NMI only once the guest's SSD driver has spoken to this
    // slot. Before the boot-time slot scan the NMI vector isn't
    // populated — pulsing it then wedges a cold boot (the app-library
    // try-flow attaches packs well inside the first second when the
    // bundle comes from the service-worker cache). A pack attached
    // before the first scan is simply found BY that scan, so skipping
    // the pulse loses nothing.
    if (m_doorCb && m_guestAccessed) m_doorCb(true);
    return true;
}

void PsionSSD::detach() {
    m_data.clear();
    m_infoByte = 0;
    m_memWidth = 0;
    m_siboControl = 0;
    m_portLatch = 0;
    m_portDcSelect = false;
    m_portBLatch = 0;
    m_portBMode = 0;
    m_portBCounter = 0;
    m_isFlash = false;
    m_writeProtected = false;
    m_flashState = FlashState::ReadArray;
    m_eraseArmed = false;
    if (m_doorCb && m_guestAccessed) m_doorCb(true);
}

void PsionSSD::computeInfo(size_t size, Type type) {
    m_infoByte = 0;
    const bool isFlash = (type == Type::Flash);
    // Type 1 Flash sets D7-D5 = 001 (0x20) — see the info-byte table in
    // reference/mame-psion/machine/psion_ssd.cpp. MAME's set_info_byte
    // historically used 0xE0 (D7-D5 = 111) here, which is the "Hardware
    // write-protected SSD" type used for MC200/MC400 system disks. The
    // EPOC16 SSD driver on real Series 3a/3c/Siena treats 0xE3 info-byte
    // packs as system-disk boot images and does NOT mount them as user
    // drives — the System screen never gets a new drive letter, no
    // FEFS read happens, the pack is invisible. Using 0x20 (Type 1
    // Flash) gives info bytes 0x23/0x2B/0x2C/0x3C/0x3D/0x3E/0x3F for
    // 128K..8M, which match the "Psion Solid State Disk N Flash" rows
    // in the same table and which EPOC16 mounts as regular user packs —
    // read/write, through the flash command interface in writeFrame.
    if (isFlash) m_infoByte |= 0x20;
    // Hardware write-protected type (D7-D5 = 111), as used by MAME for
    // every Flash image and by real MC200/400 system disks. EPOC16
    // mounts these read-only — the Directory browser labels the drive
    // "Protected" rather than "Flash".
    if (type == Type::Protected) m_infoByte |= 0xE0;

    switch (size) {
    case 0x010000: m_infoByte |= 0x09; m_memWidth = 15; break; // 64K (2 x 32K)
    case 0x020000: m_infoByte |= 0x03; m_memWidth = 17; break; // 128K (1 x 128K)
    case 0x040000: m_infoByte |= 0x0B; m_memWidth = 17; break; // 256K (2 x 128K)
    case 0x080000: m_infoByte |= 0x0C; m_memWidth = 18; break; // 512K (2 x 256K)
    case 0x100000: m_infoByte |= 0x1C; m_memWidth = 18; break; // 1M  (4 x 256K)
    case 0x200000: m_infoByte |= 0x1D; m_memWidth = 19; break; // 2M  (4 x 512K)
    case 0x400000: m_infoByte |= 0x1E; m_memWidth = 20; break; // 4M  (4 x 1M)
    case 0x800000: m_infoByte |= 0x1F; m_memWidth = 21; break; // 8M  (4 x 2M)
    default:       m_infoByte = 0;     m_memWidth = 0;  break;
    }
}

// ── Intel 28F0xx-class command interface (see psion_ssd.h) ───────────
//
// Command bytes arrive as Port A writes; the data byte of a program
// arrives the same way, which is why the chip needs a state machine to
// tell the two apart.
void PsionSSD::flashCommand(uint8_t cmd) {
    // Erase Setup (0x20) arms the device; the very next command must be
    // the Erase Confirm (0x20) or the sequence aborts, exactly as on the
    // real part — a two-cycle interlock so a stray write can't wipe a
    // device.
    if (m_eraseArmed) {
        m_eraseArmed = false;
        if (cmd == 0x20) {
            flashEraseDevice(latchedAddr());
            m_flashState = FlashState::ReadArray;
            return;
        }
        // Fall through: treat the byte as a fresh command.
    }

    switch (cmd) {
    case 0x40:
    case 0x10:  // Program Setup — next Port A write carries the data.
        m_flashState = FlashState::ProgramSetup;
        break;
    case 0x20:  // Erase Setup — needs the confirm cycle to take effect.
        m_eraseArmed = true;
        m_flashState = FlashState::ReadArray;
        break;
    case 0x90:  // Read Intelligent Identifier
        m_flashState = FlashState::ReadId;
        break;
    case 0xC0:  // Program Verify  — both simply return to array reads,
    case 0xA0:  // Erase Verify      which is what the verify read wants.
    case 0x00:  // Read Memory
    case 0xFF:  // Read Memory / Reset
    default:
        m_flashState = FlashState::ReadArray;
        break;
    }
}

void PsionSSD::flashProgram(uint32_t addr, uint8_t data) {
    // Vpp is strapped off on a hardware write-protected pack, so the
    // program cycle completes with the array unchanged and the driver's
    // verify read fails — the same outcome as the real device.
    if (m_writeProtected || addr >= m_data.size()) return;
    // Programming only clears bits; returning a cell to 1 takes an
    // erase. FEFS relies on this (deleting a file clears the entry's
    // valid bit in place), so model it rather than storing outright.
    m_data[addr] &= data;
}

void PsionSSD::flashEraseDevice(uint32_t addr) {
    if (m_writeProtected || m_memWidth <= 0 || m_data.empty()) return;
    // 28F0xx-class parts are bulk-erase: one command clears the whole
    // device. A pack is 1-4 devices of 2^memWidth bytes, selected by the
    // top bits of the latched address.
    const uint32_t deviceSize = uint32_t(1) << m_memWidth;
    uint32_t base = (addr / deviceSize) * deviceSize;
    if (base >= m_data.size()) return;
    const uint32_t len = uint32_t(std::min<size_t>(deviceSize, m_data.size() - base));
    std::fill_n(m_data.begin() + base, len, uint8_t(0xFF));
}

uint32_t PsionSSD::latchedAddr() const {
    if (m_memWidth <= 0 || m_data.empty()) return 0;
    const uint32_t mask = (uint32_t(1) << m_memWidth) - 1u;
    const uint32_t deviceSel = (m_portLatch >> 22) & 0x3u;
    uint32_t addr = (m_portLatch & mask) | (deviceSel << m_memWidth);
    if (addr >= m_data.size()) addr = uint32_t(m_data.size() - 1);
    return addr;
}

void PsionSSD::tickPortBCounter() {
    // ASIC5 counter mode: ++port_b_counter then output on Port B.
    // For SSD the Port B output IS the address LSB (m_portLatch bits
    // 0..7). The ++ happens before the output, matching MAME.
    if (bits(m_portBMode, 1, 2) == 0) { // Counter mode
        ++m_portBCounter;
        m_portLatch = (m_portLatch & 0xFFFF00u) | uint32_t(m_portBCounter);
    }
}

// Port A accesses only step the counter when the control byte asks for
// it (bit 4 = auto-increment / "multi" access). EPOC16 leans on the
// distinction: it bursts a file through Port A with control 0x90 / 0xD0
// so the address walks itself, but issues one-off accesses — flash
// commands (0x80), the byte a verify reads back (0xC0) — with bit 4
// clear so they land on the address already set up and leave it alone.
//
// Ignoring bit 4 and always stepping is what made a writable Flash pack
// mount as "Unformatted!": the driver's read-array command at offset 0
// consumed the address, so the header read that followed started at
// byte 1 and the 0xF1A5 magic came back as 0x11F1. (MAME's ASIC5 also
// steps unconditionally, but its SSD device reports every Flash pack as
// hardware write-protected, and that path never issues the command.)
void PsionSSD::tickPortAAccess() {
    if (m_siboControl & SIBO_AUTOINC) tickPortBCounter();
}

void PsionSSD::writeFrame(uint16_t frame) {
    m_guestAccessed = true;
    if (ssdTrace()) std::fprintf(stderr, "[ssd] writeFrame 0x%03X (ctrl=0x%02X addr=0x%06X%s)\n",
        frame, m_siboControl, latchedAddr(),
        m_flashState == FlashState::ProgramSetup ? " prog-data" : "");
    switch (frame & 0x300) {
    case NULL_FRAME:
        // MAME's ASIC5 NULL_FRAME calls device_reset() — clear the
        // per-frame latches but keep the loaded image intact.
        m_siboControl  = 0;
        m_portLatch    = 0;
        m_portDcSelect = false;
        m_portBLatch   = 0;
        m_portBMode    = 0;
        m_portBCounter = 0;
        break;

    case CONTROL_FRAME: {
        m_siboControl = uint8_t(frame & 0xFF);
        // SerialWrite Multi Port D and C writes (control 0x93) resets
        // the D/C alternation back to D-first.
        if (m_siboControl == 0x93) {
            m_portDcSelect = false;
        }
        break;
    }

    case DATA_FRAME: {
        const uint8_t data = uint8_t(frame & 0xFF);
        switch (m_siboControl & 0x0F) {
        case 0x00: // Port A write -> SSD memory write / flash command
            if (m_infoByte && !m_data.empty()) {
                if (!m_isFlash) {
                    // RAM pack: Port A is plain SRAM, store the byte.
                    m_data[latchedAddr()] = data;
                } else if (m_flashState == FlashState::ProgramSetup) {
                    // Second cycle of a program command: this is data.
                    flashProgram(latchedAddr(), data);
                    m_flashState = FlashState::ReadArray;
                } else {
                    // Flash pack: everything else on this bus is a
                    // command, not data. EPOC16 opens its slot scan with
                    // a read-array (0x00) at offset 0 — treating that as
                    // data is what used to zap the 0xA5 FEFS magic of an
                    // attached image.
                    flashCommand(data);
                }
            }
            // Address LSB auto-advances after the write when the
            // control byte asked for it.
            tickPortAAccess();
            break;

        case 0x01: // Port B write data — SSD address LSB in latch mode
            if (bits(m_portBMode, 1, 2) == 1) { // Latch mode
                m_portBLatch = data;
                m_portLatch  = (m_portLatch & 0xFFFF00u) | uint32_t(data);
            }
            break;

        case 0x02: // Port B mode register
            // bit 0:  0 = Memory (PACK) mode, 1 = Peripheral (UART) mode
            // bits 1-2: Port B mode (00=Counter 01=Latch 10=Baud 11=Test)
            // bit 3:  0 = Normal, 1 = Test
            m_portBMode = data;
            break;

        case 0x03: // Port D then Port C — address mid/high byte
            if (!m_portDcSelect) {
                // Port D write -> bits 8..15
                m_portLatch = (m_portLatch & 0xFF00FFu) | (uint32_t(data) << 8);
                // In counter mode, a new D write resets the counter.
                if (bits(m_portBMode, 1, 2) == 0) {
                    m_portBCounter = 0;
                    m_portLatch = (m_portLatch & 0xFFFF00u);
                }
            } else {
                // Port C write -> bits 16..23
                m_portLatch = (m_portLatch & 0x00FFFFu) | (uint32_t(data) << 16);
            }
            m_portDcSelect = true;
            break;

        default:
            // Other DATA_FRAME sub-modes (interrupt mask, control
            // register, UART, baud) are unused for SSD in PACK_MODE.
            break;
        }
        break;
    }
    }
}

uint8_t PsionSSD::readFrame() {
    m_guestAccessed = true;
    uint8_t out = readFrameInner();
    if (ssdTrace()) std::fprintf(stderr, "[ssd] readFrame ctrl=0x%02X addr=0x%06X -> 0x%02X\n",
        m_siboControl, latchedAddr(), out);
    return out;
}

uint8_t PsionSSD::readFrameInner() {
    switch (m_siboControl & 0xC0) {
    case 0x40: // SerialSelect
        switch (m_siboControl & 0x0F) {
        case 0x02: // Asic5PackId
        case 0x03: // Asic5NormalId
            // Pack-id only replies when sibo_control bit 0 matches our
            // PC6 mode pin (== 0 for PACK_MODE).
            if ((m_siboControl & 1) == PACK_MODE) {
                return m_infoByte;
            }
            return 0;
        }
        return 0;

    case 0xC0: { // SerialRead
        switch (m_siboControl & 0x0F) {
        case 0x00: { // Port A read data -> SSD memory read
            uint8_t v = 0;
            if (m_infoByte && !m_data.empty()) {
                if (m_isFlash && m_flashState == FlashState::ReadId) {
                    // Intelligent identifier: even address = maker,
                    // odd = device. Intel 28F0xx-class part, which is
                    // what a "Type 1 Flash" pack carries.
                    v = (latchedAddr() & 1) ? FLASH_DEVICE_ID : FLASH_MAKER_ID;
                } else {
                    v = m_data[latchedAddr()];
                }
            }
            // Address LSB auto-advances after the read when the
            // control byte asked for it.
            tickPortAAccess();
            return v;
        }
        case 0x01: // Port B latch read
            if (bits(m_portBMode, 1, 2) == 1) return m_portBLatch;
            return 0;
        case 0x02: // Port B counter read (advances counter)
            tickPortBCounter();
            return m_portBCounter;
        }
        return 0;
    }
    }
    return 0;
}
