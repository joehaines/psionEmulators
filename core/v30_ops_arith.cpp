// license:BSD-3-Clause
// copyright-holders: Bryan McPhail, ASG (MAME NEC port)
//                    + adaptation by the Psion emulator project, 2026.
//
// V30 arithmetic + logic dispatch.
//
// Return >=0 (cycle count) on opcode handled, -1 to defer to the next
// category.
//
// Opcodes in scope:
//   ADD:  0x00 r/m8,r8  0x01 r/m16,r16  0x02 r8,r/m8  0x03 r16,r/m16
//         0x04 AL,i8     0x05 AX,i16
//   OR:   0x08 0x09 0x0A 0x0B 0x0C 0x0D
//   ADC:  0x10 0x11 0x12 0x13 0x14 0x15
//   SBB:  0x18 0x19 0x1A 0x1B 0x1C 0x1D
//   AND:  0x20 0x21 0x22 0x23 0x24 0x25
//   SUB:  0x28 0x29 0x2A 0x2B 0x2C 0x2D
//   XOR:  0x30 0x31 0x32 0x33 0x34 0x35
//   CMP:  0x38 0x39 0x3A 0x3B 0x3C 0x3D
//   Grp1: 0x80 r/m8, imm8   0x81 r/m16, imm16
//         0x82 r/m8, imm8   0x83 r/m16, imm8-sign-extend
//         (ModR/M reg field: 0=ADD 1=OR 2=ADC 3=SBB 4=AND 5=SUB 6=XOR 7=CMP)
//   INC/DEC reg16:  0x40..0x4F
//   Grp3 (F6/F7): TEST, NOT, NEG, MUL, IMUL, DIV, IDIV
//   TEST: 0x84 r/m8,r8   0x85 r/m16,r16   0xA8 AL,imm8   0xA9 AX,imm16
//
// See reference/mame-psion/cpu/nec/necinstr.hxx for MAME reference.

#include "v30_internals.h"

#include <cstdint>

using namespace V30Detail;

namespace {

// ── 8-bit ALU primitives ──────────────────────────────────────────────

inline uint8_t alu8_add(V30& c, uint8_t a, uint8_t b) {
    uint16_t r = uint16_t(a) + uint16_t(b);
    setAddFlags8(c, a, b, r);
    return uint8_t(r);
}
inline uint8_t alu8_adc(V30& c, uint8_t a, uint8_t b) {
    uint16_t cin = cf(c) ? 1u : 0u;
    uint16_t r = uint16_t(a) + uint16_t(b) + cin;
    setAddFlags8(c, a, b, r);
    return uint8_t(r);
}
inline uint8_t alu8_sub(V30& c, uint8_t a, uint8_t b) {
    uint16_t r = uint16_t(a) - uint16_t(b);
    setSubFlags8(c, a, b, r);
    return uint8_t(r);
}
inline uint8_t alu8_sbb(V30& c, uint8_t a, uint8_t b) {
    uint16_t cin = cf(c) ? 1u : 0u;
    uint16_t r = uint16_t(a) - uint16_t(b) - cin;
    setSubFlags8(c, a, b, r);
    return uint8_t(r);
}
inline uint8_t alu8_and(V30& c, uint8_t a, uint8_t b) {
    uint8_t r = a & b; setLogicFlags8(c, r); return r;
}
inline uint8_t alu8_or(V30& c, uint8_t a, uint8_t b) {
    uint8_t r = a | b; setLogicFlags8(c, r); return r;
}
inline uint8_t alu8_xor(V30& c, uint8_t a, uint8_t b) {
    uint8_t r = a ^ b; setLogicFlags8(c, r); return r;
}

// ── 16-bit ALU primitives ─────────────────────────────────────────────

inline uint16_t alu16_add(V30& c, uint16_t a, uint16_t b) {
    uint32_t r = uint32_t(a) + uint32_t(b);
    setAddFlags16(c, a, b, r);
    return uint16_t(r);
}
inline uint16_t alu16_adc(V30& c, uint16_t a, uint16_t b) {
    uint32_t cin = cf(c) ? 1u : 0u;
    uint32_t r = uint32_t(a) + uint32_t(b) + cin;
    setAddFlags16(c, a, b, r);
    return uint16_t(r);
}
inline uint16_t alu16_sub(V30& c, uint16_t a, uint16_t b) {
    uint32_t r = uint32_t(a) - uint32_t(b);
    setSubFlags16(c, a, b, r);
    return uint16_t(r);
}
inline uint16_t alu16_sbb(V30& c, uint16_t a, uint16_t b) {
    uint32_t cin = cf(c) ? 1u : 0u;
    uint32_t r = uint32_t(a) - uint32_t(b) - cin;
    setSubFlags16(c, a, b, r);
    return uint16_t(r);
}
inline uint16_t alu16_and(V30& c, uint16_t a, uint16_t b) {
    uint16_t r = a & b; setLogicFlags16(c, r); return r;
}
inline uint16_t alu16_or(V30& c, uint16_t a, uint16_t b) {
    uint16_t r = a | b; setLogicFlags16(c, r); return r;
}
inline uint16_t alu16_xor(V30& c, uint16_t a, uint16_t b) {
    uint16_t r = a ^ b; setLogicFlags16(c, r); return r;
}

// Apply an 8-bit ALU op by group index 0..7 (0=ADD 1=OR 2=ADC 3=SBB
// 4=AND 5=SUB 6=XOR 7=CMP). Returns result; caller decides whether to
// store it (CMP discards, others store).
inline uint8_t aluGrp8(V30& c, unsigned sel, uint8_t a, uint8_t b) {
    switch (sel & 7) {
    case 0: return alu8_add(c, a, b);
    case 1: return alu8_or (c, a, b);
    case 2: return alu8_adc(c, a, b);
    case 3: return alu8_sbb(c, a, b);
    case 4: return alu8_and(c, a, b);
    case 5: return alu8_sub(c, a, b);
    case 6: return alu8_xor(c, a, b);
    case 7: return alu8_sub(c, a, b); // CMP — flags only
    }
    return 0;
}
inline uint16_t aluGrp16(V30& c, unsigned sel, uint16_t a, uint16_t b) {
    switch (sel & 7) {
    case 0: return alu16_add(c, a, b);
    case 1: return alu16_or (c, a, b);
    case 2: return alu16_adc(c, a, b);
    case 3: return alu16_sbb(c, a, b);
    case 4: return alu16_and(c, a, b);
    case 5: return alu16_sub(c, a, b);
    case 6: return alu16_xor(c, a, b);
    case 7: return alu16_sub(c, a, b); // CMP
    }
    return 0;
}

// Dispatch a two-operand 0x00-0x3F style opcode. `sel` is the group
// index (ADD=0..CMP=7); `variant` is the sub-opcode 0..5.
//   0: r/m8, r8     1: r/m16, r16
//   2: r8, r/m8     3: r16, r/m16
//   4: AL, imm8     5: AX, imm16
inline int64_t doAluStd(V30& c, unsigned sel, unsigned variant) {
    bool isCmp = (sel == 7);
    switch (variant) {
    case 0: { // r/m8, r8
        uint8_t mrm = fetch8(c);
        uint8_t reg = (mrm >> 3) & 7;
        Operand o = decodeModRM(c, mrm);
        uint8_t a = readOp8(c, o);
        uint8_t b = getReg8(c, reg);
        uint8_t r = aluGrp8(c, sel, a, b);
        if (!isCmp) writeOp8(c, o, r);
        return o.isReg ? 3 : 16;
    }
    case 1: { // r/m16, r16
        uint8_t mrm = fetch8(c);
        uint8_t reg = (mrm >> 3) & 7;
        Operand o = decodeModRM(c, mrm);
        uint16_t a = readOp16(c, o);
        uint16_t b = getReg16(c, reg);
        uint16_t r = aluGrp16(c, sel, a, b);
        if (!isCmp) writeOp16(c, o, r);
        return o.isReg ? 3 : 16;
    }
    case 2: { // r8, r/m8
        uint8_t mrm = fetch8(c);
        uint8_t reg = (mrm >> 3) & 7;
        Operand o = decodeModRM(c, mrm);
        uint8_t a = getReg8(c, reg);
        uint8_t b = readOp8(c, o);
        uint8_t r = aluGrp8(c, sel, a, b);
        if (!isCmp) setReg8(c, reg, r);
        return o.isReg ? 3 : 11;
    }
    case 3: { // r16, r/m16
        uint8_t mrm = fetch8(c);
        uint8_t reg = (mrm >> 3) & 7;
        Operand o = decodeModRM(c, mrm);
        uint16_t a = getReg16(c, reg);
        uint16_t b = readOp16(c, o);
        uint16_t r = aluGrp16(c, sel, a, b);
        if (!isCmp) setReg16(c, reg, r);
        return o.isReg ? 3 : 11;
    }
    case 4: { // AL, imm8
        uint8_t a = c.regs.b[0];
        uint8_t b = fetch8(c);
        uint8_t r = aluGrp8(c, sel, a, b);
        if (!isCmp) c.regs.b[0] = r;
        return 4;
    }
    case 5: { // AX, imm16
        uint16_t a = c.regs.w[0];
        uint16_t b = fetch16(c);
        uint16_t r = aluGrp16(c, sel, a, b);
        if (!isCmp) c.regs.w[0] = r;
        return 4;
    }
    }
    return -1;
}

} // namespace

namespace V30Detail {

int64_t dispatchArith(V30& c, uint8_t op) {
    // The 0x00..0x3D block is a tidy 6-way pattern within each 8-opcode
    // row; 0x06/0x07/0x0E/0x16/0x17/0x1E/0x1F/0x26/0x27/0x2E/0x2F/0x36/
    // 0x37/0x3E/0x3F are segment-override or BCD ops handled elsewhere.
    if (op <= 0x3D) {
        unsigned group   = op >> 3; // 0..7
        unsigned variant = op & 7;
        if (variant <= 5) return doAluStd(c, group, variant);
    }

    switch (op) {

    // ── INC reg16 0x40..0x47 / DEC reg16 0x48..0x4F (CF preserved) ────
    case 0x40: case 0x41: case 0x42: case 0x43:
    case 0x44: case 0x45: case 0x46: case 0x47: {
        unsigned r = op & 7;
        uint16_t a = getReg16(c, r);
        uint32_t res = uint32_t(a) + 1;
        uint32_t savedCf = c.carryVal;
        setAddFlags16(c, a, 1, res);
        c.carryVal = savedCf;
        setReg16(c, r, uint16_t(res));
        return 2;
    }
    case 0x48: case 0x49: case 0x4A: case 0x4B:
    case 0x4C: case 0x4D: case 0x4E: case 0x4F: {
        unsigned r = op & 7;
        uint16_t a = getReg16(c, r);
        uint32_t res = uint32_t(a) - 1;
        uint32_t savedCf = c.carryVal;
        setSubFlags16(c, a, 1, res);
        c.carryVal = savedCf;
        setReg16(c, r, uint16_t(res));
        return 2;
    }

    // ── Grp1: 0x80 r/m8, imm8 ─────────────────────────────────────────
    case 0x80: case 0x82: {
        uint8_t mrm = fetch8(c);
        unsigned sel = (mrm >> 3) & 7;
        Operand o = decodeModRM(c, mrm);
        uint8_t a = readOp8(c, o);
        uint8_t b = fetch8(c);
        uint8_t r = aluGrp8(c, sel, a, b);
        if (sel != 7) writeOp8(c, o, r);
        return o.isReg ? 4 : 16;
    }

    // ── Grp1: 0x81 r/m16, imm16 ──────────────────────────────────────
    case 0x81: {
        uint8_t mrm = fetch8(c);
        unsigned sel = (mrm >> 3) & 7;
        Operand o = decodeModRM(c, mrm);
        uint16_t a = readOp16(c, o);
        uint16_t b = fetch16(c);
        uint16_t r = aluGrp16(c, sel, a, b);
        if (sel != 7) writeOp16(c, o, r);
        return o.isReg ? 4 : 16;
    }

    // ── Grp1: 0x83 r/m16, imm8 (sign-extended) ───────────────────────
    case 0x83: {
        uint8_t mrm = fetch8(c);
        unsigned sel = (mrm >> 3) & 7;
        Operand o = decodeModRM(c, mrm);
        uint16_t a = readOp16(c, o);
        uint16_t b = uint16_t(int16_t(fetchS8(c)));
        uint16_t r = aluGrp16(c, sel, a, b);
        if (sel != 7) writeOp16(c, o, r);
        return o.isReg ? 4 : 16;
    }

    // ── TEST r/m8, r8 ─────────────────────────────────────────────────
    case 0x84: {
        uint8_t mrm = fetch8(c);
        uint8_t reg = (mrm >> 3) & 7;
        Operand o = decodeModRM(c, mrm);
        uint8_t a = readOp8(c, o);
        uint8_t b = getReg8(c, reg);
        setLogicFlags8(c, uint8_t(a & b));
        return o.isReg ? 3 : 11;
    }
    // TEST r/m16, r16
    case 0x85: {
        uint8_t mrm = fetch8(c);
        uint8_t reg = (mrm >> 3) & 7;
        Operand o = decodeModRM(c, mrm);
        uint16_t a = readOp16(c, o);
        uint16_t b = getReg16(c, reg);
        setLogicFlags16(c, uint16_t(a & b));
        return o.isReg ? 3 : 11;
    }
    // TEST AL, imm8
    case 0xA8: {
        uint8_t b = fetch8(c);
        setLogicFlags8(c, uint8_t(c.regs.b[0] & b));
        return 4;
    }
    // TEST AX, imm16
    case 0xA9: {
        uint16_t b = fetch16(c);
        setLogicFlags16(c, uint16_t(c.regs.w[0] & b));
        return 4;
    }

    // ── Grp3: 0xF6 r/m8 ──────────────────────────────────────────────
    case 0xF6: {
        uint8_t mrm = fetch8(c);
        unsigned sel = (mrm >> 3) & 7;
        Operand o = decodeModRM(c, mrm);
        uint8_t v = readOp8(c, o);
        switch (sel) {
        case 0:
        case 1: { // TEST r/m8, imm8 (case 1 is an 8086-documented alias for case 0)
            uint8_t imm = fetch8(c);
            setLogicFlags8(c, uint8_t(v & imm));
            return o.isReg ? 4 : 11;
        }
        case 2: { // NOT
            writeOp8(c, o, uint8_t(~v));
            return o.isReg ? 2 : 16;
        }
        case 3: { // NEG
            uint16_t r = uint16_t(0u) - uint16_t(v);
            setSubFlags8(c, 0, v, r);
            writeOp8(c, o, uint8_t(r));
            return o.isReg ? 2 : 16;
        }
        case 4: { // MUL (unsigned): AX = AL * r/m8
            uint16_t r = uint16_t(c.regs.b[0]) * uint16_t(v);
            c.regs.w[0] = r;
            uint32_t set = (c.regs.b[1] != 0) ? 1u : 0u;
            c.carryVal = set;
            c.overVal  = set;
            return o.isReg ? 30 : 36;
        }
        case 5: { // IMUL (signed)
            int16_t r = int16_t(int8_t(c.regs.b[0])) * int16_t(int8_t(v));
            c.regs.w[0] = uint16_t(r);
            uint32_t set = (c.regs.b[1] != 0 && c.regs.b[1] != 0xFF) ? 1u : 0u;
            c.carryVal = set;
            c.overVal  = set;
            return o.isReg ? 47 : 53;
        }
        case 6: { // DIV (unsigned): AL = AX / r/m8, AH = AX % r/m8
            if (v == 0) {
                push16(c, buildPSW(c));
                push16(c, c.sregs[1]);
                push16(c, c.ip);
                c.iflag = 0; c.tf = 0;
                c.ip       = c.busRef().readMemWord(0);
                c.sregs[1] = c.busRef().readMemWord(2);
                return 25;
            }
            uint16_t dvd = c.regs.w[0];
            uint32_t q = uint32_t(dvd) / uint32_t(v);
            uint32_t r = uint32_t(dvd) % uint32_t(v);
            if (q > 0xFF) {
                push16(c, buildPSW(c));
                push16(c, c.sregs[1]);
                push16(c, c.ip);
                c.iflag = 0; c.tf = 0;
                c.ip       = c.busRef().readMemWord(0);
                c.sregs[1] = c.busRef().readMemWord(2);
                return 25;
            }
            c.regs.b[0] = uint8_t(q);
            c.regs.b[1] = uint8_t(r);
            return o.isReg ? 25 : 35;
        }
        case 7: { // IDIV (signed)
            if (v == 0) {
                push16(c, buildPSW(c));
                push16(c, c.sregs[1]);
                push16(c, c.ip);
                c.iflag = 0; c.tf = 0;
                c.ip       = c.busRef().readMemWord(0);
                c.sregs[1] = c.busRef().readMemWord(2);
                return 25;
            }
            int16_t dvd = int16_t(c.regs.w[0]);
            int16_t dvs = int16_t(int8_t(v));
            int16_t q = int16_t(dvd / dvs);
            int16_t r = int16_t(dvd % dvs);
            if (q > 0x7F || q < -0x80) {
                push16(c, buildPSW(c));
                push16(c, c.sregs[1]);
                push16(c, c.ip);
                c.iflag = 0; c.tf = 0;
                c.ip       = c.busRef().readMemWord(0);
                c.sregs[1] = c.busRef().readMemWord(2);
                return 25;
            }
            c.regs.b[0] = uint8_t(q);
            c.regs.b[1] = uint8_t(r);
            return o.isReg ? 43 : 53;
        }
        }
        return -1;
    }

    // ── Grp3: 0xF7 r/m16 ─────────────────────────────────────────────
    case 0xF7: {
        uint8_t mrm = fetch8(c);
        unsigned sel = (mrm >> 3) & 7;
        Operand o = decodeModRM(c, mrm);
        uint16_t v = readOp16(c, o);
        switch (sel) {
        case 0:
        case 1: { // TEST r/m16, imm16 (case 1 is an 8086-documented alias for case 0)
            uint16_t imm = fetch16(c);
            setLogicFlags16(c, uint16_t(v & imm));
            return o.isReg ? 4 : 11;
        }
        case 2: { // NOT
            writeOp16(c, o, uint16_t(~v));
            return o.isReg ? 2 : 16;
        }
        case 3: { // NEG
            uint32_t r = uint32_t(0u) - uint32_t(v);
            setSubFlags16(c, 0, v, r);
            writeOp16(c, o, uint16_t(r));
            return o.isReg ? 2 : 16;
        }
        case 4: { // MUL: DX:AX = AX * r/m16
            uint32_t r = uint32_t(c.regs.w[0]) * uint32_t(v);
            c.regs.w[0] = uint16_t(r);
            c.regs.w[2] = uint16_t(r >> 16);
            uint32_t set = (c.regs.w[2] != 0) ? 1u : 0u;
            c.carryVal = set;
            c.overVal  = set;
            return o.isReg ? 30 : 36;
        }
        case 5: { // IMUL
            int32_t r = int32_t(int16_t(c.regs.w[0])) * int32_t(int16_t(v));
            c.regs.w[0] = uint16_t(r);
            c.regs.w[2] = uint16_t(uint32_t(r) >> 16);
            int16_t hi = int16_t(c.regs.w[2]);
            int16_t lo = int16_t(c.regs.w[0]);
            uint32_t set = ((lo >= 0 && hi != 0) || (lo < 0 && hi != -1)) ? 1u : 0u;
            c.carryVal = set;
            c.overVal  = set;
            return o.isReg ? 47 : 53;
        }
        case 6: { // DIV: AX = DX:AX / r/m16, DX = remainder
            if (v == 0) {
                push16(c, buildPSW(c));
                push16(c, c.sregs[1]);
                push16(c, c.ip);
                c.iflag = 0; c.tf = 0;
                c.ip       = c.busRef().readMemWord(0);
                c.sregs[1] = c.busRef().readMemWord(2);
                return 25;
            }
            uint32_t dvd = (uint32_t(c.regs.w[2]) << 16) | c.regs.w[0];
            uint32_t q = dvd / uint32_t(v);
            uint32_t r = dvd % uint32_t(v);
            if (q > 0xFFFF) {
                push16(c, buildPSW(c));
                push16(c, c.sregs[1]);
                push16(c, c.ip);
                c.iflag = 0; c.tf = 0;
                c.ip       = c.busRef().readMemWord(0);
                c.sregs[1] = c.busRef().readMemWord(2);
                return 25;
            }
            c.regs.w[0] = uint16_t(q);
            c.regs.w[2] = uint16_t(r);
            return o.isReg ? 25 : 35;
        }
        case 7: { // IDIV
            if (v == 0) {
                push16(c, buildPSW(c));
                push16(c, c.sregs[1]);
                push16(c, c.ip);
                c.iflag = 0; c.tf = 0;
                c.ip       = c.busRef().readMemWord(0);
                c.sregs[1] = c.busRef().readMemWord(2);
                return 25;
            }
            int32_t dvd = int32_t((uint32_t(c.regs.w[2]) << 16) | c.regs.w[0]);
            int32_t dvs = int32_t(int16_t(v));
            int32_t q = dvd / dvs;
            int32_t r = dvd % dvs;
            if (q > 0x7FFF || q < -0x8000) {
                push16(c, buildPSW(c));
                push16(c, c.sregs[1]);
                push16(c, c.ip);
                c.iflag = 0; c.tf = 0;
                c.ip       = c.busRef().readMemWord(0);
                c.sregs[1] = c.busRef().readMemWord(2);
                return 25;
            }
            c.regs.w[0] = uint16_t(q);
            c.regs.w[2] = uint16_t(r);
            return o.isReg ? 43 : 53;
        }
        }
        return -1;
    }

    default: return -1;
    }
}

} // namespace V30Detail
