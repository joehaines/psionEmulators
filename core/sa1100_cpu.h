// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include "emubase.h"
#include <vector>

// SA-1100-specific ARM710 bridge with TLB performance extensions.
// Overrides readVirtual / writeVirtual with optimised paths:
//   - lastTlbEntry single-entry cache (80-95% hit rate)
//   - Host-pointer fast path (skip readPhysical for RAM/ROM)
//   - Permission-decision cache for section entries
//   - Set-associative TLB (16 sets × 4 ways)
//
// These live in a separate subclass so the shared ARM710 TlbEntry
// and TLB layout are unchanged — Windermere / CLPS devices use the
// unmodified ARM710 code path.

class SA1100Bridge : public Arm710Bridge {
public:
	using Arm710Bridge::Arm710Bridge;

	struct FastTlbEntry {
		uint32_t addrMask, addr, lv1Entry, lv2Entry;
		// Permission-decision cache. permCache covers SECTION entries (uniform
		// AP across the 1 MB). permCachePg[] covers PAGE entries, one byte per
		// 1 KB sub-page (ARM small/large pages carry four AP fields selected by
		// the faulting address), so paged kernel/user memory — the common case
		// — no longer falls back to the full checkAccessPermissions() walk on
		// every access. Each byte packs the four (privileged × write) outcomes:
		// bits 4-7 = "this combo computed", bits 0-3 = "…and it is permitted".
		// Both are cleared by invalidatePermCache()/allocateFastTlbEntry() and
		// wiped by a TLB flush, exactly like the section cache.
		uint8_t  permCache;
		uint8_t  permCachePg[4];
		uint8_t* hostReadPtr;
		uint8_t* hostWritePtr;
	};

	void flushTlb() override;
	void flushTlb(uint32_t virtAddr) override;
	void onMmuPermConfigChanged() override { invalidatePermCache(); }
	void onICacheFlush() override { invalidateDecodeCache(/*includeImmutable=*/false); }

	std::pair<MaybeU32, MMUFault> readVirtual(uint32_t virtAddr, ValueSize valueSize) override;
	std::pair<MaybeU32, MMUFault> fetchVirtual(uint32_t virtAddr) override;
	MMUFault writeVirtual(uint32_t value, uint32_t virtAddr, ValueSize valueSize) override;
	// Resolve a code page for tickPageLoop: host pointer + decoded-kind array for
	// the 1 KB subpage containing va (subpage so the fetch perm stays uniform).
	bool resolveFetchPage(uint32_t va, FetchPage &out) override;

	// Authoritative (no-inlined-fast-path) read/write — the translateFast() +
	// checkPermsfast() + host-pointer/physical sequence.  Split out so the
	// inlined fast paths have a single kill-switch target (PSION_NO_MEM_FASTPATH)
	// and a cross-check reference (PSION_MEM_FASTPATH_CHECK).
	std::pair<MaybeU32, MMUFault> readVirtualAuthoritative(uint32_t virtAddr, ValueSize valueSize);
	MMUFault writeVirtualAuthoritative(uint32_t value, uint32_t virtAddr, ValueSize valueSize);
	void checkFastDivergence(const char *tag, uint32_t virtAddr, ValueSize valueSize,
	                         const std::pair<MaybeU32, MMUFault> &fast,
	                         const std::pair<MaybeU32, MMUFault> &auth);

	// Host-pointer resolution — implemented in sa1100.cpp where the
	// RAM/ROM buffer layout is known.
	uint8_t* hostPtrForRead(uint32_t physAddr) const;
	uint8_t* hostPtrForWrite(uint32_t physAddr) const;

private:
	enum { FastTlbSets = 16, FastTlbWays = 4 };
	FastTlbEntry fastTlb[FastTlbSets][FastTlbWays] = {};
	int fastTlbNextWay[FastTlbSets] = {};
	FastTlbEntry *lastFastTlbEntry = nullptr;
	// Separate MRU for instruction fetches (see fetchVirtual). Keeps the code
	// page cached across interleaved data accesses so the fetch path doesn't
	// thrash lastFastTlbEntry.
	FastTlbEntry *lastFetchEntry = nullptr;

	static int setForAddr(uint32_t addr) { return (addr >> 20) & (FastTlbSets - 1); }
	void flushFastTlb();
	void flushFastTlb(uint32_t virtAddr);
	void invalidatePermCache();
	FastTlbEntry *allocateFastTlbEntry(uint32_t addrMask, uint32_t addr);

	std::variant<FastTlbEntry *, MMUFault> translateFast(uint32_t virtAddr);
	MMUFault checkPermsfast(FastTlbEntry *entry, uint32_t virtAddr, bool isWrite);

	// ── Phys-keyed decoded-instruction cache (docs/execution-engine-scope.md) ──
	// One ArmDK kind-byte per 32-bit word of a touched 4 KB *physical* code page,
	// so a hot loop reads the cached classification instead of re-deriving it
	// every fetch. Keyed by PHYSICAL address so it survives VA→PA remaps / process
	// switches (TTBR changes) with no flush — the whole point vs. a VA key.
	// Direct-mapped over a small slot set: the hot code working set is a handful
	// of pages that stay in host cache, whereas a flat ROM-sized table lost to
	// cache misses (see the scope doc's post-mortem). Lazily allocated; excluded
	// from heap snapshots and cleared on restore (it's a derived accelerator).
	// `immutable` (no host write pointer ⇒ ROM) is never invalidated by a TLB
	// flush, so process switches don't blow away the ROM code classification.
	struct DecodedPage {
		uint32_t physBase = 0xFFFFFFFFu;   // 4 KB-aligned phys; sentinel = empty slot
		bool     immutable = false;        // ROM page (no write pointer) → never dropped
		uint8_t  kind[1024];               // ArmDK per word; DK_UNCACHED (0) = decode me
	};
	enum { DecodePageSlots = 256 };        // 256 × 4 KB pages addressable (~256 KB)
	DecodedPage *decodePages_ = nullptr;   // lazily allocated array[DecodePageSlots]
	// Returns the slot for physAddr's 4 KB page, (re)initialising it on a miss
	// (collision evicts). nullptr only on allocation failure. Caller indexes
	// kind[(physAddr & 0xFFF) >> 2].
	DecodedPage *decodePageFor(uint32_t physAddr);
	// Invalidation: drop everything non-immutable (TLB flush / I-cache flush /
	// MMU enable edge), one page (per-page code write / per-VA flush), or all
	// (snapshot restore).
	void invalidateDecodeCache(bool includeImmutable);
	void invalidateDecodePage(uint32_t physAddr);
	void checkDecodeDivergence(uint32_t physAddr, uint32_t insn, uint8_t cachedKind);
	// Builds the packed four-combo permission byte (see FastTlbEntry) for an
	// entry whose effective 2-bit AP field is `accessPerms` and whose domain is
	// `domain`. Returns 0 for a No-Access / Reserved domain (left uncached so
	// those keep faulting through the slow path). Mirrors checkAccessPermissions
	// exactly so a cache hit is identical to the full walk.
	uint8_t buildPermCacheByte(int domain, int accessPerms) const;
};
