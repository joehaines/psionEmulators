// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2019 Ash Wolf (WindEmu); modifications (c) 2024-2026 Joe Haines.
// Subject to the Mozilla Public License v2.0 (http://mozilla.org/MPL/2.0/).

#pragma once
#include "emubase.h"
#include "clps7111_defs.h"
#include "clps7600.h"
#include "hardware.h"
#include "etna.h"
#include "vcfcard.h"
#include "audio_codec.h"
#include <array>

namespace CLPS7111 {
class Emulator : public EmuBase {
public:
	// Composed ARM710 (non-T variant for the CL-PS7111 family — MC218 /
	// Osaris / Series 5 use a plain ARM710 part, not the ARM710T used in
	// Windermere). Public so the CLPS7600 PC-card controller and other
	// helpers that take a raw ARM710* in their constructors can hook in.
	Arm710Bridge cpu{this, false};
	ARM710 *getArmCpu() override { return &cpu; }
	const ARM710 *getArmCpu() const override { return &cpu; }

	uint8_t ROM[0x800000];
	uint8_t ROM2[0x40000];
	// 8 MB of RAM so the Series 5 kernel — which maps its data page near the
	// top of an 8 MB bank — has somewhere to live. MC218 / Osaris only touch
	// the first 4 MB and keep their original mirror behaviour via the
	// getRamMask() / extendedRamRegion() virtuals below.
	uint8_t MemoryBlockC0[0x800000];
	enum { MemoryBlockMask = 0x3FFFFF };

	// 2 KB on-chip SRAM mapped at CS6 (physical 0x60000000-0x600007FF) per
	// CL-PS7111 datasheet section 3.10 / 5.10.1: "The memory area decoded
	// by CS6 is reserved for the 2 Kbytes of on-chip SRAM and does not
	// require a configuration field in MEMCFG2." Always active, 32-bit, no
	// wait state. The Series 5 ROM references addresses in this region
	// (e.g., 11+ literal-pool entries pointing at 0x60000000-0x60000800),
	// so without modeling it the kernel reads back 0xFFFFFFFF.
	uint8_t OnChipSRAM[0x800] = {};

protected:
	// Subclasses override these to opt into the larger RAM layout. The
	// defaults preserve MC218 / Osaris behaviour exactly.
	virtual uint32_t getRamMask() const { return MemoryBlockMask; }
	// If true, reads/writes to physical region 0xD route to RAM (offset
	// by getRegionDRamOffset() into MemoryBlockC0). Real CL-PS7110 hardware
	// supports 4 DRAM banks at 256 MB strides (0xC0000000, 0xD0000000,
	// 0xE0000000, 0xF0000000 — see CL-PS7110.pdf §1.2.5). Series 5 hardware
	// has TWO banks of 4 MB each: Bank 0 at 0xC0000000-0xC03FFFFF and Bank 1
	// at 0xD0000000-0xD03FFFFF, each repeating within its 256 MB segment.
	// Modelling them as separate physical RAM (NOT a single mirrored 8 MB)
	// matters because the kernel writes to both 0xC0xxxxxx and 0xD0xxxxxx
	// independently, and aliasing them collides distinct kernel state.
	virtual bool aliasDRegionToRam() const { return false; }
	// Offset into MemoryBlockC0 for region 0xD accesses. Default 0 means
	// "alias to bank 0" (the historical behaviour); 0x400000 means "bank 1
	// is the upper 4 MB of MemoryBlockC0", matching the Series 5 hardware
	// layout per the schematic + photo of the motherboard (two matched
	// DRAM chips visible).
	virtual uint32_t getRegionDRamOffset() const { return 0; }
	// Region 2 = external chip-select nCS1 aperture (0x20000000-0x2FFFFFFF).
	// MC218 / Osaris don't wire anything there. Series 5 has a small ETNA-like
	// companion chip that the EPOC R1 kernel pokes at early boot (e.g. writes
	// 0x80 to 0x2000000C, reads a handful of status regs). Subclasses that
	// need a custom response override these. The default base-class behaviour
	// (read = 0, write = swallow) preserved for non-Series-5 devices: those
	// override returning {} means "no custom mapping, fall through to default".
	virtual MaybeU32 readRegion2(uint32_t physAddr, ValueSize valueSize) const { return {}; }
	virtual bool writeRegion2(uint32_t value, uint32_t physAddr, ValueSize valueSize) { return false; }
	// Region 3 = external chip-select nCS2 aperture (0x30000000-0x3FFFFFFF).
	// Same contract as region 2, and the same default: return {} to leave
	// the access unmapped, which is what every device registered before the
	// Geofox does (nothing is wired there on a Series 5 / Osaris / MC218,
	// and none of their ROMs touch it). The Geofox One DOES have a chip on
	// nCS2 — its ROM reads a power/battery status byte from 0x30000000 once
	// a second — so it claims the window; see core/geofox.h.
	virtual MaybeU32 readRegion3(uint32_t physAddr, ValueSize valueSize) const { return {}; }
	virtual bool writeRegion3(uint32_t value, uint32_t physAddr, ValueSize valueSize) { return false; }
	// First refusal on an SSI (SYNCIO) frame. The control byte is the low
	// byte of the request the kernel wrote; returning a value answers the
	// frame with it and skips the ADC decode below. Default {} = "not
	// mine", which leaves every existing device on the ADC paths exactly
	// as before. The Geofox uses it for the configuration PROM its variant
	// driver reads at boot, which sits on the SSI bus where the other
	// machines have only their ADC.
	//
	// Non-const because an SSI peripheral is a stateful bus device, not a
	// lookup table: the Geofox's mouse pad answers two consecutive frames
	// of the same control byte with the two halves of one movement packet,
	// so responding to a frame advances the peripheral. Reading a register
	// (readReg32) was never const either, so nothing else had to change.
	virtual MaybeU32 syncioResponse(uint8_t controlByte) { (void)controlByte; return {}; }
	// How the keyboard matrix lands on ports A and B when the kernel reads
	// them. The defaults are the expressions readReg8 used inline before
	// these existed, so every device but the Geofox is byte-identical:
	// port A carries seven row bits plus GPIO bit 7, port B carries the
	// four "extra" modifier keys inverted in its high nibble. The Geofox
	// wires its matrix differently — twelve rows, eight on port A and four
	// in port B's LOW nibble, none inverted — so it overrides both.
	virtual uint8_t composePortA() const {
		return (uint8_t)(((portValues >> 24) & 0x80) | (readKeyboard() & 0x7F));
	}
	virtual uint8_t composePortB() const {
		return (uint8_t)(((portValues >> 16) & 0x0F) | ((keyboardExtra ^ 0xF) << 4));
	}
	// Chip-variant virtuals. Defaults match CL-PS7111 (MC218 / Osaris). The
	// CL-PS7110 (Series 5) has a smaller register block — overrides return
	// false to make those registers behave as datasheet-specified
	// "reserved" (read undefined, write no-op). See reference/CL-PS7110.pdf
	// Table 3-2: the PS7110 register block ends at 0x880, and the upper-
	// half registers (FRBADDR=0x1000, SYSCON2=0x1100, SYSFLG2=0x1140,
	// INTSR2=0x1240, INTMR2=0x1280, UARTDR2=0x1480, UBRLCR2=0x14C0,
	// KBDEOI=0x1700) are PS7111 extensions.
	virtual bool chipHasFRBADDR()    const { return true; }
	virtual bool chipHasSysCon2()    const { return true; }
	// 2 KB on-chip SRAM mapped at CS6 — PS7111 only. PS7110 leaves CS6 as
	// a generic external chip-select; on a real Series 5 nothing is wired
	// there per the schematic walk. When this returns false we drop the
	// SRAM backing and respond to CS6 reads with open-bus 0xFF.
	virtual bool chipHasOnChipSRAM() const { return true; }
	// On-die CLPS7600 PCMCIA controller register window at 0x4C000000.
	// CL-PS7111 (Osaris, MC218, 5mx Pro flash bootloader) has it. CL-PS7110
	// (Series 5) does not — instead the region-4 windows 0x4C-0x4F are used
	// as additional CF I/O apertures by EPOC R1's CF driver (it writes LBA
	// bytes to 0x4C000004/005 and reads the data register at 0x4C000000
	// V32). When false, region-4 falls through to CF I/O for those addresses
	// and never dispatches into the CLPS7600 emulator.
	virtual bool chipHasCLPS7600()   const { return true; }
	// SYSCON1 is 24-bit on PS7110, 32-bit on PS7111. Override to mask
	// writes so reserved bits stay zero.
	virtual uint32_t sysConMask()    const { return 0xFFFFFFFFu; }
	// Called on every 8-bit write to PBDR with the port state before and
	// after (port B in bits 16..23). The Series 5 clocks its settings
	// PROM over bits 0 (select) and 1 (clock) — see Series5::Emulator.
	virtual void onPortBWrite(uint32_t oldPorts, uint32_t newPorts) { (void)oldPorts; (void)newPorts; }
	// Default value returned for unrecognised SYNCIO read requests.
	// 0xFFFFFFFF (open-bus floating-high) matches the WindEmu reference
	// emulator and lets Osaris/MC218/5mx boot. The CL-PS7110 Series 5
	// has a different ADC wired up that DOES respond, so Series 5
	// overrides this to 0x800 (mid-scale) to keep its kernel on the
	// "pen up + alignment-fault retry" code path which is paradoxically
	// the most productive branch in our current emulator.
	virtual uint32_t defaultSyncioReadValue() const { return 0xFFFFFFFFu; }
	// Raw 12-bit ADC reading for the PC-card socket Vcc-sense channel
	// (Series 5 SYNCIO control byte 0xE1). EPOC R1's PCMCIA PSU power-up
	// (DPlatPcCardVcc) samples this channel and will not enumerate the card
	// until it reads ~3.3 V; the supply is sensed through a divider so the
	// expected raw reading is ~1320 mV. Default 0 = no socket-Vcc sense wired
	// (every other device returns 0, leaving channel 0xE1 reading 0 as
	// before). Series 5 overrides this.
	virtual uint32_t socketVccSenseAdc() const { return 0; }
	// Default first-IRQ delay (in CPU cycles). PS7111 family (MC218 /
	// Osaris / 5mx) defaults to 0 = IRQs allowed immediately. Series 5
	// overrides to 10 million cycles (~0.5 sim sec). Discovered via
	// timing sweep: deferring IRQs lets the Series 5 EPOC R1 kernel
	// complete its early init before being preempted, and unlocks +144
	// unique_pcs of boot exploration at 30 sim s. PSION_FIRST_IRQ_DELAY
	// env var overrides this default.
	virtual uint64_t defaultFirstIrqDelay() const { return 0; }
	// debugPC PC hooks at 0x634 / 0x66C / 0x32304 / 0x15070 / 0x16198 are
	// pinned to the Osaris ROM v1.02 layout. On the Series 5 ROM the same
	// PCs land in unrelated functions and produce garbage log lines (e.g.
	// fake "KERNEL MMU SECTION" entries with v=c001xxxx p=0000007x size=0).
	// Subclasses with a different ROM layout override this to false and
	// can register their own hooks via overriding debugPC().
	virtual bool hasOsarisDebugHooks() const { return true; }
	// Whether to run the SYSCON1 BZTOG/BZMOD-driven buzzer pump that emits
	// square-wave audio samples to dacQueue. Off by default — Osaris and
	// MC218 boot with quirky SYSCON1 sequences (e.g. BZMOD=1 with BZTOG
	// transient, or extended buzzer dwell during init) that would otherwise
	// produce continuous tones from the pump. Series 5 overrides this to
	// true because we've verified the EPOC R1 kernel uses the buzzer pin
	// for tap-click feedback (BZTOG=0→1→0 pulses) and the audible click
	// on tap is the actual user-facing behaviour.
	virtual bool enableBuzzerPump() const { return false; }
	// Polarity of the SYSFLG1 modem-status inputs (bit 8 CTS, 9 DSR,
	// 10 DCD). These are pins, not internal state: what the SoC sees is
	// whatever the board's RS-232 line receiver puts on them, and that is
	// a per-machine wiring decision rather than a chip one.
	//
	// Psion's own CL-PS711x boards read active-high — a cable on the far
	// end asserting CTS/DSR/DCD makes the bits read 1 — and that is the
	// default. The Geofox, built by a different company around the same
	// SoC, reads them inverted, which its ROM makes unmistakable: with
	// the bits asserted our way its RemoteLinkServer configures UART1 and
	// then never transmits a byte, and with them inverted it sends the
	// Req_Req_Pdu burst the Series 5 sends and the link connects. See
	// core/geofox.h.
	virtual bool modemLinesActiveLow() const { return false; }
	// Expose LCD controller state so subclasses can render their own sizes.
	uint32_t currentLcdAddress() const { return lcdAddress; }
	uint32_t currentLcdControl() const { return lcdControl; }
	uint64_t currentLcdPalette() const { return lcdPalette; }

protected:
	// SYSFLG1 default value. Subclasses (e.g. Series5::Emulator for the
	// CL-PS7110) may override to clear bit 29 — the chip-ID bit per the
	// CL-PS7111 datasheet 5.9: '1' for CL-PS7111, '0' for CL-PS7110.
	uint32_t sysFlg1 = 0x20008000; // bit 29 = CL-PS7111 ID; bit 15 = CLDFLG (cold start)
	// Promoted to protected so Series5::Emulator's applyDeviceQuirks()
	// brute-force sweep can seed initial values from environment vars
	// (PSION_S5_INTSR1_SEED, PSION_S5_DRFPR_SEED, PSION_S5_PMPCON_SEED).
	uint16_t pendingInterrupts = 0;
	uint16_t interruptMask = 0;
private:
	uint32_t portValues = 0;
	uint32_t portDirections = 0;
	uint32_t lcdControl = 0;
	uint32_t lcdAddress = 0xC0000000;
	// CL-PS7110 (Series 5) adds a Port C that CL-PS7111 (MC218/Osaris) does
	// not have. Its data register is at byte offset 0x02 and its direction
	// register at 0x42 — same stride as Port A/B/D but between PBDR and
	// PDDR. The EPOC R1 kernel reads and writes both during early init.
	uint8_t portCData = 0;
	uint8_t portCDir  = 0;
	// MEMCFG1/MEMCFG2 are the nCS0-nCS7 external-memory timing registers.
	// On real hardware they power up with the datasheet's default waits
	// (32-bit bus, all waits, latch enabled — 0xFFFF00FF for each). We
	// weren't tracking them at all; Series 5's EPOC R1 bootloader reads
	// MEMCFG2, OR-masks a couple of bits, and writes the value back, so
	// without storage the readback would differ from what was just written
	// and would send the bootloader into a retry loop. Just storing the
	// 32-bit register is enough for that handshake.
	uint32_t memCfg1 = 0xFFFF00FF;
	uint32_t memCfg2 = 0xFFFF00FF;
protected:
	// DRAM Refresh Period Register. Same story — early boot writes a byte
	// here and later reads it back. Protected so Series 5 brute-force
	// sweep can seed via PSION_S5_DRFPR_SEED.
	uint8_t  drfpr = 0x00;
	// PMPCON (Pump Power Control) at 0x400. Series 5 EPOC R1 driver init
	// reads, masks, and writes back this register many times during boot
	// (over 100 times in 30 seconds of sim time). Without read-back
	// semantics the kernel's RMW loops never see a bit change and stay
	// stuck waiting for the LCD-pump to come up. Default the boot value
	// to 0 — Series 5 datasheet says it's all zero at reset, and the
	// kernel writes whatever it wants on top. Protected so Series 5
	// brute-force sweep can seed via PSION_S5_PMPCON_SEED.
	uint32_t pmpcon = 0;
private:
	uint32_t rtc = 0;
	uint32_t rtcDiv = 0;
	// RTC match register — when rtc == rtcMatch and not masked, fires
	// RTCMI interrupt. Real EPOC kernels program this for alarm/wakeup.
	// We store it but don't fire interrupts based on it (no kernel
	// currently relies on the alarm).
	uint32_t rtcMatch = 0;
	// CODR: codec output data register. Real CL-PS7111 has an 8-bit signed
	// PCM codec wired into the SSI. EPOC writes CODR to push DAC samples
	// (system beeps, alarms, Voice Notes playback) and reads CODR to drain
	// captured ADC samples (Voice Notes record).
	//
	// EPOC R1 (Series 5) and EPOC R5 (Osaris) drive system click / beep
	// tones via the CL-PS7110/7111 buzzer drive pin, not the codec. The
	// buzzer is controlled by SYSCON1 bits 9 (BZTOG) and 10 (BZMOD): with
	// BZMOD=0 the buzzer follows BZTOG directly (manual square-wave or
	// click); with BZMOD=1 the buzzer auto-toggles on every TC1 underflow,
	// so the audible tone is TC1's underflow frequency divided by two.
	// See datasheet section 3.2.11 (PS7110) / 5.5 (PS7111).
	//
	// Voice Notes record/play needs additional CSINT/state-machine plumbing
	// per-ROM (channel-pointer capture, etc.) that's intentionally deferred
	// to a follow-up — see notes alongside the audio rings below.
	uint32_t codrValue = 0;
	// One-time CODR write log counter — surfaces the first N writes per
	// session so a tester can quickly verify whether the EPOC kernel is
	// actually driving the codec on this device. Silent after the cap.
	int codrWritesLogged = 0;
	// Companion counter for SSI0 DMA descriptor writes. Same throttling
	// pattern; used to disambiguate "kernel uses CODR" vs "kernel uses
	// SSI DMA" for the no-audio-on-Osaris/Series-5 investigation.
	int ssiDmaWritesLogged = 0;
	// Companion counter for BZTOG 0→1 rising edges. Surfaces whether
	// the kernel is driving the buzzer pin for click feedback on a
	// given device — if the count stays at zero through a session where
	// the user pressed icons, the kernel is using a different audio
	// path entirely.
	int bztogRisingLogged = 0;
protected:
	// Shared codec / buzzer FIFO model. All the chip-agnostic state
	// (sample rings, virtual TX FIFO, deferred CSINT timing, buzzer
	// square-wave pump) lives here and is implemented identically with
	// the Windermere copy in core/audio_codec.{h,cpp}. The device-
	// specific bits below (CDENRX/CDENTX gates, BZTOG/BZMOD register
	// decode) are how we hand the model the predicates it needs.
	AudioCodecModel audio;
	// SYSCON1 bit 13 = CDENTX, bit 14 = CDENRX (codec TX / RX enable).
	// The kernel sets these to start a play / record session. Gating
	// CSINT firing on the appropriate bit avoids spurious interrupts
	// against a torn-down handler.
	bool codecTxEnabled() const { return (sysCon1 & (1u << 13)) != 0; }
	bool codecRxEnabled() const { return (sysCon1 & (1u << 14)) != 0; }
	// Buzzer drive state, derived from SYSCON1 bits 9/10. The square-
	// wave generation itself lives in AudioCodecModel; these track the
	// register decode so the executeUntil loop can decide when to fire
	// the pump.
	bool buzzerBzTog = false;
	bool buzzerBzMod = false;
private:
	// UART1 data + line-control. UART transmission isn't modelled
	// (RS232 is just stubbed for register-poll compatibility), but
	// reading the line-control register as 0 makes the kernel's
	// "is UART configured?" check fail and skip UART init early.
	uint32_t uart1Data = 0;
	uint32_t uart1LineCtl = 0;
	uint32_t uart2Data = 0;
	uint32_t uart2LineCtl = 0;
	// STDBY: writing to this register puts the CPU into standby mode.
	// We don't model standby; the write is a no-op.
	uint64_t lcdPalette = 0;
	uint16_t lastSyncioRequest = 0;
	// Cycle at which SSEOTI should fire after a SYNCIO write. Real
	// ADC1010 takes ~16 µs per conversion (~300 cycles at 18.432 MHz).
	// 0 means "fire immediately" (legacy behaviour).
	int64_t sseotiFireAt = 0;
	// True if SYNCIO write has been issued and SSEOTI fire is pending.
	bool sseotiPending = false;

protected:
	// kScan is the kernel-written keyboard scan-select. The base
	// readKeyboard interprets bit 3 as "select single column" and bits
	// 2:0 as the column index. Series 5 needs to read this in its
	// own readKeyboard override.
	uint32_t kScan = 0;
private:
	// SYSCON1 storage. Reads must reflect the previously-written value
	// (per CL-PS7110/CL-PS7111 datasheet 3.2.11 — SYSCON is RW). Without
	// this, the kernel can't read back the UART/LCD/codec/wake-enable
	// control bits it just wrote, breaking driver-init handshakes.
	uint32_t sysCon1 = 0;
protected:
	// Keyboard matrix state. CLPS7111 (MC218/Osaris) scans 7 columns;
	// CL-PS7110 (Series 5) scans 8 columns with the same physical layout
	// as Windermere's 5mx. We size the array to 8 here so the Series 5
	// override can use column 7 without resizing; the base readKeyboard
	// only walks columns 0..6 so MC218/Osaris are unaffected.
	uint8_t keyboardColumns[8] = {0,0,0,0,0,0,0,0};
	uint8_t keyboardExtra = 0;
	int32_t touchX = 0, touchY = 0;
private:

	Timer tc1, tc2;
	CLPS7600 pcCardController;
protected:
	VCFCard cfCard;  // protected so Series 5 can read irqAsserted() from tickCfBridge
private:
	bool halted = false, asleep = false;

	// CF accel-timer (Osaris): the PCCARD-ATA socket retry callback arms
	// a 2,000,000 µs (2 s) NTimer per sector. On real hardware that
	// timer is the safety net for when the card-ready IRQ fails to
	// reach the DFC; in our emulator the IRQ→DFC wake path has a
	// fidelity gap so every sector waits out the full 2 s. We rewrite
	// r1 (the µs value passed to the NTimer-arm function) at the BL
	// site from 2,000,000 → kCfAccelTimerUs to drain at ~1 sector per
	// kernel tick instead.
	uint32_t cfRomTimerArmPC1 = 0;
	uint32_t cfRomTimerArmPC2 = 0;
	bool cfAccelTimer = true;
	static constexpr uint32_t kCfAccelTimerUs = 100;
	uint32_t cfAccelTimerHits = 0;


	uint32_t getRTC();

	uint32_t readReg8(uint32_t reg);
	uint32_t readReg32(uint32_t reg);
	void writeReg8(uint32_t reg, uint8_t value);
	void writeReg32(uint32_t reg, uint32_t value);

public:
	MaybeU32 readPhysical(uint32_t physAddr, ValueSize valueSize) override;
	bool writePhysical(uint32_t value, uint32_t physAddr, ValueSize valueSize) override;

protected:
	// Hook for subclasses to mutate seed register values just before the
	// CPU is reset. Used by Series5::Emulator to pull sweep knobs from
	// environment variables. Default no-op preserves CLPS7111 / MC218 /
	// Osaris behaviour exactly.
	virtual void applyDeviceQuirks() {}

	// Hook called from executeUntil() once per outer loop iteration
	// (after the timer tick / IRQ-pending check, before CPU tick).
	// Subclasses can use it to poll kernel-side state without paying the
	// per-instruction trace cost. Default no-op. Used by Series5::Emulator
	// to track EKA1 boot progression — see core/series5.cpp.
	virtual void pollKernelState() {}

	// Per-cycle CF plumbing hook. Default impl drives the CLPS7600
	// path (Osaris). Series 5 overrides to ALSO route through ETNA
	// (which Series 5 hardware has in addition to the on-die CLPS7600),
	// matching how Windermere/5mx wires its CF IRQ. Called every
	// executeUntil iteration, AFTER cfCard.tickIrqDelay(1).
	virtual void tickCfBridge() {}
private:
	bool configured = false;
	void configure();

	const char *identifyObjectCon(uint32_t ptr);
	void fetchStr(uint32_t str, char *buf);
	void fetchName(uint32_t obj, char *buf);
	void fetchProcessFilename(uint32_t obj, char *buf);
	virtual void debugPC(uint32_t pc);
	void diffPorts(uint32_t oldval, uint32_t newval);
	virtual uint32_t readKeyboard() const;

public:
	Emulator();
	uint8_t *getROMBuffer() override;
	size_t getROMSize() override;
	void loadROM(uint8_t *buffer, size_t size) override;
	void executeUntil(int64_t cycles) override;
	int32_t getClockSpeed() const override { return CLOCK_SPEED; }
	const char *getDeviceName() const override;
	int getDigitiserWidth() const override;
	int getDigitiserHeight() const override;
	int getLCDOffsetX() const override;
	int getLCDOffsetY() const override;
	int getLCDWidth() const override;
	int getLCDHeight() const override;
	void readLCDIntoBuffer(uint8_t **lines, bool is32BitOutput) const override;
	void setKeyboardKey(EpocKey key, bool value) override;
	void updateTouchInput(int32_t x, int32_t y, bool down) override;

	bool attachCard(const uint8_t *bytes, size_t size) override;
	void detachCard() override;
	bool isCardInserted() const override { return cfCard.inserted(); }
	size_t getCardImageSize() const override { return cfCard.imageSize(); }
	const uint8_t *getCardImageData() const override { return cfCard.data(); }
	// Surface the VCFCard's ATA / sector-drain counters so the harness'
	// CF_STATS line reflects CLPS7111-family CF activity (Series 5 / Osaris /
	// MC218). The IRQ / EINT3 / reschedule / accel-timer counters are
	// Windermere/SA-1100 socket-acceleration specifics that this family
	// doesn't track, so they stay 0.
	CfStats getCfStats() const override {
		CfStats s;
		s.ataCommands  = cfCard.ataCommandCount;
		s.sectorDrains = cfCard.sectorBoundaryCount;
		return s;
	}

	// Audio surface. Series 5 (EPOC R1) and Osaris (EPOC R5) both use the
	// CL-PS7110/7111 on-chip codec as their only audio output; CODR writes
	// from the kernel become host-speaker samples here. Voice Notes
	// recording is intentionally minimal in this cut — see clps7111.h
	// docs at the audio rings.
	bool hasAudio() const override { return true; }
	int getAudioSampleRate() const override { return AudioCodecModel::kAudioSampleRate; }
	size_t readAudioOutput(int16_t *dst, size_t maxSamples) override {
		return audio.readAudioOutput(dst, maxSamples);
	}
	void writeAudioInput(const int16_t *src, size_t count) override;
	void setHostAudioEnabled(bool speaker, bool mic) override {
		audio.setHostEnabled(speaker, mic);
	}

	// ── Host serial bridge ────────────────────────────────────────────────
	// Attach/detach a virtual host-side cable to UART1 (uartIndex 1 — the
	// only UART that carries IrDA SIR framing on the CL-PS7110/7111). When
	// attached, bytes the guest writes to UARTDR1 accumulate in uart1.txQueue
	// (drained via serialReadToHost); bytes pushed via serialWriteFromHost
	// arrive in uart1.rxFifo and surface as URXINT1 (interrupt bit 13).
	// While unattached, the original stub behaviour stands so boot paths are
	// undisturbed. Declared here on the CL-PS7111 BASE class so Series 5
	// (CL-PS7110) inherits the implementation unchanged. Implemented in
	// core/clps7111_serial_bridge.cpp.
	bool serialAttachHost(int uartIndex);
	bool serialDetachHost(int uartIndex);
	size_t serialWriteFromHost(int uartIndex, const uint8_t *data, size_t len);
	size_t serialReadToHost(int uartIndex, uint8_t *dst, size_t cap);
	size_t serialHostTxAvailable(int uartIndex) const;
	bool serialIsAttached(int uartIndex) const;

protected:
	// UART1 host-bridge state (rxFifo / txQueue / hostAttached / discrete
	// interrupt latches). Default-constructed inert; cpu pointer is wired in
	// the constructor. Protected so the serial-bridge TU (a friend by being
	// in the same class) and any future subclass can reach it.
	UART uart1{};
	// Map the UART's discrete interrupt latches onto pendingInterrupts:
	// IntRx→URXINT1(13), IntTx→UTXINT(12), IntModemStatus→UMSINT(14).
	void updateUartIrqs();
};
}

