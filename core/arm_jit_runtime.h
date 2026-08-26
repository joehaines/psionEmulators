// Dispatcher for the ARM → WASM region compiler (core/arm_jit.cpp).
//
// Holds the compiled regions, decides when to enter one, and — in the WASM
// build — turns a generated module into something C++ can call. Everything
// here is a no-op on a native build: there is no WebAssembly engine to
// instantiate into, so the harness and the boot suite run the interpreter
// exactly as before and this file only has to compile.
//
// OPT-IN. PSION_JIT=1 enables it; PSION_JIT_CHECK=1 additionally runs every
// region against the interpreter and reports any divergence (see runChecked).
#pragma once

#include <cstdint>
#include <vector>

#include "arm710.h"
#include "arm_jit.h"

namespace armjit {

class Runtime {
public:
	// Prints what the JIT actually did. The dispatcher is opt-in, so this only
	// ever appears in a run that asked for it.
	~Runtime();
	void dumpStats() const;
	void noteDirtyEntry() { stats_.dirtyEntry++; }
	// Reads the environment once. False on a native build regardless.
	static bool enabled();
	// Cross-checks every region against the interpreter. Very slow; a debug aid.
	static bool checking();

	// Runs a compiled region for the instruction the CPU is about to execute,
	// compiling one first if this entry point has none.
	//
	// Returns cycles retired and sets *ticksUsed, or returns 0 having done
	// nothing — which is the answer whenever the JIT is off, the page cannot be
	// compiled from, or the region would not be worth entering.
	uint32_t run(ARM710 &cpu, const ARM710::FetchPage &fp, uint32_t entryPc,
	             uint32_t maxCycles, int maxTicks, int *ticksUsed);

	// Drops every compiled region. Call whenever guest code may have changed
	// underneath one in a way the per-page validity token cannot see — a card
	// or ROM image loaded straight into a host buffer, or a state restore.
	void flushAll();

	// Diagnostics, printed at exit when the JIT ran.
	struct Stats {
		long long entries = 0, cycles = 0, ticks = 0;
		long long compiles = 0, compileFailures = 0, evictions = 0;
		long long declined = 0, budgetHits = 0, warmups = 0, conflicts = 0;
		long long batches = 0, pendingRuns = 0, orphaned = 0, traceStopCount = 0;
		long long staleToken = 0, staleWords = 0;
		long long dirtyEntry = 0;
		// Shape of what the compiler produced, so a short region can be blamed
		// on the guest's control flow or on a limit this compiler chose.
		long long compiledSpan = 0, compiledChains = 0, compiledBridged = 0;
		long long compiledLoads = 0, compiledLiterals = 0;
		long long bridgedKind[16] = {0};
		long long bridgedLdrWhy[4] = {0};
		long long moduleBytes = 0;
		long long stopReason[8] = {0};
		long long checkRuns = 0, checkDivergences = 0, wrongRegion = 0;
	};
	const Stats &stats() const { return stats_; }

private:
	// Address runs a region may be compiled from. Must not be less than
	// armjit::Layout::maxChains + 1 — a region with more runs than this cannot
	// have its words verified, so the compiler is not allowed to make one.
	static constexpr int kMaxRuns = 8;
	struct Slot {
		uint32_t entryVa = 0;
		// Entries seen at this address since the slot was claimed. Compiling is
		// not free — each region is its own WebAssembly.Module, which costs a
		// compile and a permanent chunk of engine memory — so an entry point
		// has to prove it is hot first. Without this the first attempt compiled
		// a module for essentially every block start in the boot and ran the
		// host out of memory.
		uint32_t hits = 0;
		const uint32_t *validPtr = nullptr;   // the page's validity token
		uint32_t physBase = 0;
		uint32_t wordHash = 0;                // over the trace's own words
		// The runs of guest words the region was compiled from, as word offsets
		// from entryVa (a followed branch can go backwards, so signed). Before
		// chaining a trace was contiguous and nSpan was enough; it is not any
		// more, and hashing the wrong words would turn the check into a silent
		// pass. See armjit::Run.
		int16_t  runOff[kMaxRuns] = {0};
		uint16_t runLen[kMaxRuns] = {0};
		uint8_t  runCount = 0;
		bool     declined = false;            // the compiler refused this entry
		int      nSpan = 0;
		int      nBridged = 0;
		bool     reRunSafe = true;
		bool     loops = false;
		int      funcIndex = -1;              // table index; < 0 means empty
		bool     used = false;
	};
	// Direct-mapped, and indexed by a MULTIPLICATIVE hash rather than by low
	// address bits. The first attempt used (va >> 2) & (kSlots - 1), which maps
	// only an 8 KB window of virtual address space to distinct slots — guest
	// code spans hundreds of KB, so the table aliased hard: 1.07 M evictions
	// against 1,024 live regions over four simulated seconds.
	static constexpr int kSlots = 16384;
	Slot slots_[kSlots];
	Stats stats_;
	int compiled_ = 0;
	// First index of the reserved table range, or negative before it is taken.
	int tableBase_ = -1;

	// Regions compiled but not yet linked. Instantiating a WASM module costs
	// ~4.4 ms — one module per region was 63% of the emulator's runtime — so
	// regions wait here until there are enough to be worth a module.
	// entryVa travels with the region because a slot can change hands between
	// the compile and the flush — see flushBatch.
	struct Pending { Region region; int slot; uint32_t entryVa; int tableIndex; };
	std::vector<Pending> pending_;
	// The layout the pending regions were compiled against, kept because
	// linkRegions builds the batch's shared memory helpers from it and the
	// batch is flushed long after the compile that filled it. Only the parts
	// the helpers read (the micro-TLB seam, the CPSR address) matter, and those
	// are properties of the CPU rather than of any one region.
	Layout helperLayout_{};
	// Entries since the batch was last flushed, so a batch that never fills
	// still lands rather than stranding its regions forever.
	long long sinceFlush_ = 0;

	void flushBatch();

	static int slotFor(uint32_t va) {
		return (int)(((va >> 2) * 2654435761u) >> (32 - 14));   // 2^14 == kSlots
	}
	static uint32_t hashWords(const uint32_t *w, int n);
	// Hashes the slot's runs out of `page`, whose words are indexed from
	// `entryWords` (the entry address). Returns the same value hashWords would
	// have for a single contiguous run, so an un-chained region is unaffected.
	static uint32_t hashRuns(const Slot &s, const uint32_t *entryWords);
	static bool reg_loops(const Slot &s);
};

// The bridge the generated code calls for an instruction it cannot compile.
// Declared here so its address can be taken; defined in the .cpp.
// The two bridges a generated region calls. Declared here so their addresses
// can be taken; in Emscripten a function address IS its table index.
extern "C" uint32_t psionJitStepOne(uint32_t cpu, uint32_t pc);
extern "C" uint32_t psionJitExecOne(uint32_t cpu, uint32_t insn);

}  // namespace armjit
