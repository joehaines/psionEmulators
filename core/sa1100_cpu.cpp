// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "sa1100_cpu.h"
#include "sa1100.h"
#include <cstring>
#include <cstdio>
#include <new>

// Cache each diagnostic env var lookup in a per-call-site function-local static
// so a hot-path PSION_* probe is a single getenv at startup, not a libc env scan
// on every memory access (callgrind: raw getenv was ~35% of native runtime).
#define PSION_ENV_CSTR(name)  ([] { static const char *const _v = std::getenv(name); return _v; }())

// ── Host-pointer resolution ─────────────────────────────────────────
// Returns a pointer biased so that `*(uintN_t*)(p + physAddr)` yields
// the same value as readPhysical(physAddr). nullptr = slow path.

uint8_t* SA1100Bridge::hostPtrForRead(uint32_t physAddr) const {
	auto *sa = static_cast<const SA1100::Emulator *>(
		static_cast<const Arm710Bridge *>(this)->getOwner());
	uint8_t region = (uint8_t)(physAddr >> 24);
	if (region < 0x10)
		return const_cast<uint8_t*>(sa->romPtr()) - (physAddr & ~uint32_t(SA1100::Emulator::kRomSize - 1));
	if (region >= 0xC0 && region <= 0xC7)
		return const_cast<uint8_t*>(sa->ramPtr()) - (physAddr & ~uint32_t(SA1100::Emulator::kBankMask));
	if (region >= 0xC8 && region <= 0xCF)
		return const_cast<uint8_t*>(sa->ram2Ptr()) - (physAddr & ~uint32_t(SA1100::Emulator::kBankMask));
	// Banks 2/3 on 4-bank machines (netpad); a bank-0 read alias on 2-bank
	// ones.  MUST match readPhysical's split or the same physical address
	// resolves to two different buffers depending on which path took it.
	if (region >= 0xD0 && region <= 0xD7)
		return const_cast<uint8_t*>(sa->ramBanks() >= 4 ? sa->ram3Ptr() : sa->ramPtr()) - (physAddr & ~uint32_t(SA1100::Emulator::kBankMask));
	if (region >= 0xD8 && region <= 0xDF)
		return const_cast<uint8_t*>(sa->ramBanks() >= 4 ? sa->ram4Ptr() : sa->ramPtr()) - (physAddr & ~uint32_t(SA1100::Emulator::kBankMask));
	return nullptr;
}

uint8_t* SA1100Bridge::hostPtrForWrite(uint32_t physAddr) const {
	auto *sa = static_cast<const SA1100::Emulator *>(
		static_cast<const Arm710Bridge *>(this)->getOwner());
	uint8_t region = (uint8_t)(physAddr >> 24);
	if (region >= 0xC0 && region <= 0xC7)
		return const_cast<uint8_t*>(sa->ramPtr()) - (physAddr & ~uint32_t(SA1100::Emulator::kBankMask));
	if (region >= 0xC8 && region <= 0xCF)
		return const_cast<uint8_t*>(sa->ram2Ptr()) - (physAddr & ~uint32_t(SA1100::Emulator::kBankMask));
	// See hostPtrForRead: banks 2/3 on 4-bank machines, bank-0 alias on
	// 2-bank ones.  Writes especially must not disagree with
	// writePhysical — that loses data at the boundary (the netpad parks
	// its sleep-resume context in bank 3).
	if (region >= 0xD0 && region <= 0xD7)
		return const_cast<uint8_t*>(sa->ramBanks() >= 4 ? sa->ram3Ptr() : sa->ramPtr()) - (physAddr & ~uint32_t(SA1100::Emulator::kBankMask));
	if (region >= 0xD8 && region <= 0xDF)
		return const_cast<uint8_t*>(sa->ramBanks() >= 4 ? sa->ram4Ptr() : sa->ramPtr()) - (physAddr & ~uint32_t(SA1100::Emulator::kBankMask));
	return nullptr;
}

// ── Phys-keyed decoded-instruction cache ─────────────────────────────
// See the DecodedPage doc in sa1100_cpu.h. Lazily allocates one fixed
// direct-mapped page array; (re)inits a slot on a phys-page miss (a
// collision evicts the prior page — fine, the hot working set is small).
// NOTE: not yet wired into fetchVirtual — scaffolding only (commit 2).

SA1100Bridge::DecodedPage *SA1100Bridge::decodePageFor(uint32_t physAddr) {
	if (!decodePages_) {
		decodePages_ = new (std::nothrow) DecodedPage[DecodePageSlots];
		if (!decodePages_) return nullptr;
		// Engagement breadcrumb, only in CHECK runs (stderr, not log(), so it
		// surfaces even when the harness disables the logger for clean output).
		if (decodeCacheCheck_)
			std::fprintf(stderr, "[decode-cache] active (first page allocated)\n");
	}
	uint32_t base = physAddr & ~uint32_t(0xFFF);
	DecodedPage &p = decodePages_[(base >> 12) & (DecodePageSlots - 1)];
	if (p.physBase != base) {
		p.physBase  = base;
		p.immutable = (hostPtrForWrite(base) == nullptr);   // no write pointer ⇒ ROM
	}
	return &p;
}

void SA1100Bridge::invalidateDecodeCache(bool includeImmutable) {
	if (!decodePages_) return;
	for (int i = 0; i < DecodePageSlots; i++)
		if (includeImmutable || !decodePages_[i].immutable)
			decodePages_[i].physBase = 0xFFFFFFFFu;   // sentinel ⇒ re-init on next use
}

void SA1100Bridge::invalidateDecodePage(uint32_t physAddr) {
	if (!decodePages_) return;
	uint32_t base = physAddr & ~uint32_t(0xFFF);
	DecodedPage &p = decodePages_[(base >> 12) & (DecodePageSlots - 1)];
	if (p.physBase == base) p.physBase = 0xFFFFFFFFu;
}

// ── TLB flush overrides ──────────────────────────────────────────────
// Flush both the base ARM710 TLB and our fast TLB so cp15 c8 writes
// and clearAllValues() invalidate everything.

void SA1100Bridge::flushTlb() {
	ARM710::flushTlb();
	flushFastTlb();
	// A full TLB flush accompanies process switches (TTBR change) and page-table
	// edits. Decoded ops are phys-keyed so a pure VA remap wouldn't strictly need
	// this, but a flush can also accompany RAM being repurposed — drop the
	// non-immutable (RAM) pages; keep ROM (its phys contents can never change).
	invalidateDecodeCache(/*includeImmutable=*/false);
}

void SA1100Bridge::flushTlb(uint32_t virtAddr) {
	ARM710::flushTlb(virtAddr);
	flushFastTlb(virtAddr);
	// Phys-keyed: a single-VA remap changes which phys page that VA resolves to,
	// which the next fetch looks up by its own phys key — so no decode-cache
	// action is needed here. (The CHECK mode validates this assumption.)
}

// ── Fast TLB management ─────────────────────────────────────────────

void SA1100Bridge::flushFastTlb() {
	for (int s = 0; s < FastTlbSets; s++)
		for (int w = 0; w < FastTlbWays; w++)
			fastTlb[s][w] = FastTlbEntry{};
	lastFastTlbEntry = nullptr;
	lastFetchEntry = nullptr;
	invalidateFetchPage();   // mapping may have moved under the resolved page
	for (int w = 0; w < DataMruWays; w++) dataMru_[w] = nullptr;
	for (int s = 0; s < FastTlbSets; s++) fastTlbNextWay[s] = 0;
}

void SA1100Bridge::flushFastTlb(uint32_t virtAddr) {
	int set = setForAddr(virtAddr);
	for (int w = 0; w < FastTlbWays; w++) {
		FastTlbEntry &e = fastTlb[set][w];
		if (e.addrMask && (virtAddr & e.addrMask) == e.addr) {
			e = FastTlbEntry{};
			break;
		}
	}
	lastFastTlbEntry = nullptr;
	lastFetchEntry = nullptr;
	invalidateFetchPage();   // mapping may have moved under the resolved page
	for (int w = 0; w < DataMruWays; w++) dataMru_[w] = nullptr;
}

void SA1100Bridge::invalidatePermCache() {
	for (int s = 0; s < FastTlbSets; s++)
		for (int w = 0; w < FastTlbWays; w++) {
			fastTlb[s][w].permCache = 0;
			std::memset(fastTlb[s][w].permCachePg, 0, sizeof(fastTlb[s][w].permCachePg));
		}
}

SA1100Bridge::FastTlbEntry *SA1100Bridge::allocateFastTlbEntry(uint32_t addrMask, uint32_t addr) {
	int set = setForAddr(addr & addrMask);
	FastTlbEntry *entry = &fastTlb[set][fastTlbNextWay[set]];
	fastTlbNextWay[set] = (fastTlbNextWay[set] + 1) % FastTlbWays;
	entry->addrMask = addrMask;
	entry->addr = addr & addrMask;
	entry->permCache = 0;
	std::memset(entry->permCachePg, 0, sizeof(entry->permCachePg));
	entry->hostReadPtr = nullptr;
	entry->hostWritePtr = nullptr;
	return entry;
}

// ── Fast TLB translate ──────────────────────────────────────────────

std::variant<SA1100Bridge::FastTlbEntry *, ARM710::MMUFault>
SA1100Bridge::translateFast(uint32_t virtAddr) {
	// Last-hit cache
	if (lastFastTlbEntry && lastFastTlbEntry->addrMask &&
	    (virtAddr & lastFastTlbEntry->addrMask) == lastFastTlbEntry->addr)
		return lastFastTlbEntry;

	// Set-associative lookup
	int set = setForAddr(virtAddr);
	for (int w = 0; w < FastTlbWays; w++) {
		FastTlbEntry &e = fastTlb[set][w];
		if (e.addrMask && (virtAddr & e.addrMask) == e.addr) {
			lastFastTlbEntry = &e;
			return &e;
		}
	}

	// Miss — fall back to the ARM710 page-table walker, then copy
	// the result into our fast TLB.
	auto result = translateAddressUsingTlb(virtAddr);
	if (auto *fault = std::get_if<MMUFault>(&result))
		return *fault;
	auto *base = std::get<TlbEntry *>(result);

	FastTlbEntry *fe = allocateFastTlbEntry(base->addrMask, base->addr);
	fe->lv1Entry = base->lv1Entry;
	fe->lv2Entry = base->lv2Entry;

	// Fill host-pointer cache
	uint32_t physBase = physAddrFromTlbEntry(base, base->addr);
	uint8_t *baseR = hostPtrForRead(physBase);
	uint8_t *baseW = hostPtrForWrite(physBase);
	intptr_t delta = (intptr_t)physBase - (intptr_t)fe->addr;
	fe->hostReadPtr  = baseR ? (baseR + delta) : nullptr;
	fe->hostWritePtr = baseW ? (baseW + delta) : nullptr;

	lastFastTlbEntry = fe;
	return fe;
}

// ── Permission check with cache ─────────────────────────────────────

// Computes the packed four-combo permission byte for an entry whose effective
// 2-bit AP field is `accessPerms` under domain `domain`.  Mirrors
// checkAccessPermissions() exactly (Manager passes everything; Client runs the
// System/ROM × AP truth table; No-Access / Reserved return 0 = "do not cache").
uint8_t SA1100Bridge::buildPermCacheByte(int domain, int accessPerms) const {
	int primaryAccessControls = (cp15_domainAccessControl >> (domain * 2)) & 3;
	if (primaryAccessControls == 3)
		return 0xFF;                          // manager — all four combos pass
	if (primaryAccessControls != 1)
		return 0;                             // No-Access / Reserved — uncacheable
	const bool System = cp15_control & 0x100;
	const bool ROM    = cp15_control & 0x200;
	auto wouldPass = [&](bool priv, bool write) -> bool {
		if (accessPerms == 0) {
			if (!System && !ROM) return false;
			if (System && !ROM) return !write && priv;
			if (!System && ROM) return !write;
			return false;
		}
		if (accessPerms == 1) return priv;
		if (accessPerms == 2) return !write || priv;
		return true;
	};
	uint8_t cache = 0xF0;                      // all four combos computed
	if (wouldPass(true,  false)) cache |= 0x01;
	if (wouldPass(true,  true))  cache |= 0x02;
	if (wouldPass(false, false)) cache |= 0x04;
	if (wouldPass(false, true))  cache |= 0x08;
	return cache;
}

ARM710::MMUFault SA1100Bridge::checkPermsfast(FastTlbEntry *entry, uint32_t virtAddr, bool isWrite) {
	const bool isPage = entry->lv2Entry != 0;

	// Select the cache byte for this access: the single section byte, or — for
	// a page — the per-sub-page byte (ARM small/large pages carry four AP
	// fields, indexed by the faulting address).
	int subPage = 0;
	if (isPage)
		subPage = ((entry->lv2Entry & 3) == 1) ? ((virtAddr >> 14) & 3)   // 64 KB
		                                        : ((virtAddr >> 10) & 3);  // 4 KB
	const uint8_t cached = isPage ? entry->permCachePg[subPage] : entry->permCache;

	// Cache fast path.
	if (cached) {
		const bool priv = isPrivileged();
		const int shift = (priv ? 0 : 2) + (isWrite ? 1 : 0);
		if (cached & (uint8_t)(1u << (shift + 4))) {       // combo computed?
			return (cached & (uint8_t)(1u << shift)) ? NoFault
			    : encodeFaultSorP(SorPPermissionFault, isPage,
			                      (entry->lv1Entry >> 5) & 0xF, virtAddr);
		}
	}

	// Slow path — delegate to the authoritative ARM710 implementation.
	TlbEntry tmp;
	tmp.addrMask = entry->addrMask;
	tmp.addr     = entry->addr;
	tmp.lv1Entry = entry->lv1Entry;
	tmp.lv2Entry = entry->lv2Entry;
	auto f = checkAccessPermissions(&tmp, virtAddr, isWrite);

	// Backfill the cache (only when this access passed; a faulting access is
	// rare and just re-walks).  Compute the effective AP field for this entry /
	// sub-page, then all four (priv × write) outcomes via the shared helper.
	if (f == NoFault) {
		const int domain = (entry->lv1Entry >> 5) & 0xF;
		int accessPerms;
		if (isPage)
			accessPerms = (entry->lv2Entry >> (4 + subPage * 2)) & 3;
		else
			accessPerms = (entry->lv1Entry >> 10) & 3;
		const uint8_t cacheByte = buildPermCacheByte(domain, accessPerms);
		if (cacheByte) {
			if (isPage) entry->permCachePg[subPage] = cacheByte;
			else        entry->permCache            = cacheByte;
		}
	}

	return f;
}

// ── Optimised readVirtual ───────────────────────────────────────────

std::pair<MaybeU32, ARM710::MMUFault>
SA1100Bridge::readVirtual(uint32_t virtAddr, ValueSize valueSize) {
	// Recording-timer investigation read-watches (opt-in only).  These ran on
	// EVERY read in the host-pointer fast path; even capped, the per-access
	// address compares plus the fprintf-to-console hits made the netBook OS UI
	// render very slowly in the WASM build (console writes are far costlier
	// than native stderr).  Gate the whole block behind PSION_REC_WATCH so
	// normal runs pay a single cached-bool branch.
	static const bool kRecWatch = PSION_ENV_CSTR("PSION_REC_WATCH") != nullptr;
	if (kRecWatch) {
	if ((virtAddr & ~3u) == 0x80000228u && valueSize == V32) {
		static int rn = 0;
		if (rn++ < 50) {
			std::fprintf(stderr,
				"[h13-vr] va=%08x pc=%08x lr=%08x cpsr=%08x\n",
				virtAddr, getGPR(15), getGPR(14), getCPSR());
		}
	}
	// Watch reads on the recording NTimer's vtable slot [+0x08]
	// (= 0x5029cc48, contains 0x5029883c = trampoline to recording
	// handler).  If anything reads this, that's the dispatcher
	// resolving the callback.
	if ((virtAddr & ~3u) == 0x5029cc48u && valueSize == V32) {
		static int rn = 0;
		if (rn++ < 40) {
			std::fprintf(stderr,
				"[vt08-vr] va=%08x pc=%08x lr=%08x cpsr=%08x r0=%08x\n",
				virtAddr, getGPR(15), getGPR(14), getCPSR(),
				getGPR(0));
		}
	}
	// Also watch reads of the ntimer's vtable ptr at ntimer[+0x10]
	// (= channel[+0x18] = virt 0x8031f080).
	if ((virtAddr & ~3u) == 0x8031f080u && valueSize == V32) {
		static int rn = 0;
		if (rn++ < 40) {
			std::fprintf(stderr,
				"[ntimVT-vr] va=%08x pc=%08x lr=%08x cpsr=%08x\n",
				virtAddr, getGPR(15), getGPR(14), getCPSR());
		}
	}
	// Read-watch ntimer 0x1B (a working kernel NTimer added at
	// boot, at virt 0x80000eb4).  Captures any read in its 16-byte
	// struct.  If the kernel's expiry dispatcher walks NTimer
	// objects, we'll see periodic reads here — and we can compare
	// against the recording NTimer (0x8031f070) which gets no reads.
	if (valueSize == V32 &&
	    virtAddr >= 0x80000eb4u && virtAddr < 0x80000ec4u) {
		static int rn = 0;
		if (rn++ < 50) {
			std::fprintf(stderr,
				"[ntim1b-vr] va=%08x off=%02x pc=%08x lr=%08x cpsr=%08x\n",
				virtAddr, (virtAddr - 0x80000eb4u),
				getGPR(15), getGPR(14), getCPSR());
		}
	}
	// Read watch on ntimer[+4] (active flag) at virt 0x8031f074.
	// If any kernel dispatcher polls per-NTimer active flags, we'll
	// see reads here even when handler[0x13] isn't being walked.
	if ((virtAddr & ~3u) == 0x8031f074u && valueSize == V32) {
		static int rn = 0;
		if (rn++ < 400) {
			std::fprintf(stderr,
				"[ntim4-vr] va=%08x pc=%08x lr=%08x cpsr=%08x\n",
				virtAddr, getGPR(15), getGPR(14), getCPSR());
		}
	}
	// Read watch on extObj+0x14 = 0x8031f768 — this is the gate
	// the completion fn checks at 0x5000e088 (LDR R1, [R0, #0x14]).
	// If our fix's write of 3 isn't visible at this address, the
	// CMP R1, #3 fails and STREQ never fires.
	if ((virtAddr & ~3u) == 0x8031f768u && valueSize == V32) {
		static int rn = 0;
		if (rn++ < 400) {
			std::fprintf(stderr,
				"[ext14-vr] va=%08x pc=%08x lr=%08x cpsr=%08x ttb=%08x\n",
				virtAddr, getGPR(15), getGPR(14), getCPSR(), getCp15Ttb());
		}
	}
	// Read watch on extObj+0x24 = 0x8031f778 — the gate at
	// 0x5000e07c (LDR R12, [R0, #0x24]; CMP R12, #3).  Filter to
	// only show reads whose LR is in the DFC chain (0x50295a00..
	// 0x50295a20) so we don't drown in natural kernel dispatch.
	// DFC-chain-only version of ext14-vr.
	if ((virtAddr & ~3u) == 0x8031f768u && valueSize == V32 &&
	    getGPR(14) >= 0x50295a00u && getGPR(14) < 0x50295a20u) {
		static int rn = 0;
		if (rn++ < 30) {
			auto pa = translateAddressUsingTlb(virtAddr);
			uint32_t val = 0xDEADBEEF;
			if (auto *te = std::get_if<TlbEntry *>(&pa)) {
				uint32_t physAddr = physAddrFromTlbEntry(*te, virtAddr);
				auto *sa = static_cast<SA1100::Emulator *>(
					static_cast<Arm710Bridge *>(this)->getOwner());
				val = sa->readPhysical(physAddr, V32).value_or(0xDEADBEEF);
			}
			std::fprintf(stderr,
				"[ext14-vr-rec] va=%08x val=%08x pc=%08x lr=%08x cpsr=%08x\n",
				virtAddr, val, getGPR(15), getGPR(14), getCPSR());
		}
	}
	if ((virtAddr & ~3u) == 0x8031f778u && valueSize == V32 &&
	    getGPR(14) >= 0x50295a00u && getGPR(14) < 0x50295a20u) {
		static int rn = 0;
		if (rn++ < 30) {
			// Capture the value the CPU is ABOUT to load by doing
			// our own physical read.
			auto *sa = static_cast<SA1100::Emulator *>(
				static_cast<Arm710Bridge *>(this)->getOwner());
			auto pa = translateAddressUsingTlb(virtAddr);
			uint32_t val = 0xDEADBEEF;
			if (auto *te = std::get_if<TlbEntry *>(&pa)) {
				uint32_t physAddr = physAddrFromTlbEntry(*te, virtAddr);
				val = sa->readPhysical(physAddr, V32).value_or(0xDEADBEEF);
			}
			std::fprintf(stderr,
				"[ext24-vr-rec] va=%08x val=%08x pc=%08x lr=%08x cpsr=%08x ttb=%08x\n",
				virtAddr, val, getGPR(15), getGPR(14), getCPSR(), getCp15Ttb());
		}
	}
	// Read watch on own+0x24 = 0x80320678 (the TRequestStatus
	// pointer slot).  Tells us when/if any kernel code polls or
	// checks it.
	if ((virtAddr & ~3u) == 0x80320678u && valueSize == V32) {
		static int rn = 0;
		if (rn++ < 40) {
			std::fprintf(stderr,
				"[own24-vr] va=%08x pc=%08x lr=%08x cpsr=%08x\n",
				virtAddr, getGPR(15), getGPR(14), getCPSR());
		}
	}
	} // end PSION_REC_WATCH read-watch block
	if (!(cp15_control & 1))
		return ARM710::readVirtual(virtAddr, valueSize);

	// ── Inlined hot path ────────────────────────────────────────────────
	// The overwhelming majority of reads hit the most-recently-used TLB entry
	// with a cached permission result and a resolved host pointer. Serve those
	// without the translateFast() + checkPermsfast() calls (callgrind: ~22% of
	// total runtime between them, almost all of it call/return + the cache-hit
	// logic those functions repeat). This replicates EXACTLY the same checks —
	// MRU match, perm-cache lookup for (privileged, read), host read pointer —
	// and falls through to the authoritative slow path on any miss, so it can
	// only ever short-circuit a request the slow path would have served
	// identically.
	//
	// PSION_NO_MEM_FASTPATH disables it (falls through to the authoritative
	// path); PSION_MEM_FASTPATH_CHECK runs both and logs any divergence — the
	// two diagnostics for the "correct-by-construction" claim.
	static const bool kNoMemFast    = (PSION_ENV_CSTR("PSION_NO_MEM_FASTPATH")    != nullptr);
	static const bool kMemFastCheck = (PSION_ENV_CSTR("PSION_MEM_FASTPATH_CHECK") != nullptr);
	if (!kNoMemFast)
	if (FastTlbEntry *e = dataMru_[dataMruIdx(virtAddr)];
	    e && e->addrMask && (virtAddr & e->addrMask) == e->addr && e->hostReadPtr) {
		const bool isPage = e->lv2Entry != 0;
		const int sub = isPage
			? (((e->lv2Entry & 3) == 1) ? ((virtAddr >> 14) & 3) : ((virtAddr >> 10) & 3))
			: 0;
		const uint8_t pc = isPage ? e->permCachePg[sub] : e->permCache;
		const int shift = isPrivileged() ? 0 : 2;            // read → +0
		if (pc & (uint8_t)(1u << (shift + 4))) {             // perm combo computed?
			std::pair<MaybeU32, MMUFault> fast;
			if (pc & (uint8_t)(1u << shift)) {               // …and permitted
				uint8_t *p = e->hostReadPtr + virtAddr;
				uint32_t v;
				switch (valueSize) {
				case V32: v = *reinterpret_cast<const uint32_t*>(p); break;
				case V16: v = *reinterpret_cast<const uint16_t*>(p); break;
				default:  v = *p; break;
				}
				fast = std::make_pair(v, NoFault);
			} else {
				fast = std::make_pair(MaybeU32(),
					encodeFaultSorP(SorPPermissionFault, isPage, (e->lv1Entry >> 5) & 0xF, virtAddr));
			}
			if (kMemFastCheck) {
				// Ground truth = base ARM710 MMU (full walk, no fast TLB, no
				// perm cache) so a SHARED perm-cache error is caught too.
				auto auth = ARM710::readVirtual(virtAddr, valueSize);
				checkFastDivergence("read", virtAddr, valueSize, fast, auth);
				return auth;            // ground truth wins while cross-checking
			}
			return fast;
		}
	}

	return readVirtualAuthoritative(virtAddr, valueSize);
}

// Authoritative read — the no-fast-path translateFast() + checkPermsfast() +
// host-pointer/readPhysical sequence.  Split out of readVirtual so the inlined
// fast path can cross-check itself against it (PSION_MEM_FASTPATH_CHECK) and so
// the kill-switch (PSION_NO_MEM_FASTPATH) has a single authoritative target.
std::pair<MaybeU32, ARM710::MMUFault>
SA1100Bridge::readVirtualAuthoritative(uint32_t virtAddr, ValueSize valueSize) {
	auto result = translateFast(virtAddr);
	if (auto *fault = std::get_if<MMUFault>(&result))
		return std::make_pair(MaybeU32(), *fault);
	auto *fe = std::get<FastTlbEntry *>(result);
	dataMru_[dataMruIdx(virtAddr)] = fe;   // hint for the next access to this page

	if (auto f = checkPermsfast(fe, virtAddr, false); f != NoFault)
		return std::make_pair(MaybeU32(), f);

	// Host-pointer fast path
	if (uint8_t *hp = fe->hostReadPtr) {
		uint8_t *p = hp + virtAddr;
		uint32_t v;
		switch (valueSize) {
		case V32: v = *reinterpret_cast<const uint32_t*>(p); break;
		case V16: v = *reinterpret_cast<const uint16_t*>(p); break;
		case V8:  v = *p; break;
		default:  v = 0; break;
		}
		return std::make_pair(v, NoFault);
	}

	// Slow path
	uint32_t physAddr = physAddrFromTlbEntry(
		reinterpret_cast<TlbEntry*>(fe), virtAddr); // safe: first 4 fields match TlbEntry layout
	auto val = readPhysical(physAddr, valueSize);
	if (!val.has_value())
		return std::make_pair(MaybeU32(),
			encodeFaultSorP(SorPOtherBusError,
				fe->lv2Entry != 0,
				(fe->lv1Entry >> 5) & 0xF, virtAddr));
	return std::make_pair(val, NoFault);
}

// Cross-check helper for PSION_MEM_FASTPATH_CHECK: log (capped) when the inlined
// fast path's answer differs from the authoritative one in fault, presence, or
// value.  A divergence here is the fast-path correctness bug we're hunting.
void SA1100Bridge::checkFastDivergence(const char *tag, uint32_t virtAddr,
		ValueSize valueSize,
		const std::pair<MaybeU32, MMUFault> &fast,
		const std::pair<MaybeU32, MMUFault> &auth) {
	const bool divFault = (fast.second != auth.second);
	const bool divHas   = (fast.first.has_value() != auth.first.has_value());
	const bool divVal   = (fast.first.has_value() && auth.first.has_value()
	                       && fast.first.value() != auth.first.value());
	if (!(divFault || divHas || divVal)) return;
	static int n = 0;
	if (n++ < 64) {
		log("[memfast-DIVERGE %s] va=%08x sz=%d priv=%d pc=%08x lr=%08x "
		    "fast{has=%d val=%08x fault=%d} auth{has=%d val=%08x fault=%d}",
		    tag, virtAddr, (int)valueSize, (int)isPrivileged(),
		    getGPR(15), getGPR(14),
		    (int)fast.first.has_value(), fast.first.value_or(0), (int)(fast.second != NoFault),
		    (int)auth.first.has_value(), auth.first.value_or(0), (int)(auth.second != NoFault));
	}
}


// ── Instruction fetch ───────────────────────────────────────────────
// Always a 32-bit read, but keyed on its OWN MRU entry (lastFetchEntry) so the
// code page stays cached across interleaved data accesses instead of
// ping-ponging lastFastTlbEntry. Same correct-by-construction fast path as
// readVirtual; on a miss it does a full readVirtual (which resolves + caches
// the page into lastFastTlbEntry) and adopts that entry as the fetch MRU.
std::pair<MaybeU32, ARM710::MMUFault>
SA1100Bridge::fetchVirtual(uint32_t virtAddr) {
	static const bool kNoMemFast    = (PSION_ENV_CSTR("PSION_NO_MEM_FASTPATH")    != nullptr);
	static const bool kMemFastCheck = (PSION_ENV_CSTR("PSION_MEM_FASTPATH_CHECK") != nullptr);
	if ((cp15_control & 1) && !kNoMemFast) {
		if (FastTlbEntry *e = lastFetchEntry;
		    e && e->addrMask && (virtAddr & e->addrMask) == e->addr && e->hostReadPtr) {
			const bool isPage = e->lv2Entry != 0;
			const int sub = isPage
				? (((e->lv2Entry & 3) == 1) ? ((virtAddr >> 14) & 3) : ((virtAddr >> 10) & 3))
				: 0;
			const uint8_t pc = isPage ? e->permCachePg[sub] : e->permCache;
			const int shift = isPrivileged() ? 0 : 2;        // fetch == read
			if (pc & (uint8_t)(1u << (shift + 4))) {
				std::pair<MaybeU32, MMUFault> fast;
				if (pc & (uint8_t)(1u << shift)) {
					uint32_t insn = *reinterpret_cast<const uint32_t*>(e->hostReadPtr + virtAddr);
					// The fetch returns the word and nothing else. Callers that
					// need a decode kind classify the word themselves at the
					// point of dispatch — never from dp->kind[], and never
					// carried through the prefetch pipeline. A cached kind
					// survives a code page being rewritten by anything that
					// bypasses writeVirtual, and an `immutable` page is never
					// dropped at all; a stale entry then dispatches the wrong
					// handler for the word actually fetched. See the matching
					// note in ARM710::tickPageLoop for the netBook Quartz case
					// that proved it. decodeKind() is cheap.
					fast = std::make_pair(insn, NoFault);
				} else {
					fast = std::make_pair(MaybeU32(),
						encodeFaultSorP(SorPPermissionFault, isPage, (e->lv1Entry >> 5) & 0xF, virtAddr));
				}
				if (kMemFastCheck) {
					auto auth = ARM710::readVirtual(virtAddr, V32);   // ground truth
					checkFastDivergence("fetch", virtAddr, V32, fast, auth);
					return auth;
				}
				return fast;
			}
		}
	}
	auto r = readVirtual(virtAddr, V32);   // resolves + sets lastFastTlbEntry
	lastFetchEntry = lastFastTlbEntry;     // remember the code page for next fetch
	return r;
}

// Resolve a code page for ARM710::tickPageLoop — the host pointer + decoded-kind
// array for the 1 KB subpage containing va. Mirrors fetchVirtual's MRU-hit
// resolution; returns false (→ the loop falls back to tick()) when the MMU is
// off, the cache is disabled, the page isn't resolvable, or the fetch perm for
// this subpage isn't cached-and-permitted. The 1 KB span keeps the page perm
// uniform (page AP is per-1 KB-subpage), so one perm check covers the whole loop.
bool SA1100Bridge::resolveFetchPage(uint32_t va, FetchPage &out) {
	if (!decodeCache_ || !(cp15_control & 1)) return false;
	FastTlbEntry *e = lastFetchEntry;
	if (!(e && e->addrMask && (va & e->addrMask) == e->addr && e->hostReadPtr)) {
		auto r = translateFast(va);
		if (std::get_if<MMUFault>(&r)) return false;
		e = std::get<FastTlbEntry *>(r);
		if (!e->hostReadPtr) return false;
		lastFetchEntry = e;
	}
	const bool isPage = e->lv2Entry != 0;
	const int sub = isPage
		? (((e->lv2Entry & 3) == 1) ? ((va >> 14) & 3) : ((va >> 10) & 3))
		: 0;
	const uint8_t pc = isPage ? e->permCachePg[sub] : e->permCache;
	const int shift = isPrivileged() ? 0 : 2;                 // fetch == read
	if (!((pc & (uint8_t)(1u << (shift + 4))) && (pc & (uint8_t)(1u << shift)))) {
		// The permission byte does not yet answer this combo — so ANSWER IT,
		// rather than giving up on the page.
		//
		// This used to `return false`, and it was the single most expensive
		// thing the fetch path did. Measured over a Series 7 boot: 16.0 M of
		// 42.3 M resolveFetchPage calls bailed out here, and essentially none
		// for any other reason (translation faults: 1; missing host pointer:
		// 0). The page was mapped, readable and present — the only thing
		// missing was a cache byte nobody on the fetch path ever filled in.
		// checkPermsfast() backfills it, but only the DATA paths call it, so a
		// code page that is executed and never read as data stayed permanently
		// unresolvable: the burst engine bailed to tick(), tick() fetched
		// through the slow path, and the next burst bailed again.
		//
		// Doing the authoritative walk here costs one checkAccessPermissions()
		// on first touch of a sub-page and makes every later fetch of it
		// resolvable. It cannot loosen anything: we proceed only if the walk
		// returns NoFault *and* the byte it wrote agrees. A combo that is
		// already computed and says "denied" is a real fetch abort — bail
		// straight out and let tick() raise it, without re-walking.
		if (pc & (uint8_t)(1u << (shift + 4)))
			return false;                                    // cached, and denied
		if (checkPermsfast(e, va, /*isWrite=*/false) != NoFault)
			return false;
		const uint8_t pc2 = isPage ? e->permCachePg[sub] : e->permCache;
		if (!((pc2 & (uint8_t)(1u << (shift + 4))) && (pc2 & (uint8_t)(1u << shift))))
			return false;                                    // byte not storable (see buildPermCacheByte)
	}
	uint32_t physAddr = physAddrFromTlbEntry(reinterpret_cast<TlbEntry *>(e), va);
	DecodedPage *dp = decodePageFor(physAddr);
	if (!dp) return false;
	out.hostBase = e->hostReadPtr;
	out.physBase = dp->physBase;
	out.validPtr = &dp->physBase;
	// Span this resolution stays valid over — see FetchPage in arm710.h.
	// A section (uniform across 1 MB) and a large page (AP selected by va>>14,
	// so uniform across 16 KB) are both capped instead by the decoded-page
	// validity token, which covers one 4 KB physical page.
	//
	// A small page selects its AP field with va>>10, so only the 1 KB subpage
	// is uniform.
	//
	// Widening this to the whole 4 KB page when all four AP fields agree was
	// tried and measured: 9.34 s -> 9.32 s on a Series 7 boot, i.e. nothing.
	// resolveFetchPage is ~6.4% of the WASM profile, but the re-resolves are
	// driven by BRANCHES leaving the page, not by straight-line code crossing a
	// subpage boundary — ARM code branches every few instructions, and a branch
	// to another page re-resolves however wide the span is. Widening only helps
	// the sequential case, which is the rare one. Left at 1 KB rather than
	// carrying a wider trust window for no gain.
	const bool smallPage = isPage && ((e->lv2Entry & 3) != 1);
	const uint32_t span  = smallPage ? 0x400u : 0x1000u;
	out.startVa  = va & ~(span - 1);
	out.endVa    = out.startVa + span;
	out.priv     = isPrivileged();
	return true;
}

// ── Optimised writeVirtual ──────────────────────────────────────────

ARM710::MMUFault
SA1100Bridge::writeVirtual(uint32_t value, uint32_t virtAddr, ValueSize valueSize) {
	// PSION_NB_SELFLINK: catch the exact store that makes a kernel-heap node
	// point at ITSELF (a self-linked list node — value == address).  This is
	// the corruption behind the netBook play freeze: a node in the global list
	// at 0x800007b8 gets ->next = itself, so the kernel's NULL-terminated
	// list-walk (0x50012bb4) loops forever.  Catching the writing PC/lr tells
	// us which kernel function (and what it was fed) created the self-link.
	{
		static const bool kSelfLink = PSION_ENV_CSTR("PSION_NB_SELFLINK") != nullptr;
		// The play freeze is a node in the delta-queue at 0x800007b8 whose
		// iNext points to ITSELF (value==addr) while it is STILL the list head.
		// That is exactly the double-insert: INSERT (0x50012b7c) does
		// str r2,[node] with r2==head==node when the node is re-inserted while
		// already at the head -> node->next=node -> the cancel walk loops
		// forever.  Self-linking alone is normal (a dequeued TDblQueLink marks
		// itself), so filter on "node == current head" to catch ONLY the
		// corruption.  fprintf(stderr) — log() is a no-op in the WASM build.
		if (kSelfLink && valueSize == V32 && value == virtAddr &&
		    virtAddr >= 0x80300000u && virtAddr < 0x80340000u) {
			uint32_t head = readVirtualDebug(0x800007b8u, V32).value_or(0);
			if (head == virtAddr) {
				static int n = 0;
				if (n++ < 40)
					std::fprintf(stderr,
					    "[selflink] node=%08x IS-HEAD self-linked  pc=%08x lr=%08x "
					    "r0=%08x r1=%08x r2=%08x r12=%08x cpsr=%x\n",
					    virtAddr, getRealPC(), getGPR(14),
					    getGPR(0), getGPR(1), getGPR(2), getGPR(12),
					    getCPSR() & 0x1F);
			}
		}
	}
	// PSION_NB_WATCH=<hex>[-<hex>]: deterministic memory-write watchpoint.
	// SA1100Bridge overrides writeVirtual with a host-pointer fast path that
	// bypasses ARM710::writeVirtual (where the legacy PSION_WRITE_WATCH lives),
	// so the netBook never hit those.  Catch every write in range HERE, before
	// any fast path, logging the writing PC/lr, old->new value, and cycle —
	// batch-independent (fires on the actual store).  Used to find the exact
	// epbus socket-state write at a CF chunk-boundary gap-end that releases the
	// blocked medata thread.
	{
		static bool inited = false;
		static uint32_t wStart = 0, wEnd = 0;
		if (!inited) {
			inited = true;
			if (const char *e = PSION_ENV_CSTR("PSION_NB_WATCH")) {
				char *endp;
				wStart = (uint32_t)std::strtoul(e, &endp, 0);
				wEnd = wStart + 4;
				if (*endp == '-') wEnd = (uint32_t)std::strtoul(endp + 1, nullptr, 0);
			}
		}
		if (wStart && virtAddr >= wStart && virtAddr < wEnd) {
			uint32_t oldv = 0;
			if (auto o = readVirtualDebug(virtAddr, valueSize); o.has_value())
				oldv = o.value();
			if (oldv != value) {
				log("[nbwatch] va=%08x sz=%d %08x->%08x pc=%08x lr=%08x cpsr=%x r0=%08x r1=%08x",
				    virtAddr, (int)valueSize, oldv, value,
				    getRealPC(), getGPR(14), getCPSR() & 0x1F,
				    getGPR(0), getGPR(1));
			}
		}
	}
	// Recording-timer investigation write-watches (opt-in only) — gated behind
	// PSION_REC_WATCH for the same reason as the read side: they ran on every
	// write and the console hits crippled the netBook UI render in WASM.
	static const bool kRecWatchW = PSION_ENV_CSTR("PSION_REC_WATCH") != nullptr;
	if (kRecWatchW) {
	// Trace EVERY write while PC is in the Enque/NTimerQ::Add-middle
	// path (0x50015810..0x5001581c for Enque, 0x500128b0..0x50012920
	// for the linked-list insert it tail-jumps into).  Reveals what
	// other memory gets touched during the suspect re-entry.
	//
	// Two-phase: phase 1 captures the first 30 NTimer Adds at boot
	// (sanity check that we see R5=ntimer/R6=handle).  Phase 2
	// captures specifically the Enque call from Esdrv (LR in
	// 0x50298000..0x50298400 range) which fires once during
	// recording start.  Per-phase caps so the recording event isn't
	// drowned out by the boot-time noise.
	{
		uint32_t pc = getGPR(15) - 12;
		bool inPath = (pc >= 0x50015810u && pc < 0x50015820u) ||
		              (pc >= 0x500128b0u && pc < 0x50012920u);
		if (inPath) {
			uint32_t lr = getGPR(14);
			bool fromEsdrv = (lr >= 0x50297000u && lr < 0x50299000u);
			static int bootN = 0;
			static int esdrvN = 0;
			bool log = false;
			if (fromEsdrv && esdrvN++ < 30) log = true;
			else if (!fromEsdrv && bootN++ < 20) log = true;
			if (log) {
				std::fprintf(stderr,
					"[enq-w%s] pc=%08x va=%08x val=%08x sz=%d "
					"r5=%08x r6=%08x r3=%08x r1=%08x lr=%08x\n",
					fromEsdrv ? "-rec" : "",
					pc, virtAddr, value,
					valueSize == V32 ? 32 : (valueSize == V16 ? 16 : 8),
					getGPR(5), getGPR(6), getGPR(3),
					getGPR(1), lr);
			}
		}
	}
	// writes to handler[0x13]'s queue-head pointer at virt
	// 0x80000228 — used to identify the kernel functions that Add
	// and Cancel the recording NTimer on NTimerQ.
	//
	// On netBook v1.05(450) this consistently shows:
	//   ADD    PC=50012918 LR=500128ac    (inside NTimerQ::Add @
	//                                       0x5001288c — the STR R5,
	//                                       [R1] at 0x5001290c)
	//   CANCEL PC=5001298c LR=50012978    (inside NTimerQ::Cancel @
	//                                       0x50012920 — the STREQ
	//                                       R3,[R8] at 0x50012980)
	if ((virtAddr & ~3u) == 0x80000228u) {
		static int n = 0;
		if (n++ < 60) {
			std::fprintf(stderr,
				"[h13-vw] va=%08x val=%08x sz=%d pc=%08x lr=%08x cpsr=%08x\n",
				virtAddr, value,
				valueSize == V32 ? 32 : (valueSize == V16 ? 16 : 8),
				getGPR(15), getGPR(14), getCPSR());
		}
	}
	// Watchpoint on the recording NTimer's "active" flag at
	// ntimer[+4] = recEsdrvChannel_ + 0xC.  In the v1.05(450) run
	// this lives at virt 0x8031f074.  Logged any time it changes;
	// captures both Add (writes 0), Cancel (writes 0), and the
	// elusive "set-active-1" event we want to identify.
	if ((virtAddr & ~3u) == 0x8031f074u) {
		static int n = 0;
		if (n++ < 200) {
			std::fprintf(stderr,
				"[ntim4-vw] va=%08x val=%08x sz=%d pc=%08x lr=%08x cpsr=%08x\n",
				virtAddr, value,
				valueSize == V32 ? 32 : (valueSize == V16 ? 16 : 8),
				getGPR(15), getGPR(14), getCPSR());
		}
	}
	// Watch ch[+0x44..+0x5C] (the buffer-pointer slots) for ALL
	// writes — these get cleared between SAC detection and recording
	// tick, and we need to identify who clears them.
	if (virtAddr >= 0x8031f0acu && virtAddr < 0x8031f0c8u &&
	    valueSize == V32) {
		static int n = 0;
		if (n++ < 60) {
			std::fprintf(stderr,
				"[chBuf-vw] va=%08x off=%02x val=%08x pc=%08x lr=%08x\n",
				virtAddr, (virtAddr - 0x8031f068u), value,
				getGPR(15), getGPR(14));
		}
	}
	// Watch ALL writes to ntimer 0x1B (0x80000eb4..0x80000ec4).
	// Reveals NTimer::Set / OneShot writing the deadline field.
	if (valueSize == V32 &&
	    virtAddr >= 0x80000eb4u && virtAddr < 0x80000ec4u) {
		static int n = 0;
		if (n++ < 60) {
			std::fprintf(stderr,
				"[ntim1b-vw] va=%08x off=%02x val=%08x pc=%08x lr=%08x\n",
				virtAddr, (virtAddr - 0x80000eb4u),
				value, getGPR(15), getGPR(14));
		}
	}
	// Watch writes to extObj+0x14 = 0x8031f768 — see who modifies
	// the value that gates the STREQ delivery write.
	if ((virtAddr & ~3u) == 0x8031f768u && valueSize == V32) {
		static int wn = 0;
		if (wn++ < 60) {
			std::fprintf(stderr,
				"[ext14-vw] va=%08x val=%08x pc=%08x lr=%08x\n",
				virtAddr, value, getGPR(15), getGPR(14));
		}
	}
	// Watch writes to extObj+0x24 = 0x8031f778 — see who modifies
	// the first-gate value (MOVNE if != 3 → return).
	if ((virtAddr & ~3u) == 0x8031f778u && valueSize == V32) {
		static int wn = 0;
		if (wn++ < 60) {
			std::fprintf(stderr,
				"[ext24-vw] va=%08x val=%08x pc=%08x lr=%08x\n",
				virtAddr, value, getGPR(15), getGPR(14));
		}
	}
	// Watch ALL writes to recording NTimer 0x13 (0x8031f070..0x8031f080).
	// For direct comparison against the working ntim1b.
	if (valueSize == V32 &&
	    virtAddr >= 0x8031f070u && virtAddr < 0x8031f080u) {
		static int n = 0;
		if (n++ < 60) {
			std::fprintf(stderr,
				"[ntim13-vw] va=%08x off=%02x val=%08x pc=%08x lr=%08x\n",
				virtAddr, (virtAddr - 0x8031f070u),
				value, getGPR(15), getGPR(14));
		}
	}
	// Watch the user-side TRequestStatus address itself (virt
	// 0x00407248).  The kernel's RequestComplete STREQ at 0x5000e08c
	// should write 0 here.  Goal: verify the write actually reaches
	// user memory (TTB == app's TTB) before chasing the semaphore
	// signal.
	if ((virtAddr & ~3u) == 0x00407248u) {
		static int rsn = 0;
		// Always log writes from PC inside completion fn (0x5000e08c
		// or the +12 reported convention 0x5000e094..0x5000e0a0).
		bool fromCompl = (getGPR(15) >= 0x5000e090u &&
		                  getGPR(15) < 0x5000e0a4u);
		if (fromCompl || rsn++ < 30) {
			std::fprintf(stderr,
				"[ureq-vw]%s va=%08x val=%08x sz=%d pc=%08x lr=%08x cpsr=%08x ttb=%08x\n",
				fromCompl ? "-CPL" : "",
				virtAddr, value,
				valueSize == V32 ? 32 : (valueSize == V16 ? 16 : 8),
				getGPR(15), getGPR(14), getCPSR(), getCp15Ttb());
		}
	}
	// Write watch on own+0x14 = 0x80320668 — extObj pointer.  Tracks
	// who writes this between channel setup and DFC call (and whether
	// it stays equal to 0x8031f754 during recording).
	if ((virtAddr & ~3u) == 0x80320668u && valueSize == V32) {
		static int wn = 0;
		if (wn++ < 40) {
			std::fprintf(stderr,
				"[own14-vw] va=%08x val=%08x pc=%08x lr=%08x cpsr=%08x\n",
				virtAddr, value, getGPR(15), getGPR(14), getCPSR());
		}
	}
	// Write watch on own+0x1c = 0x80320670 — this field holds the
	// TRequestStatus address but gets cleared between DFC calls.
	if ((virtAddr & ~3u) == 0x80320670u && valueSize == V32) {
		static int wn = 0;
		if (wn++ < 60) {
			std::fprintf(stderr,
				"[own1c-vw] va=%08x val=%08x pc=%08x lr=%08x\n",
				virtAddr, value, getGPR(15), getGPR(14));
		}
		if (value == 0x00407248u) {
			auto *sa = static_cast<SA1100::Emulator*>(
				static_cast<Arm710Bridge*>(this)->getOwner());
			if (sa->recAppTtb_ == 0) {
				sa->recAppTtb_ = getCp15Ttb();
				std::fprintf(stderr,
					"[own1c-ttb] captured ttb=%08x cpsr=%08x pc=%08x lr=%08x\n",
					sa->recAppTtb_, getCPSR(), getGPR(15), getGPR(14));
			}
		}
	}
	} // end PSION_REC_WATCH write-watch block
	if (!(cp15_control & 1))
		return ARM710::writeVirtual(value, virtAddr, valueSize);

	// Inlined hot path — the write counterpart of the readVirtual fast path
	// above: MRU TLB hit + cached (privileged, write) permission + resolved
	// host write pointer. Replicates exactly translateFast()'s last-hit check
	// and checkPermsfast()'s cache-hit logic, falling through to the
	// authoritative slow path on any miss.  PSION_NO_MEM_FASTPATH disables it;
	// PSION_MEM_FASTPATH_CHECK cross-checks it against writeVirtualAuthoritative.
	static const bool kNoMemFast    = (PSION_ENV_CSTR("PSION_NO_MEM_FASTPATH")    != nullptr);
	static const bool kMemFastCheck = (PSION_ENV_CSTR("PSION_MEM_FASTPATH_CHECK") != nullptr);
	if (!kNoMemFast)
	if (FastTlbEntry *e = dataMru_[dataMruIdx(virtAddr)];
	    e && e->addrMask && (virtAddr & e->addrMask) == e->addr && e->hostWritePtr) {
		const bool isPage = e->lv2Entry != 0;
		const int sub = isPage
			? (((e->lv2Entry & 3) == 1) ? ((virtAddr >> 14) & 3) : ((virtAddr >> 10) & 3))
			: 0;
		const uint8_t pc = isPage ? e->permCachePg[sub] : e->permCache;
		const int shift = (isPrivileged() ? 0 : 2) + 1;       // write → +1
		if (pc & (uint8_t)(1u << (shift + 4))) {              // perm combo computed?
			MMUFault fast;
			if (pc & (uint8_t)(1u << shift)) {               // …and permitted
				uint8_t *p = e->hostWritePtr + virtAddr;
				switch (valueSize) {
				case V32: *reinterpret_cast<uint32_t*>(p) = value; break;
				case V16: *reinterpret_cast<uint16_t*>(p) = (uint16_t)value; break;
				default:  *p = (uint8_t)value; break;
				}
				// Code-coherency: a guest store into a cached code page (self-
				// modifying code, or an app's code loaded into RAM — the PacMan.app
				// case) makes its decoded ops stale. This host-pointer fast path
				// BYPASSES writePhysical, so the invalidation must live here.
				// invalidateDecodePage is a no-op unless this page is cached, so a
				// normal data store pays one indexed compare.
				if (decodeCache_)
					invalidateDecodePage(physAddrFromTlbEntry(reinterpret_cast<TlbEntry*>(e), virtAddr));
				fast = NoFault;
			} else {
				fast = encodeFaultSorP(SorPPermissionFault, isPage, (e->lv1Entry >> 5) & 0xF, virtAddr);
			}
			if (kMemFastCheck) {
				MMUFault auth = ARM710::writeVirtual(value, virtAddr, valueSize);  // ground truth
				checkFastDivergence("write", virtAddr, valueSize,
				                    std::make_pair(MaybeU32(), fast),
				                    std::make_pair(MaybeU32(), auth));
				return auth;
			}
			return fast;
		}
	}

	return writeVirtualAuthoritative(value, virtAddr, valueSize);
}

// ── Block-transfer span resolution ──────────────────────────────────
// See ARM710::fastSpanPtr. Every test below is one of readVirtual()'s or
// writeVirtual()'s inlined fast-path tests, in the same order and reading the
// same fields, so a span this accepts is one those would have served word by
// word with the same host pointer. Anything else returns nullptr and the caller
// keeps its per-word path — including a span whose permission byte says NOT
// permitted, because raising the fault is the per-word path's job and it has to
// raise it on the right word.
uint8_t *SA1100Bridge::fastSpanPtr(uint32_t va, uint32_t bytes, bool write) {
	static const bool kNoMemFast    = (PSION_ENV_CSTR("PSION_NO_MEM_FASTPATH")    != nullptr);
	static const bool kMemFastCheck = (PSION_ENV_CSTR("PSION_MEM_FASTPATH_CHECK") != nullptr);
	// PSION_NO_BLOCK_SPAN stands this down on its own, leaving the per-word
	// fast path in place, so a behaviour change can be attributed to the span
	// resolution rather than to the whole memory fast path.
	static const bool kNoSpan = (PSION_ENV_CSTR("PSION_NO_BLOCK_SPAN") != nullptr);
	// The cross-check mode compares the per-word fast path against the base
	// ARM710 MMU; a span would skip both, so stand it down while it runs.
	if (kNoSpan || kNoMemFast || kMemFastCheck) return nullptr;
	if (!(cp15_control & 1)) return nullptr;          // MMU off: base ARM710 path
	if (bytes == 0) return nullptr;
	// Whole span inside one 1 KB region — the bound the perm cache and the
	// decoded-page drop are both uniform over.
	if (((va ^ (va + bytes - 1)) >> 10) != 0) return nullptr;

	FastTlbEntry *e = dataMru_[dataMruIdx(va)];
	if (!(e && e->addrMask && (va & e->addrMask) == e->addr)) return nullptr;
	uint8_t *host = write ? e->hostWritePtr : e->hostReadPtr;
	if (!host) return nullptr;
	const bool isPage = e->lv2Entry != 0;
	const int sub = isPage
		? (((e->lv2Entry & 3) == 1) ? ((va >> 14) & 3) : ((va >> 10) & 3))
		: 0;
	const uint8_t pc = isPage ? e->permCachePg[sub] : e->permCache;
	const int shift = (isPrivileged() ? 0 : 2) + (write ? 1 : 0);
	if (!(pc & (uint8_t)(1u << (shift + 4)))) return nullptr;   // combo not computed
	if (!(pc & (uint8_t)(1u << shift)))       return nullptr;   // …or not permitted
	// Code-coherency, exactly as the per-word write path does it: the span is
	// inside one 4 KB decoded page, so one drop covers every word in it, and
	// nothing re-decodes the page between here and the caller's stores.
	if (write && decodeCache_)
		invalidateDecodePage(physAddrFromTlbEntry(reinterpret_cast<TlbEntry *>(e), va));
	return host;
}

// Authoritative write — translateFast() + checkPermsfast() +
// host-pointer/writePhysical.  Split out of writeVirtual for the same reason as
// readVirtualAuthoritative (kill-switch target + cross-check reference).
ARM710::MMUFault
SA1100Bridge::writeVirtualAuthoritative(uint32_t value, uint32_t virtAddr, ValueSize valueSize) {
	auto result = translateFast(virtAddr);
	if (auto *fault = std::get_if<MMUFault>(&result))
		return *fault;
	auto *fe = std::get<FastTlbEntry *>(result);
	dataMru_[dataMruIdx(virtAddr)] = fe;   // hint for the next access to this page

	if (auto f = checkPermsfast(fe, virtAddr, true); f != NoFault)
		return f;

	// Host-pointer fast path
	if (uint8_t *hp = fe->hostWritePtr) {
		uint8_t *p = hp + virtAddr;
		switch (valueSize) {
		case V32: *reinterpret_cast<uint32_t*>(p) = value; break;
		case V16: *reinterpret_cast<uint16_t*>(p) = (uint16_t)value; break;
		case V8:  *p = (uint8_t)value; break;
		default:  break;
		}
		if (decodeCache_)   // code-coherency (also bypasses writePhysical) — see writeVirtual
			invalidateDecodePage(physAddrFromTlbEntry(reinterpret_cast<TlbEntry*>(fe), virtAddr));
		return NoFault;
	}

	// Slow path
	uint32_t physAddr = physAddrFromTlbEntry(
		reinterpret_cast<TlbEntry*>(fe), virtAddr); // safe: first 4 fields match TlbEntry layout
	if (!writePhysical(value, physAddr, valueSize))
		return encodeFaultSorP(SorPOtherBusError,
			fe->lv2Entry != 0,
			(fe->lv1Entry >> 5) & 0xF, virtAddr);
	return NoFault;
}
