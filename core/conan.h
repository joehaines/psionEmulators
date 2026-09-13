// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include "revo.h"

namespace Conan {

// Psion "Conan" — the Revo's successor. Two ROMs of it are emulated, and
// both run through this class (see device_registry.cpp):
//
//   conan      roms/conan_v0.10(17)_eng.IMG — the ROM of a real machine,
//              dumped off it with tools/romdump. TRomHeader version
//              0.10(17), built 2001-06-20, 16 MB, and the dump is the
//              whole declared image.
//   conanv001  roms/conan_s2_2201.engbuild.IMG — an earlier engineering
//              image. TRomHeader version 0.01(22), built 2001-05-12,
//              declaring 12 MB.
//
// The Revo's shipping v1.06(390) image next to them reads 2000-06-26, 8 MB.
//
// The machine ROM is the one that names the device: it paints its own
// splash reading "Psion Conan (c) Psion Digital 2001 / EPOC Release 6 (c)
// Copyright Symbian LTD 2001" over a CONAN wordmark carrying ARM, EPOC and
// Bluetooth badges. So "Conan" is the ROM's own name for itself rather than
// this repository's guess, and the machine is an EPOC R6 (Symbian OS 6.0)
// build — where the engineering image still reports R5, and paints the
// *Revo's* splash rather than Conan's.
//
// It is the Revo's board, which is why this class has nothing in it. Read
// out of the images rather than assumed:
//
//   - Their HAL name tables list the same parts. The engineering image at
//     offset 0xB5FC4 has "LCD" / "LAP 53", "PEN" / "MLM 650", "ARM 710T",
//     then the device-type pair "REVO-PRO" and "REVO"; the machine ROM
//     carries the identical run at 0x92570, and the Revo v1.06 image the
//     same at 0x80E20. Two differences from the Revo, neither of them
//     hardware: both Conan images spell the panel "LAP 53" where the Revo
//     says "LAP 53S", and both store these particular strings as UCS-2 with
//     a length prefix where the Revo's are 8-bit and NUL-terminated — these
//     are later, Unicode EPOC builds.
//   - Both paint their splash correctly at 480x160 and go on to the Revo's
//     desktop layout: left Documents icon column, right System / Today /
//     Files (or Bluetooth) / Control panel silkscreen bar. A wrong LCD
//     geometry would tear both, so Revo::Emulator's 480x160 / 527x208
//     overrides are right here.
//   - No card slot, same as the Revo: the profiles in device_registry.cpp
//     set hasCFSlot false.
//
// The remaining Revo::Emulator override, hasMachineId() == false, is
// inherited on the board argument rather than measured on this image: the
// ETNA PROM lines aren't routed on Revo hardware, so there is nothing for a
// Conan build to read either.
//
// What is genuinely new is the software in the images. They are 12 and
// 16 MB where the Revo's is 8, and the extra carries a WAP stack
// (WAPSTKSRV.EXE and the WTLS alert table) and a Bluetooth stack
// (btmanserver.exe, sdp.exe, thci.exe) that the shipping Revo ROM has
// neither of — that, rather than any board change, is what makes this a
// device of its own rather than another Revo ROM revision. The machine ROM
// goes further again: it puts a "Bluetooth on" tab in the silkscreen bar
// where the Revo has "Files", and carries an Opera browser (614 files
// against the engineering image's 475). The case agrees with the image: the lid of the machine in
// frontend/public/device-skins/conan_lidClosed.png is badged "revo
// Bluetooth". Neither stack is reachable here, though: no Bluetooth radio is
// modelled, and nothing in the emulator offers the WAP stack a bearer (the
// simulated modem is wired for the 5mx family, and this machine's UART2
// belongs to Remote Link).
//
// Both images leave a dialog on their first desktop, and in both cases it is
// the image's own doing rather than the emulation's.
//
// The engineering build, being an engineering build, also puts EPOC's own
// tools on the desktop (EShell.exe, D_EXC.exe), and its Agenda panics with
// CONE 14 on every cold boot — the "Program closed / Agenda / CONE / 14"
// dialog in tests/golden/conanv001.pgm. Rerun with the host RTC moved back
// to mid-2000 and the 40-second frame comes out byte-identical to that
// golden apart from the clock cell itself. The boot otherwise runs straight
// past the panic — sending Enter dismisses the dialog and leaves the
// desktop, silkscreen bar and running clock intact.
//
// The machine ROM spends its first minute on the splash above and then
// reports "Problem initialising Mail / Not found" over its desktop
// (tests/golden/conan.pgm), which settles byte-stable from ~80 sim-seconds
// on. Behind it are the same Documents column (Agenda, Jotter, Contacts),
// the sample binaries the build ships, and a running clock.
class Emulator : public Revo::Emulator {
public:
    // Distinct from Revo::Emulator's "Psion Revo" so the frontend's
    // deviceName-keyed paths (skin fallback, header label) can tell the two
    // apart; the profile's displayName in device_registry.cpp matches.
    const char *getDeviceName() const override { return "Psion Revo (Conan)"; }
};

}
