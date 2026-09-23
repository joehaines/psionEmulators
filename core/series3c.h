// license:BSD-3-Clause
// copyright-holders:Nigel Barnes (MAME psion3a driver — Series 3a/3c/3mx/Pocket
//                  Book 2, 1993-1998)
//                  + adaptation by the Psion emulator project, 2026.
//
// Psion Series 3c driver (SIBO2: V30H + ASIC9). Mirrors core/series3.h layout
// but targets the newer 3a/3c/3mx/Siena/Workabout chipset.
//
// Memory map and chip wiring derived from
// reference/mame-psion/psion/psion3a.cpp and reference/mame-psion/machine/
// psion_asic9.cpp:
//
//   V30H program space (20-bit linear):
//     0x00000-0x1FFFFF (via ASIC9 mem_r/mem_w) — ASIC9 owns the full 1 MiB
//       window. Internally it decodes four paged 64 KiB windows at
//       0x6000/0x7000/0x8000/0x9000 (segment) that select slices of the
//       attached RAM and ROM banks, plus the bottom 0x0000-0x03FF internal
//       dispatch table. For a first-pass scaffold we don't implement the
//       ASIC9 paging — we just map the bottom 256 KiB as RAM, the upper
//       half (0x80000-0xFFFFF) as the low 512 KiB of ROM, so that the
//       reset vector at FFFF:0000 (linear FFFF0) lands inside ROM. ROM
//       paging is a TODO once the PsionAsic9 port lands.
//
//   V30H I/O space:
//     0x0000-0x00FF    ASIC9 register window (TODO: stubbed until
//                      core/psion_asic9.{h,cpp} lands on a parallel branch).
//
// When core/psion_asic9.{h,cpp} lands, the driver will compose PsionAsic9 as
// a member the same way core/series3.cpp composes PsionAsic1 + PsionAsic2,
// and route all mem/io accesses through it.

#pragma once

#include "emubase.h"
#include "psion_asic9.h"
#include "psion_condor.h"
#include "psion_honda.h"
#include "psion_ssd.h"
#include "sibo_audio.h"
#include "v30.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <vector>

namespace Series3c {

// Per-model hardware variant. Series 3a/3c/3mx and Siena all run the
// same V30H + ASIC9 stack; the differences live in clock rate, LCD
// panel geometry, and the device name that the UI + harness display.
enum class Model {
    Series3a,    // 7.68 MHz bus, 480x160 LCD, 2 MiB RAM
    Series3c,    // 7.68 MHz bus, 480x160 LCD, 2 MiB RAM (default)
    Series3mx,   // 27.648 MHz bus, 480x160 LCD, 2 MiB RAM — 3.6x faster
    Siena,       // 3.6864 MHz bus, 240x160 LCD, 512 KiB RAM + 1 MiB ROM
    Workabout,   // 7.68 MHz bus, 240x100 LCD, 1 MiB RAM, 2 MiB ROM, ASIC9
    WorkaboutMX, // 27.648 MHz bus, 240x100 LCD, 2 MiB RAM, ASIC9MX (V30MX)
};

struct Config {
    Model      model        = Model::Series3c;
    const char *displayName = "Psion Series 3c";
    int32_t    busClockHz   = 7'680'000;
    int        lcdWidth     = 480;
    int        lcdHeight    = 160;
    size_t     ramSize      = 0x200000;  // 2 MiB
    size_t     romSize      = 0x200000;  // 2 MiB
    // RAM-address translation strategy:
    //   true  — MAME's psion_asic9_device::configure_ram mirror layout.
    //           High-address probes correctly fold back into the
    //           installed devices and the kernel correctly caps the
    //           M: drive at the installed RAM size. The v5.20f / v6.16f
    //           3c/3mx kernels need this to create their default
    //           AGN/SPR/DAT/WRD/WLD subdirectories on M:.
    //   false — Simple linear `addr & (ram.size() - 1)` mask. Safer for
    //           older kernels (v3.40f 3a, v4.20f Siena) that hang on
    //           the type=3 RAM-probe alias under MAME-strict because
    //           they don't save+restore around it. The trade-off is
    //           those kernels never create the M: subdirs, so apps
    //           launched against them surface "Media is corrupt" / a
    //           SYS$SHLL exit when they try to load their data files.
    //           A separate one-shot M:-drive subdir injection runs
    //           after the kernel writes the RAMDRIVE volume label —
    //           see populateMDriveSubdirs() — to paper over that gap.
    bool       useStrictRamMirror   = true;
    // Synthesise an F1 (System) key tap a few seconds into boot so the
    // idle kernel transitions from HLT to drawing the System screen.
    // Required for ROMs (e.g. PB2 v1.30f) that park on a key-wait
    // state after POST and never auto-advance without user input.
    bool       autoWakeOnBoot       = false;
    // Synthesise a single Esc tap ~7 s into cold boot to dismiss the
    // "SYS$SHLL.$05 Process exited Exit number 72" dialog that the
    // v1.30f shell raises against a freshly cold-formatted M: drive.
    // (The v3.40f shell raises an equivalent "Media is corrupt"
    // dialog but takes a different code path that coldBootStatus=true
    // clears up; v1.30f doesn't respond to clearing MainsPresent, so
    // the post-boot Esc tap is the correct one-shot dismissal.) Fires
    // exactly once, never again, so it never collides with user
    // keypresses during normal interaction.
    bool       dismissColdBootDialog = false;
    // Set the ASIC9 boot status to Reset|Cold (0x2000|0x8000), omitting
    // the MainsPresent bit (0x0020). MainsPresent causes the v3.40f EPOC16
    // shell to auto-launch the last-used app on session-restore, which
    // triggers "Media is corrupt" against a freshly cold-formatted M: drive.
    // Set for series3a / Workabout / WorkaboutMX; NOT set for pocketbk2
    // (v1.30f) whose kernel needs the default ASIC9 boot state (with
    // MainsPresent) to initialize its LCD correctly.
    bool       coldBootStatus       = false;
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

    uint8_t *getRamBuffer() override         { return ram.data(); }
    size_t   getRamSize()   const override   { return ram.size(); }
    void     loadRamSnapshot(const uint8_t *bytes, size_t size) override {
        std::memcpy(ram.data(), bytes, std::min(size, ram.size()));
    }

    int32_t     getClockSpeed() const override { return m_cfg.busClockHz; }
    const char *getDeviceName() const override { return m_cfg.displayName; }

    // ── ROM language variant (Siena) ────────────────────────────────────
    // The Siena's v4.20f image carries four locales — English (UK),
    // English (USA), Swedish and Spanish — and picks one at boot from
    // three strap pins on ASIC9 port C. The strap is what we supply, so
    // the choice is the user's; see the locale section in series3c.cpp
    // for the chain. Every other SIBO2 model reports no choice.
    int getLanguageCount() const override { return m_localeCount; }
    const char *getLanguageName(int index) const override;
    int getLanguage() const override { return m_language; }
    bool setLanguage(int index) override;

    int getDigitiserWidth()  const override { return m_cfg.lcdWidth; }
    int getDigitiserHeight() const override { return m_cfg.lcdHeight; }
    int getLCDOffsetX()      const override { return 0; }
    int getLCDOffsetY()      const override { return 0; }
    int getLCDWidth()        const override { return m_cfg.lcdWidth; }
    int getLCDHeight()       const override { return m_cfg.lcdHeight; }

    void readLCDIntoBuffer(uint8_t **lines, bool is32BitOutput) const override;
    void setKeyboardKey(EpocKey key, bool value) override;
    void updateTouchInput(int32_t x, int32_t y, bool down) override;

    // ── Audio (piezo buzzer + ASIC9 PCM codec) ─────────────────────────
    // ASIC9 has both the buzzer pin (driven by FRC1 or BuzzTog) and an
    // 8-bit ~8 kHz PCM codec wired through a 16-byte FIFO. Both flow
    // through SiboAudio (core/sibo_audio.{h,cpp}) — deliberately isolated
    // from the AudioCodecModel that backs EPOC32 (Windermere /
    // CL-PS711x) so SIBO and EPOC32 audio paths never share code.
    bool hasAudio() const override { return true; }
    // Microphone per hardware: the Series 3a / 3c / 3mx (and the
    // 3a-based Pocket Book II) have a mic wired to the ASIC9 codec's
    // capture side; the Siena and Workabout family have a speaker but
    // no recording input.
    bool hasMicrophone() const override {
        switch (m_cfg.model) {
        case Model::Series3a:
        case Model::Series3c:
        case Model::Series3mx:
            return true;
        case Model::Siena:
        case Model::Workabout:
        case Model::WorkaboutMX:
            return false;
        }
        return false;
    }
    int  getAudioSampleRate() const override {
        return SiboAudio::kAudioSampleRate;
    }
    size_t readAudioOutput(int16_t *dst, size_t maxSamples) override {
        return audio.readAudioOutput(dst, maxSamples);
    }
    void writeAudioInput(const int16_t *src, size_t count) override {
        if (!audio.hostMicEnabled()) return;
        audio.enqueueMicSamples(src, count);
    }
    void setHostAudioEnabled(bool speaker, bool mic) override {
        audio.setHostEnabled(speaker, mic);
    }

    // Codec activity counters (harness / browser tests): how many A-law
    // bytes the ASIC9 Snd timer has pushed to (playback) or pulled from
    // (capture) the SiboAudio rings since boot. Capture counts only pops
    // that returned real mic data, not empty-ring silence fills.
    uint64_t debugPcmOutCount() const { return m_pcmOutCount; }
    uint64_t debugPcmInCount()  const { return m_pcmInCount; }
    size_t   debugDacFill()     const { return audio.dacFill(); }
    size_t   debugAdcFill()     const { return audio.adcFill(); }
    const PsionAsic9 &debugAsic9() const { return asic9; }

    // ── SSD pack hooks ────────────────────────────────────────────────
    // Slot count: 2 for Series 3a/3c/3mx, 1 for Siena (single Honda
    // slot on ASIC9 channel 4). Slot 0 = user-visible "Pack A".
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

    // ── Host serial bridge ────────────────────────────────────────────
    // Same method names as Windermere::Emulator so wasm/main.cpp can
    // dispatch by dynamic_cast without caring which SoC owns the
    // serial port. uartIndex is accepted but ignored on SIBO2 (there's
    // only one UART, the Condor).
    //
    // When attached, the Condor's TXD callback is rewired to drain
    // bytes into a host-side TX buffer (instead of the Honda slot),
    // and modem-status lines (CTS/DSR/DCD) are forced high so the
    // PLP kernel sees a live cable. Detach reverses both.
    bool   serialAttachHost(int uartIndex);
    bool   serialDetachHost(int uartIndex);
    bool   serialIsAttached(int uartIndex) const;
    size_t serialWriteFromHost(int uartIndex, const uint8_t *data, size_t len);
    size_t serialReadToHost(int uartIndex, uint8_t *dst, size_t cap);
    size_t serialHostTxAvailable(int uartIndex) const;
    // Trickle staged host RX bytes into the Condor's 8-deep FIFO as the
    // kernel drains it. Called from executeUntil and serialWriteFromHost.
    void   pumpHostRxStage();

private:
    // Host-bridge TX scratch buffer (bytes the Condor's TXD callback
    // pushes when a host is attached). Drained by serialReadToHost.
    std::vector<uint8_t> m_hostTxBuf;
    // Host-bridge RX staging: serialWriteFromHost appends here and the
    // executeUntil pump tops up the Condor's 8-deep hardware FIFO as
    // the kernel drains it. Without this any host frame longer than 8
    // bytes would silently lose its tail (PsionCondor::pushRx drops on
    // full). Survives detach so a final Disc_Pdu isn't lost — mirrors
    // the Windermere bridge's detach semantics.
    std::deque<uint8_t> m_hostRxStage;
    bool m_hostAttached = false;
    Config m_cfg;

    V30 cpu{*this, V30Variant::V30H};
    PsionAsic9 asic9;

    std::vector<uint8_t> ram;
    std::vector<uint8_t> rom;

    // SSD pack slots. Sized in the constructor based on m_cfg.model:
    // Siena → 1, others → 2. Wired to ASIC9 SIBO channels in wireChips().
    std::vector<PsionSSD> m_ssd;
    int64_t m_doorNmiClearAt = -1;

    // Siena-only Honda expansion port + Condor UART. The Honda slot
    // carries both an ASIC9 SIBO parallel channel (channel 4, for SSD
    // packs) AND a serial UART (Condor) for modems / comm cables. The
    // SSD pack on slot 0 plugs into the Honda slot rather than being
    // wired directly to ASIC9 channel 4. Unused on 3a/3c/3mx/Workabout.
    PsionCondor m_condor;
    PsionHonda  m_honda;
    // ASIC9MX models only: the second integrated 16550 ("UART0",
    // word-spaced at I/O 0x40-0x4E, interrupt = ASIC9MX status bit 8 /
    // vector 0x76). m_condor doubles as "UART1" (0x50-0x5E) on these
    // models.
    PsionCondor m_mxUart0;
    // Which PsionCondor instance serves a host-bridge uartIndex:
    //   0 = the Remote Link port — the Condor on the 3a/3c/Siena,
    //       ASIC9MX UART1 on the 3mx, ASIC9MX UART0 on the Workabout
    //       MX (its v7.20f link server defaults there);
    //   1 = the infrared port where it is a SEPARATE UART — the 3mx's
    //       ASIC9MX UART0 (the v6.16f kernel brings it up at IR-screen
    //       arm: LCR 0x83 @0x46, divisor, FCR 0x87 @0x44, MCR 0xCC
    //       @0x48, IER 0x07 @0x42). The 3c/Siena Condor multiplexes
    //       cable + IR onto the same register file, so both ride
    //       uartIndex 0 there.
    PsionCondor &hostUart(int uartIndex) {
        if (m_cfg.model == Model::WorkaboutMX) return m_mxUart0;
        if (m_cfg.model == Model::Series3mx && uartIndex == 1) return m_mxUart0;
        return m_condor;
    }
    // The uartIndex the host bridge is currently bound to (only
    // meaningful while m_hostAttached).
    int m_hostUartIndex = 0;
    // Which integrated-UART instance (if any) an I/O port decodes to
    // on the MX models; nullptr elsewhere. Defined in series3c.cpp.
    PsionCondor *mxUartAt(uint16_t port);

    // Audio: buzzer level mirrors the last ASIC9 buz_cb edge; the deadline
    // schedules the next 64 Hz buzzer-pump call into SiboAudio. The PCM
    // callbacks push/pop samples directly into SiboAudio's ring — no
    // intermediate FIFO, since ASIC9's own Snd timer already drives the
    // codec sample cadence and asserts A9IntSnd.
    SiboAudio audio;
    bool      m_buzzerLevel     = false;
    int64_t   m_nextAudioTickAt = 0;
    uint64_t  m_pcmOutCount     = 0;
    uint64_t  m_pcmInCount      = 0;

    bool initialised = false;

    // Locale table read out of the ROM by scanLocales() (Siena only):
    // the country code at the head of each locale block the ROM's
    // top-of-image table points at, in table order. m_language is the
    // index the port C strap reports — 0, what the ROM's own table
    // puts first, until the user picks another.
    static constexpr int kMaxLocales = 8;
    int      m_localeCount = 0;
    uint16_t m_localeCountry[kMaxLocales] = {};
    int      m_language = 0;
    void scanLocales();

    // Keyboard matrix: 8 columns, each an 11-bit row mask. MAME's kbd_r
    // returns OR of all columns currently strobed via A9WControlExtra's
    // low nibble. m_key_col_mask is the 1-hot byte produced by the
    // col_cb callback (bit N set -> column N selected).
    uint16_t m_key_row[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    uint8_t  m_key_col_mask = 0;

    // Auto-wake tap bookkeeping (PSION_AUTO_WAKE env var). One-shot F1
    // press a few seconds into boot to drag the idle kernel into drawing
    // the System screen — see executeUntil for rationale.
    bool m_autoWakePressed  = false;
    bool m_autoWakeReleased = false;
    // One-shot cold-boot dialog dismiss (PB2 v1.30f). Pressed → released
    // → done; the cpu loop checks m_dismissDone to avoid re-firing.
    bool m_dismissPressed = false;
    bool m_dismissDone    = false;
    // Diagnostic M:-drive overlay (opt-in via PSION_MDRIVE_INJECT=1). On
    // 3a v3.40f / Siena v4.20f the kernel never lays down the AGN/SPR/
    // DAT/WRD/WLD subdir tree that v5+ kernels create on cold boot, and
    // overlaying the captured 3c boot RAM image at the right base does
    // NOT in itself fix the resulting "Media is corrupt" / "Process
    // exited" dialog (the kernels manage their M:-drive metadata in
    // internal data structures, not by reading the RAM offsets we'd
    // overlay). The hook is kept in place — gated behind the env var so
    // it doesn't run for normal users — as a diagnostic platform for
    // future investigations into the v3.40f / v4.20f cold-boot path.
    bool m_mDriveInjectAttempted = false;
    // knownLabelAt >= 0: use that RAM offset directly (paged trigger knows
    // the exact address); -1: scan RAM for the first "RAMDRIVE   \x08"
    // signature (polling path).
    void maybeInjectMDriveSubdirs(int32_t knownLabelAt = -1);

    void wireChips();

    // Condor UART register access (I/O 0x0100..0x011F). Routed to
    // m_condor on the Siena, falls back to a stub byte pattern on the
    // 3c/3mx so the kernel's UART probe still completes without
    // perturbing those device's golden boot paths.
    uint8_t condorRead(uint16_t port);
    void    condorWrite(uint16_t port, uint8_t v);

    // Translate a 24-bit ASIC9 internal RAM address (post page-select) to
    // a `ram` buffer offset, or -1 if the address falls in an unmapped
    // gap of the kernel-selected RAM layout. Mirrors MAME's
    // configure_ram (reference/mame-psion/machine/psion_asic9.cpp:457).
    int32_t translateRamAddr(uint32_t addr) const;

    // Open-bus detection for the page-select-window writeMemByte
    // branch. Used to drop writes that fall outside the asic9's
    // install_ram coverage at the current RamDeviceSize setting. (The
    // misleading name "...IvtUnderMirror" is historic — see the comment
    // on the implementation in series3c.cpp.)
    bool writeAliasesIvtUnderMirror(uint32_t addr) const;
};

} // namespace Series3c
