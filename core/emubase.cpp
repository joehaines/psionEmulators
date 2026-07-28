// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "emubase.h"

#include <algorithm>
#include <cstring>

// Records every 32-bit little-endian occurrence of `value`, word-aligned.
// ARM literal pools are word-aligned, so the stride both halves the work and
// avoids matching a coincidental byte run straddling two unrelated words.
void EmuBase::findRomWords(const uint8_t *rom, size_t size, uint32_t value,
                           std::vector<size_t> &out) {
	out.clear();
	if (!rom || size < 4) return;
	const uint8_t want[4] = {
		(uint8_t)(value & 0xFF),         (uint8_t)((value >> 8) & 0xFF),
		(uint8_t)((value >> 16) & 0xFF), (uint8_t)((value >> 24) & 0xFF),
	};
	for (size_t off = 0; off + 4 <= size; off += 4)
		if (std::memcmp(rom + off, want, 4) == 0) out.push_back(off);
}

void EmuBase::findCardMachineIdWords(const uint8_t *card, size_t size,
                                     uint32_t factory, uint32_t current,
                                     std::vector<size_t> &out) {
	findRomWords(card, size, factory, out);
	if (current == factory) return;
	std::vector<size_t> patched;
	findRomWords(card, size, current, patched);
	out.insert(out.end(), patched.begin(), patched.end());
	std::sort(out.begin(), out.end());
	out.erase(std::unique(out.begin(), out.end()), out.end());
}

void EmuBase::writeRomWords(uint8_t *rom, const std::vector<size_t> &offsets,
                            uint32_t value) {
	if (!rom) return;
	for (size_t off : offsets) {
		rom[off]     = (uint8_t)(value & 0xFF);
		rom[off + 1] = (uint8_t)((value >> 8) & 0xFF);
		rom[off + 2] = (uint8_t)((value >> 16) & 0xFF);
		rom[off + 3] = (uint8_t)((value >> 24) & 0xFF);
	}
}
