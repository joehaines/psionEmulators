// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include "xscale.h"
#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

// Intel PXA255 application processor: the XScale core plus the on-chip
// peripherals a Linux 2.6 kernel drives — interrupt controller, OS timers,
// RTC, GPIO, the four UARTs, I2C (master and slave), SSP, DMA, and the
// clock / power / memory-controller register files. Everything off-chip
// (the static chip selects, the PCMCIA space, GPIO input levels, I2C
// devices) belongs to the board and is reached through Pxa255::Board.
//
// Time is counted in core cycles. One instruction is one cycle, and the
// board picks how many of those make a second (cpuHz); the OS timer's
// 3.6864 MHz and the RTC's 1 Hz are derived from it, so the guest's sense
// of time is consistent with how fast it runs.

namespace PXA255 {

// A device on the I2C bus that the PXA255 addresses as master.
class I2cDevice {
public:
	virtual ~I2cDevice() = default;
	virtual bool i2cStart(bool read) = 0;          // address phase; return ACK
	virtual bool i2cWrite(uint8_t byte) = 0;       // return ACK
	virtual uint8_t i2cRead() = 0;
	virtual void i2cStop() = 0;
};

class Board {
public:
	virtual ~Board() = default;
	// Static chip selects nCS0..nCS5, each 64 MB at cs * 0x04000000.
	virtual bool csRead(int cs, uint32_t off, int size, uint32_t &out) = 0;
	virtual bool csWrite(int cs, uint32_t off, int size, uint32_t v) = 0;
	// PCMCIA / CF sockets, 0x20000000-0x3fffffff.
	virtual bool pcmciaRead(uint32_t pa, int size, uint32_t &out) { (void)pa; (void)size; out = ~0u; return true; }
	virtual bool pcmciaWrite(uint32_t pa, int size, uint32_t v) { (void)pa; (void)size; (void)v; return true; }
	// Level the board drives onto a GPIO pin configured as an input.
	virtual bool gpioInput(int pin) { (void)pin; return false; }
	virtual void gpioOutputChanged(int pin, bool level) { (void)pin; (void)level; }
	virtual void uartTx(int uart, uint8_t byte) { (void)uart; (void)byte; }
	virtual I2cDevice *i2cDevice(uint8_t addr7) { (void)addr7; return nullptr; }
	// The core asked for sleep / deep sleep through CP14 PWRMODE.
	virtual void powerMode(uint32_t mode) { (void)mode; }
	// The primary AC97 codec (register file of 16-bit registers).
	virtual uint16_t ac97Read(uint8_t reg) { (void)reg; return 0xffff; }
	virtual void ac97Write(uint8_t reg, uint16_t v) { (void)reg; (void)v; }
	virtual void ac97ColdReset() {}
	// Next word of the AC97 modem-input FIFO (slot 5), read through MODR:
	// what a touchscreen codec streams in continuous mode.
	virtual uint16_t ac97ModemRead() { return 0; }
	// PCM playback: the frame rate the codec's front DAC is running at, and
	// each stereo frame as it leaves the PCM transmit FIFO (PCDR layout:
	// left in bits 15:0, right in 31:16).
	virtual uint32_t ac97PcmRate() { return 48000; }
	virtual void ac97PcmOut(uint32_t frame) { (void)frame; }
	// Board-side events: called with the current cycle; returns the cycle of
	// the next board event (UINT64_MAX for none).
	virtual uint64_t boardTick(uint64_t now) { (void)now; return UINT64_MAX; }
};

enum Uart { FFUART = 0, BTUART = 1, STUART = 2, HWUART = 3 };

class Soc : public XScale::Bus {
public:
	Soc(Board &board, uint32_t ramSize, uint32_t cpuHz);

	XScale::Cpu &cpu() { return cpu_; }
	const XScale::Cpu &cpu() const { return cpu_; }
	uint8_t *ram() { return ram_.data(); }
	const uint8_t *ram() const { return ram_.data(); }
	uint32_t ramSize() const { return (uint32_t)ram_.size(); }
	static constexpr uint32_t kRamBase = 0xA0000000u;

	void reset(uint32_t rtcSeconds);
	void runUntil(uint64_t cycle);
	uint64_t cycles() const { return now_ + cpu_.sliceDone(); }
	uint32_t cpuHz() const { return cpuHz_; }

	// Board-facing controls.
	void setGpioInput(int pin, bool level);         // input pins: drives edges
	// Pin set-up a bootloader leaves behind (direction and output level),
	// applied before the OS runs.
	void presetGpio(int pin, bool output, bool level);
	bool gpioLevel(int pin) const;
	void uartRx(int uart, const uint8_t *bytes, size_t n);
	size_t uartRxSpace(int uart) const;
	// Bytes an external I2C master (the board's power controller) writes to
	// the PXA255's own slave address. Queued and delivered whole, as one
	// transaction, when the unit is idle in slave mode.
	void i2cSlaveWrite(const uint8_t *bytes, size_t n);
	bool i2cSlaveBusy() const { return !slaveQueue_.empty() || slaveActive_; }
	// Ask runUntil to re-evaluate board events now (a board register write
	// scheduled something).
	void kick() { cpu_.stopRun(); }

	// XScale::Bus
	bool busRead(uint32_t pa, int size, uint32_t &out) override;
	bool busWrite(uint32_t pa, int size, uint32_t val) override;
	void onPowerMode(uint32_t v) override { board_.powerMode(v); }

	// Diagnostics.
	void uartDebug(int u, char *buf, size_t n) const {
		const UartState &s = uart_[u];
		snprintf(buf, n, "ier=%02x lcr=%02x fcr=%02x thrInt=%d txPending=%d txDoneAt=%llu now=%llu rx=%zu irq=%d",
		         s.ier, s.lcr, s.fcr, (int)s.thrInt, (int)s.txPending, (unsigned long long)s.txDoneAt,
		         (unsigned long long)cycles(), s.rx.size(), (int)uartIrq(u));
	}
	uint32_t icmr() const { return icmr_; }
	uint32_t icpr() const { return pending(); }
	uint64_t timerIrqs() const { return ostMatches_; }

private:
	Board &board_;
	XScale::Cpu cpu_;
	std::vector<uint8_t> ram_;
	uint32_t cpuHz_;
	uint64_t now_ = 0;

	// ── Interrupt controller ────────────────────────────────────────────
	uint32_t icmr_ = 0, iclr_ = 0, iccr_ = 0;
	uint32_t pending() const;
	void updateIrq();

	// ── OS timers ───────────────────────────────────────────────────────
	uint32_t osmr_[4] = {};
	uint32_t ossr_ = 0, oier_ = 0, ower_ = 0;
	uint32_t oscrBase_ = 0;          // OSCR value at oscrCycle_
	uint64_t oscrCycle_ = 0;
	int64_t  ostChecked_ = -1;       // ticks (since base) already compared
	uint64_t ostMatches_ = 0;
	uint64_t ticksAt(uint64_t cycle) const {
		return (cycle - oscrCycle_) * 3686400ull / cpuHz_;
	}
	uint64_t cycleOfTick(uint64_t tick) const {
		return oscrCycle_ + (tick * cpuHz_ + 3686399ull) / 3686400ull;
	}
	uint32_t oscr() const { return oscrBase_ + (uint32_t)ticksAt(cycles()); }
	void ostUpdate(uint64_t now);
	uint64_t ostNextEvent() const;

	// ── RTC ─────────────────────────────────────────────────────────────
	uint32_t rcnrBase_ = 0;          // seconds at rtcCycle_
	uint64_t rtcCycle_ = 0;
	uint32_t rtar_ = 0, rtsr_ = 0, rttr_ = 0x7fff;
	uint32_t rtcSeen_ = 0;           // RCNR last evaluated (alarm / HZ)
	uint32_t rcnr() const { return rcnrBase_ + (uint32_t)((cycles() - rtcCycle_) / cpuHz_); }
	void rtcUpdate(uint64_t now);

	// ── GPIO ────────────────────────────────────────────────────────────
	uint32_t gpdr_[3] = {}, gpOut_[3] = {}, gpIn_[3] = {};
	uint32_t grer_[3] = {}, gfer_[3] = {}, gedr_[3] = {}, gafr_[6] = {};
	uint32_t gplr(int bank) const { return (gpOut_[bank] & gpdr_[bank]) | (gpIn_[bank] & ~gpdr_[bank]); }
	void gpioEdges(int bank, uint32_t before, uint32_t after);

	// ── UARTs ───────────────────────────────────────────────────────────
	struct UartState {
		uint8_t ier = 0, lcr = 0, mcr = 0, spr = 0, dll = 0, dlh = 0, fcr = 0, isr = 0;
		std::deque<uint8_t> rx;
		bool thrInt = false;
		uint64_t txDoneAt = 0;       // the last queued character finishes sending
		bool txPending = false;      // txDoneAt not yet reached (raise THRE then)
	};
	UartState uart_[4];
	uint32_t uartRead(int u, uint32_t reg);
	void uartWrite(int u, uint32_t reg, uint32_t v);
	bool uartIrq(int u) const;
	uint64_t uartCharCycles(const UartState &s) const;
	void uartUpdate(uint64_t now);

	// ── I2C ─────────────────────────────────────────────────────────────
	uint32_t icr_ = 0, isr_ = 0, idbr_ = 0, isar_ = 0;
	I2cDevice *i2cTarget_ = nullptr;
	bool i2cMasterRead_ = false, i2cMasterActive_ = false;
	uint64_t i2cDoneAt_ = UINT64_MAX;  // pending master byte completes
	uint32_t i2cPendingIsr_ = 0;
	std::deque<std::vector<uint8_t>> slaveQueue_;
	std::vector<uint8_t> slaveMsg_;
	size_t slavePos_ = 0;
	bool slaveActive_ = false;
	uint64_t slaveNextAt_ = UINT64_MAX;
	void i2cWriteIcr(uint32_t v);
	void i2cUpdate(uint64_t now);
	bool i2cIrq() const;

	// ── MMC controller ─────────────────────────────────────────────────
	// No card is wired to it on this board model: every command runs, finds
	// no response, and completes with TIME_OUT_RESPONSE + END_CMD_RES, which
	// is how the driver learns the slot is empty.
	uint32_t mmcStat_ = 0, mmcIreg_ = 0x10, mmcImask_ = 0x7f;
	uint32_t mmcRegs_[16] = {};
	bool mmcCmdArmed_ = false;
	uint64_t mmcDoneAt_ = UINT64_MAX;
	bool mmcIrq() const { return (mmcIreg_ & ~mmcImask_ & 0x7f) != 0; }

	// ── SSP ─────────────────────────────────────────────────────────────
	uint32_t sscr0_ = 0, sscr1_ = 0, ssitr_ = 0;

	// ── DMA ─────────────────────────────────────────────────────────────
	uint32_t dcsr_[16] = {}, ddadr_[16] = {}, dsadr_[16] = {}, dtadr_[16] = {}, dcmd_[16] = {};
	uint32_t drcmr_[40] = {};
	uint32_t dint() const;
	void dmaWriteDcsr(int ch, uint32_t v);
	void dmaRun(int ch);
	bool dmaAdvance(int ch);         // current descriptor done: next one, or stop
	bool physRead32(uint32_t pa, uint32_t &v);
	bool physCopy(uint32_t dst, uint32_t src, uint32_t len);

	// ── AC97 controller ─────────────────────────────────────────────────
	uint32_t gcr_ = 0, gsr_ = 0, car_ = 0;
	uint32_t acRegs_[0x48] = {};     // POCR..MODR control registers (0x000-0x11c)
	uint16_t acLastData_ = 0;        // data latched by the previous read cycle
	uint64_t acDoneAt_ = UINT64_MAX; // pending codec access completes
	uint32_t acDoneBits_ = 0;        // SDONE and/or CDONE to raise then
	uint64_t acReadyAt_ = UINT64_MAX;// codec-ready after a reset
	// PCM out: the DMA channel DRCMR12 maps drains into PCDR one frame per
	// codec sample period, as the AC-link's slot 3/4 requests pace it.
	uint64_t pcmLast_ = 0;           // cycle up to which frames were taken
	uint64_t pcmFrac_ = 0;           // fractional frames, in units of 1/cpuHz
	void pcmUpdate(uint64_t now);
	uint64_t pcmNextEvent() const;
	int pcmChannel() const;          // running channel feeding PCDR, or -1
	uint32_t ac97Read(uint32_t off);
	void ac97Write(uint32_t off, uint32_t v);
	void ac97Update(uint64_t now);
	bool ac97Irq() const;

	// ── Clocks, power, memory controller, LCD: plain register files ─────
	uint32_t cccr_ = 0x121, cken_ = 0x17def, oscc_ = 0x3;
	uint32_t pwr_[16] = {};          // 0x40F00000 + 4*i
	uint32_t memc_[32] = {};         // 0x48000000 + 4*i
	uint32_t lcd_[64] = {};          // 0x44000000 + 4*i (unused panel controller)
	uint32_t misc_[0x100] = {};      // catch-all for quiet peripherals

	uint64_t nextEvent() const;
	void processEvents(uint64_t now);
};

} // namespace PXA255
