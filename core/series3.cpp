// license:BSD-3-Clause
// copyright-holders:Nigel Barnes (MAME psion3 driver, address + I/O maps)
//                  + adaptation by the Psion emulator project, 2026.
//
// Series 3 driver implementation. See series3.h for the high-level
// memory map and the MAME source it is derived from.

#include "series3.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace Series3 {

// Diagnostic: one-shot tracing of writes to kernel state. Enabled by
// setting PSION3_TRACE_KERNEL=1 in the environment.
static bool traceKernelState() {
    static int cached = -1;
    if (cached < 0) {
        const char *e = std::getenv("PSION3_TRACE_KERNEL");
        cached = (e && *e && *e != '0') ? 1 : 0;
    }
    return cached != 0;
}

Emulator::Emulator(const Config &cfg)
    : m_cfg(cfg),
      cpu(*this, cfg.cpuVariant),
      ram(cfg.ramSize, cfg.ramFillByte),
      rom(cfg.romSize, 0xFF),
      vram(cfg.vramSize, 0),
      m_ssd(cfg.ssdSlots) {
    asic1.setBusClock(m_cfg.busClockHz);
    asic2.setBusClock(m_cfg.busClockHz);
    asic1.setLaptopMode(m_cfg.laptopMode);
    asic1.setLcdId(m_cfg.lcdId);
    wireChips();
    audio.configure(m_cfg.busClockHz);
    // The ASIC2 BuzzerToggle bit is held longer than CL-PS711x's BZTOG
    // sub-tick pulses but shorter than Windermere's TC1 envelope — 2
    // TINT periods is the sweet spot for the SIBO key-click envelope.
    audio.buzzerClickHoldTicks = 2;
}

void Emulator::wireChips() {
    // ASIC1 INT/NMI drive the V30. The V30 stub doesn't yet have
    // setIrqLine / setNmiLine pipework — they just latch a flag and we
    // currently never service it (the CPU stub isn't far enough along
    // to dispatch interrupts). Wiring them now means once the CPU
    // gains an INT-ack path no driver-side change is needed.
    asic1.setIrqOutCb([this](bool s) { cpu.setIrqLine(s); });
    asic1.setNmiOutCb([this](bool s) { cpu.setNmiLine(s); });

    // ASIC1's FRC overflow line drives ASIC2 (used for buzzer in
    // BuzzerMode == 1).
    asic1.setFrcOvlCb([this](bool s) { asic2.frcOvlIn(s); });

    // ASIC2 piezo buzzer pin → AudioCodecModel. We just track the
    // current pin level and latch a click envelope on the rising edge;
    // the square-wave pump in executeUntil turns that into audible
    // samples (matches the Windermere pattern in
    // core/windermere.cpp:1316).
    asic2.setBuzCb([this](bool s) {
        if (s && !m_buzzerLevel) audio.noteBuzzerRisingEdge();
        m_buzzerLevel = s;
    });

    // ASIC2 outputs INT/NMI back into ASIC1 (chained PIC).
    asic2.setIrqOutCb([this](bool s) { asic1.setEint3(s); });
    asic2.setNmiOutCb([this](bool s) { asic1.setEnmi(s); });

    // The V30 LCD scanout reads VRAM directly out of the host bus. For
    // Series 3 the framebuffer lives in main RAM at offset 0x400; for
    // MC400 in laptop mode ASIC1 fetches from the dedicated 0xB8000
    // VRAM window. Both routes go through busRead() so the dispatcher
    // is the single source of truth.
    asic1.setMemReader([this](uint32_t addr) -> uint8_t {
        return busRead(addr);
    });

    // Battery sense (slow-port byte that the cold-boot dialog reads
    // to decide which battery icons to render and whether to suppress
    // the "main battery is low" warning). Bits per MAME's
    // psion3_state::port_data_r comment block:
    //   b0 MainBattery   - 0 Low, 1 Good
    //   b1 BackupBattery - 0 Low, 1 Good
    //   b3 ExternalPower - 0 Yes, 1 No
    // Two healthy cells with no mains adapter ⇒ 0x0B (b0 | b1 | b3).
    // MAME's source returns the literal 0x03 there which is inconsistent
    // with its own bit-3 comment (0=external-power-present); the v1.91f
    // kernel reads this byte during cold boot, so we honour the comment
    // rather than the literal.
    asic2.setReadPortDataCb([]() -> uint8_t { return 0x0B; });
    asic2.setWritePortDataCb([](uint8_t /*data*/) {});

    // Keyboard column scan returns "no keys pressed" until a future
    // session wires real key state in. Returning 0 matches MAME's
    // behaviour with no input ports asserted.
    asic2.setKeyboardColumnCb([this](int col) -> uint8_t {
        // MAME's psion_asic2 col_cb is bound to m_keyboard[data]->read()
        // for column 0..9. KeyScan=0 calls col_cb(-1); the real bus
        // pull-ups read 0xFF in that deselected state, matching MAME's
        // `m_col_cb(*this, 0xff)` devcb default.
        // PSION3_TRACE_KB=1 — every scan that sees a key down, plus one
        // in every 500 otherwise so an idle machine still shows it is
        // scanning. This is how the HC120's matrix was mapped: with its
        // shell up, press a slot and watch which one the ROM reads back
        // (see core/series3.cpp::hc120KeyMatrix).
        if (std::getenv("PSION3_TRACE_KB")) {
            uint8_t v = (col >= 0 && col < 10) ? m_key_row[col] : 0xFF;
            static long n = 0;
            if ((n++ % 500) == 0 || v != 0x00)
                std::fprintf(stderr, "[kb] t=%.2fs scan #%ld col=%d -> %02x\n",
                             double(passedCycles) / m_cfg.busClockHz, n, col, v);
        }
        if (col < 0 || col >= 10) return 0xFF;
        return m_key_row[col];
    });

    // ASIC2 SIBO channel 0 — laptop PSU (PS34 / ASIC3 or ASIC5). The
    // MC400 boot ROM polls this for the PowerFail bit before reaching
    // LCD-init; without it the kernel hangs in early boot. Series 3
    // doesn't have a PS34 so leaves channel 0 unwired.
    if (m_cfg.laptopMode) {
        asic2.setChannelReadCb (0, [this]() { return asic3.readFrame(); });
        asic2.setChannelWriteCb(0, [this](uint16_t f) { asic3.writeFrame(f); });
        // A2Control2 bit 1 (XySwitch) toggles which trackpad axis the
        // ASIC3 ADC multiplexer presents — 0 = X, 1 = Y. ASIC2 calls
        // dr_cb on every A2Control2 write; route it to ASIC3.
        asic2.setDigitiserDirCb([this](bool s) { asic3.setDigitiserDirection(s ? 1 : 0); });
    }

    // SSD packs: ASIC2 channel 1 = Pack A, channel 2 = Pack B.
    // Matches MAME reference/mame-psion/psion/psion3.cpp:312-315. The
    // MC400 wires four packs onto channels 1-4 (ASIC2 data_r/w<1..4>);
    // the system disk lives on Pack 3 (channel 3). Iterate over the
    // configured slots so each device's SSD count is honoured.
    for (size_t i = 0; i < m_ssd.size() && i < 4; ++i) {
        int channel = int(i) + 1;
        asic2.setChannelReadCb (channel, [this, i]() { return m_ssd[i].readFrame(); });
        asic2.setChannelWriteCb(channel, [this, i](uint16_t f) { m_ssd[i].writeFrame(f); });
    }
    // Door callback: each SSD pulses the ASIC2 door NMI on attach/detach
    // for ~200 ms so EPOC16 re-scans the slot. We arm a host-cycle
    // countdown which executeUntil() decrements.
    auto doorPulse = [this](bool s) {
        if (!s) return;
        if (passedCycles == 0) {
            // Cold attach happens before the kernel has installed its
            // NMI vector — pulsing now would dispatch through the
            // uninitialised IVT[2] and wild-jump. Defer until after
            // the kernel's IVT setup completes (~8 M cycles for the
            // MC400 V1.26F).
            m_pendingColdDoorAt = m_cfg.busClockHz * 2;  // ~2 s sim time
            return;
        }
        asic2.setDoorNmi(true);
        // ~200 ms at the V30 bus clock.
        m_doorNmiClearAt = passedCycles + (m_cfg.busClockHz / 5);
    };
    for (auto &slot : m_ssd) slot.setDoorCb(doorPulse);
}

void Emulator::loadROM(uint8_t *buffer, size_t size) {
    std::memset(rom.data(), 0xFF, rom.size());
    std::memcpy(rom.data(), buffer, std::min(size, rom.size()));

    if (!initialised) {
        cpu.reset();
        asic1.reset();
        asic2.reset();
        asic3.reset();
        // Schedule the auto-wake pulse (see series3.h for context). The
        // v1.91f kernel polls A2Status at c000:7578 looking for A1OnKey
        // ~5.7 M cycles after reset (just after the first idle entry);
        // we therefore assert OnClr at 4 M cycles and HOLD it through
        // 7 M cycles so the bit is still set when the poll occurs. A
        // shorter pulse gets eaten by the EINT3 ISR before the main
        // loop sees it. The boot then proceeds straight to the app row
        // — no battery / time dialog, exactly as if the user had tapped
        // the ESC/On key once.
        m_autoWakeAssertAt  = 4'000'000;
        m_autoWakeReleaseAt = 7'000'000;
        m_autoWakeFired     = false;
        initialised = true;
    }
}

void Emulator::executeUntil(int64_t cycles) {
    // Interleave CPU stepping with ASIC ticks. The V30 stub returns the
    // cycles consumed per step; we feed that to the ASICs so timers
    // advance at the right rate.
    const bool trace = traceKernelState();
    static uint32_t last_pc = 0;
    static uint64_t pc_changes = 0;
    // Don't gate on !cpu.halted — a HLT waits for IRQ, but the IRQ can
    // only fire if we keep ticking the ASICs. V30::step() returns a
    // small cycle count while halted so the loop still makes progress.
    while (passedCycles < cycles) {
        uint32_t pc = (uint32_t(cpu.sregs[1]) << 4) + cpu.ip;
        if (trace) {
            // First-time PC landmark tracing: record the first visit to
            // key addresses.
            static bool seen[32] = {};
            auto mark = [&](int i, uint32_t target) {
                if (!seen[i] && pc == target) {
                    std::fprintf(stderr, "[pc] first hit pc=%05x cycles=%lld\n",
                                 target, (long long)passedCycles);
                    seen[i] = true;
                }
            };
            mark(0, 0xce780);
            mark(1, 0xce808);
            mark(2, 0xce810);
            mark(3, 0xce840);
            mark(4, 0xce900);
            mark(5, 0xceac2);
            mark(6, 0xceac1);  // HLT
            mark(7, 0xc58d5);  // TINT ISR
            mark(8, 0xce860);
            mark(9, 0xce880);
            mark(10, 0xce8a0);
            mark(11, 0xce8c0);
            mark(12, 0xce8e0);
            mark(13, 0xce8cc);   // IVT fill start
            mark(14, 0xce8dc);   // IVT fill end
            mark(15, 0xd56c9);   // jmp past init
            // Periodic dump: when we're past 0xce840, print PC every 10M cycles
            static int64_t next_dump = 8000000;
            if (passedCycles > next_dump) {
                std::fprintf(stderr, "[pc] dump pc=%05x cycles=%lld\n",
                             pc, (long long)passedCycles);
                next_dump += 2000000;
            }
            // Histogram of PCs (first 32 distinct) over last 20M cycles.
            static uint32_t hist_pc[64] = {};
            static uint64_t hist_cnt[64] = {};
            static int hist_n = 0;
            static int64_t hist_next = 3000000;  // start after boot
            static int64_t hist_report = 7000000;
            if (passedCycles > hist_next) {
                bool f = false;
                for (int i = 0; i < hist_n; i++) if (hist_pc[i] == pc) { hist_cnt[i]++; f = true; break; }
                if (!f && hist_n < 64) { hist_pc[hist_n] = pc; hist_cnt[hist_n] = 1; hist_n++; }
                hist_next += 200;
            }
            if (passedCycles > hist_report) {
                hist_report += 5000000;
                // Print top 10
                int order[64];
                for (int i = 0; i < hist_n; i++) order[i] = i;
                for (int i = 0; i < hist_n; i++)
                    for (int j = i+1; j < hist_n; j++)
                        if (hist_cnt[order[j]] > hist_cnt[order[i]]) std::swap(order[i], order[j]);
                std::fprintf(stderr, "[hist] cycle=%lld top:", (long long)passedCycles);
                for (int i = 0; i < hist_n && i < 10; i++)
                    std::fprintf(stderr, " %05x:%llu", hist_pc[order[i]], (unsigned long long)hist_cnt[order[i]]);
                std::fprintf(stderr, "\n");
            }
            // Explicit PC histogram — sample every N steps.
            static int64_t pc_counters[16] = {};
            static uint32_t pc_bins[16] = {};
            static int pc_next_bin = 0;
            static int64_t pc_sample_stride = 0;
            static int64_t pc_sample_next = 200000;
            if (passedCycles > pc_sample_next) {
                pc_sample_next += 200000;
                // Find or add bin
                bool found = false;
                for (int i = 0; i < pc_next_bin; i++) {
                    if (pc_bins[i] == pc) { pc_counters[i]++; found = true; break; }
                }
                if (!found && pc_next_bin < 16) {
                    pc_bins[pc_next_bin] = pc;
                    pc_counters[pc_next_bin] = 1;
                    pc_next_bin++;
                }
                pc_sample_stride++;
                if (pc_sample_stride % 20 == 0) {
                    std::fprintf(stderr, "[pc-hist]");
                    for (int i = 0; i < pc_next_bin; i++) {
                        std::fprintf(stderr, " %05x:%lld", pc_bins[i], (long long)pc_counters[i]);
                    }
                    std::fprintf(stderr, "\n");
                }
            }
            if (pc != last_pc) {
                pc_changes++;
                last_pc = pc;
            }
        }
        int64_t consumed = cpu.step();
        if (consumed <= 0) consumed = 1;
        asic1.tick(consumed);
        asic2.tick(consumed);
        passedCycles += consumed;
        // Pump the buzzer square wave at ~64 Hz (125 samples per call at
        // 8 kHz). `active` is true while either the buzzer pin is high
        // or the click-envelope latch is still holding from a recent
        // rising edge — see AudioCodecModel::noteBuzzerRisingEdge.
        if (passedCycles >= m_nextAudioTickAt) {
            bool active = m_buzzerLevel || audio.buzzerHolding();
            audio.emitBuzzerSamples(active, audio.buzzerClickHz);
            m_nextAudioTickAt = passedCycles + (m_cfg.busClockHz / 64);
        }
        // Fire the deferred cold-attach door NMI once the kernel's had
        // time to install its real NMI vector at IVT[2].
        if (m_pendingColdDoorAt >= 0 && passedCycles >= m_pendingColdDoorAt) {
            asic2.setDoorNmi(true);
            m_doorNmiClearAt = passedCycles + (m_cfg.busClockHz / 5);
            m_pendingColdDoorAt = -1;
        }
        // Clear the SSD door NMI once the ~200 ms pulse expires.
        if (m_doorNmiClearAt >= 0 && passedCycles >= m_doorNmiClearAt) {
            asic2.setDoorNmi(false);
            m_doorNmiClearAt = -1;
        }
        // Auto-wake pulse — synthesise a single A1OnKey edge after the
        // kernel's early POST so the device boots straight to the app
        // row instead of sitting in standby waiting for an explicit ESC
        // tap. See series3.h for the rationale. Both edges are needed:
        // the assert raises EINT3 (gated by ASIC1's interrupt mask
        // until the kernel writes A1InterruptMask), and the eventual
        // release matches the ~150-200 ms button-up edge of a real
        // user tap so the kernel's wake handler sees a clean transition.
        if (!m_autoWakeFired && m_autoWakeAssertAt >= 0 &&
            passedCycles >= m_autoWakeAssertAt) {
            asic2.setOnClr(true);
            m_autoWakeFired = true;
        }
        if (m_autoWakeFired && m_autoWakeReleaseAt >= 0 &&
            passedCycles >= m_autoWakeReleaseAt) {
            asic2.setOnClr(false);
            m_autoWakeReleaseAt = -1;
        }
    }
    if (trace) {
        std::fprintf(stderr, "[pc] total pc_changes=%llu at cycle=%lld\n",
                     (unsigned long long)pc_changes, (long long)passedCycles);
    }
    // If the CPU halted mid-budget, still advance the cycle counter so
    // the harness loop terminates.
    if (passedCycles < cycles) {
        passedCycles = cycles;
    }
}

// ──────────────────────────────────────────────────────────────────────
// V30Bus — memory accesses
//
// The MAME mem_map sends 0x00000-0xFFFFF through psion_asic1_device::mem_r
// /mem_w. ASIC1 in turn has its own internal address space populated by:
//   asic1_map: 0x00000-0x7FFFF noprw, 0x80000-0xFFFFF rom("flash")
// plus the host installing RAM at 0..ram_mask in machine_start().
//
// We don't replicate MAME's nested address-space machinery here — the
// host owns the bus and decodes directly. ASIC1's protection trap is
// not exercised on read paths in MAME either.
// ──────────────────────────────────────────────────────────────────────

uint8_t Emulator::busRead(uint32_t linear) const {
    linear &= 0xFFFFF;
    // VRAM window first (0xB8000-0xBFFFF on MC400) — must take priority
    // over the RAM mirror, which on the MC400 spans the bottom half of
    // the 1 MiB space and would otherwise swallow VRAM reads.
    if (m_cfg.vramSize > 0 &&
        linear >= m_cfg.vramBase &&
        linear < m_cfg.vramBase + m_cfg.vramSize) {
        return vram[linear - m_cfg.vramBase];
    }
    if (m_cfg.ramDecodeMask > 0 && linear <= m_cfg.ramDecodeMask) {
        return ram[linear & (ram.size() - 1)];
    }
    if (m_cfg.ramDecodeMask == 0 && linear < ram.size()) {
        return ram[linear];
    }
    // Top-of-memory ROM alias (HC120) — see Config::romAliasSize.
    if (m_cfg.romAliasSize > 0 &&
        linear >= 0x100000u - m_cfg.romAliasSize &&
        rom.size() >= m_cfg.romAliasSize) {
        size_t off = rom.size() - m_cfg.romAliasSize
                   + (linear - (0x100000u - uint32_t(m_cfg.romAliasSize)));
        if (off < rom.size()) return rom[off];
    }
    if (linear >= m_cfg.romBase) {
        uint32_t off = linear - m_cfg.romBase;
        if (m_cfg.mirrorRom) off &= (rom.size() - 1);
        if (off < rom.size()) return rom[off];
    }
    // Unmapped — NOPRW in MAME's asic1_map; bus pulls high.
    return 0xFF;
}

uint8_t Emulator::readMemByte(uint32_t linear) {
    return busRead(linear);
}

uint16_t Emulator::readMemWord(uint32_t linear) {
    // V30 supports unaligned word reads; do the byte fetches separately
    // so an access that straddles a region boundary still resolves.
    uint16_t lo = readMemByte(linear);
    uint16_t hi = readMemByte((linear + 1) & 0xFFFFF);
    return uint16_t(lo | (hi << 8));
}

void Emulator::writeMemByte(uint32_t linear, uint8_t v) {
    linear &= 0xFFFFF;
    // VRAM window has priority over the RAM mirror — same dispatch order
    // as busRead() above.
    if (m_cfg.vramSize > 0 &&
        linear >= m_cfg.vramBase &&
        linear < m_cfg.vramBase + m_cfg.vramSize) {
        vram[linear - m_cfg.vramBase] = v;
        return;
    }
    if (m_cfg.ramDecodeMask > 0 && linear <= m_cfg.ramDecodeMask) {
        ram[linear & (ram.size() - 1)] = v;
        return;
    }
    if (m_cfg.ramDecodeMask == 0 && linear < ram.size()) {
        if (traceKernelState()) {
            // Watch the kernel wake flag (DS=0xA0 : offset=0x43C) and the
            // ISR's far-pointer jumptable entry at DS=0xA0:0x452.
            if (linear == 0xa43c || (linear >= 0xa452 && linear <= 0xa455)) {
                uint16_t cs = cpu.sregs[1];
                uint16_t ip = cpu.ip;
                std::fprintf(stderr,
                    "[kmem] wr byte [%05x]=%02x from %04x:%04x\n",
                    linear, v, cs, ip);
            }
            // Also record writes to the IVT region (0..0x3FF = 256 vectors × 4 bytes)
            // Print the first 30 writes at PC >= 0xce800 (after RAM test).
            if (linear < 0x400) {
                uint16_t cs = cpu.sregs[1];
                uint16_t ip = cpu.ip;
                uint32_t pc = (uint32_t(cs) << 4) + ip;
                if (pc >= 0xce8c0 && pc <= 0xce900) {
                    static int ivt_writes_logged = 0;
                    if (ivt_writes_logged < 30) {
                        std::fprintf(stderr,
                            "[ivt] wr byte [%05x]=%02x from %04x:%04x (vec %02x)\n",
                            linear, v, cs, ip, (unsigned)(linear / 4));
                        ivt_writes_logged++;
                    }
                }
            }
        }
        ram[linear] = v;
        return;
    }
    // ROM writes and unmapped writes are silently dropped.
}

void Emulator::writeMemWord(uint32_t linear, uint16_t v) {
    writeMemByte(linear, uint8_t(v & 0xFF));
    writeMemByte((linear + 1) & 0xFFFFF, uint8_t((v >> 8) & 0xFF));
}

void Emulator::dumpKernelState() {
    if (!traceKernelState()) return;
    std::fprintf(stderr,
        "[kstate] [a43c]=%02x  a452 fp=%04x:%04x  [a438]=%02x [a439]=%02x\n",
        ram[0xa43c],
        uint16_t(ram[0xa452] | (ram[0xa453] << 8)),
        uint16_t(ram[0xa454] | (ram[0xa455] << 8)),
        ram[0xa438], ram[0xa439]);
}

// ──────────────────────────────────────────────────────────────────────
// V30Bus — I/O accesses
//
// MAME io_map (psion3_state::io_map / mc400.cpp::io_map):
//   0x0000-0x001F  ASIC1 (16-bit)
//   0x0080-0x008F  ASIC2 (8-bit, low byte only — `umask16(0x00ff)`).
//                  Per MAME's word-stride interpretation each 16-bit
//                  word port pair decodes to a single 8-bit register,
//                  so port 0x80→reg0, 0x82→reg1, …, 0x8E→reg7. The
//                  MC400 boot ROM relies on this strict mapping to
//                  configure A2ChannelControl (reg 7 / port 0x8E) and
//                  enable channel 0 for the ASIC3 PSU handshake. The
//                  Series 3 v1.91f ROM hits the same ports but with a
//                  byte-stride alias (`& 7`) it followed a wedge
//                  through to the LCD-enable path; switching to strict
//                  word-stride exposes other Series-3 emulator bugs we
//                  haven't yet fixed. Until those are sorted, the
//                  laptop mode (MC400) uses the strict mapping and the
//                  handheld (Series 3) keeps the alias.
//   0x0200-0x02FF  PCD3311 DTMF (write-only, stubbed here)
// ──────────────────────────────────────────────────────────────────────

static void traceIo(const char *kind, uint16_t port, uint32_t v, uint16_t cs, uint16_t ip) {
    static int cached = -1;
    if (cached < 0) {
        const char *e = std::getenv("PSION3_TRACE_IO");
        cached = (e && *e && *e != '0') ? 1 : 0;
    }
    if (!cached) return;
    static uint64_t n = 0;
    if (n++ < 200000) {
        std::fprintf(stderr, "[io] %s port=%04x val=%04x from %04x:%04x\n",
            kind, port, v, cs, ip);
    }
}

uint8_t Emulator::readIoByte(uint16_t port) {
    if (port < 0x20) {
        // ASIC1 16-bit window, byte access — read the word and pick the
        // requested byte.
        uint16_t w = asic1.ioRead(port & 0x1E);
        uint8_t b = uint8_t((port & 1) ? (w >> 8) : (w & 0xFF));
        traceIo("rb", port, b, cpu.sregs[1], cpu.ip);
        return b;
    }
    if (port >= 0x80 && port < 0x90) {
        // ASIC2 sits behind MAME's
        //   map(0x80, 0x8F).rw(asic2, io_r, io_w).umask16(0x00ff)
        // declaration. On the V30's 16-bit bus that umask connects only
        // the LOW byte of each 16-bit word port to the chip, so the
        // chip's eight registers land at host ports 0x80, 0x82, ..., 0x8E
        // with stride 2 (NOT byte-stride). The chip-side `offset & 7`
        // then resolves to a register index 0..7. Series 3 / 3a / 3c /
        // 3mx / Siena and the MC400 all use the same word-stride
        // mapping; the auto-wake pulse below compensates for the
        // v1.91f kernel's A1OnKey side-effect that the byte-stride
        // alias used to provide.
        uint8_t b = asic2.ioRead(uint32_t(port - 0x80) >> 1);
        traceIo("rb", port, b, cpu.sregs[1], cpu.ip);
        return b;
    }
    traceIo("rb-unh", port, 0xFF, cpu.sregs[1], cpu.ip);
    return 0xFF;
}

uint16_t Emulator::readIoWord(uint16_t port) {
    if (port < 0x20) {
        uint16_t w = asic1.ioRead(port & 0x1E);
        traceIo("rw", port, w, cpu.sregs[1], cpu.ip);
        return w;
    }
    if (port >= 0x80 && port < 0x90) {
        // 8-bit chip on a 16-bit bus: low byte = chip data, high byte = FF.
        // Same stride-2 decode as readIoByte (see comment there).
        uint16_t w = uint16_t(asic2.ioRead(uint32_t(port - 0x80) >> 1)) | 0xFF00;
        traceIo("rw", port, w, cpu.sregs[1], cpu.ip);
        return w;
    }
    traceIo("rw-unh", port, 0xFFFF, cpu.sregs[1], cpu.ip);
    return 0xFFFF;
}

void Emulator::writeIoByte(uint16_t port, uint8_t v) {
    traceIo("wb", port, v, cpu.sregs[1], cpu.ip);
    if (port < 0x20) {
        // ASIC1 byte write: mask to the appropriate half.
        uint16_t mask = (port & 1) ? 0xFF00 : 0x00FF;
        uint16_t data = (port & 1) ? uint16_t(v) << 8 : uint16_t(v);
        asic1.ioWrite(port & 0x1E, data, mask);
        return;
    }
    if (port >= 0x80 && port < 0x90) {
        asic2.ioWrite(uint32_t(port - 0x80) >> 1, v);
        return;
    }
    if (port >= 0x200 && port < 0x300) {
        // DTMF write — stubbed.
        return;
    }
}

void Emulator::writeIoWord(uint16_t port, uint16_t v) {
    traceIo("ww", port, v, cpu.sregs[1], cpu.ip);
    if (port < 0x20) {
        asic1.ioWrite(port & 0x1E, v);
        return;
    }
    if (port >= 0x80 && port < 0x90) {
        asic2.ioWrite(uint32_t(port - 0x80) >> 1, uint8_t(v & 0xFF));
        return;
    }
    if (port >= 0x200 && port < 0x300) {
        return;
    }
}

uint8_t Emulator::ackInterrupt() {
    return asic1.inta();
}

// ──────────────────────────────────────────────────────────────────────
// LCD readout — MAME (reference/mame-psion/psion/psion3.cpp:288) uses
// screen.set_size(240, 80) for the Series 3 panel, with the framebuffer
// at RAM[0x400..] laid out as 30 bytes × 80 rows = 2400 bytes. We still
// expose 480x160 to the harness so the UI window looks reasonable; that
// output is a 2× nearest-neighbour upscale of the real 240×80 source.
//
// Before this fix we asked asic1.readLCD(w=480, h=160), which walked
// past the framebuffer into unrelated kernel state (the idle-queue /
// process-table structures the kernel sets up at RAM[0x10d4..]) and
// rendered those bytes as pixels. The result was a scrambled top-plus-
// noise image — the "init pattern noise" the harness saw even after
// boot had reached the kernel idle loop at 0xceac2. With this fix the
// boot-harness variance drops from ~2488 (noise dominated) to ~920 and
// the screen has a real title bar + app row + status bar.
// ──────────────────────────────────────────────────────────────────────

void Emulator::readLCDIntoBuffer(uint8_t **lines, bool is32BitOutput) const {
    const int outW = m_cfg.lcdWidth;
    const int outH = m_cfg.lcdHeight;
    const int fbW  = m_cfg.fbWidth;
    const int fbH  = m_cfg.fbHeight;
    std::vector<uint8_t> tmp(size_t(fbW) * size_t(fbH), 0);
    asic1.readLCD(tmp.data(), fbW, fbH, m_cfg.fbPlates);
    for (int y = 0; y < outH; y++) {
        const int srcY = (y * fbH) / outH;
        const uint8_t *srcRow = tmp.data() + size_t(srcY) * size_t(fbW);
        for (int x = 0; x < outW; x++) {
            const int srcX = (x * fbW) / outW;
            const uint8_t bit = srcRow[srcX];
            if (is32BitOutput) {
                auto line = reinterpret_cast<uint32_t *>(lines[y]);
                // 0 = paper (light), 1 = ink (dark).
                line[x] = bit ? 0xFF505032u : 0xFFAAB4A0u;
            } else {
                lines[y][x] = bit ? 0x00 : 0xFF;
            }
        }
    }
}

// Series 3 keyboard matrix (English, from MAME psion3.cpp COL0..COL9).
KeyMatrixEntry series3KeyMatrix(EpocKey key) {
    switch (static_cast<int>(key)) {  // ASCII char-literal cases below are intentional
    // App row F-keys (three columns, 3 keys each).
    case EStdKeyF1: return { 9, 0x01 }; // System
    case EStdKeyF2: return { 7, 0x01 }; // Data
    case EStdKeyF3: return { 8, 0x01 }; // Word
    case EStdKeyF4: return { 9, 0x02 }; // Agenda
    case EStdKeyF5: return { 7, 0x02 }; // Time
    case EStdKeyF6: return { 8, 0x02 }; // World
    case EStdKeyF7: return { 9, 0x04 }; // Calc
    case EStdKeyF8: return { 7, 0x04 }; // Program
    // Editing + navigation + modifiers.
    case EStdKeyEnter:       return { 6, 0x40 };
    case EStdKeyTab:         return { 7, 0x80 };
    case EStdKeyBackspace:   return { 6, 0x20 };
    case EStdKeyLeftArrow:   return { 3, 0x80 };
    case EStdKeyRightArrow:  return { 0, 0x80 };
    case EStdKeyUpArrow:     return { 4, 0x01 };
    case EStdKeyDownArrow:   return { 4, 0x80 };
    case EStdKeyLeftShift:   return { 6, 0x08 };
    case EStdKeyRightShift:  return { 1, 0x80 };
    case EStdKeyLeftCtrl:
    case EStdKeyRightCtrl:   return { 6, 0x04 };
    case EStdKeyLeftAlt:
    case EStdKeyRightAlt:    return { 6, 0x10 }; // Psion
    case EStdKeyMenu:        return { 3, 0x02 };
    case EStdKeyHelp:        return { 1, 0x10 };
    case EStdKeyCapsLock:    return { 4, 0x02 };
    // Punctuation that has a dedicated EStdKey constant.
    case ',':
    case EStdKeyComma:       return { 0, 0x10 };
    case '.':
    case EStdKeyFullStop:    return { 3, 0x01 };
    // Letter / digit / symbol matrix from MAME's psion3 INPUT_PORTS_START.
    // The frontend keymap (charToEpocChord) sends letters as their
    // UPPER-CASE ASCII code, digits and matrix-row punctuation as their
    // raw ASCII.
    case 'A':                return { 1, 0x02 };
    case 'B':                return { 5, 0x08 };
    case 'C':                return { 2, 0x04 };
    case 'D':                return { 3, 0x04 };
    case 'E':                return { 4, 0x04 };
    case 'F':                return { 0, 0x04 };
    case 'G':                return { 4, 0x08 };
    case 'H':                return { 5, 0x01 };
    case 'I':                return { 1, 0x20 };
    case 'J':                return { 3, 0x10 };
    case 'K':                return { 0, 0x20 };
    case 'L':                return { 5, 0x20 };
    case 'M':                return { 2, 0x10 };
    case 'N':                return { 5, 0x80 };
    case 'O':                return { 4, 0x20 };
    case 'P':                return { 4, 0x40 };
    case 'Q':                return { 0, 0x02 };
    case 'R':                return { 0, 0x08 };
    case 'S':                return { 6, 0x02 };
    case 'T':                return { 3, 0x08 };
    case 'U':                return { 4, 0x10 };
    case 'V':                return { 1, 0x04 };
    case 'W':                return { 6, 0x01 };
    case 'X':                return { 5, 0x02 };
    case 'Y':                return { 2, 0x80 };
    case 'Z':                return { 2, 0x02 };
    case '0':                return { 3, 0x40 };
    case '1':                return { 0, 0x01 };
    case '2':                return { 1, 0x01 };
    case '3':                return { 5, 0x04 };
    case '4':                return { 1, 0x08 };
    case '5':                return { 2, 0x08 };
    case '6':                return { 2, 0x01 };
    case '7':                return { 5, 0x10 };
    case '8':                return { 2, 0x20 };
    case '9':                return { 3, 0x20 };
    case ' ':
    case EStdKeySpace:       return { 6, 0x80 };
    case '/':
    case ';':                return { 0, 0x40 };
    case '-':
    case '_':                return { 1, 0x40 };
    case '=':
    case '+':                return { 2, 0x40 };
    case '*':
    case ':':                return { 5, 0x40 };
    default: return { -1, 0 };
    }
}

// MC400 keyboard matrix — verbatim from MAME's
// `src/mame/psion/mc400.cpp` INPUT_PORTS_START(psionmc) PORT_BIT block.
// The MC400 is a full QWERTY laptop without dedicated F-keys: the
// app-launchers ("Word" / "Schedule" etc.) on a real MC400 were Psion
// modifier + letter combinations rather than standalone keys, so the
// host F1-F8 don't have a direct matrix slot. Esc routes to ASIC2
// OnClr out-of-band (handled in setKeyboardKey).
KeyMatrixEntry mc400KeyMatrix(EpocKey key) {
    switch (static_cast<int>(key)) {  // ASCII char-literal cases below are intentional
    // ── COL0 — modifiers + Left/Down arrows ───────────────────────────
    case EStdKeyLeftCtrl:
    case EStdKeyRightCtrl:   return { 0, 0x01 }; // Control
    case EStdKeyLeftShift:   return { 0, 0x02 }; // Shift L
    case EStdKeyLeftAlt:
    case EStdKeyRightAlt:    return { 0, 0x04 }; // Psion
    case EStdKeyCapsLock:    return { 0, 0x08 }; // Caps Lock
    case EStdKeyRightShift:  return { 0, 0x10 }; // Shift R
    case EStdKeyLeftArrow:   return { 0, 0x40 };
    case EStdKeyDownArrow:   return { 0, 0x80 };

    // ── COL1 ──────────────────────────────────────────────────────────
    case '1': return { 1, 0x01 };
    case '2': return { 1, 0x02 };
    case 'Q': return { 1, 0x04 };
    case 'W': return { 1, 0x08 };
    case 'A': return { 1, 0x10 };
    case 'S': return { 1, 0x20 };
    case 'Z': return { 1, 0x40 };
    case 'X': return { 1, 0x80 };

    // ── COL2 ──────────────────────────────────────────────────────────
    case '3': return { 2, 0x01 };
    case '4': return { 2, 0x02 };
    case 'E': return { 2, 0x04 };
    case 'R': return { 2, 0x08 };
    case 'D': return { 2, 0x10 };
    case 'F': return { 2, 0x20 };
    case 'C': return { 2, 0x40 };
    case 'V': return { 2, 0x80 };

    // ── COL3 ──────────────────────────────────────────────────────────
    case '5': return { 3, 0x01 };
    case '6': return { 3, 0x02 };
    case 'T': return { 3, 0x04 };
    case 'Y': return { 3, 0x08 };
    case 'G': return { 3, 0x10 };
    case 'H': return { 3, 0x20 };
    case 'B': return { 3, 0x40 };
    case 'N': return { 3, 0x80 };

    // ── COL4 ──────────────────────────────────────────────────────────
    case '7': return { 4, 0x01 };
    case '8': return { 4, 0x02 };
    case 'U': return { 4, 0x04 };
    case 'I': return { 4, 0x08 };
    case 'J': return { 4, 0x10 };
    case 'K': return { 4, 0x20 };
    case 'M': return { 4, 0x40 };
    case ',':
    case EStdKeyComma:       return { 4, 0x80 };

    // ── COL5 ──────────────────────────────────────────────────────────
    case '9': return { 5, 0x01 };
    case '0': return { 5, 0x02 };
    case 'O': return { 5, 0x04 };
    case 'P': return { 5, 0x08 };
    case 'L': return { 5, 0x10 };
    case ';':
    case EStdKeySemiColon:   return { 5, 0x20 };
    case '.':
    case EStdKeyFullStop:    return { 5, 0x40 };
    case '/':
    case EStdKeyForwardSlash: return { 5, 0x80 };

    // ── COL6 — symbols + Up + Enter ───────────────────────────────────
    case '-':
    case EStdKeyMinus:       return { 6, 0x01 };
    case '=':
    case EStdKeyEquals:      return { 6, 0x02 };
    case '[':
    case EStdKeySquareBracketLeft:  return { 6, 0x04 };
    case ']':
    case EStdKeySquareBracketRight: return { 6, 0x08 };
    case '\'':
    case EStdKeySingleQuote: return { 6, 0x10 };
    case '#':
    case EStdKeyHash:        return { 6, 0x20 };
    case EStdKeyUpArrow:     return { 6, 0x40 };
    case EStdKeyEnter:       return { 6, 0x80 };

    // ── COL7 — Backspace / Tab / Space / arrows ───────────────────────
    case EStdKeyBackspace:   return { 7, 0x01 };
    case EStdKeyTab:         return { 7, 0x02 };
    case '\\':
    case EStdKeyBackSlash:   return { 7, 0x08 };
    case ' ':
    case EStdKeySpace:       return { 7, 0x40 };
    case EStdKeyRightArrow:  return { 7, 0x80 };

    // ── COL8 — extra app keys (Task / Home / PgUp / LCD / Touchpad) ──
    case EStdKeyMenu:        return { 8, 0x01 }; // Task button
    case EStdKeyHome:        return { 8, 0x02 };
    case EStdKeyPageUp:      return { 8, 0x04 };
    // EStdKeyXXX (120) is repurposed as the MC400 trackpad button — the
    // frontend's on-screen "Click" overlay sends it. Keeping it on the
    // matrix path means the kernel sees the button press as a key event,
    // exactly as MAME wires the physical click switch.
    case EStdKeyXXX:         return { 8, 0x10 };

    // ── COL9 — Delete / End / PgDn / Record ──────────────────────────
    case EStdKeyDelete:      return { 9, 0x01 };
    case EStdKeyEnd:         return { 9, 0x02 };
    case EStdKeyPageDown:    return { 9, 0x04 };

    default: return { -1, 0 };
    }
}

// ──────────────────────────────────────────────────────────────────────
// Psion HC120 keyboard.
//
// No MAME driver exists for the HC, so this table was read off the
// machine itself: with the shell up (it echoes what you type), every one
// of the 80 matrix slots was pressed in turn and the character that came
// back recorded. What fell out is a tidy grid — one matrix column per
// physical keypad row, with the six keys of that row on bits 0x20 (left)
// down to 0x01 (right):
//
//   col0  ESC   MENU  PgUp  PgDn  <-    INFO/->    + (0x40)
//   col1  A     B     C     D     E     F          . (0x40)
//   col2  G     H     I     J     K     L          0 (0x40)
//   col3  M     N     O     P     Q     R      SPACE (0x40)
//   col4  S     T     U     V     W     X      SHIFT (0x40)
//   col5  <->   7     8     9     /     Y
//   col6  DEL   4     5     6     *     Z
//   col7  LOCK  1     2     3     -     ENTER
//
// The 0x40 bits are the bottom row of the keypad (SHIFT SPACE 0 . +),
// which runs right to left as the column index rises. That accounts for
// all 54 keys on the machine.
//
// Everything above was confirmed by the character it produced, except
// the top row, which the shell gives little back on: its leftmost key
// visibly edits the input line (the characters before the cursor go),
// two slots (col0 0x10 and col0 0x80) echo a glyph outside the ASCII
// font, and the rest do nothing you can see. None of that names a key,
// so the row is mapped by position in the same left-to-right bit order
// every other row uses, which at least puts ESC on the key that edits
// the line.
KeyMatrixEntry hc120KeyMatrix(EpocKey key) {
    switch (static_cast<int>(key)) {  // ASCII char-literal cases below are intentional
    // ── COL0 — the top row of the keypad, plus '+' ───────────────────
    case EStdKeyEscape:      return { 0, 0x20 };
    case EStdKeyMenu:        return { 0, 0x10 };
    case EStdKeyUpArrow:
    case EStdKeyPageUp:      return { 0, 0x08 };
    case EStdKeyDownArrow:
    case EStdKeyPageDown:    return { 0, 0x04 };
    case EStdKeyLeftArrow:   return { 0, 0x02 };
    case EStdKeyRightArrow:  return { 0, 0x01 };
    case '+':                return { 0, 0x40 };

    // ── COL1..COL4 — the letter rows, plus the bottom row ────────────
    case 'A':                return { 1, 0x20 };
    case 'B':                return { 1, 0x10 };
    case 'C':                return { 1, 0x08 };
    case 'D':                return { 1, 0x04 };
    case 'E':                return { 1, 0x02 };
    case 'F':                return { 1, 0x01 };
    case '.':
    case EStdKeyFullStop:    return { 1, 0x40 };

    case 'G':                return { 2, 0x20 };
    case 'H':                return { 2, 0x10 };
    case 'I':                return { 2, 0x08 };
    case 'J':                return { 2, 0x04 };
    case 'K':                return { 2, 0x02 };
    case 'L':                return { 2, 0x01 };
    case '0':                return { 2, 0x40 };

    case 'M':                return { 3, 0x20 };
    case 'N':                return { 3, 0x10 };
    case 'O':                return { 3, 0x08 };
    case 'P':                return { 3, 0x04 };
    case 'Q':                return { 3, 0x02 };
    case 'R':                return { 3, 0x01 };
    case EStdKeySpace:
    case ' ':                return { 3, 0x40 };

    case 'S':                return { 4, 0x20 };
    case 'T':                return { 4, 0x10 };
    case 'U':                return { 4, 0x08 };
    case 'V':                return { 4, 0x04 };
    case 'W':                return { 4, 0x02 };
    case 'X':                return { 4, 0x01 };
    case EStdKeyLeftShift:
    case EStdKeyRightShift:  return { 4, 0x40 };

    // ── COL5..COL7 — the numeric keypad rows ─────────────────────────
    case EStdKeyTab:         return { 5, 0x20 };   // the <-> key
    case '7':                return { 5, 0x10 };
    case '8':                return { 5, 0x08 };
    case '9':                return { 5, 0x04 };
    case '/':                return { 5, 0x02 };
    case 'Y':                return { 5, 0x01 };

    case EStdKeyBackspace:
    case EStdKeyDelete:      return { 6, 0x20 };   // the DEL / ORD key
    case '4':                return { 6, 0x10 };
    case '5':                return { 6, 0x08 };
    case '6':                return { 6, 0x04 };
    case '*':                return { 6, 0x02 };
    case 'Z':                return { 6, 0x01 };

    case EStdKeyCapsLock:    return { 7, 0x20 };   // the LOCK key
    case '1':                return { 7, 0x10 };
    case '2':                return { 7, 0x08 };
    case '3':                return { 7, 0x04 };
    case '-':                return { 7, 0x02 };
    case EStdKeyEnter:       return { 7, 0x01 };

    default: return { -1, 0 };
    }
}

void Emulator::setKeyboardKey(EpocKey key, bool value) {
    // Esc is special — MAME's psion3_state::key_on routes it directly to
    // ASIC2's on_clr_w input (A1OnKey IRQ source), NOT through the COLn
    // keyboard matrix. Series 3 / HC / MC use this for ON / Esc so that
    // pressing it from standby can wake the device.
    //
    // MAME's key_on only forwards the press edge (`if (newval)`); the
    // A1OnKey bit is latched in A2Status and cleared on the next status
    // read. Calling setOnClr on the release edge as well would lower the
    // IRQ line before the kernel had a chance to dispatch the EINT3 from
    // ASIC2, so a short tap on Esc could be missed entirely.
    if (key == EStdKeyEscape && m_cfg.escIsOnKey) {
        if (value) asic2.setOnClr(true);
        return;
    }
    // A machine whose Esc is an ordinary keypad key (the HC120) still has
    // an ON button on its case, on the same OnClr line. EStdKeyOff is the
    // code the frontend's ON button sends for it.
    if (key == EStdKeyOff && !m_cfg.escIsOnKey) {
        if (value) asic2.setOnClr(true);
        return;
    }

    KeyMatrixEntry s = m_cfg.keyMatrix
        ? m_cfg.keyMatrix(key)
        : series3KeyMatrix(key);
    if (s.col < 0 || s.col >= int(sizeof(m_key_row))) return;
    if (value) m_key_row[s.col] |=  s.bit;
    else       m_key_row[s.col] &= ~s.bit;
}

void Emulator::updateTouchInput(int32_t x, int32_t y, bool down) {
    // Series 3 has no digitiser. The MC400 has a built-in trackpad whose
    // analog X/Y position is read through ASIC3's ADC channel 0 (selected
    // when A3Control2 bits 6-7 = 0; ASIC2 A2Control2 bit 1 picks which
    // axis the multiplexer presents).
    //
    // The trackpad button is NOT toggled here: position and click are
    // separate inputs on the real device, and on a touchscreen there's
    // no way to "drag without clicking" if the two are coupled. The
    // frontend renders a dedicated on-screen "Click" button that sends
    // EStdKeyXXX → COL8 bit 0x10 instead.
    if (!m_cfg.laptopMode) return;
    (void)down;

    // Map LCD pixel coordinates (0..lcdWidth-1, 0..lcdHeight-1) to the
    // 11-bit ADC range MAME uses (DIGIT0/DIGIT1 are PORT_BIT 0x7FF).
    // Y is inverted — moving the finger down should produce a smaller
    // ADC value because the trackpad's Y axis runs bottom-to-top
    // relative to the screen, matching MAME's PORT_REVERSE on DIGIT1.
    const int W = std::max(1, m_cfg.lcdWidth);
    const int H = std::max(1, m_cfg.lcdHeight);
    int32_t cx = std::clamp<int32_t>(x, 0, W - 1);
    int32_t cy = std::clamp<int32_t>(y, 0, H - 1);
    uint16_t adcX = uint16_t((cx * 0x7FF) / (W - 1));
    uint16_t adcY = uint16_t(((H - 1 - cy) * 0x7FF) / (H - 1));
    asic3.setDigitiserAxis(0, adcX);
    asic3.setDigitiserAxis(1, adcY);
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

} // namespace Series3
