// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include "sa1100.h"

// Psion Series 7 and netBook — sibling devices that share the same
// StrongARM SA-1100 SoC, 32 MB SDRAM, 16 MB flash ROM and 640×480 TFT
// panel. The only hardware difference relevant to emulation is the
// device name surfaced in the UI (and an optional USB host port on the
// netBook which the boot ROM doesn't touch). We encode that by passing
// the display name through the constructor.
//
// See core/sa1100.{h,cpp} for the shared SoC emulation — this file is
// just a thin factory wrapper.
namespace Series7 {

class Emulator : public SA1100::Emulator {
public:
    explicit Emulator(const char *name = "Psion Series 7") : name_(name) {}

protected:
    const char *deviceName() const override { return name_; }

private:
    const char *name_;
};

// netBook uses the same class; kept separate so future divergence
// (e.g. real USB host) can be added without a big-bang refactor.
class NetBookEmulator : public Emulator {
public:
    NetBookEmulator() : Emulator("Psion netBook") {}
};

// The Psion netpad — a netBook-class SA-1100 machine that boots its own
// EPOC R5 ROM (roms/Netpad.img) directly from PA 0 like the Series 7,
// but drives a colour panel at a Series-5-like resolution rather than the
// netBook/Series 7 640×480. Only the panel geometry differs from the base
// SA-1100 SoC, so we override lcdWidth()/lcdHeight(); everything else
// (CPU, SDRAM, ROM mapping, Eiger, colour LCD render path) is inherited
// unchanged. The dimensions below match what the netpad ROM programs into
// the SA-1100 LCD controller (LCCR1 PPL / LCCR2 LPP).
class NetpadEmulator : public Emulator {
public:
    NetpadEmulator() : Emulator("Psion netpad") {}

    // The netpad's digitiser covers the panel and nothing else — no
    // silkscreen columns either side, unlike the Series 7 / netBook — so
    // pen coordinates arrive in LCD pixels and the ADS7846-class board
    // codec (see netpadAdsConvert) scales them straight onto the panel.
    int getDigitiserWidth()  const override { return 640; }
    int getDigitiserHeight() const override { return 240; }

protected:
    int lcdWidth()  const override { return 640; }
    int lcdHeight() const override { return 240; }
    // The netpad is built on the Intel StrongARM SA-1110; its boot ROM
    // scans the CP15 Main ID and hangs unless (id & 0xfffffff0) matches
    // the SA-1110 signature 0x6901b110.
    uint32_t processorId() const override { return 0x6901B110; }
    // The netpad's second-stage init re-runs the reset stub from the
    // ROM's linked address (VA 0x500c0100) and disables the MMU partway
    // through, so the boot flash must stay visible at physical 0x50xxxxxx.
    bool flashAliasedAt0x50() const override { return true; }
    // 64 MB SDRAM (4 × 16 MB banks at 0xC0/0xC8/0xD0/0xD8) and a 32 MB
    // flash window, per the netpad hardware spec.
    int    ramBankCount()   const override { return 4; }
    size_t romWindowBytes() const override { return 0x2000000; }
    // The netpad image prefixes a 0xC0000 boot partition (reset stub at 0
    // → OS ROM at 0xC0000, linked at 0x50000000). Boot from the OS ROM so
    // ROMBASE 0x50000000 maps to real code.
    size_t romLoadOffset()  const override { return 0xC0000; }
    bool   isNetpad()       const override { return true; }
    // Spec-correct OS-timer rate.  The netpad kernel reaches its desktop
    // at 1×, unlike the Series 7 / netBook, and every guest-measured
    // duration depends on it — key auto-repeat, the board codec's SSP
    // settling waits and the clock.  See osTimerScale() in sa1100.h.
    int64_t osTimerScale()  const override { return 1; }
};

}
