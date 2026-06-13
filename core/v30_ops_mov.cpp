// license:BSD-3-Clause
// copyright-holders: Bryan McPhail, ASG (MAME NEC port)
//                    + adaptation by the Psion emulator project, 2026.
//
// V30 MOV / LEA / LDS / LES / XLAT dispatch.
//
// Return >=0 (cycle count) on opcode handled, -1 to defer to the next
// category.
//
// Opcodes in scope:
//   0x88 MOV r/m8, r8
//   0x89 MOV r/m16, r16
//   0x8A MOV r8, r/m8
//   0x8B MOV r16, r/m16
//   0x8C MOV r/m16, sreg
//   0x8D LEA r16, m
//   0x8E MOV sreg, r/m16
//   0xA0 MOV AL, moffs8
//   0xA1 MOV AX, moffs16
//   0xA2 MOV moffs8, AL
//   0xA3 MOV moffs16, AX
//   0xB0..0xB7 MOV reg8, imm8
//   0xB8..0xBF MOV reg16, imm16
//   0xC6 MOV r/m8, imm8  (Grp11 reg=0 only)
//   0xC7 MOV r/m16, imm16
//   0xC4 LES r16, m
//   0xC5 LDS r16, m
//   0xD7 XLAT
//
// See reference/mame-psion/cpu/nec/necinstr.hxx for MAME reference.

#include "v30_internals.h"

using namespace V30Detail;

namespace V30Detail {

int64_t dispatchMov(V30& c, uint8_t op) {
    switch (op) {

    // MOV r/m8, r8  — ModR/M: reg -> r/m
    case 0x88: {
        uint8_t mrm = fetch8(c);
        uint8_t reg = (mrm >> 3) & 7;
        Operand o   = decodeModRM(c, mrm);
        writeOp8(c, o, getReg8(c, reg));
        return o.isReg ? 2 : 10;
    }

    // MOV r/m16, r16
    case 0x89: {
        uint8_t mrm = fetch8(c);
        uint8_t reg = (mrm >> 3) & 7;
        Operand o   = decodeModRM(c, mrm);
        writeOp16(c, o, getReg16(c, reg));
        return o.isReg ? 2 : 10;
    }

    // MOV r8, r/m8
    case 0x8A: {
        uint8_t mrm = fetch8(c);
        uint8_t reg = (mrm >> 3) & 7;
        Operand o   = decodeModRM(c, mrm);
        setReg8(c, reg, readOp8(c, o));
        return o.isReg ? 2 : 10;
    }

    // MOV r16, r/m16
    case 0x8B: {
        uint8_t mrm = fetch8(c);
        uint8_t reg = (mrm >> 3) & 7;
        Operand o   = decodeModRM(c, mrm);
        setReg16(c, reg, readOp16(c, o));
        return o.isReg ? 2 : 10;
    }

    // MOV r/m16, sreg
    case 0x8C: {
        uint8_t mrm = fetch8(c);
        uint8_t sr  = (mrm >> 3) & 3;
        Operand o   = decodeModRM(c, mrm);
        writeOp16(c, o, getSReg(c, sr));
        return o.isReg ? 2 : 10;
    }

    // LEA r16, m
    case 0x8D: {
        uint8_t mrm = fetch8(c);
        uint8_t reg = (mrm >> 3) & 7;
        Operand o   = decodeModRM(c, mrm);
        if (o.isReg) return -1; // undefined; treat as defer
        // LEA loads the effective OFFSET, not the linear address. Recompute.
        // Easier: encode the offset by redoing the decode math without the
        // segment shift. For simplicity here, strip the segment back out.
        uint16_t off = uint16_t(o.linear - (uint32_t(c.sregs[o.defaultSeg]) << 4));
        setReg16(c, reg, off);
        return 2;
    }

    // MOV sreg, r/m16
    case 0x8E: {
        uint8_t mrm = fetch8(c);
        uint8_t sr  = (mrm >> 3) & 3;
        Operand o   = decodeModRM(c, mrm);
        setSReg(c, sr, readOp16(c, o));
        if (sr == 2) c.intInhibit = 1; // MOV SS: 1-insn interrupt shadow
        return o.isReg ? 2 : 10;
    }

    // MOV AL, moffs8
    case 0xA0: {
        uint16_t off = fetch16(c);
        c.regs.b[0] = c.busRef().readMemByte(lin(resolveSeg(c, 3), off));
        return 10;
    }
    // MOV AX, moffs16
    case 0xA1: {
        uint16_t off = fetch16(c);
        c.regs.w[0] = c.busRef().readMemWord(lin(resolveSeg(c, 3), off));
        return 10;
    }
    // MOV moffs8, AL
    case 0xA2: {
        uint16_t off = fetch16(c);
        c.busRef().writeMemByte(lin(resolveSeg(c, 3), off), c.regs.b[0]);
        return 10;
    }
    // MOV moffs16, AX
    case 0xA3: {
        uint16_t off = fetch16(c);
        c.busRef().writeMemWord(lin(resolveSeg(c, 3), off), c.regs.w[0]);
        return 10;
    }

    // MOV reg8, imm8  (0xB0..0xB7)
    case 0xB0: case 0xB1: case 0xB2: case 0xB3:
    case 0xB4: case 0xB5: case 0xB6: case 0xB7: {
        setReg8(c, op & 7, fetch8(c));
        return 4;
    }

    // MOV reg16, imm16  (0xB8..0xBF)
    case 0xB8: case 0xB9: case 0xBA: case 0xBB:
    case 0xBC: case 0xBD: case 0xBE: case 0xBF: {
        setReg16(c, op & 7, fetch16(c));
        return 4;
    }

    // MOV r/m8, imm8  (Grp11, ModR/M reg field must be 0)
    case 0xC6: {
        uint8_t mrm = fetch8(c);
        Operand o   = decodeModRM(c, mrm);
        uint8_t imm = fetch8(c);
        writeOp8(c, o, imm);
        return o.isReg ? 4 : 11;
    }

    // MOV r/m16, imm16
    case 0xC7: {
        uint8_t mrm = fetch8(c);
        Operand o   = decodeModRM(c, mrm);
        uint16_t imm = fetch16(c);
        writeOp16(c, o, imm);
        return o.isReg ? 4 : 11;
    }

    // LES r16, m — load ES:reg from m32
    case 0xC4: {
        uint8_t mrm = fetch8(c);
        uint8_t reg = (mrm >> 3) & 7;
        Operand o   = decodeModRM(c, mrm);
        if (o.isReg) return -1;
        uint16_t lo = c.busRef().readMemWord(o.linear);
        uint16_t hi = c.busRef().readMemWord((o.linear + 2) & 0xFFFFF);
        setReg16(c, reg, lo);
        c.sregs[0] = hi; // ES
        return 16;
    }

    // LDS r16, m — load DS:reg from m32
    case 0xC5: {
        uint8_t mrm = fetch8(c);
        uint8_t reg = (mrm >> 3) & 7;
        Operand o   = decodeModRM(c, mrm);
        if (o.isReg) return -1;
        uint16_t lo = c.busRef().readMemWord(o.linear);
        uint16_t hi = c.busRef().readMemWord((o.linear + 2) & 0xFFFFF);
        setReg16(c, reg, lo);
        c.sregs[3] = hi; // DS
        return 16;
    }

    // XLAT — AL = [BX + AL]
    case 0xD7: {
        uint16_t off = uint16_t(c.regs.w[3] + c.regs.b[0]);
        c.regs.b[0] = c.busRef().readMemByte(lin(resolveSeg(c, 3), off));
        return 11;
    }

    // XCHG r/m8, r8
    case 0x86: {
        uint8_t mrm = fetch8(c);
        uint8_t reg = (mrm >> 3) & 7;
        Operand o = decodeModRM(c, mrm);
        uint8_t a = readOp8(c, o);
        uint8_t b = getReg8(c, reg);
        writeOp8(c, o, b);
        setReg8(c, reg, a);
        return o.isReg ? 4 : 17;
    }
    // XCHG r/m16, r16
    case 0x87: {
        uint8_t mrm = fetch8(c);
        uint8_t reg = (mrm >> 3) & 7;
        Operand o = decodeModRM(c, mrm);
        uint16_t a = readOp16(c, o);
        uint16_t b = getReg16(c, reg);
        writeOp16(c, o, b);
        setReg16(c, reg, a);
        return o.isReg ? 4 : 17;
    }
    // XCHG AX, reg16 (0x91..0x97). 0x90 is NOP (AX,AX) handled in misc.
    case 0x91: case 0x92: case 0x93:
    case 0x94: case 0x95: case 0x96: case 0x97: {
        unsigned r = op & 7;
        uint16_t t = c.regs.w[0];
        c.regs.w[0] = c.regs.w[r];
        c.regs.w[r] = t;
        return 3;
    }

    // POP r/m16 (Grp1A — ModR/M reg field must be 0)
    case 0x8F: {
        uint8_t mrm = fetch8(c);
        Operand o = decodeModRM(c, mrm);
        writeOp16(c, o, pop16(c));
        return o.isReg ? 4 : 17;
    }

    default: return -1;
    }
}

} // namespace V30Detail
