// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include "clps7111.h"

namespace CLPS7110 {

// Cirrus Logic CL-PS7110 ("Clanton") — the SoC used by the Psion Series 5.
// Sibling part to the CL-PS7111 used by the MC218 / Osaris (modelled by
// CLPS7111::Emulator). Both chips share the same ARM710A core and the
// same lower-half register block (ports A/B/D/E, SYSCON1/SYSFLG1,
// MEMCFG1/2, DRFPR, INTSR/INTMR, LCDCON, TC1/TC2, RTC, palette regs,
// EOI registers, HALT — all at offsets 0x00 through 0x880); the PS7111
// adds a second register half (FRBADDR, SYSCON2/SYSFLG2, INTSR2/INTMR2,
// UARTDR2/UBRLCR2, KBDEOI) plus 2 KB of on-chip SRAM at CS6.
//
// Differences relevant to a software emulator (per reference/CL-PS7110.pdf
// Table 3-2 vs reference/CL-PS7111.PDF section 5):
//
//   * Register block ends at 0x880 — addresses 0x880-0xFFF are reserved
//     ("write has no effect; read is undefined") and the entire upper
//     half (0x1000-0x17C0) does not exist.
//   * LCD frame buffer is FIXED at physical 0xC0000000 (datasheet
//     section 1.2.10). There is no FRBADDR register.
//   * No on-chip SRAM at CS6 — CS6 is a generic external chip-select.
//   * SYSCON1 is 24-bit (bits 24-31 reserved); PS7111 widens it to 32.
//   * SYSFLG1 bit 29 (chip ID) reads '0' on PS7110, '1' on PS7111.
//   * SYSFLG1 boot-width is a single BOOT8BIT at bit 27; PS7111 uses
//     BOOTBIT0:1 across bits 27-28.
//   * Port C (data at 0x02, direction at 0x42) exists on PS7110 only.
//
// Implementation choice: CLPS7110::Emulator inherits from
// CLPS7111::Emulator and toggles the chip-variant virtuals
// (chipHasFRBADDR / chipHasSysCon2 / chipHasOnChipSRAM / sysConMask)
// declared on the base. The base's register handlers in clps7111.cpp
// then route to the correct PS7110 behaviour. The LCD render is
// overridden here to use the PS7110 fixed framebuffer at 0xC0000000
// and to derive bpp / line length from LCDCON (encoding identical on
// both chips).
//
// The Port C handling at offsets 0x02 / 0x42 currently lives in the
// shared base class; it's a no-op on PS7111 since the kernel never
// touches those offsets, so leaving it shared is harmless.
class Emulator : public CLPS7111::Emulator {
public:
    Emulator();

    // Chip-variant overrides — see reference/CL-PS7110.pdf Table 3-2.
    //
    // NOTE on chipHasOnChipSRAM: the PS7110 datasheet says CS6 is just a
    // generic external chip-select with nothing on-chip. However the
    // Series 5 ROM has 11+ literal-pool entries reading 0x60000000-
    // 0x600007FF as if they were initialised RAM, and the previous
    // emulator behaviour (commit 011610e) was to model a CLPS7111-style
    // 2 KB zeroed SRAM there. The Series 5 schematics likely show an
    // external chip wired to CS6 but the schematic walk hasn't been
    // done yet; until then we keep the zeroed-SRAM backing rather than
    // returning open-bus 0xFF, on the conservative principle of "match
    // pre-refactor behaviour for everything we can't yet verify".
    bool chipHasFRBADDR()    const override { return false; }
    bool chipHasSysCon2()    const override { return false; }
    bool chipHasOnChipSRAM() const override { return true; }   // see comment above
    uint32_t sysConMask()    const override { return 0x00FFFFFFu; }

    // First-IRQ delay: defer IRQs for the first 10M cycles (~0.5 sim sec)
    // so the EPOC R1 kernel can complete early init without preemption.
    // Discovered via the IRQ-timing sweep — unlocks +144 unique_pcs of
    // boot exploration at 30 sim s. PSION_FIRST_IRQ_DELAY env var
    // overrides this default.
    uint64_t defaultFirstIrqDelay() const override { return 10'000'000; }

    // PS7110-specific LCD render: framebuffer fixed at 0xC0000000, bpp /
    // line-length derived from LCDCON. Rendering of solid black when
    // LCDCON == 0 protects the user from seeing uninitialised RAM as
    // 4 bpp pixels during the boot window.
    void readLCDIntoBuffer(uint8_t **lines, bool is32BitOutput) const override;
};

}
