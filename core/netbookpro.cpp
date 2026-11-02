// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "netbookpro.h"
#include "inflate.h"
#include "rtc_seed.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace NetbookPro {

namespace {

constexpr uint32_t kVramBase = 0x14200000u;   // S1D13806 display buffer (nCS5 + 2 MB)
constexpr uint32_t kVramSize = 0x140000u;     // 1.25 MB embedded SDRAM

// GPIO pins the board uses.
enum : int {
	GPIO_BATT_WARN = 0, GPIO_SUSPEND_REQ = 1, GPIO_HEADPHONE = 3, GPIO_PEN = 4,
	GPIO_MMC_DETECT = 9, GPIO_NAND_RB = 20, GPIO_NAND_CLE = 21, GPIO_NAND_ALE = 22,
	GPIO_NAND_WP = 61, GPIO_NAND_NCE = 62,
	GPIO_CF_READY = 71, GPIO_CF_STATUS = 72, GPIO_CF_DETECT = 73,
	GPIO_PC_DETECT = 75, GPIO_PC_STATUS = 76, GPIO_PC_READY = 77,
};

// The power controller's register map, as nbp-pcon.c uses it.
enum : uint8_t { PCON_KEYBOARD = 0, PCON_POWER = 5, PCON_COMMAND = 12 };

// netBook Pro key codes (drivers/input/keyboard/netbookpro.c, normal_map),
// keyed by what the frontend sends. Letters and digits arrive as ASCII.
struct KeyMap { int key; uint8_t code; };
const KeyMap kKeys[] = {
	{ 'A', 0x50 }, { 'B', 0x3a }, { 'C', 0x44 }, { 'D', 0x45 }, { 'E', 0x46 }, { 'F', 0x42 },
	{ 'G', 0x39 }, { 'H', 0x61 }, { 'I', 0x1c }, { 'J', 0x2b }, { 'K', 0x1b }, { 'L', 0x20 },
	{ 'M', 0x2a }, { 'N', 0x06 }, { 'O', 0x1f }, { 'P', 0x12 }, { 'Q', 0x4f }, { 'R', 0x35 },
	{ 'S', 0x52 }, { 'T', 0x38 }, { 'U', 0x2c }, { 'V', 0x43 }, { 'W', 0x53 }, { 'X', 0x54 },
	{ 'Y', 0x03 }, { 'Z', 0x51 },
	{ '1', 0x5c }, { '2', 0x5d }, { '3', 0x47 }, { '4', 0x36 }, { '5', 0x37 },
	{ '6', 0x5e }, { '7', 0x2d }, { '8', 0x1d }, { '9', 0x1e }, { '0', 0x11 },
	{ EStdKeyEscape, 0x5b }, { EStdKeyMinus, 0x0f }, { EStdKeyEquals, 0x10 },
	{ EStdKeyBackspace, 0x0d }, { EStdKeyTab, 0x02 }, { EStdKeyEnter, 0x00 },
	{ EStdKeyNkpEnter, 0x00 }, { EStdKeyBackSlash, 0x3f }, { EStdKeySemiColon, 0x13 },
	{ EStdKeySingleQuote, 0x29 }, { EStdKeyLeftShift, 0x07 }, { EStdKeyRightShift, 0x15 },
	{ EStdKeyComma, 0x28 }, { EStdKeyFullStop, 0x5f }, { EStdKeyForwardSlash, 0x4d },
	{ EStdKeyUpArrow, 0x60 }, { EStdKeyDownArrow, 0x05 }, { EStdKeyLeftArrow, 0x04 },
	{ EStdKeyRightArrow, 0x01 }, { EStdKeyLeftCtrl, 0x23 }, { EStdKeyRightCtrl, 0x23 },
	{ EStdKeyLeftAlt, 0x41 }, { EStdKeyRightAlt, 0x41 }, { EStdKeyLeftFunc, 0x31 },
	{ EStdKeyRightFunc, 0x31 }, { EStdKeySpace, 0x34 }, { EStdKeyMenu, 0x31 },
};
// Keys the netBook Pro keyboard only has on its Fn layer (fn_map): sent as
// Fn held around the base key.
const KeyMap kFnKeys[] = {
	{ EStdKeyHome, 0x04 }, { EStdKeyEnd, 0x01 }, { EStdKeyPageUp, 0x60 },
	{ EStdKeyPageDown, 0x05 }, { EStdKeyCapsLock, 0x02 }, { EStdKeyF1, 0x4d },
};
constexpr uint8_t kFnCode = 0x31;

inline uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }
inline uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

bool envOn(const char *name) {
	const char *v = std::getenv(name);
	return v && *v && *v != '0';
}

} // namespace

Emulator::Emulator()
    : soc_(*this, kRamSize, kCpuHz), vram_(kVramSize), nand_(kNandImageSize, 0xff),
      nandBuf_(264, 0xffff) {
	soc_.cpu().addRam(vram_.data(), kVramBase, kVramSize);
	seedEeprom();
}

Emulator::~Emulator() = default;

// ── Boot ────────────────────────────────────────────────────────────────

void Emulator::loadROM(uint8_t *buffer, size_t size) {
	osImage_.assign(buffer, buffer + size);
	booted_ = false;
}

void Emulator::bootOs() {
	booted_ = true;
	bootUnixTime_ = psionInitialRtcSeconds() + 946684800u;
	soc_.reset(bootUnixTime_);                 // RCNR counts Unix time
	rtc_.offset = 0;
	std::memset(soc_.ram(), 0, soc_.ramSize());
	std::fill(vram_.begin(), vram_.end(), 0);
	std::memset(s1dRegs_, 0, sizeof(s1dRegs_));
	s1dRegs_[0x00] = 0x1c;                     // product code 7 (S1D13806), revision 0
	std::memset(codec_, 0, sizeof(codec_));
	ac97ColdReset();
	keyQueue_.clear();
	keyAwaitReady_ = false;
	powerEchoAt_ = UINT64_MAX;
	penDown_ = false;
	cle_ = ale_ = false;
	nandCmd_ = 0xff;

	XScale::Cpu &cpu = soc_.cpu();
	// nBkProOs.img: "nBk2" at 0x0c, payload length at 0x10, load offset into
	// SDRAM at 0x18; the payload is a big-endian uncompressed length and a
	// zlib stream.
	const uint8_t *img = osImage_.data();
	size_t n = osImage_.size();
	if (n < 0x108 || std::memcmp(img + 0x0c, "nBk2", 4) != 0) {
		std::fprintf(stderr, "[netbookpro] OS image has no nBk2 header\n");
		cpu.setPc(0);
		return;
	}
	uint32_t payloadLen = le32(img + 0x10);
	uint32_t loadOff = le32(img + 0x18);
	if (payloadLen > n - 0x100) payloadLen = (uint32_t)(n - 0x100);
	uint32_t rawLen = be32(img + 0x100);
	std::vector<uint8_t> out;
	out.reserve(rawLen);
	if (!Inflate::zlib(img + 0x104, payloadLen - 4, out) || out.size() != rawLen ||
	    (uint64_t)loadOff + out.size() > soc_.ramSize()) {
		std::fprintf(stderr, "[netbookpro] OS image payload did not inflate\n");
		cpu.setPc(0);
		return;
	}
	if (const char *repl = std::getenv("PSION_LX_CMDLINE")) {
		// Debug aid: replace the stub's ATAG command line (56 bytes of room).
		static const char kCmd[] = "root=/dev/ram rw console=ttySA0,115200 init=/linuxrc";
		auto it = std::search(out.begin(), out.end(), kCmd, kCmd + sizeof(kCmd) - 1);
		if (it != out.end() && std::strlen(repl) < 56) {
			std::fill(it, it + 56, 0);
			std::memcpy(&*it, repl, std::strlen(repl));
		}
	}
	std::memcpy(soc_.ram() + loadOff, out.data(), out.size());
	// The bootloader leaves the SDRAM size where the stub's ATAG_MEM reads it.
	uint32_t ramWord = kRamSize;
	std::memcpy(soc_.ram() + 0xf4, &ramWord, 4);
	// GPIOs the kernel drives without configuring them first: the NAND's
	// CLE / ALE / nCE / WP lines, the SL811HS reset and the AC97 reset.
	soc_.presetGpio(GPIO_NAND_CLE, true, false);
	soc_.presetGpio(GPIO_NAND_ALE, true, false);
	soc_.presetGpio(GPIO_NAND_NCE, true, true);
	soc_.presetGpio(GPIO_NAND_WP, true, false);
	soc_.presetGpio(14, true, true);
	soc_.presetGpio(63, true, true);
	cpu.setCpsr(0xd3);                         // SVC, IRQ and FIQ masked
	cpu.setPc(PXA255::Soc::kRamBase + loadOff);
}

void Emulator::executeUntil(int64_t cycles) {
	if (!booted_) bootOs();
	if (cycles > passedCycles) {
		soc_.runUntil((uint64_t)cycles);
		passedCycles = (int64_t)soc_.cycles();
	}
}

// ── Static memory ───────────────────────────────────────────────────────

bool Emulator::csRead(int cs, uint32_t off, int size, uint32_t &out) {
	switch (cs) {
	case 1: {                                   // NAND, 16-bit bus
		uint16_t w = nandReadData();
		out = size == 1 ? (w & 0xff) : w;
		if (size == 4) out |= (uint32_t)nandReadData() << 16;
		return true;
	}
	case 5:
		if (off < 0x100000) {                   // S1D13806 registers
			out = 0;
			for (int i = 0; i < size; i++) out |= (uint32_t)s1dRead((off + i) & 0x1ff) << (8 * i);
			return true;
		}
		if (off < 0x200000) { out = 0; return true; }   // BitBLT data port (read BLT unsupported)
		if (off - 0x200000 < kVramSize) {
			const uint8_t *p = vram_.data() + (off - 0x200000);
			out = 0;
			for (int i = 0; i < size && off - 0x200000 + i < kVramSize; i++) out |= (uint32_t)p[i] << (8 * i);
			return true;
		}
		out = 0;
		return true;
	case 0:                                     // boot flash (not modelled): erased
		out = size == 1 ? 0xff : size == 2 ? 0xffff : 0xffffffffu;
		return true;
	default:                                    // nCS2 sync read, nCS3, SL811HS (absent)
		out = 0;
		return true;
	}
}

bool Emulator::csWrite(int cs, uint32_t off, int size, uint32_t v) {
	switch (cs) {
	case 1:
		if (cle_) nandCommand((uint8_t)v);
		else if (ale_) nandAddress((uint8_t)v);
		else {
			nandWriteData((uint16_t)(size == 1 ? (v & 0xff) | 0xff00 : v));
			if (size == 4) nandWriteData((uint16_t)(v >> 16));
		}
		return true;
	case 5:
		if (off < 0x100000) {
			for (int i = 0; i < size; i++) s1dWrite((off + i) & 0x1ff, (uint8_t)(v >> (8 * i)));
			return true;
		}
		if (off < 0x200000) {
			if (size == 4) { bltDataWrite((uint16_t)v); bltDataWrite((uint16_t)(v >> 16)); }
			else bltDataWrite((uint16_t)v);
			return true;
		}
		if (off - 0x200000 < kVramSize) {
			uint8_t *p = vram_.data() + (off - 0x200000);
			for (int i = 0; i < size && off - 0x200000 + i < kVramSize; i++) p[i] = (uint8_t)(v >> (8 * i));
		}
		return true;
	default:
		return true;
	}
}

// ── GPIO ────────────────────────────────────────────────────────────────

bool Emulator::gpioInput(int pin) {
	switch (pin) {
	case GPIO_NAND_RB: return true;             // NAND ready
	case GPIO_HEADPHONE: return true;           // no headphones (high = removed)
	case GPIO_PEN: return penDown_;
	case GPIO_MMC_DETECT: return true;
	case GPIO_CF_READY: case GPIO_PC_READY: return true;
	case GPIO_CF_DETECT: case GPIO_PC_DETECT: return true;   // high = no card
	case GPIO_CF_STATUS: case GPIO_PC_STATUS: return true;
	default: return false;
	}
}

void Emulator::gpioOutputChanged(int pin, bool level) {
	switch (pin) {
	case GPIO_NAND_CLE: cle_ = level; break;
	case GPIO_NAND_ALE: ale_ = level; break;
	case GPIO_NAND_NCE: nce_ = level; break;
	case GPIO_NAND_WP: wp_ = !level; break;     // GPCR(61) asserts write protect
	default: break;
	}
}

void Emulator::uartTx(int uart, uint8_t byte) {
	if (uart != PXA255::FFUART) return;
	static const bool echo = envOn("PSION_LX_CONSOLE");
	if (echo) { std::fputc(byte, stderr); }
	if (serialLog_.size() > (1u << 20)) serialLog_.erase(0, serialLog_.size() - (1u << 19));
	serialLog_.push_back((char)byte);
}

// ── S1D13806 ────────────────────────────────────────────────────────────

uint8_t Emulator::s1dRead(uint32_t reg) {
	switch (reg) {
	case 0x000: return 0x1c;
	case 0x100: return bltActive_ ? 0x80 : 0x00;   // BitBLT active; FIFO empty
	case 0x1f1: return 0;                          // not in power save
	default: return s1dRegs_[reg];
	}
}

void Emulator::s1dWrite(uint32_t reg, uint8_t v) {
	if (reg == 0x000) return;
	s1dRegs_[reg] = v;
	switch (reg) {
	case 0x100:
		if (v & 0x80) bltStart();
		break;
	case 0x1e2:
		lutIndex_ = v; lutComponent_ = 0;
		break;
	case 0x1e4:
		lut_[lutIndex_][lutComponent_] = v;
		if (++lutComponent_ == 3) { lutComponent_ = 0; lutIndex_ = (lutIndex_ + 1) & 0xff; }
		break;
	default:
		break;
	}
}

uint16_t Emulator::rop(uint8_t code, uint16_t s, uint16_t d) const {
	switch (code & 0xf) {
	case 0x0: return 0;
	case 0x1: return (uint16_t)~(s | d);
	case 0x2: return (uint16_t)(~s & d);
	case 0x3: return (uint16_t)~s;
	case 0x4: return (uint16_t)(s & ~d);
	case 0x5: return (uint16_t)~d;
	case 0x6: return (uint16_t)(s ^ d);
	case 0x7: return (uint16_t)~(s & d);
	case 0x8: return (uint16_t)(s & d);
	case 0x9: return (uint16_t)~(s ^ d);
	case 0xa: return d;
	case 0xb: return (uint16_t)(~s | d);
	case 0xc: return s;
	case 0xd: return (uint16_t)(s | ~d);
	case 0xe: return (uint16_t)(s | d);
	default:  return 0xffff;
	}
}

void Emulator::bltStart() {
	const uint8_t *r = s1dRegs_;
	uint8_t op = r[0x103] & 0x0f, ropc = r[0x102] & 0x0f;
	bool px16 = r[0x101] & 1;
	int bpp = px16 ? 2 : 1;
	uint32_t src = r[0x104] | (r[0x105] << 8) | ((r[0x106] & 0x1f) << 16);
	uint32_t dst = r[0x108] | (r[0x109] << 8) | ((r[0x10a] & 0x1f) << 16);
	uint32_t stride = (uint32_t)(r[0x10c] | ((r[0x10d] & 0x07) << 8)) * 2;
	uint32_t w = (r[0x110] | ((r[0x111] & 0x03) << 8)) + 1;
	uint32_t h = (r[0x112] | ((r[0x113] & 0x03) << 8)) + 1;
	uint16_t fg = (uint16_t)(r[0x118] | (r[0x119] << 8));
	uint16_t bg = (uint16_t)(r[0x114] | (r[0x115] << 8));
	uint32_t sStride = (r[0x100] & 1) ? w * bpp : stride;    // source linear select
	uint32_t dStride = (r[0x100] & 2) ? w * bpp : stride;    // destination linear select
	if (!px16) { fg &= 0xff; bg &= 0xff; }

	auto get = [&](uint32_t a) -> uint16_t {
		a %= kVramSize;
		return px16 ? (uint16_t)(vram_[a] | (vram_[(a + 1) % kVramSize] << 8)) : vram_[a];
	};
	auto put = [&](uint32_t a, uint16_t v) {
		a %= kVramSize;
		vram_[a] = (uint8_t)v;
		if (px16) vram_[(a + 1) % kVramSize] = (uint8_t)(v >> 8);
	};

	switch (op) {
	case 0x0: case 0x4: case 0x8: case 0x9:     // host-fed: wait for the data port
		bltActive_ = true;
		bltData_.clear();
		bltPixelsDone_ = 0;
		return;
	case 0x2: case 0x5:                         // move, positive direction (5: transparent)
		for (uint32_t y = 0; y < h; y++)
			for (uint32_t x = 0; x < w; x++) {
				uint32_t sa = src + y * sStride + x * bpp, da = dst + y * dStride + x * bpp;
				uint16_t s = get(sa);
				if (op == 0x5) { if (s != bg) put(da, s); }
				else put(da, rop(ropc, s, get(da)));
			}
		break;
	case 0x3:                                   // move, negative direction
		for (uint32_t y = 0; y < h; y++)
			for (uint32_t x = 0; x < w; x++) {
				uint32_t sa = src - y * sStride - x * bpp, da = dst - y * dStride - x * bpp;
				put(da, rop(ropc, get(sa), get(da)));
			}
		break;
	case 0x6: case 0x7: {                       // 8x8 pattern fill (7: transparent)
		uint32_t patBytes = 64u * bpp;
		uint32_t base = src & ~(patBytes - 1);
		uint32_t start = (src - base) / bpp;
		for (uint32_t y = 0; y < h; y++)
			for (uint32_t x = 0; x < w; x++) {
				uint32_t pi = (((start >> 3) + y) & 7) * 8 + (((start & 7) + x) & 7);
				uint16_t p = get(base + pi * bpp);
				uint32_t da = dst + y * dStride + x * bpp;
				if (op == 0x7) { if (p != bg) put(da, p); }
				else put(da, rop(ropc, p, get(da)));
			}
		break;
	}
	case 0xa: case 0xb: {                       // move with colour expansion
		uint32_t startBit = r[0x102] & 7;
		uint32_t lineBytes = (w + startBit + 15) / 16 * 2;
		for (uint32_t y = 0; y < h; y++)
			for (uint32_t x = 0; x < w; x++) {
				uint32_t bit = startBit + x;
				uint8_t b = vram_[(src + y * lineBytes + bit / 8) % kVramSize];
				bool on = (b >> (7 - (bit & 7))) & 1;
				uint32_t da = dst + y * dStride + x * bpp;
				if (on) put(da, fg);
				else if (op == 0xa) put(da, bg);
			}
		break;
	}
	case 0xc:                                   // solid fill
		for (uint32_t y = 0; y < h; y++)
			for (uint32_t x = 0; x < w; x++) put(dst + y * dStride + x * bpp, fg);
		break;
	default:                                    // read BLT and reserved: nothing to do
		break;
	}
	bltActive_ = false;
	s1dRegs_[0x100] &= 0x7f;
}

void Emulator::bltDataWrite(uint16_t word) {
	if (!bltActive_) return;
	bltData_.push_back(word);
	const uint8_t *r = s1dRegs_;
	uint8_t op = r[0x103] & 0x0f;
	bool px16 = r[0x101] & 1;
	uint32_t w = (r[0x110] | ((r[0x111] & 0x03) << 8)) + 1;
	uint32_t h = (r[0x112] | ((r[0x113] & 0x03) << 8)) + 1;
	uint32_t wordsPerLine;
	if (op == 0x8 || op == 0x9) wordsPerLine = ((r[0x102] & 7) + w + 15) / 16;
	else wordsPerLine = px16 ? w : (w + 1) / 2;
	if (bltData_.size() >= (size_t)wordsPerLine * h) bltFinishWrite();
}

void Emulator::bltFinishWrite() {
	const uint8_t *r = s1dRegs_;
	uint8_t op = r[0x103] & 0x0f, ropc = r[0x102] & 0x0f;
	bool px16 = r[0x101] & 1;
	int bpp = px16 ? 2 : 1;
	uint32_t dst = r[0x108] | (r[0x109] << 8) | ((r[0x10a] & 0x1f) << 16);
	uint32_t stride = (uint32_t)(r[0x10c] | ((r[0x10d] & 0x07) << 8)) * 2;
	uint32_t w = (r[0x110] | ((r[0x111] & 0x03) << 8)) + 1;
	uint32_t h = (r[0x112] | ((r[0x113] & 0x03) << 8)) + 1;
	uint16_t fg = (uint16_t)(r[0x118] | (r[0x119] << 8));
	uint16_t bg = (uint16_t)(r[0x114] | (r[0x115] << 8));
	uint32_t dStride = (r[0x100] & 2) ? w * bpp : stride;
	auto put = [&](uint32_t a, uint16_t v) {
		a %= kVramSize;
		vram_[a] = (uint8_t)v;
		if (px16) vram_[(a + 1) % kVramSize] = (uint8_t)(v >> 8);
	};
	auto get = [&](uint32_t a) -> uint16_t {
		a %= kVramSize;
		return px16 ? (uint16_t)(vram_[a] | (vram_[(a + 1) % kVramSize] << 8)) : vram_[a];
	};
	if (op == 0x8 || op == 0x9) {
		uint32_t startBit = r[0x102] & 7;
		uint32_t wpl = (startBit + w + 15) / 16;
		for (uint32_t y = 0; y < h; y++)
			for (uint32_t x = 0; x < w; x++) {
				uint32_t bit = startBit + x;
				uint16_t word = bltData_[y * wpl + bit / 16];
				bool on = (word >> (15 - (bit & 15))) & 1;
				uint32_t da = dst + y * dStride + x * bpp;
				if (on) put(da, px16 ? fg : (fg & 0xff));
				else if (op == 0x8) put(da, px16 ? bg : (bg & 0xff));
			}
	} else {
		uint32_t wpl = px16 ? w : (w + 1) / 2;
		for (uint32_t y = 0; y < h; y++)
			for (uint32_t x = 0; x < w; x++) {
				uint16_t s = px16 ? bltData_[y * wpl + x]
				                  : (uint16_t)((bltData_[y * wpl + x / 2] >> ((x & 1) * 8)) & 0xff);
				uint32_t da = dst + y * dStride + x * bpp;
				if (op == 0x4) { if (s != bg) put(da, s); }
				else put(da, rop(ropc, s, get(da)));
			}
	}
	bltActive_ = false;
	bltData_.clear();
	s1dRegs_[0x100] &= 0x7f;
}

int Emulator::getBacklightLevel() const {
	uint8_t g = s1dRegs_[0x008];
	if (!(g & 0x06)) return 0;
	return ((g >> 3) & 0x1f) + 1;
}

void Emulator::readLCDIntoBuffer(uint8_t **lines, bool is32BitOutput) const {
	const uint8_t *r = s1dRegs_;
	int bpp = r[0x40] & 7;
	bool on = (r[0x1fc] & 7) != 0 && !(r[0x40] & 0x80);    // LCD enabled, not blanked
	uint32_t start = ((uint32_t)r[0x42] | (r[0x43] << 8) | ((r[0x44] & 0x0f) << 16)) * 4;
	uint32_t stride = (uint32_t)(r[0x46] | ((r[0x47] & 0x07) << 8)) * 2;
	int w = std::min(kLcdW, (r[0x32] + 1) * 8);
	int h = std::min(kLcdH, (r[0x38] | ((r[0x39] & 3) << 8)) + 1);
	for (int y = 0; y < kLcdH; y++) {
		if (!is32BitOutput) {
			// 8-bit grey output (harness PGM screenshots).
			uint8_t *row = lines[y];
			for (int x = 0; x < kLcdW; x++) {
				uint8_t g = 0;
				if (on && y < h && x < w && bpp == 5) {
					uint32_t a = (start + y * stride + x * 2) % kVramSize;
					uint16_t p = (uint16_t)(vram_[a] | (vram_[(a + 1) % kVramSize] << 8));
					uint32_t rr = ((p >> 11) & 31) * 255 / 31, gg = ((p >> 5) & 63) * 255 / 63, bb = (p & 31) * 255 / 31;
					g = (uint8_t)((rr * 77 + gg * 150 + bb * 29) >> 8);
				}
				row[x] = g;
			}
			continue;
		}
		uint32_t *row = reinterpret_cast<uint32_t *>(lines[y]);
		for (int x = 0; x < kLcdW; x++) {
			uint32_t px = 0xff000000u;
			if (on && y < h && x < w) {
				uint8_t rr = 0, gg = 0, bb = 0;
				if (bpp == 5) {
					uint32_t a = (start + y * stride + x * 2) % kVramSize;
					uint16_t p = (uint16_t)(vram_[a] | (vram_[(a + 1) % kVramSize] << 8));
					uint32_t r5 = (p >> 11) & 31, g6 = (p >> 5) & 63, b5 = p & 31;
					rr = (uint8_t)((r5 << 3) | (r5 >> 2));
					gg = (uint8_t)((g6 << 2) | (g6 >> 4));
					bb = (uint8_t)((b5 << 3) | (b5 >> 2));
				} else if (bpp == 3 || bpp == 2) {
					uint32_t idx;
					if (bpp == 3) idx = vram_[(start + y * stride + x) % kVramSize];
					else idx = (vram_[(start + y * stride + x / 2) % kVramSize] >> ((x & 1) ? 0 : 4)) & 0xf;
					rr = (uint8_t)((lut_[idx][0] & 0xf0) | (lut_[idx][0] >> 4));
					gg = (uint8_t)((lut_[idx][1] & 0xf0) | (lut_[idx][1] >> 4));
					bb = (uint8_t)((lut_[idx][2] & 0xf0) | (lut_[idx][2] >> 4));
				}
				px |= rr | (gg << 8) | ((uint32_t)bb << 16);
			}
			row[x] = px;
		}
	}
}

// ── NAND ────────────────────────────────────────────────────────────────
//
// 64 MB, x16, small page: 512-byte pages with 16 bytes of spare, 32 pages to
// a 16 KB block. Columns are in 16-bit words (256 main + 8 spare). The
// kernel drives CLE / ALE on GPIOs and moves data through one address on
// nCS1, so every access to that address is one bus cycle.

bool Emulator::attachInternalFlash(const uint8_t *bytes, size_t size) {
	if (size != kNandImageSize) return false;
	std::memcpy(nand_.data(), bytes, size);
	return true;
}

void Emulator::nandCommand(uint8_t cmd) {
	switch (cmd) {
	case 0x00: case 0x01: case 0x50:             // READ0 / READ1 / READOOB: set the area
		nandPointer_ = cmd;
		nandCmd_ = cmd;
		nandAddrCycles_ = 0;
		break;
	case 0x80:                                   // SEQIN
		nandCmd_ = cmd;
		nandAddrCycles_ = 0;
		std::fill(nandBuf_.begin(), nandBuf_.end(), 0xffff);
		break;
	case 0x10:                                   // PAGEPROG
		if (nandCmd_ == 0x80 && wp_ == false) {
			uint8_t *p = nandPagePtr(nandPage_);
			for (int i = 0; i < 264; i++) {
				p[2 * i] &= (uint8_t)nandBuf_[i];
				p[2 * i + 1] &= (uint8_t)(nandBuf_[i] >> 8);
			}
			nandGeneration_++;
			nandStatus_ = 0xc0;
		} else {
			nandStatus_ = (uint8_t)(wp_ ? 0x41 : 0xc1);
		}
		nandCmd_ = cmd;
		nandPointer_ = 0x00;
		break;
	case 0x60:                                   // ERASE1
		nandCmd_ = cmd;
		nandAddrCycles_ = 0;
		break;
	case 0xd0:                                   // ERASE2
		if (nandCmd_ == 0x60 && !wp_) {
			uint32_t first = (nandPage_ / kNandPagesPerBlock) * kNandPagesPerBlock;
			std::memset(nandPagePtr(first), 0xff, (size_t)kNandPagesPerBlock * (kNandPage + kNandOob));
			nandGeneration_++;
			nandStatus_ = 0xc0;
		} else {
			nandStatus_ = (uint8_t)(wp_ ? 0x41 : 0xc1);
		}
		nandCmd_ = cmd;
		break;
	case 0x70:                                   // STATUS
		nandCmd_ = cmd;
		break;
	case 0x90:                                   // READID
		nandCmd_ = cmd;
		nandAddrCycles_ = 0;
		nandIdPos_ = 0;
		break;
	case 0xff:                                   // RESET
		nandCmd_ = 0xff;
		nandPointer_ = 0x00;
		nandStatus_ = 0xc0;
		break;
	default:
		nandCmd_ = cmd;
		break;
	}
}

void Emulator::nandAddress(uint8_t a) {
	if (nandAddrCycles_ < 5) nandAddr_[nandAddrCycles_] = a;
	nandAddrCycles_++;
	switch (nandCmd_) {
	case 0x00: case 0x01: case 0x50: case 0x80:
		// column, then three row cycles
		if (nandAddrCycles_ == 1) {
			uint32_t col = a;
			if (nandPointer_ == 0x50) col = 256 + (col & 7);
			else if (nandPointer_ == 0x01) col += 128;
			nandCol_ = col;
		} else if (nandAddrCycles_ >= 2 && nandAddrCycles_ <= 4) {
			nandPage_ = (nandAddr_[1] | (nandAddrCycles_ >= 3 ? nandAddr_[2] << 8 : 0) |
			             (nandAddrCycles_ >= 4 ? (nandAddr_[3] & 0x0f) << 16 : 0)) %
			            (kNandSize / kNandPage);
		}
		break;
	case 0x60:
		if (nandAddrCycles_ <= 3)
			nandPage_ = (nandAddr_[0] | (nandAddrCycles_ >= 2 ? nandAddr_[1] << 8 : 0) |
			             (nandAddrCycles_ >= 3 ? (nandAddr_[2] & 0x0f) << 16 : 0)) %
			            (kNandSize / kNandPage);
		break;
	default:
		break;
	}
}

uint16_t Emulator::nandReadData() {
	switch (nandCmd_) {
	case 0x70: return nandStatus_ | (wp_ ? 0 : 0x80) | 0x40;
	case 0x90: {
		static const uint8_t id[4] = { 0xec, 0x56, 0xa5, 0xc0 };   // Samsung, 64 MiB 3.3 V x16
		uint8_t v = id[nandIdPos_ & 3];
		nandIdPos_++;
		return v;
	}
	case 0x00: case 0x01: case 0x50: {
		if (nandCol_ >= 264) return 0xffff;
		const uint8_t *p = nandPagePtr(nandPage_);
		uint16_t v = (uint16_t)(p[2 * nandCol_] | (p[2 * nandCol_ + 1] << 8));
		nandCol_++;
		return v;
	}
	default:
		return 0xffff;
	}
}

void Emulator::nandWriteData(uint16_t v) {
	if (nandCmd_ != 0x80) return;
	if (nandCol_ < 264) nandBuf_[nandCol_++] = v;
}

// ── PCON ────────────────────────────────────────────────────────────────

bool Emulator::Pcon::i2cStart(bool read) {
	reading_ = read;
	if (!read) { ptr_ = -1; written_.clear(); }
	return true;
}

bool Emulator::Pcon::i2cWrite(uint8_t b) {
	if (ptr_ < 0) ptr_ = b;
	else regs[(ptr_++) & 0xff] = b;
	written_.push_back(b);
	return true;
}

uint8_t Emulator::Pcon::i2cRead() {
	int p = ptr_ < 0 ? 0 : ptr_;
	ptr_ = p + 1;
	if (p == PCON_COMMAND) return 0;            // PCON_CMD_Ready
	return regs[p & 0xff];
}

void Emulator::Pcon::i2cStop() {
	if (!reading_ && !written_.empty()) emu_.pconWritten(written_);
	written_.clear();
}

void Emulator::pconWritten(const std::vector<uint8_t> &b) {
	if (b.size() < 2) return;
	switch (b[0]) {
	case PCON_POWER:
		// The PCON acknowledges a power-state request by writing the new
		// state into the PXA255's slave register 5.
		powerEchoValue_ = b[1];
		powerEchoAt_ = soc_.cycles() + kCpuHz / 1000;
		soc_.kick();
		break;
	case PCON_KEYBOARD:
		if (b[1] == 0) { keyAwaitReady_ = false; soc_.kick(); }
		break;
	default:
		break;
	}
}

void Emulator::sendScancode(uint8_t code) {
	keyQueue_.push_back(code);
	soc_.kick();
}

void Emulator::setKeyboardKey(EpocKey key, bool value) {
	int k = (int)key;
	if (k >= 'a' && k <= 'z') k -= 32;
	for (const KeyMap &m : kKeys) {
		if (m.key == k) {
			if (m.code == kFnCode) fnHeld_ = value;
			sendScancode((uint8_t)(m.code | (value ? 0 : 0x80)));
			return;
		}
	}
	if (k == EStdKeyDelete) {
		// The keyboard has no Delete; the X keymap puts it on Shift+Backspace.
		if (value) { sendScancode(0x07); sendScancode(0x0d); }
		else { sendScancode(0x8d); sendScancode(0x87); }
		return;
	}
	for (const KeyMap &m : kFnKeys) {
		if (m.key == k) {
			if (value) {
				if (!fnHeld_) sendScancode(kFnCode);
				sendScancode(m.code);
			} else {
				sendScancode((uint8_t)(m.code | 0x80));
				if (!fnHeld_) sendScancode(kFnCode | 0x80);
			}
			return;
		}
	}
}

uint64_t Emulator::boardTick(uint64_t now) {
	uint64_t next = UINT64_MAX;
	if (powerEchoAt_ != UINT64_MAX) {
		if (now >= powerEchoAt_) {
			if (!soc_.i2cSlaveBusy()) {
				uint8_t msg[2] = { PCON_POWER, powerEchoValue_ };
				soc_.i2cSlaveWrite(msg, 2);
				powerEchoAt_ = UINT64_MAX;
			} else {
				next = std::min(next, now + kCpuHz / 10000);
			}
		} else {
			next = std::min(next, powerEchoAt_);
		}
	}
	if (!keyQueue_.empty()) {
		uint64_t timeout = keySentAt_ + kCpuHz / 20;
		if (keyAwaitReady_ && now < timeout) {
			next = std::min(next, timeout);
		} else if (soc_.i2cSlaveBusy()) {
			next = std::min(next, now + kCpuHz / 10000);
		} else {
			uint8_t msg[2] = { PCON_KEYBOARD, keyQueue_.front() };
			keyQueue_.pop_front();
			soc_.i2cSlaveWrite(msg, 2);
			keyAwaitReady_ = true;
			keySentAt_ = now;
			if (!keyQueue_.empty()) next = std::min(next, now + kCpuHz / 20);
		}
	}
	return next;
}

PXA255::I2cDevice *Emulator::i2cDevice(uint8_t addr7) {
	switch (addr7) {
	case 0x13: return &pcon_;
	case 0x32: return &rtc_;
	case 0x53: return &eeprom_;
	default: return nullptr;
	}
}

// ── Ricoh RTC ───────────────────────────────────────────────────────────

int64_t Emulator::RicohRtc::now() const {
	// Host time at boot plus guest seconds since, plus whatever the guest set.
	return (int64_t)emu_.bootUnixTime_ + (int64_t)(emu_.soc_.cycles() / kCpuHz) + offset;
}

namespace {
inline uint8_t toBcd(int v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }
inline int fromBcd(uint8_t v) { return (v >> 4) * 10 + (v & 0xf); }
// Days from 1970-01-01 to y-m-d (proleptic Gregorian), and back.
int64_t daysFromCivil(int y, int m, int d) {
	y -= m <= 2;
	int64_t era = (y >= 0 ? y : y - 399) / 400;
	int64_t yoe = y - era * 400;
	int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + doe - 719468;
}
void civilFromDays(int64_t z, int &y, int &m, int &d) {
	z += 719468;
	int64_t era = (z >= 0 ? z : z - 146096) / 146097;
	int64_t doe = z - era * 146097;
	int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	int64_t mp = (5 * doy + 2) / 153;
	d = (int)(doy - (153 * mp + 2) / 5 + 1);
	m = (int)(mp < 10 ? mp + 3 : mp - 9);
	y = (int)(yoe + era * 400 + (m <= 2));
}
} // namespace

bool Emulator::RicohRtc::i2cStart(bool read) {
	reading_ = read;
	if (!read) { first_ = true; written_ = 0; }
	return true;
}

bool Emulator::RicohRtc::i2cWrite(uint8_t b) {
	if (first_) { ptr_ = b >> 4; first_ = false; return true; }
	int r = ptr_ & 15;
	pending_[r] = b;
	written_ |= (uint16_t)(1u << r);
	ptr_ = (ptr_ + 1) & 15;
	return true;
}

uint8_t Emulator::RicohRtc::i2cRead() {
	int r = ptr_ & 15;
	ptr_ = (ptr_ + 1) & 15;
	int64_t t = now();
	int64_t days = t >= 0 ? t / 86400 : (t - 86399) / 86400;
	int64_t secs = t - days * 86400;
	int y, m, d;
	civilFromDays(days, y, m, d);
	switch (r) {
	case 0: return toBcd((int)(secs % 60));
	case 1: return toBcd((int)(secs / 60 % 60));
	case 2: return toBcd((int)(secs / 3600));
	case 3: return toBcd((int)((days + 4) % 7 + 7) % 7);      // 1970-01-01 was a Thursday
	case 4: return toBcd(d);
	case 5: return (uint8_t)(toBcd(m) | (y >= 2000 ? 0x80 : 0));
	case 6: return toBcd(y % 100);
	case 7: return osc & 0x7f;                                 // bit 7 sticks only on an RV5C386
	case 14: return ctrl1;
	case 15: return ctrl2;
	default: return 0;
	}
}

void Emulator::RicohRtc::i2cStop() {
	if (reading_ || !written_) return;
	if (written_ & 0x7f) {
		// A time write: rebuild the date from the registers written, taking
		// the rest from the current time.
		int64_t t = now();
		int64_t days = t >= 0 ? t / 86400 : (t - 86399) / 86400;
		int64_t secs = t - days * 86400;
		int y, m, d;
		civilFromDays(days, y, m, d);
		int sec = (int)(secs % 60), min = (int)(secs / 60 % 60), hour = (int)(secs / 3600);
		if (written_ & 0x01) sec = fromBcd(pending_[0] & 0x7f);
		if (written_ & 0x02) min = fromBcd(pending_[1] & 0x7f);
		if (written_ & 0x04) hour = fromBcd(pending_[2] & 0x3f);
		if (written_ & 0x10) d = fromBcd(pending_[4] & 0x3f);
		if (written_ & 0x20) m = fromBcd(pending_[5] & 0x1f);
		if (written_ & 0x40) y = ((pending_[5] & 0x80) ? 2000 : 1900) + fromBcd(pending_[6]);
		int64_t want = daysFromCivil(y, m, d) * 86400 + hour * 3600 + min * 60 + sec;
		offset += want - t;
	}
	if (written_ & 0x80) osc = pending_[7] & 0x7f;
	if (written_ & 0x4000) ctrl1 = pending_[14];
	if (written_ & 0x8000) ctrl2 = pending_[15];
	written_ = 0;
}

// ── WM9705 ──────────────────────────────────────────────────────────────

void Emulator::ac97ColdReset() {
	std::memset(codec_, 0, sizeof(codec_));
	codec_[0x02 >> 1] = 0x8000;
	codec_[0x04 >> 1] = 0x8000;
	codec_[0x06 >> 1] = 0x8000;
	codec_[0x18 >> 1] = 0x8808;
	codec_[0x2c >> 1] = 0xbb80;
	codec_[0x32 >> 1] = 0xbb80;
}

uint16_t Emulator::ac97Read(uint8_t reg) {
	reg &= 0x7e;
	switch (reg) {
	case 0x00: return 0x6150;                   // reset: capabilities
	case 0x26: return (uint16_t)((codec_[0x26 >> 1] & 0xff00) | 0x000f);   // ADC/DAC/analogue/ref ready
	case 0x28: return 0x0001;                   // extended audio: variable rate
	case 0x7a: {                                // digitiser read-back
		uint16_t v = penDown_ ? 0x8000 : 0;
		uint16_t sel = codec_[0x76 >> 1] & 0x7000;
		if (sel == 0x1000) v |= 0x1000 | touchX_;
		else if (sel == 0x2000) v |= 0x2000 | touchY_;
		else if (sel == 0x3000) v |= 0x3000 | (penDown_ ? 100 : 0xfff);
		return v;
	}
	case 0x7c: return 0x574d;                   // 'WM'
	case 0x7e: return 0x4c05;                   // WM9705
	default: return codec_[reg >> 1];
	}
}

void Emulator::ac97Write(uint8_t reg, uint16_t v) {
	reg &= 0x7e;
	if (reg == 0x76) v &= 0x7fff;               // a poll completes at once
	codec_[reg >> 1] = v;
}

uint16_t Emulator::ac97ModemRead() {
	// Continuous mode: the digitiser streams X, Y, pressure into slot 5
	// while the pen is down.
	if (!penDown_ || !(codec_[0x76 >> 1] & 0x0400)) return 0;
	uint16_t v;
	switch (modemPhase_) {
	case 0: v = 0x1000 | touchX_; break;
	case 1: v = 0x2000 | touchY_; break;
	default: v = 0x3000 | 100; break;
	}
	modemPhase_ = (modemPhase_ + 1) % 3;
	return v;
}

void Emulator::setPen(bool down) {
	if (down == penDown_) return;
	penDown_ = down;
	modemPhase_ = 0;
	soc_.setGpioInput(GPIO_PEN, down);
}

// The touch panel's raw ADC readings for a screen position: linear across the
// driver's default window (the wm97xx abs_x / abs_y module parameters).
uint16_t Emulator::touchAdcX(int x) { return (uint16_t)(350 + x * (3900 - 350) / (kLcdW - 1)); }
uint16_t Emulator::touchAdcY(int y) { return (uint16_t)(320 + y * (3750 - 320) / (kLcdH - 1)); }

void Emulator::updateTouchInput(int32_t x, int32_t y, bool down) {
	x = std::clamp<int32_t>(x, 0, kLcdW - 1);
	y = std::clamp<int32_t>(y, 0, kLcdH - 1);
	touchX_ = touchAdcX(x);
	touchY_ = touchAdcY(y);
	setPen(down);
}

void Emulator::seedEeprom() {
	// Factory touch calibration, where xtscal -e2cal (run by the session's
	// gpe-xcalibrate.sh on the first X start) reads it: bytes 21-40, five
	// big-endian (x, y) raw readings taken at the screen points it uses —
	// the centre, then 80/60 in from each corner, anticlockwise from top
	// left. Written from the same mapping the emulated panel uses, so the
	// calibration the guest computes is exact. Everything else stays erased.
	static const int pts[5][2] = { { 400, 300 }, { 80, 60 }, { 80, 540 }, { 720, 540 }, { 720, 60 } };
	std::fill(eeprom_.data, eeprom_.data + 128, 0xff);
	for (int i = 0; i < 5; i++) {
		uint16_t x = touchAdcX(pts[i][0]), y = touchAdcY(pts[i][1]);
		uint8_t *p = eeprom_.data + 21 + i * 4;
		p[0] = (uint8_t)(x >> 8); p[1] = (uint8_t)x;
		p[2] = (uint8_t)(y >> 8); p[3] = (uint8_t)y;
	}
}

// ── Diagnostics ─────────────────────────────────────────────────────────

std::string Emulator::kernelLog() const {
	// log_buf lives in the kernel image; find it by its first line.
	const uint8_t *ram = soc_.ram();
	static const char kHead[] = "<5>Linux version";
	const uint8_t *end = ram + std::min<uint32_t>(soc_.ramSize(), 0x02000000u);
	const uint8_t *p = std::search(ram + 0x400000, end, kHead, kHead + sizeof(kHead) - 1);
	if (p == end) return std::string();
	const uint8_t *q = p;
	while (q < end && *q) q++;
	return std::string((const char *)p, (size_t)(q - p));
}

} // namespace NetbookPro
