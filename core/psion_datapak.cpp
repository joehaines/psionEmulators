// license:BSD-3-Clause
// copyright-holders:Sandro Ronco (MAME psion_pack_device)
//                  + adaptation by the Psion emulator project, 2026.
//
// Psion Organiser II Datapak / Rampak — scaffold implementation.
// See psion_datapak.h for the porting source and what's stubbed.

#include "psion_datapak.h"

#include <algorithm>
#include <cstring>

namespace {
// .opk file format per MAME's psion_pack_device::call_load:
//   bytes 0-2: "OPK" magic signature
//   bytes 3-5: reserved / padding
//   byte    6: pack ID byte. Bit assignments:
//                bit 1 (DP_ID_EPROM = 0x02) — set = Datapak (EPROM),
//                                              clear = Rampak (SRAM)
//                bit 2 (DP_ID_PAGED = 0x04) — paged memory
//                bit 3 (DP_ID_WRITE = 0x08) — write-capable
//   byte    7: size code
//
// Anything without the OPK magic is treated as a raw image and
// defaulted to a writable Rampak so user-saved data persists.
EmuBase::PackKind sniffKind(const uint8_t *bytes, size_t size) {
    if (!bytes || size < 4) return EmuBase::PackKind::None;
    if (size >= 8 && bytes[0] == 'O' && bytes[1] == 'P' && bytes[2] == 'K') {
        // Pack ID byte at offset 6: bit 1 set ⇒ EPROM (Datapak).
        return (bytes[6] & 0x02)
            ? EmuBase::PackKind::Datapak
            : EmuBase::PackKind::Rampak;
    }
    // Raw image (no OPK header) — assume writable Rampak.
    return EmuBase::PackKind::Rampak;
}
}  // namespace

bool PsionDatapak::attach(const uint8_t *bytes, size_t size) {
    if (!bytes || size == 0 || size > 0x100000) return false;
    m_data.assign(bytes, bytes + size);
    m_kind = sniffKind(bytes, size);
    m_addrCounter = 0;
    m_lastControl = 0;
    return true;
}

void PsionDatapak::detach() {
    m_data.clear();
    m_kind = PackKind::None;
    m_addrCounter = 0;
    m_lastControl = 0;
}

void PsionDatapak::reset() {
    m_addrCounter = 0;
    m_lastControl = 0;
}

void PsionDatapak::writeControl(uint8_t v) {
    // Stub: latch the control byte so a future port has the previous
    // edge available when CLK / RES toggle. The real protocol clocks
    // the address counter on the rising edge of CLK and resets it on
    // RES; the pack returns the byte at addrCounter on the next read.
    m_lastControl = v;
}

void PsionDatapak::writeData(uint8_t v) {
    // Rampak: writable. Stub increments the counter so a sequential
    // write fills the image; full protocol uses CLK edges to advance.
    if (m_kind == PackKind::Rampak && m_addrCounter < m_data.size()) {
        m_data[m_addrCounter] = v;
    }
    m_addrCounter = (m_addrCounter + 1) & 0xFFFF;
}

uint8_t PsionDatapak::readData() const {
    if (m_data.empty()) return 0xFF;
    uint32_t addr = m_addrCounter & 0xFFFF;
    if (addr >= m_data.size()) return 0xFF;
    return m_data[addr];
}
