// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "windermere_cpu.h"
#include "windermere.h"
#include <cstring>

// ── Host-pointer resolution ─────────────────────────────────────────
// Returns a pointer biased so that `*(uintN_t*)(p + physAddr)` yields
// the same value as readPhysical(physAddr). nullptr = slow path.
//
// The Windermere readPhysical dispatches on `region = (physAddr >> 24) & 0xF1`.
// Hot regions for host-pointer acceleration:
//   0x00       : ROM       (16 MB, mask 0xFFFFFF)
//   0x10       : ROM2      (256 KB, mask 0x3FFFF)
//   0xC0, 0xC1 : MemoryBlockC0 (INCLUDE_D mode, mask 0x7FFFFF)
//   0xD0, 0xD1 : MemoryBlockD0 (INCLUDE_D mode, mask 0x7FFFFF)
//
// Peripheral regions (0x20 Etna, 0x40-0x60 CF, 0x80 SoC regs) return
// nullptr → slow readPhysical path.

uint8_t* WindermereBridge::hostPtrForRead(uint32_t physAddr) const {
	auto *w = static_cast<const Windermere::Emulator *>(getOwner());
	uint8_t region = (uint8_t)((physAddr >> 24) & 0xF1);
	if (region == 0x00)
		return const_cast<uint8_t*>(w->ROM) - (physAddr & 0xFF000000u);
	if (region == 0x10)
		return const_cast<uint8_t*>(w->ROM2) - (physAddr & 0xFFFC0000u);
	if (region == 0xC0 || region == 0xC1)
		return const_cast<uint8_t*>(w->MemoryBlockC0) - (physAddr & ~uint32_t(Windermere::Emulator::MemoryBlockMask));
	if (region == 0xD0 || region == 0xD1)
		return const_cast<uint8_t*>(w->MemoryBlockD0) - (physAddr & ~uint32_t(Windermere::Emulator::MemoryBlockMask));
	return nullptr;
}

uint8_t* WindermereBridge::hostPtrForWrite(uint32_t physAddr) const {
	auto *w = static_cast<const Windermere::Emulator *>(getOwner());
	uint8_t region = (uint8_t)((physAddr >> 24) & 0xF1);
	if (region == 0xC0 || region == 0xC1)
		return const_cast<uint8_t*>(w->MemoryBlockC0) - (physAddr & ~uint32_t(Windermere::Emulator::MemoryBlockMask));
	if (region == 0xD0 || region == 0xD1)
		return const_cast<uint8_t*>(w->MemoryBlockD0) - (physAddr & ~uint32_t(Windermere::Emulator::MemoryBlockMask));
	return nullptr;
}

// ── TLB flush overrides ──────────────────────────────────────────────

void WindermereBridge::flushTlb() {
	ARM710::flushTlb();
	flushFastTlb();
}

void WindermereBridge::flushTlb(uint32_t virtAddr) {
	ARM710::flushTlb(virtAddr);
	flushFastTlb(virtAddr);
}

// ── Fast TLB management ─────────────────────────────────────────────

void WindermereBridge::flushFastTlb() {
	for (int s = 0; s < FastTlbSets; s++)
		for (int w = 0; w < FastTlbWays; w++)
			fastTlb[s][w] = FastTlbEntry{};
	lastFastTlbEntry = nullptr;
	for (int s = 0; s < FastTlbSets; s++) fastTlbNextWay[s] = 0;
}

void WindermereBridge::flushFastTlb(uint32_t virtAddr) {
	int set = setForAddr(virtAddr);
	for (int w = 0; w < FastTlbWays; w++) {
		FastTlbEntry &e = fastTlb[set][w];
		if (e.addrMask && (virtAddr & e.addrMask) == e.addr) {
			e = FastTlbEntry{};
			break;
		}
	}
	lastFastTlbEntry = nullptr;
}

void WindermereBridge::invalidatePermCache() {
	for (int s = 0; s < FastTlbSets; s++)
		for (int w = 0; w < FastTlbWays; w++)
			fastTlb[s][w].permCache = 0;
}

WindermereBridge::FastTlbEntry *WindermereBridge::allocateFastTlbEntry(uint32_t addrMask, uint32_t addr) {
	int set = setForAddr(addr & addrMask);
	FastTlbEntry *entry = &fastTlb[set][fastTlbNextWay[set]];
	fastTlbNextWay[set] = (fastTlbNextWay[set] + 1) % FastTlbWays;
	entry->addrMask = addrMask;
	entry->addr = addr & addrMask;
	entry->permCache = 0;
	entry->hostReadPtr = nullptr;
	entry->hostWritePtr = nullptr;
	return entry;
}

// ── Fast TLB translate ──────────────────────────────────────────────

std::variant<WindermereBridge::FastTlbEntry *, ARM710::MMUFault>
WindermereBridge::translateFast(uint32_t virtAddr) {
	if (lastFastTlbEntry && lastFastTlbEntry->addrMask &&
	    (virtAddr & lastFastTlbEntry->addrMask) == lastFastTlbEntry->addr)
		return lastFastTlbEntry;

	int set = setForAddr(virtAddr);
	for (int w = 0; w < FastTlbWays; w++) {
		FastTlbEntry &e = fastTlb[set][w];
		if (e.addrMask && (virtAddr & e.addrMask) == e.addr) {
			lastFastTlbEntry = &e;
			return &e;
		}
	}

	auto result = translateAddressUsingTlb(virtAddr);
	if (auto *fault = std::get_if<MMUFault>(&result))
		return *fault;
	auto *base = std::get<TlbEntry *>(result);

	FastTlbEntry *fe = allocateFastTlbEntry(base->addrMask, base->addr);
	fe->lv1Entry = base->lv1Entry;
	fe->lv2Entry = base->lv2Entry;

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

ARM710::MMUFault WindermereBridge::checkPermsFast(FastTlbEntry *entry, uint32_t virtAddr, bool isWrite) {
	if (entry->permCache && entry->lv2Entry == 0) {
		const bool priv = isPrivileged();
		const int shift = (priv ? 0 : 2) + (isWrite ? 1 : 0);
		const uint8_t computedBit = (uint8_t)(1u << (shift + 4));
		const uint8_t okBit       = (uint8_t)(1u << shift);
		if (entry->permCache & computedBit) {
			return (entry->permCache & okBit) ? NoFault
			    : encodeFaultSorP(SorPPermissionFault, false,
			                      (entry->lv1Entry >> 5) & 0xF, virtAddr);
		}
	}

	TlbEntry tmp;
	tmp.addrMask = entry->addrMask;
	tmp.addr     = entry->addr;
	tmp.lv1Entry = entry->lv1Entry;
	tmp.lv2Entry = entry->lv2Entry;
	auto f = checkAccessPermissions(&tmp, virtAddr, isWrite);

	if (f == NoFault && entry->lv2Entry == 0) {
		int domain = (entry->lv1Entry >> 5) & 0xF;
		int accessPerms = (entry->lv1Entry >> 10) & 3;
		int primaryAccessControls = (cp15_domainAccessControl >> (domain * 2)) & 3;
		if (primaryAccessControls == 3) {
			entry->permCache = 0xFF;
		} else if (primaryAccessControls == 1) {
			bool System = cp15_control & 0x100;
			bool ROM    = cp15_control & 0x200;
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
			uint8_t cache = 0xF0;
			if (wouldPass(true,  false)) cache |= 0x01;
			if (wouldPass(true,  true))  cache |= 0x02;
			if (wouldPass(false, false)) cache |= 0x04;
			if (wouldPass(false, true))  cache |= 0x08;
			entry->permCache = cache;
		}
	}

	return f;
}

// ── Optimised readVirtual ───────────────────────────────────────────

std::pair<MaybeU32, ARM710::MMUFault>
WindermereBridge::readVirtual(uint32_t virtAddr, ValueSize valueSize) {
	if (!(cp15_control & 1))
		return ARM710::readVirtual(virtAddr, valueSize);

	auto result = translateFast(virtAddr);
	if (auto *fault = std::get_if<MMUFault>(&result))
		return std::make_pair(MaybeU32(), *fault);
	auto *fe = std::get<FastTlbEntry *>(result);

	if (auto f = checkPermsFast(fe, virtAddr, false); f != NoFault)
		return std::make_pair(MaybeU32(), f);

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

	uint32_t physAddr = physAddrFromTlbEntry(
		reinterpret_cast<TlbEntry*>(fe), virtAddr);
	auto val = readPhysical(physAddr, valueSize);
	if (!val.has_value())
		return std::make_pair(MaybeU32(),
			encodeFaultSorP(SorPOtherBusError,
				fe->lv2Entry != 0,
				(fe->lv1Entry >> 5) & 0xF, virtAddr));
	return std::make_pair(val, NoFault);
}

// ── Optimised writeVirtual ──────────────────────────────────────────

ARM710::MMUFault
WindermereBridge::writeVirtual(uint32_t value, uint32_t virtAddr, ValueSize valueSize) {
	if (!(cp15_control & 1))
		return ARM710::writeVirtual(value, virtAddr, valueSize);

	auto result = translateFast(virtAddr);
	if (auto *fault = std::get_if<MMUFault>(&result))
		return *fault;
	auto *fe = std::get<FastTlbEntry *>(result);

	if (auto f = checkPermsFast(fe, virtAddr, true); f != NoFault)
		return f;

	if (uint8_t *hp = fe->hostWritePtr) {
		uint8_t *p = hp + virtAddr;
		switch (valueSize) {
		case V32: *reinterpret_cast<uint32_t*>(p) = value; break;
		case V16: *reinterpret_cast<uint16_t*>(p) = (uint16_t)value; break;
		case V8:  *p = (uint8_t)value; break;
		default:  break;
		}
		return NoFault;
	}

	uint32_t physAddr = physAddrFromTlbEntry(
		reinterpret_cast<TlbEntry*>(fe), virtAddr);
	if (!writePhysical(value, physAddr, valueSize))
		return encodeFaultSorP(SorPOtherBusError,
			fe->lv2Entry != 0,
			(fe->lv1Entry >> 5) & 0xF, virtAddr);
	return NoFault;
}
