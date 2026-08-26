#include "arm_jit_runtime.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

void ARM710::jitCheckBridgeHandoff(uint32_t pc) {
	static int reported = 0;
	if (reported >= 20) return;
	const char *what = nullptr;
	uint32_t got = 0, want = 0;
	if (prefetchCount != 2)       { what = "prefetchCount"; got = (uint32_t)prefetchCount; want = 2; }
	else if (GPRs[15] != pc + 8)  { what = "r15";           got = GPRs[15];  want = pc + 8; }
	else {
		auto w = readVirtualDebug(pc, V32);
		if (w && *w != prefetch[1]) { what = "prefetch[1]"; got = prefetch[1]; want = *w; }
	}
	if (!what) return;
	reported++;
	std::fprintf(stderr, "[jit] BRIDGE HAND-OFF WRONG at pc=%08x: %s is %08x, expected %08x\n",
	             pc, what, got, want);
}

// Names the first place a generated region and the interpreter disagree. Lives
// here rather than in arm710.cpp because it exists only for the JIT.
void ARM710::jitReportDivergence(uint32_t entryPc, int ticks,
                                 uint32_t jitCycles, uint32_t refCycles,
                                 const JitCheckpoint &jit, const JitCheckpoint &ref) {
	std::fprintf(stderr, "[jit] DIVERGENCE at entry %08x after %d ticks\n", entryPc, ticks);
	if (jitCycles != refCycles)
		std::fprintf(stderr, "  cycles: jit %u, interpreter %u\n", jitCycles, refCycles);
	for (int i = 0; i < 16; i++)
		if (jit.gprs[i] != ref.gprs[i])
			std::fprintf(stderr, "  r%-2d: jit %08x, interpreter %08x\n", i, jit.gprs[i], ref.gprs[i]);
	if (jit.cpsr != ref.cpsr)
		std::fprintf(stderr, "  CPSR: jit %08x, interpreter %08x (NZCV %x vs %x)\n",
		             jit.cpsr, ref.cpsr, jit.cpsr >> 28, ref.cpsr >> 28);
	if (jit.prefetch[1] != ref.prefetch[1] || jit.prefetch[0] != ref.prefetch[0] ||
	    jit.prefetchCount != ref.prefetchCount)
		std::fprintf(stderr, "  pipeline: jit [%08x %08x]/%d, interpreter [%08x %08x]/%d\n",
		             jit.prefetch[1], jit.prefetch[0], jit.prefetchCount,
		             ref.prefetch[1], ref.prefetch[0], ref.prefetchCount);
	if (jit.insnCycle != ref.insnCycle)
		std::fprintf(stderr, "  insnCycleApprox: jit %llu, interpreter %llu\n",
		             (unsigned long long)jit.insnCycle, (unsigned long long)ref.insnCycle);
	if (jit.sink != ref.sink)
		std::fprintf(stderr, "  device cycles: jit %lld, interpreter %lld\n",
		             (long long)jit.sink, (long long)ref.sink);
}

namespace armjit {

extern "C" uint32_t psionJitExecOne(uint32_t cpu, uint32_t insn) {
	return reinterpret_cast<ARM710 *>(static_cast<uintptr_t>(cpu))->jitExecOne(insn);
}

extern "C" uint32_t psionJitStepOne(uint32_t cpu, uint32_t pc) {
	ARM710 *c = reinterpret_cast<ARM710 *>(static_cast<uintptr_t>(cpu));
	if (Runtime::checking()) c->jitCheckBridgeHandoff(pc);
	return c->jitStepOne();
}

namespace {

// How far a region may trace. Long enough that a loop body fits in one region,
// short enough that a compile stays cheap and a stale-word check stays cheap.
// PSION_JIT_TRACE_INSNS overrides it. It used to be 24, which was well below
// what the burst engine retires per entry — but raising it alone did nothing,
// because a trace ended at the first unconditional branch long before it ran
// out of room. Now that a trace follows those (Layout::allowChain), the limit
// binds again.
//
// 96 is chosen against the TICK budget, not the instruction count: the loop
// head admits a pass only if a whole pass fits in the caller's remaining ticks,
// and the caller's cap is 128. A trace of 96 positions with the full seven
// followed branches costs 96 + 2*7 = 110 tick-equivalents, so it still fits and
// the region is enterable. A longer one would compile and then never run.
inline int maxTraceInsns() {
	static const int v = [] {
		const char *e = std::getenv("PSION_JIT_TRACE_INSNS");
		int n = e ? (int)std::strtol(e, nullptr, 0) : 96;
		return n < 1 ? 1 : n;
	}();
	return v;
}
// Below this a region is not worth the call: the measured cost of one call per
// instruction is ~5.8% of runtime, so a two-instruction region gives most of
// that straight back.
constexpr int kMinTraceInsns = 4;
// Entries at one address before it is worth compiling. Every region is a
// separate WebAssembly.Module — a compile and a permanent allocation in the
// engine — so cold code must not pay for one. Measured over a Series 7 boot
// there are 115,409 distinct block starts against 45.2 M executions, so waiting
// gives up almost nothing and avoids almost everything.
[[maybe_unused]] uint32_t hotThreshold() {
	static const uint32_t v = []{
		const char *e = std::getenv("PSION_JIT_HOT");
		// High on purpose. V8 charges for every byte it compiles, and a region
		// is entered only ~100 times over a boot (391 at the ceiling, measured
		// across all block starts), so a region that is merely warm never
		// repays its own compile. Measured on a Series 7 boot: 64 costs 9%,
		// 16,000 costs nothing and still catches everything an application
		// re-enters.
		return e ? (uint32_t)std::strtoul(e, nullptr, 0) : 16000u;
	}();
	return v;
}
// Hard ceiling on live regions. Every region is its own WebAssembly.Module and
// the engine keeps one alive for as long as its function is reachable from the
// table, so this is the real memory bound — and it is the reason the first
// attempt, which compiled unconditionally, exhausted an 8 GB heap.
// Regions per generated module. Instantiation cost is per module, not per
// function, so this divides it by the batch size; 128 turns thousands of
// instantiations into tens.
constexpr int kBatchSize = 128;
// Entries to wait before linking a partly-filled batch, so regions compiled in
// a quiet stretch still become reachable.
constexpr long long kBatchFlushEntries = 20000;

[[maybe_unused]] int maxCompiled() {
	static const int v = []{
		const char *e = std::getenv("PSION_JIT_MAX");
		// The ceiling on live regions. 8,192 was reached by 4.1 M entries on an
		// application workload — the cap itself was throttling the thing it was
		// meant to protect.
		return e ? (int)std::strtol(e, nullptr, 0) : 65536;
	}();
	return v;
}

// PSION_JIT_CHECK_ALL=1 also re-runs regions that are not re-run safe.
// See the note at the re-run check: this corrupts the run it is used on.
bool checkAll() {
	static const bool on = [] {
		const char *e = std::getenv("PSION_JIT_CHECK_ALL");
		return e && e[0] && e[0] != '0';
	}();
	return on;
}

// The tick cap a region honours.
//
// Defaults to the caller's, which is the burst engine's cap (kBurstTicks) — and
// that is what keeps the caller's per-batch device housekeeping on the cadence
// it was tuned for. PSION_JIT_MAX_TICKS raises it past the caller's, which is
// the whole point of a loop region: one entry that runs the body until its
// budget is spent. How far it can be raised is a property of the SoC's
// per-batch hooks, not of the code generator, and it is not far: on the netBook
// running an image off a CF card the boot stops working somewhere between 128
// and 2048 (see docs/netbook-s7-performance.md), which is why the burst cap is
// where it is and why raising this beyond it is opt-in.
[[maybe_unused]] int jitMaxTicks(int callerCap) {
	static const int v = [] {
		const char *e = std::getenv("PSION_JIT_MAX_TICKS");
		return e ? (int)std::strtol(e, nullptr, 0) : 0;
	}();
	return v > 0 ? v : callerCap;
}

// Compile only regions with a loop edge in them. DEFAULT ON;
// PSION_JIT_LOOPS_ONLY=0 compiles everything.
//
// This is what the arithmetic says. A region costs about 23 us to compile —
// most of it V8's, instantiating the module — and saves about 13 ns per
// instruction it runs, so it has to retire roughly 1,800 instructions before it
// has paid for itself. A straight-line region of thirteen positions has to be
// entered a hundred and forty times to get there, and on a flat profile most
// never are: compiling everything measured 4.16-4.51 s against the
// interpreter's 4.00-4.19, and compiling only loops measured 3.94-4.05.
//
// A loop is a different proposition — one entry runs the body until its budget
// is spent — and it is the only shape that reliably clears the bar.
[[maybe_unused]] bool loopsOnly() {
	static const bool on = [] {
		const char *e = std::getenv("PSION_JIT_LOOPS_ONLY");
		return !e || (e[0] && e[0] != '0');
	}();
	return on;
}

// The tick cap a LOOPING region gets, as opposed to the caller's.
//
// A loop compiled and then cut off after the caller's hundred and twenty-eight
// ticks is most of the compile cost for none of the benefit: measured, loops at
// the caller's cap ran 4.26-4.42 s against 3.94-4.05 with this. What makes it
// safe is that a loop stays where it is — the SoC's per-batch housekeeping is
// delayed, but no address the batch loop watches for goes past unobserved,
// because the region is not going anywhere. PSION_JIT_MAX_TICKS overrides it.
[[maybe_unused]] int loopTickCap() {
	static const int v = [] {
		const char *e = std::getenv("PSION_JIT_LOOP_TICKS");
		return e ? (int)std::strtol(e, nullptr, 0) : 2048;
	}();
	return v;
}

// Hash the words a region was compiled from and re-check them on every entry.
//
// This used to be on by default, because the page-validity token does not see a
// host-side write — a card or ROM image copied straight into the RAM/ROM buffer
// — and a ROM page is marked immutable and never dropped at all. The cost was
// real: about twenty loads and a multiply each per entry, measured at 0.4 s of a
// 4.4 s run.
//
// It is off by default now because that hole is closed at the source instead:
// every site that replaces guest code host-side calls
// ARM710::onGuestCodeReplaced(), which drops every compiled region. What is left
// for the token to catch — a guest store into its own code page, a TLB flush —
// it already caught. PSION_JIT_VERIFY_WORDS=1 turns the hash back on for a run
// that wants to prove there is no third way in.
[[maybe_unused]] bool verifyWords() {
	static const bool on = [] {
		const char *e = std::getenv("PSION_JIT_VERIFY_WORDS");
		return e && e[0] && e[0] != '0';
	}();
	return on;
}

bool envFlag(const char *name) {
	const char *v = std::getenv(name);
	return v && v[0] && v[0] != '0';
}

}  // namespace

bool Runtime::enabled() {
#ifdef __EMSCRIPTEN__
	static const bool on = envFlag("PSION_JIT");
	return on;
#else
	// No WebAssembly engine to instantiate into. The compiler is still built
	// and tested natively (tests/unit/arm_jit_test) — only the dispatch is
	// browser-only.
	return false;
#endif
}

bool Runtime::checking() {
	static const bool on = envFlag("PSION_JIT_CHECK");
	return on;
}

bool Runtime::reg_loops(const Slot &s) { return s.loops; }

uint32_t Runtime::hashWords(const uint32_t *w, int n) {
	uint32_t h = 2166136261u;                 // FNV-1a
	for (int i = 0; i < n; i++) { h ^= w[i]; h *= 16777619u; }
	return h;
}

uint32_t Runtime::hashRuns(const Slot &s, const uint32_t *entryWords) {
	uint32_t h = 2166136261u;                 // FNV-1a, same as hashWords
	for (int r = 0; r < s.runCount; r++) {
		const uint32_t *w = entryWords + s.runOff[r];
		for (int i = 0; i < (int)s.runLen[r]; i++) { h ^= w[i]; h *= 16777619u; }
	}
	return h;
}

Runtime::~Runtime() { dumpStats(); }

void Runtime::dumpStats() const {
	if (!stats_.entries && !stats_.compiles && !stats_.dirtyEntry) return;
	std::fprintf(stderr,
	    "[jit] regions=%lld in %lld modules, %lld KB of wasm (declined %lld, "
	    "budget-capped %lld, evictions %lld, slot conflicts %lld)\n"
	    "[jit] entries=%lld  cycles=%lld  ticks=%lld  cycles/entry=%.1f\n"
	    "[jit] warm-ups=%lld  stale: token %lld, words %lld  orphaned=%lld\n"
	    "[jit] bursts the dispatcher could not be entered from (dirty pipeline)=%lld\n",
	    stats_.compiles, stats_.batches, stats_.moduleBytes / 1024,
	    stats_.compileFailures, stats_.budgetHits, stats_.evictions,
	    stats_.conflicts,
	    stats_.entries, stats_.cycles, stats_.ticks,
	    stats_.entries ? (double)stats_.cycles / stats_.entries : 0.0,
	    stats_.warmups, stats_.staleToken, stats_.staleWords, stats_.orphaned,
	    stats_.dirtyEntry);
	std::fprintf(stderr, "[jit] trace-stop addresses in effect: %lld\n", stats_.traceStopCount);
	if (stats_.compiles) {
		static const char *const kStop[8] = {
			"trace limit", "off page", "already traced", "device watch address",
			"branch not followed", "mode change", "bridge refused", "branch refused"
		};
		std::fprintf(stderr, "[jit] compiled span avg=%.1f insns (%.0f%% bridged, "
		             "%.0f%% inlined loads, %.0f%% literals), %.2f branches followed\n",
		             (double)stats_.compiledSpan / (double)stats_.compiles,
		             100.0 * (double)stats_.compiledBridged / (double)stats_.compiledSpan,
		             100.0 * (double)stats_.compiledLoads / (double)stats_.compiledSpan,
		             100.0 * (double)stats_.compiledLiterals / (double)stats_.compiledSpan,
		             (double)stats_.compiledChains / (double)stats_.compiles);
		static const char *const kKind[16] = {
			"uncached", "slow", "dataproc", "ldr/str", "ldm/stm", "branch",
			"mul", "mull", "swp", "halfword", "?", "?", "?", "?", "?", "?"
		};
		std::fprintf(stderr, "[jit] bridged by kind:");
		for (int i = 0; i < 16; i++)
			if (stats_.bridgedKind[i])
				std::fprintf(stderr, " %s=%lld", kKind[i], stats_.bridgedKind[i]);
		std::fprintf(stderr, "\n");
		std::fprintf(stderr, "[jit] bridged ldr/str: store=%lld pc-relative=%lld "
		             "into-pc=%lld other=%lld\n",
		             stats_.bridgedLdrWhy[0], stats_.bridgedLdrWhy[1],
		             stats_.bridgedLdrWhy[2], stats_.bridgedLdrWhy[3]);
		std::fprintf(stderr, "[jit] traces ended on:");
		for (int i = 0; i < 8; i++)
			if (stats_.stopReason[i])
				std::fprintf(stderr, " %s=%lld", kStop[i], stats_.stopReason[i]);
		std::fprintf(stderr, "\n");
	}
	if (stats_.checkRuns)
		std::fprintf(stderr, "[jit] self-check: %lld regions verified, %lld diverged, "
		             "%lld ran the wrong region\n",
		             stats_.checkRuns, stats_.checkDivergences, stats_.wrongRegion);
}

// Links every pending region into ONE module and instantiates it, placing each
// exported function at the table index its slot was promised.
void Runtime::flushBatch() {
	sinceFlush_ = 0;
	if (pending_.empty()) return;
#ifdef __EMSCRIPTEN__
	std::vector<Region> regions;
	std::vector<int> indices;
	regions.reserve(pending_.size());
	indices.reserve(pending_.size());
	for (auto &p : pending_) { regions.push_back(std::move(p.region)); indices.push_back(p.tableIndex); }

	const auto bytes = linkRegions(regions, helperLayout_);
	stats_.moduleBytes += (long long)bytes.size();
	const int ok = EM_ASM_INT({
		try {
			const inst = new WebAssembly.Instance(
				new WebAssembly.Module(HEAPU8.subarray($0, $0 + $1)),
				{ env: { memory: wasmMemory, __indirect_function_table: wasmTable } });
			const idx = new Int32Array(HEAP32.buffer, $2, $3);
			for (let i = 0; i < $3; i++) wasmTable.set(idx[i], inst.exports['blk' + i]);
			return 1;
		} catch (e) {
			console.error('[jit] batch instantiate failed:', e);
			return 0;
		}
	}, bytes.data(), (int)bytes.size(), indices.data(), (int)indices.size());

	if (ok) {
		stats_.batches++;
		for (size_t i = 0; i < pending_.size(); i++) {
			Slot &s = slots_[pending_[i].slot];
			// The slot may have changed hands since this region was compiled:
			// another address hashing to it re-claims it, and a compile only
			// records funcIndex here, at flush time. Writing the index in
			// anyway pointed the new occupant at the old occupant's code — the
			// new occupant's own hash and tag both agreed, so nothing else
			// caught it, and it ran entirely the wrong instructions. Measured
			// as 29 divergences over two simulated seconds, every one of them
			// this.
			if (s.entryVa != pending_[i].entryVa) { stats_.orphaned++; continue; }
			s.funcIndex = pending_[i].tableIndex;
		}
	} else {
		for (auto &p : pending_)
			if (slots_[p.slot].entryVa == p.entryVa) slots_[p.slot].declined = true;
	}
#endif
	pending_.clear();
}

void Runtime::flushAll() {
	for (auto &s : slots_) { s.funcIndex = -1; s.used = false; }
}

uint32_t Runtime::run(ARM710 &cpu, const ARM710::FetchPage &fp, uint32_t entryPc,
                      uint32_t maxCycles, int maxTicks, int *ticksUsed) {
#ifndef __EMSCRIPTEN__
	(void)cpu; (void)fp; (void)entryPc; (void)maxCycles; (void)maxTicks; (void)ticksUsed;
	return 0;
#else
	if (!enabled()) return 0;

	// The compiler needs the page as an array of words indexed from its base.
	// fp.hostBase is biased so that hostBase + va is the host address of va.
	const CodePage page{
		reinterpret_cast<const uint32_t *>(fp.hostBase + fp.startVa),
		fp.startVa, fp.endVa
	};
	if (entryPc < fp.startVa || entryPc + 12 > fp.endVa) return 0;

	Slot &slot = slots_[slotFor(entryPc)];
	const uint32_t *words = page.words + ((entryPc - page.baseVa) >> 2);

	bool valid = slot.used && slot.funcIndex >= 0 && slot.entryVa == entryPc;
	if (valid) {
		// Two independent staleness checks, because neither alone is enough.
		//
		// The page token is what the interpreter uses, and it catches a guest
		// store into this code page — writeVirtual invalidates the decoded page
		// on both of its write paths. What it does not catch is a host-side
		// write that never goes through writeVirtual at all: a card or ROM
		// image loaded straight into the RAM buffer. That is a documented gap
		// in the decoded-page cache and it only ever cost a wrong dispatch
		// kind; for a compiled region it would run entirely wrong code.
		//
		// So the words the region was compiled FROM are hashed as well. It is
		// nSpan loads on entry, which a region that loops amortises away, and
		// it makes a stale region impossible rather than unlikely.
		if (slot.validPtr && *slot.validPtr != slot.physBase) { valid = false; stats_.staleToken++; }
		else if (verifyWords() && hashRuns(slot, words) != slot.wordHash) {
			valid = false; stats_.staleWords++;
		}
	}

	if (!valid) {
		// A conflicting address does NOT simply take the slot. It knocks the
		// incumbent down by one and goes away, so a slot is won by whichever
		// address visits it most rather than by whichever visited last.
		//
		// The old policy — take the slot and reset the count — is a
		// thrash whenever two live addresses hash together, and on a real
		// application they do constantly: measured on the netBook driving its
		// spreadsheet, 420,309 evictions against 128 regions compiled and 6.7 M
		// entries that never reached the threshold because something else kept
		// resetting them. Almost nothing ever got hot enough to compile.
		//
		// It also protects a COMPILED region for free. Its count sits at the
		// threshold, so displacing it takes that many conflicting visits — which
		// is exactly the "prove you are hotter than what is already there" rule,
		// with no extra state to keep.
		if (slot.used && slot.entryVa != entryPc && slot.hits > 0) {
			slot.hits--;
			stats_.conflicts++;
			return 0;
		}
		if (!slot.used || slot.entryVa != entryPc) {
			if (slot.used) stats_.evictions++;
			slot.used = true;
			slot.entryVa = entryPc;
			slot.hits = 0;
			slot.declined = false;
			slot.funcIndex = -1;
			// Everything the staleness checks read belonged to the previous
			// occupant. Leaving it behind would let the new one validate against
			// the old one's hash.
			slot.validPtr = nullptr;
			slot.physBase = 0;
			slot.wordHash = 0;
			slot.nSpan = 0;
			slot.runCount = 0;
			slot.nBridged = 0;
			slot.reRunSafe = true;
		}
		// An address the compiler has already refused is not re-traced on every
		// entry — tracing is not free either.
		if (slot.declined) { stats_.declined++; return 0; }
		if (++slot.hits < hotThreshold()) { stats_.warmups++; return 0; }
		// The ceiling applies to NEW regions. Recompiling one that went stale
		// reuses its table slot, so it does not add to the total.
		if (slot.funcIndex < 0 && compiled_ >= maxCompiled()) { stats_.budgetHits++; return 0; }

		Layout layout{
			(uint32_t)cpu.gprFileAddr(), (uint32_t)cpu.cpsrAddr(),
			(uint32_t)cpu.prefetch0Addr(), (uint32_t)cpu.prefetch1Addr(),
			(uint32_t)cpu.prefetchCountAddr(),
			(uint32_t)cpu.prefetchFault0Addr(), (uint32_t)cpu.prefetchFault1Addr(),
			(uint32_t)cpu.insnCycleAddr(),
			(uint32_t)cpu.cycleSinkPtrAddr(), (uint32_t)cpu.pendingIrqAddr(),
			(uint32_t)cpu.pendingFiqAddr(), (uint32_t)cpu.jitExitTicksAddr(),
			(uint32_t)(uintptr_t)&cpu,
			(uint32_t)(uintptr_t)&psionJitExecOne,
			(uint32_t)(uintptr_t)&psionJitStepOne,
			cpu.armIsTVersion(),
			!envFlag("PSION_JIT_NO_BRIDGE"),
			!envFlag("PSION_JIT_NO_BRANCH"),
			!envFlag("PSION_JIT_NO_LOOP"),
			!envFlag("PSION_JIT_NO_CHAIN"),
			!envFlag("PSION_JIT_NO_LITERAL"),
			// One fewer than the dispatcher can verify words for: the region
			// itself is a run, and each followed branch adds another.
			Runtime::kMaxRuns - 1,
			!envFlag("PSION_JIT_NO_BRIDGE_SLOW"),
			[]{ const char *e = std::getenv("PSION_JIT_BRIDGE_KINDS");
			    // Falls back to Layout's own default, which excludes the class
			    // still under investigation. Hard-coding 0xFFFFFFFF here would
			    // silently re-enable it.
			    return e ? (uint32_t)std::strtoul(e, nullptr, 0) : Layout{}.bridgeKinds; }(),
			envFlag("PSION_JIT_SAFE_BRIDGES"),
			checking(),          // stamp the entry address, for the check below
			(uint32_t)cpu.jitDebugEntryAddr(),
			cpu.jitTraceStops(),
			(uint32_t)cpu.jitTraceStopCount(),
			(uint32_t)(uintptr_t)fp.validPtr,
			fp.physBase,
		};
		// The data-side micro-TLB, so loads compile inline instead of calling
		// out. Assigned rather than passed positionally: it is a struct, and a
		// field added to Layout must not silently shift what everything after
		// it means.
		layout.inlineLoads = !envFlag("PSION_JIT_NO_INLINE_LOAD")
		                     && cpu.jitMemSeam(layout.mem);
		stats_.traceStopCount = (long long)layout.traceStopCount;
		helperLayout_ = layout;
		Region reg;
		if (!compileRegion(page, entryPc, maxTraceInsns(), kMinTraceInsns, layout, reg)) {
			stats_.compileFailures++;
			slot.declined = true;
			return 0;
		}
		// Compiling is not cheap and it is not amortised the way a textbook JIT
		// assumes. V8 charges for every byte it compiles, and a region is only
		// entered ~100 times over a boot (391 at the absolute ceiling, measured
		// across all block starts) — so a straight-line region has to repay its
		// compile out of a hundred passes of eight instructions, and cannot.
		//
		// A region with a LOOP EDGE is a different proposition: one entry runs
		// the body until its cycle budget is spent, so it retires hundreds of
		// instructions rather than eight. PSION_JIT_LOOPS_ONLY compiles nothing
		// else.
		if (loopsOnly() && !reg.loops) {
			stats_.compileFailures++;
			slot.declined = true;
			return 0;
		}
		// Instantiate against the emulator's own memory and table. The export
		// lands at a table index, and in Emscripten a table index IS a C
		// function pointer — so the call below is an ordinary indirect call with
		// no JS on the path. A stale region overwrites its old table slot: the
		// engine holds a module alive for as long as its function is reachable,
		// so growing the table for every recompile would leak one per stale
		// page for the life of the session.
		// Reserve the whole table range up front, once.
		//
		// Growing the table per region was measured at ~13 us per REGION ENTRY
		// — nothing to do with the generated code, which a microbenchmark puts
		// at 15 ns for a 24-instruction region. Every wasmTable.grow()
		// invalidates the engine's indirect-call state for the whole module, so
		// growing it thousands of times during a run repeatedly threw away the
		// optimised code for the entire 1.1 MB emulator. One grow, then only
		// set().
		if (tableBase_ < 0) {
			tableBase_ = EM_ASM_INT({
				const base = wasmTable.length;
				wasmTable.grow($0);
				return base;
			}, maxCompiled());
			if (tableBase_ < 0) { slot.declined = true; return 0; }
		}
		const int idx = (slot.funcIndex >= 0) ? slot.funcIndex : tableBase_ + compiled_;
		if (slot.funcIndex < 0) compiled_++;
		stats_.compiles++;
		// The slot stops being runnable until the new code lands.
		//
		// A recompile reuses the old table index, and the validity metadata
		// below is updated NOW while the code itself only arrives at the next
		// batch flush. Leaving funcIndex pointing at the old function in the
		// meantime meant every check passed — right tag, right token, and a
		// hash that had just been updated to match the new words — while the
		// OLD compiled code ran. Guest code loaded from a card lives in RAM and
		// is rewritten, so this fired there and never on a ROM-only boot.
		slot.funcIndex = -1;
		slot.validPtr = fp.validPtr;
		slot.physBase = fp.physBase;
		slot.nSpan = reg.nSpan;
		slot.nBridged = reg.nBridged;
		slot.reRunSafe = reg.reRunSafe;
		slot.loops = reg.loops;
		stats_.compiledSpan += reg.nSpan;
		stats_.compiledChains += reg.nChained;
		stats_.compiledBridged += reg.nBridged;
		stats_.compiledLoads += reg.nInlineLoads;
		stats_.compiledLiterals += reg.nLiterals;
		for (int i = 0; i < 16; i++) stats_.bridgedKind[i] += reg.bridgedKind[i];
		for (int i = 0; i < 4; i++) stats_.bridgedLdrWhy[i] += reg.bridgedLdrWhy[i];
		stats_.stopReason[(int)reg.stop]++;
		// The runs the region actually reads. compileRegion is capped at
		// maxChains followed branches so this always fits; the guard is here
		// because a region whose words cannot be verified must not be run at
		// all, and silently truncating would verify a prefix and pass.
		if ((int)reg.runs.size() > kMaxRuns) { slot.declined = true; return 0; }
		slot.runCount = (uint8_t)reg.runs.size();
		for (int r = 0; r < slot.runCount; r++) {
			slot.runOff[r] = (int16_t)(((int32_t)reg.runs[r].startVa - (int32_t)entryPc) >> 2);
			slot.runLen[r] = (uint16_t)reg.runs[r].words;
		}
		slot.wordHash = hashRuns(slot, words);
		// The region is not callable until its batch is linked; until then this
		// entry point keeps interpreting.
		pending_.push_back(Pending{std::move(reg), slotFor(entryPc), entryPc, idx});
		if ((int)pending_.size() >= kBatchSize) flushBatch();
		return 0;
	}

	if (slot.funcIndex < 0) { stats_.pendingRuns++; return 0; }

	using BlkFn = uint32_t (*)(uint32_t, uint32_t);
	BlkFn rawFn = reinterpret_cast<BlkFn>(static_cast<uintptr_t>((uint32_t)slot.funcIndex));
	// A looping region gets its own cap — see loopTickCap. Everything else
	// honours the caller's, which is what keeps a straight-line region
	// indistinguishable from the burst engine to the batch loop around it.
	//
	// The override is read ONCE. In Emscripten a getenv is a JS-side string
	// lookup costing ~50-100 ns, and this is on the path of every region entry:
	// reading it per entry measured 4.36-4.61 s against 3.94-4.05 with it
	// hoisted, which is larger than everything the code generator saves.
	static const bool kTicksPinned = std::getenv("PSION_JIT_MAX_TICKS") != nullptr;
	const int tickCap = kTicksPinned ? jitMaxTicks(maxTicks)
	                                 : (slot.loops ? loopTickCap() : maxTicks);
	// Every call goes through here so the entry stamp is verified on both the
	// checked and the unchecked path. A region stamps the address it was
	// compiled for; if that is ever not the address being dispatched, a slot is
	// serving the wrong region and every other check is meaningless.
	auto fn = [&](uint32_t budget) {
		if (checking()) cpu.jitDebugEntry_ = 0;
		const uint32_t r = rawFn(budget, (uint32_t)tickCap);
		if (checking() && cpu.jitDebugEntry_ != entryPc) {
			stats_.wrongRegion++;
			if (stats_.wrongRegion <= 10)
				std::fprintf(stderr, "[jit] WRONG REGION: slot %d dispatched for %08x ran a "
				             "region compiled for %08x\n", slotFor(entryPc), entryPc,
				             cpu.jitDebugEntry_);
		}
		return r;
	};

	// A region containing a bridge cannot normally be checked by re-running it:
	// the bridge runs the interpreter, which may store to guest memory or
	// advance device state, and restoring registers rolls back neither. So the
	// re-run check is bridge-free only.
	//
	// PSION_JIT_CHECK_ALL=1 lifts that restriction and checks bridged regions
	// too. The run it produces is GARBAGE from the first bridged region onward
	// — every store in one happens twice — but both engines still start the
	// first divergent region from identical state, so the first report is
	// sound. It is a debugging mode, not a validation mode.
	if (checking() && (slot.reRunSafe || checkAll())) {
		// Run the region, then run the interpreter over the same span from the
		// same starting state, and compare. This is the WASM-side equivalent of
		// the state-hash harness: the interpreter is the oracle, on the real
		// workload, with no trace files involved.
		ARM710::JitCheckpoint before;
		cpu.jitSnapshot(before);
		const uint32_t jitCycles = fn(maxCycles);
		const int jitTicks = (int)cpu.jitExitTicks_;
		ARM710::JitCheckpoint after;
		cpu.jitSnapshot(after);

		cpu.jitRestore(before);
		uint32_t refCycles = 0;
		for (int i = 0; i < jitTicks; i++) refCycles += cpu.jitStepOne();
		ARM710::JitCheckpoint ref;
		cpu.jitSnapshot(ref);

		stats_.checkRuns++;
		if (refCycles != jitCycles || !ARM710::jitCheckpointEqual(after, ref)) {
			stats_.checkDivergences++;
			if (stats_.checkDivergences <= 20) {
				ARM710::jitReportDivergence(entryPc, jitTicks, jitCycles, refCycles, after, ref);
				// The region's own instructions, so a divergence in guest code
				// that came off a card (and so is not in any ROM file) can still
				// be read.
				std::fprintf(stderr, "  region: span %d, %d bridged, loops=%d, %d runs\n",
				             slot.nSpan, slot.nBridged, (int)reg_loops(slot),
				             (int)slot.runCount);
				// By run, because a region that followed a branch is not one
				// contiguous stretch of guest code any more — and the runs are
				// also the only bounds known to be inside the page.
				for (int r = 0; r < slot.runCount; r++)
					for (int i = 0; i < (int)slot.runLen[r]; i++)
						std::fprintf(stderr, "    %08x: %08x\n",
						             entryPc + 4u * (unsigned)(slot.runOff[r] + i),
						             words[slot.runOff[r] + i]);
				std::fprintf(stderr, "  entry state: CPSR %08x (NZCV %x)\n",
				             before.cpsr, before.cpsr >> 28);
				for (int r = 0; r < 16; r++)
					std::fprintf(stderr, "    r%-2d = %08x\n", r, before.gprs[r]);
			}
		}
		// The interpreter's run is the one that stands, so a divergence cannot
		// compound into the rest of the boot and bury its own cause.
		*ticksUsed = jitTicks;
		stats_.entries++; stats_.cycles += refCycles; stats_.ticks += jitTicks;
		return refCycles;
	}

	// Cheap, and it works for bridged regions the re-run check cannot touch:
	// the device cycle sink must advance by exactly what the region reports.
	// Over-advancing it does not corrupt a register — executeUntil simply
	// believes time has passed and stops doing work, so the guest appears to
	// idle and the run looks impossibly fast.
	const int64_t sinkBefore = cpu.jitDeviceCycles();
	const uint32_t cycles = fn(maxCycles);
	if (checking()) {
		const int64_t delta = cpu.jitDeviceCycles() - sinkBefore;
		stats_.checkRuns++;
		if (delta != (int64_t)cycles) {
			stats_.checkDivergences++;
			if (stats_.checkDivergences <= 20)
				std::fprintf(stderr,
				    "[jit] CYCLE MISMATCH at entry %08x: device cycles advanced %lld, "
				    "region reported %u (span %d, %d bridged)\n",
				    entryPc, (long long)delta, cycles, slot.nSpan, slot.nBridged);
		}
	}
	*ticksUsed = (int)cpu.jitExitTicks_;
	stats_.entries++;
	stats_.cycles += cycles;
	stats_.ticks += *ticksUsed;
	// A batch that stops filling still has to land.
	if (!pending_.empty() && ++sinceFlush_ >= kBatchFlushEntries) flushBatch();
	return cycles;
#endif
}

// ── Free-function seam used by ARM710 ──────────────────────────────────────
// arm710.h cannot include this header (the generator includes arm710.h), so the
// CPU reaches the dispatcher through these instead of through the class.

void jitEnsure(Runtime *&slot) {
	if (!slot && Runtime::enabled()) slot = new Runtime();
}

void jitDestroy(Runtime *&slot) { delete slot; slot = nullptr; }

void jitDumpStats(Runtime *slot) { if (slot) slot->dumpStats(); }
void jitFlushAll(Runtime *slot) { if (slot) slot->flushAll(); }
void jitNoteDirtyEntry(Runtime *slot) { if (slot) slot->noteDirtyEntry(); }

uint32_t jitRun(Runtime *rt, ARM710 &cpu,
                const uint8_t *hostBase, uint32_t startVa, uint32_t endVa,
                const uint32_t *validPtr, uint32_t physBase,
                uint32_t entryPc, uint32_t maxCycles, int maxTicks, int *ticksUsed) {
	if (!rt) return 0;
	ARM710::FetchPage fp{};
	fp.hostBase = hostBase;
	fp.startVa = startVa;
	fp.endVa = endVa;
	fp.validPtr = validPtr;
	fp.physBase = physBase;
	return rt->run(cpu, fp, entryPc, maxCycles, maxTicks, ticksUsed);
}

}  // namespace armjit
