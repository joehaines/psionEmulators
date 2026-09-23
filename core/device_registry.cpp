// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "device_registry.h"
#include "emubase.h"
#include "windermere.h"
#include "series5.h"
#include "revo.h"
#include "conan.h"
#include "clps7111.h"
#include "osaris.h"
#include "geofox.h"
#include "series3.h"
#include "series3c.h"
#include "series7.h"
#include "organiser1.h"
#include "organiser2.h"
#include <cstring>

// Per-device Windermere factories. Each one stamps the variant on the
// emulator so the shared Windermere audio + ROM-detection code can
// branch explicitly on device identity rather than inferring it from
// runtime flags. Adding a new variant: pick a Variant enum, add a
// factory here, point the device profile at it.
// The PROM device name each Windermere machine reports: EPOC reads the
// ETNA identity PROM at boot and shows this string as "Type" in the
// System screen's Machine information dialog. Etna's own default is
// WindEmu's "PockEmul" joke name, which would now be user-visible (the
// PROM only started being accepted once its serial protocol was fixed —
// see kPromAddressBits in core/etna.h), so every device sets its real
// one here. The strings match what EPOC displayed from its built-in
// fallbacks while the PROM was being rejected.
static EmuBase *makeWindermere5mx() {
    auto *emu = new Windermere::Emulator;
    emu->setVariant(Windermere::Variant::Mx5);
    emu->setPromDeviceName("SERIES5 MX");
    return emu;
}
static EmuBase *makeWindermere5mxPro() {
    auto *emu = new Windermere::Emulator;
    emu->setVariant(Windermere::Variant::Mx5Pro);
    emu->setPromDeviceName("SERIES5 MX");
    return emu;
}
static EmuBase *makeWindermereMC218() {
    auto *emu = new Windermere::Emulator;
    emu->setVariant(Windermere::Variant::Mc218);
    emu->setPromDeviceName("Ericsson MC 218");
    return emu;
}
static EmuBase *makeCLPS7111()   { return new CLPS7111::Emulator; }
// Oregon Scientific Osaris (1998) — ARM710 + CL-PS7111. The in-tree
// CLPS7111::Emulator already reports the Osaris-correct LCD/digitiser
// dimensions (320x200 LCD inside a 440x200 digitiser at offset 60,0,
// matching upstream WindEmu's Osaris config); Osaris::Emulator just
// overrides the device-name string.
static EmuBase *makeOsaris()     { return new Osaris::Emulator; }
// Geofox One (1997) — the EPOC R1 clamshell from Psion licensee Geofox
// Ltd. ARM710 on a CL-PS7110-class SoC (the ROM never programs FRBADDR,
// so the frame buffer is the 7110's fixed 0xC0000000) with a 640x320
// 2 bpp panel — taller than any Psion of the era. See core/geofox.h.
static EmuBase *makeGeofox()     { return new Geofox::Emulator; }
static EmuBase *makeSeries5()    { return new Series5::Emulator; }
// Psion Revo uses a CL-PS7111-family SoC but the CLPS7111::Emulator in this
// tree does not boot any real ROM (see the mc218 profile comment). The Revo
// EPOC R5 ROM runs cleanly against Windermere in the native harness, and the
// Revo::Emulator subclass overrides the LCD/digitiser dimensions so the
// native 480x160 screen is walked correctly instead of 5mx's 640x240.
static EmuBase *makeRevo() {
    auto *emu = new Revo::Emulator;
    emu->setVariant(Windermere::Variant::Revo);
    // No setPromDeviceName: the Revo ROM never reads the ETNA PROM (see
    // Revo::Emulator::hasMachineId). "REVO" in Machine information is the
    // ROM's own string.
    return emu;
}
// Psion "Conan" — the Revo's successor, running a 2001 Psion Digital build
// on the Revo's own board. Conan::Emulator is Revo::Emulator with a
// different reported device name; the Conan variant keeps the two builds
// separable for any future ROM-specific quirk without disturbing the Revo.
// Like the Revo it never reads the ETNA PROM, so no setPromDeviceName.
// Both Conan profiles below share this factory — they differ only in which
// ROM image they load.
static EmuBase *makeConan() {
    auto *emu = new Conan::Emulator;
    emu->setVariant(Windermere::Variant::Conan);
    return emu;
}
// Psion Series 3 (V30 + ASIC1 + ASIC2, 1991 handheld).
static EmuBase *makeSeries3()    { return new Series3::Emulator; }
// Acorn Pocket Book (original, 1992) — rebadged Psion Series 3 sold by
// Acorn to UK schools. Same V30 + ASIC1/ASIC2 hardware as the Series 3,
// 512 KiB ROM, 256 KiB RAM. The Pocket Book II (1995, separate
// pocketbk2 profile) is rebadged Series 3a with 2 MiB ROM. Both are
// Acorn educational variants but use different generation hardware.
static EmuBase *makeAcornPocketBook() { return new Series3::Emulator; }

// Psion MC400 (1989 clamshell laptop, V30 + ASIC1 + ASIC2 — same SIBO1
// chip set as the Series 3, with a 640x400 mono LCD in laptop mode and
// a different memory map (256 KiB RAM + 32 KiB VRAM at 0xB8000 + 256 KiB
// ROM at 0xC0000)). The Config struct toggles ASIC1 into laptopMode +
// LCD-id 0 so the framebuffer is fetched from the alternate VRAM base.
static EmuBase *makeMC400() {
    Series3::Config c;
    c.model       = Series3::Model::MC400;
    c.displayName = "Psion MC400";
    c.cpuVariant  = V30Variant::I8086;
    // CPU bus clock — matches MAME's `I8086(... 15.36_MHz_XTAL / 2)`
    // wiring in mc400.cpp. The V30 core's I8086 variant scales its
    // per-instruction cycle counts by 2x (V30 was ~2x faster per cycle
    // than the i8086 it replaced) so this 7.68 MHz figure produces the
    // same instructions-per-wall-second as MAME, with timer / watchdog
    // hz_t periods correctly derived as bus_clock / 4.
    c.busClockHz  = 7'680'000;
    // 640x400 mono panel — no upscale; the framebuffer is the panel.
    c.lcdWidth    = 640;
    c.lcdHeight   = 400;
    c.fbWidth     = 640;
    c.fbHeight    = 400;
    c.fbPlates    = 2;       // dual-plate: lower 200 rows from VRAM+0x4000
    c.ramSize     = 0x40000; // 256 KiB main RAM
    // MAME's mc400.cpp asic1_map declares 0x00000-0x3FFFF as RAM and
    // 0x40000-0xB7FFF as noprw() (unmapped). The earlier V30-mode
    // setup needed `ramDecodeMask = 0x7FFFF` because pre-i8086-mode
    // cycle drift made the kernel emit spurious far-jumps into the
    // upper half. With V30Variant::I8086 in place those jumps don't
    // happen, and the mirror actively *breaks* boot — the boot ROM's
    // RAM POST walks segments 0x0000-0xC000 testing 64 KiB at a time
    // and decides RAM ends where SCASW first fails; with the mirror
    // POST mistakenly reports 0xC000 (768 KiB) as the top, the kernel
    // sets `SS = 0xBFC0` and pushes return addresses into ROM. Leave
    // ramDecodeMask = 0 so POST correctly fails at segment 0x4000
    // and `[0x414]` ends up as the actual top-of-RAM segment.
    c.ramDecodeMask = 0;
    c.romSize     = 0x40000; // 256 KiB ROM (no mirror)
    c.romBase     = 0xC0000; // reset vector EA 00 00 00 C0 → C000:0000
    c.mirrorRom   = false;
    c.vramBase    = 0xB8000;
    c.vramSize    = 0x8000;  // 32 KiB framebuffer window
    c.laptopMode  = true;
    c.lcdId       = 0;       // 640x400 in laptop mode
    c.keyMatrix   = &Series3::mc400KeyMatrix;
    c.ssdSlots    = 4;       // Pack 1..4 wired on ASIC2 ch1..4 (MAME mc400.cpp).
                             // Pack D (slot index 3) holds the ROM:: System
                             // Disk, pre-inserted by the frontend on cold boot
                             // (defaultSsdFor) from roms/MC400_V2.60F_system.ssd.
    // Cold-boot RAM as 0xFF — the V1.26F boot ROM's RAM-POST is gated
    // on a non-zero "post-done" marker at [0x416]+[0x418]; zeroed RAM
    // makes the kernel skip POST and leave [0x414] (top-of-RAM segment)
    // uninitialised, which then puts SS at 0xFFC0 (a stack inside ROM)
    // and corrupts the first CALL/RET pair.
    c.ramFillByte = 0xFF;
    return new Series3::Emulator(c);
}
// Psion MC200 (1989) — the MC400's smaller sibling. MAME builds it from
// the same `psionmc` machine (I8086 at 15.36 MHz / 2, ASIC1 in laptop
// mode, ASIC2, the ASIC5 PSU and four SSD slots), changing exactly three
// things in `psionmc_state::mc200`: a 640x200 screen instead of 640x400,
// `screen_update_single` instead of `screen_update_dual`, and 128 KiB of
// RAM instead of 256 KiB. This factory is makeMC400 with those three
// changes plus the LCD identity that follows from the panel — ASIC1
// reports LCD type 1 for a 200-row laptop panel and 0 for a 400-row one
// (MAME psion_asic1.cpp::lcd_type), and the ROM reads that field out of
// A1Status to decide how to drive the display.
//
// Both machines shipped the same V2.12F boot ROM, so the comments on
// makeMC400 above — why V30Variant::I8086, why ramDecodeMask stays 0, why
// cold-boot RAM is filled with 0xFF — apply here unchanged.
static EmuBase *makeMC200() {
    Series3::Config c;
    c.model       = Series3::Model::MC200;
    c.displayName = "Psion MC200";
    c.cpuVariant  = V30Variant::I8086;
    c.busClockHz  = 7'680'000;
    // 640x200 mono panel, single-plate: the whole framebuffer is one
    // 16 KiB VRAM map, where the MC400 walks a second map at VRAM+0x4000
    // for its lower 200 rows.
    c.lcdWidth    = 640;
    c.lcdHeight   = 200;
    c.fbWidth     = 640;
    c.fbHeight    = 200;
    c.fbPlates    = 1;
    c.ramSize     = 0x20000; // 128 KiB main RAM
    c.ramDecodeMask = 0;
    c.romSize     = 0x40000; // 256 KiB ROM (2 x 28F010, joined by
                             // scripts/build-mc200-rom.mts)
    c.romBase     = 0xC0000; // reset vector EA 00 00 00 C0 → C000:0000
    c.mirrorRom   = false;
    c.vramBase    = 0xB8000;
    c.vramSize    = 0x8000;
    c.laptopMode  = true;
    c.lcdId       = 1;       // 640x200 in laptop mode
    c.keyMatrix   = &Series3::mc400KeyMatrix;  // same psionmc keyboard
    c.ssdSlots    = 4;       // Pack 1..4 on ASIC2 ch1..4; Pack D (slot 3)
                             // holds the ROM:: System Disk, pre-inserted
                             // by the frontend from
                             // roms/MC200_V2.12F_system.ssd
    c.ramFillByte = 0xFF;
    return new Series3::Emulator(c);
}
// Psion HC120 (1991) — the industrial handheld of the SIBO1 generation:
// V30H + ASIC1 + ASIC2 like the Series 3, but a 160x80 panel, 512 KiB of
// RAM and a 256 KiB ROM sitting directly above it. First emulated in MAME
// by Nigel Barnes (src/mame/psion/psionhc.cpp), whose driver this config
// follows; see docs/hc120-rom.md.
static EmuBase *makeHC120() {
    Series3::Config c;
    c.model       = Series3::Model::HC120;
    c.displayName = "Psion HC120";
    c.cpuVariant  = V30Variant::V30H;
    c.lcdWidth    = 320;     // 160x80 panel, 2x upscale for the UI
    c.lcdHeight   = 160;
    c.fbWidth     = 160;
    c.fbHeight    = 80;
    c.fbPlates    = 1;
    c.ramSize     = 0x80000; // 512 KiB (HC120; HC110 has 256, HC100 128)
    // 256 KiB of flash at 0xA0000-0xDFFFF, with the top 128 KiB window
    // reading the upper chip again — the reset vector at 0xFFFF0 lives
    // in that alias and jumps to A000:0000, the boot block at the bottom
    // of the lower chip. See docs/hc120-rom.md.
    c.romSize     = 0x40000;
    c.romBase     = 0xA0000;
    c.mirrorRom   = false;
    c.romAliasSize = 0x20000;
    c.laptopMode  = false;
    c.lcdId       = 0;
    c.ssdSlots    = 2;
    c.keyMatrix   = &Series3::hc120KeyMatrix;
    // ESC is a key on the keypad here, not the ON button — the HC has a
    // separate ON/OFF button on the case above the screen.
    c.escIsOnKey  = false;
    return new Series3::Emulator(c);
}

// Psion Series 3c (V30H + ASIC9). SIBO2 bring-up scaffold; the CPU core
// is still a stub and PsionAsic9 hasn't been ported yet, so the factory
// exists to let the harness boot Series 3a/3c/3mx ROMs far enough to
// print the first unimplemented V30H opcode.
static EmuBase *makeSeries3c() {
    Series3c::Config c;
    c.model       = Series3c::Model::Series3c;
    c.displayName = "Psion Series 3c";
    return new Series3c::Emulator(c);
}
static EmuBase *makeSeries3a() {
    Series3c::Config c;
    c.model       = Series3c::Model::Series3a;
    c.displayName = "Psion Series 3a";
    // Keep the default ASIC9 boot status (0xE020 = MainsPresent|Reset|
    // PowerFail|Cold). We previously tried clearing MainsPresent here to
    // mirror MAME's psion_asic9_device::device_reset, on the theory that
    // it would stop the v3.40f shell from taking the session-restore
    // path on cold boot. Empirically that's the opposite of what's
    // helpful once the writeMemByte mirror-aliased-write fix is in: with
    // MainsPresent ON and the mirror writes letting the kernel's
    // RAM-size probe succeed at its second cascade level (psel=0x04),
    // the kernel populates the AGN/SPR/DAT/WRD/WLD subdirs on cold boot
    // and the System screen comes up clean. With MainsPresent OFF the
    // shell still ends up at the "Media is corrupt" dialog because the
    // session-restore branch isn't the actual trigger — see the
    // FINDINGS write-up under tools/mame-vs-ours/.
    // RAM: 2 MiB — matching MAME's psion3a2 variant (the high-RAM 3a).
    // The base psion3a uses 512K (MAME psion3a.cpp:539), but 512K triggers
    // address-aliasing differences in the direct memory path (linear
    // 0x10000-0x5FFFF) that our emulator doesn't apply ASIC9 configure_ram
    // mapping to. 2MiB works correctly: type=2 addrmirror=0xC00000 has no
    // effect on the 0x0-0x5FFFF direct range.
    c.ramSize            = 0x200000;  // 2 MiB
    c.useStrictRamMirror = true;
    // autoWakeOnBoot left false (default): the session-restore path draws the
    // System screen without needing synthetic key presses. Sending F1 every
    // 1.5 s would cycle through apps and cause continuous screen redraws.
    return new Series3c::Emulator(c);
}
static EmuBase *makeSeries3mx() {
    Series3c::Config c;
    c.model       = Series3c::Model::Series3mx;
    c.displayName = "Psion Series 3mx";
    // 3mx crystal is 3.6864 MHz × 15/2 = ~27.648 MHz per MAME's
    // psion3mx_state::psion3mx machine_config.
    c.busClockHz  = 27'648'000;
    return new Series3c::Emulator(c);
}
static EmuBase *makePocketBook2() {
    Series3c::Config c;
    c.model       = Series3c::Model::Series3a;  // PB2 inherits psion3a_state in MAME
    c.displayName = "Acorn Pocket Book II";
    // PB2 is the Acorn-rebadged Series 3a/3a2 sold to UK schools 1996-99,
    // running an Acorn-customised v1.30f kernel. Hardware is identical to
    // the Series 3a (V30H + ASIC9 + LCD); only the ROM differs. We pin
    // the emulator config to the same values makeSeries3a() uses so
    // there's a single tested code path for both devices:
    //   - ramSize       = 2 MiB (matches 3a; MAME ships PB2 at 1 MiB
    //                     by default but our direct-memory path doesn't
    //                     apply ASIC9 configure_ram mapping below the
    //                     paged window, so 1 MiB tripped address-
    //                     aliasing differences that 2 MiB sidesteps).
    //   - autoWakeOnBoot = false (a previous PB2 build set this to true
    //                     to drag a key-wait POST into drawing the
    //                     System screen for headless tests; with the
    //                     May 2026 writeMemByte mirror fix the v1.30f
    //                     boot path no longer parks, AND the F1 train
    //                     it generated collided with user keypresses
    //                     in interactive use, causing the OS to reboot
    //                     when an app was launched in the browser).
    c.ramSize            = 0x200000;  // 2 MiB (match 3a)
    c.useStrictRamMirror = true;
    // v1.30f's session-restore takes a different code path than v3.40f's
    // and surfaces a "SYS$SHLL.$05 Process exited Exit number 72" dialog
    // a few seconds into cold boot. Pressing Esc dismisses it and the
    // System screen comes up clean — so we synthesise a single Esc tap
    // ~7 s in to do that for the user automatically. Distinct from
    // autoWakeOnBoot's F1 train: this fires once and never again, so it
    // doesn't collide with user keypresses during app launch.
    c.dismissColdBootDialog = true;
    return new Series3c::Emulator(c);
}
// Psion Series 7 / netBook share an SA-1100 SoC, 32 MB SDRAM and a
// 640×480 LCD. The ROMs differ (the netBook ships a later EPOC R5
// release with USB-host drivers), but the emulator surface is identical
// — only the displayed device name changes.
static EmuBase *makeSeries7() { return new Series7::Emulator("Psion Series 7"); }
static EmuBase *makeNetBook() { return new Series7::NetBookEmulator; }
static EmuBase *makeNetpad() { return new Series7::NetpadEmulator; }

// Psion Organiser II (1986) — Hitachi HD6303X + HD44780 character LCD +
// two Datapak/Rampak slots. Scaffold landing: the HD6303 core in
// core/hd6303.cpp is a stub that prints "first unimplemented opcode" and
// halts, so the device boots far enough to confirm the wiring before the
// CPU port lands. Status flips to Supported once the boot reaches the
// LZ menu screen. See plan + reference/mame-psion-org2/ for the porting
// source (MAME's src/mame/psion/psion.cpp, BSD-3-Clause).
static EmuBase *makeOrganiser2() { return new Organiser2::Emulator; }

// Psion Organiser I (1984) — the first of the line, and a much smaller
// machine than the Organiser II: an HD6301X0 running its whole program
// out of the CPU's own 4 KiB mask ROM, 2 KiB of external RAM, one row of
// 16 characters on an HD44780, and the same two Datapak slots. The
// driver is core/organiser1.{h,cpp}; the ROM image is MAME's `psion1`
// byte for byte.
static EmuBase *makeOrganiser1() { return new Organiser1::Emulator; }

// Psion Workabout (1995, V30H + ASIC9). Industrial handheld variant
// of the Series 3a/3c stack — same chipset, different keyboard, smaller
// 240x100 LCD, 1 MiB RAM.
static EmuBase *makeWorkabout() {
    Series3c::Config c;
    c.model       = Series3c::Model::Workabout;
    c.displayName = "Psion Workabout";
    c.lcdWidth    = 240;
    c.lcdHeight   = 100;
    c.ramSize     = 0x100000;   // 1 MiB
    c.romSize     = 0x200000;   // 2 MiB
    c.busClockHz  = 7'680'000;  // V30H, per MAME PSION_ASIC9(... 7.68_MHz_XTAL)
    c.useStrictRamMirror = true;
    c.coldBootStatus     = true;
    return new Series3c::Emulator(c);
}

// Psion WorkaboutMX (1998-2000, V30MX + ASIC9MX). Successor to the
// Workabout with the same form factor + keyboard but a 27.6864 MHz CPU
// (V30MX = V30H at 3.6864 MHz × 15/2, same clock as Series 3mx) and
// 2 MiB RAM. PsionAsic9 in this tree already serves as the Series 3mx
// driver, so the same code path covers ASIC9MX here.
static EmuBase *makeWorkaboutMX() {
    Series3c::Config c;
    c.model       = Series3c::Model::WorkaboutMX;
    c.displayName = "Psion WorkaboutMX";
    c.lcdWidth    = 240;
    c.lcdHeight   = 100;
    c.ramSize     = 0x200000;   // 2 MiB
    c.romSize     = 0x200000;   // 2 MiB
    c.busClockHz  = 27'648'000; // 3.6864 MHz × 15/2 per MAME workabout.cpp
    c.useStrictRamMirror = true;
    c.coldBootStatus     = true;
    return new Series3c::Emulator(c);
}

static EmuBase *makeSiena() {
    Series3c::Config c;
    c.model       = Series3c::Model::Siena;
    c.displayName = "Psion Siena";
    // Siena uses the same V30H + ASIC9 as 3c but a 240×160 panel,
    // 512 KiB RAM + 1 MiB ROM per reference/mame-psion/psion/siena.cpp.
    //
    // We previously tried bumping RAM to 1 MiB (MAME's
    // `set_extra_options("1M")` tier) to give the kernel more heap so
    // Word/Sheet/Agenda wouldn't trip "No system memory" on launch.
    // That exposed a worse regression: under our simple-stride
    // translation `setRamTypeDefault(initialType)` seeds m_a9_ram_type
    // from ram.size() (core/series3c.cpp ~line 53), so 1 MiB seeds
    // type=2 (1 MiB-device-pair layout spanning 0..2 MiB internal),
    // but only addresses < 1 MiB actually map to physical RAM under
    // simple-stride. The kernel's M: drive / FAT writes to "device 1
    // storage" at internal 0x100000-0x1FFFFF vanish into open bus,
    // SYS$SHLL.$05 hits a corrupt allocation table during cold boot
    // and exits with code 7 — the device is bricked.
    //
    // 512 KiB is the canonical config and avoids the SYS$SHLL crash.
    // The Word/Sheet/Agenda OOM regression on 512K is the lesser
    // evil — Data and World still launch, the System screen is fully
    // usable, and Esc dismisses any OOM dialog cleanly.
    //
    // A future 1 MiB Siena would need either a MAME-strict mirror
    // path that preserves the kernel's IVT-clobbering DAT-directory
    // write, or backing-store for the unmapped "device 1" range so
    // writes survive even though no physical second device exists.
    c.lcdWidth    = 240;
    c.lcdHeight   = 160;
    c.ramSize     = 0x80000;    // 512 KiB
    c.romSize     = 0x100000;   // 1 MiB
    c.busClockHz  = 7'680'000;  // 7.68 MHz per MAME PSION_ASIC9(... 7.68_MHz_XTAL)
    //
    // Regression note: this used to be wired to 3'686'400 (3.6864 MHz —
    // the *Condor UART* clock from siena.cpp:298, not the ASIC9 XTAL).
    // Running the kernel at half speed left every timer-driven scheduler
    // path racing the wrong way: the System screen rendered briefly and
    // then the OS crashed ~40 sim-seconds in (kernel jumped into ROM
    // string-table bytes, halted on opcodes 0x65/0x64/0xFF, watchdog
    // attempted a reset, screen blanked). Switching to MAME's actual
    // 7.68 MHz also makes the v4.20f kernel populate the AGN/SPR/DAT/
    // WRD/WLD M:-drive subdir tree on cold boot — same behaviour as the
    // 3a v3.40f kernel under MAME-strict mirror — so several previously
    // documented Siena-specific quirks no longer apply.
    c.useStrictRamMirror = false;
    return new Series3c::Emulator(c);
}

static const DeviceProfile kProfiles[] = {
    {
        "5mx",
        "Psion Series 5mx",
        "5mx_v1.05(260)_eng.bin",
        0x1000000,
        0x7060001,
        "5mx.svg",
        DeviceStatus::Supported,
        makeWindermere5mx,
        true,   // CompactFlash slot
        0, 0,   // no SSD / Datapak slots
        2, 1,   // Remote Link on UART2, IrDA on UART1
        1, 1,   // PLP+RFSV32 cable, IrDA+EikonIR beam
    },
    {
        "5mxpro",
        "Psion Series 5mx Pro",
        // Real 5mx Pro hardware has no embedded OS in mask ROM; it boots from a
        // 128 KB bootloader Flash that loads SYS$ROM.BIN from the CF card into
        // DRAM. The frontend synthesises a default FAT16 CF image carrying a
        // SYS$ROM.BIN payload so the bootloader has something to load on first
        // run — see buildDefault5mxProCard in useEmulator.ts.
        "5mxPRO_BL_v1.09_ger.bin",
        0x20000,        // 128 KiB — actual bootloader size on disk
        0x7060001,
        "5mx.svg",
        DeviceStatus::Supported,
        makeWindermere5mxPro,
        true,   // CompactFlash slot
        0, 0,
        2, 1,   // Remote Link on UART2, IrDA on UART1
        1, 1,
    },
    {
        // The Ericsson MC218 is a rebadged Psion Series 5mx (same
        // Windermere SoC, same ETNA PCMCIA controller, same ROM
        // layout — the ROM's variant-ID reports 0x07060001, matching
        // 5mx, and the PCCARD-ATA module sits at identical offsets
        // to 5mx v1.05(260)). It is NOT a CLPS7111-based device;
        // pointing the profile at makeCLPS7111 made the emulator
        // fail to boot the ROM. The Series 5 (original, not 5mx) is
        // the CLPS7111-based machine.
        "mc218",
        "Ericsson MC218",
        "MC218_v1.05(259)_eng.bin",
        0xC00000,
        // MC218 ROM carries the same variant ID (0x7060001) as the 5mx, so
        // findProfileByVariant will always match the 5mx profile first and
        // MC218 is picked up by that path in the shipping build. Leaving
        // variant-id 0 here keeps that behaviour and stops this profile from
        // shadowing 5mx if someone reorders the registry.
        0,
        "mc218.png",
        DeviceStatus::Supported,
        // CLPS7111::Emulator is the correct chip for MC218 but doesn't boot
        // any real MC218 ROM in this tree (loops at pc=0x0a44). Windermere is
        // close enough that the EPOC ROM runs cleanly on it and that is what
        // auto-detection has been shipping in practice. Use it explicitly so
        // --device mc218 produces the same result as the auto-detect path.
        makeWindermereMC218,
        true,   // CompactFlash slot
        0, 0,
        2, 1,   // Remote Link on UART2, IrDA on UART1
        1, 1,
    },
    {
        "osaris",
        "Oregon Scientific Osaris",
        "Osaris_v1.02(209)_eng.bin",
        0x800000,
        0,
        "osaris.svg",
        DeviceStatus::Supported,
        makeOsaris,
        true,   // CompactFlash slot
        0, 0,
        1, 1,   // cable + IrDA both on the single host-bridged UART1
                // (CL-PS7111; the ROM autostarts the PLP Req_Req on
                // it — the cable handshake needs the ER3/ER4 0x22
                // Req_Con flavour, see RemoteLinkDialog's conSeq)
        1, 1,
    },
    {
        "series5",
        "Psion Series 5",
        "series5_v1.01(144)_eng.bin",
        0x600000,
        0,
        "series5.svg",
        // Series 5 boots fully to the interactive EPOC R1 desktop
        // (icon sidebar + taskbar + app grid render from the live
        // framebuffer; see tests/golden/series5.pgm). See core/series5.h
        // for the device specifics and the preserved bring-up log.
        DeviceStatus::Supported,
        makeSeries5,
        true,   // CompactFlash slot (the original Series 5 introduced it)
        0, 0,
        1, 1,   // cable + IrDA both on the single host-bridged UART1
                // (CL-PS7110; ER3 link — same 0x22 Req_Con flavour as
                // the Osaris, see RemoteLinkDialog's conSeq)
        1, 1,
    },
    {
        // Geofox One — 1997 EPOC32 clamshell from Geofox Ltd, a Psion
        // licensee. Same ARM710 / CL-PS711x platform and the same EPOC
        // Release 1 kernel generation as the Series 5 (its ROM reports
        // version 1.01(146) against the Series 5's 1.01(144)), but a
        // genuinely different machine: a 640x320 panel where the Series 5
        // has 640x240, a trackpad instead of a touchscreen, and a row of
        // hardware application keys down the left of the keyboard deck.
        // See core/geofox.h for how the panel geometry was read out of
        // the ROM's own LCDCON programming.
        "geofox",
        "Geofox One",
        "Geofox_v1.01(146)_eng.bin",
        0x800000,
        // No EPOC variant ID: this ROM's header carries 0 where the
        // Osaris / 5mx images carry a variant-file pointer, so
        // detectROMVariant finds nothing and auto-detection falls back to
        // matching on ROM size. 8 MB is shared with the Osaris and the
        // Revo, both registered ahead of this entry, so the fallback will
        // not pick the Geofox — the frontend and harness always name it
        // explicitly by id, which is the path every 8 MB device already
        // relies on.
        0,
        nullptr,  // skin resolved frontend-side, as for every device
        DeviceStatus::Supported,
        makeGeofox,
        false,  // The case has a Type II PC Card slot and the ROM's
                // battery warnings talk about powering a PC Card, but a
                // card attached through the shared CF path does not
                // mount: with one inserted the guest never touches the
                // PC-card window at region 4 at all (measured with
                // PSION_PHYS_WATCH over 0x40000000-0x4FFFFFFF across a
                // full boot), so its socket detect/power is on hardware
                // this emulator does not model yet. The Series 5's route
                // in is an SSI ADC channel (0xE1, Vcc sense) and the
                // Geofox makes exactly 32 SSI transfers in a whole boot,
                // all of them the configuration-PROM scan. (The mouse pad
                // is on that bus too, but it only transfers when the pad
                // has something to report, so an untouched boot is still
                // those 32 and nothing else.) Advertising
                // the slot would only offer a card UI that silently does
                // nothing.
        0, 0,
        1, 1,   // cable + IrDA both on the single host-bridged UART1, the
                // CL-PS7110 arrangement the Series 5 uses; the ROM ships
                // Euart1.pdd / Euart2.pdd, Plp.prt and IrDA.prt. The cable
                // link is verified: the machine ships with Remote Link set
                // to Cable at 115200, so nothing has to be switched on
                // first, and attaching the bridge draws the Req_Req_Pdu
                // burst that carries the handshake through to NCP Info —
                // tests/integration/test-remote-link.sh has the row. That
                // needed the board's modem lines read active-low, which is
                // where this machine parts company with Psion's own
                // CL-PS711x boards; see core/geofox.h.
        1, 1,
    },
    {
        // Psion Series 7 (StrongARM SA-1100). Same hardware family as
        // the netBook — same SA-1100 SoC, 32 MB SDRAM, 640x480 TFT,
        // Eiger ASIC. Real Series 7 differs in the "personality chip"
        // (mask ROM contents) and the lack of a separate YModem
        // bootloader: the SA-1100 boots straight from the 16 MB Series
        // 7 ROM at PA 0.
        //
        // The shipping v1.05(254) Series 7 ROM (b754 / b756) boots all
        // the way through to an interactive EPOC R5 desktop (boot-check
        // variance ~5140, no traps).  CompactFlash works too: a card
        // attached via the CF Card dialog mounts as drive D: through the
        // OS's own medata driver (default-on native CF, see
        // docs/series7-cf-investigation.md; kill switch
        // PSION_S7_NO_NATIVE_CF).  The earlier HAL-dispatch / WFI-freeze
        // limitation is resolved.
        "series7",
        "Psion Series 7",
        "series7_v1.05(254)_b756_eng.bin",
        0x1000000,
        0,
        "series7.svg",
        DeviceStatus::Supported,
        makeSeries7,
        true,   // CompactFlash slot (two of them on the Series 7)
        0, 0,
        3, 2,   // Remote Link on UART3, IrDA on the ICP (UART2)
        1, 1,
    },
    {
        "revo",
        "Psion Revo",
        "Revo_v1.06(390)_eng.bin",
        0x800000,
        // Shares variantId 0x7060001 with 5mx; leave 0 so auto-detect stays
        // on the 5mx profile and Revo is only selected explicitly.
        0,
        "revo.svg",
        DeviceStatus::Supported,
        makeRevo,
        false,  // Revo has no memory-card slot — internal 8 MB flash only.
        0, 0,
        2, 1,   // Remote Link on UART2, IrDA on UART1
        1, 1,
    },
    {
        // Psion "Conan" — the Revo's successor, from the ROM of a real
        // machine, dumped with tools/romdump (TRomHeader version 0.10(17),
        // built 2001-06-20, 16 MB — the whole declared image, every part
        // read back against the ROM on the device). Same board as the Revo,
        // so it takes the whole Revo profile: 480x160 LCD inside a 527x208
        // digitiser, no card slot, Remote Link on UART2 and IrDA on UART1.
        // See core/conan.h for what the image establishes about the
        // hardware, and for the WAP + Bluetooth stacks that make it a
        // different machine from the shipping Revo. The one thing it does
        // NOT inherit is Remote Link: its ROM carries a later connectivity
        // stack that our PLP client can't talk to, so linkProtocol is 0 —
        // see the field comment below.
        //
        // This image, unlike the 0.01(22) engineering build kept below,
        // names the machine itself: it paints its own splash reading
        // "Psion Conan (c) Psion Digital 2001 / EPOC Release 6 (c)
        // Copyright Symbian LTD 2001", over a CONAN wordmark with ARM,
        // EPOC and Bluetooth badges. So the codename this repository has
        // always used for the device is the ROM's own, and the machine is
        // an EPOC R6 (Symbian OS 6.0) build rather than the R5 its
        // predecessor image reports.
        //
        // It boots to an interactive EPOC desktop — Agenda, Jotter and
        // Contacts down the Documents sidebar, a "Bluetooth on" tab in the
        // toolbar the shipping Revo has no equivalent of — byte-stable
        // from ~80 sim-seconds on (tests/golden/conan.pgm; identical PGMs
        // at 80 / 100 / 120 / 180 s), settling at variance ~2615. It takes
        // longer to settle than the engineering build because it spends
        // its first minute on the splash above. The "Problem initialising
        // Mail / Not found" dialog in the golden is the image's own
        // startup fault, not an emulation one, and the desktop behind it
        // is interactive — the same class of thing as the older image's
        // Agenda panic.
        "conan",
        "Psion Revo (Conan)",
        "conan_v0.10(17)_eng.IMG",
        // 0x1000000 — the image's real length, and exactly the iRomSize
        // its TRomHeader declares. The dump is complete, so unlike the
        // engineering build below there is no undelivered tail.
        0x1000000,
        // Same 0x7060001 variant ID as the 5mx and Revo — see the Revo
        // profile above. Left 0 for the same reason: auto-detect stays on
        // the 5mx profile and Conan is only selected explicitly.
        0,
        "revo.svg",
        DeviceStatus::Supported,
        makeConan,
        false,  // No memory-card slot, same as the Revo.
        0, 0,
        2, 1,   // Remote Link on UART2, IrDA on UART1
        // linkProtocol 0 — the cable is wired and the bridge works, but this
        // ROM does not speak the PLP dialect our host client implements, so
        // the frontend hides Remote Link on it, drops the PLP "Printer via
        // PC" tab (raw serial printer capture still works — it doesn't go
        // through PLP), and stops offering cable app installs. This is the
        // ER5u/ER6 connectivity generation, the one that needed a new
        // PsiWin on real hardware, and both Conan images say so the same
        // way: every other supported EPOC ROM ships PlpDL.prt, the module
        // that carries the "PLP Link" data link (SYN 16 / DLE-STX framing,
        // CRC-CCITT trailer) our frontend/src/lib/plp talks, and neither
        // Conan image has it at all — Plp.prt provides "PLP Link" itself,
        // over a new escaped serial transport. The escape coding, the
        // ENQ/ACK probes and the Unicode link-service names are recorded
        // against the engineering build in the profile below, which is
        // where they were established; this image ships the same module
        // set (Plp.prt, PlpSvr.dll, PlpRfs.rsy, no PlpDL.prt), so the
        // conclusion carries over unchanged.
        0, 1,
    },
    {
        // The older Conan engineering image (TRomHeader version 0.01(22),
        // built 2001-05-12, declaring 12 MB). Kept alongside the dumped
        // 0.10(17) ROM above because it is a genuinely different build of
        // the machine, not merely an earlier one: it paints the *Revo's*
        // splash ("Psion Revo (c) Psion PLC 1999 / EPOC Release 5") where
        // the later image paints Conan's own, and it reaches its desktop
        // in half the time. Same hardware and factory as the default
        // profile above — only the ROM image differs — so it shares
        // makeConan, the skin and the (absent) slot layout. Hidden from
        // the picker (hiddenFromPicker = true): the frontend offers it via
        // the same discreet header link the MC400's two ROMs use, rather
        // than a second near-duplicate list entry, but it keeps its own id
        // so save states and the device label stay distinct.
        //
        // It boots to an interactive EPOC R5 desktop, fully painted and
        // byte-stable from ~40 sim-seconds on (tests/golden/conanv001.pgm;
        // identical PGMs at 40 / 45 / 50 s). The image's own Agenda
        // panics with CONE 14 about 16 sim-seconds in and leaves a
        // "Program closed" dialog over the desktop — an engineering-build
        // fault, not an emulation one (with the host RTC moved back to
        // mid-2000 the frame is byte-identical to the golden apart from
        // the clock cell, and dismissing the dialog leaves a working
        // machine), so the golden screenshot carries it.
        "conanv001",
        "Psion Revo (Conan v0.01)",
        "conan_s2_2201.engbuild.IMG",
        // 0xBB2000 — the image's real length. Its TRomHeader declares a
        // 12 MB ROM, but the image stops short of that; Windermere's
        // 16 MB ROM[] is zero-filled before loadROM's copy, so the
        // undelivered tail reads as 0 rather than as stale bytes. Nothing
        // is lost: the image's own file data ends at 0xBB15D3, inside the
        // delivered bytes, so the short tail is padding the build never
        // filled.
        0xBB2000,
        // Same 0x7060001 variant ID as the 5mx and Revo — see the Revo
        // profile above. Left 0 for the same reason: auto-detect stays on
        // the 5mx profile and Conan is only selected explicitly.
        0,
        "revo.svg",
        DeviceStatus::Supported,
        makeConan,
        false,  // No memory-card slot, same as the Revo.
        0, 0,
        2, 1,   // Remote Link on UART2, IrDA on UART1
        // linkProtocol 0 — see the default Conan profile above for why
        // neither Conan image can use our PLP client. This is the image
        // the finding was established on, and the detail lives here:
        //
        //   - Conan has no PlpDL.prt at all: Plp.prt (0x502740b0) provides
        //     "PLP Link" itself, over a new escaped serial transport.
        //   - That transport is an XON/XOFF-safe byte stream: ESC = 0x19,
        //     with 0x11 -> 19 20, 0x13 -> 19 21, 0x19 -> 19 19, plus two
        //     in-band control codes, ENQ = 19 23 and ACK = 19 24 (encoder at
        //     0x50275308, decoder jump table at 0x50275648). Attach the
        //     bridge and the machine answers a host ENQ with an ACK within
        //     one poll, and probes with its own ENQ ~2 s after any inbound
        //     byte — the `19 23 19 23 19 23` a user sees in the Remote Link
        //     dialog's raw-byte panel. No other supported ROM contains that
        //     code.
        //   - It exports the Unicode link services (SYS$RFSVU.* /
        //     SYS$RPCSU.*) where the Revo and 5mx export SYS$RFSV.* /
        //     SYS$RPCS.*, so even a completed handshake would have our
        //     RFSV32 client connecting to a server name this ROM never
        //     registers.
        //
        // And it behaves accordingly: it answers no PLP frame we can send.
        // Harness sweeps (tests/integration/test-remote-link.sh conan, plus
        // the wider matrix recorded in docs/conan-remote-link.md) covered
        // every PDU type, framed with and without the SYN prefix, and all
        // 65536 possible CRC trailers on Req_Req_Pdu — the device stayed
        // silent apart from its own ENQ probes. Supporting it needs the
        // newer client written and validated against this transport, not a
        // tweak to the existing one.
        0, 1,
        true,   // hiddenFromPicker — alternate-ROM variant of conan
    },
    {
        // The Psion netBook.  Real hardware ships a 2 MB YModem
        // bootloader (netBook_BL_v011_eng.bin) in on-board flash and
        // loads the full EPOC R5 OS image (D:\OS.IMG) from a FAT16 CF
        // card.  We default to the bootloader so the user sees the
        // familiar "insert a CompactFlash card" splash on cold boot,
        // matching the real device.  The frontend's "Insert CF card
        // containing OS" button then attaches a synthesised FAT16
        // image carrying netBook_v1.05(450)_eng.img as D:\OS.IMG; the
        // bootloader's CF-detect chain on v0.11 silicon doesn't drive
        // the OS load itself (no slot-32 IRQ handler — see
        // docs/netbook-cf-investigation.md), so attachCard() in
        // core/sa1100.cpp does the bootloader's job host-side: scans
        // the FAT16 image for D:\OS.IMG, strips the 256-byte EPOCARM
        // ROM header, copies the body into RAM bank 1 and ROM (which
        // backs the reset vector via the existing flash mapping), and
        // resets the CPU so the OS boots exactly the way it would on
        // real hardware after the bootloader handoff.
        //
        // Once booted, a CompactFlash data card mounts as drive D: through
        // the netBook OS's own medata driver — a faithful port of the
        // Series 7 native-CF path to the v450 ROM (the OS card that boots
        // the device also mounts as D:, and any card swapped in via the CF
        // dialog mounts the same way).  Default-on; kill switch
        // PSION_NB_NO_NATIVE_CF.  See docs/netbook-cf-investigation.md
        // (2026-06-03 resolution).
        "netbook",
        "Psion netBook",
        "netBook_BL_v011_eng.bin",
        0x200000,               // 2 MB YModem bootloader flash image
        0,
        "netbook.svg",
        DeviceStatus::Supported,
        makeNetBook,
        true,   // CompactFlash slot (two of them on the netBook)
        0, 0,
        3, 2,   // Remote Link on UART3, IrDA on the ICP (UART2)
        1, 1,
    },
    {
        // The Psion netpad — a netBook-class SA-1100 machine with a colour
        // panel at a Series-5-like resolution.  Unlike the netBook (2 MB
        // YModem bootloader + CF OS.IMG handoff), Netpad.img is a complete
        // EPOC R5 ROM that boots straight from PA 0, exactly like the
        // Series 7 16 MB image — SA1100::Emulator::loadROM copies it into
        // ROM[] with no EPOCARM-header strip (isNetBookRom_ stays false).
        // The 12.3 MB image fits the fixed 16 MB ROM[] buffer.
        //
        // The board peripherals the netpad boot ROM drives are modelled in
        // core/sa1100.cpp: the GPIO ready/handshake pull-ups, the nCS4
        // serial device, the bit-banged I2C board controller, and the
        // SA-1110 deep-sleep / wake-by-reset path (the OS boots into the
        // EPOC "off" state and is switched on by a synthetic power-button
        // press, resuming through the ROM's PSPR context).  It boots to the
        // netpad EPOC desktop; the 640x240 panel is 8 bpp palettised, and
        // the palette the ROM programs is a 6x6x6 colour cube, so the panel
        // renders in colour.  The stylus and the board's ADC channels hang
        // off the SoC's SSP as an ADS7846-class codec (see the netpad SSP
        // model in core/sa1100.cpp).
        //
        // Remote Link rides UART3, the same port the Series 7 / netBook
        // use: the netpad's EPOC R5 build starts RemoteLinkServer4 at
        // boot and its Req_Req_Pdu / Req_Con / NCP-Info handshake
        // completes over the host bridge (see
        // tests/integration/test-remote-link.sh).  Note EPOC only opens
        // the port once Remote link is switched on from the System
        // screen's Tools menu — reachable via the frontend's Menu key,
        // which the netpad's synthetic key path now delivers.
        //
        // Infrared rides the SoC's Infrared Communications Port on SER2 /
        // UART2, the same wiring as the Series 7 / netBook — the netpad's
        // Tools menu carries the usual EPOC R5 Infrared -> Send / Receive
        // submenu and its ROM ships the same IrDA.prt / IrCOMM.csy stack.
        //
        // The removable-media slot takes an MMC card, not a PC-Card: the
        // card hangs off the board FPGA's SPI port and EPOC reaches it
        // through the variant's own card-init state machine and
        // medmmc.pdd, mounting it as drive D:.  See core/netpad_mmc.cpp
        // and docs/netpad-rom-and-mmc.md.
        //
        // The frontend has both a photo skin and a skinless view for the
        // machine (device-skins/netpad.png and the silkscreen icon column
        // in skins/netpad_buttons_right.png), including tap zones for the
        // five silkscreen keys.
        // Selection is by explicit device id or the (unique) exact ROM size.
        "netpad",
        "Psion netpad",
        "Netpad.img",
        12296196,               // exact ROM size (unique -> size auto-detect)
        0,                      // no variant-id autodetect; id/size selects it
        nullptr,                // skin resolved frontend-side, as for every device
        DeviceStatus::Supported,
        makeNetpad,
        false,  // no CompactFlash / PC-Card socket
        0, 0,
        3, 2,   // Remote Link on UART3, IrDA on the ICP (UART2)
        1, 1,   // EPOC32 PLP + RFSV32 over the cable; IrDA + Eikon-IR beam
        false,  // shown in the device picker
        true,   // MMC card slot (drive D:)
    },
    {
        "series3",
        "Psion Series 3",
        "series3_v1.91f_eng.bin",
        0x80000,
        0,
        nullptr,
        DeviceStatus::Supported,
        makeSeries3,
        false,  // No CompactFlash slot
        2,      // Two SSD pack slots (Pack A on ASIC2 ch1, Pack B on ch2)
    },
    {
        "pocketbk",
        "Acorn Pocket Book",
        "acorn_pocketBook_II.bin",
        0x80000,
        0,
        nullptr,
        DeviceStatus::Supported,
        makeAcornPocketBook,
        false,
        2,
    },
    {
        // Psion MC400 (1989) — clamshell laptop ancestor of the Series 3.
        // Same V30 + ASIC1 + ASIC2 SIBO1 chip set, but a 640x400 mono LCD
        // in laptopMode (VRAM at 0xB8000), 256 KiB ROM at 0xC0000 (reset
        // vector EA 00 00 00 C0 → C000:0000), and a full QWERTY laptop
        // keyboard. Auto-detect by ROM size (0x40000) — no other profile
        // ships a 256 KiB image in the SIBO family.  Defaults to the v2.60F
        // ROM; the older v1.26F ships as a hidden mc400v126 variant below,
        // reachable from a header link in the frontend.  This entry is
        // registered first so the size-based auto-detect (any 256 KiB SIBO
        // image with no variant ID) resolves to the default device.
        "mc400",
        "Psion MC400",
        "MC400_v2.60F.bin",
        0x40000,
        0,
        "mc400.svg",
        DeviceStatus::Supported,
        makeMC400,
        false,  // No CompactFlash slot
        4,      // Four SSD pack slots on ASIC2 ch1..4 (MAME mc400.cpp).
                // Pack 3 holds the system disk loaded by the boot ROM.
    },
    {
        // Older MC400 boot ROM (v1.26F).  Same hardware/factory as the
        // default mc400 above — only the ROM image differs — so it shares
        // makeMC400, the skin and the slot layout.  Hidden from the picker
        // (hiddenFromPicker = true): the frontend offers it via a discreet
        // header link rather than a second near-duplicate list entry, but
        // it keeps its own id so save states and the device label stay
        // distinct from the v2.60F default.
        "mc400v126",
        "Psion MC400 (v1.26F)",
        "MC400_V1.26F.bin",
        0x40000,
        0,
        "mc400.svg",
        DeviceStatus::Supported,
        makeMC400,
        false,  // No CompactFlash slot
        4,      // Four SSD pack slots on ASIC2 ch1..4 (MAME mc400.cpp).
        0,      // datapakSlotCount
        -1,     // remoteLinkUart
        -1,     // infraredUart
        0,      // linkProtocol
        0,      // irProtocol
        true,   // hiddenFromPicker — alternate-ROM variant of mc400
    },
    {
        // Psion MC200 (1989) — the 640x200 half-height sibling of the
        // MC400, same SIBO1 chip set and the same V2.12F boot ROM. The
        // ROM image is the two 28F010 chip dumps in
        // roms/MC200_V2.12F_ROM_disk/ interleaved into the 256 KiB image
        // the CPU sees (scripts/build-mc200-rom.mts).
        //
        // romVariantId stays 0 and the 0x40000 size is shared with the two
        // MC400 profiles registered above, so size-based auto-detect still
        // resolves a bare 256 KiB SIBO image to the MC400 — the MC200 is
        // selected by id, the way the MC400's own v1.26F variant is.
        "mc200",
        "Psion MC200",
        "MC200_v2.12F.bin",
        0x40000,
        0,
        "mc400.svg",
        DeviceStatus::Supported,
        makeMC200,
        false,  // No CompactFlash slot
        4,      // Four SSD pack slots on ASIC2 ch1..4, as on the MC400.
                // Pack D (slot 3) holds the ROM:: System Disk, pre-inserted
                // by the frontend on cold boot from
                // roms/MC200_V2.12F_system.ssd — the real machine's factory
                // pack, dumped (it is MAME's mc200_system_disk.bin).
    },
    {
        // Psion HC120 (1991) — the sealed industrial handheld of the
        // SIBO1 generation. Its ROM carries EPOC/Os V3.95F and a command
        // shell but no applications: those live on an SSD pack, so with
        // no pack in it the machine boots to "Insert Pack / and press
        // enter" and waits. The image is the machine's two flash chips
        // (roms/hc120/) joined by scripts/build-hc120-rom.mts.
        //
        // romVariantId stays 0 and the 0x40000 size is shared with the
        // MC profiles registered above, so size-based auto-detect still
        // resolves a bare 256 KiB SIBO image to the MC400 — the HC is
        // selected by id.
        "hc120",
        "Psion HC120",
        "hc120_v1.72F.bin",
        0x40000,
        0,
        "hc120.png",
        DeviceStatus::Supported,
        makeHC120,
        false,  // No CompactFlash slot
        2,      // Two SSD pack slots
    },
    {
        "series3a",
        "Psion Series 3a",
        "series3a_v3.40f_eng.bin",
        0x200000,
        0,
        nullptr,
        DeviceStatus::Supported,
        makeSeries3a,
        false,  // No CompactFlash slot
        2,      // Two SSD pack slots (ASIC9 SIBO ch1 / ch0)
    },
    {
        "pocketbk2",
        "Acorn Pocket Book II",
        "pb2_v1.30f_acn.bin",
        0x200000,
        0,
        nullptr,
        DeviceStatus::Supported,
        makePocketBook2,
        false,
        2,
    },
    {
        "series3c",
        "Psion Series 3c",
        "series3c_v5.20f_eng.bin",
        0x200000,
        0,
        nullptr,
        DeviceStatus::Supported,
        makeSeries3c,
        false,  // No CompactFlash slot
        2,      // Two SSD pack slots (ASIC9 SIBO ch1 / ch0)
        0,      // no Datapak slots
        0, 0,   // Remote Link + Infrared both ride the Condor host
                // bridge (uartIndex 0; the chip multiplexes cable/IR)
        2, 2,   // EPOC16 PLP + RFSV16 cable; "Psion IRLink" beam over
                // IrDA (both verified live against v5.20f — see
                // docs/sibo-remote-link.md)
    },
    {
        "series3mx",
        "Psion Series 3mx",
        "series3mx_v6.16f_eng.bin",
        0x200000,
        0,
        nullptr,
        DeviceStatus::Supported,
        makeSeries3mx,
        false,  // No CompactFlash slot
        2,      // Two SSD pack slots (ASIC9 SIBO ch1 / ch0)
        0,
        0, 1,   // Remote Link rides the ASIC9MX-integrated 16550
                // "UART1" (word-spaced at I/O 0x50-0x5E, interrupt =
                // ASIC9MX status bit 9 / vector 0x77); Infrared rides
                // "UART0" (0x40-0x4E, bit 8 / vector 0x76 — the
                // v6.16f kernel brings it up when the Infrared screen
                // is armed). Both pinned from the ROM + live traces,
                // see docs/sibo-remote-link.md.
        2, 2,   // EPOC16 PLP + RFSV16 cable; "Psion IRLink" beam over
                // IrDA (same 136-byte-header dialect as the 3c)
    },
    {
        "siena",
        "Psion Siena",
        "siena_v4.20f_eng.bin",
        0x100000,
        0,
        nullptr,
        DeviceStatus::Supported,
        makeSiena,
        false,  // No CompactFlash slot
        1,      // Single SSD slot via Honda connector (ASIC9 SIBO ch4)
        0,
        0, 0,   // Remote Link + Infrared on the Condor bridge
        2, 2,
    },
    {
        "workabout",
        "Psion Workabout",
        "workabout_w1_v2.40f_eng.bin",
        0x200000,
        0,
        "workabout.svg",
        DeviceStatus::Supported,
        makeWorkabout,
        false,  // Psion SSD packs, not CompactFlash
        2,      // Two SSD packs (Pack A + Pack B), per MAME workabout.cpp
    },
    {
        "workaboutmx",
        "Psion WorkaboutMX",
        "workaboutMX_v7.20f_eng.bin",
        0x200000,
        0,
        "workabout.svg",
        DeviceStatus::Supported,
        makeWorkaboutMX,
        false,  // Psion SSD packs, not CompactFlash
        2,      // Two SSD packs, same wiring as the Workabout
        0,
        0, -1,  // Remote Link rides the ASIC9MX-integrated 16550
                // "UART0" (word-spaced at I/O 0x40-0x4E, interrupt =
                // ASIC9MX status bit 8 / vector 0x76) — the v7.20f
                // link server defaults to it, unlike the 3mx which
                // links through UART1. No IR (no transceiver fitted).
        2, 0,   // EPOC16 PLP + RFSV16 cable (verified live: Req_Pdu
                // retries + NCP Info v3 handshake against v7.20f)
    },
    {
        // Psion Organiser I (1984). 4 KiB image — the HD6301X0's own
        // internal mask ROM, mapped at $F000-$FFFF; 2 KiB of external
        // RAM; a single row of 16 characters on an HD44780 driven as
        // two 8-position lines; two Datapak slots. See
        // core/organiser1.h for the memory map and the port wiring.
        //
        // The machine switches itself off at the end of cold boot and
        // waits for the ON key — the driver synthesises that press half
        // a second in so the emulator comes up showing the prompt
        // rather than a blank panel.
        "organiser1",
        "Psion Organiser I",
        "Organiser1.rom",
        0x1000,     // 4 KiB
        0,          // no EPOC variant ID (pre-EPOC machine)
        "organiser1.png",
        DeviceStatus::Supported,
        makeOrganiser1,
        false,      // No CompactFlash slot
        0,          // No SIBO SSD packs (different protocol)
        2,          // Pack A + Pack B (Datapak / Rampak)
    },
    {
        // Psion Organiser II (LZ / LZ64). 64 KiB ROM image; HD6303X CPU
        // running at 0.92 MHz; 16x4 character LCD via HD44780; two
        // Datapak / Rampak slots. The CPU core in core/hd6303.cpp is a
        // stub at this point — the factory exists so the harness can
        // boot the ROM far enough to print the first unimplemented
        // opcode (mirroring how the V30 stub bootstrapped Series 3).
        "organiser2",
        "Psion Organiser II",
        "OrganiserII.rom",
        0x10000,    // 64 KiB
        0,          // no EPOC variant ID (pre-EPOC machine)
        "organiser2.svg",
        DeviceStatus::Supported,
        makeOrganiser2,
        false,      // No CompactFlash slot
        0,          // No SIBO SSD packs (different protocol)
        2,          // Pack A + Pack B (Datapak / Rampak)
    },
    // Sentinel
    { nullptr, nullptr, nullptr, 0, 0, nullptr, DeviceStatus::ComingSoon, nullptr, false },
};

uint32_t detectROMVariant(const uint8_t *romData, size_t romSize) {
    if (romSize < 0x400000) return 0;
    uint32_t variantFile = (*reinterpret_cast<const uint32_t *>(&romData[0x80 + 0x4C])) & 0xFFFFFFF;
    if (variantFile >= romSize - 8) return 0;
    uint32_t variantImg = (*reinterpret_cast<const uint32_t *>(&romData[variantFile + 4])) & 0xFFFFFFF;
    if (variantImg >= romSize - 0x70) return 0;
    return *reinterpret_cast<const uint32_t *>(&romData[variantImg + 0x60]);
}

const DeviceProfile *findProfileByVariant(uint32_t variantId) {
    for (const DeviceProfile *p = kProfiles; p->id != nullptr; ++p) {
        if (p->romVariantId == variantId && p->romVariantId != 0) return p;
    }
    return nullptr;
}

const DeviceProfile *findProfileById(const char *id) {
    for (const DeviceProfile *p = kProfiles; p->id != nullptr; ++p) {
        if (strcmp(p->id, id) == 0) return p;
    }
    return nullptr;
}

const DeviceProfile *allProfiles() {
    return kProfiles;
}
