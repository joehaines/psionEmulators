// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2019 Ash Wolf (WindEmu); modifications (c) 2024-2026 Joe Haines.
// Subject to the Mozilla Public License v2.0 (http://mozilla.org/MPL/2.0/).



#include "windermere.h"
#include "rtc_seed.h"
#include "wind_defs.h"
#include "hardware.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <time.h>
#include "common.h"

#define PSION_ENV_CSTR(name)  ([] { static const char *const _v = std::getenv(name); return _v; }())



// 5mx Pro bootloader needs the C and D DRAM regions to be distinct
// physical memory: it installs its L1 page table at 0xd07f8000 (bank D0)
// while loading the OS image into 0xc0000000+ (bank C0). With the
// default single-block aliasing, image data being written to 0xc07f8000
// would land in the same backing array the MMU walker reads from for
// 0xd07f8000 — corrupting the page tables right when the loader fills
// the last megabyte of bank C0, producing a prefetch abort at PC=0x8284
// the moment the next instruction fetch re-walks PT[0]. INCLUDE_D
// gives us C0+C1 backed by MemoryBlockC0 and D0+D1 backed by
// MemoryBlockD0 — preserving real-hardware aliasing within each region
// while still keeping C and D as separate physical memories.
#define INCLUDE_D
//#define INCLUDE_BANK1

namespace Windermere {
Emulator::Emulator() : etna(&cpu) {
	// Zero the large member arrays explicitly. They're declared as
	// uninitialised C arrays (uint8_t ROM[...], MemoryBlockC0[...] etc),
	// which under C++ default-initialisation rules contain indeterminate
	// bytes — and `new Windermere::Emulator` in device_registry.cpp uses
	// the no-parens form which IS default-init, not value-init. With
	// modern dlmalloc reusing the previously-freed block when one
	// Windermere::Emulator is deleted and another (e.g. switching from
	// 5mx to Revo) is immediately allocated, those uninitialised bytes
	// leaked the old emulator's ROM / DRAM into the new instance —
	// loadROM only overwrites the front of ROM[] (memcpy of `size`
	// bytes), so 5mx → Revo would leave the upper 8 MiB of ROM and ALL
	// of MemoryBlockC0/C1/D0/D1 holding 5mx state. After a state-save
	// round trip that stale state replayed as "5mx data appearing in
	// Revo". memset gives every new instance a clean slate.
	std::memset(ROM,            0, sizeof(ROM));
	std::memset(ROM2,           0, sizeof(ROM2));
	std::memset(MemoryBlockC0,  0, sizeof(MemoryBlockC0));
	std::memset(MemoryBlockC1,  0, sizeof(MemoryBlockC1));
	std::memset(MemoryBlockD0,  0, sizeof(MemoryBlockD0));
	std::memset(MemoryBlockD1,  0, sizeof(MemoryBlockD1));
	cfCard.setOwner(&cpu);
	// Opt-in deep tracing of the first CF ISR. Costs zero when unset; when
	// set, produces a bounded burst of DIAG lines for ~20k instructions
	// around the dispatch. Used by the CF-per-sector-delay investigation.
	if (const char *env = getenv("PSION_CF_DIAG"); env && *env && *env != '0') {
		cfDiagEnabled = true;
	}
	// Kill switch for the (default-on) CF direct-invoke accelerator — see
	// the cfDirectInvoke doc in windermere.h.
	if (const char *env = getenv("PSION_NO_CF_DIRECT_INVOKE"); env && *env && *env != '0') {
		cfDirectInvoke = false;
	}
}


uint32_t Emulator::getRTC() {
    uint32_t val = psionInitialRtcSeconds();   // PSION_RTC_SEED pins this
    //log("getRTC: %04x", val);
    return  val;
}


uint32_t Emulator::readReg8(uint32_t reg) {
	if ((reg & 0xF00) == 0x600) {
		uint32_t v = uart1.readReg8(reg & 0xFF);
		// Log every byte the CPU actually pulls off the host-bridged
		// UART's RX FIFO. Quiet during boot (no host attached → no RX
		// bytes ever appear). Once a Remote Link session is active
		// this fires once per byte the guest serial driver consumes —
		// confirming our pushed bytes actually reach EPOC.
		if (uart1.hostAttached && (reg & 0xFF) == 0 && v != 0xFF) {
			log("uart1 rx byte 0x%02x consumed by CPU (rxFifo left=%zu)",
			    v, uart1.rxFifoBytes());
		}
		updateUartIrqs();
		return v;
	} else if ((reg & 0xF00) == 0x700) {
		uint32_t v = uart2.readReg8(reg & 0xFF);
		if (uart2.hostAttached && (reg & 0xFF) == 0 && v != 0xFF) {
			log("uart2 rx byte 0x%02x consumed by CPU (rxFifo left=%zu)",
			    v, uart2.rxFifoBytes());
		}
		updateUartIrqs();
		return v;
	} else if (reg == TC1CTRL) {
		return tc1.config;
	} else if (reg == TC2CTRL) {
		return tc2.config;
	} else if (reg == CODR) {
		// Pop one ADC sample. AudioCodecModel handles the per-cycle
		// 16-sample FIFO cap (so Voice Notes' DFC drain loop stops
		// where real hardware does), the int8 narrowing, and the
		// deferred-CSINT re-fire scheduling.
		auto r = audio.popAdcSample(passedCycles);
		if (r.ringEmpty) pendingInterrupts &= ~(1u << CSINT);
		return r.sample;
	} else if (reg == COLFG) {
		// FIFO status. Bit 0 = RX empty, bit 1 = TX full.
		//   RX_EMPTY is asserted when the ADC ring is empty OR when
		//   the current drain cycle has already consumed the full
		//   hardware-FIFO depth (kAudioFifoDepth) — see the note on
		//   adcFifoReadsSinceRefill in windermere.h.
		//   TX_FULL is asserted when the virtual hardware FIFO
		//   (virtualTxFifoLevel, drained at 8 kHz by the tick loop)
		//   has reached kAudioFifoDepth — NOT the host-side dacQueue
		//   fill, which empties only at RAF cadence and would keep
		//   the DFC in TX_FULL mid-playback and starve the output.
		uint8_t val = 0;
		if (audio.isRxFifoEmpty()) val |= 0x01;
		if (audio.isTxFifoFull())  val |= 0x02;
		return val;
	} else if (reg == COEOI) {
		return 0;
	} else if (reg == COTEST) {
		return 0;
	} else if (reg == CONFG) {
		// Codec configuration register (see decompiled ROM's
		// FUN_5009fad8 / FUN_5000abe4 family at
		// reference/5MX_decompiled/5mx_v1.05(260)_eng.bin.c).
		//
		// EPOC's dictaphone driver reads CONFG to detect whether the
		// codec is already in use: the check is `(CONFG & 3) != 0`
		// in line 211493. The unknown-register fall-through that used
		// to serve this access returned 0xFF, so the app always saw
		// "codec in use" and Voice Notes fired the "In use" dialog the
		// instant the user pressed REC. Mirror the guest's own writes
		// instead — cold CONFG reads as 0 (idle, not in use) and the
		// CONFG=3 the driver writes to enable RX+TX reads back
		// correctly.
		return (uint32_t)(codecConfig & 0xFF);
	} else if (reg == BZCONT) {
		return buzzerCtrl;
	} else if (reg == PADR) {
		return readKeyboard();
	} else if (reg == PBDR) {
		return (portValues >> 16) & 0xFF;
	} else if (reg == PCDR) {
		return (portValues >> 8) & 0xFF;
	} else if (reg == PDDR) {
		// PD bit 3 is the ETNA CF-door sense line on Psion 5mx (see the
		// "PRT etna door" bit in diffPorts). On real hardware it reports
		// the physical bay-door switch, which is independent of whether
		// a card is inserted: the user can close the door with no card
		// inside, or theoretically open the door without removing the
		// card. Active-low sense: door-closed pulls the line low.
		uint8_t pd = portValues & 0xFF;
		pd = (pd & ~0x08) | (doorOpen ? 0x08 : 0x00);
		// PD bit 7 is the ETNA "ready / no-error" input (PDDDR bit 7 is left
		// as INPUT by EPOC — the CLPS711x direction convention is 1=input).
		// PCCARD-ARM's vt[0x40] reads PDDR, tests bit 7, and returns
		// KErrNotReady (-2) when the bit is clear, which in turn drives the
		// stack down the PowerOff path during PowerOn and leaves drive D:
		// unmounted. Drive the line high whenever a card is attached; our
		// virtual CF is always ready once inserted.
		if (etna.isCardPresent())
			pd |= 0x80;
		else
			pd &= ~0x80;
		return pd;
	} else if (reg == PADDR) {
		return (portDirections >> 24) & 0xFF;
	} else if (reg == PBDDR) {
		return (portDirections >> 16) & 0xFF;
	} else if (reg == PCDDR) {
		return (portDirections >> 8) & 0xFF;
	} else if (reg == PDDDR) {
		return portDirections & 0xFF;
	} else {
		return scratchRegs[reg & 0xFFF];
	}
}
uint32_t Emulator::readReg32(uint32_t reg) {
	if (reg == LCDCTL) {
		log("LCD control read pc=%08x lr=%08x !!!", getGPR(15), getGPR(14));
		return lcdControl;
	} else if (reg == LCDST) {
		log("LCD state read pc=%08x lr=%08x !!!", getGPR(15), getGPR(14));
		return 0xFFFFFFFF;
	} else if (reg == PWRSR) {
		return pwrsr;
	} else if (reg == INTSR) {
		return pendingInterrupts & interruptMask;
	} else if (reg == INTRSR) {
		return pendingInterrupts;
	} else if (reg == INTENS) {
		return interruptMask;
	} else if ((reg & 0xF00) == 0x600) {
		uint32_t v = uart1.readReg32(reg & 0xFF);
		if (uart1.hostAttached && (reg & 0xFF) == 0 && (v & 0xFF) != 0xFF) {
			log("uart1 rx byte 0x%02x consumed by CPU (32b, rxFifo left=%zu)",
			    v & 0xFF, uart1.rxFifoBytes());
		}
		updateUartIrqs();
		return v;
	} else if ((reg & 0xF00) == 0x700) {
		uint32_t v = uart2.readReg32(reg & 0xFF);
		if (uart2.hostAttached && (reg & 0xFF) == 0 && (v & 0xFF) != 0xFF) {
			log("uart2 rx byte 0x%02x consumed by CPU (32b, rxFifo left=%zu)",
			    v & 0xFF, uart2.rxFifoBytes());
		}
		updateUartIrqs();
		return v;
	} else if (reg == TC1VAL) {
		return tc1.value;
	} else if (reg == TC2VAL) {
		return tc2.value;
	} else if (reg == SSDR) {
		// as per 5000A7B0 in 5mx rom
		uint16_t ssiValue = 0;
		// Map the touch coordinate onto the same ADC range the 5mx ROM was
		// tuned against (X: ~50..4012, Y: ~131..3834) regardless of the
		// device's digitiser size. The original constants 5.7 and 13.225
		// were the per-pixel slopes for a 695 x 280 digitiser; on the Revo
		// (527 x 208) those compress the ADC range to 50..3054 / 1083..3834,
		// which the EPOC pen calibration interprets as taps squashed into
		// the top-left ~75% of the screen. Scaling by getDigitiserWidth/
		// Height gives every Windermere-class device the full ADC swing
		// the pen driver expects.
		constexpr double kAdcXSpan = 695.0 * 5.7;     // 3961.5 — 5mx X span
		constexpr double kAdcYSpan = 280.0 * 13.225;  // 3703   — 5mx Y span
		const double xSlope = kAdcXSpan / getDigitiserWidth();
		const double ySlope = kAdcYSpan / getDigitiserHeight();
		switch (lastSSIRequest) {
		// While pen is lifted the touch digitiser reads open-circuit, not
		// a valid coordinate. Return an out-of-band ADC value so the pen
		// driver's debounce sees "pen gone" on release and registers a tap.
		case 0xD0D3:
			ssiValue = penDown ? (uint16_t)(50 + (touchX * xSlope)) : 0;
			// Count completed X-channel conversions (one per 6-read
			// group; the first data byte arrives at read 4) so the
			// tap latch in updateTouchInput can tell a serviced tap
			// from one the pen ISR never got to sample.
			if (penDown && ssiReadCounter == 4) penSamplesSinceDown++;
			break;
		case 0x9093:
			ssiValue = penDown ? (uint16_t)(3834 - (touchY * ySlope)) : 4095;
			break;
		// Battery ADC channels for the UCB-style touch/battery codec
		// hanging off SSI. Both raw 12-bit samples are fed through the
		// EPOC HAL's mV scaling, so the value the kernel sees is the
		// number we hand back. Tuning differs per device:
		//
		//   5mx / MC218 / 5mx Pro: 2x AA alkaline (3 V nominal, ~3.2 V
		//   fresh) + CR2032 backup. 3100 mV reads as "full" on both
		//   channels.
		//
		//   Revo (rechargeable 3-cell NiMH + CR2032 backup): the HAL's
		//   battery curve table at 0x500fe800 starts with the row
		//   (4000, 50000, 32000); 4000 is the full-charge ADC anchor
		//   for the main channel and the kernel clamps higher readings
		//   to 4000 at 0x500c7eec. Pin main at 4000 to match.
		//
		//   The backup channel on the Revo is wired up to suppress the
		//   warning icon below 2050 (the "very low / not present"
		//   threshold checked at 0x5000488c against the literal at
		//   0x500048bc): above that line the EPOC shell flashes the
		//   bottom-right empty-battery icon every second; below it the
		//   shell shows a solid "full" icon, presumably because the HAL
		//   then assumes the device is running on AC via the dock and
		//   suppresses the rechargeable-pack warnings. The 5mx HAL
		//   doesn't share this code path, so its CR2032 stays on 3100.
		case 0xA4A4: ssiValue = isRevoFamily() ? 4000 : 3100; break; // MainBattery
		case 0xE4E4: ssiValue = isRevoFamily() ? 2000 : 3100; break; // BackupBattery
		}

		uint32_t ret = 0;
		if (ssiReadCounter == 4) ret = (ssiValue >> 5) & 0x7F;
		if (ssiReadCounter == 5) ret = (ssiValue << 3) & 0xF8;
		ssiReadCounter++;
		if (ssiReadCounter == 6) ssiReadCounter = 0;

		// by hardware we should be clearing SSEOTI here, i think
		// but we just leave it on to simplify things
		return ret;
	} else if (reg == SSSR) {
		return 0;
    } else if (reg == RTCDRL) {
        rtc = getRTC();
        uint16_t v = rtc & 0xFFFF;
        // log("RTCDRL: %04x", v);
        return v;
    } else if (reg == RTCDRU) {
        rtc = getRTC();
        uint16_t v = rtc >> 16;
        // log("RTCDRU: %04x", v);
        return v;
    } else if (reg == KSCAN) {
        return kScan;
	} else if (reg == CODR) {
		// 32-bit CODR read — same FIFO behaviour as the 8-bit path.
		auto r = audio.popAdcSample(passedCycles);
		if (r.ringEmpty) pendingInterrupts &= ~(1u << CSINT);
		return (uint32_t)r.sample;
	} else if (reg == COLFG) {
		uint32_t val = codecLeftGain & ~0x03u;
		if (audio.isRxFifoEmpty()) val |= 0x01;
		if (audio.isTxFifoFull())  val |= 0x02;
		return val;
	} else if (reg == COEOI) {
		return 0;
	} else if (reg == COTEST) {
		return 0;
	} else if (reg == BZCONT) {
		return buzzerCtrl;
	} else if (reg == PADR) {
		// V32 path for the keyboard column read. The 5mx Pro bootloader
		// uses ldr (32-bit) on PADR while polling the keyboard matrix —
		// most notably the "about" Easter-egg detector. Without this
		// case the read fell through to scratchRegs (always 0) and the
		// bootloader saw every column as empty regardless of which key
		// the user was holding. EPOC and the other CLPS-derived ROMs
		// use ldrb here, which goes through readReg8's existing PADR
		// case, so they were unaffected.
		return readKeyboard();
	} else {
		uint32_t off = reg & 0xFFC;
		return (uint32_t)scratchRegs[off]
		     | ((uint32_t)scratchRegs[off + 1] << 8)
		     | ((uint32_t)scratchRegs[off + 2] << 16)
		     | ((uint32_t)scratchRegs[off + 3] << 24);
	}
}

void Emulator::writeReg8(uint32_t reg, uint8_t value) {
	if ((reg & 0xF00) == 0x600) {
		uart1.writeReg8(reg & 0xFF, value);
		updateUartIrqs();
	} else if ((reg & 0xF00) == 0x700) {
		uart2.writeReg8(reg & 0xFF, value);
		updateUartIrqs();
	} else if (reg == TC1CTRL) {
		tc1.setConfig(value);
	} else if (reg == TC2CTRL) {
		tc2.setConfig(value);
	} else if (reg == CODR) {
		// Push one 8-bit DAC sample onto the TX queue. AudioCodecModel
		// handles the int16 expansion, virtual FIFO bookkeeping and
		// drain scheduling; the dacHistory ring below is a Windermere-
		// only diagnostic mirror the browser audio test scrapes.
		int8_t raw = (int8_t)value;
		audio.pushDacSample(raw, passedCycles);
		dacHistory[dacHistoryHead] = raw;
		dacHistoryHead = (dacHistoryHead + 1) % kDacHistorySize;
		if (dacHistoryCount < kDacHistorySize) dacHistoryCount++;
	} else if (reg == CONFG) {
		uint32_t before = codecConfig;
		codecConfig = (codecConfig & ~0xFFu) | value;
		// Note the wall-clock cycle of the 0 → non-zero transition so
		// the tick-loop's settle-delayed CSINT firing knows when to
		// start. See `codecOnAtCycles` in windermere.h and the
		// post-settle gate in executeUntil().
		if ((before & 0xFF) == 0 && value != 0) {
			codecOnAtCycles = passedCycles;
		}
		if (value == 0) {
			codecOnAtCycles = -1;
		}
		// CONFG = 3 is EPOC's "start recording" signal from FUN_5009fcb0.
		// R5 holds the channel-struct pointer thanks to `mov r5, r0` at
		// 5009fcb8. We capture it here so executeUntil() can drive the
		// dictaphone DFC (FUN_5009f4ec) ourselves — see windermere.h.
		// When EPOC writes CONFG=3 from the dictaphone driver's start
		// path (either FUN_5009fad8 init or FUN_5009fcb0 record-start),
		// R5 holds the channel-struct pointer — the `mov r5, r0`
		// prologue at 5009fcb8/5009fae8 stashes it there. Capture it so
		// external tooling (the audio harness, a future DFC-injector)
		// can find the channel without walking kernel RAM.
		if (value == 3 && recordChannelPtr == 0) {
			uint32_t ch = getGPR(5);
			bool chValid = (ch >= 0x40000000 && ch < 0xe0000000);
			// 5mx Pro patched OS REC diagnostic. The user-reported
			// crash at 0x80000001 (prefetch fault) happens inside
			// the patched OS's record-start sequence AFTER the
			// CONFG=3 write — the kernel dispatches a callback at
			// virtual 0x80000bfc whose function pointer is the
			// uninitialised sentinel 0x80000001. Recording works on
			// real hardware, so the kernel SHOULD initialise that
			// callback before dispatching it; we want to find what
			// init code we're skipping and fix it properly. While
			// investigating, dump the relevant kernel-RAM state at
			// the moment of CONFG=3 so the user's console log
			// captures what's there. The previous short-circuit (set
			// codecConfig=0xFFFFFFFF and return early) is gone —
			// recording attempts will crash again, but only with
			// the diagnostic logging on, which is what we need to
			// find the missing init.
			// (Diagnostic kernel-TDfc + channel + object-table dump
			// removed once the 5mxPro REC crash root cause was found.
			// Root cause: recursive IRQ from tick-loop CSINT firing
			// before the patched OS's CSINT handler reliably acks.
			// Fixed by removing the patchedOsRecordingSettled tick-loop
			// firing path entirely; writeAudioInput now drives CSINT
			// only when mic samples actually arrive, with the existing
			// settling gate.)
			// If the host mic isn't enabled but the kernel just
			// programmed the channel into state 1 (recording), our
			// emulator will never push any mic samples in — the
			// kernel's drain loop then starves and, on MC218 at
			// least, this manifests as a kernel-level crash. Detect
			// the case early by probing the channel struct's state
			// field at +0x1c via the MMU and report "codec busy" so
			// the dictaphone driver surfaces a clean error dialog
			// instead. Playback (state 2) and open-warmup (state 3)
			// don't need the host mic and proceed normally.
			if (chValid && !audio.hostMicEnabled()) {
				if (auto s = readRamVirt32(ch + 0x1c); s.has_value() && s.value() == 1) {
					log("CONFG=3 record-start with host mic disabled (channel=%08x, state=1). Reporting codec busy so the dictaphone driver aborts cleanly.",
					    ch);
					codecConfig = 0xFFFFFFFF;
					return;
				}
			}
			if (chValid) {
				recordChannelPtr = ch;
				log("Captured dictaphone channel pointer = %08x (lr=%08x) ch+18=%08x ch+1c=%08x",
				    ch, getGPR(14),
				    readRamVirt32(ch + 0x18).value_or(0xdeadbeef),
				    readRamVirt32(ch + 0x1c).value_or(0xdeadbeef));
			} else {
				// CONFG=3 from a code path that doesn't have the
				// channel pointer in r5. Observed on MC218
				// v1.05(259) for the touch-driven REC button (lr ≈
				// 0x5009e750 area) — distinct from the REC-key path
				// which DOES stash r5. Without the channel ptr we
				// can't drive the DFC, AND empirically the MC218 ROM
				// crashes (AlignmentFault pc=0 lr=0x3f) shortly
				// after this CONFG=3 write. Fail-safe: disable our
				// codec emulation for the rest of this session.
				// Subsequent codec-register reads return as
				// unhandled (0xFFFFFFFF) and EPOC's dictaphone
				// driver aborts on its "(COLFG & 3) != 0" probe
				// instead of running into the crashing path. 5mx
				// keeps the valid-r5 capture path so its audio
				// continues to work normally.
				log("CONFG=3 with r5=%08x out-of-range (lr=%08x r0=%08x r1=%08x). Disabling codec emulation for this session.",
				    ch, getGPR(14), getGPR(0), getGPR(1));
				audioRegisterHandlingEnabled = false;
				// Make CONFG read return 0xFFFFFFFF on subsequent
				// probes — same as a fully-unhandled register, so
				// the driver's "(CONFG & 3) != 0" probe catches.
				codecConfig = 0xFFFFFFFF;
			}
		}
		// CONFG = 0 means the session is tearing down; drop any pending
		// CSINT and reset the FIFOs so the next "in use" probe sees a
		// clean idle state.
		if (value == 0) {
			pendingInterrupts &= ~(1u << CSINT);
			audio.resetAll();
			recordChannelPtr = 0;
			bootloaderPseudoDfc = false;
		}
		if (before != codecConfig)
			log("CODEC CONFG: %02x -> %02x", before & 0xFF, value);
	} else if (reg == COLFG) {
		codecLeftGain = (codecLeftGain & ~0xFFu) | value;
	} else if (reg == COEOI) {
		// COEOI ack ends the current FIFO drain cycle. Reset the
		// per-cycle read counter so the next CSINT can surface a new
		// batch of up to kAudioFifoDepth samples. Deliberately don't
		// immediately re-fire CSINT here — doing so (previously) let
		// the kernel dispatcher chain re-entries faster than our
		// scheduled 8 kHz cadence, driving delivery rates 2–9× too
		// fast and making recordings sound pitched down / stretched.
		pendingInterrupts &= ~(1u << CSINT);
		audio.ackCsint();
	} else if (reg == COTEST) {
		// ignore
	} else if (reg == PADR) {
		uint32_t oldPorts = portValues;
		portValues &= 0x00FFFFFF;
		portValues |= (uint32_t)value << 24;
		diffPorts(oldPorts, portValues);
	} else if (reg == PBDR) {
		uint32_t oldPorts = portValues;
		portValues &= 0xFF00FFFF;
		portValues |= (uint32_t)value << 16;
		if ((portValues & 0x10000) && !(oldPorts & 0x10000))
			etna.setPromBit0High();
		else if (!(portValues & 0x10000) && (oldPorts & 0x10000))
			etna.setPromBit0Low();
		if ((portValues & 0x20000) && !(oldPorts & 0x20000))
			etna.setPromBit1High();
		diffPorts(oldPorts, portValues);
	} else if (reg == PCDR) {
		uint32_t oldPorts = portValues;
		portValues &= 0xFFFF00FF;
		portValues |= (uint32_t)value << 8;
		diffPorts(oldPorts, portValues);
	} else if (reg == PDDR) {
		uint32_t oldPorts = portValues;
		portValues &= 0xFFFFFF00;
		portValues |= (uint32_t)value;
		diffPorts(oldPorts, portValues);
	} else if (reg == PADDR) {
		portDirections &= 0x00FFFFFF;
		portDirections |= (uint32_t)value << 24;
	} else if (reg == PBDDR) {
		portDirections &= 0xFF00FFFF;
		portDirections |= (uint32_t)value << 16;
	} else if (reg == PCDDR) {
		portDirections &= 0xFFFF00FF;
		portDirections |= (uint32_t)value << 8;
	} else if (reg == PDDDR) {
		portDirections &= 0xFFFFFF00;
		portDirections |= (uint32_t)value;
    } else if (reg == KSCAN) {
        kScan = value;
    } else if (reg == BZCONT) {
        // CLPS711x buzzer control. Bit 0 = BZTOG (manual toggle when
        // BZMOD set), bit 1 = BZMOD (1 = manual, 0 = TC1-driven tone).
        // EPOC writes here for keyboard clicks, alarms, calculator
        // beeps, and other system sounds — the codec at 0xA00 is just
        // for the dictaphone. Latch a minimum-duration hold on the
        // rising edge of bit 0 so the sub-tick "click" pulses EPOC
        // generates (BZCONT 0→1→0 in microseconds) survive into our
        // 64 Hz synthesiser window and produce audible clicks.
        uint8_t oldCtrl = buzzerCtrl;
        bool wasOn = (buzzerCtrl & 0x01) != 0;
        buzzerCtrl = value & 0xFF;
        bool nowOn = (buzzerCtrl & 0x01) != 0;
        if (nowOn && !wasOn) audio.noteBuzzerRisingEdge();
        if (oldCtrl != buzzerCtrl)
            log("BZCONT8 %02x->%02x (BZMOD=%d BZTOG=%d) pc=%08x",
                oldCtrl, buzzerCtrl, (buzzerCtrl >> 1) & 1, buzzerCtrl & 1, getGPR(15));
    } else {
		scratchRegs[reg & 0xFFF] = value;
	}
}
void Emulator::writeReg32(uint32_t reg, uint32_t value) {
	if (reg == LCDCTL) {
		log("LCD: ctl write %08x", value);
		lcdControl = value;
	} else if (reg == LCD_DBAR1) {
		log("LCD: address write %08x", value);
        lcdAddress = value;
	} else if (reg == LCDT0) {
		log("LCD: horz timing write %08x", value);
	} else if (reg == LCDT1) {
		log("LCD: vert timing write %08x", value);
	} else if (reg == LCDT2) {
		log("LCD: clocks write %08x", value);
	} else if (reg == INTENS) {
//		diffInterrupts(interruptMask, interruptMask | value);
		uint16_t before = interruptMask;
		interruptMask |= value;
		// Arm "level" mode (cfReenableMode==5) the first time the driver
		// enables EINT3. From that point on we prevent INTENC from ever
		// clearing bit 7 again.
		if (value & (1u << EINT3)) cfLevelArmed = true;
		bool eint3Changed = ((before ^ interruptMask) >> EINT3) & 1;
		if ((intMaskLogs < 30 && (before ^ interruptMask) != 0) || eint3Changed) {
			log("INTENS: mask %04x -> %04x (FIQ bits %02x, MCINT=%d, EINT3=%d) pc=%08x lr=%08x",
			    before, interruptMask, interruptMask & FIQ_INTERRUPTS,
			    (interruptMask >> MCINT) & 1, (interruptMask >> EINT3) & 1,
			    getGPR(15) - 4, getGPR(14));
			if (!eint3Changed) intMaskLogs++;
		}
	} else if (reg == INTENC) {
//		diffInterrupts(interruptMask, interruptMask &~ value);
		// Gate bit 7 out of the clear for strategies that want it latched
		// on. Mode 1 (intenc-block) only blocks when the caller is the
		// kernel dispatcher (lr=50011a04) — the driver itself may legit-
		// imately want to mask CF temporarily. Mode 5 (level) blocks
		// unconditionally once the driver has enabled bit 7 once.
		uint32_t effectiveValue = value;
		if ((value & (1u << EINT3)) != 0) {
			uint32_t lr = getGPR(14);
			bool fromKernelDispatcher = (lr == 0x50011a04);
			if (cfReenableMode == 1 && fromKernelDispatcher) {
				effectiveValue &= ~(1u << EINT3);
			} else if ((cfReenableMode == 5 || cfReenableMode == 8) && cfLevelArmed) {
				// Mode 8 uses "level" semantics at the mask level and then
				// gates the dispatch below to defer the actual IRQ to a
				// User32 context.
				effectiveValue &= ~(1u << EINT3);
			}
		}
		uint16_t before = interruptMask;
		interruptMask &= ~effectiveValue;
		bool eint3Changed = ((before ^ interruptMask) >> EINT3) & 1;
		if ((intMaskLogs < 30 && (before ^ interruptMask) != 0) || eint3Changed) {
			log("INTENC: mask %04x -> %04x (FIQ bits %02x, MCINT=%d, EINT3=%d) pc=%08x lr=%08x",
			    before, interruptMask, interruptMask & FIQ_INTERRUPTS,
			    (interruptMask >> MCINT) & 1, (interruptMask >> EINT3) & 1,
			    getGPR(15) - 4, getGPR(14));
			if (!eint3Changed) intMaskLogs++;
		}
	} else if (reg == HALT) {
		halted = true;
	} else if (reg == BLEOI) {
		// Battery-Low End of Interrupt — ack the BLINT FIQ on the
		// Windermere interrupt fabric. We don't currently raise BLINT
		// from the model (the SSI ADC always reports a healthy reading),
		// but handle the EOI explicitly so any future code path that
		// pulses BLINT can be cleared cleanly rather than re-firing.
		pendingInterrupts &= ~(1 << BLINT);
	} else if (reg == STFCLR) {
		// Status Flag Clear — clears the sticky PWRSR flag bits 9..13
		// (cold-reset, pump-on, watchdog, power-fail, etc). Without
		// this, the kernel's "clear post-cold-reset state" sequence at
		// boot is a no-op and CLDFLG remains set forever. The 5mx HAL
		// tolerates a stuck CLDFLG (its battery curve treats the
		// 5mx-tuned 3100 mV ADC as "good" regardless), but the Revo's
		// rechargeable-battery HAL keys off CLDFLG to mean "battery
		// state has not been re-established since cold boot" and pins
		// the battery icon at 0% / "Recharge!" until the bit clears.
		// Mirrors MAME's psion5.cpp:523 STFCLR handler.
		pwrsr &= ~0x00003e00;
	} else if (reg == MCEOI) {
		// Acknowledge the media-change FIQ. On real hardware ETNA keeps its
		// cause bits latched until W1C-cleared via PcCdIntStatus, but EPOC's
		// boot-time PCMCIA driver runs with pcCdIntMask=0 and doesn't clear
		// our latch — leaving the level-triggered line asserted would cause
		// MCINT to re-fire every tick. Treat MCEOI as an edge ack so the FIQ
		// only fires once per attach/detach transition; EPOC still learns of
		// the card by polling SktVarA0/A1 during its regular socket probe.
		pendingInterrupts &= ~(1 << MCINT);
		mcintEdgePending = false;
		etna.ackMediaChange();
	} else if (reg == TEOI) {
		pendingInterrupts &= ~(1 << TINT);
	// TEOI = 0x418,
	// STFCLR = 0x41C,
	// E2EOI = 0x420,
	} else if ((reg & 0xF00) == 0x600) {
		uart1.writeReg32(reg & 0xFF, value);
		updateUartIrqs();
	} else if ((reg & 0xF00) == 0x700) {
		uart2.writeReg32(reg & 0xFF, value);
		updateUartIrqs();
	} else if (reg == SSDR) {
		if (value != 0)
			lastSSIRequest = (lastSSIRequest >> 8) | (value & 0xFF00);
	} else if (reg == TC1LOAD) {
		tc1.load(value);
	} else if (reg == TC1CTRL) {
		// V32 path for TC1 config — the 5mx Pro bootloader programs the
		// timers via 32-bit stores (str rN, [r4]), and without this case
		// the writes fall through to scratchRegs and tc1.setConfig is
		// never called, so TC1OI never fires.
		tc1.setConfig((uint8_t)value);
	} else if (reg == TC2CTRL) {
		// V32 path for TC2 config — same story: bootloader uses str rN,
		// [r4] to enable TC2 (mode 0x80), expects TC2OI to fire, halts
		// in a wait-flag loop until the TC2 handler writes the flag.
		// Without this, TC2 stays disabled forever and the bootloader
		// never wakes from its halt.
		tc2.setConfig((uint8_t)value);
	} else if (reg == TC1EOI) {
		pendingInterrupts &= ~(1 << TC1OI);
	} else if (reg == TC2LOAD) {
		// --cf-fast-watchdog: experimental hack that scales TC2LOAD
		// writes from the TC2-setter FUN_5000aa38 (pc=0x5000aa44) down
		// by kCfFastWatchdogDiv (~30x) while a CF card is inserted. The
		// intent was to compress the 5mx nanokernel tick that feeds the
		// CF driver's iBusyTimeout.OneShot(KNotBusyTestInterval) polling
		// loop — see DPcCardMediaDriverAta::TimerDfcFunction in
		// kernelhwsrv's pccd_ata.cpp, which OneShots every 5 ms
		// (release) / 30 ms (debug) up to KBusyTimeOut = 400 / 67
		// iterations for ~2010 ms total. Gating:
		//   * cfCard.inserted() — only touch TC2 once a card is in.
		//   * pc == 0x5000aa44 — skip writes from other callers.
		//   * value in [256, 0x8000) — skip the 0xFFFF val-read probe
		//     and tiny debounce values; also dodge the 0xFFFF TC2LOAD
		//     the setter issues as part of its TC2VAL-readback sequence.
		// EMPIRICAL RESULT: does NOT move sector_drains. The 2 s
		// per-sector gap is driven by an instruction-count watchdog in
		// guest memory (established by the mode-9 experiments in
		// b62bd3f), not by TC2 cadence — the driver's 30 ms OneShot
		// doesn't reach TC2LOAD at a 30 ms magnitude during the gap
		// (the highest observed post-attach TC2LOAD value is ~4087
		// ticks ≈ 8 ms at 512 kHz). Enabling the flag adds downstream
		// TC2OI firings, so wall-clock time rises (~1.7x) while
		// sector_drains stays at the baseline 10 / 15 s. Flag is kept
		// as infrastructure: a future attempt that finds the correct
		// timer-queue Add callsite (FUN_5000f04c / NTimer::OneShot) can
		// reuse the same harness plumbing (setCfFastWatchdog + counters).
		if (cfFastWatchdog && cfCard.inserted()
		    && value >= 256 && value < 0x8000) {
			uint32_t pc = getGPR(15) - 4;
			if (pc == 0x5000aa44) {
				uint32_t scaled = value / kCfFastWatchdogDiv;
				if (scaled < 16) scaled = 16; // keep a lower bound
				if (cfFastWatchdogLogs < 12) {
					log("CF fast-watchdog: TC2LOAD %u -> %u pc=%08x lr=%08x",
					    value, scaled, pc, getGPR(14));
					cfFastWatchdogLogs++;
				}
				value = scaled;
				cfFastWatchdogHits++;
			}
		}
		tc2.load(value);
	} else if (reg == TC2EOI) {
		pendingInterrupts &= ~(1 << TC2OI);
	} else if (reg == RTCDRL) {

        log("RTC init value: %04x", rtc);
		rtc &= 0xFFFF0000;
		rtc |= (value & 0xFFFF);
		log("RTC write lower: %04x", value);
	} else if (reg == RTCDRU) {
		rtc &= 0x0000FFFF;
		rtc |= (value & 0xFFFF) << 16;
		log("RTC write upper: %04x", value);
	} else if (reg == CODR) {
		// Codec data write: 8-bit signed PCM sample. AudioCodecModel
		// handles the int16 widen, virtual TX FIFO and drain schedule.
		int8_t raw = (int8_t)(value & 0xFF);
		audio.pushDacSample(raw, passedCycles);
		dacHistory[dacHistoryHead] = raw;
		dacHistoryHead = (dacHistoryHead + 1) % kDacHistorySize;
		if (dacHistoryCount < kDacHistorySize) dacHistoryCount++;
	} else if (reg == CONFG) {
		uint32_t before = codecConfig;
		codecConfig = value;
		// Note the wall-clock cycle of the 0 → non-zero transition so
		// the tick-loop's settle-delayed CSINT firing knows when to
		// start. See `codecOnAtCycles` in windermere.h.
		if (before == 0 && value != 0) {
			codecOnAtCycles = passedCycles;
			recordingCodrBaseline = audio.codrReads;
		}
		if (value == 0) {
			pendingInterrupts &= ~(1u << CSINT);
			audio.resetAll();
			recordChannelPtr = 0;
			bootloaderPseudoDfc = false;
			codecOnAtCycles = -1;
		}
		// 0→3 transition on the V32 path. Two sources:
		//  - 5mx Pro bootloader: bare-metal codec init, no EPOC channel
		//    in memory yet. R5 contains whatever the bootloader's call
		//    chain happens to hold (not a valid channel pointer).
		//  - Patched 5mx-family OS (e.g. 5mx Pro v1.05(319)) that may
		//    have rewritten the dictaphone driver to do a STR instead
		//    of the STRB the original 5mx ROM uses. In this case R5
		//    holds the channel pointer per the surviving "mov r5, r0"
		//    prologue and we want to capture it just like the V8 path.
		// Try R5 first; if it's a plausible kernel/heap pointer treat
		// it as a real channel capture. If not, fall back to the
		// bootloader pseudo-DFC mode (which only needs CSINT to keep
		// firing as the FIFO drains, no channel struct required).
		if (before == 0 && value == 3 && recordChannelPtr == 0) {
			uint32_t ch = getGPR(5);
			if (ch >= 0x40000000 && ch < 0xe0000000) {
				recordChannelPtr = ch;
				log("Captured dictaphone channel pointer (V32) = %08x (lr=%08x)",
				    ch, getGPR(14));
			} else {
				bootloaderPseudoDfc = true;
				sawBootloaderPseudoDfc = true;
				log("V32 CONFG=3 with r5=%08x out-of-range (lr=%08x). Using bootloader pseudo-DFC mode.",
				    ch, getGPR(14));
			}
			pendingInterrupts |= (1u << CSINT);
		}
		if (!codecIntLoggedOnce || before != value) {
			log("CODEC CONFG: %08x -> %08x", before, value);
			codecIntLoggedOnce = true;
		}
	} else if (reg == COLFG) {
		codecLeftGain = value;
	} else if (reg == COEOI) {
		pendingInterrupts &= ~(1u << CSINT);
		audio.ackCsint();
	} else if (reg == COTEST) {
		// Test register — ignore.
	} else if (reg == BZCONT) {
		uint8_t oldCtrl = buzzerCtrl;
		bool wasOn = (buzzerCtrl & 0x01) != 0;
		buzzerCtrl = value & 0xFF;
		bool nowOn = (buzzerCtrl & 0x01) != 0;
		if (nowOn && !wasOn) audio.noteBuzzerRisingEdge();
		if (oldCtrl != buzzerCtrl)
			log("BZCONT32 %02x->%02x (BZMOD=%d BZTOG=%d) pc=%08x",
			    oldCtrl, buzzerCtrl, (buzzerCtrl >> 1) & 1, buzzerCtrl & 1, getGPR(15));
	} else if (reg == KSCAN) {
		// V32 path for the keyboard column-select. The 5mx Pro
		// bootloader's column scanner (FUN_6998) does
		//   str r0, [KSCAN]
		// to drive column N low, delays ~20 cycles, then reads
		// PADR for the row data. Without this case the 32-bit
		// write fell through to scratchRegs, kScan stayed at 0,
		// readKeyboard() returned the all-columns OR-of-all
		// fallback, and the bootloader's per-column scan saw
		// the same value for every column — so the "about"
		// Easter-egg matcher (which needs to identify which
		// specific key was pressed) never resolved a key.
		kScan = value & 0xFF;
	} else {
		uint32_t off = reg & 0xFFC;
		scratchRegs[off]     =  value        & 0xFF;
		scratchRegs[off + 1] = (value >>  8) & 0xFF;
		scratchRegs[off + 2] = (value >> 16) & 0xFF;
		scratchRegs[off + 3] = (value >> 24) & 0xFF;
	}
}

// PSION_TRACE_PERIPH=1 — emit a uniform per-access trace line for every
// peripheral / Etna register touched by the guest.  Same format used in
// sa1100.cpp so the resulting logs can be diffed directly between a 5mx
// boot and a Series 7 boot to spot driver-init divergences.  Off by default;
// adds ~one printf per MMIO access when enabled.  See
// scripts/diff_periph_trace.py for the comparator.
static bool peripheralTraceEnabled() {
	static const bool v = PSION_ENV_CSTR("PSION_TRACE_PERIPH") != nullptr;
	return v;
}

void Emulator::tracePeriph(const char *op, uint32_t physAddr, int sz,
                           uint32_t value) {
	if (!peripheralTraceEnabled()) return;
	// Classify the access into a stable device tag so a diff against the
	// S7 trace lines up by function rather than physical address (the two
	// SoCs use entirely different memory maps).
	const char *tag = "OTHER";
	uint8_t region = (uint8_t)(physAddr >> 24);
	uint16_t reg = physAddr & 0xFFF;
	if (region == 0x20) tag = "ETNA";
	else if (region == 0x80) {
		// Windermere SoC peripheral block.  Sub-categorise by 0xF00 group.
		uint16_t group = reg & 0xF00;
		switch (group) {
		case 0x500: tag = "INTC"; break;
		case 0xB00: tag = "SSP"; break;
		case 0xA00: tag = "CODEC"; break;
		case 0xC00: tag = "TIMER"; break;
		case 0xD00: tag = "RTC"; break;
		case 0xE00: tag = "GPIO"; break;
		case 0x200: tag = "LCD"; break;
		case 0x400: tag = "PWR"; break;
		case 0x600: tag = "UART1"; break;
		case 0x700: tag = "UART2"; break;
		default:    tag = "SOC"; break;
		}
	}
	log("PERIPH %s %-5s phys=%08x sz=%d val=%08x pc=%08x lr=%08x cyc=%lld",
	    op, tag, physAddr, sz, value, getGPR(15) - 4, getGPR(14),
	    (long long)passedCycles);
}

MaybeU32 Emulator::readPhysical(uint32_t physAddr, ValueSize valueSize) {
	uint8_t region = (physAddr >> 24) & 0xF1;
	uint8_t topByte = (uint8_t)(physAddr >> 24);
	// CF window classification. Windermere's PCMCIA controller places the
	// three CF windows for slot A inside bank 4:
	//   0x40000000-0x43FFFFFF: attribute memory (CIS + CCR)
	//   0x4C000000-0x4FFFFFFF: I/O  (ATA task-file registers)
	// and common memory slot A in bank 5 (0x50000000). Empirically confirmed
	// by tracing EPOC's PCCARD-ARM.LDD register writes. The bank-level
	// `region & 0xF1` mask collapses 0x40/0x48/0x4C into a single code, so
	// we test the full top byte for CF-specific routing.
	bool isAttrib = (topByte >= 0x40 && topByte <= 0x43);
	bool isCfIO   = (topByte >= 0x4C && topByte <= 0x4F);
	// Alternate I/O window: the 5mx Pro bootloader programs the SoC's PCMCIA
	// window register to map 0x44xxxxxx onto the same I/O space EPOC reaches
	// at 0x4Cxxxxxx. The bootloader's bare-metal PCMCIA driver does its
	// 0x55/0xAA scratch test (0x7f58) on the ATA Sector Count register and
	// then waits for ATA Status BSY (offset 7, bit 7) to clear (0x7fbc). On
	// the real SoC there are two configurable I/O windows; without the
	// alias here the bootloader's accesses land on unmapped memory, the
	// scratch test fails, and CF mount aborts before any ATA command issues.
	bool isCfIoAlt = (topByte >= 0x44 && topByte <= 0x47);
	// The ATA Data register at I/O offset 0 produces one access per halfword
	// of every sector transfer (256 per sector, 4096 per 16-sector read). We
	// never need to see each one — skip logging it entirely so the browser's
	// JS logger doesn't stall under the sustained call rate. Task-file
	// registers (offsets 1..7, 0xE..0xF) stay logged so boot-time and
	// command-issue sequences remain traceable.
	bool isCfDataReg = (isCfIO || isCfIoAlt) && ((physAddr & 0xF) == 0);
	// For V8 reads on CF windows, compute the value up front so the log can
	// show what EPOC actually sees. The ataRead16() side-effects are order-
	// sensitive, so we leave those (data-register and V16 reads) alone.
	bool logCfV8 = (valueSize == V8) &&
	               (isAttrib || ((isCfIO || isCfIoAlt) && !isCfDataReg) || region == 0x50) &&
	               cfProbeLogs < 2000;
	if (logCfV8) {
		uint32_t off = physAddr;
		uint8_t v;
		if (isAttrib)                  v = cfCard.readAttributeByte(off & 0x03FFFFFF);
		else if (isCfIO || isCfIoAlt)  v = cfCard.ataRead8(off & 0xF);
		else                           v = cfCard.readByte(off & 0xFFFFFF);
		const char *what = isAttrib  ? "attr"
		                 : isCfIoAlt ? "io-alt"
		                 : isCfIO    ? "io"
		                 : "common";
		log("CF %s read: addr=%08x -> %02x pc=%08x",
		    what, physAddr, v, getGPR(15) - 4);
		cfProbeLogs++;
		return v;
	}
	if ((isCfDataReg || (region == 0x60 && (physAddr & 0xF) != 0)) &&
	    cfProbeLogs < 2000 && valueSize != V16) {
		// Task-file / data register V8/V32 reads without the pre-read trick.
		int sizeTag = valueSize == V8 ? 1 : 4;
		log("CF io read: addr=%08x size=%d pc=%08x",
		    physAddr, sizeTag, getGPR(15) - 4);
		cfProbeLogs++;
	}
	// 16-bit reads land here when EPOC's PCCARD chunk helpers use LDRH to
	// drain the ATA Data register (see arm_pccd_chunk.cia's ReadHWordMultiple).
	// The CF I/O window must hand back a real 16-bit value so each LDRH
	// consumes one halfword of the 512-byte sector buffer; for all other
	// regions we synthesise from two byte reads to keep behaviour consistent.
	if (valueSize == V16) {
		if (isCfIO || isCfIoAlt) {
			// Log PC/LR and surrounding stack on the FIRST halfword of
			// each 256-halfword sector-drain burst. That first read's
			// stack frame tells us exactly which ROM function contains
			// the drain loop and who called it. If we know that, we
			// can invoke it directly to bypass the wait.
			static uint32_t lastDrainLogCyc = 0;
			if (cfCard.sectorBoundaryCount > 0 && passedCycles > lastDrainLogCyc + CLOCK_SPEED / 2) {
				uint32_t sp = getGPR(13);
				uint32_t s0 = readVirtualDebug(sp + 0x00, V32).value_or(0xDEADBEEF);
				uint32_t s4 = readVirtualDebug(sp + 0x04, V32).value_or(0xDEADBEEF);
				uint32_t s8 = readVirtualDebug(sp + 0x08, V32).value_or(0xDEADBEEF);
				uint32_t sc = readVirtualDebug(sp + 0x0c, V32).value_or(0xDEADBEEF);
				uint32_t s10 = readVirtualDebug(sp + 0x10, V32).value_or(0xDEADBEEF);
				uint32_t s14 = readVirtualDebug(sp + 0x14, V32).value_or(0xDEADBEEF);
				log("CF DATA READ: pc=%08x lr=%08x cpsr_mode=%02x  sp=%08x  stack: %08x %08x %08x %08x %08x %08x",
				    getGPR(15) - 4, getGPR(14), getCPSR() & 0x1F,
				    sp, s0, s4, s8, sc, s10, s14);
				lastDrainLogCyc = passedCycles;
			}
			return cfCard.ataRead16();
		}
		if (region == 0x60)
			return cfCard.ataRead16();
		MaybeU32 lo = readPhysical(physAddr, V8);
		MaybeU32 hi = readPhysical(physAddr + 1, V8);
		if (lo.has_value() && hi.has_value())
			return (lo.value() & 0xFF) | ((hi.value() & 0xFF) << 8);
		return {};
	}
	if (valueSize == V8) {
		if (region == 0)
			return ROM[physAddr & 0xFFFFFF];
		else if (region == 0x10)
			return ROM2[physAddr & 0x3FFFF];
		else if (region == 0x20 && physAddr <= 0x20000FFF) {
			uint32_t v = etna.readReg8(physAddr & 0xFFF);
			tracePeriph("rd", physAddr, 1, v);
			return v;
		}
		// CF card windows (see table above).
		else if (isAttrib)
			return cfCard.readAttributeByte(physAddr & 0x03FFFFFF);
		else if (isCfIO || isCfIoAlt)
			return cfCard.ataRead8(physAddr & 0xF);
		else if (region == 0x50)
			return cfCard.readByte(physAddr & 0xFFFFFF);
		else if (region == 0x60)
			return cfCard.ataRead8(physAddr & 0xF);
		else if (region == 0x80 && physAddr <= 0x80000FFF) {
			uint32_t v = readReg8(physAddr & 0xFFF);
			tracePeriph("rd", physAddr, 1, v);
			return v;
		}
#if defined(INCLUDE_BANK1)
		else if (region == 0xC0)
			return MemoryBlockC0[physAddr & MemoryBlockMask];
		else if (region == 0xC1)
			return MemoryBlockC1[physAddr & MemoryBlockMask];
		else if (region == 0xD0)
			return MemoryBlockD0[physAddr & MemoryBlockMask];
		else if (region == 0xD1)
			return MemoryBlockD1[physAddr & MemoryBlockMask];
#elif defined(INCLUDE_D)
		else if (region == 0xC0 || region == 0xC1)
			return MemoryBlockC0[physAddr & MemoryBlockMask];
		else if (region == 0xD0 || region == 0xD1)
			return MemoryBlockD0[physAddr & MemoryBlockMask];
#else
		else if (region == 0xC0 || region == 0xC1 || region == 0xD0 || region == 0xD1)
			return MemoryBlockC0[physAddr & MemoryBlockMask];
#endif
		else if (region >= 0xC0)
			return 0xFF; // just throw accesses to unmapped RAM away
	} else {
		uint32_t result;
		if (region == 0)
			LOAD_32LE(result, physAddr & 0xFFFFFF, ROM);
		else if (region == 0x10)
			LOAD_32LE(result, physAddr & 0x3FFFF, ROM2);
		else if (region == 0x20 && physAddr <= 0x20000FFF) {
			result = etna.readReg32(physAddr & 0xFFF);
			tracePeriph("rd", physAddr, 4, result);
		}
		else if (isAttrib) {
			// Attribute window: only even bytes carry CIS data. Assemble a
			// word from four byte reads so 32-bit accesses still see
			// consistent CIS content.
			uint32_t off = physAddr & 0x03FFFFFF;
			result = (uint32_t)cfCard.readAttributeByte(off)
			       | ((uint32_t)cfCard.readAttributeByte(off + 1) << 8)
			       | ((uint32_t)cfCard.readAttributeByte(off + 2) << 16)
			       | ((uint32_t)cfCard.readAttributeByte(off + 3) << 24);
		}
		else if (isCfIO || isCfIoAlt) {
			// 32-bit read pulls two 16-bit words from the ATA Data register
			// so back-to-back LDRs stream sector bytes at PIO width.
			uint32_t lo = cfCard.ataRead16();
			uint32_t hi = cfCard.ataRead16();
			result = lo | (hi << 16);
		}
		else if (region == 0x50)
			result = cfCard.readWord(physAddr & 0xFFFFFF);
		else if (region == 0x60) {
			uint32_t lo = cfCard.ataRead16();
			uint32_t hi = cfCard.ataRead16();
			result = lo | (hi << 16);
		}
		else if (region == 0x80 && physAddr <= 0x80000FFF) {
			result = readReg32(physAddr & 0xFFF);
			tracePeriph("rd", physAddr, 4, result);
		}
#if defined(INCLUDE_BANK1)
		else if (region == 0xC0)
			LOAD_32LE(result, physAddr & MemoryBlockMask, MemoryBlockC0);
		else if (region == 0xC1)
			LOAD_32LE(result, physAddr & MemoryBlockMask, MemoryBlockC1);
		else if (region == 0xD0)
			LOAD_32LE(result, physAddr & MemoryBlockMask, MemoryBlockD0);
		else if (region == 0xD1)
			LOAD_32LE(result, physAddr & MemoryBlockMask, MemoryBlockD1);
#elif defined(INCLUDE_D)
		else if (region == 0xC0 || region == 0xC1)
			LOAD_32LE(result, physAddr & MemoryBlockMask, MemoryBlockC0);
		else if (region == 0xD0 || region == 0xD1)
			LOAD_32LE(result, physAddr & MemoryBlockMask, MemoryBlockD0);
#else
		else if (region == 0xC0 || region == 0xC1 || region == 0xD0 || region == 0xD1)
			LOAD_32LE(result, physAddr & MemoryBlockMask, MemoryBlockC0);
#endif
		else if (region >= 0xC0)
			return 0xFFFFFFFF; // just throw accesses to unmapped RAM away
		else
			return {};
		return result;
	}

	return {};
}

bool Emulator::writePhysical(uint32_t value, uint32_t physAddr, ValueSize valueSize) {
	// 5mx Pro bootloader: bit 14 of [stash+0x8d4] is the EEPROM-derived
	// "boot mode" flag computed by the wake1-bit-banged PROM read at PC
	// 0x43c4..0x43cc. When the bit is CLEAR the bootloader runs the
	// cold-boot animation (logo splash + boot tune) and then sits in a
	// UART2 DSR poll waiting for a serial-cable connection. When the
	// bit is SET it skips both stages and CF-probes immediately.
	// We previously forced the bit set at the 0x43cc write to escape
	// the YModem-fallback DSR hang, but that ALSO skipped the boot
	// animation the user wants to see. Disabled for now while we look
	// at modelling the cold-boot path properly. (Currently the
	// bootloader will hang in the DSR poll until we either model the
	// EEPROM bit-bang or break the poll itself.)
	// if (physAddr == 0xd07fe8d4 && valueSize == V32 &&
	//     (getGPR(15) - 8) == 0x000043cc) {
	// 	value |= 0x4000;
	// }
	// Revo main-battery "100%" workaround. The SSI ADC bus delivers
	// at most 12 bits per sample (the kernel's reader at 0x5000d4a0
	// reads two SSDR bytes and reassembles them as
	// `(read1 & 0xf8) >> 3 | (read2 & 0x7f) << 5`, giving 4095 max),
	// but the Revo's HAL percentage curve treats 4000 as ~69% — its
	// "fully charged" anchor is set above what the 12-bit bus can
	// deliver, presumably because real hardware drives the divider
	// from a higher rail and the HAL takes the headroom into account.
	// As a result, even after pinning the main SSI channel at its
	// 4000 clamp (see the 0xA4A4 case in readReg32) the icon still
	// shows only four of five bars.
	//
	// The kernel caches each ADC sample at virt 0x8000001c (= phys
	// 0xd07e501c) via the STR at 0x5000c228, then folds it into the
	// running sum at +0x24 and an averaged percentage further down.
	// On the Revo family only, intercept that store and replace the clamped
	// 0xfa0 (4000) with a value past the 12-bit ceiling that the
	// curve maps to ~100%. Bumping to 5000 (= 1.25x the ADC max)
	// lights all five bars stably across all sampling iterations
	// without touching the ADC bit-packing protocol that 5mx / MC218
	// / 5mx Pro depend on.
	if (isRevoFamily() && physAddr == 0xd07e501c && valueSize == V32 && value == 0xfa0) {
		value = 5000;
	}

	// Hook-table probe: any write to physical 0xd07e5010..0xd07e502c
	// (= virtual 0x80000010..0x8000002c, the EKA1 IRQ-dispatch hook table).
	// Bounded to 64 entries to keep the log tidy; we only need to know
	// whether/when it ever gets populated.
	if (physAddr >= 0xd07e5010 && physAddr < 0xd07e5030) {
		static int hookTableWriteLogs = 0;
		if (hookTableWriteLogs < 64) {
			hookTableWriteLogs++;
			int sz = valueSize == V8 ? 1 : (valueSize == V16 ? 2 : 4);
			log("HOOKTAB write: phys=%08x (virt=%08x) size=%d value=%08x pc=%08x lr=%08x cpsr=%08x",
			    physAddr, 0x80000000u + (physAddr - 0xd07e5000u), sz, value,
			    getGPR(15) - 4, getGPR(14), getCPSR());
		}
	}
	// Diagnostic: log every write to the kernel TDfc / DFC-manager
	// area (virt 0x80000bf0..0x80000c30, phys 0xd07e5bf0..0xd07e5c30).
	// User-reported 5mxPro REC crash points the kernel to a callback
	// at virt 0x80000bfc whose fn pointer is 0x80000001 (uninit
	// sentinel). We need to know what the OS writes there, who writes
	// it, and what writes the 0x80000001 specifically. Throttle the
	// log to 256 entries so it doesn't drown the rest of the trace.
	if (physAddr >= 0xd07e5bf0 && physAddr < 0xd07e5c30) {
		static int tdfcWriteLogs = 0;
		if (tdfcWriteLogs < 256) {
			tdfcWriteLogs++;
			int sz = valueSize == V8 ? 1 : (valueSize == V16 ? 2 : 4);
			log("TDfc-area write: phys=%08x (virt=%08x) size=%d value=%08x pc=%08x lr=%08x",
			    physAddr, 0x80000000u + (physAddr - 0xd07e5000u), sz, value,
			    getGPR(15) - 4, getGPR(14));
		}
	}
	// Diagnostic: track every 32-bit write of the suspicious
	// 0x80000001 sentinel anywhere in the kernel data region. That
	// value showing up in a TDfc.iFunction slot is what crashes the
	// patched OS on REC, so seeing every write of it tells us which
	// kernel code path produces the bad pointer. Throttle to 64
	// entries.
	// Diagnostic: track every 32-bit write of the suspicious
	// 0x80000001 sentinel anywhere in PHYSICAL memory — not just the
	// kernel data region. The previous narrower filter
	// (0xd07e5000..0xd0800000) caught no writes, which suggests the
	// kernel writes the sentinel via a virtual address we don't expect
	// or to a different physical mapping. Cast a wider net.
	if (valueSize == V32 && value == 0x80000001) {
		static int sentinelWriteLogs = 0;
		if (sentinelWriteLogs < 128) {
			sentinelWriteLogs++;
			log("SENTINEL write: phys=%08x value=80000001 pc=%08x lr=%08x cpsr=%08x",
			    physAddr,
			    getGPR(15) - 4, getGPR(14), getCPSR());
		}
	}
	// Diagnostic: track every write to the suspect TDfc page-offset
	// area (offset 0xbf0..0xc30 within the page) for ANY page in BOTH
	// the C-bank (0xc0..0xc1) and D-bank (0xd0..0xd1) DRAM regions.
	// Latest user log (74c249d6 build) confirmed *(0x80000bfc) reads as
	// 0 at CONFG=3 time but the kernel dispatches 0x80000001 from the
	// same address moments later — and no write to this range was
	// caught. Two possibilities the wider filter rules in or out:
	//   * the kernel writes via a C-region alias mapping (which the
	//     previous D-only filter ignored); INCLUDE_D mode keeps the
	//     banks distinct, so a C-write wouldn't show up in our
	//     D-bank reads either — meaning the kernel MUST be reading
	//     from D for the dump to be authoritative. But it's cheap
	//     belt-and-braces.
	//   * the kernel writes via a path that doesn't go through
	//     writePhysical — caught by the virt-level write watcher we
	//     also add to arm710.cpp.
	if (((physAddr >= 0xd0000000 && physAddr < 0xd1000000) ||
	     (physAddr >= 0xc0000000 && physAddr < 0xc1000000)) &&
	    (physAddr & 0xFFF) >= 0xbf0 && (physAddr & 0xFFF) <= 0xc30) {
		static int tdfcAreaWriteLogs = 0;
		if (tdfcAreaWriteLogs < 256) {
			tdfcAreaWriteLogs++;
			int sz = valueSize == V8 ? 1 : (valueSize == V16 ? 2 : 4);
			log("TDfcAlias write: phys=%08x size=%d value=%08x pc=%08x lr=%08x",
			    physAddr, sz, value, getGPR(15) - 4, getGPR(14));
		}
	}
	// Mode 10 signal capture: the final store in FUN_5000f124 clears
	// DAT_5000f11c (0x80000820 virtual, 0xd07e5820 physical) at
	// pc=0x5000f1b8. This marks the end of a CF timer-queue DFC pass; we
	// latch a flag and defer the actual mask-bit-7 re-enable until the
	// CPU reaches the null-thread idle loop (safe context, no ISR in
	// flight). Avoids the re-entry hang caused by re-enabling directly
	// from IRQ mode.
	if (cfReenableMode == 10 && physAddr == 0xd07e5820 && value == 0) {
		uint32_t pc = getGPR(15) - 0xC;
		if (pc == 0x5000f1b8) {
			cfDfcCleanupSeen = true;
		}
	}
	uint8_t region = (physAddr >> 24) & 0xF1;
	uint8_t topByte = (uint8_t)(physAddr >> 24);
	bool isAttrib = (topByte >= 0x40 && topByte <= 0x43);
	bool isCfIoAlt = (topByte >= 0x44 && topByte <= 0x47);
	bool isCfIO   = (topByte >= 0x4C && topByte <= 0x4F);
	// Suppress Data-register noise (see matching comment in readPhysical).
	bool isCfDataReg = (isCfIO || isCfIoAlt) && ((physAddr & 0xF) == 0);
	if ((isAttrib || ((isCfIO || isCfIoAlt) && !isCfDataReg) || region == 0x50 ||
	     (region == 0x60 && (physAddr & 0xF) != 0)) && cfProbeLogs < 500) {
		const char *what = isAttrib  ? "attr"
		                 : isCfIoAlt ? "io-alt"
		                 : isCfIO    ? "io"
		                 : region == 0x50 ? "common" : "io60";
		int sizeTag = valueSize == V8 ? 1 : (valueSize == V16 ? 2 : 4);
		log("CF %s write: addr=%08x size=%d value=%08x pc=%08x",
		    what, physAddr, sizeTag, value, getGPR(15) - 4);
		cfProbeLogs++;
	}
	// Log caller at each CF I/O write, INCLUDING stack frame to find
	// outer caller (FUN_500552d8 pushes {r4, r5, r6, r7, lr} at entry,
	// so stack top holds saved r4 and further in is the outer return).
	if ((isCfIO || isCfIoAlt) && (physAddr & 0xF) == 7) {  // writes to ATA Command/Status reg
		static int logged = 0;
		if (logged < 8) {
			uint32_t sp = getGPR(13);
			uint32_t s0 = readVirtualDebug(sp + 0x00, V32).value_or(0xDEADBEEF);
			uint32_t s4 = readVirtualDebug(sp + 0x04, V32).value_or(0xDEADBEEF);
			uint32_t s8 = readVirtualDebug(sp + 0x08, V32).value_or(0xDEADBEEF);
			uint32_t sc = readVirtualDebug(sp + 0x0c, V32).value_or(0xDEADBEEF);
			uint32_t s10 = readVirtualDebug(sp + 0x10, V32).value_or(0xDEADBEEF);
			uint32_t s14 = readVirtualDebug(sp + 0x14, V32).value_or(0xDEADBEEF);
			log("CF cmd-reg write value=%02x lr=%08x stack[+0..+14]: %08x %08x %08x %08x %08x %08x",
			    value, getGPR(14), s0, s4, s8, sc, s10, s14);
			logged++;
		}
	}
	// 16-bit writes (STRH). CF I/O window takes one halfword as a push into
	// the ATA Data register; elsewhere we synthesise from two byte writes.
	if (valueSize == V16) {
		if (isCfIO || isCfIoAlt) {
			cfCard.ataWrite16((uint16_t)value);
			return true;
		}
		if (region == 0x60) {
			cfCard.ataWrite16((uint16_t)value);
			return true;
		}
		bool okLo = writePhysical(value & 0xFF, physAddr, V8);
		bool okHi = writePhysical((value >> 8) & 0xFF, physAddr + 1, V8);
		return okLo && okHi;
	}
	if (valueSize == V8) {
#if defined(INCLUDE_BANK1)
		if (region == 0xC0)
			MemoryBlockC0[physAddr & MemoryBlockMask] = (uint8_t)value;
		else if (region == 0xC1)
			MemoryBlockC1[physAddr & MemoryBlockMask] = (uint8_t)value;
		else if (region == 0xD0)
			MemoryBlockD0[physAddr & MemoryBlockMask] = (uint8_t)value;
		else if (region == 0xD1)
			MemoryBlockD1[physAddr & MemoryBlockMask] = (uint8_t)value;
#elif defined(INCLUDE_D)
		if (region == 0xC0 || region == 0xC1)
			MemoryBlockC0[physAddr & MemoryBlockMask] = (uint8_t)value;
		else if (region == 0xD0 || region == 0xD1)
			MemoryBlockD0[physAddr & MemoryBlockMask] = (uint8_t)value;
#else
		if (region == 0xC0 || region == 0xC1 || region == 0xD0 || region == 0xD1)
			MemoryBlockC0[physAddr & MemoryBlockMask] = (uint8_t)value;
#endif
		else if (region >= 0xC0)
			return true; // just throw accesses to unmapped RAM away
		else if (region == 0x20 && physAddr <= 0x20000FFF) {
			tracePeriph("wr", physAddr, 1, value);
			etna.writeReg8(physAddr & 0xFFF, value);
		}
		// Writes to attribute memory route into the card so EPOC can update
		// the Card Configuration Register (CCR) at attr offset 0x200 and
		// flip the card into I/O mode.
		else if (isAttrib) {
			cfCard.writeAttributeByte(physAddr & 0x03FFFFFF, (uint8_t)value);
			return true;
		}
		// I/O windows: ATA task-file registers. EPOC's PCCARD-ARM.LDD uses
		// 0x4Cxxxxxx on Windermere; the 5mx Pro bootloader uses an alias at
		// 0x44xxxxxx (see isCfIoAlt comment in readPhysical).
		else if (isCfIO || isCfIoAlt) {
			cfCard.ataWrite8(physAddr & 0xF, (uint8_t)value);
			return true;
		}
		else if (region == 0x50) {
			cfCard.writeByte(physAddr & 0xFFFFFF, (uint8_t)value);
			return true;
		}
		else if (region == 0x60) {
			cfCard.ataWrite8(physAddr & 0xF, (uint8_t)value);
			return true;
		}
		else if (region == 0x80 && physAddr <= 0x80000FFF) {
			tracePeriph("wr", physAddr, 1, value);
			writeReg8(physAddr & 0xFFF, value);
		}
		else if (region == 0 || region == 0x10)
			return true; // ROM/Flash: silently ignore stray writes
		else
			return false;
	} else {
#if defined(INCLUDE_BANK1)
		if (region == 0xC0)
			STORE_32LE(value, physAddr & MemoryBlockMask, MemoryBlockC0);
		else if (region == 0xC1)
			STORE_32LE(value, physAddr & MemoryBlockMask, MemoryBlockC1);
		else if (region == 0xD0)
			STORE_32LE(value, physAddr & MemoryBlockMask, MemoryBlockD0);
		else if (region == 0xD1)
			STORE_32LE(value, physAddr & MemoryBlockMask, MemoryBlockD1);
#elif defined(INCLUDE_D)
		if (region == 0xC0 || region == 0xC1)
			STORE_32LE(value, physAddr & MemoryBlockMask, MemoryBlockC0);
		else if (region == 0xD0 || region == 0xD1)
			STORE_32LE(value, physAddr & MemoryBlockMask, MemoryBlockD0);
#else
		if (region == 0xC0 || region == 0xC1 || region == 0xD0 || region == 0xD1)
			STORE_32LE(value, physAddr & MemoryBlockMask, MemoryBlockC0);
#endif
		else if (region >= 0xC0)
			return true; // just throw accesses to unmapped RAM away
		else if (region == 0x20 && physAddr <= 0x20000FFF) {
			tracePeriph("wr", physAddr, 4, value);
			etna.writeReg32(physAddr & 0xFFF, value);
		}
		else if (isAttrib) {
			// 32-bit write hits 4 attribute bytes. Only even ones carry
			// data (AD0 tied low); odd are 0xFF-mirrors on real CF.
			uint32_t off = physAddr & 0x03FFFFFF;
			cfCard.writeAttributeByte(off,     (uint8_t)value);
			cfCard.writeAttributeByte(off + 2, (uint8_t)(value >> 16));
			return true;
		}
		else if (isCfIO || isCfIoAlt) {
			// 32-bit write to the I/O window is a 16-bit Data-register push.
			cfCard.ataWrite16((uint16_t)value);
			return true;
		}
		else if (region == 0x50) {
			cfCard.writeWord(physAddr & 0xFFFFFF, value);
			return true;
		}
		else if (region == 0x60) {
			cfCard.ataWrite16((uint16_t)value);
			return true;
		}
		else if (region == 0x80 && physAddr <= 0x80000FFF) {
			tracePeriph("wr", physAddr, 4, value);
			writeReg32(physAddr & 0xFFF, value);
		}
		else if (region == 0 || region == 0x10)
			return true; // ROM/Flash: silently ignore stray writes
		else
			return false;
	}
	return true;
}



void Emulator::configure() {
	if (configured) return;
	configured = true;

	log("=== diag-build-v4 loaded ===");
	srand(1000);

	uart1.cpu = &cpu;
	uart2.cpu = &cpu;
	memset(&tc1, 0, sizeof(tc1));
	memset(&tc2, 0, sizeof(tc1));
	tc1.clockSpeed = CLOCK_SPEED;
	tc2.clockSpeed = CLOCK_SPEED;

	nextTickAt = TICK_INTERVAL;
	tc1.nextTickAt = tc1.tickInterval();
	tc2.nextTickAt = tc2.tickInterval();
	rtc = getRTC();

	audio.configure(CLOCK_SPEED);
	// Windermere kernels (5mx / MC218 / Revo / 5mx Pro) generate the
	// keyboard click as a single sub-tick BZCONT toggle, expecting the
	// audible burst to last ~one TINT (15.6 ms). AudioCodecModel's
	// default 3-tick hold paints a longer continuous tone that some
	// click patterns end up running into each other, smearing the
	// individual clicks into a flat tone. Match the original Windermere
	// constant exactly so historical click character is preserved.
	audio.buzzerClickHoldTicks = 1;

    log("RTC: %04x", rtc);

	reset();
}

uint8_t *Emulator::getROMBuffer() {
	return ROM;
}
size_t Emulator::getROMSize() {
	return sizeof(ROM);
}
// Finds the model UID constant (the high half of EPOC's "Unique id") in the
// freshly-loaded ROM. Every 5mx-family ROM here carries exactly one copy, in
// a literal pool in the variant code; a build that doesn't carry it at all
// (the 5mx Pro bootloader, whose OS comes off the CF card) leaves the list
// empty, and locateMachineIdPrefixOnCard finds the live copy instead.
void Emulator::locateMachineIdPrefix() {
	machineIdPrefix = kMachineIdPrefixDefault;
	findRomWords(ROM, sizeof(ROM), kMachineIdPrefixDefault, machineIdPrefixOffsets);
}

// Where the model UID sits in the attached CF image. Only the 5mx Pro needs
// this: on 5mx / MC218 the OS is the ROM, so the ROM copy is the live one.
void Emulator::locateMachineIdPrefixOnCard() {
	machineIdPrefixCardOffsets.clear();
	if (!isMx5Pro() || !cfCard.inserted()) return;
	findCardMachineIdWords(cfCard.data(), cfCard.imageSize(),
	                       kMachineIdPrefixDefault, machineIdPrefix,
	                       machineIdPrefixCardOffsets);
}

void Emulator::applyMachineIdPrefixToCard() {
	if (!cfCard.inserted() || machineIdPrefixCardOffsets.empty()) return;
	writeRomWords(cfCard.data(), machineIdPrefixCardOffsets, machineIdPrefix);
	log("Unique id: patched model UID %08x into %zu place(s) on the CF image",
	    machineIdPrefix, machineIdPrefixCardOffsets.size());
}

bool Emulator::setMachineIdPrefix(uint32_t prefix) {
	if (!canSetMachineIdPrefix()) return false;
	writeRomWords(ROM, machineIdPrefixOffsets, prefix);
	machineIdPrefix = prefix;
	// 5mx Pro: the OS that prints the id comes off the CF card, so patch
	// the copy there too. With no card in the slot this is a no-op and the
	// value is applied when one is attached — which is the normal order,
	// the frontend ejecting the OS card on every reset.
	applyMachineIdPrefixToCard();
	return true;
}

void Emulator::loadROM(uint8_t *buffer, size_t size) {
	memcpy(ROM, buffer, std::min(size, sizeof(ROM)));
	// The ROM is the only copy of the model UID, so find it before anything
	// else patches or runs (the host programs the Unique id right after
	// this returns, before the first instruction).
	locateMachineIdPrefix();
	// Auto-detect which Windermere ROM variant we have and resolve the
	// PCCARD-ATA retry-callback + import-trampoline addresses, so the
	// CF accel-timer and direct-invoke hooks work across 5mx, 5mx Pro
	// and MC218. Signature-match on the first 4 bytes of the callback
	// prologue (`push {r4-r6, lr}; sub sp, sp, #8` = e92d4070, e24dd008).
	auto rom32 = [&](uint32_t romOff) -> uint32_t {
		if (romOff + 4 > sizeof(ROM)) return 0;
		return (uint32_t)ROM[romOff] | ((uint32_t)ROM[romOff+1] << 8) |
		       ((uint32_t)ROM[romOff+2] << 16) | ((uint32_t)ROM[romOff+3] << 24);
	};
	struct Variant { uint32_t callback; uint32_t trampoline; };
	static constexpr Variant candidates[] = {
		{0x50083178, 0x500842fc},  // 5mx v1.05(260) / MC218 v1.05(259)
		{0x500a5108, 0x500a628c},  // 5mx Pro v1.05(319)
	};
	// On the bootloader-booted 5mx Pro path the OS lives in DRAM after the
	// bootloader copies SYS$ROM.BIN out of CF, so neither candidate matches
	// against ROM[] (the 128 KB bootloader flash). Detect that case by ROM
	// size and pre-set the 5mx Pro v1.05(319) trampoline addresses, so
	// the tick-loop CF-accel hook is armed and ready by the time the OS
	// starts executing. The hook itself is safely gated by
	// `pc == trampoline && r1 == 2000000 && r2 == callback`, so pre-arming
	// is harmless: it can't fire while the bootloader runs (PCs are in
	// the 0x00000000 region, not 0x500a628c) and only kicks in once the
	// OS image is live in DRAM and starts arming PCCARD-ATA retries.
	cfRomCallbackAddr = 0;
	cfRomTrampolinePC = 0;
	for (auto &v : candidates) {
		uint32_t cbOff   = v.callback   - 0x50000000;
		uint32_t tpOff   = v.trampoline - 0x50000000;
		if (rom32(cbOff)   == 0xe92d4070 &&
		    rom32(cbOff+4) == 0xe24dd008 &&
		    rom32(tpOff)   == 0xe59fc000 &&
		    rom32(tpOff+4) == 0xe59cf000) {
			cfRomCallbackAddr = v.callback;
			cfRomTrampolinePC = v.trampoline;
			break;
		}
	}
	// 5mx Pro bootloader path: ROM is 128 KB (just the bootloader flash;
	// the OS gets copied into DRAM at runtime). Pre-arm the v1.05(319)
	// trampoline so CF accel kicks in the moment the loaded OS starts
	// arming PCCARD-ATA retries — without this the OS spends ~2 s per
	// FAT sector on its post-handoff remount, making boot effectively
	// hang from the user's perspective unless they detach the card.
	if (cfRomTrampolinePC == 0 && size <= 0x40000) {
		cfRomCallbackAddr = 0x500a5108;
		cfRomTrampolinePC = 0x500a628c;
	}
	log("CF ROM detect: callback=%08x trampoline=%08x",
	    cfRomCallbackAddr, cfRomTrampolinePC);
}

void Emulator::executeUntil(int64_t cycles) {
	if (!configured)
		configure();

	// PSION_PROBE_5MXPRO_STATE=1 prints a one-line dump of dictaphone
	// state every ~1 sim-second while recordChannelPtr != 0. Inert
	// when the env var is unset. Used to investigate why recording
	// doesn't advance on the patched 5mx Pro OS.
	auto probe5mxProState = [&]() {
		static int s_probeEnabled = -1;
		if (s_probeEnabled == -1) {
			const char *e = PSION_ENV_CSTR("PSION_PROBE_5MXPRO_STATE");
			s_probeEnabled = (e && *e && *e != '0') ? 1 : 0;
		}
		if (!s_probeEnabled) return;
		// High-resolution mode: when recordChannelPtr is set, probe at
		// ~1 ms intervals for the first 500 ms (captures the post-CONFG=3
		// window where the patched OS sets state=1), then 5 s intervals.
		static int64_t s_nextProbe = 0;
		static int64_t s_recordChannelSetAt = -1;
		if (recordChannelPtr != 0 && s_recordChannelSetAt < 0) {
			s_recordChannelSetAt = passedCycles;
			s_nextProbe = passedCycles;  // probe immediately on capture
		}
		if (recordChannelPtr == 0) s_recordChannelSetAt = -1;
		if (passedCycles < s_nextProbe) return;
		bool fineWindow = (s_recordChannelSetAt >= 0) &&
		    ((passedCycles - s_recordChannelSetAt) < CLOCK_SPEED / 2);
		s_nextProbe = passedCycles + (fineWindow ? CLOCK_SPEED / 1000 : CLOCK_SPEED * 5);
		auto sopt = readRamVirt32(recordChannelPtr + 0x1c);
		uint32_t cpsr = cpu.getCPSR();
		log("PROBE-5mxPro: cyc=%lld ch=%08x state=%s%u adcFill=%zu csintPend=%d cfg=%02x codrReads=%llu cap=%d defAt=%lld intens=%04x halted=%d cpsr=%02x",
		    (long long)passedCycles,
		    recordChannelPtr, sopt.has_value() ? "" : "?",
		    (unsigned)sopt.value_or(0),
		    audio.adcRingFill(), (pendingInterrupts >> CSINT) & 1,
		    codecConfig & 0xff,
		    (unsigned long long)audio.codrReads,
		    audio.debugAdcFifoReadsSinceRefill(),
		    (long long)audio.debugCsintDeferredAt(),
		    interruptMask & 0xffff,
		    halted ? 1 : 0,
		    cpsr & 0xff);
	};

	while (!asleep && passedCycles < cycles) {
		probe5mxProState();
		if (passedCycles >= nextTickAt) {
			// increment RTCDIV
			if ((pwrsr & 0x3F) == 0x3F) {
				rtc++;
				pwrsr &= ~0x3F;
			} else {
				pwrsr++;
			}

			nextTickAt += TICK_INTERVAL;
			pendingInterrupts |= (1<<TINT);

			// Bootloader-OS DRAM rescan (fallback). The loadROM-time
			// pre-arm above handles the 128 KB 5mx Pro bootloader case,
			// but keep a per-TINT signature scan over DRAM as a safety
			// net so any future bootloader-loaded OS variant that
			// doesn't hit the pre-arm path still gets CF accel once
			// the OS image lands in RAM. Cheap: 32 bytes of DRAM read
			// per TINT (64 Hz sim time) until the trampoline is found,
			// then dormant. Without this the OS post-handoff remount
			// drains FAT at ~2 s per sector, which is the symptom the
			// "Insert CF card containing OS" flow used to paper over
			// with an auto-detach 20 s in.
			if (cfRomTrampolinePC == 0) {
				static constexpr struct { uint32_t cb, tp; } kDramCands[] = {
					{0x500a5108, 0x500a628c},  // 5mx Pro v1.05(319)
					{0x50083178, 0x500842fc},  // 5mx v1.05(260) / MC218 v1.05(259)
				};
				auto dram32 = [&](const uint8_t *bank, uint32_t off) -> uint32_t {
					if (off + 4 > sizeof(MemoryBlockC0)) return 0;
					return (uint32_t)bank[off]
					     | ((uint32_t)bank[off+1] << 8)
					     | ((uint32_t)bank[off+2] << 16)
					     | ((uint32_t)bank[off+3] << 24);
				};
				for (const uint8_t *bank : {(const uint8_t*)MemoryBlockC0,
				                            (const uint8_t*)MemoryBlockC1}) {
					if (cfRomTrampolinePC != 0) break;
					for (const auto &v : kDramCands) {
						uint32_t cbOff = v.cb - 0x50000000;
						uint32_t tpOff = v.tp - 0x50000000;
						if (dram32(bank, cbOff)   == 0xe92d4070 &&
						    dram32(bank, cbOff+4) == 0xe24dd008 &&
						    dram32(bank, tpOff)   == 0xe59fc000 &&
						    dram32(bank, tpOff+4) == 0xe59cf000) {
							cfRomCallbackAddr = v.cb;
							cfRomTrampolinePC = v.tp;
							log("CF DRAM detect (fallback): callback=%08x trampoline=%08x bank=%s",
							    cfRomCallbackAddr, cfRomTrampolinePC,
							    bank == MemoryBlockC0 ? "C0" : "C1");
							break;
						}
					}
				}
			}

			// Codec service tick. Wake the DFC whenever the channel is
			// in any non-idle state so the state 3 warmup counter
			// decrements, state 4 winddown completes, and state=1
			// delivery callbacks fire even if the app hasn't pushed
			// mic data yet. For state=1 with mic data queued, we
			// additionally re-assert CSINT after every COEOI in the
			// writeReg handlers so the DFC keeps draining 16 samples
			// per run until adcQueue is empty — that's what actually
			// matches the 8 kHz mic input rate (TINT at 64 Hz gives
			// only 1024 samples/s, which underruns and aborts the app).
			if (recordChannelPtr != 0) {
				// Probe the channel state at +0x1c; on the original 5mx
				// ROM it's 1-4 for active codec phases. Patched ROMs
				// (notably 5mx Pro v1.05(319)) appear to use a different
				// layout — the probe lands either a value outside that
				// range or, more commonly, just 0. To keep recordings
				// running on those ROMs while preserving the original
				// 5mx behaviour where state==0 means "the kernel hasn't
				// armed recording yet, don't fire spurious CSINT", we
				// fire whenever EITHER (a) the probed state is in the
				// known-active range, OR (b) the codec config has the
				// recording/playback bits on. (b) is the kernel-level
				// "I want audio to be moving" signal and matches what
				// recordChannelPtr being set already implies via the
				// CONFG=0 cleanup path.
				uint32_t channelState = 0;
				bool stateKnown = false;
				if (auto v = readRamVirt32(recordChannelPtr + 0x1c); v.has_value()) {
					channelState = v.value();
					stateKnown = true;
				}

				// Mic-disabled record fallback. The kernel programs
				// state=1 when the user taps REC. If the host mic
				// hasn't been granted, writeAudioInput drops every
				// pushed sample; the DFC's drain loop then runs
				// against an empty ADC ring and the kernel reboots
				// back to splash a moment later (the user-visible
				// "5mx record crash" symptom). Tearing the codec
				// down mid-stream (the previous fix) was fragile —
				// it left codecConfig=0xFFFFFFFF as a sentinel that
				// downstream register paths weren't designed to
				// see, and the abort fired every tick which flooded
				// the log. Instead, synthesise silence at the host's
				// rate: enqueue 125 zero samples per TINT (64 Hz
				// tick × 125 samples = 8000/s = the codec rate) so
				// the kernel's drain loop completes normally with
				// silence and the codec state machine stays sane.
				// The user gets a silent recording — equivalent to
				// recording in a soundproof room — and no crash.
				if (stateKnown && channelState == 1 &&
				    !audio.hostMicEnabled()) {
					// Throttle the ring so we don't pile up more
					// than the kernel can drain. AudioCodecModel
					// caps per-cycle drains at 16 samples (one FIFO
					// depth); keep ~32 samples queued so each CSINT
					// has a full FIFO to drain.
					constexpr int kSamplesPerTick = AudioCodecModel::kAudioSampleRate / 64;
					if (audio.adcRingFill() < (size_t)kSamplesPerTick) {
						static const int16_t kSilence[kSamplesPerTick] = {};
						audio.enqueueMicSamples(kSilence, kSamplesPerTick);
					}
					// Fall through to the CSINT firing below — same
					// path the mic-enabled flow takes once samples
					// are queued.
				}
				// CSINT firing rule.
				//
				// Real hardware fires CSINT when:
				//   - the RX FIFO has data (recording: state=1), and/or
				//   - the TX FIFO has room (playback: state=2).
				// Our emulator already covers those via:
				//   - writeAudioInput's empty→nonempty edge firing,
				//     plus the silence-feed branch above for the
				//     mic-off case, both for state=1.
				//   - tickTxFifoDrain firing CSINT below the TX
				//     watermark for state=2.
				//   - tickDeferredCsint re-firing after a per-cycle
				//     16-sample cap was hit with data still queued.
				// So for state=1 / state=2 the tick-loop firing here is
				// REDUNDANT and harmful — it races the kernel during
				// the narrow window between writeReg8 CONFG=3
				// (our capture point) and the kernel's tail-end
				// thunk_FUN_500119e4 (unmask CSINT) / thunk_FUN_5001b9e0
				// (schedule DFC), dispatching a CSINT against a not-
				// yet-fully-installed DFC and triggering the
				// "all-IRQs-off → cp15=0 → reboot" exception path the
				// user-reported "5mx record crash" log shows.
				//
				// State=3 (open/warmup) is different: the kernel's DFC
				// expects CSINT to advance its 0x20 warmup counter
				// regardless of RX/TX FIFO state, so the tick-loop
				// firing IS what drives that flow. State=4 (winddown)
				// works the same way.
				//
				// Bootloader pseudo-DFC: fire CSINT while the
				// bootloader's own V32 CONFG=3 is LIVE — the bootloader
				// has no EPOC channel struct but its CSINT handler is
				// designed to keep being woken up as the TX FIFO
				// drains. CRITICAL: gate on `bootloaderPseudoDfc`
				// (live, cleared on bootloader's CONFG=0) NOT
				// `sawBootloaderPseudoDfc` (sticky for the whole
				// session). The sticky form was used in earlier
				// revisions to also drive 5mx Pro patched-OS Voice
				// Notes recording, but the patched OS installs its
				// CSINT handler AFTER writing CONFG=3 — the inverse
				// of stock 5mx FUN_5009fcb0 — so a CSINT firing
				// anywhere in the window between CONFG=3 and the
				// kernel's tail-end install would dispatch into an
				// un-installed handler chain and crash the OS to
				// splash (the user-reported "5mxPro REC button
				// reboots OS", recurring after every refactor of
				// this gate). Using the live flag means:
				//   - During the bootloader's own CONFG=3 active
				//     period: CSINT fires every TINT, pseudo-DFC
				//     progresses. ✓
				//   - After the bootloader writes CONFG=0:
				//     bootloaderPseudoDfc=false, the fallback stops
				//     firing. The OS runs without our tick-loop
				//     poking its handlers. ✓
				//   - 5mx Pro Voice Notes recording (after the
				//     bootloader has run): bootloaderPseudoDfc is
				//     false, but sawBootloaderPseudoDfc is true and
				//     the patched-OS channel struct doesn't put
				//     state at +0x1c so stateNeedsTickCsint stays
				//     false. We DO want CSINT to fire so the
				//     kernel's drain loop runs and the user's
				//     recording timer advances — but ONLY after the
				//     patched OS has had time to install its CSINT
				//     handler. The crash window is small (a few
				//     register writes after CONFG=3), so a
				//     ~100 ms settling delay puts the first CSINT
				//     well past the install while still feeling
				//     responsive to the user. The window is
				//     bracketed by codecOnAtCycles (set on the
				//     0 → non-zero CONFG transition above, reset
				//     on CONFG=0) and re-arms every recording
				//     session.
				bool stateNeedsTickCsint =
					stateKnown && (channelState == 3 || channelState == 4);
				// Patched-OS (post-bootloader) settling delay.
				constexpr int64_t kPatchedOsSettleCycles = (int64_t)CLOCK_SPEED / 10;  // ~100 ms; protects against CSINT firing during the patched-OS handler install window
				bool sufficientlySettled = !sawBootloaderPseudoDfc ||
				    (codecOnAtCycles >= 0 &&
				     (passedCycles - codecOnAtCycles) > kPatchedOsSettleCycles);
				// CRITICAL: gate stateNeedsTickCsint on the settling delay
				// too. On the 5mx Pro patched OS, the channel struct
				// doesn't put state at +0x1c — any value the probe reads
				// is whatever the patched dictaphone driver happens to
				// leave in that kernel-RAM region. It can occasionally
				// coincide with 3 or 4 (warmup / winddown range) and
				// fire CSINT immediately on the first TINT after
				// CONFG=3, which races the patched-OS handler install
				// and crashes into 0x80000001. Gate on the same
				// codecOnAtCycles settling check writeAudioInput uses
				// — the firing waits until well past the install
				// window on 5mx Pro, and behaves as before (immediate)
				// on stock 5mx / MC218 / Revo where sawBootloaderPseudoDfc
				// is never set.
				bool stateGatedTickCsint = stateNeedsTickCsint && sufficientlySettled;
				// Patched-OS recording path: re-enable per-TINT CSINT
				// firing but with BACKPRESSURE — only fire when CSINT
				// isn't already pending. The recursive-IRQ corruption
				// the previous code triggered came from CSINT firing
				// over and over while the kernel's IRQ dispatcher had
				// re-entered the handler 100+ times (each entry pushing
				// to SP_irq, walking it through kernel data and clobbering
				// the object-table base pointer). Backpressure prevents
				// us from PILING UP additional pendings while the kernel
				// still holds the previous one — so the kernel sees
				// exactly one CSINT, processes it, acks via CODR drain
				// or COEOI, and only THEN do we fire the next.
				//
				// Backpressure is APPLIED ONLY to the patchedOsRecordingSettled
				// branch. Stock 5mx / MC218 / Revo use the stateGatedTickCsint
				// or bootloaderPseudoDfc branches — neither of which had
				// the recursive-IRQ vulnerability (stock OS installs its
				// CSINT handler before CONFG=3; bootloader's own handler
				// is similarly pre-installed), and both relied on the
				// unconditional `ackCsint()` per TINT to reset the FIFO
				// read-counter so the kernel's tight CODR-drain loops
				// could keep getting fresh 16-sample budgets. Narrowing
				// backpressure preserves their exact prior behaviour.
				bool patchedOsRecordingSettled = sawBootloaderPseudoDfc &&
				    !bootloaderPseudoDfc &&
				    codecOnAtCycles >= 0 &&
				    (passedCycles - codecOnAtCycles) > kPatchedOsSettleCycles;
				// Restore the commit-418c9585 simple "fire CSINT every TINT
				// when codec active" approach. The backpressure that was
				// added later to fix a recursive-IRQ crash is now
				// unnecessary because:
				//   - The recursive-IRQ root cause was the kernel reading
				//     garbage from channel+0x1c (the wrong-block bug —
				//     fixed in commit 6c7e8744). With correct state reads
				//     the kernel takes the right path through its CSINT
				//     handler and acks CSINT cleanly each time.
				//   - Without backpressure, audio.ackCsint() gets called
				//     every TINT, resetting the per-cycle FIFO cap so the
				//     kernel can drain another 16 samples on its next
				//     CSINT handler entry. That's what drives continuous
				//     8 kHz drain.
				//
				// FIX (2026-05-30): for CONFIRMED recording (channel state==1)
				// do NOT force-fire CSINT every TINT. Proven via clean
				// isolation: the exact same 5mxPro OS image records perfectly
				// when booted as variant Mx5 (edge-driven CSINT) but dies at
				// one buffer when force-driven here. The force-fire drives the
				// raw FIFO drain but starves the kernel's per-buffer completion
				// DFC — the kernel reaches buffer-full, never enqueues the
				// completion (static TDfc slot stays 0), idles in WFI, and
				// resets. Driving CSINT edge-accurately (writeAudioInput's
				// empty→nonempty edge + AudioCodecModel::tickDeferredCsint
				// re-fire, both already settle-gated for the patched OS) lets
				// the kernel's real CSINT ISR FUN_5009f4ec run, queue the
				// completion DFC, and the app resubmit — exactly the working
				// Mx5 path. Force-fire is still used for warmup/winddown
				// (stateGatedTickCsint) and the bootloader pseudo-DFC, neither
				// of which has a per-buffer completion to starve.
				bool confirmedRecording = stateKnown && channelState == 1;
				// Kick-start for confirmed recording: the edge-fired CSINT
				// (writeAudioInput, empty→nonempty) is missed when that
				// transition lands inside the ~100 ms settle window, so the
				// drain never starts (codrReads stuck at 0, adcFill climbs).
				// Fire CSINT after settle ONLY until the drain begins
				// (codrReads advances past the per-session baseline), then
				// stop and let the hardware-accurate edge+deferred path
				// sustain it exactly like the working Mx5 boot. Critically we
				// do NOT keep firing once draining (the old behaviour), so at
				// buffer-full the kernel's per-buffer completion DFC is no
				// longer starved.
				bool recordingNeedsKick =
				    confirmedRecording && patchedOsRecordingSettled &&
				    audio.codrReads <= recordingCodrBaseline + 8;
				bool codecActive = stateGatedTickCsint ||
				                   (bootloaderPseudoDfc && (codecConfig & 0x03) != 0) ||
				                   (patchedOsRecordingSettled && (codecConfig & 0x03) != 0
				                    && !confirmedRecording) ||
				                   recordingNeedsKick;
				if (codecActive) {
					audio.ackCsint();
					pendingInterrupts |= (1u << CSINT);
				}
				// (per-TINT refresh disabled along with the install
				// — see above.)

				// 5mxPro patched-OS recording: when CONFG.RX is enabled
				// but the host mic is NOT, inject silence into the ADC
				// ring at the codec sample rate. Real codec would
				// generate continuous silence samples in this state; we
				// need to feed equivalent data so the kernel's CSINT
				// handler drains via CODR reads (which is what clears
				// INTSR.CSINT — without data to drain, the handler
				// would leave CSINT asserted and re-fire). One TINT =
				// 8000 / 64 = 125 samples per tick.
				if (patchedOsRecordingSettled && (codecConfig & 0x01) != 0 &&
				    !audio.hostMicEnabled()) {
					static const int16_t silence[125] = {0};
					audio.enqueueMicSamples(silence, 125);
				}
			}

			// Buzzer synthesiser. BZCONT bit 1 selects manual vs TC1-
			// tone mode; emitBuzzerSamples handles the square-wave
			// pump and idle-transition zero-tick, mirroring the codec
			// DAC ring it shares.
			//
			// Buzzer activation rules — same for all four Windermere
			// variants (5mx / MC218 / Revo / 5mx Pro). Real hardware
			// drives the piezo on every BZTOG transition via the
			// mechanical/capacitive envelope, regardless of TC1 state,
			// so all four devices produce clicks on the rising-edge
			// hold latch.
			//
			// History note re: 5mx Pro: commit 5a7ab547 added a
			// `tc1.config & Timer::ENABLED` gate specifically on
			// 5mx Pro to suppress one driver-init BZTOG pulse that
			// was empirically associated with a CF-mount stall on
			// the bootloader-loaded OS. ebf22337 then re-applied the
			// gate after f19e6709 dropped it. The CF accel path has
			// since been hardened — 3df2affa pre-arms the trampoline
			// at loadROM time, 47f1e812 added a per-TINT DRAM rescan
			// safety net — so the underlying stall is fixed at its
			// real cause. The buzzer gate was only ever masking a
			// cosmetic click on top of the real CF accel fix. Drop
			// the gate so 5mx Pro keyboard clicks become audible to
			// match real piezo behaviour. If the CF mount stall
			// recurs on 5mx Pro, fix the CF accel path (the actual
			// root cause) rather than re-gate the buzzer.
			bool bzActive = false;
			if (buzzerCtrl & 0x02) {
				// BZMOD=1: TC1-driven tone — auto-toggles the pin on
				// every TC1 underflow on real hardware. We gate on
				// BZTOG=1 as a workaround so brief BZMOD=1 phases at
				// boot with BZTOG=0 don't pump a spurious tone.
				bzActive = (buzzerCtrl & 0x01) != 0;
			} else if (audio.buzzerHolding()) {
				// BZMOD=0: manual mode where BZTOG drives the pin
				// level directly. Real piezo only sounds on BZTOG
				// transitions (mechanical/capacitive click), NOT
				// while held high — a steady BZTOG=1 is a DC pin
				// level, silent on hardware. The hold latch (set on
				// the rising edge by noteBuzzerRisingEdge) is what
				// carries the sub-tick BZTOG pulse the kernel emits
				// into our 64 Hz pump window. Dropping the previous
				// "(buzzerCtrl & 0x01)" steady gate silenced the
				// continuous-tone symptom on devices where a kernel
				// holds BZTOG=1 without clearing it.
				bzActive = true;
			}
			int toneHz = audio.buzzerClickHz;
			if (bzActive && (buzzerCtrl & 0x01)) {
				int tickRate = (tc1.config & Timer::MODE_512KHZ) ? 512000 : 2000;
				int interval = tc1.interval > 0 ? (int)tc1.interval : 1;
				int underflowHz = tickRate / interval;
				int tc1Hz = underflowHz / 2;
				if (tc1Hz >= 50 && tc1Hz <= 4000) toneHz = tc1Hz;
			}
			audio.emitBuzzerSamples(bzActive, toneHz);
		}
		if (tc1.tick(passedCycles))
			pendingInterrupts |= (1<<TC1OI);
		if (tc2.tick(passedCycles))
			pendingInterrupts |= (1<<TC2OI);

		// Execute any deferred codec CSINT re-fire scheduled by the
		// CODR read path. See AudioCodecModel::tickDeferredCsint —
		// summary: EPOC's state-1 drain doesn't ack with COEOI, so
		// the 16-sample per-drain cap never clears and subsequent
		// drains return empty even with a full ADC ring. When the
		// cap is hit with data remaining, the model schedules a
		// re-fire ~125 µs later. Gating CSINT on recordChannelPtr
		// keeps the 5mx state==0 idle window from spuriously firing.
		//
		// CRITICAL: on the 5mxPro patched OS, the kernel installs its
		// CSINT handler AFTER writing CONFG=3 (the inverse of stock 5mx
		// FUN_5009fcb0 ordering). Firing CSINT inside the install
		// window triggers a recursive IRQ — INTSR.CSINT stays asserted
		// (no COEOI yet) and the kernel's IRQ dispatcher transitions
		// to Undef mode with I=0 at the 0x50006738 MSR, which re-fires
		// IRQ immediately, recursing through SP_irq until it clobbers
		// the kernel object-table base pointer at 0x80000908. Apply
		// the same `sawBootloaderPseudoDfc + 100 ms` settling gate
		// the tick-loop and writeAudioInput use.
		constexpr int64_t kPatchedOsSettleCyc = (int64_t)CLOCK_SPEED / 10;  // ~100 ms
		bool patchedOsRecordingSettled = !sawBootloaderPseudoDfc ||
		    (codecOnAtCycles >= 0 &&
		     (passedCycles - codecOnAtCycles) > kPatchedOsSettleCyc);
		if (audio.tickDeferredCsint(passedCycles) && recordChannelPtr != 0
		    && patchedOsRecordingSettled) {
			pendingInterrupts |= (1u << CSINT);
		}
		// Virtual TX-FIFO drain. AudioCodecModel decrements the
		// virtual FIFO at the codec sample rate and returns true on
		// each tick that crosses below the TX watermark. Normally
		// gated on EPOC's audio driver having set recordChannelPtr;
		// the 5mx Pro bootloader has no such driver but still wants
		// CSINT to fire as its FIFO drains (its CSINT handler invokes
		// a wake callback once it sees TX_FULL clear). Allow either.
		// The patchedOsRecordingSettled gate above also protects this
		// path from the 5mxPro patched-OS install-window race; the
		// bootloader pseudo-DFC path stays immediate because
		// sawBootloaderPseudoDfc is true but bootloaderPseudoDfc is
		// ALSO true, so codecOnAtCycles is current and the settling
		// check evaluates against the bootloader's own CONFG=3 — which
		// is fine because the bootloader's CSINT handler is already
		// installed before its CONFG=3.
		while (audio.tickTxFifoDrain(passedCycles)) {
			bool bootloaderOk = bootloaderPseudoDfc;
			bool patchedOsOk = (recordChannelPtr != 0) && patchedOsRecordingSettled;
			if ((bootloaderOk || patchedOsOk) &&
			    (pendingInterrupts & (1u << CSINT)) == 0) {
				pendingInterrupts |= (1u << CSINT);
			}
		}

		// Mode 9 / 10: wall-clock-compress the 2 s CF poll gap.
		// Diagnostics established (via --pc-sample-hz 128 traces) that:
		//   * During the 2 s gap, halted=0 in every dump (EPOC doesn't
		//     write the HALT register).
		//   * ~93% of executed PCs sit in the ARM IRQ-dispatcher stub at
		//     0x50004980..0x500049a4 — the CPU is endlessly servicing
		//     TC2/TINT micro-interrupts without making CF progress.
		//   * Sector drains are ~74 M cycles apart (2.01 s) regardless
		//     of how fast we force TC2/TINT/RTC to advance (see mode 9
		//     iterations 1 & 2 — both failed to shorten the sim-time
		//     gap), so the watchdog timer lives in guest memory and
		//     decrements per CPU instruction, not per timer tick.
		// Conclusion: we cannot shrink the *sim-time* 2 s without
		// patching guest RAM. But the user only cares about wall-clock
		// latency — so treat the 93%-idle IRQ-dispatcher window as
		// halted, letting the existing halt fast-forward path skip
		// through to the next event. Sim-time per sector stays 2 s; the
		// browser just runs those 2 s in microseconds of wall time.
		// Mode 10 (idle-fast-forward) is abandoned: PC histogram during the
		// 2 s gap shows the CPU spends >99 % of cycles in kernel IRQ-handler
		// code (pc=0x5000b458..0x50011a10) with CPSR.I=1, not in the user
		// idle dispatcher. Halting mid-handler freezes the CPU. The 2 s
		// constant appears to be an instruction-count-driven watchdog in
		// guest RAM that can only be shortened by ROM patch or by
		// wall-clock bursting at the WASM/browser layer (see
		// cfGapActive() below, wired up in wasm/main.cpp).

		// Step down the CF card's post-assertion IRQ delay. Takes one
		// emulator instruction's worth of cycles per iteration; the delay
		// value set by the card (see kIrqAssertDelay in vcfcard.cpp) is
		// conservative so a small single-cycle step is fine.
		cfCard.tickIrqDelay(1);

		// Feed the CF card's live IREQ# line into ETNA's PcCdIntStatus bit 0.
		// When EPOC's pccd_ata driver writes PcCdIntMask=0x01 around each
		// ATA command, the bit reflects the card's data-ready edges.
		bool cfIrqNow = cfCard.irqAsserted();
		bool cfIrqPrevLatched = cfIrqPrev;
		if (cfIrqNow != cfIrqPrev) {
			if (cfIrqNow) cfStatIrqAssertions++;
			else          cfStatIrqDeassertions++;
			if (cfIrqTransitionLogs < 40) {
				log("CF IREQ# %s: mask=%04x pc=%08x lr=%08x",
				    cfIrqNow ? "ASSERT" : "deassert",
				    interruptMask, getGPR(15) - 4, getGPR(14));
				cfIrqTransitionLogs++;
			}
			cfIrqPrev = cfIrqNow;
		}
		etna.setCfIreq(cfIrqNow);
		// ETNA's INT output is wired to Windermere EINT3 on the 5mx.
		// EINT3 is also the touch digitiser pen-down line (WindEmu's
		// original mapping, inherited from Series 5): when the pen
		// touches the screen the kernel's EINT3 ISR reads 0xD0D3 /
		// 0x9093 via SSDR to sample coords. Both sources share the
		// line on real hardware, so OR them here — the ETNA aggregate
		// (CF IRQ#) and the pen-down state from updateTouchInput —
		// otherwise this tick-loop gate silently clobbers the
		// updateTouchInput-set bit before the IRQ dispatch below ever
		// sees it, and taps never register.
		//
		// Drive EINT3 from the ETNA aggregate: mediaChangePending()
		// returns (pcCdIntStatus & pcCdIntMask) != 0, giving us
		// edge-latch + mask-gate in one step. Raw CF IREQ# mirrored
		// directly would bypass ETNA's mask (PcCdIntMask=0 by default)
		// and fire spuriously before the driver is ready.
		if (etna.mediaChangePending() || penDown)
			pendingInterrupts |= (1u << EINT3);
		else
			pendingInterrupts &= ~(1u << EINT3);

		// Deliver a deferred pen-up (see the tap latch in
		// updateTouchInput): once the pen ISR has taken enough samples
		// of the latched tap for its debounce to register it — or the
		// safety deadline passes — drop the pen so the driver sees the
		// release and completes the tap.
		if (penUpLatched &&
		    (penSamplesSinceDown >= kPenLatchMinSamples ||
		     passedCycles >= penLatchDeadline)) {
			penUpLatched = false;
			penDown = false;
			log("TOUCH latched release delivered (samples=%u)",
			    penSamplesSinceDown);
		}

		// Runtime-selectable CF-IRQ re-enable strategy. Off by default;
		// --cf-reenable-mode N on the harness opts in to one of the
		// candidate fixes for the "2s per sector" problem. See the enum
		// doc in windermere.h for what each mode does. Pass the pre-
		// update cfIrqPrev value so edge-sensitive modes (2/3/6/10) can
		// see the transition on the same tick it happens.
		runCfReenableStrategyEdge(cfIrqPrevLatched, cfIrqNow, 1);

		// CF reschedule-flag poke workaround. Off by default;
		// --cf-reschedule-poke on the harness enables it. Compensates
		// for our emulator's TDfc::Add()-from-IRQ failure by force-
		// setting the kernel reschedule flag when we spot the stuck
		// idle-loop state. See maybePokeReschedule() for the full
		// condition set and rate-limiting.
		maybePokeReschedule();

		// CF direct-invoke: synchronously call the PCCARD-ATA retry
		// callback at 0x50083178 via callRomFunctionSync. Bypasses the
		// 64 Hz NTimer queue grain left as a ceiling by the r1-rewrite
		// accelerator. Only fires when we're in the CF gap state (card
		// inserted + IRQ asserted + EINT3 masked) AND we've captured
		// the driver `this` pointer. On success (the callback drains a
		// sector) we poll again after a short interval; on no-progress
		// we back off aggressively so we don't burn wall-time invoking
		// a callback that has no work. callRomFunctionSync switches to
		// Supervisor32 with interrupts disabled for the duration — the
		// real callback runs in UND32 null-thread context; if SVC mode
		// breaks kernel primitives called from within, this hook
		// regresses (in which case disable via setCfDirectInvoke).
		// Mode gate: only hijack the CPU for the synchronous callback when
		// it is in the UND32 null-thread (EPOC R5's idle context — where
		// the real timer-driven callback would run anyway) or USR mode.
		// Invoking from IRQ32 means pre-empting a live ISR mid-flight;
		// on the 5mx Pro's bootloader→OS boot (card present from t≈0, the
		// OS re-mounts the boot card during early boot) those IRQ-mode
		// invokes wedged the kernel right at mount completion and the
		// desktop never came up. UND32/USR invokes carry all the
		// throughput (>85% of hits in both scenarios measured).
		uint32_t cpuMode = getCPSR() & 0x1F;
		if (cfDirectInvoke && cfDirectInvokeThis && cfCard.inserted() &&
		    cfCard.irqAsserted() &&
		    (interruptMask & (1u << EINT3)) == 0 &&
		    (cpuMode == 0x1B || cpuMode == 0x10) &&
		    passedCycles >= cfDirectInvokeLastCyc + kCfDirectInvokeInterval) {
			uint32_t drainsBefore = cfCard.sectorBoundaryCount;
			uint32_t ret = callRomFunctionSync(cfRomCallbackAddr, cfDirectInvokeThis);
			uint32_t drainsAfter = cfCard.sectorBoundaryCount;
			cfDirectInvokeHits++;
			bool progressed = drainsAfter != drainsBefore;
			// Progress: poll again in ~270 us. Stall: back off 10 ms —
			// long enough that the natural kernel timer (accelerator
			// rewrites it to 15.6 ms) can rescue us without us racing
			// it constantly.
			cfDirectInvokeLastCyc = passedCycles +
				(progressed ? 0 : (int64_t)CLOCK_SPEED / 100);
			if (cfDirectInvokeHits <= 4 || progressed ||
			    (cfDirectInvokeHits & 0xFFF) == 0)
				log("CF direct-invoke #%u: ret=%08x drains %u->%u mode=%02x%s",
				    cfDirectInvokeHits, ret, drainsBefore, drainsAfter,
				    cpuMode, progressed ? "" : " [stall]");
		}
		// CF IREQ# routing into Windermere's interrupt fabric —
		// findings from --cf-irq-line sweep (all 16 bits, 25 s
		// post-attach, 5mx v1.05.260 ROM). Steady-state INTENS at CF
		// attach time = 0x28ae = bits 1, 2, 3, 5, 7, 11, 13.
		// Behaviour on the in-mask bits:
		//   - bit 2 (WEINT) and bit 5 (EINT1): FIQ/IRQ delivers, but into
		//     the wrong handler — ~390 MMU section reloads, i.e. kernel
		//     panic/recovery path.
		//   - bit 1 (BLINT), bit 11 (TINT): delivery blocks tick/battery
		//     paths so EPOC never reaches the pcCdIntMask=01 phase.
		//   - bit 3 (MCINT): no change — our own MCINT override (below)
		//     clears the bit every tick, so routing is masked out.
		//   - bit 7 (EINT3): **this IS the CF IREQ# line**. The first
		//     dispatch after attach fires cleanly into the pccd_ata
		//     handler at pc=5005531c (not touch, as we previously
		//     assumed from WindEmu). INTENS sets bit 7 from the PCCD
		//     driver at lr=50089674, and the first CF-command
		//     completion dispatches EINT3 into pc=5005531c. The kernel
		//     dispatcher (lr=50011a04) then clears bit 7 as part of
		//     its normal "disable, call handler, re-enable" flow, but
		//     unlike the TC2 path (which re-enables from lr=50007580)
		//     bit 7 is **never re-enabled** — presumably because the
		//     ISR binding hands off to a DFC that never runs, or
		//     expects a binding ack we don't provide. Brute-forcing
		//     the mask to stay enabled after dispatch causes an IRQ
		//     storm / ISR re-entry and hangs the CPU before even
		//     cmd=0x20 is issued. So multi-sector READ reverts to the
		//     2 s watchdog-DFC path; only IDENTIFY benefits.
		//   - bit 13 (UART2): same symptom as bit 1 — the handler blocks
		//     EPOC before it enables pcCdIntMask.
		//   - bit 9 (TC2OI): delivery starves the timer, system hangs.
		//   - all bits outside 0x28ae: no effect (IRQ never dispatched
		//     since interruptMask gates it).
		// Separately, driving MCINT from mediaChangePending() (i.e. using
		// the natural ETNA→Windermere aggregate line) DOES dispatch a
		// FIQ — but the handler re-issues an IDENTIFY (cmd=0xEC) on
		// every invocation, i.e. EPOC treats MCINT as card-status not
		// CF-IREQ. Sectors drain in short 100 ms bursts between 12 s
		// stalls — worse overall than the polling path.
		// Net: CF IREQ# does NOT aggregate onto any stock-enabled
		// Windermere interrupt line in a way the pccd_ata driver listens
		// to. Either (a) the BSP enables an additional INTENS bit we miss
		// on boot, or (b) the CF IRQ path on 5mx is edge-driven via a
		// different mechanism (e.g. ETNA drives a GPIO that the kernel
		// samples). For now the driver continues on its ~2 s polling path.
		// MCINT edge: raised by attachCard() / detachCard() on the door-switch
		// transition real hardware would see. Held asserted until MCEOI acks.
		// We deliberately do NOT feed ETNA's (pcCdIntStatus & pcCdIntMask)
		// aggregate onto MCINT on 5mx, even with ETNA's edge-latching in
		// place: empirically EPOC's MCINT handler still treats every
		// delivered FIQ as a card-state-change and re-issues IDENTIFY. A
		// harness trace with the aggregate enabled shows 29 IDENTIFY
		// commands vs 3 READ commands over 15 s post-attach, while the
		// sector-drain counter appears to advance only because the IDENTIFY
		// 512-byte buffer drains through vcfcard's READ state machine.
		// The ETNA latch still matters in isolation (correct register
		// semantics for pcCdIntStatus) — the live-level path is simply
		// unused. CF IREQ# likely reaches pccd_ata via a different SoC
		// path not yet identified; the schematic confirms there's no
		// discrete ETNA chip (all PC-Card nets go direct to Windermere)
		// and wind_defs.h labels bit 4 (CSINT) as IrqCodec, not a PC-Card
		// line, so the remaining routing is internal Windermere silicon.
		//
		// What the 2 s gap actually is (per-cause FIQ/IRQ counter dump,
		// 500 ms cadence): TINT fires steadily at 64 Hz throughout, with
		// halted=0 — the CPU is alive running the Symbian scheduler tick
		// and EPOC's system-tick work. TC2, however, is 5mx's nanokernel
		// tick source in one-shot-reprogrammed-in-ISR mode (LOAD=1536 @
		// 512 kHz = 3 ms between fires). TC2 runs in short bursts (~10
		// fires) then goes dormant when NTimerQ has no pending entries,
		// and is NOT re-armed by iBusyTimeout.OneShot(5ms) the way real
		// hardware must — the ROM's __MSTIM_MACHINE_CODED__ Add hook
		// likely pokes TC2 synchronously via a path our register-level
		// Timer emulation doesn't observe. So the 5 ms poll never fires
		// at 5 ms; it fires 2 s later when PCCARD-ARM's socket-watchdog
		// DFC (pc=50085cbc, period ~2 s) re-primes the NTimerQ side of
		// things while polling CF status. One sector drains per watchdog
		// cycle, giving the exact 2.00 s / sector cadence we observe.
		// The per-cause-INT dump (halted + fiq_total + irq_total per
		// bit, 500 ms interval) below makes this diagnosable without
		// re-instrumenting; keep it unless log volume matters.
		// MCINT on 5mx is the "media change" line — door-switch state.
		// Driving it from ETNA's mediaChangePending() makes the MCINT
		// handler re-issue IDENTIFY on every CF IREQ# because that
		// handler treats the FIQ as a card-state-change event
		// (PcCdIntMask=0, SktCtrl re-init). CF IREQ# on 5mx goes via a
		// different path (EINT3 in IRQ_INTERRUPTS); see below.
		bool mcintNow = mcintEdgePending;
		if (mcintNow != mcintPrev) {
			// Log every MCINT assert/deassert so we can see whether the FIQ
			// is truly re-firing or just being retriggered by a higher-level
			// driver retry. Capped so a genuine refire doesn't fill the log.
			if (mcintTransitionLogs < 40) {
				log("MCINT %s: mask=%04x pendingFIQ=%04x pc=%08x lr=%08x",
				    mcintNow ? "ASSERT" : "deassert",
				    interruptMask, pendingInterrupts & FIQ_INTERRUPTS,
				    getGPR(15) - 4, getGPR(14));
				mcintTransitionLogs++;
			}
			mcintPrev = mcintNow;
		}
		if (mcintNow)
			pendingInterrupts |= (1<<MCINT);
		else
			pendingInterrupts &= ~(1<<MCINT);

		// (Removed: the 5mxPro REC "stuck button" idle-park recovery gate.
		// It un-parked the CPU after the idle-thread sentinel redirect left
		// CPSR stranded in IRQ-mode+I-set. Both were workarounds for the
		// SYMPTOM of the recording bug; with the root cause fixed
		// (edge-driven CSINT for recording) the idle thread never parks on
		// the sentinel, so neither is needed.)

		if ((pendingInterrupts & interruptMask & FIQ_INTERRUPTS) != 0 && canAcceptFIQ()) {
			uint16_t cause = pendingInterrupts & interruptMask & FIQ_INTERRUPTS;
			if (fiqDeliveryLogs < 20) {
				log("FIQ delivery #%d: cause=%04x pc=%08x lr=%08x",
				    fiqDeliveryCount, cause, getGPR(15) - 4, getGPR(14));
				fiqDeliveryLogs++;
			}
			for (int b = 0; b < 16; b++)
				if (cause & (1u << b))
					fiqCauseCount[b]++;
			fiqDeliveryCount++;
			requestFIQ();
			halted = false;
		}
		if ((pendingInterrupts & interruptMask & IRQ_INTERRUPTS) != 0 && canAcceptIRQ()) {
			uint16_t cause = pendingInterrupts & interruptMask & IRQ_INTERRUPTS;
			// Mode 8 dispatch gate: defer EINT3 (CF IRQ) unless CPU is in
			// User32. Prevents the hang observed when a CF IRQ fires inside
			// the kernel's interrupt-controller code (pc=5000b568 region).
			if (cfReenableMode == 8 && (cause & (1u << EINT3))) {
				uint32_t cpuMode = getCPSR() & 0x1F;
				if (cpuMode != 0x10) cause &= ~(1u << EINT3);
			}
			// Mode 10 dispatch gate: turn the dispatch into a one-shot.
			// Clear mask bit 7 the moment we dispatch EINT3 so a second
			// IRQ can't fire on top of the first one while the card is
			// still asserting IREQ#. Side-effect: FUN_500072cc's inner
			// loop reads INTSR (pending & mask) and wouldn't dispatch
			// EINT3 with the bit cleared, but the outer IRQ dispatch
			// here has already captured the cause into 'cause' and
			// requested IRQ, so the handler still gets invoked for
			// this one tick. Prevents re-entry hangs.
			if (cfReenableMode == 10 && (cause & (1u << EINT3))) {
				interruptMask &= ~(1u << EINT3);
			}
			if (cause != 0) {
				for (int b = 0; b < 16; b++)
					if (cause & (1u << b))
						irqCauseCount[b]++;
				irqDeliveryCount++;
				// Log IRQ dispatches that include EINT3 so we can see which
				// handler the ROM jumps to (touch ISR vs PCMCIA/CF ISR). The
				// INTENS trace shows EINT3 gets enabled from pc=50089674 at
				// boot, before any touch — suggesting the PCMCIA driver is
				// binding EINT3 and WindEmu's "EINT3 = touch" mapping we
				// inherited may be wrong.
				if (cause & (1u << EINT3)) {
					cfStatEint3Dispatches++;
					if (eint3DispatchLogs < 40) {
						log("IRQ dispatch #%u cause=%04x pc=%08x lr=%08x (EINT3 set, cf_ireq=%d)",
						    irqDeliveryCount, cause, getGPR(15) - 4, getGPR(14),
						    (int)cfIrqNow);
						eint3DispatchLogs++;
					}
					// If PSION_CF_DIAG env var is set, enable ARM core deep
					// tracing for a short window around the FIRST CF dispatch.
					if (cfDiagEnabled && cfStatEint3Dispatches == 1 && !getCfDiagEnabled()) {
						log("=== DIAG window OPEN for CF ISR (first EINT3 dispatch) ===");
						// Dump kernel-data snapshot so post-hoc analysis can see
						// the pre-IRQ state of the IDFC-queue/flag globals.
						for (uint32_t va = 0x80000010; va < 0x80000030; va += 4) {
							auto v = readVirtualDebug(va, V32).value_or(0xDEADBEEF);
							log("DIAG snap [%08x] = %08x", va, v);
						}
						for (uint32_t va = 0x8000081c; va <= 0x80000824; va += 4) {
							auto v = readVirtualDebug(va, V32).value_or(0xDEADBEEF);
							log("DIAG snap [%08x] = %08x", va, v);
						}
						for (uint32_t va = 0x80000870; va <= 0x80000884; va += 4) {
							auto v = readVirtualDebug(va, V32).value_or(0xDEADBEEF);
							log("DIAG snap [%08x] = %08x", va, v);
						}
						// Wide scan: look for DFC-queue-like structures across
						// the entire kernel data page. A TDfcQue in EKA1 looks
						// like: (TDblQueLink)(iQue.iNext, iQue.iPrev pointing
						// to itself when empty, or to a queued TDfc when non-
						// empty) followed by optional priority-list data and
						// iThread pointer. We look for entries where a word
						// points back to itself (self-pointing head) — typical
						// empty queue head — and log their neighbouring words.
							setCfDiagEnabled(true);
						// ~20k instructions is plenty to see the full ISR body
						// and the return path; each op executed while enabled
						// costs one log line, so keep this bounded.
						cfDiagCyclesRemaining = 20000;
					}
				}
				requestIRQ();
				halted = false;
			}
		}

		// what's running?
		if (halted) {
			// keep the clock moving
			// when does the next earliest thing happen?
			// this stops us from spinning needlessly
			int64_t nextEvent = nextTickAt;
			if (tc1.nextTickAt < nextEvent) nextEvent = tc1.nextTickAt;
			if (tc2.nextTickAt < nextEvent) nextEvent = tc2.nextTickAt;
			if (cycles < nextEvent) nextEvent = cycles;
			passedCycles = nextEvent;
		} else {
			if (auto v = virtToPhys(getGPR(15) - 0xC); v.has_value() && instructionReady())
				debugPC(v.value());
			// CF 2s-timer accelerator. The PCCARD-ATA socket callback
			// at ROM 0x50083178 arms a 2,000,000 us (2 s) one-shot
			// timer on itself via the kernel NTimer-import stub at
			// 0x500842fc (BL-site 0x500831fc). That timer is the
			// safety net for when the card IRQ / DFC wake fails to
			// deliver. On real hardware the IRQ rescues the transfer
			// within microseconds and the 2 s timeout is never
			// reached; in our emulator the IRQ->DFC wake path has a
			// fidelity gap so every sector waits out the full 2 s.
			// Rewrite r1 (microseconds) from 2,000,000 to
			// kCfAccelTimerUs right before the BL executes. Guarded by
			// r1 == 2,000,000 AND r2 == 0x50083178 so other timer
			// arms are untouched. See EMULATOR_FIDELITY_ANALYSIS.md
			// for the call-chain investigation.
			// Root-cause note: instrumentation at IRQ-exit postamble
			// (0x50004b10 decision tree — gates on SPSR mode, kcs,
			// iRescheduleNeededFlag, then BL Reschedule at 0x50004b68)
			// confirmed that during CF IRQ dispatch, RSF is ALWAYS 0
			// when the IRQ exits. The full Reschedule branch
			// (0x50004b68 -> 0x50005214) never runs for CF IRQs. This
			// means TDfc::Add from CardIntCallBack is failing to
			// signal the DFC-thread semaphore (which would Ready() the
			// CF DFC thread and set RSF=1). The workaround stack
			// (accel-timer + direct-invoke) compensates without fixing
			// the root cause. See EMULATOR_FIDELITY_ANALYSIS.md for
			// the full trace. The instrumentation was removed to keep
			// release logs quiet — re-add locally if re-investigating.

			if (cfAccelTimer && cfCard.inserted() && cfRomTrampolinePC != 0) {
				uint32_t pc = getRealPC();
				// The shared import trampoline is where all 4 retry-
				// timer arm-sites route through. Intercept there and
				// rewrite r1 (us timeout) from 2,000,000 to
				// kCfAccelTimerUs. Guard tightly on r1==2,000,000 AND
				// r2==<callback> so unrelated kernel timer work is
				// untouched. PC values are resolved per-ROM-variant
				// at loadROM time so this works on 5mx / MC218 /
				// 5mx Pro without per-device branching here.
				if (pc == cfRomTrampolinePC &&
				    getGPR(1) == 0x001e8480 &&
				    getGPR(2) == cfRomCallbackAddr) {
					setGPR(1, kCfAccelTimerUs);
					cfAccelTimerHits++;
					if (cfAccelTimerHits <= 8 || (cfAccelTimerHits & 0xFF) == 0)
						log("CF accel-timer: r1 2000000->%u at trampoline %08x, lr=%08x (hit #%u)",
						    kCfAccelTimerUs, cfRomTrampolinePC, getGPR(14), cfAccelTimerHits);
				}
				// Capture `this` for the direct-invoke path: first time
				// the natural retry-callback runs, r0 holds the socket
				// object we can re-supply to re-invoke it.
				if (cfDirectInvokeThis == 0 && pc == cfRomCallbackAddr) {
					uint32_t r0 = getGPR(0);
					if ((r0 & 0xFFF00000) == 0x80300000) {
						cfDirectInvokeThis = r0;
						log("CF direct-invoke: captured callback this=%08x", r0);
					}
				}
			}
			// Live-driver `this` capture: CardIntCallBack(TAny* aMediaDriver)
			// at ROM 0x500890dc receives `this` in r0 by ARM calling
			// convention. Capture it and also dump the first 0x100 bytes
			// so we can identify which layer's driver object this is.
			if (cfDfcLiveDriverPtr == 0 && cfCard.inserted()) {
				uint32_t pc = getRealPC();
				if (pc == 0x500890dc) {
					uint32_t r0 = getGPR(0);
					if ((r0 & 0xFFF00000) == 0x80300000) {
						cfDfcLiveDriverPtr = r0;
						log("CF live-driver captured: this=%08x (at CardIntCallBack entry)", r0);
						// Dump first 0x100 bytes — look for vtable at +0,
						// NTimer iBusyTimeout fields, TDfc pointers, etc.
						for (uint32_t off = 0; off < 0x100; off += 0x10) {
							auto w0 = readVirtualDebug(r0 + off +  0, V32).value_or(0xDEADBEEF);
							auto w1 = readVirtualDebug(r0 + off +  4, V32).value_or(0xDEADBEEF);
							auto w2 = readVirtualDebug(r0 + off +  8, V32).value_or(0xDEADBEEF);
							auto w3 = readVirtualDebug(r0 + off + 12, V32).value_or(0xDEADBEEF);
							log("  +%02x: %08x %08x %08x %08x", off, w0, w1, w2, w3);
						}
					}
				}
			}
			passedCycles += tick();

			// Wind down the deep-diag window. Each tick ticks; once the budget
			// runs out we flip the ARM-core trace off so the log stops
			// drowning. Also triggers a closing marker so the trace region is
			// trivially grep-able.
			if (getCfDiagEnabled()) {
				cfDiagCyclesRemaining--;
				if (cfDiagCyclesRemaining <= 0) {
					setCfDiagEnabled(false);
					log("=== DIAG window CLOSE ===");
				}
			}

#ifndef __EMSCRIPTEN__
			uint32_t new_pc = getGPR(15) - 0xC;
			if (_breakpoints.find(new_pc) != _breakpoints.end()) {
				log("⚠️ Breakpoint triggered at %08x!", new_pc);
				return;
			}
#endif
		}
	}
}

// Runtime-selectable strategy for re-enabling the EINT3 mask bit after the
// 5mx kernel dispatcher clears it. Called once per emulator iteration from
// executeUntil(). Each mode is a candidate fix for the 2s-per-sector
// problem; the harness picks one via --cf-reenable-mode N and scores the
// resulting sector-drain throughput. See windermere.h for the enum doc.
void Emulator::runCfReenableStrategyEdge(bool cfIrqPrevLatched, bool cfIrqNow, int /*passedCyclesStep*/) {
	if (cfReenableMode == 0) return;
	bool maskBit7Off = (interruptMask & (1u << EINT3)) == 0;

	// Mode 2: deassert-user — re-enable on 1->0 edge if CPU in User32.
	// Mode 3: deassert-nonirq — re-enable on 1->0 edge if CPU NOT in
	//         FIQ/IRQ/Abort/Undefined mode (User or Supervisor only).
	if ((cfReenableMode == 2 || cfReenableMode == 3) &&
	    cfIrqPrevLatched && !cfIrqNow && maskBit7Off) {
		uint32_t mode = getCPSR() & 0x1F;
		bool allow = false;
		if (cfReenableMode == 2) {
			allow = (mode == 0x10); // User32
		} else {
			allow = (mode == 0x10 || mode == 0x13); // User32 | Supervisor32
		}
		if (allow) {
			interruptMask |= (1u << EINT3);
		}
	}

	// Mode 4: intclear — re-enable on the tick after a bit-0 IntClear write
	// (deterministic ack point; pc=50085cbc,lr=500072c0 in observed logs).
	if (cfReenableMode == 4) {
		uint32_t curr = etna.intClearBit0Count;
		if (curr != prevIntClearCount) {
			if (maskBit7Off) interruptMask |= (1u << EINT3);
			prevIntClearCount = curr;
		}
	}

	// Mode 5: level — handled at the INTENC write site; nothing to do here.

	// Mode 6: delayed — re-enable N cycles after the 1->0 edge. Gives the
	// kernel dispatcher time to unwind its stack frame + restore CPSR
	// before we re-arm the cause.
	if (cfReenableMode == 6) {
		if (cfIrqPrevLatched && !cfIrqNow) {
			cfReenableDelayCyc = 4000; // ~108us at 36.864 MHz
		}
		if (cfReenableDelayCyc > 0) {
			if (cfReenableDelayCyc > 1) cfReenableDelayCyc--;
			else {
				cfReenableDelayCyc = 0;
				if (maskBit7Off) interruptMask |= (1u << EINT3);
			}
		}
	}

	// Mode 7: user-reenable — re-enable bit 7 every iteration when the CPU
	// is in User32 mode. The sweep's mode 4 showed a second CF IRQ always
	// dispatches while the CPU is in kernel code (pc=5000b568) and hangs.
	// Gating re-enable on User32 guarantees the next dispatch only fires
	// from a safe context, because mask-bit-7 is off in every other mode.
	if (cfReenableMode == 7 && cfLevelArmed && maskBit7Off) {
		if ((getCPSR() & 0x1F) == 0x10) { // User32
			interruptMask |= (1u << EINT3);
		}
	}

	// Mode 8 is implemented as a dispatch-time filter (see the IRQ-dispatch
	// site in executeUntil) rather than here. It leaves mask-bit-7 latched
	// via the same "level" treatment as mode 5, then defers the actual
	// dispatch until the CPU returns to User32.

	// Mode 10: cfDfcCleanupSeen is armed at the writePhysical site when
	// pc=0x5000f1b8 clears the busy-add lock (see comment there). We
	// don't re-enable directly there — that fires every TC2 cycle while
	// the CF state machine is still actively progressing, and re-
	// enabling mid-sequence causes re-entry hangs. Instead we only
	// re-enable once the CPU has been parked in the null-thread idle
	// loop for >= cfDfcIdleThresholdCyc continuous cycles, which
	// indicates the DFC state machine has genuinely quiesced (waiting
	// for the 2 s watchdog). At that point re-enabling bit 7 is safe
	// and lets the card's asserted IREQ# dispatch immediately instead
	// of waiting out the watchdog.
	if (cfReenableMode == 10 && cfDfcCleanupSeen) {
		uint32_t cpsrMode = getCPSR() & 0x1F;
		uint32_t pc = getGPR(15) - 0xC;
		bool pcInIdle = (pc >= 0x5000b550 && pc <= 0x5000b57f);
		bool inNullThread = (cpsrMode == 0x1B); // Undefined32
		if (pcInIdle && inNullThread) {
			if (cfIdleSinceCyc < 0) cfIdleSinceCyc = passedCycles;
			int64_t idleFor = passedCycles - cfIdleSinceCyc;
			// ~5 ms continuous idle before we trust the state machine
			// has stopped progressing on its own.
			const int64_t kIdleThresholdCyc = (int64_t)CLOCK_SPEED / 200;
			if (cfIrqNow && maskBit7Off && idleFor >= kIdleThresholdCyc) {
				interruptMask |= (1u << EINT3);
				cfDfcCleanupSeen = false;
				cfIdleSinceCyc = -1;
				cfMode10ReenableCount++;
				if (cfMode10ReenableCount <= 8) {
					log("CF mode10: re-enabled EINT3 (idle for %lld cyc), pc=%08x mask=%04x",
					    (long long)idleFor, getGPR(15) - 0xC, interruptMask);
				}
			}
		} else {
			// Any non-idle execution resets the idle counter.
			cfIdleSinceCyc = -1;
		}
	}

	// Mode 12 (cf-dfc-direct-call): implemented in maybeCallCfDfc(),
	// called from maybePokeReschedule() on stuck-state detection.
	(void)cfMode12ReenableCount;
	(void)cfMode12NextAllowedCyc;

	// Mode 11 was an attempt to call the ROM's CF DFC processor
	// FUN_5000f124 directly from emulator C++ (via callRomFunctionSync).
	// It fires, changes the timer queue head (first call observed
	// 80309f28 -> 800002ac), but doesn't move sector_drains — the
	// downstream DFC wake is still broken. ROM analysis also shows
	// 0x5000f124 has ZERO static BL callers in the image, meaning either
	// Ghidra misidentified it as a function entry or it's only reached via
	// an indirect/dynamic dispatch we haven't mapped. Left disabled; the
	// callRomFunctionSync primitive stays in ARM710 as infrastructure for
	// a future attempt that finds the correct entry point (e.g. the
	// TimerDfcFunction body, whose address we don't yet know).
	(void)cfMode11CallCount; (void)cfMode11NextAllowedCyc;
}

// Workaround: force-set the EKA1 kernel's reschedule-needed flag at virtual
// 0x80000878 (physical 0xd07e5878) when we detect the "CF stuck in idle"
// state. That state is:
//   * card present and its IREQ# line is asserted (data ready);
//   * Windermere EINT3 mask bit is off (kernel disabled it after first
//     dispatch and never re-enabled — the symptom of TDfc::Add() silently
//     failing from the CF ISR);
//   * CPU is in Undefined32 mode (CPSR.mode == 0x1B) — EKA1's null thread
//     / idle loop;
//   * PC is spinning in the 0x5000b550..0x5000b57f range (the idle loop
//     body).
// We also require the stuck state to be stable for >= ~5 ms and we
// rate-limit to at most one poke every ~10 ms to avoid clobbering the
// scheduler once it's making progress. After we write the flag, the next
// IRQ-handler exit (at 0x50004980) observes flag=1 and jumps to the full
// rescheduler at FUN_50005214, which runs the CF DFC thread and drains a
// sector. The kernel clears the flag on its own.
//
// The virtual->physical mapping is a standard MMU section entry
// (v:80000000 -> p:d07e5000) installed very early in boot; writeVirtual
// routes through the normal MMU path so it both validates the mapping
// is present and keeps the TLB coherent.
void Emulator::maybePokeReschedule() {
	// cfReschedulePokeEnabled gates the reschedule-flag poke, but we
	// reuse the stuck-state detection logic to trigger mode-12 direct
	// DFC calls independently.
	if (!cfReschedulePokeEnabled && cfReenableMode != 12) return;
	if (!cfCard.inserted()) {
		cfStuckSinceCycles = -1;
		return;
	}
	// Cool down first — we may poke again once the window elapses.
	if (passedCycles < cfNextPokeAllowedAt) return;

	const bool cfIrqNow = cfCard.irqAsserted();
	const bool maskOff = (interruptMask & (1u << EINT3)) == 0;
	const uint32_t cpsrMode = getCPSR() & 0x1F;
	const bool idleMode = (cpsrMode == 0x1B); // Undefined32 (EKA1 null thread)
	const uint32_t pc = getGPR(15) - 0xC;
	const bool pcInIdle = (pc >= 0x5000b550 && pc <= 0x5000b57f);

	const bool stuck = cfIrqNow && maskOff && idleMode && pcInIdle;

	// Additional guard: require no drain progress for >= 30 ms. The natural
	// polling path manages ~1 drain / 2 s; we don't want to spam pokes while
	// that path is still making forward progress on the current sector.
	uint32_t drains = cfCard.sectorBoundaryCount;
	if (drains != cfLastDrainCountAtCheck) {
		cfLastDrainCountAtCheck = drains;
		cfLastDrainProgressCycles = passedCycles;
	}
	const int64_t sinceProgressCyc = passedCycles - cfLastDrainProgressCycles;
	const int64_t kMinStuckCyc = (int64_t)CLOCK_SPEED / 200;       // ~5 ms
	const int64_t kNoProgressCyc = (int64_t)CLOCK_SPEED / 32;      // ~31 ms

	if (!stuck) {
		cfStuckSinceCycles = -1;
		return;
	}
	if (cfStuckSinceCycles < 0)
		cfStuckSinceCycles = passedCycles;

	const int64_t stuckFor = passedCycles - cfStuckSinceCycles;
	if (stuckFor < kMinStuckCyc) return;
	if (sinceProgressCyc < kNoProgressCyc) return;

	bool didSomething = false;
	if (cfReschedulePokeEnabled) {
		// Write 1 to the kernel's reschedule-needed flag via the MMU path.
		uint32_t before = readVirtualDebug(0x80000878, V32).value_or(0xDEADBEEF);
		MMUFault f = writeVirtual(1, 0x80000878, V32);
		uint32_t after = readVirtualDebug(0x80000878, V32).value_or(0xDEADBEEF);
		if (f == NoFault) {
			cfStatReschedulePokes++;
			if (cfStatReschedulePokes <= 8) {
				log("CF reschedule-poke #%u: *0x80000878 %08x->%08x "
				    "(pc=%08x cpsr_mode=%02x mask=%04x stuck_for=%lld cyc)",
				    cfStatReschedulePokes, before, after, pc, cpsrMode,
				    interruptMask, (long long)stuckFor);
			}
			didSomething = true;
		} else {
			// Mapping not present yet — push the re-check out a little.
			cfStuckSinceCycles = -1;
			cfNextPokeAllowedAt = passedCycles + (int64_t)CLOCK_SPEED / 1000; // ~1 ms
			return;
		}
	}
	if (cfReenableMode == 12) {
		// Mode 12: synchronously invoke CardIreqDfcFunction, bypassing
		// the broken TDfc scheduler wake path.
		maybeCallCfDfc();
		didSomething = true;
	}
	if (!didSomething) return;
	// Re-arm: require another fresh 5 ms of "stuck" before next poke,
	// and impose a 10 ms hard cooldown so we can't spam writes even if
	// the state persists because the scheduler ignored us.
	cfStuckSinceCycles = -1;
	cfNextPokeAllowedAt = passedCycles + (int64_t)CLOCK_SPEED / 100; // ~10 ms
}

// Scan the kernel heap for the CF driver's TDfc objects. EKA1 TDfc layout
// in this ROM:
//   +0: iNext (SDblQueLink) / +4: iPrev / +8: iPriority / +C: iFunction
//   +10: iPtr / +14: iDfcQ.
// DPcCardMediaDriverAta embeds iCardIreqDfc at member offset 0x30 and
// iTimerDfc at +0x44 (derived from ReadSectorsCommand-like dispatchers
// that call TDfc::Add with driver+0x30 / driver+0x44).
void Emulator::scanForCfDfcs() {
	constexpr uint32_t kPcCardDriverCodeLo = 0x50088000;
	constexpr uint32_t kPcCardDriverCodeHi = 0x5008a000;

	uint32_t foundVa[64] = {0};
	uint32_t foundFn[64] = {0};
	uint32_t foundPtr[64] = {0};
	int foundN = 0;

	for (uint32_t va = 0x80300000; va < 0x80320000 && foundN < 64; va += 4) {
		auto wC = readVirtualDebug(va + 0xC, V32);
		auto w10 = readVirtualDebug(va + 0x10, V32);
		if (!wC.has_value() || !w10.has_value()) continue;
		uint32_t fn = wC.value();
		uint32_t ptr = w10.value();
		if (fn < kPcCardDriverCodeLo || fn >= kPcCardDriverCodeHi) continue;
		if ((ptr & 0xFFF00000) != 0x80300000) continue;
		foundVa[foundN] = va;
		foundFn[foundN] = fn;
		foundPtr[foundN] = ptr;
		foundN++;
	}

	uint32_t bestPtr = 0;
	int bestCount = 0;
	for (int i = 0; i < foundN; i++) {
		int c = 0;
		for (int j = 0; j < foundN; j++)
			if (foundPtr[j] == foundPtr[i]) c++;
		if (c > bestCount) { bestCount = c; bestPtr = foundPtr[i]; }
	}
	if (bestCount < 2) return;
	cfDfcDriverPtr = bestPtr;
	// Offsets observed in 5mx v1.05(260): the FIRST TDfc member in the
	// driver (at +0x30) is iTimerDfc (TimerDfcFunction, simple-body
	// fast-return path), and the SECOND (+0x44) is iCardIreqDfc
	// (CardIreqDfcFunction, state-machine over iCardStatus at driver+0xa8).
	// This contradicts the declaration order in modern Symbian headers
	// (TDfc iCardIreqDfc; TDfc iTimerDfc;), but the emitted machine code
	// for the ctor in this ROM interleaves init for iBusyTimeout (NTimer)
	// between them, leaving iTimerDfc at the earlier offset. Verified by
	// disassembly: FUN_5008914c = TimerDfcFunction (calls
	// iSocket->InterruptDisable then Add(iTimerDfc) on failure);
	// FUN_500891c0 = CardIreqDfcFunction (switches on
	// *(driver+0xa8) == iCardStatus).
	for (int i = 0; i < foundN; i++) {
		if (foundPtr[i] != bestPtr) continue;
		uint32_t off = foundVa[i] - bestPtr;
		if (off == 0x30) cfDfcTimerFn = foundFn[i];
		else if (off == 0x44) cfDfcCardIreqFn = foundFn[i];
	}
	log("CF DFC bodies located: driver=%08x CardIreqFn=%08x TimerFn=%08x (%d TDfcs scanned)",
	    cfDfcDriverPtr, cfDfcCardIreqFn, cfDfcTimerFn, foundN);
}

// Mode 12: synchronously invoke CardIreqDfcFunction(driver) via
// callRomFunctionSync. Bypasses the broken TDfc::Add scheduler wake.
void Emulator::maybeCallCfDfc() {
	// Discover the DFC function addresses (done once).
	if (cfDfcCardIreqFn == 0) {
		scanForCfDfcs();
		if (cfDfcCardIreqFn == 0) return;
	}
	// Prefer the LIVE driver `this` captured at the first CF cmd write.
	// The static scan's "most-referenced heap ptr" is an uninitialised
	// placeholder; calling CardIreqDfcFunction on it reads iCardStatus=0
	// and takes the idle branch (observed regression: 10 -> 3 drains).
	// cfDfcLiveDriverPtr is the real driver instance whose iCardStatus
	// tracks the active ATA command.
	uint32_t driver = cfDfcLiveDriverPtr ? cfDfcLiveDriverPtr : cfDfcDriverPtr;
	if (driver == 0) return;
	if (passedCycles < cfMode12DfcNextAllowedCyc) return;
	cfMode12DfcNextAllowedCyc = passedCycles + (int64_t)CLOCK_SPEED / 1000; // 1 ms

	// Try TimerDfcFunction first — it reads CF status directly and calls
	// CardIreqDfcFunction on its own if card is not busy. Bypasses
	// iCardStatus check that was causing CardIreqDfcFunction-direct to
	// take the idle branch.
	if (cfDfcTimerFn) {
		auto beforeDrain = cfCard.sectorBoundaryCount;
		uint32_t retT = callRomFunctionSync(cfDfcTimerFn, driver);
		auto afterDrain = cfCard.sectorBoundaryCount;
		if (cfMode12DfcInvocations < 4 || afterDrain != beforeDrain) {
			log("CF mode12 TimerDfc call: TimerDfcFunction(%08x)=%08x drains %u->%u",
			    driver, retT, beforeDrain, afterDrain);
		}
		cfMode12DfcInvocations++;
		cfMode12DfcNextAllowedCyc = passedCycles + (int64_t)CLOCK_SPEED / 1000;
		return;
	}
	uint32_t drainsBefore = cfCard.sectorBoundaryCount;
	// On first invocation, diagnose the current-thread context so we can
	// identify which thread is actually stuck waiting. Read:
	//   DAT_50005168 (= 0x80000964): TheScheduler.iCurrentThread
	//   The NThreadBase layout (from EKA2 nkern.h) has iNState @ +0x34
	//   (EWaitSemaphore=4, EWaitDfc=5, ...) and iPriority @+0x35.
	//   Thread name for readability: usually a TBuf8 member — walk
	//   known offsets or scan for ASCII.
	if (cfMode12DfcInvocations == 0) {
		auto curThreadPtr = readVirtualDebug(0x80000964, V32);
		uint32_t curThread = curThreadPtr.value_or(0);
		log("CF stuck-state thread diag: current-thread-ptr=%08x", curThread);
		// Scan heap for NThread instances (vtable = 0x5002600c).
		auto vtOpt = readVirtualDebug(curThread, V32);
		uint32_t nThreadVt = vtOpt.value_or(0);
		if (nThreadVt) {
			log("  NThread vtable = %08x; scanning heap for instances",
			    nThreadVt);
			// Follow iCardIreqDfc.iDfcQ -> TDfcQue -> iThread to find the
			// CF DFC thread directly. Our Mode-12 scan showed iDfcQ of
			// iCardIreqDfc at driver+0x58 (offset +0x58 of DPcCardMediaDriverAta).
			auto dfcQPtr = readVirtualDebug(driver + 0x58, V32);
			uint32_t dfcQ = dfcQPtr.value_or(0);
			log("  iCardIreqDfc.iDfcQ = %08x", dfcQ);
			if (dfcQ) {
				// Dump TDfcQue contents. In EKA2: iPriorityMap, iQueue[8],
				// iPresent, iThread. iThread probably at some offset —
				// dump the whole struct.
				for (uint32_t off = 0; off < 0x40; off += 0x10) {
					auto w0 = readVirtualDebug(dfcQ + off + 0, V32).value_or(0xDEADBEEF);
					auto w1 = readVirtualDebug(dfcQ + off + 4, V32).value_or(0xDEADBEEF);
					auto w2 = readVirtualDebug(dfcQ + off + 8, V32).value_or(0xDEADBEEF);
					auto w3 = readVirtualDebug(dfcQ + off + 12, V32).value_or(0xDEADBEEF);
					log("  dfcQ+%02x: %08x %08x %08x %08x", off, w0, w1, w2, w3);
				}
			}
			for (uint32_t va = 0x80300000; va < 0x80320000; va += 4) {
				auto w = readVirtualDebug(va, V32);
				if (!w.has_value() || w.value() != nThreadVt) continue;
				auto n4 = readVirtualDebug(va + 0x04, V32).value_or(0);
				auto n1c = readVirtualDebug(va + 0x1c, V32).value_or(0);
				auto n24 = readVirtualDebug(va + 0x24, V32).value_or(0);
				auto n2c = readVirtualDebug(va + 0x2c, V32).value_or(0);
				auto n20b = readVirtualDebug(va + 0x20, V8).value_or(0);
				auto n21b = readVirtualDebug(va + 0x21, V8).value_or(0);
				log("  thread@%08x +04=%08x +1c=%08x +24=%08x +2c=%08x  [20]=%02x [21]=%02x%s",
				    va, n4, n1c, n24, n2c, n20b, n21b,
				    va == curThread ? " <-CURRENT" : "");
			}
		}
	}
	auto statusBefore = readVirtualDebug(driver + 0xa8, V32).value_or(0xDEADBEEF);
	uint32_t retval = callRomFunctionSync(cfDfcCardIreqFn, driver);
	cfMode12DfcInvocations++;
	uint32_t drainsAfter = cfCard.sectorBoundaryCount;
	auto statusAfter = readVirtualDebug(driver + 0xa8, V32).value_or(0xDEADBEEF);
	if (cfMode12DfcInvocations <= 8 || drainsAfter != drainsBefore) {
		log("CF mode12 DFC call #%u: CardIreqDfcFunction(%08x)=%08x drains %u->%u status %08x->%08x%s",
		    cfMode12DfcInvocations, driver, retval,
		    drainsBefore, drainsAfter, statusBefore, statusAfter,
		    driver == cfDfcLiveDriverPtr ? " [live]" : " [static]");
	}
}

const char *Emulator::identifyObjectCon(uint32_t ptr) {
	if (ptr == readVirtualDebug(0x80000980, V32).value()) return "process";
	if (ptr == readVirtualDebug(0x80000984, V32).value()) return "thread";
	if (ptr == readVirtualDebug(0x80000988, V32).value()) return "chunk";
//	if (ptr == readVirtualDebug(0x8000098C, V32).value()) return "semaphore";
//	if (ptr == readVirtualDebug(0x80000990, V32).value()) return "mutex";
	if (ptr == readVirtualDebug(0x80000994, V32).value()) return "logicaldevice";
	if (ptr == readVirtualDebug(0x80000998, V32).value()) return "physicaldevice";
	if (ptr == readVirtualDebug(0x8000099C, V32).value()) return "channel";
	if (ptr == readVirtualDebug(0x800009A0, V32).value()) return "server";
//	if (ptr == readVirtualDebug(0x800009A4, V32).value()) return "unk9A4"; // name always null
	if (ptr == readVirtualDebug(0x800009AC, V32).value()) return "library";
//	if (ptr == readVirtualDebug(0x800009B0, V32).value()) return "unk9B0"; // name always null
//	if (ptr == readVirtualDebug(0x800009B4, V32).value()) return "unk9B4"; // name always null
	return NULL;
}

void Emulator::fetchStr(uint32_t str, char *buf) {
	if (str == 0) {
		strcpy(buf, "<NULL>");
		return;
	}
	int size = readVirtualDebug(str, V32).value();
	for (int i = 0; i < size; i++) {
		buf[i] = readVirtualDebug(str + 4 + i, V8).value();
	}
	buf[size] = 0;
}

void Emulator::fetchName(uint32_t obj, char *buf) {
	fetchStr(readVirtualDebug(obj + 0x10, V32).value(), buf);
}

void Emulator::fetchProcessFilename(uint32_t obj, char *buf) {
	fetchStr(readVirtualDebug(obj + 0x3C, V32).value(), buf);
}

void Emulator::debugPC(uint32_t pc) {
	char objName[1000];
	if (pc == 0x2CBC4) {
		// CObjectCon::AddL()
		uint32_t container = getGPR(0);
		uint32_t obj = getGPR(1);
		const char *wut = identifyObjectCon(container);
		if (wut) {
			fetchName(obj, objName);
			if (strcmp(wut, "process") == 0) {
				char procName[1000];
				fetchProcessFilename(obj, procName);
				log("OBJS: added %s at %08x <%s> <%s>", wut, obj, objName, procName);
			} else {
				log("OBJS: added %s at %08x <%s>", wut, obj, objName);
			}
		}
	}

	if (pc == 0x6D8) {
		uint32_t virtAddr = getGPR(0);
		uint32_t physAddr = getGPR(1);
		uint32_t btIndex = getGPR(2);
		uint32_t regionSize = getGPR(3);
		log("KERNEL MMU SECTION: v:%08x p:%08x size:%08x idx:%02x",
			virtAddr, physAddr, regionSize, btIndex);
	}
	if (pc == 0x710) {
		uint32_t virtAddr = getGPR(0);
		uint32_t physAddr = getGPR(1);
		uint32_t btIndex = getGPR(2);
		uint32_t regionSize = getGPR(3);
		uint32_t pageTableA = getGPR(4);
		uint32_t pageTableB = getGPR(5);
		log("KERNEL MMU PAGES: v:%08x p:%08x size:%08x idx:%02x tableA:%08x tableB:%08x",
			virtAddr, physAddr, regionSize, btIndex, pageTableA, pageTableB);
	}

	if (pc == 0x1576C) {
		uint32_t rawEvent = getGPR(0);
		uint32_t evtType = readVirtualDebug(rawEvent, V32).value_or(0);
		uint32_t evtTick = readVirtualDebug(rawEvent + 4, V32).value_or(0);
		uint32_t evtParamA = readVirtualDebug(rawEvent + 8, V32).value_or(0);
		uint32_t evtParamB = readVirtualDebug(rawEvent + 0xC, V32).value_or(0);
		const char *n = "???";
		switch (evtType) {
		case 0: n = "ENone"; break;
		case 1: n = "EPointerMove"; break;
		case 2: n = "EPointerSwitchOn"; break;
		case 3: n = "EKeyDown"; break;
		case 4: n = "EKeyUp"; break;
		case 5: n = "ERedraw"; break;
		case 6: n = "ESwitchOn"; break;
		case 7: n = "EActive"; break;
		case 8: n = "EInactive"; break;
		case 9: n = "EUpdateModifiers"; break;
		case 10: n = "EButton1Down"; break;
		case 11: n = "EButton1Up"; break;
		case 12: n = "EButton2Down"; break;
		case 13: n = "EButton2Up"; break;
		case 14: n = "EButton3Down"; break;
		case 15: n = "EButton3Up"; break;
		case 16: n = "ESwitchOff"; break;
		}
		log("EVENT %s: tick=%d params=%d,%d", n, evtTick, evtParamA, evtParamB);
	}

	// PSION_5MX_TRACE_SUBSCRIBE — log every execution of the 5mx
	// SWI 0x1b thunk at 0x5004b200 (the sole `SWI #0x1b` instruction
	// in the ROM — invoked by every user-mode subscribe wrapper).
	// r14 (lr) at this PC is the actual user-mode caller — i.e.
	// WServ's CActive subclass that issued the subscribe request.
	// Mirrors S7's PSION_S7_TRACE_SUBSCRIBE (thunk at 0x5004d5bc).
	if (pc == 0x4b200 || pc == 0x15948) {
		static int kSubCountThunk = 0;
		static int kSubCountKern  = 0;
		static int64_t kLastCycKern = -10;
		static const char *env = getenv("PSION_5MX_TRACE_SUBSCRIBE");
		if (env && *env && *env != '0') {
			int budget = std::atoi(env);
			if (budget <= 0) budget = 200;
			if (pc == 0x15948) {
				// Dedupe consecutive same-PC hits.
				if (passedCycles - kLastCycKern > 100) {
					uint32_t r0 = getGPR(0);
					uint32_t r1 = getGPR(1);
					uint32_t lr = getGPR(14);
					uint32_t sp = getGPR(13);
					uint32_t curThread = readVirtualDebug(0x80000964, V32).value_or(0);
					uint32_t stk0 = readVirtualDebug(sp + 0, V32).value_or(0);
					uint32_t stk4 = readVirtualDebug(sp + 4, V32).value_or(0);
					++kSubCountKern;
					if (kSubCountKern <= budget || (kSubCountKern & 0x1ff) == 0) {
						log("[5mx-SUB-KERN] #%d cyc=%lld owner=%08x status=%08x lr=%08x sp=%08x stk0=%08x stk4=%08x curThread=%08x",
						    kSubCountKern, (long long)passedCycles,
						    r0, r1, lr, sp, stk0, stk4, curThread);
					}
					kLastCycKern = passedCycles;
				}
			} else if (pc == 0x4b200) {
				uint32_t r0 = getGPR(0);
				uint32_t lr = getGPR(14);
				uint32_t cpsr = getCPSR();
				uint32_t curThread = readVirtualDebug(0x80000964, V32).value_or(0);
				++kSubCountThunk;
				if (kSubCountThunk <= budget) {
					log("[5mx-SUBSCRIBE] #%d cyc=%lld r0=%08x lr=%08x cpsr=%08x curThread=%08x",
					    kSubCountThunk, (long long)passedCycles, r0, lr, cpsr, curThread);
				} else if ((kSubCountThunk & 0x1ff) == 0) {
					log("[5mx-SUBSCRIBE] #%d (sampled) cyc=%lld lr=%08x",
					    kSubCountThunk, (long long)passedCycles, lr);
				}
			}
		}
	}
}


const char *Emulator::getDeviceName() const { return "Series 5mx"; }
int Emulator::getDigitiserWidth()  const { return 695; }
int Emulator::getDigitiserHeight() const { return 280; }
int Emulator::getLCDOffsetX()      const { return 45; }
int Emulator::getLCDOffsetY()      const { return 5; }
int Emulator::getLCDWidth()        const { return 640; }
int Emulator::getLCDHeight()       const { return 240; }

// TODO move this elsewhere
static bool initRgbValues = false;
static uint32_t rgbValues[16];

void Emulator::readLCDIntoBuffer(uint8_t **lines, bool is32BitOutput) const {
	if (!initRgbValues) {
		initRgbValues = true;
		for (int i = 0; i < 16; i++) {
			int r = (0x99 * i) / 15;
			int g = (0xAA * i) / 15;
			int b = (0x88 * i) / 15;
			rgbValues[15 - i] = r | (g << 8) | (b << 16) | 0xFF000000;
		}
	}

	// Pick the right backing block: under INCLUDE_D the C and D regions are
	// distinct (5mx Pro bootloader needs this — see windermere.cpp:13). The
	// 5mx Series 5 ROM uses 0xD0xxxxxx for its framebuffer; older builds
	// aliased everything to MemoryBlockC0 so this function only checked C0.
	uint8_t lcdTop = (uint8_t)(lcdAddress >> 24);
	if (lcdTop == 0xC0 || lcdTop == 0xC1 || lcdTop == 0xD0 || lcdTop == 0xD1) {
#if defined(INCLUDE_BANK1)
		const uint8_t *lcdBuf = (lcdTop == 0xC1) ? &MemoryBlockC1[lcdAddress & MemoryBlockMask]
		                      : (lcdTop == 0xD0) ? &MemoryBlockD0[lcdAddress & MemoryBlockMask]
		                      : (lcdTop == 0xD1) ? &MemoryBlockD1[lcdAddress & MemoryBlockMask]
		                      : &MemoryBlockC0[lcdAddress & MemoryBlockMask];
#elif defined(INCLUDE_D)
		const uint8_t *lcdBuf = (lcdTop == 0xD0 || lcdTop == 0xD1)
		                      ? &MemoryBlockD0[lcdAddress & MemoryBlockMask]
		                      : &MemoryBlockC0[lcdAddress & MemoryBlockMask];
#else
		const uint8_t *lcdBuf = &MemoryBlockC0[lcdAddress & MemoryBlockMask];
#endif
		int width = 640, height = 240;

		// fetch palette
		int bpp = 1 << (lcdBuf[1] >> 4);
		int ppb = 8 / bpp;
		uint16_t palette[16];
		for (int i = 0; i < 16; i++)
			palette[i] = lcdBuf[i*2] | ((lcdBuf[i*2+1] << 8) & 0xF00);

		// build our image out
		int lineWidth = (width * bpp) / 8;
		for (int y = 0; y < height; y++) {
			int lineOffs = 0x20 + (lineWidth * y);
			for (int x = 0; x < width; x++) {
				uint8_t byte = lcdBuf[lineOffs + (x / ppb)];
				int shift = (x & (ppb - 1)) * bpp;
				int mask = (1 << bpp) - 1;
				int palIdx = (byte >> shift) & mask;
				int palValue = palette[palIdx];

				if (is32BitOutput) {
					auto line = (uint32_t *)lines[y];
					line[x] = rgbValues[palValue];
				} else {
					palValue |= (palValue << 4);
					lines[y][x] = palValue ^ 0xFF;
				}
			}
		}
	}
}


void Emulator::diffPorts(uint32_t oldval, uint32_t newval) {
	uint32_t changes = oldval ^ newval;
	if (changes & 1) {
		log("PRT codec enable: %d", newval&1);
		codecEnabled = (newval & 1) != 0;
	}
	if (changes & 2) {
		log("PRT audio amp enable: %d", newval&2);
		audioAmpEnabled = (newval & 2) != 0;
	}
	if (changes & 4) log("PRT lcd power: %d", newval&4);
	if (changes & 8) log("PRT etna door: %d", newval&8);
	if (changes & 0x10) log("PRT sled: %d", newval&0x10);
	if (changes & 0x20) log("PRT pump pwr2: %d", newval&0x20);
	if (changes & 0x40) log("PRT pump pwr1: %d", newval&0x40);
	if (changes & 0x80) {
		// EPOC drives bit 7 itself during PCMCIA polling, producing tens of
		// thousands of log lines per second. Cap to a handful so the real
		// boot trace stays readable.
		static int s_etnaErrLogged = 0;
		if (s_etnaErrLogged++ < 8) log("PRT etna err: %d", newval&0x80);
	}
	if (changes & 0x100) log("PRT rs-232 rts: %d", newval&0x100);
	if (changes & 0x200) log("PRT rs-232 dtr toggle: %d", newval&0x200);
	if (changes & 0x400) log("PRT disable power led: %d", newval&0x400);
	if (changes & 0x800) log("PRT enable uart1: %d", newval&0x800);
	if (changes & 0x1000) log("PRT lcd backlight: %d", newval&0x1000);
	if (changes & 0x2000) log("PRT enable uart0: %d", newval&0x2000);
	if (changes & 0x4000) log("PRT dictaphone: %d", newval&0x4000);
// PROM read process makes this super spammy in stdout
//	if (changes & 0x10000) log("PRT EECS: %d", newval&0x10000);
//	if (changes & 0x20000) log("PRT EECLK: %d", newval&0x20000);
	if (changes & 0x40000) log("PRT contrast0: %d", newval&0x40000);
	if (changes & 0x80000) log("PRT contrast1: %d", newval&0x80000);
	if (changes & 0x100000) log("PRT contrast2: %d", newval&0x100000);
	if (changes & 0x200000) log("PRT contrast3: %d", newval&0x200000);
	if (changes & 0x400000) log("PRT case open: %d", newval&0x400000);
	if (changes & 0x800000) log("PRT etna cf power: %d", newval&0x800000);
}

void Emulator::diffInterrupts(uint16_t oldval, uint16_t newval) {
	uint16_t changes = oldval ^ newval;
	if (changes & 1) log("INTCHG external=%d", newval & 1);
	if (changes & 2) log("INTCHG lowbat=%d", newval & 2);
	if (changes & 4) log("INTCHG watchdog=%d", newval & 4);
	if (changes & 8) log("INTCHG mediachg=%d", newval & 8);
	if (changes & 0x10) log("INTCHG codec=%d", newval & 0x10);
	if (changes & 0x20) log("INTCHG ext1=%d", newval & 0x20);
	if (changes & 0x40) log("INTCHG ext2=%d", newval & 0x40);
	if (changes & 0x80) log("INTCHG ext3=%d", newval & 0x80);
	if (changes & 0x100) log("INTCHG timer1=%d", newval & 0x100);
	if (changes & 0x200) log("INTCHG timer2=%d", newval & 0x200);
	if (changes & 0x400) log("INTCHG rtcmatch=%d", newval & 0x400);
	if (changes & 0x800) log("INTCHG tick=%d", newval & 0x800);
	if (changes & 0x1000) log("INTCHG uart1=%d", newval & 0x1000);
	if (changes & 0x2000) log("INTCHG uart2=%d", newval & 0x2000);
	if (changes & 0x4000) log("INTCHG lcd=%d", newval & 0x4000);
	if (changes & 0x8000) log("INTCHG spi=%d", newval & 0x8000);
}


uint32_t Emulator::readKeyboard() {
	if (kScan & 8) {
		// Select one keyboard
		return keyboardColumns[kScan & 7];
	} else if (kScan == 0) {
		// Report all columns combined
		uint8_t val = 0;
		for (int i = 0; i < 8; i++)
			val |= keyboardColumns[i];
		return val;
	} else {
		return 0;
	}
}

void Emulator::setKeyboardKey(EpocKey key, bool value) {
	int idx = -1;
#define KEY(column, bit) idx = (column << 8) | (1 << bit); break

	switch ((int)key) {
	case EStdKeyDictaphoneRecord: KEY(0, 6);
	case '1':                     KEY(0, 5);
	case '2':                     KEY(0, 4);
	case '3':                     KEY(0, 3);
	case '4':                     KEY(0, 2);
	case '5':                     KEY(0, 1);
	case '6':                     KEY(0, 0);

	case EStdKeyDictaphonePlay:   KEY(1, 6);
	case '7':                     KEY(1, 5);
	case '8':                     KEY(1, 4);
	case '9':                     KEY(1, 3);
	case '0':                     KEY(1, 2);
	case EStdKeyBackspace:        KEY(1, 1);
	case EStdKeySingleQuote:      KEY(1, 0);

	case EStdKeyEscape:           KEY(2, 6);
	case 'Q':                     KEY(2, 5);
	case 'W':                     KEY(2, 4);
	case 'E':                     KEY(2, 3);
	case 'R':                     KEY(2, 2);
	case 'T':                     KEY(2, 1);
	case 'Y':                     KEY(2, 0);

	// Series 5mx has no dedicated Psion modifier — it has a Menu key
	// at matrix slot (3, 6). The frontend sends host Alt as
	// EStdKeyLeftAlt / RightAlt (the canonical "Psion modifier" code on
	// SIBO devices); route those to the same Menu slot here so users
	// who hold Alt for "open the menu" muscle memory keep working on
	// Series 5mx.
	case EStdKeyLeftAlt:
	case EStdKeyRightAlt:
	case EStdKeyMenu:             KEY(3, 6);
	case 'U':                     KEY(3, 5);
	case 'I':                     KEY(3, 4);
	case 'O':                     KEY(3, 3);
	case 'P':                     KEY(3, 2);
	case 'L':                     KEY(3, 1);
	case EStdKeyEnter:            KEY(3, 0);

	case EStdKeyLeftCtrl:         KEY(4, 6);
	case EStdKeyTab:              KEY(4, 5);
	case 'A':                     KEY(4, 4);
	case 'S':                     KEY(4, 3);
	case 'D':                     KEY(4, 2);
	case 'F':                     KEY(4, 1);
	case 'G':                     KEY(4, 0);

	case EStdKeyLeftFunc:         KEY(5, 6);
	case 'H':                     KEY(5, 5);
	case 'J':                     KEY(5, 4);
	case 'K':                     KEY(5, 3);
	case 'M':                     KEY(5, 2);
	case EStdKeyFullStop:         KEY(5, 1);
	case EStdKeyDownArrow:        KEY(5, 0);

	case EStdKeyRightShift:       KEY(6, 6);
	case 'Z':                     KEY(6, 5);
	case 'X':                     KEY(6, 4);
	case 'C':                     KEY(6, 3);
	case 'V':                     KEY(6, 2);
	case 'B':                     KEY(6, 1);
	case 'N':                     KEY(6, 0);

	case EStdKeyLeftShift:        KEY(7, 6);
	case EStdKeyDictaphoneStop:   KEY(7, 5);
	case EStdKeySpace:            KEY(7, 4);
	case EStdKeyUpArrow:          KEY(7, 3);
	case EStdKeyComma:            KEY(7, 2);
	case EStdKeyLeftArrow:        KEY(7, 1);
	case EStdKeyRightArrow:       KEY(7, 0);
	}

	if (idx >= 0) {
		if (value)
			keyboardColumns[idx >> 8] |= (idx & 0xFF);
		else
			keyboardColumns[idx >> 8] &= ~(idx & 0xFF);
	}
}

void Emulator::updateTouchInput(int32_t x, int32_t y, bool down) {
	touchX = x;
	touchY = y;
	if (down) {
		if (!penDown) penSamplesSinceDown = 0;
		penUpLatched = false;
		penDown = true;
		pendingInterrupts |= (1 << EINT3);
	} else if (penDown && penSamplesSinceDown < kPenLatchMinSamples) {
		// The guest never sampled this tap enough to register it. Two
		// ways that happens: EINT3 couldn't be serviced for the whole
		// touch (the kernel wedged mounting a freshly-inserted CF card),
		// or the host delivered a pen-down/pen-up pair with (almost) no
		// sim frames in between — e.g. a quick browser tap while the
		// frontend bursts sim frames through the CF mount, where both
		// pointer events land in the same inter-frame gap. Hold the pen
		// down so the tap is delivered late, like a queued key press,
		// instead of being lost. Released by the tick loop once the pen
		// ISR has sampled it (or the deadline passes).
		penUpLatched = true;
		penLatchDeadline = passedCycles + kPenLatchTimeoutCycles;
	} else {
		penDown = false;
		penUpLatched = false;
		pendingInterrupts &= ~(1 << EINT3);
	}
	log("TOUCH %s (%d,%d) INTENS=%04x pending=%04x samples=%u%s",
	    down ? "DOWN" : "up", x, y,
	    interruptMask, pendingInterrupts & interruptMask,
	    penSamplesSinceDown, (!down && penUpLatched) ? " [latched]" : "");
}

bool Emulator::attachCard(const uint8_t *bytes, size_t size) {
	if (!bytes || size == 0) return false;
	cfCard.attach(bytes, size);
	// 5mx Pro: this card carries SYS$ROM.BIN, i.e. the OS image the
	// bootloader is about to copy into DRAM — and with it the only copy of
	// the model UID that reaches EPOC's Unique id. Find it and stamp the
	// programmed value on before the guest reads a single sector.
	locateMachineIdPrefixOnCard();
	applyMachineIdPrefixToCard();
	cfProbeLogs = 0;
	cfIntLogs = 0;
	mcintTransitionLogs = 0;
	mcintPrev = false;
	cfIrqTransitionLogs = 0;
	cfIrqPrev = false;
	fiqDeliveryLogs = 0;
	fiqDeliveryCount = 0;
	log("CF attach: size=%zu bytes, cardPresent=true [diag-build-v8 door-edge-mcint]", size);
	etna.setCardPresent(true);
	// Simulate the physical door switch: opening the bay to insert a card
	// and letting it snap shut raises MCINT on real hardware. EPOC's boot
	// PCCARD-ARM stops polling SktVarA0/A1 after ~1 s, so without this edge
	// a mid-session attach is invisible to the driver.
	doorOpen = false;
	mcintEdgePending = true;
	return true;
}

void Emulator::detachCard() {
	cfCard.detach();
	machineIdPrefixCardOffsets.clear();
	cfProbeLogs = 0;
	log("CF detach: cardPresent=false");
	etna.setCardPresent(false);
	// Door-switch edge as above: opening the bay to remove a card fires MCINT.
	mcintEdgePending = true;
}

void Emulator::writeAudioInput(const int16_t *src, size_t count) {
	// Drop mic samples on the floor when the host mic is off OR when the
	// guest hasn't enabled the codec RX path via CONFG. CONFG bit 0 = RX
	// enable; FUN_5009fcb0 writes CONFG=3 after setting the driver state
	// to "recording", so gating on CONFG&1 is the guest's opt-in signal.
	if (!audio.hostMicEnabled()) return;
	if ((codecConfig & 0x01) == 0) return;
	bool wasEmpty = audio.enqueueMicSamples(src, count);
	// Only raise CSINT once the guest's record-start path has run to
	// completion. FUN_5009fcb0 (dictaphone record-start) runs these in
	// order:
	//   FUN_50011a18  → register CSINT handler in the kernel chain
	//   *(chan+0x1c)=1 → set state=recording
	//   FUN_5000abf0(3) → write CONFG=3 (we capture recordChannelPtr here)
	//   FUN_500119e4  → unmask CSINT (kernel-side, not the Windermere
	//                   INTENS path — that path is vtable-driven via
	//                   DAT_50007228 rather than a direct INTENS write,
	//                   so our interruptMask field never shows the bit)
	// So the capture of recordChannelPtr (on the CONFG write) is a
	// safe "handler is now registered" signal. Before that, asserting
	// CSINT would spin the kernel's FUN_500072cc IRQ dispatcher on a
	// handler-less slot. Edge-trigger on rising empty→non-empty so we
	// don't flood the kernel with IRQs.
	// Peek the dictaphone channel's current state via the MMU. The
	// driver has two distinct CONFG=3 write paths:
	//   - FUN_5009fad8 (open/warmup): state ← 3, counter 0x20 ← 40.
	//     The DFC's else-branch fires up to 40 times, writing 0x55
	//     warmup tokens to CODR, then transitions state → 2.
	//   - FUN_5009fcb0 (start-record): state ← 1, which triggers the
	//     RX-drain path that actually reads CODR.
	// We fire CSINT for states {1,2,3} — recording, playback, warmup —
	// all of which make progress through the DFC. Edge-triggering on
	// the empty→non-empty transition fires CSINT once per arrival
	// batch; for warmup (state 3) we'd need multiple DFC invocations
	// to advance the counter, so we additionally fire CSINT on any
	// batch arrival if the adc ring is non-empty (see below).
	// Probe the dictaphone channel's state field at +0x1c. On 5mx, EPOC
	// sets this to 1 (recording), 2 (playback), 3 (warmup) or 4 (winddown).
	// Firing CSINT only during those phases preserves the original 5mx
	// behaviour: we don't accidentally re-fire during state 0 (idle) or
	// during the brief gap between CONFG=0 cleanup and recordChannelPtr
	// being reset, both of which can put the kernel's DFC in an
	// unexpected context.
	uint32_t channelState = 0;
	bool stateKnown = false;
	if (recordChannelPtr != 0) {
		if (auto v = readRamVirt32(recordChannelPtr + 0x1c); v.has_value()) {
			channelState = v.value();
			stateKnown = true;
		}
	}
	bool stateRecording = (stateKnown && channelState >= 1 && channelState <= 3);
	// CRITICAL: gate the writeAudioInput CSINT firing on the same
	// ~100 ms settling delay the tick-loop uses, but ONLY for
	// sessions where the 5mx Pro bootloader has run
	// (sawBootloaderPseudoDfc=true). The patched OS installs its
	// CSINT handler AFTER writing CONFG=3 — the inverse of stock 5mx
	// FUN_5009fcb0 ordering — and the patched-OS channel-struct
	// layout occasionally puts a value at +0x1c that probes back as
	// stateRecording=true (it's a kernel-RAM region with whatever
	// the dictaphone driver happens to leave there, sometimes
	// coincidentally in the 1-3 range). Firing CSINT into the
	// half-installed handler chain crashes the OS to splash with a
	// prefetch fault at 0x80000001 — the recurring "5mxPro REC
	// reboots" regression that re-appears whenever this gate is
	// weakened.
	//
	// Stock 5mx / MC218 / Revo never run the 5mx Pro bootloader so
	// sawBootloaderPseudoDfc is always false on those devices and
	// the settling check short-circuits to true → behaves as before
	// (immediate firing on the mic-batch edge).
	constexpr int64_t kPatchedOsSettleCycles = (int64_t)CLOCK_SPEED / 10;  // ~100 ms; protects against CSINT firing during the patched-OS handler install window
	bool sufficientlySettled = !sawBootloaderPseudoDfc ||
	    (codecOnAtCycles >= 0 &&
	     (passedCycles - codecOnAtCycles) > kPatchedOsSettleCycles);
	bool codecActive = stateRecording && sufficientlySettled;
	// PSION_5MXPRO_NO_BATCH_CSINT=1 suppresses the writeAudioInput
	// batch-edge CSINT firing on the patched 5mx Pro path only — used
	// for investigating the post-buffer panic (see docs/5mxpro-rec-
	// investigation.md). The tick-loop's patchedOsCsintTrigger plus
	// the codec model's deferred-re-fire still drive the drain.
	// Default-off because empirical testing showed the panic-vs-no-
	// panic outcome is timing-dependent rather than determined by
	// this gate; the env var is here for future bisection / experiments.
	// Inert on stock 5mx / MC218 / Revo (sawBootloaderPseudoDfc = false).
	bool suppressBatchEdge = false;
	if (sawBootloaderPseudoDfc) {
		static int s_supp = -1;
		if (s_supp == -1) {
			const char *e = PSION_ENV_CSTR("PSION_5MXPRO_NO_BATCH_CSINT");
			s_supp = (e && *e && *e != '0') ? 1 : 0;
		}
		suppressBatchEdge = (s_supp == 1);
	}
	if (wasEmpty && codecActive && !suppressBatchEdge) {
		audio.ackCsint();
		pendingInterrupts |= (1u << CSINT);
	}
}


bool Emulator::installDfcDrainThunk() {
	// Plant a small ARM thunk into a known-zero region of the ROM at
	// virtual 0x50027000 (ROM offset 0x27000, verified to be ~0x9b0 bytes
	// of trailing zeros after the kernel syscall jump table). Writing to
	// ROM[] is safe because our emulator fetches instructions straight
	// out of that byte buffer — modifying it changes what the CPU sees.
	//
	// The thunk becomes the post-IRQ DFC drain hook (stored at virtual
	// 0x80000020 = *(DAT_50005174+0x10)). The kernel IRQ epilogue at
	// FUN_50004980 calls it each IRQ; a null value short-circuits it,
	// so on our unpatched boot nothing drives EPOC's codec DFC forward.
	//
	// CURRENT STATE (audio-emulation branch): the thunk reads a channel
	// pointer from a kernel-RAM slot (virtual 0x80000030). The slot is
	// always zero today — see the CONFG-write path in writeReg32 —
	// because calling FUN_5009f4ec from the post-IRQ hook triggers an
	// ARM MMU domain-access fault: the dictaphone channel's memory
	// chunk is unmapped in whichever user-process page table happens to
	// be active when the IRQ arrives. Making this work requires
	// emulating EPOC's per-chunk page-table swap (or a software Kern::
	// InvalidateCache equivalent) before calling into kernel code. Left
	// as a documented follow-up. The thunk below stays in place so the
	// hook pointer is wired and the mechanism is ready; the slot read
	// just returns 0 and the thunk is a no-op.
	//
	// Tried calling FUN_5001ba70 (the generic DFC-queue runner) with
	// the DFC manager pointer from virtual 0x80000c28 — that faulted on
	// unmapped virtual 0x81c01eb8 during list traversal. Our DFC
	// manager is populated but some of the downstream queues it walks
	// aren't in our context, so the generic runner isn't safe either.
	//
	// Layout (offsets within the thunk):
	//   0x00: stmdb sp!, {r4, lr}        0xe92d4010
	//   0x04: ldr   r4, [pc, #0x1c]      0xe59f401c   ; r4 = *(0x28) = lit1
	//   0x08: ldr   r0, [r4]              0xe5940000   ; r0 = *(0x80000030) channel
	//   0x0c: cmp   r0, #0                0xe3500000
	//   0x10: beq   0x20                  0x0a000002   ; no channel → skip
	//   0x14: ldr   r4, [pc, #0x10]       0xe59f4010   ; r4 = *(0x2c) = lit2
	//   0x18: mov   lr, pc                0xe1a0e00f   ; lr = 0x20
	//   0x1c: bx    r4                    0xe12fff14   ; call FUN_5009f4ec(channel)
	//   0x20: mov   r0, #0                0xe3a00000   ; return 0
	//   0x24: ldmia sp!, {r4, pc}         0xe8bd8010
	//   0x28: .word 0x80000030            channel-ptr slot address
	//   0x2c: .word 0x5009f4ec            dictaphone DFC handler
	//
	// ARM710T supports `bx` (ARMv4T) but not `blx Rn`, so the call site
	// uses the classic mov-lr-pc / bx-rN idiom.

	constexpr uint32_t kThunkVirt = 0x50027000;
	constexpr uint32_t kThunkRomOffset = 0x27000;
	constexpr uint32_t kHookSlotVirt = 0x80000020;  // *(DAT_50005174+0x10)
	constexpr uint32_t kChannelSlotVirt = 0x80000030;

	static const uint32_t thunkWords[12] = {
		0xe92d4010,
		0xe59f401c,
		0xe5940000,
		0xe3500000,
		0x0a000002,
		0xe59f4010,
		0xe1a0e00f,
		0xe12fff14,
		0xe3a00000,
		0xe8bd8010,
		kChannelSlotVirt,   // literal 1: address of channel-ptr storage
		0x5009f4ec,         // literal 2: FUN_5009f4ec
	};

	// Sanity: the region we're about to clobber should still be zero.
	for (size_t i = 0; i < 12 * 4; ++i) {
		if (ROM[kThunkRomOffset + i] != 0 && !dfcThunkInstalled) {
			log("installDfcDrainThunk: ROM[+%zx] is 0x%02x, not zero — "
			    "aborting install to avoid stomping code",
			    (size_t)kThunkRomOffset + i, ROM[kThunkRomOffset + i]);
			return false;
		}
	}

	auto writeWord = [&](uint8_t *block, size_t off, uint32_t w) {
		block[off + 0] = (uint8_t)(w);
		block[off + 1] = (uint8_t)(w >> 8);
		block[off + 2] = (uint8_t)(w >> 16);
		block[off + 3] = (uint8_t)(w >> 24);
	};
	for (size_t i = 0; i < 12; ++i)
		writeWord(ROM, kThunkRomOffset + i * 4, thunkWords[i]);

	// Resolve the physical RAM address for the kernel hook slot and the
	// channel-ptr slot. Both must be mapped (they live in the first page
	// of kernel RAM, which EPOC pages in very early).
	auto slotPhys = virtToPhys(kHookSlotVirt);
	auto chanPhys = virtToPhys(kChannelSlotVirt);
	if (!slotPhys.has_value() || !chanPhys.has_value()) {
		if (!dfcThunkInstalled)
			log("installDfcDrainThunk: kernel RAM not mapped yet "
			    "(hook=%d chan=%d)",
			    (int)slotPhys.has_value(), (int)chanPhys.has_value());
		return false;
	}
	uint32_t physAddr = slotPhys.value();
	uint8_t region = (physAddr >> 24) & 0xFF;
	if (region != 0xC0 && region != 0xC1 && region != 0xD0 && region != 0xD1) {
		log("installDfcDrainThunk: hook slot phys 0x%08x not in kernel RAM",
		    physAddr);
		return false;
	}
	// The default build aliases C1/D0/D1 onto MemoryBlockC0.
	writeWord(MemoryBlockC0, physAddr & MemoryBlockMask, kThunkVirt);

	// Initialise the channel slot to the currently-captured pointer, if
	// any; subsequent captures in writeReg32 update it in-place.
	writeWord(MemoryBlockC0, chanPhys.value() & MemoryBlockMask,
	          recordChannelPtr);

	if (!dfcThunkInstalled) {
		log("installDfcDrainThunk: thunk at virt 0x%08x; hook slot "
		    "0x%08x (phys 0x%08x) = 0x%08x; channel slot 0x%08x "
		    "(phys 0x%08x) = 0x%08x",
		    kThunkVirt, kHookSlotVirt, physAddr, kThunkVirt,
		    kChannelSlotVirt, chanPhys.value(), recordChannelPtr);
		dfcThunkInstalled = true;
	}
	return true;
}

void Emulator::debugInjectTestTone() {
	// Generate ~0.5s of 440 Hz square at the codec sample rate (8 kHz)
	// and push directly into the codec DAC ring. The host pump will
	// drain into the worklet as for any other DAC traffic. Bypasses
	// BZCONT / CODR so this works regardless of whether the guest is
	// touching the audio registers — pure pipeline test.
	const int kSampleRate = 8000;
	const int kDurationMs = 500;
	const int kTotalSamples = kSampleRate * kDurationMs / 1000;
	const int kAmplitude = 100; // int8 range, audible without clipping
	const int kHalfPeriod = kSampleRate / (440 * 2); // ~9 samples
	int phase = 0;
	for (int i = 0; i < kTotalSamples; i++) {
		int8_t raw = ((phase / kHalfPeriod) & 1) ? kAmplitude : -kAmplitude;
		audio.pushDacSample(raw, passedCycles);
		phase++;
		if (phase >= kHalfPeriod * 2) phase = 0;
	}
	log("debugInjectTestTone: pushed %d samples (%d ms @ 8kHz)",
	    kTotalSamples, kDurationMs);
}

bool Emulator::debugForceChannelState(uint32_t s) {
	if (recordChannelPtr == 0) return false;
	auto p = virtToPhys(recordChannelPtr + 0x1c);
	if (!p.has_value()) return false;
	uint32_t phys = p.value();
	uint8_t *blk = memoryBlockForPhys(phys);
	if (!blk) return false;
	uint32_t off = phys & MemoryBlockMask;
	blk[off + 0] = (uint8_t)(s);
	blk[off + 1] = (uint8_t)(s >> 8);
	blk[off + 2] = (uint8_t)(s >> 16);
	blk[off + 3] = (uint8_t)(s >> 24);
	log("debugForceChannelState: *(%08x+0x1c)=%u (phys=%08x)",
	    recordChannelPtr, (unsigned)s, phys);
	return true;
}

void Emulator::publishChannelPtrToThunk() {
	if (!dfcThunkInstalled) return;
	auto chanPhys = virtToPhys(0x80000030);
	if (!chanPhys.has_value()) return;
	uint8_t *blk = memoryBlockForPhys(chanPhys.value());
	if (!blk) return;
	uint32_t off = chanPhys.value() & MemoryBlockMask;
	blk[off + 0] = (uint8_t)(recordChannelPtr);
	blk[off + 1] = (uint8_t)(recordChannelPtr >> 8);
	blk[off + 2] = (uint8_t)(recordChannelPtr >> 16);
	blk[off + 3] = (uint8_t)(recordChannelPtr >> 24);
	log("publishChannelPtrToThunk: channel slot <- %08x (phys %08x)",
	    recordChannelPtr, chanPhys.value());
}

// ── Host serial bridge ────────────────────────────────────────────────────
void Emulator::updateUartIrqs() {
	if (uart1.wantsIrq()) pendingInterrupts |=  (1u << UART1);
	else                  pendingInterrupts &= ~(1u << UART1);
	if (uart2.wantsIrq()) pendingInterrupts |=  (1u << UART2);
	else                  pendingInterrupts &= ~(1u << UART2);
}

bool Emulator::serialAttachHost(int uartIndex) {
	UART *u = (uartIndex == 1) ? &uart1 : (uartIndex == 2) ? &uart2 : nullptr;
	if (!u) return false;
	u->hostAttached = true;
	u->rxFifo.clear();
	u->txQueue.clear();
	// Tell EPOC's serial driver "a cable just plugged in" — without
	// this IRQ the kernel won't notice that CTS/DSR/DCD just went
	// high, and any service that gates on cable-present (Remote Link
	// in particular) stays asleep.
	u->interrupts |= UART::IntModemStatus;
	updateUartIrqs();
	log("serial: host attached to UART%d", uartIndex);
	return true;
}

bool Emulator::serialDetachHost(int uartIndex) {
	UART *u = (uartIndex == 1) ? &uart1 : (uartIndex == 2) ? &uart2 : nullptr;
	if (!u) return false;
	u->hostAttached = false;
	// Preserve rxFifo across detach. The Remote Link dialog sends a
	// Disc_Pdu via serialWriteFromHost right before calling us, which
	// just pushes bytes into rxFifo — the emulated CPU hasn't had a
	// RAF tick to drain them yet. Dropping rxFifo here would lose the
	// Disc_Pdu and leave the device's link layer thinking the cable
	// is still up, so the next reconnect's Req_Req_Pdu gets ignored
	// and the host times out with "link handshake timed out". The
	// CPU drains rxFifo naturally on its next tick; rxFifo is cleared
	// on the next attach (see serialAttachHost) so stale bytes never
	// carry across sessions.
	u->txQueue.clear();
	// Drop pending TX latch and fire a modem-status IRQ so EPOC sees a
	// clean "cable just unplugged" rather than a glitch. Without this
	// the kernel can sit in a tight buzzer-alarm loop after detach.
	// Keep IntRx asserted if rxFifo still has data so the serial
	// driver gets one more IRQ to drain the Disc_Pdu before the
	// cable-unplug event tears the link down.
	u->interrupts &= ~UART::IntTx;
	if (u->rxFifo.empty()) u->interrupts &= ~UART::IntRx;
	u->interrupts |= UART::IntModemStatus;
	updateUartIrqs();
	log("serial: host detached from UART%d (rxFifo retained, %zu B)",
	    uartIndex, u->rxFifo.size());
	return true;
}

size_t Emulator::serialWriteFromHost(int uartIndex, const uint8_t *data, size_t len) {
	UART *u = (uartIndex == 1) ? &uart1 : (uartIndex == 2) ? &uart2 : nullptr;
	if (!u || !u->hostAttached) return 0;
	size_t n = u->pushRxFromHost(data, len);
	updateUartIrqs();
	return n;
}

size_t Emulator::serialReadToHost(int uartIndex, uint8_t *dst, size_t cap) {
	UART *u = (uartIndex == 1) ? &uart1 : (uartIndex == 2) ? &uart2 : nullptr;
	if (!u || !u->hostAttached) return 0;
	size_t n = u->drainTxToHost(dst, cap);
	updateUartIrqs();
	return n;
}

size_t Emulator::serialHostTxAvailable(int uartIndex) const {
	const UART *u = (uartIndex == 1) ? &uart1 : (uartIndex == 2) ? &uart2 : nullptr;
	if (!u || !u->hostAttached) return 0;
	return u->txQueuedBytes();
}

// Host→device RX FIFO depth (bytes the host has pushed that the CPU's serial
// driver has not yet consumed). Diagnostic counterpart to TxAvailable: a FIFO
// that stays full while the device emits nothing localises a wedge to the
// device not draining its RX (vs the host not sending). See the browser repro.
size_t Emulator::serialHostRxPending(int uartIndex) const {
	const UART *u = (uartIndex == 1) ? &uart1 : (uartIndex == 2) ? &uart2 : nullptr;
	if (!u || !u->hostAttached) return 0;
	return u->rxFifoBytes();
}

// Packed serial-interrupt state for the wedge diagnosis (see test/plp-browser).
// A large upload deadlocks with the device idle and not waking on incoming
// bytes; this tells whether the UART's RX interrupt is even enabled, and
// whether the controller would deliver it:
//   bits  0-7  : UART.interrupts      (pending sources; IntRx=1, IntTx=2)
//   bits  8-15 : UART.interruptMask   (which sources the driver enabled)
//   bit   16   : controller pendingInterrupts has this UART's bit
//   bit   17   : controller interruptMask enables this UART's bit
//   bit   18   : UART.wantsIrq()
uint32_t Emulator::debugSerialIrq(int uartIndex) const {
	const UART *u = (uartIndex == 1) ? &uart1 : (uartIndex == 2) ? &uart2 : nullptr;
	if (!u) return 0;
	const uint32_t cbit = (uartIndex == 1) ? (1u << UART1) : (1u << UART2);
	return (uint32_t)(u->interrupts & 0xFF)
	     | ((uint32_t)(u->interruptMask & 0xFF) << 8)
	     | ((pendingInterrupts & cbit) ? (1u << 16) : 0)
	     | ((interruptMask     & cbit) ? (1u << 17) : 0)
	     | (u->wantsIrq()              ? (1u << 18) : 0);
}

bool Emulator::serialIsAttached(int uartIndex) const {
	const UART *u = (uartIndex == 1) ? &uart1 : (uartIndex == 2) ? &uart2 : nullptr;
	if (!u) return false;
	return u->hostAttached;
}

}
