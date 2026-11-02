// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Bluetooth LE keyboard host (Bluedroid + esp_hid). The ESP32-S3 radio is
// LE-only, so the keyboard must support Bluetooth Low Energy.
//
// While no keyboard is connected it scans for one advertising the HID
// service as a keyboard (or any keyboard it has bonded with before) and
// connects. On first pairing the keyboard usually asks for a passkey: it is
// shown on screen, type it on the keyboard and press Enter. Bonds are kept
// in NVS, so a known keyboard reconnects on its own after a power cycle.

#pragma once

#include "esp_err.h"

#include <cstddef>
#include <cstdint>

namespace ble_keyboard {

struct Callbacks {
    // A keyboard input report (modifiers, reserved, six usages).
    void (*onReport)(const uint8_t *report, size_t len);
    // Passkey to show while pairing, or -1 when pairing is over.
    void (*onPasskey)(int passkey);
    void (*onConnection)(bool connected);
};

esp_err_t start(const Callbacks &callbacks);
// Forget every bonded keyboard (and drop the current one), so a different
// keyboard can be paired.
void forgetAll();

} // namespace ble_keyboard
