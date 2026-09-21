// license:BSD-3-Clause
// copyright-holders: Bryan McPhail, ASG (MAME NEC port)
//                    + adaptation by the Psion emulator project, 2026.
//
// V30 internals shared between v30.cpp and the per-category opcode files
// (v30_ops_mov.cpp, v30_ops_arith.cpp, v30_ops_ctrl.cpp, v30_ops_misc.cpp).
//
// The core/v30.cpp file owns the main dispatch switch plus reset/run.
// Each per-category file exports one `dispatch<Category>(V30&, uint8_t op)`
// entry point that the main switch calls for its opcode range. That keeps
// each translation unit small and lets opcode work fan out across parallel
// workstreams without editing the same file.

#pragma once

#include "v30.h"

#include <cstdint>

namespace V30Detail {

// ──────────────────────────────────────────────────────────────────────
// Exact 8086 instruction timing
//
// An op calls this with the real 8086 figure for the path it just took.
// On a V30 instance it does nothing and the op's own return value stands;
// on an I8086 instance it pins the instruction's cost to that figure
// instead of letting step() double the V30 count. Use it where doubling
// is plainly wrong — the loop and string instructions, which is where the
// V30's advantage over the 8086 is nothing like 2x. See V30::absCycles.
inline void i8086Exact(V30& c, int i8086Cycles) {
    if (c.getVariant() == V30Variant::I8086) c.absCycles = i8086Cycles;
}

// ──────────────────────────────────────────────────────────────────────
// Register index mapping
//
// ModR/M encodes 8-bit registers as: 0=AL, 1=CL, 2=DL, 3=BL,
// 4=AH, 5=CH, 6=DH, 7=BH. On little-endian the Regs union's b[]
// array stores: b[0]=AL, b[1]=AH, b[2]=CL, b[3]=CH, b[4]=DL, b[5]=DH,
// b[6]=BL, b[7]=BH. This table maps ModR/M reg field -> b[] index.
inline constexpr uint8_t kReg8Index[8] = { 0, 2, 4, 6, 1, 3, 5, 7 };

inline uint8_t  getReg8 (V30& c, unsigned r) { return c.regs.b[kReg8Index[r & 7]]; }
inline void     setReg8 (V30& c, unsigned r, uint8_t v) { c.regs.b[kReg8Index[r & 7]] = v; }
inline uint16_t getReg16(V30& c, unsigned r) { return c.regs.w[r & 7]; }
inline void     setReg16(V30& c, unsigned r, uint16_t v) { c.regs.w[r & 7] = v; }

// Segment register indices for ModR/M sreg field (0-3):
// 0=ES(DS1), 1=CS(PS), 2=SS, 3=DS(DS0).
inline uint16_t getSReg(V30& c, unsigned s) { return c.sregs[s & 3]; }
inline void     setSReg(V30& c, unsigned s, uint16_t v) { c.sregs[s & 3] = v; }

// ──────────────────────────────────────────────────────────────────────
// Instruction fetch helpers (advance IP)

inline uint32_t lin(uint16_t seg, uint16_t off) {
    return ((uint32_t(seg) << 4) + off) & 0xFFFFF;
}

inline uint8_t fetch8(V30& c) {
    uint8_t v = c.busRef().readMemByte(lin(c.sregs[1], c.ip));
    c.ip = uint16_t(c.ip + 1);
    return v;
}

inline uint16_t fetch16(V30& c) {
    uint16_t v = c.busRef().readMemWord(lin(c.sregs[1], c.ip));
    c.ip = uint16_t(c.ip + 2);
    return v;
}

inline int8_t  fetchS8 (V30& c) { return int8_t(fetch8(c)); }
inline int16_t fetchS16(V30& c) { return int16_t(fetch16(c)); }

// ──────────────────────────────────────────────────────────────────────
// Segment override (set by 0x26/0x2E/0x36/0x3E prefixes, consumed by the
// next memory-addressing instruction). -1 = no override.

inline int  segOverride(V30& c)               { return c.segOverride; }
inline void setSegOverride(V30& c, int s)     { c.segOverride = s; }
inline void clearSegOverride(V30& c)          { c.segOverride = -1; }

// Resolve effective segment for a memory access. Default is DS; BP-indexed
// addressing modes default to SS; segment prefix wins unconditionally.
inline uint16_t resolveSeg(V30& c, int defaultSeg) {
    return c.sregs[c.segOverride >= 0 ? c.segOverride : defaultSeg];
}

// ──────────────────────────────────────────────────────────────────────
// ModR/M decode
//
// Resolves the ModR/M byte (already fetched by the caller) to a concrete
// operand. For register operands, set `isReg=true` and `regIdx` is the
// r/m field. For memory operands, `linear` is the effective linear
// address after segment + displacement. `displacementBytes` is how many
// extra bytes after the ModR/M we consumed (0/1/2).
struct Operand {
    bool     isReg;
    uint8_t  regIdx;       // valid if isReg
    uint32_t linear;       // valid if !isReg
    uint8_t  defaultSeg;   // valid if !isReg (2=SS, 3=DS)
};

inline Operand decodeModRM(V30& c, uint8_t mrm) {
    uint8_t mod = (mrm >> 6) & 3;
    uint8_t rm  = mrm & 7;

    if (mod == 3) {
        return { true, rm, 0, 3 };
    }

    // Memory addressing modes (mod 0/1/2)
    uint16_t base = 0;
    uint8_t  defaultSeg = 3; // DS by default
    switch (rm) {
    case 0: base = c.regs.w[3] + c.regs.w[6]; break;                        // [BX + SI]
    case 1: base = c.regs.w[3] + c.regs.w[7]; break;                        // [BX + DI]
    case 2: base = c.regs.w[5] + c.regs.w[6]; defaultSeg = 2; break;        // [BP + SI] (SS)
    case 3: base = c.regs.w[5] + c.regs.w[7]; defaultSeg = 2; break;        // [BP + DI] (SS)
    case 4: base = c.regs.w[6]; break;                                      // [SI]
    case 5: base = c.regs.w[7]; break;                                      // [DI]
    case 6:
        if (mod == 0) {
            // Special case: 16-bit displacement only, segment = DS.
            uint16_t disp = fetch16(c);
            return { false, 0, lin(resolveSeg(c, 3), disp), 3 };
        }
        base = c.regs.w[5]; defaultSeg = 2; break;                          // [BP] (SS)
    case 7: base = c.regs.w[3]; break;                                      // [BX]
    }

    uint16_t disp = 0;
    if (mod == 1)      disp = uint16_t(int16_t(fetchS8(c)));
    else if (mod == 2) disp = fetch16(c);

    uint16_t off = uint16_t(base + disp);
    return { false, 0, lin(resolveSeg(c, defaultSeg), off), defaultSeg };
}

// ──────────────────────────────────────────────────────────────────────
// Operand read/write (either register or memory path, size-dispatched)

inline uint8_t readOp8(V30& c, const Operand& o) {
    return o.isReg ? getReg8(c, o.regIdx) : c.busRef().readMemByte(o.linear);
}
inline uint16_t readOp16(V30& c, const Operand& o) {
    return o.isReg ? getReg16(c, o.regIdx) : c.busRef().readMemWord(o.linear);
}
inline void writeOp8(V30& c, const Operand& o, uint8_t v) {
    if (o.isReg) setReg8(c, o.regIdx, v);
    else c.busRef().writeMemByte(o.linear, v);
}
inline void writeOp16(V30& c, const Operand& o, uint16_t v) {
    if (o.isReg) setReg16(c, o.regIdx, v);
    else c.busRef().writeMemWord(o.linear, v);
}

// ──────────────────────────────────────────────────────────────────────
// Flag helpers — split-flag style matching MAME's necmacro.h. Each field
// is recomputed only when the result of an operation meaningfully
// determines that flag; the PSW is rebuilt on demand.
//
// CarryVal  stores the raw result high bits (nonzero means CF set).
// OverVal   nonzero means OF set.
// SignVal   sign-extended result (negative means SF set).
// ZeroVal   stored result (0 means ZF set).
// AuxVal    nonzero means AF set.
// ParityVal stored parity byte (parity of low byte).

inline uint8_t parityByte(uint8_t v) {
    v ^= v >> 4; v ^= v >> 2; v ^= v >> 1;
    return uint8_t(~v & 1); // 1 = even parity = PF set
}

inline void setLogicFlags8(V30& c, uint8_t r) {
    c.carryVal = 0;
    c.overVal  = 0;
    c.auxVal   = 0;
    c.signVal  = int8_t(r);
    c.zeroVal  = r;
    c.parityVal = r;
}
inline void setLogicFlags16(V30& c, uint16_t r) {
    c.carryVal = 0;
    c.overVal  = 0;
    c.auxVal   = 0;
    c.signVal  = int16_t(r);
    c.zeroVal  = r;
    c.parityVal = uint8_t(r);
}

inline void setAddFlags8(V30& c, uint8_t a, uint8_t b, uint16_t r) {
    c.carryVal = (r >> 8) & 1;
    c.overVal  = ((~(a ^ b) & (a ^ r)) >> 7) & 1;
    c.auxVal   = ((a ^ b ^ r) >> 4) & 1;
    uint8_t r8 = uint8_t(r);
    c.signVal  = int8_t(r8);
    c.zeroVal  = r8;
    c.parityVal = r8;
}
inline void setAddFlags16(V30& c, uint16_t a, uint16_t b, uint32_t r) {
    c.carryVal = (r >> 16) & 1;
    c.overVal  = ((~(a ^ b) & (a ^ r)) >> 15) & 1;
    c.auxVal   = ((a ^ b ^ r) >> 4) & 1;
    uint16_t r16 = uint16_t(r);
    c.signVal  = int16_t(r16);
    c.zeroVal  = r16;
    c.parityVal = uint8_t(r16);
}
inline void setSubFlags8(V30& c, uint8_t a, uint8_t b, uint16_t r) {
    c.carryVal = (r >> 8) & 1;
    c.overVal  = (((a ^ b) & (a ^ r)) >> 7) & 1;
    c.auxVal   = ((a ^ b ^ r) >> 4) & 1;
    uint8_t r8 = uint8_t(r);
    c.signVal  = int8_t(r8);
    c.zeroVal  = r8;
    c.parityVal = r8;
}
inline void setSubFlags16(V30& c, uint16_t a, uint16_t b, uint32_t r) {
    c.carryVal = (r >> 16) & 1;
    c.overVal  = (((a ^ b) & (a ^ r)) >> 15) & 1;
    c.auxVal   = ((a ^ b ^ r) >> 4) & 1;
    uint16_t r16 = uint16_t(r);
    c.signVal  = int16_t(r16);
    c.zeroVal  = r16;
    c.parityVal = uint8_t(r16);
}

inline bool cf(V30& c) { return (c.carryVal & 1) != 0; }
inline bool zf(V30& c) { return c.zeroVal == 0; }
inline bool sf(V30& c) { return c.signVal < 0; }
inline bool of(V30& c) { return (c.overVal & 1) != 0; }
inline bool pf(V30& c) { return parityByte(uint8_t(c.parityVal)) != 0; }
inline bool af(V30& c) { return (c.auxVal & 1) != 0; }

inline uint16_t buildPSW(V30& c) {
    uint16_t p = 0xF002; // V30 reserved bits
    if (cf(c)) p |= 0x0001;
    if (pf(c)) p |= 0x0004;
    if (af(c)) p |= 0x0010;
    if (zf(c)) p |= 0x0040;
    if (sf(c)) p |= 0x0080;
    if (c.tf)    p |= 0x0100;
    if (c.iflag) p |= 0x0200;
    if (c.df)    p |= 0x0400;
    if (of(c)) p |= 0x0800;
    return p;
}

inline void loadPSW(V30& c, uint16_t p) {
    c.carryVal  = (p >> 0) & 1;
    c.parityVal = (p & 0x0004) ? 0 : 1; // PF from stored bit
    c.auxVal    = (p >> 4) & 1;
    c.zeroVal   = (p & 0x0040) ? 0 : 1;
    c.signVal   = (p & 0x0080) ? -1 : 0;
    c.tf        = (p >> 8) & 1;
    c.iflag     = (p >> 9) & 1;
    c.df        = (p >> 10) & 1;
    c.overVal   = (p >> 11) & 1;
}

// ──────────────────────────────────────────────────────────────────────
// Stack helpers

inline void push16(V30& c, uint16_t v) {
    c.regs.w[4] = uint16_t(c.regs.w[4] - 2);
    c.busRef().writeMemWord(lin(c.sregs[2], c.regs.w[4]), v);
}
inline uint16_t pop16(V30& c) {
    uint16_t v = c.busRef().readMemWord(lin(c.sregs[2], c.regs.w[4]));
    c.regs.w[4] = uint16_t(c.regs.w[4] + 2);
    return v;
}

// ──────────────────────────────────────────────────────────────────────
// Condition codes for Jcc (opcode nibble 0x70..0x7F).
//
//  0 JO    1 JNO   2 JB/JC 3 JAE/JNC 4 JE/JZ 5 JNE/JNZ 6 JBE/JNA 7 JA/JNBE
//  8 JS    9 JNS   A JP/JPE B JNP/JPO C JL/JNGE D JGE/JNL E JLE/JNG F JG/JNLE

inline bool condCC(V30& c, uint8_t cc) {
    switch (cc & 0x0F) {
    case 0x0: return of(c);
    case 0x1: return !of(c);
    case 0x2: return cf(c);
    case 0x3: return !cf(c);
    case 0x4: return zf(c);
    case 0x5: return !zf(c);
    case 0x6: return cf(c) || zf(c);
    case 0x7: return !cf(c) && !zf(c);
    case 0x8: return sf(c);
    case 0x9: return !sf(c);
    case 0xA: return pf(c);
    case 0xB: return !pf(c);
    case 0xC: return sf(c) != of(c);
    case 0xD: return sf(c) == of(c);
    case 0xE: return zf(c) || (sf(c) != of(c));
    case 0xF: return !zf(c) && (sf(c) == of(c));
    }
    return false;
}

} // namespace V30Detail

// ──────────────────────────────────────────────────────────────────────
// Per-category dispatch entry points. Each returns the cycle count for
// the executed instruction, or -1 if the category does not handle this
// opcode (caller should fall through to the next category / report
// unimplemented).

namespace V30Detail {
int64_t dispatchMov  (V30& c, uint8_t op);
int64_t dispatchArith(V30& c, uint8_t op);
int64_t dispatchCtrl (V30& c, uint8_t op);
int64_t dispatchMisc (V30& c, uint8_t op);
}
