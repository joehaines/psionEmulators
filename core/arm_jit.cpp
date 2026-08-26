#include "arm_jit.h"

#include "wasm_emit.h"

namespace armjit {

using namespace wasmemit;

namespace {

// ── Field accessors, named to match the interpreter's decode ────────────────
inline uint32_t bits(uint32_t v, int hi, int lo) { return (v >> lo) & ((1u << (hi - lo + 1)) - 1); }
inline uint32_t bit(uint32_t v, int b)           { return (v >> b) & 1; }

inline uint32_t fCond(uint32_t i)   { return bits(i, 31, 28); }
inline uint32_t fI(uint32_t i)      { return bit(i, 25); }
inline uint32_t fOpcode(uint32_t i) { return bits(i, 24, 21); }
inline uint32_t fS(uint32_t i)      { return bit(i, 20); }
inline uint32_t fRn(uint32_t i)     { return bits(i, 19, 16); }
inline uint32_t fRd(uint32_t i)     { return bits(i, 15, 12); }
inline uint32_t fRm(uint32_t i)     { return bits(i, 3, 0); }
inline uint32_t fShiftBy(uint32_t i){ return bits(i, 11, 7); }
inline uint32_t fShiftTy(uint32_t i){ return bits(i, 6, 5); }
inline uint32_t fRot(uint32_t i)    { return bits(i, 11, 8); }
inline uint32_t fImm8(uint32_t i)   { return bits(i, 7, 0); }
inline uint32_t fBranchL(uint32_t i){ return bit(i, 24); }

inline uint32_t ror32(uint32_t v, uint32_t n) { n &= 31; return n ? ((v >> n) | (v << (32 - n))) : v; }

// execBranch's target: sign-extend the 24-bit offset, shift left 2, and add to
// the PC as it stands during execution (instruction address + 8).
inline uint32_t branchTarget(uint32_t insn, uint32_t pc) {
	const int32_t sext = (int32_t)(insn << 8) >> 6;
	return pc + 8u + (uint32_t)sext;
}

inline bool opIsLogical(uint32_t op) {
	switch (op) {
	case 0x0: case 0x1: case 0x8: case 0x9:
	case 0xC: case 0xD: case 0xE: case 0xF: return true;
	default: return false;
	}
}
inline bool opIsOutputless(uint32_t op) { return op >= 8 && op <= 0xB; }

enum : uint32_t {
	CPSR_V = 0x10000000u, CPSR_C = 0x20000000u,
	CPSR_Z = 0x40000000u, CPSR_N = 0x80000000u,
	CPSR_FlagMask = 0xF0000000u,
};

// The interpreter's packed condition truth table (ARM710::kCondTable): entry
// [cond] is the truth of that condition over all 16 NZCV states, indexed by
// CPSR>>28. Duplicated rather than shared so the generated code is provably
// testing the same table the interpreter tests.
constexpr uint16_t kCondTable[16] = {
	0xf0f0, 0x0f0f, 0xcccc, 0x3333,
	0xff00, 0x00ff, 0xaaaa, 0x5555,
	0x0c0c, 0xf3f3, 0xaa55, 0x55aa,
	0x0a05, 0xf5fa, 0xffff, 0x0000,
};

// ── Trace ──────────────────────────────────────────────────────────────────
enum class Step : uint8_t {
	Inline,      // compiled into wasm
	Bridge,      // handed to ARM710::tick() for one instruction
	BranchOut,   // branch whose target leaves the region
	BranchBack,  // branch back to the region entry — the loop edge
	BranchThrough, // unconditional branch FOLLOWED: the trace continues at its
	               // target, so the region spans both sides of it
	InlineLoad,  // load compiled against the micro-TLB, bridged on a miss
	Literal,     // LDR from a PC-relative address in this page: a constant
	InlineStore, // store compiled against the micro-TLB, bridged on a miss
};

struct TracePos {
	uint32_t pc;
	uint32_t insn;
	Step step;
};

// ── Codegen context ────────────────────────────────────────────────────────
struct Ctx {
	Func f;
	Layout L;
	const CodePage *page;
	uint32_t entryPc;
	// scratch
	uint32_t lTotal, lFlushed, lTicks, lTmp;
	uint32_t lOp1, lOp2, lRes, lRes1, lCarry, lShift;
	// Scratch for an inlined load: transfer address, write-back value, TLB
	// entry, host base. Separate from the data-processing scratch above because
	// a load's address is computed before the guard and read after it.
	uint32_t lMemA, lMemB, lMemC, lMemD, lMemE;
	uint32_t pMaxCycles = 0;      // parameter 0
	uint32_t pMaxTicks = 1;       // parameter 1
	uint32_t bridgeType = 0;      // call_indirect signature index

	uint32_t gpr(uint32_t r) const { return L.gprBase + 4 * r; }
	uint32_t wordAt(uint32_t pc) const { return page->words[(pc - page->baseVa) >> 2]; }
	bool     readable(uint32_t pc) const { return pc >= page->baseVa && pc + 4 <= page->endVa; }

	void loadGpr(uint32_t r)  { f.loadAbs(gpr(r)); }
	void storeGprFrom(uint32_t local, uint32_t r) {
		f.storeAbsPrep(); f.localGet(local); f.storeAbs(gpr(r));
	}
	void storeConst(uint32_t addr, uint32_t v) {
		f.storeAbsPrep(); f.i32Const((int32_t)v); f.storeAbs(addr);
	}

	void loadCpsr() { f.loadAbs(L.cpsrAddr); }
	void pushOldC() { loadCpsr(); f.i32Const(29); f.op(OP_I32_SHRU); f.i32Const(1); f.op(OP_I32_AND); }

	// $cyc and $total both accumulate; $cyc is what has not yet been written
	// through to the emulator's counters.
	// $total is every cycle the pass has charged; $flushed is how many of them
	// have been written through. The difference is what a flush owes, so there
	// is no second accumulator to keep in step — which matters because this is
	// emitted once per guest instruction and the bytes are what V8 charges for.
	void charge(uint32_t cycles, uint32_t ticks) {
		f.localGet(lTotal); f.i32Const((int32_t)cycles); f.op(OP_I32_ADD); f.localSet(lTotal);
		f.localGet(lTicks); f.i32Const((int32_t)ticks);  f.op(OP_I32_ADD); f.localSet(lTicks);
	}

	// Writes the cycles charged since the last flush through to insnCycleApprox
	// and, if it is set, through the device cycle sink.
	//
	// The interpreter does this per instruction, because "the next instruction
	// may read a peripheral whose answer depends on it". Batching is only safe
	// while nothing can observe the difference, which is exactly the inline
	// subset: no memory access, so no peripheral read, and the region is
	// bounded so it cannot cross the next scheduled device event. Anything that
	// CAN observe device time is bridged, and every bridge flushes first.
	void flush() {
		f.localGet(lTotal); f.localGet(lFlushed); f.op(OP_I32_SUB);
		f.call(kHelperFlush);
		f.localGet(lTotal); f.localSet(lFlushed);
	}

	// Leaves the region. `pc` is the instruction that would run next; the
	// prefetch pipeline is restored to what the burst loop would hold at that
	// point — full, with the next two words in it.
	void exitAt(uint32_t pc) {
		setState(pc + 8, wordAt(pc), wordAt(pc + 4), 2);
		emitReturn();
	}
	// r15 and the whole prefetch pipeline, through the shared helper.
	void setState(uint32_t r15, uint32_t word1, uint32_t word0, uint32_t count) {
		f.i32Const((int32_t)r15);
		f.i32Const((int32_t)word1);
		f.i32Const((int32_t)word0);
		f.i32Const((int32_t)count);
		f.call(kHelperExitState);
	}

	// Leaves the region with the pipeline already in whatever state the last
	// thing to run left it — used after a bridge (tick() maintains it) and
	// after a taken branch (execBranch flushes it).
	void exitRaw() { emitReturn(); }

	void emitReturn() {
		flush();
		f.storeAbsPrep(); f.localGet(lTicks); f.storeAbs(L.exitTicksAddr);
		f.localGet(lTotal);
		f.ret();
	}

	// Pushes 1 when the condition holds, using the interpreter's own table with
	// the entry folded to a constant.
	void pushCondition(uint32_t cond) {
		f.i32Const((int32_t)kCondTable[cond]);
		loadCpsr(); f.i32Const(28); f.op(OP_I32_SHRU);
		f.op(OP_I32_SHRU);
		f.i32Const(1); f.op(OP_I32_AND);
	}
};

// Emits the shifter for a register operand with a compile-time shift amount,
// leaving op2 in lOp2 and — only when `wantCarry` — the shifter carry-out (0/1)
// in lCarry. Mirrors execDataProcessing's switch case for case, including the
// encodings where a zero shift amount means something other than "no shift"
// (LSR #0 is LSR #32, ASR #0 is ASR #32, ROR #0 is RRX).
void emitRegShift(Ctx &c, uint32_t insn, bool wantCarry) {
	const uint32_t rm = fRm(insn), ty = fShiftTy(insn), by = fShiftBy(insn);
	auto &f = c.f;

	c.loadGpr(rm); f.localSet(c.lOp2);

	switch (ty) {
	case 0:  // LSL
		if (by == 0) {
			if (wantCarry) { c.pushOldC(); f.localSet(c.lCarry); }
		} else {
			if (wantCarry) {
				f.localGet(c.lOp2); f.i32Const(32 - (int32_t)by); f.op(OP_I32_SHRU);
				f.i32Const(1); f.op(OP_I32_AND); f.localSet(c.lCarry);
			}
			f.localGet(c.lOp2); f.i32Const((int32_t)by); f.op(OP_I32_SHL); f.localSet(c.lOp2);
		}
		break;
	case 1:  // LSR (by == 0 encodes LSR #32)
		if (by == 0) {
			if (wantCarry) { f.localGet(c.lOp2); f.i32Const(31); f.op(OP_I32_SHRU); f.localSet(c.lCarry); }
			f.i32Const(0); f.localSet(c.lOp2);
		} else {
			if (wantCarry) {
				f.localGet(c.lOp2); f.i32Const((int32_t)by - 1); f.op(OP_I32_SHRU);
				f.i32Const(1); f.op(OP_I32_AND); f.localSet(c.lCarry);
			}
			f.localGet(c.lOp2); f.i32Const((int32_t)by); f.op(OP_I32_SHRU); f.localSet(c.lOp2);
		}
		break;
	case 2:  // ASR (by == 0 encodes ASR #32)
		if (by == 0) {
			if (wantCarry) { f.localGet(c.lOp2); f.i32Const(31); f.op(OP_I32_SHRU); f.localSet(c.lCarry); }
			f.localGet(c.lOp2); f.i32Const(31); f.op(OP_I32_SHRS); f.localSet(c.lOp2);
		} else {
			if (wantCarry) {
				f.localGet(c.lOp2); f.i32Const((int32_t)by - 1); f.op(OP_I32_SHRU);
				f.i32Const(1); f.op(OP_I32_AND); f.localSet(c.lCarry);
			}
			f.localGet(c.lOp2); f.i32Const((int32_t)by); f.op(OP_I32_SHRS); f.localSet(c.lOp2);
		}
		break;
	default: // ROR (by == 0 encodes RRX)
		if (by == 0) {
			if (wantCarry) { f.localGet(c.lOp2); f.i32Const(1); f.op(OP_I32_AND); f.localSet(c.lCarry); }
			f.localGet(c.lOp2); f.i32Const(1); f.op(OP_I32_SHRU);
			c.pushOldC(); f.i32Const(31); f.op(OP_I32_SHL);
			f.op(OP_I32_OR); f.localSet(c.lOp2);
		} else {
			if (wantCarry) {
				f.localGet(c.lOp2); f.i32Const((int32_t)by - 1); f.op(OP_I32_SHRU);
				f.i32Const(1); f.op(OP_I32_AND); f.localSet(c.lCarry);
			}
			f.localGet(c.lOp2); f.i32Const((int32_t)by); f.op(OP_I32_ROTR); f.localSet(c.lOp2);
		}
		break;
	}
}

// N and Z from lRes, ORed onto the flag word already on the stack.
void emitNZ(Ctx &c) {
	auto &f = c.f;
	f.localGet(c.lRes); f.i32Const((int32_t)CPSR_N); f.op(OP_I32_AND); f.op(OP_I32_OR);
	f.localGet(c.lRes); f.op(OP_I32_EQZ); f.i32Const(30); f.op(OP_I32_SHL); f.op(OP_I32_OR);
}

// One data-processing instruction's body — the condition guard is the caller's.
void emitDataProc(Ctx &c, uint32_t insn) {
	auto &f = c.f;
	const uint32_t opcode = fOpcode(insn), rn = fRn(insn), rd = fRd(insn);
	const bool S = fS(insn) != 0;
	const bool logical = opIsLogical(opcode);
	const bool outputless = opIsOutputless(opcode);
	// The shifter carry-out is only ever read by the logical flag path, so for
	// an arithmetic opcode (carry comes from the adder) or S clear (nobody
	// reads the flags at all) it is not computed. 65% of executed
	// data-processing instructions have S clear.
	const bool wantCarry = S && logical;
	// RSB/RSC reverse the operands; SUB/RSB/SBC/RSC/CMP feed the adder the
	// complement of the second one. Defined once because both the adder and the
	// V computation need the same answer.
	const bool swap   = (opcode == 0x3 || opcode == 0x7);
	const bool invert = (opcode == 0x2 || opcode == 0x3 ||
	                     opcode == 0x6 || opcode == 0x7 || opcode == 0xA);
	const uint32_t la = swap ? c.lOp2 : c.lOp1;
	const uint32_t lb = swap ? c.lOp1 : c.lOp2;

	c.loadGpr(rn); f.localSet(c.lOp1);

	if (fI(insn)) {
		const uint32_t imm = ror32(fImm8(insn), fRot(insn) * 2);
		f.i32Const((int32_t)imm); f.localSet(c.lOp2);
		if (wantCarry) {
			// ARMv4: rotate 0 leaves C alone; otherwise C is bit 31 of the
			// rotated immediate — both compile-time constants here.
			if (fRot(insn) == 0) { c.pushOldC(); f.localSet(c.lCarry); }
			else { f.i32Const((int32_t)(imm >> 31)); f.localSet(c.lCarry); }
		}
	} else {
		emitRegShift(c, insn, wantCarry);
	}

	if (logical) {
		switch (opcode) {
		case 0x0: case 0x8: f.localGet(c.lOp1); f.localGet(c.lOp2); f.op(OP_I32_AND); break;  // AND / TST
		case 0x1: case 0x9: f.localGet(c.lOp1); f.localGet(c.lOp2); f.op(OP_I32_XOR); break;  // EOR / TEQ
		case 0xC:           f.localGet(c.lOp1); f.localGet(c.lOp2); f.op(OP_I32_OR);  break;  // ORR
		case 0xD:           f.localGet(c.lOp2); break;                                        // MOV
		case 0xE:           f.localGet(c.lOp1); f.localGet(c.lOp2);                           // BIC
		                    f.i32Const(-1); f.op(OP_I32_XOR); f.op(OP_I32_AND); break;
		default:            f.localGet(c.lOp2); f.i32Const(-1); f.op(OP_I32_XOR); break;      // MVN
		}
		f.localSet(c.lRes);
	} else {
		// The interpreter computes every arithmetic opcode as
		// ADD_OP(a, b, carryIn), with SUB_OP(a,b,c) == ADD_OP(a, ~b, c), so
		// normalise to (a, b, carryIn) and emit one adder:
		//   SUB  a=op1 b=~op2 c=1      RSB  a=op2 b=~op1 c=1
		//   ADD  a=op1 b=op2  c=0      ADC  a=op1 b=op2  c=oldC
		//   SBC  a=op1 b=~op2 c=oldC   RSC  a=op2 b=~op1 c=oldC
		//   CMP  a=op1 b=~op2 c=1      CMN  a=op1 b=op2  c=0
		const int carryIn = (opcode == 0x2 || opcode == 0x3 || opcode == 0xA) ? 1
		                  : (opcode == 0x4 || opcode == 0xB)                  ? 0
		                  : -1;                                                // -1 = old C

		f.localGet(lb);
		if (invert) { f.i32Const(-1); f.op(OP_I32_XOR); }
		f.localSet(c.lShift);              // b as fed to the adder

		f.localGet(la); f.localGet(c.lShift); f.op(OP_I32_ADD); f.localSet(c.lRes1);
		if (S) {
			f.localGet(c.lRes1); f.localGet(la); f.op(OP_I32_LTU); f.localSet(c.lCarry);
		}
		if (carryIn == 0) {
			f.localGet(c.lRes1); f.localSet(c.lRes);
		} else {
			f.localGet(c.lRes1);
			if (carryIn == 1) f.i32Const(1); else c.pushOldC();
			f.op(OP_I32_ADD); f.localSet(c.lRes);
			if (S) {
				// The second add contributes 0 or 1, so it carries exactly when
				// the sum comes out below what went in.
				f.localGet(c.lRes); f.localGet(c.lRes1); f.op(OP_I32_LTU);
				f.localGet(c.lCarry); f.op(OP_I32_OR); f.localSet(c.lCarry);
			}
		}
	}

	// Writeback then flags, in the interpreter's order: execDataProcessing
	// writes GPRs[Rd] and then takes the `else if (S)` branch for CPSR.
	if (!outputless) c.storeGprFrom(c.lRes, rd);

	if (S) {
		f.storeAbsPrep();
		c.loadCpsr(); f.i32Const((int32_t)~CPSR_FlagMask); f.op(OP_I32_AND);
		emitNZ(c);
		f.localGet(c.lCarry); f.i32Const(29); f.op(OP_I32_SHL); f.op(OP_I32_OR);
		if (logical) {
			c.loadCpsr(); f.i32Const((int32_t)CPSR_V); f.op(OP_I32_AND); f.op(OP_I32_OR);
		} else {
			// V = ~(a ^ b) & (a ^ res), with a and b the actual addends the
			// interpreter used — for a subtract that is a and ~b, which is what
			// lShift holds.
			f.localGet(la); f.localGet(c.lShift); f.op(OP_I32_XOR); f.i32Const(-1); f.op(OP_I32_XOR);
			f.localGet(la); f.localGet(c.lRes); f.op(OP_I32_XOR);
			f.op(OP_I32_AND);
			// That expression lands the overflow in bit 31 (it is built from
			// sign bits), but V is bit 28. Shift down BEFORE masking — masking a
			// bit-31 value with CPSR_V yields zero, which is a V flag that is
			// never set and an ADDS whose signed overflow silently disappears.
			f.i32Const(3); f.op(OP_I32_SHRU);
			f.i32Const((int32_t)CPSR_V); f.op(OP_I32_AND); f.op(OP_I32_OR);
		}
		f.storeAbs(c.L.cpsrAddr);
	}
}

// Hands one instruction to the interpreter through the imported table, then
// checks the three things that let the trace carry on.
//
// `slow` selects the full tick() bridge, which re-fetches the instruction and
// so needs the whole pipeline written out first. It is only for DK_SLOW, which
// ends the trace, so the expensive form is paid at most once per region. Every
// other bridge takes the fast form: the word is a compile-time constant, the
// pipeline is left alone (the region maintains it only at its exits), and the
// only state that has to be in place is r15.
void emitBridge(Ctx &c, uint32_t pc, uint32_t insn, bool slow) {
	auto &f = c.f;
	if (slow) {
		// tick() fetches at r15 and executes what the pipeline holds, so both
		// have to describe this instruction.
		c.setState(pc + 8, c.wordAt(pc), c.wordAt(pc + 4), 2);
	} else {
		// jitExecOne executes the word it is handed and reads r15 only for
		// PC-relative operands, so this is the only state it needs — the value
		// the burst loop would have left after its own fetch.
		c.storeConst(c.gpr(15), pc + 12);
	}
	// Anything the trace has been keeping in a local has to be in memory before
	// the interpreter can observe device time.
	c.flush();

	f.i32Const((int32_t)c.L.cpuPtr);
	if (slow) {
		// The instruction's own address travels with the call. The interpreter
		// does not need it — it reads the pipeline — but PSION_JIT_CHECK uses
		// it to assert that the pipeline the region handed over really does
		// describe this instruction.
		f.i32Const((int32_t)pc);
		f.i32Const((int32_t)c.L.slowBridgeFuncIndex);
	} else {
		f.i32Const((int32_t)insn);
		f.i32Const((int32_t)c.L.bridgeFuncIndex);
	}
	f.callIndirect(c.bridgeType);
	// Both bridges advance insnCycleApprox and the device cycle sink
	// themselves, exactly as the interpreter does for an instruction it runs.
	// So their cycles count towards what the region returns, but they are
	// already written through — which is what $flushed records, and why it
	// moves with $total here and nowhere else.
	f.localTee(c.lTmp);
	f.localGet(c.lTotal); f.op(OP_I32_ADD); f.localSet(c.lTotal);
	f.localGet(c.lTmp);
	f.localGet(c.lFlushed); f.op(OP_I32_ADD); f.localSet(c.lFlushed);
	f.localGet(c.lTicks); f.i32Const(1); f.op(OP_I32_ADD); f.localSet(c.lTicks);

	// The instruction redirected execution or flushed the pipeline: leave with
	// whatever it wrote, which is already right — a flushed pipeline refills
	// from r15 and its stale contents are never read.
	//
	// The pipeline test is not redundant with the PC test, and missing it was a
	// real divergence. The Series 7 ROM dispatches into an unrolled byte-copy
	// with `ADD r12, pc, #0x78 ; SUB pc, r12, r2, LSL #3`, and for one value of
	// r2 the computed target is exactly pc+12 — the address a fall-through
	// reaches. The PC test alone says nothing happened, but writing r15 set
	// prefetchCount to 0 and the interpreter owes two refill ticks.
	f.loadAbs(c.gpr(15)); f.i32Const((int32_t)(pc + 12)); f.op(OP_I32_NE);
	f.loadAbs(c.L.prefetchCountAddr); f.i32Const(2); f.op(OP_I32_NE); f.op(OP_I32_OR);
	f.ifVoid();
		c.exitRaw();
	f.end();
	// Nothing was redirected, but the region has to yield anyway: either an
	// interrupt is pending, or this instruction was a store that invalidated
	// the very page the region is executing from.
	//
	// The page test is the one the interpreter makes before every instruction,
	// and a region needs it for the same reason — its instruction words, branch
	// targets and pipeline restores are all constants baked in from that page.
	// Only a bridge can write memory, so here is the one place it can change.
	// Missing it did not show up on a boot, where code pages are ROM: it showed
	// up on an application, where the guest's code came off a card and lives in
	// RAM, and only once regions ran long enough to still be inside one when a
	// store landed.
	//
	// This exit writes the pipeline out, because the fast bridge does not
	// maintain it.
	f.load8Abs(c.L.pendingIrqAddr);
	f.load8Abs(c.L.pendingFiqAddr); f.op(OP_I32_OR);
	if (c.L.validPtrAddr) {
		f.loadAbs(c.L.validPtrAddr);
		f.i32Const((int32_t)c.L.validPhysBase); f.op(OP_I32_NE);
		f.op(OP_I32_OR);
	}
	f.ifVoid();
		c.exitAt(pc + 4);
	f.end();
}

// ── Inline loads ───────────────────────────────────────────────────────────
//
// A load compiled inline instead of bridged. This is the same lookup
// SA1100Bridge::readVirtual's fast path makes — MRU way, address tag, cached
// permission byte, host read pointer — emitted in place, with a bridge for the
// whole instruction whenever any of it misses.
//
// The bridge is what makes this safe to reason about. The inline path never has
// to be COMPLETE, only to agree with the interpreter wherever it fires: an
// unmapped page, an uncached permission, a denied one, a device address with no
// host pointer, an unaligned word — every one of them falls through to the
// interpreter, which then does exactly what it always did, faults included.

// The transfer address, and the write-back value, in `addr` and `mod`.
// Addressing mode 2: an immediate offset, or a register offset with a
// compile-time shift amount. Mirrors execSingleDataTransfer's offset block,
// including the encodings where a zero shift amount means something else.
void emitLdrAddress(Ctx &c, uint32_t insn, uint32_t lAddr, uint32_t lMod) {
	auto &f = c.f;
	const bool reg = fI(insn) != 0;          // bit 25 SET is the register form here
	const bool up  = bit(insn, 23) != 0;
	const bool pre = bit(insn, 24) != 0;

	if (!reg) {
		f.i32Const((int32_t)bits(insn, 11, 0));
	} else {
		const uint32_t rm = fRm(insn), ty = fShiftTy(insn), by = fShiftBy(insn);
		c.loadGpr(rm);
		switch (ty) {
		case 0:  if (by) { f.i32Const((int32_t)by); f.op(OP_I32_SHL); } break;
		case 1:  if (by) { f.i32Const((int32_t)by); f.op(OP_I32_SHRU); }
		         else    { f.op(OP_DROP); f.i32Const(0); }          // LSR #0 is LSR #32
		         break;
		case 2:  f.i32Const((int32_t)(by ? by : 31)); f.op(OP_I32_SHRS); break;  // ASR #0 is ASR #32
		default: // ROR, and ROR #0 is RRX
			if (by) { f.i32Const((int32_t)by); f.op(OP_I32_ROTR); }
			else {
				f.i32Const(1); f.op(OP_I32_SHRU);
				c.loadCpsr(); f.i32Const((int32_t)CPSR_C); f.op(OP_I32_AND);
				f.i32Const(2); f.op(OP_I32_SHL);            // C (bit 29) -> bit 31
				f.op(OP_I32_OR);
			}
			break;
		}
	}
	f.localSet(lMod);                        // the offset, for now
	c.loadGpr(fRn(insn));
	f.localTee(lAddr);                       // base, which is also the post-index address
	f.localGet(lMod);
	f.op(up ? OP_I32_ADD : OP_I32_SUB);
	f.localTee(lMod);                        // base +/- offset
	if (pre) f.localSet(lAddr); else f.op(OP_DROP);
}

// Pushes 1 when the interpreter's read fast path would have served a load of
// `lAddr`, leaving the host base it would have used in `lPtr`. Every term is
// one SA1100Bridge::readVirtual tests, in the same order; anything it does not
// answer yes to falls through to the bridge, so the interpreter still sees
// every unmapped page, uncached or denied permission, device address and
// unaligned word exactly as it always did.
void emitAccessGuard(Ctx &c, bool byteAccess, bool isWrite, uint32_t lAddr,
                     uint32_t lEnt, uint32_t lPtr, uint32_t lTmp, uint32_t lSh) {
	auto &f = c.f;
	const auto &M = c.L.mem;

	// e = dataMru_[(addr >> shift) & wayMask]
	f.localGet(lAddr);
	f.i32Const((int32_t)M.mruShift); f.op(OP_I32_SHRU);
	f.i32Const((int32_t)M.mruWayMask); f.op(OP_I32_AND);
	f.i32Const(2); f.op(OP_I32_SHL);
	f.i32Load((uint32_t)M.mruBase);
	f.localTee(lEnt);
	f.ifI32();                  // a null way ends it; everything below loads through lEnt
		// addrMask != 0 && (addr & addrMask) == the entry's tag
		f.localGet(lEnt); f.i32Load(M.offAddrMask); f.localTee(lTmp);
		f.localGet(lAddr); f.localGet(lTmp); f.op(OP_I32_AND);
		f.localGet(lEnt); f.i32Load(M.offAddr); f.op(OP_I32_EQ);
		f.op(OP_I32_AND);
		f.ifI32();
			// The host pointer, biased so that ptr + va is the host address.
			// Null means the page has no host mapping — a device register — and
			// the interpreter has to run the access.
			f.localGet(lEnt);
			f.i32Load(isWrite ? M.offHostWrite : M.offHostRead);
			f.localTee(lPtr);
			f.ifI32();
				// The permission byte: one per sub-page for a page entry (a
				// large page carries four AP fields over 16 KB each, a small
				// one over 1 KB), one for the whole megabyte for a section.
				f.localGet(lEnt); f.i32Load(M.offLv2); f.localTee(lTmp);
				f.ifI32();
					f.localGet(lEnt);
					f.localGet(lAddr);
					f.localGet(lTmp); f.i32Const(3); f.op(OP_I32_AND);
					f.i32Const(1); f.op(OP_I32_EQ);
					f.i32Const(14); f.i32Const(10); f.op(OP_SELECT);
					f.op(OP_I32_SHRU);
					f.i32Const(3); f.op(OP_I32_AND);
					f.op(OP_I32_ADD);
					f.i32Load8u(M.offPermCachePg);
				f.op(OP_ELSE);
					f.localGet(lEnt); f.i32Load8u(M.offPermCache);
				f.end();
				f.localSet(lTmp);
				// (privileged ? 0 : 2) + (write ? 1 : 0) selects this access's
				// pair of bits: bit shift+4 says the combination has been
				// computed, bit shift says it is allowed.
				c.loadCpsr(); f.i32Const(0x1F); f.op(OP_I32_AND);
				f.i32Const(0x10); f.op(OP_I32_GTU);        // isPrivileged()
				f.i32Const(1); f.op(OP_I32_XOR);
				f.i32Const(1); f.op(OP_I32_SHL);           // 0 privileged, 2 not
				if (isWrite) { f.i32Const(1); f.op(OP_I32_ADD); }
				f.localSet(lSh);
				f.localGet(lTmp); f.localGet(lSh); f.op(OP_I32_SHRU);
				f.localGet(lTmp);
				f.localGet(lSh); f.i32Const(4); f.op(OP_I32_ADD);
				f.op(OP_I32_SHRU);
				f.op(OP_I32_AND);
				f.i32Const(1); f.op(OP_I32_AND);
				if (!byteAccess && !isWrite) {
					// A word LOAD must be aligned. The interpreter reads at
					// addr & ~3 and rotates the result; rather than emit that
					// on every load, the unaligned case goes to the
					// interpreter, which is the only place it has ever been.
					// A store does no such thing — writeVirtual writes at the
					// address it is given — so there is nothing to check.
					f.localGet(lAddr); f.i32Const(3); f.op(OP_I32_AND);
					f.op(OP_I32_EQZ);
					f.op(OP_I32_AND);
				}
			f.op(OP_ELSE); f.i32Const(0);
			f.end();
		f.op(OP_ELSE); f.i32Const(0);
		f.end();
	f.op(OP_ELSE); f.i32Const(0);
	f.end();
}

// The code-coherency drop a store owes: ARM710::physAddrFromTlbEntry followed by
// SA1100Bridge::invalidateDecodePage. A guest write into a cached code page
// makes its decoded ops stale, and because this path writes through a host
// pointer it never reaches writePhysical, where the drop would otherwise
// happen. A store that lands on no cached page pays one indexed compare, which
// is what it costs in the interpreter too.
void emitCodeCoherency(Ctx &c, uint32_t lEnt, uint32_t lAddr,
                       uint32_t lTmp, uint32_t lKind, uint32_t lWork) {
	auto &f = c.f;
	const auto &M = c.L.mem;
	f.loadAbs((uint32_t)M.decodePagesPtr);       // *decodePages_
	f.localTee(lWork);
	f.ifVoid();
		f.localGet(lEnt); f.i32Load(M.offLv2); f.localTee(lTmp);
		f.i32Const(3); f.op(OP_I32_AND); f.localTee(lKind);
		f.i32Const(2); f.op(OP_I32_EQ);
		f.ifI32();                                    // small page
			f.localGet(lTmp); f.i32Const((int32_t)0xFFFFF000); f.op(OP_I32_AND);
			f.localGet(lAddr); f.i32Const(0xFFF); f.op(OP_I32_AND);
			f.op(OP_I32_OR);
		f.op(OP_ELSE);
			f.localGet(lKind); f.i32Const(1); f.op(OP_I32_EQ);
			f.ifI32();                                // large page
				f.localGet(lTmp); f.i32Const((int32_t)0xFFFF0000); f.op(OP_I32_AND);
				f.localGet(lAddr); f.i32Const(0xFFFF); f.op(OP_I32_AND);
				f.op(OP_I32_OR);
			f.op(OP_ELSE);                            // section
				f.localGet(lEnt); f.i32Load(M.offLv1);
				f.i32Const((int32_t)0xFFF00000); f.op(OP_I32_AND);
				f.localGet(lAddr); f.i32Const(0xFFFFF); f.op(OP_I32_AND);
				f.op(OP_I32_OR);
			f.end();
		f.end();
		// base = phys & ~0xFFF; slot = decodePages_[(base >> 12) & mask]
		f.i32Const((int32_t)0xFFFFF000); f.op(OP_I32_AND);
		f.localTee(lTmp);
		f.i32Const(12); f.op(OP_I32_SHRU);
		f.i32Const((int32_t)M.decodePageSlotMask); f.op(OP_I32_AND);
		f.i32Const((int32_t)M.decodePageStride); f.op(OP_I32_MUL);
		f.localGet(lWork); f.op(OP_I32_ADD);
		f.localTee(lWork);
		f.i32Load(M.offDecodePhysBase);
		f.localGet(lTmp); f.op(OP_I32_EQ);
		f.ifVoid();
			f.localGet(lWork);
			f.i32Const(-1);                            // the empty-slot sentinel
			f.i32Store(M.offDecodePhysBase);
		f.end();
	f.end();
}

// LDR Rd, [pc, #imm] — a constant-pool read, and the cheapest instruction in
// the set to compile: the address is known when the region is built, and so is
// the word at it, because it lies in the very page the region was compiled
// from. There is no memory access left to make.
//
// What keeps that sound is the same pair of checks the region's instruction
// words rest on: the page-validity token, re-read at the loop head and after
// every bridge, and the dispatcher's hash of the words the region was compiled
// from — which is why the literal is added to the region's runs rather than
// read on the quiet.
void emitLiteralLoad(Ctx &c, uint32_t insn, uint32_t addr) {
	auto &f = c.f;
	const bool byteAccess = bit(insn, 22) != 0;
	const uint32_t word = c.wordAt(addr & ~3u);
	const uint32_t v = byteAccess ? ((word >> (8 * (addr & 3))) & 0xFF) : word;

	auto body = [&]() {
		c.storeConst(c.gpr(fRd(insn)), v);
		// tickPageLoop's 1 for the tick and 1 for executing, plus the 1
		// execSingleDataTransfer charges for an immediate-offset load.
		c.charge(3, 1);
	};
	if (fCond(insn) == 0xE) {
		body();
	} else {
		c.pushCondition(fCond(insn));
		f.ifVoid();
			body();
		f.op(OP_ELSE);
			c.charge(kCyclesPerInsn, 1);
		f.end();
	}
}

// The four memory helpers every batch shares — see kHelper* in arm_jit.h.
//
// These used to be emitted into every region that made a memory access, which
// is sixty instructions for a load and a hundred and fifty for a store, in
// thousands of regions. V8 charges for every byte it compiles: module
// compilation and instantiation measured 164 ms of a 6.4 s run, against 446 ms
// the generated code was saving. One copy per module, reached by a direct
// `call`, is the same work with a fraction of the bytes.
//
//   hostReadW/B(addr)     -> the host address to read from, or 0 to bridge
//   storeW/B(addr, value) -> 1 once stored, or 0 to bridge
//
// The store helpers carry the write through themselves because the coherency
// drop needs the TLB entry, which is not worth returning.
std::vector<Func> buildMemHelpers(const Layout &L) {
	std::vector<Func> out;
	const bool have = L.mem.mruBase != 0;
	for (int which = 0; which < kHelperCount; which++) {
		Ctx c;
		c.L = L;
		c.page = nullptr;
		c.entryPc = 0;

		if (which == kHelperFlush) {
			// (cycles) -> void.  insnCycleApprox += cycles, and the device
			// cycle sink too when there is one.
			c.f.nParams = 1;
			const uint32_t lTmp = c.f.addLocal();
			c.f.i32Const(0);
			c.f.load64Abs(L.insnCycleAddr);
			c.f.localGet(0); c.f.op(OP_I64_EXT_U);
			c.f.op(OP_I64_ADD);
			c.f.i64Store(L.insnCycleAddr);
			c.f.loadAbs(L.cycleSinkPtrAddr); c.f.localTee(lTmp);
			c.f.ifVoid();
				c.f.localGet(lTmp);
				c.f.localGet(lTmp); c.f.i64Load(0);
				c.f.localGet(0); c.f.op(OP_I64_EXT_U);
				c.f.op(OP_I64_ADD);
				c.f.i64Store(0);
			c.f.end();
			out.push_back(std::move(c.f));
			continue;
		}

		if (which == kHelperExitState) {
			// (r15, word1, word0, count) -> void.  The prefetch fault slots go
			// out with the words: leaving a stale fault beside a fresh word
			// makes the interpreter raise a prefetch abort for an instruction
			// that fetched cleanly. MMUFault is 64 bits, written as two zero
			// words rather than teaching the emitter an i64 constant.
			c.f.nParams = 4;
			auto put = [&](uint32_t addr, uint32_t param) {
				c.f.storeAbsPrep(); c.f.localGet(param); c.f.storeAbs(addr);
			};
			put(L.gprBase + 4 * 15, 0);
			put(L.prefetch1Addr, 1);
			put(L.prefetch0Addr, 2);
			put(L.prefetchCountAddr, 3);
			c.storeConst(L.prefetchFault1Addr, 0);
			c.storeConst(L.prefetchFault1Addr + 4, 0);
			c.storeConst(L.prefetchFault0Addr, 0);
			c.storeConst(L.prefetchFault0Addr + 4, 0);
			out.push_back(std::move(c.f));
			continue;
		}

		const bool isWrite = (which == kHelperStoreW || which == kHelperStoreB);
		const bool byteAccess = (which == kHelperHostReadB || which == kHelperStoreB);
		c.f.nParams = isWrite ? 2 : 1;
		const uint32_t pAddr = 0, pValue = 1;
		if (!have) {
			// No micro-TLB on this CPU: every access bridges, and the helper is
			// never called. It still has to exist so the indices are fixed.
			c.f.i32Const(0);
			out.push_back(std::move(c.f));
			continue;
		}
		const uint32_t lEnt = c.f.addLocal(), lPtr = c.f.addLocal();
		const uint32_t lTmp = c.f.addLocal(), lSh = c.f.addLocal();
		const uint32_t lWork = c.f.addLocal();
		emitAccessGuard(c, byteAccess, isWrite, pAddr, lEnt, lPtr, lTmp, lSh);
		c.f.ifI32();
			if (isWrite) {
				c.f.localGet(lPtr); c.f.localGet(pAddr); c.f.op(OP_I32_ADD);
				c.f.localGet(pValue);
				if (byteAccess) c.f.i32Store8(0); else c.f.i32Store(0);
				emitCodeCoherency(c, lEnt, pAddr, lTmp, lSh, lWork);
				c.f.i32Const(1);
			} else {
				c.f.localGet(lPtr); c.f.localGet(pAddr); c.f.op(OP_I32_ADD);
			}
		c.f.op(OP_ELSE);
			c.f.i32Const(0);
		c.f.end();
		out.push_back(std::move(c.f));
	}
	return out;
}

// One load: the shared helper decides, and the instruction is bridged when it
// says no. `pc` is the instruction's own address, which the bridge needs.
void emitInlineLoad(Ctx &c, uint32_t pc, uint32_t insn) {
	auto &f = c.f;
	const bool byteAccess = bit(insn, 22) != 0;
	const bool pre = bit(insn, 24) != 0;
	const bool wb  = bit(insn, 21) != 0;
	const uint32_t rd = fRd(insn), rn = fRn(insn);
	// tickPageLoop charges 1 for the tick and 1 for executing rather than
	// refilling; execSingleDataTransfer adds 1, and 1 more for a load whose
	// offset came out of a register.
	const uint32_t cycles = 2 + 1 + (fI(insn) ? 1u : 0u);

	auto body = [&]() {
		emitLdrAddress(c, insn, c.lMemA, c.lMemB);
		f.localGet(c.lMemA);
		f.call(byteAccess ? kHelperHostReadB : kHelperHostReadW);
		f.localTee(c.lMemD);
		f.ifVoid();
			f.storeAbsPrep();
			f.localGet(c.lMemD);
			if (byteAccess) f.i32Load8u(0); else f.i32Load(0);
			f.storeAbs(c.gpr(rd));
			// Write-back is the interpreter's `(preIndex && writeback) ||
			// !preIndex`, and a faulting transfer never gets here.
			if ((pre && wb) || !pre) c.storeGprFrom(c.lMemB, rn);
			c.charge(cycles, 1);
		f.op(OP_ELSE);
			emitBridge(c, pc, insn, false);
		f.end();
	};

	if (fCond(insn) == 0xE) {
		body();
	} else {
		c.pushCondition(fCond(insn));
		f.ifVoid();
			body();
		f.op(OP_ELSE);
			// Not taken. The interpreter still pays the tick and the executed
			// charge; execSingleDataTransfer is never reached.
			c.charge(kCyclesPerInsn, 1);
		f.end();
	}
}

// One store. The helper carries the write and the coherency drop; all that is
// left here is the write-back and the charge.
void emitInlineStore(Ctx &c, uint32_t pc, uint32_t insn) {
	auto &f = c.f;
	const bool byteAccess = bit(insn, 22) != 0;
	const bool pre = bit(insn, 24) != 0;
	const bool wb  = bit(insn, 21) != 0;
	const uint32_t rd = fRd(insn), rn = fRn(insn);

	auto body = [&]() {
		emitLdrAddress(c, insn, c.lMemA, c.lMemB);
		f.localGet(c.lMemA);
		// Rd is read here, before the write-back below, which is what makes
		// STR Rd,[Rd],#n store the ORIGINAL base.
		c.loadGpr(rd);
		f.call(byteAccess ? kHelperStoreB : kHelperStoreW);
		f.ifVoid();
			if ((pre && wb) || !pre) c.storeGprFrom(c.lMemB, rn);
			// The tick, the executed charge, and execSingleDataTransfer's 1.
			c.charge(3, 1);
		f.op(OP_ELSE);
			emitBridge(c, pc, insn, false);
		f.end();
	};

	if (fCond(insn) == 0xE) {
		body();
	} else {
		c.pushCondition(fCond(insn));
		f.ifVoid();
			body();
		f.op(OP_ELSE);
			c.charge(kCyclesPerInsn, 1);
		f.end();
	}
}

// A branch whose target leaves the region. Mirrors execBranch: link register
// first, then the pipeline flush and the new PC — all compile-time constants.
void emitBranchOut(Ctx &c, uint32_t pc, uint32_t insn) {
	if (fBranchL(insn)) c.storeConst(c.gpr(14), pc + 4);
	// The shuffle ran before execBranch did, so the slots hold the two words
	// after the branch even though the count says they are stale.
	c.setState(branchTarget(insn, pc), c.wordAt(pc + 4), c.wordAt(pc + 8), 0);
	// 1 tick base + 1 for executing + execBranch's 2 for the refill it forces.
	c.charge(4, 1);
	c.exitRaw();
}

// An address the device watches for by sampling getRealPC(). A region that
// contained one would hide it: a region runs for hundreds of tick-equivalents
// without returning, so an address inside it is never observable at a sampling
// point and the hook keyed on it stops firing.
bool isTraceStop(const Layout &L, uint32_t pc) {
	for (uint32_t i = 0; i < L.traceStopCount; i++)
		if (L.traceStops[i] == pc) return true;
	return false;
}

// Whether this instruction can change processor mode or privilege.
//
// A region is compiled against ONE resolved code page, and that resolution is
// permission-checked for one privilege level — ARM710::tickPageLoop re-checks
// `fp.priv != isPrivileged()` before every instruction and re-resolves when it
// changes. A region cannot re-resolve, so it must not keep executing across a
// mode change: after one, the page it is reading its instructions from may be
// one the guest is no longer allowed to fetch from, where the interpreter would
// take a permission fault and the region would not.
//
// So an instruction that can change mode is still bridged — it is the
// interpreter that runs it either way — but it ends the trace.
bool bridgeCanChangeMode(uint32_t insn, uint8_t kind) {
	switch (kind) {
	case DK_DATAPROC: {
		const uint32_t op = fOpcode(insn);
		if (opIsOutputless(op) && !fS(insn)) return true;      // MRS / MSR
		if (fRd(insn) == 15 && fS(insn)) return true;          // MOVS pc, lr
		return false;
	}
	case DK_LDM_STM:
		// LDM ^ with PC in the list restores CPSR from the SPSR.
		return bit(insn, 22) && bit(insn, 20) && (insn & 0x8000);
	case DK_MULTIPLY:
	case DK_MULTIPLY_LONG:
	case DK_LDR_STR:
	case DK_HALFWORD:
	case DK_SWAP:
		return false;
	default:
		// DK_SLOW is SWI, coprocessor, BX/BLX and undefined — every one of them
		// either raises an exception or is not worth reasoning about here.
		return true;
	}
}

// Whether re-running this instruction from the same registers is a no-op the
// second time round. Anything that stores, swaps, or touches mode or banked
// state is not: restoring registers rolls back neither guest memory nor the
// bank the registers came from.
bool bridgeIsReRunSafe(uint32_t insn, uint8_t kind) {
	switch (kind) {
	case DK_MULTIPLY:
	case DK_MULTIPLY_LONG:
		return true;
	case DK_LDR_STR:
	case DK_HALFWORD:
		return bit(insn, 20) != 0;              // load, not store
	case DK_LDM_STM:
		return bit(insn, 20) != 0 && !bit(insn, 22);   // load, and not the ^ form
	case DK_DATAPROC: {
		// MRS/MSR touch CPSR mode and the SPSR banks. So does a PC write with S
		// set — that is an exception return (MOVS pc, lr), which reloads CPSR
		// from the SPSR and switches bank, and a register snapshot captures
		// neither. Marking one of those re-runnable does not just weaken the
		// check, it makes it unsound: the second run executes in a different
		// bank from the first.
		const uint32_t op = fOpcode(insn);
		if (opIsOutputless(op) && !fS(insn)) return false;     // MRS / MSR
		if (fRd(insn) == 15 && fS(insn)) return false;         // exception return
		return true;
	}
	default:
		return false;                            // DK_SLOW, SWP, anything new
	}
}

}  // namespace

bool acceptsInline(uint32_t insn, bool isTVersion) {
	if (ARM710::decodeKindV(insn, isTVersion) != DK_DATAPROC) return false;
	const uint32_t rd = fRd(insn), rn = fRn(insn), opcode = fOpcode(insn);
	if (rd == 15 || rn == 15) return false;
	if (!fI(insn)) {
		if (bit(insn, 4)) return false;        // shift amount in a register
		if (fRm(insn) == 15) return false;
	}
	if (opIsOutputless(opcode) && !fS(insn)) return false;   // MRS / MSR
	if (fCond(insn) == 0xF) return false;      // NV: never executes
	return true;
}

// The address a PC-relative load reads, or 0 when the instruction is not one.
//
// r15 reads as the instruction's address plus 12 in this interpreter, and
// execSingleDataTransfer subtracts 4 from a base of r15 — so the base is
// address + 8, which is what the architecture says it should be.
uint32_t literalAddress(uint32_t insn, uint32_t pc, bool isTVersion) {
	if (ARM710::decodeKindV(insn, isTVersion) != DK_LDR_STR) return 0;
	if (!bit(insn, 20)) return 0;                      // a store to [pc] is not this
	if (fCond(insn) == 0xF) return 0;
	if (fRn(insn) != 15) return 0;
	if (fRd(insn) == 15) return 0;                     // loading the PC is an exit
	if (fI(insn)) return 0;                            // register offset: not a literal
	if (!bit(insn, 24) || bit(insn, 21)) return 0;     // post-index or write-back writes r15
	const uint32_t off = bits(insn, 11, 0);
	const uint32_t addr = bit(insn, 23) ? (pc + 8 + off) : (pc + 8 - off);
	if (!bit(insn, 22) && (addr & 3)) return 0;        // unaligned word: the interpreter's
	return addr;
}

bool acceptsInlineStore(uint32_t insn, bool isTVersion) {
	if (ARM710::decodeKindV(insn, isTVersion) != DK_LDR_STR) return false;
	if (bit(insn, 20)) return false;                   // loads have their own path
	if (fCond(insn) == 0xF) return false;
	// r15 as the value stored or as the base: a region keeps it in constants.
	if (fRd(insn) == 15 || fRn(insn) == 15) return false;
	if (!bit(insn, 24) && bit(insn, 21)) return false; // STRT
	if (fI(insn)) {
		if (bit(insn, 4)) return false;
		if (fRm(insn) == 15) return false;
	}
	return true;
}

bool acceptsInlineLoad(uint32_t insn, bool isTVersion) {
	if (ARM710::decodeKindV(insn, isTVersion) != DK_LDR_STR) return false;
	if (!bit(insn, 20)) return false;                  // stores stay bridged
	if (fCond(insn) == 0xF) return false;              // NV: never executes
	const uint32_t rd = fRd(insn), rn = fRn(insn);
	// A region keeps r15 in constants and materialises it only at an exit, so
	// an instruction that reads or writes it belongs to the interpreter. Rd==15
	// also flushes the pipeline, which is an exit either way.
	if (rd == 15 || rn == 15) return false;
	// LDRT: post-indexed with write-back runs the access as if unprivileged,
	// which is a different permission lookup from the one emitted here.
	if (!bit(insn, 24) && bit(insn, 21)) return false;
	if (fI(insn)) {                                    // register offset
		if (bit(insn, 4)) return false;                // not addressing mode 2
		if (fRm(insn) == 15) return false;
	}
	// Write-back to the transfer's own destination register is ambiguous
	// ordering: the interpreter writes Rd from memory and then Rn from the
	// modified base, so Rn wins — but only because it happens second. Rather
	// than depend on that, leave it to the interpreter.
	if (rd == rn && ((bit(insn, 24) && bit(insn, 21)) || !bit(insn, 24))) return false;
	return true;
}

bool compileRegion(const CodePage &page, uint32_t entryPc, int maxInsns,
                   int minInsns, const Layout &layout, Region &out) {
	// ── Pass 1: walk the trace ─────────────────────────────────────────────
	// A position needs its own word plus the two that would be in flight after
	// it, because every exit restores the pipeline from constants.
	auto readable = [&](uint32_t pc) { return pc >= page.baseVa && pc + 12 <= page.endVa; };
	auto wordAt   = [&](uint32_t pc) { return page.words[(pc - page.baseVa) >> 2]; };

	std::vector<TracePos> trace;
	uint32_t pc = entryPc;
	bool loops = false;
	int chained = 0;
	Region::Stop stop = Region::Stop::MaxInsns;
	// The guest words the region reads, as contiguous runs. A followed branch
	// closes one and opens another; see Region::runs.
	std::vector<Run> runs;
	// [runLo, runEnd) — every word the current run has to cover: its
	// instructions plus the two past the last one that an exit restores the
	// prefetch pipeline from, plus any literal it reads out of the code page.
	uint32_t runLo = entryPc, runEnd = entryPc + 12;
	auto noteInsn = [&](uint32_t at) {
		if (at < runLo) runLo = at;
		if (at + 12 > runEnd) runEnd = at + 12;
	};
	// A literal the region reads as a constant. Refused when it would grow the
	// run by more than this, because every word in a run is hashed on entry:
	// a constant pool sits just past the code that uses it, and one far enough
	// away to blow the budget is better left to the interpreter.
	auto noteLiteral = [&](uint32_t at) -> bool {
		constexpr uint32_t kMaxGrow = 64 * 4;
		const uint32_t lo = at < runLo ? at : runLo;
		const uint32_t end = at + 4 > runEnd ? at + 4 : runEnd;
		if ((runLo - lo) + (end - runEnd) > kMaxGrow) return false;
		if (end > page.endVa) return false;
		runLo = lo; runEnd = end;
		return true;
	};
	auto closeRun = [&]() { runs.push_back({runLo, (runEnd - runLo) >> 2}); };
	while (true) {
		if ((int)trace.size() >= maxInsns) { stop = Region::Stop::MaxInsns; break; }
		if (!readable(pc)) { stop = Region::Stop::OffPage; break; }
		// Re-entering the trace anywhere but through the entry loop edge would
		// need a branch target wasm cannot express without a full CFG.
		bool seen = false;
		for (const auto &t : trace) if (t.pc == pc) { seen = true; break; }
		if (seen) { stop = Region::Stop::Seen; break; }

		// Stop before an address the device watches for. If the region would
		// START on one there is nothing to compile: the interpreter has to run
		// it, every time, so that the hook can see it.
		if (isTraceStop(layout, pc)) { stop = Region::Stop::TraceStop; break; }

		const uint32_t insn = wordAt(pc);
		const uint8_t kind = ARM710::decodeKindV(insn, layout.isTVersion);

		if (kind == DK_BRANCH && fCond(insn) != 0xF) {
			if (!layout.allowBranch) { stop = Region::Stop::BranchRefused; break; }
			const uint32_t target = branchTarget(insn, pc);
			const bool back = layout.allowLoop && (target == entryPc) && !fBranchL(insn);
			// An UNCONDITIONAL branch that is not the loop edge: follow it and
			// keep tracing at the target, rather than ending the region here.
			//
			// Only unconditional. A conditional branch has two live successors
			// and a trace has one — following the taken side would leave the
			// fall-through unreachable, which is a CFG, not a trace. Those keep
			// the old shape: exit inside the `if`, carry on underneath.
			//
			// The target has to be somewhere the region can actually read: in
			// this page (its instructions and pipeline restores are constants
			// baked in from it), not already in the trace (re-entering anywhere
			// but the entry loop edge needs a branch target wasm cannot express),
			// and not an address the device is watching for.
			if (!back && fCond(insn) == 0xE && layout.allowChain &&
			    chained < layout.maxChains && readable(target) &&
			    !isTraceStop(layout, target)) {
				bool tseen = false;
				for (const auto &t : trace) if (t.pc == target) { tseen = true; break; }
				if (!tseen) {
					trace.push_back({pc, insn, Step::BranchThrough});
					chained++;
					noteInsn(pc);
					closeRun();
					pc = target;
					runLo = target; runEnd = target + 12;
					continue;
				}
			}
			trace.push_back({pc, insn, back ? Step::BranchBack : Step::BranchOut});
			if (back) loops = true;
			noteInsn(pc);
			// An unconditional branch ends the trace either way: nothing after
			// it is reachable. A conditional one falls through, so keep going.
			if (fCond(insn) == 0xE) {
				stop = back ? Region::Stop::BranchNotTaken
				            : Region::Stop::BranchNotTaken;
				pc += 4; break;
			}
			pc += 4;
			continue;
		}

		// A constant-pool read: the cheapest thing here to compile, and the
		// commonest load in ARM code. Taken only when the literal is in this
		// page and close enough that adding it to the region's hashed words is
		// not itself the cost.
		if (layout.allowLiteral) {
			const uint32_t la = literalAddress(insn, pc, layout.isTVersion);
			if (la && la + 4 <= page.endVa && la >= page.baseVa) {
				const uint32_t saveLo = runLo, saveEnd = runEnd;
				noteInsn(pc);
				if (noteLiteral(la)) {
					trace.push_back({pc, insn, Step::Literal});
					pc += 4;
					continue;
				}
				runLo = saveLo; runEnd = saveEnd;      // too far: leave it bridged
			}
		}

		// A store the micro-TLB can serve inline. Like an inlined load it can
		// still miss and hand over, so it is only taken where a bridge would
		// have been allowed — and unlike a load it writes guest memory, so the
		// region stops being checkable by re-running it.
		if (layout.inlineLoads && layout.mem.mruBase && layout.mem.decodePagesPtr &&
		    acceptsInlineStore(insn, layout.isTVersion) &&
		    layout.allowBridge && ((layout.bridgeKinds >> kind) & 1u) &&
		    !layout.bridgeReRunSafeOnly) {
			trace.push_back({pc, insn, Step::InlineStore});
			noteInsn(pc);
			pc += 4;
			continue;
		}

		// A load the micro-TLB can serve inline. It still counts as bridgeable
		// for the budget — the guard can miss at runtime and hand it over — so
		// the trace only takes it where a bridge would have been allowed too.
		if (layout.inlineLoads && layout.mem.mruBase &&
		    acceptsInlineLoad(insn, layout.isTVersion) &&
		    layout.allowBridge && ((layout.bridgeKinds >> kind) & 1u) &&
		    !(layout.bridgeReRunSafeOnly && !bridgeIsReRunSafe(insn, kind))) {
			trace.push_back({pc, insn, Step::InlineLoad});
			noteInsn(pc);
			pc += 4;
			continue;
		}

		const bool inl = acceptsInline(insn, layout.isTVersion);
		if (!inl && (!layout.allowBridge
		             || (layout.bridgeReRunSafeOnly && !bridgeIsReRunSafe(insn, kind))
		             || (!layout.allowBridgeSlow && kind <= DK_SLOW)
		             || !((layout.bridgeKinds >> kind) & 1u))) {
			stop = Region::Stop::BridgeRefused; break;
		}
		trace.push_back({pc, insn, inl ? Step::Inline : Step::Bridge});
		noteInsn(pc);
		// An unconditional write to r15 redirects execution, and the bridge's
		// own post-check makes the region leave when it does. Everything the
		// trace would go on to compile after one is unreachable — dead code in
		// the module, and compile time spent on it. `LDR pc, [...]` is the
		// common shape: 4,733 of them in one netBook run's regions, none of
		// which could run.
		if (!inl && fCond(insn) == 0xE &&
		    ((kind == DK_LDR_STR && bit(insn, 20) && fRd(insn) == 15) ||
		     (kind == DK_LDM_STM && bit(insn, 20) && (insn & 0x8000)))) {
			stop = Region::Stop::ModeChange; pc += 4; break;
		}
		// A mode change invalidates the privilege the page was resolved under,
		// so the trace stops here — the bridge still runs the instruction, and
		// the region exits straight afterwards. See bridgeCanChangeMode.
		if (!inl && bridgeCanChangeMode(insn, kind)) {
			stop = Region::Stop::ModeChange; pc += 4; break;
		}
		pc += 4;
	}
	// Where execution continues when the trace simply runs out — which is not
	// `last position + 4` any more, because a followed branch moves it.
	const uint32_t fallthroughPc = pc;
	closeRun();

	if ((int)trace.size() < minInsns) return false;

	// One walk over the trace for everything the caller needs to know about it:
	// how much of it compiled, what it costs, and where the bridges sit.
	//
	// Worst-case cycles for one pass feed the budget guard at the loop head. A
	// bridged instruction's real cost is not known until it runs, so assume the
	// most the interpreter charges for one (LDM of 16 registers). Ticks are one
	// per position EXCEPT a taken branch, which costs three — the branch tick
	// plus the two pure refill ticks its flushed pipeline forces before anything
	// executes again. That applies to the loop edge and to every followed one.
	constexpr uint32_t kBridgeWorstCase = 32;
	int nInline = 0, nBridged = 0, nInlineLoads = 0, nLiterals = 0, nInlineStores = 0;
	int bridgedKind[16] = {0}, bridgedLdrWhy[4] = {0};
	bool reRunSafe = true;
	std::vector<int> bridgeAt;
	std::vector<uint32_t> bridgePc;
	std::vector<char> bridgeFast;
	uint32_t traceCycles = 0, traceTicks = 0;
	for (const auto &t : trace) {
		switch (t.step) {
		case Step::Inline:
			nInline++;
			traceCycles += kCyclesPerInsn; traceTicks += 1;
			break;
		case Step::Bridge: {
			const uint8_t k = ARM710::decodeKindV(t.insn, layout.isTVersion);
			nBridged++;
			// Tick offset into a pass, not trace position: those stopped being
			// the same number when a followed branch started costing three.
			bridgeAt.push_back((int)traceTicks);
			bridgePc.push_back(t.pc);
			bridgeFast.push_back(k > DK_SLOW ? 1 : 0);
			bridgedKind[k & 15]++;
			if (k == DK_LDR_STR)
				bridgedLdrWhy[!bit(t.insn, 20) ? 0
				              : fRn(t.insn) == 15 ? 1
				              : fRd(t.insn) == 15 ? 2 : 3]++;
			if (!bridgeIsReRunSafe(t.insn, k)) reRunSafe = false;
			traceCycles += kBridgeWorstCase; traceTicks += 1;
			break;
		}
		case Step::Literal:
			// No memory access and no way to miss: this one really is inline.
			nInline++; nLiterals++;
			traceCycles += 3; traceTicks += 1;
			break;
		case Step::InlineStore:
			// A store is not re-run safe whether it is inlined or bridged:
			// restoring registers rolls back neither guest memory nor the
			// decoded page the write may have dropped.
			nInline++; nInlineStores++; reRunSafe = false;
			bridgeAt.push_back((int)traceTicks);
			bridgePc.push_back(t.pc);
			bridgeFast.push_back(1);
			traceCycles += kBridgeWorstCase; traceTicks += 1;
			break;
		case Step::InlineLoad:
			// Counted as a bridge for the budget and as one for re-run safety:
			// the guard can miss and hand the instruction over, so the region
			// has to be able to afford that.
			nInline++; nInlineLoads++;
			bridgeAt.push_back((int)traceTicks);
			bridgePc.push_back(t.pc);
			bridgeFast.push_back(1);
			traceCycles += kBridgeWorstCase; traceTicks += 1;
			break;
		case Step::BranchOut:
			traceCycles += 4; traceTicks += 1;
			break;
		case Step::BranchBack:
		case Step::BranchThrough:
			traceCycles += kBranchCycles; traceTicks += kBranchTicks;
			break;
		}
	}
	// A region that compiles nothing is strictly worse than the interpreter: it
	// adds a call and a table lookup around the same work.
	if (nInline == 0) return false;

	// ── Pass 2: emit ───────────────────────────────────────────────────────
	Ctx c;
	c.L = layout;
	c.page = &page;
	c.entryPc = entryPc;
	c.f.nParams = 2;                       // $maxCycles, $maxTicks
	c.pMaxCycles = 0;
	c.lTotal   = c.f.addLocal();
	c.lFlushed = c.f.addLocal();
	c.lTicks = c.f.addLocal();
	c.lTmp   = c.f.addLocal();
	c.lOp1   = c.f.addLocal();
	c.lOp2   = c.f.addLocal();
	c.lRes   = c.f.addLocal();
	c.lRes1  = c.f.addLocal();
	c.lCarry = c.f.addLocal();
	c.lShift = c.f.addLocal();
	c.lMemA  = c.f.addLocal();
	c.lMemB  = c.f.addLocal();
	c.lMemC  = c.f.addLocal();
	c.lMemD  = c.f.addLocal();
	c.lMemE  = c.f.addLocal();

	// The bridge signature index has to match the one the linked module will
	// intern. Both are (i32) -> i32 and it is the first signature either module
	// interns, so it is index 0 in both — asserted by construction below, where
	// linkRegions interns the same signature first.
	c.bridgeType = 0;   // (cpu, pc) -> cycles; see linkRegions

	if (layout.stampEntry) c.storeConst(layout.stampAddr, entryPc);
	c.f.loopVoid();
	{
		// Guard, once per pass rather than once per instruction.
		//
		// Interrupts: no inline instruction can raise one (no memory access, no
		// device contact) and the budget below keeps the pass inside the
		// current device-event window, so nothing can become pending mid-pass.
		// Bridges do their own check.
		c.f.load8Abs(layout.pendingIrqAddr);
		c.f.load8Abs(layout.pendingFiqAddr);
		c.f.op(OP_I32_OR);
		// Budget: leave rather than start a pass that could run past the
		// caller's cycle ceiling. Conservative on purpose — the interpreter
		// picks up the few instructions that did not fit. Overshooting the
		// ceiling slides interrupt delivery, which is the drift bug Stage 1
		// spent three rounds finding.
		c.f.localGet(c.lTotal); c.f.i32Const((int32_t)traceCycles); c.f.op(OP_I32_ADD);
		c.f.localGet(c.pMaxCycles); c.f.op(OP_I32_GTU);
		c.f.op(OP_I32_OR);
		// And the same for TICKS, which is not the same bound at all.
		//
		// The caller batches its per-batch device housekeeping once per
		// stepCpu() call — CF mount fixups, timer veneers — and the burst
		// engine is capped at 16 ticks so that cadence holds. A looping region
		// happily retires a hundred ticks in one call, which runs those hooks
		// six times less often; measured on the netBook with a Quartz card,
		// that alone changed the final framebuffer. Honouring the same tick cap
		// makes a region indistinguishable from the burst engine to the caller.
		c.f.localGet(c.lTicks); c.f.i32Const((int32_t)traceTicks); c.f.op(OP_I32_ADD);
		c.f.localGet(c.pMaxTicks); c.f.op(OP_I32_GTU);
		c.f.op(OP_I32_OR);
		// And the page this region is built out of must still be the page it was
		// built from — see the note at the bridge, which is the only place it
		// can change.
		if (layout.validPtrAddr) {
			c.f.loadAbs(layout.validPtrAddr);
			c.f.i32Const((int32_t)layout.validPhysBase); c.f.op(OP_I32_NE);
			c.f.op(OP_I32_OR);
		}
		c.f.ifVoid();
			c.exitAt(entryPc);
		c.f.end();

		for (const auto &t : trace) {
			switch (t.step) {
			case Step::Inline:
				if (fCond(t.insn) == 0xE) {
					emitDataProc(c, t.insn);
				} else {
					c.pushCondition(fCond(t.insn));
					c.f.ifVoid();
						emitDataProc(c, t.insn);
					c.f.end();
				}
				c.charge(kCyclesPerInsn, 1);
				break;

			case Step::Bridge:
				emitBridge(c, t.pc, t.insn,
				           ARM710::decodeKindV(t.insn, layout.isTVersion) <= DK_SLOW);
				break;

			case Step::InlineLoad:
				emitInlineLoad(c, t.pc, t.insn);
				break;

			case Step::InlineStore:
				emitInlineStore(c, t.pc, t.insn);
				break;

			case Step::Literal:
				emitLiteralLoad(c, t.insn,
				                literalAddress(t.insn, t.pc, layout.isTVersion));
				break;

			case Step::BranchOut:
				if (fCond(t.insn) == 0xE) {
					emitBranchOut(c, t.pc, t.insn);
				} else {
					c.pushCondition(fCond(t.insn));
					c.f.ifVoid();
						emitBranchOut(c, t.pc, t.insn);
					c.f.end();
					// Not taken: the tick still costs its base plus the executed
					// charge, exactly as the loop's `clocks = 1; clocks += 1`.
					c.charge(kCyclesPerInsn, 1);
				}
				break;

			case Step::BranchThrough:
				// A taken unconditional branch the trace followed. Nothing has
				// to be materialised: r15 and the prefetch pipeline are only
				// read at a bridge or an exit, and both write them from the
				// trace position they are at, which is now the target. All that
				// is owed is the link register and the cost — the branch tick
				// plus the two refills, exactly what the loop edge charges.
				if (fBranchL(t.insn)) c.storeConst(c.gpr(14), t.pc + 4);
				c.charge(kBranchCycles, kBranchTicks);
				break;

			case Step::BranchBack:
				// Restore the entry pipeline before looping: a bridge earlier in
				// the pass may have moved it on.
				if (fCond(t.insn) == 0xE) {
					c.charge(kBranchCycles, kBranchTicks);
					c.setState(entryPc + 8, wordAt(entryPc), wordAt(entryPc + 4), 2);
					c.f.br(0);                      // to the enclosing loop
				} else {
					c.pushCondition(fCond(t.insn));
					c.f.ifVoid();
						c.charge(kBranchCycles, kBranchTicks);
						c.setState(entryPc + 8, wordAt(entryPc), wordAt(entryPc + 4), 2);
						c.f.br(1);                  // out through the `if`, to the loop
					c.f.end();
					c.charge(kCyclesPerInsn, 1);    // not taken
				}
				break;
			}
		}

		// Ran off the end of the trace: leave where execution continues, which
		// a followed branch has moved away from `last position + 4`.
		c.exitAt(fallthroughPc);
	}
	c.f.end();                                  // loop
	// Unreachable — every path above returns or branches — but a function body
	// still has to be well-typed at its end.
	c.f.i32Const(0);

	out.func = std::move(c.f);
	out.nInsns = nInline;
	out.nBridged = nBridged;
	out.nSpan = (int)trace.size();
	out.traceCycles = traceCycles;
	out.traceTicks = traceTicks;
	out.loops = loops;
	out.bridgeAt = std::move(bridgeAt);
	out.bridgePc = std::move(bridgePc);
	out.bridgeFast = std::move(bridgeFast);
	out.reRunSafe = reRunSafe;
	out.runs = std::move(runs);
	out.nChained = chained;
	out.nInlineLoads = nInlineLoads;
	out.nLiterals = nLiterals;
	out.nInlineStores = nInlineStores;
	for (int i = 0; i < 16; i++) out.bridgedKind[i] = bridgedKind[i];
	for (int i = 0; i < 4; i++) out.bridgedLdrWhy[i] = bridgedLdrWhy[i];
	out.stop = stop;
	return true;
}

std::string exportName(size_t i) { return "blk" + std::to_string(i); }

std::vector<uint8_t> linkRegions(std::vector<Region> &regions, const Layout &layout) {
	Module m;
	m.importTable();
	// Interned FIRST so it lands at type index 0, which is what compileRegion
	// baked into every call_indirect it emitted. Regions are compiled before
	// the module that will hold them exists, so the two have to agree on this
	// by construction rather than by lookup.
	const uint32_t bridgeType = m.signatureIndex({VT_I32, VT_I32}, true);
	(void)bridgeType;
	// And the helpers go in FIRST, so their function indices are kHelper* —
	// which is what compileRegion baked into every `call` it emitted, for the
	// same reason.
	auto helpers = buildMemHelpers(layout);
	for (int i = 0; i < kHelperCount; i++) {
		std::vector<ValType> params;
		bool ret = true;
		switch (i) {
		case kHelperHostReadW: case kHelperHostReadB: params = {VT_I32}; break;
		case kHelperStoreW:    case kHelperStoreB:    params = {VT_I32, VT_I32}; break;
		case kHelperFlush:     params = {VT_I32}; ret = false; break;
		default:               params = {VT_I32, VT_I32, VT_I32, VT_I32}; ret = false; break;
		}
		m.addFunc("", params, ret, std::move(helpers[(size_t)i]));
	}
	for (size_t i = 0; i < regions.size(); i++)
		m.addFunc(exportName(i), {VT_I32, VT_I32}, true, std::move(regions[i].func));
	return m.finish();
}

}  // namespace armjit
