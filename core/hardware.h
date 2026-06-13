// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2019 Ash Wolf (WindEmu); modifications (c) 2024-2026 Joe Haines.
// Subject to the Mozilla Public License v2.0 (http://mozilla.org/MPL/2.0/).

#pragma once
#include "arm710.h"
#include <deque>
#include <vector>
#include <cstddef>

struct Timer {
	ARM710 *cpu;

	enum {
		MODE_512KHZ = 1<<3,
		PERIODIC = 1<<6,
		ENABLED = 1<<7
	};
    int64_t nextTickAt;
	uint8_t config;
	uint32_t interval;
	int32_t value;
	int clockSpeed;

	int tickInterval() const {
		return (config & MODE_512KHZ) ? (clockSpeed / 512000) : (clockSpeed / 2000);
	}
	void load(uint32_t lval) {
		interval = lval;
		value = lval;
	}
	void setConfig(uint8_t cval) {
		nextTickAt -= tickInterval();
		config = cval;
		nextTickAt += tickInterval();
	}
    bool tick(int64_t cycles) {
		if (cycles >= nextTickAt) {
			nextTickAt += tickInterval();

			if (config & ENABLED) {
				--value;
				if (value <= 0) {
					// Periodic mode reloads from the programmed interval;
					// free-running mode wraps at 16-bit like real hardware,
					// so the counter continues from 0xFFFF and will fire
					// TCxOI again ~32 s later even if the ROM never
					// programmed the timer.
					if (config & PERIODIC)
						value = interval;
					else
						value = 0xFFFF;
					return true;
				}
			}
		}
		return false;
	}
	void dump() {
		if (!cpu) return;
		cpu->log("enabled=%s periodic=%s interval=%d value=%d",
			(config & ENABLED) ? "true" : "false",
			(config & PERIODIC) ? "true" : "false",
			interval, value
		);
	}
};

enum UartRegs {
	UART0DATA = 0x600,
	UART0FCR = 0x604,
	UART0LCR = 0x608,
	UART0CON = 0x60C,
	UART0FLG = 0x610,
	UART0INT = 0x614,
	UART0INTM = 0x618,
	UART0INTR = 0x61C,
	UART0TEST1 = 0x620,
	UART0TEST2 = 0x624,
	UART0TEST3 = 0x628,
	UART1DATA = 0x700,
	UART1FCR = 0x704,
	UART1LCR = 0x708,
	UART1CON = 0x70C,
	UART1FLG = 0x710,
	UART1INT = 0x714,
	UART1INTM = 0x718,
	UART1INTR = 0x71C,
	UART1TEST1 = 0x720,
	UART1TEST2 = 0x724,
	UART1TEST3 = 0x728,
};

struct UART {
	ARM710 *cpu;

	enum {
		IntRx = 1,
		IntTx = 2,
		IntModemStatus = 4,
		PortCtrlEnable = 1,
		PortCtrlSirEnable = 2,
		PortCtrlIrdaTx = 4,
		FrameCtrlBreak = 1,
		FrameCtrlParityEnable = 2,
		FrameCtrlEvenParity = 4,
		FrameCtrlExtraStopBit = 8,
		FrameCtrlUFifoEn = 0x10,
		FrameCtrlWrdLenMask = 0x60,
		FrameCtrlWlen5 = 0,
		FrameCtrlWlen6 = 0x20,
		FrameCtrlWlen7 = 0x40,
		FrameCtrlWlen8 = 0x60,
		RecvFrameError = 0x100,
		RecvParityError = 0x200,
		RecvOverrunError = 0x400,
		FlagClearToSend = 1,
		FlagDataSetReady = 2,
		FlagDataCarrierDetect = 4,
		FlagBusy = 8,
		FlagReceiveFifoEmpty = 0x10,
		FlagTransmitFifoFull = 0x20
	};
	uint8_t portControl = 0;
	uint8_t frameControl = 0;
	uint8_t interrupts = 0, interruptMask = 0;

	// Host-bridge buffers. When a host (browser side) is attached, bytes the
	// CPU writes to UART_DATA go into txQueue (drained by host polls), and
	// bytes the host pushes go into rxFifo (popped by CPU UART_DATA reads).
	// While unattached, the original stub behaviour stands so devices that
	// boot through a UART poll (e.g. 5mx Pro bootloader DSR check) aren't
	// disturbed.
	bool hostAttached = false;
	std::deque<uint8_t> rxFifo;
	std::vector<uint8_t> txQueue;
	// Soft cap on outstanding bytes the CPU has written but the host hasn't
	// drained. PLP at 115200 with a 50 Hz host poll only needs ~290 B/frame,
	// so 4 KiB gives plenty of headroom without unbounded growth.
	static constexpr size_t kTxQueueCap = 4096;
	// Soft cap on bytes the host has pushed but the CPU hasn't consumed.
	// EPOC's serial driver typically drains the FIFO inside the ISR, so
	// 4 KiB is similarly comfortable.
	static constexpr size_t kRxFifoCap = 4096;

	// UART0DATA = 0x600, byte write, long read
	// UART0FCR = 0x604, long
	// UART0LCR = 0x608, long
	// UART0CON = 0x60C, byte
	// UART0FLG = 0x610, byte
	// UART0INT = 0x614, long write, byte read
	// UART0INTM = 0x618, byte
	// UART0INTR = 0x61C, byte
	// UART0TEST1 = 0x620,
	// UART0TEST2 = 0x624,
	// UART0TEST3 = 0x628,
	uint32_t readReg8(uint32_t reg) {
		// UART0DATA
		if (reg == (UART0DATA & 0xFF)) {
			return popRxByte();
		} else if (reg == (UART0CON & 0xFF)) {
			return portControl;
		} else if (reg == (UART0FLG & 0xFF)) {
			return computeFlags();
		// UART0INT?
		} else if (reg == (UART0INT & 0xFF)) {
			return interrupts;
		// UART0INTM?
		} else if (reg == (UART0INTM & 0xFF)) {
			return interruptMask;
		// UART0INTR?
		} else if (reg == (UART0INTR & 0xFF)) {
			return interrupts & interruptMask;
		} else {
			if (cpu) cpu->log("unhandled 8bit uart read %x at pc=%08x lr=%08x", reg, cpu->getGPR(15), cpu->getGPR(14));
			return 0xFF;
		}
	}
	uint32_t readReg32(uint32_t reg) {
		// UART0DATA
		if (reg == (UART0DATA & 0xFF)) {
			return popRxByte();
		} else if (reg == (UART0FCR & 0xFF)) {
			return frameControl;
		// UART0LCR
		} else if (reg == (UART0FLG & 0xFF)) {
			return computeFlags();
		} else {
			if (cpu) cpu->log("unhandled 32bit uart read %x at pc=%08x lr=%08x", reg, cpu->getGPR(15), cpu->getGPR(14));
			return 0xFFFFFFFF;
		}
	}
	void writeReg8(uint32_t reg, uint8_t value) {
		// UART0DATA
		if (reg == (UART0DATA & 0xFF)) {
			pushTxByte(value);
		} else if (reg == (UART0CON & 0xFF)) {
			portControl = value;
			if (cpu) cpu->log("portcon updated: enable=%d sirenable=%d irdatx=%d", value&1, value&2, value&4);
		} else if (reg == (UART0INTM & 0xFF)) {
			interruptMask = value;
			if (cpu) cpu->log("uart interruptmask updated: %d", value);
		} else if (reg == (UART0INTR & 0xFF)) {
			// Write-1-to-clear interrupt acknowledge.
			interrupts &= ~value;
		} else {
			if (cpu) cpu->log("unhandled 8bit uart write %x value %02x at pc=%08x lr=%08x", reg, value, cpu->getGPR(15), cpu->getGPR(14));
		}
	}
	void writeReg32(uint32_t reg, uint32_t value) {
		// UART0DATA (32-bit data writes happen on some EPOC drivers)
		if (reg == (UART0DATA & 0xFF)) {
			pushTxByte((uint8_t)value);
		} else if (reg == (UART0FCR & 0xFF)) {
			frameControl = value;
			if (cpu) cpu->log("frameControl updated: break=%d parityEn=%d evenParity=%d extraStop=%d ufifoEn=%d wrdLen=%d",
				value&1,
				value&2,
				value&4,
				value&8,
				value&0x10,
				((value&0x60)>>5)+5);
		} else if (reg == (UART0LCR & 0xFF)) {
			if (cpu) cpu->log("** uart writing lcr %x **", value);
		} else if (reg == (UART0INT & 0xFF)) {
			if (cpu) cpu->log("uart interrupts %x -> %x", interrupts, value);
			interrupts = value;
		} else {
			if (cpu) cpu->log("unhandled 32bit uart write %x value %08x at pc=%08x lr=%08x", reg, value, cpu->getGPR(15), cpu->getGPR(14));
		}
	}

	// ── Host bridge helpers ──────────────────────────────────────────────
	// Returns true when the UART has at least one masked interrupt asserted —
	// caller routes this onto the SoC's pendingInterrupts UARTn bit.
	bool wantsIrq() const {
		return (interrupts & interruptMask) != 0;
	}

	// Push bytes the host has just sent into the device-side RX FIFO. Drops
	// overflow silently (rare; only matters if the CPU is wedged). Asserts
	// IntRx so an EPOC serial driver will pick up the bytes.
	size_t pushRxFromHost(const uint8_t *data, size_t len) {
		size_t accepted = 0;
		while (accepted < len && rxFifo.size() < kRxFifoCap) {
			rxFifo.push_back(data[accepted++]);
		}
		if (!rxFifo.empty()) interrupts |= IntRx;
		return accepted;
	}

	// Drain bytes the CPU has written but the host hasn't yet read. Returns
	// the number of bytes actually copied. After a successful drain the TX
	// FIFO is empty, which on real hardware deasserts IntTx; we mirror that
	// here so the driver's interrupt-driven pattern works (write byte → IRQ
	// fires when TX completes → driver writes next byte).
	size_t drainTxToHost(uint8_t *dst, size_t cap) {
		size_t n = txQueue.size();
		if (n > cap) n = cap;
		for (size_t i = 0; i < n; ++i) dst[i] = txQueue[i];
		txQueue.erase(txQueue.begin(), txQueue.begin() + (std::ptrdiff_t)n);
		if (txQueue.empty()) interrupts &= ~IntTx;
		return n;
	}

	size_t txQueuedBytes() const { return txQueue.size(); }
	size_t rxFifoBytes()   const { return rxFifo.size(); }

	// Enqueue a byte the CPU has written to the UART data register into the
	// host-side TX queue. Public so CLPS7111's serial bridge (a separate
	// translation unit) can call it directly from its UARTDR write handler —
	// Windermere reaches it via the UART::writeReg* helpers, but CLPS7111
	// dispatches its own register decode and needs the entry point exposed.
	void pushTxByte(uint8_t value) {
		if (hostAttached) {
			if (txQueue.size() < kTxQueueCap) {
				txQueue.push_back(value);
			}
			// On real hardware IntTx fires when the byte completes the
			// shift register. Our TX is queue-only, so latch IntTx now;
			// drainTxToHost clears it once the host has consumed every
			// queued byte. The CPU's typical pattern (write byte → poll
			// FLG.BUSY → write next) still works because we never
			// advertise FlagBusy.
			interrupts |= IntTx;
		}
		// When not attached we silently drop TX, matching the original
		// stub behaviour.
	}

	// True iff there is RX data the CPU has not yet consumed — drives the
	// CLPS7111 SYSFLG1 URXFE bit (clear = data available).
	bool rxHasData() const { return !rxFifo.empty(); }

	// Pop one RX byte for the CPU (deasserts IntRx when the FIFO drains).
	// Public for the same reason as pushTxByte.
	uint8_t popRxByte() {
		if (rxFifo.empty()) {
			interrupts &= ~IntRx;
			return 0xFF;
		}
		uint8_t v = rxFifo.front();
		rxFifo.pop_front();
		if (rxFifo.empty()) interrupts &= ~IntRx;
		// else leave IntRx asserted so the driver keeps draining.
		return v;
	}

private:
	uint8_t computeFlags() const {
		uint8_t flags = 0;
		if (rxFifo.empty())            flags |= FlagReceiveFifoEmpty;
		if (txQueue.size() >= kTxQueueCap) flags |= FlagTransmitFifoFull;
		if (hostAttached) {
			// Pretend the cable's other end is asserting all modem-status
			// lines so the EPOC driver sees a live link.
			flags |= FlagClearToSend | FlagDataSetReady | FlagDataCarrierDetect;
		}
		return flags;
	}

};
