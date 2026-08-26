// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include "emubase.h"
#include <cstddef>
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
	// Guest code replaced host-side: drop the decoded pages too, immutable ones
	// included — "immutable" means the guest cannot write them, not that a ROM
	// image cannot be swapped underneath.
	void onGuestCodeReplaced() override {
		invalidateDecodeCache(/*includeImmutable=*/true);
		invalidateFetchPage();
		ARM710::onGuestCodeReplaced();
	}

	std::pair<MaybeU32, MMUFault> readVirtual(uint32_t virtAddr, ValueSize valueSize) override;
	std::pair<MaybeU32, MMUFault> fetchVirtual(uint32_t virtAddr) override;
	MMUFault writeVirtual(uint32_t value, uint32_t virtAddr, ValueSize valueSize) override;
	// Resolve a code page for tickPageLoop: host pointer + decoded-kind array for
	// the 1 KB subpage containing va (subpage so the fetch perm stays uniform).
	bool resolveFetchPage(uint32_t va, FetchPage &out) override;
	uint8_t *fastSpanPtr(uint32_t va, uint32_t bytes, bool write) override;

	// The data-side micro-TLB, described for the code generator — see
	// ARM710::JitMemSeam. Offsets come from offsetof rather than being written
	// out, so a field added to FastTlbEntry cannot silently move the ones the
	// generated code reads.
	bool jitMemSeam(JitMemSeam &out) const override {
		out.mruBase = reinterpret_cast<uintptr_t>(&dataMru_[0]);
		out.mruWayMask = DataMruWays - 1;
		out.mruShift = 12;                       // matches dataMruIdx()
		out.offAddrMask   = (uint32_t)offsetof(FastTlbEntry, addrMask);
		out.offAddr       = (uint32_t)offsetof(FastTlbEntry, addr);
		out.offLv1        = (uint32_t)offsetof(FastTlbEntry, lv1Entry);
		out.offLv2        = (uint32_t)offsetof(FastTlbEntry, lv2Entry);
		out.offPermCache  = (uint32_t)offsetof(FastTlbEntry, permCache);
		out.offPermCachePg= (uint32_t)offsetof(FastTlbEntry, permCachePg);
		out.offHostRead   = (uint32_t)offsetof(FastTlbEntry, hostReadPtr);
		out.offHostWrite  = (uint32_t)offsetof(FastTlbEntry, hostWritePtr);
		out.decodePagesPtr = reinterpret_cast<uintptr_t>(&decodePages_);
		out.decodePageSlotMask = DecodePageSlots - 1;
		out.decodePageStride = (uint32_t)sizeof(DecodedPage);
		out.offDecodePhysBase = (uint32_t)offsetof(DecodedPage, physBase);
		return true;
	}

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
	// Data-side micro-TLB.
	//
	// lastFastTlbEntry is a SINGLE most-recently-used entry shared by every
	// data access, and a typical instruction stream touches stack, globals and
	// heap in turn — so it ping-pongs. Measured on a Series 7 desktop boot:
	// 51,886,944 readVirtual calls, 33,083,397 of them falling through to the
	// full translateFast() + checkPermsfast() walk. That is a 63.8% MISS rate,
	// against the "80-95% hit rate" this header used to claim. The instruction
	// side already got its own MRU (lastFetchEntry) and hits 93.3%; this is the
	// same fix for the data side, widened to a small direct-mapped set so
	// stack/globals/heap can be resident at once instead of evicting each other.
	//
	// Indexed on VA bits 12+ so 4 KB pages spread across the ways; a 1 MB
	// section entry simply ends up cached in several slots, which costs nothing
	// but a little redundancy. Entries live in the fixed fastTlb[][] array and
	// are never freed, so a stale pointer here stays valid memory — and every
	// use re-checks addrMask/addr, which is authoritative. A slot is therefore
	// only ever a HINT: it can miss, it can never mis-serve.
	enum { DataMruWays = 8 };
	FastTlbEntry *dataMru_[DataMruWays] = {};
	static int dataMruIdx(uint32_t va) { return (int)((va >> 12) & (DataMruWays - 1)); }
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

	// ── Phys-keyed code-page validity tokens ──────────────────────────────
	// One slot per touched 4 KB *physical* code page. Keyed by PHYSICAL address
	// so a slot survives VA→PA remaps / process switches (TTBR changes) with no
	// flush — the whole point vs. a VA key. Direct-mapped over a small slot set.
	// Lazily allocated; excluded from heap snapshots and cleared on restore
	// (it's a derived accelerator). `immutable` (no host write pointer ⇒ ROM) is
	// never invalidated by a TLB flush, so process switches don't drop ROM pages.
	//
	// These slots used to carry a 1 KB `kind[]` array — one ArmDK byte per word,
	// so a hot loop could read a cached classification instead of re-deriving
	// it. Nothing reads a cached kind any more: both engines classify from the
	// instruction word at the point of dispatch, because a cached or carried
	// kind can go stale or out of step and then silently dispatch the wrong
	// handler (see ARM710::tickPageLoop). What is left is the part the fetch
	// path still needs — a token that says "this physical page has not been
	// written since you resolved it", which is what `physBase` is: the page loop
	// holds &physBase and compares. Dropping kind[] took a slot from 1032 bytes
	// to 8, so the whole table is 2 KB and stays in L1 instead of costing a
	// cache miss per resolve, and a slot eviction no longer memsets 1 KB.
	struct DecodedPage {
		uint32_t physBase = 0xFFFFFFFFu;   // 4 KB-aligned phys; sentinel = empty slot
		bool     immutable = false;        // ROM page (no write pointer) → never dropped
	};
	enum { DecodePageSlots = 256 };        // 256 × 4 KB pages addressable
	DecodedPage *decodePages_ = nullptr;   // lazily allocated array[DecodePageSlots]
	// Returns the slot for physAddr's 4 KB page, (re)initialising it on a miss
	// (collision evicts). nullptr only on allocation failure.
	DecodedPage *decodePageFor(uint32_t physAddr);
	// Invalidation: drop everything non-immutable (TLB flush / I-cache flush /
	// MMU enable edge), one page (per-page code write / per-VA flush), or all
	// (snapshot restore).
	void invalidateDecodeCache(bool includeImmutable);
	void invalidateDecodePage(uint32_t physAddr);
	// Builds the packed four-combo permission byte (see FastTlbEntry) for an
	// entry whose effective 2-bit AP field is `accessPerms` and whose domain is
	// `domain`. Returns 0 for a No-Access / Reserved domain (left uncached so
	// those keep faulting through the slow path). Mirrors checkAccessPermissions
	// exactly so a cache hit is identical to the full walk.
	uint8_t buildPermCacheByte(int domain, int accessPerms) const;
};
