// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2019 Ash Wolf (WindEmu); modifications (c) 2024-2026 Joe Haines.
// Subject to the Mozilla Public License v2.0 (http://mozilla.org/MPL/2.0/).

#pragma once
#include <stdint.h>
#include <optional>
#include <variant>
#include <functional>


//using namespace std;

// Everything I thought is a lie.
// Turns out the 5mx/Windermere is an ARM710T, not an ARM710a.

// ASSUMPTIONS:
// - Little-endian will be used
// - 26-bit address spaces will not be used
// - Alignment faults will always be on

// Write buffer is 4 address FIFO, 8 data FIFO
// TLB is 64 entries

// Speedhacks:
//#define ARM710T_CACHE  // incomplete — uses undeclared ARM710T methods
#define ARM710T_TLB

typedef std::optional<uint32_t> MaybeU32;

// Decoded-instruction kinds for the ROM decoded-op cache (Phase 1 of
// docs/execution-engine-scope.md). decodeKind() classifies a raw ARM word into
// one of these, mirroring executeInstruction()'s dispatch chain exactly. Only
// the "fast" kinds (>= DK_DATAPROC) take the cached fast path in tick(); SWI,
// coprocessor, BX/BLX, undefined and anything fetched from non-ROM all map to
// DK_SLOW and fall through to the full executeInstruction() path.
enum ArmDK : uint8_t {
	DK_UNCACHED = 0,   // ROM-cache sentinel: not yet decoded
	DK_SLOW     = 1,   // decoded → must use the full slow path
	DK_DATAPROC,
	DK_LDR_STR,
	DK_LDM_STM,
	DK_BRANCH,
	DK_MULTIPLY,
	DK_MULTIPLY_LONG,
	DK_SWAP,
	DK_HALFWORD,
};

class ARM710
{
public:
	enum ValueSize { V8 = 0, V32 = 1, V16 = 2 };

	enum MMUFault : uint64_t {
		// ref: datasheet 9-13 (p111)
		NoFault                 = 0,
		AlignmentFault          = 1,
		// the ARM gods say there is to be no fault 2 or 3
		SectionLinefetchError   = 4,
		SectionTranslationFault = 5,
		PageLinefetchError      = 6,
		PageTranslationFault    = 7,
		SectionOtherBusError    = 8,
		SectionDomainFault      = 9,
		PageOtherBusError       = 0xA,
		PageDomainFault         = 0xB,
		Lv1TranslationError     = 0xC,
		SectionPermissionFault  = 0xD,
		Lv2TranslationError     = 0xE,
		PagePermissionFault     = 0xF,

		// not actually in the ARM datasheet
		// so we are reusing it for nefarious purposes
		NonMMUError             = 3,

		MMUFaultTypeMask        = 0xF,
		MMUFaultDomainMask      = 0xF0,
		MMUFaultDomainShift     = 4,
		MMUFaultAddressMask     = 0xFFFFFFFF00000000,
		MMUFaultAddressShift    = 32
	};



	ARM710(bool _isTVersion) {
		isTVersion = _isTVersion;
		cp15_id = _isTVersion ? 0x41807100 : 0x41047100;
		clearAllValues();
		initFastPathGate();
	}
	virtual ~ARM710() { }

	void clearAllValues() {
		bank = MainBank;
		CPSR = 0;
		for (int i = 0; i < 16; i++) GPRs[i] = 0;
		for (int i = 0; i < 5; i++) {
			fiqBankedRegisters[0][i] = 0;
			fiqBankedRegisters[1][i] = 0;
		}
		for (int i = 0; i < 6; i++) {
			SPSRs[i] = 0;
		}
		for (int i = 0; i < 6; i++) {
			allModesBankedRegisters[i][0] = 0;
			allModesBankedRegisters[i][1] = 0;
		}

		cp15_control = 0;
		cp15_translationTableBase = 0;
		cp15_domainAccessControl = 0;
		cp15_faultStatus = 0;
		cp15_faultAddress = 0;
		prefetchCount = 0;
		series5ShadowActive = false;
		for (uint32_t &w : series5ShadowNThread) w = 0;
#ifdef ARM710T_CACHE
		clearCache();
#endif
#ifdef ARM710T_TLB
		flushTlb();
#endif
	}

	// L10: the constructor preloads cp15_id with the generic ARM710 /
	// ARM710T main-ID register value derived from `isTVersion`. The
	// owning Emulator (e.g. SA1100, Series 5) calls setProcessorID()
	// from its configure() path to override with the chip-specific
	// part number that real silicon would report (e.g. SA-1100 reports
	// 0x4401A118 instead of the generic 0x41807100). Last-wins is
	// intentional: the chip-specific value supersedes the generic core
	// default before reset() releases the CPU.
	void setProcessorID(uint32_t v) { cp15_id = v; }
	uint32_t getCp15Control() const { return cp15_control; }
	uint32_t getCp15Ttb()     const { return cp15_translationTableBase; }
	uint32_t getCp15Dacr()    const { return cp15_domainAccessControl; }
	void     setCp15Dacr(uint32_t v) { cp15_domainAccessControl = v; }
	// Public CP15 control-register setter for emulator-side bootloader
	// → OS handoff surgery (sa1100.cpp netBookHandoffToOs).  Real
	// silicon's v0.11 bootloader disables the MMU and the caches via
	// MCR p15,0,_,c1,c0,0 with bits M / C / W cleared before jumping
	// to the OS image at PA 0xC8000000; we mirror that here so the OS
	// boot resumes with VA == PA and the kernel's reset handler can
	// build its own page tables from scratch.
	void     setCp15Control(uint32_t v) { cp15_control = v; }
	void     setCp15Ttb(uint32_t v)     { cp15_translationTableBase = v; }
	// Public TLB-invalidate for emulator-side context-switch surgery
	// (PSION_S7_FORCE_WSERV_DISPATCH).  Real silicon's context-switch
	// code follows DACR write with MCR p15,0,_,c8,c7,0 to flush the
	// TLB; we replicate that here.  No-op on builds without
	// ARM710T_TLB (single-entry mode flushes inline on every miss).
	void invalidateTlb() {
#ifdef ARM710T_TLB
		flushTlb();
#endif
	}
	bool canAcceptFIQ() const { return !(CPSR & CPSR_FIQDisable); }
	bool canAcceptIRQ() const { return !(CPSR & CPSR_IRQDisable); }
	// IRQ / FIQ are sampled at instruction boundaries — see tick().
	// requestIRQ() / requestFIQ() now set a pending flag instead of
	// raising the exception synchronously, otherwise a peripheral
	// write that triggers an IRQ from inside execSingleDataTransfer
	// (writeIntc → recomputeInterrupts → cpu.requestIRQ) would
	// mode-switch mid-instruction, landing the post-indexed writeback
	// into the IRQ-bank register set.
	void requestFIQ();
	void requestIRQ();
	void reset();      // pull nRESET low
	bool pendingIRQ = false;
	bool pendingFIQ = false;
	// Sampled at instruction boundary from tick().
	void sampleAndDispatchPendingExceptions();

	// SA-1100 Wait-For-Interrupt state. Set by execCP15RegisterTransfer
	// when the guest issues `MCR p15,0,_,c15,c8,2` — the owning device
	// (see SA1100::Emulator::executeUntil) reads-and-clears this flag
	// to skip passedCycles forward until an IRQ is about to deliver.
	// Plain ARM710 parts never set the flag.
	bool wfiRequested = false;

	bool instructionReady() const { return (prefetchCount == 2); }
	uint32_t tick();   // run the chip for at least 1 clock cycle

	// ── Page-anchored fast loop (docs/execution-engine-scope.md Tier 2) ──
	// Runs up to maxInsns straight-line, fast-dispatchable instructions from one
	// resolved code page WITHOUT the per-instruction tick()/fetchVirtual call
	// framework — replicating tick()'s exact prefetch/PC/IRQ/fault bookkeeping
	// with the fetch inlined. Returns total cycles consumed. Falls back to a
	// single tick() when it can't engage (pipeline not full, page not resolvable,
	// the next instruction is DK_SLOW/faulted, a branch flushed the pipeline, a
	// page/subpage boundary, or self-modifying code invalidated the page). pageLoop_
	// gates it; resolveFetchPage() provides the device's host pointer + decoded-kind
	// array (base returns false ⇒ no-op on non-SA1100 devices).
	struct FetchPage {
		const uint8_t *hostBase;     // *(hostBase+va) == the instruction word
		uint8_t *kinds;              // decoded-kind array for the 4 KB phys page
		uint32_t endVa;              // exclusive: loop must re-resolve at/after this VA
		uint32_t physBase;           // expected page phys base (re-validation token)
		const uint32_t *validPtr;    // &DecodedPage::physBase; != physBase ⇒ invalidated
	};
	virtual bool resolveFetchPage(uint32_t va, FetchPage &out) { (void)va; (void)out; return false; }
	uint32_t tickPageLoop(int maxInsns);
	bool pageLoop_ = false;

	MaybeU32 readVirtualDebug(uint32_t virtAddr, ValueSize valueSize);
	MaybeU32 virtToPhys(uint32_t virtAddr);

    virtual std::pair<MaybeU32, MMUFault> readVirtual(uint32_t virtAddr, ValueSize valueSize);
    // Instruction fetch (always a 32-bit read). Split from readVirtual so a
    // subclass can keep a fetch-specific TLB MRU entry: fetches and data
    // accesses target different pages, so a single shared MRU ping-pongs and
    // misses on every alternation. Default routes to readVirtual.
    virtual std::pair<MaybeU32, MMUFault> fetchVirtual(uint32_t virtAddr) {
        return readVirtual(virtAddr, V32);
    }
    // Classify a raw ARM word into an ArmDK (mirrors executeInstruction()'s
    // dispatch chain). Pure function of the word + isTVersion; the SA-1100
    // bridge caches the result per ROM address.
    uint8_t decodeKind(uint32_t insn) const;
	void initFastPathGate();
	virtual MaybeU32 readPhysical(uint32_t physAddr, ValueSize valueSize) = 0;
	virtual MMUFault writeVirtual(uint32_t value, uint32_t virtAddr, ARM710::ValueSize valueSize);
	virtual bool writePhysical(uint32_t value, uint32_t physAddr, ARM710::ValueSize valueSize) = 0;

	uint32_t getGPR(int index) const { return GPRs[index]; }
	void setGPR(int index, uint32_t value) { GPRs[index] = value; }
	uint32_t getCPSR() const { return CPSR; }
	// Linear-memory byte offsets of the live register file. In the WASM build a
	// host pointer *is* the offset into the module's linear memory, so these are
	// exactly the addresses a runtime-generated JIT block must target to read and
	// write the real GPRs[]/CPSR in place (see docs/jit-engine-scope.md, W2).
	uintptr_t gprFileAddr() const { return reinterpret_cast<uintptr_t>(&GPRs[0]); }
	uintptr_t cpsrAddr()    const { return reinterpret_cast<uintptr_t>(&CPSR); }
	// SPSR / banked-LR accessors used by trace probes that need to see
	// the previous-mode CPU state at exception/SWI entry.  Forward to
	// the *Impl variants defined further down in the class body so
	// they can reference BankIndex.  Bank values per ARM710::BankIndex:
	//   FiqBank=0, IrqBank=1, SvcBank=2, AbtBank=3, UndBank=4, MainBank=5.
	uint32_t getSPSR() const { return getSPSRImpl(); }
	uint32_t getBankedLR(unsigned bnk) const { return getBankedLRImpl(bnk); }
	uint32_t getBankedSP(unsigned bnk) const { return getBankedSPImpl(bnk); }
	uint32_t getBankedSPSR(unsigned bnk) const {
		if (bnk >= 6) return 0;
		return SPSRs[bnk];
	}
	// Banked register write accessors. These ONLY update the
	// allModesBankedRegisters[] / SPSRs[] save slots — they do NOT touch
	// the live GPRs[13]/GPRs[14] even if `bnk` is the current bank.
	// Callers that need the change to be live for the current mode must
	// either (a) target the current bank (handled below) or (b) call
	// setCPSR()/switchMode() to rebank afterwards.
	//
	// Used by the Series-7 PSION_S7_FORCE_WSERV_DISPATCH experiment
	// (sa1100.cpp) to install a saved-thread context as if it had just
	// been picked by the scheduler. No-op for non-S7 builds at runtime
	// (everything is env-gated).
	void setBankedLR(unsigned bnk, uint32_t value) {
		if (bnk >= 6) return;
		if ((BankIndex)bnk == currentBank()) GPRs[14] = value;
		else allModesBankedRegisters[bnk][1] = value;
	}
	void setBankedSP(unsigned bnk, uint32_t value) {
		if (bnk >= 6) return;
		if ((BankIndex)bnk == currentBank()) GPRs[13] = value;
		else allModesBankedRegisters[bnk][0] = value;
	}
	void setBankedSPSR(unsigned bnk, uint32_t value) {
		if (bnk >= 6) return;
		SPSRs[bnk] = value;
	}
	// Bank index for a CPSR mode value. Exposed so callers (sa1100.cpp
	// context-switch surgery) can compute the bank from a SPSR-style
	// CPSR value read out of kernel memory.
	unsigned bankForMode(uint32_t cpsrModeValue) const {
		return (unsigned)modeToBank[cpsrModeValue & 0xF];
	}
	uint32_t getRealPC() const {
		return GPRs[15] - (4 * prefetchCount);
	}
	// Redirect execution to `pc`. Use for emulator-side "skip past this
	// kernel function" workarounds where we synthesise a return without
	// running the function body.
	void setRealPC(uint32_t pc) {
		GPRs[15] = pc;
		prefetchCount = 0;
		// L7: zero the prefetch slots too so a downstream tick() that
		// reads them before they're refilled can't see stale instructions
		// from the previous fetch stream. (prefetchCount == 0 means
		// instructionReady() will return false, but a tick() that
		// advances prefetchCount past 0 BEFORE refilling could observe
		// the old contents.)
		prefetch[0] = prefetch[1] = 0;
		prefetchFaults[0] = prefetchFaults[1] = NoFault;
		prefetchKind[0] = prefetchKind[1] = DK_SLOW;
		lastFetchKind = DK_SLOW;
	}

	// Force-set CPSR (including mode bits) and re-bank registers
	// accordingly. Use for emulator-side workarounds where we need to
	// synthesise a mode switch + flag change without running an actual
	// MSR/SUBS-pc-lr instruction (e.g. bypassing the netBook
	// bootloader's wake-from-sleep handler).
	void setCPSR(uint32_t value) {
		switchMode((Mode)(value & 0x1F));
		CPSR = (CPSR & CPSR_ModeMask) | (value & ~CPSR_ModeMask);
	}

	// Synchronously invoke a ROM function from emulator C++ code, blocking
	// until the function returns to a sentinel PC. Used to work around the
	// EKA1 TDfc::Add() thread-wake bug: when we detect the CF state machine
	// is stuck waiting for a DFC dispatch that will never happen, we directly
	// invoke the ROM-side DFC runner (e.g. FUN_5000f124) to advance the state
	// machine by one step. Runs in the CURRENT CPU mode with existing SP/bank
	// state — this matches how the kernel would call it from IRQ context,
	// using whatever SP is valid. Returns the value left in r0.
	//
	// LIMITATIONS: no interrupt dispatch happens during the synchronous call
	// (we call tick() directly, not executeUntil). Cycles consumed by the
	// function are not added to passedCycles. Use sparingly.
	uint32_t callRomFunctionSync(uint32_t targetPC, uint32_t arg0,
	                             int maxCycles = 200000,
	                             int64_t *passedCyclesPtr = nullptr);
	// Variant accepting up to 4 args (r0..r3).  Used for calls that take
	// multiple register arguments like Kern::RequestComplete(NThread*,
	// TRequestStatus*, errcode).
	uint32_t callRomFunctionSyncN(uint32_t targetPC, uint32_t arg0,
	                               uint32_t arg1, uint32_t arg2, uint32_t arg3,
	                               int maxCycles = 200000);

	void setLogger(std::function<void(const char *)> newLogger) { logger = newLogger; }
	// Gates the expensive vsnprintf + logger callback in log() below. Off by
	// default so a release build without "Show Logs" open pays nothing for
	// the thousands of log() call sites the core contains.
	void setLoggingEnabled(bool enabled) { loggingEnabled = enabled; }
	// Gated deep-diagnostic mode. When enabled, the core logs every SWP,
	// every MSR/MRS that touches the mode bits, every raised/returned
	// exception, and every store to the EKA1 kernel-data range
	// (virtual 0x80000000-0x80006FFF). Intended to be flipped on for a
	// short window around a specific IRQ dispatch (e.g. the first CF EINT3)
	// then off again; leaving it on flood-logs the bootstrap path.
	void setCfDiagEnabled(bool e) { cfDiagEnabled = e; }
	bool getCfDiagEnabled() const { return cfDiagEnabled; }
	// Series 5 HAL-root stack-overlap workaround (see writeVirtual).
	// Off by default; enabled by Series5::Emulator's constructor so it
	// doesn't affect any other device.
	void setSeries5HalFix(bool e) { series5HalFix = e; }
	bool getSeries5HalFix() const { return series5HalFix; }
	// netBook bootloader: force a known-good SP_abt / SP_und on every
	// raiseException() entry into Abort32 / Undefined32.  The BL never
	// initialises these banked SPs (the netBook OS and Series 7 ROM both
	// do this during early-mode init — that's why the same hardware runs
	// them cleanly).  Off by default; enabled by SA1100::Emulator when the
	// netBook bootloader ROM is loaded.
	void setNetBookBlBankedSpFix(bool e) { netBookBlBankedSpFix_ = e; }
	// Runtime-toggleable abort/exception trace for the booted netBook OS:
	// logs every data/prefetch/undef exception with FAR + faulting PC.  Used
	// to localize the drive-D-open crash by enabling it just before the access.
	void setNbExcTrace(bool e) { nbExcTrace_ = e; }
	bool getNbExcTrace() const { return nbExcTrace_; }
	// EXPERIMENT (Series 5 corruption): allow runtime toggle of ARM710T
	// decode features. PSION_S5_T_VERSION=1 promotes the core to ARM710T
	// so halfword/long-multiply instructions execute natively instead of
	// silently mis-decoding as data-processing. Default off.
	void setTVersionEnabled(bool e) {
		isTVersion = e;
		cp15_id = e ? 0x41807100u : 0x41047100u;
	}
	// L3: index back one step into the ring buffer without relying on
	// unsigned-int underflow wrap (which works in practice but is fragile).
	uint32_t lastPcExecuted() const {
		uint32_t idx = (pcHistoryIndex == 0) ? (PcHistoryCount - 1) : (pcHistoryIndex - 1);
		return pcHistory[idx].addr;
	}
public:
	// PC-history ring accessor: back=0 is the most-recently-recorded PC,
	// back=1 the one before it, etc. (up to PcHistoryCount-1).
	uint32_t pcHistoryAddr(int back) const {
		int idx = (int)pcHistoryIndex - 1 - back;
		idx %= (int)PcHistoryCount;
		if (idx < 0) idx += (int)PcHistoryCount;
		return pcHistory[idx].addr;
	}
	static constexpr int pcHistoryDepth() { return PcHistoryCount; }
	void log(const char *format, ...);
	void logPcHistory();
private:
	std::function<void(const char *)> logger;
	bool loggingEnabled = false;
	bool cfDiagEnabled = false;
	bool series5HalFix = false;
	bool netBookBlBankedSpFix_ = false;
	bool nbExcTrace_ = false;
	// Approximate cycle counter, incremented by every executeInstruction()
	// return value. Used by the optional PSION_INSN_TRACE_CYC trace
	// (see executeInstruction). Not exposed publicly because it's a
	// loose approximation — IRQ entry, prefetch faults, etc., advance
	// the owning emulator's passedCycles by routes that don't go
	// through executeInstruction.
	uint64_t insnCycleApprox = 0;
	// Series 5: TRequestStatus address that was overlay-written to 0 by the
	// HAL::Get short-circuit. The kernel SWI dispatcher's scratch write at
	// 0x5001950C (`STR R12, [SP]`) tends to clobber this same slot. We
	// re-write 0 to it on every executed instruction at PC=0x5001950C as
	// long as this is non-zero. Set by HAL::Get short-circuit; cleared on
	// reset.
	uint32_t series5HalStatusAddr = 0;
	// Series 5: armed by the HAL::Get short-circuit so the immediately
	// following SWI 0xc00077 (WaitForAnyRequest, in the back-to-back thunk
	// pair at 0x5004BD44/0x5004BD48) is also short-circuited.
	bool series5HalShortCircuitWait = false;

	// Series 5: MMU shadow-page aliasing for the bootstrap NThread block.
	// The kernel allocates the bootstrap NThread at virtual 0x80003CD0 on
	// the SVC stack inside FUN_50010E44 (auStack_218[512]). Its 0x128-byte
	// extent (0x80003CD0..0x80003DF8) is later overwritten by SVC-stack
	// pushes (the SVC stack grows DOWN through the same region). To survive
	// the clobber while still letting function-frame STMDB/LDM pairs work
	// correctly, we mirror writes to a private shadow buffer and serve
	// reads of bootstrap NThread fields from the shadow. Stack-push writes
	// go to real RAM (so matching pop sees what it stored); kernel reads of
	// NThread fields come from the (live, non-clobbered) shadow.
	enum { Series5BootstrapNThreadSize = 0x128 };
	uint32_t series5ShadowNThread[Series5BootstrapNThreadSize / 4] = {0};
	bool     series5ShadowActive = false;
	enum {
		Series5BootstrapNThreadVirt = 0x80003CD0,
		Series5BootstrapNThreadEnd  = 0x80003CD0 + 0x128, // 0x80003DF8
		// Mirror window covers the explicit field-init body after the
		// iCurrentThread store at 0x50010EA0, up to (but not including)
		// the function epilogue. Excludes the function prologue (which
		// pushes registers onto the stack within the same virtual range)
		// and excludes nested calls into FUN_5001ae48 / memset helpers
		// (whose writes were already captured by the snapshot).
		Series5FunBootstrapNThreadInitStart = 0x50010EA0,
		Series5FunBootstrapNThreadInitEnd   = 0x50010F00,
	};

	enum { PcHistoryCount = 64 };
	struct { uint32_t addr, insn; } pcHistory[PcHistoryCount];
	uint32_t pcHistoryIndex = 0;

	enum Mode : uint8_t {
		User32       = 0x10,
		FIQ32        = 0x11,
		IRQ32        = 0x12,
		Supervisor32 = 0x13,
		Abort32      = 0x17,
		Undefined32  = 0x1B,
		System32     = 0x1F   // ARMv4+ privileged user mode
	};

	enum BankIndex : uint8_t {
		FiqBank,
		IrqBank,
		SvcBank,
		AbtBank,
		UndBank,
		MainBank
	};

	constexpr static const BankIndex modeToBank[16] = {
		MainBank, FiqBank,  IrqBank,  SvcBank,
		MainBank, MainBank, MainBank, AbtBank,
		MainBank, MainBank, MainBank, UndBank,
		MainBank, MainBank, MainBank, MainBank
	};

	enum : uint32_t {
		CPSR_ModeMask   = 0x0000001F,
		CPSR_FIQDisable = 0x00000040,
		CPSR_IRQDisable = 0x00000080,
		CPSR_V          = 0x10000000,
		CPSR_C          = 0x20000000,
		CPSR_Z          = 0x40000000,
		CPSR_N          = 0x80000000,
		CPSR_FlagMask   = 0xF0000000
	};

	// active state
	BankIndex bank;
	uint32_t CPSR;
	uint32_t GPRs[16];

	// saved state
	uint32_t fiqBankedRegisters[2][5];      // R8..R12 inclusive
	uint32_t allModesBankedRegisters[6][2]; // R13, R14
	// 6 entries: one per BankIndex (FiqBank..MainBank). User/System mode
	// both map to MainBank; per ARM spec, MSR/MRS SPSR in those modes is
	// UNPREDICTABLE, but we need a real slot so the access doesn't run
	// past the end of the array into cp15_id. (H3 fix.)
	uint32_t SPSRs[6];

protected:
	// coprocessor 15 — protected so SA1100Bridge can read S/R/DACR bits
	uint32_t cp15_id;                   // 0: read-only
	uint32_t cp15_control;              // 1: write-only
	uint32_t cp15_translationTableBase; // 2: write-only
	uint32_t cp15_domainAccessControl;  // 3: write-only
	uint8_t  cp15_faultStatus;          // 5: read-only (writing has unrelated effects)
	uint32_t cp15_faultAddress;         // 6: read-only (writing has unrelated effects)

	// First-fault capture: the prefetch-abort loop (vector page unmapped)
	// floods the log and pushes the *initial* fault — the real crash — off
	// the top of any bounded log viewer.  raiseException() records the first
	// Abort/Undef here so tick()'s stuck-loop detector can re-emit it as the
	// final line, where FSR + DACR survive to identify the cause.
	bool     firstFaultCaptured_ = false;
	uint32_t firstFaultPC_ = 0, firstFaultFAR_ = 0, firstFaultFSR_ = 0, firstFaultDACR_ = 0;

	// Set once the prefetch-abort loop is confirmed unrecoverable (vector page
	// unmapped — the CPU can never make forward progress).  The worker polls
	// this to auto-halt the dead device so it stops flooding logs and "Power
	// Off" isn't fighting a runaway loop.  Cleared on reset/reload.
	bool faultLoopDetected_ = false;
public:
	bool faultLoopDetected() const { return faultLoopDetected_; }
private:

	bool isTVersion;

	bool flagV() const { return CPSR & CPSR_V; }
	bool flagC() const { return CPSR & CPSR_C; }
	bool flagZ() const { return CPSR & CPSR_Z; }
	bool flagN() const { return CPSR & CPSR_N; }
	bool checkCondition(int cond) const {
		switch (cond) {
		/*EQ*/ case 0:   return flagZ();
		/*NE*/ case 1:   return !flagZ();
		/*CS*/ case 2:   return flagC();
		/*CC*/ case 3:   return !flagC();
		/*MI*/ case 4:   return flagN();
		/*PL*/ case 5:   return !flagN();
		/*VS*/ case 6:   return flagV();
		/*VC*/ case 7:   return !flagV();
		/*HI*/ case 8:   return flagC() && !flagZ();
		/*LS*/ case 9:   return !flagC() || flagZ();
		/*GE*/ case 0xA: return flagN() == flagV();
		/*LT*/ case 0xB: return flagN() != flagV();
		/*GT*/ case 0xC: return !flagZ() && (flagN() == flagV());
		/*LE*/ case 0xD: return flagZ() || (flagN() != flagV());
		/*AL*/ case 0xE: return true;
		/*NV*/ /*case 0xF:*/
		default:  return false;
		}
	}

	static Mode modeFromCPSR(uint32_t v) { return (Mode)(v & CPSR_ModeMask); }
	Mode currentMode()             const { return modeFromCPSR(CPSR); }
	BankIndex currentBank()        const { return modeToBank[(Mode)(CPSR & 0xF)]; }

	// Inline impls of the trace-probe accessors declared above.
	// Returns 0 for current bank == MainBank (USR/SYS mode, no real SPSR).
	uint32_t getSPSRImpl() const { return SPSRs[currentBank()]; }
	uint32_t getBankedLRImpl(unsigned bnk) const {
		if (bnk >= 6) return 0;
		if ((BankIndex)bnk == currentBank()) return GPRs[14];
		return allModesBankedRegisters[bnk][1];
	}
	uint32_t getBankedSPImpl(unsigned bnk) const {
		if (bnk >= 6) return 0;
		if ((BankIndex)bnk == currentBank()) return GPRs[13];
		return allModesBankedRegisters[bnk][0];
	}
protected:
	bool isPrivileged()            const { return (CPSR & 0x1F) > User32; }
	bool isMMUEnabled()            const { return (cp15_control & 1); }
	bool isAlignmentFaultEnabled() const { return (cp15_control & 2); }
	bool isCacheEnabled()          const { return (cp15_control & 4); }
	bool isWriteBufferEnabled()    const { return (cp15_control & 8); }

	void switchMode(Mode mode);
	void switchBank(BankIndex bank);
	void raiseException(Mode mode, uint32_t savedPC, uint32_t newPC);

protected:
	// MMU/TLB — protected so SA1100Bridge can access TlbEntry and helpers
	enum MMUFaultSorP : uint64_t {
		SorPLinefetchError      = 4,
		SorPTranslationFault    = 5,
		SorPOtherBusError       = 8,
		SorPDomainFault         = 9,
		SorPPermissionFault     = 0xD,
	};

	MMUFault encodeFault(MMUFault fault, int domain, uint32_t virtAddr) const {
		return (MMUFault)(fault | (domain << 4) | ((uint64_t)virtAddr << 32));
	}
	MMUFault encodeFaultSorP(MMUFaultSorP baseFault, bool isPage, int domain, uint32_t virtAddr) const {
		return (MMUFault)(baseFault | (isPage ? 2 : 0) | (domain << 4) | ((uint64_t)virtAddr << 32));
	}

	struct TlbEntry { uint32_t addrMask, addr, lv1Entry, lv2Entry; };
#ifdef ARM710T_TLB
	enum { TlbSize = 64 };
	TlbEntry tlb[TlbSize];
	int nextTlbIndex = 0;

	// Direct-mapped front cache for translateAddressUsingTlb(): maps
	// (VA >> 12) to the tlb[] slot that last translated that page so the
	// hot path skips the 64-entry linear scan (the single largest cost of
	// every emulated memory access). Each cached pointer is re-validated
	// against the slot's current addrMask/addr before use, so a slot that
	// has since been evicted/reused or zeroed by flushTlb() simply falls
	// back to the scan — a fast-map hit can only ever return an entry the
	// scan itself would have returned. Pointers always reference slots
	// inside tlb[], never caller-provided temporaries.
	enum { TlbFastMapSize = 256 };
	TlbEntry *tlbFastMap[TlbFastMapSize] = {};
	void noteTlbFastMap(uint32_t virtAddr, TlbEntry *e) {
		tlbFastMap[(virtAddr >> 12) & (TlbFastMapSize - 1)] = e;
	}

	virtual void flushTlb();
	virtual void flushTlb(uint32_t virtAddr);
	// Called when a CP15 write changes a register that feeds permission
	// decisions (control S/R bits, domain access control). Lets a subclass
	// that caches permission results (SA1100Bridge) drop them. Default no-op.
	virtual void onMmuPermConfigChanged() {}
	// CP15 c7 cache-flush — the architectural code-coherency sync point: a guest
	// MUST flush the I-cache after writing code before executing it. A subclass
	// that caches decoded instructions (SA1100Bridge) overrides this to drop the
	// stale ops, which covers ALL code-load paths (CPU stores AND DMA) at the one
	// point the guest is required to use. Default no-op.
	virtual void onICacheFlush() {}
#else
	TlbEntry singleTlbEntry;
	void noteTlbFastMap(uint32_t, TlbEntry *) {}
#endif

	TlbEntry *_allocateTlbEntry(uint32_t addrMask, uint32_t addr);
    std::variant<TlbEntry *, MMUFault> translateAddressUsingTlb(uint32_t virtAddr, TlbEntry *useMe=nullptr);
	static uint32_t physAddrFromTlbEntry(TlbEntry *tlbEntry, uint32_t virtAddr);
	MMUFault checkAccessPermissions(TlbEntry *entry, uint32_t virtAddr, bool isWrite) const;

	// Series 5 lazy stack-grow page-table install. See block comment above
	// readVirtual() in arm710.cpp. Returns true if a fresh L1+L2 mapping was
	// successfully written into the kernel's page tables for virtAddr.
	bool series5InstallLazyMapping(uint32_t virtAddr);

	bool faultTriggeredThisCycle = false;
	void reportFault(MMUFault fault);

	// Instruction/Data Cache
#ifdef ARM710T_CACHE
	enum {
		CacheSets = 4,
		CacheBlocksPerSet = 128,
		CacheBlockSize = 0x10,

		CacheAddressLineMask = 0x0000000F,
		CacheAddressSetMask  = 0x00000030, CacheAddressSetShift = 4,
		CacheAddressTagMask  = 0xFFFFFFC0,

		CacheBlockEnabled = 1
	};
	uint32_t cacheBlockTags[CacheSets][CacheBlocksPerSet];
	uint8_t cacheBlocks[CacheSets][CacheBlocksPerSet][CacheBlockSize];

	void clearCache();
	uint8_t *findCacheLine(uint32_t virtAddr);
	pair<MaybeU32, MMUFault> addCacheLineAndRead(uint32_t physAddr, uint32_t virtAddr, ValueSize valueSize, int domain, bool isPage);
	MaybeU32 readCached(uint32_t virtAddr, ValueSize valueSize);
	bool writeCached(uint32_t value, uint32_t virtAddr, ValueSize valueSize);
#endif

	// Instruction Loop
	int prefetchCount;
	uint32_t prefetch[2];
	MMUFault prefetchFaults[2];
	// Decoded-op cache plumbing (see ArmDK / docs/execution-engine-scope.md).
	// prefetchKind shadows prefetch[]: the decode kind for each prefetched
	// word, carried through the pipeline so tick() can fast-dispatch. fetchVirtual
	// writes lastFetchKind for the word it just fetched. decodeFast_ gates the
	// fast path; it is OPT-IN (default off, PSION_DECODE_FASTPATH=1 enables) while
	// the Pac-Man KERN-EXEC 3 regression is investigated — see initFastPathGate().
	uint8_t prefetchKind[2] = { DK_SLOW, DK_SLOW };
	uint8_t lastFetchKind = DK_SLOW;
	bool    decodeFast_ = false;
	// Phys-keyed decoded-instruction cache (docs/execution-engine-scope.md Tier 1).
	// decodeCache_ gates it; decodeCacheCheck_ additionally runs the cached kind
	// alongside a fresh decodeKind() and logs on divergence — proving invalidation
	// coherency, the proven failure mode (the Pac-Man KERN-EXEC 3). Both opt-in via
	// PSION_DECODE_CACHE[_CHECK]=1; any other PSION_* var forces them off so
	// per-instruction diagnostic traces still run (same rule as decodeFast_).
	bool    decodeCache_ = false;
	bool    decodeCacheCheck_ = false;

	uint32_t executeInstruction(uint32_t insn);

	uint32_t execDataProcessing(bool I, uint32_t Opcode, bool S, uint32_t Rn, uint32_t Rd, uint32_t Operand2);
	uint32_t execMultiply(uint32_t AS, uint32_t Rd, uint32_t Rn, uint32_t Rs, uint32_t Rm);
	uint32_t execMultiplyLong(uint32_t UAS, uint32_t RdHi, uint32_t RdLo, uint32_t Rs, uint32_t Rm);
	uint32_t execSingleDataSwap(bool B, uint32_t Rn, uint32_t Rd, uint32_t Rm);
	uint32_t execSingleDataTransfer(uint32_t IPUBWL, uint32_t Rn, uint32_t Rd, uint32_t offset);
	uint32_t execHalfwordDataTransfer(uint32_t insn);
	uint32_t execBlockDataTransfer(uint32_t PUSWL, uint32_t Rn, uint32_t registerList);
	uint32_t execBranch(bool L, uint32_t offset);
	uint32_t execCP15RegisterTransfer(uint32_t CPOpc, bool L, uint32_t CRn, uint32_t Rd, uint32_t CP, uint32_t CRm);
};
