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

}
