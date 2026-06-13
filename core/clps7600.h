// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2019 Ash Wolf (WindEmu); modifications (c) 2024-2026 Joe Haines.
// Subject to the Mozilla Public License v2.0 (http://mozilla.org/MPL/2.0/).

#pragma once
#include <stdint.h>
#include "arm710.h"

class CLPS7600
{
private:
	ARM710 *cpu;

	uint32_t interruptStatus = 0;
	uint32_t interruptMask = 0;
	uint32_t systemInterfaceConfig = 0x1F8;
	uint32_t cardInterfaceConfig = 0;
	uint32_t powerManagement = 0;
	uint32_t cardPowerControl = 0;
	uint32_t cardInterfaceTiming0A = 0x1F00;
	uint32_t cardInterfaceTiming0B = 0;
	uint32_t cardInterfaceTiming1A = 0x1F00;
	uint32_t cardInterfaceTiming1B = 0;
	uint32_t dmaControl = 0;
	uint32_t deviceInformation = 0x40;

	bool isIOMode() const { return (cardInterfaceConfig & 0x100); }
	bool isMemoryMode() const { return !(cardInterfaceConfig & 0x100); }
	uint32_t getInputLevel() const;

	bool cardPresent = false;

public:
	CLPS7600(ARM710 *_cpu);

	uint32_t read(uint32_t addr, ARM710::ValueSize valueSize);
	void write(uint32_t value, uint32_t addr, ARM710::ValueSize valueSize);

	// Flipped by the emulator when a virtual CF card is attached/detached.
	// Surfaces through the PCM_CD1/CD2 bits of the input-level register so
	// EPOC sees the slot transition, and latches CD1_CHG/CD2_CHG bits in
	// the Interrupt Status register so the driver's IRQ handler advances
	// past its "wait for card-detect change" stage. Bits per CLPS7600
	// datasheet section 4.4 (Interrupt Status):
	//   bit 0  BVD1_CHG, bit 1 BVD2_CHG
	//   bit 2  CD1_CHG,  bit 3 CD2_CHG
	//   bit 4  VS1_CHG,  bit 5 VS2_CHG
	void setCardPresent(bool present) {
		if (cardPresent == present) return;
		cardPresent = present;
		// CD1/CD2 both transitioned. Latch both change bits.
		interruptStatus |= 0x0C;
	}

	// Driven from cfCard.irqAsserted() on each tick. Tracks the CF
	// IREQ# line, which on real CLPS7600 hardware routes through the
	// chip's RDY input. When the line transitions, bit 10 of Interrupt
	// Status (RDY_CHG) is latched, and bit 10 (PCM_RDY) of the input
	// level register tracks the live state.
	void setCardIreq(bool asserted) {
		if (cfIreqAsserted == asserted) return;
		cfIreqAsserted = asserted;
		interruptStatus |= 0x400;  // RDY change latched
	}
	bool cfIreq() const { return cfIreqAsserted; }

	// True when interruptStatus & interruptMask is nonzero — i.e. when
	// the chip's IRQ output line is high. Owner uses this to drive
	// the CL-PS7111 SoC's EINT3 input.
	bool irqAsserted() const {
		return (interruptStatus & interruptMask) != 0;
	}

private:
	bool cfIreqAsserted = false;
};

