// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include "device_profile.h"

// Parses ROM variant ID from the EPOC ROM header.
// Returns 0 if the ROM is too small or header is invalid.
uint32_t detectROMVariant(const uint8_t *romData, size_t romSize);

// Returns the first profile matching variantId, or nullptr if none.
const DeviceProfile *findProfileByVariant(uint32_t variantId);

// Returns the profile with the given id string, or nullptr if none.
const DeviceProfile *findProfileById(const char *id);

// Returns a pointer to the first element of the profiles array.
// The array is terminated by an entry with id == nullptr.
const DeviceProfile *allProfiles();
