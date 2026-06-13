// license:BSD-3-Clause
// copyright-holders:Aaron Giles, Sandro Ronco (MAME m6800/m6801)
//                  + adaptation by the Psion emulator project, 2026.
//
// Hitachi HD6303X CPU core.
//
// The HD6303X is a CMOS derivative of the Motorola 6803 with extra
// instructions (XGDX, SLP, AIM, OIM, EIM, TIM) and on-chip 128 bytes
// of RAM, a free-running counter, an 8-bit timer with output compare /
// input capture, an SCI, and four 8-bit ports. Used in the Psion
// Organiser II at 3.6864 MHz / 4 = 0.9216 MHz E-clock.
//
// Reference: Hitachi HD6301/HD6303 series handbook
//   https://www.jaapsch.net/psion/pdffiles/hd6301-3_handbook.pdf
// MAME implementation we mirror semantics from (BSD-3-Clause):
//   src/devices/cpu/m6800/m6800.cpp + m6801.cpp + 6800ops.hxx
//
// What's implemented in this revision:
//   - Full 6800/6801/6303 opcode set (all 256 entries, with
//     unimplemented ones halting the CPU and printing a diagnostic
//     so the harness can drive opcode-by-opcode bring-up).
//   - All four addressing modes (immediate, direct, extended, indexed).
//   - IRQ / NMI / SWI dispatch.
//   - On-chip 128 bytes of RAM at $0080-$00FF.
//   - Stub on-chip register window at $0000-$001F (read returns 0,
//     writes ignored). The Organiser II driver decodes external MMIO
//     starting at $0100; the internal regs control the on-chip timer
//     and ports, which are TBD.

#pragma once

#include <array>
#include <cstdint>
#include <functional>

class HD6303Bus {
public:
    virtual ~HD6303Bus() = default;
    // 16-bit address space, byte-only accesses (the 6303 has no word
    // bus ops; word reads/writes are two byte cycles handled inside
    // the CPU core).
    virtual uint8_t readByte(uint16_t addr) = 0;
    virtual void    writeByte(uint16_t addr, uint8_t v) = 0;
};

class HD6303 {
public:
    // CCR (Condition Code Register) bit assignments.
    static constexpr uint8_t CCR_C = 0x01;   // Carry
    static constexpr uint8_t CCR_V = 0x02;   // Overflow
    static constexpr uint8_t CCR_Z = 0x04;   // Zero
    static constexpr uint8_t CCR_N = 0x08;   // Negative
    static constexpr uint8_t CCR_I = 0x10;   // IRQ mask
    static constexpr uint8_t CCR_H = 0x20;   // Half-carry
    static constexpr uint8_t CCR_FIXED = 0xC0; // Bits 6-7 always set

    // Interrupt vectors. Standard 6800-family layout, top of the
    // 64 KiB address space. The RES vector is fetched on reset.
    static constexpr uint16_t VEC_TRAP = 0xFFEE;
    static constexpr uint16_t VEC_SCI  = 0xFFF0;
    static constexpr uint16_t VEC_TOF  = 0xFFF2;
    static constexpr uint16_t VEC_OCF  = 0xFFF4;
    static constexpr uint16_t VEC_ICF  = 0xFFF6;
    static constexpr uint16_t VEC_IRQ  = 0xFFF8;
    static constexpr uint16_t VEC_SWI  = 0xFFFA;
    static constexpr uint16_t VEC_NMI  = 0xFFFC;
    static constexpr uint16_t VEC_RES  = 0xFFFE;

    // Visible register file.
    uint8_t  a = 0, b = 0;
    uint16_t x = 0;
    uint16_t sp = 0;
    uint16_t pc = 0;
    uint8_t  ccr = CCR_FIXED | CCR_I;
    bool     irqLine = false;
    bool     nmiLine = false;
    bool     halted  = false;     // SLP / WAI sets this
    bool     waiting = false;     // True during WAI/SLP (resumes on IRQ/NMI)
    bool     waiContext = false;  // True if WAI pre-pushed context (skip push in service)

    explicit HD6303(HD6303Bus &bus) : bus_(bus) {}

    // Optional callbacks used by the Psion Organiser II driver to feed
    // keyboard column scan data into Port 5 ($17) and datapak control
    // status into Port 6 ($18). When set, reads of those registers
    // return cb() instead of m_iregs[$X]. Other devices (none yet)
    // can leave these null. Port 5 is keyboard + battery + ON key per
    // MAME's psion_state::port5_r; Port 6 is the datapak control
    // OR-bus.
    std::function<uint8_t()> port5Reader;
    std::function<uint8_t()> port6Reader;
    // Optional handlers for Port 2 ($03) data and Port 6 ($18) writes
    // — Port 2 is the datapak's 8-bit data bus; Port 6 writes drive
    // CLK / RES / OE / slot-select / power-on lines. With no callback
    // set, reads return the iregs value and writes go to iregs.
    std::function<uint8_t()> port2Reader;
    std::function<void(uint8_t)> port2Writer;
    std::function<void(uint8_t)> port6Writer;

    void reset();

    // Decode + execute one instruction. Returns cycles consumed.
    int64_t step();
    // Pages 6-F dispatch (memory-operand opcodes). Split out from
    // step() purely to keep each function under ~250 lines.
    int64_t step2(uint8_t op);

    // Run for a budget. Default loops on step() until budget exhausts.
    int64_t run(int64_t budget);

    void setIrqLine(bool s) { irqLine = s; }
    void setNmiLine(bool s) { nmiLine = s; }

    // Combined accumulators (D = A:B).
    uint16_t getD() const { return uint16_t(a) << 8 | b; }
    void     setD(uint16_t v) { a = uint8_t(v >> 8); b = uint8_t(v); }

private:
    HD6303Bus &bus_;

    // On-chip 192 bytes of RAM at $0040-$00FF (HD6303X-specific layout
    // per MAME hd6303x_mem map). The earlier 6301/6803/6303R variants
    // had only 128 bytes at $0080-$00FF; the HD6303X extends usable
    // RAM downward by 64 bytes. Critical for the Psion LZ because the
    // OS's zero-page data structures (latched cold-boot state, scratch
    // pointers) live in $0040-$007F. Reading those as "external bus
    // open" sends the OS down a wrong cold-boot branch.
    std::array<uint8_t, 192> m_iram{};

    // On-chip register window at $0000-$001F. Read/write goes through
    // dedicated paths for the timer registers ($08-$0C); other regs
    // mirror to a simple byte array for now.
    std::array<uint8_t, 32>  m_iregs{};

    // ── On-chip 16-bit free-running counter + output compare ────────
    // The FRC ticks every E-clock cycle (one per CPU cycle in our
    // model). OCR contains the value at which the comparator fires
    // OCF. TCSR ($08) bit assignments:
    //   bit 7 ICF (input capture flag) — unused in the LZ
    //   bit 6 OCF (output compare flag) — set on FRC == OCR
    //   bit 5 TOF (timer overflow flag) — set on FRC wrap to 0
    //   bit 3 EOCI (enable OCF IRQ)
    //   bit 2 ETOI (enable TOF IRQ)
    // Reading TCSR clears the read latches; the flags themselves are
    // cleared by a subsequent read of OCR (for OCF) or FRC (for TOF).
    // We keep things simple: clear OCF on any OCR read; clear TOF on
    // any FRC read.
    uint16_t m_frc = 0;
    uint16_t m_ocr = 0xFFFF;
    uint8_t  m_tcsrLatchedFlags = 0;  // last TCSR read snapshot

public:
    void tickTimer(int64_t consumed);
private:

    // Tracks whether we've already reported the first unimplemented
    // opcode so the harness gets one diagnostic line per session.
    bool _firstOpcodeReported = false;

    // ── Bus access (intercepts on-chip RAM + register window) ───────
    uint8_t  read8(uint16_t addr);
    void     write8(uint16_t addr, uint8_t v);
    uint16_t read16(uint16_t addr);
    void     write16(uint16_t addr, uint16_t v);

    // ── Operand fetch (advances PC) ─────────────────────────────────
    uint8_t  fetchOp() { uint8_t v = read8(pc); pc++; return v; }
    uint8_t  fetchByte() { uint8_t v = read8(pc); pc++; return v; }
    uint16_t fetchWord() {
        uint8_t hi = read8(pc); pc++;
        uint8_t lo = read8(pc); pc++;
        return uint16_t(hi) << 8 | lo;
    }

    // ── Effective-address helpers per addressing mode ───────────────
    uint16_t eaDIR() { return fetchByte(); }                 // zero-page
    uint16_t eaEXT() { return fetchWord(); }                 // 16-bit absolute
    uint16_t eaIDX() { return uint16_t(x + fetchByte()); }   // X + uint8_t

    // ── Stack ───────────────────────────────────────────────────────
    void push8 (uint8_t v)  { write8(sp, v); sp--; }
    uint8_t pop8 ()         { sp++; return read8(sp); }
    void push16(uint16_t v) { push8(uint8_t(v & 0xFF)); push8(uint8_t(v >> 8)); }
    uint16_t pop16()        { uint8_t hi = pop8(); uint8_t lo = pop8();
                              return uint16_t(hi) << 8 | lo; }

    // ── Flag helpers ────────────────────────────────────────────────
    void setNZ8 (uint8_t v) {
        ccr = (ccr & ~(CCR_N | CCR_Z))
            | (v & 0x80 ? CCR_N : 0)
            | (v == 0   ? CCR_Z : 0);
    }
    void setNZ16(uint16_t v) {
        ccr = (ccr & ~(CCR_N | CCR_Z))
            | (v & 0x8000 ? CCR_N : 0)
            | (v == 0     ? CCR_Z : 0);
    }
    void setFlag(uint8_t mask, bool on) {
        ccr = on ? (ccr | mask) : (ccr & ~mask);
    }
    bool flag(uint8_t mask) const { return (ccr & mask) != 0; }

    // ── Interrupt service ───────────────────────────────────────────
    void serviceInterrupt(uint16_t vec);

    // ── Operation helpers (modify A/B/mem in place) ─────────────────
    uint8_t  doADD8(uint8_t l, uint8_t r, uint8_t cIn);
    uint8_t  doSUB8(uint8_t l, uint8_t r, uint8_t cIn);
    uint16_t doADD16(uint16_t l, uint16_t r);
    uint16_t doSUB16(uint16_t l, uint16_t r);
    void     doCMP8(uint8_t l, uint8_t r);
    void     doCMP16(uint16_t l, uint16_t r);
    uint8_t  doAND8(uint8_t l, uint8_t r);
    uint8_t  doOR8 (uint8_t l, uint8_t r);
    uint8_t  doEOR8(uint8_t l, uint8_t r);
    void     doBIT8(uint8_t l, uint8_t r);
    uint8_t  doINC8(uint8_t v);
    uint8_t  doDEC8(uint8_t v);
    uint8_t  doNEG8(uint8_t v);
    uint8_t  doCOM8(uint8_t v);
    uint8_t  doASL8(uint8_t v);
    uint8_t  doLSR8(uint8_t v);
    uint8_t  doROL8(uint8_t v);
    uint8_t  doROR8(uint8_t v);
    uint8_t  doASR8(uint8_t v);
    void     doTST8(uint8_t v);
    uint8_t  doCLR8();

    // Branch helper: if `cond`, PC += signed_offset (operand byte).
    void     doBRA(bool cond);

    // Reports an unimplemented opcode, halts the CPU.
    int64_t  unimplemented(uint8_t op);
};
