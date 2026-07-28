// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include "arm710.h"
#include "v30.h"
#include "hd6303.h"
#include <cstdarg>
#include <cstdio>
#include <functional>
#include <unordered_set>
#include <utility>
#include <vector>

// Forward declaration for the V30 CPU used by NEC-V30-based Psions
// (Series 3 / 3a / 3c / 3mx / Siena / Workabout). EmuBase exposes a
// virtual getV30Cpu() that V30-based subclasses override; ARM-based
// subclasses leave it returning nullptr. The full V30 class lives in
// core/v30.h (a parallel workstream); only the forward declaration is
// needed here.
class V30;

// HD6303 is included above (used by the Psion Organiser II — 1986 8-bit
// handheld, predates the SIBO/EPOC families). Same surface pattern as
// V30: getHd6303Cpu() defaults to nullptr; the Organiser II driver
// overrides it.

enum EpocKey {
	EStdKeyDial = 161,
	EStdKeyOff = 160,
	EStdKeyHelp = 159,
	EStdKeyDictaphoneRecord = 158,
	EStdKeyDictaphoneStop = 157,
	EStdKeyDictaphonePlay = 156,
	EStdKeySliderUp = 155,
	EStdKeySliderDown = 154,
	EStdKeyDecContrast = 153,
	EStdKeyIncContrast = 152,
	EStdKeyBacklightToggle = 151,
	EStdKeyBacklightOff = 150,
	EStdKeyBacklightOn = 149,
	EStdKeyMenu = 148,
	EStdKeyNkpFullStop = 147,
	EStdKeyNkp0 = 146,
	EStdKeyNkp9 = 145,
	EStdKeyNkp8 = 144,
	EStdKeyNkp7 = 143,
	EStdKeyNkp6 = 142,
	EStdKeyNkp5 = 141,
	EStdKeyNkp4 = 140,
	EStdKeyNkp3 = 139,
	EStdKeyNkp2 = 138,
	EStdKeyNkp1 = 137,
	EStdKeyNkpEnter = 136,
	EStdKeyNkpPlus = 135,
	EStdKeyNkpMinus = 134,
	EStdKeyNkpAsterisk = 133,
	EStdKeyNkpForwardSlash = 132,
	EStdKeyEquals = 131,
	EStdKeyMinus = 130,
	EStdKeySquareBracketRight = 129,
	EStdKeySquareBracketLeft = 128,
	EStdKeyHash = 127,
	EStdKeySingleQuote = 126,
	EStdKeySemiColon = 125,
	EStdKeyBackSlash = 124,
	EStdKeyForwardSlash = 123,
	EStdKeyFullStop = 122,
	EStdKeyComma = 121,
	EStdKeyXXX = 120,
	EStdKeyF24 = 119,
	EStdKeyF23 = 118,
	EStdKeyF22 = 117,
	EStdKeyF21 = 116,
	EStdKeyF20 = 115,
	EStdKeyF19 = 114,
	EStdKeyF18 = 113,
	EStdKeyF17 = 112,
	EStdKeyF16 = 111,
	EStdKeyF15 = 110,
	EStdKeyF14 = 109,
	EStdKeyF13 = 108,
	EStdKeyF12 = 107,
	EStdKeyF11 = 106,
	EStdKeyF10 = 105,
	EStdKeyF9 = 104,
	EStdKeyF8 = 103,
	EStdKeyF7 = 102,
	EStdKeyF6 = 101,
	EStdKeyF5 = 100,
	EStdKeyF4 = 99,
	EStdKeyF3 = 98,
	EStdKeyF2 = 97,
	EStdKeyF1 = 96,
	EStdKeyScrollLock = 28,
	EStdKeyNumLock = 27,
	EStdKeyCapsLock = 26,
	EStdKeyRightFunc = 25,
	EStdKeyLeftFunc = 24,
	EStdKeyRightCtrl = 23,
	EStdKeyLeftCtrl = 22,
	EStdKeyRightAlt = 21,
	EStdKeyLeftAlt = 20,
	EStdKeyRightShift = 19,
	EStdKeyLeftShift = 18,
	EStdKeyDownArrow = 17,
	EStdKeyUpArrow = 16,
	EStdKeyRightArrow = 15,
	EStdKeyLeftArrow = 14,
	EStdKeyDelete = 13,
	EStdKeyInsert = 12,
	EStdKeyPageDown = 11,
	EStdKeyPageUp = 10,
	EStdKeyEnd = 9,
	EStdKeyHome = 8,
	EStdKeyPause = 7,
	EStdKeyPrintScreen = 6,
	EStdKeySpace = 5,
	EStdKeyEscape = 4,
	EStdKeyEnter = 3,
	EStdKeyTab = 2,
	EStdKeyBackspace = 1,
	EStdKeyNull = 0
};

// CPU-agnostic emulator base.
//
// Historically EmuBase was `class EmuBase : public ARM710` — every device
// subclass IS-A ARM710. That hard-coupling made it impossible to add a
// V30-based device (Series 3 / 3a / 3c / 3mx / Siena / Workabout) under
// the same interface, since the V30 CPU has a completely different
// register file and instruction model.
//
// In the composed model:
//   - EmuBase owns nothing CPU-specific itself.
//   - ARM-based subclasses (Windermere, CLPS7111, Series5, Revo, MC218)
//     compose an `Arm710Bridge` member (see below) and override
//     getArmCpu().
//   - V30-based subclasses (forthcoming) compose a V30 member and
//     override getV30Cpu().
//
// To keep the diff small in subclass `.cpp` files (windermere.cpp alone
// contains ~200 unprefixed calls to inherited ARM710 methods like log(),
// getGPR(), requestFIQ(), tick() etc.), EmuBase exposes thin forwarders
// for the ARM710 public surface. Calls in subclass code resolve to these
// forwarders, which dispatch through getArmCpu(). The forwarders are
// inline and zero-cost.
class EmuBase
{
protected:
#ifndef __EMSCRIPTEN__
	std::unordered_set<uint32_t> _breakpoints;
#endif
	int64_t passedCycles = 0;
	int64_t nextTickAt = 0;
	uint8_t readKeyboard(int kScan);

public:
	// Re-exposed ARM710 type aliases / enum values so subclass code that
	// writes `V8`, `V16`, `V32`, `ValueSize`, `MMUFault` (without the
	// ARM710:: prefix) keeps compiling unchanged.
	using ValueSize = ARM710::ValueSize;
	using MMUFault = ARM710::MMUFault;
	static constexpr ValueSize V8  = ARM710::V8;
	static constexpr ValueSize V16 = ARM710::V16;
	static constexpr ValueSize V32 = ARM710::V32;
	// MMUFault values that subclass code (.cpp) used to reach via the
	// inherited ARM710 scope. Only the ones actually referenced are
	// re-exposed here; add more if a future call site needs them.
	static constexpr MMUFault NoFault = ARM710::NoFault;

	EmuBase() = default;
	virtual ~EmuBase() = default;

	// CPU accessors. ARM-based subclasses override getArmCpu(); V30-based
	// subclasses override getV30Cpu(). Default returns nullptr so callers
	// that don't care about the CPU type can ignore the unused side.
	virtual ARM710 *getArmCpu() { return nullptr; }
	virtual const ARM710 *getArmCpu() const { return nullptr; }
	virtual V30 *getV30Cpu() { return nullptr; }
	virtual const V30 *getV30Cpu() const { return nullptr; }
	virtual HD6303 *getHd6303Cpu() { return nullptr; }
	virtual const HD6303 *getHd6303Cpu() const { return nullptr; }

	// Physical-memory operations — overridden by every device subclass and
	// driven by the CPU bridge member. Default implementations are no-ops
	// so a never-instantiated EmuBase compiles.
	virtual MaybeU32 readPhysical(uint32_t physAddr, ValueSize valueSize) {
		(void)physAddr; (void)valueSize; return std::nullopt;
	}
	virtual bool writePhysical(uint32_t value, uint32_t physAddr, ValueSize valueSize) {
		(void)value; (void)physAddr; (void)valueSize; return false;
	}

	// ── ARM710 public-surface forwarders ────────────────────────────────
	// These exist so windermere.cpp / clps7111.cpp keep compiling without
	// having to prefix every call with `cpu.`. Each forwards through
	// getArmCpu() and is a no-op (or returns a sentinel) on V30-only
	// devices. If you add a method here it must also exist on ARM710.
	void setLogger(std::function<void(const char *)> newLogger) {
		if (auto *c = getArmCpu()) c->setLogger(std::move(newLogger));
	}
	void setLoggingEnabled(bool enabled) {
		if (auto *c = getArmCpu()) c->setLoggingEnabled(enabled);
	}
	void setCfDiagEnabled(bool e) {
		if (auto *c = getArmCpu()) c->setCfDiagEnabled(e);
	}
	bool getCfDiagEnabled() const {
		auto *c = getArmCpu(); return c ? c->getCfDiagEnabled() : false;
	}
	uint32_t lastPcExecuted() const {
		if (auto *c = getArmCpu()) return c->lastPcExecuted();
		if (auto *v = getV30Cpu()) return (uint32_t(v->sregs[1]) << 4) + v->ip;
		if (auto *h = getHd6303Cpu()) return uint32_t(h->pc);
		return 0;
	}
	void setProcessorID(uint32_t v) {
		if (auto *c = getArmCpu()) c->setProcessorID(v);
	}
	bool canAcceptFIQ() const {
		auto *c = getArmCpu(); return c ? c->canAcceptFIQ() : false;
	}
	bool canAcceptIRQ() const {
		auto *c = getArmCpu(); return c ? c->canAcceptIRQ() : false;
	}
	void requestFIQ() { if (auto *c = getArmCpu()) c->requestFIQ(); }
	void requestIRQ() { if (auto *c = getArmCpu()) c->requestIRQ(); }
	void reset()      { if (auto *c = getArmCpu()) c->reset(); }
	bool instructionReady() const {
		auto *c = getArmCpu(); return c ? c->instructionReady() : false;
	}
	uint32_t tick() { auto *c = getArmCpu(); return c ? c->tick() : 0; }
	MaybeU32 readVirtualDebug(uint32_t va, ValueSize sz) {
		auto *c = getArmCpu(); return c ? c->readVirtualDebug(va, sz) : MaybeU32{};
	}
	MaybeU32 virtToPhys(uint32_t va) {
		auto *c = getArmCpu(); return c ? c->virtToPhys(va) : MaybeU32{};
	}
	std::pair<MaybeU32, MMUFault> readVirtual(uint32_t va, ValueSize sz) {
		auto *c = getArmCpu();
		return c ? c->readVirtual(va, sz)
		         : std::make_pair(MaybeU32{}, ARM710::NoFault);
	}
	MMUFault writeVirtual(uint32_t v, uint32_t va, ValueSize sz) {
		auto *c = getArmCpu(); return c ? c->writeVirtual(v, va, sz) : ARM710::NoFault;
	}
	uint32_t getGPR(int idx) const {
		auto *c = getArmCpu(); return c ? c->getGPR(idx) : 0;
	}
	void setGPR(int idx, uint32_t v) {
		if (auto *c = getArmCpu()) c->setGPR(idx, v);
	}
	uint32_t getCPSR() const {
		auto *c = getArmCpu(); return c ? c->getCPSR() : 0;
	}
	uint32_t getRealPC() const {
		auto *c = getArmCpu(); return c ? c->getRealPC() : 0;
	}
	uint32_t callRomFunctionSync(uint32_t targetPC, uint32_t arg0) {
		auto *c = getArmCpu(); return c ? c->callRomFunctionSync(targetPC, arg0) : 0;
	}
	void logPcHistory() {
		if (auto *c = getArmCpu()) c->logPcHistory();
	}
	// Variadic logger forwarder. ARM710::log is a varargs function with
	// a printf-style format string; we round-trip through vsnprintf to
	// avoid touching ARM710's signature.
	void log(const char *fmt, ...) __attribute__((format(printf, 2, 3))) {
		auto *c = getArmCpu();
		if (!c) return;
		char buf[1024];
		va_list args;
		va_start(args, fmt);
		vsnprintf(buf, sizeof(buf), fmt, args);
		va_end(args);
		c->log("%s", buf);
	}

	virtual uint8_t *getROMBuffer() = 0;
	virtual size_t getROMSize() = 0;
	virtual void loadROM(uint8_t *buffer, size_t size) = 0;
	virtual void executeUntil(int64_t cycles) = 0;

	// RAM snapshot hooks. The frontend exports a "Download RAM Snapshot"
	// button to capture RAM contents at the moment a bug surfaces (e.g.
	// the SIBO2 "Media is corrupt" dialog) so the harness can reload the
	// bytes via --ram-snapshot for offline analysis. Default no-ops let
	// devices without a meaningful host-visible RAM (Series 7) opt out.
	virtual uint8_t *getRamBuffer()       { return nullptr; }
	virtual size_t   getRamSize()  const  { return 0; }
	virtual void     loadRamSnapshot(const uint8_t *bytes, size_t size) {
		(void)bytes; (void)size;
	}

	// LCD electroluminescent backlight pin state. The frontend polls this
	// to drive the on-screen backlight overlay so it tracks whatever the
	// running EPOC kernel is doing — Fn+Space toggling, the OS-level
	// auto-off timeout, the Control Panel brightness slider, etc.
	// Default false for devices that either have no EL panel (Series 3 /
	// 3a / Osaris / Organiser II / MC400) or whose backlight pin isn't
	// modelled yet (Series 3c / 3mx — MAME hasn't wired the port pin
	// either; the relevant TODO is in reference/psion3a.cpp). Windermere
	// (Series 5 / 5mx / 5mx Pro) exposes the real PRT bit.
	virtual bool getBacklight() const { return false; }

	// Orientation the running OS is drawing the screen at, as the number
	// of quarter-turns ANTICLOCKWISE a host has to apply to the panel
	// image to show it the way the machine's user is holding the machine.
	// 0 on every device whose OS only ever draws one way up; the netpad
	// (whose Tools menu carries "Switch orientation") reports 1 while it
	// is drawing portrait — see netpadScreenOrientation in core/sa1100.cpp.
	// The framebuffer itself is unaffected: readLCDIntoBuffer keeps
	// returning the panel exactly as the LCD controller scans it.
	virtual int getScreenOrientation() const { return 0; }

	// True once the CPU is wedged in an unrecoverable prefetch-abort loop
	// (vector page unmapped after a crash).  The worker auto-halts on this
	// so a dead device stops flooding the log.  ARM-based cores report it
	// via the CPU; V30-only cores have no ARM CPU and stay false.
	virtual bool isCpuCrashed() const {
		const ARM710 *c = getArmCpu();
		return c && c->faultLoopDetected();
	}

	// ── Unique id (System → Information → Machine) ────────────────────
	// EPOC prints a 64-bit "Unique id" in four hex groups. It is built
	// from two places, and this interface exposes both:
	//
	//   • the LOW half is the factory word in the machine's identity chip
	//     (getMachineId / setMachineId) — the Series 5mx family's ETNA
	//     PROM, the SA-1100 machines' Eiger EEPROM;
	//   • the HIGH half is a model UID the kernel takes from a constant
	//     compiled into the ROM (getMachineIdPrefix / setMachineIdPrefix).
	//     Changing that means patching the ROM image, which is only done
	//     when the constant can be located unambiguously — see
	//     canSetMachineIdPrefix.
	//
	// EPOC software that licenses itself to one machine keys off the pair,
	// so the frontend exposes both behind the "Show debugging" setting.
	//
	// hasMachineId() is false by default and true only where a programmed
	// id has been OBSERVED reaching EPOC's dialog:
	//   • Series 5mx / 5mx Pro / MC218 — ETNA PROM, core/etna.h.
	//     A 5mx set to CAFEBABE reports 1000-118A-CAFE-BABE.
	//   • Series 7 / netBook / netpad — Eiger EEPROM, core/sa1100.h.
	//     A Series 7 asked for 0BADF00D-DEADBEEF reports exactly that.
	// The Series 5 and Osaris have an ETNA-shaped register block but no
	// PROM bit-bang wiring, and the Revo ROM never reads the PROM at all,
	// so nothing the guest reads would change — they stay false and the UI
	// hides the control.
	virtual bool hasMachineId() const { return false; }
	virtual uint32_t getMachineId() const { return 0; }
	// Programs the identity chip. Returns false when the device has no
	// writable one; the whole word is stored verbatim where it does. The
	// guest sees the new value on its next read of the chip; a running
	// EPOC has already cached the old one, so a reset is needed for the
	// OS itself to report the change.
	virtual bool setMachineId(uint32_t id) { (void)id; return false; }

	// The model UID EPOC prints as the high half. Zero means "not known
	// for this machine" — it is a measured constant per EPOC build, and
	// machines whose dialog hasn't been read report 0 rather than a guess,
	// which the UI shows by displaying only the half it does know.
	virtual uint32_t getMachineIdPrefix() const { return 0; }
	// True when the constant was found in the loaded ROM and can therefore
	// be patched. False leaves the high half fixed, and
	// setMachineIdPrefix() below refuses — the netBook and netpad, whose
	// model UID has never been read out of their dialog, and a 5mx Pro
	// still running its bootloader (the OS carrying the constant arrives
	// later from the CF card).
	virtual bool canSetMachineIdPrefix() const { return false; }
	// Patches the model UID in the loaded ROM image, so the whole 64-bit
	// Unique id becomes settable. Every copy of the constant is rewritten
	// together: the 5mx family carries one, the Series 7 ten (seven of
	// them in a repeated module-header field), and patching a subset of
	// the Series 7's black-screens the machine while patching all ten
	// boots cleanly. Returns false when the constant could not be located
	// (canSetMachineIdPrefix() is false), leaving the ROM untouched. The
	// patch lives in the loaded image only — reloading the ROM restores
	// it, which is why the host re-applies on every load.
	// NOTE this is the machine's model UID, not just a display field: code
	// that identifies the machine sees the new value too.
	virtual bool setMachineIdPrefix(uint32_t prefix) { (void)prefix; return false; }

	// Helpers for the model-UID patch above. The constant is a plain
	// 32-bit literal in the ROM image, so a device locates every
	// occurrence once at load time and rewrites exactly those offsets on
	// each change. Recording the offsets (rather than re-scanning for the
	// current value) matters: a user-supplied UID could be a byte pattern
	// that occurs all over the ROM — 00000000 would match thousands of
	// words — and a re-scan would then splatter it.
	//
	// Public because they are stateless byte-buffer utilities with no
	// emulator state behind them, and tests/unit/machine_id_test drives
	// them directly.
	static void findRomWords(const uint8_t *rom, size_t size, uint32_t value,
	                         std::vector<size_t> &out);
	static void writeRomWords(uint8_t *rom, const std::vector<size_t> &offsets,
	                          uint32_t value);
	// Same scan, over a storage-card image rather than the ROM buffer, for
	// the two machines whose OS is not in their own flash at all: the 5mx
	// Pro (128 KB bootloader; the OS is SYS$ROM.BIN on the card) and the
	// netBook (2 MB YModem bootloader; the OS is D:\OS.IMG). Patching ROM[]
	// on those does nothing to the id the booted OS prints, because the OS
	// carrying the constant arrives later, off the card.
	//
	// Unlike the ROM scan this matches TWO values — the factory constant
	// and whatever is programmed now — because a card patched in an earlier
	// session already carries the latter, and the frontend hands the same
	// image back on the next boot. Both are specific 32-bit literals, so
	// the chance of a stray hit in a card image is negligible; the caller
	// still records the offsets once and rewrites only those afterwards,
	// so a subsequent id (0 included) can't splatter.
	static void findCardMachineIdWords(const uint8_t *card, size_t size,
	                                   uint32_t factory, uint32_t current,
	                                   std::vector<size_t> &out);

	// Storage-card (CompactFlash / PC Card) hooks. Default implementations
	// are no-ops so devices without a card slot can ignore them.
	virtual bool attachCard(const uint8_t *bytes, size_t size) { (void)bytes; (void)size; return false; }
	// Replace the card image bytes WITHOUT signalling a card removal/insert to
	// the OS — the socket stays "inserted" and the mount stays alive.  Used to
	// push edited contents (same-geometry card) so the device re-reads the new
	// bytes on its next disk access, sidestepping the hot-swap re-mount path.
	// Default: fall back to a full re-attach.
	virtual bool updateCardImageInPlace(const uint8_t *bytes, size_t size) {
		return attachCard(bytes, size);
	}
	virtual void detachCard() {}
	virtual bool isCardInserted() const { return false; }
	virtual size_t getCardImageSize() const { return 0; }
	virtual const uint8_t *getCardImageData() const { return nullptr; }

	// ── Psion SSD pack hooks (SIBO Series 3 family + Siena) ────────────
	// Slot indexing matches the user-visible "Pack A / Pack B" labels.
	// Series 3 / 3a / 3c / 3mx have 2 slots; Siena has 1; ARM-based
	// devices have 0. See core/psion_ssd.{h,cpp} for the pack model.
	virtual int  getSsdSlotCount() const { return 0; }
	// `ssdType` selects the pack type presented in the SIBO info byte:
	// 0 = auto (sniff 0xF1A5 magic -> Flash, else RAM), 1 = RAM,
	// 2 = Type 1 Flash, 3 = hardware write-protected. Mirrors
	// PsionSSD::Type. On real packs the type comes from hardware straps,
	// not the contents, so callers that know the type (the SSD dialog,
	// the harness) should pass it explicitly.
	virtual bool attachSSD(int slot, const uint8_t *bytes, size_t size,
	                       int ssdType = 0) {
		(void)slot; (void)bytes; (void)size; (void)ssdType; return false;
	}
	virtual void detachSSD(int slot) { (void)slot; }
	virtual bool   isSSDInserted(int slot)   const { (void)slot; return false; }
	virtual size_t getSSDImageSize(int slot) const { (void)slot; return 0; }
	virtual const uint8_t *getSSDImageData(int slot) const {
		(void)slot; return nullptr;
	}

	// ── Psion Organiser II Datapak / Rampak hooks ─────────────────────
	// Distinct from the SIBO SSD pack interface above: the Organiser II
	// (1986) predates SIBO and uses a different bit-banged pack protocol
	// (modelled in core/psion_datapak.cpp). The frontend dialog (Datapak
	// vs SSD) is also distinct because Datapaks have a kind discriminant
	// — read-only EPROM (Datapak) vs writable battery-backed SRAM
	// (Rampak) — that the SIBO SSD UI doesn't surface. Slot indexing
	// matches the user-visible "Pack A / Pack B" labels on the device.
	enum class PackKind { None = 0, Datapak = 1, Rampak = 2 };
	virtual int  getDatapakSlotCount() const { return 0; }
	virtual bool attachDatapak(int slot, const uint8_t *bytes, size_t size) {
		(void)slot; (void)bytes; (void)size; return false;
	}
	virtual void detachDatapak(int slot) { (void)slot; }
	virtual bool   isDatapakInserted(int slot)   const { (void)slot; return false; }
	virtual size_t getDatapakImageSize(int slot) const { (void)slot; return 0; }
	virtual const uint8_t *getDatapakImageData(int slot) const {
		(void)slot; return nullptr;
	}
	virtual PackKind getDatapakKind(int slot) const {
		(void)slot; return PackKind::None;
	}
	// Experimental: select which pendingInterrupts bit ETNA's CF IREQ# gets
	// mirrored onto. -1 (default) means no routing. Used by the native
	// harness to sweep candidate Windermere IRQ lines.
	virtual void setCfIrqLine(int bit) { (void)bit; }
	// Experimental: select the strategy used to re-enable the CF IREQ# mask
	// bit after the kernel dispatcher clears it. 0=off, 1=intenc-block,
	// 2=deassert-user, 3=deassert-nonirq, 4=intclear, 5=level, 6=delayed.
	// Default 0 (off). See Windermere::runCfReenableStrategy for details.
	virtual void setCfReenableMode(int mode) { (void)mode; }
	// Experimental workaround: when enabled, detect the "CF stuck in idle"
	// state (card has IREQ# asserted, EINT3 mask off, CPU spinning in the
	// EKA1 null-thread idle loop at 0x5000b55c in Undefined32 mode) and
	// force-set the kernel's reschedule-needed flag at physical
	// 0xd07e5878 / virtual 0x80000878. The IRQ handler at 0x50004980
	// checks that flag on exit and, if non-zero, calls the full
	// rescheduler — which picks up the CF DFC thread blocked on its
	// semaphore. This compensates for the fact that TDfc::Add() from IRQ
	// context silently fails in our ARM710 + EKA1 emulator pairing. Off
	// by default. See Emulator::cfReschedulePoke for details.
	virtual void setCfReschedulePoke(bool enable) { (void)enable; }
	// Experimental speed-up: intercept TC2LOAD writes during the CF "gap"
	// (card inserted, IREQ# asserted, EINT3 masked off) and divide them
	// by a fixed factor so the nanokernel tick runs ~30x faster. This
	// collapses the CF driver's 67 * 30 ms busy-timeout polling loop
	// from 2010 ms/sector down to ~67 ms/sector, without touching the
	// ROM or the normal timer path. Off by default. See
	// Windermere::Emulator::cfFastWatchdog.
	virtual void setCfFastWatchdog(bool enable) { (void)enable; }
	// Disable the 2s->100us CF retry-timer rewrite. Default on; flip off
	// only for A/B-comparing against the unaccelerated baseline.
	virtual void setCfAccelTimer(bool enable) { (void)enable; }
	// Direct-invoke the PCCARD-ATA retry callback (0x50083178) from
	// emulator C++ via ARM710::callRomFunctionSync. Opt-in only —
	// experimental. Off by default.
	virtual void setCfDirectInvoke(bool enable) { (void)enable; }
	// Snapshot of CF-path counters used by the native harness to score
	// candidate re-enable strategies. Zero-valued defaults mean the device
	// has no CF slot (MC218 also overrides this).
	struct CfStats {
		uint32_t ataCommands = 0;
		uint32_t sectorDrains = 0;
		uint32_t irqAssertions = 0;
		uint32_t irqDeassertions = 0;
		uint32_t eint3Dispatches = 0;
		uint32_t reschedulePokes = 0;
		uint32_t accelTimerHits = 0;
	};
	virtual CfStats getCfStats() const { return {}; }

	// True when the emulator is stuck in the 2 s-per-sector CF polling gap.
	// The frontend uses this to burst extra stepFrames per RAF so the
	// user-visible wall-clock latency of a file-copy is compressed to a few
	// hundred ms rather than minutes. Default false for devices with no
	// CF slot or while the card is idle.
	virtual bool cfGapActive() const { return false; }

	// Audio codec hooks. Default no-op so devices without a codec emulation
	// path compile unchanged and the frontend suppresses the UI.
	// Host samples are normalised 16-bit signed PCM mono; each emulator
	// subclass converts to/from its native register format internally.
	virtual bool hasAudio() const { return false; }
	// True when the device hardware has a microphone (recording input).
	// Defaults to hasAudio() so every EPOC32 machine (which all have
	// mics) is unchanged; speaker-only SIBO devices override to false so
	// the frontend hides the Mic toggle the hardware never had.
	virtual bool hasMicrophone() const { return hasAudio(); }
	virtual int getAudioSampleRate() const { return 0; }
	// Drains the speaker DAC queue into `dst` (up to `maxSamples`).
	// Returns the number of samples actually written. If the host has muted
	// the speaker the queue is still drained (to keep EPOC's FIFO draining)
	// but the caller receives zeros / nothing usable.
	virtual size_t readAudioOutput(int16_t *dst, size_t maxSamples) {
		(void)dst; (void)maxSamples; return 0;
	}
	// Pushes host mic samples into the microphone ADC queue.
	virtual void writeAudioInput(const int16_t *src, size_t count) {
		(void)src; (void)count;
	}
	// Host-side gate for each stream; independent of EPOC's own codec enable.
	virtual void setHostAudioEnabled(bool speaker, bool mic) {
		(void)speaker; (void)mic;
	}

	virtual int32_t getClockSpeed() const = 0;
	virtual const char *getDeviceName() const = 0;
	virtual int getDigitiserWidth() const = 0;
	virtual int getDigitiserHeight() const = 0;
	virtual int getLCDOffsetX() const = 0;
	virtual int getLCDOffsetY() const = 0;
	virtual int getLCDWidth() const = 0;
	virtual int getLCDHeight() const = 0;
	virtual void readLCDIntoBuffer(uint8_t **lines, bool is32BitOutput) const = 0;
	virtual void setKeyboardKey(EpocKey key, bool value) = 0;
	virtual void updateTouchInput(int32_t x, int32_t y, bool down) = 0;

#ifndef __EMSCRIPTEN__
	std::unordered_set<uint32_t> &breakpoints() { return _breakpoints; }
#endif
	uint64_t currentCycles() const { return passedCycles; }
};

// Concrete ARM710 subclass that bridges the CPU's pure-virtual physical
// memory operations back to the owning EmuBase device. Devices compose an
// Arm710Bridge member instead of inheriting ARM710 themselves; the bridge
// forwards readPhysical/writePhysical to the owner so the device's
// existing implementations of those two methods continue to be the
// authoritative source of truth.
class Arm710Bridge : public ARM710 {
	EmuBase *owner;
public:
	Arm710Bridge(EmuBase *o, bool isTVersion) : ARM710(isTVersion), owner(o) {}
	EmuBase *getOwner() const { return owner; }
	MaybeU32 readPhysical(uint32_t physAddr, ValueSize valueSize) override {
		return owner->readPhysical(physAddr, valueSize);
	}
	bool writePhysical(uint32_t value, uint32_t physAddr, ValueSize valueSize) override {
		return owner->writePhysical(value, physAddr, valueSize);
	}
};

