// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include "emubase.h"
#include "pxa255.h"
#include <algorithm>
#include <deque>
#include <string>
#include <vector>

// Psion netBook Pro, as the Psion LX: the 2004/2005 Linux prototype that
// Psion Teklogix and Simtec built on the netBook Pro's hardware. The board
// is the one Simtec's kernel (arch/arm/mach-pxa/netbookpro.c and friends)
// drives:
//
//  - Intel PXA255 (XScale, ARMv5TE) — core/pxa255.*, core/xscale.*
//  - 128 MB SDRAM at 0xA0000000
//  - Epson S1D13806 display controller on nCS5: registers at 0x14000000,
//    the BitBLT data port at 0x14100000, 1.25 MB of display memory at
//    0x14200000, driving the 800x600 16 bpp TFT
//  - 64 MB x16 small-page NAND on nCS1 (CLE/ALE/nCE/WP on GPIO 21/22/62/61,
//    ready/busy on GPIO 20), holding the JFFS2 root filesystem
//  - the "PCON" power controller on I2C (address 0x13): power states, the
//    serial/USB/MMC power switches, and the keyboard, whose key codes it
//    writes into the PXA255's own I2C slave (address 0x70)
//  - a Wolfson WM9705 AC97 codec with its resistive touch digitiser, pen
//    down on GPIO 4 and continuous-mode samples in AC97 slot 5
//
// Boot follows the netBook Pro bootloader's handoff: the OS image
// (nBkProOs.img: a 256-byte "nBk2" header, then a big-endian length and a
// zlib stream) is inflated to 0xA0408000 and entered with the MMU off. The
// image is self-contained — its first instruction calls a stub that moves
// the kernel into place, writes the ATAGs at 0xA0400100 (machine 503,
// NETBOOKPRO), copies the 8 MB ext2 initrd to 0xA1000000 and jumps to the
// kernel — so all the bootloader has to provide is the RAM size word it
// leaves at 0xA00000F4. The initrd's /linuxrc then mounts the root
// filesystem from the NAND and pivots into it.

namespace NetbookPro {

class Emulator : public EmuBase, public PXA255::Board {
public:
	Emulator();
	~Emulator() override;

	// ── EmuBase ─────────────────────────────────────────────────────────
	uint8_t *getROMBuffer() override { return osImage_.data(); }
	size_t getROMSize() override { return osImage_.size(); }
	void loadROM(uint8_t *buffer, size_t size) override;
	void executeUntil(int64_t cycles) override;
	uint8_t *getRamBuffer() override { return soc_.ram(); }
	size_t getRamSize() const override { return soc_.ramSize(); }

	int32_t getClockSpeed() const override { return (int32_t)kCpuHz; }
	const char *getDeviceName() const override { return "Psion LX"; }
	int getDigitiserWidth() const override { return kLcdW; }
	int getDigitiserHeight() const override { return kLcdH; }
	int getLCDOffsetX() const override { return 0; }
	int getLCDOffsetY() const override { return 0; }
	int getLCDWidth() const override { return kLcdW; }
	int getLCDHeight() const override { return kLcdH; }
	void readLCDIntoBuffer(uint8_t **lines, bool is32BitOutput) const override;
	void setKeyboardKey(EpocKey key, bool value) override;
	void updateTouchInput(int32_t x, int32_t y, bool down) override;

	bool hasBacklight() const override { return true; }
	bool isBacklightDimmable() const override { return true; }
	int getBacklightLevel() const override;

	bool isCpuCrashed() const override { return false; }
	uint32_t otherCpuPc() const override { return soc_.cpu().pc(); }

	bool attachInternalFlash(const uint8_t *bytes, size_t size) override;
	size_t getInternalFlashSize() const override { return nand_.size(); }
	const uint8_t *getInternalFlashData() const override { return nand_.data(); }
	uint32_t getInternalFlashGeneration() const override { return nandGeneration_; }

	// ── Host-side extras (harness, diagnostics) ─────────────────────────
	// Text the kernel and userspace wrote to the FFUART (the serial console
	// when the command line names one).
	const std::string &serialLog() const { return serialLog_; }
	void serialInput(const uint8_t *bytes, size_t n) { soc_.uartRx(PXA255::FFUART, bytes, n); }
	PXA255::Soc &soc() { return soc_; }
	// Read the kernel's printk ring (log_buf) out of RAM, if it can be found.
	std::string kernelLog() const;
	// Send one netBook Pro key code (bit 7 = release) as the PCON would.
	void sendScancode(uint8_t code);

	// Geometry of the panel and the guest clock.
	static constexpr int kLcdW = 800, kLcdH = 600;
	static constexpr uint32_t kCpuHz = 200000000;   // instructions per guest second
	static constexpr uint32_t kRamSize = 128u << 20;
	static constexpr uint32_t kNandSize = 64u << 20;
	static constexpr uint32_t kNandPage = 512, kNandOob = 16, kNandPagesPerBlock = 32;
	static constexpr size_t kNandImageSize = (size_t)(kNandSize / kNandPage) * (kNandPage + kNandOob);

private:
	PXA255::Soc soc_;
	std::vector<uint8_t> osImage_;
	bool booted_ = false;
	uint32_t bootUnixTime_ = 0;
	void bootOs();

	// ── PXA255::Board ───────────────────────────────────────────────────
	bool csRead(int cs, uint32_t off, int size, uint32_t &out) override;
	bool csWrite(int cs, uint32_t off, int size, uint32_t v) override;
	bool gpioInput(int pin) override;
	void gpioOutputChanged(int pin, bool level) override;
	void uartTx(int uart, uint8_t byte) override;
	PXA255::I2cDevice *i2cDevice(uint8_t addr7) override;
	uint16_t ac97Read(uint8_t reg) override;
	void ac97Write(uint8_t reg, uint16_t v) override;
	void ac97ColdReset() override;
	uint16_t ac97ModemRead() override;
	uint64_t boardTick(uint64_t now) override;

	std::string serialLog_;

	// ── Epson S1D13806 ──────────────────────────────────────────────────
	uint8_t s1dRegs_[0x200] = {};
	std::vector<uint8_t> vram_;                     // 1.25 MB display buffer
	uint8_t lut_[256][3] = {};
	int lutIndex_ = 0, lutComponent_ = 0;
	uint8_t s1dRead(uint32_t reg);
	void s1dWrite(uint32_t reg, uint8_t v);
	// BitBLT engine
	bool bltActive_ = false;
	std::vector<uint16_t> bltData_;                 // words written to the data port
	uint32_t bltPixelsDone_ = 0;
	void bltStart();
	void bltDataWrite(uint16_t w);
	void bltFinishWrite();
	uint16_t rop(uint8_t code, uint16_t s, uint16_t d) const;

	// ── NAND ────────────────────────────────────────────────────────────
	std::vector<uint8_t> nand_;                     // pages of 512 + 16 bytes
	uint32_t nandGeneration_ = 0;
	bool cle_ = false, ale_ = false, nce_ = true, wp_ = true;
	uint8_t nandCmd_ = 0xff, nandPointer_ = 0x00;   // last command, read area
	uint32_t nandAddr_[5] = {};
	int nandAddrCycles_ = 0;
	uint32_t nandCol_ = 0;                          // word offset within page (incl. spare)
	uint32_t nandPage_ = 0;
	std::vector<uint16_t> nandBuf_;                 // program buffer, one page
	bool nandBufDirty_[264] = {};
	int nandIdPos_ = 0;
	uint8_t nandStatus_ = 0xc0;
	void nandCommand(uint8_t cmd);
	void nandAddress(uint8_t a);
	uint16_t nandReadData();
	void nandWriteData(uint16_t v);
	uint8_t *nandPagePtr(uint32_t page) { return nand_.data() + (size_t)page * (kNandPage + kNandOob); }

	// ── PCON (I2C 0x13) and the keyboard ────────────────────────────────
	class Pcon : public PXA255::I2cDevice {
	public:
		explicit Pcon(Emulator &e) : emu_(e) {}
		bool i2cStart(bool read) override;
		bool i2cWrite(uint8_t b) override;
		uint8_t i2cRead() override;
		void i2cStop() override;
		uint8_t regs[256] = {};
	private:
		Emulator &emu_;
		bool reading_ = false;
		int ptr_ = -1;
		std::vector<uint8_t> written_;
	};
	Pcon pcon_{ *this };
	friend class Pcon;

	// Ricoh R2051 real-time clock (I2C 0x32): BCD time registers, addressed
	// with the register number in the high nibble of the first byte.
	class RicohRtc : public PXA255::I2cDevice {
	public:
		explicit RicohRtc(Emulator &e) : emu_(e) {}
		bool i2cStart(bool read) override;
		bool i2cWrite(uint8_t b) override;
		uint8_t i2cRead() override;
		void i2cStop() override;
		int64_t offset = 0;          // guest-set time minus host-derived time
		uint8_t ctrl1 = 0x20, ctrl2 = 0x20, osc = 0;   // 24-hour, oscillator running
	private:
		Emulator &emu_;
		int ptr_ = 0;
		bool first_ = false, reading_ = false;
		uint8_t pending_[16] = {};
		uint16_t written_ = 0;
		int64_t now() const;
	};
	RicohRtc rtc_{ *this };
	friend class RicohRtc;

	// 24C01-style configuration EEPROM (I2C 0x53): serial numbers, hardware
	// revision, touch calibration, keyboard type. Erased on this machine.
	class Eeprom : public PXA255::I2cDevice {
	public:
		bool i2cStart(bool read) override { if (!read) first_ = true; return true; }
		bool i2cWrite(uint8_t b) override {
			if (first_) { ptr_ = b & 0x7f; first_ = false; }
			else { data[ptr_] = b; ptr_ = (ptr_ + 1) & 0x7f; }
			return true;
		}
		uint8_t i2cRead() override { uint8_t v = data[ptr_]; ptr_ = (ptr_ + 1) & 0x7f; return v; }
		void i2cStop() override {}
		uint8_t data[128];
		Eeprom() { std::fill(data, data + 128, 0xff); }
	private:
		int ptr_ = 0;
		bool first_ = false;
	};
	Eeprom eeprom_;
	void pconWritten(const std::vector<uint8_t> &bytes);
	std::deque<uint8_t> keyQueue_;
	bool keyAwaitReady_ = false;
	uint64_t keySentAt_ = 0;
	uint64_t powerEchoAt_ = UINT64_MAX;
	uint8_t powerEchoValue_ = 0;
	bool fnHeld_ = false;

	// ── WM9705 codec and touch panel ────────────────────────────────────
	uint16_t codec_[64] = {};                       // 16-bit registers, index reg >> 1
	bool penDown_ = false;
	uint16_t touchX_ = 0, touchY_ = 0;
	int modemPhase_ = 0;
	void setPen(bool down);
	static uint16_t touchAdcX(int x);
	static uint16_t touchAdcY(int y);
	void seedEeprom();
};

} // namespace NetbookPro
