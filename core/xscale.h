// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

// Intel XScale (ARMv5TE) core, as found in the PXA255 that runs the
// Psion netBook Pro / Psion LX.
//
// This is a separate core from ARM710 (core/arm710.*): that one is an
// ARMv4 interpreter tuned for EPOC, shared by every ARM machine in the tree,
// and has no Thumb state, no v5 instructions and no XScale coprocessors.
// Rather than widen a hot, shared core for one machine, the XScale lives on
// its own. What it implements:
//
//  - ARM and Thumb state, the whole ARMv5TE instruction set: BLX, CLZ, BKPT,
//    the saturating Q-ops, the 16-bit signed multiplies (SMLAxy & co),
//    LDRD/STRD, PLD, MCRR/MRRC, and v5 interworking on loads to the PC.
//  - The XScale coprocessors: CP15 (MMU, caches, FCSE PID, coprocessor access
//    register), CP14 (clock configuration, power mode / idle, performance
//    monitor), and CP0's 40-bit DSP accumulator (MIA*, MAR, MRA). CP1/CP2 and
//    every other coprocessor raise the undefined-instruction trap, which is
//    what Linux's NWFPE floating-point emulator expects.
//  - The ARMv5 MMU with XScale's extended small pages, S/R protection bits,
//    domains, alignment checking, and "base restored" data aborts (the
//    abort model Linux selects for XScale, CPU_ABRT_EV5T).
//
// Memory goes through a small software TLB that caches, per 4 KB virtual
// page and per privilege level, the physical page and a host pointer when
// the page is RAM, so ordinary loads, stores and fetches never walk the
// tables. Everything else (I/O) goes to XScaleBus.

namespace XScale {

class Bus {
public:
	virtual ~Bus() = default;
	// Physical access outside RAM. size is 1, 2 or 4. Return false for an
	// external abort (nothing decodes the address).
	virtual bool busRead(uint32_t pa, int size, uint32_t &out) = 0;
	virtual bool busWrite(uint32_t pa, int size, uint32_t val) = 0;
	// CP14 CCLKCFG write (frequency change / turbo). Optional.
	virtual void onClockConfig(uint32_t v) { (void)v; }
	// CP14 PWRMODE write with a mode other than idle (sleep / deep sleep).
	virtual void onPowerMode(uint32_t v) { (void)v; }
};

enum Mode : uint32_t {
	USR = 0x10, FIQ = 0x11, IRQ = 0x12, SVC = 0x13,
	ABT = 0x17, UND = 0x1B, SYS = 0x1F,
};

class Cpu {
public:
	explicit Cpu(Bus &bus);

	void reset();
	// RAM fast path: physical [base, base+size) is backed by host memory.
	// Region 0 (setRam) is main memory, the only place page tables are read
	// from directly; addRam adds further directly-mapped regions (a
	// framebuffer, say). Accesses anywhere else go to the Bus.
	void setRam(uint8_t *ram, uint32_t base, uint32_t size) {
		ram_ = ram; ramBase_ = base; ramSize_ = size;
		regions_[0] = { ram, base, size };
		if (nRegions_ == 0) nRegions_ = 1;
		invalidateDecodeAll();
		dPages_.assign(size >> 12, nullptr);
		codeMask_.assign(size >> 12, 0);
		flushTlb();
	}
	void addRam(uint8_t *mem, uint32_t base, uint32_t size) {
		if (nRegions_ == 0) nRegions_ = 1;
		if (nRegions_ < kMaxRegions) regions_[nRegions_++] = { mem, base, size };
		flushTlb();
	}

	// Execute up to n instructions. Returns the number executed. Stops early
	// (returning fewer) when the core enters idle with nothing pending.
	uint32_t run(uint32_t n);

	// Instructions executed so far in the current run() call (for devices
	// that read a free-running counter mid-slice), and a request to end the
	// slice after the current instruction (a register write moved the next
	// timer event earlier, say).
	uint32_t sliceDone() const { return runDone_; }
	void stopRun() { runLimit_ = 0; }

	void setIrqLine(bool level) { irqLine_ = level; if (level) idle_ = false; }
	void setFiqLine(bool level) { fiqLine_ = level; if (level) idle_ = false; }
	bool isIdle() const { return idle_; }
	void wake() { idle_ = false; }

	// Register access (current mode's view).
	uint32_t reg(int i) const { return i == 15 ? pc_ : r_[i]; }
	void setReg(int i, uint32_t v) { if (i == 15) pc_ = v; else r_[i] = v; }
	uint32_t pc() const { return pc_; }
	void setPc(uint32_t v) { pc_ = v; }
	uint32_t cpsr() const;
	void setCpsr(uint32_t v);   // full write, switches banks
	bool thumb() const { return tbit_; }

	// Debug / host access: translate without side effects (no fault state).
	bool debugTranslate(uint32_t va, uint32_t &pa);
	bool debugRead32(uint32_t va, uint32_t &out);
	bool debugRead8(uint32_t va, uint8_t &out);

	// Statistics.
	uint64_t instructions() const { return insnCount_; }
	uint32_t lastExceptionPc() const { return lastExcPc_; }
	uint32_t controlReg() const { return cp15Control_; }
	uint32_t ttb() const { return ttb_; }

	// Optional trace hook: called before each instruction when non-null.
	void (*traceHook)(void *ctx, uint32_t pc, uint32_t insn, bool thumb) = nullptr;
	void *traceCtx = nullptr;

	// Counts of exceptions taken, by kind, for diagnostics.
	uint64_t excCount[8] = {};

	void flushTlb();

	// Predecoded execution of ARM-state code (see "Decoded instruction
	// cache" in xscale.cpp). On by default; off runs every instruction
	// through the plain decoder, which is also what the trace hook uses.
	void setPredecode(bool on) { predecode_ = on; invalidateDecodeAll(); }
	// Something other than the core (DMA, the host) changed main memory.
	void notifyRamWrite(uint32_t pa, uint32_t len);
	void invalidateDecodeAll();

private:
	// ── Architectural state ──────────────────────────────────────────────
	uint32_t r_[16] = {};          // r_[15] unused; pc_ is authoritative
	uint32_t pc_ = 0;              // address of the instruction being executed
	uint32_t fN_ = 0, fZ_ = 0, fC_ = 0, fV_ = 0, fQ_ = 0;
	uint32_t mode_ = SVC;
	bool     iMask_ = true, fMask_ = true, tbit_ = false;
	// Banked registers.
	uint32_t usrR8_14_[7] = {};    // r8..r14 for USR/SYS (and non-FIQ r8..r12)
	uint32_t fiqR8_14_[7] = {};
	uint32_t irqR13_14_[2] = {}, svcR13_14_[2] = {}, abtR13_14_[2] = {}, undR13_14_[2] = {};
	uint32_t spsrFiq_ = 0, spsrIrq_ = 0, spsrSvc_ = 0, spsrAbt_ = 0, spsrUnd_ = 0;

	uint32_t *spsrPtr();
	void switchMode(uint32_t newMode);
	bool privileged() const { return mode_ != USR; }

	// ── Coprocessor state ────────────────────────────────────────────────
	uint32_t cp15Control_ = 0x00000078;
	uint32_t cp15Aux_ = 0;
	uint32_t ttb_ = 0, dacr_ = 0, fsr_ = 0, far_ = 0, pid_ = 0, cpar_ = 0;
	uint32_t cp15Dbg_[4] = {};
	uint32_t cclkcfg_ = 0, pwrmode_ = 0;
	uint32_t pmnc_ = 0, ccnt_ = 0, pmn0_ = 0, pmn1_ = 0, evtsel_ = 0, inten_ = 0, flag_ = 0;
	uint64_t ccntBase_ = 0;
	int64_t  acc0_ = 0;            // 40-bit DSP accumulator, kept sign-extended

	// ── Interrupt lines ─────────────────────────────────────────────────
	bool irqLine_ = false, fiqLine_ = false, idle_ = false;

	// ── Memory ──────────────────────────────────────────────────────────
	Bus &bus_;
	uint8_t *ram_ = nullptr;
	uint32_t ramBase_ = 0, ramSize_ = 0;
	struct Region { uint8_t *mem; uint32_t base, size; };
	static constexpr int kMaxRegions = 4;
	Region regions_[kMaxRegions] = {};
	int nRegions_ = 0;
	// Host pointer for physical pa if it lies in a RAM region, else null.
	uint8_t *hostPtr(uint32_t pa) const {
		for (int i = 0; i < nRegions_; i++)
			if (pa - regions_[i].base < regions_[i].size) return regions_[i].mem + (pa - regions_[i].base);
		return nullptr;
	}

	struct TlbEnt {
		uint32_t tag;      // (va & ~0xfff) | gen, or 0
		uint32_t pa;       // physical address of the 4 KB page
		uint8_t *hostR;    // RAM host pointer to the page if readable, else null
		uint8_t *hostW;    // ... if writable
		uint8_t  perm;     // bit0 readable, bit1 writable
	};
	enum { TlbBits = 10, TlbSize = 1 << TlbBits };
	TlbEnt tlbPriv_[TlbSize];
	TlbEnt tlbUser_[TlbSize];
	uint32_t tlbGen_ = 1;
	// Instruction fetch page cache.
	uint32_t fetchTag_ = 0;        // (pc & ~0xfff) | gen | 1-if-user<<? (reset on mode change)
	uint8_t *fetchHost_ = nullptr;

	enum AccessKind { ReadAcc = 0, WriteAcc = 1, FetchAcc = 2 };
	// Translate va for an access. On success fills pa and (maybe) the TLB.
	// On failure returns the FSR status code (nonzero) and domain.
	uint32_t translate(uint32_t va, bool write, bool user, uint32_t &pa, uint32_t &domainOut,
	                   TlbEnt *fill);
	uint32_t mva(uint32_t va) const { return (va < 0x02000000u) ? (va | pid_) : va; }

	bool readSlow(uint32_t va, int size, uint32_t &out, bool user);
	bool writeSlow(uint32_t va, int size, uint32_t val, bool user);
	bool fetchSlow(uint32_t va, uint32_t &out, int size);

	inline bool read32(uint32_t va, uint32_t &out, bool user);
	inline bool read16(uint32_t va, uint32_t &out, bool user);
	inline bool read8(uint32_t va, uint32_t &out, bool user);
	inline bool write32(uint32_t va, uint32_t v, bool user);
	inline bool write16(uint32_t va, uint32_t v, bool user);
	inline bool write8(uint32_t va, uint32_t v, bool user);
	inline bool fetch32(uint32_t va, uint32_t &out);
	inline bool fetch16(uint32_t va, uint32_t &out);

	bool alignCheck() const { return (cp15Control_ & 2) != 0; }
	void dataAbort(uint32_t va, uint32_t status, uint32_t domain);
	void prefetchAbort(uint32_t va, uint32_t status, uint32_t domain);
	// Pending abort raised by a memory helper during the current instruction.
	bool aborted_ = false;

	// ── Execution ───────────────────────────────────────────────────────
	uint64_t insnCount_ = 0;
	uint32_t runDone_ = 0, runLimit_ = 0;
	uint32_t lastExcPc_ = 0;
	bool branched_ = false;        // instruction wrote the PC
	void branchTo(uint32_t addr) { pc_ = addr; branched_ = true; }
	void branchInterwork(uint32_t addr) {
		if (addr & 1) { tbit_ = true; pc_ = addr & ~1u; }
		else          { tbit_ = false; pc_ = addr & ~3u; }
		branched_ = true;
	}
	void exception(uint32_t vectorOffset, uint32_t newMode, uint32_t lrValue, bool disableFiq);
	void undefined();

	void execArm(uint32_t insn);
	void execThumb(uint32_t insn);
	inline bool condPassed(uint32_t cond) const {
		switch (cond) {
		case 0x0: return fZ_;
		case 0x1: return !fZ_;
		case 0x2: return fC_;
		case 0x3: return !fC_;
		case 0x4: return fN_;
		case 0x5: return !fN_;
		case 0x6: return fV_;
		case 0x7: return !fV_;
		case 0x8: return fC_ && !fZ_;
		case 0x9: return !fC_ || fZ_;
		case 0xA: return fN_ == fV_;
		case 0xB: return fN_ != fV_;
		case 0xC: return !fZ_ && fN_ == fV_;
		case 0xD: return fZ_ || fN_ != fV_;
		default:  return true;
		}
	}

	void armDataProc(uint32_t insn);
	void armMisc(uint32_t insn);
	void armMultiply(uint32_t insn);
	void armExtraLoadStore(uint32_t insn);
	void armSingleLoadStore(uint32_t insn);
	void armBlock(uint32_t insn);
	void armCoproc(uint32_t insn);
	void armUncond(uint32_t insn);

	uint32_t armReadReg(int n) const { return n == 15 ? pc_ + 8 : r_[n]; }
	void writeRegAlu(int d, uint32_t v);         // ALU write: d==15 branches (no interwork)
	void writeRegLoad(int d, uint32_t v);        // load write: d==15 interworks
	void restoreCpsrFromSpsr();

	// Coprocessor dispatch. Return false -> undefined instruction.
	bool mcr(int cp, int opc1, int crn, int crm, int opc2, uint32_t v);
	bool mrc(int cp, int opc1, int crn, int crm, int opc2, uint32_t &v);

	inline void setNZ(uint32_t v) { fN_ = v >> 31; fZ_ = (v == 0); }

	// ── Decoded instruction cache ───────────────────────────────────────
public:
	struct DInsn {
		uint16_t op;          // handler index; 0 = not decoded yet
		uint8_t cond, rd, rn, rm, rs, aux;
		uint32_t imm;         // operand / offset / raw instruction, by handler
	};
private:
	struct DPage { DInsn ins[1024]; };
	std::vector<DPage *> dPages_;    // per 4 KB page of main memory
	std::vector<uint16_t> codeMask_; // per page: 256-byte chunks with decoded entries
	std::vector<DPage *> dFree_;
	size_t dLive_ = 0;
	bool predecode_ = true;
	uint32_t dVpage_ = 1;            // virtual page of dPage_ (1 = none; reset by
	                                 // any TLB flush or mode change)
	uint8_t *dHost_ = nullptr;
	DPage *dPage_ = nullptr;
	uint32_t dIndex_ = 0;
	bool lookupDecoded(uint32_t pc);
	void decodeArm(DInsn &d, uint32_t insn, uint32_t pc);
	void invalidateCodeChunk(uint32_t page, uint32_t chunk);
	bool isCodePage(uint32_t pa) const {
		uint32_t off = pa - ramBase_;
		return off < ramSize_ && codeMask_[off >> 12] != 0;
	}
	void execArmBody(uint32_t insn);
public:
	// Handlers (public only so the handler table can name them).
	static void hGeneric(Cpu &c, const DInsn &d);
	static void hB(Cpu &c, const DInsn &d);
	static void hBL(Cpu &c, const DInsn &d);
	static void hBlock(Cpu &c, const DInsn &d);
	static void hBlockFast(Cpu &c, const DInsn &d);
	static void hMul(Cpu &c, const DInsn &d);
	static void hLdrLit(Cpu &c, const DInsn &d);
	template <unsigned OP, bool S, unsigned K> static void hDp(Cpu &c, const DInsn &d);
	template <bool L, bool B, bool P, bool W, bool R> static void hSls(Cpu &c, const DInsn &d);
	template <unsigned T, bool P, bool W, bool R> static void hExtra(Cpu &c, const DInsn &d);
	typedef void (*Handler)(Cpu &, const DInsn &);
private:
};

} // namespace XScale
