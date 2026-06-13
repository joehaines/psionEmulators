// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include "clps7111.h"

namespace Osaris {

// Oregon Scientific Osaris (1998) — ARM710 + CL-PS7111, 320 x 200 mono
// 4 bpp LCD, 440 x 200 digitiser, 8 MB ROM. The in-tree CLPS7111::Emulator
// is already an Osaris-shaped emulator (its LCD/digitiser dimensions in
// core/clps7111.cpp:635-641 match upstream WindEmu's Osaris config at
// WindEmu-master/WindCore/clps7111.cpp:468-474 verbatim — the historical
// "MC218" device-name string is vestigial). All this subclass does is
// override the device name so the frontend / harness display the right
// label; everything else is inherited.
class Emulator : public CLPS7111::Emulator {
public:
    const char *getDeviceName() const override { return "Oregon Scientific Osaris"; }
    // EPOC R5 on Osaris emits click feedback through the same
    // CL-PS7111 buzzer pin Series 5 uses. The earlier "loud high-
    // pitched tone mid-boot" regression came from the buzzer pump
    // running in BZMOD=1 mode regardless of BZTOG; that gate has
    // since been tightened to require BZTOG=1, so re-enabling the
    // pump here only audibly fires on the kernel's deliberate
    // BZTOG=1 click pulses.
    bool enableBuzzerPump() const override { return true; }
};

}
