// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include "revo.h"

namespace Conan {

// Psion "Conan" — the Revo's successor, emulated from the engineering ROM
// roms/conan_s2_2201.engbuild.IMG. Its TRomHeader reads version 0.01(22),
// built 2001-05-12, declaring a 12 MB ROM; the Revo's shipping v1.06(390)
// image next to it reads 2000-06-26 and 8 MB.
//
// It is the Revo's board, which is why this class has nothing in it. Read
// out of the two images rather than assumed:
//
//   - Their HAL name tables list the same parts. Conan at image offset
//     0xB5FC4 has "LCD" / "LAP 53", "PEN" / "MLM 650", "ARM 710T", then the
//     device-type pair "REVO-PRO" and "REVO"; the Revo v1.06 image carries
//     the same run at 0x80E20. Two differences, neither of them hardware:
//     Conan spells the panel "LAP 53" where the Revo says "LAP 53S", and
//     Conan stores these particular strings as UCS-2 with a length prefix
//     where the Revo's are 8-bit and NUL-terminated — this is a later,
//     Unicode EPOC build.
//   - It paints the Revo's own boot splash ("Psion Revo (c) Psion PLC 1999
//     / EPOC Release 5") correctly at 480x160, and goes on to the Revo's
//     desktop layout: left Documents icon column, right System / Today /
//     Files / Control panel silkscreen bar. A wrong LCD geometry would tear
//     both, so Revo::Emulator's 480x160 / 527x208 overrides are right here.
//   - No card slot, same as the Revo: the profile in device_registry.cpp
//     sets hasCFSlot false.
//
// The remaining Revo::Emulator override, hasMachineId() == false, is
// inherited on the board argument rather than measured on this image: the
// ETNA PROM lines aren't routed on Revo hardware, so there is nothing for a
// Conan build to read either.
//
// What is genuinely new is the software in the image. It is 12 MB where the
// Revo's is 8, and the extra carries a WAP stack (WAPSTKSRV.EXE and the WTLS
// alert table) and a Bluetooth stack (btmanserver.exe, sdp.exe, thci.exe)
// that the shipping Revo ROM has neither of — that, rather than any board
// change, is what makes this a device of its own rather than another Revo
// ROM revision. The case agrees with the image: the lid of the machine in
// frontend/public/device-skins/conan_lidClosed.png is badged "revo
// Bluetooth". Neither stack is reachable here, though: no Bluetooth radio is
// modelled, and nothing in the emulator offers the WAP stack a bearer (the
// simulated modem is wired for the 5mx family, and this machine's UART2
// belongs to Remote Link).
//
// Being an engineering build it also puts EPOC's own tools on the desktop
// (EShell.exe, D_EXC.exe), and its Agenda panics with CONE 14 on every cold
// boot — the "Program closed / Agenda / CONE / 14" dialog in
// tests/golden/conan.pgm. That is the image, not the emulation: rerun with
// the host RTC moved back to mid-2000, the 40-second frame comes out
// byte-identical to that golden apart from the clock cell itself. The boot
// otherwise runs straight past the panic — sending Enter dismisses the
// dialog and leaves the desktop, silkscreen bar and running clock intact.
class Emulator : public Revo::Emulator {
public:
    // Distinct from Revo::Emulator's "Psion Revo" so the frontend's
    // deviceName-keyed paths (skin fallback, header label) can tell the two
    // apart; the profile's displayName in device_registry.cpp matches.
    const char *getDeviceName() const override { return "Psion Revo (Conan)"; }
};

}
