// license:BSD-3-Clause
// copyright-holders: Bryan McPhail, ASG (MAME NEC port)
//                    + adaptation by the Psion emulator project, 2026.
//
// V30 misc dispatch — stack, flag ops, I/O, prefixes, NOP, HLT,
// string ops, shifts/rotates, REP prefix.
//
// Returns:
//   >=0  opcode handled, cycles consumed
//   -1   not our category
//   -2   sentinel: segment-override prefix handled; don't clear override

#include "v30_internals.h"

#include <cstdint>
#include <cstdio>

using namespace V30Detail;

namespace V30Detail {

// ──────────────────────────────────────────────────────────────────────
// String primitives (single iteration). The source segment for MOVS /
// CMPS / LODS defaults to DS and honours a segment prefix; the dest
// segment for MOVS / STOS / SCAS is always ES.

static inline void stringMovsb(V30& c) {
    uint16_t srcSeg = resolveSeg(c, 3);
    uint8_t v = c.busRef().readMemByte(lin(srcSeg, c.regs.w[6]));
    c.busRef().writeMemByte(lin(c.sregs[0], c.regs.w[7]), v);
    int16_t step = c.df ? -1 : +1;
    c.regs.w[6] = uint16_t(c.regs.w[6] + step);
    c.regs.w[7] = uint16_t(c.regs.w[7] + step);
}

static inline void stringMovsw(V30& c) {
    uint16_t srcSeg = resolveSeg(c, 3);
    uint16_t v = c.busRef().readMemWord(lin(srcSeg, c.regs.w[6]));
    c.busRef().writeMemWord(lin(c.sregs[0], c.regs.w[7]), v);
    int16_t step = c.df ? -2 : +2;
    c.regs.w[6] = uint16_t(c.regs.w[6] + step);
    c.regs.w[7] = uint16_t(c.regs.w[7] + step);
}

static inline void stringCmpsb(V30& c) {
    uint16_t srcSeg = resolveSeg(c, 3);
    uint8_t a = c.busRef().readMemByte(lin(srcSeg, c.regs.w[6]));
    uint8_t b = c.busRef().readMemByte(lin(c.sregs[0], c.regs.w[7]));
    setSubFlags8(c, a, b, uint16_t(a - b));
    int16_t step = c.df ? -1 : +1;
    c.regs.w[6] = uint16_t(c.regs.w[6] + step);
    c.regs.w[7] = uint16_t(c.regs.w[7] + step);
}

static inline void stringCmpsw(V30& c) {
    uint16_t srcSeg = resolveSeg(c, 3);
    uint16_t a = c.busRef().readMemWord(lin(srcSeg, c.regs.w[6]));
    uint16_t b = c.busRef().readMemWord(lin(c.sregs[0], c.regs.w[7]));
    setSubFlags16(c, a, b, uint32_t(a - b));
    int16_t step = c.df ? -2 : +2;
    c.regs.w[6] = uint16_t(c.regs.w[6] + step);
    c.regs.w[7] = uint16_t(c.regs.w[7] + step);
}

static inline void stringStosb(V30& c) {
    c.busRef().writeMemByte(lin(c.sregs[0], c.regs.w[7]), c.regs.b[0]);
    int16_t step = c.df ? -1 : +1;
    c.regs.w[7] = uint16_t(c.regs.w[7] + step);
}

static inline void stringStosw(V30& c) {
    c.busRef().writeMemWord(lin(c.sregs[0], c.regs.w[7]), c.regs.w[0]);
    int16_t step = c.df ? -2 : +2;
    c.regs.w[7] = uint16_t(c.regs.w[7] + step);
}

static inline void stringLodsb(V30& c) {
    uint16_t srcSeg = resolveSeg(c, 3);
    c.regs.b[0] = c.busRef().readMemByte(lin(srcSeg, c.regs.w[6]));
    int16_t step = c.df ? -1 : +1;
    c.regs.w[6] = uint16_t(c.regs.w[6] + step);
}

static inline void stringLodsw(V30& c) {
    uint16_t srcSeg = resolveSeg(c, 3);
    c.regs.w[0] = c.busRef().readMemWord(lin(srcSeg, c.regs.w[6]));
    int16_t step = c.df ? -2 : +2;
    c.regs.w[6] = uint16_t(c.regs.w[6] + step);
}

static inline void stringScasb(V30& c) {
    uint8_t a = c.regs.b[0];
    uint8_t b = c.busRef().readMemByte(lin(c.sregs[0], c.regs.w[7]));
    setSubFlags8(c, a, b, uint16_t(a - b));
    int16_t step = c.df ? -1 : +1;
    c.regs.w[7] = uint16_t(c.regs.w[7] + step);
}

static inline void stringScasw(V30& c) {
    uint16_t a = c.regs.w[0];
    uint16_t b = c.busRef().readMemWord(lin(c.sregs[0], c.regs.w[7]));
    setSubFlags16(c, a, b, uint32_t(a - b));
    int16_t step = c.df ? -2 : +2;
    c.regs.w[7] = uint16_t(c.regs.w[7] + step);
}

// Real 8086 cycle count for one un-REPed string instruction (Intel's
// 8086 timings). The V30 figures below are roughly a quarter of these,
// so the flat 2x an I8086 instance would otherwise apply undercharges
// them badly; i8086Exact pins the real number instead. See
// V30::absCycles.
static int i8086StringCycles(uint8_t op) {
    switch (op) {
    case 0xA4: case 0xA5: return 18;  // MOVS
    case 0xA6: case 0xA7: return 22;  // CMPS
    case 0xAA: case 0xAB: return 11;  // STOS
    case 0xAC: case 0xAD: return 12;  // LODS
    case 0xAE: case 0xAF: return 15;  // SCAS
    default:              return 0;
    }
}

// Run a single string op iteration (no REP). Returns per-iter cycles.
static int stringStep(V30& c, uint8_t op) {
    switch (op) {
    case 0xA4: stringMovsb(c); return 8;
    case 0xA5: stringMovsw(c); return 8;
    case 0xA6: stringCmpsb(c); return 8;
    case 0xA7: stringCmpsw(c); return 8;
    case 0xAA: stringStosb(c); return 4;
    case 0xAB: stringStosw(c); return 4;
    case 0xAC: stringLodsb(c); return 4;
    case 0xAD: stringLodsw(c); return 4;
    case 0xAE: stringScasb(c); return 4;
    case 0xAF: stringScasw(c); return 4;
    default:   return 0;
    }
}

// Execute a REP-prefixed string op. `repZ` is true for REPE/REPZ (0xF3),
// false for REPNE/REPNZ (0xF2). For MOVS/STOS/LODS the ZF condition is
// ignored; for CMPS/SCAS the loop terminates when ZF no longer matches.
static int64_t repString(V30& c, uint8_t op, bool repZ) {
    bool checksZ = (op == 0xA6 || op == 0xA7 || op == 0xAE || op == 0xAF);
    uint32_t count = 0;
    while (c.regs.w[1] != 0) {
        stringStep(c, op);
        c.regs.w[1] = uint16_t(c.regs.w[1] - 1);
        ++count;
        if (checksZ) {
            if (repZ  && !zf(c)) break;
            if (!repZ &&  zf(c)) break;
        }
    }
    // Intel's 8086 figures for a REP-prefixed string op are 9 + n*per-iter,
    // where the per-iter cost is a cycle or two under the un-REPed one
    // (the prefix is fetched once): MOVS 17, CMPS 22, SCAS 15, LODS 13,
    // STOS 10. Charging the un-REPed figure per iteration plus the 9-cycle
    // prefix is within a cycle of that across the set.
    i8086Exact(c, int(9 + int64_t(i8086StringCycles(op)) * int64_t(count)));
    return int64_t(8) * int64_t(count) + 14;
}

// ──────────────────────────────────────────────────────────────────────
// Shift / rotate primitives. `count` is already masked to 5 bits. Each
// helper updates CF / OF / SF / ZF / PF where applicable. A zero count
// leaves all flags unchanged, matching V30/8086 behaviour.

static inline uint8_t shiftRotateByte(V30& c, uint8_t op, uint8_t val, uint8_t count) {
    if (count == 0) return val;
    uint32_t r = val;
    bool one = (count == 1);
    uint8_t oldSign = val & 0x80;
    bool newCF = cf(c);
    bool newOF = of(c);
    switch (op) {
    case 0: // ROL
        for (uint8_t i = 0; i < count; ++i) {
            newCF = (r & 0x80) != 0;
            r = uint8_t((r << 1) | (newCF ? 1 : 0));
        }
        c.carryVal = newCF ? 1 : 0;
        if (one) c.overVal = ((uint8_t(r) ^ oldSign) & 0x80) ? 1 : 0;
        return uint8_t(r);
    case 1: // ROR
        for (uint8_t i = 0; i < count; ++i) {
            newCF = (r & 1) != 0;
            r = uint8_t((r >> 1) | (newCF ? 0x80 : 0));
        }
        c.carryVal = newCF ? 1 : 0;
        if (one) c.overVal = ((uint8_t(r) ^ oldSign) & 0x80) ? 1 : 0;
        return uint8_t(r);
    case 2: // RCL
        for (uint8_t i = 0; i < count; ++i) {
            bool oldCF = newCF;
            newCF = (r & 0x80) != 0;
            r = uint8_t((r << 1) | (oldCF ? 1 : 0));
        }
        c.carryVal = newCF ? 1 : 0;
        if (one) c.overVal = (newCF != ((r & 0x80) != 0)) ? 1 : 0;
        return uint8_t(r);
    case 3: // RCR
        for (uint8_t i = 0; i < count; ++i) {
            bool oldCF = newCF;
            newCF = (r & 1) != 0;
            r = uint8_t((r >> 1) | (oldCF ? 0x80 : 0));
        }
        c.carryVal = newCF ? 1 : 0;
        if (one) c.overVal = (((uint8_t(r) ^ oldSign) & 0x80) ? 1 : 0);
        return uint8_t(r);
    case 4: // SHL / SAL
    case 6:
        for (uint8_t i = 0; i < count; ++i) {
            newCF = (r & 0x80) != 0;
            r = uint8_t(r << 1);
        }
        c.carryVal = newCF ? 1 : 0;
        if (one) {
            newOF = (newCF != ((r & 0x80) != 0));
            c.overVal = newOF ? 1 : 0;
        }
        setLogicFlags8(c, uint8_t(r));
        c.carryVal = newCF ? 1 : 0;
        if (one) c.overVal = newOF ? 1 : 0;
        return uint8_t(r);
    case 5: // SHR
        for (uint8_t i = 0; i < count; ++i) {
            newCF = (r & 1) != 0;
            r = uint8_t(r >> 1);
        }
        c.carryVal = newCF ? 1 : 0;
        if (one) newOF = (oldSign != 0);
        setLogicFlags8(c, uint8_t(r));
        c.carryVal = newCF ? 1 : 0;
        if (one) c.overVal = newOF ? 1 : 0;
        return uint8_t(r);
    case 7: { // SAR
        int8_t s = int8_t(val);
        for (uint8_t i = 0; i < count; ++i) {
            newCF = (s & 1) != 0;
            s = int8_t(s >> 1);
        }
        c.carryVal = newCF ? 1 : 0;
        if (one) newOF = false;
        setLogicFlags8(c, uint8_t(s));
        c.carryVal = newCF ? 1 : 0;
        if (one) c.overVal = newOF ? 1 : 0;
        return uint8_t(s);
    }
    }
    return val;
}

static inline uint16_t shiftRotateWord(V30& c, uint8_t op, uint16_t val, uint8_t count) {
    if (count == 0) return val;
    uint32_t r = val;
    bool one = (count == 1);
    uint16_t oldSign = val & 0x8000;
    bool newCF = cf(c);
    bool newOF = of(c);
    switch (op) {
    case 0: // ROL
        for (uint8_t i = 0; i < count; ++i) {
            newCF = (r & 0x8000) != 0;
            r = uint16_t((r << 1) | (newCF ? 1 : 0));
        }
        c.carryVal = newCF ? 1 : 0;
        if (one) c.overVal = ((uint16_t(r) ^ oldSign) & 0x8000) ? 1 : 0;
        return uint16_t(r);
    case 1: // ROR
        for (uint8_t i = 0; i < count; ++i) {
            newCF = (r & 1) != 0;
            r = uint16_t((r >> 1) | (newCF ? 0x8000 : 0));
        }
        c.carryVal = newCF ? 1 : 0;
        if (one) c.overVal = ((uint16_t(r) ^ oldSign) & 0x8000) ? 1 : 0;
        return uint16_t(r);
    case 2: // RCL
        for (uint8_t i = 0; i < count; ++i) {
            bool oldCF = newCF;
            newCF = (r & 0x8000) != 0;
            r = uint16_t((r << 1) | (oldCF ? 1 : 0));
        }
        c.carryVal = newCF ? 1 : 0;
        if (one) c.overVal = (newCF != ((r & 0x8000) != 0)) ? 1 : 0;
        return uint16_t(r);
    case 3: // RCR
        for (uint8_t i = 0; i < count; ++i) {
            bool oldCF = newCF;
            newCF = (r & 1) != 0;
            r = uint16_t((r >> 1) | (oldCF ? 0x8000 : 0));
        }
        c.carryVal = newCF ? 1 : 0;
        if (one) c.overVal = (((uint16_t(r) ^ oldSign) & 0x8000) ? 1 : 0);
        return uint16_t(r);
    case 4: // SHL / SAL
    case 6:
        for (uint8_t i = 0; i < count; ++i) {
            newCF = (r & 0x8000) != 0;
            r = uint16_t(r << 1);
        }
        c.carryVal = newCF ? 1 : 0;
        if (one) newOF = (newCF != ((r & 0x8000) != 0));
        setLogicFlags16(c, uint16_t(r));
        c.carryVal = newCF ? 1 : 0;
        if (one) c.overVal = newOF ? 1 : 0;
        return uint16_t(r);
    case 5: // SHR
        for (uint8_t i = 0; i < count; ++i) {
            newCF = (r & 1) != 0;
            r = uint16_t(r >> 1);
        }
        c.carryVal = newCF ? 1 : 0;
        if (one) newOF = (oldSign != 0);
        setLogicFlags16(c, uint16_t(r));
        c.carryVal = newCF ? 1 : 0;
        if (one) c.overVal = newOF ? 1 : 0;
        return uint16_t(r);
    case 7: { // SAR
        int16_t s = int16_t(val);
        for (uint8_t i = 0; i < count; ++i) {
            newCF = (s & 1) != 0;
            s = int16_t(s >> 1);
        }
        c.carryVal = newCF ? 1 : 0;
        if (one) newOF = false;
        setLogicFlags16(c, uint16_t(s));
        c.carryVal = newCF ? 1 : 0;
        if (one) c.overVal = newOF ? 1 : 0;
        return uint16_t(s);
    }
    }
    return val;
}

int64_t dispatchMisc(V30& c, uint8_t op) {
    switch (op) {

    // NOP
    case 0x90: return 3;

    // HLT
    case 0xF4:
        if (std::getenv("PSION_WAKE_TRACE")) {
            uint32_t pc = (uint32_t(c.sregs[1]) << 4) + uint16_t(c.ip - 1);
            static uint64_t hltCount = 0;
            if (++hltCount <= 10 || hltCount % 100 == 0)
                std::fprintf(stderr,
                    "[hlt] #%llu pc=%05X iflag=%d irqLine=%d\n",
                    (unsigned long long)hltCount, pc, c.iflag, (int)c.irqLine);
        }
        c.halted = true; return 5;

    // ──────── Flag ops ────────
    case 0xF5: // CMC
        c.carryVal ^= 1;
        return 4;
    case 0xF8: c.carryVal = 0; return 4; // CLC
    case 0xF9: c.carryVal = 1; return 4; // STC
    case 0xFA: c.iflag = 0; return 4;    // CLI
    case 0xFB: // STI: IRQs stay inhibited until the NEXT instruction
               // completes (lets `sti; ret` / `sti; hlt` finish first).
        if (!c.iflag) { c.iflag = 1; c.intInhibit = 1; }
        return 4;
    case 0xFC: c.df    = 0; return 4;    // CLD
    case 0xFD: c.df    = 1; return 4;    // STD

    // LAHF / SAHF
    case 0x9F: // LAHF — load low byte of PSW into AH
        c.regs.b[1] = uint8_t(buildPSW(c) & 0xFF);
        return 4;
    case 0x9E: // SAHF — store AH into low byte of PSW (only CF/PF/AF/ZF/SF)
        {
            uint16_t p = buildPSW(c);
            p = (p & 0xFF00) | (c.regs.b[1] & 0xD5) | 0x02;
            loadPSW(c, p);
        }
        return 4;

    // ──────── Segment override prefixes ────────
    case 0x26: setSegOverride(c, 0); return -2; // ES
    case 0x2E: setSegOverride(c, 1); return -2; // CS
    case 0x36: setSegOverride(c, 2); return -2; // SS
    case 0x3E: setSegOverride(c, 3); return -2; // DS

    // ──────── Stack ────────
    // PUSH reg16 (0x50..0x57)
    case 0x50: case 0x51: case 0x52: case 0x53:
    case 0x54: case 0x55: case 0x56: case 0x57:
        push16(c, c.regs.w[op & 7]);
        return 11;
    // POP reg16 (0x58..0x5F)
    case 0x58: case 0x59: case 0x5A: case 0x5B:
    case 0x5C: case 0x5D: case 0x5E: case 0x5F:
        c.regs.w[op & 7] = pop16(c);
        return 8;

    // PUSHA / POPA (80186/V30 extension). PUSHA pushes
    // AX, CX, DX, BX, SP(orig), BP, SI, DI in that order. POPA pops
    // them in reverse, discarding the saved SP (it's already on
    // the stack but we don't restore it — see Intel SDM / NEC V30
    // manual). The Psion Series 3 kernel relies on this for its
    // interrupt prologue/epilogue.
    case 0x60: { // PUSHA
        uint16_t origSp = c.regs.w[4];
        push16(c, c.regs.w[0]); // AX
        push16(c, c.regs.w[1]); // CX
        push16(c, c.regs.w[2]); // DX
        push16(c, c.regs.w[3]); // BX
        push16(c, origSp);      // SP (pre-PUSHA)
        push16(c, c.regs.w[5]); // BP
        push16(c, c.regs.w[6]); // SI
        push16(c, c.regs.w[7]); // DI
        return 17;
    }
    case 0x61: { // POPA
        c.regs.w[7] = pop16(c); // DI
        c.regs.w[6] = pop16(c); // SI
        c.regs.w[5] = pop16(c); // BP
        (void)pop16(c);         // discarded SP
        c.regs.w[3] = pop16(c); // BX
        c.regs.w[2] = pop16(c); // DX
        c.regs.w[1] = pop16(c); // CX
        c.regs.w[0] = pop16(c); // AX
        return 19;
    }

    // PUSH seg
    case 0x06: push16(c, c.sregs[0]); return 10; // PUSH ES
    case 0x0E: push16(c, c.sregs[1]); return 10; // PUSH CS
    case 0x16: push16(c, c.sregs[2]); return 10; // PUSH SS
    case 0x1E: push16(c, c.sregs[3]); return 10; // PUSH DS
    // POP seg (CS disallowed on 8086; allow all for now)
    case 0x07: c.sregs[0] = pop16(c); return 8;
    case 0x17: c.sregs[2] = pop16(c); c.intInhibit = 1; return 8; // POP SS: 1-insn int shadow
    case 0x1F: c.sregs[3] = pop16(c); return 8;

    // PUSHF / POPF
    case 0x9C: push16(c, buildPSW(c)); return 10;
    case 0x9D: loadPSW(c, pop16(c));    return 8;

    // PUSH imm8 / imm16 (V30)
    case 0x6A: push16(c, uint16_t(int16_t(fetchS8(c)))); return 9;
    case 0x68: push16(c, fetch16(c)); return 9;

    // 0x64-0x67 are documented as "no effect" on the i8086 (Intel
    // 8086 reference manual classes them as undefined; on 80286+ they
    // were repurposed as REP-prefix variants and FS/GS overrides).
    // The MC400 boot ROM hits 0x67 once at a RAM-resident PC during
    // EPOC kernel init; treating it as a 1-cycle NOP lets the kernel
    // proceed past it (matches the behaviour observed on real i8086
    // silicon — the bus cycle completes with no architectural side
    // effect). On the V30 these are REPNC / REPC prefixes; we leave
    // those for the V30/V30H variants if/when an EPOC ROM hits them.
    case 0x64: case 0x65: case 0x66: case 0x67:
        if (c.getVariant() == V30Variant::I8086) return 2;
        return -1;

    // CBW / CWD
    case 0x98: // AL sign-extend to AX
        c.regs.w[0] = uint16_t(int16_t(int8_t(c.regs.b[0])));
        return 2;
    case 0x99: // AX sign-extend to DX:AX
        c.regs.w[2] = int16_t(c.regs.w[0]) < 0 ? 0xFFFF : 0x0000;
        return 4;

    // ──────── I/O ────────
    case 0xE4: { // IN AL, imm8
        uint8_t port = fetch8(c);
        c.regs.b[0] = c.busRef().readIoByte(port);
        return 10;
    }
    case 0xE5: { // IN AX, imm8
        uint8_t port = fetch8(c);
        c.regs.w[0] = c.busRef().readIoWord(port);
        return 10;
    }
    case 0xE6: { // OUT imm8, AL
        uint8_t port = fetch8(c);
        c.busRef().writeIoByte(port, c.regs.b[0]);
        return 10;
    }
    case 0xE7: { // OUT imm8, AX
        uint8_t port = fetch8(c);
        c.busRef().writeIoWord(port, c.regs.w[0]);
        return 10;
    }
    case 0xEC: // IN AL, DX
        c.regs.b[0] = c.busRef().readIoByte(c.regs.w[2]);
        return 8;
    case 0xED: // IN AX, DX
        c.regs.w[0] = c.busRef().readIoWord(c.regs.w[2]);
        return 8;
    case 0xEE: // OUT DX, AL
        c.busRef().writeIoByte(c.regs.w[2], c.regs.b[0]);
        return 8;
    case 0xEF: // OUT DX, AX
        c.busRef().writeIoWord(c.regs.w[2], c.regs.w[0]);
        return 8;

    // LOCK — treated as NOP
    case 0xF0: return 2;

    // ──── ESC / coprocessor opcodes 0xD8..0xDF ────
    // On the V30 with no 8087 attached these decode-and-consume the
    // ModR/M byte (plus any disp) but do nothing. This is essential —
    // ROMs use them as "known-length NOPs" inside timing loops
    // (e.g. series3_v1.91f_eng.bin's 0xce74c delay loop is
    // `DB AD 03 D0; LOOP $-5` — if we halt here the CPU stops before
    // the kernel ever finishes its RAM init).
    case 0xD8: case 0xD9: case 0xDA: case 0xDB:
    case 0xDC: case 0xDD: case 0xDE: case 0xDF: {
        uint8_t mrm = fetch8(c);
        Operand o = decodeModRM(c, mrm);
        if (!o.isReg) (void)readOp8(c, o); // consume any memory operand side-effectlessly
        return 2;
    }

    // ──── 0x0F: i8086 `POP CS` OR V30/V20 extended-opcode prefix ────
    // On the i8086 the byte 0x0F is `POP CS` — a 1-byte instruction
    // popping a 16-bit segment from the top of stack into CS. Intel
    // dropped POP CS on the 80186 and NEC reused the freed encoding
    // as the prefix to a V30/V20 extended-opcode map (TEST1/CLR1/
    // SET1/NOT1 0x10-0x1F, BCD/INS/EXT ops 0x20-0x3F). MC400 / MC200
    // / MC Word were all assembled for the i8086 (per MAME mc400.cpp
    // `I8086(...)`) so an I8086-variant V30 instance must take the
    // POP-CS path; native V30 instances (Series 3 / 3a / 3c / 3mx /
    // Siena / Workabout) take the extended-opcode path.
    case 0x0F: {
        if (c.getVariant() == V30Variant::I8086) {
            // POP CS — 8 cycles per Intel's i8086 manual.
            c.sregs[1] = pop16(c);
            return 8;
        }
        uint8_t sub = fetch8(c);
        switch (sub) {
        // TEST1/CLR1/SET1/NOT1 (reg or mem8/mem16) with bit index in CL
        case 0x10: case 0x12: case 0x14: case 0x16: {
            uint8_t mrm = fetch8(c);
            Operand o = decodeModRM(c, mrm);
            uint8_t v = readOp8(c, o);
            uint8_t bit = c.regs.b[kReg8Index[1]] & 7; // CL
            uint8_t mask = uint8_t(1u << bit);
            if (sub == 0x10) { // TEST1 — ZF = !(bit set); CF/OF cleared
                c.zeroVal = (v & mask) ? 1 : 0;
                c.carryVal = c.overVal = 0;
                return 4;
            }
            if (sub == 0x12) v = uint8_t(v & ~mask);   // CLR1
            if (sub == 0x14) v = uint8_t(v |  mask);   // SET1
            if (sub == 0x16) v = uint8_t(v ^  mask);   // NOT1
            writeOp8(c, o, v);
            return 5;
        }
        case 0x11: case 0x13: case 0x15: case 0x17: {
            uint8_t mrm = fetch8(c);
            Operand o = decodeModRM(c, mrm);
            uint16_t v = readOp16(c, o);
            uint8_t bit = c.regs.b[kReg8Index[1]] & 0xf; // CL
            uint16_t mask = uint16_t(1u << bit);
            if (sub == 0x11) {
                c.zeroVal = (v & mask) ? 1 : 0;
                c.carryVal = c.overVal = 0;
                return 4;
            }
            if (sub == 0x13) v = uint16_t(v & ~mask);
            if (sub == 0x15) v = uint16_t(v |  mask);
            if (sub == 0x17) v = uint16_t(v ^  mask);
            writeOp16(c, o, v);
            return 5;
        }
        // TEST1/CLR1/SET1/NOT1 with bit index as immediate byte
        case 0x18: case 0x1A: case 0x1C: case 0x1E: {
            uint8_t mrm = fetch8(c);
            Operand o = decodeModRM(c, mrm);
            uint8_t v = readOp8(c, o);
            uint8_t bit = fetch8(c) & 7;
            uint8_t mask = uint8_t(1u << bit);
            if (sub == 0x18) {
                c.zeroVal = (v & mask) ? 1 : 0;
                c.carryVal = c.overVal = 0;
                return 4;
            }
            if (sub == 0x1A) v = uint8_t(v & ~mask);
            if (sub == 0x1C) v = uint8_t(v |  mask);
            if (sub == 0x1E) v = uint8_t(v ^  mask);
            writeOp8(c, o, v);
            return 6;
        }
        case 0x19: case 0x1B: case 0x1D: case 0x1F: {
            uint8_t mrm = fetch8(c);
            Operand o = decodeModRM(c, mrm);
            uint16_t v = readOp16(c, o);
            uint8_t bit = fetch8(c) & 0xf;
            uint16_t mask = uint16_t(1u << bit);
            if (sub == 0x19) {
                c.zeroVal = (v & mask) ? 1 : 0;
                c.carryVal = c.overVal = 0;
                return 4;
            }
            if (sub == 0x1B) v = uint16_t(v & ~mask);
            if (sub == 0x1D) v = uint16_t(v |  mask);
            if (sub == 0x1F) v = uint16_t(v ^  mask);
            writeOp16(c, o, v);
            return 6;
        }
        // ADD4S / SUB4S / CMP4S — packed-BCD string arithmetic.
        // CL counts digit pairs; DS:SI = src, ES:DI = dst (ADD/SUB) or
        // DS:SI vs ES:DI (CMP). Flags: ZF and CF per MAME semantics.
        case 0x20: case 0x22: case 0x26: {
            uint16_t seg = resolveSeg(c, 3);
            uint16_t si = c.regs.w[6];
            uint16_t di = c.regs.w[7];
            uint8_t  count = c.regs.b[kReg8Index[1]] & 0xFF; // CL
            uint8_t  cf_in = 0;
            uint8_t  zf_any = 0;
            for (uint8_t i = 0; i < count; ++i) {
                uint8_t src = c.busRef().readMemByte(lin(seg, uint16_t(si + i)));
                uint8_t dst = c.busRef().readMemByte(lin(c.sregs[0], uint16_t(di + i)));
                uint16_t al = src & 0x0F;
                uint16_t bl = dst & 0x0F;
                uint16_t ah = (src >> 4) & 0x0F;
                uint16_t bh = (dst >> 4) & 0x0F;
                uint16_t lo, hi;
                if (sub == 0x20) { // ADD4S
                    lo = bl + al + cf_in; cf_in = (lo >= 10); if (cf_in) lo -= 10;
                    hi = bh + ah + cf_in; cf_in = (hi >= 10); if (cf_in) hi -= 10;
                    uint8_t r = uint8_t((hi << 4) | (lo & 0xF));
                    if (sub != 0x26) c.busRef().writeMemByte(lin(c.sregs[0], uint16_t(di + i)), r);
                    if (r) zf_any = 1;
                } else { // SUB4S / CMP4S
                    int16_t dlo = int16_t(bl) - int16_t(al) - int16_t(cf_in);
                    cf_in = (dlo < 0) ? 1 : 0;
                    if (cf_in) dlo += 10;
                    int16_t dhi = int16_t(bh) - int16_t(ah) - int16_t(cf_in);
                    cf_in = (dhi < 0) ? 1 : 0;
                    if (cf_in) dhi += 10;
                    uint8_t r = uint8_t((dhi << 4) | (dlo & 0xF));
                    if (sub == 0x22) c.busRef().writeMemByte(lin(c.sregs[0], uint16_t(di + i)), r);
                    if (r) zf_any = 1;
                }
            }
            c.carryVal = cf_in;
            c.zeroVal  = zf_any ? 0 : 1;
            return 7;
        }
        // ROL4 / ROR4 — BCD digit rotate between AL low nibble and
        // rm8. Ported from MAME.
        case 0x28: {
            uint8_t mrm = fetch8(c);
            Operand o = decodeModRM(c, mrm);
            uint16_t t = readOp8(c, o);
            t <<= 4;
            t |= (c.regs.b[0] & 0x0F);
            c.regs.b[0] = uint8_t((c.regs.b[0] & 0xF0) | ((t >> 8) & 0xF));
            writeOp8(c, o, uint8_t(t & 0xFF));
            return 13;
        }
        case 0x2A: {
            uint8_t mrm = fetch8(c);
            Operand o = decodeModRM(c, mrm);
            uint16_t t = readOp8(c, o);
            uint16_t t2 = uint16_t(c.regs.b[0] & 0x0F) << 4;
            c.regs.b[0] = uint8_t((c.regs.b[0] & 0xF0) | (t & 0xF));
            t = t2 | (t >> 4);
            writeOp8(c, o, uint8_t(t & 0xFF));
            return 17;
        }
        // INS reg1,reg2 — bit-field insert from AW into ES:IY with
        // position from r/m low nibble and length+1 from reg low nibble.
        case 0x31: {
            uint8_t mrm = fetch8(c);
            uint8_t rm = mrm & 7;
            uint8_t rg = (mrm >> 3) & 7;
            uint8_t tmp  = getReg8(c, rm) & 0x0F;
            uint8_t tmp2 = (getReg8(c, rg) & 0x0F) + 1;
            uint16_t iy = c.regs.w[7];     // DI/IY
            uint16_t aw = c.regs.w[0];
            uint16_t w = c.busRef().readMemWord(lin(c.sregs[0], iy));
            uint16_t mask1 = uint16_t((1u << tmp2) - 1);
            w = uint16_t((w & ~(mask1 << tmp)) | ((aw & mask1) << tmp));
            c.busRef().writeMemWord(lin(c.sregs[0], iy), w);
            if (tmp + tmp2 > 15) {
                iy = uint16_t(iy + 2);
                uint16_t w2 = c.busRef().readMemWord(lin(c.sregs[0], iy));
                uint16_t len2 = uint16_t(tmp2 - (16 - tmp));
                uint16_t mask2 = uint16_t((1u << len2) - 1);
                w2 = uint16_t((w2 & ~mask2) | ((aw >> (16 - tmp)) & mask2));
                c.busRef().writeMemWord(lin(c.sregs[0], iy), w2);
                c.regs.w[7] = iy;
            }
            setReg8(c, rm, uint8_t((tmp + tmp2) & 0x0F));
            return 35;
        }
        // EXT reg1,reg2 — bit-field extract from DS:IX into AW with
        // position from r/m low nibble and length+1 from reg low nibble.
        case 0x33: {
            uint8_t mrm = fetch8(c);
            uint8_t rm = mrm & 7;
            uint8_t rg = (mrm >> 3) & 7;
            uint8_t tmp  = getReg8(c, rm) & 0x0F;
            uint8_t tmp2 = (getReg8(c, rg) & 0x0F) + 1;
            uint16_t ix = c.regs.w[6];     // SI/IX
            uint16_t srcSeg = resolveSeg(c, 3);
            uint32_t aw = c.busRef().readMemWord(lin(srcSeg, ix));
            aw >>= tmp;
            if (tmp + tmp2 > 15) {
                ix = uint16_t(ix + 2);
                uint32_t hi = c.busRef().readMemWord(lin(srcSeg, ix));
                aw |= hi << (16 - tmp);
                c.regs.w[6] = ix;
            }
            uint16_t mask = uint16_t((1u << tmp2) - 1);
            c.regs.w[0] = uint16_t(aw & mask);
            setReg8(c, rm, uint8_t((tmp + tmp2) & 0x0F));
            return 34;
        }
        // INS reg,imm4
        case 0x39: {
            uint8_t mrm = fetch8(c);
            uint8_t rm = mrm & 7;
            uint8_t tmp  = getReg8(c, rm) & 0x0F;
            uint8_t tmp2 = (fetch8(c) & 0x0F) + 1;
            uint16_t iy = c.regs.w[7];
            uint16_t aw = c.regs.w[0];
            uint16_t w = c.busRef().readMemWord(lin(c.sregs[0], iy));
            uint16_t mask1 = uint16_t((1u << tmp2) - 1);
            w = uint16_t((w & ~(mask1 << tmp)) | ((aw & mask1) << tmp));
            c.busRef().writeMemWord(lin(c.sregs[0], iy), w);
            if (tmp + tmp2 > 15) {
                iy = uint16_t(iy + 2);
                uint16_t w2 = c.busRef().readMemWord(lin(c.sregs[0], iy));
                uint16_t len2 = uint16_t(tmp2 - (16 - tmp));
                uint16_t mask2 = uint16_t((1u << len2) - 1);
                w2 = uint16_t((w2 & ~mask2) | ((aw >> (16 - tmp)) & mask2));
                c.busRef().writeMemWord(lin(c.sregs[0], iy), w2);
                c.regs.w[7] = iy;
            }
            setReg8(c, rm, uint8_t((tmp + tmp2) & 0x0F));
            return 35;
        }
        // EXT reg,imm4
        case 0x3B: {
            uint8_t mrm = fetch8(c);
            uint8_t rm = mrm & 7;
            uint8_t tmp  = getReg8(c, rm) & 0x0F;
            uint8_t tmp2 = (fetch8(c) & 0x0F) + 1;
            uint16_t ix = c.regs.w[6];
            uint16_t srcSeg = resolveSeg(c, 3);
            uint32_t aw = c.busRef().readMemWord(lin(srcSeg, ix));
            aw >>= tmp;
            if (tmp + tmp2 > 15) {
                ix = uint16_t(ix + 2);
                uint32_t hi = c.busRef().readMemWord(lin(srcSeg, ix));
                aw |= hi << (16 - tmp);
                c.regs.w[6] = ix;
            }
            uint16_t mask = uint16_t((1u << tmp2) - 1);
            c.regs.w[0] = uint16_t(aw & mask);
            setReg8(c, rm, uint8_t((tmp + tmp2) & 0x0F));
            return 34;
        }
        // Unknown sub-opcode. The ROM may have been assembled for the
        // 8086 (e.g. the Psion MC400, MAME wires it as I8086) where the
        // single byte 0x0F is `POP CS` — a 1-byte instruction. POP CS
        // was removed from the 80186 onwards and replaced with the V30
        // / V20 extended-opcode prefix we handled above. Rather than
        // either popping the stack (risky if 0x0F is mis-executed data)
        // or consuming a ModR/M byte (mis-aligns IP relative to 8086
        // semantics), rewind the sub-byte we already fetched so IP
        // advances by exactly 1 — the length of POP CS on the 8086 —
        // and treat the instruction as a no-op. The MC400 boot ROM
        // hits this once during early init at a deterministic PC; the
        // kernel proceeds past the LCD-enable path either way.
        default: {
            static uint8_t seen[256] = {0};
            if (!seen[sub]) {
                seen[sub] = 1;
                std::fprintf(stderr,
                    "V30: unhandled 0x0F %02X at %04X:%04X — treating as 1-byte NOP (this msg once)\n",
                    sub, c.sregs[1], uint16_t(c.ip - 2));
            }
            c.ip = uint16_t(c.ip - 1);
            return 5;
        }
        }
    }

    // WAIT — mirrors MAME NEC: `if (!m_poll_state) m_ip--;`
    // When the POLL line is deasserted (SIB transfer in progress), the
    // instruction re-executes itself, stalling the CPU until the ASIC9
    // busy timer reasserts POLL (24 bus cycles per SIB frame transfer).
    case 0x9B:
        if (!c.pollLine) c.ip = uint16_t(c.ip - 1);
        return 5;

    // SALC (undocumented) — AL = CF ? 0xFF : 0x00
    case 0xD6:
        c.regs.b[0] = cf(c) ? 0xFF : 0x00;
        return 3;

    // INT1 / BRKS (V30-specific) — software INT through vector 1
    case 0xF1: {
        push16(c, buildPSW(c));
        push16(c, c.sregs[1]);
        push16(c, c.ip);
        c.iflag = 0;
        c.tf = 0;
        c.ip       = c.busRef().readMemWord(0x04);
        c.sregs[1] = c.busRef().readMemWord(0x06);
        return 50;
    }

    // ──────── String ops (single iteration) ────────
    // Each carries its real 8086 figure (see i8086StringCycles) so an
    // I8086 instance is not charged twice the V30's much lower count.
    case 0xA4: case 0xA5: case 0xA6: case 0xA7:
    case 0xAA: case 0xAB: case 0xAC: case 0xAD:
    case 0xAE: case 0xAF: {
        int v30Cycles = stringStep(c, op);
        i8086Exact(c, i8086StringCycles(op));
        return v30Cycles;
    }

    // ──────── REP prefixes (F2 REPNE / F3 REPE) ────────
    // Fetch the prefixed opcode inline. Only string ops (A4..AF) are
    // repeated; anything else is treated as NOP with IP rewound so the
    // next step() re-dispatches the following byte normally. Segment
    // override set by a preceding prefix survives into the string op.
    case 0xF2:
    case 0xF3: {
        uint8_t next = fetch8(c);
        bool repZ = (op == 0xF3);
        if (next >= 0xA4 && next <= 0xAF && next != 0xA8 && next != 0xA9) {
            return repString(c, next, repZ);
        }
        // Not a string op: rewind and let the outer dispatch handle it.
        c.ip = uint16_t(c.ip - 1);
        return 2;
    }

    // ──────── Shifts / rotates — Grp2 ────────
    case 0xD0: { // r/m8, 1
        uint8_t mrm = fetch8(c);
        uint8_t sub = (mrm >> 3) & 7;
        Operand o = decodeModRM(c, mrm);
        uint8_t v = readOp8(c, o);
        v = shiftRotateByte(c, sub, v, 1);
        writeOp8(c, o, v);
        return o.isReg ? 2 : 16;
    }
    case 0xD1: { // r/m16, 1
        uint8_t mrm = fetch8(c);
        uint8_t sub = (mrm >> 3) & 7;
        Operand o = decodeModRM(c, mrm);
        uint16_t v = readOp16(c, o);
        v = shiftRotateWord(c, sub, v, 1);
        writeOp16(c, o, v);
        return o.isReg ? 2 : 16;
    }
    case 0xD2: { // r/m8, CL
        uint8_t mrm = fetch8(c);
        uint8_t sub = (mrm >> 3) & 7;
        Operand o = decodeModRM(c, mrm);
        uint8_t v = readOp8(c, o);
        uint8_t cnt = c.regs.b[2];
        v = shiftRotateByte(c, sub, v, cnt);
        writeOp8(c, o, v);
        return (o.isReg ? 7 : 19) + cnt;
    }
    case 0xD3: { // r/m16, CL
        uint8_t mrm = fetch8(c);
        uint8_t sub = (mrm >> 3) & 7;
        Operand o = decodeModRM(c, mrm);
        uint16_t v = readOp16(c, o);
        uint8_t cnt = c.regs.b[2];
        v = shiftRotateWord(c, sub, v, cnt);
        writeOp16(c, o, v);
        return (o.isReg ? 7 : 19) + cnt;
    }
    case 0xC0: { // r/m8, imm8 (V30)
        uint8_t mrm = fetch8(c);
        uint8_t sub = (mrm >> 3) & 7;
        Operand o = decodeModRM(c, mrm);
        uint8_t v = readOp8(c, o);
        uint8_t cnt = fetch8(c) & 0x1F;
        v = shiftRotateByte(c, sub, v, cnt);
        writeOp8(c, o, v);
        return (o.isReg ? 7 : 19) + cnt;
    }
    case 0xC1: { // r/m16, imm8 (V30)
        uint8_t mrm = fetch8(c);
        uint8_t sub = (mrm >> 3) & 7;
        Operand o = decodeModRM(c, mrm);
        uint16_t v = readOp16(c, o);
        uint8_t cnt = fetch8(c) & 0x1F;
        v = shiftRotateWord(c, sub, v, cnt);
        writeOp16(c, o, v);
        return (o.isReg ? 7 : 19) + cnt;
    }

    // ──────── BCD / ASCII adjust ────────
    // Semantics follow MAME's ADJ4 / ADJB macros (necmacro.h) and match the
    // canonical 8086/V30 behaviour.
    case 0x27: { // DAA — decimal adjust AL after addition
        if (af(c) || ((c.regs.b[0] & 0x0F) > 9)) {
            uint16_t tmp = uint16_t(c.regs.b[0]) + 6;
            c.regs.b[0] = uint8_t(tmp);
            c.auxVal = 1;
            if (tmp & 0x100) c.carryVal = 1;
        }
        if (cf(c) || (c.regs.b[0] > 0x9F)) {
            c.regs.b[0] = uint8_t(c.regs.b[0] + 0x60);
            c.carryVal = 1;
        }
        // SF/ZF/PF set from the final AL; CF/AF preserved from above.
        uint8_t r = c.regs.b[0];
        c.signVal   = int8_t(r);
        c.zeroVal   = r;
        c.parityVal = r;
        return 3;
    }
    case 0x2F: { // DAS — decimal adjust AL after subtraction
        if (af(c) || ((c.regs.b[0] & 0x0F) > 9)) {
            uint16_t tmp = uint16_t(c.regs.b[0]) - 6;
            c.regs.b[0] = uint8_t(tmp);
            c.auxVal = 1;
            if (tmp & 0x100) c.carryVal = 1;
        }
        if (cf(c) || (c.regs.b[0] > 0x9F)) {
            c.regs.b[0] = uint8_t(c.regs.b[0] - 0x60);
            c.carryVal = 1;
        }
        uint8_t r = c.regs.b[0];
        c.signVal   = int8_t(r);
        c.zeroVal   = r;
        c.parityVal = r;
        return 3;
    }
    case 0x37: { // AAA — ASCII adjust AL after addition
        if (af(c) || ((c.regs.b[0] & 0x0F) > 9)) {
            c.regs.b[0] = uint8_t(c.regs.b[0] + 6);
            c.regs.b[1] = uint8_t(c.regs.b[1] + 1);
            c.auxVal   = 1;
            c.carryVal = 1;
        } else {
            c.auxVal   = 0;
            c.carryVal = 0;
        }
        c.regs.b[0] &= 0x0F;
        return 4;
    }
    case 0x3F: { // AAS — ASCII adjust AL after subtraction
        if (af(c) || ((c.regs.b[0] & 0x0F) > 9)) {
            c.regs.b[0] = uint8_t(c.regs.b[0] - 6);
            c.regs.b[1] = uint8_t(c.regs.b[1] - 1);
            c.auxVal   = 1;
            c.carryVal = 1;
        } else {
            c.auxVal   = 0;
            c.carryVal = 0;
        }
        c.regs.b[0] &= 0x0F;
        return 4;
    }
    case 0xD4: { // AAM imm8 — ASCII adjust AX after multiply
        uint8_t base = fetch8(c);
        if (base == 0) {
            // Divide-by-zero -> INT 0. Match Grp3 DIV behaviour.
            push16(c, buildPSW(c));
            push16(c, c.sregs[1]);
            push16(c, c.ip);
            c.iflag = 0; c.tf = 0;
            c.ip       = c.busRef().readMemWord(0);
            c.sregs[1] = c.busRef().readMemWord(2);
            return 15;
        }
        c.regs.b[1] = uint8_t(c.regs.b[0] / base);
        c.regs.b[0] = uint8_t(c.regs.b[0] % base);
        // SF/ZF/PF derived from the full AX result (matches MAME).
        uint16_t r = c.regs.w[0];
        c.signVal   = int16_t(r);
        c.zeroVal   = r;
        c.parityVal = uint8_t(r);
        return 15;
    }
    case 0xD5: { // AAD imm8 — ASCII adjust AX before divide
        uint8_t base = fetch8(c);
        c.regs.b[0] = uint8_t(c.regs.b[1] * base + c.regs.b[0]);
        c.regs.b[1] = 0;
        uint8_t r = c.regs.b[0];
        c.signVal   = int8_t(r);
        c.zeroVal   = r;
        c.parityVal = r;
        return 7;
    }

    // ──────── BOUND r16, m32 (0x62) ────────
    // Range check; generates INT 5 if out of range. Decode + skip for now —
    // we consume the ModR/M operand but don't raise the exception. V30-era
    // Psion ROMs don't rely on this.
    case 0x62: {
        uint8_t mrm = fetch8(c);
        Operand o = decodeModRM(c, mrm);
        if (!o.isReg) {
            (void)c.busRef().readMemWord(o.linear);
            (void)c.busRef().readMemWord(o.linear + 2);
        }
        return 13;
    }

    // ──────── ARPL (0x63) — adjust RPL. Not used pre-286; NOP ────────
    case 0x63: {
        uint8_t mrm = fetch8(c);
        (void)decodeModRM(c, mrm);
        return 2;
    }

    // ──────── IMUL r16, r/m16, imm (V30 / 80186+) ────────
    case 0x69: { // IMUL r16, r/m16, imm16
        uint8_t mrm = fetch8(c);
        Operand o = decodeModRM(c, mrm);
        uint16_t src = readOp16(c, o);
        int16_t imm = int16_t(fetch16(c));
        int32_t r32 = int32_t(int16_t(src)) * int32_t(imm);
        uint16_t r = uint16_t(r32);
        setReg16(c, (mrm >> 3) & 7, r);
        uint32_t set = ((r32 >> 15) != 0 && (r32 >> 15) != -1) ? 1u : 0u;
        c.carryVal = set;
        c.overVal  = set;
        return o.isReg ? 42 : 52;
    }
    case 0x6B: { // IMUL r16, r/m16, imm8 (sign-extended)
        uint8_t mrm = fetch8(c);
        Operand o = decodeModRM(c, mrm);
        uint16_t src = readOp16(c, o);
        int16_t imm = int16_t(int8_t(fetch8(c)));
        int32_t r32 = int32_t(int16_t(src)) * int32_t(imm);
        uint16_t r = uint16_t(r32);
        setReg16(c, (mrm >> 3) & 7, r);
        uint32_t set = ((r32 >> 15) != 0 && (r32 >> 15) != -1) ? 1u : 0u;
        c.carryVal = set;
        c.overVal  = set;
        return o.isReg ? 34 : 44;
    }

    // ──────── INS / OUTS (V30 / 80186+) ────────
    // No I/O device wired for these block primitives; stub as NOP but still
    // advance SI/DI as MAME does so any caller that checks post-increment
    // behaviour stays consistent.
    case 0x6C: { // INSB — ES:[DI] <- port DX
        c.busRef().writeMemByte(lin(c.sregs[0], c.regs.w[7]), 0);
        int16_t step = c.df ? -1 : +1;
        c.regs.w[7] = uint16_t(c.regs.w[7] + step);
        return 8;
    }
    case 0x6D: { // INSW
        c.busRef().writeMemWord(lin(c.sregs[0], c.regs.w[7]), 0);
        int16_t step = c.df ? -2 : +2;
        c.regs.w[7] = uint16_t(c.regs.w[7] + step);
        return 8;
    }
    case 0x6E: { // OUTSB — port DX <- DS:[SI]
        uint16_t srcSeg = resolveSeg(c, 3);
        (void)c.busRef().readMemByte(lin(srcSeg, c.regs.w[6]));
        int16_t step = c.df ? -1 : +1;
        c.regs.w[6] = uint16_t(c.regs.w[6] + step);
        return 8;
    }
    case 0x6F: { // OUTSW
        uint16_t srcSeg = resolveSeg(c, 3);
        (void)c.busRef().readMemWord(lin(srcSeg, c.regs.w[6]));
        int16_t step = c.df ? -2 : +2;
        c.regs.w[6] = uint16_t(c.regs.w[6] + step);
        return 8;
    }

    // ──────── ENTER imm16, imm8 / LEAVE (V30 / 80186+) ────────
    case 0xC8: {
        uint16_t frameSize = fetch16(c);
        uint8_t  level     = fetch8(c) & 0x1F;
        push16(c, c.regs.w[5]);                  // PUSH BP
        uint16_t frameTemp = c.regs.w[4];        // SP -> frame pointer
        if (level > 0) {
            static bool warned = false;
            if (!warned) {
                std::fprintf(stderr,
                    "V30: ENTER level=%u encountered; using level=0 fallback\n",
                    level);
                warned = true;
            }
            // Level-0 fallback: ignore nesting levels. Matches MAME's
            // behaviour when no nested frames are in play (the Psion ROMs
            // we care about pass level=0).
        }
        c.regs.w[5] = frameTemp;                 // BP = frame
        c.regs.w[4] = uint16_t(c.regs.w[4] - frameSize); // SP -= size
        return 23;
    }
    case 0xC9: { // LEAVE — SP = BP; POP BP
        c.regs.w[4] = c.regs.w[5];
        c.regs.w[5] = pop16(c);
        return 8;
    }

    default: return -1;
    }
}

} // namespace V30Detail
