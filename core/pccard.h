// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include "vcfcard.h"
#include <cstdint>

// A 16-bit PC Card (or CompactFlash card) as a socket controller sees it:
// three address spaces — attribute memory (the CIS and the configuration
// registers), common memory and I/O — an interrupt request line, and the
// RESET pin. The socket owns the card-detect and READY/IREQ# wiring; the
// card only says whether it is asking for an interrupt.
//
// Offsets are the card's own address lines (A0-A25); sizes are 1, 2 or 4
// bytes as the host bus presents them (a 32-bit access on a 16-bit socket is
// two 16-bit cycles, which the card sees as one call with size 4).
class PcCard {
public:
	virtual ~PcCard() = default;
	virtual uint32_t attrRead(uint32_t off, int size) = 0;
	virtual void attrWrite(uint32_t off, int size, uint32_t v) = 0;
	virtual uint32_t memRead(uint32_t off, int size) { (void)off; (void)size; return size == 1 ? 0xff : 0xffff; }
	virtual void memWrite(uint32_t off, int size, uint32_t v) { (void)off; (void)size; (void)v; }
	virtual uint32_t ioRead(uint32_t off, int size) = 0;
	virtual void ioWrite(uint32_t off, int size, uint32_t v) = 0;
	// IREQ# asserted (the card wants an interrupt), in I/O mode.
	virtual bool ireq() const = 0;
	// The socket's RESET pin was pulsed.
	virtual void reset() = 0;
	// Advance card-internal time to `now` (guest cycles); returns the cycle
	// of the card's next event, UINT64_MAX for none. Only the board's event
	// loop calls it, never from inside a register access.
	virtual uint64_t tick(uint64_t now) { (void)now; return UINT64_MAX; }
	// The cycle of the card's next event as things stand (no side effects).
	virtual uint64_t nextEvent() const { return UINT64_MAX; }
};

// A CompactFlash storage card in "contiguous I/O" mode: the shared VCFCard
// ATA model (CIS, configuration registers, task file) plus what a Linux
// IDE driver adds on top of EPOC's — the device-control register's nIEN bit
// gating the interrupt, and SRST.
class CfAtaCard final : public PcCard {
public:
	VCFCard &disk() { return disk_; }
	const VCFCard &disk() const { return disk_; }

	uint32_t attrRead(uint32_t off, int size) override {
		uint32_t v = disk_.readAttributeByte(off & ~1u);
		return size == 1 ? v : (v | 0xff00u | (size == 4 ? 0xffff0000u : 0));
	}
	void attrWrite(uint32_t off, int size, uint32_t v) override {
		(void)size;
		disk_.writeAttributeByte(off & ~1u, (uint8_t)v);
	}
	uint32_t ioRead(uint32_t off, int size) override {
		uint32_t r = off & 0xf;
		if (r == 0 && size >= 2) {
			uint32_t v = disk_.ataRead16();
			if (size == 4) v |= (uint32_t)disk_.ataRead16() << 16;
			return v;
		}
		if (size == 1) return readReg(r);
		// A 16-bit access to an even register reads it and its odd neighbour.
		return readReg(r) | ((uint32_t)readReg(r + 1) << 8);
	}
	void ioWrite(uint32_t off, int size, uint32_t v) override {
		uint32_t r = off & 0xf;
		if (r == 0 && size >= 2) {
			disk_.ataWrite16((uint16_t)v);
			if (size == 4) disk_.ataWrite16((uint16_t)(v >> 16));
			return;
		}
		writeReg(r, (uint8_t)v);
		if (size >= 2) writeReg(r + 1, (uint8_t)(v >> 8));
	}
	bool ireq() const override { return !nIEN_ && disk_.irqPendingRaw() && disk_.irqDelayRemaining() == 0; }
	void reset() override { disk_.resetAtaState(); nIEN_ = false; slave_ = false; }
	uint64_t tick(uint64_t now) override {
		if (now > last_) disk_.tickIrqDelay((int)std::min<uint64_t>(now - last_, 1u << 30));
		last_ = now;
		return nextEvent();
	}
	uint64_t nextEvent() const override {
		if (disk_.irqPendingRaw() && disk_.irqDelayRemaining() > 0) return last_ + (uint64_t)disk_.irqDelayRemaining();
		return UINT64_MAX;
	}

private:
	VCFCard disk_;
	bool nIEN_ = false;
	bool slave_ = false;                         // DEV bit: device 1, which is not there
	uint8_t driveReg_ = 0xa0;
	uint64_t last_ = 0;
	uint8_t readReg(uint32_t r) {
		// With device 1 selected nothing drives the bus but the register
		// latch: status reads as zero, so the probe finds no slave.
		if (slave_) return r == 6 ? driveReg_ : 0;
		return disk_.ataRead8(r);
	}
	void writeReg(uint32_t r, uint8_t v) {
		if (r == 0xe) {                          // device control
			nIEN_ = (v & 0x02) != 0;
			if (v & 0x04) { disk_.resetAtaState(); slave_ = false; }   // SRST
			return;
		}
		if (r == 6) { driveReg_ = v; slave_ = (v & 0x10) != 0; }
		if (slave_ && r == 7) return;            // a command for the absent device
		// RECALIBRATE, which the IDE driver issues after IDENTIFY, has nothing
		// to do on a flash card: complete it like the other no-data commands.
		if (r == 7 && (v & 0xf0) == 0x10) v = 0x91;
		disk_.ataWrite8(r, v);
	}
};
