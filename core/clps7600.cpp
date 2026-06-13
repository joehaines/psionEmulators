// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2019 Ash Wolf (WindEmu); modifications (c) 2024-2026 Joe Haines.
// Subject to the Mozilla Public License v2.0 (http://mozilla.org/MPL/2.0/).

#include "clps7600.h"
#include "arm710.h"
#include <cstdlib>

CLPS7600::CLPS7600(ARM710 *_cpu)
{
	cpu = _cpu;
}

enum {
	PCM_BVD1 = 1,
	PCM_BVD2 = 2,
	PCM_CD1 = 4,
	PCM_CD2 = 8,
	PCM_VS1 = 0x10,
	PCM_VS2 = 0x20,
	PDREQ_L = 0x40,
	PCTL = 0x100,
	PCM_WP = 0x200,
	PCM_RDY = 0x400,
	FIFOTHLD = 0x800,
	IDLE = 0x1000,
	WR_FAIL = 0x2000,
	RD_FAIL = 0x4000,
	RESERVED = 0x8000
};

uint32_t CLPS7600::getInputLevel() const {
	uint32_t v = 0;

	if (isMemoryMode())
		v |= PCM_RDY; // we are ALWAYS ready

	// PCM_CD1 and PCM_CD2 are active-low card-detect pins on real hardware.
	// The "detected" state here means both CD pins are asserted, which the
	// CLPS7600 reports as zero in those bits. When no card is inserted we
	// set both bits (pins released).
	if (!cardPresent) {
		v |= (PCM_CD1 | PCM_CD2);
		return v;
	}

	// Card present: report a healthy CF-card pin state. The Osaris (and
	// Series 5) EPOC kernel polls input-level and runs separate checks on
	//   r0 & 0xC   — CD1/CD2 (card present, both 0 = OK)
	//   r0 & 0x3   — BVD1/BVD2 (battery voltage OK if both 1)
	//   r0 & 0x200 — PCM_WP (write protect)
	//   r0 & 0x400 — PCM_RDY (card ready)
	// (verified by disassembly at Osaris 0x500856d0/f8/710/728). Without
	// BVD1/BVD2 set, the kernel treats the card as dead-battery and never
	// reaches the Vcc-ramp / CIS-read stage — it just spins reconfiguring
	// the controller. CF cards are 3.3V keyed, so VS1#=0, VS2#=1 ⇒ in
	// CLPS7600 register convention PCM_VS2 set, PCM_VS1 clear.
	v |= PCM_BVD1 | PCM_BVD2 | PCM_VS2;
	return v;
}

uint32_t CLPS7600::read(uint32_t addr, ARM710::ValueSize valueSize)
{
	if (std::getenv("PSION_CF_TRACE"))
		cpu->log("CLPS7600 read: addr=%07x size=%d pc=%08x lr=%08x", addr, (valueSize == ARM710::V32) ? 32 : 8, cpu->getRealPC(), cpu->getGPR(14));
	if (valueSize == ARM710::V32) {
		switch (addr) {
		case 0xC000000: // Interrupt Status
			return interruptStatus;
		case 0xC000400: // Interrupt Mask
			return interruptMask;
		case 0xC001C00: // Interrupt Input Level
			return getInputLevel();
		case 0xC002000: // System Interface Configuration
			return systemInterfaceConfig;
		case 0xC002400: // Card Interface Configuration
			return cardInterfaceConfig;
		case 0xC002800: // Power Management
			return powerManagement;
		case 0xC002C00: // Card Power Control
			return cardPowerControl;
		case 0xC003000: // Card Interface Timing 0A
			return cardInterfaceTiming0A;
		case 0xC003400: // Card Interface Timing 0B
			return cardInterfaceTiming0B;
		case 0xC003800: // Card Interface Timing 1A
			return cardInterfaceTiming1A;
		case 0xC003C00: // Card Interface Timing 1B
			return cardInterfaceTiming1B;
		case 0xC004000: // DMA Control
			return dmaControl;
		case 0xC004400: // Device Information
			return deviceInformation;
		default:
			cpu->log("CLPS7600 unknown register read: addr=%07x pc=%08x lr=%08x", addr, cpu->getRealPC(), cpu->getGPR(14));
			return 0xFFFFFFFF;
		}
	}
	cpu->log("unknown!!");
	return 0xFF;
}

void CLPS7600::write(uint32_t value, uint32_t addr, ARM710::ValueSize valueSize)
{
	if (std::getenv("PSION_CF_TRACE"))
		cpu->log("CLPS7600 write: addr=%07x size=%d value=%08x pc=%08x lr=%08x", addr, (valueSize == ARM710::V32) ? 32 : 8, value, cpu->getRealPC(), cpu->getGPR(14));
	if (valueSize == ARM710::V32) {
		switch (addr) {
		case 0xC000400: // Interrupt Mask
			interruptMask = value;
			break;
		case 0xC000800: // Interrupt Clear (write-1-to-clear status bits)
			interruptStatus &= ~value;
			break;
		case 0xC000C00: // Interrupt Output Select
			break;
		case 0xC001000: // Interrupt Reserved Register 1
			break;
		case 0xC001400: // Interrupt Reserved Register 2
			break;
		case 0xC001800: // Interrupt Reserved Register 3
			break;
		case 0xC002000: // System Interface Configuration
			systemInterfaceConfig = value;
			break;
		case 0xC002400: // Card Interface Configuration
			cardInterfaceConfig = value;
			cpu->log("PC card enabled: %s", (value & 0x400) ? "yes" : "no");
			cpu->log("PC card write protect: %s", (value & 0x200) ? "yes" : "no");
			cpu->log("PC card mode: %s", (value & 0x100) ? "i/o" : "memory");
			break;
		case 0xC002800: // Power Management
			powerManagement = value;
			break;
		case 0xC002C00: // Card Power Control
			cardPowerControl = value;
			break;
		case 0xC003000: // Card Interface Timing 0A
			cardInterfaceTiming0A = value;
			break;
		case 0xC003400: // Card Interface Timing 0B
			cardInterfaceTiming0B = value;
			break;
		case 0xC003800: // Card Interface Timing 1A
			cardInterfaceTiming1A = value;
			break;
		case 0xC003C00: // Card Interface Timing 1B
			cardInterfaceTiming1B = value;
			break;
		case 0xC004000: // DMA Control
			dmaControl = value;
			break;
		case 0xC004400: // Device Information
			deviceInformation = value;
			break;
		default:
			cpu->log("CLPS7600 unknown register write: addr=%07x value=%08x pc=%08x lr=%08x", addr, value, cpu->getRealPC(), cpu->getGPR(14));
		}
	} else {
		cpu->log("unknown write!!");
	}
}
