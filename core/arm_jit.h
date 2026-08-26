// ARM region → WebAssembly code generator (Stage 3, see
// docs/jit-engine-scope.md).
//
// Compiles a REGION — a trace through one resolved code page — into a single
// WASM function that runs on the emulator's real register file in linear
// memory. The generated function is semantically identical to running the same
// instructions through ARM710::tickPageLoop, including its cycle accounting;
// that is the contract, and it is what the differential tests check.
//
// Why a region rather than a basic block: measured on a Series 7 boot, a basic
// block is 3.20 instructions and a run of instructions this generator can
// compile inline is 1.52. A generated function entered for 1.52 instructions
// loses to the interpreter on call overhead alone. So a region does not stop
// at the first instruction it cannot compile, and it does not stop at a
// branch:
//
//   * an instruction outside the inline subset is BRIDGED — the region stores
//     the architectural state the interpreter expects, calls ARM710::tick()
//     for that one instruction, and carries on;
//   * a branch back to the region entry becomes a WASM loop, so a tight guest
//     loop compiles into one function with no dispatch at all — which is where
//     the measured 391x block amortisation is concentrated.
//
// This file has no dependency on WebAssembly at runtime — it assembles bytes.
// So it builds, and is tested, in the native harness, where the interpreter is
// available as an oracle.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "arm710.h"
#include "wasm_emit.h"

namespace armjit {

// Cycles the burst loop charges for one executed instruction in the inline
// subset. ARM710::tickPageLoop starts every tick at 1 and adds 1 more when the
// tick executes an instruction rather than refilling the prefetch pipeline; the
// handler's own extras (+1 for a register-specified shift, +2 for a PC
// destination) are excluded from the subset by acceptsInline(). Getting this
// wrong does not corrupt any register — it slides interrupt delivery, which is
// the hardest kind of divergence to find, so it is named rather than inlined.
constexpr uint32_t kCyclesPerInsn = 2;

// Cycles and tick-equivalents a TAKEN branch costs, from the instruction that
// branches to the instruction that runs at the target. The branch tick is
// 1 (tick base) + 1 (executed, not a refill) + 2 (execBranch's pipeline-refill
// charge) = 4, and it leaves prefetchCount at 0, so the loop then spends two
// pure refill ticks at 1 cycle each before it can execute again.
constexpr uint32_t kBranchCycles = 6;
constexpr uint32_t kBranchTicks  = 3;

// Linear-memory addresses of the CPU state a generated region touches. In the
// WASM build these come from the live ARM710 object; in the native tests they
// point into a scratch buffer laid out the same way.
struct Layout {
	uint32_t gprBase;             // &GPRs[0]
	uint32_t cpsrAddr;            // &CPSR
	uint32_t prefetch0Addr;       // &prefetch[0]
	uint32_t prefetch1Addr;       // &prefetch[1]
	uint32_t prefetchCountAddr;   // &prefetchCount   (int)
	// &prefetchFaults[0..1]. The region rewrites the prefetch words from
	// constants, so it has to rewrite their fault status too — leaving a stale
	// fault beside a fresh word makes the interpreter raise a prefetch abort
	// for an instruction that fetched cleanly.
	uint32_t prefetchFault0Addr;  // MMUFault (8 bytes)
	uint32_t prefetchFault1Addr;
	uint32_t insnCycleAddr;       // &insnCycleApprox (uint64)
	uint32_t cycleSinkPtrAddr;    // &cycleSink_      (int64*, may be null)
	uint32_t pendingIrqAddr;      // &pendingIRQ      (bool)
	uint32_t pendingFiqAddr;      // &pendingFIQ      (bool)
	uint32_t exitTicksAddr;       // &jitExitTicks_   (uint32, written on exit)
	uint32_t cpuPtr;              // the ARM710 `this` the bridge is called with
	// Table indices of the two bridges. The fast one takes the instruction word
	// the region already holds as a constant and runs just the burst loop's
	// per-instruction body; the full one re-fetches through tick() and is only
	// needed for DK_SLOW, which ends a trace anyway.
	uint32_t bridgeFuncIndex;     // psionJitExecOne(cpu, insn)
	uint32_t slowBridgeFuncIndex; // psionJitStepOne(cpu, pc)
	// ARMv4T rather than ARMv4: decides whether the halfword-transfer and
	// long-multiply encodings exist at all, which is exactly what keeps them
	// from being mistaken for data processing. See ARM710::decodeKindV.
	bool     isTVersion;
	// Trace features, so a divergence can be bisected by turning one off
	// (PSION_JIT_NO_BRIDGE / _NO_BRANCH / _NO_LOOP / _NO_CHAIN). All on by
	// default.
	bool     allowBridge = true;
	bool     allowBranch = true;
	bool     allowLoop   = true;
	// Follow an unconditional branch and keep tracing at its target, instead of
	// leaving the region there. This is what makes a region longer than a basic
	// block: measured without it, regions average 18 instructions and cover
	// about 5% of what the guest executes, because they end at the first B or BL
	// they meet. The chain is materialised at compile time into one function, so
	// there is no cross-function dispatch to pay for.
	bool     allowChain  = true;
	// Compile a PC-relative load to the constant it reads, when that constant
	// is in the same code page. PSION_JIT_NO_LITERAL turns it off.
	bool     allowLiteral = true;
	// How many branches one region may follow. Bounds the compile, the code
	// size, and the number of address runs the dispatcher has to hash on entry
	// (see Region::runs and Runtime::kMaxRuns — they must agree).
	int      maxChains   = 7;
	// Bridge only instructions the decoder recognises — refuse DK_SLOW (SWI,
	// coprocessor, BX/BLX, undefined), which are the ones that change mode and
	// PC in unusual ways. Bisection knob: PSION_JIT_NO_BRIDGE_SLOW.
	bool     allowBridgeSlow = true;
	// Bitmask over ArmDK of which kinds may be bridged
	// (PSION_JIT_BRIDGE_KINDS), which is also how a divergence gets bisected
	// down to one class of instruction.
	//
	uint32_t bridgeKinds = 0xFFFFFFFFu;
	// Bridge only instructions that can be re-run safely, so every compiled
	// region is checkable against the interpreter (PSION_JIT_SAFE_BRIDGES).
	bool     bridgeReRunSafeOnly = false;
	// Stamp the region's own entry address into jitDebugEntry_ on the way in, so
	// the dispatcher can prove a slot is serving the region it thinks it is.
	// Check builds only — it is a store per entry.
	bool     stampEntry = false;
	uint32_t stampAddr = 0;
	// Guest addresses a trace must stop at rather than swallow — the ones the
	// device's per-batch hooks find by sampling getRealPC(). See
	// kJitTraceStopPcs in core/sa1100.cpp.
	const uint32_t *traceStops = nullptr;
	uint32_t traceStopCount = 0;
	// The data-side micro-TLB, so a load can be compiled inline instead of
	// bridged. Zero mruBase means the CPU has no such path and loads stay
	// bridged, which is also what PSION_JIT_NO_INLINE_LOAD gives.
	//
	// STORES are deliberately not inlined here. A store into a cached code page
	// has to invalidate its decoded ops, and that invalidation lives inside the
	// interpreter's write path; inlining the store without it would let a
	// region keep executing constants it baked in from a page the guest has
	// just rewritten. Loads owe nothing of the sort.
	ARM710::JitMemSeam mem;
	bool     inlineLoads = false;
	// The resolved code page's validity token, and the value it holds while the
	// page is unchanged. The interpreter re-reads this before EVERY instruction
	// and breaks out when it changes; a region has to do the same or it keeps
	// executing constants it baked in from a page that has since been written.
	uint32_t validPtrAddr = 0;
	uint32_t validPhysBase = 0;
};

// A page of guest code the region is compiled out of. `words` is indexed by
// (pc - baseVa) / 4 and must be readable over the whole [baseVa, endVa) span.
struct CodePage {
	const uint32_t *words;
	uint32_t baseVa;
	uint32_t endVa;      // exclusive
};

// A contiguous span of guest words a region reads: its instructions plus the
// two that follow the last one, which every exit needs to restore the prefetch
// pipeline from. A region that follows no branch has exactly one; each followed
// branch adds another, because the trace jumps.
//
// The dispatcher hashes these on entry to catch guest code changed by something
// that never went through writeVirtual — a card image loaded straight into the
// RAM buffer. Before chaining the trace was contiguous and a single count did;
// it is not contiguous any more, and hashing the wrong words would be a silent
// miss on exactly the netBook path that motivated the check.
struct Run {
	uint32_t startVa;
	uint32_t words;
};

struct Region {
	// The compiled function body. NOT a module: instantiating a WASM module
	// costs ~4.4 ms, so regions are linked in batches (see linkRegions) rather
	// than one module apiece — building one module per region put 63% of the
	// emulator's runtime inside WebAssembly.Instance.
	wasmemit::Func func;
	int      nInsns = 0;           // instructions emitted inline
	int      nBridged = 0;         // instructions handed to the interpreter
	int      nSpan = 0;            // trace positions, inline + bridged
	uint32_t traceCycles = 0;      // worst-case cycles for one pass of the trace
	uint32_t traceTicks = 0;       // worst-case tick-equivalents for one pass
	bool     loops = false;        // the trace ends by branching back to entry
	// True when running this region twice from the same registers produces the
	// same answer — no bridged instruction writes guest memory, changes mode,
	// or does anything else a register snapshot cannot roll back. Only such a
	// region can be checked against the interpreter by re-running it, which is
	// how PSION_JIT_CHECK validates the generator on a real workload.
	bool     reRunSafe = true;
	// Tick offsets into a single pass at which the interpreter is handed an
	// instruction, in trace order — which is how the differential test lines a
	// bridge up against the interpreter timeline it is checked against. These
	// used to be trace positions, which was the same number until a followed
	// branch started costing three ticks.
	std::vector<int> bridgeAt;
	// Parallel to bridgeAt: the guest address of each bridged instruction. Not
	// derivable from the entry any more — the trace is not contiguous.
	std::vector<uint32_t> bridgePc;
	// Parallel to bridgeAt: true where the FAST bridge form was emitted (the
	// word is a constant, r15 is left at address+12 and the pipeline untouched)
	// and false for the full tick() form used by DK_SLOW.
	std::vector<char> bridgeFast;
	// Every guest word the region was compiled from — see Run. Always at least
	// one, and one more per followed branch.
	std::vector<Run> runs;
	// Branches followed rather than exited at.
	int      nChained = 0;
	// Instructions handed to the interpreter, counted by ArmDK kind — the
	// histogram that says which encoding is worth compiling next.
	int      bridgedKind[16] = {0};
	// Why a single data transfer was bridged rather than compiled: 0 a store,
	// 1 PC-relative (Rn == 15), 2 loading into the PC, 3 anything else. Says
	// which of them is worth the next piece of code generation.
	int      bridgedLdrWhy[4] = {0};
	// Loads compiled against the micro-TLB rather than bridged. Each one can
	// still reach the interpreter at runtime — the guard decides — so this
	// counts opportunities, not hits.
	int      nInlineLoads = 0;
	// PC-relative loads compiled to the constant they read. These cannot miss.
	int      nLiterals = 0;
	// Stores compiled against the micro-TLB rather than bridged. Like an inlined
	// load each can still miss; unlike one, each makes the region unre-runnable.
	int      nInlineStores = 0;
	// Why the trace stopped where it did. The only way to know whether a region
	// is short because of the guest's control flow or because of a limit this
	// compiler chose — which is the difference between "as good as it gets" and
	// "raise a constant".
	enum class Stop : uint8_t {
		MaxInsns,        // hit the trace-length limit: the region is as long as allowed
		OffPage,         // the next instruction is outside the resolved code page
		Seen,            // already in this trace, and not the entry (needs a CFG)
		TraceStop,       // an address the device watches for
		BranchNotTaken,  // an unconditional branch the trace would not follow
		ModeChange,      // a bridged instruction that can change mode
		BridgeRefused,   // an instruction bridging is not allowed for
		BranchRefused,   // a branch, with branches turned off
	};
	Stop     stop = Stop::MaxInsns;
};

// True when `insn` can be compiled inline (as opposed to bridged).
//
// Requires: a data-processing instruction (as ARM710::decodeKindV classifies
// it, so the multiply / halfword / swap encodings that alias into the
// data-processing space are already excluded); no r15 as Rd, Rn or Rm; a
// shift-by-immediate rather than shift-by-register; and, for the output-less
// opcodes TST/TEQ/CMP/CMN, S set — with S clear those encodings are MRS/MSR,
// which touch mode and banked state and are nothing like a comparison.
bool acceptsInline(uint32_t insn, bool isTVersion);

// True when `insn` is a load the generator can compile inline against the
// interpreter's data-side micro-TLB (Layout::mem).
//
// Requires: a single data transfer (as ARM710::decodeKindV classifies it) with
// L set; no r15 as Rd, Rn or Rm — a region keeps r15 in constants and only
// materialises it at an exit, so an instruction that reads or writes it has to
// be the interpreter's; and not the post-indexed-writeback form, which is
// LDRT and runs the access as if unprivileged.
//
// The runtime conditions — TLB hit, permission cached and granted, host pointer
// resolved, word access aligned — are tested by the generated code, which
// bridges the instruction when any of them fails. So this only has to be right
// about the SHAPE of the instruction.
bool acceptsInlineLoad(uint32_t insn, bool isTVersion);

// True when `insn` is a store the generator can compile inline. Same shape
// rules as acceptsInlineLoad, minus the alignment one — writeVirtual writes at
// the address it is given — and plus nothing: the code-coherency drop a store
// owes is emitted, not avoided.
bool acceptsInlineStore(uint32_t insn, bool isTVersion);

// Compiles a region starting at `entryPc`, following the trace for at most
// maxInsns positions.
//
// Returns false when the region would not be worth entering — fewer than
// minInsns positions, or nothing compiled inline at all (a region made
// entirely of bridges is strictly slower than letting the interpreter run).
bool compileRegion(const CodePage &page, uint32_t entryPc, int maxInsns,
                   int minInsns, const Layout &layout, Region &out);

// Module-internal helpers every batch shares, in the order linkRegions defines
// them — so their function indices are these values, and a region's `call` can
// be emitted before the module they end up in exists.
//
// They are shared because V8 charges for every byte of a module it compiles,
// and inlining the micro-TLB guard into thousands of regions was the largest
// single cost the code generator added: 164 ms of module compilation and
// instantiation in a 6.4 s run.
enum : uint32_t {
	kHelperHostReadW = 0,   // (addr) -> host address for a word read, or 0
	kHelperHostReadB,       // (addr) -> host address for a byte read, or 0
	kHelperStoreW,          // (addr, value) -> 1 once stored, or 0 to bridge
	kHelperStoreB,
	// Not memory accesses, but the same argument: these appear at every bridge
	// and every exit, and written out they are the bulk of a region's bytes.
	kHelperFlush,           // (cycles) -> void: insnCycleApprox and the sink
	kHelperExitState,       // (r15, word1, word0, count) -> void: r15 + pipeline
	kHelperCount,
};

// Links compiled regions into one module, exporting them as "blk0".."blkN-1"
// in the order given, behind the shared helpers above. Returns the module bytes.
std::vector<uint8_t> linkRegions(std::vector<Region> &regions, const Layout &layout);

// The export name of the i'th region in a linked batch.
std::string exportName(size_t i);

}  // namespace armjit
