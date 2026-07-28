// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include "windermere.h"

namespace Revo {

// Psion Revo (and Revo Plus) uses a CL-PS7111-family SoC with an EPOC R5 ROM.
// The CPU + peripheral emulation reuses Windermere almost verbatim — the Revo
// ROM programs the same MMIO block and boots cleanly through the native
// harness — but a handful of hardware parameters are genuinely different from
// the 5mx and need overriding:
//
//   - LCD: 480 x 160 (5mx is 640 x 240). Without this, readLCDIntoBuffer
//     would walk off the end of each framebuffer line.
//   - Digitiser: the touch area covers the ~480 x 160 screen plus a small
//     bezel. Use matching coordinates so the frontend's pointer mapping
//     lines up.
//   - LCD offset within the device skin differs; the frontend's SkinLayout
//     handles that, but we report offsets the core code can use.
//
// Keyboard layout differences are not handled here — the EPOC key map is
// close enough that existing bindings work; proper Revo-specific scancodes
// can be layered on later without changing this subclass.
class Emulator : public Windermere::Emulator {
public:
    const char *getDeviceName() const override { return "Psion Revo"; }

    // No settable machine ID, unlike the rest of the Windermere family.
    // The Revo ROM never consumes the ETNA identity PROM in this pairing:
    // the real machine is CL-PS7111-family and reads its identity chip over
    // pins we don't route to Etna, so the PROM sits unread. Verified by
    // filling the whole image with a positional pattern — Machine
    // information still reports Type "REVO" (the ROM's own fallback string,
    // not the PROM's name field) and Unique id 0000-0000-0000-0000. Letting
    // the debug panel write an ID here would change nothing the guest can
    // see, so the control stays hidden.
    bool hasMachineId() const override { return false; }

    // Revo LCD is 480 x 160, mono 4 bpp.
    int getLCDWidth()  const override { return 480; }
    int getLCDHeight() const override { return 160; }

    // Digitiser dimensions and LCD offset taken straight from the Revo
    // ROM itself: Machine Information → Screen reports
    //   Resolution        : 480 x 160     (LCD)
    //   Pointer resolution: 527 x 208     (digitiser)
    //
    // The LCD sits flush in the top-right of the digitiser. The extra
    // width to its LEFT (47 px) is a silkscreen tap column and the
    // extra height BELOW it (48 px) is the silkscreen shortcut bar.
    // The text labels under each shortcut icon (System / Contacts /
    // Agenda / Email / Phone / Time / Calc / Jotter / Extras) are
    // printed on the casing, not on the digitiser, so they don't
    // belong inside the 527 x 208 touch surface.
    int getDigitiserWidth()  const override { return 527; }
    int getDigitiserHeight() const override { return 208; }
    int getLCDOffsetX()      const override { return 47; }
    int getLCDOffsetY()      const override { return 0;  }

    // Windermere::readLCDIntoBuffer has the 640 x 240 size baked in; we
    // replace it so the framebuffer is walked at the correct per-line stride
    // and no out-of-bounds rows are sampled.
    void readLCDIntoBuffer(uint8_t **lines, bool is32BitOutput) const override;
};

}
