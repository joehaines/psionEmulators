// license:BSD-3-Clause
// copyright-holders:Nigel Barnes (MAME psion3a driver — address + I/O maps)
//                  + adaptation by the Psion emulator project, 2026.
//
// Series 3c driver implementation. See series3c.h for the memory map and
// the MAME source it is derived from. This is a bring-up scaffold: the
// V30 core is still a ~4-opcode stub and PsionAsic9 hasn't been ported
// yet, so the harness just runs the CPU far enough to print which opcode
// it hits first. See core/series3.cpp for the fully-wired Series 3
// analogue this file mirrors.

#include "series3c.h"
#include "series3c_mdrive_blob.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Series3c {

Emulator::Emulator(const Config &cfg)
    : m_cfg(cfg),
      // Zero-init RAM. MAME's ram_device defaults to a host-filled
      // pattern, but since the EPOC16 cold-boot path runs its own RAM
      // test + fill (0xA5 everywhere first, then zero) the initial
      // value is overwritten before any checksum is computed.
      ram(cfg.ramSize, [&]{
          // Diagnostic: PSION_RAM_FILL=NN seeds RAM with byte 0xNN at
          // boot. Used for hunting kernel cold-boot RAM-pattern checks
          // (e.g. v3.40f 3a's "Media is corrupt" → does the kernel take
          // a different path on 0xFF vs 0x00?). Defaults to 0x00, which
          // matches the post-RAM-test cleared state EPOC16 expects.
          const char *v = std::getenv("PSION_RAM_FILL");
          if (!v || !v[0]) return uint8_t(0);
          return uint8_t(std::strtoul(v, nullptr, 16) & 0xFF);
      }()),
      rom(cfg.romSize, 0xFF),
      // Siena has a single SSD slot via the Honda connector; the
      // 3a/3c/3mx have two dedicated pack slots.
      m_ssd(cfg.model == Model::Siena ? 1 : 2) {
    wireChips();
    audio.configure(m_cfg.busClockHz);
    // Match the click-envelope hold used by Series 3 (ASIC2 buzzer) — the
    // ASIC9 BuzzTog bit has comparable hold timing to ASIC2's
    // BuzzerToggle bit.
    audio.buzzerClickHoldTicks = 2;
}

void Emulator::wireChips() {
    asic9.setBusClock(m_cfg.busClockHz);
    // Devices that set coldBootStatus boot with Reset|Cold (0x2000|0x8000),
    // omitting the MainsPresent bit (0x0020). When MainsPresent is set, the
    // v3.40f EPOC16 shell's session-restore path auto-launches the last-used
    // app on a freshly cold-formatted M: drive whose data directory does not
    // yet exist, triggering a "Media is corrupt" dialog. Omitting MainsPresent
    // (battery-only boot) causes the shell to skip session-restore and show
    // the System screen — the correct first-boot state. The pocketbk2 (v1.30f)
    // keeps the default ASIC9 boot state (with MainsPresent) because its kernel
    // requires MainsPresent to initialize the LCD correctly.
    if (m_cfg.coldBootStatus) {
        // Reset|Cold only — no MainsPresent (avoids session-restore "Media is
        // corrupt" on 3a), no PowerFail (MainsPresent=0+PowerFail=1 is a
        // MAME-divergent combination that risks an unexpected recovery path).
        asic9.setBootStatus(0x2000 | 0x8000);
    }
    if (m_cfg.model == Model::Series3a) {
        // Series 3a corrections that match MAME but would regress other kernels
        // that were validated against the legacy approximations:
        // - 32.768 Hz tick (MAME-exact); other devices keep legacy 32 Hz.
        // - No SIB heartbeat: MAME never sets A9MNoBattery (bit 12); setting it
        //   periodically triggers the v3.40f 3a "no backup battery" code path.
        //   3c / Siena / Workabout kernels rely on the heartbeat for keyboard.
        asic9.setAccurateTick(true);
        asic9.enableSibHeartbeat(false);
    }
    // Diagnostic override: PSION_SIB_HEARTBEAT=0/1 forces the heartbeat
    // off/on regardless of model defaults (experiments only).
    if (const char *hb = std::getenv("PSION_SIB_HEARTBEAT")) {
        if (hb[0]) asic9.enableSibHeartbeat(hb[0] != '0');
    }
    asic9.setIrqOutCb([this](bool s) { cpu.setIrqLine(s); });
    asic9.setNmiOutCb([this](bool s) { cpu.setNmiLine(s); });
    // SIB POLL line: deasserted for 24 cycles during each SIB frame transfer
    // so the V30 WAIT instruction stalls correctly (MAME NEC_INPUT_LINE_POLL).
    asic9.setPollCb([this](bool s) { cpu.setPollLine(s); });

    // Seed the ASIC9's "configured RAM type" with the value MAME
    // computes from the actual installed RAM size at machine_start
    // (psion_asic9.cpp:126-128, get_ram_type). The kernel's first
    // writes to A9WControl on the 3a/3c/Siena leave bits 3-4 = 0,
    // and MAME treats that as a no-op for the RAM layout because
    // the bits haven't *changed* — so the layout stays at the
    // default. translateRamAddr() reads `asic9.ramType()` to drive
    // the configure_ram-style address match.
    uint8_t initialType;
    switch (ram.size()) {
    case 0x010000: case 0x020000: initialType = 0; break;
    case 0x040000: case 0x080000: initialType = 1; break;
    case 0x100000: case 0x200000: initialType = 2; break;
    case 0x400000: case 0x800000: initialType = 3; break;
    default:                      initialType = 2; break;
    }
    asic9.setRamTypeDefault(initialType);
    // LCD scanout reads VRAM — route through our own memory decode so
    // ASIC9 sees the same view the CPU does (RAM low, ROM high).
    asic9.setMemReader([this](uint32_t addr) { return readMemByte(addr); });
    // Type-3 RAM-probe shadow (Series 3a + Pocket Book II only — the
    // method short-circuits for other models). Fires when the v3.40f
    // kernel sets RamDeviceSize=3 transiently during its RAM probe.
    // (No RamDeviceSize callback needed — the type-3 RAM-probe shadow
    //  that this hook used to feed has been removed; we now match MAME
    //  by letting the mirror-aliased writes through directly.)
    // Keyboard: ASIC9 drives column select via A9WControlExtra; read of
    // PortAB returns the OR of all pressed keys in the selected columns.
    // Matches MAME's psion3a_base_state::kbd_r and the col_cb hookup.
    asic9.setColCb([this](uint8_t colMask) { m_key_col_mask = colMask; });
    asic9.setPortAbReader([this]() -> uint16_t {
        uint16_t v = 0;
        for (int i = 0; i < 8; ++i) {
            if (m_key_col_mask & (1 << i)) v |= m_key_row[i];
        }
        return v;
    });

    // SSD packs: per MAME reference/mame-psion/psion/psion3a.cpp:507-510,
    // ASIC9 SIBO channel 1 is "Pack 1" and channel 0 is "Pack 2". We
    // expose those as user-visible slot 0 (Pack A) and slot 1 (Pack B)
    // respectively — i.e. the channel ↔ slot mapping is inverted on
    // 3a/3c/3mx so the UI labelling matches the System screen drive
    // letters. Siena has one pack on channel 4 (Honda slot, per
    // reference/mame-psion/psion/siena.cpp:305-312). The Siena routes
    // its SSD through the Honda slot rather than wiring ASIC9 ch4
    // directly: this models the real expansion port that also carries
    // the Condor UART (serial path for modems / comm cables / RS-232
    // external drives), even though in MAME the SSD's own data still
    // travels on the parallel SIBO half.
    if (m_cfg.model == Model::Siena) {
        // Condor UART <-> Honda serial wiring (siena.cpp:299-309).
        m_condor.setTxdCb([this](uint8_t b) { m_honda.writeTxd(b); });
        m_condor.setRtsOutCb([this](bool s) { m_honda.writeRts(s); });
        m_condor.setDtrOutCb([this](bool s) { m_honda.writeDtr(s); });
        m_condor.setIrqOutCb([this](bool s) { asic9.setEint1(s); });
        m_honda.setRxdInCb([this](uint8_t b) { m_condor.pushRx(b); });
        m_honda.setCtsInCb([this](bool s) { m_condor.setCts(s); });
        m_honda.setDsrInCb([this](bool s) { m_condor.setDsr(s); });
        m_honda.setDcdInCb([this](bool s) { m_condor.setDcd(s); });
        // Honda medchng output -> ASIC9 media-change NMI line.
        m_honda.setMedChngOutCb([this](bool s) { asic9.setMedChng(s); });
        // ASIC9 SIBO ch4 -> Honda parallel half (which routes to the
        // SSD carrier when one is attached).
        asic9.setSibChannelReader(4, [this]() { return m_honda.readSibFrame(); });
        asic9.setSibChannelWriter(4, [this](uint16_t f) { m_honda.writeSibFrame(f); });
        // SSD slot 0 is plugged into the Honda slot as the carrier.
        m_honda.attachSsdCarrier(&m_ssd[0]);
    } else {
        asic9.setSibChannelReader(1, [this]() { return m_ssd[0].readFrame(); });
        asic9.setSibChannelWriter(1, [this](uint16_t f) { m_ssd[0].writeFrame(f); });
        asic9.setSibChannelReader(0, [this]() { return m_ssd[1].readFrame(); });
        asic9.setSibChannelWriter(0, [this](uint16_t f) { m_ssd[1].writeFrame(f); });
        // 3a / 3c: Condor IRQ → ASIC9 eint1, matching MAME
        // psion3a.cpp:578. On the MX models the link UART is the
        // ASIC9MX-integrated 16550 "UART1" (same PsionCondor instance,
        // mapped word-spaced at I/O 0x50-0x5E) and its interrupt is
        // ASIC9MX status bit 9 (0x0200) — the bit the v6.16f kernel
        // RMWs into the 16-bit interrupt mask at link-enable. The
        // TXD/RTS/DTR callbacks are wired by serialAttachHost when a
        // host bridge attaches; until then they're unset (bytes drop
        // silently, matching the no-cable carrier state).
        if (m_cfg.model == Model::Series3mx || m_cfg.model == Model::WorkaboutMX) {
            m_condor.setPlain16550IrqMode(true);
            m_condor.setIrqOutCb([this](bool s) { asic9.setMxUartInt(1, s); });
            m_mxUart0.setPlain16550IrqMode(true);
            m_mxUart0.setIrqOutCb([this](bool s) { asic9.setMxUartInt(0, s); });
        } else {
            m_condor.setIrqOutCb([this](bool s) { asic9.setEint1(s); });
        }
    }

    // ASIC9 piezo buzzer pin → AudioCodecModel. Same pattern as the
    // Series 3 ASIC2 wiring: cache the pin level for the per-tick
    // square-wave pump and latch a click envelope on the rising edge.
    asic9.setBuzCb([this](bool s) {
        if (s && !m_buzzerLevel) audio.noteBuzzerRisingEdge();
        m_buzzerLevel = s;
    });

    // ASIC9 A-law codec (M7702 / M7542). Its internal Snd timer
    // (psion_asic9.cpp's fireSnd) drives both directions autonomously and
    // asserts A9IntSnd when the FIFO needs servicing, so the driver only
    // forwards raw A-law bytes to/from the SiboAudio ring — the A-law
    // expand/compress happens inside SiboAudio.
    asic9.setPcmOut([this](uint8_t s) {
        m_pcmOutCount++;
        audio.pushPcmSample(s);
    });
    asic9.setPcmIn([this]() -> uint8_t {
        auto r = audio.popPcmSample();
        if (r.valid) m_pcmInCount++;
        // popPcmSample already returns the A-law silence byte when the
        // ring is empty, so the unconditional .sample read is safe.
        return r.sample;
    });

    // Door callback: SSDs pulse ASIC9's media-change NMI on
    // hot-plug attach / detach for ~200 ms so EPOC16 re-scans the
    // slot. Cold attach (before any cycles have run) skips the pulse
    // because the IVT isn't populated yet — EPOC16 scans the slot
    // during boot regardless. Cleared by the executeUntil tick loop
    // once the deadline elapses.
    auto doorPulse = [this](bool s) {
        if (!s || passedCycles == 0) return;
        asic9.setMedChng(true);
        // ~200 ms at the V30H bus clock.
        m_doorNmiClearAt = passedCycles + (m_cfg.busClockHz / 5);
    };
    for (auto &ssd : m_ssd) ssd.setDoorCb(doorPulse);
}

void Emulator::loadROM(uint8_t *buffer, size_t size) {
    std::fill(rom.begin(), rom.end(), 0xFF);
    std::memcpy(rom.data(), buffer, std::min(size, rom.size()));

    if (!initialised) {
        cpu.reset();
        asic9.reset();
        // Condor is now wired (IRQ at minimum) on all SIBO2 models, so
        // reset it everywhere so its register state is well-defined
        // before the kernel's first UART probe.
        m_condor.reset();
        m_mxUart0.reset();
        initialised = true;
    }
}

void Emulator::executeUntil(int64_t cycles) {
    // Don't gate on !cpu.halted — after a HLT the CPU parks until an
    // interrupt wakes it, but that interrupt can only fire if we keep
    // driving asic9.tick() (which advances the ASIC's timer scheduler).
    // V30::step() returns a small cycle count while halted so the loop
    // still makes wall-clock progress.
    //
    // Auto-wake tap (optional): Series 3c cold-boot only unmasks the Frc1
    // interrupt and then parks in HLT, waiting for the user to press an
    // app-row key. The MAME driver's wakeup callback fires EINT0 on any
    // keypress from those rows, which the kernel treats as "please draw
    // the System screen". For headless benches / automated screenshots we
    // synthesise that tap after a short delay so the harness sees UI
    // content instead of a blank paper field.
    //
    // Active when the device config sets autoWakeOnBoot (e.g. PB2 v1.30f,
    // which parks in a key-wait state after POST), or via the
    // PSION_AUTO_WAKE=1 env-var override for other devices. Interactive
    // front-ends drive the keyboard matrix themselves so neither path
    // fires during normal use.
    const char *autoWakeEnv = std::getenv("PSION_AUTO_WAKE");
    const bool autoWakeEnabled = m_cfg.autoWakeOnBoot ||
        (autoWakeEnv && autoWakeEnv[0] != 0 && autoWakeEnv[0] != '0');

    // One-shot Esc tap to dismiss the v1.30f cold-boot "Process exited
    // Exit number 72" dialog (see Config::dismissColdBootDialog). Press
    // at ~7 s, release ~50 ms later, then never again — m_dismissDone
    // latches so a single tap is all the user ever sees.
    constexpr int64_t DISMISS_PRESS_AT  = 53'760'000; // ~7.0 s @ 7.68 MHz
    constexpr int64_t DISMISS_PRESS_DUR =    384'000; // ~50 ms @ 7.68 MHz
    while (passedCycles < cycles) {
        if (m_cfg.dismissColdBootDialog && !m_dismissDone) {
            if (passedCycles >= DISMISS_PRESS_AT && !m_dismissPressed) {
                setKeyboardKey(EStdKeyEscape, true);
                m_dismissPressed = true;
            } else if (m_dismissPressed
                    && passedCycles >= DISMISS_PRESS_AT + DISMISS_PRESS_DUR) {
                setKeyboardKey(EStdKeyEscape, false);
                m_dismissDone = true;
            }
        }
        if (autoWakeEnabled) {
            // Press F1 (System) at ~6.5s, release ~50 ms later, then
            // retry every 1.5 s — the ROM's POST may reach its
            // keyboard-wait state at an unpredictable time, so we send
            // a train of taps rather than a single one.
            constexpr int64_t PRESS_INTERVAL = 11'520'000; // 1.5 s @ 7.68 MHz
            constexpr int64_t FIRST_PRESS    = 50'000'000; // ~6.5 s
            constexpr int64_t PRESS_DUR      =    384'000; // 50 ms @ 7.68 MHz
            if (passedCycles >= FIRST_PRESS) {
                int64_t since = passedCycles - FIRST_PRESS;
                int64_t phase = since % PRESS_INTERVAL;
                bool wantDown = phase < PRESS_DUR;
                if (wantDown && !m_autoWakePressed) {
                    setKeyboardKey(EStdKeyF1, true);
                    m_autoWakePressed = true;
                } else if (!wantDown && m_autoWakePressed) {
                    setKeyboardKey(EStdKeyF1, false);
                    m_autoWakePressed = false;
                }
            }
        }
        int64_t consumed = cpu.step();
        if (consumed <= 0) consumed = 1;
        passedCycles += consumed;
        asic9.tick(consumed);
        // Pump the buzzer square wave at ~64 Hz (125 samples per call at
        // 8 kHz). Active while the buzzer pin is high or the click
        // envelope is still holding — see AudioCodecModel docstring.
        if (passedCycles >= m_nextAudioTickAt) {
            bool active = m_buzzerLevel || audio.buzzerHolding();
            audio.emitBuzzerSamples(active, audio.buzzerClickHz);
            m_nextAudioTickAt = passedCycles + (m_cfg.busClockHz / 64);
        }
        // Top up the Condor RX FIFO from the host-bridge staging queue
        // as the kernel drains it (cheap no-op when the queue is empty).
        if (!m_hostRxStage.empty()) pumpHostRxStage();
        // Clear the SSD door NMI once the ~200 ms pulse expires.
        if (m_doorNmiClearAt >= 0 && passedCycles >= m_doorNmiClearAt) {
            asic9.setMedChng(false);
            m_doorNmiClearAt = -1;
        }
        // Opt-in M: drive overlay diagnostic. Throttled to once every
        // ~16 ms so the cost is negligible when the env-var is unset
        // (the static `enabled` cache short-circuits immediately).
        if (!m_mDriveInjectAttempted) {
            constexpr int64_t kCheckInterval = 122'880; // ~16 ms @ 7.68 MHz
            if ((passedCycles % kCheckInterval) < consumed) {
                maybeInjectMDriveSubdirs();
            }
        }
    }
}

// Diagnostic M:-drive subdir injection. Off by default for all devices.
//
// The real fix for the Series 3a "Media is corrupt" dialog is booting
// without the MainsPresent bit (see wireChips): that prevents the v3.40f
// shell's session-restore path from auto-launching the last-used app
// against a freshly cold-formatted M: drive whose data directories do
// not yet exist.
//
// The injection hook is preserved here for diagnostic investigation only.
// Experiments confirmed it does NOT suppress the dialog on its own: the
// v3.40f / v4.20f kernels manage M:-drive metadata in internal EFSYS
// state rather than reading from the RAM offsets we overlay, so the
// blob is silently ignored by the filesystem driver.
//
// Enable via PSION_MDRIVE_INJECT=1. Siena would need a separate blob.
void Emulator::maybeInjectMDriveSubdirs(int32_t knownLabelAt) {
    if (m_mDriveInjectAttempted) return;
    static int enabled = -1;
    if (enabled < 0) {
        const char *v = std::getenv("PSION_MDRIVE_INJECT");
        if (v && v[0]) {
            enabled = (v[0] != '0') ? 1 : 0;
        } else {
            // Off by default for all devices — the injection doesn't
            // fix the dialog (see comment above maybeInjectMDriveSubdirs).
            enabled = 0;
        }
    }
    if (!enabled) {
        // Mark attempted so we stop polling — the env-var doesn't
        // change at runtime, no point re-checking on every tick.
        m_mDriveInjectAttempted = true;
        return;
    }
    if (m_cfg.model != Model::Series3a && m_cfg.model != Model::Siena) {
        m_mDriveInjectAttempted = true;
        return;
    }

    // Locate the kernel's "RAMDRIVE   \x08" volume-label signature.
    // When called from the paged-window trigger we already know the exact
    // RAM offset (knownLabelAt), so skip the scan.  The polling path uses
    // knownLabelAt=-1 and searches the entire buffer.
    static const uint8_t kLabelSig[] = {
        'R','A','M','D','R','I','V','E',' ',' ',' ', 0x08
    };
    int32_t labelAt = knownLabelAt;
    if (labelAt < 0) {
        for (size_t i = 0; i + sizeof(kLabelSig) <= ram.size(); ++i) {
            if (ram[i] == 'R' &&
                std::memcmp(ram.data() + i, kLabelSig, sizeof(kLabelSig)) == 0) {
                labelAt = int32_t(i);
                break;
            }
        }
    }
    if (labelAt < 0) return; // not yet written — try again next tick

    if (m_cfg.model == Model::Series3a) {
        int32_t base = labelAt - int32_t(kMDriveBlobLabelOffset);
        if (base < 0 || size_t(base) + 0x7600 > ram.size()) {
            m_mDriveInjectAttempted = true;
            return;
        }
        for (size_t i = 0; i < kMDriveBlobChunkCount; ++i) {
            const auto &c = kMDriveBlobChunks[i];
            std::memcpy(ram.data() + base + c.offset, c.bytes, c.length);
        }
        std::fprintf(stderr,
                     "[mdrive] 3a overlay applied: label@0x%05X base@0x%05X "
                     "(%zu chunks)\n",
                     labelAt, base, kMDriveBlobChunkCount);
    } else {
        // Siena: the v4.20f kernel uses a top-anchored layout and 32 KiB
        // of headroom isn't available below the label. We don't have a
        // captured Siena cold-boot blob yet, so the diagnostic injection
        // here is intentionally minimal — just append 5 zero-cluster
        // subdir placeholders after the label so the kernel's directory
        // scan sees AGN/SPR/DAT/WRD/WLD entries even if their contents
        // are empty. This is a known no-op on the v4.20f kernel (see
        // series3c.h) but it's the cleanest hook for a future fix.
        static const char *kSubdirNames[5] = {
            "WLD", "SPR", "DAT", "WRD", "AGN"
        };
        for (int n = 0; n < 5; ++n) {
            int32_t entryAt = labelAt + 0x20 + (n * 0x20);
            if (size_t(entryAt) + 0x20 > ram.size()) break;
            uint8_t entry[0x20] = {0};
            std::memset(entry, ' ', 11);
            const char *nm = kSubdirNames[n];
            for (size_t k = 0; k < 3 && nm[k]; ++k) entry[k] = uint8_t(nm[k]);
            entry[0x0B] = 0x10;     // directory attribute
            entry[0x16] = 0x21;     // time-low (matches captured 3c)
            entry[0x17] = 0x48;     // time-high
            entry[0x18] = 0x21;     // date-low
            entry[0x19] = 0x20;     // date-high
            std::memcpy(ram.data() + entryAt, entry, sizeof(entry));
        }
        std::fprintf(stderr,
                     "[mdrive] Siena placeholder subdirs written at 0x%05X\n",
                     labelAt);
    }
    m_mDriveInjectAttempted = true;
}

// ──────────────────────────────────────────────────────────────────────
// PSION_RAM_TRACE: dump every RAM write through the paged + unpaged
// windows to a file (or stderr if path is "-"). Used to diff our address
// resolution against the kernel's intent during cold-boot M: drive
// format. Off by default — set PSION_RAM_TRACE=<path> to enable.
// Output format:
//
//   W cy=<cycles> pc=<csip> la=<linear> path=<L|6|7|8|9> \
//       psel=<pp> ia=<internal24> off=<ramOffset|drop=reason> val=<byte>
//
// where cycles is passedCycles at access time (lets us correlate with
// the harness PC sampler). path L = unpaged low window 0x00000..0x5FFFF;
// 6/7/8/9 = paged window at V30 segment 0x6/7/8/9000.
//
// Findings (2026-04 instrumentation run on v3.40f vs v5.20f):
//   - On 2 MiB Series 3a v3.40f, cold-format completes with only 6 of
//     2.16M writes dropped — and all 6 are legitimate type=3 RAM-size
//     probe writes (PC 0xaf05f / 0xaf0bc / 0xaf0c1) that we *want* to
//     drop. So "Agenda: Media is corrupt" is NOT caused by lost writes
//     in our address-translation path — the lost-write hypothesis from
//     prior investigations is falsified.
//   - (Corrected by empirical MAME-vs-ours boot diff via
//     tools/mame-vs-ours/, May 2026.) Prior write-up claimed "v3.40f
//     writes only the RAMDRIVE volume label and leaves the root
//     directory otherwise empty." That's wrong on real MAME — MAME's
//     psion3a2 driver running the same v3.40f ROM writes the full
//     AGN/SPR/DAT/WRD/WLD attr=0x10 entries (visible at the high-RAM
//     M: drive site, ~0x84200, AND in a low-RAM cache copy at
//     ~0x9c7a). The "Media is corrupt" failure was caused by US not
//     reaching the FAT-subdir-write code path, not by v3.40f
//     deliberately skipping it. Resolved by the writeMemByte fix
//     (May 2026): stop dropping the v3.40f kernel's mirror-aliased
//     RAM-size probe writes — they're supposed to fail the first
//     cascade (psel=0x10 → corrupts the RAM[0] marker → fall through
//     to psel=0x04 → probe step BH = 0x10 not 0x40). With the fix in
//     place, the FAT-subdir-write code path runs and the System
//     screen displays the full app set. See
//     tools/mame-vs-ours/FINDINGS.md for the eight-hop chain that
//     led to this conclusion.
//   - Fix: maybeInjectMDriveSubdirs fires synchronously inside
//     writeMemByte at the exact cycle the 0x08 attribute byte is stored
//     — before the kernel instruction even returns. This means the
//     blob's FAT16 BPB, FAT sectors, root-dir entries (RAMDRIVE label +
//     WLD/SPR/DAT/WRD/AGN), and cluster data are in RAM before the
//     kernel has a chance to read them back and build any directory
//     cache. The harness boot test confirms the Agenda opens cleanly
//     after injection (System screen at ~7 s, clean Agenda UI at ~14 s,
//     no "Media is corrupt" dialog). Interactive validation is still
//     recommended for edge-case scenarios (second boot, file writes).
// ──────────────────────────────────────────────────────────────────────
static std::FILE *ramTraceFp() {
    static std::FILE *fp = nullptr;
    static bool inited   = false;
    if (!inited) {
        inited = true;
        const char *p = std::getenv("PSION_RAM_TRACE");
        if (p && p[0]) {
            if (p[0] == '-' && p[1] == '\0') fp = stderr;
            else                              fp = std::fopen(p, "w");
        }
    }
    return fp;
}
static inline bool ramTraceOn() { return ramTraceFp() != nullptr; }

static void emitRamTrace(char op, int64_t cycles, uint32_t csip,
                         uint32_t la, char path, uint8_t psel,
                         uint32_t internal, int64_t ramOff,
                         const char *dropReason, uint8_t val) {
    std::FILE *fp = ramTraceFp();
    if (!fp) return;
    if (ramOff >= 0) {
        std::fprintf(fp,
            "%c cy=%lld pc=%05x la=%05x path=%c psel=%02x ia=%06x off=%06x val=%02x\n",
            op, (long long)cycles, csip & 0xFFFFF, la & 0xFFFFF, path, psel,
            internal & 0xFFFFFF, uint32_t(ramOff), val);
    } else {
        std::fprintf(fp,
            "%c cy=%lld pc=%05x la=%05x path=%c psel=%02x ia=%06x drop=%s val=%02x\n",
            op, (long long)cycles, csip & 0xFFFFF, la & 0xFFFFF, path, psel,
            internal & 0xFFFFFF, dropReason ? dropReason : "?", val);
    }
}

// ──────────────────────────────────────────────────────────────────────
// V30Bus — memory accesses
//
// MAME's psion_asic9_device::mem_map covers the entire 0x00000-0xFFFFF
// V30 program window and funnels every access through mem_r/mem_w, which
// dispatches through ASIC9's own internal address space. That internal
// space is populated at machine_reset time by psion_asic9_device::
// machine_reset:
//   - RAM banks at 0x000000..(N*ram_size-1)
//   - ROM  at 0x800000..0x800000 + rom_size - 1, mirrored across 0x7FFFFF
// plus four 64 KiB paged windows at V30 segments 0x6000/0x7000/0x8000/
// 0x9000 selected via A9BPageSelect{6,7,8,9}000 I/O registers.
//
// Series 3c memory layout (matches MAME's psion_asic9_device internal
// decode, collapsed onto the 1 MiB V30 address space):
//
//     0x00000-0x5FFFF   RAM, low 384 KiB (unpaged)
//     0x60000-0x9FFFF   four 64 KiB paged windows, psel_{6,7,8,9}000
//                       selects an 8-bit 64 KiB bank of the ASIC9
//                       internal address space (RAM banks at 0x00xxxx,
//                       ROM at 0x80xxxx). For cold boot, the Series 3c
//                       ROM reprograms these via A9WPSelN000; until then
//                       all four default to page 0xFE so the windows
//                       mirror the top of ROM (which is what the reset
//                       code expects to see through the windows).
//     0xA0000-0xFFFFF   top 384 KiB of ROM (unpaged). Reset vector at
//                       FFFF:0000 (linear FFFF0) therefore reads ROM
//                       offset 0x1FFFF0 = `EA 00 00 00 A0 00` = JMP
//                       A000:0000. Control then lands at 0xA0000 which
//                       is ROM offset 0x1A0000 — the kernel entry.
// ──────────────────────────────────────────────────────────────────────

// Translate a 24-bit ASIC9 internal RAM address to a `ram` offset, or
// return -1 if unmapped. This is a port of MAME's configure_ram
// (reference/mame-psion/machine/psion_asic9.cpp:457): for each device
// installed in the ram_device_size("m_ram_type") sized array, an
// install range is laid out at `device_size_test * i` with mirror at
// `device_size_test * 4` minus an actual-vs-test gap-fix. We compute
// the same address-match logic on every access so the layout follows
// whatever RamDeviceSize the kernel has currently programmed in
// A9WControl bits 3-4 — matching MAME exactly is what the SIBO2 ROMs
// expect (in particular the 3a / 3c boot path writes type=3 and then
// uses psel banks 0..0x0F for device 0 only; with our previous simple
// `2 * ram.size()` mirror, banks 0x10..0x1F aliased into device 1's
// real bytes and the kernel's M: drive layout overlapped its own FAT
// metadata, surfacing as "Agenda: Media is corrupt").
//
// Until the kernel writes A9WControl for the first time we fall back
// to the get_ram_type(ram.size()) default that MAME applies in
// machine_start, so accesses during the brief reset-to-first-write
// window see the actual installed RAM rather than the vacuous type=0
// (64 KiB devices, only 128 KiB visible) layout that bare m_a9_control=0
// would otherwise pick.
int32_t Emulator::translateRamAddr(uint32_t addr) const {
    addr &= 0xFFFFFFu;
    if (ram.empty()) return -1;

    // Per MAME's get_ram_type / ram_device_size tables (machine/psion_asic9.cpp:165
    // and :427), the per-device size is fixed by the configured RAM type, and the
    // number of devices equals total_ram / device_size_actual. For sub-1-MiB RAM
    // configs there's only ONE device, not two — we previously hard-coded two
    // which made the kernel's RAM-size probe see a wrong layout.
    auto deviceSize = [](uint8_t t) -> uint32_t {
        switch (t & 3) {
        case 0: return 0x010000;
        case 1: return 0x040000;
        case 2: return 0x100000;
        case 3: return 0x400000;
        }
        return 0x100000;
    };
    uint32_t deviceSizeActual = deviceSize(0);  // start; corrected below
    // Compute the actual device size from the kernel-default RAM type, not the
    // current ramType register (which the kernel may transiently bump during
    // its probe). Use ram_size to derive it the same way MAME does.
    if      (ram.size() >= 0x800000) deviceSizeActual = 0x400000;
    else if (ram.size() >= 0x400000) deviceSizeActual = 0x400000;
    else if (ram.size() >= 0x200000) deviceSizeActual = 0x100000;
    else if (ram.size() >= 0x100000) deviceSizeActual = 0x100000;
    else if (ram.size() >= 0x080000) deviceSizeActual = 0x040000;
    else if (ram.size() >= 0x040000) deviceSizeActual = 0x040000;
    else                              deviceSizeActual = 0x010000;
    uint32_t numDevices = uint32_t(ram.size()) / deviceSizeActual;
    if (numDevices == 0) numDevices = 1;

    if (!m_cfg.useStrictRamMirror) {
        // Linear mirror — used by Siena (v4.20f) and PocketBook 2.
        // Plain `addr & (ram.size() - 1)`.
        uint32_t mask = uint32_t(ram.size()) - 1u;
        return int32_t(addr & mask);
    }

    // MAME-strict mirror: install_ram(addrstart, addrend, addrmirror) per device.
    uint32_t deviceSizeTest = deviceSize(asic9.ramType());

    for (uint32_t i = 0; i < numDevices; ++i) {
        uint32_t addrstart  = deviceSizeTest * i;
        uint32_t span       = std::min(deviceSizeActual, deviceSizeTest);
        uint32_t addrmirror = 0xFFFFFFu ^ ((deviceSizeTest * 4u) - 1u);
        if (deviceSizeActual < deviceSizeTest) {
            addrmirror ^= (deviceSizeTest - deviceSizeActual);
        }
        uint32_t base = addr & ~addrmirror;
        if (base >= addrstart && base < addrstart + span) {
            uint32_t offsetInDevice = base - addrstart;
            return int32_t(deviceSizeActual * i + offsetInDevice);
        }
    }
    return -1;
}

// Returns true if a write to ASIC9 internal `addr` under strict-mirror
// would alias back into the V30 IVT region (RAM offset 0..0x3FF on
// device 0). The caller (writeMemByte's page-select branch) drops such
// writes so the v3.40f 3a kernel's type=3 RAM probe doesn't silently
// clobber its own interrupt vector table while testing for a non-fitted
// upper RAM device. With the writes dropped, the read-back returns
// whatever IVT byte was already there — the kernel's probe sees its
// test pattern not echo back, concludes the address has no real RAM
// behind it, and downgrades A9Control bits 3-4 from type=3 (8 MiB) to
// type=2 (2 MiB), at which point its M: drive layout matches what the
// v5.20f 3c kernel does and the AGN/SPR/DAT/WRD/WLD subdirs get laid
// down on cold boot.
bool Emulator::writeAliasesIvtUnderMirror(uint32_t addr) const {
    // (Misleading name kept for the moment — used to encode "drop the
    //  write because mirror-folding would corrupt the IVT" + "drop the
    //  write because the address is beyond physical RAM". The first half
    //  has been removed — see series3c.cpp:writeMemByte comment dated
    //  May 2026 / tools/mame-vs-ours/FINDINGS.md for the reason. The
    //  second half — open-bus detection for non-strict-mirror models —
    //  still needs explicit dropping because translateRamAddr's
    //  non-strict path masks the address into ram.size() instead of
    //  returning -1.)
    if (ram.empty()) return false;
    if (m_cfg.model != Model::Series3a) return false;
    if (!m_cfg.useStrictRamMirror) {
        // Non-strict (linear-mirror) configs — currently PocketBook II
        // (1 MiB). Writes beyond installed RAM go to open bus.
        // translateRamAddr's non-strict path masks rather than dropping,
        // so we need to gate here.
        if (addr >= ram.size()) return true;
        return false;
    }
    // Strict-mirror models (series3a, 2 MiB): translateRamAddr already
    // returns -1 for any address that doesn't fall in some device's
    // install range under the current RamDeviceSize. So nothing
    // additional needs dropping at this layer — the open-bus signal is
    // carried out of translateRamAddr directly.
    return false;
}

uint8_t Emulator::readMemByte(uint32_t linear) {
    linear &= 0xFFFFF;
    // Unpaged RAM window
    if (linear < 0x60000) {
        uint8_t v = linear < ram.size() ? ram[linear] : 0xFF;
        return v;
    }
    // Unpaged ROM window: 0xA0000..0xFFFFF -> top 384 KiB of ROM image.
    if (linear >= 0xA0000) {
        // For a 1 MiB ROM (Siena) or 2 MiB ROM (3a/3c/3mx), the top
        // 384 KiB (0x60000 bytes) is what's visible here. The reset
        // vector always lives in the final 16 bytes of the image.
        uint32_t romOff = (uint32_t(rom.size()) - 0x60000) + (linear - 0xA0000);
        return rom[romOff];
    }
    // Paged windows at 0x60000/0x70000/0x80000/0x90000. ASIC9 holds an
    // 8-bit page-select for each window. The page index is a 64 KiB
    // bank in the ASIC9 24-bit internal address space: banks 0x00-0x7F
    // pick a RAM page, 0x80-0xFF pick a ROM page (0x80 = ROM offset 0,
    // 0xFF = ROM offset 0x7F0000). If the page lands outside the
    // installed RAM/ROM, return 0xFF.
    // segTop is the segment nibble 6..9. Using (linear >> 16) & 3 would
    // wrap 6/7/8/9 to 2/3/0/1 — a nasty bug that silently routes seg6000
    // writes through psel8000 and breaks the 3mx POST's RAM-size probe.
    uint32_t segTop = (linear >> 16) & 0xF;   // 6..9 for segments 6/7/8/9
    uint32_t off    = linear & 0xFFFF;
    uint8_t psel = 0;
    bool isRomSpace;
    switch (segTop) {
    case 6: psel = asic9.psel6000(); isRomSpace = false; break;
    case 7: psel = asic9.psel7000(); isRomSpace = false; break;
    // V30 segments 0x80000/0x90000 always dispatch to ROM space in
    // MAME's mem_r/mem_w (psion_asic9.cpp:544 — `switch (offset & 0x80000)`
    // routes byte-offset bit 19 to RAM_space vs ROM_space, BEFORE looking
    // at the psel value). Our previous decode picked RAM/ROM by psel
    // value (`bank < 0x800000`), so a 3a/3c kernel that wrote a low psel
    // value into psel8000/9000 (e.g. for a ROM-bank fetch using a base
    // of 0x40 plus an offset) ended up reading our `ram` buffer with
    // unrelated data. EPOC then trusted the wrong bytes and reported
    // "Media is corrupt" the first time the affected app touched M:.
    case 8: psel = asic9.psel8000(); isRomSpace = true; break;
    case 9: psel = asic9.psel9000(); isRomSpace = true; break;
    default: psel = 0; isRomSpace = false; break;
    }
    uint32_t bank = uint32_t(psel) << 16;
    if (!isRomSpace) {
        uint32_t internal = bank + off;
        int32_t off2 = translateRamAddr(internal);
        uint8_t v = off2 >= 0 ? ram[uint32_t(off2)] : 0xFF;
        return v;
    }
    // Segments 8/9 → ROM space. ROM is installed at internal address
    // 0x800000..0x800000+rom_size-1 with `addrmirror = 0x7fffff ^ (rom_size-1)`
    // (configure_rom in psion_asic9.cpp:478). For the SIBO2 ROM sizes we
    // ship — 1 MiB Siena, 2 MiB 3a/3c/3mx — the mirror lets every 1 or 2
    // MiB stride within the 8 MiB ROM window resolve into the image. A
    // psel < 0x80 lands BELOW the ROM install entirely (internal addr
    // < 0x800000), which is unmapped; return 0xFF.
    uint32_t addr = bank + off;
    if (addr < 0x800000) return 0xFF;
    uint32_t romOff = (addr - 0x800000) & uint32_t(rom.size() - 1);
    return rom[romOff];
}

uint16_t Emulator::readMemWord(uint32_t linear) {
    uint16_t lo = readMemByte(linear);
    uint16_t hi = readMemByte((linear + 1) & 0xFFFFF);
    return uint16_t(lo | (hi << 8));
}

void Emulator::writeMemByte(uint32_t linear, uint8_t v) {
    linear &= 0xFFFFF;
    // Count writes to VRAM (RAM[0x0400..0x5FFF] spans both planes for a
    // 480×160×2-plane framebuffer with A9WLcdSize=0x7257). Gated by an
    // env var so production builds pay nothing.
    if (std::getenv("PSION_VRAM_TRACE")) {
        static uint64_t vramP1 = 0;     // plane 1 (black) writes 0x400..0x2980
        static uint64_t vramP2 = 0;     // plane 2 (grey)  writes 0x2980..0x4f00
        static uint64_t vramHi = 0;     // 0x4f00..0x6000 (post-FB scratch)
        static uint64_t otherWrites = 0;
        if (linear >= 0x400 && linear < 0x2980)        vramP1++;
        else if (linear >= 0x2980 && linear < 0x4f00)  vramP2++;
        else if (linear >= 0x4f00 && linear < 0x6000)  vramHi++;
        else                                            otherWrites++;
        static uint64_t nextReport = 10'000'000;
        if (passedCycles > int64_t(nextReport)) {
            nextReport += 10'000'000;
            std::fprintf(stderr, "[vram] cycles=%lld p1=%llu p2=%llu hi=%llu others=%llu\n",
                         (long long)passedCycles,
                         (unsigned long long)vramP1,
                         (unsigned long long)vramP2,
                         (unsigned long long)vramHi,
                         (unsigned long long)otherWrites);
        }
    }
    // Write-watch (PSION_MEM_WATCH=<hexstart>,<hexend>[,<hexFromCycle>]):
    // log writes in a linear range with the writing PC — corruption
    // forensics. The optional third field suppresses logging until the
    // given cycle count so boot-time traffic doesn't exhaust the cap.
    {
        static uint32_t wStart = 0xFFFFFFFFu, wEnd = 0;
        static int64_t wFrom = 0;
        static bool parsed = false;
        if (!parsed) {
            parsed = true;
            if (const char *w = std::getenv("PSION_MEM_WATCH")) {
                unsigned a = 0, b = 0; unsigned long long f = 0;
                int got = std::sscanf(w, "%x,%x,%llx", &a, &b, &f);
                if (got >= 2) { wStart = a; wEnd = b; wFrom = int64_t(f); }
            }
        }
        if (linear >= wStart && linear <= wEnd && passedCycles >= wFrom) {
            static int n = 0;
            if (n++ < 4000)
                std::fprintf(stderr, "[memw] %05X <= %02X pc=%04X:%04X cyc=%lld\n",
                             linear, v, cpu.sregs[1], cpu.ip, (long long)passedCycles);
        }
    }
    // Unpaged RAM window
    if (linear < 0x60000) {
        if (linear < ram.size()) ram[linear] = v;
        // Write-watch: trace every write to [0x043c] (the idle-loop wake flag).
        // 0x4F3C is the same variable as seen from the kernel's DS=0x4b0
        // (the record-loop wake flag at ds:[0x43c]).
        if (std::getenv("PSION_WAKE_TRACE") && (linear == 0x043cu || linear == 0x4F3Cu)) {
            uint32_t pc = (uint32_t(cpu.sregs[1]) << 4) + cpu.ip;
            std::fprintf(stderr,
                "[wake] cyc=%lld pc=%05X [0x043c]<=%02x mask=%02x status=%02x\n",
                (long long)passedCycles, pc, v,
                asic9.interruptMask(), asic9.interruptStatus());
        }
        if (ramTraceOn()) {
            uint32_t pc = (uint32_t(cpu.sregs[1]) << 4) + cpu.ip;
            if (linear < ram.size()) {
                emitRamTrace('W', passedCycles, pc, linear, 'L', 0,
                             linear, int64_t(linear), nullptr, v);
            } else {
                emitRamTrace('W', passedCycles, pc, linear, 'L', 0,
                             linear, -1, "above-ram", v);
            }
        }
        // Synchronous M:-drive subdir injection. Fires the moment the
        // v3.40f cold-format function (PC 0xacaf2 helper) writes the
        // 12th byte of the volume-label entry — the 0x08 attribute byte
        // at ramOff = label_base + 11. By injecting at this exact point,
        // we beat any kernel directory cache: the kernel can't have
        // read M: drive between writing the label and our injection,
        // because both happen within a single CPU instruction window.
        if (!m_mDriveInjectAttempted &&
            v == 0x08 &&
            (linear & 0x1F) == 0x0B &&
            linear >= 0x40000 &&
            linear < ram.size() &&
            std::memcmp(ram.data() + (linear - 0x0B),
                        "RAMDRIVE   ", 11) == 0) {
            maybeInjectMDriveSubdirs();
        }
        return;
    }
    // Unpaged ROM window: writes dropped.
    if (linear >= 0xA0000) return;
    // Paged windows — same decode as readMemByte.
    uint32_t segTop = (linear >> 16) & 0xF;
    uint32_t off    = linear & 0xFFFF;
    uint8_t psel = 0;
    bool isRomSpace = false;
    switch (segTop) {
    // See readMemByte for why segments 6/7 are RAM-space and 8/9 are
    // ROM-space regardless of psel value — matches MAME's mem_w
    // (psion_asic9.cpp:558).
    case 6: psel = asic9.psel6000(); isRomSpace = false; break;
    case 7: psel = asic9.psel7000(); isRomSpace = false; break;
    case 8: psel = asic9.psel8000(); isRomSpace = true;  break;
    case 9: psel = asic9.psel9000(); isRomSpace = true;  break;
    default: break;
    }
    uint32_t bank = uint32_t(psel) << 16;
    if (!isRomSpace) {
        // Writes that land outside the kernel-selected RAM layout are
        // dropped (no addrmirror would catch them in MAME's
        // configure_ram, so the V30 sees them go to open bus).
        uint32_t internal = bank + off;
        // IVT-protect mirror writes (only matters under strict mirror —
        // see writeAliasesIvtUnderMirror's comment for why). On the
        // v3.40f 3a kernel's type=3 RAM probe through psel9000=0x40,
        // the V30 0x9000+off access lands at internal 0x400000+off
        // which under strict mirror folds back to RAM offset off — the
        // first 0x500 bytes of which are the IVT and kernel scratch.
        // Dropping the write makes the read-back differ from the test
        // pattern, the kernel concludes the upper device isn't fitted,
        // and downgrades A9Control bits 3-4 from type=3 to type=2 —
        // which matches the v5.20f 3c kernel's cold-boot path and lets
        // the AGN/SPR/DAT/WRD/WLD subdir tree get laid down on M:.
        // Drop writes that fall outside any of the asic9's install_ram
        // ranges for the current RamDeviceSize layout (open-bus). The
        // simple "addr >= ram.size()" check for non-strict mirror plus
        // translateRamAddr's "no matching device range" -1 return for
        // strict mirror together identify the unmapped case.
        //
        // CORRECTNESS NOTE (May 2026, after mame-vs-ours bisection — see
        // tools/mame-vs-ours/FINDINGS.md):  this branch used to ALSO drop
        // mirror-aliased writes that would corrupt the IVT during the
        // v3.40f kernel's type-3 RAM-size probe.  That extra drop was
        // the *cause* of our M: drive corruption: on real / MAME
        // hardware the probe's first attempt at psel=0x10 (ia=0x100000)
        // is *expected* to corrupt the IVT marker so the kernel falls
        // through to the psel=0x04 cascade — which is what sets the
        // probe iteration step BH to 0x10 (vs 0x40 when we dropped the
        // write and let the kernel take the wrong branch).  The whole
        // 8-hop chain back to the missing M:-drive subdirs resolves
        // once we let the mirror-aliased writes through.  No extra IVT
        // shielding needed: the kernel itself rewrites the IVT with
        // proper vectors once the probe phase exits and steady-state
        // init runs.
        bool ivtDrop = writeAliasesIvtUnderMirror(internal);
        int32_t off2 = ivtDrop ? -1 : translateRamAddr(internal);
        if (off2 >= 0) {
            ram[uint32_t(off2)] = v;
            // Write-watch: paged-window writes to [0x043c].
            if (std::getenv("PSION_WAKE_TRACE") && uint32_t(off2) == 0x043cu) {
                uint32_t pc = (uint32_t(cpu.sregs[1]) << 4) + cpu.ip;
                std::fprintf(stderr,
                    "[wake-pg] cyc=%lld pc=%05X [0x043c]<=%02x psel6=%02x ia=%05x mask=%02x status=%02x\n",
                    (long long)passedCycles, pc, v, psel, internal,
                    asic9.interruptMask(), asic9.interruptStatus());
            }
            // Track paged-window writes that land in VRAM (black or grey plane).
            if (std::getenv("PSION_VRAM_TRACE") && uint32_t(off2) >= 0x400u) {
                static uint64_t pagedP1 = 0, pagedP2 = 0, pagedHi = 0;
                if      (uint32_t(off2) < 0x2980u)  pagedP1++;
                else if (uint32_t(off2) < 0x4f00u)  pagedP2++;
                else if (uint32_t(off2) < 0x6000u)  pagedHi++;
                static int64_t nextPagedReport = 10'000'000;
                if (passedCycles > nextPagedReport) {
                    nextPagedReport += 10'000'000;
                    std::fprintf(stderr,
                        "[vram-pg] cyc=%lld paged_p1=%llu paged_p2=%llu paged_hi=%llu psel6=%02x psel7=%02x\n",
                        (long long)passedCycles,
                        (unsigned long long)pagedP1, (unsigned long long)pagedP2,
                        (unsigned long long)pagedHi,
                        (unsigned)asic9.psel6000(), (unsigned)asic9.psel7000());
                }
            }
            // Synchronous M:-drive subdir injection for paged-window writes.
            // The kernel writes the RAMDRIVE label through the seg-6/7 paged
            // window (V30 linear ~0x66660), so the unpaged-window check below
            // never fires for it. Firing here — within the single instruction
            // that commits the final label byte — guarantees we patch RAM
            // before EFSYS$M can read and cache the (otherwise-empty) root dir.
            if (!m_mDriveInjectAttempted &&
                v == 0x08 &&
                (uint32_t(off2) & 0x1Fu) == 0x0Bu &&
                uint32_t(off2) >= 0x40000u &&
                uint32_t(off2) < uint32_t(ram.size()) &&
                std::memcmp(ram.data() + (off2 - 0x0B), "RAMDRIVE   ", 11) == 0) {
                // Pass the exact label address so the injection targets the
                // real M: drive root dir (at off2-0x0B), not a scratch copy
                // that the scan might find at a lower RAM offset first.
                maybeInjectMDriveSubdirs(int32_t(off2) - 0x0B);
            }
        }
        if (ramTraceOn()) {
            uint32_t pc = (uint32_t(cpu.sregs[1]) << 4) + cpu.ip;
            const char *reason = ivtDrop ? "ivt-protect"
                                : (off2 < 0 ? "unmapped-ram" : nullptr);
            emitRamTrace('W', passedCycles, pc, linear,
                         char((segTop == 6) ? '6' : (segTop == 7) ? '7' :
                              (segTop == 8) ? '8' : '9'),
                         psel, internal, reason ? -1 : int64_t(off2), reason, v);
        }
        return;
    }
    // Segments 8/9 → ROM space, all writes silently dropped.
    if (ramTraceOn()) {
        uint32_t pc = (uint32_t(cpu.sregs[1]) << 4) + cpu.ip;
        emitRamTrace('W', passedCycles, pc, linear,
                     char((segTop == 8) ? '8' : '9'),
                     psel, bank + off, -1, "rom-space", v);
    }
}

void Emulator::writeMemWord(uint32_t linear, uint16_t v) {
    writeMemByte(linear, uint8_t(v & 0xFF));
    writeMemByte((linear + 1) & 0xFFFFF, uint8_t((v >> 8) & 0xFF));
}

// ──────────────────────────────────────────────────────────────────────
// V30Bus — I/O accesses
//
// MAME's psion_asic9_device::io_map installs a single 16-bit register
// window at 0x0000-0x00FF. All accesses are stubbed until
// core/psion_asic9.{h,cpp} lands.
// ──────────────────────────────────────────────────────────────────────

// Dev-only I/O tracing. Guarded by PSION_IO_DEBUG env var so we can
// leave the hook checked in without polluting release logs. Prints the
// port/value of every I/O touch plus the interrupt-mask state, which
// is the primary way to watch the ROM progressively unmask sources
// past the cold-boot minimum.
static bool ioDebugEnabled() {
    static int state = -1;
    if (state < 0) {
        const char *v = std::getenv("PSION_IO_DEBUG");
        state = (v && v[0] && v[0] != '0') ? 1 : 0;
    }
    return state != 0;
}

// V30 exposes a byte-addressable I/O space, but ASIC9's register file is
// 16-bit wide with many registers that split control info across the two
// byte lanes (e.g. InterruptMask lo / NmiClear hi at 0x08, TimerEoi lo /
// SerialSlaveEoi hi at 0x0c, PortCDDDR at 0x26). MAME's io_r/io_w route
// everything as word accesses with mem_mask selecting the active lane;
// we reproduce that here by promoting odd-port byte accesses to the
// correct lane rather than folding them onto the low byte of the even
// register — the previous behaviour silently swallowed all writes to
// odd ports (port 0x03 -> A9WControl hi byte, port 0x15 -> ProtectionOff
// hi, port 0x27 -> PortDDDR, etc.), which matters for the boot sequence.
//
// The Series 3c and Siena map a Condor UART at I/O 0x0100..0x011F per
// reference/mame-psion/psion/psion3a.cpp::psion3c_state. Without filtering
// these we'd route them through ASIC9 — and because ASIC9::ioRead masks
// the offset with 0xFE, ports 0x100..0x11F alias to ASIC9 registers
// 0x00..0x1E, corrupting the index/control file and causing the 3c kernel
// to wedge into a cold-boot loop.
//
// On the Siena we drive the real PsionCondor device (so the SSD pack
// on the Honda slot has correct modem-control / RXD plumbing for any
// future RS-232 carrier work). On the 3c/3mx — which also map Condor
// here but use it only for a comm-cable port that the goldens don't
// exercise — we fall back to the historic stub so the existing 3c
// kernel boot path stays bit-identical. Either way the kernel's UART
// probe sees the same byte pattern at reset (LSR=0x40, MSR=0xF0,
// Status=0x03) because PsionCondor::reset() seeds those values.
static bool portIsCondor(uint16_t port) {
    return port >= 0x0100 && port <= 0x011F;
}
// ASIC9MX (Series 3mx / Workabout MX) integrates two standard 16550
// UARTs on-die, mapped WORD-SPACED (register = (port - base) >> 1;
// odd bytes open bus): UART0 at I/O 0x40-0x4E and UART1 — the Remote
// Link port — at 0x50-0x5E. Layout pinned two ways: tracing the
// v6.16f link bring-up with PSION_IO_DEBUG (LCR 0x83 @0x56, divisor
// @0x50/0x52, FCR 0x87 @0x54, IER 0x0F @0x52, MCR toggles @0x58, LSR
// poll @0x5A) and the ROM's own driver at 0x1b3200-0x1b3a80, whose
// per-UART helpers select base 0x40 vs 0x50 off their device structs.
// The register shape matches the Condor's 16550 half, so PsionCondor
// instances serve both windows: m_condor doubles as UART1 and
// m_mxUart0 is UART0 (the v6.16f / v7.20f kernels never touch
// 0x100-0x11F). The Remote Link port differs per machine — the 3mx
// links through UART1, the Workabout MX through UART0 (its v7.20f
// link server runs the same bring-up at 0x40-0x4E and then polls LSR
// @0x4A) — hostUart() resolves which instance the host bridge binds.
// Windows deliberately stop at +0x0F: regs 8-15 of the Condor file
// are Condor-specific (reads of reg 8 pop the RX FIFO), so aliasing
// 0x60-0x6F onto them would corrupt the stream if the kernel ever
// probed up there.
PsionCondor *Emulator::mxUartAt(uint16_t port) {
    if (m_cfg.model != Model::Series3mx && m_cfg.model != Model::WorkaboutMX)
        return nullptr;
    if (port >= 0x0040 && port <= 0x004F) return &m_mxUart0;
    if (port >= 0x0050 && port <= 0x005F) return &m_condor;
    return nullptr;
}
static uint8_t condorStubRead(uint16_t port) {
    switch (port & 0x0F) {
    case 0x05: return 0x40; // UARTLineStatusRegister: TEMT (transmitter empty)
    case 0x06: return 0xF0; // UARTModemStatusRegister: CTS+DSR+RI+DCD all high
    case 0x0E: return 0x03; // StatusRegister: RXFE+TXFE (both FIFOs empty)
    default:   return 0x00;
    }
}
uint8_t Emulator::condorRead(uint16_t port) {
    // All SIBO2 models route to the real PsionCondor. The chip sits on
    // a 16-bit bus with its registers on the LOW byte lane of each
    // word — MAME installs it with lane mask 0x00ff (siena.cpp:186) —
    // so register index = (byte offset) >> 1 and odd byte addresses
    // are open bus. Decoding it byte-spaced (the previous `& 0x1F`)
    // made the v5.20f kernel's entire link bring-up land on the wrong
    // registers: its divisor writes leaked onto the wire as TX bytes
    // and its StatusRegister poll (raw 0x11C -> register 0x0E) read
    // dead space, so the Link-cable port never opened.
    if (port & 1) return 0;
    return m_condor.readReg(uint8_t((port >> 1) & 0x0F));
}
void Emulator::condorWrite(uint16_t port, uint8_t v) {
    if (port & 1) return;
    m_condor.writeReg(uint8_t((port >> 1) & 0x0F), v);
}

uint8_t  Emulator::readIoByte(uint16_t port) {
    if (portIsCondor(port)) {
        uint8_t b = condorRead(port);
        if (ioDebugEnabled())
            std::fprintf(stderr, "[IO] r8  condor port=%03X => %02X\n", port, b);
        return b;
    }
    if (PsionCondor *u = mxUartAt(port)) {
        uint8_t b = (port & 1) ? 0 : u->readReg(uint8_t((port & 0x0F) >> 1));
        if (ioDebugEnabled())
            std::fprintf(stderr, "[IO] r8  mxuart port=%03X => %02X\n", port, b);
        return b;
    }
    uint16_t mask = (port & 1) ? 0xFF00 : 0x00FF;
    uint16_t v = asic9.ioRead(port & ~1, mask);
    uint8_t b = uint8_t((port & 1) ? (v >> 8) : (v & 0xFF));
    if (ioDebugEnabled())
        std::fprintf(stderr, "[IO] r8  port=%03X => %02X\n", port, b);
    return b;
}
uint16_t Emulator::readIoWord(uint16_t port) {
    if (PsionCondor *u = mxUartAt(port)) {
        return u->readReg(uint8_t((port & 0x0F) >> 1));
    }
    if (portIsCondor(port)) {
        uint16_t v = uint16_t(condorRead(port)) | (uint16_t(condorRead(port + 1)) << 8);
        if (ioDebugEnabled())
            std::fprintf(stderr, "[IO] r16 condor port=%03X => %04X\n", port, v);
        return v;
    }
    uint16_t v = asic9.ioRead(port, 0xFFFF);
    if (ioDebugEnabled())
        std::fprintf(stderr, "[IO] r16 port=%03X => %04X\n", port, v);
    return v;
}
void     Emulator::writeIoByte(uint16_t port, uint8_t v) {
    if (portIsCondor(port)) {
        if (ioDebugEnabled())
            std::fprintf(stderr, "[IO] w8  condor port=%03X <= %02X\n", port, v);
        condorWrite(port, v);
        return;
    }
    if (PsionCondor *u = mxUartAt(port)) {
        if (ioDebugEnabled())
            std::fprintf(stderr, "[IO] w8  mxuart port=%03X <= %02X\n", port, v);
        if (!(port & 1)) u->writeReg(uint8_t((port & 0x0F) >> 1), v);
        return;
    }
    if (ioDebugEnabled())
        std::fprintf(stderr, "[IO] w8  port=%03X <= %02X\n", port, v);
    if (port & 1) {
        asic9.ioWrite(port & ~1, uint16_t(uint16_t(v) << 8), 0xFF00);
    } else {
        asic9.ioWrite(port, v, 0x00FF);
    }
}
void     Emulator::writeIoWord(uint16_t port, uint16_t v) {
    if (PsionCondor *u = mxUartAt(port)) {
        u->writeReg(uint8_t((port & 0x0F) >> 1), uint8_t(v & 0xFF));
        return;
    }
    if (portIsCondor(port)) {
        if (ioDebugEnabled())
            std::fprintf(stderr, "[IO] w16 condor port=%03X <= %04X\n", port, v);
        condorWrite(port,     uint8_t(v & 0xFF));
        condorWrite(port + 1, uint8_t((v >> 8) & 0xFF));
        return;
    }
    if (ioDebugEnabled())
        std::fprintf(stderr, "[IO] w16 port=%03X <= %04X\n", port, v);
    // PSION_SND_TRACE: identify the kernel code path that switches the
    // codec off (A9WControl bit 11 falling edge) — invaluable when
    // chasing record/playback aborts.
    if (port == 0x02 && (asic9.control() & 0x0800) && !(v & 0x0800)) {
        static const char *t = std::getenv("PSION_SND_TRACE");
        if (t && t[0] && t[0] != '0') {
            std::fprintf(stderr, "[snd] SoundEnable cleared by cs:ip=%04x:%04x (v=%04x) stack:",
                         cpu.sregs[1], cpu.ip, v);
            uint32_t ss = cpu.sregs[2], sp = cpu.regs.w[4];
            for (int i = 0; i < 16; i++) {
                uint32_t lin = ((ss << 4) + uint16_t(sp + i * 2)) & 0xFFFFF;
                std::fprintf(stderr, " %04x", readMemWord(lin));
            }
            std::fprintf(stderr, "\n");
        }
    }
    asic9.ioWrite(port, v, 0xFFFF);
}

uint8_t Emulator::ackInterrupt() {
    return asic9.inta();
}

// ──────────────────────────────────────────────────────────────────────
// LCD readout — Series 3c is 480x160, 2bpp grayscale driven by ASIC9's
// LCD controller. Until ASIC9 is ported, emit a blank "paper" field so
// the harness has a valid buffer to work with.
// ──────────────────────────────────────────────────────────────────────

void Emulator::readLCDIntoBuffer(uint8_t **lines, bool is32BitOutput) const {
    const int w = getLCDWidth();
    const int h = getLCDHeight();
    // ASIC9 drives a dual-plane greyscale panel. PsionAsic9::readLCD
    // returns per-pixel levels: 0 = paper, 1 = ink (black plane bit set),
    // 2 = grey (grey plane bit set but not black). Map each to its own
    // output colour so the three levels stay distinct in the harness
    // screenshot and the frontend canvas.
    std::vector<uint8_t> fb(size_t(w) * size_t(h), 0);
    asic9.readLCD(fb.data(), w, h);
    if (std::getenv("PSION_VRAM_TRACE")) {
        uint32_t planeSize = uint32_t(((asic9.lcdSizeReg() & 0x07FFu) + 1u) * 16u);
        if (planeSize == 16u) planeSize = uint32_t(((w + 7) / 8) * h);
        uint32_t fbBase = (uint32_t(asic9.psel6000()) << 16) + 0x0400u;
        uint32_t p1NonZero = 0, p2NonZero = 0;
        for (uint32_t i = 0; i < planeSize && (fbBase + i) < ram.size(); ++i) {
            if (ram[fbBase + i]) p1NonZero++;
            if ((fbBase + planeSize + i) < ram.size() && ram[fbBase + planeSize + i]) p2NonZero++;
        }
        // Dump direct-RAM framebuffer at 0x0400 (what readLCD actually reads).
        uint32_t dir_nz = 0;
        for (uint32_t i = 0; i < planeSize && (0x400u + i) < ram.size(); ++i)
            if (ram[0x400u + i]) dir_nz++;
        std::fprintf(stderr,
            "[vram-snap] cyc=%lld lcdSize=0x%04x planeBytes=%u "
            "p1_nz=%u p2_nz=%u dir_nz=%u psel6=%02x psel7=%02x ctrl=0x%04x\n",
            (long long)passedCycles, (unsigned)asic9.lcdSizeReg(),
            (unsigned)planeSize, (unsigned)p1NonZero, (unsigned)p2NonZero,
            (unsigned)dir_nz,
            (unsigned)asic9.psel6000(), (unsigned)asic9.psel7000(),
            (unsigned)asic9.control());
        // Hex-dump first 32 bytes of the direct framebuffer.
        std::fprintf(stderr, "[fb-raw] ram[0x400..0x41f] =");
        for (uint32_t i = 0; i < 32 && (0x400u + i) < ram.size(); ++i)
            std::fprintf(stderr, " %02x", ram[0x400u + i]);
        std::fprintf(stderr, "\n");
    }
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            uint8_t p = fb[size_t(y) * size_t(w) + size_t(x)];
            if (is32BitOutput) {
                auto line = reinterpret_cast<uint32_t *>(lines[y]);
                uint32_t c = 0xFFAAB4A0u; // paper
                if (p == 1)      c = 0xFF202828u; // ink
                else if (p == 2) c = 0xFF607068u; // grey
                line[x] = c;
            } else {
                // 8bpp output for the PGM harness: 0=black, 105=grey, 160=paper.
                uint8_t g = 160;
                if (p == 1)      g = 40;
                else if (p == 2) g = 105;
                lines[y][x] = g;
            }
        }
    }
}

void Emulator::setKeyboardKey(EpocKey key, bool value) {
    // Each entry is {col, bit}. -1 col => key not mapped (ignored).
    struct Slot { int col; uint16_t bit; };
    Slot s = { -1, 0 };

    // Siena uses a completely different matrix from the 3a/3c/3mx — see
    // reference/mame-psion/psion/siena.cpp:79-173. Most navigation +
    // editing keys (Esc, Enter, arrows, Backspace, Tab) live in
    // different (col, bit) slots than the larger 3a/3c keyboard, and
    // the 3a's COL0 row (which holds Enter/Tab/Right/Left/Down on the
    // 3a) holds the digits 7..0 + On/CE on the Siena. Reusing the 3a
    // matrix on Siena meant text keys produced wrong column hits and
    // navigation keys were silently dropped.
    if (m_cfg.model == Model::Siena) {
        switch (static_cast<int>(key)) {  // ASCII char-literal cases below are intentional
        // App row F-keys (wake-eligible per siena_state::wakeup).
        case EStdKeyF1: s = { 7, 0x200 }; break; // System
        case EStdKeyF2: s = { 6, 0x200 }; break; // Data
        case EStdKeyF3: s = { 1, 0x200 }; break; // Word
        case EStdKeyF4: s = { 2, 0x200 }; break; // Agenda
        case EStdKeyF5: s = { 3, 0x200 }; break; // Time
        case EStdKeyF6: s = { 4, 0x200 }; break; // World
        case EStdKeyF7: s = { 5, 0x200 }; break; // Calc
        case EStdKeyF8: s = { 5, 0x100 }; break; // Sheet
        case EStdKeyF10: s = { 2, 0x008 }; break; // Help
        case EStdKeyF11: s = { 7, 0x008 }; break; // Menu Info
        // Dedicated infrared keys (siena.cpp COL1/COL2 bit 0x100; MAME
        // binds them to PGUP/PGDN, mirrored here so the browser keymap
        // reaches them). IR Send is a wake key per MAME.
        case EStdKeyPageUp:   s = { 1, 0x100 }; break; // IR Send
        case EStdKeyPageDown: s = { 2, 0x100 }; break; // IR Receive
        // Editing & navigation.
        case EStdKeyEnter:      s = { 4, 0x020 }; break;
        case EStdKeyTab:        s = { 7, 0x001 }; break;
        case EStdKeyBackspace:  s = { 5, 0x020 }; break; // Del
        case EStdKeyEscape:     s = { 6, 0x100 }; break; // Esc (also wakes)
        case EStdKeyLeftArrow:  s = { 3, 0x008 }; break;
        case EStdKeyRightArrow: s = { 5, 0x008 }; break;
        case EStdKeyUpArrow:    s = { 4, 0x010 }; break;
        case EStdKeyDownArrow:  s = { 4, 0x008 }; break;
        case EStdKeyLeftShift:  s = { 3, 0x080 }; break;
        case EStdKeyRightShift: s = { 4, 0x080 }; break;
        case EStdKeyLeftCtrl:
        case EStdKeyRightCtrl:  s = { 7, 0x080 }; break;
        case EStdKeyLeftAlt:
        case EStdKeyRightAlt:   s = { 2, 0x080 }; break; // Psion
        case EStdKeyMenu:       s = { 7, 0x008 }; break;
        case EStdKeyHelp:       s = { 2, 0x008 }; break;
        case EStdKeyCapsLock:   s = { 6, 0x008 }; break;
        // Punctuation with dedicated EStdKey constants (>= 96).
        case ',':
        case EStdKeyComma:      s = { 2, 0x010 }; break;
        case '.':
        case EStdKeyFullStop:   s = { 3, 0x010 }; break;
        // Letters.
        case 'A':               s = { 7, 0x002 }; break;
        case 'B':               s = { 3, 0x004 }; break;
        case 'C':               s = { 1, 0x004 }; break;
        case 'D':               s = { 1, 0x002 }; break;
        case 'E':               s = { 2, 0x001 }; break;
        case 'F':               s = { 2, 0x002 }; break;
        case 'G':               s = { 3, 0x002 }; break;
        case 'H':               s = { 4, 0x002 }; break;
        case 'I':               s = { 6, 0x020 }; break;
        case 'J':               s = { 5, 0x002 }; break;
        case 'K':               s = { 7, 0x010 }; break;
        case 'L':               s = { 6, 0x010 }; break;
        case 'M':               s = { 5, 0x004 }; break;
        case 'N':               s = { 4, 0x004 }; break;
        case 'O':               s = { 1, 0x020 }; break;
        case 'P':               s = { 2, 0x020 }; break;
        case 'Q':               s = { 6, 0x001 }; break;
        case 'R':               s = { 3, 0x001 }; break;
        case 'S':               s = { 6, 0x002 }; break;
        case 'T':               s = { 4, 0x001 }; break;
        case 'U':               s = { 7, 0x020 }; break;
        case 'V':               s = { 2, 0x004 }; break;
        case 'W':               s = { 1, 0x001 }; break;
        case 'X':               s = { 6, 0x004 }; break;
        case 'Y':               s = { 5, 0x001 }; break;
        case 'Z':               s = { 7, 0x004 }; break;
        // Digits — all live in COL0 on the Siena.
        case '0':               s = { 0, 0x080 }; break;
        case '1':               s = { 0, 0x040 }; break;
        case '2':               s = { 0, 0x020 }; break;
        case '3':               s = { 0, 0x010 }; break;
        case '4':               s = { 0, 0x008 }; break;
        case '5':               s = { 0, 0x004 }; break;
        case '6':               s = { 0, 0x002 }; break;
        case '7':               s = { 0, 0x001 }; break;
        case '8':               s = { 1, 0x040 }; break;
        case '9':               s = { 2, 0x040 }; break;
        case ' ':
        case EStdKeySpace:      s = { 1, 0x008 }; break;
        case ';':
        case ':':               s = { 1, 0x010 }; break; // KEYCODE_COLON PORT_CHAR(';'/':')
        case '/':
        case '?':               s = { 5, 0x010 }; break;
        case '-':
        case '_':               s = { 3, 0x020 }; break;
        case '=':               s = { 3, 0x040 }; break;
        case '+':               s = { 4, 0x040 }; break; // dedicated KEYCODE_PLUS_PAD
        case '*':               s = { 5, 0x040 }; break;
        default: break;
        }
        if (s.col >= 0) {
            if (value) m_key_row[s.col] |=  s.bit;
            else       m_key_row[s.col] &= ~s.bit;
        }
        bool wakeKey = false;
        switch (static_cast<int>(key)) {  // ASCII char-literal cases below are intentional
        case EStdKeyF1: case EStdKeyF2: case EStdKeyF3: case EStdKeyF4:
        case EStdKeyF5: case EStdKeyF6: case EStdKeyF7: case EStdKeyF8:
        case EStdKeyPageUp:  // IR Send carries MAME's wakeup hook
        case EStdKeyEscape:
            wakeKey = true;
            break;
        default: break;
        }
        if (wakeKey) asic9.setEint0(value);
        return;
    }

    // Workabout / WorkaboutMX share the same industrial-handheld key
    // matrix (8 columns, bits 0x001..0x100) per MAME's
    // reference/mame-psion/psion/workabout.cpp INPUT_PORTS_START. Only
    // the On/Esc key wakes the machine via eint0_w (no F-key wakeup —
    // there's just F11=Menu and the dedicated On/Off + Contrast +
    // Backlight membrane buttons). Three keys on COL6 / COL7 (Off,
    // Contrast, Backlight) have no matching EStdKey constant; we wire
    // Off → EStdKeyOff (sleep) but leave Contrast / Backlight unmapped
    // since the host browser has no analogue for adjusting hardware
    // contrast or backlight on a virtual device.
    if (m_cfg.model == Model::Workabout ||
        m_cfg.model == Model::WorkaboutMX) {
        switch (static_cast<int>(key)) {  // ASCII char-literal cases below are intentional
        // COL0
        case 'S':                s = { 0, 0x001 }; break;
        case 'U':                s = { 0, 0x002 }; break;
        case 'Q':                s = { 0, 0x004 }; break;
        case 'A':                s = { 0, 0x008 }; break;
        case '/':
        case '?':                s = { 0, 0x010 }; break;
        case '6':                s = { 0, 0x020 }; break;
        case EStdKeyDownArrow:   s = { 0, 0x040 }; break;
        case EStdKeyEscape:      s = { 0, 0x100 }; break; // On/Esc (wakes)
        // COL1
        case 'T':                s = { 1, 0x001 }; break;
        case 'V':                s = { 1, 0x002 }; break;
        case 'R':                s = { 1, 0x004 }; break;
        case 'B':                s = { 1, 0x008 }; break;
        case EStdKeyEnter:       s = { 1, 0x010 }; break;
        case '-':
        case '_':                s = { 1, 0x020 }; break;
        case EStdKeyRightArrow:  s = { 1, 0x040 }; break;
        // COL2
        case EStdKeyLeftShift:
        case EStdKeyRightShift:  s = { 2, 0x001 }; break;
        case 'W':                s = { 2, 0x002 }; break;
        case 'G':                s = { 2, 0x004 }; break;
        case 'C':                s = { 2, 0x008 }; break;
        case '1':                s = { 2, 0x010 }; break;
        case '7':                s = { 2, 0x020 }; break;
        case EStdKeyBackspace:   s = { 2, 0x040 }; break; // Del
        // COL3
        case EStdKeyLeftAlt:
        case EStdKeyRightAlt:    s = { 3, 0x001 }; break; // Psion
        case 'X':                s = { 3, 0x002 }; break;
        case 'H':                s = { 3, 0x004 }; break;
        case 'D':                s = { 3, 0x008 }; break;
        case '2':                s = { 3, 0x010 }; break;
        case '8':                s = { 3, 0x020 }; break;
        case EStdKeyTab:         s = { 3, 0x040 }; break;
        // COL4
        case 'Y':                s = { 4, 0x001 }; break;
        case 'M':                s = { 4, 0x002 }; break;
        case 'I':                s = { 4, 0x004 }; break;
        case 'E':                s = { 4, 0x008 }; break;
        case '3':                s = { 4, 0x010 }; break;
        case '9':                s = { 4, 0x020 }; break;
        case EStdKeyUpArrow:     s = { 4, 0x040 }; break;
        // COL5
        case 'Z':                s = { 5, 0x001 }; break;
        case 'N':                s = { 5, 0x002 }; break;
        case 'J':                s = { 5, 0x004 }; break;
        case 'F':                s = { 5, 0x008 }; break;
        case '+':
        case '=':                s = { 5, 0x010 }; break;
        case '*':
        case ':':                s = { 5, 0x020 }; break;
        case EStdKeyF11:
        case EStdKeyMenu:        s = { 5, 0x040 }; break; // Menu
        // COL6
        case EStdKeyLeftCtrl:
        case EStdKeyRightCtrl:   s = { 6, 0x001 }; break;
        case 'O':                s = { 6, 0x002 }; break;
        case 'K':                s = { 6, 0x004 }; break;
        case '0':
        case ';':
        case '<':                s = { 6, 0x008 }; break;
        case '4':
        case '$':                s = { 6, 0x010 }; break;
        case EStdKeyOff:         s = { 6, 0x020 }; break; // Off
        // COL7
        case ' ':
        case EStdKeySpace:       s = { 7, 0x001 }; break;
        case 'P':                s = { 7, 0x002 }; break;
        case 'L':                s = { 7, 0x004 }; break;
        case '.':
        case ',':                s = { 7, 0x008 }; break;
        case '5':
        case '%':                s = { 7, 0x010 }; break;
        case EStdKeyLeftArrow:   s = { 7, 0x020 }; break;
        default: break;
        }
        if (s.col >= 0) {
            if (value) m_key_row[s.col] |=  s.bit;
            else       m_key_row[s.col] &= ~s.bit;
        }
        if (key == EStdKeyEscape) asic9.setEint0(value);
        return;
    }

    // Diagnostic raw-matrix injection: EpocKey codes 200..287 map to
    // (col = (code-200)/11, bit = 1 << ((code-200)%11)). Used by key
    // sweeps to probe matrix positions that have no EStdKey mapping
    // (the 3c re-used 3a-unused positions for its new keys — Jotter
    // was found this way). No frontend path ever sends these codes.
    if (static_cast<int>(key) >= 200 && static_cast<int>(key) < 200 + 88) {
        int idx = static_cast<int>(key) - 200;
        int col = idx / 11;
        uint16_t bit = uint16_t(1u << (idx % 11));
        if (value) m_key_row[col] |=  bit;
        else       m_key_row[col] &= ~bit;
        return;
    }

    // Series 3a/3c/3mx (English layout, from MAME's
    // reference/mame-psion/psion/psion3a.cpp COL0..COL7 PORT_BIT map).
    switch (static_cast<int>(key)) {  // ASCII char-literal cases below are intentional
    // App row (F-keys wake the machine per MAME's wakeup INPUT_CHANGED).
    case EStdKeyF1: s = { 1, 0x400 }; break; // System
    case EStdKeyF2: s = { 0, 0x400 }; break; // Data
    case EStdKeyF3: s = { 2, 0x400 }; break; // Word
    case EStdKeyF4: s = { 1, 0x200 }; break; // Agenda
    case EStdKeyF5: s = { 0, 0x200 }; break; // Time
    case EStdKeyF6: s = { 2, 0x200 }; break; // World
    case EStdKeyF7: s = { 1, 0x100 }; break; // Calc
    case EStdKeyF8: s = { 0, 0x100 }; break; // Sheet
    case EStdKeyF9: s = { 2, 0x100 }; break; // Jotter (3c/3mx; IPT_UNUSED on 3a)
    // F10/F11 alias to Help / Menu — MAME's psion3a INPUT_PORTS_START
    // wires F10 to "Help Dial" (COL3 bit 0x004) and F11 to "Menu" (COL5
    // bit 0x080). The browser keymap (frontend/src/lib/keymap.ts) sends
    // the F-key as its EpocKey value, so without this alias a desktop
    // user pressing F10/F11 would have their key silently dropped. The
    // EStdKeyHelp/EStdKeyMenu cases below handle the on-screen Help /
    // Menu buttons (and any frontend that sends those codes directly).
    case EStdKeyF10: s = { 3, 0x004 }; break; // Help Dial
    case EStdKeyF11: s = { 5, 0x080 }; break; // Menu
    // Editing & navigation.
    case EStdKeyEnter:     s = { 0, 0x001 }; break;
    case EStdKeyTab:       s = { 0, 0x004 }; break;
    case EStdKeyBackspace: s = { 2, 0x001 }; break;
    case EStdKeyEscape:    s = { 7, 0x100 }; break; // Esc-On (wakes too)
    case EStdKeyLeftArrow:  s = { 0, 0x010 }; break;
    case EStdKeyRightArrow: s = { 0, 0x002 }; break;
    case EStdKeyUpArrow:    s = { 7, 0x020 }; break;
    case EStdKeyDownArrow:  s = { 0, 0x020 }; break;
    case EStdKeyLeftShift:  s = { 1, 0x080 }; break;
    case EStdKeyRightShift: s = { 3, 0x080 }; break;
    case EStdKeyLeftCtrl:
    case EStdKeyRightCtrl:  s = { 2, 0x080 }; break;
    case EStdKeyLeftAlt:
    case EStdKeyRightAlt:   s = { 0, 0x080 }; break; // Psion key
    case EStdKeyMenu:       s = { 5, 0x080 }; break;
    case EStdKeyHelp:       s = { 3, 0x004 }; break;
    case EStdKeyCapsLock:   s = { 4, 0x080 }; break;
    // Punctuation that has a dedicated EStdKey constant (>= 96 so they
    // collide with letter ASCII codes — must be matched before the
    // char-literal arms). Comma=121, FullStop=122 from core/emubase.h.
    // We also accept the bare ASCII codes (',' = 44, '.' = 46) so that
    // any path that synthesises "type this character" by sending the
    // ASCII value (rather than the EStdKey constant) still produces the
    // expected matrix bit. The frontend's charToEpocChord already routes
    // these through EStdKeyComma / EStdKeyFullStop, but other callers
    // (and the harness's per-key sweep) may use the raw value.
    case ',':
    case EStdKeyComma:      s = { 3, 0x002 }; break;
    case '.':
    case EStdKeyFullStop:   s = { 7, 0x010 }; break;
    // Letter / digit / symbol keys. The frontend's charToEpocChord
    // (frontend/src/lib/keymap.ts) sends letters as their UPPER-CASE
    // ASCII code so the values here don't collide with EStdKeyF1..F12
    // (96-107), and digits / matrix-row punctuation arrive as their
    // raw ASCII. (col, bit) pairs are taken straight from MAME's
    // psion3a INPUT_PORTS_START — see reference/mame-psion/psion/
    // psion3a.cpp lines 191-278. The matrix is identical on 3a / 3c /
    // 3mx / Siena.
    case 'A':               s = { 6, 0x004 }; break;
    case 'B':               s = { 4, 0x040 }; break;
    case 'C':               s = { 5, 0x008 }; break;
    case 'D':               s = { 5, 0x010 }; break;
    case 'E':               s = { 5, 0x020 }; break;
    case 'F':               s = { 5, 0x002 }; break;
    case 'G':               s = { 4, 0x020 }; break;
    case 'H':               s = { 7, 0x040 }; break;
    case 'I':               s = { 2, 0x004 }; break;
    case 'J':               s = { 3, 0x010 }; break;
    case 'K':               s = { 2, 0x002 }; break;
    case 'L':               s = { 2, 0x040 }; break;
    case 'M':               s = { 3, 0x008 }; break;
    case 'N':               s = { 0, 0x040 }; break;
    case 'O':               s = { 2, 0x020 }; break;
    case 'P':               s = { 1, 0x020 }; break;
    case 'Q':               s = { 6, 0x002 }; break;
    case 'R':               s = { 4, 0x002 }; break;
    case 'S':               s = { 6, 0x010 }; break;
    case 'T':               s = { 4, 0x010 }; break;
    case 'U':               s = { 3, 0x020 }; break;
    case 'V':               s = { 5, 0x004 }; break;
    case 'W':               s = { 6, 0x020 }; break;
    case 'X':               s = { 6, 0x040 }; break;
    case 'Y':               s = { 0, 0x008 }; break;
    case 'Z':               s = { 6, 0x008 }; break;
    case '0':               s = { 1, 0x010 }; break;
    case '1':               s = { 7, 0x002 }; break;
    case '2':               s = { 7, 0x004 }; break;
    case '3':               s = { 5, 0x040 }; break;
    case '4':               s = { 4, 0x004 }; break;
    case '5':               s = { 4, 0x008 }; break;
    case '6':               s = { 7, 0x008 }; break;
    case '7':               s = { 3, 0x040 }; break;
    case '8':               s = { 2, 0x008 }; break;
    case '9':               s = { 2, 0x010 }; break;
    case ' ':
    case EStdKeySpace:      s = { 4, 0x001 }; break;
    case '/':               s = { 1, 0x002 }; break;
    case '-':               s = { 1, 0x004 }; break;
    case '=':
    case '+':               s = { 1, 0x008 }; break;
    case '*':
    case ':':               s = { 1, 0x040 }; break;
    default: return;
    }
    if (s.col < 0) return;

    if (value) m_key_row[s.col] |=  s.bit;
    else       m_key_row[s.col] &= ~s.bit;

    // EINT0 wakeup: MAME's psion3a_base_state::wakeup is bound to the
    // app-row keys (F1-F8 on 3a, F1-F9 on 3c/3mx) and Esc/On — see
    // PORT_CHANGED_MEMBER in reference/mame-psion/psion/psion3a.cpp. Other
    // keys go through the COL strobe / kbd_r path and trigger A9MKeyboard
    // (Status bit 7) on the next strobe; they do NOT raise EINT0. We
    // previously raised eint0 on every key press, which kept the line
    // asserted whenever a text key was held down and forced the kernel
    // to take a spurious ExpIntC every instruction boundary. Restricting
    // the eint0 pulse to wake-eligible keys matches MAME and avoids the
    // spurious-IRQ path during normal typing.
    bool wakeKey = false;
    switch (static_cast<int>(key)) {  // ASCII char-literal cases below are intentional
    case EStdKeyF1:
    case EStdKeyF2:
    case EStdKeyF3:
    case EStdKeyF4:
    case EStdKeyF5:
    case EStdKeyF6:
    case EStdKeyF7:
    case EStdKeyF8:
    case EStdKeyF9: // Jotter (3c/3mx)
    case EStdKeyEscape:
        wakeKey = true;
        break;
    default:
        break;
    }
    if (wakeKey) asic9.setEint0(value);
}

void Emulator::updateTouchInput(int32_t /*x*/, int32_t /*y*/, bool /*down*/) {
    // Series 3c has no digitiser.
}

// ── SSD pack hooks ────────────────────────────────────────────────────

bool Emulator::attachSSD(int slot, const uint8_t *bytes, size_t size,
                         int ssdType) {
    if (slot < 0 || slot >= int(m_ssd.size())) return false;
    return m_ssd[slot].attach(bytes, size, static_cast<PsionSSD::Type>(ssdType));
}

void Emulator::detachSSD(int slot) {
    if (slot < 0 || slot >= int(m_ssd.size())) return;
    m_ssd[slot].detach();
}

bool Emulator::isSSDInserted(int slot) const {
    if (slot < 0 || slot >= int(m_ssd.size())) return false;
    return m_ssd[slot].isInserted();
}

size_t Emulator::getSSDImageSize(int slot) const {
    if (slot < 0 || slot >= int(m_ssd.size())) return 0;
    return m_ssd[slot].imageSize();
}

const uint8_t *Emulator::getSSDImageData(int slot) const {
    if (slot < 0 || slot >= int(m_ssd.size())) return nullptr;
    return m_ssd[slot].imageData();
}

} // namespace Series3c
