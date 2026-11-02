// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The board's Wi-Fi, for syncing with Google: a station joining the network
// set in the board's settings. Bluetooth (the keyboard) shares the radio.

#pragma once

#include "esp_err.h"

#include <string>
#include <vector>

namespace wifi_link {

esp_err_t start();
// The networks in range, strongest first (a blocking scan, a few s).
std::vector<std::string> scan();
// Join (or rejoin) a network; connected() turns true once it has an
// address.
void join(const std::string &ssid, const std::string &password);
bool connected();

} // namespace wifi_link
