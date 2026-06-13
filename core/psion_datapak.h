// license:BSD-3-Clause
// copyright-holders:Sandro Ronco (MAME psion_pack_device)
//                  + adaptation by the Psion emulator project, 2026.
//
// Psion Organiser II Datapak / Rampak emulation — scaffold.
//
// Datapak: parallel EPROM module, read-only after burn-in.
// Rampak:  battery-backed SRAM module, same connector + protocol but
//          writable.
// File format: `.opk` files used by every existing Organiser II
// emulator. The header byte distinguishes the two kinds (see
// detectKind() comment in the .cpp). Sizes range from 8 KiB to 64 KiB
// in the original packs, with extension cards going larger.
//
// This is a stub: attach() / detach() / image readback are wired so
// the frontend dialog can round-trip an .opk image, but the actual
// bit-banged read protocol (CLK / RES / address counter / data byte)
// returns 0xFF until the HD6303 CPU port is far enough to drive it.
//
// Porting source: MAME src/mame/psion/psion_pack.{cpp,h} (BSD-3-Clause).

#pragma once

#include "emubase.h"   // EmuBase::PackKind
#include <cstddef>
#include <cstdint>
#include <vector>

class PsionDatapak {
public:
    using PackKind = EmuBase::PackKind;

    PsionDatapak() = default;

    // Image management -- frontend uploads a buffer; the host figures
    // out whether it's a Datapak or Rampak from the .opk header.
    bool attach(const uint8_t *bytes, size_t size);
    void detach();
    bool   isInserted() const { return !m_data.empty(); }
    size_t imageSize()  const { return m_data.size(); }
    const uint8_t *imageData() const { return m_data.data(); }
    PackKind kind() const { return m_kind; }

    // Bit-banged bus interface — driven by the Organiser II driver
    // from inside the $0100-$03FF I/O block. Stub returns 0xFF to mean
    // "no pack" until the address-counter + CLK protocol is ported.
    void    writeControl(uint8_t v);
    void    writeData(uint8_t v);
    uint8_t readData() const;

    void reset();

private:
    std::vector<uint8_t> m_data;
    PackKind             m_kind = PackKind::None;
    uint32_t             m_addrCounter = 0;
    uint8_t              m_lastControl = 0;
};
