// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The board's own settings (Ui::Settings), kept in NVS: they belong to the
// board rather than the Siena, so they survive new firmware and a Siena
// started afresh.

#pragma once

#include "ui.h"

#include <string>

namespace settings_store {

Ui::Settings load();
void save(const Ui::Settings &s);

// The names of the apps installed from the store into a profile's Siena,
// newline separated.
std::string loadInstalled(int profile);
void saveInstalled(int profile, const std::string &names);

// The Wi-Fi network and Google account. NVS isn't encrypted: the
// passwords sit in the board's flash as typed.
Ui::Account loadAccount();
void saveAccount(const Ui::Account &a);

// The profile in use, to start on next time.
int  loadProfile();
void saveProfile(int profile);

} // namespace settings_store
