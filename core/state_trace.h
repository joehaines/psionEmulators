// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include <cstdint>
#include <cstdio>

// ── Differential architectural-state tracing ─────────────────────────
//
// The execution engine is being restructured (block cache, and later a WASM
// code generator), and every one of those changes has to be provably
// behaviour-preserving rather than "the boot screenshot still looks right".
// A screenshot only catches divergence that survives to the panel, and only
// after millions of instructions have washed over it.
//
// So: fold the architectural state at every executed-instruction boundary
// into a running 64-bit FNV-1a hash, and emit (instruction-count, hash) every
// `interval` instructions. Run the same workload twice — reference engine and
// new engine — and diff the two traces. Identical traces prove the engines
// agree on every register, on CPSR and on the cycle count, at every one of
// (say) 250 million instruction boundaries. A differing line brackets the
// first divergence to one interval, and re-running that window with a smaller
// interval walks it down to the instruction.
//
// Cost when disabled is a single predictable branch on `enabled`.
//
//   PSION_STATE_TRACE=<path>       write the trace here
//   PSION_STATE_TRACE_INTERVAL=N   sample every N instructions (default 65536)
//   PSION_STATE_TRACE_FROM=N       also dump a full per-instruction record...
//   PSION_STATE_TRACE_TO=N         ...for instruction indices in [FROM, TO).
//
// Narrowing a divergence is therefore: run both engines at a coarse interval
// to bracket it, then re-run both with FROM/TO around that bracket to get the
// register-level diff at the exact instruction.
struct StateTrace {
	bool      enabled  = false;
	uint64_t  hash     = 0xcbf29ce484222325ull;   // FNV-1a offset basis
	uint64_t  insns    = 0;
	uint64_t  interval = 65536;
	uint64_t  dumpFrom = 0;
	uint64_t  dumpTo   = 0;
	std::FILE *out     = nullptr;

	static constexpr uint64_t kPrime = 0x100000001b3ull;

	inline void mix(uint32_t v) { hash ^= v; hash *= kPrime; }

	// Called once per *executed* instruction (not per pipeline-refill tick) by
	// whichever engine is running, so the two engines' traces are comparable
	// even though they structure their tick loops differently. The cycle
	// counter is folded in as well: two engines that agree on every register
	// but disagree on cycle cost would drive the peripherals differently, and
	// that has to count as a divergence.
	// insn/insn2/pfCount carry the prefetch pipeline. Registers alone are not
	// enough: two engines can hold identical architectural state and still be
	// about to execute different instruction words, because a stale or
	// mis-resolved fetch does not show up until the bad word retires — by which
	// point the trail is cold. Hashing the pipeline catches it at the fetch.
	inline void step(const uint32_t *gprs, uint32_t cpsr, uint64_t cycles,
	                 const int64_t *devCycles,
	                 uint32_t insn, uint32_t insn2, int pfCount,
	                 unsigned kind, unsigned kind2) {
		for (int i = 0; i < 16; i++) mix(gprs[i]);
		mix(cpsr);
		mix((uint32_t)cycles);
		mix((uint32_t)(cycles >> 32));
		mix(insn);
		mix(insn2);
		mix((uint32_t)pfCount);
		// kind/kind2 are dumped but NOT hashed: they are an engine's internal
		// dispatch selector, not architectural state, and the reference
		// interpreter does not compute them at all (it runs the full decode
		// chain). Hashing them would make the two engines incomparable by
		// construction — the exact mistake this harness exists to avoid.
		++insns;
		if (!out) return;
		if (insns >= dumpFrom && insns < dumpTo) {
			// GPRs[15] is the next fetch address at this boundary, so the
			// instruction about to execute lives at GPRs[15] - 8.
			std::fprintf(out, "#%llu pc=%08x cpsr=%08x cyc=%llu dev=%lld "
			             "i=%08x i2=%08x pf=%d k=%u k2=%u",
			             (unsigned long long)insns, gprs[15] - 8, cpsr,
			             (unsigned long long)cycles,
			             (long long)(devCycles ? *devCycles : -1),
			             insn, insn2, pfCount, kind, kind2);
			for (int i = 0; i < 15; i++) std::fprintf(out, " r%d=%08x", i, gprs[i]);
			std::fprintf(out, " r15=%08x\n", gprs[15]);
			std::fflush(out);
		} else if (insns % interval == 0) {
			std::fprintf(out, "%llu %016llx\n",
			             (unsigned long long)insns, (unsigned long long)hash);
			std::fflush(out);
		}
	}

	void open(const char *path, uint64_t iv) {
		if (!path) return;
		out = std::fopen(path, "w");
		if (!out) return;
		if (iv) interval = iv;
		enabled = true;
	}
	void close() {
		if (!out) return;
		// Always emit a final line so two runs of different total length still
		// have a comparable tail, and a short run (< interval) is not silent.
		std::fprintf(out, "%llu %016llx FINAL\n",
		             (unsigned long long)insns, (unsigned long long)hash);
		std::fclose(out);
		out = nullptr;
		enabled = false;
	}
};
