// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The Siena kept on the board's microSD card, standing in for the real
// machine's backup battery: its RAM, plus the rest of the running machine
// (CPU, ASIC9, timers; SienaMachine::saveState).
//
// Restoring both resumes the Siena exactly where it was. The machine state
// is only good for the firmware build that wrote it, so after a reflash
// just the RAM is restored and the Siena warm-boots, as from its reset
// button: the M: drive and its files survive, running apps don't.
//
// Two slot files (SIENA/RAM_A.BIN, RAM_B.BIN) alternate, each with a
// sequence number and CRC, so a write cut short by unplugging leaves the
// other intact. The card is never formatted; with no usable card the Siena
// simply cold-boots as before.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ram_store {

// Mount the card. `romId` identifies the ROM and language: a saved RAM is
// only restored into the machine that wrote it. `packId` identifies the
// app store pack in the Siena's slot (0 for none).
// Starts on `profile` (see below).
bool start(uint32_t romId, size_t ramSize, uint32_t packId, int profile);

// Fill `ram` from the newest good slot; `state` too if that slot was
// written by this firmware build, and `packWrites` (what the Siena wrote
// to the pack, SienaMachine::savePackWrites) if it was the same pack.
// Those not restorable are left empty. False if there is no slot.
bool load(uint8_t *ram, std::vector<uint8_t> &state, std::vector<uint8_t> &packWrites);

// Copy all three (consistent as of now) and write them out in the
// background. False, copying nothing, if the previous save is still being
// written.
bool save(const uint8_t *ram, const std::vector<uint8_t> &state,
          const std::vector<uint8_t> &packWrites);

// Whether there is a card to keep the Siena on.
bool available();
// Wait for a save in progress to reach the card.
void flush();

// Profiles: separate Sienas, each with its own slots (profile 1 in
// SIENA/, n in SIENA/P<n>/). setProfile picks whose slots load and save
// use; the caller restores that Siena (load) after.
int  profile();
// A file of the current profile's own, beside its slots ("" with no card).
std::string profileFile(const char *name);
bool setProfile(int profile);
std::vector<int> profiles();         // those on the card, 1 first
int  createProfile();                // a new, empty one; -1 if none left
bool deleteProfile(int profile);     // not 1, nor the current one

} // namespace ram_store
