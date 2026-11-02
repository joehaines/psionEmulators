// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "pxa255.h"
#include <algorithm>
#include <cstdio>
#include <numeric>

namespace PXA255 {

namespace {

// Interrupt numbers (ICPR bit positions).
enum : int {
	IRQ_HWUART = 7, IRQ_GPIO0 = 8, IRQ_GPIO1 = 9, IRQ_GPIO_X = 10, IRQ_AC97 = 14,
	IRQ_I2C = 18, IRQ_STUART = 20, IRQ_MMC = 23, IRQ_BTUART = 21, IRQ_FFUART = 22, IRQ_DMA = 25,
	IRQ_OST0 = 26, IRQ_RTC_HZ = 30, IRQ_RTC_AL = 31,
};

// I2C ICR / ISR bits.
enum : uint32_t {
	ICR_START = 1u << 0, ICR_STOP = 1u << 1, ICR_ACKNAK = 1u << 2, ICR_TB = 1u << 3,
	ICR_MA = 1u << 4, ICR_SCLE = 1u << 5, ICR_IUE = 1u << 6, ICR_ITEIE = 1u << 8,
	ICR_IRFIE = 1u << 9, ICR_BEIE = 1u << 10, ICR_SSDIE = 1u << 11, ICR_ALDIE = 1u << 12,
	ICR_SADIE = 1u << 13, ICR_UR = 1u << 14,
	ISR_RWM = 1u << 0, ISR_ACKNAK = 1u << 1, ISR_UB = 1u << 2, ISR_IBB = 1u << 3,
	ISR_SSD = 1u << 4, ISR_ALD = 1u << 5, ISR_ITE = 1u << 6, ISR_IRF = 1u << 7,
	ISR_GCAD = 1u << 8, ISR_SAD = 1u << 9, ISR_BED = 1u << 10,
};

// DMA DCSR / DCMD bits.
enum : uint32_t {
	DCSR_RUN = 1u << 31, DCSR_NODESC = 1u << 30, DCSR_STOPIRQEN = 1u << 29,
	DCSR_STOPSTATE = 1u << 3, DCSR_ENDINTR = 1u << 2, DCSR_STARTINTR = 1u << 1, DCSR_BUSERR = 1u << 0,
	DCMD_INCSRC = 1u << 31, DCMD_INCTRG = 1u << 30, DCMD_FLOWSRC = 1u << 29, DCMD_FLOWTRG = 1u << 28,
	DCMD_STARTIRQEN = 1u << 22, DCMD_ENDIRQEN = 1u << 21,
};

// AC97 GCR / GSR bits.
enum : uint32_t {
	GCR_GIE = 1u << 0, GCR_COLD_RST = 1u << 1, GCR_WARM_RST = 1u << 2, GCR_ACLINK_OFF = 1u << 3,
	GCR_PRIRDY_IEN = 1u << 8, GCR_SDONE_IE = 1u << 18, GCR_CDONE_IE = 1u << 19,
	GSR_GSCI = 1u << 0, GSR_PCR = 1u << 8, GSR_RDCS = 1u << 15, GSR_SDONE = 1u << 18, GSR_CDONE = 1u << 19,
};

constexpr uint64_t kMaxSlice = 8192;

} // namespace

Soc::Soc(Board &board, uint32_t ramSize, uint32_t cpuHz)
    : board_(board), cpu_(*this), ram_(ramSize), cpuHz_(cpuHz) {
	cpu_.setRam(ram_.data(), kRamBase, ramSize);
}

void Soc::reset(uint32_t rtcSeconds) {
	cpu_.reset();
	now_ = 0;
	icmr_ = iclr_ = iccr_ = 0;
	for (auto &m : osmr_) m = 0;
	ossr_ = oier_ = ower_ = 0;
	oscrBase_ = 0; oscrCycle_ = 0; ostChecked_ = -1;
	rcnrBase_ = rtcSeconds; rtcCycle_ = 0; rtcSeen_ = rtcSeconds;
	rtar_ = 0; rtsr_ = 0; rttr_ = 0x7fff;
	for (int b = 0; b < 3; b++) {
		gpdr_[b] = gpOut_[b] = grer_[b] = gfer_[b] = gedr_[b] = 0;
		uint32_t in = 0;
		for (int i = 0; i < 32 && b * 32 + i < 85; i++)
			if (board_.gpioInput(b * 32 + i)) in |= 1u << i;
		gpIn_[b] = in;
	}
	for (auto &g : gafr_) g = 0;
	for (auto &u : uart_) u = UartState();
	icr_ = isr_ = idbr_ = isar_ = 0;
	i2cTarget_ = nullptr; i2cMasterActive_ = false; i2cDoneAt_ = UINT64_MAX;
	slaveQueue_.clear(); slaveActive_ = false; slaveNextAt_ = UINT64_MAX;
	sscr0_ = sscr1_ = ssitr_ = 0;
	mmcStat_ = 0; mmcIreg_ = 0x10; mmcImask_ = 0x7f; mmcCmdArmed_ = false; mmcDoneAt_ = UINT64_MAX;
	for (auto &r : mmcRegs_) r = 0;
	for (int i = 0; i < 16; i++) { dcsr_[i] = DCSR_STOPSTATE; ddadr_[i] = dsadr_[i] = dtadr_[i] = dcmd_[i] = 0; }
	for (auto &d : drcmr_) d = 0;
	gcr_ = gsr_ = car_ = 0;
	for (auto &r : acRegs_) r = 0;
	acDoneAt_ = acReadyAt_ = UINT64_MAX; acDoneBits_ = 0;
	pcmLast_ = 0; pcmFrac_ = 0;
	cccr_ = 0x121; cken_ = 0x17def; oscc_ = 0x3;
	for (auto &p : pwr_) p = 0;
	pwr_[0x30 / 4] = 1;               // RCSR: hardware reset
	for (auto &m : memc_) m = 0;
	memc_[0x00 / 4] = 0x00001ac9;     // MDCNFG
	memc_[0x04 / 4] = 0x0001c018;     // MDREFR
	memc_[0x08 / 4] = 0x7ff07ff0;     // MSC0
	memc_[0x0c / 4] = 0x7ff07ff0;     // MSC1
	memc_[0x10 / 4] = 0x7ff07ff0;     // MSC2
	memc_[0x1c / 4] = 0x40044004;     // SXCNFG
	for (auto &l : lcd_) l = 0;
	for (auto &m : misc_) m = 0;
	updateIrq();
}

// ── Scheduling ──────────────────────────────────────────────────────────

uint64_t Soc::nextEvent() const {
	uint64_t ev = ostNextEvent();
	if (rtsr_ & 0xc) {                    // HZE or ALE: next second boundary
		uint64_t s = (now_ - rtcCycle_) / cpuHz_ + 1;
		ev = std::min(ev, rtcCycle_ + s * cpuHz_);
	}
	ev = std::min(ev, i2cDoneAt_);
	ev = std::min(ev, slaveNextAt_);
	ev = std::min(ev, acDoneAt_);
	ev = std::min(ev, acReadyAt_);
	ev = std::min(ev, mmcDoneAt_);
	ev = std::min(ev, pcmNextEvent());
	for (const auto &s : uart_)
		if (s.txPending) ev = std::min(ev, s.txDoneAt);
	return ev;
}

void Soc::processEvents(uint64_t now) {
	ostUpdate(now);
	rtcUpdate(now);
	i2cUpdate(now);
	ac97Update(now);
	pcmUpdate(now);
	uartUpdate(now);
	if (now >= mmcDoneAt_) {
		mmcDoneAt_ = UINT64_MAX;
		mmcStat_ |= 0x2002;                         // TIME_OUT_RESPONSE, END_CMD_RES
		mmcIreg_ |= 0x04;                           // END_CMD_RES
	}
	updateIrq();
}

void Soc::runUntil(uint64_t target) {
	while (now_ < target) {
		processEvents(now_);
		uint64_t ev = std::min(nextEvent(), board_.boardTick(now_));
		updateIrq();
		ev = std::min(ev, target);
		if (ev <= now_) ev = now_ + 1;
		uint64_t slice = std::min<uint64_t>(ev - now_, kMaxSlice);
		if (cpu_.isIdle()) {
			now_ += slice;
			continue;
		}
		now_ += cpu_.run((uint32_t)slice);
	}
	processEvents(now_);
}

// ── Interrupt controller ────────────────────────────────────────────────

uint32_t Soc::pending() const {
	uint32_t p = 0;
	if (gedr_[0] & 1) p |= 1u << IRQ_GPIO0;
	if (gedr_[0] & 2) p |= 1u << IRQ_GPIO1;
	if ((gedr_[0] & ~3u) | gedr_[1] | (gedr_[2] & 0x1fffff)) p |= 1u << IRQ_GPIO_X;
	if (uartIrq(HWUART)) p |= 1u << IRQ_HWUART;
	if (ac97Irq()) p |= 1u << IRQ_AC97;
	if (i2cIrq()) p |= 1u << IRQ_I2C;
	if (uartIrq(STUART)) p |= 1u << IRQ_STUART;
	if (uartIrq(BTUART)) p |= 1u << IRQ_BTUART;
	if (uartIrq(FFUART)) p |= 1u << IRQ_FFUART;
	if (mmcIrq()) p |= 1u << IRQ_MMC;
	if (dint()) p |= 1u << IRQ_DMA;
	p |= (ossr_ & 0xf) << IRQ_OST0;
	if ((rtsr_ & 0xa) == 0xa) p |= 1u << IRQ_RTC_HZ;
	if ((rtsr_ & 0x5) == 0x5) p |= 1u << IRQ_RTC_AL;
	return p;
}

void Soc::updateIrq() {
	uint32_t ip = pending() & icmr_;
	cpu_.setIrqLine((ip & ~iclr_) != 0);
	cpu_.setFiqLine((ip & iclr_) != 0);
}

// ── OS timers ───────────────────────────────────────────────────────────

void Soc::ostUpdate(uint64_t now) {
	uint64_t t = ticksAt(now);
	if ((int64_t)t > ostChecked_) {
		for (int i = 0; i < 4; i++) {
			uint32_t next = oscrBase_ + (uint32_t)(ostChecked_ + 1);
			uint64_t k0 = (uint64_t)(ostChecked_ + 1) + (uint32_t)(osmr_[i] - next);
			if (k0 <= t && (oier_ & (1u << i))) {
				ossr_ |= 1u << i;
				ostMatches_++;
			}
		}
		ostChecked_ = (int64_t)t;
	}
	// Keep the tick arithmetic well inside 64 bits: rebase by whole periods.
	if (now - oscrCycle_ > (1ull << 36)) {
		uint64_t g = std::gcd<uint64_t>(3686400ull, cpuHz_);
		uint64_t pc = cpuHz_ / g, pt = 3686400ull / g;
		uint64_t n = (now - oscrCycle_) / pc;
		oscrCycle_ += n * pc;
		oscrBase_ += (uint32_t)(n * pt);
		ostChecked_ -= (int64_t)(n * pt);
	}
}

uint64_t Soc::ostNextEvent() const {
	uint64_t ev = UINT64_MAX;
	for (int i = 0; i < 4; i++) {
		if (!(oier_ & (1u << i))) continue;
		uint32_t next = oscrBase_ + (uint32_t)(ostChecked_ + 1);
		uint64_t k0 = (uint64_t)(ostChecked_ + 1) + (uint32_t)(osmr_[i] - next);
		ev = std::min(ev, cycleOfTick(k0));
	}
	return ev;
}

// ── RTC ─────────────────────────────────────────────────────────────────

void Soc::rtcUpdate(uint64_t now) {
	uint32_t cur = rcnrBase_ + (uint32_t)((now - rtcCycle_) / cpuHz_);
	while (rtcSeen_ != cur) {
		rtcSeen_++;
		if (rtsr_ & 0x8) rtsr_ |= 0x2;                 // HZ
		if ((rtsr_ & 0x4) && rtcSeen_ == rtar_) rtsr_ |= 0x1;   // AL
	}
}

// ── GPIO ────────────────────────────────────────────────────────────────

void Soc::gpioEdges(int bank, uint32_t before, uint32_t after) {
	uint32_t rise = ~before & after & grer_[bank];
	uint32_t fall = before & ~after & gfer_[bank];
	gedr_[bank] |= rise | fall;
}

void Soc::setGpioInput(int pin, bool level) {
	if (pin < 0 || pin >= 85) return;
	int b = pin >> 5;
	uint32_t bit = 1u << (pin & 31);
	uint32_t before = gplr(b);
	if (level) gpIn_[b] |= bit; else gpIn_[b] &= ~bit;
	gpioEdges(b, before, gplr(b));
	updateIrq();
	cpu_.stopRun();
}

void Soc::presetGpio(int pin, bool output, bool level) {
	if (pin < 0 || pin >= 85) return;
	int b = pin >> 5;
	uint32_t bit = 1u << (pin & 31);
	if (output) gpdr_[b] |= bit; else gpdr_[b] &= ~bit;
	if (level) gpOut_[b] |= bit; else gpOut_[b] &= ~bit;
	if (output) board_.gpioOutputChanged(pin, level);
}

bool Soc::gpioLevel(int pin) const {
	if (pin < 0 || pin >= 85) return false;
	return (gplr(pin >> 5) >> (pin & 31)) & 1;
}

// ── UARTs ───────────────────────────────────────────────────────────────

bool Soc::uartIrq(int u) const {
	const UartState &s = uart_[u];
	if (!(s.ier & 0x40)) return false;                 // UUE: unit enabled
	if ((s.ier & 0x01) && !s.rx.empty()) return true;
	if ((s.ier & 0x10) && !s.rx.empty()) return true;  // receiver timeout
	if ((s.ier & 0x02) && s.thrInt) return true;
	return false;
}

// One character (start, 8 data, stop) at the programmed divisor, from the
// 14.7456 MHz UART clock: 87 us at 115200 baud.
uint64_t Soc::uartCharCycles(const UartState &s) const {
	uint32_t div = s.dll | (s.dlh << 8);
	if (!div) div = 1;
	return std::max<uint64_t>(1, (uint64_t)10 * 16 * div * cpuHz_ / 14745600u);
}

void Soc::uartUpdate(uint64_t now) {
	for (auto &s : uart_) {
		if (s.txPending && now >= s.txDoneAt) {
			s.txPending = false;
			s.thrInt = true;                           // transmit FIFO drained
		}
	}
}

uint32_t Soc::uartRead(int u, uint32_t reg) {
	UartState &s = uart_[u];
	switch (reg) {
	case 0:
		if (s.lcr & 0x80) return s.dll;
		if (s.rx.empty()) return 0;
		{
			uint8_t b = s.rx.front();
			s.rx.pop_front();
			return b;
		}
	case 1: return (s.lcr & 0x80) ? s.dlh : s.ier;
	case 2: {
		uint32_t base = (s.fcr & 1) ? 0xc0 : 0;
		if ((s.ier & 0x41) == 0x41 && !s.rx.empty()) return base | 0x04;
		if ((s.ier & 0x50) == 0x50 && !s.rx.empty()) return base | 0x0c;
		if ((s.ier & 0x42) == 0x42 && s.thrInt) { s.thrInt = false; return base | 0x02; }
		return base | 0x01;
	}
	case 3: return s.lcr;
	case 4: return s.mcr;
	case 5: {
		// TDRQ while the 64-byte FIFO has room, TEMT once everything is out.
		uint64_t now = cycles();
		uint32_t lsr = s.rx.empty() ? 0u : 1u;
		uint64_t ch = uartCharCycles(s);
		if (now >= s.txDoneAt) lsr |= 0x60;
		else if (s.txDoneAt - now < 32 * ch) lsr |= 0x20;
		return lsr;
	}
	case 6: return 0xb0;                               // CTS, DSR, DCD
	case 7: return s.spr;
	case 8: return s.isr;
	default: return 0;
	}
}

void Soc::uartWrite(int u, uint32_t reg, uint32_t v) {
	UartState &s = uart_[u];
	switch (reg) {
	case 0:
		if (s.lcr & 0x80) { s.dll = (uint8_t)v; break; }
		if (s.ier & 0x40) board_.uartTx(u, (uint8_t)v);
		{
			uint64_t now = cycles();
			s.txDoneAt = std::max(s.txDoneAt, now) + uartCharCycles(s);
			s.txPending = true;
			s.thrInt = false;
			cpu_.stopRun();
		}
		break;
	case 1:
		if (s.lcr & 0x80) { s.dlh = (uint8_t)v; break; }
		if (!(s.ier & 0x02) && (v & 0x02) && cycles() >= s.txDoneAt) s.thrInt = true;
		s.ier = (uint8_t)v;
		break;
	case 2:
		s.fcr = (uint8_t)v;
		if (v & 2) s.rx.clear();
		break;
	case 3: s.lcr = (uint8_t)v; break;
	case 4: s.mcr = (uint8_t)v; break;
	case 7: s.spr = (uint8_t)v; break;
	case 8: s.isr = (uint8_t)v; break;
	default: break;
	}
}

void Soc::uartRx(int u, const uint8_t *bytes, size_t n) {
	if (u < 0 || u > 3) return;
	for (size_t i = 0; i < n; i++) uart_[u].rx.push_back(bytes[i]);
	updateIrq();
	cpu_.stopRun();
}

size_t Soc::uartRxSpace(int u) const {
	if (u < 0 || u > 3) return 0;
	return uart_[u].rx.size() >= 64 ? 0 : 64 - uart_[u].rx.size();
}

// ── I2C ─────────────────────────────────────────────────────────────────

bool Soc::i2cIrq() const {
	uint32_t en = 0;
	if (icr_ & ICR_ITEIE) en |= ISR_ITE;
	if (icr_ & ICR_IRFIE) en |= ISR_IRF;
	if (icr_ & ICR_BEIE) en |= ISR_BED;
	if (icr_ & ICR_SSDIE) en |= ISR_SSD;
	if (icr_ & ICR_ALDIE) en |= ISR_ALD;
	if (icr_ & ICR_SADIE) en |= ISR_SAD;
	return (isr_ & en) != 0;
}

void Soc::i2cWriteIcr(uint32_t v) {
	if (v & ICR_UR) {
		icr_ = v;
		isr_ = 0;
		if (i2cTarget_) i2cTarget_->i2cStop();
		i2cTarget_ = nullptr; i2cMasterActive_ = false; i2cDoneAt_ = UINT64_MAX;
		slaveActive_ = false; slaveNextAt_ = UINT64_MAX;
		return;
	}
	icr_ = v;
	if (!(icr_ & ICR_IUE)) return;
	if (v & ICR_MA) {
		if (i2cTarget_) i2cTarget_->i2cStop();
		i2cTarget_ = nullptr;
		i2cMasterActive_ = false;
		isr_ &= ~(ISR_UB | ISR_IBB);
		icr_ &= ~ICR_TB;
		return;
	}
	if (!(v & ICR_TB)) return;
	// One byte's worth of bus time (9 clocks at 100 kHz is 90 us; the model
	// uses ~20 us so a busy kernel is not held up by its PCON chatter).
	uint64_t delay = std::max<uint64_t>(cpuHz_ / 50000, 64);
	if (slaveActive_) {
		slaveNextAt_ = cycles() + delay;
		cpu_.stopRun();
		return;
	}
	if ((v & ICR_SCLE) && i2cDoneAt_ == UINT64_MAX) {
		i2cDoneAt_ = cycles() + delay;
		cpu_.stopRun();
	}
}

void Soc::i2cUpdate(uint64_t now) {
	if (now >= i2cDoneAt_) {
		i2cDoneAt_ = UINT64_MAX;
		uint32_t icr = icr_;
		if (icr & ICR_START) {
			uint8_t addr = (uint8_t)idbr_;
			if (i2cTarget_ && i2cMasterActive_) i2cTarget_->i2cStop();   // repeated start
			i2cTarget_ = board_.i2cDevice(addr >> 1);
			i2cMasterRead_ = addr & 1;
			isr_ |= ISR_UB | ISR_IBB;
			if (i2cMasterRead_) isr_ |= ISR_RWM; else isr_ &= ~ISR_RWM;
			bool ack = i2cTarget_ && i2cTarget_->i2cStart(i2cMasterRead_);
			isr_ |= ISR_ITE;
			if (!ack) {
				isr_ |= ISR_BED | ISR_ACKNAK;
				i2cTarget_ = nullptr;
				i2cMasterActive_ = false;
				isr_ &= ~(ISR_UB | ISR_IBB);
			} else {
				isr_ &= ~ISR_ACKNAK;
				i2cMasterActive_ = true;
			}
		} else if (i2cMasterActive_ && !i2cMasterRead_) {
			bool ack = i2cTarget_->i2cWrite((uint8_t)idbr_);
			isr_ |= ISR_ITE;
			if (!ack) isr_ |= ISR_BED | ISR_ACKNAK; else isr_ &= ~ISR_ACKNAK;
		} else if (i2cMasterActive_) {
			idbr_ = i2cTarget_->i2cRead();
			isr_ |= ISR_IRF;
		}
		if (icr & ICR_STOP) {
			if (i2cTarget_ && i2cMasterActive_) i2cTarget_->i2cStop();
			i2cTarget_ = nullptr;
			i2cMasterActive_ = false;
			isr_ &= ~(ISR_UB | ISR_IBB);
		}
		icr_ &= ~ICR_TB;
	}
	// Slave side: an external master (the power controller) writing to us.
	if (slaveActive_) {
		if (now >= slaveNextAt_) {
			slaveNextAt_ = UINT64_MAX;
			if (slavePos_ < slaveMsg_.size()) {
				idbr_ = slaveMsg_[slavePos_++];
				isr_ |= ISR_IRF;
			} else {
				isr_ |= ISR_SSD;
				isr_ &= ~(ISR_UB | ISR_IBB);
				slaveActive_ = false;
			}
			icr_ &= ~ICR_TB;
		}
	} else if (!slaveQueue_.empty() && (icr_ & ICR_IUE) && !(icr_ & ICR_SCLE) &&
	           !i2cMasterActive_ && i2cDoneAt_ == UINT64_MAX && !(isr_ & (ISR_SAD | ISR_SSD | ISR_IRF))) {
		slaveMsg_ = std::move(slaveQueue_.front());
		slaveQueue_.pop_front();
		slavePos_ = 0;
		slaveActive_ = true;
		slaveNextAt_ = UINT64_MAX;
		isr_ |= ISR_SAD | ISR_UB | ISR_IBB;
		isr_ &= ~ISR_RWM;                  // slave receive
	}
}

void Soc::i2cSlaveWrite(const uint8_t *bytes, size_t n) {
	slaveQueue_.emplace_back(bytes, bytes + n);
	cpu_.stopRun();
}

// ── AC97 ────────────────────────────────────────────────────────────────

bool Soc::ac97Irq() const {
	if ((gsr_ & GSR_SDONE) && (gcr_ & GCR_SDONE_IE)) return true;
	if ((gsr_ & GSR_CDONE) && (gcr_ & GCR_CDONE_IE)) return true;
	if ((gsr_ & GSR_GSCI) && (gcr_ & GCR_GIE)) return true;
	return false;
}

uint32_t Soc::ac97Read(uint32_t off) {
	if (off >= 0x200 && off < 0x300) {
		// Primary audio codec register: the read returns the data of the
		// previous cycle and starts a new one, which completes with SDONE.
		uint8_t reg = (uint8_t)((off - 0x200) >> 1);
		uint16_t v = acLastData_;
		acLastData_ = board_.ac97Read(reg);
		car_ |= 1;
		acDoneAt_ = cycles() + std::max<uint64_t>(cpuHz_ / 48000, 32);
		acDoneBits_ |= GSR_SDONE;
		cpu_.stopRun();
		return v;
	}
	if (off >= 0x300) return 0xffff;   // secondary / modem codecs: absent
	switch (off) {
	case 0x0c: return gcr_;
	case 0x1c: return gsr_;
	case 0x20: return car_;
	case 0x40: return 0;               // PCDR
	case 0x60: return 0;               // MCDR
	case 0x140: return board_.ac97ModemRead();   // MODR
	default:
		return off / 4 < 0x48 ? acRegs_[off / 4] : 0;
	}
}

void Soc::ac97Write(uint32_t off, uint32_t v) {
	if (off >= 0x200 && off < 0x300) {
		uint8_t reg = (uint8_t)((off - 0x200) >> 1);
		board_.ac97Write(reg, (uint16_t)v);
		car_ |= 1;
		acDoneAt_ = cycles() + std::max<uint64_t>(cpuHz_ / 48000, 32);
		acDoneBits_ |= GSR_CDONE;
		cpu_.stopRun();
		return;
	}
	if (off >= 0x300) return;
	switch (off) {
	case 0x0c: {
		uint32_t old = gcr_;
		gcr_ = v;
		bool coldRelease = !(old & GCR_COLD_RST) && (v & GCR_COLD_RST);
		bool warm = (v & GCR_WARM_RST) != 0;
		if (!(v & GCR_COLD_RST)) gsr_ &= ~GSR_PCR;     // reset asserted
		if (coldRelease) board_.ac97ColdReset();
		if (coldRelease || warm) {
			acReadyAt_ = cycles() + std::max<uint64_t>(cpuHz_ / 10000, 64);
			gcr_ &= ~GCR_WARM_RST;
			cpu_.stopRun();
		}
		if (v & GCR_ACLINK_OFF) gsr_ &= ~GSR_PCR;
		break;
	}
	case 0x1c:
		gsr_ &= ~(v & (GSR_SDONE | GSR_CDONE | GSR_RDCS | GSR_GSCI | (1u << 10) | (1u << 11)));
		break;
	case 0x20: car_ = v & 1; break;
	case 0x40: board_.ac97PcmOut(v); break;    // PCDR written by the CPU
	default:
		if (off / 4 < 0x48) acRegs_[off / 4] = v;
		break;
	}
}

void Soc::ac97Update(uint64_t now) {
	if (now >= acReadyAt_) {
		acReadyAt_ = UINT64_MAX;
		if (gcr_ & GCR_COLD_RST) gsr_ |= GSR_PCR;
	}
	if (now >= acDoneAt_) {
		acDoneAt_ = UINT64_MAX;
		gsr_ |= acDoneBits_;
		acDoneBits_ = 0;
		car_ &= ~1u;
	}
}

// ── DMA ─────────────────────────────────────────────────────────────────

uint32_t Soc::dint() const {
	uint32_t d = 0;
	for (int i = 0; i < 16; i++) {
		uint32_t c = dcsr_[i];
		if ((c & (DCSR_ENDINTR | DCSR_STARTINTR | DCSR_BUSERR)) ||
		    ((c & DCSR_STOPSTATE) && (c & DCSR_STOPIRQEN)))
			d |= 1u << i;
	}
	return d;
}

bool Soc::physRead32(uint32_t pa, uint32_t &v) {
	if (pa - kRamBase < ram_.size() - 3) { std::memcpy(&v, ram_.data() + (pa - kRamBase), 4); return true; }
	return busRead(pa, 4, v);
}

bool Soc::physCopy(uint32_t dst, uint32_t src, uint32_t len) {
	for (uint32_t i = 0; i < len; i++) {
		uint32_t b;
		if (src + i - kRamBase < ram_.size()) b = ram_[src + i - kRamBase];
		else if (!busRead(src + i, 1, b)) return false;
		if (dst + i - kRamBase < ram_.size()) ram_[dst + i - kRamBase] = (uint8_t)b;
		else if (!busWrite(dst + i, 1, b)) return false;
	}
	return true;
}

bool Soc::dmaAdvance(int ch) {
	// The current descriptor has moved all its bytes: raise its end
	// interrupt, then load the next descriptor or stop.
	if (dcmd_[ch] & DCMD_ENDIRQEN) dcsr_[ch] |= DCSR_ENDINTR;
	if ((dcsr_[ch] & DCSR_NODESC) || (ddadr_[ch] & 1)) { dcsr_[ch] &= ~DCSR_RUN; dcsr_[ch] |= DCSR_STOPSTATE; return false; }
	uint32_t d = ddadr_[ch] & ~0xfu, w[4];
	for (int i = 0; i < 4; i++)
		if (!physRead32(d + 4 * i, w[i])) { dcsr_[ch] |= DCSR_BUSERR | DCSR_STOPSTATE; dcsr_[ch] &= ~DCSR_RUN; return false; }
	ddadr_[ch] = w[0]; dsadr_[ch] = w[1]; dtadr_[ch] = w[2]; dcmd_[ch] = w[3];
	if (dcmd_[ch] & DCMD_STARTIRQEN) dcsr_[ch] |= DCSR_STARTINTR;
	return true;
}

void Soc::dmaRun(int ch) {
	// Memory-to-memory descriptors complete at once; peripheral-paced ones
	// (FLOWSRC / FLOWTRG) wait for their peripheral's requests — the AC97
	// PCM transmit FIFO is the only one that raises them (pcmUpdate).
	for (int guard = 0; guard < 256 && (dcsr_[ch] & DCSR_RUN); guard++) {
		if (dcmd_[ch] & (DCMD_FLOWSRC | DCMD_FLOWTRG)) return;
		uint32_t len = dcmd_[ch] & 0x1fff;
		if (len) {
			if (!physCopy(dtadr_[ch], dsadr_[ch], len)) { dcsr_[ch] |= DCSR_BUSERR; dcsr_[ch] &= ~DCSR_RUN; break; }
			if (dcmd_[ch] & DCMD_INCSRC) dsadr_[ch] += len;
			if (dcmd_[ch] & DCMD_INCTRG) dtadr_[ch] += len;
			dcmd_[ch] &= ~0x1fffu;
		}
		if (!dmaAdvance(ch)) break;
	}
	if (!(dcsr_[ch] & DCSR_RUN)) dcsr_[ch] |= DCSR_STOPSTATE;
}

// ── AC97 PCM out ────────────────────────────────────────────────────────

namespace { constexpr uint32_t kPcdrPhys = 0x40500040u; }

int Soc::pcmChannel() const {
	uint32_t m = drcmr_[12];                          // DRCMRTXPCDR
	if (!(m & 0x80)) return -1;                       // MAPVLD
	int ch = (int)(m & 0xf);
	return (dcsr_[ch] & DCSR_RUN) ? ch : -1;
}

void Soc::pcmUpdate(uint64_t now) {
	if (now <= pcmLast_) return;
	uint64_t elapsed = now - pcmLast_;
	pcmLast_ = now;
	int ch = pcmChannel();
	if (ch < 0 || !(gsr_ & GSR_PCR)) { pcmFrac_ = 0; return; }
	uint32_t rate = std::clamp<uint32_t>(board_.ac97PcmRate(), 4000, 48000);
	// Frames due: elapsed * rate / cpuHz, carrying the remainder.
	pcmFrac_ += elapsed * rate;
	uint64_t frames = pcmFrac_ / cpuHz_;
	pcmFrac_ %= cpuHz_;
	if (frames > 48000) frames = 48000;               // a long idle gap: don't burst a second's worth
	for (int guard = 0; frames && guard < 4096 && (dcsr_[ch] & DCSR_RUN); guard++) {
		if (!(dcmd_[ch] & DCMD_FLOWTRG) || dtadr_[ch] != kPcdrPhys) {
			// Fragment-marker descriptors and the like: memory-to-memory.
			uint32_t before = ddadr_[ch];
			dmaRun(ch);
			if (ddadr_[ch] == before && !(dcmd_[ch] & DCMD_FLOWTRG)) break;
			continue;
		}
		uint32_t len = dcmd_[ch] & 0x1fff;
		while (len >= 4 && frames) {
			uint32_t w;
			if (!physRead32(dsadr_[ch], w)) { dcsr_[ch] |= DCSR_BUSERR | DCSR_STOPSTATE; dcsr_[ch] &= ~DCSR_RUN; break; }
			board_.ac97PcmOut(w);
			if (dcmd_[ch] & DCMD_INCSRC) dsadr_[ch] += 4;
			len -= 4;
			frames--;
		}
		dcmd_[ch] = (dcmd_[ch] & ~0x1fffu) | len;
		if (len < 4 && (dcsr_[ch] & DCSR_RUN)) {
			dcmd_[ch] &= ~0x1fffu;
			if (!dmaAdvance(ch)) break;
		}
	}
	if (ch >= 0 && !(dcsr_[ch] & DCSR_RUN)) pcmFrac_ = 0;
}

uint64_t Soc::pcmNextEvent() const {
	int ch = pcmChannel();
	if (ch < 0 || !(gsr_ & GSR_PCR)) return UINT64_MAX;
	uint32_t rate = std::clamp<uint32_t>(board_.ac97PcmRate(), 4000, 48000);
	// Wake when the current descriptor drains (its end interrupt is due), or
	// after 64 frames at the latest, so the board's queue stays fed.
	// A marker descriptor runs on the next frame.
	uint32_t frames = (dcmd_[ch] & DCMD_FLOWTRG) && dtadr_[ch] == kPcdrPhys
	                ? std::clamp<uint32_t>((dcmd_[ch] & 0x1fff) / 4, 1, 64) : 1;
	uint64_t need = (uint64_t)frames * cpuHz_;
	uint64_t wait = need > pcmFrac_ ? (need - pcmFrac_ + rate - 1) / rate : 1;
	return pcmLast_ + std::max<uint64_t>(wait, 1);
}

void Soc::dmaWriteDcsr(int ch, uint32_t v) {
	uint32_t c = dcsr_[ch];
	c &= ~(v & (DCSR_ENDINTR | DCSR_STARTINTR | DCSR_BUSERR));
	c = (c & ~(DCSR_RUN | DCSR_NODESC | DCSR_STOPIRQEN)) | (v & (DCSR_RUN | DCSR_NODESC | DCSR_STOPIRQEN));
	bool start = !(dcsr_[ch] & DCSR_RUN) && (v & DCSR_RUN);
	dcsr_[ch] = c;
	if (start) {
		dcsr_[ch] &= ~DCSR_STOPSTATE;
		if ((drcmr_[12] & 0x8f) == (0x80u | (uint32_t)ch)) { pcmLast_ = cycles(); pcmFrac_ = 0; }
		if (!(c & DCSR_NODESC)) {
			uint32_t d = ddadr_[ch] & ~0xfu, w[4];
			for (int i = 0; i < 4; i++) if (!physRead32(d + 4 * i, w[i])) { dcsr_[ch] |= DCSR_BUSERR; return; }
			ddadr_[ch] = w[0]; dsadr_[ch] = w[1]; dtadr_[ch] = w[2]; dcmd_[ch] = w[3];
			if (dcmd_[ch] & DCMD_STARTIRQEN) dcsr_[ch] |= DCSR_STARTINTR;
		}
		dmaRun(ch);
		cpu_.stopRun();                               // a paced channel needs scheduling
	} else if (!(v & DCSR_RUN)) {
		dcsr_[ch] |= DCSR_STOPSTATE;
	}
}

// ── Bus decode ──────────────────────────────────────────────────────────

bool Soc::busRead(uint32_t pa, int size, uint32_t &out) {
	if (pa < 0x18000000u) return board_.csRead(pa >> 26, pa & 0x03ffffffu, size, out);
	if (pa >= 0x20000000u && pa < 0x40000000u) return board_.pcmciaRead(pa, size, out);
	if (pa - kRamBase < ram_.size()) {
		const uint8_t *p = ram_.data() + (pa - kRamBase);
		out = size == 4 ? (uint32_t)p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24)
		    : size == 2 ? (uint32_t)p[0] | (p[1] << 8) : p[0];
		return true;
	}
	if (pa < 0x40000000u || pa >= 0x4c000000u) return false;

	uint32_t off = pa & 0xffff;
	out = 0;
	switch (pa & 0xffff0000u) {
	case 0x40000000u:                                     // DMA
		if (off < 0x40) out = dcsr_[off >> 2];
		else if (off == 0xf0) out = dint();
		else if (off >= 0x100 && off < 0x1a0) out = drcmr_[(off - 0x100) >> 2];
		else if (off >= 0x200 && off < 0x300) {
			int ch = (off - 0x200) >> 4;
			switch (off & 0xc) {
			case 0x0: out = ddadr_[ch]; break;
			case 0x4: out = dsadr_[ch]; break;
			case 0x8: out = dtadr_[ch]; break;
			default:  out = dcmd_[ch]; break;
			}
		}
		break;
	case 0x40100000u: out = uartRead(FFUART, off >> 2); break;
	case 0x40200000u: out = uartRead(BTUART, off >> 2); break;
	case 0x40700000u: out = uartRead(STUART, off >> 2); break;
	case 0x41600000u: out = uartRead(HWUART, off >> 2); break;
	case 0x40300000u:                                     // I2C
		switch (off) {
		case 0x1680: out = 3; break;                      // IBMR: SCL and SDA high
		case 0x1688: out = idbr_; break;
		case 0x1690: out = icr_; break;
		case 0x1698: out = isr_; break;
		case 0x16a0: out = isar_; break;
		default: break;
		}
		break;
	case 0x40500000u: out = ac97Read(off); break;
	case 0x40900000u:                                     // RTC
		rtcUpdate(cycles());
		switch (off) {
		case 0x0: out = rcnr(); break;
		case 0x4: out = rtar_; break;
		case 0x8: out = rtsr_; break;
		case 0xc: out = rttr_; break;
		default: break;
		}
		break;
	case 0x40a00000u:                                     // OS timers
		switch (off) {
		case 0x00: case 0x04: case 0x08: case 0x0c: out = osmr_[off >> 2]; break;
		case 0x10: out = oscr(); break;
		case 0x14: ostUpdate(cycles()); out = ossr_; break;
		case 0x18: out = ower_; break;
		case 0x1c: out = oier_; break;
		default: break;
		}
		break;
	case 0x40d00000u:                                     // interrupt controller
		switch (off) {
		case 0x00: out = pending() & icmr_ & ~iclr_; break;
		case 0x04: out = icmr_; break;
		case 0x08: out = iclr_; break;
		case 0x0c: out = pending() & icmr_ & iclr_; break;
		case 0x10: out = pending(); break;
		case 0x14: out = iccr_; break;
		default: break;
		}
		break;
	case 0x40e00000u:                                     // GPIO
		if (off < 0x0c) out = gplr(off >> 2);
		else if (off < 0x18) out = gpdr_[(off - 0x0c) >> 2];
		else if (off < 0x30) out = 0;                     // GPSR/GPCR are write-only
		else if (off < 0x3c) out = grer_[(off - 0x30) >> 2];
		else if (off < 0x48) out = gfer_[(off - 0x3c) >> 2];
		else if (off < 0x54) out = gedr_[(off - 0x48) >> 2];
		else if (off < 0x6c) out = gafr_[(off - 0x54) >> 2];
		break;
	case 0x40f00000u: if (off < 0x40) out = pwr_[off >> 2]; break;
	case 0x41000000u:                                     // SSP
		switch (off) {
		case 0x00: out = sscr0_; break;
		case 0x04: out = sscr1_; break;
		case 0x08: out = 0x4; break;                      // SSSR: TX not full, RX empty
		case 0x0c: out = ssitr_; break;
		default: break;
		}
		break;
	case 0x41100000u:                                     // MMC
		switch (off) {
		case 0x04: out = mmcStat_; break;
		case 0x28: out = mmcImask_; break;
		case 0x2c: out = mmcIreg_; break;
		case 0x3c: out = 0; mmcIreg_ &= ~0x04u; break;     // MMC_RES: nothing came back
		case 0x40: out = 0; break;                        // RXFIFO
		default: if (off < 0x40) out = mmcRegs_[off >> 2]; break;
		}
		break;
	case 0x41300000u:                                     // clocks
		if (off == 0) out = cccr_;
		else if (off == 4) out = cken_;
		else if (off == 8) out = oscc_;
		break;
	case 0x44000000u: if (off < 0x100) out = lcd_[off >> 2]; break;
	case 0x48000000u: if (off < 0x80) out = memc_[off >> 2]; break;
	default:
		// Quiet peripherals (I2S, UDC, FICP, PWM, MMC, NSSP, ...): read as zero.
		break;
	}
	if (size == 1) out &= 0xff;
	else if (size == 2) out &= 0xffff;
	return true;
}

bool Soc::busWrite(uint32_t pa, int size, uint32_t v) {
	if (pa < 0x18000000u) return board_.csWrite(pa >> 26, pa & 0x03ffffffu, size, v);
	if (pa >= 0x20000000u && pa < 0x40000000u) return board_.pcmciaWrite(pa, size, v);
	if (pa - kRamBase < ram_.size()) {
		uint8_t *p = ram_.data() + (pa - kRamBase);
		for (int i = 0; i < size; i++) p[i] = (uint8_t)(v >> (8 * i));
		return true;
	}
	if (pa < 0x40000000u || pa >= 0x4c000000u) return false;

	uint32_t off = pa & 0xffff;
	switch (pa & 0xffff0000u) {
	case 0x40000000u:                                     // DMA
		if (off < 0x40) dmaWriteDcsr(off >> 2, v);
		else if (off >= 0x100 && off < 0x1a0) drcmr_[(off - 0x100) >> 2] = v;
		else if (off >= 0x200 && off < 0x300) {
			int ch = (off - 0x200) >> 4;
			switch (off & 0xc) {
			case 0x0: ddadr_[ch] = v; break;
			case 0x4: dsadr_[ch] = v; break;
			case 0x8: dtadr_[ch] = v; break;
			default:  dcmd_[ch] = v; break;
			}
		}
		break;
	case 0x40100000u: uartWrite(FFUART, off >> 2, v); break;
	case 0x40200000u: uartWrite(BTUART, off >> 2, v); break;
	case 0x40700000u: uartWrite(STUART, off >> 2, v); break;
	case 0x41600000u: uartWrite(HWUART, off >> 2, v); break;
	case 0x40300000u:                                     // I2C
		switch (off) {
		case 0x1688: idbr_ = v & 0xff; break;
		case 0x1690: i2cWriteIcr(v & 0xffff); break;
		case 0x1698:
			isr_ &= ~(v & (ISR_SSD | ISR_ALD | ISR_ITE | ISR_IRF | ISR_SAD | ISR_BED | ISR_GCAD));
			break;
		case 0x16a0: isar_ = v & 0x7f; break;
		default: break;
		}
		break;
	case 0x40500000u: ac97Write(off, v); break;
	case 0x40900000u:                                     // RTC
		rtcUpdate(cycles());
		switch (off) {
		case 0x0:
			rcnrBase_ = v; rtcCycle_ = cycles(); rtcSeen_ = v;
			break;
		case 0x4: rtar_ = v; break;
		case 0x8: rtsr_ = (rtsr_ & ~(v & 0x3)) & 0x3; rtsr_ |= v & 0xc; break;
		case 0xc: rttr_ = v; break;
		default: break;
		}
		cpu_.stopRun();
		break;
	case 0x40a00000u:                                     // OS timers
		ostUpdate(cycles());
		switch (off) {
		case 0x00: case 0x04: case 0x08: case 0x0c: osmr_[off >> 2] = v; break;
		case 0x10:
			oscrBase_ = v; oscrCycle_ = cycles(); ostChecked_ = -1;
			break;
		case 0x14: ossr_ &= ~(v & 0xf); break;
		case 0x18: ower_ = v & 1; break;
		case 0x1c: oier_ = v & 0xf; break;
		default: break;
		}
		cpu_.stopRun();
		break;
	case 0x40d00000u:                                     // interrupt controller
		switch (off) {
		case 0x04: icmr_ = v; break;
		case 0x08: iclr_ = v; break;
		case 0x14: iccr_ = v & 1; break;
		default: break;
		}
		break;
	case 0x40e00000u: {                                   // GPIO
		int b;
		uint32_t before[3] = { gplr(0), gplr(1), gplr(2) };
		uint32_t outBefore[3] = { gpOut_[0] & gpdr_[0], gpOut_[1] & gpdr_[1], gpOut_[2] & gpdr_[2] };
		if (off >= 0x0c && off < 0x18) { b = (off - 0x0c) >> 2; gpdr_[b] = v; }
		else if (off >= 0x18 && off < 0x24) { b = (off - 0x18) >> 2; gpOut_[b] |= v; }
		else if (off >= 0x24 && off < 0x30) { b = (off - 0x24) >> 2; gpOut_[b] &= ~v; }
		else if (off >= 0x30 && off < 0x3c) grer_[(off - 0x30) >> 2] = v;
		else if (off >= 0x3c && off < 0x48) gfer_[(off - 0x3c) >> 2] = v;
		else if (off >= 0x48 && off < 0x54) gedr_[(off - 0x48) >> 2] &= ~v;
		else if (off >= 0x54 && off < 0x6c) gafr_[(off - 0x54) >> 2] = v;
		for (int i = 0; i < 3; i++) {
			gpioEdges(i, before[i], gplr(i));
			uint32_t now = gpOut_[i] & gpdr_[i];
			uint32_t changed = now ^ outBefore[i];
			for (int bit = 0; changed && bit < 32; bit++)
				if (changed & (1u << bit)) board_.gpioOutputChanged(i * 32 + bit, (now >> bit) & 1);
		}
		break;
	}
	case 0x40f00000u: if (off < 0x40) pwr_[off >> 2] = v; break;
	case 0x41000000u:                                     // SSP
		switch (off) {
		case 0x00: sscr0_ = v; break;
		case 0x04: sscr1_ = v; break;
		case 0x0c: ssitr_ = v; break;
		default: break;
		}
		break;
	case 0x41100000u:                                     // MMC
		switch (off) {
		case 0x00:                                        // STRPCL
			if (v & 1) {                                  // stop clock
				mmcStat_ &= ~0x100u;
				mmcIreg_ |= 0x10;                         // CLK_IS_OFF
			} else if (v & 2) {                           // start clock
				mmcStat_ |= 0x100u;
				mmcIreg_ &= ~0x10u;
				if (mmcCmdArmed_) {
					mmcCmdArmed_ = false;
					mmcStat_ &= ~0x38ffu & ~0x100u;       // clear the last command's results
					mmcStat_ |= 0x100u;
					mmcIreg_ &= ~0x07u;
					mmcDoneAt_ = cycles() + std::max<uint64_t>(cpuHz_ / 20000, 64);
					cpu_.stopRun();
				}
			}
			break;
		case 0x28: mmcImask_ = v & 0x7f; break;
		case 0x30: mmcRegs_[off >> 2] = v; mmcCmdArmed_ = true; break;   // MMC_CMD
		default: if (off < 0x40) mmcRegs_[off >> 2] = v; break;
		}
		break;
	case 0x41300000u:
		if (off == 0) cccr_ = v;
		else if (off == 4) cken_ = v;
		else if (off == 8) oscc_ = (v & 2) ? 3 : oscc_;
		break;
	case 0x44000000u: if (off < 0x100) lcd_[off >> 2] = v; break;
	case 0x48000000u: if (off < 0x80) memc_[off >> 2] = v; break;
	default: break;
	}
	updateIrq();
	return true;
}

} // namespace PXA255
