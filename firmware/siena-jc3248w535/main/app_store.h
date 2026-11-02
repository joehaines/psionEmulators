// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The app store's apps (tools/build_app_store.py puts them on a Flash SSD
// pack, which sits in the Siena's slot as drive A:) and installing one:
// the Siena's own System screen Install (Psion+I), typed for the user.
//
// Portable: no ESP-IDF dependency.

#pragma once

#include "ui.h"

#include <cstdint>
#include <string>
#include <vector>

class KeyPlayer;

namespace app_store {

struct Entry {
    Ui::App app;
    std::string_view program;   // its path on the pack, e.g. APP\CASTLE2.OPA
};

// The index after the pack in the store image ("SIENASTORE1 <pack CRC>"
// and one NAME \t PATH \t DESCRIPTION line per app). Empty if it isn't
// one. `packId` gets the pack's CRC, which identifies it. The entries are
// views of `text`, which must outlive them.
std::vector<Entry> parseIndex(const char *text, size_t size, uint32_t &packId);

// Queue the keys that install `program` (from drive A:) on the System
// screen, from wherever the Siena is. Each step waits for the Siena to
// finish the last; the longest is the Install dialog reading the pack.
void queueInstall(KeyPlayer &player, std::string_view program);

} // namespace app_store
