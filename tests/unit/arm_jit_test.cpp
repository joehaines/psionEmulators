// Differential test for the ARM → WASM region compiler (core/arm_jit.cpp).
//
// Every case runs the same guest code twice: once through the real interpreter
// (ARM710, driven exactly the way ARM710::tickPageLoop drives it) and once
// through the generated WASM region, from the same register file and CPSR.
// Every GPR, every CPSR flag, the prefetch pipeline, the cycle count and the
// tick count must come out identical.
//
// The WASM half cannot run in a native process, so this program writes a binary
// case file — the guest page, the pre-state, the interpreter's post-state, and
// the generated module — and tests/unit/arm_jit_test.mjs replays the modules
// under node and does the comparison. Splitting it that way keeps the ORACLE in
// the real interpreter rather than in a reimplementation of it, which is the
// whole point of the exercise.
#include "../../core/arm710.h"
#include "../../core/arm_jit.h"
#include "../../core/wasm_emit.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <array>
#include <map>
#include <utility>
#include <vector>

namespace {

constexpr uint32_t kPageBase = 0x00021000;
constexpr uint32_t kPageWords = 64;
constexpr uint32_t kPageEnd = kPageBase + kPageWords * 4;

// Scratch-memory layout the generated code is compiled against. Mirrors the
// real ARM710 field order closely enough to be readable; the addresses
// themselves are arbitrary and are handed to the compiler in a Layout.
constexpr uint32_t kGprBase       = 256;
constexpr uint32_t kCpsrAddr      = kGprBase + 64;
constexpr uint32_t kPrefetch1     = kCpsrAddr + 4;
constexpr uint32_t kPrefetch0     = kCpsrAddr + 8;
constexpr uint32_t kPrefetchCount = kCpsrAddr + 12;
constexpr uint32_t kFault0        = kCpsrAddr + 48;   // 8 bytes each
constexpr uint32_t kFault1        = kCpsrAddr + 56;
constexpr uint32_t kPendingIrq    = kCpsrAddr + 16;
constexpr uint32_t kPendingFiq    = kCpsrAddr + 17;
constexpr uint32_t kExitTicks     = kCpsrAddr + 20;
constexpr uint32_t kInsnCycle     = kCpsrAddr + 24;   // 8-byte aligned below
constexpr uint32_t kCycleSinkPtr  = kCpsrAddr + 32;
constexpr uint32_t kCycleSink     = kCpsrAddr + 40;
// ── Synthetic data-side micro-TLB ──────────────────────────────────────────
// An inlined load reads the interpreter's own TLB structures out of linear
// memory, so the test builds a small one and points the compiler at it. Three
// entries cover the three shapes the guard has to get right — a section, a
// small page and a large page — plus a null way, an uncached sub-page and an
// unaligned word, which are the misses that have to reach the interpreter
// instead. tests/unit/arm_jit_test.mjs lays the same bytes down; the two agree
// by these constants and by testDataWord().
constexpr uint32_t kMruBase   = 7168;                 // 8 entry pointers
constexpr uint32_t kEnt0      = 7232, kEnt1 = 7264, kEnt2 = 7296;
constexpr uint32_t kData0     = 8192, kData1 = 8448, kData2 = 8704;   // 256 B each
constexpr uint32_t kGuest0    = 0x00300000;           // section entry
constexpr uint32_t kGuest1    = 0x00401000;           // small-page entry
constexpr uint32_t kGuest2    = 0x00502000;           // large-page entry
constexpr uint32_t kGuestNull = 0x00307000;           // way 7, which is empty
// Entry field offsets. Chosen here rather than taken from the real
// FastTlbEntry: the generated code reads whatever the Layout says, so the test
// exercises the same emitted sequence against its own layout.
constexpr uint32_t kOffAddrMask = 0, kOffAddr = 4, kOffLv1 = 8, kOffLv2 = 12;
constexpr uint32_t kOffPermCache = 16, kOffPermCachePg = 17;
constexpr uint32_t kOffHostRead = 24, kOffHostWrite = 28;
constexpr uint32_t kDecodePtr = 6144, kDecodeSlots = 256, kDecodeStride = 8;

// The guest's data, as a function of address so both halves can compute it.
uint32_t testDataWord(uint32_t va) { return 0xA5A50000u ^ (va * 2654435761u); }
constexpr uint32_t kDataLo = 0x00300000, kDataHi = 0x00600000;

constexpr uint32_t kCpuPtr        = 0xC0DE0000;       // opaque to the test
constexpr uint32_t kBridgeIndex   = 1;                // table slot the stub sits in
constexpr uint32_t kSlowBridgeIdx = 1;                // the test stubs both the same way

// A concrete ARM710 whose memory is the test page. The region compiler's inline
// subset never touches memory, but a BRIDGED instruction runs through the real
// tick(), which fetches — so the CPU has to be able to read the page.
struct TestCpu : ARM710 {
	explicit TestCpu(bool t) : ARM710(t) {}

	const uint32_t *page = nullptr;

	MaybeU32 readPhysical(uint32_t physAddr, ValueSize valueSize) override {
		uint32_t w;
		if (physAddr >= kDataLo && physAddr < kDataHi) {
			// Bytes this run has already written win over the pattern.
			uint32_t out = 0;
			const int n = valueSize == V32 ? 4 : valueSize == V16 ? 2 : 1;
			const uint32_t a0 = valueSize == V32 ? (physAddr & ~3u) : physAddr;
			for (int i = 0; i < n; i++) {
				auto it = mem.find(a0 + (uint32_t)i);
				const uint8_t b = it != mem.end()
					? it->second
					: (uint8_t)(testDataWord((a0 + (uint32_t)i) & ~3u) >>
					            (8 * ((a0 + (uint32_t)i) & 3)));
				out |= (uint32_t)b << (8 * i);
			}
			return out;
		}
		if (physAddr >= kPageBase && physAddr < kPageEnd) {
			w = page[(physAddr - kPageBase) >> 2];
		} else {
			return MaybeU32();
		}
		switch (valueSize) {
		case V32: return w;
		case V16: return (w >> (8 * (physAddr & 2))) & 0xFFFF;
		default:  return (w >> (8 * (physAddr & 3))) & 0xFF;
		}
	}
	// Loads go through readVirtual with the MMU off, so they land in
	// readPhysical above — the same words the WASM half was given.
	// Guest stores, kept so a case can be checked on the memory it left behind
	// and not only on registers. Logged with the tick they happened on, because
	// the region reports how many ticks it retired and the comparison has to be
	// made at that point in the interpreter's timeline.
	std::map<uint32_t, uint8_t> mem;
	std::vector<std::array<uint32_t, 3>> stores;   // {tick, address, byte}
	uint32_t curTick = 0;
	bool writePhysical(uint32_t value, uint32_t physAddr, ValueSize valueSize) override {
		const int n = valueSize == V32 ? 4 : valueSize == V16 ? 2 : 1;
		for (int i = 0; i < n; i++) {
			const uint8_t b = (uint8_t)(value >> (8 * i));
			mem[physAddr + (uint32_t)i] = b;
			stores.push_back({curTick, physAddr + (uint32_t)i, b});
		}
		return true;
	}

	// tickPageLoop's per-instruction sequence, written out so the oracle cannot
	// drift from the thing it is an oracle for. Returns cycles; *ticks counts
	// tick-equivalents.
	uint32_t runTicks(int nTicks, int *ticks) {
		uint32_t total = 0;
		for (int i = 0; i < nTicks; i++) { total += tick(); (*ticks)++; }
		return total;
	}

	void setGpr(int i, uint32_t v) { GPRs[i] = v; }
	uint32_t getGprRaw(int i) const { return GPRs[i]; }
	void setCpsrRaw(uint32_t v) { CPSR = v; }
	uint32_t getCpsrRaw() const { return CPSR; }
	void primePipeline(uint32_t entryPc, const uint32_t *words) {
		GPRs[15] = entryPc + 8;
		prefetch[1] = words[(entryPc - kPageBase) >> 2];
		prefetch[0] = words[((entryPc - kPageBase) >> 2) + 1];
		prefetchFaults[0] = prefetchFaults[1] = NoFault;
		prefetchCount = 2;
	}
	uint32_t getPrefetch(int i) const { return prefetch[i]; }
	int getPrefetchCount() const { return prefetchCount; }
};

void put32(std::vector<uint8_t> &o, uint32_t v) {
	o.push_back(v & 0xFF); o.push_back((v >> 8) & 0xFF);
	o.push_back((v >> 16) & 0xFF); o.push_back((v >> 24) & 0xFF);
}

uint32_t mkDataProc(uint32_t cond, uint32_t I, uint32_t opcode, uint32_t S,
                    uint32_t rn, uint32_t rd, uint32_t operand2) {
	return (cond << 28) | (I << 25) | (opcode << 21) | (S << 20) |
	       (rn << 16) | (rd << 12) | (operand2 & 0xFFF);
}
// B / BL with a target relative to the instruction at `pc`.
uint32_t mkBranch(uint32_t cond, bool link, uint32_t pc, uint32_t target) {
	const int32_t off = ((int32_t)target - (int32_t)pc - 8) >> 2;
	return (cond << 28) | (0x5u << 25) | ((link ? 1u : 0u) << 24) | ((uint32_t)off & 0x00FFFFFF);
}

armjit::Layout makeLayout(bool isTVersion) {
	armjit::Layout L{ kGprBase, kCpsrAddr, kPrefetch0, kPrefetch1, kPrefetchCount,
	                  kFault0, kFault1,
	                  kInsnCycle, kCycleSinkPtr, kPendingIrq, kPendingFiq,
	                  kExitTicks, kCpuPtr, kBridgeIndex, kSlowBridgeIdx, isTVersion };
	// ARMJIT_TEST_NO_INLINE_LOAD=1 compiles the same cases with every load
	// bridged instead. The cases must still all pass — and the hand-over count
	// the WASM half prints must go UP, which is how the inline path is shown to
	// be firing rather than quietly missing every time.
	L.inlineLoads = std::getenv("ARMJIT_TEST_NO_INLINE_LOAD") == nullptr;
	L.mem.mruBase = kMruBase;
	L.mem.mruWayMask = 7;
	L.mem.mruShift = 12;
	L.mem.offAddrMask = kOffAddrMask;
	L.mem.offAddr = kOffAddr;
	L.mem.offLv2 = kOffLv2;
	L.mem.offPermCache = kOffPermCache;
	L.mem.offPermCachePg = kOffPermCachePg;
	L.mem.offLv1 = kOffLv1;
	L.mem.offHostRead = kOffHostRead;
	L.mem.offHostWrite = kOffHostWrite;
	L.mem.decodePagesPtr = kDecodePtr;
	L.mem.decodePageSlotMask = kDecodeSlots - 1;
	L.mem.decodePageStride = kDecodeStride;
	L.mem.offDecodePhysBase = 0;
	return L;
}

// STR / STRB, addressing mode 2.
uint32_t mkStr(uint32_t cond, bool reg, bool pre, bool up, bool byteAccess,
               bool wb, uint32_t rn, uint32_t rd, uint32_t operand) {
	return (cond << 28) | (1u << 26) | ((reg ? 1u : 0u) << 25) |
	       ((pre ? 1u : 0u) << 24) | ((up ? 1u : 0u) << 23) |
	       ((byteAccess ? 1u : 0u) << 22) | ((wb ? 1u : 0u) << 21) |
	       (rn << 16) | (rd << 12) | (operand & 0xFFF);
}

// LDR / LDRB, addressing mode 2.
uint32_t mkLdr(uint32_t cond, bool reg, bool pre, bool up, bool byteAccess,
               bool wb, uint32_t rn, uint32_t rd, uint32_t operand) {
	return (cond << 28) | (1u << 26) | ((reg ? 1u : 0u) << 25) |
	       ((pre ? 1u : 0u) << 24) | ((up ? 1u : 0u) << 23) |
	       ((byteAccess ? 1u : 0u) << 22) | ((wb ? 1u : 0u) << 21) | (1u << 20) |
	       (rn << 16) | (rd << 12) | (operand & 0xFFF);
}

}  // namespace

int main(int argc, char **argv) {
	if (argc < 2) { std::fprintf(stderr, "usage: arm_jit_test <out.bin>\n"); return 2; }
	const bool isTVersion = true;

	const uint32_t seeds[][4] = {
		{ 0x00000000, 0x00000001, 0xFFFFFFFF, 0x80000000 },
		{ 0x7FFFFFFF, 0x80000000, 0x00000001, 0x00000002 },
		{ 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x7FFFFFFF },
		{ 0x12345678, 0x9ABCDEF0, 0x0000FFFF, 0xFFFF0000 },
		{ 0x00000010, 0x00000020, 0x80000001, 0x7FFFFFFE },
	};
	const uint32_t conds[] = { 0xE, 0x0, 0x1, 0x2, 0x4, 0x6, 0xC, 0xD };
	const uint32_t kNop = 0xE1A00000;   // MOV r0,r0 — inline-compilable filler

	// ── Instruction forms ──────────────────────────────────────────────────
	std::vector<uint32_t> forms;
	for (uint32_t opcode = 0; opcode < 16; opcode++)
		for (uint32_t S = 0; S < 2; S++)
			for (uint32_t rot : { 0u, 1u, 4u, 8u, 12u })
				for (uint32_t imm : { 0x00u, 0x01u, 0x80u, 0xFFu })
					forms.push_back(mkDataProc(0xE, 1, opcode, S, 1, 0, (rot << 8) | imm));
	for (uint32_t opcode = 0; opcode < 16; opcode++)
		for (uint32_t S = 0; S < 2; S++)
			for (uint32_t ty = 0; ty < 4; ty++)
				for (uint32_t by : { 0u, 1u, 7u, 31u })
					forms.push_back(mkDataProc(0xE, 0, opcode, S, 1, 0, (by << 7) | (ty << 5) | 2));
	for (uint32_t cond : conds) {
		forms.push_back(mkDataProc(cond, 1, 0x4, 0, 1, 0, 0x001));
		forms.push_back(mkDataProc(cond, 1, 0x2, 1, 1, 0, 0x001));
		forms.push_back(mkDataProc(cond, 0, 0xD, 1, 0, 0, 0x002));
	}

	std::vector<uint8_t> out;
	put32(out, 0x334A4D41);          // "AMJ3"

	// A trampoline module, emitted with the same writer the regions use so the
	// test is not also debugging a hand-assembled module. Its only export is a
	// function of the bridge signature that forwards to an imported JS
	// function; the test puts that export in the shared table, which is how a
	// JS stub can stand in for ARM710::jitStepOne.
	{
		wasmemit::Module stub;
		stub.importTable();
		// Signature must match the one compileRegion emits for its bridge:
		// (cpu, pc) -> cycles. The stub ignores both and replays the
		// interpreter's recorded effect, but a mismatch here is a runtime trap
		// rather than a comparison failure, so it is worth stating.
		const uint32_t f = stub.addImportFunc("env", "f",
		                                     {wasmemit::VT_I32, wasmemit::VT_I32}, true);
		wasmemit::Func body;
		body.nParams = 2;
		body.localGet(0);
		body.localGet(1);
		body.op(wasmemit::OP_CALL, f);
		stub.addFunc("trampoline", {wasmemit::VT_I32, wasmemit::VT_I32}, true, std::move(body));
		const auto bytes = stub.finish();
		put32(out, (uint32_t)bytes.size());
		out.insert(out.end(), bytes.begin(), bytes.end());
		while (out.size() % 4) out.push_back(0);
	}

	const size_t countPos = out.size();
	put32(out, 0);
	uint32_t emitted = 0, rejected = 0, withBridge = 0, withLoop = 0, withBranch = 0;
	uint32_t withChain = 0, loadPositions = 0, literalPositions = 0, storePositions = 0;

	// Builds one case.
	//
	// The interpreter cannot be asked "run exactly what the region will run" —
	// which path a region takes depends on the guest's flags. So instead the
	// oracle records a TIMELINE: the full architectural state after each
	// interpreter tick, plus the cycles spent to reach it. The region reports
	// how many ticks it retired, and the comparison is against the timeline
	// entry at that index. A wrong tick count shows up as a state mismatch, and
	// the cycle count is compared directly at the same index.
	auto emitCase = [&](const std::vector<uint32_t> &body, int maxInsns,
	                    const uint32_t *seed, uint32_t nzcv, int passes = 1,
	                    const std::vector<std::pair<int, uint32_t>> &regs = {}) -> bool {
		std::vector<uint32_t> page(kPageWords, kNop);
		for (size_t i = 0; i < body.size() && i < kPageWords; i++) page[i] = body[i];

		armjit::CodePage cp{ page.data(), kPageBase, kPageEnd };
		armjit::Region reg;
		if (!armjit::compileRegion(cp, kPageBase, maxInsns, 1, makeLayout(isTVersion), reg))
			return false;

		TestCpu cpu(isTVersion);
		cpu.page = page.data();
		cpu.mem.clear();
		cpu.stores.clear();
		cpu.curTick = 0;
		uint32_t pre[16];
		for (int r = 0; r < 15; r++) pre[r] = seed[r & 3] ^ (uint32_t)(r * 0x01010101u);
		pre[15] = kPageBase + 8;
		// A load needs its base register pointing somewhere the synthetic TLB
		// actually maps, which the seeds cannot arrange on their own.
		for (const auto &rv : regs) pre[rv.first] = rv.second;
		const uint32_t preCpsr = (nzcv << 28) | 0xC0u | 0x13u;
		for (int r = 0; r < 16; r++) cpu.setGpr(r, pre[r]);
		cpu.setCpsrRaw(preCpsr);
		cpu.primePipeline(kPageBase, page.data());

		// Long enough to cover any path the budget allows: every trace position
		// costs one tick except a taken loop edge, which costs three.
		const int timelineLen = (int)(reg.traceTicks + 4) * passes;
		std::vector<uint32_t> timeline;      // 21 words per entry, see below
		uint32_t cum = 0;
		auto snapshot = [&]() {
			for (int r = 0; r < 16; r++) timeline.push_back(cpu.getGprRaw(r));
			timeline.push_back(cpu.getCpsrRaw());
			timeline.push_back(cpu.getPrefetch(1));
			timeline.push_back(cpu.getPrefetch(0));
			timeline.push_back((uint32_t)cpu.getPrefetchCount());
			timeline.push_back(cum);
		};
		snapshot();                          // entry state, index 0
		for (int t = 0; t < timelineLen; t++) {
			cpu.curTick = (uint32_t)t;
			cum += cpu.tick();
			snapshot();
		}

		// The loop-head guard admits a pass while total + traceCycles is not
		// greater than the budget, so N traces' worth of budget runs the body
		// at most N times. passes > 1 is what makes a compiled loop actually
		// iterate rather than fall straight out at the guard.
		const uint32_t maxCycles = reg.traceCycles * (uint32_t)passes;

		put32(out, (uint32_t)body.size());
		for (uint32_t i = 0; i < kPageWords; i++) put32(out, page[i]);
		for (int r = 0; r < 16; r++) put32(out, pre[r]);
		put32(out, preCpsr);
		put32(out, maxCycles);
		put32(out, (uint32_t)reg.nInsns);
		put32(out, (uint32_t)reg.nBridged);
		put32(out, (uint32_t)reg.nSpan);
		put32(out, reg.loops ? 1u : 0u);
		// Per bridge, in the order the region calls them: the tick offset into a
		// pass (which is how the stub indexes the interpreter timeline), whether
		// the fast form was emitted, and the instruction's guest address.
		put32(out, (uint32_t)reg.bridgeAt.size());
		for (size_t b = 0; b < reg.bridgeAt.size(); b++) {
			put32(out, (uint32_t)reg.bridgeAt[b]);
			put32(out, (uint32_t)reg.bridgeFast[b]);
			put32(out, reg.bridgePc[b]);
		}
		// The guest bytes the interpreter wrote, with the tick each landed on:
		// the WASM half replays the ones inside the region's tick count and
		// compares them against what the region actually left in memory.
		put32(out, (uint32_t)cpu.stores.size());
		for (const auto &st : cpu.stores) { put32(out, st[0]); put32(out, st[1]); put32(out, st[2]); }
		const bool hadBridge = reg.nBridged != 0;
		const bool hadLoop = reg.loops;
		const bool hadChain = reg.nChained != 0;
		const uint32_t loads = reg.nInlineLoads;
		put32(out, (uint32_t)(timelineLen + 1));
		for (uint32_t w : timeline) put32(out, w);
		// One region per module here: the test is checking the generated code,
		// not the batching, and a module per case keeps each failure isolated.
		std::vector<armjit::Region> one;
		one.push_back(std::move(reg));
		const auto moduleBytes = armjit::linkRegions(one, makeLayout(isTVersion));
		put32(out, (uint32_t)moduleBytes.size());
		out.insert(out.end(), moduleBytes.begin(), moduleBytes.end());
		while (out.size() % 4) out.push_back(0);
		emitted++;
		if (hadBridge) withBridge++;
		if (hadLoop) withLoop++;
		if (hadChain) withChain++;
		loadPositions += loads;
		literalPositions += reg.nLiterals;
		storePositions += reg.nInlineStores;
		return true;
	};

	// ── 1. One instruction per region ──────────────────────────────────────
	for (uint32_t insn : forms) {
		if (!armjit::acceptsInline(insn, isTVersion)) { rejected++; continue; }
		for (const auto &seed : seeds)
			for (uint32_t nzcv = 0; nzcv < 16; nzcv++)
				emitCase({ insn }, 1, seed, nzcv);
	}

	// ── 2. Multi-instruction straight-line regions ─────────────────────────
	std::vector<uint32_t> pool;
	for (uint32_t insn : forms)
		if (armjit::acceptsInline(insn, isTVersion)) pool.push_back(insn);
	for (uint32_t stride : { 1u, 7u, 31u, 101u }) {
		for (int len = 2; len <= 8; len++) {
			for (uint32_t start = 0; start < pool.size(); start += 17) {
				std::vector<uint32_t> body;
				for (int k = 0; k < len; k++)
					body.push_back(pool[(start + (uint32_t)k * stride) % pool.size()]);
				for (uint32_t nzcv = 0; nzcv < 16; nzcv += 5)
					emitCase(body, len, seeds[start % 5], nzcv);
			}
		}
	}

	// ── 3. Regions containing bridges ──────────────────────────────────────
	// Instructions the inline subset refuses, so the region has to hand them to
	// the interpreter and carry on: a register-specified shift, an MRS, a
	// multiply, and a data-processing op writing r15.
	const uint32_t bridgeForms[] = {
		mkDataProc(0xE, 0, 0x4, 0, 1, 0, (2u << 8) | (1u << 4) | 3u),   // ADD r0,r1,r3,LSL r2
		mkDataProc(0xE, 0, 0x8, 0, 0xF, 0, 0),                          // MRS r0, CPSR
		0xE0000291u,                                                    // MUL r0, r1, r2
		mkDataProc(0xE, 1, 0xD, 0, 0, 15, 0x0FF),                       // MOV r15, #0xFF
	};
	for (uint32_t bf : bridgeForms) {
		for (int pos = 0; pos < 3; pos++) {
			std::vector<uint32_t> body;
			for (int k = 0; k < 4; k++) body.push_back(k == pos ? bf : pool[(uint32_t)k * 13 % pool.size()]);
			for (const auto &seed : seeds)
				for (uint32_t nzcv = 0; nzcv < 16; nzcv += 5)
					emitCase(body, 4, seed, nzcv);
		}
	}

	// ── 4. Regions containing branches ─────────────────────────────────────
	// A forward branch out of the region, and a backward branch to the entry —
	// the loop edge, which is the whole reason regions exist.
	for (uint32_t cond : conds) {
		// forward: three instructions then B to word 8
		{
			std::vector<uint32_t> body;
			for (int k = 0; k < 3; k++) body.push_back(pool[(uint32_t)k * 29 % pool.size()]);
			body.push_back(mkBranch(cond, false, kPageBase + 12, kPageBase + 32));
			for (const auto &seed : seeds)
				for (uint32_t nzcv = 0; nzcv < 16; nzcv += 5)
					if (emitCase(body, 8, seed, nzcv)) withBranch++;
		}
		// backward: three instructions then B back to the entry. Run these with
		// a multi-pass budget too — a loop edge that is only ever taken once
		// exercises the branch but not the loop.
		{
			std::vector<uint32_t> body;
			for (int k = 0; k < 3; k++) body.push_back(pool[(uint32_t)k * 37 % pool.size()]);
			body.push_back(mkBranch(cond, false, kPageBase + 12, kPageBase));
			for (const auto &seed : seeds)
				for (uint32_t nzcv = 0; nzcv < 16; nzcv += 5)
					for (int passes : { 1, 2, 5 })
						if (emitCase(body, 8, seed, nzcv, passes)) withBranch++;
		}
		// A counted loop: SUBS r4,r4,#1 then BNE back to the entry — the shape
		// the whole region design exists for, run long enough to iterate.
		{
			std::vector<uint32_t> body = {
				mkDataProc(0xE, 1, 0x4, 0, 3, 3, 0x001),          // ADD r3,r3,#1
				mkDataProc(0xE, 1, 0x2, 1, 4, 4, 0x001),          // SUBS r4,r4,#1
				mkBranch(0x1, false, kPageBase + 8, kPageBase),   // BNE entry
			};
			for (const auto &seed : seeds)
				for (uint32_t nzcv = 0; nzcv < 16; nzcv += 5)
					for (int passes : { 1, 3, 8 })
						if (emitCase(body, 8, seed, nzcv, passes)) withBranch++;
			(void)cond;
		}
		// BL forward — the link register write
		{
			std::vector<uint32_t> body;
			for (int k = 0; k < 2; k++) body.push_back(pool[(uint32_t)k * 41 % pool.size()]);
			body.push_back(mkBranch(cond, true, kPageBase + 8, kPageBase + 40));
			for (const auto &seed : seeds)
				for (uint32_t nzcv = 0; nzcv < 16; nzcv += 5)
					if (emitCase(body, 8, seed, nzcv)) withBranch++;
		}
	}

	// ── 4b. Chained regions ────────────────────────────────────────────────
	// An unconditional branch the trace FOLLOWS, so the region spans both sides
	// of it. Everything here would have been four instructions and an exit
	// before chaining; the point of each case is that the state on the far side
	// of the branch is the interpreter's, including the six cycles and three
	// tick-equivalents the taken branch and its two pipeline refills cost.
	{
		// One hop: work, B forward, more work.
		{
			std::vector<uint32_t> body(16, kNop);
			for (int k = 0; k < 3; k++) body[k] = pool[(uint32_t)k * 23 % pool.size()];
			body[3] = mkBranch(0xE, false, kPageBase + 12, kPageBase + 32);
			for (int k = 8; k < 12; k++) body[k] = pool[(uint32_t)k * 31 % pool.size()];
			for (const auto &seed : seeds)
				for (uint32_t nzcv = 0; nzcv < 16; nzcv += 5)
					emitCase(body, 32, seed, nzcv);
		}
		// Two hops, the second backwards to an address the trace has not
		// reached — the case where the region's guest words stop being one
		// contiguous run in address order at all.
		{
			std::vector<uint32_t> body(24, kNop);
			body[0] = pool[3 % pool.size()];
			body[1] = mkBranch(0xE, false, kPageBase + 4, kPageBase + 64);
			for (int k = 16; k < 19; k++) body[k] = pool[(uint32_t)k * 17 % pool.size()];
			body[19] = mkBranch(0xE, false, kPageBase + 76, kPageBase + 24);
			for (int k = 6; k < 10; k++) body[k] = pool[(uint32_t)k * 11 % pool.size()];
			for (const auto &seed : seeds)
				for (uint32_t nzcv = 0; nzcv < 16; nzcv += 5)
					emitCase(body, 32, seed, nzcv);
		}
		// A followed BL: the link register is written from the branch's own
		// address even though the region never leaves.
		{
			std::vector<uint32_t> body(20, kNop);
			body[0] = pool[5 % pool.size()];
			body[1] = mkBranch(0xE, true, kPageBase + 4, kPageBase + 48);
			for (int k = 12; k < 15; k++) body[k] = pool[(uint32_t)k * 7 % pool.size()];
			for (const auto &seed : seeds)
				for (uint32_t nzcv = 0; nzcv < 16; nzcv += 5)
					emitCase(body, 32, seed, nzcv);
		}
		// A chain that ends in the loop edge: hop forward, work, then branch
		// back to the entry. The loop head restores the ENTRY pipeline, which is
		// no longer where the trace last was.
		{
			std::vector<uint32_t> body(20, kNop);
			body[0] = mkDataProc(0xE, 1, 0x4, 0, 3, 3, 0x001);      // ADD r3,r3,#1
			body[1] = mkBranch(0xE, false, kPageBase + 4, kPageBase + 40);
			body[10] = mkDataProc(0xE, 1, 0x2, 1, 4, 4, 0x001);     // SUBS r4,r4,#1
			body[11] = mkBranch(0x1, false, kPageBase + 44, kPageBase);   // BNE entry
			for (const auto &seed : seeds)
				for (uint32_t nzcv = 0; nzcv < 16; nzcv += 5)
					for (int passes : { 1, 3, 6 })
						emitCase(body, 32, seed, nzcv, passes);
		}
		// A bridge on the far side of a hop: the bridged instruction's r15 and
		// prefetch pipeline have to describe ITS address, not the trace's
		// entry, and the branch is what moved them apart.
		{
			std::vector<uint32_t> body(20, kNop);
			body[0] = pool[9 % pool.size()];
			body[1] = mkBranch(0xE, false, kPageBase + 4, kPageBase + 36);
			body[9] = 0xE0000291u;                                  // MUL r0,r1,r2
			body[10] = pool[2 % pool.size()];
			for (const auto &seed : seeds)
				for (uint32_t nzcv = 0; nzcv < 16; nzcv += 5)
					emitCase(body, 32, seed, nzcv);
		}
	}

	// ── 4c. Inlined loads ──────────────────────────────────────────────────
	// LDR and LDRB compiled against the synthetic micro-TLB above. Each shape
	// is run against an address the TLB serves AND against one it does not, so
	// both halves of every guard term are exercised: the inline path has to
	// produce the interpreter's value, and the miss path has to hand the
	// instruction over and still produce it.
	{
		struct Site { uint32_t base; const char *what; };
		const Site sites[] = {
			{ kGuest0,          "section, permitted" },
			{ kGuest1,          "small page, sub-page 0 permitted" },
			{ kGuest1 + 0x400,  "small page, sub-page 1 not cached" },
			{ kGuest2,          "large page, sub-page 0 permitted" },
			{ kGuest2 + 0x4000, "large page, sub-page 1 not cached" },
			{ kGuestNull,       "empty micro-TLB way" },
			{ kGuest0 + 2,      "section, unaligned word" },
		};
		for (const auto &site : sites) {
			for (uint32_t cond : { 0xEu, 0x0u, 0x1u }) {
				// Immediate offset, pre-indexed, no write-back.
				{
					std::vector<uint32_t> body = {
						mkLdr(cond, false, true, true, false, false, 4, 0, 8),
						mkDataProc(0xE, 1, 0x4, 0, 3, 3, 0x001),      // ADD r3,r3,#1
						mkLdr(cond, false, true, true, true, false, 4, 1, 5),  // LDRB
					};
					for (const auto &seed : seeds)
						for (uint32_t nzcv = 0; nzcv < 16; nzcv += 5)
							emitCase(body, 8, seed, nzcv, 1, {{4, site.base}});
				}
				// Immediate offset with write-back, and a post-indexed form —
				// the two ways Rn is updated.
				{
					std::vector<uint32_t> body = {
						mkLdr(cond, false, true, true, false, true, 4, 0, 4),
						mkLdr(cond, false, false, true, false, false, 4, 1, 4),
						mkLdr(cond, false, true, false, false, false, 5, 2, 8),
					};
					for (const auto &seed : seeds)
						for (uint32_t nzcv = 0; nzcv < 16; nzcv += 5)
							emitCase(body, 8, seed, nzcv, 1,
							         {{4, site.base}, {5, site.base + 0x20}});
				}
				// Register offset, every shift form including RRX.
				for (uint32_t sh : { 0u, 1u, 2u, 3u }) {
					for (uint32_t by : { 0u, 2u }) {
						std::vector<uint32_t> body = {
							mkLdr(cond, true, true, true, false, false, 4, 0,
							      (by << 7) | (sh << 5) | 6),
							mkDataProc(0xE, 1, 0x4, 0, 3, 3, 0x001),
						};
						for (const auto &seed : seeds)
							emitCase(body, 8, seed, 5, 1, {{4, site.base}, {6, 4}});
					}
				}
			}
		}
	}

	// ── 4c2. Inlined stores ────────────────────────────────────────────────
	// The same sites as the loads, checked on the bytes they leave behind as
	// well as on registers — an inlined store that writes the right value to
	// the wrong address is invisible in a register comparison.
	{
		const uint32_t bases[] = {
			kGuest0, kGuest0 + 4, kGuest0 + 1,          // aligned, offset, unaligned
			kGuest1, kGuest1 + 0x400, kGuest2, kGuestNull,
		};
		for (uint32_t base : bases) {
			for (uint32_t cond : { 0xEu, 0x0u, 0x1u }) {
				// Word and byte, pre-indexed, with and without write-back.
				{
					std::vector<uint32_t> body = {
						mkStr(cond, false, true, true, false, false, 4, 0, 8),
						mkStr(cond, false, true, true, true, false, 4, 1, 3),
						mkDataProc(0xE, 1, 0x4, 0, 3, 3, 0x001),
						mkStr(cond, false, true, true, false, true, 4, 2, 16),
					};
					for (const auto &seed : seeds)
						for (uint32_t nzcv = 0; nzcv < 16; nzcv += 5)
							emitCase(body, 8, seed, nzcv, 1, {{4, base}});
				}
				// Post-indexed, and a store whose value register IS the base —
				// the interpreter reads Rd before it updates Rn, so the
				// original base is what lands in memory.
				{
					std::vector<uint32_t> body = {
						mkStr(cond, false, false, true, false, false, 4, 4, 4),
						mkStr(cond, false, true, false, false, false, 5, 0, 4),
					};
					for (const auto &seed : seeds)
						for (uint32_t nzcv = 0; nzcv < 16; nzcv += 5)
							emitCase(body, 8, seed, nzcv, 1,
							         {{4, base}, {5, base + 0x10}});
				}
				// Register offset, with a shift.
				{
					std::vector<uint32_t> body = {
						mkStr(cond, true, true, true, false, false, 4, 0, (2u << 7) | 6),
						mkDataProc(0xE, 1, 0x4, 0, 3, 3, 0x001),
					};
					for (const auto &seed : seeds)
						emitCase(body, 8, seed, 5, 1, {{4, base}, {6, 2}});
				}
			}
		}
	}

	// ── 4d. Constant-pool loads ────────────────────────────────────────────
	// LDR Rd, [pc, #imm] compiled to the word it reads. The literal is placed
	// in the page after the code, which is where a real constant pool sits;
	// backwards, unaligned and byte forms are here too, and so is one whose
	// literal is off the end of the page and therefore has to be bridged.
	{
		for (uint32_t cond : { 0xEu, 0x0u, 0x1u }) {
			// Forwards to a pool just past the body, and backwards into it.
			for (uint32_t off : { 8u, 16u, 40u }) {
				std::vector<uint32_t> body(24, kNop);
				body[0] = mkLdr(cond, false, true, true, false, false, 15, 0, off);
				body[1] = mkDataProc(0xE, 1, 0x4, 0, 3, 3, 0x001);
				body[2] = mkLdr(cond, false, true, false, false, false, 15, 1, 4);
				body[3] = mkLdr(cond, false, true, true, true, false, 15, 2, off + 1);
				// The pool. Values chosen so a wrong address is obvious.
				for (int k = 6; k < 20; k++) body[k] = 0xC0DE0000u + (uint32_t)k;
				for (const auto &seed : seeds)
					for (uint32_t nzcv = 0; nzcv < 16; nzcv += 5)
						emitCase(body, 8, seed, nzcv);
			}
			// A literal past the end of the page: no constant to fold, so the
			// instruction has to reach the interpreter.
			{
				std::vector<uint32_t> body(4, kNop);
				body[0] = mkLdr(cond, false, true, true, false, false, 15, 0, 0xF00);
				body[1] = mkDataProc(0xE, 1, 0x4, 0, 3, 3, 0x001);
				for (const auto &seed : seeds)
					for (uint32_t nzcv = 0; nzcv < 16; nzcv += 5)
						emitCase(body, 8, seed, nzcv);
			}
		}
	}

	// ── 5. Regression bodies ───────────────────────────────────────────────
	// Exact instruction sequences that diverged in the running emulator. Each
	// one is here because a live self-check found it, and the fastest way to
	// work on it is with the interpreter next door as an oracle.
	{
		const std::vector<std::vector<uint32_t>> regressions = {
			// netBook + Quartz, 0x5000e3a8: a bridged LDR, a not-taken
			// conditional branch, an inline MOV, then a BL out of the region.
			{ 0xE5900018u, 0x1A000004u, 0xE1A01003u, 0xEB00036Au },
			// Series 7, 0x50051a30: ADD r12,pc,#0x78 / SUB pc,r12,r2,LSL #3 —
			// a computed jump whose target can be the very next instruction.
			{ 0xE28FC078u, 0xE04CF182u, 0xE1A00000u, 0xE1A00000u },
			// Series 7, 0x50068e2c: three branches and an inline compare.
			{ 0xCA000002u, 0xE3530000u, 0x0A000002u, 0xEA000003u },
		};
		for (const auto &body : regressions)
			for (const auto &seed : seeds)
				for (uint32_t nzcv = 0; nzcv < 16; nzcv++)
					for (int passes : { 1, 3 })
						emitCase(body, 8, seed, nzcv, passes);
	}

	out[countPos + 0] = emitted & 0xFF;
	out[countPos + 1] = (emitted >> 8) & 0xFF;
	out[countPos + 2] = (emitted >> 16) & 0xFF;
	out[countPos + 3] = (emitted >> 24) & 0xFF;

	FILE *fp = std::fopen(argv[1], "wb");
	if (!fp) { std::perror("fopen"); return 2; }
	std::fwrite(out.data(), 1, out.size(), fp);
	std::fclose(fp);
	std::printf("arm_jit_test: %u cases (%u with a bridge, %u with a loop edge, "
	            "%u with a branch, %u with a followed branch, %u inlined loads, "
	            "%u folded literals, %u inlined stores), "
	            "%u forms rejected by acceptsInline()\n",
	            emitted, withBridge, withLoop, withBranch, withChain, loadPositions,
	            literalPositions, storePositions, rejected);
	return 0;
}
