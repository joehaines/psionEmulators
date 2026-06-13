// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include <cstddef>
#include <cstdint>

class EmuBase;

enum class DeviceStatus { Supported, ComingSoon };

struct DeviceProfile {
    const char *id;
    const char *displayName;
    const char *romFilename;
    size_t expectedRomSize;
    uint32_t romVariantId;
    const char *skinFilename;
    DeviceStatus status;
    EmuBase *(*createEmulator)();
    // True if the device has a CompactFlash slot (as opposed to the
    // Psion SSD packs on SIBO-family machines or no external-storage
    // slot at all on the Revo). The frontend hides the CF-card dialog
    // on devices where this is false so users aren't presented with
    // an option the hardware never had.
    bool hasCFSlot;
    // Number of Psion SSD pack slots (0 for ARM machines without SSD
    // hardware; 2 for Series 3 / 3a / 3c / 3mx; 1 for Siena). The
    // frontend shows the SSD dialog when this is > 0 and uses the
    // value to render the right number of slot rows.
    int ssdSlotCount = 0;
    // Number of Psion Organiser II Datapak / Rampak slots (2 on every
    // shipping Organiser II model; 0 elsewhere). The frontend shows the
    // Datapak dialog — distinct from the SIBO SSD dialog because the
    // pack model is different (per-slot kind discriminant, EPROM vs
    // SRAM) — when this is > 0.
    int datapakSlotCount = 0;
    // Remote Link (PsiWin/PLP cable) capability: the uartIndex to pass
    // to serialAttachHost, or -1 when the device has no host-bridgeable
    // serial port. The frontend shows the Remote Link button when >= 0.
    // (Windermere routes the cable through UART2; SA-1100 through
    // UART3; SIBO2 devices use 0 = the Condor cable path.)
    int remoteLinkUart = -1;
    // Infrared capability: the uartIndex of the IR transceiver, or -1
    // when the device has no IR port. (Windermere/CL-PS711x IrDA is
    // UART1 in SIR mode; SA-1100 uses the ICP on UART2; SIBO2 devices
    // use 1 = the Condor IR media path.)
    int infraredUart = -1;
    // Which link-layer protocol family the device's ROM speaks over the
    // Remote Link: 0 = none, 1 = EPOC32 PLP + RFSV32, 2 = EPOC16 (SIBO)
    // PLP + RFSV16. Drives the frontend's protocol selection.
    uint8_t linkProtocol = 0;
    // Which protocol the device speaks over IR: 0 = none, 1 = IrDA +
    // Eikon-IR beam (EPOC32), 2 = PLP-over-IR remote link (SIBO).
    uint8_t irProtocol = 0;
};
