// license:BSD-3-Clause
// copyright-holders:Nigel Barnes (MAME psion3 driver, 1991/1994)
//                  + adaptation by the Psion emulator project, 2026.
//
// Psion Series 3 / HC / MC driver — wires the V30 CPU + ASIC1 + ASIC2
// into an EmuBase subclass. Memory map and chip wiring are derived
// directly from reference/mame-psion/psion/psion3.cpp.
//
// This file backs two device families that share the SIBO1 chip set:
//
//   Series 3 (1991, handheld, 240x80 LCD, 256 KiB RAM, 512 KiB ROM):
//
//     V30 program space:
//       0x00000-0x3FFFF  RAM (256 KiB)
//       0x40000-0x7FFFF  unmapped
//       0x80000-0xFFFFF  ROM (512 KiB image, mirrored throughout the
//                        upper half; reset vector FFFF:0000 ⇒ FFFF0)
//
//   MC400 (1989, clamshell laptop, 640x400 mono LCD, 256 KiB RAM, 256 KiB ROM):
//
//     V30 program space:
//       0x00000-0x3FFFF  RAM (256 KiB main)
//       0x40000-0xB7FFF  unmapped
//       0xB8000-0xBFFFF  VRAM (32 KiB, dual-plate framebuffer; ASIC1
//                        in laptopMode reads pixels here via the
//                        host-installed memReader callback)
//       0xC0000-0xFFFFF  ROM (256 KiB, no mirror; reset vector FFFF:0000
//                        lands inside the ROM image, the entry point
//                        EA 00 00 00 C0 jumps to C000:0000 = ROM offset 0)
//
//   Common I/O (both):
//     0x00-0x1F        ASIC1 register window (16-bit access)
//     0x80-0x8F        ASIC2 register window (8-bit even bytes only)
//     0x200-0x2FF      DTMF (PCD3311) — write-only, stubbed here
//
//   Common chip wiring:
//     ASIC1.intCb  -> CPU IRQ
//     ASIC1.nmiCb  -> CPU NMI
//     ASIC1.frcOvl -> ASIC2.frcOvlIn
//     ASIC2.intCb  -> ASIC1.eint3
//     ASIC2.nmiCb  -> ASIC1.enmi

#pragma once

#include "emubase.h"
#include "psion_asic1.h"
#include "psion_asic2.h"
#include "psion_asic3.h"
#include "psion_ssd.h"
#include "sibo_audio.h"
#include "v30.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace Series3 {

enum class Model {
    Series3,    // 1991 handheld, 240x80 LCD upscaled to 480x160
    MC400,      // 1989 clamshell laptop, 640x400 mono LCD (dual-plate)
    MC200,      // 1989 clamshell laptop, 640x200 mono LCD (single-plate)
    HC120,      // 1991 industrial handheld, 160x80 mono LCD
};

struct KeyMatrixEntry {
    int8_t  col;   // -1 = unmapped
    uint8_t bit;
};

// Key-table function: returns the (col, bit) slot for a given EpocKey,
// or {-1, 0} when the key isn't on this device's matrix. Esc is handled
// out-of-band (routed to ASIC2 OnClr); table entries for it are ignored.
using KeyMatrixFn = KeyMatrixEntry(*)(EpocKey);

KeyMatrixEntry series3KeyMatrix(EpocKey key);
KeyMatrixEntry mc400KeyMatrix(EpocKey key);
KeyMatrixEntry hc120KeyMatrix(EpocKey key);

struct Config {
    Model       model         = Model::Series3;
    const char *displayName   = "Psion Series 3";
    int32_t     busClockHz    = 3'840'000; // 7.68 MHz crystal / 2

    // CPU variant. Series 3 / HC use NEC V30. MC200 / MC400 / MC Word
    // were assembled for an Intel 80C86A — per MAME `mc400.cpp` the
    // CPU is `I8086(... 15.36_MHz_XTAL / 2)`. The two share an
    // instruction encoding but differ in cycle counts (V30 ~2x faster)
    // and the meaning of opcode 0x0F (POP CS on i8086, extended-prefix
    // on V30). Switching this to V30Variant::I8086 makes the V30 core
    // emit i8086-throughput cycle counts and dispatch 0x0F as POP CS
    // — matching MAME's MC400 boot timing.
    // CPU variant. Series 3 / HC use NEC V30. MC200 / MC400 / MC Word
    // were assembled for an Intel 80C86A. See V30Variant::I8086.
    V30Variant  cpuVariant    = V30Variant::V30;

    // Cold-boot RAM fill byte. Real DRAM powers up with undefined
    // contents — many chips read 0xFF, some read random. The MC400
    // V1.26F boot ROM's RAM POST is gated on `[0x416] + [0x418] != 0`;
    // initialised-to-zero RAM trips the skip, the POST never writes
    // [0x414] (top-of-RAM segment), and the kernel's later
    // `SS = [0x414] - 0x40` ends up as 0xFFC0 (a stack inside ROM).
    // Defaulting to 0xFF keeps the skip check non-zero on cold boot
    // so the POST runs and [0x414] gets set correctly. Series 3 boots
    // either way.
    uint8_t     ramFillByte   = 0x00;

    // LCD geometry that gets reported to the harness/UI.
    int         lcdWidth      = 480;       // Series 3: 480 (2x upscaled from 240)
    int         lcdHeight     = 160;       // Series 3: 160 (2x upscaled from 80)

    // Native panel dimensions handed to ASIC1::readLCD. For Series 3 the
    // physical panel is 240x80 and we upscale to lcdWidth x lcdHeight in
    // readLCDIntoBuffer. For MC400 the panel is the framebuffer (no scale).
    int         fbWidth       = 240;
    int         fbHeight      = 80;
    int         fbPlates      = 1;         // 1 single-plate, 2 dual-plate (HC/MC)

    // Memory map.
    size_t      ramSize       = 0x40000;   // 256 KiB
    size_t      romSize       = 0x80000;   // 512 KiB
    uint32_t    romBase       = 0x80000;   // upper half of 1 MiB
    bool        mirrorRom     = true;      // Series 3 mirrors a 512K image; MC400 doesn't

    // Extra ROM window above the chips' own range. Some SIBO1 machines
    // decode their ROM into more of the address space than the chips
    // fill: the HC120's 256 KiB pair answers at 0xA0000-0xDFFFF, and the
    // top 128 KiB window (0xE0000-0xFFFFF) reads the upper chip a second
    // time — it has to answer something, because the CPU fetches its
    // reset vector from 0xFFFF0. Setting this to N makes the last N bytes
    // of the image answer for the top N bytes of the address space; 0
    // (the default) leaves the plain romBase decode alone.
    size_t      romAliasSize  = 0;      // HC120: 0x20000

    // RAM-decoder mask. Series 3 decodes only the bottom 256 KiB
    // (0x00000-0x3FFFF, mask = ramSize-1 with no upper bound) — addresses
    // 0x40000-0x7FFFF are unmapped. The MC400 wires the same 256 KiB chip
    // through a coarser chip-select that lights up for the bottom half of
    // the 1 MiB address space, so RAM mirrors through 0x00000-0x7FFFF and
    // the kernel routinely far-jumps to RAM-shadow segments like 0x46C0.
    // Set ramDecodeMask to 0x7FFFF to enable that mirror; leave at 0
    // (the default) to fall back to "addresses past ramSize are unmapped".
    uint32_t    ramDecodeMask = 0;

    // Optional VRAM window (laptop mode). vramSize == 0 disables.
    uint32_t    vramBase      = 0;         // MC400: 0xB8000
    size_t      vramSize      = 0;         // MC400: 0x8000 (32 KiB)

    // ASIC1 LCD identification + framebuffer base. The MAME default for
    // Series 3 is a non-laptop 240x80 panel reported as id 0; MC400 sets
    // laptopMode true and reports the 640x400 LCD via id 0 too.
    bool        laptopMode    = false;
    uint8_t     lcdId         = 0;

    // Keyboard matrix translator. Defaults to the Series 3 layout.
    KeyMatrixFn keyMatrix     = &series3KeyMatrix;

    // Whether Esc is the machine's ON key. On the Series 3 and the MC the
    // two are the same key, wired to ASIC2's OnClr input rather than to
    // the keyboard matrix so that it can wake the machine from standby —
    // so setKeyboardKey routes Esc there and never to the matrix. The
    // HC120 has both: ESC on the keypad and a separate ON/OFF button on
    // the case, so its Esc belongs on the matrix like any other key — and
    // EStdKeyOff drives OnClr in its place, which is what the frontend's
    // ON button sends.
    bool        escIsOnKey    = true;

    // SSD pack slot count (Series 3 / MC400 both ship 2: Pack A on
    // ASIC2 channel 1, Pack B on channel 2).
    int         ssdSlots      = 2;
};

class Emulator : public EmuBase, public V30Bus {
public:
    Emulator() : Emulator(Config{}) {}
    explicit Emulator(const Config &cfg);
    ~Emulator() override = default;

    // ── EmuBase overrides ───────────────────────────────────────────────
    V30 *getV30Cpu() override { return &cpu; }
    const V30 *getV30Cpu() const override { return &cpu; }

    uint8_t *getROMBuffer() override { return rom.data(); }
    size_t   getROMSize() override   { return rom.size(); }
    void     loadROM(uint8_t *buffer, size_t size) override;
    void     executeUntil(int64_t cycles) override;

    uint8_t *getRamBuffer() override       { return ram.data(); }
    size_t   getRamSize()   const override { return ram.size(); }
    void     loadRamSnapshot(const uint8_t *bytes, size_t size) override {
        std::memcpy(ram.data(), bytes, std::min(size, ram.size()));
    }

    int32_t     getClockSpeed() const override { return m_cfg.busClockHz; }
    const char *getDeviceName() const override { return m_cfg.displayName; }

    int getDigitiserWidth()  const override { return m_cfg.lcdWidth; }
    int getDigitiserHeight() const override { return m_cfg.lcdHeight; }
    int getLCDOffsetX()      const override { return 0; }
    int getLCDOffsetY()      const override { return 0; }
    int getLCDWidth()        const override { return m_cfg.lcdWidth; }
    int getLCDHeight()       const override { return m_cfg.lcdHeight; }

    void readLCDIntoBuffer(uint8_t **lines, bool is32BitOutput) const override;
    void setKeyboardKey(EpocKey key, bool value) override;
    void updateTouchInput(int32_t x, int32_t y, bool down) override;

    // ── Audio (piezo buzzer only — SIBO1 has no PCM codec) ─────────────
    // SIBO audio runs on its own SiboAudio model (core/sibo_audio.{h,cpp}),
    // deliberately separate from the AudioCodecModel that backs Windermere
    // and CL-PS711x. Each chip family owns its own DAC ring + pump so a
    // change here can never regress EPOC32 recording, and vice versa.
    bool hasAudio() const override { return true; }
    // SIBO1 (Series 3 / MC400 / Pocket Book) has a piezo buzzer only —
    // no codec, no microphone.
    bool hasMicrophone() const override { return false; }
    int  getAudioSampleRate() const override {
        return SiboAudio::kAudioSampleRate;
    }
    size_t readAudioOutput(int16_t *dst, size_t maxSamples) override {
        return audio.readAudioOutput(dst, maxSamples);
    }
    void writeAudioInput(const int16_t *src, size_t count) override {
        // No PCM codec on SIBO1 — silently drop host mic input.
        if (!audio.hostMicEnabled()) return;
        audio.enqueueMicSamples(src, count);
    }
    void setHostAudioEnabled(bool speaker, bool mic) override {
        audio.setHostEnabled(speaker, mic);
    }

    // ── SSD pack hooks (slots wired to ASIC2 channels 1/2) ─────────────
    int  getSsdSlotCount() const override { return int(m_ssd.size()); }
    bool attachSSD(int slot, const uint8_t *bytes, size_t size,
                   int ssdType = 0) override;
    void detachSSD(int slot) override;
    bool   isSSDInserted(int slot) const override;
    size_t getSSDImageSize(int slot) const override;
    const uint8_t *getSSDImageData(int slot) const override;

    // ── V30Bus implementation ───────────────────────────────────────────
    uint8_t  readMemByte(uint32_t linear) override;
    uint16_t readMemWord(uint32_t linear) override;
    void     writeMemByte(uint32_t linear, uint8_t v) override;
    void     writeMemWord(uint32_t linear, uint16_t v) override;
    uint8_t  readIoByte(uint16_t port) override;
    uint16_t readIoWord(uint16_t port) override;
    void     writeIoByte(uint16_t port, uint8_t v) override;
    void     writeIoWord(uint16_t port, uint16_t v) override;
    uint8_t  ackInterrupt() override;

private:
    Config m_cfg;

    V30        cpu;
    PsionAsic1 asic1;
    PsionAsic2 asic2;

    // MC / HC laptops include a Maxim MAX616 PSU ASIC (PS34) on SIBO
    // channel 0 of ASIC2. The boot ROM polls A3Status for the PowerFail
    // bit before enabling the LCD, so the laptop config wires this up;
    // Series 3 leaves channel 0 empty (handheld doesn't have PS34).
    PsionAsic3 asic3{PsionAsic3::Variant::Asic5};

    std::vector<uint8_t> ram;
    std::vector<uint8_t> rom;
    std::vector<uint8_t> vram;       // empty unless cfg.vramSize > 0

    // SSD pack slots, sized from cfg.ssdSlots in the constructor.
    std::vector<PsionSSD> m_ssd;

    // Audio: piezo buzzer pumped at 64 Hz from executeUntil. m_buzzerLevel
    // mirrors the last edge ASIC2 reported on its buz_cb; m_nextAudioTickAt
    // is the next host-cycle deadline at which to push another square-wave
    // burst into the SiboAudio ring.
    SiboAudio audio;
    bool      m_buzzerLevel     = false;
    int64_t   m_nextAudioTickAt = 0;
    int64_t m_doorNmiClearAt = -1;
    // Cold-attach door NMI is deferred until after the kernel has had
    // time to install its NMI vector at IVT[2]. The MC400 boot ROM
    // overwrites IVT[2] around cycle 7.5M (~2 s into boot); we arm the
    // post-boot pulse at 8 M cycles to be safely after that. -1 means
    // no pulse pending.
    int64_t m_pendingColdDoorAt = -1;

    // Auto-wake pulse — see executeUntil for the rationale. The Series 3
    // boots in a low-power "off" state and only wakes on an A1OnKey edge
    // (the ESC/On key). On real hardware the user taps that key to power
    // the device on; emulating without a manual press leaves the kernel
    // sitting in its standby loop forever. We synthesise a one-shot
    // OnKey press shortly after the kernel finishes early POST so the
    // device boots straight to the app row.
    int64_t m_autoWakeAssertAt = -1;
    int64_t m_autoWakeReleaseAt = -1;
    bool    m_autoWakeFired = false;

public:
    void dumpKernelState();
private:

    // Keyboard: ten scanned columns × eight rows, strobed via ASIC2's
    // keyboard-column callback. The MC400 only uses 8 of the 10 columns;
    // the unused entries stay zero.
    uint8_t m_key_row[10] = { 0 };

    bool initialised = false;

    // Unified physical-bus read used by both V30 fetches and the ASIC1
    // memReader callback that feeds LCD scanout. Returns 0xFF for
    // unmapped windows (ROM-style open-bus pull-up).
    uint8_t busRead(uint32_t linear) const;

    // Translate an x86 I/O port in the 0x80-0x8F window to an ASIC2
    // register index. Laptop mode (MC400) uses MAME's word-stride
    // mapping (port 0x8E → reg 7 = A2ChannelControl); handheld Series 3
    // uses the byte-stride alias preserved for golden compatibility.
    uint32_t asic2Reg(uint16_t port) const;

    void wireChips();
};

} // namespace Series3
