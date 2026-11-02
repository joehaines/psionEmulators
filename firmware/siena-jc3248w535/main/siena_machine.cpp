// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "siena_machine.h"

#include "series3c.h"
#include "state_archive.h"

#include <cstring>
#include <new>

namespace {

// Where the ASIC9 scans the LCD from (PsionAsic9::readLCD): RAM offset
// 0x400, black plane then grey, one bit per pixel. The Siena kernel sets
// 256-pixel (32-byte) lines, so each plane is 32 x 160 bytes.
constexpr size_t kVramBase  = 0x400;
constexpr size_t kVramBytes = 2 * 32 * SienaMachine::kLcdH;
// Decode anyway this often, in case the LCD registers changed the
// picture without the framebuffer changing.
constexpr int kDecodeAtLeastEvery = 30;

// The Siena profile from core/device_registry.cpp (makeSiena). Repeated
// here rather than linking the registry, which would pull every other
// machine — ARM cores included — into the firmware.
Series3c::Config sienaConfig() {
    Series3c::Config c;
    c.model       = Series3c::Model::Siena;
    c.displayName = "Psion Siena";
    c.lcdWidth    = SienaMachine::kLcdW;
    c.lcdHeight   = SienaMachine::kLcdH;
    c.ramSize     = 0x80000;    // 512 KiB
    c.romSize     = 0x100000;   // 1 MiB
    c.busClockHz  = int32_t(SienaMachine::kClockHz);
    c.useStrictRamMirror = false;
    return c;
}

} // namespace

size_t SienaMachine::emulatorSize() { return sizeof(Series3c::Emulator); }

SienaMachine::SienaMachine(void *storage)
    : m_emu(storage ? new (storage) Series3c::Emulator(sienaConfig())
                    : new Series3c::Emulator(sienaConfig())),
      m_placed(storage != nullptr),
      m_grey(new uint8_t[kLcdW * kLcdH]),
      m_last(new uint8_t[kLcdW * kLcdH]),
      m_vram(new uint8_t[kVramBytes]) {
    for (int y = 0; y < kLcdH; ++y) m_rows[y] = m_grey + y * kLcdW;
    std::memset(m_last, 0xFF, kLcdW * kLcdH);
}

SienaMachine::~SienaMachine() {
    if (m_placed) m_emu->~EmuBase();
    else          delete m_emu;
    delete[] m_grey;
    delete[] m_last;
    delete[] m_vram;
}

void SienaMachine::loadRom(const uint8_t *rom, size_t size, int language) {
    // The locale strap is sampled during boot, so it is set before the
    // first instruction runs; loadROM is what scans the ROM's locale table.
    m_emu->loadROM(const_cast<uint8_t *>(rom), size);
    m_emu->setLanguage(language);
}

void SienaMachine::runUntilMs(int64_t ms) {
    m_emu->executeUntil(ms * (kClockHz / 1000));
}

int64_t SienaMachine::nowMs() const {
    return int64_t(m_emu->currentCycles()) / (kClockHz / 1000);
}

void SienaMachine::saveState(std::vector<uint8_t> &out) {
    StateArchive a(out);
    static_cast<Series3c::Emulator *>(m_emu)->serializeState(a);
}

bool SienaMachine::loadState(const uint8_t *data, size_t size) {
    StateArchive a(data, size);
    static_cast<Series3c::Emulator *>(m_emu)->serializeState(a);
    m_framesSinceDecode = 0;   // decode the restored screen on the next frame
    return a.ok() && a.left() == 0;
}

bool SienaMachine::attachPack(const uint8_t *pack, size_t size) {
    return static_cast<Series3c::Emulator *>(m_emu)->attachSSDInPlace(0, pack, size);
}

void SienaMachine::savePackWrites(std::vector<uint8_t> &out) {
    StateArchive a(out);
    static_cast<Series3c::Emulator *>(m_emu)->serializeSSDOverlay(0, a);
}

bool SienaMachine::loadPackWrites(const uint8_t *data, size_t size) {
    StateArchive a(data, size);
    static_cast<Series3c::Emulator *>(m_emu)->serializeSSDOverlay(0, a);
    return a.ok() && a.left() == 0;
}

bool SienaMachine::readLcd(uint8_t *levels) {
    // Decoding the panel is the dear part (a call per framebuffer byte),
    // and most frames the Siena hasn't drawn anything.
    const uint8_t *vram = m_emu->getRamBuffer() + kVramBase;
    if (m_framesSinceDecode != 0 && ++m_framesSinceDecode < kDecodeAtLeastEvery &&
        std::memcmp(vram, m_vram, kVramBytes) == 0)
        return false;
    std::memcpy(m_vram, vram, kVramBytes);
    m_framesSinceDecode = 1;

    // 8bpp output: 160 paper, 105 grey, 40 ink.
    m_emu->readLCDIntoBuffer(m_rows, false);
    for (int i = 0; i < kLcdW * kLcdH; ++i)
        levels[i] = m_grey[i] == 40 ? 1 : (m_grey[i] == 105 ? 2 : 0);
    if (std::memcmp(levels, m_last, kLcdW * kLcdH) == 0) return false;
    std::memcpy(m_last, levels, kLcdW * kLcdH);
    return true;
}
