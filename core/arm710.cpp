// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2019 Ash Wolf (WindEmu); modifications (c) 2024-2026 Joe Haines.
// Subject to the Mozilla Public License v2.0 (http://mozilla.org/MPL/2.0/).

#include "arm710.h"
#include "common.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// this will need changing if this code ever compiles on big-endian procs
inline uint32_t read32LE(uint8_t *p) {
	return *((uint32_t *)p);
}
inline void write32LE(uint8_t *p, uint32_t v) {
	*((uint32_t *)p) = v;
}

// L1: cache the result of `std::getenv(name)` in a function-local static so
// only the first call hits libc. Used by the various PSION_* debug-trace
// switches that are otherwise re-resolved on every tick() iteration. Each
// call site gets its own cached pointer (so a single header pass-through
// doesn't pessimise compile-time inlining), but they all share the same
// libc-resolved value.
#define PSION_ENV_BOOL(name)  ([] { static const bool _v = std::getenv(name) != nullptr; return _v; }())
#define PSION_ENV_CSTR(name)  ([] { static const char *const _v = std::getenv(name); return _v; }())

extern "C" { extern char **environ; }

// Defined further down; forward-declared so the decoded-op fast path in tick()
// (above their definitions) can extract operands the same way the dispatch does.
static inline uint32_t extract(uint32_t value, uint32_t hiBit, uint32_t loBit);
static inline bool extract1(uint32_t value, uint32_t bit);

// Decode (decoded-op) fast path gate.
//
// decodeFast_ (tick()'s fast dispatch) stays OPT-IN. It was disabled while
// chasing the Pac-Man KERN-EXEC 3 regression; the wrong-dispatch bug fixed
// alongside this (a carried decode kind getting out of step with its
// instruction word) is a strong candidate for that regression's cause, and the
// fast path is now differentially proven across the boot suite — but there is
// no Pac-Man fixture in the tree to confirm it against, so the default is left
// alone. It is also worth little now that the page loop is on: measured
// together, +0.4 MHz on the Series 7.
//
// pageLoop_ IS on by default — see the note at its assignment below.

// The "any other PSION_ var forces the fast paths off" rule exists so that the
// per-instruction diagnostic traces in tick()'s slow body still run when
// someone is debugging — an engine that skips that body would silently
// swallow the trace they just asked for.
//
// But a handful of PSION_ vars are not per-instruction diagnostics at all, and
// applying the rule to them makes the engines untestable: the differential
// verification harness has to run WITH an engine enabled (or it is not
// verifying anything), and it needs the RTC pinned (or two runs of the same
// workload diverge on the first RTC read and nothing is comparable). Those are
// listed here explicitly rather than pattern-matched, so adding one is a
// deliberate act.
//
// Prefixes are matched without a trailing '=' so a var and its "_SUFFIX"
// siblings are both covered — and so this cannot repeat the off-by-one that
// kept PSION_DECODE_PAGELOOP permanently disabled (matching "NAME=" requires
// the length to include the '='; matching "NAME" does not).
static bool isEngineNeutralVar(const char *ep) {
	static const char *const kNeutral[] = {
		"PSION_STATE_TRACE",   // differential state-hash harness (+ _INTERVAL)
		"PSION_RTC_SEED",      // pins the RTC so two runs are comparable
		"PSION_JIT",           // the code generator and all its knobs
		"PSION_BATCH_TICKS",   // the burst/batch length knobs: both are
		"PSION_BURST_TICKS",   // properties of the engine under test
		"PSION_NB_HOOK_MASK",  // which assist hooks run at all
		"PSION_NO_BLOCK_SPAN", // whether LDM/STM resolves its span once
		"PSION_NB_CF_PROGRESS",// read-only progress log
		                       // — the thing under test must not switch off the
		                       // engine it sits on
	};
	for (const char *n : kNeutral)
		if (std::strncmp(ep, n, std::strlen(n)) == 0) return true;
	return false;
}

void ARM710::initFastPathGate() {
	decodeFast_ = false;
	decodeCache_ = false;
	decodeCacheCheck_ = false;
	pageLoop_ = false;
	if (!environ) return;
	const char *optIn = nullptr;
	const char *cacheOptIn = nullptr;
	const char *cacheCheckOptIn = nullptr;
	const char *pageLoopOptIn = nullptr;
	bool anyOtherPsionVar = false;
	for (char **ep = environ; *ep; ++ep) {
		if (std::strncmp(*ep, "PSION_DECODE_FASTPATH=", 22) == 0) optIn = *ep + 22;
		else if (std::strncmp(*ep, "PSION_DECODE_CACHE_CHECK=", 25) == 0) cacheCheckOptIn = *ep + 25;
		else if (std::strncmp(*ep, "PSION_DECODE_CACHE=", 19) == 0) cacheOptIn = *ep + 19;
		else if (std::strncmp(*ep, "PSION_DECODE_PAGELOOP=", 22) == 0) pageLoopOptIn = *ep + 22;
		else if (isEngineNeutralVar(*ep)) { /* see isEngineNeutralVar */ }
		else if (std::strncmp(*ep, "PSION_", 6) == 0) anyOtherPsionVar = true;
	}
	// The CHECK/PAGELOOP vars are themselves PSION_ vars; they must NOT count as
	// "any other PSION var" (they're the cache's own knobs), so they're matched
	// before the generic PSION_ branch above.
	decodeFast_       = (optIn && optIn[0] == '1') && !anyOtherPsionVar;
	decodeCacheCheck_ = (cacheCheckOptIn && cacheCheckOptIn[0] == '1') && !anyOtherPsionVar;
	// Page-anchored burst execution: DEFAULT ON, PSION_DECODE_PAGELOOP=0 is the
	// kill switch. Worth +47% on the Series 7 and +54% on the netBook, and
	// validated bit-exact against the interpreter rather than by inspection:
	// the state-hash harness (core/state_trace.h) reports identical
	// architectural state at every sample point over 551 M instructions across
	// the Series 7, the netBook's stock OS and its Quartz image, and all 25
	// devices in the boot suite render byte-identical screenshots either way.
	// Still subject to the any-other-PSION_-var rule below, so a diagnostic run
	// drops back to the interpreter and its per-instruction trace gates.
	pageLoop_         = (!pageLoopOptIn || pageLoopOptIn[0] != '0') && !anyOtherPsionVar;
	// The code generator sits on top of the burst engine — it is entered from
	// tickPageLoop and hands back to it — so it only exists when that does.
	if (pageLoop_) armjit::jitEnsure(jit_);
	// The page loop needs the phys-keyed code-page table enabled: it no longer
	// reads a cached kind from it (nothing does), but it holds a pointer into a
	// slot as the "this page has not been written under you" validity token.
	decodeCache_      = ((cacheOptIn && cacheOptIn[0] == '1') || decodeCacheCheck_ || pageLoop_) && !anyOtherPsionVar;
}

// Differential state tracing (see state_trace.h). Opt-in via
// PSION_STATE_TRACE=<path>; PSION_STATE_TRACE_INTERVAL=N sets the sampling
// interval. Deliberately NOT subject to the initFastPathGate() "any other
// PSION_ var forces the fast paths off" rule — the whole point of this trace
// is to run WITH an execution engine enabled and compare it against the
// reference, so it must not itself disable the thing under test — see
// isEngineNeutralVar().
void ARM710::initStateTrace() {
	const char *path = std::getenv("PSION_STATE_TRACE");
	if (!path) return;
	const char *iv = std::getenv("PSION_STATE_TRACE_INTERVAL");
	stateTrace_.open(path, iv ? std::strtoull(iv, nullptr, 0) : 0);
	if (const char *f = std::getenv("PSION_STATE_TRACE_FROM"))
		stateTrace_.dumpFrom = std::strtoull(f, nullptr, 0);
	if (const char *t = std::getenv("PSION_STATE_TRACE_TO"))
		stateTrace_.dumpTo = std::strtoull(t, nullptr, 0);
}


// F-MULTIAGENT fix-candidate(2): SWI 0xc00084 entry/return trace state,
// shared between the SWI-decode site (entry log + pending-return record)
// and tick() prologue (return matcher).  Default-zero; only touched when
// PSION_S7_TRACE_WAITFOR=1 is set.  Cap entry-count to bound log size.
struct PsionWaitForTrace {
	static constexpr int kCap = 50;
	static constexpr int kPending = 8;
	int entryCount = 0;
	int returnCount = 0;
	// After each SWI 0xc00084 entry we arm "followUserPCs" so the tick()
	// matcher logs the next ~10 user-mode PCs we land on, so we can see
	// where the kernel actually returns control (vs the theoretical
	// swi_pc + 4 sequential return).  Decremented each match.
	int followUserPCs = 0;
	// Pending return addresses to match against PC after kernel returns
	// to User mode.  Small ring buffer; matches are O(kPending).
	uint32_t pendRet[kPending] = {0};
	uint64_t pendCyc[kPending] = {0};
	int pendHead = 0;
};
static PsionWaitForTrace g_psionWaitForTrace;


void ARM710::switchBank(BankIndex newBank) {
	if (newBank != bank) {
		// R13 and R14 need saving/loading for all banks
		allModesBankedRegisters[bank][0] = GPRs[13];
		allModesBankedRegisters[bank][1] = GPRs[14];
		GPRs[13] = allModesBankedRegisters[newBank][0];
		GPRs[14] = allModesBankedRegisters[newBank][1];

		// R8 to R12 are only banked in FIQ mode
		auto oldBankR8to12 = (bank == FiqBank) ? 1 : 0;
		auto newBankR8to12 = (newBank == FiqBank) ? 1 : 0;
		if (oldBankR8to12 != newBankR8to12) {
			// swap these sets around
			for (int i = 0; i < 5; i++)
				fiqBankedRegisters[oldBankR8to12][i] = GPRs[8 + i];
			for (int i = 0; i < 5; i++)
				GPRs[8 + i] = fiqBankedRegisters[newBankR8to12][i];
		}

		bank = newBank;
	}
}


void ARM710::switchMode(Mode newMode) {
	// Series 5 EPOC R1 26-bit-mode coercion: the kernel's thread-context
	// initialiser at 0x50014D88 produces CPSR with mode bits 0x08 (bit 4 = 0
	// → ARMv3 26-bit User mode) for OPL/16-bit-EPOC legacy compatibility.
	// Our emulator doesn't implement 26-bit mode; coerce 0x08 → User32 so
	// the thread runs in 32-bit User mode (same MainBank register file,
	// just with a working fetch path). System32 was tried as alternative
	// but produced identical boot progress (81 unique PCs at 15s test).
	if (series5HalFix && (newMode & 0x1F) == 0x08)
		newMode = (Mode)((newMode & ~0x1F) | User32);
	if (PSION_ENV_BOOL("PSION_MODE_TRACE")) {
		static uint64_t n = 0;
		// Only log mode-switches involving abort (0x17) — spammy otherwise.
		// PSION_MODE_TRACE=user adds User32 (0x10) transitions to surface
		// scheduler dispatch (which should always end with a switch to 0x10).
		uint32_t oldMode = CPSR & 0x1F;
		uint32_t nm = newMode & 0x1F;
		const char *e = PSION_ENV_CSTR("PSION_MODE_TRACE");
		bool wantUser = e && (e[0] == 'u' || e[0] == '2');
		bool wantAll  = e && (e[0] == '2');
		bool involvesUser = (oldMode == 0x10 || nm == 0x10);
		bool involvesAbort = (oldMode == 0x17 || nm == 0x17);
		bool show = wantAll || involvesAbort || (wantUser && involvesUser) || n < 80;
		if (show) {
			if (n++ < 4000)
				log("[mode] %02x -> %02x at pc=%08x lr=%08x",
					oldMode, nm, GPRs[15] - 0xC, GPRs[14]);
		}
	}
	auto oldMode = currentMode();
	if (newMode != oldMode) {
//		log("Switching mode! %x", newMode);
		switchBank(modeToBank[newMode & 0xF]);

		CPSR &= ~CPSR_ModeMask;
		CPSR |= newMode;
	}
}

// Desktop-idle diagnostics: count exceptions by mode (USR/FIQ/IRQ/SVC/ABT/UND),
// WFI executions, and instructions, so a harness can derive IRQ/SWI/WFI rates per
// simulated second and answer why the desktop never idles.  Read via main.cpp.
uint64_t g_armExc[16] = {0};
uint64_t g_armWfi = 0;
uint64_t g_armInsn = 0;
// SWI (executive-call) histogram by low byte, with an example full SWI word and
// caller PC per bucket, to identify the syscall loop burning the idle desktop.
uint64_t g_swiHist[256] = {0};
uint32_t g_swiInsnEx[256] = {0};
uint32_t g_swiPcEx[256] = {0};

void ARM710::raiseException(Mode mode, uint32_t savedPC, uint32_t newPC) {
	g_armExc[mode & 0xF]++;          // desktop-idle diagnostics (see main.cpp getters)
	auto bankIndex = modeToBank[mode & 0xF];
//	log("Raising exception mode %x, saving PC %08x, CPSR %08x", mode, savedPC, CPSR);
	if (cfDiagEnabled) {
		log("DIAG exception mode=%02x savedPC=%08x newPC=%08x CPSR-pre=%08x (saved to SPSR bank %d)",
		    mode, savedPC, newPC, CPSR, bankIndex);
	}
	// (Diagnostic PrefetchAbort log removed once the 5mxPro REC root
	// cause was identified — see the idle-thread sentinel-return
	// redirect in tick().)
	// Always-on (hard-capped) first-fault dump.  When EPOC's reschedule
	// self-faults right after writing the new process's DACR (the worker-mode
	// Series 7 recording crash), the FSR + FAR + DACR together say *why*:
	// a domain fault (FSR type 0x9/0xB) with a DACR that doesn't grant the
	// faulting page's domain means the DACR value is garbage (corrupt
	// DProcess / ready-list), whereas a translation fault (0x5/0x7) points
	// at a stale TLB or bad page table.  Routed through log() so it reaches
	// the worker console without any env setup; the 12-entry cap keeps it
	// from joining the prefetch-abort flood.
	if (mode == Abort32 || mode == Undefined32) {
		if (!firstFaultCaptured_) {
			firstFaultCaptured_ = true;
			firstFaultPC_   = savedPC;
			firstFaultFAR_  = cp15_faultAddress;
			firstFaultFSR_  = cp15_faultStatus;
			firstFaultDACR_ = cp15_domainAccessControl;
		}
		static int firstFaults = 0;
		if (firstFaults++ < 12) {
			const char *k = (mode == Undefined32) ? "undef"
			              : (newPC == 0x0Cu)       ? "prefetch-abort" : "data-abort";
			log("[fault] #%d %s faultPC=%08x FAR=%08x FSR=%08x DACR=%08x lr=%08x cpsr=%08x",
			    firstFaults, k, savedPC, cp15_faultAddress, cp15_faultStatus,
			    cp15_domainAccessControl, getGPR(14), CPSR);
		}
	}
	// PSION_EXC_TRACE=1: log CPU faults (data/prefetch abort, undefined) to
	// stderr — forwarded to the page console under worker mode — so an
	// unhandled-exception panic (KERN-EXEC 3) can be traced to the faulting PC /
	// address. Capped so it can't flood. Enable via setEnvVar before loading.
	if ((mode == Abort32 || mode == Undefined32) && PSION_ENV_CSTR("PSION_EXC_TRACE")) {
		static int excN = 0;
		if (excN++ < 400) {
			const char *kind = (mode == Undefined32) ? "undef"
			                 : (newPC == 0x0Cu)       ? "prefetch-abort" : "data-abort";
			std::fprintf(stderr, "[exc] #%d %s faultPC=%08x FAR=%08x lr=%08x cpsr=%08x\n",
			             excN, kind, savedPC, cp15_faultAddress, getGPR(14), CPSR);
		}
	}
	SPSRs[bankIndex] = CPSR;

	// netBook bootloader restart/abort trace (PSION_NB_TRACE_EXC): the
	// post-faithful-read restart re-enters bootloader early-init, which
	// aborts before rebuilding the kernel handler tables.  Log every
	// Abort/Undef exception (and the first few of any mode) with the
	// faulting PC so the restart→fault sequence is visible.
	if (((netBookBlBankedSpFix_ && PSION_ENV_BOOL("PSION_NB_TRACE_EXC")) || nbExcTrace_) &&
	    (mode == Abort32 || mode == Undefined32)) {
		static int nExc = 0;
		if (nExc++ < 80)
			std::fprintf(stderr,
			    "[nb-exc] #%d mode=%02x savedPC(LR)=%08x vec=%08x FAR=%08x CPSR-pre=%08x\n",
			    nExc, mode, savedPC, newPC, cp15_faultAddress, CPSR);
	}

	// netBook IRQ-entry trace (env-gated): log every transition into
	// IRQ32 so we can correlate IRQ delivery with the pulse-fire log.
	if (PSION_ENV_BOOL("PSION_NB_TRACE_IRQ") && mode == IRQ32) {
		// Log every 256th IRQ — captures activity across the full
		// 30 s sim window without overwhelming the log.
		static int total = 0;
		total++;
		if ((total & 0xff) == 0 || total < 16) {
			log("[nb-irq] #%d savedPC=%08x newPC=%08x oldCPSR=%08x",
			    total, savedPC, newPC, CPSR);
		}
	}

	switchMode(mode);

	// netBook BL fixup: the bootloader never initialises SP_abt or SP_und,
	// so any fault entering Abort/Undef mode pushes LR to garbage memory
	// (typically inside an unmapped page in the 0x80200000 L1 section) and
	// the abort itself recurses indefinitely.  Force a known-good slot
	// every time we land in either mode.  Distinct addresses from SP_irq
	// (0x80005b08) so the three mode stacks don't overlap.
	if (netBookBlBankedSpFix_) {
		if (mode == Abort32)       GPRs[13] = 0x80005a08u;
		else if (mode == Undefined32) GPRs[13] = 0x80005908u;
	}

	prefetchCount = 0;
	GPRs[14] = savedPC;
	GPRs[15] = newPC;

	// Real ARM hardware masks IRQs on entry to ANY exception (SWI, IRQ,
	// FIQ, Undef, Prefetch Abort, Data Abort, Reset). FIQ entry also
	// masks FIQ. Our previous implementation didn't set the I bit here;
	// the only paths that did were requestIRQ/requestFIQ at the call
	// site. That left SWI/Undef/Abort exceptions running with IRQs
	// still enabled — which on Series 5 caused IRQ entries during the
	// SWI 0x6C handler to push to SP_irq, which happens to share
	// memory with SP_svc (kernel-allocated stack collision around
	// 0x80105790), corrupting the SVC stack and routing the SWI 0x6C
	// return path back into ITSELF with R0 = iCurrentThread. That
	// stale R0 was then propagated to FUN_5003DDB8 as a "heap" pointer,
	// triggering KERN-NO-SESSION reason 20. See series5.h "2026-05-11
	// (cont. 3)" notes for the full chain. With IRQs correctly masked
	// across all exception entries, the SWI handler runs to completion
	// without preemption and the panic is gone.
	//
	// 2026-05-23 (Cross-WindEmu experiment, see HANDOVER): WindEmu does
	// NOT set the I-bit on SWI/Undef/Abort entry (only IRQ/FIQ).  We
	// gate the all-exception I-bit set behind the series5HalFix flag —
	// Series 5 genuinely needs it to avoid the SVC/IRQ stack collision
	// that triggers KERN-NO-SESSION; all other devices follow WindEmu's
	// behaviour.  Override with PSION_ALL_EXC_IBIT=0/1.
	//
	// 2026-09-17 (Geofox One bring-up): the Geofox needs it too, and its
	// ROM shows exactly why the architectural behaviour is the right one.
	// Its EKern hands SVC and IRQ the *same* 1 KB stack — the setup code
	// at 0x50019BC8 loads SP_svc and SP_irq from the same pointer
	// (SuperPage+0x388 -> 0x801057EC) — which is only safe because real
	// hardware enters SWI with IRQs masked. Leave them enabled and the
	// first IRQ taken during a fast-path executive call pushes r0-r3/ip/lr
	// straight over the SVC frame, so the call returns into whatever the
	// IRQ handler happened to leave behind. The Geofox gets it via
	// setAllExceptionIBit(), not series5HalFix, whose other workarounds
	// are specific to the Series 5 ROM.
	bool allExcIBit = series5HalFix || allExcIBit_;
	if (const char *e = PSION_ENV_CSTR("PSION_ALL_EXC_IBIT"))
		allExcIBit = std::atoi(e) != 0;
	if (allExcIBit || mode == IRQ32 || mode == FIQ32)
		CPSR |= CPSR_IRQDisable;
	if (mode == FIQ32)
		CPSR |= CPSR_FIQDisable;
	// F-MULTIAGENT step 3 probe — log I-bit transitions via
	// exception entry.  raiseException always raises I-bit, so log
	// only when the previous I-bit was 0 (=> a genuine 0->1 set).
	if (PSION_ENV_BOOL("PSION_S7_TRACE_CPSR_I")
			&& ((SPSRs[bankIndex] & 0x80u) == 0)) {
		static int n = 0;
		if (n++ < 200)
			log("[cpsr-i] 0->1 src=exc-enter mode=%02x savedPC=%08x newPC=%08x oldCPSR=%08x",
			    mode, savedPC, newPC, SPSRs[bankIndex]);
	}
}

void ARM710::requestFIQ() {
	// PSION_IMMEDIATE_IRQ=1 also affects FIQ for symmetry — WindEmu
	// dispatches both immediately.
	if (canAcceptFIQ() && PSION_ENV_BOOL("PSION_IMMEDIATE_IRQ")) {
		uint32_t vec = (cp15_control & (1u << 13)) ? 0xFFFF001Cu : 0x1Cu;
		raiseException(FIQ32, getRealPC() + 4, vec);
		CPSR |= CPSR_FIQDisable;
		CPSR |= CPSR_IRQDisable;
		return;
	}
	// Architectural model: FIQ is sampled at instruction boundary.
	// Just set the flag; tick() handles the actual exception entry.
	pendingFIQ = true;
}

void ARM710::requestIRQ() {
	// Honour the high-vectors bit (CP15.SCTLR[13]). EPOC kernels on ARMv4
	// later cores use 0xFFFF0000 as the exception base; ARM710 itself
	// also supports it via the V bit. Without this the exception
	// dispatch always lands on physical 0x18, which on a SA-1100 ROM
	// is just a `B kernel_entry` thunk, not the IRQ trampoline.
	uint32_t vec = (cp15_control & (1u << 13)) ? 0xFFFF0018u : 0x18u;

	// PSION_IMMEDIATE_IRQ=1 — WindEmu-compatible IRQ dispatch.  WindEmu
	// calls raiseException() immediately when the SoC raises an IRQ,
	// even if mid-instruction (e.g., during an LDM block transfer).
	// This is architecturally "wrong" per the ARM Reference Manual
	// (IRQs are sampled at instruction boundary on real silicon), but
	// it's what EPOC was developed and tested against — the kernel may
	// have been written with WindEmu-style timing implicitly assumed.
	// Default OFF (we use the architecturally-correct deferred model
	// via `pendingIRQ` checked in sampleAndDispatchPendingExceptions()).
	if (canAcceptIRQ() && PSION_ENV_BOOL("PSION_IMMEDIATE_IRQ")) {
		raiseException(IRQ32, getRealPC() + 4, vec);
		CPSR |= CPSR_IRQDisable;  // re-assert (raiseException may not, depending on series5HalFix)
		return;
	}
	(void)vec;  // unused in default deferred path — tick() reads cp15_control directly
	// PSION_S5_IRQ_SP_TRACE=N logs the per-mode SPs at the first N IRQ
	// entries. On Series 5 we suspect SP_irq is set to the same value
	// as SP_svc (identical stack region), causing IRQs during SVC mode
	// to corrupt the SVC stack contents.
	if (series5HalFix) {
		static int irqSpTraceN = -1;
		if (irqSpTraceN < 0) {
			const char *e = PSION_ENV_CSTR("PSION_S5_IRQ_SP_TRACE");
			irqSpTraceN = e ? std::atoi(e) : 0;
		}
		if (irqSpTraceN > 0) {
			irqSpTraceN--;
			// SP_xxx is in allModesBankedRegisters[bank][0]
			uint32_t sp_svc = allModesBankedRegisters[modeToBank[Supervisor32 & 0xF]][0];
			uint32_t sp_irq = allModesBankedRegisters[modeToBank[IRQ32 & 0xF]][0];
			uint32_t sp_und = allModesBankedRegisters[modeToBank[Undefined32 & 0xF]][0];
			uint32_t sp_abt = allModesBankedRegisters[modeToBank[Abort32 & 0xF]][0];
			uint32_t sp_fiq = allModesBankedRegisters[modeToBank[FIQ32 & 0xF]][0];
			uint32_t sp_cur = GPRs[13];
			log("[irq-sp] cur-mode-SP=%08x  SP_svc=%08x SP_irq=%08x SP_und=%08x SP_abt=%08x SP_fiq=%08x  CPSR=%02x",
				sp_cur, sp_svc, sp_irq, sp_und, sp_abt, sp_fiq, CPSR & 0x1F);
		}
	}
	// Architectural model: IRQ is sampled at instruction boundary.
	// Just set the flag; tick() handles the actual exception entry.
	pendingIRQ = true;
}

// Sample pending FIQ/IRQ flags at instruction boundary and raise the
// corresponding exception if the CPSR mask allows it.  Called from
// tick() before fetching the next instruction.  FIQ has priority
// over IRQ (real silicon behaviour).
//
// Saved PC formula: at the start of a tick the popped-and-executed
// instruction completed, GPRs[15] points 8 bytes past the instruction
// just executed (one fetch ahead).  getRealPC() = GPRs[15] - 4 = the
// next instruction's address.  LR_<exc> wants "next instr + 4" so the
// handler can subtract 4 to resume — that's getRealPC() + 4.
void ARM710::sampleAndDispatchPendingExceptions() {
	if (pendingFIQ && canAcceptFIQ()) {
		pendingFIQ = false;
		uint32_t vec = (cp15_control & (1u << 13)) ? 0xFFFF001Cu : 0x1Cu;
		raiseException(FIQ32, getRealPC() + 4, vec);
		return;
	}
	if (pendingIRQ && canAcceptIRQ()) {
		pendingIRQ = false;
		if (PSION_ENV_CSTR("PSION_NB_IRQ_ENTRY_TRACE")) {
			uint32_t sp_irq = allModesBankedRegisters[modeToBank[IRQ32 & 0xF]][0];
			log("[irq-entry] cycle ? pc=%08x cur_mode=%02x  SP_irq(pre-bank)=%08x  CPSR=%08x",
			    getRealPC(), CPSR & 0x1f, sp_irq, CPSR);
		}
		uint32_t vec = (cp15_control & (1u << 13)) ? 0xFFFF0018u : 0x18u;
		raiseException(IRQ32, getRealPC() + 4, vec);
	}
}

void ARM710::reset() {
#ifdef ARM710T_CACHE
	clearCache();
#endif
	firstFaultCaptured_ = false;   // fresh crash-capture per reset/reload
	faultLoopDetected_ = false;
	raiseException(Supervisor32, 0, 0);
	// Real ARM reset asserts BOTH I and F. raiseException sets I-bit
	// for all exceptions but only sets F-bit when entering FIQ mode.
	// Force F-bit here so FIQs are masked until the boot ROM is ready
	// to receive them (matches ARM710T datasheet: I=F=1 on reset). (H2)
	CPSR |= CPSR_FIQDisable;
}



uint32_t ARM710::tick() {
	g_armInsn++;          // desktop-idle diagnostics
	// Sample pending IRQ/FIQ at the instruction boundary BEFORE popping
	// the next instruction.  This is the architecturally correct point
	// — between two executed instructions.  If an IRQ is taken here it
	// flushes the prefetch and redirects PC to the vector.
	sampleAndDispatchPendingExceptions();

	// pop an instruction off the end of the pipeline
	bool haveInsn = false;
	uint32_t insn;
	MMUFault insnFault;
	if (prefetchCount == 2) {
		haveInsn = true;
		insn = prefetch[1];
		insnFault = prefetchFaults[1];
	}

	// Differential verification hash (see state_trace.h). Taken here — after
	// the pop, before the shuffle — because tickPageLoop() hashes at exactly
	// the same architectural point, so the two engines' traces are directly
	// comparable. Only executed instructions are hashed; pipeline-refill ticks
	// are not, since the engines structure their refills differently but must
	// agree on the cycle count, which is folded into the hash.
	if (__builtin_expect(stateTrace_.enabled, false) && haveInsn)
		stateTrace_.step(GPRs, CPSR, insnCycleApprox, devCycles_,
		                 insn, prefetch[0], prefetchCount,
		                 decodeKind(insn), decodeKind(prefetch[0]));

	// move the instruction we fetched last tick once along
	if (prefetchCount >= 1) {
		prefetch[1] = prefetch[0];
		prefetchFaults[1] = prefetchFaults[0];
	}

	// 5mxPro patched-OS idle-thread sentinel-return redirect.
	//
	// The patched OS's idle thread (around PC=0x500112c4) loops:
	//   bl wait_for_irq           ; halt, wake on any IRQ
	//   ldr r0, [pc, #4]          ; r0 = 0x80000bfc (static TDfc slot)
	//   ldmfd sp!, {lr}           ; pop lr from stack — the value
	//                             ; queued there is the "must not return"
	//                             ; sentinel 0x80000001
	//   b dispatch_dfc            ; tail-jump to FUN_5005a430(r0)
	//
	// On real hardware the static TDfc slot at 0x80000bfc is always
	// populated by the time the idle thread runs (some kernel-init
	// path our emulator skips queues a "system tick" or similar
	// always-present DFC). So FUN_5005a430 always falls through to
	// its `ldr pc, [r3]` indirect call which transfers control to the
	// queued DFC's handler. The fast-return path at 0x5005a440
	// (`ldmeqfd sp!, {pc}` — taken when *r3 == 0) is never hit on
	// real hardware, and the sentinel return address never gets used.
	//
	// In our emulator the slot is 0 (no DFC queued), so FUN_5005a430
	// takes the fast return, pops 0x80000001 into PC, and prefetch-
	// faults. The user-visible symptom is "REC reboots the device".
	//
	// Workaround: when the CPU is about to fetch from PC=0x80000001
	// (or any other low-bit-set kernel-data sentinel that signals a
	// must-not-return-here state), redirect PC back to the idle
	// thread's loop top so the kernel resumes its normal idle/IRQ
	// servicing cadence. This is a behavioural patch rather than a
	// hardware-accurate fix — the proper fix is to find what kernel
	// init code populates the static TDfc slot on real hardware and
	// trigger it, but that's a multi-day investigation. The
	// workaround leaves no observable side effect: the idle thread
	// simply loops once more rather than running the (non-existent)
	// DFC, then halts and waits for the next IRQ.
	// (Removed: the 5mxPro idle-thread sentinel-return redirect at
	// PC=0x80000001. It was a workaround for the SYMPTOM of the recording
	// bug — when the forced-CSINT model starved the kernel's per-buffer
	// completion DFC, the kernel eventually parked the idle thread onto a
	// must-not-return sentinel and prefetch-faulted. With the root cause
	// fixed (edge-driven CSINT for recording — see windermere.cpp), the
	// sentinel is never reached and the redirect fired 0 times across all
	// recording/boot tests, so it has been removed rather than left as an
	// inert behavioural patch. If execution ever reaches a low kernel-data
	// sentinel again it now surfaces as a genuine prefetch abort.)

	// (Removed: PLAT-91 panic redirect at 0x500156ec → 0x50015834 and
	// BfbcOverride at 0x5000dfbc/lr=0x50015904. Both were attempts to
	// extend 5mxPro Voice Notes recording past the 125 ms mark by
	// short-circuiting the patched-OS buffer-completion path. Neither
	// extended recording in practice — the kernel still hits a cp15
	// reset within ~0.4 ms of the redirect because the IRQ→idle-loop
	// chain that follows the buffer-fill DFC dispatch leaves CPSR in
	// an exception mode and the subsequent halt is unbreakable. The
	// correct fix needs the patched-OS kernel-server message-queue
	// initialised properly (DAT_5000c128/DAT_5000c12c populated, a
	// session registered for syscall 0x2f) — see
	// docs/5mxpro-rec-investigation.md for the dead ends explored.)
	// fetch a new instruction
	auto newInsn = fetchVirtual(GPRs[15]);
	GPRs[15] += 4;
	prefetch[0] = newInsn.first.value_or(0);
	prefetchFaults[0] = newInsn.second;
	if (prefetchCount < 2)
		prefetchCount++;

	// now deal with the one we popped
	uint32_t clocks = 1;

	// ── Decoded-op fast path (Phase 1, see docs/execution-engine-scope.md) ──
	// insnKind is non-DK_SLOW only for a clean fetch (ROM or RAM) pre-classified
	// by fetchVirtual to a simple, side-effect-contained kind. In that case (and
	// with no diagnostic env active) skip the ~40 per-instruction trace gates of
	// the slow body and the dispatch chain, and call the handler directly — the
	// operand extractions are byte-for-byte the ones executeInstruction() uses.
	// Clearing haveInsn makes the unchanged slow body below a no-op; the shared
	// fault handling (faultTriggeredThisCycle) after it still runs. Anything else
	// — SWI, coprocessor, BX/BLX, undefined, a faulted fetch, or a non-SA-1100
	// device (base fetchVirtual leaves DK_SLOW) — runs the full path.
	// insnKind, as carried through the prefetch pipeline from fetchVirtual, can
	// get out of step with the word it describes — proven on the netBook Quartz
	// image, where an IRQ left prefetchKind[] shifted one slot against
	// prefetch[] and a BEQ was dispatched as data-processing. A dispatch
	// selector that disagrees with its instruction silently runs the wrong
	// handler, so derive it from `insn` here instead, exactly as
	// tickPageLoop() does. The fault check is now explicit: fetchVirtual used
	// to fold "this fetch aborted" into DK_SLOW, and decoding the word cannot
	// know that (a faulted fetch reads as 0, which classifies as
	// data-processing and would take the fast path into a handler instead of
	// raising the abort).
	const uint8_t insnKindFast =
		(haveInsn && decodeFast_ && insnFault == NoFault) ? decodeKind(insn) : DK_SLOW;
	if (haveInsn && insnKindFast >= DK_DATAPROC && decodeFast_) {
		pcHistory[pcHistoryIndex] = {GPRs[15] - 0xC, insn};
		pcHistoryIndex = (pcHistoryIndex + 1) % PcHistoryCount;
		clocks += 1;   // mirror executeInstruction()'s base `cycles = 1` (counted
		               // even on a failed condition) so cycle accounting matches
		               // the slow path exactly.
		if (checkCondition(extract(insn, 31, 28))) {
			switch (insnKindFast) {
			case DK_DATAPROC:      clocks += execDataProcessing(extract1(insn,25), extract(insn,24,21), extract1(insn,20), extract(insn,19,16), extract(insn,15,12), extract(insn,11,0)); break;
			case DK_LDR_STR:       clocks += execSingleDataTransfer(extract(insn,25,20), extract(insn,19,16), extract(insn,15,12), extract(insn,11,0)); break;
			case DK_LDM_STM:       clocks += execBlockDataTransfer(extract(insn,24,20), extract(insn,19,16), extract(insn,15,0)); break;
			case DK_BRANCH:        clocks += execBranch(extract1(insn,24), extract(insn,23,0)); break;
			case DK_MULTIPLY:      clocks += execMultiply(extract(insn,21,20), extract(insn,19,16), extract(insn,15,12), extract(insn,11,8), extract(insn,3,0)); break;
			case DK_MULTIPLY_LONG: clocks += execMultiplyLong(extract(insn,22,20), extract(insn,19,16), extract(insn,15,12), extract(insn,11,8), extract(insn,3,0)); break;
			case DK_SWAP:          clocks += execSingleDataSwap(extract1(insn,22), extract(insn,19,16), extract(insn,15,12), extract(insn,3,0)); break;
			case DK_HALFWORD:      clocks += execHalfwordDataTransfer(insn); break;
			}
		}
		haveInsn = false;   // handled — skip the slow body
	}

	if (haveInsn) {
		pcHistory[pcHistoryIndex] = {GPRs[15] - 0xC, insn};
		pcHistoryIndex = (pcHistoryIndex + 1) % PcHistoryCount;

		// PSION_S7_TRACE_WAITFOR=1 — return-side matcher.  When PC is
		// about to execute in User mode AFTER a SWI 0xc00084 was
		// recorded, log the user-mode PCs we land on so we can see
		// where the kernel actually returns control.  Logs the first
		// N user-mode PCs after each SWI entry (followUserPCs counter).
		if (PSION_ENV_BOOL("PSION_S7_TRACE_WAITFOR") &&
		    (CPSR & 0x1F) == 0x10 &&
		    g_psionWaitForTrace.returnCount < PsionWaitForTrace::kCap * 20) {
			uint32_t pc = GPRs[15] - 0xC;
			// Skip recording while we're still in the SWI thunk table —
			// PC in [0x5004d6f0, 0x5004d800) is a chain of SWIs.
			bool inSwiThunkTable = (pc >= 0x5004d6f0u && pc < 0x5004d800u);
			if (g_psionWaitForTrace.followUserPCs > 0 && !inSwiThunkTable) {
				g_psionWaitForTrace.followUserPCs--;
				g_psionWaitForTrace.returnCount++;
				log("[waitfor] usr-post cyc=%llu #%d pc=%08x r0=%08x r1=%08x lr=%08x sp=%08x cpsr=%08x",
				    (unsigned long long)insnCycleApprox,
				    g_psionWaitForTrace.returnCount, pc,
				    GPRs[0], GPRs[1], GPRs[14], GPRs[13], CPSR);
			}
		}
		// PSION_S7_TRACE_WAITFOR_KERNEL=<cyc_lo>:<cyc_hi> — periodically
		// sample the executing PC + CPSR mode in a cycle window, so we
		// can see what the kernel is doing between the last SWI entry
		// and the sleep transition.  Sample every 1M cycles inside the
		// window to bound log size.
		if (const char *e = PSION_ENV_CSTR("PSION_S7_TRACE_WAITFOR_KERNEL")) {
			static bool parsed = false;
			static uint64_t kLo = 0, kHi = 0;
			static uint64_t kNextSample = 0;
			static int kCnt = 0;
			if (!parsed) {
				parsed = true;
				char buf[64] = {};
				std::strncpy(buf, e, sizeof(buf) - 1);
				char *p = buf;
				char *c1 = std::strchr(p, ':');
				if (c1) { *c1 = 0; kLo = std::strtoull(p, nullptr, 0); kHi = std::strtoull(c1 + 1, nullptr, 0); }
				kNextSample = kLo;
			}
			if (kLo && insnCycleApprox >= kNextSample && insnCycleApprox < kHi && kCnt < 200) {
				kNextSample = insnCycleApprox + 1000000;
				kCnt++;
				log("[waitfor-k] cyc=%llu #%d pc=%08x lr=%08x sp=%08x cpsr=%08x r0=%08x",
				    (unsigned long long)insnCycleApprox, kCnt,
				    GPRs[15] - 0xC, GPRs[14], GPRs[13], CPSR, GPRs[0]);
			}
		}

		// PSION_INSN_TRACE=<lo>,<hi>: log every instruction executed with
		// PC in [lo, hi). Prints instruction word + register state so we
		// can see what BL / LDR actually did.
		{
			static bool inited = false;
			static uint32_t traceLo = 0, traceHi = 0;
			if (!inited) {
				inited = true;
				if (const char *e = PSION_ENV_CSTR("PSION_INSN_TRACE")) {
					char *endp;
					traceLo = (uint32_t)std::strtoul(e, &endp, 0);
					if (*endp == ',') traceHi = (uint32_t)std::strtoul(endp + 1, nullptr, 0);
				}
			}
			if (traceLo && traceHi) {
				uint32_t pc = GPRs[15] - 0xC;
				if (pc >= traceLo && pc < traceHi) {
					log("[trace] pc=%08x insn=%08x r0=%08x r1=%08x r2=%08x r3=%08x r4=%08x r14=%08x sp=%08x cpsr=%02x",
						pc, insn, GPRs[0], GPRs[1], GPRs[2], GPRs[3], GPRs[4], GPRs[14], GPRs[13], CPSR & 0x1F);
				}
			}

			// PSION_R0_WATCH=<target>: log every instruction when R0 becomes
			// exactly that value, plus its predecessor instruction's R0. This
			// is for tracking down where R0 gets mutated during long call
			// chains (e.g. the Series 5 SWI dispatcher).
			{
				static bool inited = false;
				static uint32_t target = 0;
				static bool active = false;
				static uint32_t prevR0 = 0;
				static uint32_t prevPc = 0;
				if (!inited) {
					inited = true;
					if (const char *e = PSION_ENV_CSTR("PSION_R0_WATCH"))
						target = (uint32_t)std::strtoul(e, nullptr, 0);
					active = (target != 0);
				}
				if (active) {
					uint32_t r0 = GPRs[0];
					if (prevR0 != target && r0 == target) {
						uint32_t pc = GPRs[15] - 0xC;
						log("[r0] became %08x at pc=%08x (prev r0=%08x at pc=%08x)",
							r0, pc, prevR0, prevPc);
					}
					prevR0 = r0;
					prevPc = GPRs[15] - 0xC;
				}
			}
		}
		// PSION_INSN_TRACE_CYC=<lo>,<hi>: log every executed instruction
		// when our approximate cycle counter is in [lo, hi). Useful for
		// capturing a narrow time window (e.g., a thread's brief
		// activation slot) without filtering by PC range. The cycle
		// counter is incremented by executeInstruction's return value
		// at end of step, so this check uses the PRE-increment value
		// = cycles consumed BEFORE this instruction. Series 5
		// supervisor activations of ~4K cycles each at known
		// timestamps benefit from this.
		{
			static bool initedCyc = false;
			static uint64_t cycLo = 0, cycHi = 0;
			if (!initedCyc) {
				initedCyc = true;
				if (const char *e = PSION_ENV_CSTR("PSION_INSN_TRACE_CYC")) {
					char *endp;
					cycLo = std::strtoull(e, &endp, 0);
					if (*endp == ',')
						cycHi = std::strtoull(endp + 1, nullptr, 0);
				}
			}
			if (cycLo && cycHi && insnCycleApprox >= cycLo
			    && insnCycleApprox < cycHi) {
				uint32_t pc = GPRs[15] - 0xC;
				log("[cyctrc] cyc=%llu pc=%08x insn=%08x r0=%08x r1=%08x r4=%08x lr=%08x sp=%08x cpsr=%02x",
				    (unsigned long long)insnCycleApprox, pc, insn,
				    GPRs[0], GPRs[1], GPRs[4], GPRs[14], GPRs[13],
				    CPSR & 0x1F);
			}
		}
		// (5mxPro REC-crash diagnostics — IrqEntrySP-LOW at 0x50006980,
		// HandleBoundsFail at 0x50010c3c, HandleDeref-BAD at 0x50010c50 —
		// removed once the root cause was identified. See the long
		// comment at the bottom of writeVirtual for the full chain.)
		// PSION_S5_FORCE_GATE3=1 hook: at PC=0x50009D40 (the LDR R3,
		// [R4, #0x24] in FUN_50009D2C, the response-delivery gate),
		// forcibly set *(R4 + 0x14) and *(R4 + 0x24) to 3 so the gate's
		// CMP R3, #3 passes and the wait sentinel gets written.
		// Empirical effect: 274 unique_pcs vs 268 baseline (+6),
		// 10 traps from new code paths (vs 0 baseline). The traps
		// indicate the boot reaches code that depends on the thread
		// genuinely being in state 3, not just having those fields.
		//
		// PSION_S5_DIRECT_WRITE=1 alternative: at PC=0x50009D58 (the
		// STREQ R3, [R0]), unconditionally execute the store regardless
		// of CPU flags. This delivers the result without disturbing
		// the thread state. Less invasive than FORCE_GATE3.
		if (PSION_ENV_BOOL("PSION_S5_FORCE_GATE3")) {
			uint32_t pcNow = GPRs[15] - 0xC;
			if (pcNow == 0x50009D40u) {
				static int forceCount = 0;
				if (++forceCount <= 4) {
					log("[force-gate3 #%d] thread=%08x — pre-LDR forcing +0x14 & +0x24 = 3",
					    forceCount, GPRs[4]);
				}
				writeVirtual(3, GPRs[4] + 0x14, V32);
				writeVirtual(3, GPRs[4] + 0x24, V32);
			}
		}
		// PSION_S5_SYNTH_DELIVERY=1 hook: at PC=0x500025E8 (the slot[0x20]=2
		// store inside FUN_500025BC for SWI 0xc00076 R0=0x1c IPC dispatch),
		// synthesise the level-2 IPC delivery (server-thread queue drain
		// missing in our emulation) AND prevent the SWI return from being
		// preempted:
		//   1. Write 0 (KErrNone) to the wait sentinel at 0x80104528.
		//   2. Clear the iRescheduleNeededFlag at 0x80100348 — the SWI
		//      dispatcher tail at PC=0x50019630 calls FUN_500191DC if
		//      *0x8010032c < 2, and that returns nonzero when iCurrent-
		//      Thread changes, which forces a context save instead of
		//      MOVS PC,LR. Clearing 0x80100348 keeps the scheduler from
		//      switching threads on this SWI return so the supervisor
		//      resumes at 0x5003AD10 with R0=0 and proceeds.
		// Bypasses the missing case-0-loader-thread queue drain step.
		// PSION_S5_SYNTH_DELIVERY=1: synthesise the missing level-2 IPC
		// delivery for SWI 0xc00076 R0=0x1c (RProcess::Create-equivalent)
		// AND keep the supervisor running through both the spawn SWI and
		// the subsequent WaitForAnyRequest loop:
		//
		//   1. At PC=0x500025E8 (slot[0x20]=2 in FUN_500025BC): write 0
		//      (KErrNone) to wait sentinel at 0x80104528, pre-bump the
		//      supervisor's request-semaphore counter at *(thread+0x68)+
		//      0x14 so the wait loop's SWI 0xc0004d returns without
		//      blocking, and arm no-reschedule for the SWI 0xc00076
		//      dispatcher exit.
		//
		//   2. At PC=0x50019644 (CMP in SWI dispatcher tail): if armed,
		//      force r0=0 so MOVSEQ PC,LR fires (return to caller) and
		//      re-arm so the next dispatcher tail (after the SWI 0xc0004d
		//      in the wait loop) also returns instead of preempting.
		if (PSION_ENV_BOOL("PSION_S5_SYNTH_DELIVERY")) {
			uint32_t pcNow = GPRs[15] - 0xC;
			static int noReschedRemaining = 0;
			static bool fakeDProcInited = false;

			// Fake DProcess scratch lives at 0x80108000+ — populated lazily
			// inside the FUN_5000D840 hook in executeInstruction().
			(void)fakeDProcInited;

			if (pcNow == 0x500025E8u) {
				auto deliverPtr = readVirtualDebug(GPRs[4] + 0x24, V32);
				if (deliverPtr.has_value() && deliverPtr.value() == 0x80104528u) {
					auto curThread = readVirtualDebug(0x8010061Cu, V32);
					uint32_t semCounterAddr = 0;
					if (curThread.has_value() && curThread.value() != 0) {
						auto subObj = readVirtualDebug(curThread.value() + 0x68u, V32);
						if (subObj.has_value() && subObj.value() != 0)
							semCounterAddr = subObj.value() + 0x14u;
					}
					static int synthCount = 0;
					if (++synthCount <= 4) {
						log("[synth-delivery #%d] slot=%08x curThread=%08x semCtr=%08x — wrote 0/+1, arming 2x no-reschedule",
						    synthCount, GPRs[4], curThread.value_or(0), semCounterAddr);
					}
					writeVirtual(0, 0x80104528u, V32);
					if (semCounterAddr) {
						auto curCtr = readVirtualDebug(semCounterAddr, V32);
						writeVirtual(curCtr.value_or(0) + 1, semCounterAddr, V32);
					}
					noReschedRemaining = 2;
				}
			}
			if (noReschedRemaining > 0 && pcNow == 0x50019644u) {
				GPRs[0] = 0;
				--noReschedRemaining;
			}

		}
		// PSION_S5_DUMP_C0002E_ARGS=1: at PC=0x5000B968 (entry of the
		// selector-0x2e handler that EFile.main() calls last before
		// giving up), dump R0 and R1 (user-mode descriptor pointers)
		// and the bytes they point to. Goal: identify what EFile is
		// trying to look up that fails.
		if (PSION_ENV_BOOL("PSION_S5_DUMP_C0002E_ARGS")) {
			uint32_t pcNow = GPRs[15] - 0xC;
			if (pcNow == 0x5000B968u) {
				static int dumpCount = 0;
				++dumpCount;
				if (dumpCount > 4) {
					// Only dump first 4 instances
				} else {
					log("[c0002e-dump #%d] R0=%08x R1=%08x R2=%08x R3=%08x",
					    dumpCount, GPRs[0], GPRs[1], GPRs[2], GPRs[3]);
					// Dump the bytes at R0 (descriptor 1)
					auto readByte = [this](uint32_t va) -> int {
						auto v = readVirtualDebug(va, V8);
						return v.has_value() ? (int)v.value() : -1;
					};
					auto readWord = [this](uint32_t va) -> uint32_t {
						auto v = readVirtualDebug(va, V32);
						return v.value_or(0xDEADBEEFu);
					};
					// Also dump 0x5022c0 (which words[2,3] point to)
					{
						auto rb = [this](uint32_t va) -> int {
							auto v = readVirtualDebug(va, V8);
							return v.has_value() ? (int)v.value() : -1;
						};
						auto rw = [this](uint32_t va) -> uint32_t {
							auto v = readVirtualDebug(va, V32);
							return v.value_or(0xDEADBEEFu);
						};
						uint32_t base = 0x005022C0u;
						log("  *0x%08x: words [%08x %08x %08x %08x %08x %08x %08x %08x]",
						    base, rw(base), rw(base+4), rw(base+8), rw(base+12),
						    rw(base+16), rw(base+20), rw(base+24), rw(base+28));
						// As string
						char buf[64] = {};
						for (int i = 0; i < 32; i++) {
							int b = rb(base + i);
							if (b < 0) break;
							buf[i] = (b >= 0x20 && b <= 0x7E) ? (char)b : '?';
						}
						log("  *0x%08x as ASCII: \"%s\"", base, buf);
					}
					for (int slot = 0; slot < 2; slot++) {
						uint32_t va = (slot == 0) ? GPRs[0] : GPRs[1];
						const char *name = (slot == 0) ? "R0" : "R1";
						uint32_t w0 = readWord(va);
						uint32_t w1 = readWord(va + 4);
						uint32_t w2 = readWord(va + 8);
						uint32_t w3 = readWord(va + 12);
						log("  %s=%08x: words [%08x %08x %08x %08x]",
						    name, va, w0, w1, w2, w3);
						// If word 0 looks like a TPtrC descriptor (high nibble ~ 0-9),
						// follow word 1 as a string pointer
						uint32_t lengthOrType = w0;
						uint32_t contentPtr = w1;
						if (contentPtr >= 0x00400000 && contentPtr < 0x80000000) {
							// Looks like user-mode pointer to string content
							char buf[64] = {};
							int n = (lengthOrType & 0xFFF) < 60 ? (int)(lengthOrType & 0xFFF) : 60;
							for (int i = 0; i < n; i++) {
								int b = readByte(contentPtr + i);
								if (b < 0) break;
								buf[i] = (b >= 0x20 && b <= 0x7E) ? (char)b : '?';
							}
							log("  %s deref: \"%s\"  (len=%d ptr=%08x)",
							    name, buf, lengthOrType & 0xFFF, contentPtr);
						}
						// Also try reading content directly from R0 + 4 (for inline TBuf)
						char buf[64] = {};
						for (int i = 0; i < 32; i++) {
							int b = readByte(va + 4 + i);
							if (b < 0 || b == 0) break;
							buf[i] = (b >= 0x20 && b <= 0x7E) ? (char)b : '?';
						}
						if (buf[0])
							log("  %s+4 inline: \"%s\"", name, buf);
					}
				}
			}
		}
		if (PSION_ENV_BOOL("PSION_S5_DIRECT_WRITE")) {
			uint32_t pcNow = GPRs[15] - 0xC;
			// At PC=0x50009D54 (LDREQ R3, [SP]), force-load R3 from SP
			// regardless of EQ. At 0x50009D58 (STREQ R3, [R0]), force-
			// store regardless of EQ. Together they unconditionally
			// deliver the result.
			if (pcNow == 0x50009D54u) {
				// Just before the conditional load — pre-load R3
				// from SP so even if BNE skipped past, downstream is OK.
				if (auto v = readVirtualDebug(GPRs[13], V32); v.has_value()) {
					GPRs[3] = v.value();
				}
			}
			if (pcNow == 0x50009D58u) {
				static int dwCount = 0;
				if (++dwCount <= 4) {
					log("[direct-write #%d] writing R3=%08x to *R0=%08x",
					    dwCount, GPRs[3], GPRs[0]);
				}
				writeVirtual(GPRs[3], GPRs[0], V32);
			}
		}
		// PSION_S5_WAIT_STATE3=1: when a thread issues SWI 0xc0004d
		// (WaitForAnyRequest), set its NThread+0x14 and +0x24 to 3.
		// This simulates the kernel's "transition to EWaitFastSemaphore"
		// step that we don't observe firing naturally. Done at SWI
		// dispatch time so it fires for whichever thread is calling.
		if (PSION_ENV_BOOL("PSION_S5_WAIT_STATE3") &&
		    (insn & 0x0F000000) == 0x0F000000 &&
		    (insn & 0xFFFFFF) == 0xc0004d) {
			// iCurrentThread is at virt 0x8010061C
			if (auto cur = readVirtualDebug(0x8010061Cu, V32); cur.has_value()) {
				static int wsCount = 0;
				if (++wsCount <= 4) {
					log("[wait-state3 #%d] thread=%08x — setting +0x14 & +0x24 = 3",
					    wsCount, cur.value());
				}
				writeVirtual(3, cur.value() + 0x14, V32);
				writeVirtual(3, cur.value() + 0x24, V32);
			}
		}
		// PSION_S5_SKIP_REFDEC=1: at PC=0x50008E90 (the LDR R3, [R4,
		// #0x10] that faults when R4 is garbage 0x00704000), skip the
		// next 4 instructions (LDR/SUB/STR/CMP — the entire atomic-
		// decrement-and-test) and proceed as if refcount didn't reach 0.
		// Goal: bypass this layer of the cascade to see what the next
		// blocker is.
		// PSION_S5_FIX_R4_AT_8E90=1: at PC=0x50008E90, when R4 is an
		// invalid low-virtual address (< 0x80000000), redirect it to
		// kernel scratch RAM so the LDR/SUB/STR refcount doesn't fault.
		// Using 0x80105000 (kernel scratch area, not on any iCurrentThread
		// path we know cares about +0x10). This is a heuristic redirect
		// to test what's downstream of the fault.
		if (PSION_ENV_BOOL("PSION_S5_FIX_R4_AT_8E90")) {
			uint32_t pcNow = GPRs[15] - 0xC;
			if (pcNow == 0x50008E90u && GPRs[4] < 0x80000000u) {
				static int fixCount = 0;
				if (++fixCount <= 4) {
					log("[fix-r4 #%d] redirecting R4=%08x → 0x80105000",
					    fixCount, GPRs[4]);
				}
				GPRs[4] = 0x80105000u;
			}
		}

		// Hook-table / DFC-queue tracer. When cfDiagEnabled, snapshot the
		// queue-head pointer contents every time we enter FUN_50004980
		// (IRQ dispatcher) so we can see whether CF DFCs are ever present
		// on the iDfcs list at IRQ exit.
		if (cfDiagEnabled) {
			uint32_t pc = GPRs[15] - 0xC;
			if (pc == 0x50004980 || pc == 0x50005228) {
				// DAT_500052c4 is at ROM offset 0x52c4 — it stores the
				// virtual address of the iDfcs list head (a 2-word
				// self-pointer when empty).
				auto qhAddrOpt = readVirtualDebug(0x500052c4, V32);
				uint32_t qhAddr = qhAddrOpt.value_or(0);
				uint32_t qhNext = 0, qhPrev = 0;
				if (qhAddr) {
					qhNext = readVirtualDebug(qhAddr,     V32).value_or(0);
					qhPrev = readVirtualDebug(qhAddr + 4, V32).value_or(0);
				}
				log("DIAG IRQ-entry pc=%08x lr=%08x iDfcs=%08x next=%08x prev=%08x CPSR=%08x",
				    pc, GPRs[14], qhAddr, qhNext, qhPrev, CPSR);
			}
		}
		// PSION_S5_FAULT_CALLSITE=1 — log every time we execute the BL
		// at PC=0x5000CD7C and the LDR at 0x5000CD84 (the fault site).
		// Used to confirm whether the BL's target is what we think.
		// PSION_S5_MATCHF_ENTRY_LR=N — log the LR (caller address) and last
		// few PCs whenever PC=0x5000C208 (MatchF entry) executes. Tells us
		// who is calling MatchF when LR happens to be 0x5000CD80 at entry.
		if (series5HalFix && (GPRs[15] - 0xC) == 0x5000C208u) {
			static int matchfEntryLogN = -1;
			if (matchfEntryLogN == -1) {
				const char *e = PSION_ENV_CSTR("PSION_S5_MATCHF_ENTRY_LR");
				matchfEntryLogN = e ? std::atoi(e) : 0;
			}
			if (matchfEntryLogN > 0 && GPRs[14] == 0x5000CD80u) {
				matchfEntryLogN--;
				log("[matchf-entry] PC=0x5000C208 entered with LR=0x5000CD80!");
				log("[matchf-entry]   r0=%08x r1=%08x r2=%08x r3=%08x",
					GPRs[0], GPRs[1], GPRs[2], GPRs[3]);
				log("[matchf-entry]   sp=%08x cpsr=%08x", GPRs[13], CPSR);
				log("[matchf-entry]   recent PCs (oldest → newest):");
				logPcHistory();
			}
		}
		if (series5HalFix) {
			static int callsiteLogN = -1;
			if (callsiteLogN == -1) {
				const char *e = PSION_ENV_CSTR("PSION_S5_FAULT_CALLSITE");
				callsiteLogN = e ? std::atoi(e) : 0;
			}
			uint32_t cspc = GPRs[15] - 0xC;
			// PSION_S5_THREAD_TRACE=N — log entries to NThread::Resume
			// (0x50011424) and AddToReadyList (0x50042D54). If a thread
			// is created but Resume() never fires for it, that's the
			// boot blocker — EFile/downstream user threads never run.
			// Also log NFastSemaphore::Wait (0x5000F5C0) — this is the
			// function that sets iNState=3 (EWaitFastSemaphore), the
			// state the SWI 0xC00076 response-delivery gate checks for.
			// PSION_S5_GATE_FIX=1 — at WaitForAnyRequest dispatch
			// (PC=0x5000B158, LDR PC, [R3, #0x20]), force the
			// thread+0x24 field = 3 to satisfy the SWI 0xC00076
			// R0=0x1C IPC response-delivery gate documented at
			// series5.h:2706. Empirically the kernel transitions
			// the thread's iNState (+0xC4) to 2 via 0x5000F76C
			// but never writes +0x24=3, so the gate never fires.
			if (cspc == 0x5000B158u) {
				static int gateFix = -1;
				if (gateFix < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_GATE_FIX");
					gateFix = (e && e[0] == '1') ? 1 : 0;
				}
				if (gateFix) {
					auto cur = readVirtualDebug(0x8010061Cu, V32);
					if (cur.has_value() && cur.value() != 0 &&
					    cur.value() != 0xFFFFFFFFu) {
						writeVirtual(3, cur.value() + 0x24, V32);
						static int gateLogN = 0;
						if (++gateLogN <= 8)
							log("[gate-fix] PC=0x5000B158: forced *(curThread=0x%08x + 0x24) = 3; fire %d/8",
								cur.value(), gateLogN);
					}
				}
			}
			if (cspc == 0x50011424u || cspc == 0x50042D54u || cspc == 0x5000F5C0u || cspc == 0x5000F59Cu || cspc == 0x5000B144u || cspc == 0x5000B158u || cspc == 0x500090E0u) {
				static int threadTraceN = -1;
				if (threadTraceN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_THREAD_TRACE");
					threadTraceN = e ? std::atoi(e) : 0;
				}
				if (threadTraceN > 0) {
					threadTraceN--;
					const char *name =
						(cspc == 0x50011424u) ? "Resume"
						: (cspc == 0x50042D54u) ? "AddToReadyList"
						: (cspc == 0x5000F59Cu) ? "FastSemWait_entry"
						: (cspc == 0x5000F5C0u) ? "FastSemWait_setstate3"
						: (cspc == 0x5000B144u) ? "WaitForAnyReq_entry"
						: (cspc == 0x500090E0u) ? "ThreadInit_setstate3"
						: "WaitForAnyReq_dispatch";
					log("[%s] r0=%08x r1=%08x r2=%08x r3=%08x lr=%08x cpsr=%02x",
						name, GPRs[0], GPRs[1], GPRs[2], GPRs[3], GPRs[14], CPSR & 0x1F);
				}
			}
			// PSION_S5_GATE_TRACE=N — log first N entries to FUN_50009d24
			// (the IPC response-delivery gate). On 2026-05-10 the existing
			// handover hypothesis ("find the +0x14=3 writer") was disproved
			// by re-disassembling the ROM bytes at 0x50009d24:
			//   50009d40: LDR R3, [R4, #0x24]
			//   50009d44: CMP R3, #3
			//   50009d48: BNE 0x50009d8c   ; <-- jumps to function epilogue
			//   50009d4c: LDR R3, [R4, #0x14]
			//   50009d50: CMP R3, #3
			//   50009d54: LDREQ R3, [SP]
			//   50009d58: STREQ R3, [R0]
			//   50009d5c: BEQ 0x50009d7c
			//   50009d60-78: vtable[0x48] dispatch (slow VM-mediated write)
			//   50009d7c-88: wakeup R0=*(R4+0x68); LDR PC, [R0+0x24]
			//   50009d8c: ADD SP,SP,#4 ; LDMFD ...
			// So the actual gate is +0x24==3 only; +0x14==3 just selects
			// the fast vs slow delivery path — both fire delivery + wakeup.
			// This trace logs param_1 (R0 at entry), *(param_1+0x24),
			// *(param_1+0x14), p2 (slot ptr R1), p3 (value R2), and LR.
			if (cspc == 0x50009D24u) {
				static int gateTraceN = -1;
				if (gateTraceN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_GATE_TRACE");
					gateTraceN = e ? std::atoi(e) : 0;
				}
				if (gateTraceN > 0) {
					gateTraceN--;
					uint32_t p1 = GPRs[0];
					auto v24 = readVirtualDebug(p1 + 0x24, V32);
					auto v14 = readVirtualDebug(p1 + 0x14, V32);
					char b24[16], b14[16];
					if (v24.has_value()) std::snprintf(b24, sizeof(b24), "%08x", v24.value()); else std::snprintf(b24, sizeof(b24), "??");
					if (v14.has_value()) std::snprintf(b14, sizeof(b14), "%08x", v14.value()); else std::snprintf(b14, sizeof(b14), "??");
					log("[gate-trace] p1=%08x +0x24=%s +0x14=%s p2=%08x p3=%08x lr=%08x",
						p1, b24, b14, GPRs[1], GPRs[2], GPRs[14]);
				}
			}
			// PSION_S5_WAKE_TRACE=N — at PC=0x50009d88 (LDR PC, [R3, #0x24])
			// inside FUN_50009d24's wakeup tail, log the resolved target.
			//   R4 = server thread (param_1)
			//   R0 = *(R4 + 0x68) = sub-object pointer
			//   R3 = *R0           = sub-object vtable
			//   target = *(R3 + 0x24)
			// If target is NOT NThreadBase::Resume (0x50011424), the wakeup
			// is dispatching to a no-op or wrong virtual — the real
			// blocker for the user-thread scheduling.
			if (cspc == 0x50009D88u) {
				static int wakeTraceN = -1;
				if (wakeTraceN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_WAKE_TRACE");
					wakeTraceN = e ? std::atoi(e) : 0;
				}
				if (wakeTraceN > 0) {
					wakeTraceN--;
					uint32_t r3 = GPRs[3];
					uint32_t r4 = GPRs[4];
					uint32_t r0 = GPRs[0];
					auto target = readVirtualDebug(r3 + 0x24, V32);
					auto subobj = readVirtualDebug(r4 + 0x68, V32);
					char btgt[16], bsub[16];
					if (target.has_value()) std::snprintf(btgt, sizeof(btgt), "%08x", target.value()); else std::snprintf(btgt, sizeof(btgt), "??");
					if (subobj.has_value()) std::snprintf(bsub, sizeof(bsub), "%08x", subobj.value()); else std::snprintf(bsub, sizeof(bsub), "??");
					// Sub-object's +0x14 is the FastSemaphore counter.
					// If >= 0 at Signal entry, no waiter exists (no-op path).
					// If < 0, a waiter exists and AddToReadyList should fire.
					auto cnt = readVirtualDebug(r0 + 0x14, V32);
					char bcnt[16];
					if (cnt.has_value()) std::snprintf(bcnt, sizeof(bcnt), "%08x", cnt.value()); else std::snprintf(bcnt, sizeof(bcnt), "??");
					log("[wake-trace] server=%08x sub=%s vtable=%08x +0x24=%s sub+0x14=%s",
						r4, bsub, r3, btgt, bcnt);
				}
			}
			// PSION_S5_SR_TRACE=N — at PC=0x5003ACF4, log the entry to the
			// user-mode SendReceive wrapper (subroutine that issues SWI
			// 0xc00076 with selector R0). Captures: R0 (selector), R1
			// (data), LR (caller PC), and the caller-of-caller pulled
			// from [R13]+8 if present (depends on caller stack frame).
			// Use to identify what user-mode function is issuing the 4
			// cycling F32 selectors {0x15, 0x19, 0x1B, 0x27} at LR=
			// 0x5003AD10. Default-off; only fires when we're in CPSR=0x10
			// (true user-mode call) so kernel-mode hits aren't logged.
			if (cspc == 0x5003ACF4u) {
				static int srTraceN = -1;
				if (srTraceN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_SR_TRACE");
					srTraceN = e ? std::atoi(e) : 0;
				}
				if (srTraceN > 0) {
					srTraceN--;
					log("[sr-trace] cpsr=%02x sel=R0=%08x R1=%08x R2=%08x R3=%08x lr=%08x sp=%08x",
						CPSR & 0x1F, GPRs[0], GPRs[1], GPRs[2], GPRs[3], GPRs[14], GPRs[13]);
				}
			}
			// PSION_S5_DESTROY_ITER_TRACE=N — at PC=0x5004C380
			// (the indirect virtual-dispatch LDR PC, [R3, #8]
			// inside FUN_5004C360). Logs R0 (target object) AND
			// the caller-of-FUN_5004C360 (read from [SP+4]).
			// Filter to fires where the eventual target is
			// 0x80006900 (the failing NThread). Tells us who
			// triggers the broken destroy.
			if (cspc == 0x5004C380u && GPRs[0] == 0x80006900u) {
				static int destroyTraceN = -1;
				if (destroyTraceN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_DESTROY_ITER_TRACE");
					destroyTraceN = e ? std::atoi(e) : 0;
				}
				if (destroyTraceN > 0) {
					destroyTraceN--;
					// FUN_5004C360 prologue: STMFD SP!, {R4, LR}
					// — so [SP+0]=R4_old, [SP+4]=LR_old (the
					// caller-of-FUN_5004C360).
					auto callerLR = readVirtualDebug(GPRs[13] + 4, V32);
					auto containerR4 = readVirtualDebug(GPRs[13], V32);
					log("[destroy-iter] target=%08x R3=%08x containerR4_old=%08x callerLR=%08x sp=%08x",
						GPRs[0], GPRs[3],
						containerR4.has_value() ? containerR4.value() : 0u,
						callerLR.has_value() ? callerLR.value() : 0u,
						GPRs[13]);
				}
			}
			// PSION_S5_TRAP_SWI_TRACE=N — at PC=0x5000CD6C (the
			// STR R3, [R4, #0x48] inside SWI 0xC00072 handler,
			// which copies *(iCurrentThread + 0x40) into the
			// trap frame's pending-cleanup slot). Filter to
			// fires when R3 = 0x80006900 (the offending value).
			// Logs the current iCurrentThread (= R0 at that
			// point), R3 (value), R4 (trap frame address). The
			// iCurrentThread tells us WHICH thread's +0x40 field
			// holds 0x80006900 — narrowing where we should
			// watch the prior writer.
			if (cspc == 0x5000CD6Cu && GPRs[3] != 0) {
				static int trapSwiTraceN = -1;
				if (trapSwiTraceN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_TRAP_SWI_TRACE");
					trapSwiTraceN = e ? std::atoi(e) : 0;
				}
				if (trapSwiTraceN > 0) {
					trapSwiTraceN--;
					log("[trap-swi] cpsr=%02x iCurrentThread=%08x R3=%08x R4=%08x lr=%08x",
						CPSR & 0x1F, GPRs[0], GPRs[3], GPRs[4], GPRs[14]);
				}
			}
			// PSION_S5_FAILING_DISP=N — at PC=0x5004C36C (LDR R0,
			// [R4, #0x48] right after the SWI 0x72 returns). Logs
			// R4 (jmp_buf addr) and the value at *(R4+0x48). Filter
			// to only fire when the value is 0x80006900 (the
			// failing case) to find the kernel thread + sp at the
			// moment of the failing destroy dispatch.
			// PSION_S5_TRAP_LEAVE_TRACE=N — at PC=0x50009628 (CMP R3, #0
			// inside FUN_500095F8 right after the TRAP body ran).
			// R3 = local_58 = the Leave error code (0 = no leave,
			// non-zero = User::Leave was raised). Logs whenever a
			// Leave occurred during session creation. This tests
			// hypothesis (ii) from series5.h: the destroy chain may
			// be triggered by a half-constructed thread cleanup
			// after session-create leaves.
			// PSION_S5_DTOR_TRACE=N — at FUN_500111C4 entry (the compound
			// destructor that tail-calls NThread destructor). Logs R0
			// (the object being destroyed) and LR (caller). Filtered to
			// fire only when R0 is the failing NThread (0x80006900) so
			// we capture the chain that leads to the destructive cycle's
			// assertion failure.
			// PSION_S5_SP_TRACE=N — at PC=0x50010EA0 (the STR R0, [R4]
			// scheduler step that updates iCurrentThread). Logs the
			// banked SP values just before/after the context switch so
			// we can see what the boot thread's UND-mode stack pointer
			// is initialised to.
			// PSION_S5_SVC_SP_TRACE=N — log every change to SVC-mode SP
			// during early boot.
			{
				static int svcSpTraceN = -1;
				static uint32_t lastSvcSp = 0xFFFFFFFE;
				if (svcSpTraceN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_SVC_SP_TRACE");
					svcSpTraceN = e ? std::atoi(e) : 0;
				}
				if (svcSpTraceN > 0) {
					uint32_t curSvcSp = (currentBank() == SvcBank)
						? GPRs[13]
						: allModesBankedRegisters[SvcBank][0];
					if (curSvcSp != lastSvcSp) {
						lastSvcSp = curSvcSp;
						svcSpTraceN--;
						log("[svc-sp] new SVC_SP=%08x curBank=%d cpsr=%02x pc=%08x",
							curSvcSp, currentBank(), CPSR & 0x1F, cspc);
					}
				}
			}
			// PSION_S5_UND_SP_TRACE=N — log every change to UND-mode SP
			// (live or banked) during early boot to find where the kernel
			// initialises SP_und for the boot thread.
			{
				static int undSpTraceN = -1;
				static uint32_t lastUndSp = 0xFFFFFFFF;
				if (undSpTraceN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_UND_SP_TRACE");
					undSpTraceN = e ? std::atoi(e) : 0;
				}
				if (undSpTraceN > 0) {
					uint32_t curUndSp = (currentBank() == UndBank)
						? GPRs[13]
						: allModesBankedRegisters[UndBank][0];
					if (curUndSp != lastUndSp) {
						lastUndSp = curUndSp;
						undSpTraceN--;
						log("[und-sp] new UND_SP=%08x curBank=%d cpsr=%02x pc=%08x lr=%08x",
							curUndSp, currentBank(), CPSR & 0x1F, cspc, GPRs[14]);
					}
				}
			}
			// PSION_S5_MMU_PROBE=1 — at PC=0x5004C36C (LDR R0, [R4, #0x48]
			// inside FUN_5004C360, just before the LDR executes), log
			// virt→phys translation AND the value at the phys address
			// via both readVirtualDebug and a direct physical read.
			// If they disagree, there's an MMU-translation bug.
			if (cspc == 0x5004C36Cu && PSION_ENV_BOOL("PSION_S5_MMU_PROBE")) {
				static int mmuProbeN = -1;
				if (mmuProbeN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_MMU_PROBE");
					mmuProbeN = (e && e[0]) ? std::atoi(e) : 5;
				}
				if (mmuProbeN > 0) {
					mmuProbeN--;
					uint32_t va = GPRs[4] + 0x48;
					auto debugVal = readVirtualDebug(va, V32);
					auto ph = virtToPhys(va);
					uint32_t physVal = 0xDEADBEEF;
					if (ph.has_value()) {
						auto p = readPhysical(ph.value(), V32);
						if (p.has_value()) physVal = p.value();
					}
					log("[mmu-probe] cpsr=%02x R4=%08x va=%08x phys=%s debugRead=%s physRead=%08x",
						CPSR & 0x1F, GPRs[4], va,
						ph.has_value() ? std::to_string(ph.value()).c_str() : "?",
						debugVal.has_value() ? std::to_string(debugVal.value()).c_str() : "?",
						physVal);
				}
			}
			// PSION_S5_WRITER_TRACE=N — log all the destructive-cycle
			// writers to *(boot_thread+0x40) at function entries
			// (PC=0x500125E8, 0x5002DF60, 0x5002E92C, 0x5003968C).
			// For each, log SP + R4 + cpsr to verify which mode/SP
			// is doing the corrupting push.
			if (cspc == 0x500125E8u || cspc == 0x5002DF60u
			    || cspc == 0x5002E92Cu || cspc == 0x5003968Cu) {
				static int writerTraceN = -1;
				if (writerTraceN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_WRITER_TRACE");
					writerTraceN = e ? std::atoi(e) : 0;
				}
				if (writerTraceN > 0) {
					writerTraceN--;
					log("[writer-trace] pc=%08x cpsr=%02x sp=%08x R4=%08x lr=%08x",
						cspc, CPSR & 0x1F, GPRs[13], GPRs[4], GPRs[14]);
				}
			}
			if (cspc == 0x50010EA0u) {
				static int spTraceN = -1;
				if (spTraceN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_SP_TRACE");
					spTraceN = e ? std::atoi(e) : 0;
				}
				if (spTraceN > 0) {
					spTraceN--;
					BankIndex curBank = currentBank();
					log("[sp-trace] curBank=%d cpsr=%02x liveSP=%08x | banked: FIQ=%08x IRQ=%08x SVC=%08x ABT=%08x UND=%08x MAIN=%08x newThread=R0=%08x",
						curBank, CPSR & 0x1F, GPRs[13],
						allModesBankedRegisters[FiqBank][0],
						allModesBankedRegisters[IrqBank][0],
						allModesBankedRegisters[SvcBank][0],
						allModesBankedRegisters[AbtBank][0],
						allModesBankedRegisters[UndBank][0],
						allModesBankedRegisters[MainBank][0],
						GPRs[0]);
				}
			}
			if (cspc == 0x500111C4u && GPRs[0] == 0x80006900u) {
				static int dtorTraceN = -1;
				if (dtorTraceN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_DTOR_TRACE");
					dtorTraceN = e ? std::atoi(e) : 0;
				}
				if (dtorTraceN > 0) {
					dtorTraceN--;
					log("[dtor-entry] cpsr=%02x R0=%08x R1=%08x lr=%08x sp=%08x",
						CPSR & 0x1F, GPRs[0], GPRs[1], GPRs[14], GPRs[13]);
					log("[dtor-entry] PC history (oldest -> newest):");
					logPcHistory();
				}
			}
			if (cspc == 0x50009628u) {
				static int leaveTraceN = -1;
				if (leaveTraceN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_TRAP_LEAVE_TRACE");
					leaveTraceN = e ? std::atoi(e) : 0;
				}
				if (leaveTraceN > 0) {
					if (GPRs[3] != 0) {  // only log when Leave occurred
						leaveTraceN--;
						log("[trap-leave] LEAVE in session-create: code=%d (0x%x) thread(R4)=%08x lr=%08x sp=%08x",
							(int32_t)GPRs[3], GPRs[3], GPRs[4], GPRs[14], GPRs[13]);
					}
				}
			}
			if (cspc == 0x5004C36Cu) {
				static int failDispN = -1;
				if (failDispN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_FAILING_DISP");
					failDispN = e ? std::atoi(e) : 0;
				}
				if (failDispN > 0) {
					auto v48 = readVirtualDebug(GPRs[4] + 0x48, V32);
					// Filter to the specific failing SP range observed
					// in the destroy-iter trace (sp=0x80003D60). Only
					// fires for the destructive-cycle's outer destroy.
					if (GPRs[13] >= 0x80003D00u && GPRs[13] <= 0x80003E00u
					    && (CPSR & 0x1F) != 0x10) {
						failDispN--;
						log("[fail-disp] cpsr=%02x R4=%08x *(R4+0x48)=%08x lr=%08x sp=%08x",
							CPSR & 0x1F, GPRs[4],
							v48.has_value() ? v48.value() : 0xFFFFFFFFu,
							GPRs[14], GPRs[13]);
					}
				}
			}
			// PSION_S5_ASSERT_TRACE=N — at PC=0x5002DE9C (entry of
			// the function that asserts *(R4+4) == 0 and panics
			// "E32USER-CBase 33" if not). Logs R0 (this pointer),
			// the value at *(R0+4) (the asserted field), and LR
			// (caller). Goal: identify what calls this function
			// and which object's +0x04 is non-zero in our boot.
			if (cspc == 0x5002DE9Cu) {
				static int assertTraceN = -1;
				if (assertTraceN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_ASSERT_TRACE");
					assertTraceN = e ? std::atoi(e) : 0;
				}
				if (assertTraceN > 0) {
					assertTraceN--;
					auto v04 = readVirtualDebug(GPRs[0] + 4, V32);
					auto v00 = readVirtualDebug(GPRs[0], V32);
					log("[assert-entry] cpsr=%02x this=%08x *(this+0)=%08x *(this+4)=%08x R1=%08x lr=%08x",
						CPSR & 0x1F, GPRs[0],
						v00.has_value() ? v00.value() : 0u,
						v04.has_value() ? v04.value() : 0u,
						GPRs[1], GPRs[14]);
					// When the failing case fires (this=0x80006900 with
					// *(this+4)==1), also dump PC history so we can see
					// the chain that reached the assert.
					if (GPRs[0] == 0x80006900u && v04.has_value() && v04.value() != 0) {
						log("[assert-entry] failing-case PC history (oldest -> newest):");
						logPcHistory();
					}
				}
			}
			// PSION_S5_PANIC_CALLSITE=N — at PC=0x5004BD44 (the SWI
			// 0xc00076 instruction). When R0=0x2F (User::Panic
			// raised through the SWI), log R14 (the BL-return
			// address = wrapper return) and the caller-of-wrapper
			// pulled from the stack frame [SP+8] (wrapper saved
			// LR_old). Goal: find the ROM PC that calls
			// User::Panic("E32USER-CBase", 33) on the boot thread
			// each cycle of the destructive recovery loop.
			if (cspc == 0x5004BD44u && GPRs[0] == 0x2Fu) {
				static int panicCsN = -1;
				if (panicCsN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_PANIC_CALLSITE");
					panicCsN = e ? std::atoi(e) : 0;
				}
				if (panicCsN > 0) {
					panicCsN--;
					// Stack layout at SWI inside wrapper-at-0x5003AD2C
					// (called by User::Panic at 0x5003F9D0 → 0x5003F9F0):
					//   [SP+0]:  0x80000001 (wrapper local)
					//   [SP+4]:  R4_old (wrapper STMFD)
					//   [SP+8]:  LR_old = 0x5003F9F4 (return into User::Panic)
					//   [SP+12]: User::Panic local0 = *R0 (cat hdr)
					//   [SP+16]: User::Panic local1 = caller's R2
					//   [SP+20]: User::Panic local2 = caller's R1
					//   [SP+24]: User::Panic local3 (unused)
					//   [SP+28]: User::Panic saved LR_caller — the actual
					//            ROM PC that invoked User::Panic.
					// 4 frames deep: wrapper → User::Panic → panic_thunk
					// → panic_helper_NN → ROM caller.
					// SWI SP + 28 = post-BL in thunk (0x5003BF34)
					// SWI SP + 36 = post-BL in panic_helper_NN
					// SWI SP + 56 = saved LR of panic_helper's caller
					//                 (= actual ROM panic source)
					auto upLR1 = readVirtualDebug(GPRs[13] + 28, V32);
					auto upLR2 = readVirtualDebug(GPRs[13] + 36, V32);
					auto upLR3 = readVirtualDebug(GPRs[13] + 56, V32);
					auto p2_1 = readVirtualDebug(GPRs[1] + 4, V32);
					log("[panic-callsite] cpsr=%02x sp=%08x thunkRet=%08x helperRet=%08x romCaller=%08x reason=%d",
						CPSR & 0x1F, GPRs[13],
						upLR1.has_value() ? upLR1.value() : 0u,
						upLR2.has_value() ? upLR2.value() : 0u,
						upLR3.has_value() ? upLR3.value() : 0u,
						p2_1.has_value() ? (int32_t)p2_1.value() : 0);
				}
			}
			// PSION_S5_DISP_TRACE=N — at FUN_500096CC (PC=0x500096CC),
			// the kernel-side dispatcher for SWI 0xc00076. Logs the
			// selector (R0), arg ptr (R1), and arg2 (R2) on entry,
			// and at the panic path (FUN_5000FF68 = PC=0x5000FF68)
			// logs the panic context. Goal: see whether selector 0x27
			// goes through the same path as 0x15/0x19/0x1B or diverts
			// to an early-exit/panic.
			if (cspc == 0x500096CCu || cspc == 0x5000FF68u) {
				static int dispTraceN = -1;
				if (dispTraceN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_DISP_TRACE");
					dispTraceN = e ? std::atoi(e) : 0;
				}
				if (dispTraceN > 0) {
					dispTraceN--;
					const char *label = (cspc == 0x500096CCu) ? "disp" : "panic";
					log("[%s] cpsr=%02x sel=R0=%08x R1=%08x R2=%08x R3=%08x lr=%08x",
						label, CPSR & 0x1F, GPRs[0], GPRs[1], GPRs[2], GPRs[3], GPRs[14]);
					if (cspc == 0x500096CCu) {
						// PSION_S5_DISP_TRACE also dumps iCurrentThread and
						// iCurrentThread->iSession (+0x2C) — the panic gate
						// FUN_500096CC checks at entry. When +0x2C==0 the
						// dispatcher panics with KERN-NO-SESSION reason=sel.
						auto cur = readVirtualDebug(0x8010061Cu, V32);
						if (cur.has_value() && cur.value()) {
							auto sess = readVirtualDebug(cur.value() + 0x2C, V32);
							log("[disp-ctx] iCurrentThread=0x%08x session(+0x2C)=0x%08x",
							    cur.value(),
							    sess.has_value() ? sess.value() : 0xDEADBEEFu);
						}
					}
					if (cspc == 0x5000FF68u) {
						// At panic raise: R0 = TDesC8 descriptor.
						// EPOC TDesC8 layout: word 0 has type (top
						// 4 bits) + length (low 28 bits). For TPtrC8
						// (type 1) word 1 is a pointer to the data.
						// For TBufC8 (type 0) the data follows the
						// header in-place.
						auto h = readVirtualDebug(GPRs[0], V32);
						if (h.has_value()) {
							uint32_t hdr = h.value();
							uint32_t typ = hdr >> 28;
							uint32_t len = hdr & 0x0FFFFFFFu;
							if (len > 64) len = 64;
							uint32_t dataAddr = GPRs[0] + 4;
							if (typ == 1) {  // TPtrC8 — fetch indirect pointer
								auto p = readVirtualDebug(GPRs[0] + 4, V32);
								if (p.has_value()) dataAddr = p.value();
							}
							char text[80] = {};
							size_t ti = 0;
							for (uint32_t i = 0; i < len && ti + 1 < sizeof(text); i++) {
								auto c = readVirtualDebug(dataAddr + i, V8);
								if (!c.has_value()) break;
								char ch = (char)(c.value() & 0xFF);
								text[ti++] = (ch >= 0x20 && ch < 0x7F) ? ch : '.';
							}
							text[ti] = 0;
							log("[panic-info] cpsr=%02x typ=%u len=%u dataAddr=%08x text='%s' reason=%d",
								CPSR & 0x1F, typ, len, dataAddr, text, (int32_t)GPRs[1]);
						}
					}
				}
			}
			// PSION_S5_LOCK_TRACE=N — at PC=0x5004BDA0 (SWI 0x8E wrapper,
			// the inline atomic-CAS call site reached from FUN_5003A79C's
			// fast lock acquire) and PC=0x5004BC20 (SWI 0xC0002A wrapper,
			// the slow path). Logs R0 + *(R0) + *(R0-4) + iCurrentThread.
			// Use this to determine whether the lock-count word the
			// atomic operates on is in the expected "free" state.
			if (cspc == 0x5004BDA0u || cspc == 0x5004BC20u) {
				static int lockTraceN = -1;
				if (lockTraceN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_LOCK_TRACE");
					lockTraceN = e ? std::atoi(e) : 0;
				}
				if (lockTraceN > 0) {
					lockTraceN--;
					auto cnt = readVirtualDebug(GPRs[0], V32);
					auto w0  = readVirtualDebug(GPRs[0] - 4, V32);
					auto cur = readVirtualDebug(0x8010061Cu, V32);
					const char *lbl = (cspc == 0x5004BDA0u) ? "lock-fast" : "lock-slow";
					log("[%s] R0=%08x *(R0)=%08x *(R0-4)=%08x cur=%08x lr=%08x cpsr=%02x",
					    lbl, GPRs[0],
					    cnt.has_value() ? cnt.value() : 0xDEADBEEFu,
					    w0.has_value() ? w0.value() : 0xDEADBEEFu,
					    cur.has_value() ? cur.value() : 0u,
					    GPRs[14], CPSR & 0x1F);
					// When the fast-lock acquire is going to FAIL (*(R0) != 1)
					// dump the recent PC history so we can identify the caller
					// of FUN_5003A79C and the kernel function that's locking
					// the wrong object.
					if (cspc == 0x5004BDA0u && cnt.has_value() && cnt.value() != 1) {
						log("[lock-fast-fail] recent PCs (oldest -> newest):");
						logPcHistory();
					}
				}
			}
			// PSION_S5_HEAP_REALLOC_TRACE=N — at PC=0x5003DDB8 (FUN_5003DDB8
			// entry, the heap reallocator). Logs R0 (heap ptr) + R1 + R2
			// + LR + iCurrentThread + CPSR. Use to see WHICH heap object
			// the kernel is operating on right before the fast-lock fails.
			if (cspc == 0x5003DDB8u) {
				static int hrTraceN = -1;
				if (hrTraceN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_HEAP_REALLOC_TRACE");
					hrTraceN = e ? std::atoi(e) : 0;
				}
				if (hrTraceN > 0) {
					hrTraceN--;
					auto cur = readVirtualDebug(0x8010061Cu, V32);
					log("[heap-realloc] R0(heap)=0x%08x R1=0x%08x R2=0x%08x lr=0x%08x cur=0x%08x cpsr=%02x",
					    GPRs[0], GPRs[1], GPRs[2], GPRs[14],
					    cur.has_value() ? cur.value() : 0u, CPSR & 0x1F);
				}
			}
			// PSION_S5_HEAP_TRACE=N — at PC=0x500031F0 (entry of SWI 0x6C
			// handler, "GetHeap": returns *(iCurrentThread+0x38)).
			// Also at PC=0x500031FC (handler exit, LDMFD SP!, {PC}) to
			// see the actual R0 being returned. Dumps iCurrentThread
			// + *(thread+0x34) + *(thread+0x38) + (at exit) R0.
			// PSION_S5_IRQ_TABLE_DUMP=N — at kernel-idle PC=0x5001AE04
			// (FUN_5001AE04, hit ~12.96M cycles into boot), dump the
			// kernel's IRQ handler table at *(0x801003AC) + i*4 for
			// i=0..15. This tells us which IRQ source maps to which
			// handler, and lets us identify the pen-down IRQ on Series 5.
			// PSION_S5_IRQ_TABLE_DUMP=N — at PC=0x5001AE04, dump up to N times.
			// PSION_S5_IRQ_DUMP_CYC=M — every M cycles, also dump. This catches
			// IRQ-table changes that happen long after first idle (e.g., the
			// touch / pen driver registering its handler late in boot, after
			// EFile mounts Z: and loads the device-driver DLLs).
			{
				static int irqTblN = -1;
				static int64_t irqDumpCyc = -1;
				static int64_t nextIrqDumpAt = 0;
				if (irqTblN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_IRQ_TABLE_DUMP");
					irqTblN = e ? std::atoi(e) : 0;
					const char *c = PSION_ENV_CSTR("PSION_S5_IRQ_DUMP_CYC");
					irqDumpCyc = c ? std::strtoll(c, nullptr, 0) : 0;
					if (irqDumpCyc > 0) nextIrqDumpAt = irqDumpCyc;
				}
				bool wantDump = false;
				if (cspc == 0x5001AE04u && irqTblN > 0) {
					irqTblN--;
					wantDump = true;
				}
				if (irqDumpCyc > 0 && (int64_t)insnCycleApprox >= nextIrqDumpAt) {
					nextIrqDumpAt = (int64_t)insnCycleApprox + irqDumpCyc;
					wantDump = true;
				}
				if (wantDump) {
					auto tblPtr = readVirtualDebug(0x801003ACu, V32);
					uint32_t tbl = tblPtr.has_value() ? tblPtr.value() : 0u;
					log("[irq-table] base=0x%08x", tbl);
					if (tbl) for (int i = 0; i < 16; i++) {
						auto h = readVirtualDebug(tbl + i*4, V32);
						uint32_t handler = h.has_value() ? h.value() : 0u;
						uint32_t vtable = 0, entry = 0;
						if (handler) {
							auto v = readVirtualDebug(handler + 4, V32);
							if (v.has_value()) {
								vtable = v.value();
								auto e = readVirtualDebug(vtable + 8, V32);
								if (e.has_value()) entry = e.value();
							}
						}
						const char *desc =
							i == 0 ? "EXTFIQ" :
							i == 1 ? "BLINT" :
							i == 2 ? "WEINT" :
							i == 3 ? "MCINT" :
							i == 4 ? "CSINT" :
							i == 5 ? "EINT1" :
							i == 6 ? "EINT2" :
							i == 7 ? "EINT3" :
							i == 8 ? "TC1OI" :
							i == 9 ? "TC2OI" :
							i == 0xA ? "RTCMI" :
							i == 0xB ? "TINT" :
							i == 0xC ? "UTXINT1" :
							i == 0xD ? "URXINT1" :
							i == 0xE ? "UMSINT" :
							i == 0xF ? "SSEOTI" : "?";
						log("[irq-table]   id=0x%X (%-7s) handler=0x%08x entry=0x%08x",
						    i, desc, handler, entry);
					}
				}
			}
			// PSION_S5_FRAME_TRACE=N — at PC=0x500031F4 (the BL inside the
			// SWI 0x6C handler, hit RIGHT before the IRQ that diverts the
			// handler in the failing case). The exception frame is at
			// iCurrentThread + 0xE4 per FUN_500193F8 (LDR LR=*(0x8010061C);
			// ADD LR, #0xE4; STMIA LR, {R0-R12}). Dumps savedR0 + savedPC.
			if (cspc == 0x500031F4u) {
				static int frN = -1;
				if (frN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_FRAME_TRACE");
					frN = e ? std::atoi(e) : 0;
				}
				if (frN > 0) {
					frN--;
					auto cur = readVirtualDebug(0x8010061Cu, V32);
					uint32_t t = cur.has_value() ? cur.value() : 0u;
					auto sR0 = readVirtualDebug(t + 0xE4, V32);
					auto sR1 = readVirtualDebug(t + 0xE8, V32);
					auto sPc = readVirtualDebug(t + 0x120, V32);
					// Also dump the IRQ-handler context pointer at 0x80100624
					// (= "iIrqThread" / per-CPU IRQ stack context). If the
					// IRQ frame uses a different context, watch IT for what
					// gets written to its savedR0 slot during the IRQ chain.
					auto irqPtr = readVirtualDebug(0x80100624u, V32);
					uint32_t irqCtx = irqPtr.has_value() ? irqPtr.value() : 0u;
					auto iR0 = readVirtualDebug(irqCtx + 0xE4, V32);
					auto iPc = readVirtualDebug(irqCtx + 0x120, V32);
					log("[frame] cur=0x%08x SWI_savedR0=0x%08x SWI_savedR1=0x%08x SWI_savedPC=0x%08x | IRQ_ctx@0x80100624=0x%08x IRQ_savedR0=0x%08x IRQ_savedPC=0x%08x",
					    t,
					    sR0.has_value() ? sR0.value() : 0xDEADBEEFu,
					    sR1.has_value() ? sR1.value() : 0xDEADBEEFu,
					    sPc.has_value() ? sPc.value() : 0xDEADBEEFu,
					    irqCtx,
					    iR0.has_value() ? iR0.value() : 0xDEADBEEFu,
					    iPc.has_value() ? iPc.value() : 0xDEADBEEFu);
				}
			}
			// PSION_S5_TIMER2_DEEP=N — at PC=0x5001DA8C (FUN_5001DA6C, the
			// real Timer 2 callback, just before LDR R0, [R4, #0] which
			// fetches the object pointer from *(0x80100C98)).
			if (cspc == 0x5001DA8Cu) {
				static int t2dN = -1;
				if (t2dN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_TIMER2_DEEP");
					t2dN = e ? std::atoi(e) : 0;
				}
				if (t2dN > 0) {
					t2dN--;
					auto p = readVirtualDebug(0x80100C98u, V32);
					uint32_t obj = p.has_value() ? p.value() : 0xDEADBEEFu;
					auto v18 = readVirtualDebug(obj + 0x18, V32);
					uint32_t vt = v18.has_value() ? v18.value() : 0xDEADBEEFu;
					auto e8 = readVirtualDebug(vt + 0x8, V32);
					uint32_t entry = e8.has_value() ? e8.value() : 0xDEADBEEFu;
					auto cur = readVirtualDebug(0x8010061Cu, V32);
					log("[timer2-deep] *(0x80100C98)=0x%08x *(obj+0x18)=0x%08x *(vt+8)=0x%08x cur=0x%08x lr=0x%08x cpsr=%02x",
					    obj, vt, entry,
					    cur.has_value() ? cur.value() : 0u,
					    GPRs[14], CPSR & 0x1F);
				}
			}
			// PSION_S5_TIMER2_TRACE=N — at PC=0x50017408 (Timer 2 handler
			// FUN_500173AC's BL via vtable indirect at 0x50026A9C with
			// R0 = some kernel object). Logs R0/R1/iCurrentThread so we
			// can trace which object the Timer 2 IRQ ultimately operates
			// on — and whether that object's heap pointer is what's wrong.
			if (cspc == 0x50017408u) {
				static int t2N = -1;
				if (t2N < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_TIMER2_TRACE");
					t2N = e ? std::atoi(e) : 0;
				}
				if (t2N > 0) {
					t2N--;
					auto cur = readVirtualDebug(0x8010061Cu, V32);
					// Also dump *(0x50400094), the function pointer the
					// indirect dispatch at 0x50026A9C reads.
					auto fp = readVirtualDebug(0x50400094u, V32);
					// And the object's first few words.
					auto obj0 = readVirtualDebug(GPRs[0], V32);
					auto obj4 = readVirtualDebug(GPRs[0] + 4, V32);
					log("[timer2-call] R0=0x%08x *R0=0x%08x *(R0+4)=0x%08x fp@0x50400094=0x%08x cur=0x%08x lr=0x%08x cpsr=%02x",
					    GPRs[0],
					    obj0.has_value() ? obj0.value() : 0xDEADBEEFu,
					    obj4.has_value() ? obj4.value() : 0xDEADBEEFu,
					    fp.has_value() ? fp.value() : 0xDEADBEEFu,
					    cur.has_value() ? cur.value() : 0u,
					    GPRs[14], CPSR & 0x1F);
				}
			}
			// PSION_S5_BANKED_SP=N — at PC=0x50019678 (IRQ vector entry),
			// dump all banked R13s so we can see the kernel's intended
			// SP_irq vs SP_svc/SP_und. If they overlap, that's the
			// underlying stack-layout problem.
			if (cspc == 0x50019678u) {
				static int bspN = -1;
				if (bspN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_BANKED_SP");
					bspN = e ? std::atoi(e) : 0;
				}
				if (bspN > 0) {
					bspN--;
					log("[banked-sp] cur(IRQ)=0x%08x svc=0x%08x und=0x%08x main=0x%08x fiq=0x%08x abt=0x%08x",
					    GPRs[13],
					    allModesBankedRegisters[SvcBank][0],
					    allModesBankedRegisters[UndBank][0],
					    allModesBankedRegisters[MainBank][0],
					    allModesBankedRegisters[FiqBank][0],
					    allModesBankedRegisters[AbtBank][0]);
				}
			}
			// PSION_S5_LDM_TRACE=N — at PC=0x50019888 (the IRQ-exit
			// LDMFD SP!, {R0-R3, R12, PC}^). Dumps the SP-relative stack
			// slots so we can see EXACTLY what the LDM is about to load.
			if (cspc == 0x50019888u) {
				static int ldmN = -1;
				if (ldmN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_LDM_TRACE");
					ldmN = e ? std::atoi(e) : 0;
				}
				if (ldmN > 0) {
					ldmN--;
					uint32_t sp = GPRs[13];
					auto v0  = readVirtualDebug(sp + 0x00, V32);
					auto v4  = readVirtualDebug(sp + 0x04, V32);
					auto v8  = readVirtualDebug(sp + 0x08, V32);
					auto v12 = readVirtualDebug(sp + 0x0C, V32);
					auto v16 = readVirtualDebug(sp + 0x10, V32);
					auto v20 = readVirtualDebug(sp + 0x14, V32);
					log("[irq-ldm] sp=0x%08x [+0]=0x%08x [+4]=0x%08x [+8]=0x%08x [+12]=0x%08x [+16]=0x%08x [+20]=0x%08x",
					    sp,
					    v0.value_or(0xDEAD),
					    v4.value_or(0xDEAD),
					    v8.value_or(0xDEAD),
					    v12.value_or(0xDEAD),
					    v16.value_or(0xDEAD),
					    v20.value_or(0xDEAD));
				}
			}
			// PSION_S5_IRQ_TRACE=N — at PC=0x50016FD0 (FUN_50016FD0 entry,
			// the IRQ-by-id dispatcher called from FUN_5001A770's loop).
			// Logs R0 (the IRQ ID byte from the priority-encoder tables)
			// + iCurrentThread + the handler object pointer it'll dispatch
			// through. Combine with PSION_S5_HEAP_TRACE_ALL to identify
			// which IRQ fires inside the SWI 0x6C handler at the failing
			// cycle.
			if (cspc == 0x50016FD0u) {
				static int irqTraceN = -1;
				if (irqTraceN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_IRQ_TRACE");
					irqTraceN = e ? std::atoi(e) : 0;
				}
				if (irqTraceN > 0) {
					irqTraceN--;
					auto cur = readVirtualDebug(0x8010061Cu, V32);
					auto tblPtrAddr = readVirtualDebug(0x801003ACu, V32);
					uint32_t handler = 0xDEADBEEFu;
					uint32_t vtable = 0xDEADBEEFu;
					uint32_t entry = 0xDEADBEEFu;
					if (tblPtrAddr.has_value() && tblPtrAddr.value()) {
						auto h = readVirtualDebug(tblPtrAddr.value() + (GPRs[0] << 2), V32);
						if (h.has_value()) {
							handler = h.value();
							auto vt = readVirtualDebug(handler + 4, V32);
							if (vt.has_value()) {
								vtable = vt.value();
								auto e = readVirtualDebug(vtable + 8, V32);
								if (e.has_value()) entry = e.value();
							}
						}
					}
					// Also dump *(handler+0), *(handler+8), *(handler+0x18) — fields
					// used by the Timer 2 handler chain (FUN_5001A904).
					uint32_t h0 = 0xDEADBEEFu, h8 = 0xDEADBEEFu, h18 = 0xDEADBEEFu;
					if (handler != 0xDEADBEEFu) {
						auto v0 = readVirtualDebug(handler, V32);
						auto v8 = readVirtualDebug(handler + 8, V32);
						auto v18 = readVirtualDebug(handler + 0x18, V32);
						if (v0.has_value()) h0 = v0.value();
						if (v8.has_value()) h8 = v8.value();
						if (v18.has_value()) h18 = v18.value();
					}
					log("[irq] id=0x%02x cur=0x%08x handler=0x%08x *h=0x%08x *h+8=0x%08x *h+0x18=0x%08x entry=0x%08x lr=0x%08x cpsr=%02x",
					    GPRs[0],
					    cur.has_value() ? cur.value() : 0u,
					    handler, h0, h8, h18, entry,
					    GPRs[14], CPSR & 0x1F);
				}
			}
			// PSION_S5_HEAP_TRACE_ALL — at PC=0x500031F4 / 0x500031F8 / 0x500031FC,
			// IRQ vector entry — virt 0x18 in the high vectors mapping, OR
			// wherever the kernel maps it. In Series 5 the kernel relocates
			// vectors via MMU. Hook at PC=0x18 to see if we ever take the
			// IRQ vector — but more productively, hook at the kernel's IRQ
			// handler entry (which is FUN_500193F8 for SWI). Look for the
			// IRQ vector branch target by scanning for "SUB LR, LR, #4"
			// patterns at low addresses.
			//
			// For now, hook PC=0x500031F4 + dump the next-PC after the BL
			// fully executes. We can compare to expected 0x5000FF40 (target).
			if (cspc == 0x500031F4u || cspc == 0x500031F8u || cspc == 0x500031FCu) {
				static int hxAll = -1;
				if (hxAll < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_HEAP_TRACE_ALL");
					hxAll = e ? std::atoi(e) : 0;
				}
				if (hxAll > 0) {
					hxAll--;
					log("[heap-getter-step-%02x] R0=0x%08x lr=0x%08x cpsr=%02x",
					    cspc & 0xFF, GPRs[0], GPRs[14], CPSR & 0x1F);
				}
			}
			// (Removed older 0x500031F8 hook in favour of the multi-PC step
			// trace above — keep cspc == 0x500031F8u below for the original
			// pre-load value capture but only if PSION_S5_HEAP_TRACE is set.)
			if (cspc == 0x500031F8u) {
				static int hxN = -1;
				if (hxN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_HEAP_TRACE");
					hxN = e ? std::atoi(e) : 0;
				}
				if (hxN > 0) {
					hxN--;
					auto val = readVirtualDebug(GPRs[0] + 0x38, V32);
					log("[heap-getter-pre-load] R0(thread)=0x%08x *(R0+0x38)=0x%08x lr=0x%08x cpsr=%02x",
					    GPRs[0],
					    val.has_value() ? val.value() : 0xDEADBEEFu,
					    GPRs[14], CPSR & 0x1F);
				}
			}
			if (cspc == 0x500031F0u) {
				static int heapTraceN = -1;
				if (heapTraceN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_HEAP_TRACE");
					heapTraceN = e ? std::atoi(e) : 0;
				}
				if (heapTraceN > 0) {
					heapTraceN--;
					auto cur = readVirtualDebug(0x8010061Cu, V32);
					uint32_t t = cur.has_value() ? cur.value() : 0u;
					auto h34 = readVirtualDebug(t + 0x34, V32);
					auto h38 = readVirtualDebug(t + 0x38, V32);
					log("[heap-getter] cur=0x%08x *(cur+0x34)=0x%08x *(cur+0x38)=0x%08x lr=0x%08x",
					    t,
					    h34.has_value() ? h34.value() : 0xDEADBEEFu,
					    h38.has_value() ? h38.value() : 0xDEADBEEFu,
					    GPRs[14]);
				}
			}
			// PSION_S5_SR_RETURN_TRACE=N — at PC=0x5003AD10 (CMP R0, #0
			// right after SWI 0xc00076 returns). Logs R0 (return code)
			// and the contents of [SP] (which holds the result slot
			// the SWI was supposed to populate). If R0 != 0 → SWI
			// failed; if R0 == 0 but [SP] still equals 0x80000001 →
			// kernel claimed it succeeded but never wrote back.
			//
			// Same hook fires at the 4 caller-side BL-return PCs
			// (lr values for selectors 0x15/0x19/0x1B/0x27) so we
			// can see whether the WRAPPER returns OR whether some
			// caller's epilogue is the one that doesn't run.
			if (cspc == 0x5003AD10u || cspc == 0x5003EF3Cu ||
			    cspc == 0x5003F20Cu || cspc == 0x5003F29Cu ||
			    cspc == 0x5003F670u) {
				static int srRetTraceN = -1;
				if (srRetTraceN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_SR_RETURN_TRACE");
					srRetTraceN = e ? std::atoi(e) : 0;
				}
				if (srRetTraceN > 0) {
					srRetTraceN--;
					auto slot = readVirtualDebug(GPRs[13], V32);
					char b[16];
					if (slot.has_value()) std::snprintf(b, sizeof(b), "%08x", slot.value()); else std::snprintf(b, sizeof(b), "??");
					const char *label =
						(cspc == 0x5003AD10u) ? "wrap-ret"
						: (cspc == 0x5003EF3Cu) ? "caller15-ret"
						: (cspc == 0x5003F20Cu) ? "caller19-ret"
						: (cspc == 0x5003F29Cu) ? "caller1B-ret"
						: "caller27-ret";
					log("[sr-ret %s] cpsr=%02x R0=%08x slot[SP]=%s sp=%08x",
						label, CPSR & 0x1F, GPRs[0], b, GPRs[13]);
				}
			}
			// PSION_S5_INJECT=spec;spec;... — generic hypothesis testing.
			// Each spec is one of:
			//   poke:ADDR:VAL          write VAL (V32) to ADDR once at first tick
			//   pokepc:PC:ADDR:VAL     write VAL (V32) to ADDR when PC executes
			//   ret:PC:R0VAL           when PC reached, R0=R0VAL and PC:=LR
			//   skip:PC                when PC reached, PC:=PC+4
			//   force_r0:PC:VAL        when PC reached, R0:=VAL (continue)
			// Numbers can be decimal or 0x-prefixed hex. Each rule fires
			// up to 8 times (bound runaway behaviour).
			{
				struct InjectRule {
					enum { POKE, POKEPC, RET, SKIP, FORCE_R0 } kind;
					uint32_t pc;
					uint32_t addr;
					uint32_t value;
					int fired;
					int maxFires;
				};
				static std::vector<InjectRule> injectRules;
				static bool injectInited = false;
				if (!injectInited) {
					injectInited = true;
					if (const char *spec = PSION_ENV_CSTR("PSION_S5_INJECT")) {
						std::string s = spec;
						size_t i = 0;
						while (i < s.size()) {
							size_t j = s.find(';', i);
							if (j == std::string::npos) j = s.size();
							std::string item = s.substr(i, j - i);
							i = j + 1;
							std::vector<std::string> parts;
							size_t k = 0;
							while (k < item.size()) {
								size_t l = item.find(':', k);
								if (l == std::string::npos) l = item.size();
								parts.push_back(item.substr(k, l - k));
								k = l + 1;
							}
							if (parts.empty()) continue;
							auto parseU = [](const std::string &x) -> uint32_t {
								return (uint32_t)std::strtoul(x.c_str(), nullptr, 0);
							};
							InjectRule r{};
							r.fired = 0;
							r.maxFires = 8;
							if (parts[0] == "poke" && parts.size() >= 3) {
								r.kind = InjectRule::POKE;
								r.addr = parseU(parts[1]);
								r.value = parseU(parts[2]);
								if (parts.size() >= 4) r.maxFires = parseU(parts[3]);
							} else if (parts[0] == "pokepc" && parts.size() >= 4) {
								r.kind = InjectRule::POKEPC;
								r.pc = parseU(parts[1]);
								r.addr = parseU(parts[2]);
								r.value = parseU(parts[3]);
								if (parts.size() >= 5) r.maxFires = parseU(parts[4]);
							} else if (parts[0] == "ret" && parts.size() >= 3) {
								r.kind = InjectRule::RET;
								r.pc = parseU(parts[1]);
								r.value = parseU(parts[2]);
								if (parts.size() >= 4) r.maxFires = parseU(parts[3]);
							} else if (parts[0] == "skip" && parts.size() >= 2) {
								r.kind = InjectRule::SKIP;
								r.pc = parseU(parts[1]);
								if (parts.size() >= 3) r.maxFires = parseU(parts[2]);
							} else if (parts[0] == "force_r0" && parts.size() >= 3) {
								r.kind = InjectRule::FORCE_R0;
								r.pc = parseU(parts[1]);
								r.value = parseU(parts[2]);
								if (parts.size() >= 4) r.maxFires = parseU(parts[3]);
							} else {
								log("[inject] BAD SPEC: %s", item.c_str());
								continue;
							}
							injectRules.push_back(r);
							log("[inject] rule loaded: %s", item.c_str());
						}
					}
				}
				for (auto &r : injectRules) {
					if (r.fired >= r.maxFires) continue;
					bool fire = false;
					if (r.kind == InjectRule::POKE) {
						if (r.fired == 0) fire = true;
					} else if (cspc == r.pc) {
						fire = true;
					}
					if (!fire) continue;
					r.fired++;
					switch (r.kind) {
					case InjectRule::POKE:
					case InjectRule::POKEPC:
						writeVirtual(r.value, r.addr, V32);
						log("[inject] %s addr=%08x val=%08x (fire %d/%d)",
							r.kind == InjectRule::POKE ? "POKE" : "POKEPC",
							r.addr, r.value, r.fired, r.maxFires);
						break;
					case InjectRule::RET:
						GPRs[0] = r.value;
						GPRs[15] = GPRs[14];
						prefetchCount = 0;
						log("[inject] RET pc=%08x r0=%08x lr=%08x (fire %d/%d)",
							r.pc, r.value, GPRs[14], r.fired, r.maxFires);
						break;
					case InjectRule::SKIP:
						GPRs[15] = cspc + 4;
						prefetchCount = 0;
						log("[inject] SKIP pc=%08x next=%08x (fire %d/%d)",
							r.pc, cspc + 4, r.fired, r.maxFires);
						break;
					case InjectRule::FORCE_R0:
						GPRs[0] = r.value;
						log("[inject] FORCE_R0 pc=%08x r0=%08x (fire %d/%d)",
							r.pc, r.value, r.fired, r.maxFires);
						break;
					}
				}
			}
			// PSION_S5_PT_ENTRY_TRACE=N — at SWI 0x73 PopTrap handler
			// entry (PC=0x5000CD78), log LR + stack contents. Goal:
			// find the upstream caller responsible for LR=0xBBBBBBBB.
			// Only one ROM ref to 0x5000CD78 exists: function-pointer
			// entry at 0x50027B28 (= FAST exec table[0x73] per dispatcher
			// at 0x500194DC). So entry is via SWI dispatch — LR at entry
			// is whatever the SWI-issuing code had in LR.
			// PSION_S5_MATCHF_BLSITE_TRACE=N — at MatchF entry (PC=0x5000C208),
			// when LR == 0x5000CD80, decode the instruction at LR-4 to see
			// what BL'd here. Goal: identify the actual call site that ends
			// up returning to 0x5000CD80 (not necessarily the static
			// 0x5000CD7C BL).
			if (cspc == 0x5000C208u && GPRs[14] == 0x5000CD80u) {
				static int blsTraceN = -1;
				if (blsTraceN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_MATCHF_BLSITE_TRACE");
					blsTraceN = e ? std::atoi(e) : 0;
				}
				if (blsTraceN > 0) {
					blsTraceN--;
					// Read the instruction at LR-4 (the BL site).
					auto blInsn = readVirtualDebug(0x5000CD7Cu, V32);
					log("[matchf-blsite] LR=0x5000CD80 → BL site instr at 0x5000CD7C = %08x",
						blInsn.value_or(0));
					log("[matchf-blsite]   r0=%08x r1=%08x sp=%08x cpsr=%02x",
						GPRs[0], GPRs[1], GPRs[13], CPSR & 0x1F);
					// PC history of the cycles leading up to this point.
					logPcHistory();
				}
			}
			if (cspc == 0x5000CD78u) {
				static int ptTraceN = -1;
				if (ptTraceN < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_PT_ENTRY_TRACE");
					ptTraceN = e ? std::atoi(e) : 0;
				}
				// Filter: only log "interesting" entries where LR or
				// iCurrentThread looks bad. Skips the thousands of
				// normal entries with LR=0x500194FC + valid iCurrentThread.
				bool badLr = (GPRs[14] & 0xF0000000u) != 0x50000000u;
				auto cur = readVirtualDebug(0x8010061Cu, V32);
				bool badCur = !cur.has_value() ||
				              cur.value() == 0xFFFFFFFFu ||
				              (cur.value() != 0 &&
				               (cur.value() & 0xF0000000u) != 0x80000000u);
				if (ptTraceN > 0 && (badLr || badCur)) {
					ptTraceN--;
					log("[pt-entry-bad] PC=0x5000CD78 LR=%08x SP=%08x cpsr=%02x cur=%08x",
						GPRs[14], GPRs[13], CPSR & 0x1F, cur.value_or(0));
					for (int i = 0; i < 12; i++) {
						uint32_t a = GPRs[13] + i*4;
						auto v = readVirtualDebug(a, V32);
						log("[pt-entry-bad]   sp+%02x @ %08x = %08x",
							i*4, a, v.value_or(0xDEADBEEFu));
					}
					logPcHistory();
				}
			}
			// Always log the fault-imminent case (R2 == 0xFFFFFFFF at the
			// LDR), independent of the regular budget. This is the
			// pathological case we're hunting.
			if (cspc == 0x5000CD84u && GPRs[2] == 0xFFFFFFFFu) {
				static int faultLogN = 5;
				if (faultLogN > 0) {
					faultLogN--;
					log("[callsite-fault] FAULT IMMINENT at PC=0x5000CD84 (R2=-1):");
					log("[callsite-fault]   r0=%08x r1=%08x r2=%08x r3=%08x",
						GPRs[0], GPRs[1], GPRs[2], GPRs[3]);
					log("[callsite-fault]   r4=%08x r5=%08x r6=%08x r7=%08x",
						GPRs[4], GPRs[5], GPRs[6], GPRs[7]);
					log("[callsite-fault]   r8=%08x r9=%08x r10=%08x r11=%08x",
						GPRs[8], GPRs[9], GPRs[10], GPRs[11]);
					log("[callsite-fault]   r12=%08x sp=%08x lr=%08x cpsr=%08x",
						GPRs[12], GPRs[13], GPRs[14], CPSR);
					log("[callsite-fault]   PC history (oldest → newest):");
					logPcHistory();
				}
			}
			if (callsiteLogN > 0 &&
			    (cspc == 0x5000CD7Cu || cspc == 0x5000CD80u || cspc == 0x5000CD84u
			     || cspc == 0x5000FF40u || cspc == 0x5000FF44u || cspc == 0x5000FF48u
			     || cspc == 0x5000C208u || cspc == 0x5000C2A4u || cspc == 0x5000C414u)) {
				callsiteLogN--;
				log("[callsite] pc=%08x r0=%08x r2=%08x lr=%08x cpsr=%x",
					cspc, GPRs[0], GPRs[2], GPRs[14], CPSR & 0x1F);
			}
			// PSION_S5_FIX_SCHED_RESTORE — at the scheduler context-restore
			// LDM at PC=0x50019828, replace -1 R0..R3 values with 0. The
			// kernel fills the SVC stack region with 0xFFFFFFFF at boot
			// (canary, see PC=0x5000063C); the hypothesis was that an
			// uninitialised NThread+0xE4 block propagates -1 through the
			// scheduler.
			//
			// EXPERIMENT RESULT (2026-05-06): the patch never fires. PC
			// 0x5001982C is NOT in the fault path. Verified: the R0=-1 at
			// PC=0x5000CD80 comes from MatchF's MVN R0,#0 at 0x5000C2A4
			// (its "no match" return), NOT from a scheduler restore.
			//
			// Kept as opt-in for future investigators in case some other
			// scheduler-restore path matters. Default OFF.
			if (series5HalFix && cspc == 0x5001982Cu) {
				static int fixSched = -1;
				if (fixSched < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_FIX_SCHED_RESTORE");
					fixSched = (e && e[0] == '1') ? 1 : 0;
				}
				if (fixSched) {
					int patched = 0;
					if (GPRs[0] == 0xFFFFFFFFu) { GPRs[0] = 0; patched |= 1; }
					if (GPRs[1] == 0xFFFFFFFFu) { GPRs[1] = 0; patched |= 2; }
					if (GPRs[2] == 0xFFFFFFFFu) { GPRs[2] = 0; patched |= 4; }
					if (GPRs[3] == 0xFFFFFFFFu) { GPRs[3] = 0; patched |= 8; }
					if (patched) {
						static int fixCount = 0;
						if (++fixCount <= 8)
							log("[fix-sched] PC=0x5001982C patched R0..R3 mask=0x%x (fix %d/8)",
								patched, fixCount);
					}
				}
			}
			// Series 5 deterministic fault-loop break-out (default ON).
			//
			// Function at 0x5000CD78 disassembles to:
			//   0x5000CD78: STMFD SP!, {LR}
			//   0x5000CD7C: BL ...                  ; returns -1 on no-match
			//   0x5000CD80: MOV R2, R0              ; R2 = result
			//   0x5000CD84: LDR R0, [R2, #0x3C]     ; FAULTS if R2 = -1
			//   0x5000CD88: CMP R0, #0
			//   0x5000CD8C: LDRNE R3, [R0, #0x40]
			//   0x5000CD90: STRNE R3, [R2, #0x3C]
			//   0x5000CD94: LDMFD SP!, {PC}         ; return via stacked LR
			// It's a "pop-the-head-of-a-kernel-object-chain" helper — it
			// dereferences the lookup result without a -1 guard. Real
			// hardware doesn't take this path because the lookup succeeds;
			// in our emulator missing kernel-side state makes it return -1.
			//
			// IMPORTANT: this patch is DIAGNOSTIC, not a real fix. The bug
			// is structurally upstream. Tracing the LDMFD at 0x5000CD94
			// shows it pops 0xBBBBBBBB from the saved-LR slot — meaning
			// the STMFD at 0x5000CD78 was entered with LR = 0xBBBBBBBB
			// already. Whoever called this function had a corrupt LR.
			// So either patch (R0=iCurrentThread to satisfy the LDR, OR
			// jump to the epilogue to skip the body) trips over the bad
			// stacked LR a few instructions later, yielding the same
			// number of trap events — just at different PCs.
			//
			// We keep the R0=iCurrentThread variant because the resulting
			// LDR succeeds and the kernel explores more downstream code
			// (132 unique PCs at 15 s vs 118 with the skip-to-epilogue
			// variant) before tripping on the bad LR. More PC coverage
			// gives later investigation more material to work with.
			//
			// PSION_S5_BREAK_FAULT_LOOP=0 disables for diagnosis.
			// 2026-05-11: defaulted OFF. This was a workaround for the
			// destructive abort cycle. With the I-bit fix
			// (commit eddf64b7) the cycle no longer fires; runtime trace
			// during a 15-sim-sec boot shows ZERO firings, but the hack
			// was still default-ON and could fire post-boot when the OS
			// hands off to applications. Keeping it active risks
			// corrupting kernel state that's now legitimately reached.
			// Set PSION_S5_BREAK_FAULT_LOOP=1 to re-enable for historical
			// comparisons.
			if (series5HalFix && cspc == 0x5000CD80u && GPRs[0] == 0xFFFFFFFFu) {
				static int breakLoop = -1;
				if (breakLoop < 0) {
					const char *e = PSION_ENV_CSTR("PSION_S5_BREAK_FAULT_LOOP");
					breakLoop = (e && e[0] == '1') ? 1 : 0;  // default OFF
				}
				if (breakLoop) {
					static int allHits = 0;
					if (allHits++ < 3) log("[hack-break-fault-loop] fired (R0 was -1)");
					auto cur = readVirtualDebug(0x8010061Cu, V32);
					if (cur.has_value() && cur.value() != 0 &&
					    cur.value() != 0xFFFFFFFFu) {
						static int patchCount = 0;
						if (++patchCount <= 8)
							log("[break-loop] PC=0x5000CD80 R0=-1 → patch R0 = iCurrentThread (0x%08x), patch #%d",
								cur.value(), patchCount);
						GPRs[0] = cur.value();
					}
				}
			}
		}
		// PSION_S5_MATCHF_FAKE_EFILE=1 — when MatchF is called with the
		// EFile.exe lookup pattern (header type=3, contains "EFile["),
		// short-circuit MatchF to return 0 (= success / match) instead of
		// running the loop. This tests whether the destructive recovery
		// cycle collapses when the kernel's EFile lookup succeeds on
		// first try (as it would on real hardware where EFile is already
		// registered).
		if (series5HalFix && (GPRs[15] - 0xC) == 0x5000C208u) {
			static int fakeEfile = -1;
			if (fakeEfile < 0) {
				const char *e = PSION_ENV_CSTR("PSION_S5_MATCHF_FAKE_EFILE");
				fakeEfile = (e && std::atoi(e) != 0) ? 1 : 0;
			}
			if (fakeEfile) {
				// Decode pattern (R1). If first 6 bytes of data spell "EFile[",
				// short-circuit return 0 (match success).
				uint32_t pat = GPRs[1];
				if (pat >= 0x40000000u) {
					auto hdr = readVirtualDebug(pat, V32);
					if (hdr.has_value()) {
						uint32_t header = hdr.value();
						uint32_t type = (header >> 28) & 0xF;
						// EKA1 TDesC8 layouts (per readback at known descriptors):
						//   type 0 (TBufC8): hdr | inline-data  (data at desc+4)
						//   type 1 (TPtrC8): hdr | iPtr         (deref desc+4)
						//   type 2 (TBuf8):  hdr | iMaxLen | inline-data  (data at desc+8)
						//   type 3 (TPtr8):  hdr | iMaxLen | iPtr         (deref desc+8)
						uint32_t dataPtr = pat + 4;
						if (type == 1) {
							if (auto p = readVirtualDebug(pat + 4, V32); p.has_value())
								dataPtr = p.value();
						} else if (type == 2) {
							dataPtr = pat + 8;
						} else if (type == 3) {
							dataPtr = pat + 8;
						}
						char name[8] = {};
						for (int k = 0; k < 6; k++) {
							auto b = readVirtualDebug(dataPtr + k, V8);
							name[k] = (char)b.value_or(0);
						}
						if (std::memcmp(name, "EFile[", 6) == 0) {
							static int firedCount = 0;
							if (++firedCount <= 3)
								log("[matchf-fake] forcing R0=0 for EFile lookup (call #%d)", firedCount);
							GPRs[0] = 0;
							// Return immediately (skip MatchF body)
							GPRs[15] = GPRs[14] | (GPRs[15] & 1);  // PC = LR
							prefetchCount = 0;
							return 1;
						}
					}
				}
			}
		}
		// PSION_S5_MATCHF_TRACE=N — at FUN_5000C208 entry (Symbian
		// MatchF wildcard pattern matcher), decode and log the candidate
		// (R0) and pattern (R1) descriptors. Both are TDesC8: header word
		// (length in low 28 bits, type in top 4 bits) followed by data.
		// MatchF returns -1 from PC=0x5000C2A4 when no match found, which
		// drives the destructive recovery cycle. Logging the names tells
		// us what the kernel is looking for that we're failing to provide.
		if (series5HalFix && (GPRs[15] - 0xC) == 0x5000C208u) {
			static int matchfTraceN = -1;
			if (matchfTraceN == -1) {
				const char *e = PSION_ENV_CSTR("PSION_S5_MATCHF_TRACE");
				matchfTraceN = e ? std::atoi(e) : 0;
			}
			if (matchfTraceN > 0) {
				matchfTraceN--;
				auto decode = [&](uint32_t descPtr, char *buf, size_t bufLen) -> uint32_t {
					if (!descPtr || descPtr < 0x40000000u) {
						std::snprintf(buf, bufLen, "<invalid:0x%08x>", descPtr);
						return 0;
					}
					auto hdr = readVirtualDebug(descPtr, V32);
					if (!hdr.has_value()) {
						std::snprintf(buf, bufLen, "<unmapped:0x%08x>", descPtr);
						return 0;
					}
					uint32_t header = hdr.value();
					uint32_t length = header & 0x0FFFFFFFu;
					uint32_t type   = (header >> 28) & 0xF;
					// Type 0 = TBufC8 (inline), 1 = TPtrC8 (out-of-line).
					// For type 0, data starts at descPtr+4. For type 1,
					// descPtr+4 holds a pointer to the data.
					uint32_t dataPtr = descPtr + 4;
					if (type == 1) {
						if (auto p = readVirtualDebug(descPtr + 4, V32); p.has_value())
							dataPtr = p.value();
					} else if (type == 2 || type == 3) {
						dataPtr = descPtr + 8;
					}
					size_t toRead = (length < 96) ? length : 96;
					size_t pos = std::snprintf(buf, bufLen,
						"hdr=%08x type=%u len=%u data@%08x \"",
						header, type, length, dataPtr);
					for (size_t i = 0; i < toRead && pos + 4 < bufLen; i++) {
						auto b = readVirtualDebug(dataPtr + i, V8);
						uint8_t c = b.value_or('?');
						if (c >= 0x20 && c < 0x7F) buf[pos++] = (char)c;
						else                       buf[pos++] = '.';
					}
					if (pos + 1 < bufLen) buf[pos++] = '"';
					if (pos < bufLen)     buf[pos]   = 0;
					return length;
				};
				char candBuf[256] = {}, patBuf[256] = {};
				decode(GPRs[0], candBuf, sizeof(candBuf));
				decode(GPRs[1], patBuf,  sizeof(patBuf));
				log("[matchf] candidate=%s", candBuf);
				log("[matchf] pattern  =%s", patBuf);
				log("[matchf]   r0=%08x r1=%08x r2=%08x lr=%08x cpsr=%x",
					GPRs[0], GPRs[1], GPRs[2], GPRs[14], CPSR & 0x1F);
			}
		}
		if (insnFault != NoFault) {
			// Raise a prefetch error
			// These do not set FSR or FAR
			uint32_t faultAddr = insnFault >> MMUFaultAddressShift;
			// Cap the log flood once we're clearly stuck in a prefetch-abort loop
			// (e.g. an exception whose vector page is itself unmapped — the CPU
			// re-faults at 0x0C forever). Without this the 65-line-per-fault dump
			// evicts the *pre-crash* logs that show the real cause.
			static uint32_t lastPfAddr = 0xFFFFFFFFu;
			static int pfRepeat = 0;
			if (faultAddr == lastPfAddr) pfRepeat++; else { pfRepeat = 0; lastPfAddr = faultAddr; }
			const bool pfLog = pfRepeat < 3;
			if (pfRepeat == 3) {
				faultLoopDetected_ = true;   // worker polls this to auto-halt
				log("prefetch error! %08x — STUCK in fault loop (vector page unmapped?), suppressing further dumps", faultAddr);
				// Re-emit the *first* fault as the last line: in a bounded log
				// viewer the initial fault has long scrolled off, but its
				// FSR/DACR are what identify the crash.  FSR[3:0] fault type:
				// 0x5/0x7 = translation (stale TLB / bad page table), 0x9/0xB =
				// domain (DACR doesn't grant FSR[7:4]'s domain → garbage DACR
				// from a corrupt DProcess), 0xD/0xF = permission.
				if (firstFaultCaptured_)
					log("CRASH ROOT: first fault PC=%08x FAR=%08x FSR=%08x (type=%x dom=%x) DACR=%08x",
					    firstFaultPC_, firstFaultFAR_, firstFaultFSR_,
					    firstFaultFSR_ & 0xF, (firstFaultFSR_ >> 4) & 0xF, firstFaultDACR_);
			}
			if (pfLog)
			log("prefetch error! %08x", faultAddr);
			// PSION_S7_TRACE_RESCHED_FAULT: when the faulting fetch is in the
			// unmapped gap (0x60000000..0x7fffffff) — i.e. a CPSR-shaped value
			// loaded into PC by a bad context-restore — dump full register +
			// banked/SPSR state ONCE so we can pin which frame field it is.
			if (PSION_ENV_BOOL("PSION_S7_TRACE_RESCHED_FAULT")
					&& ((faultAddr >= 0x60000000u && faultAddr < 0x80000000u)
					    || (faultAddr >= 0x90000000u && faultAddr < 0xC0000000u))) {
				static int dumped = 0;
				if (dumped++ < 2) {
					for (int g = 0; g < 16; g += 4)
						log("[rfault] r%d-%d = %08x %08x %08x %08x",
						    g, g+3, GPRs[g], GPRs[g+1], GPRs[g+2], GPRs[g+3]);
					log("[rfault] CPSR=%08x mode=%x sp=%08x lr=%08x",
					    CPSR, CPSR & 0x1F, GPRs[13], GPRs[14]);
					for (int b = 0; b < 6; b++)
						log("[rfault] SPSR[bank%d]=%08x bankedSP=%08x bankedLR=%08x",
						    b, SPSRs[b], allModesBankedRegisters[b][0],
						    allModesBankedRegisters[b][1]);
				}
			}
			if (pfLog) logPcHistory();
			raiseException(Abort32, GPRs[15] - 8, 0xC);
		} else {
			// Series 5 EPOC R1: null-TDesC-pointer short-circuit.
			//
			// 0x50037A34 is a TDesC-type-dispatch helper: it reads *R0, takes
			// the upper 4 bits as a type tag, and either jump-tables to a
			// case-specific extractor (0..4) or panics with code 0x13. In
			// our emulation several HAL paths call it with R0 = NULL because
			// an earlier kernel global read (*0x8010061c) chained through
			// LDR [R0,#0x38] lands on an uninitialised field and returns 0.
			// The panic handler at 0x50043E98 then prints/logs, which
			// re-enters the TDesC pipeline with further null args; that
			// recurses ~120 times at ~0x78 bytes per frame, blowing out the
			// SVC stack (SP walks down from 0x80105764 through 0x80000000
			// and page-translate-faults at 0x7FFFFFFC).
			//
			// Upstream root cause is a missing kernel global initialisation
			// that we haven't located yet. Until we do, observe that the
			// fall-through arm of this dispatcher *already* returns R0 = 0
			// (the instructions at 0x50037A94-98 execute after the panic
			// handler returns), so we can safely short-circuit by turning
			// the whole call into "R0 = 0; PC = LR" when R0 is null on
			// entry. This matches the ABI of the native dispatcher's
			// error path, just without the 120-frame recursion.
			//
			// Kept behind series5HalFix so non-Series-5 devices are
			// unaffected.
			// 2026-05-11: gated behind PSION_S5_HAL_LEGACY (off by default)
			// — destructive-cycle workaround, obsolete with the I-bit fix.
			if (series5HalFix && (GPRs[0] == 0 || GPRs[0] == 0xFFFFFFFF) &&
			    (GPRs[15] - 0xC) == 0x50037A34
			    && PSION_ENV_BOOL("PSION_S5_HAL_LEGACY")) {
				static int hits = 0; if (hits++ < 3) log("[hack-50037A34-shortcircuit] fired R0=0x%08x", GPRs[0]);
				// Same short-circuit also covers R0=0xFFFFFFFF (uninitialised
				// TDesC pointer from a slot whose backing storage was
				// never written). Without this, the inner LDR R0,[R0] at
				// PC=0x500379A4 alignment-faults on address 0xFFFFFFFF and
				// triggers the kernel abort recovery cycle.
				GPRs[15] = GPRs[14];  // PC = LR
				prefetchCount = 0;    // flush prefetch after PC change
				clocks += 1;
			} else if (series5HalFix && GPRs[0] == 0 &&
			           (GPRs[15] - 0xC) == 0x50043E40
			           && PSION_ENV_BOOL("PSION_S5_HAL_LEGACY")) {
				static int hits = 0; if (hits++ < 3) log("[hack-50043E40-shortcircuit] fired");
				// Series 5 EPOC R1 cleanup-stack PopTrap-and-call short-circuit.
				//
				// FUN_50043e38 is the kernel's "if a TRAP harness is currently
				// active, run its iCleanup handler" tail. Disassembly:
				//
				//   0x50043e38: STMFD SP!, {LR}
				//   0x50043e3c: BL 0x5004bd34          ; SWI 0x73 PopTrap →
				//                                       ;   R0 = NThread.iTrap
				//   0x50043e40: LDR R0, [R0, #0x48]    ; R0 = *(iTrap + 0x48)
				//                                       ;   = TTrap.iCleanup
				//   0x50043e44: CMP R0, #0             ; NULL check
				//   0x50043e48: LDMEQFD SP!, {PC}      ; if NULL, return
				//   0x50043e4c: LDR R3, [R0]           ; vtbl ptr — FAULTS
				//   0x50043e50..58: call vtbl[3] and return
				//
				// When no TRAP harness is installed, SWI 0x73 returns 0
				// (iTrap == NULL). The LDR at +0x48 then reads virtual
				// address 0x48, which on Series 5 falls inside the low-memory
				// IVT region and contains the SWI vector instruction word
				// 0xe1a03104 (MOV R3, R4 LSL #2). The kernel sees a non-NULL
				// "iCleanup" and dereferences it at 0x50043e4c → recurring
				// SectionTranslationFault every ~12M cycles for the entire
				// 15s boot window.
				//
				// On real hardware, virtual 0x48 either traps (no IVT page
				// mapped at this stage) or returns 0; either way the kernel
				// proceeds past the NULL check. We replicate the "returns 0"
				// behaviour: when input R0 is 0, skip the LDR by setting
				// PC = 0x50043e44 and R0 = 0. The CMP + LDMEQFD then fires
				// the NULL-pop and the function returns cleanly.
				//
				// Kept behind series5HalFix so non-Series-5 devices are
				// unaffected.
				if (PSION_ENV_BOOL("PSION_HAL_TRACE")) {
					// Read the original caller's LR (pushed by 0x50043e38's
					// STMFD before BL clobbered LR with 0x50043e40).
					uint32_t callerLr = 0;
					if (auto v = readVirtualDebug(GPRs[13], V32); v.has_value())
						callerLr = v.value();
					log("HAL-fix: short-circuit FUN_50043e38 null-iTrap "
					    "callerLr=%08x sp=%08x cpsr=%02x",
					    callerLr, GPRs[13], CPSR & 0x1F);
				}
				// R0 already 0 — leave it. Skip the offending LDR by re-
				// fetching from the next instruction (the CMP). The CMP
				// sees R0=0 → Z=1, and the conditional LDMEQFD at
				// 0x50043e48 pops the saved LR into PC for a clean return.
				GPRs[15] = 0x50043E44;   // re-fetch starts at CMP
				prefetchCount = 0;       // pipeline flush
				clocks += 1;
			} else if (series5HalFix && GPRs[0] == 0 && GPRs[1] == 0 &&
			           (GPRs[15] - 0xC) == 0x5000C208
			           && PSION_ENV_BOOL("PSION_S5_HAL_LEGACY")) {
					static int hits = 0; if (hits++ < 3) log("[hack-5000C208-null-tdesc-shortcircuit] fired");
				// Series 5 EPOC R1: TDesC-compare null-args short-circuit.
				//
				// 0x5000C208 is a TDesC byte-by-byte compare/lookup helper
				// in the HAL vtable. Its prologue STMDBs {R4-R7,R9-R11,LR},
				// then MOV R5,R0; MOV R4,R1; ... and the body walks R5 and
				// R4 byte-by-byte. With both args NULL the loop walks
				// virtual address 0 down toward unmapped low memory and the
				// pre-indexed LDRBNE at 0x5000C3C4 finally faults at v=0x1000
				// (R4 wrapped/decremented from the start).
				//
				// How this entry happens: the kernel's data-abort handler
				// at 0x50019678 is the upstream culprit. While trying to
				// satisfy a HAL attribute lookup (here, attribute 0x30 in
				// R4), it patches the SVC stack so that the abort-mode
				// LDMIA-with-SPSR resume eventually returns into a chain
				// 0x500031F8 -> 0x5000C208. The default-stub at 0x500031F8
				// dereferences R0+0x38 each step, walking through HAL root
				// pointer until R0 becomes 0; the next pop lands here in
				// 0x5000C208 with R0=R1=0 and LR=0x500031F8. There is no
				// real frame above on the SVC stack (it's all 0xBBBBBBBB
				// canaries), so PC=LR or the function's own LDMIA epilogue
				// would re-enter 0x500031F8 and fault on the null R0.
				//
				// Tracing the call chain shows the upstream SVC entry
				// returns through the exception-return thunk at 0x500194FC
				// (MOV LR,PC; MOVS PC,LR — restores SPSR/CPSR from the
				// pushed exception frame). That's the clean continuation
				// for HAL-lookup-failed: jump straight to 0x500194FC and
				// skip the broken default-stub chain entirely.
				//
				// R0 = 0xFFFFFFFF on return: the kernel's HAL-lookup
				// callers treat negative as "attribute not implemented"
				// (so they take the not-found branch and don't try to
				// dereference the result). R0=0 instead causes downstream
				// code to interpret the SWI argument as a TDesC pointer
				// and fault dereferencing it.
				//
				// Kept behind series5HalFix so non-Series-5 devices are
				// unaffected.
				if (PSION_ENV_BOOL("PSION_HAL_TRACE"))
					log("HAL-fix: short-circuit C208 null-TDesC compare "
					    "r4=%08x lr=%08x sp=%08x cpsr=%02x",
					    GPRs[4], GPRs[14], GPRs[13], CPSR & 0x1F);
				GPRs[0] = 0xFFFFFFFF;   // TDesC compare = "not found"
				GPRs[15] = 0x500194FC;  // exception-return thunk
				prefetchCount = 0;      // flush prefetch after PC change
				clocks += 1;
			} else {
				clocks += executeInstruction(insn);
			}
			// Series 5 HAL-fix: re-write 0 to the TRequestStatus that the
			// short-circuited HAL::Get pre-completed if the SWI dispatcher's
			// scratch `STR R12, [SP]` at 0x5001950C just clobbered it.
			// Without this re-write, the caller's `LDR R0, [SP]` at
			// 0x5003AD20 reads back R12 (whatever kernel pointer lived in
			// it) as a "completion error code", and the boot panic
			// chain at 0x50011110 fires every iteration.
			if (series5HalFix && series5HalStatusAddr &&
			    (GPRs[15] - 0xC) == 0x50019510
			    && PSION_ENV_BOOL("PSION_S5_HAL_LEGACY")) {
				static int hits = 0; if (hits++ < 3) log("[hack-HAL-status-rewrite] fired va=0x%08x", series5HalStatusAddr);
				writeVirtual(0, series5HalStatusAddr, V32);
			}
		}
	}

	if (faultTriggeredThisCycle) {
		// data abort time!
		faultTriggeredThisCycle = false;
		// PSION_S5_ABORT_TRACE=1 logs the first N data-aborts taken,
		// with the faulting PC + the saved-PC + register state so we
		// can chase the kernel's abort-handler chain (including the
		// destructive kernel-data-init memcpy that drives the slow
		// boot's productive recovery cycle).
		static int abortTraceN = -1;
		if (abortTraceN == -1) {
			const char *e = PSION_ENV_CSTR("PSION_S5_ABORT_TRACE");
			abortTraceN = (e ? std::atoi(e) : 0);
		}
		if (abortTraceN > 0) {
			abortTraceN--;
			log("[abort] FAR=%08x FSR=%02x  faultingPC=%08x  savedPC=%08x  sp=%08x  cpsr=%08x  r0=%08x r1=%08x r2=%08x r4=%08x",
				cp15_faultAddress, cp15_faultStatus,
				GPRs[15] - 0xC, GPRs[15] - 4, GPRs[13], CPSR,
				GPRs[0], GPRs[1], GPRs[2], GPRs[4]);
		}
		if (nbExcTrace_) {
			static int nbAbN = 0;
			if (nbAbN++ < 80)
				std::fprintf(stderr, "[nb-abort] #%d DATA FAR=%08x FSR=%02x "
				    "faultingPC=%08x sp=%08x cpsr=%08x lr=%08x "
				    "r0=%08x r1=%08x r2=%08x r3=%08x\n",
				    nbAbN, cp15_faultAddress, cp15_faultStatus,
				    GPRs[15] - 0xC, GPRs[13], CPSR, GPRs[14],
				    GPRs[0], GPRs[1], GPRs[2], GPRs[3]);
		}
		// PSION_S5_ALIGNMENT_DEEP=1 dumps full register state + recent PC
		// history specifically for the recurring AlignmentFault at
		// PC=0x5000CD84 (R0=0xFFFFFFFF). That fault drives the destructive
		// recovery cycle on Series 5; finding which kernel global feeds R0
		// the -1 value is the path to fixing the boot's 1000× slowdown.
		if (series5HalFix && (GPRs[15] - 0xC) == 0x5000CD84) {
			static int alignDeepN = -1;
			if (alignDeepN < 0) {
				const char *e = PSION_ENV_CSTR("PSION_S5_ALIGNMENT_DEEP");
				alignDeepN = (e ? std::atoi(e) : 0);
			}
			if (alignDeepN > 0) {
				alignDeepN--;
				log("[align-deep] PC=0x5000CD84 R0..R12 + recent history:");
				log("  r0=%08x r1=%08x r2=%08x r3=%08x", GPRs[0], GPRs[1], GPRs[2], GPRs[3]);
				log("  r4=%08x r5=%08x r6=%08x r7=%08x", GPRs[4], GPRs[5], GPRs[6], GPRs[7]);
				log("  r8=%08x r9=%08x r10=%08x r11=%08x", GPRs[8], GPRs[9], GPRs[10], GPRs[11]);
				log("  r12=%08x sp=%08x lr=%08x cpsr=%08x", GPRs[12], GPRs[13], GPRs[14], CPSR);
				log("[align-deep] recent PC history (oldest → newest):");
				logPcHistory();
			}
		}
		raiseException(Abort32, GPRs[15] - 4, 0x10);
	}

	insnCycleApprox += clocks;
	return clocks;
}

// One instruction, from a word the caller already has. See the header.
//
// Every line here mirrors ARM710::tickPageLoop's per-instruction body, and it
// has to keep mirroring it: the cycle charge (1 for the tick, 1 more because
// this tick executes rather than refills), the pcHistory write, the condition
// test, the dispatch, the fault check, and the two counters the loop advances.
uint32_t ARM710::jitExecOne(uint32_t insn) {
	uint32_t clocks = 2;
	pcHistory[pcHistoryIndex] = {GPRs[15] - 0xC, insn};
	pcHistoryIndex = (pcHistoryIndex + 1) % PcHistoryCount;
	if (checkCondition(extract(insn, 31, 28))) {
		switch (decodeKind(insn)) {
		case DK_DATAPROC:      clocks += execDataProcessing(extract1(insn,25), extract(insn,24,21), extract1(insn,20), extract(insn,19,16), extract(insn,15,12), extract(insn,11,0)); break;
		case DK_LDR_STR:       clocks += execSingleDataTransfer(extract(insn,25,20), extract(insn,19,16), extract(insn,15,12), extract(insn,11,0)); break;
		case DK_LDM_STM:       clocks += execBlockDataTransfer(extract(insn,24,20), extract(insn,19,16), extract(insn,15,0)); break;
		case DK_BRANCH:        clocks += execBranch(extract1(insn,24), extract(insn,23,0)); break;
		case DK_MULTIPLY:      clocks += execMultiply(extract(insn,21,20), extract(insn,19,16), extract(insn,15,12), extract(insn,11,8), extract(insn,3,0)); break;
		case DK_MULTIPLY_LONG: clocks += execMultiplyLong(extract(insn,22,20), extract(insn,19,16), extract(insn,15,12), extract(insn,11,8), extract(insn,3,0)); break;
		case DK_SWAP:          clocks += execSingleDataSwap(extract1(insn,22), extract(insn,19,16), extract(insn,15,12), extract(insn,3,0)); break;
		case DK_HALFWORD:      clocks += execHalfwordDataTransfer(insn); break;
		default: break;        // DK_SLOW never reaches here — see the header
		}
	}
	if (faultTriggeredThisCycle) {
		faultTriggeredThisCycle = false;
		raiseException(Abort32, GPRs[15] - 4, 0x10);
	}
	insnCycleApprox += clocks;
	if (cycleSink_) *cycleSink_ += clocks;
	return clocks;
}

// Page-anchored fast loop — see arm710.h. Replicates tick()'s prefetch / PC /
// IRQ / fault model EXACTLY, with the fetch inlined from a resolved code page so
// a run of straight-line fast instructions pays neither the fetchVirtual call +
// re-translate nor the per-instruction tick() framework. Runs only when no
// diagnostic env is active (same gate as decodeCache_), so the ~40 per-insn
// trace blocks in tick()'s slow body are inactive and need not be mirrored.
uint32_t ARM710::tickPageLoop(int maxInsns, uint32_t maxCycles, int *ticksUsed) {
	*ticksUsed = 1;      // every exit below consumes at least one tick-equivalent
	// Contract: this function advances the device cycle sink for EVERY cycle it
	// reports, including the ones a fallback tick() runs — the caller adds
	// nothing. Miss one of these paths and simulated time silently runs slow.
	auto fallbackTick = [&]() -> uint32_t {
		const uint32_t c = tick();
		if (cycleSink_) *cycleSink_ += c;
		return c;
	};
	// Reuse the page the last burst ended in when execution resumed inside it —
	// see fetchPage_. The conditions are the loop's own, so a reused resolution
	// is indistinguishable from a fresh one.
	FetchPage &fp = fetchPage_;
	if (!fetchPageValid_ || GPRs[15] < fp.startVa || GPRs[15] >= fp.endVa ||
	    fp.priv != isPrivileged() || *fp.validPtr != fp.physBase) {
		if (!resolveFetchPage(GPRs[15], fp)) {
			fetchPageValid_ = false;
			return fallbackTick();   // not fast-resolvable → let tick() handle it
		}
		fetchPageValid_ = true;
	}

	uint32_t total = 0;
	int n = 0;
	// Whether the generated-region dispatcher has had its look at this burst.
	// It gets exactly one, at the first instruction boundary with a full
	// pipeline — see the note where it is taken.
	bool jitOffered = false;
	for (; n < maxInsns && total < maxCycles; n++) {
		samplePendingExceptionsFast();

		// (Re)resolve when the fetch address has left the current 1 KB subpage —
		// after a branch into another (sub)page, or crossing a boundary in
		// straight-line code. A branch BACK within the same subpage (a tight loop)
		// keeps fp, so resolveFetchPage is paid ~once per loop, not per burst — the
		// whole point of staying in the loop across branches.
		const uint32_t fetchAddr = GPRs[15];
		if (fetchAddr < fp.startVa || fetchAddr >= fp.endVa ||
		    fp.priv != isPrivileged()) {
			if (!resolveFetchPage(fetchAddr, fp)) { fetchPageValid_ = false; break; }
		}
		if (*fp.validPtr != fp.physBase) break;            // page invalidated (self-modify)

		// Mirror tick(): execute the popped instruction only when the pipeline is
		// full; otherwise this is a pure refill tick (e.g. right after a branch /
		// exception flushed the pipeline — the loop refills it itself instead of
		// returning to tick()).
		const bool haveInsn = (prefetchCount == 2);

		// ── Generated region (Stage 3, docs/jit-engine-scope.md) ───────────
		// A region's contract is the state at an instruction boundary with a
		// full pipeline: r15 is the executing instruction's address plus 8, and
		// the two prefetch slots hold that instruction and the next. It keeps
		// the same contract on the way out and advances the device cycle sink
		// itself, so from here it is indistinguishable from a run of this loop.
		//
		// Offered INSIDE the loop rather than before it, because half of all
		// bursts resume with the pipeline mid-refill — a branch flushed it, and
		// the burst that ran the branch ended there. Offering only before the
		// loop meant the dispatcher never saw those at all: measured on the
		// netBook, 944,764 bursts against 897,800 it did see. The two refill
		// ticks this loop runs first cost the same either way; all that changes
		// is that the region gets its turn afterwards instead of being skipped.
		//
		// Once per burst, at the first boundary where an instruction would
		// actually execute. After that `jitOffered` makes it a single predicted
		// test per instruction.
		if (__builtin_expect(jit_ != nullptr, false) && !jitOffered && haveInsn) {
			jitOffered = true;
			if (prefetchFaults[0] == NoFault && prefetchFaults[1] == NoFault) {
				int jitTicks = 0;
				const uint32_t jitCycles =
					armjit::jitRun(jit_, *this, fp.hostBase, fp.startVa, fp.endVa,
					               fp.validPtr, fp.physBase, GPRs[15] - 8,
					               maxCycles - total, maxInsns - n, &jitTicks);
				if (jitCycles) { *ticksUsed = n + jitTicks; return total + jitCycles; }
			} else {
				armjit::jitNoteDirtyEntry(jit_);
			}
		}
		// Classify the word about to execute FROM that word, here, rather than
		// reading the kind carried alongside it through the prefetch pipeline.
		//
		// The carried kind can get out of step with the word it describes: on the
		// netBook Quartz image, an IRQ taken between two instructions left
		// prefetchKind[] shifted one slot against prefetch[], so `0a000001` (BEQ,
		// with Z set) was dispatched as data-processing — it executed as
		// AND r0,r0,#1, wrote 0 to r0 instead of branching, and the guest hit a
		// prefetch abort four instructions later. Deriving the kind from the word
		// at the point of use makes that disagreement impossible by construction,
		// which matters far more here than the shifts it costs: this is a
		// dispatch selector, and a wrong one silently runs the wrong handler.
		const uint32_t insn   = prefetch[1];
		const uint8_t  exKind = decodeKind(insn);
		// Defer DK_SLOW / faulted instructions to tick() — peek BEFORE mutating, so
		// the pipeline is left intact for tick() to handle this exact instruction.
		if (haveInsn && (exKind < DK_DATAPROC || prefetchFaults[1] != NoFault))
			break;

		// Same hash point as tick() — see the note there. Placed after the
		// DK_SLOW / faulted break above, so an instruction deferred to tick()
		// is hashed once, by tick(), and never twice.
		if (__builtin_expect(stateTrace_.enabled, false) && haveInsn)
			stateTrace_.step(GPRs, CPSR, insnCycleApprox, devCycles_,
			                 insn, prefetch[0], prefetchCount,
			                 exKind, decodeKind(prefetch[0]));

		// ── shuffle + inlined fetch (mirrors tick() ~388-466) ──
		if (prefetchCount >= 1) {
			prefetch[1]       = prefetch[0];
			prefetchFaults[1] = prefetchFaults[0];
		}
		const uint32_t newInsn = *reinterpret_cast<const uint32_t*>(fp.hostBase + fetchAddr);
		GPRs[15] += 4;
		prefetch[0]       = newInsn;
		prefetchFaults[0] = NoFault;
		// No decode classification is carried through the pipeline at all.
		// The dispatch selector is derived from the instruction word at the
		// point of use (`exKind` above) — never from a per-page kind[] cache,
		// and never from a shadow array shuffled alongside prefetch[].
		//
		// A cached kind is only valid while the page's contents are unchanged,
		// and the invalidation is not airtight: a code page written by anything
		// that does not go through writeVirtual (the netBook's OS-image load
		// straight into the RAM buffer, for one) leaves stale entries behind,
		// and a page whose host write pointer is null is marked `immutable` and
		// never dropped at all. A stale entry is not a slow path, it is a
		// WRONG DISPATCH: measured on the netBook Quartz image, `0a000001`
		// (BEQ, with Z set) was dispatched as data-processing 39,438,709
		// instructions in — it wrote r0 instead of branching, and the guest
		// walked into a prefetch abort four instructions later.
		//
		// decodeKind() is a switch on two bits plus a handful of masks, so the
		// cache was never buying much; correctness is worth far more than the
		// load it saves — and classifying at the point of use costs one decode
		// per instruction rather than the two the shadow array needed.
		if (prefetchCount < 2) prefetchCount++;

		uint32_t clocks = 1;   // tick()'s base (counted even on an empty refill tick)
		if (haveInsn) {
			clocks += 1;       // the fast-path's extra (tick line 484)
			pcHistory[pcHistoryIndex] = {GPRs[15] - 0xC, insn};
			pcHistoryIndex = (pcHistoryIndex + 1) % PcHistoryCount;
			if (checkCondition(extract(insn, 31, 28))) {
				switch (exKind) {
				case DK_DATAPROC:      clocks += execDataProcessing(extract1(insn,25), extract(insn,24,21), extract1(insn,20), extract(insn,19,16), extract(insn,15,12), extract(insn,11,0)); break;
				case DK_LDR_STR:       clocks += execSingleDataTransfer(extract(insn,25,20), extract(insn,19,16), extract(insn,15,12), extract(insn,11,0)); break;
				case DK_LDM_STM:       clocks += execBlockDataTransfer(extract(insn,24,20), extract(insn,19,16), extract(insn,15,0)); break;
				case DK_BRANCH:        clocks += execBranch(extract1(insn,24), extract(insn,23,0)); break;
				case DK_MULTIPLY:      clocks += execMultiply(extract(insn,21,20), extract(insn,19,16), extract(insn,15,12), extract(insn,11,8), extract(insn,3,0)); break;
				case DK_MULTIPLY_LONG: clocks += execMultiplyLong(extract(insn,22,20), extract(insn,19,16), extract(insn,15,12), extract(insn,11,8), extract(insn,3,0)); break;
				case DK_SWAP:          clocks += execSingleDataSwap(extract1(insn,22), extract(insn,19,16), extract(insn,15,12), extract(insn,3,0)); break;
				case DK_HALFWORD:      clocks += execHalfwordDataTransfer(insn); break;
				}
			}
			if (faultTriggeredThisCycle) {
				faultTriggeredThisCycle = false;
				raiseException(Abort32, GPRs[15] - 4, 0x10);
			}
		}
		insnCycleApprox += clocks;
		total += clocks;
		// Advance the device clock now, not at the end of the burst: the next
		// instruction may read a peripheral whose answer depends on it. See
		// cycleSink_.
		if (cycleSink_) *cycleSink_ += clocks;
		// Deliberately NO break on prefetchCount<2: a branch / exception that
		// flushed the pipeline is handled by the re-resolve + refill at the top of
		// the next iteration. That is the across-branches amortization.
	}
	if (total == 0) return fallbackTick();   // forward progress (ticksUsed = 1)
	*ticksUsed = n;
	return total;
}

// Synchronously invoke a ROM function. See header doc for context.
//
// Mechanism: we save the full CPU state (GPRs, CPSR, SPSRs, banked regs,
// prefetch buffer), set up r0 = arg0, lr = sentinel, pc = target, and tick
// until pc returns to the sentinel. A guard limits the call to a generous
// instruction count in case the function loops or diverges. On return we
// restore the saved state and hand back whatever was in r0 when the sentinel
// was reached.
//
// The sentinel 0xFFFFFFF0 is chosen to be outside any ROM / RAM mapping in
// the 5mx memory map; if the function ever actually tries to fetch from it
// (e.g. tail-calls without restoring lr) we'd detect that as a divergence
// before the first instruction at the sentinel runs.
uint32_t ARM710::callRomFunctionSync(uint32_t targetPC, uint32_t arg0,
                                     int maxCycles,
                                     int64_t *passedCyclesPtr) {
	constexpr uint32_t TRAP_ADDR = 0xFFFFFFF0;
	const int MAX_CYCLES = maxCycles;

	// Save CPU state.
	uint32_t savedGPRs[16];
	uint32_t savedSPSRs[6];
	uint32_t savedPrefetch[2];
	MMUFault savedPrefetchFaults[2];
	memcpy(savedGPRs, GPRs, sizeof(GPRs));
	uint32_t savedCPSR = CPSR;
	memcpy(savedSPSRs, SPSRs, sizeof(SPSRs));
	memcpy(savedPrefetch, prefetch, sizeof(prefetch));
	memcpy(savedPrefetchFaults, prefetchFaults, sizeof(prefetchFaults));
	int savedPrefetchCount = prefetchCount;
	BankIndex savedBank = bank;
	uint32_t savedFiq[2][5];
	uint32_t savedAllModes[6][2];
	memcpy(savedFiq, fiqBankedRegisters, sizeof(fiqBankedRegisters));
	memcpy(savedAllModes, allModesBankedRegisters, sizeof(allModesBankedRegisters));
	bool savedFaultFlag = faultTriggeredThisCycle;

	// Set up the call. r0 = arg, lr = trap, pc = target. Also ensure we're
	// in Supervisor32 with interrupts disabled so any kernel primitives
	// the function calls (e.g. NKern::DisableAllInterrupts) can be run
	// without raising a privilege fault.
	switchMode(Supervisor32);
	CPSR = (CPSR & ~0x1F) | 0x13;
	CPSR |= CPSR_IRQDisable;
	CPSR |= CPSR_FIQDisable;
	GPRs[0] = arg0;
	GPRs[14] = TRAP_ADDR;
	GPRs[15] = targetPC;
	prefetchCount = 0;
	faultTriggeredThisCycle = false;

	// Tick until the function returns to the sentinel, or we run out of
	// budget. Check before each tick so we stop before attempting to fetch
	// from the invalid trap address.
	int guard = MAX_CYCLES;
	uint32_t retval = 0;
	bool trapped = false;
	while (guard-- > 0) {
		uint32_t pc = getRealPC();
		if (pc == TRAP_ADDR) {
			trapped = true;
			retval = GPRs[0];
			break;
		}
		tick();
		if (passedCyclesPtr) ++(*passedCyclesPtr);
		if (faultTriggeredThisCycle) {
			log("callRomFunctionSync(%08x): fault at pc=%08x (%d cycles in)",
			    targetPC, getRealPC(), MAX_CYCLES - guard);
			break;
		}
	}

	// Restore CPU state.
	memcpy(GPRs, savedGPRs, sizeof(GPRs));
	CPSR = savedCPSR;
	memcpy(SPSRs, savedSPSRs, sizeof(SPSRs));
	memcpy(prefetch, savedPrefetch, sizeof(prefetch));
	memcpy(prefetchFaults, savedPrefetchFaults, sizeof(prefetchFaults));
	prefetchCount = savedPrefetchCount;
	bank = savedBank;
	memcpy(fiqBankedRegisters, savedFiq, sizeof(fiqBankedRegisters));
	memcpy(allModesBankedRegisters, savedAllModes, sizeof(allModesBankedRegisters));
	faultTriggeredThisCycle = savedFaultFlag;

	if (!trapped) {
		log("callRomFunctionSync(%08x) did NOT reach trap within %d cycles",
		    targetPC, MAX_CYCLES);
	}
	return retval;
}

// Variant accepting r0..r3.  Same mechanism as callRomFunctionSync but
// also sets r1, r2, r3.
uint32_t ARM710::callRomFunctionSyncN(uint32_t targetPC, uint32_t arg0,
                                       uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                       int maxCycles) {
	constexpr uint32_t TRAP_ADDR = 0xFFFFFFF0;
	const int MAX_CYCLES = maxCycles;
	uint32_t savedGPRs[16];
	uint32_t savedSPSRs[6];
	uint32_t savedPrefetch[2];
	MMUFault savedPrefetchFaults[2];
	memcpy(savedGPRs, GPRs, sizeof(GPRs));
	uint32_t savedCPSR = CPSR;
	memcpy(savedSPSRs, SPSRs, sizeof(SPSRs));
	memcpy(savedPrefetch, prefetch, sizeof(prefetch));
	memcpy(savedPrefetchFaults, prefetchFaults, sizeof(prefetchFaults));
	int savedPrefetchCount = prefetchCount;
	BankIndex savedBank = bank;
	uint32_t savedFiq[2][5];
	uint32_t savedAllModes[6][2];
	memcpy(savedFiq, fiqBankedRegisters, sizeof(fiqBankedRegisters));
	memcpy(savedAllModes, allModesBankedRegisters, sizeof(allModesBankedRegisters));
	bool savedFaultFlag = faultTriggeredThisCycle;
	switchMode(Supervisor32);
	CPSR = (CPSR & ~0x1F) | 0x13;
	CPSR |= CPSR_IRQDisable;
	CPSR |= CPSR_FIQDisable;
	GPRs[0] = arg0;
	GPRs[1] = arg1;
	GPRs[2] = arg2;
	GPRs[3] = arg3;
	GPRs[14] = TRAP_ADDR;
	GPRs[15] = targetPC;
	prefetchCount = 0;
	faultTriggeredThisCycle = false;
	int guard = MAX_CYCLES;
	uint32_t retval = 0;
	bool trapped = false;
	while (guard-- > 0) {
		uint32_t pc = getRealPC();
		if (pc == TRAP_ADDR) {
			trapped = true;
			retval = GPRs[0];
			break;
		}
		tick();
		if (faultTriggeredThisCycle) {
			break;
		}
	}
	memcpy(GPRs, savedGPRs, sizeof(GPRs));
	CPSR = savedCPSR;
	memcpy(SPSRs, savedSPSRs, sizeof(SPSRs));
	memcpy(prefetch, savedPrefetch, sizeof(prefetch));
	memcpy(prefetchFaults, savedPrefetchFaults, sizeof(prefetchFaults));
	prefetchCount = savedPrefetchCount;
	bank = savedBank;
	memcpy(fiqBankedRegisters, savedFiq, sizeof(fiqBankedRegisters));
	memcpy(allModesBankedRegisters, savedAllModes, sizeof(allModesBankedRegisters));
	faultTriggeredThisCycle = savedFaultFlag;
	if (!trapped) {
		log("callRomFunctionSyncN(%08x) did NOT reach trap within %d cycles",
		    targetPC, MAX_CYCLES);
	}
	return retval;
}


static inline uint32_t extract(uint32_t value, uint32_t hiBit, uint32_t loBit) {
	return (value >> loBit) & ((1 << (hiBit - loBit + 1)) - 1);
//	return (value >> (32 - offset - length)) & ((1 << length) - 1);
}
static inline bool extract1(uint32_t value, uint32_t bit) {
	return (value >> bit) & 1;
}


uint32_t ARM710::executeInstruction(uint32_t i) {
	uint32_t cycles = 1;
//	log("executing insn %08x @ %08x", i, GPRs[15] - 0xC);

	// PSION_S5_SYNTH_DELIVERY=1: Series 5 fake-DProcess hooks. Skip the
	// real handle-to-DProcess lookup inside EUser when called from the
	// supervisor — return our scratch fake instead. Also handles a few
	// follow-up routines that take the fake DProcess and dereference
	// chains we've pre-populated.
	if (PSION_ENV_BOOL("PSION_S5_SYNTH_DELIVERY")) {
		uint32_t pcNow = GPRs[15] - 0xC;
		// FUN_5000D840 (EUser handle lookup) entry — return fake DProcess.
		if (pcNow == 0x5000D840u) {
			auto curThread = readVirtualDebug(0x8010061Cu, V32);
			if (curThread.has_value() && curThread.value() == 0x80006DACu) {
				// Probe to find mapped range (run once)
				static bool probed = false;
				if (!probed) {
					probed = true;
					for (uint32_t addr = 0x80100000u; addr <= 0x80200000u; addr += 0x1000u) {
						writeVirtual(0xCAFE0000u | (addr >> 8), addr, V32);
						auto rb = readVirtualDebug(addr, V32);
						if (!rb.has_value() || rb.value() != (0xCAFE0000u | (addr >> 8))) {
							log("[probe] %08x NOT mapped (read=%08x)", addr, rb.value_or(0xDEAD));
							break;
						}
					}
					log("[probe] mapped scan complete");
				}
				// Layout: pick a mapped slot below the supervisor stack region
				constexpr uint32_t kFakeDProc = 0x80105000u;
				constexpr uint32_t kFakeSubA  = 0x80105100u;
				constexpr uint32_t kFakeBuf   = 0x80105400u;
				// Zero header + subA region (0x400 bytes is enough for fields)
				for (uint32_t off = 0; off < 0x400u; off += 4)
					writeVirtual(0, kFakeDProc + off, V32);
				// Wire up the chain.
				writeVirtual(kFakeSubA, kFakeDProc + 0x8cu, V32);
				writeVirtual(kFakeBuf, kFakeSubA + 0x28u, V32);
				static int fakeCount = 0;
				if (++fakeCount <= 4) {
					auto v1 = readVirtualDebug(kFakeDProc + 0x8cu, V32);
					auto v2 = readVirtualDebug(kFakeSubA + 0x28u, V32);
					log("[fake-dproc #%d] FUN_5000D840 re-init+return %08x; chain: +0x8c=%08x +0x28=%08x lr=%08x",
					    fakeCount, kFakeDProc,
					    v1.value_or(0xDEAD), v2.value_or(0xDEAD), GPRs[14]);
				}
				GPRs[0] = kFakeDProc;
				// Return immediately: set PC to LR.
				GPRs[15] = GPRs[14];
				prefetchCount = 0;
				return cycles;
			}
		}
	}

	// a big old dispatch thing here
	// but first, conditions!
	if (!checkCondition(extract(i, 31, 28)))
		return cycles;

	// PSION_S7_FORCE_SIGNAL_PREEMPT — force-preempt hook for the EKA1
	// NFastSemaphore::Signal preemption wedge.  Per F-MULTIAGENT step 22:
	// EFile.exe's FileServer (priority 14) doesn't immediately preempt
	// the C32 worker (priority 13) on each NFastSemaphore::Signal.  The
	// worker fills the IPC RMessageK pool before FileServer drains it,
	// getting KErrServerBusy retry bursts.
	//
	// Empirical (this branch) refinement: the actual signal pattern is
	// many kernel-server threads (FileServer 0x8030abb0 pri=760, Loader
	// 0x8030b0fc pri=750, WSrv 0x8030c774 pri=650, others) all signalling
	// the Supervisor thread 0x80309460 (pri=970, highest) which never
	// runs.  The kernel sets its own reschedule flag at *0x80000810 = 1
	// inside Signal but the IRQ-exit path at PC=0x50009d34 SKIPS the
	// reschedule if *0x800007f0 (kernel-lock-count / IRQ-nesting) is
	// non-zero.  So the wedge is "kernel never reaches a lockless
	// reschedule window".
	//
	// Hook point: PC=0x5000c100 — this is the LDR r3, [pc, #0x4b8]
	// inside NFastSemaphore::Signal (FUN_5000c040) immediately AFTER
	// the woken thread has been dequeued from the wait queue and its
	// iNState set to EReady (4).  At this moment:
	//   r1   = woken thread NThread*
	//   *0x80000900 = current thread NThread*
	//   NThread+0xcc = iPriority (verified by scheduler-skip probe;
	//                  see commit e85505a7 "scheduler-skip probes")
	//
	// Knob values (env var content):
	//   1, "B", "b"  : Option B — set both *0x80000154 and *0x80000810
	//                  to 1 (default; minimum intervention)
	//   "BC", "bc"   : Option B + force *0x800007f0 (kernCSLock count)
	//                  to 0 — opens an immediate reschedule window
	//   "C", "c"     : Option C — also write the woken thread to
	//                  *0x80000900 (manual context-switch top half).
	//                  Most invasive; only useful if BC fails.
	// Default OFF.  See docs/series7-c32-retry-loop.md,
	// docs/series7-handle-walker.md (branch
	// claude/ucb1200-ssp-handle-walker).
	if (PSION_ENV_BOOL("PSION_S7_FORCE_SIGNAL_PREEMPT")
	        && (GPRs[15] - 0xC) == 0x5000C100u) {
		uint32_t wokenNT = GPRs[1];
		// Bail if r1 doesn't look like a kernel-heap NThread pointer.
		if (wokenNT >= 0x80300000u && wokenNT < 0x80400000u
		        && (wokenNT & 3) == 0) {
			auto curNT = readVirtualDebug(0x80000900u, V32);
			if (curNT.has_value()
			    && curNT.value() != 0u
			    && curNT.value() != wokenNT) {
				// NThread+0xcc is iPriority (byte field, but stored
				// as a word here per the scheduler comparison at
				// 0x5000c168:
				//   `if (piVar3[2] < *(int *)(iVar5 + 0xcc))`).
				auto wokenPri = readVirtualDebug(wokenNT + 0xCC, V32);
				auto curPri   = readVirtualDebug(curNT.value() + 0xCC, V32);
				if (wokenPri.has_value() && curPri.has_value()
				        && wokenPri.value() > curPri.value()) {
					// Mode select.  We cache the parsed value on
					// first call.
					static int forceMode = -1;
					if (forceMode < 0) {
						const char *e = std::getenv(
						    "PSION_S7_FORCE_SIGNAL_PREEMPT");
						forceMode = 1; // default B
						if (e && *e) {
							if (e[0] == 'C' || e[0] == 'c')
								forceMode = 3;
							else if ((e[0] == 'B' || e[0] == 'b')
							         && (e[1] == 'C' || e[1] == 'c'))
								forceMode = 2;
							else if (e[0] == 'B' || e[0] == 'b')
								forceMode = 1;
							// any other non-empty (e.g. "1") = B.
						}
					}
					// Option B: set both reschedule flags.  The
					// Signal-side flag at 0x80000810 is set by the
					// kernel itself 4 insns later; setting it now is
					// harmless and idempotent.  *0x80000154 is the
					// OSMR1/idle-loop polled flag — setting it ensures
					// the idle thread also sees "work pending" if it
					// happens to be running.
					writeVirtual(1u, 0x80000154u, V32);
					writeVirtual(1u, 0x80000810u, V32);
					// Option BC: also clear the kernel-lock-count at
					// *0x800007f0.  The IRQ-exit reschedule path is
					// gated on *0x800007f0 == 0; if the worker thread
					// is holding a kernel lock across multiple Signal
					// calls, no IRQ exit will trigger Reschedule.
					// Clearing it on each higher-prio Signal exit
					// opens a window for the next IRQ to dispatch.
					if (forceMode >= 2) {
						writeVirtual(0u, 0x800007f0u, V32);
					}
					// Option C: also write the woken thread directly
					// to *0x80000900 (iCurrentThread).  This is the
					// "manual context switch top half" — the kernel
					// will see the new current thread and on next
					// dispatch its register state will be loaded.
					// Highly invasive; only enable if BC fails.
					if (forceMode >= 3) {
						writeVirtual(wokenNT, 0x80000900u, V32);
					}
					static int forceCount = 0;
					forceCount++;
					if (PSION_ENV_BOOL("PSION_S7_TRACE_FORCE_SIGNAL_PREEMPT")
					        && (forceCount <= 16
					            || (forceCount & 0xFF) == 0)) {
						// lockCt = kernel-lock count gating IRQ-exit
						// reschedule.  Empirically 1 during ALL hook
						// fires — the wedge is "lockCt never drops to
						// 0 between Signal calls" — see commit msg
						// for detail.
						auto lockCt = readVirtualDebug(0x800007f0u, V32);
						log("[force-preempt mode=%d] Signal exit: cur=%08x(pri=%u) woken=%08x(pri=%u) lockCt=%08x cpsr=%02x #%d",
						    forceMode, curNT.value(), curPri.value(),
						    wokenNT, wokenPri.value(),
						    lockCt.value_or(0xDEADBEEFu),
						    CPSR & 0xFF, forceCount);
					}
				}
			}
		}
	}

	// Series 5 construct-chain register dump. Gated on
	// PSION_S5_CONSTRUCT_TRACE=N (max samples; default 50). Logs
	// R0/R1/R5/R6/LR/SP at the BL site and each entry of the format
	// chain. Used to diagnose register-corruption bugs in the
	// supervisor's GetName -> string-format path. The original use
	// (2026-05-04) pinpointed an SVC-stack-shadow write bug where
	// the FIRST store of an STMFD failed to propagate to shadow
	// because GPRs[13] hadn't been written-back yet at the moment
	// of the first store; later loads at that address read stale
	// init-time data. Fixed by widening the propagation window —
	// see the `virtAddr + 64 >= currentSP` check below.
	// Series 5: supervisor-skip for blocking HAL::Get-async-style
	// functions. The supervisor reaches one of these PCs and gets
	// preempted there; each ~17.85M cycles it re-dispatches and
	// resumes at the same PC. We synthesize a "completed
	// successfully" response (R0=0 = KErrNone) and jump to the
	// function epilogue:
	//   0x5003ad10 -> jump to 0x5003ad24  (function returns)
	//   0x5003ad48 -> jump to 0x5003ad6c  (function returns)
	// Both look like wrappers: CMP R0,#0 / BNE skip / call inner /
	// LDR result / ADD SP,#4 / LDMFD SP!,{R4,PC}. Verified by static
	// analysis as the only matches in 0x5003a000-0x5003c000.
	//
	// Without this, the supervisor blocks indefinitely waiting for
	// async HAL responses that never arrive (no real I/O hardware
	// to fire them). The skip lets the supervisor proceed past the
	// blocking call, accumulating progress over each cycle:
	//   30s:  +4 unique_pcs (+1.7%)
	//   60s:  +22 unique_pcs (+6.9%)
	//   120s: +49 unique_pcs (+11.0%)
	// Zero traps; no cascade failures.
	//
	// DEFAULT-OFF: although PSION_S5_SKIP_HAL=1 increases unique_pcs
	// at 30s/60s/120s, it BREAKS the kernel's transition to USER mode.
	// Without the skip, kernel reaches CPSR=0x10 (User mode) at cycle
	// 247M (= 13.4 sim sec) and starts executing EFile.exe code. With
	// the skip, kernel stays in Abort mode and never escalates to User.
	//
	// Comparison at 120s:
	//   skip OFF: 444 unique_pcs, max CPSR mode 0x10 (User)
	//   skip ON:  493 unique_pcs, max CPSR mode 0x17 (Abort)
	//
	// The user-mode transition is more valuable than +49 kernel PCs
	// because it's the path to the splash painter. The skip's "extra"
	// PCs are kernel-side code reached due to repeated cycling, not
	// real boot progress. Set PSION_S5_SKIP_HAL=1 to re-enable for
	// experimentation; default-off ships the better behavior.
	if (series5HalFix) {
		static bool initedSkip = false;
		static bool skipEnabled = false;
		if (!initedSkip) {
			initedSkip = true;
			const char *e = PSION_ENV_CSTR("PSION_S5_SKIP_HAL");
			if (e && std::strcmp(e, "0") != 0) skipEnabled = true;
		}
		if (skipEnabled) {
			uint32_t pc = GPRs[15] - 0xC;
			uint32_t skipTo = 0;
			if (pc == 0x5003ad10u) skipTo = 0x5003ad24u;
			else if (pc == 0x5003ad48u) skipTo = 0x5003ad6cu;
			else if (pc == 0x500428a8u) skipTo = 0x500428bcu;
			if (skipTo != 0) {
				// Verify supervisor is running (avoid false positives).
				auto cur = readVirtualDebug(0x8010061Cu, V32);
				if (cur.has_value() && cur.value() == 0x80006DACu) {
					static int hits = 0;
					if (++hits <= 30 && PSION_ENV_BOOL("PSION_S5_SKIP_TRACE"))
						log("Series 5: SKIP %08x -> %08x (hit #%d)", pc, skipTo, hits);
					GPRs[0] = 0;             // pretend success
					GPRs[15] = skipTo + 8;   // jump to function epilogue
					prefetchCount = 0;       // flush prefetch
					return cycles;
				}
			}
		}
	}

	if (series5HalFix) {
		uint32_t pc = GPRs[15] - 0xC;
		bool isChainPC = (pc == 0x5002dfac || pc == 0x5003965c ||
		                  pc == 0x5003968c || pc == 0x50034220 ||
		                  pc == 0x500418a0 || pc == 0x50037a34);
		if (isChainPC && PSION_ENV_BOOL("PSION_S5_CONSTRUCT_TRACE")) {
			static int chainHits = 0;
			int limit = std::atoi(PSION_ENV_CSTR("PSION_S5_CONSTRUCT_TRACE"));
			if (limit <= 0) limit = 50;
			if (chainHits++ < limit) {
				const char *label = "?";
				switch (pc) {
				case 0x5002dfac: label = "FUN_5002DF60_BL_5003965c"; break;
				case 0x5003965c: label = "FUN_5003965c_entry"; break;
				case 0x5003968c: label = "FUN_5003968c_entry"; break;
				case 0x50034220: label = "FUN_50034220_entry"; break;
				case 0x500418a0: label = "FUN_500418A0_entry"; break;
				case 0x50037a34: label = "FUN_50037A34_entry"; break;
				}
				log("S5-CHAIN[%s] pc=%08x r0=%08x r1=%08x r2=%08x r3=%08x r5=%08x r6=%08x lr=%08x sp=%08x cpsr=%08x",
				    label, pc, GPRs[0], GPRs[1], GPRs[2], GPRs[3],
				    GPRs[5], GPRs[6], GPRs[14], GPRs[13], CPSR);
			}
		}
	}

	if ((i & 0x0F000000) == 0x0F000000) {
		if (PSION_ENV_BOOL("PSION_SWI_TRACE")) {
			static int swiCount = 0;
			int limit = std::atoi(PSION_ENV_CSTR("PSION_SWI_TRACE"));
			if (limit <= 1) limit = 400;
			if (swiCount++ < limit)
				log("SWI #%06x at pc=%08x lr=%08x r0=%08x r1=%08x r2=%08x r3=%08x",
					i & 0xFFFFFF, GPRs[15] - 8, GPRs[14],
					GPRs[0], GPRs[1], GPRs[2], GPRs[3]);
		}
		// PSION_USER_SWI_TRACE=N logs up to N SWIs issued from CPSR=0x10
		// (User mode) only. The base PSION_SWI_TRACE captures both kernel
		// and user but kernel-context SWIs swamp the log.
		//
		// 2026-05-10: previous implementation checked SPSR which is the
		// PRIOR exception's saved CPSR (stale by the time this SWI runs).
		// At the SWI instruction we have not yet called raiseException so
		// CPSR is still the caller's mode — that's the right thing to
		// gate on. (The earlier 12-hits-in-15s baseline was the rare
		// case where SPSR happened to still hold 0x10.)
		if (PSION_ENV_BOOL("PSION_USER_SWI_TRACE")) {
			if ((CPSR & 0x1F) == 0x10) {
				static int userSwiCount = 0;
				int limit = std::atoi(PSION_ENV_CSTR("PSION_USER_SWI_TRACE"));
				if (limit <= 1) limit = 400;
				if (userSwiCount++ < limit)
					log("USER SWI #%06x at pc=%08x lr=%08x r0=%08x r1=%08x r2=%08x r3=%08x",
						i & 0xFFFFFF, GPRs[15] - 8, GPRs[14],
						GPRs[0], GPRs[1], GPRs[2], GPRs[3]);
			}
		}
		// PSION_S7_TRACE_SWI_REQ=<lo_cyc>:<hi_cyc>[:<cap>]  — log every
		// user-mode (CPSR mode = 0x10) SWI executed within the
		// [lo_cyc, hi_cyc) cycle window.  Designed to capture digitiser/
		// keyboard DoRequest / DoControl / DoCancel SWIs that fire AT
		// tap time, after boot-phase SWI churn has settled and the global
		// PSION_USER_SWI_TRACE cap would otherwise be exhausted.
		//
		// Example: PSION_S7_TRACE_SWI_REQ=3538000000:3545000000:200
		// Default cap if unspecified: 200 entries.
		if (const char *e = PSION_ENV_CSTR("PSION_S7_TRACE_SWI_REQ")) {
			static bool parsed = false;
			static uint64_t winLo = 0, winHi = 0;
			static int winCap = 200, winCount = 0;
			if (!parsed) {
				parsed = true;
				char buf[64] = {};
				std::strncpy(buf, e, sizeof(buf) - 1);
				char *p = buf;
				char *c1 = std::strchr(p, ':');
				if (c1) { *c1 = 0; winLo = std::strtoull(p, nullptr, 0); p = c1 + 1; }
				char *c2 = std::strchr(p, ':');
				if (c2) { *c2 = 0; winHi = std::strtoull(p, nullptr, 0); p = c2 + 1;
				          if (*p) winCap = std::atoi(p); }
				else    { winHi = std::strtoull(p, nullptr, 0); }
				log("[swi-req] armed window cyc=[%llu, %llu) cap=%d",
				    (unsigned long long)winLo, (unsigned long long)winHi, winCap);
			}
			if ((CPSR & 0x1F) == 0x10 &&
			    insnCycleApprox >= winLo && insnCycleApprox < winHi &&
			    winCount < winCap) {
				winCount++;
				log("[swi-req] cyc=%llu #%06x pc=%08x lr=%08x r0=%08x r1=%08x r2=%08x r3=%08x",
				    (unsigned long long)insnCycleApprox,
				    i & 0xFFFFFF, GPRs[15] - 8, GPRs[14],
				    GPRs[0], GPRs[1], GPRs[2], GPRs[3]);
				if (winCount == winCap)
					log("[swi-req] cap reached (%d entries)", winCap);
			}
		}
		// PSION_S7_TRACE_WAITFOR=1 — F-MULTIAGENT fix-candidate(2) step:
		// log every SWI 0xc00084 (= EKA1 WaitForAnyRequest) issued from
		// user mode AND log the corresponding return to user mode (PC
		// matching the saved return address).  Cap at 50 entries to
		// bound log size.  Designed to answer "does the last SWI
		// 0xc00084 entry ever return?" — if entry log shows N entries
		// but only N-1 returns, the final entry never returned (kernel
		// wedged inside the handler).  Return-side matching done in
		// tick() prologue via shared file-scope state in
		// g_psionWaitForTrace (see top of file).
		if ((i & 0xFFFFFF) == 0xc00084 &&
		    (CPSR & 0x1F) == 0x10 &&
		    PSION_ENV_BOOL("PSION_S7_TRACE_WAITFOR")) {
			if (g_psionWaitForTrace.entryCount < PsionWaitForTrace::kCap) {
				g_psionWaitForTrace.entryCount++;
				// SWI insn is at GPRs[15]-0xC (we're 2 prefetch slots
				// ahead).  Kernel saves LR_svc = SWI_addr + 4 and
				// returns via movs pc, lr — so user resumes at
				// SWI_addr + 4.
				uint32_t swiPc = GPRs[15] - 0xC;
				uint32_t lr = GPRs[14];
				uint32_t retAddr = swiPc + 4; // PC after SWI returns
				log("[waitfor] enter cyc=%llu #%d swiPc=%08x lr=%08x retAddr=%08x r0=%08x r1=%08x r2=%08x r3=%08x sp=%08x",
				    (unsigned long long)insnCycleApprox,
				    g_psionWaitForTrace.entryCount, swiPc, lr, retAddr,
				    GPRs[0], GPRs[1], GPRs[2], GPRs[3], GPRs[13]);
				// Track this return address for later matching.
				int slot = g_psionWaitForTrace.pendHead;
				g_psionWaitForTrace.pendRet[slot] = retAddr;
				g_psionWaitForTrace.pendCyc[slot] = insnCycleApprox;
				g_psionWaitForTrace.pendHead =
				    (slot + 1) % PsionWaitForTrace::kPending;
				// Arm the user-PC follower in tick() — log the next
				// N user-mode PCs after this SWI to see where the
				// kernel actually returns control.  Use a bigger
				// follow count for the WSrv-tagged entries (LR in
				// 0x5012a000..a800 range) — they are the suspected
				// wedge culprits and we want the full trail.
				bool isWSrv = (lr >= 0x5012a000u && lr < 0x5012a800u);
				g_psionWaitForTrace.followUserPCs = isWSrv ? 80 : 5;
				if (g_psionWaitForTrace.entryCount == PsionWaitForTrace::kCap)
					log("[waitfor] entry cap reached (%d)",
					    PsionWaitForTrace::kCap);
			}
		}
		// PSION_S5_SWI_INJECT=<swi>:<r0>:<resultVa>:<resultVal>:<R0return>
		// — generalised SWI-completion injector. When this matches the
		// in-flight SWI + R0 selector, write resultVal to virt resultVa
		// (simulating the kernel's async fulfilment writing to the wait
		// loop's result slot) and force R0 = R0return as the SWI return.
		// Designed to test "can the boot proceed if we pretend the SWI
		// 0xc00076 R0=0x1c spawn succeeded with TFindFile result X?"
		// without re-running the full boot for each X.
		if (const char *e = PSION_ENV_CSTR("PSION_S5_SWI_INJECT")) {
			static bool inited = false;
			static uint32_t injSwi = 0, injR0 = 0;
			static uint32_t injResultVa = 0, injResultVal = 0, injR0Return = 0;
			static bool injValid = false;
			if (!inited) {
				inited = true;
				char buf[256] = {};
				strncpy(buf, e, sizeof(buf) - 1);
				char *p = buf;
				auto next = [&p]() -> uint32_t {
					char *colon = strchr(p, ':');
					if (colon) *colon = 0;
					uint32_t v = (uint32_t)strtoul(p, nullptr, 0);
					p = colon ? colon + 1 : p + strlen(p);
					return v;
				};
				injSwi = next();
				injR0 = next();
				injResultVa = next();
				injResultVal = next();
				injR0Return = next();
				injValid = true;
				log("SWI-inject ARMED: swi=%06x r0=%08x → write *0x%08x=0x%08x, return R0=0x%08x",
				    injSwi, injR0, injResultVa, injResultVal, injR0Return);
			}
			if (injValid && (i & 0xFFFFFF) == injSwi && GPRs[0] == injR0) {
				static int injectCount = 0;
				if (++injectCount <= 6)
					log("SWI-inject FIRED #%d: swi=%06x r0=%08x", injectCount,
					    injSwi, injR0);
				writeVirtual(injResultVal, injResultVa, V32);
				GPRs[0] = injR0Return;
				return cycles;
			}
		}
		// Series 5 workaround: short-circuit HAL::Get for attributes whose
		// kernel-side handlers never complete the TRequestStatus in our
		// emulation. This SWI is HAL::Get/Get-async at 0xc00076 with:
		//   R0 = HAL attribute index
		//   R1 = pointer to TInt result slot
		//   R2 = pointer to caller's TRequestStatus
		//   R3 = client SID/handle (0x80000001 for the kernel callers we see)
		//
		// The two callers we observe (0x5003ad10 and 0x5003ad48) wrap a
		// HAL::Get-async call followed by a busy-wait at 0x50039e64 that
		// polls *TRequestStatus until it leaves the active sentinel
		// (0x360, KRequestPending). For some attributes the HAL handler
		// either has no driver to complete the request, or — for the
		// kernel-stack TRequestStatus case — the caller's slot lives on
		// the SVC/Undef shared stack which the SWI dispatcher's
		// `STR R12, [SP]` at 0x5001950C clobbers, so even a successful
		// completion is overwritten before the busy-wait reads it.
		//
		// For these attributes we intercept the SWI here, write 0
		// (KErrNone) to *R2, return R0=0, and skip the kernel handler
		// entirely. The caller's CMP R0,#0 / BNE-skip path falls through
		// to the busy-wait, which reads the now-quiet status and returns
		// immediately. No real driver state is needed since we don't
		// emulate the corresponding hardware.
		//
		// Currently shorted:
		//   0x1c — first major boot stall: ~17.8M cycles of busy-wait at
		//          LR=0x50039e78 polling SVC-stack TRS at 0x80104528.
		//          Caller LR=0x5003ad10. Without this fix the kernel
		//          loops in c0004d (WaitForRequest) before timing out.
		//   0x2f — EPenClickVolume; documented above.
		// The pre-decompile gate for this short-circuit
		// (R0==0x1c||0x2f && R2>=0x80000000 && R3==0x80000001) was thought
		// to match HAL::Get; the decompile reveals it actually matches
		// RProcess::Create's IPC roundtrip at PC=0x5004BD48 (selector
		// 0x1C = EProcessExecCreate). Forcing R0=0 across the burst
		// made the loader THINK the process was created when it wasn't,
		// so EFile.exe never got mapped → the user-mode entry at virt
		// 0x80004080 had no MMU mapping → prefetch abort → kernel never
		// re-attempted user mode.
		//
		// With the short-circuit disabled, the kernel's real
		// RProcess::Create handler runs, the user-mode mapping is
		// established, and the kernel transitions to User32 (CPSR=0x10)
		// executing EFile.exe code at PC=0x500537E4+. unique_pcs grows
		// from 257 → 305 in 30 sim s. The path is gated behind the
		// PSION_S5_HAL_LEGACY env var for emergency reactivation only.
		if (series5HalFix && (i & 0xFFFFFF) == 0xc00076 &&
		    (GPRs[0] == 0x1c || GPRs[0] == 0x2f) &&
		    GPRs[2] >= 0x80000000 && GPRs[3] == 0x80000001 &&
		    PSION_ENV_BOOL("PSION_S5_HAL_LEGACY")) {
			uint32_t halAttr = GPRs[0];
			writeVirtual(0, GPRs[2], V32);
			// Also zero the result slot at R1. The HAL::Get caller
			// allocates a TInt at R1 and expects the handler to populate
			// it. Without populating, the caller reads stack garbage as
			// the result and uses it as a kernel object pointer, which
			// causes downstream aborts at PC=0x50002BA4 and similar.
			if (GPRs[1] >= 0x80000000)
				writeVirtual(0, GPRs[1], V32);
			// Remember the status address so subsequent SWI dispatches
			// (which scratch-write to [SP] at 0x5001950C and corrupt this
			// slot) can be undone — see the per-instruction guard below.
			series5HalStatusAddr = GPRs[2];
			GPRs[0] = 0;
			// Arm the next SWI 0xc00077 (the WaitForAnyRequest in the
			// HAL::Get + WaitForRequest pair at 0x5004BD44/0x5004BD48) so
			// it also short-circuits with R0=0. Without this, the kernel
			// WaitForAnyRequest handler runs, blocks on the now-completed
			// status, and returns R0 = scheduler idle-thread pointer
			// (0x80006DAC), which the caller's CMP R0,#0 mistakes for an
			// error and skips the success path.
			series5HalShortCircuitWait = true;
			if (PSION_ENV_BOOL("PSION_HAL_TRACE"))
				log("HAL-fix: short-circuit HAL::Get(0x%02x) status_ptr=0x%08x",
				    halAttr, GPRs[2]);
			return cycles;
		}
		// Companion to the HAL::Get short-circuit above: the wrapper at
		// 0x5004BD44 issues FOUR back-to-back SWIs as inline thunks:
		//   0x5004BD44: SWI 0xc00076  (HAL::Get / SendReceive)
		//   0x5004BD48: SWI 0xc00077  (WaitForAnyRequest)
		//   0x5004BD4C: SWI 0xc00078  (CompleteRequest)
		//   0x5004BD50: SWI 0xc00079  (final scheduler hand-off; this one
		//                              effectively does MOV PC, LR back
		//                              to the BL caller)
		// Each non-short-circuited handler trashes R0 (the kernel returns
		// the scheduler idle-thread block 0x80006DAC), so the BL caller's
		// CMP R0,#0 sees an "error" and skips the success path. Stay armed
		// across all three following SWIs and force R0=0 each time so
		// the caller sees a clean success return.
		if (series5HalFix && series5HalShortCircuitWait &&
		    ((i & 0xFFFFFF) == 0xc00077 ||
		     (i & 0xFFFFFF) == 0xc00078 ||
		     (i & 0xFFFFFF) == 0xc00079)) {
			GPRs[0] = 0;
			if ((i & 0xFFFFFF) == 0xc00079)
				series5HalShortCircuitWait = false;
			if (PSION_ENV_BOOL("PSION_HAL_TRACE"))
				log("HAL-fix: short-circuit SWI 0x%06x after HAL::Get",
				    i & 0xFFFFFF);
			return cycles;
		}
		// Disarm if any other SWI runs first (defensive — should not
		// happen given the back-to-back thunk pattern, but keeps the flag
		// from leaking across unrelated callers).
		if (series5HalShortCircuitWait) series5HalShortCircuitWait = false;

		// PSION_S7_C32_SHORT_CIRCUIT - emergency unblock for the C32exe.exe
		// retry-on-KErrServerBusy loop that starves WServ.  Two C32 kernel
		// threads (0x8030fe84 / 0x80312c58) ping-pong sending opcode 0x16
		// to handle 0x40060004 via SWI 0xc00031 (RHandleBase::Send async).
		// Our emulator's IPC enqueue returns -16 (busy), so the client
		// spins.  Short-circuit: force the SWI to succeed (return 0, write
		// KErrNone to *r2) so the C32 init proceeds and the threads
		// transition to proper Wait states, un-starving lower-priority
		// threads.  Default OFF.  See docs/series7-c32-retry-loop.md.
		if ((i & 0xFFFFFF) == 0xc00031u && (CPSR & 0x1F) == 0x10
		        && PSION_ENV_BOOL("PSION_S7_C32_SHORT_CIRCUIT")) {
			auto ct = readVirtualDebug(0x80000900u, V32);
			if (ct.has_value() &&
			    (ct.value() == 0x8030fe84u || ct.value() == 0x80312c58u)) {
				if (GPRs[2] >= 0x40000000u)
					writeVirtual(0u, GPRs[2], V32);
				GPRs[0] = 0;
				static int scN = 0;
				if (scN++ < 16)
					log("[c32-sc] SWI 0xc00031 forced success pc=%08x lr=%08x r0=%08x r1=%08x r2=%08x r3=%08x thread=%08x",
					    GPRs[15] - 8, GPRs[14],
					    GPRs[0], GPRs[1], GPRs[2], GPRs[3],
					    ct.value());
				return cycles;
			}
		}

		{ uint8_t b = i & 0xFF; g_swiHist[b]++; g_swiInsnEx[b] = i; g_swiPcEx[b] = GPRs[14]; } // LR = real caller of the exec thunk
		raiseException(Supervisor32, GPRs[15] - 8, 0x08);
		cycles += 2; // SWI exception entry: 3 cycles total (T2)
	}
	else if ((i & 0x0F000F10) == 0x0E000F10)
		cycles += execCP15RegisterTransfer(extract(i,23,21), extract1(i,20), extract(i,19,16), extract(i,15,12), extract(i,7,5), extract(i,3,0));
	else if ((i & 0x0E000000) == 0x0A000000)
		cycles += execBranch(extract1(i,24), extract(i,23,0));
	else if ((i & 0x0E000000) == 0x08000000)
		cycles += execBlockDataTransfer(extract(i,24,20), extract(i,19,16), extract(i,15,0));
	else if ((i & 0x0C000000) == 0x04000000)
		cycles += execSingleDataTransfer(extract(i,25,20), extract(i,19,16), extract(i,15,12), extract(i,11,0));
	else if ((i & 0x0FB00FF0) == 0x01000090)
		cycles += execSingleDataSwap(extract1(i,22), extract(i,19,16), extract(i,15,12), extract(i,3,0));
	else if ((i & 0x0F8000F0) == 0x00000090)
		cycles += execMultiply(extract(i,21,20), extract(i,19,16), extract(i,15,12), extract(i,11,8), extract(i,3,0));
	else if ((i & 0x0F8000F0) == 0x00800090 && isTVersion)
		cycles += execMultiplyLong(extract(i,22,20), extract(i,19,16), extract(i,15,12), extract(i,11,8), extract(i,3,0));
	// ARMv4 halfword/signed-byte loads and halfword stores. Pattern is
	// `000 PUIWL Rn Rd xxxx 1SH1 xxxx` where SH identifies the variant
	// (01=H, 10=SB load only, 11=SH load only). Bits 7:4 are 1011/1101/1111
	// — disjoint from multiply (1001) and swap (handled above). Must be
	// tested before the generic data-processing fallthrough, which would
	// otherwise mis-decode these as MVN with a register-specified shift.
	else if ((i & 0x0E000090) == 0x00000090 && isTVersion && (i & 0x00000060) != 0)
		cycles += execHalfwordDataTransfer(i);
	// ARMv4T BX Rm:  cond 0001 0010 1111 1111 1111 0001 Rm
	// ARMv5T BLX Rm: cond 0001 0010 1111 1111 1111 0011 Rm
	// Both must be detected BEFORE the generic data-processing fallthrough,
	// which would mis-decode the encoding as MSR (Opcode=9 with Rn=15) and
	// silently overwrite CPSR with the branch target. We don't support
	// Thumb, so clear bits 0-1 of Rm. (T5 fix.)
	else if ((i & 0x0FFFFFF0) == 0x012FFF10) {
		uint32_t Rm = extract(i, 3, 0);
		uint32_t target = GPRs[Rm] & ~3u;
		prefetchCount = 0;
		GPRs[15] = target;
		cycles += 2; // pipeline refill cost (caller adds 1 base = 3 total)
	}
	else if ((i & 0x0FFFFFF0) == 0x012FFF30) {
		// BLX register form. LR <- next-insn (current PC - 4 because PC
		// has already been advanced by prefetch).
		uint32_t Rm = extract(i, 3, 0);
		uint32_t target = GPRs[Rm] & ~3u;
		GPRs[14] = GPRs[15] - 4;
		prefetchCount = 0;
		GPRs[15] = target;
		cycles += 2;
	}
	else if ((i & 0x0C000000) == 0x00000000)
		cycles += execDataProcessing(extract1(i,25), extract(i,24,21), extract1(i,20), extract(i,19,16), extract(i,15,12), extract(i,11,0));
	else {
		if (PSION_ENV_BOOL("PSION_UNDEF_TRACE")) {
			static int n = 0;
			if (n++ < 40)
				log("UNDEF instr=%08x at pc=%08x lr=%08x cpsr=%08x",
					i, GPRs[15] - 8, GPRs[14], CPSR);
		}
		raiseException(Undefined32, GPRs[15] - 8, 0x04);
		cycles += 2; // Undef exception entry: 3 cycles total (T2)
	}

	return cycles;
}

uint32_t ARM710::execDataProcessing(bool I, uint32_t Opcode, bool S, uint32_t Rn, uint32_t Rd, uint32_t Operand2)
{
	// Base cost (1 cycle) is added by the caller. We add extras here:
	//   +1 if register-specified shift amount is used (datasheet 6-7)
	//   +2 if destination is PC (pipeline refill)  -- added below
	uint32_t cycles = 0;
	if (!I && extract1(Operand2, 4)) {
		// Shift-by-Register costs an extra Internal cycle.
		cycles += 1;
	}
	bool shifterCarryOutput;

	// compute our Op1 (may be unnecessary but that's ok)
	uint32_t op1 = GPRs[Rn];

	// compute our Op2
	uint32_t op2;
	if (!I) {
		// REGISTER
		uint32_t Rm = extract(Operand2, 3, 0);
		op2 = GPRs[Rm];

		uint8_t shiftBy;

		// this is the real painful one, honestly
		if (extract(Operand2, 4, 4)) {
			// Shift by Register
			uint32_t Rs = extract(Operand2, 11, 8);
			shiftBy = GPRs[Rs] & 0xFF;
		} else {
			// Shift by Immediate
			shiftBy = extract(Operand2, 11, 7);

			if (Rn == 15) // if PC is fetched...
				op1 -= 4; // compensate for prefetching
			if (Rm == 15)
				op2 -= 4;
		}

		if (extract(Operand2, 4, 4) && (shiftBy == 0)) {
			// register shift by 0 never does anything
			shifterCarryOutput = flagC();
		} else {
			switch (extract(Operand2, 6, 5)) {
			case 0: // Logical Left (LSL)
				if (shiftBy == 0) {
					shifterCarryOutput = flagC();
					// no change to op2!
				} else if (shiftBy <= 31) {
					// ARM ARM: LSL #N carry-out = Rm[32 - N]
					// (the MSB-side bit that gets shifted off the top).
					// Previously this read Rm[31 - N] which is off by one;
					// it broke 64-bit shift-and-subtract division used by
					// EUSER's TTime/DAY_us conversion (FUN_5004c0b8),
					// producing year 8058 instead of year 2026 for Series 5
					// (the Agenda crash dialog's "7 Aug 1274" was a
					// downstream consequence of this wrong year).
					shifterCarryOutput = extract1(op2, 32 - shiftBy);
					op2 <<= shiftBy;
				} else if (shiftBy == 32) {
					shifterCarryOutput = extract1(op2, 0);
					op2 = 0;
				} else /*if (shiftBy >= 33)*/ {
					shifterCarryOutput = false;
					op2 = 0;
				}
				break;
			case 1: // Logical Right (LSR)
				if (shiftBy == 0 || shiftBy == 32) {
					shifterCarryOutput = extract1(op2, 31);
					op2 = 0;
				} else if (shiftBy <= 31) {
					shifterCarryOutput = extract1(op2, shiftBy - 1);
					op2 >>= shiftBy;
				} else /*if (shiftBy >= 33)*/ {
					shifterCarryOutput = false;
					op2 = 0;
				}
				break;
			case 2: // Arithmetic Right (ASR)
				if (shiftBy == 0 || shiftBy >= 32) {
					shifterCarryOutput = extract1(op2, 31);
					op2 = (int32_t)op2 >> 31;
				} else /*if (shiftBy <= 31)*/ {
					shifterCarryOutput = extract1(op2, shiftBy - 1);
					op2 = (int32_t)op2 >> shiftBy;
				}
				break;
			case 3: // Rotate Right (ROR)
				if (shiftBy == 0) { // treated as RRX
					shifterCarryOutput = op2 & 1;
					op2 >>= 1;
					op2 |= flagC() ? 0x80000000 : 0;
				} else {
					shiftBy %= 32;
					if (shiftBy == 0) { // like 32
						shifterCarryOutput = extract1(op2, 31);
						// no change to op2
					} else {
						shifterCarryOutput = extract1(op2, shiftBy - 1);
						op2 = ROR(op2, shiftBy);
					}
				}
				break;
			}
		}
	} else {
		// IMMEDIATE
		if (Rn == 15) // if PC is fetched...
			op1 -= 4; // compensate for prefetching

		uint32_t Rotate = extract(Operand2, 11, 8);
		uint32_t Imm = extract(Operand2, 7, 0);
		op2 = ROR(Imm, Rotate * 2);
		// M6: when the rotate field is non-zero, the carry-out from
		// the barrel shifter is bit 31 of the rotated immediate
		// (ARMv4 ARM A5.1.3). Only Rotate == 0 leaves the C flag
		// unchanged (carry stays as flagC()). Previously we always
		// preserved flagC(), which produces wrong C/Z flag results
		// for things like `TST Rn, #0xC0000000` where the rotated
		// immediate has its MSB set.
		if (Rotate == 0)
			shifterCarryOutput = flagC();
		else
			shifterCarryOutput = (op2 >> 31) & 1;
	}

	// we have our operands, what next
	uint64_t result = 0;
	uint32_t flags = 0;

#define LOGICAL_OP(v) \
	result = v; \
	flags |= (result & 0xFFFFFFFF) ? 0 : CPSR_Z; \
	flags |= (result & 0x80000000) ? CPSR_N : 0; \
	flags |= shifterCarryOutput ? CPSR_C : 0; \
	flags |= (CPSR & CPSR_V);

#define ADD_OP(a, b, c) \
	result = (uint64_t)(a) + (uint64_t)(b) + (uint64_t)(c); \
	flags |= (result & 0xFFFFFFFF) ? 0 : CPSR_Z; \
	flags |= (result & 0x80000000) ? CPSR_N : 0; \
	flags |= (result & 0x100000000) ? CPSR_C : 0; \
	flags |= ((((a) & 0x80000000) == ((b) & 0x80000000)) && (((a) & 0x80000000) != (result & 0x80000000))) ? CPSR_V : 0;

#define SUB_OP(a, b, c) ADD_OP(a, ~b, c)


	switch (Opcode) {
	case 0:   LOGICAL_OP(op1 & op2);     break; // AND
	case 1:   LOGICAL_OP(op1 ^ op2);     break; // EOR
	case 2:   SUB_OP(op1, op2, 1);       break; // SUB
	case 3:   SUB_OP(op2, op1, 1);       break; // RSB
	case 4:   ADD_OP(op1, op2, 0);       break; // ADD
	case 5:   ADD_OP(op1, op2, flagC()); break; // ADC
	case 6:   SUB_OP(op1, op2, flagC()); break; // SBC
	case 7:   SUB_OP(op2, op1, flagC()); break; // RSC
	case 8:   LOGICAL_OP(op1 & op2);     break; // TST
	case 9:   LOGICAL_OP(op1 ^ op2);     break; // TEQ
	case 0xA: SUB_OP(op1, op2, 1);       break; // CMP
	case 0xB: ADD_OP(op1, op2, 0);       break; // CMN
	case 0xC: LOGICAL_OP(op1 | op2);     break; // ORR
	case 0xD: LOGICAL_OP(op2);           break; // MOV
	case 0xE: LOGICAL_OP(op1 & ~op2);    break; // BIC
	case 0xF: LOGICAL_OP(~op2);          break; // MVN
	}

	if (Opcode >= 8 && Opcode <= 0xB) {
		// Output-less opcodes: special behaviour
		if (S) {
			CPSR = (CPSR & ~CPSR_FlagMask) | flags;
//			log("CPSR setflags=%08x results in CPSR=%08x", flags, CPSR);
		} else if (Opcode == 8) {
			// MRS, CPSR -> Reg
			GPRs[Rd] = CPSR;
//			log("r%d <- CPSR(%08x)", Rd, GPRs[Rd]);
		} else if (Opcode == 9) {
			// MSR, Reg -> CPSR
			// Rn[3:0] is the field mask {f, s, x, c}: bit 0 = c (CPSR[7:0],
			// control byte, requires privileged), bit 1 = x (CPSR[15:8]),
			// bit 2 = s (CPSR[23:16]), bit 3 = f (CPSR[31:24], flags).
			// Build a 32-bit byte-mask from those nibbles. (H4)
			uint32_t fieldMask = 0;
			if (Rn & 1) fieldMask |= 0x000000FF;
			if (Rn & 2) fieldMask |= 0x0000FF00;
			if (Rn & 4) fieldMask |= 0x00FF0000;
			if (Rn & 8) fieldMask |= 0xFF000000;
			// In non-privileged mode, the control byte cannot be written.
			if (!isPrivileged()) fieldMask &= ~0x000000FFu;
			auto src = I ? op2 : GPRs[extract(Operand2, 3, 0)];
			// Series 5 26-bit-mode coercion (see switchMode/LDM^).
			if (series5HalFix && isPrivileged() && (fieldMask & 0x1F)
			    && (src & 0x1F) == 0x08)
				src = (src & ~0x1F) | User32;
			// netBook BL: the bootloader's sleep-save area contains
			// uninitialised CPSR/SPSR slots that surface as 26-bit
			// compat modes (M[4]=0) when the wake handler restores
			// them.  Those modes are invalid in our v4 build and the
			// next memory access faults.  Coerce M[4]=0 to SVC32 +
			// I + F so the wake path completes cleanly.
			if (netBookBlBankedSpFix_ && isPrivileged() && (fieldMask & 0x1F)
			    && (src & 0x10) == 0) {
				src = (src & ~0x1Fu) | Supervisor32;
				src |= CPSR_IRQDisable | CPSR_FIQDisable;
			}
			uint32_t newCPSR = (CPSR & ~fieldMask) | (src & fieldMask);
			// F-MULTIAGENT step 3 probe — log I-bit transitions.
			if (PSION_ENV_BOOL("PSION_S7_TRACE_CPSR_I")
					&& ((CPSR ^ newCPSR) & 0x80u)) {
				static int n = 0;
				++n;
				// Log first 5000 unconditionally then sample 1 in
				// 100 — keeps file size sane while preserving
				// post-freeze coverage.
				if (n <= 5000 || (n % 100) == 0)
					log("[cpsr-i] %d->%d src=msr-reg pc=%08x lr=%08x oldCPSR=%08x newCPSR=%08x",
					    (CPSR >> 7) & 1, (newCPSR >> 7) & 1,
					    GPRs[15] - 0xC, GPRs[14], CPSR, newCPSR);
			}
			// If the mode bits changed, switch banks first.
			if ((fieldMask & CPSR_ModeMask) && (newCPSR & CPSR_ModeMask) != (CPSR & CPSR_ModeMask))
				switchMode(modeFromCPSR(newCPSR));
			CPSR = newCPSR;
		} else if (Opcode == 0xA) {
			// MRS, SPSR -> Reg
			if (isPrivileged()) {
				GPRs[Rd] = SPSRs[currentBank()];
//				log("r%d <- SPSR(%08x)", Rd, GPRs[Rd]);
			}
		} else /*if (Opcode == 0xB)*/ {
			// MSR, Reg/Imm -> SPSR (privileged only; no control-byte
			// restriction since SPSR isn't the live processor state). (H4)
			if (isPrivileged()) {
				uint32_t fieldMask = 0;
				if (Rn & 1) fieldMask |= 0x000000FF;
				if (Rn & 2) fieldMask |= 0x0000FF00;
				if (Rn & 4) fieldMask |= 0x00FF0000;
				if (Rn & 8) fieldMask |= 0xFF000000;
				auto src = I ? op2 : GPRs[extract(Operand2, 3, 0)];
				// netBook BL: sanitise invalid (M[4]=0) modes — see
				// MSR-CPSR comment above for rationale.  A later
				// LDMFD ^ will restore this SPSR into live CPSR, so
				// the value must be a valid 32-bit mode.
				if (netBookBlBankedSpFix_ && (fieldMask & 0x1F)
				    && (src & 0x10) == 0) {
					src = (src & ~0x1Fu) | Supervisor32;
					src |= CPSR_IRQDisable | CPSR_FIQDisable;
				}
				auto cb = currentBank();
				SPSRs[cb] = (SPSRs[cb] & ~fieldMask) | (src & fieldMask);
			}
		}
	} else {
		GPRs[Rd] = result & 0xFFFFFFFF;

		if (Rd == 15) {
			// Writing to PC
			// Special things occur here!
			GPRs[Rd] &= ~3; // TODO is this really necessary??
			prefetchCount = 0;
			cycles += 2; // pipeline refill (T2)
			if (S && isPrivileged()) {
				// We SHOULD be privileged
				// (Raise an error otherwise...?)
				auto saved = SPSRs[currentBank()];
				if (cfDiagEnabled) {
					uint32_t oldPc = GPRs[15] - 0xC;
					log("DIAG exc-return (dataproc) pc=%08x newPc=%08x oldCPSR=%08x newCPSR=%08x",
					    oldPc, GPRs[15], CPSR, saved);
				}
				// Series 5 26-bit-mode coercion (see switchMode/LDM^).
				if (series5HalFix && (saved & 0x1F) == 0x08)
					saved = (saved & ~0x1F) | User32;
				// F-MULTIAGENT step 3 probe — log I-bit transitions.
				if (PSION_ENV_BOOL("PSION_S7_TRACE_CPSR_I")
						&& ((CPSR ^ saved) & 0x80u)) {
					static int n = 0;
					++n;
					if (n <= 5000 || (n % 100) == 0)
						log("[cpsr-i] %d->%d src=excret-dp pc=%08x lr=%08x oldCPSR=%08x newCPSR=%08x",
						    (CPSR >> 7) & 1, (saved >> 7) & 1,
						    GPRs[15] - 0xC, GPRs[14], CPSR, saved);
				}
				switchMode(modeFromCPSR(saved));
				CPSR = saved;
//				log("dataproc restore CPSR: %08x", saved);
			}
		} else if (S) {
			CPSR = (CPSR & ~CPSR_FlagMask) | flags;
//			log("dataproc flag change: flags=%08x CPSR=%08x", flags, CPSR);
		}
	}

	return cycles;
}

uint32_t ARM710::execMultiply(uint32_t AS, uint32_t Rd, uint32_t Rn, uint32_t Rs, uint32_t Rm)
{
	// no need for R15 fuckery
	// datasheet says it's not allowed here
	if (AS & 2)
		GPRs[Rd] = GPRs[Rm] * GPRs[Rs] + GPRs[Rn];
	else
		GPRs[Rd] = GPRs[Rm] * GPRs[Rs];

	if (AS & 1) {
		CPSR &= ~(CPSR_N | CPSR_Z);
		CPSR |= GPRs[Rd] ? 0 : CPSR_Z;
		CPSR |= (GPRs[Rd] & 0x80000000) ? CPSR_N : 0;
	}

	// MUL: 2-5 cycles depending on Rs early-termination pattern; MLA: 3-6.
	// Use a safe constant approximation (caller adds 1 base; we return the
	// extra). Datasheet ranges: MUL +1..4, MLA +2..5. (T2)
	return (AS & 2) ? 4 : 3;
}

// ARM710T only!
uint32_t ARM710::execMultiplyLong(uint32_t UAS, uint32_t RdHi, uint32_t RdLo, uint32_t Rs, uint32_t Rm)
{
	// no need for R15 fuckery
	// datasheet says it's not allowed here
	//
	// UAS = bits[22:20] = {U, A, S}. Bit 22 (the 0x4 bit) is the
	// signedness selector: U==1 → signed (SMULL/SMLAL), U==0 → unsigned
	// (UMULL/UMLAL). This decode was previously inverted — UAS&4 ran the
	// unsigned path — so SMULL produced a zero-extended product and UMULL
	// a sign-extended one. (ARM ARM, Multiply Long; cross-checked against
	// MAME arm7core: `insn & 0x00400000 ? SMulLong : UMulLong`.)
	//
	// The signed product must sign-extend each 32-bit operand to 64 bits
	// *before* multiplying — cast through int32_t so the uint32_t →
	// int64_t widening sign-extends. Without the int32_t step,
	// SMULL(-10, 3) would compute 0x00000002_FFFFFFE2 instead of
	// 0xFFFFFFFF_FFFFFFE2 (RdLo right by accident, RdHi garbage).
	//
	// Sign- vs zero-extension only diverges when an operand's bit 31 is
	// set — exactly the case for the normalised mantissas EPOC's software
	// floating-point library feeds to these instructions. Getting the
	// selector backwards corrupted Calc/Agenda arithmetic on the SA-1100
	// (Series 7 / netBook, e.g. 5+5 → 7.142857…) and the 64-bit date
	// maths on the ARM710T devices (a "year -182" Agenda display, which a
	// `legacySmull` zero-extend hack used to paper over — no longer needed
	// now the UMULL path is correctly unsigned).
	uint64_t result;
	if (UAS & 4) {  // signed (SMULL / SMLAL)
		result = (uint64_t)((int64_t)(int32_t)GPRs[Rm] * (int64_t)(int32_t)GPRs[Rs]);
	} else {        // unsigned (UMULL / UMLAL)
		result = (uint64_t)GPRs[Rm] * (uint64_t)GPRs[Rs];
	}

	if (UAS & 2) {
		// accumulate
		uint64_t addend = (uint64_t)GPRs[RdLo] | ((uint64_t)GPRs[RdHi] << 32);
		result += addend;
	}

	if (UAS & 1) {
		CPSR &= ~(CPSR_N | CPSR_Z);
		CPSR |= result ? 0 : CPSR_Z;
		CPSR |= (result & 0x8000000000000000) ? CPSR_N : 0;
	}

	GPRs[RdLo] = result & 0xFFFFFFFF;
	GPRs[RdHi] = result >> 32;

	// UMULL/SMULL/UMLAL/SMLAL: 3-6 cycles. Caller adds 1 base; we return
	// the extra. Use 4 as a safe approximation. (T2)
	return 4;
}

uint32_t ARM710::execSingleDataSwap(bool B, uint32_t Rn, uint32_t Rd, uint32_t Rm)
{
	auto valueSize = B ? V8 : V32;
	uint32_t addr = GPRs[Rn];
	uint32_t swapIn = GPRs[Rm];
	auto readResult = readVirtual(addr, valueSize);
	auto fault = readResult.second;

	if (fault == NoFault) {
		fault = writeVirtual(swapIn, addr, valueSize);
		if (fault == NoFault)
			GPRs[Rd] = readResult.first.value();
	}

	if (fault != NoFault)
		reportFault(fault);

	if (cfDiagEnabled) {
		uint32_t pc = GPRs[15] - 0xC;
		log("DIAG SWP%s pc=%08x addr=%08x Rm=r%d->%08x read=%08x CPSR=%08x mode=%02x fault=%d",
		    B ? "B" : "", pc, addr, Rm, swapIn,
		    readResult.first.has_value() ? readResult.first.value() : 0xdeadbeef,
		    CPSR, CPSR & 0x1F, (int)fault);
	}

	// SWP/SWPB: 4 cycles total (1 base + 3 extra). (T2)
	return 3;
}

uint32_t ARM710::execSingleDataTransfer(uint32_t IPUBWL, uint32_t Rn, uint32_t Rd, uint32_t offset)
{
	bool load = extract1(IPUBWL, 0);
	bool writeback = extract1(IPUBWL, 1);
	auto valueSize = extract1(IPUBWL, 2) ? V8 : V32;
	bool up = extract1(IPUBWL, 3);
	bool preIndex = extract1(IPUBWL, 4);
	bool immediate = !extract1(IPUBWL, 5);

	// calculate the offset
	uint32_t calcOffset;
	if (!immediate) {
		// REGISTER
		uint32_t Rm = extract(offset, 3, 0);
		calcOffset = GPRs[Rm];

		uint8_t shiftBy = extract(offset, 11, 7);

		switch (extract(offset, 6, 5)) {
		case 0: // Logical Left (LSL)
			if (shiftBy > 0)
				calcOffset <<= shiftBy;
			break;
		case 1: // Logical Right (LSR)
			if (shiftBy == 0)
				calcOffset = 0;
			else
				calcOffset >>= shiftBy;
			break;
		case 2: // Arithmetic Right (ASR)
			if (shiftBy == 0)
				calcOffset = (int32_t)calcOffset >> 31;
			else
				calcOffset = (int32_t)calcOffset >> shiftBy;
			break;
		case 3: // Rotate Right (ROR)
			if (shiftBy == 0) { // treated as RRX
				calcOffset >>= 1;
				calcOffset |= flagC() ? 0x80000000 : 0;
			} else
				calcOffset = ROR(calcOffset, shiftBy);
			break;
		}
	} else {
		// IMMEDIATE
		// No rotation or anything here
		calcOffset = offset;
	}

	uint32_t base = GPRs[Rn];
	if (Rn == 15) base -= 4; // prefetch adjustment
	uint32_t modifiedBase = up ? (base + calcOffset) : (base - calcOffset);
	uint32_t transferAddr = preIndex ? modifiedBase : base;

	bool changeModes = !preIndex && writeback && isPrivileged();
	auto saveMode = currentMode();

	MMUFault fault;
	bool loadedPC = false;

	if (load) {
		if (changeModes) switchMode(User32);
		// H5: ARMv4 LDR from an unaligned address loads the word at
		// (addr & ~3) and rotates right by (addr & 3)*8 so that the
		// addressed byte ends at bits 7..0. (When CP15.A=1 readVirtual
		// would have already returned an AlignmentFault above.) The
		// host-side LOAD_32LE doesn't model the chip's auto-align, so
		// explicitly align the read here.
		uint32_t readAddr = (valueSize == V32) ? (transferAddr & ~3u) : transferAddr;
		auto readResult = readVirtual(readAddr, valueSize);
		if (changeModes) switchMode(saveMode);
		if (readResult.first.has_value()) {
			uint32_t v = readResult.first.value();
			if (valueSize == V32 && (transferAddr & 3)) {
				unsigned r = (transferAddr & 3) * 8;
				v = (v >> r) | (v << (32 - r));
			}
			GPRs[Rd] = v;
			if (Rd == 15) { prefetchCount = 0; loadedPC = true; }
		}
		fault = readResult.second;
	} else {
		uint32_t value = GPRs[Rd];
		if (changeModes) switchMode(User32);
		fault = writeVirtual(value, transferAddr, valueSize);
		if (changeModes) switchMode(saveMode);
	}

	if ((preIndex && writeback) || !preIndex)
		GPRs[Rn] = modifiedBase;

	if (fault != NoFault)
		reportFault(fault);

	// LDR: 1 extra (caller's +1 base = 2 total) + 2 if PC dest (pipeline
	// refill) + 1 if register-shifted address.
	// STR: 1 extra (2 total).
	// (T2 — supersedes the old flat `return 2`.)
	uint32_t extra = 1;
	if (load && loadedPC) extra += 2;
	if (load && !immediate) extra += 1;
	return extra;
}

// ARMv4 halfword / signed-byte transfer (LDRH, STRH, LDRSB, LDRSH).
//   `cond 000 P U I W L Rn Rd immH 1 S H 1 immL/Rm`
// Addressing mode 3: either 8-bit immediate offset (I=1, split across immH/immL)
// or register offset (I=0, low nybble is Rm, immH=0). PUW bits control pre/post
// indexing and writeback just like addressing mode 2.
uint32_t ARM710::execHalfwordDataTransfer(uint32_t insn)
{
	bool P = extract1(insn, 24);
	bool U = extract1(insn, 23);
	bool I = extract1(insn, 22);
	bool W = extract1(insn, 21);
	bool L = extract1(insn, 20);
	uint32_t Rn = extract(insn, 19, 16);
	uint32_t Rd = extract(insn, 15, 12);
	bool S = extract1(insn, 6);
	bool H = extract1(insn, 5);

	uint32_t offsetVal;
	if (I)
		offsetVal = (extract(insn, 11, 8) << 4) | extract(insn, 3, 0);
	else
		offsetVal = GPRs[extract(insn, 3, 0)];

	uint32_t base = GPRs[Rn];
	if (Rn == 15) base -= 4; // prefetch adjustment
	uint32_t modifiedBase = U ? (base + offsetVal) : (base - offsetVal);
	uint32_t transferAddr = P ? modifiedBase : base;

	MMUFault fault = NoFault;

	if (L) {
		ValueSize vs = H ? V16 : V8;
		auto readResult = readVirtual(transferAddr, vs);
		fault = readResult.second;
		if (readResult.first.has_value()) {
			uint32_t v = readResult.first.value();
			if (S) {
				// Sign-extend: LDRSB extends bit 7, LDRSH extends bit 15.
				if (H)
					v = (uint32_t)(int32_t)(int16_t)(uint16_t)v;
				else
					v = (uint32_t)(int32_t)(int8_t)(uint8_t)v;
			}
			GPRs[Rd] = v;
			if (Rd == 15) prefetchCount = 0;
		}
	} else {
		// Only STRH is defined (S=0, H=1). S=1 store variants are unpredictable.
		fault = writeVirtual(GPRs[Rd] & 0xFFFF, transferAddr, V16);
	}

	if ((P && W) || !P)
		GPRs[Rn] = modifiedBase;

	if (fault != NoFault)
		reportFault(fault);

	// LDRH/STRH/LDRSB/LDRSH: base 1 + 1 here, +2 if PC dest (halfword
	// loads to PC are unusual but possible). (T2)
	uint32_t extra = 1;
	if (L && Rd == 15) extra += 2;
	return extra;
}

uint32_t ARM710::execBlockDataTransfer(uint32_t PUSWL, uint32_t Rn, uint32_t registerList)
{
	bool load = extract1(PUSWL, 0);
	bool store = !load;
	bool writeback = extract1(PUSWL, 1);
	bool psrForceUser = extract1(PUSWL, 2);
	bool up = extract1(PUSWL, 3);
	bool preIndex = extract1(PUSWL, 4);

	MMUFault fault = NoFault;
	uint32_t base = GPRs[Rn] & ~3;
	uint32_t blockSize = popcount32(registerList) * 4;

	uint32_t lowAddr, updatedBase;
	if (up) {
		updatedBase = base + blockSize;
		lowAddr = base + (preIndex ? 4 : 0);
	} else {
		updatedBase = base - blockSize;
		lowAddr = updatedBase + (preIndex ? 0 : 4);
	}

	auto saveBank = bank;
	if (psrForceUser && (store || !(registerList & 0x8000)))
		switchBank(MainBank);

	bool doneWriteback = false;
	if (load && writeback) {
		doneWriteback = true;
		GPRs[Rn] = updatedBase;
	}

	// M7: STM with Rn in the register list. ARMv4 spec:
	//   - if Rn is the LOWEST-numbered register in the list, the
	//     ORIGINAL value of Rn is stored;
	//   - otherwise the WRITEBACK (updated) value is stored.
	// Our code already implements this correctly via the placement of
	// the writeback assignment below: when storing, the writeback fires
	// AFTER the first transfer completes. So if Rn happens to be the
	// lowest-set bit, iteration 1 stores the old GPRs[Rn] before the
	// writeback overwrites it; subsequent iterations see the new value.
	// LDM resolves the same way by doing writeback up-front (above),
	// then loading registers in order (including possibly Rn).
	// One host pointer for the whole block instead of one readVirtual() /
	// writeVirtual() per register — see ARM710::fastSpanPtr. Null whenever the
	// device has no such path, or the span leaves a 1 KB region, or any of the
	// fast-path checks miss; then every access below takes the per-word path
	// exactly as it always did.
	uint8_t *const span = registerList ? fastSpanPtr(lowAddr, blockSize, store)
	                                   : nullptr;
	uint32_t addr = lowAddr;
	for (int i = 0; i < 16; i++) {
		if (registerList & (1 << i)) {
			// work on this one
			if (load) {
				// handling for LDM faults may be kinda iffy...
				// wording on datasheet is a bit unclear
				uint32_t loaded     = 0;
				bool     haveLoaded = false;
				if (span) {
					loaded = *reinterpret_cast<const uint32_t *>(span + addr);
					haveLoaded = true;
					GPRs[i] = loaded;
				} else {
					auto readResult = readVirtual(addr, V32);
					if (readResult.first.has_value()) {
						loaded = readResult.first.value();
						haveLoaded = true;
						GPRs[i] = loaded;
					}
					if (readResult.second != NoFault) {
						fault = readResult.second;
						break;
					}
				}
				// LDM-watch: log when LDM pops the kernel panic stub
				// address 0x50019280 from memory. Helps locate boot
				// divergence into the kernel's `B .` panic loop.
				if (haveLoaded && loaded == 0x50019280u &&
				    PSION_ENV_BOOL("PSION_LDM_PANIC_WATCH")) {
					uint32_t pc = GPRs[15] - 0xC;
					log("LDM-PANIC pop: addr=%08x val=0x50019280 -> R%d  pc=%08x lr=%08x cpsr=%x",
						addr, i, pc, GPRs[14], CPSR & 0x1F);
				}
				// Series 5 LDMFD-rescue: when the boot's supervisor
				// Construct chain (FUN_5000925C -> FUN_5002EB0C ->
				// FUN_5002DF60 -> FUN_50037958) reaches its return
				// LDMFD with a corrupted saved-PC slot containing
				// 0x50019280 (a `B .` ROM panic stub), redirect the
				// popped PC to the saved LR (R14), which holds the
				// BL caller's correct return address. Gated on
				// PSION_S5_LDMFD_RESCUE=1; only active when popping
				// PC (i==15) with the corrupt sentinel value.
				if (i == 15 && haveLoaded && loaded == 0x50019280u &&
				    GPRs[14] != 0x50019280u &&
				    PSION_ENV_BOOL("PSION_S5_LDMFD_RESCUE")) {
					uint32_t oldPc = GPRs[15] - 0xC;
					log("LDMFD-RESCUE: corrupt PC pop 0x50019280 at "
					    "addr=%08x ldm-pc=%08x; redirecting to LR=%08x",
					    addr, oldPc, GPRs[14]);
					GPRs[i] = GPRs[14];
				}
			} else if (span) {
				*reinterpret_cast<uint32_t *>(span + addr) = GPRs[i];
			} else {
				auto newFault = writeVirtual(GPRs[i], addr, V32);
				if (newFault != NoFault)
					fault = newFault;
			}

			addr += 4;

			if (writeback && !doneWriteback) {
				doneWriteback = true;
				GPRs[Rn] = updatedBase;
			}
		}
	}

	// datasheet specifies that base register must be
	// restored if an error occurs during LDM
	if (load && fault != NoFault)
		GPRs[Rn] = writeback ? updatedBase : base;

	if (psrForceUser && (store || !(registerList & 0x8000)))
		switchBank(saveBank);

	// PSION_S7_TRACE_RESCHED_FAULT: when an LDM loads a bad PC (unmapped gap),
	// dump the source frame slot-by-slot so we can see whether it's a
	// register-offset mismatch (valid frame, wrong slots) or a corrupt frame.
	if (load && (registerList & 0x8000) && fault == NoFault
			&& PSION_ENV_BOOL("PSION_S7_TRACE_RESCHED_FAULT")
			&& ((GPRs[15] >= 0x60000000u && GPRs[15] < 0x80000000u)
			    || (GPRs[15] >= 0x90000000u && GPRs[15] < 0xC0000000u))) {
		static int dumped = 0;
		if (dumped++ < 3) {
			log("[ldm-pc] bad PC=%08x base(R%d)=%08x list=%04x ^=%d pc=%08x",
			    GPRs[15], Rn, GPRs[Rn], registerList & 0xffff,
			    psrForceUser ? 1 : 0, GPRs[15] - 0xC);
			for (uint32_t off = 0; off < 0x40; off += 0x10) {
				uint32_t b = lowAddr + off;
				log("[ldm-pc]  frame[+%02x] = %08x %08x %08x %08x", off,
				    readVirtual(b, V32).first.value_or(0xDEADBEEF),
				    readVirtual(b+4, V32).first.value_or(0xDEADBEEF),
				    readVirtual(b+8, V32).first.value_or(0xDEADBEEF),
				    readVirtual(b+12, V32).first.value_or(0xDEADBEEF));
			}
		}
	}
	if (load && (registerList & 0x8000)) {
		prefetchCount = 0;
		if (psrForceUser && isPrivileged() && fault == NoFault) {
			auto saved = SPSRs[currentBank()];
			if (cfDiagEnabled) {
				uint32_t oldPc = GPRs[15] - 0xC;
				log("DIAG exc-return (ldm^) pc=%08x newPc=%08x oldCPSR=%08x newCPSR=%08x",
				    oldPc, GPRs[15], CPSR, saved);
			}
			// Series 5 26-bit-mode coercion (mirror of switchMode hack):
			// the kernel's thread-context init produces saved CPSR with
			// mode bits 0x08. Coerce to System32 here too, since CPSR =
			// saved would otherwise overwrite switchMode's adjustment.
			if (series5HalFix && (saved & 0x1F) == 0x08)
				saved = (saved & ~0x1F) | User32;
			// F-MULTIAGENT step 3 probe — log I-bit transitions.
			if (PSION_ENV_BOOL("PSION_S7_TRACE_CPSR_I")
					&& ((CPSR ^ saved) & 0x80u)) {
				static int n = 0;
				++n;
				// Log first 5000 unconditionally then sample 1 in
				// 100 — keeps file size sane while preserving
				// post-freeze coverage.
				if (n <= 5000 || (n % 100) == 0)
					log("[cpsr-i] %d->%d src=excret-ldm pc=%08x lr=%08x oldCPSR=%08x newCPSR=%08x",
					    (CPSR >> 7) & 1, (saved >> 7) & 1,
					    GPRs[15] - 0xC, GPRs[14], CPSR, saved);
			}
			switchMode(modeFromCPSR(saved));
			CPSR = saved;
//			log("reloading saved SPSR: %08x", saved);

			// Series 5 EPOC R1 trampoline-fault synthesis. The kernel
			// creates a "legacy" thread context whose saved-PC slot at
			// virtual 0x80100308 is set to 0x80004090 — an address inside
			// a data page (the thread's own NThread descriptor block).
			// On real ARMv3 hardware that page is mapped User-no-access,
			// so the LDM^-restored PC immediately prefetch-aborts and the
			// kernel's abort vector at 0x0000000C dispatches the actual
			// scheduler. Our MMU permission model is too lenient so the
			// fetch succeeds and the CPU wanders ~7% of execution time
			// through conditional-NOP-as-data, never reaching real work.
			//
			// Detect the trampoline LDM^ here and synthesize the prefetch
			// abort so the kernel can do its real scheduling. Gated on
			// series5HalFix and the 0x80004000-0x80005000 page so it can
			// only fire for the EPOC R1 trampoline pattern.
			if (series5HalFix
			    && GPRs[15] >= 0x80004000 && GPRs[15] < 0x80005000) {
				raiseException(Abort32, GPRs[15] + 4, 0xC);
			}

			// (Removed) Earlier "Series 5 EPOC R1 EFile.exe .data init"
			// hook copied 0xA88 bytes from ROM[0x5005E6F8] to virt
			// 0x504009A0 on first user-mode dispatch. That offset
			// (0x504009A0) was decoded from the TRomImageHeader at
			// +0x38 assuming the EKA2 (5mx) layout — but EPOC R1's
			// TRomImageHeader is 0x58 bytes, lacks iDataBssLinearBase,
			// and the value at +0x38 is actually iDllRefTable. The
			// .data prototype for EFile.exe is also all zero, so the
			// synthesis was a no-op even when it fired. Removed to
			// avoid clobbering iDllRefTable on any future EXE/DLL
			// with non-zero .data. The real .data-init responsibility
			// belongs to the kernel's process-load chain (SWI 0xc00076
			// selector 0x1C through FUN_5002ED14).
		}
	}

	if (fault != NoFault)
		reportFault(fault);

	// LDM/STM: N + 1 cycles (N = register count); +1 more for LDM with PC
	// dest. Caller adds 1 base; we return the extra. (T2)
	uint32_t n = popcount32(registerList);
	uint32_t extra = n; // n + 1 total => +n extra over base 1
	if (load && (registerList & 0x8000)) extra += 1;
	return extra;
}

uint32_t ARM710::execBranch(bool L, uint32_t offset)
{
	if (L)
		GPRs[14] = GPRs[15] - 8;

	// start with 24 bits, shift left 2, sign extend to 32
	int32_t sextOffset = (int32_t)(offset << 8) >> 6;

	prefetchCount = 0;
	GPRs[15] -= 4; // account for our prefetch being +4 too much
	GPRs[15] += sextOffset;
	// B/BL: 3 cycles total (base 1 + 2 for pipeline refill). (T2)
	return 2;
}

uint32_t ARM710::execCP15RegisterTransfer(uint32_t CPOpc, bool L, uint32_t CRn, uint32_t Rd, uint32_t CP, uint32_t CRm)
{
	(void)CP;
	(void)CRm;

	if (!isPrivileged())
		return 0;

	if (L) {
		// read a value
		uint32_t what = 0;

		switch (CRn) {
		case 0: what = cp15_id; break;
		// CP15 c1-c3 are read-write on the ARM710T and SA-1100; EPOC's
		// Series 7 boot does `MRC p15,0,Rx,c3,c0` to save the current
		// DACR before temporarily elevating domain 2 to manager via
		// `ORR; MCR`, then restores with another MCR. If the read
		// returns 0 the "restore" write sets DACR to `0 | 0x30 = 0x30`,
		// which strips access from every domain except domain 2 and
		// the very next instruction fetch on a code page in any other
		// domain takes a domain fault — which vectors to 0x0C, whose
		// own fetch faults, and the CPU infinite-loops. Return the
		// mirrored value we stored on the corresponding write.
		case 1: what = cp15_control; break;
		case 2: what = cp15_translationTableBase; break;
		case 3: what = cp15_domainAccessControl; break;
		case 5: what = cp15_faultStatus; break;
		case 6: what = cp15_faultAddress; break;
		// M4: SA-1100 CP15 c13 is the Process ID (PID) register used by
		// FCSE (Fast Context Switch Extension) — virtual addresses < 32MB
		// get OR'd with PID before TLB lookup. We don't model FCSE; the
		// kernel's PID is always 0 in practice on EPOC. CP15 c14 holds
		// debug-trap status on later cores; on the SA-1100 it's reserved.
		// Return 0 (the natural read-on-reset value) and log once so we
		// can tell if the kernel ever queries them and depends on a
		// non-zero result.
		case 13:
		case 14: {
			static bool warned[2] = {false, false};
			int idx = (CRn == 13) ? 0 : 1;
			if (!warned[idx]) {
				warned[idx] = true;
				log("CP15: read of c%u (%s) returns 0 (unmodelled)",
				    (unsigned)CRn, (CRn == 13) ? "PID/FCSE" : "debug");
			}
			what = 0;
			break;
		}
		// SA-1100 CP15 c15 reads (debug/clock-status). Plain ARM710 leaves
		// these unmapped. EPOC's idle thread issues `MRC p15,0,Rx,c15,c1,0`
		// to query the current clock-mode bits, but on our model it just
		// gets 0. Log once.
		case 15: {
			static bool warned15 = false;
			if (!warned15) {
				warned15 = true;
				log("CP15: read of c15 returns 0 (SA-1100 debug/clock-status unmodelled)");
			}
			what = 0;
			break;
		}
		}

		if (Rd == 15)
			CPSR = (CPSR & ~CPSR_FlagMask) | (what & CPSR_FlagMask);
		else
			GPRs[Rd] = what;
	} else {
		// store a value
		uint32_t what = GPRs[Rd];

		switch (CRn) {
		case 1: {
			// PSION_NB_TRACE_RTR: pin the ramtorom restart's MMU teardown.
			// ramtorom.ldd's restart body (0x500c8478..0x500c8518) ends with
			// `mcr p15,0,r2,c1,c0,0` at 0x500c8514 that clears the M/C/W/I
			// bits, then `mov pc,#0` -> PA 0 = bootloader reset (warm reboot).
			// Catch the MMU on->off transition here (executor-level, NOT a
			// batch boundary) so we know whether the restart actually reaches
			// the teardown or faults earlier in case-1's HW pokes.
			if (PSION_ENV_CSTR("PSION_NB_TRACE_RTR") &&
			    (cp15_control & 1u) && !(what & 1u)) {
				log("[rtr] CP15-c1 MMU-OFF write: ctrl %08x->%08x pc=%08x "
				    "lr=%08x cpsr=%08x ttb=%08x",
				    cp15_control, what, getGPR(15), getGPR(14), getCPSR(),
				    cp15_translationTableBase);
			}
			cp15_control = what; log("setting cp15_control to %08x", what);
			// The S/R (System/ROM) bits feed every Client-domain permission
			// decision, so any cached permission results are now potentially
			// stale.  Drop them (no-op unless a subclass caches perms).
			onMmuPermConfigChanged();
			break;
		}
		case 2: cp15_translationTableBase = what; break;
		case 3:
			cp15_domainAccessControl = what;
			// DACR selects each domain's access-control mode (No-Access /
			// Client / Manager); a change can flip a cached "permitted" to
			// "fault" or vice-versa, so invalidate any cached perms.
			onMmuPermConfigChanged();
			break;
		case 5:
			if (isTVersion)
				// L8: cp15_faultStatus is 8 bits wide on the ARM710T
				// (fault type [3:0] + domain [7:4]); the kernel writes
				// the full 32-bit register but only the low 8 bits matter.
				// Cast to make the narrowing explicit so compilers don't
				// warn and so the intent is documented.
				cp15_faultStatus = (uint8_t)what;
#ifdef ARM710T_TLB
			else
				flushTlb();
#endif
			break;
		case 6:
			if (isTVersion)
				cp15_faultAddress = what;
#ifdef ARM710T_TLB
			else
				flushTlb(what);
#endif
			break;
		case 7:
			// Code-coherency sync point: drop any decoded ops that the guest is
			// about to invalidate by flushing the I-cache after writing code.
			onICacheFlush();
#ifdef ARM710T_CACHE
			clearCache();
			log("cache cleared");
#endif
			// PSION_S7_CACHE_FLUSH_DELAY: env-gated cycle penalty for
			// CP15 c7 cache-flush ops.  Real SA-1100 takes ~512 cycles
			// to clean+invalidate the I/D caches; we no-op them.  If
			// kernel behaviour around the sim ~1.23 s cliff depends on
			// the flush taking wall time (busy-wait timing loop, etc.),
			// the delay reveals it.  Default OFF.  Value of env var (if
			// numeric) becomes the cycle penalty; "1" means 512.
			if (const char *e = PSION_ENV_CSTR("PSION_S7_CACHE_FLUSH_DELAY")) {
				int n = atoi(e);
				return n > 1 ? (uint32_t)n : 512;
			}
			break;
		case 8: {
#ifdef ARM710T_TLB
			// CP15 c8 is the TLB-invalidate register on both the
			// ARM710 (untagged) and ARM710T. CPOpc==1 invalidates a
			// single entry (operand = virtual address); any other
			// opcode invalidates the entire TLB.
			if (CPOpc == 1)
				flushTlb(what);
			else
				flushTlb();
#endif
			break;
		}
		// CRn=15: SA-1100-specific cache / write-buffer / idle / clock
		// operations. Plain ARM710 leaves this unmapped; the SA-1100
		// reuses this coprocessor register for several things:
		//
		//   c15,c1,2  clean a single D-cache entry (Rd = virtual addr)
		//   c15,c2,2  drain write buffer
		//   c15,c4,1  clock-switch (idle/sleep mode select)
		//   c15,c8,2  wait-for-interrupt (halt until IRQ/FIQ)
		//
		// Most of these are silent NOPs for us — we don't model a write
		// buffer or a D-cache, and our clock model is fixed at 221 MHz.
		// WFI needs real treatment: EPOC's idle thread runs a tight loop
		// of (mask IRQs; WFI; unmask) and without an actual halt that
		// becomes a spin that the emulator burns billions of instructions
		// on before the OST match tick has a chance to fire. Set
		// `wfiRequested` so the owning device's tick loop can stall
		// passedCycles until the next IRQ is ready to deliver.
		case 15: {
			if (CRm == 8 && (CPOpc == 0 || CPOpc == 2)) {
				g_armWfi++;          // desktop-idle diagnostics
				wfiRequested = true;
				break;
			}
			// M3: recognise the rest of the SA-1100 c15 op space as
			// NOPs (drain write buffer, clean D-cache line, clock
			// switch). Log unknown CRm/CPOpc combinations ONCE so a
			// future ROM trying to use a feature we don't model becomes
			// visible without flood-logging.
			// PSION_S7_CACHE_FLUSH_DELAY: same cycle penalty for SA-1100
			// c15 cache-line / drain-write-buffer ops.  Real silicon
			// stalls a few cycles for the drain; we currently no-op.
			if (PSION_ENV_CSTR("PSION_S7_CACHE_FLUSH_DELAY")
			    && (CRm == 1 || CRm == 2 || CRm == 7))
				return 16;
			bool isKnown =
				// c15,c1,* — D-cache line ops (flush/clean variants)
				(CRm == 1) ||
				// c15,c2,* — drain write buffer (both CPOpc=0 used by
				// EKA1 and CPOpc=2 documented in datasheet seen in
				// practice).
				(CRm == 2) ||
				// c15,c4,1 — clock-switch / idle-mode select
				(CRm == 4 && CPOpc == 1) ||
				// c15,c7,* — flush both D+I cache
				(CRm == 7) ||
				// c15,c8,* — I-cache flush / WFI variants (WFI handled
				// above before this check)
				(CRm == 8);
			if (!isKnown) {
				static bool warnedCp15[8][8] = {};
				unsigned cr = CRm & 7, op = CPOpc & 7;
				if (!warnedCp15[cr][op]) {
					warnedCp15[cr][op] = true;
					log("CP15: MCR p15,%u,Rd,c15,c%u,%u (NOP, SA-1100 implementation-defined)",
					    (unsigned)CPOpc, (unsigned)CRm, (unsigned)CPOpc);
				}
			}
			break;
		}
		}
	}

	return 0;
}



#ifdef ARM710T_CACHE
void ARM710T::clearCache() {
	for (uint32_t i = 0; i < CacheSets; i++) {
		for (uint32_t j = 0; j < CacheBlocksPerSet; j++) {
			cacheBlockTags[i][j] = 0;
		}
	}
}

uint8_t *ARM710T::findCacheLine(uint32_t virtAddr) {
	uint32_t set = virtAddr & CacheAddressSetMask;
	uint32_t tag = virtAddr & CacheAddressTagMask;
	set >>= CacheAddressSetShift;

	for (uint32_t i = 0; i < CacheBlocksPerSet; i++) {
		if (cacheBlockTags[set][i] & CacheBlockEnabled) {
			if ((cacheBlockTags[set][i] & ~CacheBlockEnabled) == tag)
				return &cacheBlocks[set][i][0];
		}
	}

	return nullptr;
}

pair<MaybeU32, ARM710T::MMUFault> ARM710T::addCacheLineAndRead(uint32_t physAddr, uint32_t virtAddr, ValueSize valueSize, int domain, bool isPage) {
	uint32_t set = virtAddr & CacheAddressSetMask;
	uint32_t tag = virtAddr & CacheAddressTagMask;
	set >>= CacheAddressSetShift;

	// "it will be randomly placed in a cache bank"
	//    - the ARM710a data sheet, 6-2 (p90)
	uint32_t i = rand() % CacheBlocksPerSet;
	uint8_t *block = &cacheBlocks[set][i][0];
	MaybeU32 result;
	MMUFault fault = NoFault;

	for (uint32_t j = 0; j < CacheBlockSize; j += 4) {
		auto word = readPhysical((physAddr & ~CacheAddressLineMask) + j, V32);
		if (word.has_value()) {
			write32LE(&block[j], word.value());
			if (valueSize == V8 && j == (virtAddr & CacheAddressLineMask & ~3))
				result = (word.value() >> ((virtAddr & 3) * 8)) & 0xFF;
			else if (valueSize == V32 && j == (virtAddr & CacheAddressLineMask))
				result = word.value();
		} else {
			// read error, great
			// TODO: should probably prioritise specific kinds of faults over others
			fault = encodeFaultSorP(SorPLinefetchError, isPage, domain, virtAddr & ~CacheAddressLineMask);
			break;
		}
	}

	// the cache block is only stored if it's complete
	if (fault == NoFault)
		cacheBlockTags[set][i] = tag | CacheBlockEnabled;

	return make_pair(result, fault);
}

MaybeU32 ARM710T::readCached(uint32_t virtAddr, ValueSize valueSize) {
	uint8_t *line = findCacheLine(virtAddr);
	if (line) {
		if (valueSize == V8)
			return line[virtAddr & CacheAddressLineMask];
		else /*if (valueSize == V32)*/
			return read32LE(&line[virtAddr & CacheAddressLineMask]);
	}
	return {};
}


bool ARM710T::writeCached(uint32_t value, uint32_t virtAddr, ValueSize valueSize) {
	uint8_t *line = findCacheLine(virtAddr);
	if (line) {
		if (valueSize == V8)
			line[virtAddr & CacheAddressLineMask] = value & 0xFF;
		else /*if (valueSize == V32)*/
			write32LE(&line[virtAddr & CacheAddressLineMask], value);
		return true;
	}
	return false;
}
#endif


uint32_t ARM710::physAddrFromTlbEntry(TlbEntry *tlbEntry, uint32_t virtAddr) {
	if ((tlbEntry->lv2Entry & 3) == 2) {
		// Smøl page
		return (tlbEntry->lv2Entry & 0xFFFFF000) | (virtAddr & 0xFFF);
	} else if ((tlbEntry->lv2Entry & 3) == 1) {
		// Lørge page
		return (tlbEntry->lv2Entry & 0xFFFF0000) | (virtAddr & 0xFFFF);
	} else {
		// Section
		return (tlbEntry->lv1Entry & 0xFFF00000) | (virtAddr & 0xFFFFF);
	}
}


MaybeU32 ARM710::virtToPhys(uint32_t virtAddr) {
	if (!isMMUEnabled())
		return virtAddr;

	TlbEntry tempEntry;
	auto translated = translateAddressUsingTlb(virtAddr, &tempEntry);
    if (std::holds_alternative<TlbEntry *>(translated)) {
        auto tlbEntry = std::get<TlbEntry *>(translated);
		return physAddrFromTlbEntry(tlbEntry, virtAddr);
	} else {
		return MaybeU32();
	}
}


MaybeU32 ARM710::readVirtualDebug(uint32_t virtAddr, ValueSize valueSize) {
	if (auto v = virtToPhys(virtAddr); v.has_value())
		return readPhysical(v.value(), valueSize);
	else
		return {};
}


// Series 5 lazy stack-grow alias.
//
// The EPOC R1 kernel grows its UND/SVC stack down through virtual
// 0x80003C5C → 0x80000000 → 0x7FFFFFFC → … and the kernel's own
// page-fault handler is responsible for allocating fresh backing
// pages on demand. Our emulator doesn't model that abort path, so
// stack-deep pushes that cross the page boundary fault and stall
// the boot.
//
// As a stand-in for the kernel's lazy allocation, when the MMU walk
// would raise a PageTranslationFault for a data access inside the
// guard range [STACK_GUARD_BASE, 0x80000000), redirect to a stable
// backing slice in the upper part of MemoryBlockC0.
//
// PSION_S5_STACK_GUARD_KB=N (default 8) controls the guard size.
// Set to 0 to disable the alias entirely (boot will fault on the
// first stack-deep push that crosses 0x7FFFFFFC).
//
// **Counter-intuitive:** Sweep 2026-05-06 (8/16/32/64/128/256 KB)
// showed unique_pcs at 15 sim sec: 169/159/154/32/32/32. The boot
// hits a CLIFF between 32 KB and 64 KB. Larger guards STARVE the
// kernel of the productive abort-handler recovery cycle: with too
// much stack room, the kernel doesn't overflow → never enters the
// abort path → never re-runs the kernel-data-init that lets the next
// object creation fire. The destructive cycle IS the slow boot
// mechanism.
//
// **Fine sweep + per-page first-touch probe (2026-05-06):** the
// cliff is sharp: 52 KB → pcs=160 (13 pages aliased, 0x7FFF3000-
// 0x7FFFF000), 54 KB → pcs=54 (page 0x7FFF2000 partially aliased
// from 0x7FFF2800 up; first write inside that page lands at
// 0x7FFF2FF0 and is silently satisfied instead of faulting). Once
// the alias swallows that one write, 81.9 % of CPU time loops at
// 0x5001A770 — the kernel is stuck in a wait the recovery cycle
// would otherwise unstick. The "productive fault" is therefore the
// FIRST stack push to a virtual address ≤ 0x7FFF2FFF.
//
// 8 KB is the empirical optimum. It catches the FIRST overflow site
// (giving the kernel a few extra frames to complete a per-cycle
// burst of work) but doesn't suppress the abort cascade. Sizes up
// to ~52 KB are also safe; ≥54 KB hits the cliff.
//
// **Lazy-page-grow alternative (PSION_S5_LAZY_PAGE_GROW=1):** instead
// of inline aliasing, install a real coarse-table L1 + small-page L2
// entry in the kernel's page tables and retry through the TLB. Sweep
// 2026-05-06 confirmed this produces IDENTICAL boot progression to
// the alias (169/168/32 PCs at 8/40/64 KB), because the kernel never
// reads its own page tables on this path — what matters is whether
// the access succeeds, not how. Provided as an option for parity
// with how a real EKA1 abort handler would lazy-grow stack pages.
//
// Both reads and writes hit the same backing, so an STMDB push is
// visible to a later LDMIA pop (acting like real RAM). The alias only
// fires AFTER the TLB walk fails, so it never shadows a legitimate
// kernel mapping. Gated behind series5HalFix so non-Series-5 devices
// are unaffected.
static inline uint32_t series5StackGuardSizeBytes() {
	static int cached = -1;
	if (cached < 0) {
		cached = 0x2000;  // 8 KB default — empirical sweet spot
		if (const char *e = PSION_ENV_CSTR("PSION_S5_STACK_GUARD_KB")) {
			int kb = std::atoi(e);
			if (kb >= 0 && kb <= 1024) cached = kb * 1024;
		}
	}
	return (uint32_t)cached;
}
static inline bool isSeries5StackGuard(uint32_t virtAddr) {
	uint32_t sz = series5StackGuardSizeBytes();
	if (sz == 0) return false;
	return virtAddr >= (0x80000000u - sz) && virtAddr < 0x80000000u;
}
static inline uint32_t series5StackGuardAlias(uint32_t virtAddr) {
	// Backing slice in the 8 MB RAM block. 256 KB at MemoryBlockC0[0x7C0000]
	// corresponds to physical 0xC0FC0000 (well above the framebuffer at
	// 0xC0000000-0xC012C000 and below the kernel data area at 0xD0FE6000+).
	uint32_t sz = series5StackGuardSizeBytes();
	uint32_t mask = sz - 1;
	uint32_t base = 0xC1000000u - sz;
	return base | (virtAddr & mask);
}

// PSION_S5_STACK_GUARD_LOG=1 logs first hit per page inside the guard
// range. Used to diff which pages get touched at 52 KB (boot makes
// progress, 160 PCs) vs 56 KB (cliff, 32 PCs) to identify the specific
// page whose fault drives the productive recovery cycle.
static inline void series5LogStackGuardHit(uint32_t virtAddr, bool isWrite) {
	static int gate = -1;
	if (gate < 0) {
		const char *e = PSION_ENV_CSTR("PSION_S5_STACK_GUARD_LOG");
		gate = (e && std::atoi(e) != 0) ? 1 : 0;
	}
	if (!gate) return;
	static uint32_t seen[512] = {0};
	static int seenN = 0;
	uint32_t page = virtAddr & 0xFFFFF000u;
	for (int i = 0; i < seenN; i++)
		if (seen[i] == page) return;
	if (seenN < 512) seen[seenN++] = page;
	std::fprintf(stderr, "[stkguard] %s page=%08x first-touch va=%08x\n",
		isWrite ? "W" : "R", page, virtAddr);
}

// Series 5 page-table-modifying lazy stack-grow.
//
// PSION_S5_LAZY_PAGE_GROW=1 swaps the inline read/write alias for a
// real-hardware-style abort handler: on PageTranslationFault inside the
// stack guard range, walk the kernel's L1, install a coarse-table L1
// entry pointing to a fresh L2 we own, install an L2 small-page entry
// pointing at the alias backing slice, flush TLB, and let the access
// retry through the now-valid mapping. This mirrors what an EKA1 abort
// handler would do on real hardware: lazy-grow stack pages by mapping
// fresh backing on first touch.
//
// L2-table storage is reserved at physical 0xC07BFC00 (1 KB at
// MemoryBlockC0[0x7BFC00]), just below the maximum 256 KB alias backing
// range so it can never collide with a stack page. One L2 table covers
// section 0x7FF (the only section the guard range touches).
//
// **Same caveat as the alias path:** silently satisfying these faults
// can starve the kernel of the destructive recovery cycle that drives
// boot progress today. See the cliff sweep in the alias comment above.
static inline bool series5LazyPageGrowEnabled() {
	static int cached = -1;
	if (cached < 0) {
		cached = 0;
		if (const char *e = PSION_ENV_CSTR("PSION_S5_LAZY_PAGE_GROW"))
			cached = (std::atoi(e) != 0) ? 1 : 0;
	}
	return cached != 0;
}
static constexpr uint32_t SERIES5_L2_TABLE_PHYS = 0xC07BFC00u;

bool ARM710::series5InstallLazyMapping(uint32_t virtAddr) {
	uint32_t sectionIndex = virtAddr >> 20;
	uint32_t l1Addr = cp15_translationTableBase | (sectionIndex << 2);
	auto l1Opt = readPhysical(l1Addr, V32);
	if (!l1Opt.has_value()) return false;
	uint32_t l1Entry = l1Opt.value();

	uint32_t coarseTableAddr;
	if ((l1Entry & 3) == 0) {
		// Section unmapped: install a fresh coarse-table descriptor.
		// We can only safely service one section (0x7FF) — guard the rest.
		if (sectionIndex != 0x7FFu) return false;
		coarseTableAddr = SERIES5_L2_TABLE_PHYS;
		// Zero our reserved L2 table on first install so all entries start
		// invalid. We re-zero only when L1 was previously invalid; once
		// installed, subsequent calls find the L1 already pointing at us
		// and skip this branch (preserving any L2 entries we wrote).
		// L4: 256 writePhysical calls is acceptable — only runs once per
		// section install during Series 5 boot, never on the hot path.
		for (int i = 0; i < 256; i++) {
			if (!writePhysical(0, coarseTableAddr + i * 4, V32))
				return false;
		}
		// L1 coarse-table descriptor: bits[31:10]=phys>>10, bits[8:5]=domain,
		// bits[1:0]=01. Domain 0 (manager — see DACR override).
		uint32_t newL1 = (coarseTableAddr & 0xFFFFFC00u) | (0u << 5) | 1u;
		if (!writePhysical(newL1, l1Addr, V32)) return false;
	} else if ((l1Entry & 3) == 1) {
		// Already a coarse table — reuse the kernel's (or our own) L2.
		coarseTableAddr = l1Entry & 0xFFFFFC00u;
	} else {
		// Section descriptor or reserved: leave alone.
		return false;
	}

	// Small-page L2 descriptor: bits[31:12]=page phys, bits[11:4]=AP[3..0]
	// (4 sub-pages × 2 bits, 0b11 = privileged+user RW), bits[3:2]=CB=11
	// (cacheable+bufferable), bits[1:0]=10 (small page).
	uint32_t l2Addr = coarseTableAddr | (((virtAddr >> 12) & 0xFFu) << 2);
	uint32_t backingPhys = series5StackGuardAlias(virtAddr) & 0xFFFFF000u;
	uint32_t newL2 = backingPhys | (0xFFu << 4) | 0xCu | 2u;
	if (!writePhysical(newL2, l2Addr, V32)) return false;

#ifdef ARM710T_TLB
	flushTlb(virtAddr);
#else
	singleTlbEntry = {0, 0, 0, 0};
#endif
	return true;
}

// Series 5 bootstrap NThread shadow helpers. See block comment near the
// definition of series5ShadowNThread in arm710.h.
static inline bool isSeries5BootstrapNThread(uint32_t virtAddr) {
	return virtAddr >= 0x80003CD0u && virtAddr < 0x80003DF8u;
}

std::pair<MaybeU32, ARM710::MMUFault> ARM710::readVirtual(uint32_t virtAddr, ValueSize valueSize) {
	if (isAlignmentFaultEnabled() && valueSize == V32 && virtAddr & 3)
		return make_pair(MaybeU32(), encodeFault(AlignmentFault, 0, virtAddr));
	if (isAlignmentFaultEnabled() && valueSize == V16 && virtAddr & 1)
		return make_pair(MaybeU32(), encodeFault(AlignmentFault, 0, virtAddr));

	// PSION_READ_WATCH=<hex>[-<hex>] logs every virtual read in that range.
	{
		static bool inited = false;
		static uint32_t watchStart = 0, watchEnd = 0;
		if (!inited) {
			inited = true;
			const char *e = PSION_ENV_CSTR("PSION_READ_WATCH");
			if (e) {
				char *endp;
				watchStart = (uint32_t)std::strtoul(e, &endp, 0);
				watchEnd = watchStart + 4;
				if (*endp == '-') watchEnd = (uint32_t)std::strtoul(endp + 1, nullptr, 0);
			}
		}
		if (watchStart && virtAddr >= watchStart && virtAddr < watchEnd) {
			uint32_t pc = GPRs[15] - 0xC;
			// Peek the translated value so we can log it
			uint32_t peek = 0xDEADBEEF;
			if (isMMUEnabled()) {
				auto t = translateAddressUsingTlb(virtAddr);
				if (std::holds_alternative<TlbEntry *>(t)) {
					auto e = std::get<TlbEntry *>(t);
					if (auto v = readPhysical(physAddrFromTlbEntry(e, virtAddr), valueSize); v.has_value())
						peek = v.value();
				}
			} else {
				if (auto v = readPhysical(virtAddr, valueSize); v.has_value())
					peek = v.value();
			}
			log("READ watch va=%08x size=%d val=%08x pc=%08x lr=%08x r0=%08x r1=%08x cpsr=%x",
				virtAddr, (int)valueSize, peek, pc, GPRs[14], GPRs[0], GPRs[1], CPSR & 0x1F);
		}
	}

	// fast path: cache
#ifdef ARM710T_CACHE
	if (auto v = readCached(virtAddr, valueSize); v.has_value())
		return make_pair(v.value(), NoFault);
#endif

	if (!isMMUEnabled()) {
		// things are very simple without a MMU
		if (auto v = readPhysical(virtAddr, valueSize); v.has_value())
            return std::make_pair(v.value(), NoFault);
		else
			return make_pair(MaybeU32(), encodeFault(NonMMUError, 0, virtAddr));
	}

	auto translated = translateAddressUsingTlb(virtAddr);
    if (std::holds_alternative<MMUFault>(translated)) {
        // Series 5 lazy stack-grow alias (read side). See block comment above
        // readVirtual(). When the TLB walk fails inside the stack-guard range,
        // serve the read from a stable backing slice at the top of RAM.
        MMUFault f = std::get<MMUFault>(translated);
        if (series5HalFix && (f & MMUFaultTypeMask) == PageTranslationFault
            && isSeries5StackGuard(virtAddr)) {
            series5LogStackGuardHit(virtAddr, false);
            // PSION_S5_LAZY_PAGE_GROW=1 mode: install a real L1+L2 mapping
            // and let the next translation succeed through the TLB.
            if (series5LazyPageGrowEnabled() && series5InstallLazyMapping(virtAddr)) {
                auto retried = translateAddressUsingTlb(virtAddr);
                if (std::holds_alternative<TlbEntry *>(retried)) {
                    auto te = std::get<TlbEntry *>(retried);
                    uint32_t pa = physAddrFromTlbEntry(te, virtAddr);
                    if (auto v = readPhysical(pa, valueSize); v.has_value())
                        return std::make_pair(v.value(), NoFault);
                }
            }
            uint32_t aliasPhys = series5StackGuardAlias(virtAddr);
            if (auto v = readPhysical(aliasPhys, valueSize); v.has_value())
                return std::make_pair(v.value(), NoFault);
        }
        return make_pair(MaybeU32(), f);
    }

	// resolve this boy
    auto tlbEntry = std::get<TlbEntry *>(translated);

	if (auto f = checkAccessPermissions(tlbEntry, virtAddr, false); f != NoFault)
		return make_pair(MaybeU32(), f);

	int domain = (tlbEntry->lv1Entry >> 5) & 0xF;
	bool isPage = (tlbEntry->lv2Entry != 0);

	uint32_t physAddr = physAddrFromTlbEntry(tlbEntry, virtAddr);

#ifdef ARM710T_CACHE
	bool cacheable = tlbEntry->lv2Entry ? (tlbEntry->lv2Entry & 8) : (tlbEntry->lv1Entry & 8);
	if (cacheable && isCacheEnabled())
		return addCacheLineAndRead(physAddr, virtAddr, valueSize, domain, isPage);
	else
#endif
	if (auto result = readPhysical(physAddr, valueSize); result.has_value())
	{
		// Series 5 EPOC R1 bootstrap NThread shadow READ.
		// The bootstrap NThread block at virt 0x80003CD0 is allocated INSIDE
		// the SVC stack frame of FUN_50010E44 (auStack_218[512]). Its
		// 0x128-byte extent is later overwritten by SVC-stack pushes when
		// the stack grows down through the same region. Reads of NThread
		// fields (iAllocator, iCurrentDfcQ, iHandlers, savedCPSR, ...) thus
		// return clobbered values, which the kernel's slow-exec dispatcher
		// dereferences and faults on.
		//
		// Once the shadow has been activated (at PC=0x50010EA0 in
		// writeVirtual) we serve all reads of bytes inside the bootstrap
		// extent from the private shadow buffer. The shadow is updated on
		// each kernel-init write inside FUN_50010E44 so it reflects the
		// post-init state. Stack-push writes still hit real RAM but never
		// the shadow, so subsequent reads see the live NThread fields, not
		// the stack frame's overlay.
		//
		// This supersedes the earlier read-side iAllocator rescue at
		// PC=0x500031F8 (commit d6c4c6c), which returned a hard-coded
		// 0x80004000 only for that single LDR; the shadow gives correct
		// values for ALL fields and ALL readers.
		// PSION_S5_NO_SHADOW=1 disables the bootstrap-NThread shadow
		// read intercept. The shadow was added to serve stale boot
		// thread field values to the kernel after the SVC stack
		// corrupts them — but it also intercepts UNRELATED stack
		// reads (like the TRAP frame slot read in FUN_5004C360)
		// because they happen to fall in the same virt range. With
		// the shadow disabled, those reads see actual stack memory
		// (correctly set to 0 by SWI 0x72), which lets the destroy
		// iterator return early without firing the destructive cycle.
		//
		// 2026-05-11: defaulted ON (= shadow disabled). The shadow
		// was a workaround for the IRQ-during-SWI stack-corruption
		// chain caused by raiseException not setting the I-bit (see
		// commit eddf64b7). With that fix in place, IRQs no longer
		// preempt SVC handlers mid-push, the SVC stack doesn't grow
		// deep enough to overwrite the bootstrap NThread region, and
		// the shadow is unnecessary. Empirically, default-disabled
		// makes the Series 5 boot splash render (variance ~6900,
		// 11 distinct pixel values) — same as if PSION_S5_NO_SHADOW=1
		// was set explicitly. Set PSION_S5_SHADOW=1 (note the
		// inverted name) to RE-ENABLE the legacy shadow path for
		// historical comparisons.
		static int noShadow = -1;
		if (noShadow < 0) {
			const char *eOff = PSION_ENV_CSTR("PSION_S5_NO_SHADOW");
			const char *eOn  = PSION_ENV_CSTR("PSION_S5_SHADOW");
			if (eOn && eOn[0] == '1')
				noShadow = 0;  // legacy: explicitly re-enable shadow
			else if (eOff && eOff[0] == '1')
				noShadow = 1;  // legacy: explicitly disable
			else
				noShadow = 1;  // default: shadow OFF post-I-bit fix
		}
		if (!noShadow && series5HalFix && series5ShadowActive
		    && isSeries5BootstrapNThread(virtAddr)) {
			// Only serve the shadow when the address is NOT currently on
			// the live SVC stack. If the SP has descended into (or below)
			// this region, the bytes the caller is reading are the stack
			// frame's actual contents (saved LR / R4-R6 from STMFD pushes
			// in functions like memset / FUN_50037958), and overlaying
			// shadow data corrupts function returns. The classic symptom:
			// memset's LDMFD pop of the saved-PC slot at virt 0x80003D2C
			// returns the shadow's 0x50019280 (kernel panic stub address)
			// instead of the BL caller's 0x5002df80, sending the kernel
			// into the panic loop right at the supervisor thread's
			// Construct return — see series5.h "panic stub identified"
			// notes.
			//
			// Stack guard: when SP is currently inside the bootstrap
			// NThread region, the live stack frame is at virtual
			// addresses >= currentSP. The most recent caller's BL
			// store at *(SP-4) is what STMFD pushed, and what LDMFD
			// will pop. We need to return the LIVE stack content
			// for those, not the shadow's NThread field.
			//
			// Empirically, returning shadow when virtAddr >= currentSP
			// (the original condition before this guard) gives 6x more
			// kernel coverage than always-shadow (30 -> 183 unique
			// PCs). This is because the kernel reads the bootstrap
			// NThread fields LATER, after the stack has shrunk above
			// this region — at that point virtAddr (the field) IS
			// >= currentSP (the higher SP) — perfect for the shadow.
			//
			// During SVC-stack-overlapping pushes/pops (memset etc.),
			// SP descends INTO this region. The reads we want for
			// those LDMFDs are at addresses just above SP — also
			// >= currentSP — those would also be shadowed and
			// would corrupt the function return. Returning shadow
			// for these reads still leaves the boot in a panic loop
			// for the supervisor case, but it's a marked improvement
			// over always-shadow.
			//
			// TODO: distinguish "field read" vs "stack pop" more
			// reliably so shadow is returned for the former and real
			// RAM for the latter.
			uint32_t currentSP = GPRs[13];
			// Serve shadow only when virtAddr >= currentSP. Below SP,
			// real RAM is authoritative. With the matching write hook
			// updating the shadow on stack pushes (see writeVirtual
			// below), this round-trips push/pop pairs correctly.
			if (virtAddr >= currentSP) {
				uint32_t off = virtAddr - 0x80003CD0u;
				uint32_t word = series5ShadowNThread[off >> 2];
				uint32_t v;
				switch (valueSize) {
				case V8:  v = (word >> ((off & 3) * 8)) & 0xFF; break;
				case V16: v = (word >> ((off & 2) * 8)) & 0xFFFF; break;
				default:  v = word; break;
				}
				return make_pair(MaybeU32(v), NoFault);
			}
			// Falls through to real RAM.
		}
		return make_pair(result, NoFault);
	}
	else {
		if (PSION_ENV_BOOL("PSION_MMU_FAULT_TRACE")) {
			std::fprintf(stderr, "[mmu] readVirt fault va=%08x pa=%08x pc=%08x\n",
				virtAddr, physAddr, GPRs[15] - 0xC);
		}
		return make_pair(result, encodeFaultSorP(SorPOtherBusError, isPage, domain, virtAddr));
	}
}

ARM710::MMUFault ARM710::writeVirtual(uint32_t value, uint32_t virtAddr, ValueSize valueSize) {
	// PSION_PHYS_WATCH=<hex>[-<hex>] logs every write whose translated
	// physical address is in that range. Used to find aliases — if
	// multiple distinct VIRTUAL addresses all map to the same physical
	// range, this watch catches them all.
	{
		static bool inited = false;
		static uint32_t physWatchStart = 0, physWatchEnd = 0;
		if (!inited) {
			inited = true;
			const char *e = PSION_ENV_CSTR("PSION_PHYS_WATCH");
			if (e) {
				char *endp;
				physWatchStart = (uint32_t)std::strtoul(e, &endp, 0);
				physWatchEnd = physWatchStart + 4;
				if (*endp == '-') physWatchEnd = (uint32_t)std::strtoul(endp + 1, nullptr, 0);
			}
		}
		if (physWatchStart) {
			uint32_t physTrace = 0xFFFFFFFFu;
			if (isMMUEnabled()) {
				auto t = translateAddressUsingTlb(virtAddr);
				if (std::holds_alternative<TlbEntry *>(t)) {
					physTrace = physAddrFromTlbEntry(std::get<TlbEntry *>(t), virtAddr);
				}
			} else physTrace = virtAddr;
			if (physTrace >= physWatchStart && physTrace < physWatchEnd) {
				uint32_t pc = GPRs[15] - 0xC;
				log("PHYS watch va=%08x phys=%08x size=%d val=%08x pc=%08x lr=%08x cpsr=%x",
					virtAddr, physTrace, (int)valueSize, value, pc, GPRs[14], CPSR & 0x1F);
			}
		}
	}
	// PSION_WRITE_WATCH=<hex>[-<hex>] logs every virtual write in that range.
	{
		static bool inited = false;
		static uint32_t watchStart = 0, watchEnd = 0;
		if (!inited) {
			inited = true;
			const char *e = PSION_ENV_CSTR("PSION_WRITE_WATCH");
			if (e) {
				char *endp;
				watchStart = (uint32_t)std::strtoul(e, &endp, 0);
				watchEnd = watchStart + 4;
				if (*endp == '-') watchEnd = (uint32_t)std::strtoul(endp + 1, nullptr, 0);
			}
		}
		if (watchStart && virtAddr >= watchStart && virtAddr < watchEnd) {
			uint32_t pc = GPRs[15] - 0xC;
			uint32_t physTrace = 0xFFFFFFFFu;
			if (isMMUEnabled()) {
				auto t = translateAddressUsingTlb(virtAddr);
				if (std::holds_alternative<TlbEntry *>(t)) {
					physTrace = physAddrFromTlbEntry(std::get<TlbEntry *>(t), virtAddr);
				}
			} else physTrace = virtAddr;
			log("WRITE watch insncyc=%llu va=%08x phys=%08x size=%d val=%08x pc=%08x lr=%08x cpsr=%x",
				(unsigned long long)insnCycleApprox,
				virtAddr, physTrace, (int)valueSize, value, pc, GPRs[14], CPSR & 0x1F);
		}
	}
	// PSION_WRITE_CYC=<lo>,<hi> logs every virtual write whose insnCycleApprox
	// is in [lo, hi). Companion to PSION_INSN_TRACE_CYC for capturing the full
	// memory-write surface of a narrow time window (e.g. an entire SWI handler
	// run) without prefiltering by address. Excludes writes already logged by
	// PSION_WRITE_WATCH so we don't double-print.
	{
		static bool inited = false;
		static uint64_t cycLo = 0, cycHi = 0;
		if (!inited) {
			inited = true;
			if (const char *e = PSION_ENV_CSTR("PSION_WRITE_CYC")) {
				char *endp;
				cycLo = std::strtoull(e, &endp, 0);
				if (*endp == ',') cycHi = std::strtoull(endp + 1, nullptr, 0);
			}
		}
		if (cycLo && cycHi && insnCycleApprox >= cycLo && insnCycleApprox < cycHi) {
			uint32_t pc = GPRs[15] - 0xC;
			log("[wrtcyc] cyc=%llu va=%08x sz=%d val=%08x pc=%08x lr=%08x cpsr=%x",
				(unsigned long long)insnCycleApprox,
				virtAddr, (int)valueSize, value, pc, GPRs[14], CPSR & 0x1F);
		}
	}
	// PSION_VALUE_WATCH=<hex> logs every virtual write of the given value.
	// Used during Series 5 boot debug to find what writes 0x50019280
	// (the kernel panic-loop address) into the SVC stack frame, corrupting
	// the saved LR slot for the FUN_5002DF60 -> FUN_50037958 call chain.
	{
		static bool inited = false;
		static uint32_t watchValue = 0;
		if (!inited) {
			inited = true;
			const char *e = PSION_ENV_CSTR("PSION_VALUE_WATCH");
			if (e) watchValue = (uint32_t)std::strtoul(e, nullptr, 0);
		}
		if (watchValue && value == watchValue && valueSize == V32) {
			uint32_t pc = GPRs[15] - 0xC;
			log("VALUE watch val=%08x va=%08x size=%d pc=%08x lr=%08x cpsr=%x",
				value, virtAddr, (int)valueSize, pc, GPRs[14], CPSR & 0x1F);
		}
	}
	// [serialdesc capture] Bounded register-file dump on every 32-bit write to
	// the kernel serial buffer descriptor at VA 0x80000b00-0x80000b0c (5mx
	// v1.05(260)). Snapshot-diffing a healthy transfer against the wedged
	// state (test/plp-browser) showed this block flip from an armed
	// descriptor {ptr=800009ec len=400 fn=50074910 flags=20000400} to a
	// disarmed one {heap-ptr, 0, user-addr, user-addr} at the wedge — the
	// only stable kernel-data signature of the large-upload freeze. This
	// capture identifies WHO arms/disarms it (pc/lr per write). printf — not
	// log() — so it reaches the browser console without the logging gate, and
	// hard-capped so it can't distort timing beyond the first captures.
	{
		static int s_serialDescCaps = 0;
		if (virtAddr >= 0x80000b00 && virtAddr < 0x80000b10 &&
		    valueSize == V32 && s_serialDescCaps < 96) {
			s_serialDescCaps++;
			printf("[serialdesc #%d] [%08x]<-%08x pc=%08x lr=%08x cpsr=%02x "
			       "r0=%08x r1=%08x r4=%08x r5=%08x\n",
			       s_serialDescCaps, virtAddr, value, GPRs[15] - 0xC, GPRs[14],
			       CPSR & 0x1F, GPRs[0], GPRs[1], GPRs[4], GPRs[5]);
		}
	}

	// PSION_SCHEDULE_TRACE: log every write to iCurrentThread (virt
	// 0x8010061C) — i.e. every reschedule. Logs PC of the writer and
	// the new value. Spamming-throttled to first 200 entries.
	if (PSION_ENV_BOOL("PSION_SCHEDULE_TRACE") &&
	    valueSize == V32 && virtAddr == 0x8010061Cu) {
		static int schedTraceCount = 0;
		if (schedTraceCount++ < 200) {
			uint32_t pc = GPRs[15] - 0xC;
			log("SCHEDULE: iCurrentThread <- 0x%08x at pc=%08x lr=%08x cpsr=%02x cyc=%llu",
			    value, pc, GPRs[14], CPSR & 0x1F,
			    (unsigned long long)insnCycleApprox);
		}
	}

	// (5mxPro REC-crash diagnostics — TDfcVirtWrite, TblBaseWrite,
	// UserPtrIntoKern — removed once the root cause was identified and
	// fixed. Root cause: the tick-loop's patchedOsRecordingSettled CSINT-
	// firing path raised CSINT every TINT after a 100-500 ms settle,
	// but the patched OS's CSINT handler does not reliably ack INTSR.CSINT
	// within one TINT, and the kernel's IRQ dispatcher at 0x50006738
	// transitions to Undef mode with the I bit cleared — letting the
	// still-asserted CSINT re-fire immediately. The recursive IRQ chain
	// walked SP_irq down through kernel data until it overwrote the
	// kernel object-table base pointer at virt 0x80000908. Fixed by
	// removing the tick-loop firing entirely; writeAudioInput now drives
	// CSINT on actual mic-batch arrival (~50 Hz) with the 500 ms gate,
	// well within the handler's ack capacity.)

	// Series 5 EPOC R1 bootstrap NThread shadow ACTIVATION + MIRROR.
	// FUN_50010E44 line 21323 stores the bootstrap NThread pointer into the
	// kernel global *DAT_50010f9c (= virt 0x8010061C, iCurrentThread). The
	// store happens at PC=0x50010EA0 with value=0x80003CD0. At this point
	// the NThread fields are mostly initialised; we snapshot the live
	// region into our shadow buffer and switch to shadow-served reads.
	//
	// Subsequent writes inside FUN_50010E44's body (PC range
	// [0x50010E44, 0x50010F90)) that target the bootstrap NThread region
	// are mirrored into the shadow as well — notably the iAllocator init
	// at PC=0x50010EE0 which writes 0x80004000 to virt 0x80003D08 (off
	// 0x38). Writes from outside FUN_50010E44 are NOT mirrored: those are
	// the SVC stack pushes that we want to "appear" in real RAM but not
	// corrupt the shadow.
	if (series5HalFix && valueSize == V32 && virtAddr == 0x8010061Cu
	    && value == 0x80003CD0u && (GPRs[15] - 0xC) == 0x50010EA0u) {
		// Snapshot the live NThread state from real RAM into the shadow.
		for (uint32_t i = 0; i < Series5BootstrapNThreadSize; i += 4) {
			uint32_t va = 0x80003CD0u + i;
			uint32_t word = 0;
			if (auto v = readVirtualDebug(va, V32); v.has_value())
				word = v.value();
			series5ShadowNThread[i >> 2] = word;
		}
		// Series 5 EPOC R1 cleanup-stack panic-recovery short-circuit.
		//
		// FUN_50043e38 (the kernel's "PopTrap-and-call-handler" cleanup-stack
		// dispatch) reads NThread+0x48 (iCleanup) and tail-calls
		// (*(int**)(iCleanup))[3] if the field is non-NULL. The function has
		// an explicit `if (iCleanup == NULL) return;` early-out — but on
		// the bootstrap NThread the field is uninitialised garbage from the
		// overlapping auStack_218 frame, so the NULL check fails and the
		// kernel dereferences a bogus pointer. The resulting
		// SectionTranslationFault recurs every ~12M cycles and is the
		// single dominant fault throughout the 15s boot window.
		//
		// On the bootstrap NThread the kernel never installs a real
		// cleanup-stack handler — that would happen later, in EFile or
		// after thread switch to the Supervisor NThread (0x80006DAC). So
		// forcing iCleanup to NULL here lets the existing kernel NULL-check
		// at PC=0x50043E40 fire correctly and FUN_50043e38 returns cleanly.
		// This is the surgical version of the previously-reverted blanket
		// short-circuit (`d6c4c6c`): we don't change PC/control flow at
		// all, we just supply the correct sentinel value.
		// Bootstrap NThread iCleanup (offset 0x48): force NULL. In practice
		// the field is already zero in the live snapshot, but this is a
		// belt-and-braces guard against an SVC-stack push landing on the
		// shadowed range right before snapshot. The targeted FUN_50043e38
		// short-circuit elsewhere handles the case where the kernel has
		// switched to a different (non-shadowed) NThread.
		series5ShadowNThread[0x48 / 4] = 0;
		series5ShadowActive = true;
		// Fall through so the iCurrentThread store still happens.
	}
	// Mirror writes that happen inside FUN_50010E44's NThread-init body.
	// The function's prologue and epilogue ALSO push frame-saves through
	// this stack range (the auStack_218 ARRAY itself lives at
	// 0x80003BB8..0x80003DB8, overlapping the NThread). To keep the
	// shadow in sync with the kernel-state init writes (e.g. iAllocator
	// at +0x38 = 0x80003D08, written at PC=0x50010EE0), mirror any V32
	// store from FUN_50010E44 into the shadow.
	if (series5HalFix && series5ShadowActive
	    && isSeries5BootstrapNThread(virtAddr)) {
		uint32_t pc = GPRs[15] - 0xC;
		uint32_t currentSP = GPRs[13];
		// Two write paths update the shadow:
		// 1. Writes from inside FUN_50010E44's NThread-init body — these
		//    are the kernel's authoritative iAllocator / iCurrentDfcQ
		//    / etc. stores. (Original purpose of this hook.)
		// 2. Writes whose virtAddr is at or above (currentSP - 64) —
		//    i.e., STMFD pushes from any function whose stack frame
		//    overlaps the bootstrap NThread region (memset's saved LR
		//    / saved R4-R6, etc.). The 64-byte tolerance below SP
		//    captures the FIRST store of a multi-register STMFD: ARM
		//    block-data-transfer doesn't write back SP until after the
		//    first store completes, so during that first write
		//    GPRs[13] still equals the PRE-decrement SP. Without the
		//    tolerance, the first pushed register's slot doesn't get
		//    propagated to shadow; a subsequent LDR at that address
		//    then reads stale init-time data (e.g., 0x50039770 —
		//    seen at FUN_5003965c's PUSH R1,R2,R3 -> later LDR R1).
		//    Max valid STMFD width is 16 registers = 64 bytes, so
		//    the window matches the worst case exactly. Writes
		//    outside any STMFD frame still go to real RAM regardless;
		//    the shadow update is harmless for those because the
		//    read path serves real RAM when virtAddr < currentSP.
		bool isInitWrite = (pc >= Series5FunBootstrapNThreadInitStart
		                   && pc < Series5FunBootstrapNThreadInitEnd);
		bool isLiveStackPush = (virtAddr + 64 >= currentSP);
		if (isInitWrite || isLiveStackPush) {
			uint32_t off = virtAddr - 0x80003CD0u;
			uint32_t &word = series5ShadowNThread[off >> 2];
			switch (valueSize) {
			case V8: {
				uint32_t shift = (off & 3) * 8;
				word = (word & ~(0xFFu << shift)) | ((value & 0xFF) << shift);
				break;
			}
			case V16: {
				uint32_t shift = (off & 2) * 8;
				word = (word & ~(0xFFFFu << shift)) | ((value & 0xFFFF) << shift);
				break;
			}
			default:
				word = value;
				break;
			}
			// Don't return early — let the write also reach real RAM so
			// matching reads from real-RAM paths see the same value.
		}
	}
	// Series 5 EPOC R1 abort-recursion fix: at PC=0x50016CE4 the kernel
	// stores a sentinel value (0x3C) at virtual 0x80100220 — the
	// iHandlers slot of the kernel's NThread block at 0x801001F4. The
	// dispatcher at 0x500096E0 then reads this slot and tail-calls
	// 0x50002B40 with R0=0x3C. Function 0x50002B40 dereferences R0 as
	// an SHalEntry2 struct pointer, reading from virtual 0x58/0x60
	// (which map to ROM[0x2058]/[0x2060] — the exception vector
	// handler code interpreted as data). The non-zero values drive
	// the function into a STR-with-corrupted-R1 path that section-
	// faults at 0x50002BA4 — recursive abort that fires millions of
	// times during boot.
	//
	// Force the slot to 0 so the dispatcher takes the no-handler path
	// at 0x500096EC (CMP R12,#0 → BEQ skips the BNE-target dispatch).
	// Per the agent investigation: the 0x3C is broken kernel data
	// (probably an EKA1 power-state heartbeat) that was never meant
	// to be dereferenced as a pointer.
	// PSION_S5_DROP_0x3C_DISPATCH=1 re-enables the legacy hack that forces
	// any write of 0x3C to virt 0x80100220 (kernel iHandlers slot) to be
	// stored as 0 instead. The hack was added when the destructive abort
	// recovery cycle was firing — the kernel re-init wrote 0x3C as a
	// half-initialised value, then the dispatcher tried to deref it as a
	// pointer and section-faulted. With the I-bit fix (commit eddf64b7)
	// the abort cycle no longer fires, so the 0x3C write happens once in
	// the normal init path and the kernel handles it correctly. Default-
	// OFF; set to 1 only for historical comparison.
	if (series5HalFix && virtAddr == 0x80100220 && valueSize == V32 &&
	    (GPRs[15] - 0xC) == 0x50016CE4) {
		static int dropDispatch = -1;
		if (dropDispatch < 0) {
			const char *e = PSION_ENV_CSTR("PSION_S5_DROP_0x3C_DISPATCH");
			dropDispatch = (e && e[0] == '1') ? 1 : 0;
		}
		if (dropDispatch)
			value = 0;
	}
	// Series 5 EPOC R1 IPC-globals preservation. The kernel's data-page
	// init memcpy (PC=0x5004D90C inside FUN_5004D8FC, called from
	// FUN_5000FF68 line 20302) copies EKern.exe's data prototype from
	// ROM[0x500290B4] into virt 0x80100000. The prototype's slots for
	// 0x80100004 (IPC server pointer) and 0x80100008 (server queue head)
	// are 0 in the ROM image. The live values written by FUN_50007830
	// at PC=0x5000784c/0x5000785c (= 0x80005558 / 0x800055B0) get
	// clobbered to 0 by every memcpy iteration, breaking every IPC
	// issued after the first ~92ms window.
	//
	// Empirically, the boot triggers an alignment fault at PC=0x5000CF84
	// (slow-exec slot 0x8E with bogus R0=0x1F) every ~1 sim s. The abort
	// handler runs FUN_5000FF68 to "recover", which is the source of the
	// periodic memcpy. Until we find the root cause of the bad SWI arg,
	// preserve the IPC slots so the loader can actually deliver messages
	// across re-init cycles.
	//
	// Drop zero-writes to 0x80100004-0x80100020 from PC=0x5004D90C only
	// — the legitimate one-time init at PC=0x50000650 (early boot) is
	// allowed through. Both reads and writes still go through normally
	// for any other writer.
	// 2026-05-11: gated behind PSION_S5_HAL_LEGACY. The abort-handler memcpy
	// at PC=0x5004D90C is part of the destructive-cycle code path; with
	// the I-bit fix it doesn't run, so this hack is dead. Re-enable only
	// for historical comparisons.
	if (series5HalFix && valueSize == V32 && value == 0
	    && virtAddr >= 0x80100004 && virtAddr <= 0x80100020
	    && (GPRs[15] - 0xC) == 0x5004D90C
	    && PSION_ENV_BOOL("PSION_S5_HAL_LEGACY")) {
		static int hits = 0; if (hits++ < 3) log("[hack-ipc-globals-preserve] dropped zero write to va=0x%08x", virtAddr);
		return NoFault;
	}
	// PSION_S5_PRESERVE_KDATA=1 broadens the IPC-slot preservation to the
	// FULL kernel-data-init memcpy range (0x80100000-0x80100E1C — see
	// PSION_S5_MEMCPY_DUMP trace). Per the destructive-recovery notes in
	// series5.h, the kernel's abort handler re-runs this memcpy every
	// ~17.85M cycles, copying the ROM template at 0x500290D4 over the
	// kernel's data page and zeroing live pointers (iCurrentThread at
	// 0x8010061C, server queues, etc.). The first iteration is the
	// legitimate init; subsequent iterations just clobber live state
	// the kernel built up in the previous cycle.
	//
	// Strategy: skip any write to this range from PC=0x5004D90C IF
	//   (a) the new value is zero, AND
	//   (b) the current memory value is NON-zero
	// Non-zero writes still go through (so ROM constants land), and the
	// first-time init still works (memory starts as 0xFF / random RAM,
	// gets the write). Subsequent re-runs encounter live pointers and
	// skip the zero overwrite. Gated behind series5HalFix + the env var.
	//
	// **Sweep 2026-05-06: NULL RESULT.** PSION_S5_PRESERVE_KDATA=1 fires
	// 100+ times in a 5 sim sec window, correctly preserving heap pointers
	// like 0x8000598c / 0x80005d8c / 0x80004090 across abort cycles. But
	// boot progression at 30 sim sec is IDENTICAL — 234 unique_pcs in
	// both baseline and preserve modes. The kernel re-creates the same
	// objects fresh each cycle through its init code; whether a previous
	// cycle's pointer survives is irrelevant — what state happened to be
	// in 0x80100xxx doesn't change which code path the kernel takes.
	// Kept opt-in so future investigation can extend it (e.g., to the
	// heap chunk at 0x80004000+ which the kernel destroys via separate
	// PCs 0x5001472C / 0x50014DC4 / 0x5003D648 / 0x5003A788, NOT via
	// the 0x5004D90C memcpy this guards).
	if (series5HalFix && valueSize == V32 && value == 0
	    && virtAddr >= 0x80100000 && virtAddr <= 0x80100E1C
	    && (GPRs[15] - 0xC) == 0x5004D90C) {
		static int preserveKData = -1;
		if (preserveKData < 0) {
			const char *e = PSION_ENV_CSTR("PSION_S5_PRESERVE_KDATA");
			preserveKData = (e && std::atoi(e) != 0) ? 1 : 0;
		}
		if (preserveKData) {
			// Peek the current word at this address; if it's a non-zero
			// pointer / value, suppress the zero-overwrite.
			auto cur = readVirtualDebug(virtAddr, V32);
			if (cur.has_value() && cur.value() != 0) {
				return NoFault;
			}
		}
	}
	// PSION_S5_MEMCPY_DUMP=1 logs every write made by the destructive
	// kernel-data-init memcpy at PC=0x5004D90C (FUN_5004D8FC inside the
	// abort handler). Used to map out which kernel-data slots get
	// clobbered every recovery cycle so we can decide which to preserve.
	if (series5HalFix && (GPRs[15] - 0xC) == 0x5004D90C) {
		static int memcpyDump = -1;
		if (memcpyDump < 0) {
			const char *e = PSION_ENV_CSTR("PSION_S5_MEMCPY_DUMP");
			memcpyDump = (e && std::atoi(e) != 0) ? 1 : 0;
		}
		if (memcpyDump) {
			log("[memcpy] insncyc=%llu va=%08x sz=%d val=%08x cpsr=%x r0=%08x r1=%08x r2=%08x r3=%08x lr=%08x",
				(unsigned long long)insnCycleApprox,
				virtAddr, (int)valueSize, value, CPSR & 0x1F,
				GPRs[0], GPRs[1], GPRs[2], GPRs[3], GPRs[14]);
		}
	}
	// Series 5 EPOC R1 HAL-root-stack-overlap workaround.
	// On MC218 the HAL root object is at virtual 0x80005484 — well away
	// from the SVC stack. On Series 5 the same kernel code allocates the
	// HAL root at 0x80105794 (its vtable slot at 0x80105790) which IS the
	// SVC stack region, so STMDB pushes clobber the vtable slot with a
	// stale LR value from a previous BL inside the default-stub code
	// range 0x500031xx-0x500034xx.
	// The correct fix is a different MMU mapping that separates stack
	// and heap; pending that, drop writes to the known-vulnerable slot
	// when the value looks like a stack-pushed return address into the
	// default-stub code region. The kernel's real HAL method pointers
	// are all in the 0x5000Bxxx-0x5000Dxxx range, so this heuristic is
	// safe for the vtable slot itself.
	//
	// 2026-05-11: NARROWED from the original 0x80105700..0x801057FF
	// range to ONLY the actual HAL vtable slot at 0x80105790. The wider
	// range was incorrectly dropping legitimate kernel stack pushes —
	// notably the IRQ handler's STMFD at PC=0x5001967C, which pushes
	// LR_irq=0x500031F8 (a real return address into the SWI 0x6C
	// handler at PC=0x500031F0) to virt 0x8010578C as part of a
	// 6-register exception save. Dropping that write caused the IRQ
	// LDMFD return to pop a stale LR (=0x500194FC from a prior SVC
	// push at the SAME address), which then routed the SWI 0x6C path
	// through the wrong return point, resumed user code with R0 =
	// iCurrentThread (the supervisor address, not the heap pointer),
	// and panicked downstream as KERN-NO-SESSION reason 20 in
	// FUN_500096CC. See series5.h "2026-05-11 (cont. 3)" notes.
	// 2026-05-11: gated behind PSION_S5_HAL_LEGACY. The HAL-vtable-poisoning
	// scenario was a symptom of the destructive cycle; with the I-bit fix
	// the cycle no longer fires, so the vtable slot is no longer written
	// with stub-code pointers during normal boot. Drop the protection in
	// the default path so legitimate writes from the running OS aren't
	// silently dropped (which can corrupt user-mode-visible state, e.g.
	// app icons / save handlers that happen to use this slot).
	if (series5HalFix && valueSize == V32
	    && virtAddr == 0x80105790
	    && value >= 0x50003000 && value < 0x50003500
	    && PSION_ENV_BOOL("PSION_S5_HAL_LEGACY")) {
		static int hits = 0; if (hits++ < 3) log("[hack-hal-vtable-protect] dropped value 0x%08x write to 0x80105790", value);
		return NoFault;
	}
	// The kernel's heap allocates the HAL root object at virtual 0x80105794
	// (whose vtable method pointer lives at 0x80105790). During normal
	// execution the kernel's SVC stack grows down through this region, so
	// STMDB pushes onto the SVC stack clobber the vtable slot with stale
	// saved registers (typically an LR value from a prior BL return that
	// happens to point at an instruction inside the default-stub region
	// 0x500031Fx). The resulting bogus vtable dispatch faults.
	// Protect the vtable slot from any write that looks like a stack-push
	// of a code pointer into a default-stub — the valid HAL method
	// pointers that the kernel legitimately installs at this slot are in
	// the 0x5000Bxxx-0x5000Dxxx range, so a write in 0x500030xx-0x500034xx
	// is pretty safely a stack-push poisoning.
	if (PSION_ENV_BOOL("PSION_SERIES5_HAL_FIX") && valueSize == V32
	    && virtAddr == 0x80105790
	    && value >= 0x50003000 && value < 0x50003500) {
		return NoFault;
	}
	if (cfDiagEnabled && virtAddr >= 0x80000000 && virtAddr < 0x80007000) {
		uint32_t pc = GPRs[15] - 0xC;
		log("DIAG write va=%08x size=%d value=%08x pc=%08x lr=%08x CPSR=%08x mode=%02x",
		    virtAddr, (int)valueSize, value, pc, GPRs[14], CPSR, CPSR & 0x1F);
	}
	// PSION_WATCH=<hex>[-<hex>] logs every virtual write in that range with PC + LR,
	// for tracking down which ROM routine should be initialising a particular
	// kernel global. Set as "0x8010061C" or "0x80100600-0x80100640".
	{
		static bool inited = false;
		static uint32_t watchStart = 0, watchEnd = 0;
		if (!inited) {
			inited = true;
			const char *e = PSION_ENV_CSTR("PSION_WATCH");
			if (e) {
				char *endp;
				watchStart = (uint32_t)std::strtoul(e, &endp, 0);
				watchEnd = watchStart + 4;
				if (*endp == '-') watchEnd = (uint32_t)std::strtoul(endp + 1, nullptr, 0);
			}
		}
		if (watchStart && virtAddr >= watchStart && virtAddr < watchEnd) {
			uint32_t pc = GPRs[15] - 0xC;
			log("WATCH write va=%08x size=%d value=%08x pc=%08x lr=%08x r0=%08x r1=%08x r4=%08x r12=%08x sp=%08x",
				virtAddr, (int)valueSize, value, pc, GPRs[14],
				GPRs[0], GPRs[1], GPRs[4], GPRs[12], GPRs[13]);
		}
	}
	if (isAlignmentFaultEnabled() && valueSize == V32 && virtAddr & 3)
		return encodeFault(AlignmentFault, 0, virtAddr);
	if (isAlignmentFaultEnabled() && valueSize == V16 && virtAddr & 1)
		return encodeFault(AlignmentFault, 0, virtAddr);

	if (!isMMUEnabled()) {
		// direct virtual -> physical mapping, sans MMU
		if (!writePhysical(value, virtAddr, valueSize))
			return encodeFault(NonMMUError, 0, virtAddr);
	} else {
		auto translated = translateAddressUsingTlb(virtAddr);
        if (std::holds_alternative<MMUFault>(translated)) {
            // Series 5 lazy stack-grow alias (write side). See block comment
            // above readVirtual(). When the TLB walk fails inside the
            // stack-guard range, persist the write into the stable backing
            // slice so a later LDMIA pop sees the same bytes.
            MMUFault f = std::get<MMUFault>(translated);
            if (series5HalFix && (f & MMUFaultTypeMask) == PageTranslationFault
                && isSeries5StackGuard(virtAddr)) {
                series5LogStackGuardHit(virtAddr, true);
                if (series5LazyPageGrowEnabled() && series5InstallLazyMapping(virtAddr)) {
                    auto retried = translateAddressUsingTlb(virtAddr);
                    if (std::holds_alternative<TlbEntry *>(retried)) {
                        auto te = std::get<TlbEntry *>(retried);
                        uint32_t pa = physAddrFromTlbEntry(te, virtAddr);
                        if (writePhysical(value, pa, valueSize))
                            return NoFault;
                    }
                }
                uint32_t aliasPhys = series5StackGuardAlias(virtAddr);
                if (writePhysical(value, aliasPhys, valueSize))
                    return NoFault;
            }
            return f;
        }

		// resolve this boy
        auto tlbEntry = std::get<TlbEntry *>(translated);

		if (auto f = checkAccessPermissions(tlbEntry, virtAddr, true); f != NoFault)
			return f;

		uint32_t physAddr = physAddrFromTlbEntry(tlbEntry, virtAddr);
		int domain = (tlbEntry->lv1Entry >> 5) & 0xF;
		bool isPage = (tlbEntry->lv2Entry != 0);

		if (!writePhysical(value, physAddr, valueSize)) {
			if (PSION_ENV_BOOL("PSION_MMU_FAULT_TRACE")) {
				std::fprintf(stderr, "[mmu] writeVirt fault va=%08x pa=%08x pc=%08x value=%08x\n",
					virtAddr, physAddr, GPRs[15] - 0xC, value);
			}
			return encodeFaultSorP(SorPOtherBusError, isPage, domain, virtAddr);
		}
	}

	// commit to cache if all was good
#ifdef ARM710T_CACHE
	writeCached(value, virtAddr, valueSize);
#endif
	return NoFault;
}



// TLB
#ifdef ARM710T_TLB
void ARM710::flushTlb() {
	for (TlbEntry &e : tlb)
		e = {0, 0, 0, 0};
}
void ARM710::flushTlb(uint32_t virtAddr) {
	for (TlbEntry &e : tlb) {
		if (e.addrMask && (virtAddr & e.addrMask) == e.addr) {
			e = {0, 0, 0, 0};
			break;
		}
	}
}
#endif

ARM710::TlbEntry *ARM710::_allocateTlbEntry(uint32_t addrMask, uint32_t addr) {
#ifdef ARM710T_TLB
	TlbEntry *entry = &tlb[nextTlbIndex];
	nextTlbIndex = (nextTlbIndex + 1) % TlbSize;
#else
	TlbEntry *entry = &singleTlbEntry;
#endif
	entry->addrMask = addrMask;
	entry->addr = addr & addrMask;
	return entry;
}

std::variant<ARM710::TlbEntry *, ARM710::MMUFault> ARM710::translateAddressUsingTlb(uint32_t virtAddr, TlbEntry *useMe) {
#ifdef ARM710T_TLB
	// fast path: the slot that translated this page last time, if it still
	// holds a matching translation (see tlbFastMap declaration)
	if (TlbEntry *f = tlbFastMap[(virtAddr >> 12) & (TlbFastMapSize - 1)]) {
		if (f->addrMask && (virtAddr & f->addrMask) == f->addr)
			return f;
	}
	// first things first, do we have a matching entry in the TLB?
	for (TlbEntry &e : tlb) {
		if (e.addrMask && (virtAddr & e.addrMask) == e.addr) {
			noteTlbFastMap(virtAddr, &e);
			return &e;
		}
	}
#endif

	// no, so do a page table walk
	TlbEntry *entry;
	uint32_t tableIndex = virtAddr >> 20;

	// fetch the Level 1 entry
	auto lv1EntryOpt = readPhysical(cp15_translationTableBase | (tableIndex << 2), V32);
	if (!lv1EntryOpt.has_value())
		return Lv1TranslationError;
	auto lv1Entry = lv1EntryOpt.value();
	int domain = (lv1Entry >> 5) & 0xF;

	switch (lv1Entry & 3) {
	case 0:
	case 3:
		// invalid!
		return encodeFault(SectionTranslationFault, domain, virtAddr);
	case 2:
		// a Section entry is straightforward
		// we just throw that immediately into the TLB
		entry = useMe ? useMe : _allocateTlbEntry(0xFFF00000, virtAddr);
		entry->lv1Entry = lv1Entry;
		entry->lv2Entry = 0;
		if (!useMe) noteTlbFastMap(virtAddr, entry);
		return entry;
	case 1:
		// a Page requires a Level 2 read
		uint32_t pageTableAddr = lv1Entry & 0xFFFFFC00;
		uint32_t lv2TableIndex = (virtAddr >> 12) & 0xFF;

		auto lv2EntryOpt = readPhysical(pageTableAddr | (lv2TableIndex << 2), V32);
		if (!lv2EntryOpt.has_value())
			return encodeFault(Lv2TranslationError, domain, virtAddr);
		auto lv2Entry = lv2EntryOpt.value();

		switch (lv2Entry & 3) {
		case 0:
		case 3:
			// invalid!
			return encodeFault(PageTranslationFault, domain, virtAddr);
		case 1:
			// Large 64kb page
			entry = useMe ? useMe : _allocateTlbEntry(0xFFFF0000, virtAddr);
			entry->lv1Entry = lv1Entry;
			entry->lv2Entry = lv2Entry;
			if (!useMe) noteTlbFastMap(virtAddr, entry);
			return entry;
		case 2:
			// Small 4kb page
			entry = useMe ? useMe : _allocateTlbEntry(0xFFFFF000, virtAddr);
			entry->lv1Entry = lv1Entry;
			entry->lv2Entry = lv2Entry;
			if (!useMe) noteTlbFastMap(virtAddr, entry);
			return entry;
		}
	}

	// we should never get here as the switch covers 0, 1, 2, 3
	// but this satisfies a compiler warning
	return SectionTranslationFault;
}



ARM710::MMUFault ARM710::checkAccessPermissions(ARM710::TlbEntry *entry, uint32_t virtAddr, bool isWrite) const {
	int domain;
	int accessPerms;
	bool isPage;

	// extract info from the entries
	domain = (entry->lv1Entry >> 5) & 0xF;
	if (entry->lv2Entry) {
		// Page
		accessPerms = (entry->lv2Entry >> 4) & 0xFF;

		int permIndex;
		if ((entry->lv2Entry & 3) == 1) // Large 64kb
			permIndex = (virtAddr >> 14) & 3;
		else                            // Small 4kb
			permIndex = (virtAddr >> 10) & 3;

		accessPerms >>= (permIndex * 2);
		accessPerms &= 3;
		isPage = true;
	} else {
		// Section
		accessPerms = (entry->lv1Entry >> 10) & 3;
		isPage = false;
	}

	// now, do our checks
	//
	// T6 fix: read the 2-bit DACR field for the page's domain instead of
	// hard-coding Manager. This restores real ARM behaviour: domains can
	// be configured as No-Access (0), Client (1, AP-checked), Reserved (2,
	// treated as No-Access here per ARM7 datasheet), or Manager (3, always
	// allowed).
	//
	// Historical workaround: EPOC R5 on the netBook restricts DACR to 0x30
	// after the splash painter completes (only domain 2 = manager). Our
	// patched L1 entries used domain 0 and the kernel's own L1 entries use
	// domains 5/6, so a strict DACR read caused every privileged fetch to
	// take a domain fault. With T6 enforced, that hazard is back. An env
	// var keeps the legacy "force Manager" behaviour available for boot
	// regression bisection: PSION_FORCE_MANAGER_DOMAINS=1.
	int primaryAccessControls;
	{
		static int forceManager = -1;
		if (forceManager < 0) {
			const char *e = PSION_ENV_CSTR("PSION_FORCE_MANAGER_DOMAINS");
			forceManager = (e && e[0] == '1') ? 1 : 0;
		}
		if (forceManager) {
			primaryAccessControls = 3;
		} else {
			primaryAccessControls = (cp15_domainAccessControl >> (domain * 2)) & 3;
		}
	}

	// Manager: always allowed
	if (primaryAccessControls == 3)
		return NoFault;

	// Client: enforce checks!
	if (primaryAccessControls == 1) {
#define OK_IF_TRUE(b) return ((b) ? NoFault : encodeFaultSorP(SorPPermissionFault, isPage, domain, virtAddr))
		bool System = cp15_control & 0x100;
		bool ROM = cp15_control & 0x200;

		if (accessPerms == 0) {
			if (!System && !ROM) {
				// 00/0/0: Any access generates a permission fault
				OK_IF_TRUE(false);
			} else if (System && !ROM) {
				// 00/1/0: Supervisor read only permitted
				OK_IF_TRUE(!isWrite && isPrivileged());
			} else if (!System && ROM) {
				// 00/0/1: Any write generates a permission fault
				OK_IF_TRUE(!isWrite);
			} else /*if (System && ROM)*/ {
				// Reserved
				OK_IF_TRUE(false);
			}
		} else if (accessPerms == 1) {
			// 01/x/x: Access allowed only in Supervisor mode
			OK_IF_TRUE(isPrivileged());
		} else if (accessPerms == 2) {
			// 10/x/x: Writes in User mode cause permission fault
			OK_IF_TRUE(!isWrite || isPrivileged());
		} else /*if (accessPerms == 3)*/ {
			// 11/x/x: All access types permitted in both modes
			OK_IF_TRUE(true);
		}
#undef OK_IF_TRUE
	}

	// No Access or Reserved: never allowed (Domain Fault)
	return encodeFaultSorP(SorPDomainFault, isPage, domain, virtAddr);
}


void ARM710::reportFault(MMUFault fault) {
	if (fault != NoFault) {
		if ((fault & 0xF) != NonMMUError) {
			// L8: explicit narrowing — cp15_faultStatus is 8 bits, the
			// type+domain nybbles fit in that exactly.
			cp15_faultStatus = (uint8_t)(fault & (MMUFaultTypeMask | MMUFaultDomainMask));
			cp15_faultAddress = (uint32_t)(fault >> MMUFaultAddressShift);
		}

		static const char *faultTypes[] = {
			"NoFault",
			"AlignmentFault",
			"???",
			"NonMMUError",
			"SectionLinefetchError",
			"SectionTranslationFault",
			"PageLinefetchError",
			"PageTranslationFault",
			"SectionOtherBusError",
			"SectionDomainFault",
			"PageOtherBusError",
			"PageDomainFault",
			"Lv1TranslationError",
			"SectionPermissionFault",
			"Lv2TranslationError",
			"PagePermissionFault"
		};
		// Throttle: identical fault tuples repeated more than a handful
		// of times flood stderr/log without adding signal. Track the
		// last-seen tuple and silence after the third repeat.
		uint32_t pcKey = GPRs[15] - 0xC;
		uint32_t addrKey = fault >> MMUFaultAddressShift;
		uint32_t typeKey = fault & MMUFaultTypeMask;
		static uint32_t lastPc = 0, lastAddr = 0, lastType = 0xFFFFFFFFu;
		static int sameRunCount = 0;
		if (pcKey == lastPc && addrKey == lastAddr && typeKey == lastType) {
			sameRunCount++;
			if (sameRunCount == 3)
				log("⚠️ Fault (further repeats of pc=%08x address=%08x suppressed)",
				    pcKey, addrKey);
			if (sameRunCount >= 3) {
				// fall through to the rest of reportFault but skip the spammy log
				goto skipFaultLog;
			}
		} else {
			lastPc = pcKey; lastAddr = addrKey; lastType = typeKey;
			sameRunCount = 1;
		}
		log("⚠️ Fault type=%s domain=%u address=%08x pc=%08x lr=%08x sp=%08x cpsr=%08x",
			faultTypes[fault & MMUFaultTypeMask],
			(unsigned)((fault & MMUFaultDomainMask) >> MMUFaultDomainShift),
			(uint32_t)(fault >> MMUFaultAddressShift),
			GPRs[15] - 0xC, GPRs[14], GPRs[13], CPSR);
		// Probe what's mapped near the faulting address — page-by-page.
		// Useful when chasing kernel-stack-overflow style faults: the
		// emulator can show which adjacent pages exist vs. which are
		// missing, so we can tell if we mapped one page when the kernel
		// allocated several.
		if (PSION_ENV_BOOL("PSION_FAULT_PROBE")) {
			uint32_t fa = (uint32_t)(fault >> MMUFaultAddressShift);
			uint32_t baseSec = fa & 0xFFFFF000u;
			for (int delta = -3; delta <= 3; delta++) {
				uint32_t va = baseSec + delta * 0x1000;
				auto p = virtToPhys(va);
				if (p.has_value())
					log("    map probe: virt=%08x -> phys=%08x", va, p.value());
				else
					log("    map probe: virt=%08x -> UNMAPPED", va);
			}
		}
		skipFaultLog:;
		if (PSION_ENV_BOOL("PSION_FAULT_REGS")) {
			log("   r0=%08x r1=%08x r2=%08x r3=%08x r4=%08x r5=%08x r6=%08x r7=%08x",
				GPRs[0], GPRs[1], GPRs[2], GPRs[3], GPRs[4], GPRs[5], GPRs[6], GPRs[7]);
			log("   r8=%08x r9=%08x r10=%08x r11=%08x r12=%08x sp=%08x lr=%08x cpsr=%08x",
				GPRs[8], GPRs[9], GPRs[10], GPRs[11], GPRs[12], GPRs[13], GPRs[14], CPSR);
			// Dump the value at the "suspect" global + a few neighbours
			auto peek = [&](uint32_t va) -> uint32_t {
				auto r = readVirtualDebug(va, V32);
				return r.value_or(0xDEADBEEFu);
			};
			log("   *0x80100614=%08x *0x80100618=%08x *0x8010061c=%08x *0x80100620=%08x *0x80100624=%08x",
				peek(0x80100614), peek(0x80100618), peek(0x8010061c), peek(0x80100620), peek(0x80100624));
			// Also dump the global object pointer chain used by the virtual
			// call site 0x500195F0 (the one that landed us here):
			//   *(0x80100388) = vtable-bearing object
			//   *(*0x80100388 - 4) = method entry (should be valid ARM code)
			uint32_t vtableObj = peek(0x80100388);
			log("   *0x80100388=%08x", vtableObj);
			if (vtableObj != 0xDEADBEEFu && vtableObj >= 0x80000000) {
				log("   vtable[-4]=*(0x%08x)=%08x", vtableObj - 4, peek(vtableObj - 4));
			}
			// Dump the stack near the undef-mode SP so we can see the
			// saved LR chain and figure out who called the faulting method.
			uint32_t sp = GPRs[13];
			log("   stack (SP=%08x):", sp);
			for (int off = 0; off < 0x40; off += 4) {
				log("     SP+0x%02x: %08x", off, peek(sp + off));
			}
		}

		// this signals a branch to DataAbort after the
		// instruction is done executing
		faultTriggeredThisCycle = true;
	}
}


void ARM710::log(const char *format, ...) {
	if (!loggingEnabled || !logger) return;

	char buffer[1024];

	va_list vaList;
	va_start(vaList, format);
	vsnprintf(buffer, sizeof(buffer), format, vaList);
	va_end(vaList);

	logger(buffer);
}

void ARM710::logPcHistory() {
	for (int i = 0; i < PcHistoryCount; i++) {
		pcHistoryIndex = (pcHistoryIndex + 1) % PcHistoryCount;
		log("%03d: %08x %08x", i, pcHistory[pcHistoryIndex].addr, pcHistory[pcHistoryIndex].insn);
	}
}
