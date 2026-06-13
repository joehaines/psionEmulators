// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include "emubase.h"

// Windermere-specific ARM710 bridge with TLB performance extensions.
// Same architecture as SA1100Bridge: overrides readVirtual / writeVirtual
// with optimised paths (host-pointer cache, permCache, set-associative
// fast TLB, lastTlbEntry cache).  Covers 5mx, 5mxPro, MC218, Revo.

class WindermereBridge : public Arm710Bridge {
public:
	using Arm710Bridge::Arm710Bridge;

	struct FastTlbEntry {
		uint32_t addrMask, addr, lv1Entry, lv2Entry;
		uint8_t  permCache;
		uint8_t* hostReadPtr;
		uint8_t* hostWritePtr;
	};

	void flushTlb() override;
	void flushTlb(uint32_t virtAddr) override;

	std::pair<MaybeU32, MMUFault> readVirtual(uint32_t virtAddr, ValueSize valueSize) override;
	MMUFault writeVirtual(uint32_t value, uint32_t virtAddr, ValueSize valueSize) override;

	uint8_t* hostPtrForRead(uint32_t physAddr) const;
	uint8_t* hostPtrForWrite(uint32_t physAddr) const;

private:
	enum { FastTlbSets = 16, FastTlbWays = 4 };
	FastTlbEntry fastTlb[FastTlbSets][FastTlbWays] = {};
	int fastTlbNextWay[FastTlbSets] = {};
	FastTlbEntry *lastFastTlbEntry = nullptr;

	static int setForAddr(uint32_t addr) { return (addr >> 20) & (FastTlbSets - 1); }
	void flushFastTlb();
	void flushFastTlb(uint32_t virtAddr);
	void invalidatePermCache();
	FastTlbEntry *allocateFastTlbEntry(uint32_t addrMask, uint32_t addr);

	std::variant<FastTlbEntry *, MMUFault> translateFast(uint32_t virtAddr);
	MMUFault checkPermsFast(FastTlbEntry *entry, uint32_t virtAddr, bool isWrite);
};
