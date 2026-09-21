// license:BSD-3-Clause
// copyright-holders:Aaron Giles, Sandro Ronco (MAME m6800/m6801)
//                  + adaptation by the Psion emulator project, 2026.
//
// HD6303X CPU core. See hd6303.h for the porting source and what's
// modelled. Semantics mirror MAME's m6800.cpp + m6801.cpp + 6800ops.hxx
// (BSD-3-Clause); structure is rewritten to fit our HD6303Bus shape.

#include "hd6303.h"

#include <cstdio>
#include <cstdlib>

namespace {
bool traceEnv() {
    static int cached = -1;
    if (cached < 0) {
        const char *e = std::getenv("ORG2_TRACE");
        cached = (e && *e && *e != '0') ? 1 : 0;
    }
    return cached != 0;
}
}  // namespace

// ── Bus access ────────────────────────────────────────────────────────
//
// On the HD6303X (Psion Organiser II): on-chip RAM lives at $0040-$00FF
// (192 bytes), and the on-chip register window lives at $0000-$001F.
// The gap between them ($0020-$003F) and everything else passes through
// to the external bus, which in the Organiser II case decodes the
// $0100-$03FF MMIO block, the RAM/ROM banks, and the datapak protocol.
// Note: earlier 6301/6303R variants had only 128 bytes of RAM at
// $0080-$00FF; the X variant extends usable RAM downward — critical
// for the Psion OS because its zero-page workspace lives below $0080.

uint8_t HD6303::read8(uint16_t addr) {
    if (addr < 0x20) {
        switch (addr) {
        case 0x08:  // TCSR — flags + control bits
            m_tcsrLatchedFlags = m_iregs[0x08] & 0xE0;  // snapshot for clear-on-read
            return m_iregs[0x08];
        case 0x09:  // FRC high
            // Reading FRC high clears TOF if previously latched.
            if (m_tcsrLatchedFlags & 0x20) {
                m_iregs[0x08] &= ~0x20;
                m_tcsrLatchedFlags &= ~0x20;
            }
            return uint8_t(m_frc >> 8);
        case 0x0A:  // FRC low
            return uint8_t(m_frc & 0xFF);
        case 0x0B:  // OCR high — clears OCF when latched
            if (m_tcsrLatchedFlags & 0x40) {
                m_iregs[0x08] &= ~0x40;
                m_tcsrLatchedFlags &= ~0x40;
            }
            return uint8_t(m_ocr >> 8);
        case 0x0C:  // OCR low
            return uint8_t(m_ocr & 0xFF);
        case 0x03:  // Port 2 — Organiser II datapak data bus
            if (port2Reader) return port2Reader();
            return m_iregs[addr];
        case 0x15:  // Port 5 (HD6303X) — Organiser II kb / battery / ON
            if (port5Reader) return port5Reader();
            return m_iregs[addr];
        case 0x17:  // Port 6 (HD6303X) — Organiser II datapak control
            if (port6Reader) return port6Reader();
            return m_iregs[addr];
        default:
            return m_iregs[addr];
        }
    }
    if (addr >= 0x40 && addr < 0x100) return m_iram[addr - 0x40];
    return bus_.readByte(addr);
}

void HD6303::write8(uint16_t addr, uint8_t v) {
    if (addr < 0x20) {
        switch (addr) {
        case 0x08:  // TCSR — only the lower 5 bits are writable
            m_iregs[0x08] = (m_iregs[0x08] & 0xE0) | (v & 0x1F);
            return;
        case 0x09:  // FRC high — writing latches FRC to $FFF8
            m_frc = 0xFFF8;
            return;
        case 0x0A:  // FRC low — same effect as writing high
            m_frc = (m_frc & 0xFF00) | v;
            return;
        case 0x0B:  // OCR high — write also clears OCF if latched
            m_ocr = (m_ocr & 0x00FF) | (uint16_t(v) << 8);
            if (m_tcsrLatchedFlags & 0x40) {
                m_iregs[0x08] &= ~0x40;
                m_tcsrLatchedFlags &= ~0x40;
            }
            return;
        case 0x0C:  // OCR low — write also clears OCF if latched
            m_ocr = (m_ocr & 0xFF00) | v;
            if (m_tcsrLatchedFlags & 0x40) {
                m_iregs[0x08] &= ~0x40;
                m_tcsrLatchedFlags &= ~0x40;
            }
            return;
        case 0x03:  // Port 2 — datapak data bus
            m_iregs[addr] = v;
            if (port2Writer) port2Writer(v);
            return;
        case 0x17:  // Port 6 (HD6303X) — datapak control bus
            m_iregs[addr] = v;
            if (port6Writer) port6Writer(v);
            return;
        default:
            m_iregs[addr] = v;
            return;
        }
    }
    if (addr >= 0x40 && addr < 0x100) { m_iram[addr - 0x40] = v; return; }
    bus_.writeByte(addr, v);
}

void HD6303::tickTimer(int64_t consumed) {
    // Advance the free-running counter by the cycles consumed by the
    // last instruction. On wrap, set TOF; on cross of OCR, set OCF.
    uint32_t before = m_frc;
    uint32_t after  = (before + uint32_t(consumed)) & 0xFFFF;
    bool wrapped = (uint32_t(before) + uint32_t(consumed)) > 0xFFFF;
    bool hitOcr  = (before <= m_ocr && (before + uint32_t(consumed)) > m_ocr)
                || (wrapped && m_ocr <= after);
    m_frc = uint16_t(after);
    if (wrapped) m_iregs[0x08] |= 0x20;   // TOF
    if (hitOcr)  m_iregs[0x08] |= 0x40;   // OCF
    // Note: irqLine is for EXTERNAL IRQ1 line only. Timer interrupts
    // are checked at the start of step() against TCSR's flag+enable
    // bits and dispatched to their dedicated vectors (VEC_OCF /
    // VEC_TOF / VEC_ICF), NOT through VEC_IRQ.
}

uint16_t HD6303::read16(uint16_t addr) {
    uint8_t hi = read8(addr);
    uint8_t lo = read8(addr + 1);
    return uint16_t(hi) << 8 | lo;
}

void HD6303::write16(uint16_t addr, uint16_t v) {
    write8(addr,     uint8_t(v >> 8));
    write8(addr + 1, uint8_t(v & 0xFF));
}

// ── Reset / interrupt service ─────────────────────────────────────────

void HD6303::reset() {
    // RAMCR ($14) survives reset in one bit only — see the comment on the
    // write below.
    const uint8_t prevRamcr = m_iregs[0x14];
    pc  = read16(VEC_RES);
    a = b = 0;
    x = 0;
    sp = 0x00FF;     // Top of HD6303X internal RAM
    ccr = CCR_FIXED | CCR_I;
    irqLine = false;
    nmiLine = false;
    halted = false;
    waiting = false;
    waiContext = false;
    _firstOpcodeReported = false;
    // Battery-backed SRAM in real Organiser II hardware is typically
    // un-initialised (random / 0xFF) on first power-on. We default to
    // 0 here (cleared at boot, no warm-state to preserve) which is
    // also what MAME's m6800 does.
    m_iram.fill(0);
    m_iregs.fill(0);
    // Default the HD6303X port-data registers to HIGH (0xFF). Real
    // hardware usually has pull-ups on the bidirectional port pins,
    // so reads of un-driven pins return 1. The Psion OS reads several
    // port-data registers during cold-boot init to test for hardware
    // presence (battery good, datapak insertion, etc.); returning 0
    // for those reads makes the OS think every test failed and parks
    // it in an "off" state.
    m_iregs[0x02] = 0xFF;  // P1 data
    m_iregs[0x03] = 0xFF;  // P2 data
    m_iregs[0x06] = 0xFF;  // P3 data
    m_iregs[0x07] = 0xFF;  // P4 data
    m_iregs[0x17] = 0xFF;  // P5 data
    m_iregs[0x18] = 0xFF;  // P6 data
    // RAMCR ($14). Bit 7 (STBY PWR) is the software-set "the RAM has been
    // initialised and stayed alive" flag: the Psion OS sets it once it has
    // formatted RAM (Organiser I: OIM #$80,$14 at $F039) and reads it back
    // on the next start to choose warm boot over cold (Organiser II at
    // $C026, BPL). It is not cleared by reset, which is the whole point of
    // it — hardware clears it only when the backup supply fails. On a true
    // cold start (battery in for the first time) it is 0, which is what a
    // freshly constructed core has.
    //
    // Every other bit is (re)set by reset itself: MAME's
    // hd6301x_cpu_device::device_reset does
    // `m_ram_ctrl = (m_ram_ctrl & 0x80) | 0x7c`. Bit 6 matters most — it
    // is in effect "the machine is switched on". The Organiser I's ROM
    // clears it just before dropping into standby (AIM #$BF,$14 at $F065)
    // and its periodic-NMI handler tests it (TIM #$40,$14 at $F43B),
    // returning immediately when clear. Come out of reset with bit 6 clear
    // and the machine runs, paints its screen, and never advances its
    // clock, because every tick turns straight round.
    m_iregs[0x14] = uint8_t((prevRamcr & 0x80) | 0x7C);
    if (traceEnv()) {
        std::fprintf(stderr,
            "[hd6303] reset pc=%04x sp=%04x\n", pc, sp);
    }
}

void HD6303::resumeFromStandby() {
    // Same restart as reset(), minus the two things standby preserves:
    // the on-chip RAM keeps its contents on the backup supply, and
    // RAMCR ($14) keeps its standby-power bit, so the ROM can tell this
    // from a battery-in cold start and trust what is in RAM instead of
    // formatting over it. On the real chip that bit (b6, STBY PWR) is
    // set by software once it is happy with RAM and cleared by hardware
    // if the backup supply ever fails — so carrying it across is the
    // whole of what "the RAM survived" means.
    //
    // The Psion Organiser I's periodic-NMI handler is gated on exactly
    // that bit (`TIM #$40,$14` / `BEQ` straight to RTI at $F43B): clear
    // it on wake and the machine comes up, paints its screen and then
    // never advances its clock, because every tick returns immediately.
    const uint8_t ramcr = m_iregs[0x14];
    pc  = read16(VEC_RES);
    a = b = 0;
    x = 0;
    sp = 0x00FF;
    ccr = CCR_FIXED | CCR_I;
    irqLine = false;
    nmiLine = false;
    halted = false;
    waiting = false;
    waiContext = false;
    m_iregs.fill(0);
    m_iregs[0x02] = 0xFF;  // P1 data
    m_iregs[0x03] = 0xFF;  // P2 data
    m_iregs[0x06] = 0xFF;  // P3 data
    m_iregs[0x07] = 0xFF;  // P4 data
    m_iregs[0x17] = 0xFF;  // P5 data
    m_iregs[0x18] = 0xFF;  // P6 data
    // Coming out of standby is a reset on this hardware (ON pulls the
    // reset line), so RAMCR lands the same way it does on any other
    // reset: bit 7 carried across, the rest re-asserted.
    m_iregs[0x14] = uint8_t((ramcr & 0x80) | 0x7C);
    if (traceEnv()) {
        std::fprintf(stderr,
            "[hd6303] wake from standby pc=%04x sp=%04x\n", pc, sp);
    }
}

// Standard 6800 interrupt sequence: push every register onto the stack
// (PC_lo, PC_hi, X_lo, X_hi, A, B, CCR — pushed in that order so the
// stack ends up with PC at the top when RTI pops), set the I-flag, and
// jump to the vector.
void HD6303::serviceInterrupt(uint16_t vec) {
    // WAI pre-pushes the full register context before halting; on
    // interrupt, the CPU jumps directly to the vector without pushing
    // again. SLP and a regular interrupt (no WAI) require a push.
    if (!waiContext) {
        push16(pc);
        push16(x);
        push8(a);
        push8(b);
        push8(ccr);
    }
    setFlag(CCR_I, true);
    pc = read16(vec);
    waiting = false;
    waiContext = false;
    halted = false;
}

// ── Operation helpers ─────────────────────────────────────────────────

uint8_t HD6303::doADD8(uint8_t l, uint8_t r, uint8_t cIn) {
    uint16_t sum = uint16_t(l) + uint16_t(r) + cIn;
    uint8_t  res = uint8_t(sum);
    setNZ8(res);
    setFlag(CCR_C, sum > 0xFF);
    setFlag(CCR_V, ((l ^ res) & (r ^ res) & 0x80) != 0);
    setFlag(CCR_H, ((l & 0x0F) + (r & 0x0F) + cIn) > 0x0F);
    return res;
}

uint8_t HD6303::doSUB8(uint8_t l, uint8_t r, uint8_t cIn) {
    uint16_t diff = uint16_t(l) - uint16_t(r) - cIn;
    uint8_t  res  = uint8_t(diff);
    setNZ8(res);
    setFlag(CCR_C, (diff & 0x100) != 0);
    setFlag(CCR_V, ((l ^ r) & (l ^ res) & 0x80) != 0);
    return res;
}

uint16_t HD6303::doADD16(uint16_t l, uint16_t r) {
    uint32_t sum = uint32_t(l) + uint32_t(r);
    uint16_t res = uint16_t(sum);
    setNZ16(res);
    setFlag(CCR_C, sum > 0xFFFF);
    setFlag(CCR_V, ((l ^ res) & (r ^ res) & 0x8000) != 0);
    return res;
}

uint16_t HD6303::doSUB16(uint16_t l, uint16_t r) {
    uint32_t diff = uint32_t(l) - uint32_t(r);
    uint16_t res  = uint16_t(diff);
    setNZ16(res);
    setFlag(CCR_C, (diff & 0x10000) != 0);
    setFlag(CCR_V, ((l ^ r) & (l ^ res) & 0x8000) != 0);
    return res;
}

void HD6303::doCMP8(uint8_t l, uint8_t r) {
    (void)doSUB8(l, r, 0);
}

void HD6303::doCMP16(uint16_t l, uint16_t r) {
    (void)doSUB16(l, r);
}

uint8_t HD6303::doAND8(uint8_t l, uint8_t r) {
    uint8_t res = l & r;
    setNZ8(res); setFlag(CCR_V, false);
    return res;
}

uint8_t HD6303::doOR8(uint8_t l, uint8_t r) {
    uint8_t res = l | r;
    setNZ8(res); setFlag(CCR_V, false);
    return res;
}

uint8_t HD6303::doEOR8(uint8_t l, uint8_t r) {
    uint8_t res = l ^ r;
    setNZ8(res); setFlag(CCR_V, false);
    return res;
}

void HD6303::doBIT8(uint8_t l, uint8_t r) {
    uint8_t res = l & r;
    setNZ8(res); setFlag(CCR_V, false);
}

uint8_t HD6303::doINC8(uint8_t v) {
    uint8_t res = v + 1;
    setNZ8(res);
    setFlag(CCR_V, v == 0x7F);
    return res;
}

uint8_t HD6303::doDEC8(uint8_t v) {
    uint8_t res = v - 1;
    setNZ8(res);
    setFlag(CCR_V, v == 0x80);
    return res;
}

uint8_t HD6303::doNEG8(uint8_t v) {
    uint8_t res = uint8_t(-int8_t(v));
    setNZ8(res);
    setFlag(CCR_V, v == 0x80);
    setFlag(CCR_C, v != 0x00);
    return res;
}

uint8_t HD6303::doCOM8(uint8_t v) {
    uint8_t res = ~v;
    setNZ8(res);
    setFlag(CCR_V, false);
    setFlag(CCR_C, true);
    return res;
}

uint8_t HD6303::doASL8(uint8_t v) {
    uint8_t res = uint8_t(v << 1);
    setNZ8(res);
    setFlag(CCR_C, (v & 0x80) != 0);
    setFlag(CCR_V, ((v ^ res) & 0x80) != 0);
    return res;
}

uint8_t HD6303::doLSR8(uint8_t v) {
    uint8_t res = v >> 1;
    setNZ8(res);
    setFlag(CCR_C, (v & 0x01) != 0);
    setFlag(CCR_V, flag(CCR_N) ^ flag(CCR_C));
    return res;
}

uint8_t HD6303::doROL8(uint8_t v) {
    uint8_t cIn = flag(CCR_C) ? 1 : 0;
    uint8_t res = uint8_t((v << 1) | cIn);
    setNZ8(res);
    setFlag(CCR_C, (v & 0x80) != 0);
    setFlag(CCR_V, flag(CCR_N) ^ flag(CCR_C));
    return res;
}

uint8_t HD6303::doROR8(uint8_t v) {
    uint8_t cIn = flag(CCR_C) ? 0x80 : 0;
    uint8_t res = uint8_t((v >> 1) | cIn);
    setNZ8(res);
    setFlag(CCR_C, (v & 0x01) != 0);
    setFlag(CCR_V, flag(CCR_N) ^ flag(CCR_C));
    return res;
}

uint8_t HD6303::doASR8(uint8_t v) {
    uint8_t res = uint8_t(int8_t(v) >> 1);
    setNZ8(res);
    setFlag(CCR_C, (v & 0x01) != 0);
    setFlag(CCR_V, flag(CCR_N) ^ flag(CCR_C));
    return res;
}

void HD6303::doTST8(uint8_t v) {
    setNZ8(v);
    setFlag(CCR_V, false);
    setFlag(CCR_C, false);
}

uint8_t HD6303::doCLR8() {
    setFlag(CCR_N, false);
    setFlag(CCR_Z, true);
    setFlag(CCR_V, false);
    setFlag(CCR_C, false);
    return 0;
}

void HD6303::doBRA(bool cond) {
    int8_t off = int8_t(fetchByte());
    if (cond) pc = uint16_t(pc + off);
}

int64_t HD6303::unimplemented(uint8_t op) {
    if (!_firstOpcodeReported) {
        _firstOpcodeReported = true;
        std::fprintf(stderr,
            "[hd6303] unimplemented opcode 0x%02x at pc=$%04x "
            "(returning to fetch loop will halt CPU; the next session "
            "ports this opcode)\n",
            op, uint16_t(pc - 1));
    }
    halted = true;
    return 4;
}

// ── Main dispatch ─────────────────────────────────────────────────────
//
// Cycle counts are approximate (pulled from MAME's m6800 dispatch table
// for the same opcodes). The Organiser II emulation doesn't need
// instruction-cycle perfection — it just needs the right ratio between
// CPU work and timer ticks for the keyboard scan to fire at the right
// rate, which is captured in this rough table.

int64_t HD6303::step() {
    // Service interrupts before fetching. NMI is unmaskable; everything
    // else checks the I flag.
    //
    // The 6803/6303 has separate vectors for each TIMER interrupt
    // source — OCF, TOF, ICF — independent of the external IRQ1 line
    // (which has its own VEC_IRQ). My earlier wrong dispatch routed
    // every interrupt through VEC_IRQ → state-7 entry → RTI handler,
    // putting the CPU in an infinite no-op IRQ loop the moment the OS
    // enabled OCF interrupts. Now each source has its own vector.
    if (nmiLine) {
        nmiLine = false;
        serviceInterrupt(VEC_NMI);
        return 12;
    }
    if (!flag(CCR_I)) {
        const uint8_t tcsr = m_iregs[0x08];
        // ICF + EICI: input capture interrupt
        if ((tcsr & 0x90) == 0x90) {
            serviceInterrupt(VEC_ICF);
            return 12;
        }
        // OCF + EOCI: output compare interrupt
        if ((tcsr & 0x48) == 0x48) {
            serviceInterrupt(VEC_OCF);
            return 12;
        }
        // TOF + ETOI: timer overflow interrupt
        if ((tcsr & 0x24) == 0x24) {
            serviceInterrupt(VEC_TOF);
            return 12;
        }
        // External IRQ1 (irqLine driven by something other than the
        // on-chip timer — none in the Organiser II case).
        if (irqLine) {
            serviceInterrupt(VEC_IRQ);
            return 12;
        }
    }
    if (waiting) return 4;
    if (halted)  return 4;

    // Per-step trace gated by ORG2_TRACE — prints every instruction's
    // PC + register file. Useful for tracking down the first bad jump
    // during boot bring-up. The first ~1000 lines should be enough to
    // see boot land in rambank2 unexpectedly.
    if (traceEnv()) {
        // ORG2_TRACE=1 dumps PC + register file at every step. Capped
        // at 4000 lines per session to keep the terminal manageable.
        // ORG2_TRACE_FROM=N starts at sim cycle N (skip early init).
        static int n = 0;
        static int64_t startCyc = -1;
        if (startCyc < 0) {
            const char *e = std::getenv("ORG2_TRACE_FROM");
            startCyc = e ? atoll(e) : 0;
        }
        if (n < 4000) {
            // Get cycles from external — simpler than threading through.
            // Use a static cycle counter that we tick as a proxy.
            static int64_t pseudoCyc = 0;
            pseudoCyc++;
            if (pseudoCyc >= startCyc) {
                std::fprintf(stderr,
                    "[hd6303] pc=%04x sp=%04x x=%04x a=%02x b=%02x ccr=%02x\n",
                    pc, sp, x, a, b, ccr);
                n++;
            }
        }
    }

    uint8_t op = fetchOp();
    switch (op) {

    // ── HD6303 TRAP opcodes ────────────────────────────────────────
    // The 6303 family fires the TRAP exception ($FFEE vector) on the
    // following undefined opcodes (lifted verbatim from MAME's
    // hd63701_insn dispatch table in src/devices/cpu/m6800/m6801.cpp).
    // The OS's TRAP handler typically reports the fault and restarts;
    // without this the dispatcher would halt instead of letting the
    // OS recover.
    case 0x00: case 0x02: case 0x03:
    case 0x14: case 0x15:
    case 0x1C: case 0x1D: case 0x1E: case 0x1F:
    case 0x41: case 0x42: case 0x45: case 0x4B: case 0x4E:
    case 0x51: case 0x52: case 0x55: case 0x5B: case 0x5E:
        // Back PC up to the trap-fault opcode so the handler can see
        // it on the stack (matches MAME's m6801.cpp::trap_t behaviour).
        pc = uint16_t(pc - 1);
        serviceInterrupt(VEC_TRAP);
        return 12;

    // ── HD6303 undocumented-but-implemented opcodes ────────────────
    // X += memory[SP+1]. Documented as undoc1 / undoc2 in MAME's
    // 6800ops.hxx; the firmware uses these in tight inner loops.
    case 0x12: case 0x13: {
        x = uint16_t(x + read8(uint16_t(sp + 1)));
        return 3;
    }

    // ── Page 0: inherent + 6303 extras ─────────────────────────────
    case 0x01:  /* NOP  */ return 2;
    case 0x04:  /* LSRD */ {
        uint16_t d = getD();
        bool c = (d & 1) != 0;
        d >>= 1;
        setD(d);
        setFlag(CCR_C, c);
        setNZ16(d);
        setFlag(CCR_V, flag(CCR_N) ^ flag(CCR_C));
        return 3;
    }
    case 0x05:  /* ASLD / LSLD */ {
        uint16_t d = getD();
        bool c = (d & 0x8000) != 0;
        d <<= 1;
        setD(d);
        setFlag(CCR_C, c);
        setNZ16(d);
        setFlag(CCR_V, flag(CCR_N) ^ flag(CCR_C));
        return 3;
    }
    case 0x06:  /* TAP  */ ccr = (a | CCR_FIXED); return 2;
    case 0x07:  /* TPA  */ a = ccr | CCR_FIXED; return 2;
    case 0x08:  /* INX  */ x = uint16_t(x + 1); setFlag(CCR_Z, x == 0); return 3;
    case 0x09:  /* DEX  */ x = uint16_t(x - 1); setFlag(CCR_Z, x == 0); return 3;
    case 0x0A:  /* CLV  */ setFlag(CCR_V, false); return 2;
    case 0x0B:  /* SEV  */ setFlag(CCR_V, true);  return 2;
    case 0x0C:  /* CLC  */ setFlag(CCR_C, false); return 2;
    case 0x0D:  /* SEC  */ setFlag(CCR_C, true);  return 2;
    case 0x0E:  /* CLI  */ setFlag(CCR_I, false); return 2;
    case 0x0F:  /* SEI  */ setFlag(CCR_I, true);  return 2;

    // ── Page 1: register-to-register and 6303 extras ───────────────
    case 0x10:  /* SBA  */ a = doSUB8(a, b, 0); return 2;
    case 0x11:  /* CBA  */ doCMP8(a, b);        return 2;
    case 0x16:  /* TAB  */ b = a; setNZ8(b); setFlag(CCR_V, false); return 2;
    case 0x17:  /* TBA  */ a = b; setNZ8(a); setFlag(CCR_V, false); return 2;
    case 0x18:  /* XGDX (HD6303) — exchange D and X */ {
        uint16_t t = getD();
        setD(x);
        x = t;
        return 2;
    }
    case 0x19:  /* DAA */ {
        // Decimal-adjust A after add. Pulled directly from MAME's
        // 6800ops.hxx daa_t: corrects the BCD result based on A's
        // nibbles and the H/C flags.
        uint16_t cf = 0;
        uint8_t hi = a >> 4;
        uint8_t lo = a & 0x0F;
        if (flag(CCR_H) || lo > 9)  cf |= 0x06;
        if (flag(CCR_C) || hi > 9 || (hi >= 9 && lo > 9))
                                    cf |= 0x60;
        uint16_t t = uint16_t(a) + cf;
        setFlag(CCR_C, flag(CCR_C) || (t & 0x100));
        a = uint8_t(t);
        setNZ8(a);
        return 2;
    }
    case 0x1A:  /* SLP (HD6303) — sleep/halt until interrupt */
        waiting = true;
        return 4;
    case 0x1B:  /* ABA */ a = doADD8(a, b, 0); return 2;

    // ── Page 2: branches ───────────────────────────────────────────
    case 0x20:  /* BRA */ doBRA(true);  return 3;
    case 0x21:  /* BRN */ doBRA(false); return 3;   // branch never
    case 0x22:  /* BHI */ doBRA(!(flag(CCR_C) || flag(CCR_Z))); return 3;
    case 0x23:  /* BLS */ doBRA( (flag(CCR_C) || flag(CCR_Z))); return 3;
    case 0x24:  /* BCC / BHS */ doBRA(!flag(CCR_C)); return 3;
    case 0x25:  /* BCS / BLO */ doBRA( flag(CCR_C)); return 3;
    case 0x26:  /* BNE */ doBRA(!flag(CCR_Z)); return 3;
    case 0x27:  /* BEQ */ doBRA( flag(CCR_Z)); return 3;
    case 0x28:  /* BVC */ doBRA(!flag(CCR_V)); return 3;
    case 0x29:  /* BVS */ doBRA( flag(CCR_V)); return 3;
    case 0x2A:  /* BPL */ doBRA(!flag(CCR_N)); return 3;
    case 0x2B:  /* BMI */ doBRA( flag(CCR_N)); return 3;
    case 0x2C:  /* BGE */ doBRA(!(flag(CCR_N) ^ flag(CCR_V))); return 3;
    case 0x2D:  /* BLT */ doBRA( (flag(CCR_N) ^ flag(CCR_V))); return 3;
    case 0x2E:  /* BGT */ doBRA(!(flag(CCR_Z) || (flag(CCR_N) ^ flag(CCR_V)))); return 3;
    case 0x2F:  /* BLE */ doBRA( (flag(CCR_Z) || (flag(CCR_N) ^ flag(CCR_V)))); return 3;

    // ── Page 3: stack / index / system control ─────────────────────
    case 0x30:  /* TSX  */ x = uint16_t(sp + 1); return 3;
    case 0x31:  /* INS  */ sp = uint16_t(sp + 1); return 3;
    case 0x32:  /* PULA */ a = pop8(); return 4;
    case 0x33:  /* PULB */ b = pop8(); return 4;
    case 0x34:  /* DES  */ sp = uint16_t(sp - 1); return 3;
    case 0x35:  /* TXS  */ sp = uint16_t(x - 1); return 3;
    case 0x36:  /* PSHA */ push8(a); return 3;
    case 0x37:  /* PSHB */ push8(b); return 3;
    case 0x38:  /* PULX */ x = pop16(); return 5;
    case 0x39:  /* RTS  */ pc = pop16(); return 5;
    case 0x3A:  /* ABX  */ x = uint16_t(x + b); return 3;
    case 0x3B:  /* RTI  */ {
        ccr = pop8() | CCR_FIXED;
        b = pop8();
        a = pop8();
        x = pop16();
        pc = pop16();
        return 10;
    }
    case 0x3C:  /* PSHX */ push16(x); return 4;
    case 0x3D:  /* MUL  */ {
        uint16_t r = uint16_t(a) * uint16_t(b);
        setD(r);
        setFlag(CCR_C, (b & 0x80) != 0);    // Per HD6303 datasheet
        return 10;
    }
    case 0x3E:  /* WAI  */ {
        // Pre-push the register context so the next interrupt skips
        // the standard push in serviceInterrupt — see HD6303 / 6800
        // WAI semantics. waiContext flags this state so RTI cleanly
        // restores everything once an interrupt fires.
        push16(pc); push16(x); push8(a); push8(b); push8(ccr);
        waiting = true;
        waiContext = true;
        return 9;
    }
    case 0x3F:  /* SWI  */ serviceInterrupt(VEC_SWI); return 12;

    // ── Page 4: A-register inherent ────────────────────────────────
    case 0x40:  /* NEGA */ a = doNEG8(a); return 2;
    case 0x43:  /* COMA */ a = doCOM8(a); return 2;
    case 0x44:  /* LSRA */ a = doLSR8(a); return 2;
    case 0x46:  /* RORA */ a = doROR8(a); return 2;
    case 0x47:  /* ASRA */ a = doASR8(a); return 2;
    case 0x48:  /* ASLA */ a = doASL8(a); return 2;
    case 0x49:  /* ROLA */ a = doROL8(a); return 2;
    case 0x4A:  /* DECA */ a = doDEC8(a); return 2;
    case 0x4C:  /* INCA */ a = doINC8(a); return 2;
    case 0x4D:  /* TSTA */ doTST8(a); return 2;
    case 0x4F:  /* CLRA */ a = doCLR8(); return 2;

    // ── Page 5: B-register inherent ────────────────────────────────
    case 0x50:  /* NEGB */ b = doNEG8(b); return 2;
    case 0x53:  /* COMB */ b = doCOM8(b); return 2;
    case 0x54:  /* LSRB */ b = doLSR8(b); return 2;
    case 0x56:  /* RORB */ b = doROR8(b); return 2;
    case 0x57:  /* ASRB */ b = doASR8(b); return 2;
    case 0x58:  /* ASLB */ b = doASL8(b); return 2;
    case 0x59:  /* ROLB */ b = doROL8(b); return 2;
    case 0x5A:  /* DECB */ b = doDEC8(b); return 2;
    case 0x5C:  /* INCB */ b = doINC8(b); return 2;
    case 0x5D:  /* TSTB */ doTST8(b); return 2;
    case 0x5F:  /* CLRB */ b = doCLR8(); return 2;

    default:
        // Drop into the page 6/7/8/9/A/B/C/D/E/F handler tables below;
        // anything truly unhandled hits unimplemented(op).
        break;
    }

    // The rest of the table is handled by a second switch in step2()
    // to keep this function under a reasonable line count for
    // diagnostic readability and incremental edit safety.
    return step2(op);
}

// Pages 6-F: memory-operand opcodes (idx, ext, imm, dir, idx, ext for
// each of A then B). All four addressing modes for the same operation
// share their helper; we just plumb the effective address through.

int64_t HD6303::step2(uint8_t op) {
    switch (op) {

    // ── Page 6: indexed memory ops ─────────────────────────────────
    case 0x60: /* NEG idx */ { uint16_t a_ = eaIDX(); write8(a_, doNEG8(read8(a_))); return 6; }
    case 0x61: /* AIM idx (HD6303) — AND immediate-with-memory */ {
        uint8_t imm = fetchByte(); uint16_t a_ = eaIDX();
        uint8_t res = read8(a_) & imm;
        write8(a_, res); setNZ8(res); setFlag(CCR_V, false);
        return 7;
    }
    case 0x62: /* OIM idx (HD6303) */ {
        uint8_t imm = fetchByte(); uint16_t a_ = eaIDX();
        uint8_t res = read8(a_) | imm;
        write8(a_, res); setNZ8(res); setFlag(CCR_V, false);
        return 7;
    }
    case 0x63: /* COM idx */ { uint16_t a_ = eaIDX(); write8(a_, doCOM8(read8(a_))); return 6; }
    case 0x64: /* LSR idx */ { uint16_t a_ = eaIDX(); write8(a_, doLSR8(read8(a_))); return 6; }
    case 0x65: /* EIM idx (HD6303) */ {
        uint8_t imm = fetchByte(); uint16_t a_ = eaIDX();
        uint8_t res = read8(a_) ^ imm;
        write8(a_, res); setNZ8(res); setFlag(CCR_V, false);
        return 7;
    }
    case 0x66: /* ROR idx */ { uint16_t a_ = eaIDX(); write8(a_, doROR8(read8(a_))); return 6; }
    case 0x67: /* ASR idx */ { uint16_t a_ = eaIDX(); write8(a_, doASR8(read8(a_))); return 6; }
    case 0x68: /* ASL idx */ { uint16_t a_ = eaIDX(); write8(a_, doASL8(read8(a_))); return 6; }
    case 0x69: /* ROL idx */ { uint16_t a_ = eaIDX(); write8(a_, doROL8(read8(a_))); return 6; }
    case 0x6A: /* DEC idx */ { uint16_t a_ = eaIDX(); write8(a_, doDEC8(read8(a_))); return 6; }
    case 0x6B: /* TIM idx (HD6303) — test immediate AND memory */ {
        uint8_t imm = fetchByte(); uint16_t a_ = eaIDX();
        doBIT8(read8(a_), imm);
        return 5;
    }
    case 0x6C: /* INC idx */ { uint16_t a_ = eaIDX(); write8(a_, doINC8(read8(a_))); return 6; }
    case 0x6D: /* TST idx */ { uint16_t a_ = eaIDX(); doTST8(read8(a_)); return 4; }
    case 0x6E: /* JMP idx */ pc = eaIDX(); return 3;
    case 0x6F: /* CLR idx */ { uint16_t a_ = eaIDX(); write8(a_, doCLR8()); return 6; }

    // ── Page 7: extended memory ops (and HD6303 dir variants of AIM/OIM/EIM/TIM) ─
    case 0x70: /* NEG ext */ { uint16_t a_ = eaEXT(); write8(a_, doNEG8(read8(a_))); return 6; }
    case 0x71: /* AIM dir (HD6303) */ {
        uint8_t imm = fetchByte(); uint16_t a_ = eaDIR();
        uint8_t res = read8(a_) & imm;
        write8(a_, res); setNZ8(res); setFlag(CCR_V, false);
        return 6;
    }
    case 0x72: /* OIM dir (HD6303) */ {
        uint8_t imm = fetchByte(); uint16_t a_ = eaDIR();
        uint8_t res = read8(a_) | imm;
        write8(a_, res); setNZ8(res); setFlag(CCR_V, false);
        return 6;
    }
    case 0x73: /* COM ext */ { uint16_t a_ = eaEXT(); write8(a_, doCOM8(read8(a_))); return 6; }
    case 0x74: /* LSR ext */ { uint16_t a_ = eaEXT(); write8(a_, doLSR8(read8(a_))); return 6; }
    case 0x75: /* EIM dir (HD6303) */ {
        uint8_t imm = fetchByte(); uint16_t a_ = eaDIR();
        uint8_t res = read8(a_) ^ imm;
        write8(a_, res); setNZ8(res); setFlag(CCR_V, false);
        return 6;
    }
    case 0x76: /* ROR ext */ { uint16_t a_ = eaEXT(); write8(a_, doROR8(read8(a_))); return 6; }
    case 0x77: /* ASR ext */ { uint16_t a_ = eaEXT(); write8(a_, doASR8(read8(a_))); return 6; }
    case 0x78: /* ASL ext */ { uint16_t a_ = eaEXT(); write8(a_, doASL8(read8(a_))); return 6; }
    case 0x79: /* ROL ext */ { uint16_t a_ = eaEXT(); write8(a_, doROL8(read8(a_))); return 6; }
    case 0x7A: /* DEC ext */ { uint16_t a_ = eaEXT(); write8(a_, doDEC8(read8(a_))); return 6; }
    case 0x7B: /* TIM dir (HD6303) */ {
        uint8_t imm = fetchByte(); uint16_t a_ = eaDIR();
        doBIT8(read8(a_), imm);
        return 4;
    }
    case 0x7C: /* INC ext */ { uint16_t a_ = eaEXT(); write8(a_, doINC8(read8(a_))); return 6; }
    case 0x7D: /* TST ext */ { uint16_t a_ = eaEXT(); doTST8(read8(a_)); return 4; }
    case 0x7E: /* JMP ext */ pc = eaEXT(); return 3;
    case 0x7F: /* CLR ext */ { uint16_t a_ = eaEXT(); write8(a_, doCLR8()); return 6; }

    // ── Page 8: A-register immediate / misc immediate ─────────────
    case 0x80: /* SUBA imm */ a = doSUB8(a, fetchByte(), 0); return 2;
    case 0x81: /* CMPA imm */ doCMP8(a, fetchByte()); return 2;
    case 0x82: /* SBCA imm */ a = doSUB8(a, fetchByte(), flag(CCR_C) ? 1 : 0); return 2;
    case 0x83: /* SUBD imm */ {
        uint16_t imm = fetchWord();
        setD(doSUB16(getD(), imm));
        return 4;
    }
    case 0x84: /* ANDA imm */ a = doAND8(a, fetchByte()); return 2;
    case 0x85: /* BITA imm */ doBIT8(a, fetchByte()); return 2;
    case 0x86: /* LDAA imm */ a = fetchByte(); setNZ8(a); setFlag(CCR_V, false); return 2;
    case 0x88: /* EORA imm */ a = doEOR8(a, fetchByte()); return 2;
    case 0x89: /* ADCA imm */ a = doADD8(a, fetchByte(), flag(CCR_C) ? 1 : 0); return 2;
    case 0x8A: /* ORAA imm */ a = doOR8(a, fetchByte()); return 2;
    case 0x8B: /* ADDA imm */ a = doADD8(a, fetchByte(), 0); return 2;
    case 0x8C: /* CPX imm  */ doCMP16(x, fetchWord()); return 4;
    case 0x8D: /* BSR rel  */ {
        int8_t off = int8_t(fetchByte());
        push16(pc);
        pc = uint16_t(pc + off);
        return 6;
    }
    case 0x8E: /* LDS imm  */ sp = fetchWord(); setNZ16(sp); setFlag(CCR_V, false); return 3;

    // ── Page 9: A-register direct ──────────────────────────────────
    case 0x90: /* SUBA dir */ a = doSUB8(a, read8(eaDIR()), 0); return 3;
    case 0x91: /* CMPA dir */ doCMP8(a, read8(eaDIR())); return 3;
    case 0x92: /* SBCA dir */ a = doSUB8(a, read8(eaDIR()), flag(CCR_C) ? 1 : 0); return 3;
    case 0x93: /* SUBD dir */ setD(doSUB16(getD(), read16(eaDIR()))); return 5;
    case 0x94: /* ANDA dir */ a = doAND8(a, read8(eaDIR())); return 3;
    case 0x95: /* BITA dir */ doBIT8(a, read8(eaDIR())); return 3;
    case 0x96: /* LDAA dir */ a = read8(eaDIR()); setNZ8(a); setFlag(CCR_V, false); return 3;
    case 0x97: /* STAA dir */ { uint16_t a_ = eaDIR(); write8(a_, a); setNZ8(a); setFlag(CCR_V, false); return 3; }
    case 0x98: /* EORA dir */ a = doEOR8(a, read8(eaDIR())); return 3;
    case 0x99: /* ADCA dir */ a = doADD8(a, read8(eaDIR()), flag(CCR_C) ? 1 : 0); return 3;
    case 0x9A: /* ORAA dir */ a = doOR8(a, read8(eaDIR())); return 3;
    case 0x9B: /* ADDA dir */ a = doADD8(a, read8(eaDIR()), 0); return 3;
    case 0x9C: /* CPX dir  */ doCMP16(x, read16(eaDIR())); return 5;
    case 0x9D: /* JSR dir  */ {
        uint16_t a_ = eaDIR();
        push16(pc);
        pc = a_;
        return 5;
    }
    case 0x9E: /* LDS dir  */ sp = read16(eaDIR()); setNZ16(sp); setFlag(CCR_V, false); return 4;
    case 0x9F: /* STS dir  */ { uint16_t a_ = eaDIR(); write16(a_, sp); setNZ16(sp); setFlag(CCR_V, false); return 4; }

    // ── Page A: A-register indexed ─────────────────────────────────
    case 0xA0: /* SUBA idx */ a = doSUB8(a, read8(eaIDX()), 0); return 4;
    case 0xA1: /* CMPA idx */ doCMP8(a, read8(eaIDX())); return 4;
    case 0xA2: /* SBCA idx */ a = doSUB8(a, read8(eaIDX()), flag(CCR_C) ? 1 : 0); return 4;
    case 0xA3: /* SUBD idx */ setD(doSUB16(getD(), read16(eaIDX()))); return 6;
    case 0xA4: /* ANDA idx */ a = doAND8(a, read8(eaIDX())); return 4;
    case 0xA5: /* BITA idx */ doBIT8(a, read8(eaIDX())); return 4;
    case 0xA6: /* LDAA idx */ a = read8(eaIDX()); setNZ8(a); setFlag(CCR_V, false); return 4;
    case 0xA7: /* STAA idx */ { uint16_t a_ = eaIDX(); write8(a_, a); setNZ8(a); setFlag(CCR_V, false); return 4; }
    case 0xA8: /* EORA idx */ a = doEOR8(a, read8(eaIDX())); return 4;
    case 0xA9: /* ADCA idx */ a = doADD8(a, read8(eaIDX()), flag(CCR_C) ? 1 : 0); return 4;
    case 0xAA: /* ORAA idx */ a = doOR8(a, read8(eaIDX())); return 4;
    case 0xAB: /* ADDA idx */ a = doADD8(a, read8(eaIDX()), 0); return 4;
    case 0xAC: /* CPX idx  */ doCMP16(x, read16(eaIDX())); return 6;
    case 0xAD: /* JSR idx  */ {
        uint16_t a_ = eaIDX();
        push16(pc);
        pc = a_;
        return 5;
    }
    case 0xAE: /* LDS idx  */ sp = read16(eaIDX()); setNZ16(sp); setFlag(CCR_V, false); return 5;
    case 0xAF: /* STS idx  */ { uint16_t a_ = eaIDX(); write16(a_, sp); setNZ16(sp); setFlag(CCR_V, false); return 5; }

    // ── Page B: A-register extended ────────────────────────────────
    case 0xB0: /* SUBA ext */ a = doSUB8(a, read8(eaEXT()), 0); return 4;
    case 0xB1: /* CMPA ext */ doCMP8(a, read8(eaEXT())); return 4;
    case 0xB2: /* SBCA ext */ a = doSUB8(a, read8(eaEXT()), flag(CCR_C) ? 1 : 0); return 4;
    case 0xB3: /* SUBD ext */ setD(doSUB16(getD(), read16(eaEXT()))); return 6;
    case 0xB4: /* ANDA ext */ a = doAND8(a, read8(eaEXT())); return 4;
    case 0xB5: /* BITA ext */ doBIT8(a, read8(eaEXT())); return 4;
    case 0xB6: /* LDAA ext */ a = read8(eaEXT()); setNZ8(a); setFlag(CCR_V, false); return 4;
    case 0xB7: /* STAA ext */ { uint16_t a_ = eaEXT(); write8(a_, a); setNZ8(a); setFlag(CCR_V, false); return 4; }
    case 0xB8: /* EORA ext */ a = doEOR8(a, read8(eaEXT())); return 4;
    case 0xB9: /* ADCA ext */ a = doADD8(a, read8(eaEXT()), flag(CCR_C) ? 1 : 0); return 4;
    case 0xBA: /* ORAA ext */ a = doOR8(a, read8(eaEXT())); return 4;
    case 0xBB: /* ADDA ext */ a = doADD8(a, read8(eaEXT()), 0); return 4;
    case 0xBC: /* CPX ext  */ doCMP16(x, read16(eaEXT())); return 6;
    case 0xBD: /* JSR ext  */ {
        uint16_t a_ = eaEXT();
        push16(pc);
        pc = a_;
        return 6;
    }
    case 0xBE: /* LDS ext  */ sp = read16(eaEXT()); setNZ16(sp); setFlag(CCR_V, false); return 5;
    case 0xBF: /* STS ext  */ { uint16_t a_ = eaEXT(); write16(a_, sp); setNZ16(sp); setFlag(CCR_V, false); return 5; }

    // ── Page C: B-register immediate / D / X immediate ────────────
    case 0xC0: /* SUBB imm */ b = doSUB8(b, fetchByte(), 0); return 2;
    case 0xC1: /* CMPB imm */ doCMP8(b, fetchByte()); return 2;
    case 0xC2: /* SBCB imm */ b = doSUB8(b, fetchByte(), flag(CCR_C) ? 1 : 0); return 2;
    case 0xC3: /* ADDD imm */ {
        uint16_t imm = fetchWord();
        setD(doADD16(getD(), imm));
        return 4;
    }
    case 0xC4: /* ANDB imm */ b = doAND8(b, fetchByte()); return 2;
    case 0xC5: /* BITB imm */ doBIT8(b, fetchByte()); return 2;
    case 0xC6: /* LDAB imm */ b = fetchByte(); setNZ8(b); setFlag(CCR_V, false); return 2;
    case 0xC8: /* EORB imm */ b = doEOR8(b, fetchByte()); return 2;
    case 0xC9: /* ADCB imm */ b = doADD8(b, fetchByte(), flag(CCR_C) ? 1 : 0); return 2;
    case 0xCA: /* ORAB imm */ b = doOR8(b, fetchByte()); return 2;
    case 0xCB: /* ADDB imm */ b = doADD8(b, fetchByte(), 0); return 2;
    case 0xCC: /* LDD imm  */ {
        uint16_t v = fetchWord();
        setD(v); setNZ16(v); setFlag(CCR_V, false);
        return 3;
    }
    case 0xCE: /* LDX imm  */ x = fetchWord(); setNZ16(x); setFlag(CCR_V, false); return 3;

    // ── Page D: B-register direct ──────────────────────────────────
    case 0xD0: /* SUBB dir */ b = doSUB8(b, read8(eaDIR()), 0); return 3;
    case 0xD1: /* CMPB dir */ doCMP8(b, read8(eaDIR())); return 3;
    case 0xD2: /* SBCB dir */ b = doSUB8(b, read8(eaDIR()), flag(CCR_C) ? 1 : 0); return 3;
    case 0xD3: /* ADDD dir */ setD(doADD16(getD(), read16(eaDIR()))); return 5;
    case 0xD4: /* ANDB dir */ b = doAND8(b, read8(eaDIR())); return 3;
    case 0xD5: /* BITB dir */ doBIT8(b, read8(eaDIR())); return 3;
    case 0xD6: /* LDAB dir */ b = read8(eaDIR()); setNZ8(b); setFlag(CCR_V, false); return 3;
    case 0xD7: /* STAB dir */ { uint16_t a_ = eaDIR(); write8(a_, b); setNZ8(b); setFlag(CCR_V, false); return 3; }
    case 0xD8: /* EORB dir */ b = doEOR8(b, read8(eaDIR())); return 3;
    case 0xD9: /* ADCB dir */ b = doADD8(b, read8(eaDIR()), flag(CCR_C) ? 1 : 0); return 3;
    case 0xDA: /* ORAB dir */ b = doOR8(b, read8(eaDIR())); return 3;
    case 0xDB: /* ADDB dir */ b = doADD8(b, read8(eaDIR()), 0); return 3;
    case 0xDC: /* LDD dir  */ {
        uint16_t v = read16(eaDIR());
        setD(v); setNZ16(v); setFlag(CCR_V, false);
        return 4;
    }
    case 0xDD: /* STD dir  */ {
        uint16_t a_ = eaDIR(); uint16_t d = getD();
        write16(a_, d); setNZ16(d); setFlag(CCR_V, false);
        return 4;
    }
    case 0xDE: /* LDX dir  */ x = read16(eaDIR()); setNZ16(x); setFlag(CCR_V, false); return 4;
    case 0xDF: /* STX dir  */ { uint16_t a_ = eaDIR(); write16(a_, x); setNZ16(x); setFlag(CCR_V, false); return 4; }

    // ── Page E: B-register indexed ─────────────────────────────────
    case 0xE0: /* SUBB idx */ b = doSUB8(b, read8(eaIDX()), 0); return 4;
    case 0xE1: /* CMPB idx */ doCMP8(b, read8(eaIDX())); return 4;
    case 0xE2: /* SBCB idx */ b = doSUB8(b, read8(eaIDX()), flag(CCR_C) ? 1 : 0); return 4;
    case 0xE3: /* ADDD idx */ setD(doADD16(getD(), read16(eaIDX()))); return 6;
    case 0xE4: /* ANDB idx */ b = doAND8(b, read8(eaIDX())); return 4;
    case 0xE5: /* BITB idx */ doBIT8(b, read8(eaIDX())); return 4;
    case 0xE6: /* LDAB idx */ b = read8(eaIDX()); setNZ8(b); setFlag(CCR_V, false); return 4;
    case 0xE7: /* STAB idx */ { uint16_t a_ = eaIDX(); write8(a_, b); setNZ8(b); setFlag(CCR_V, false); return 4; }
    case 0xE8: /* EORB idx */ b = doEOR8(b, read8(eaIDX())); return 4;
    case 0xE9: /* ADCB idx */ b = doADD8(b, read8(eaIDX()), flag(CCR_C) ? 1 : 0); return 4;
    case 0xEA: /* ORAB idx */ b = doOR8(b, read8(eaIDX())); return 4;
    case 0xEB: /* ADDB idx */ b = doADD8(b, read8(eaIDX()), 0); return 4;
    case 0xEC: /* LDD idx  */ {
        uint16_t v = read16(eaIDX());
        setD(v); setNZ16(v); setFlag(CCR_V, false);
        return 5;
    }
    case 0xED: /* STD idx  */ {
        uint16_t a_ = eaIDX(); uint16_t d = getD();
        write16(a_, d); setNZ16(d); setFlag(CCR_V, false);
        return 5;
    }
    case 0xEE: /* LDX idx  */ x = read16(eaIDX()); setNZ16(x); setFlag(CCR_V, false); return 5;
    case 0xEF: /* STX idx  */ { uint16_t a_ = eaIDX(); write16(a_, x); setNZ16(x); setFlag(CCR_V, false); return 5; }

    // ── Page F: B-register extended ────────────────────────────────
    case 0xF0: /* SUBB ext */ b = doSUB8(b, read8(eaEXT()), 0); return 4;
    case 0xF1: /* CMPB ext */ doCMP8(b, read8(eaEXT())); return 4;
    case 0xF2: /* SBCB ext */ b = doSUB8(b, read8(eaEXT()), flag(CCR_C) ? 1 : 0); return 4;
    case 0xF3: /* ADDD ext */ setD(doADD16(getD(), read16(eaEXT()))); return 6;
    case 0xF4: /* ANDB ext */ b = doAND8(b, read8(eaEXT())); return 4;
    case 0xF5: /* BITB ext */ doBIT8(b, read8(eaEXT())); return 4;
    case 0xF6: /* LDAB ext */ b = read8(eaEXT()); setNZ8(b); setFlag(CCR_V, false); return 4;
    case 0xF7: /* STAB ext */ { uint16_t a_ = eaEXT(); write8(a_, b); setNZ8(b); setFlag(CCR_V, false); return 4; }
    case 0xF8: /* EORB ext */ b = doEOR8(b, read8(eaEXT())); return 4;
    case 0xF9: /* ADCB ext */ b = doADD8(b, read8(eaEXT()), flag(CCR_C) ? 1 : 0); return 4;
    case 0xFA: /* ORAB ext */ b = doOR8(b, read8(eaEXT())); return 4;
    case 0xFB: /* ADDB ext */ b = doADD8(b, read8(eaEXT()), 0); return 4;
    case 0xFC: /* LDD ext  */ {
        uint16_t v = read16(eaEXT());
        setD(v); setNZ16(v); setFlag(CCR_V, false);
        return 5;
    }
    case 0xFD: /* STD ext  */ {
        uint16_t a_ = eaEXT(); uint16_t d = getD();
        write16(a_, d); setNZ16(d); setFlag(CCR_V, false);
        return 5;
    }
    case 0xFE: /* LDX ext  */ x = read16(eaEXT()); setNZ16(x); setFlag(CCR_V, false); return 5;
    case 0xFF: /* STX ext  */ { uint16_t a_ = eaEXT(); write16(a_, x); setNZ16(x); setFlag(CCR_V, false); return 5; }

    default:
        return unimplemented(op);
    }
}

int64_t HD6303::run(int64_t budget) {
    int64_t spent = 0;
    while (spent < budget) {
        int64_t consumed = step();
        if (consumed <= 0) consumed = 1;
        tickTimer(consumed);
        spent += consumed;
        if (halted) break;
    }
    return spent;
}
