// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "device_registry.h"
#include "emubase.h"
#include "windermere.h"
#include "series5.h"
#include "revo.h"
#include "clps7111.h"
#include "osaris.h"
#include "series3.h"
#include "series3c.h"
#include "series7.h"
#include "organiser2.h"
#include <cstring>

// Per-device Windermere factories. Each one stamps the variant on the
// emulator so the shared Windermere audio + ROM-detection code can
// branch explicitly on device identity rather than inferring it from
// runtime flags. Adding a new variant: pick a Variant enum, add a
// factory here, point the device profile at it.
static EmuBase *makeWindermere5mx() {
    auto *emu = new Windermere::Emulator;
    emu->setVariant(Windermere::Variant::Mx5);
    return emu;
}
static EmuBase *makeWindermere5mxPro() {
    auto *emu = new Windermere::Emulator;
    emu->setVariant(Windermere::Variant::Mx5Pro);
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
static EmuBase *makeSeries5()    { return new Series5::Emulator; }
// Psion Revo uses a CL-PS7111-family SoC but the CLPS7111::Emulator in this
// tree does not boot any real ROM (see the mc218 profile comment). The Revo
// EPOC R5 ROM runs cleanly against Windermere in the native harness, and the
// Revo::Emulator subclass overrides the LCD/digitiser dimensions so the
// native 480x160 screen is walked correctly instead of 5mx's 640x240.
static EmuBase *makeRevo() {
    auto *emu = new Revo::Emulator;
    emu->setVariant(Windermere::Variant::Revo);
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

// Psion Organiser II (1986) — Hitachi HD6303X + HD44780 character LCD +
// two Datapak/Rampak slots. Scaffold landing: the HD6303 core in
// core/hd6303.cpp is a stub that prints "first unimplemented opcode" and
// halts, so the device boots far enough to confirm the wiring before the
// CPU port lands. Status flips to Supported once the boot reaches the
// LZ menu screen. See plan + reference/mame-psion-org2/ for the porting
// source (MAME's src/mame/psion/psion.cpp, BSD-3-Clause).
static EmuBase *makeOrganiser2() { return new Organiser2::Emulator; }

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
