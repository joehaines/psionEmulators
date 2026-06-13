// license:BSD-3-Clause
// copyright-holders: Bryan McPhail, ASG (MAME NEC port)
//                    + adaptation by the Psion emulator project, 2026.
//
// V30 control flow dispatch — JMP / Jcc / CALL / RET / LOOP / INT / IRET.
//
// A minimum set is implemented here so the ROM reset vector advances.
// The main gap is: LOOPx, CALL/RET far, and indirect JMP/CALL via
// Grp5 (0xFF). Those belong here but are expected to be filled in by
// follow-up workstreams.

#include "v30_internals.h"

using namespace V30Detail;

namespace V30Detail {

int64_t dispatchCtrl(V30& c, uint8_t op) {
    switch (op) {

    // JMP short (disp8)
    case 0xEB: {
        int8_t d = fetchS8(c);
        c.ip = uint16_t(c.ip + d);
        return 15;
    }

    // JMP near (disp16)
    case 0xE9: {
        int16_t d = fetchS16(c);
        c.ip = uint16_t(c.ip + d);
        return 15;
    }

    // JMP far direct (offset16, segment16)
    case 0xEA: {
        uint16_t newIp = fetch16(c);
        uint16_t newCs = fetch16(c);
        c.ip = newIp;
        c.sregs[1] = newCs;
        return 15;
    }

    // Jcc short (0x70..0x7F)
    case 0x70: case 0x71: case 0x72: case 0x73:
    case 0x74: case 0x75: case 0x76: case 0x77:
    case 0x78: case 0x79: case 0x7A: case 0x7B:
    case 0x7C: case 0x7D: case 0x7E: case 0x7F: {
        int8_t d = fetchS8(c);
        if (condCC(c, op & 0x0F)) {
            c.ip = uint16_t(c.ip + d);
            return 13;
        }
        return 4;
    }

    // CALL near (disp16)
    case 0xE8: {
        int16_t d = fetchS16(c);
        push16(c, c.ip);
        c.ip = uint16_t(c.ip + d);
        return 19;
    }

    // RET near (no immediate)
    case 0xC3: {
        c.ip = pop16(c);
        return 16;
    }

    // RET near with imm16 stack adjust
    case 0xC2: {
        uint16_t adj = fetch16(c);
        c.ip = pop16(c);
        c.regs.w[4] = uint16_t(c.regs.w[4] + adj);
        return 20;
    }

    // RET far
    case 0xCB: {
        c.ip       = pop16(c);
        c.sregs[1] = pop16(c);
        return 22;
    }

    // RET far with imm16 stack adjust
    case 0xCA: {
        uint16_t adj = fetch16(c);
        c.ip       = pop16(c);
        c.sregs[1] = pop16(c);
        c.regs.w[4] = uint16_t(c.regs.w[4] + adj);
        return 25;
    }

    // CALL far direct
    case 0x9A: {
        uint16_t newIp = fetch16(c);
        uint16_t newCs = fetch16(c);
        push16(c, c.sregs[1]);
        push16(c, c.ip);
        c.ip = newIp;
        c.sregs[1] = newCs;
        return 28;
    }

    // INT 3
    case 0xCC: {
        push16(c, buildPSW(c));
        push16(c, c.sregs[1]);
        push16(c, c.ip);
        c.iflag = 0;
        c.tf = 0;
        c.ip       = c.busRef().readMemWord(0x0C);
        c.sregs[1] = c.busRef().readMemWord(0x0E);
        return 50;
    }

    // INT imm8
    case 0xCD: {
        uint8_t v = fetch8(c);
        push16(c, buildPSW(c));
        push16(c, c.sregs[1]);
        push16(c, c.ip);
        c.iflag = 0;
        c.tf = 0;
        uint32_t vec = uint32_t(v) * 4;
        c.ip       = c.busRef().readMemWord(vec);
        c.sregs[1] = c.busRef().readMemWord(vec + 2);
        return 50;
    }

    // INTO — INT 4 if OF
    case 0xCE: {
        if (!of(c)) return 4;
        push16(c, buildPSW(c));
        push16(c, c.sregs[1]);
        push16(c, c.ip);
        c.iflag = 0; c.tf = 0;
        c.ip       = c.busRef().readMemWord(0x10);
        c.sregs[1] = c.busRef().readMemWord(0x12);
        return 50;
    }

    // IRET
    case 0xCF: {
        c.ip       = pop16(c);
        c.sregs[1] = pop16(c);
        loadPSW(c, pop16(c));
        return 24;
    }

    // JCXZ short
    case 0xE3: {
        int8_t d = fetchS8(c);
        if (c.regs.w[1] == 0) {
            c.ip = uint16_t(c.ip + d);
            return 13;
        }
        return 4;
    }

    // LOOP (disp8) — dec CX, branch if CX != 0
    case 0xE2: {
        int8_t d = fetchS8(c);
        c.regs.w[1] = uint16_t(c.regs.w[1] - 1);
        if (c.regs.w[1] != 0) {
            c.ip = uint16_t(c.ip + d);
            return 13;
        }
        return 4;
    }

    // LOOPE / LOOPZ
    case 0xE1: {
        int8_t d = fetchS8(c);
        c.regs.w[1] = uint16_t(c.regs.w[1] - 1);
        if (c.regs.w[1] != 0 && zf(c)) {
            c.ip = uint16_t(c.ip + d);
            return 13;
        }
        return 4;
    }

    // LOOPNE / LOOPNZ
    case 0xE0: {
        int8_t d = fetchS8(c);
        c.regs.w[1] = uint16_t(c.regs.w[1] - 1);
        if (c.regs.w[1] != 0 && !zf(c)) {
            c.ip = uint16_t(c.ip + d);
            return 13;
        }
        return 4;
    }

    // ── Grp4 (0xFE) — INC/DEC r/m8 ─────────────────────────────────────
    case 0xFE: {
        uint8_t mrm = fetch8(c);
        unsigned sel = (mrm >> 3) & 7;
        Operand o = decodeModRM(c, mrm);
        uint8_t a = readOp8(c, o);
        uint32_t savedCf = c.carryVal;
        if (sel == 0) { // INC
            uint16_t r = uint16_t(a) + 1;
            setAddFlags8(c, a, 1, r);
            c.carryVal = savedCf;
            writeOp8(c, o, uint8_t(r));
        } else if (sel == 1) { // DEC
            uint16_t r = uint16_t(a) - 1;
            setSubFlags8(c, a, 1, r);
            c.carryVal = savedCf;
            writeOp8(c, o, uint8_t(r));
        } else return -1;
        return o.isReg ? 3 : 15;
    }

    // ── Grp5 (0xFF) — INC/DEC r/m16, CALL/JMP indirect, PUSH r/m16 ─────
    case 0xFF: {
        uint8_t mrm = fetch8(c);
        unsigned sel = (mrm >> 3) & 7;
        Operand o = decodeModRM(c, mrm);
        switch (sel) {
        case 0: { // INC r/m16
            uint16_t a = readOp16(c, o);
            uint32_t r = uint32_t(a) + 1;
            uint32_t savedCf = c.carryVal;
            setAddFlags16(c, a, 1, r);
            c.carryVal = savedCf;
            writeOp16(c, o, uint16_t(r));
            return o.isReg ? 3 : 15;
        }
        case 1: { // DEC r/m16
            uint16_t a = readOp16(c, o);
            uint32_t r = uint32_t(a) - 1;
            uint32_t savedCf = c.carryVal;
            setSubFlags16(c, a, 1, r);
            c.carryVal = savedCf;
            writeOp16(c, o, uint16_t(r));
            return o.isReg ? 3 : 15;
        }
        case 2: { // CALL near (indirect)
            uint16_t target = readOp16(c, o);
            push16(c, c.ip);
            c.ip = target;
            return o.isReg ? 11 : 21;
        }
        case 3: { // CALL far (indirect, m32)
            if (o.isReg) return -1;
            uint16_t offNew = c.busRef().readMemWord(o.linear);
            uint16_t segNew = c.busRef().readMemWord((o.linear + 2) & 0xFFFFF);
            push16(c, c.sregs[1]);
            push16(c, c.ip);
            c.ip = offNew;
            c.sregs[1] = segNew;
            return 29;
        }
        case 4: { // JMP near (indirect)
            uint16_t target = readOp16(c, o);
            c.ip = target;
            return o.isReg ? 11 : 18;
        }
        case 5: { // JMP far (indirect, m32)
            if (o.isReg) return -1;
            uint16_t offNew = c.busRef().readMemWord(o.linear);
            uint16_t segNew = c.busRef().readMemWord((o.linear + 2) & 0xFFFFF);
            c.ip = offNew;
            c.sregs[1] = segNew;
            return 24;
        }
        case 6: { // PUSH r/m16
            push16(c, readOp16(c, o));
            return o.isReg ? 11 : 16;
        }
        default: return -1;
        }
    }

    default: return -1;
    }
}

} // namespace V30Detail
