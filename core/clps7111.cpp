// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2019 Ash Wolf (WindEmu); modifications (c) 2024-2026 Joe Haines.
// Subject to the Mozilla Public License v2.0 (http://mozilla.org/MPL/2.0/).

#include "clps7111.h"
#include "rtc_seed.h"
#include "clps7111_defs.h"
#include "hardware.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <time.h>
#include "common.h"


namespace CLPS7111 {

// PSION_UART_TRACE=1 logs every UART1 data / line-control register access
// with the PC that made it — what the guest's serial driver actually does
// with the port, which is the only way to tell "the ROM never opened the
// cable" apart from "the ROM opened it and we dropped its bytes". Bounded
// so a chatty driver can't distort timing; off by default.
static bool uartTraceOn() {
	static int on = -1;
	if (on < 0) on = std::getenv("PSION_UART_TRACE") ? 1 : 0;
	return on != 0;
}
static bool uartTraceBudget() {
	static int n = 0;
	return uartTraceOn() && n++ < 4000;
}

// Kill switches for the two halves of the cable-plug model below, so what
// each one buys can be re-measured rather than taken on trust.
// PSION_UART_NO_ENABLE_RESET=1 keeps the stale interrupt latches across a
// UARTEN 0 -> 1.
static bool uartNoEnableReset() {
	static int on = -1;
	if (on < 0) on = std::getenv("PSION_UART_NO_ENABLE_RESET") ? 1 : 0;
	return on != 0;
}
// PSION_UART_MODEM_INVERT=0/1 forces the SYSFLG1 modem-line polarity,
// overriding the device's modemLinesActiveLow(). -1 = leave it to the
// device.
static int uartModemInvertOverride() {
	static int v = -2;
	if (v == -2) {
		const char *e = std::getenv("PSION_UART_MODEM_INVERT");
		v = e ? (std::atoi(e) ? 1 : 0) : -1;
	}
	return v;
}

Emulator::Emulator() : pcCardController(&cpu) {
	// See Windermere::Emulator() for the full rationale. CLPS7111 has
	// the same uninitialised-member-array layout (uint8_t ROM[8 MiB],
	// ROM2[256 KiB], MemoryBlockC0[8 MiB]); dlmalloc reusing a freed
	// block when the user switches between CLPS7111-family devices
	// (Series 5 ↔ Osaris ↔ MC218 fallback) would leak the previous
	// emulator's state into the new instance and survive a state-save
	// round trip.
	std::memset(ROM,           0, sizeof(ROM));
	std::memset(ROM2,          0, sizeof(ROM2));
	std::memset(MemoryBlockC0, 0, sizeof(MemoryBlockC0));
	cfCard.setOwner(&cpu);
	// Wire the host-serial-bridge UART1 to the CPU so its logging /
	// interrupt-raise helpers have a back-reference. Inert until a host
	// attaches via serialAttachHost(1).
	uart1.cpu = &cpu;
}


uint32_t Emulator::getRTC() {
	// PSION_S5_RTC_OVERRIDE=<unsigned> overrides the RTC seconds count
	// (allows empirical date probing — see series5.h notes).
	if (const char *e = std::getenv("PSION_S5_RTC_OVERRIDE")) {
		return (uint32_t)std::strtoul(e, nullptr, 0);
	}
	return psionInitialRtcSeconds();   // PSION_RTC_SEED pins this
}



uint32_t Emulator::readReg8(uint32_t reg) {
	if (reg == PADR) {
		return composePortA();
	} else if (reg == PBDR) {
		return composePortB();
	} else if (reg == 0x02) {
		return portCData;    // Series 5 PortC data (CL-PS7110 only)
	} else if (reg == PDDR) {
		return (portValues >> 8) & 0xFF;
	} else if (reg == PEDR) {
		return portValues & 0xFF;
	} else if (reg == PADDR) {
		return (portDirections >> 24) & 0xFF;
	} else if (reg == PBDDR) {
		return (portDirections >> 16) & 0xFF;
	} else if (reg == 0x42) {
		return portCDir;     // Series 5 PortC direction (CL-PS7110 only)
	} else if (reg == PDDDR) {
		return (portDirections >> 8) & 0xFF;
	} else if (reg == PEDDR) {
		return portDirections & 0xFF;
	} else if (reg == CODR) {
		// 8-bit CODR read — pop one captured ADC sample. Same FIFO-depth
		// cap and codrValue fall-back as the 32-bit path; EPOC's audio
		// driver typically issues this via LDRB on the codec data port.
		auto r = audio.popAdcSample(passedCycles);
		if (!r.valid) return codrValue & 0xFF;
		if (r.ringEmpty) pendingInterrupts &= ~(1u << CSINT);
		return r.sample;
	} else if (reg == UARTDR1) {
		// UART1 data register (byte access). Mirror the 32-bit path: pop
		// the next inbound IrDA byte from the host bridge when attached,
		// else return 0 (legacy stub).
		if (uart1.hostAttached) {
			uint8_t b = uart1.popRxByte();
			updateUartIrqs();
			if (uartTraceBudget())
				log("UART1 RX read8 -> %02x (left=%zu) pc=%08x lr=%08x",
				    b, uart1.rxFifoBytes(), getRealPC(), getGPR(14));
			return b;
		}
		if (uartTraceBudget())
			log("UART1 RX read8 (unattached) pc=%08x lr=%08x",
			    getRealPC(), getGPR(14));
		return 0;
	} else if (reg == UBRLCR1) {
		// UART1 baud / line-control (byte access). Return last-written
		// value for RMW loops (matches the 32-bit path).
		return uart1LineCtl;
	} else {
		log("RegRead8 unknown:: pc=%08x lr=%08x reg=%03x", getRealPC(), getGPR(14), reg);
		return 0xFF;
	}
}
uint32_t Emulator::readReg32(uint32_t reg) {
	if (reg == SYSCON1) {
		// Return the full SYSCON1 value the kernel wrote (per datasheet
		// 3.2.11 — RW register). The timer/kscan bits are derivative state
		// that we re-derive on read for consistency, but the rest (UART
		// enable, LCD enable, codec enable, IrDA, wake-disable, etc.) must
		// reflect the kernel's last write.
		uint32_t flg = sysCon1 & ~0xFFu;  // keep upper bits as written
		if (tc1.config & Timer::PERIODIC) flg |= 0x10;
		if (tc1.config & Timer::MODE_512KHZ) flg |= 0x20;
		if (tc2.config & Timer::PERIODIC) flg |= 0x40;
		if (tc2.config & Timer::MODE_512KHZ) flg |= 0x80;
		flg |= (kScan & 0xF);
		return flg;
	} else if (reg == SYSFLG1) {
		uint32_t flg = sysFlg1;
		flg |= 2; // external power present (DCDET)
		flg |= (rtcDiv << 16);
		// SYSFLG status bits we don't actively model: report the post-reset
		// "idle" values per CL-PS7110 datasheet 3.2.12 / CL-PS7111 5.9.
		// Without these, the kernel reads "data available" when nothing is
		// queued and pulls garbage from empty FIFOs:
		//   bit 22 URXFE (UART RX FIFO empty)  — set: no RX data
		//   bit 24 CRXFE (codec RX FIFO empty) — set: no codec RX
		// Bits we leave 0 (kernel reads "not busy / not full" — correct
		// for a quiescent emulated device):
		//   bit 11 UBUSY    (UART transmitter busy)
		//   bit 23 UTXFF    (UART TX FIFO full)
		//   bit 25 CTXFF    (codec TX FIFO full)
		//   bit 26 SSIBUSY  (sync serial busy)
		// URXFE (UART1 RX FIFO empty): clear only when the bridge is
		// attached AND has queued bytes. Unattached it stays set, which is
		// the legacy stub behaviour boot has always seen.
		if (!uart1.hostAttached || !uart1.rxHasData())
			flg |= (1u << 22);
		// Modem-status inputs (CTS bit 8 / DSR bit 9 / DCD bit 10): a host
		// cable is the far end asserting all three. Whether "asserted"
		// reaches the SoC pin as a 1 or a 0 is the board's line receiver's
		// business, so it comes from the device — see
		// modemLinesActiveLow() in clps7111.h. On an active-low machine
		// the bits therefore read 1 (all three negated) with NO cable
		// attached, which is the honest reading of an unplugged port.
		{
			int ov = uartModemInvertOverride();
			bool inverted = ov >= 0 ? (ov != 0) : modemLinesActiveLow();
			bool asserted = uart1.hostAttached;
			if (asserted != inverted)
				flg |= (1u << 8) | (1u << 9) | (1u << 10);
		}
		// CRXFE: codec RX FIFO empty. Reflects adcQueue state so the
		// kernel's drain loop knows when to stop reading CODR. Also
		// goes high when the per-cycle FIFO-read cap is hit, mirroring
		// the real 16-entry hardware FIFO emptying out.
		if (audio.isRxFifoEmpty())
			flg |= (1u << 24);
		// CTXFF: codec TX FIFO full. Reflects the virtual TX FIFO so
		// the kernel's playback driver stops refilling when full and
		// waits for CSINT after the drain.
		if (audio.isTxFifoFull())
			flg |= (1u << 25);
		return flg;
	} else if (reg == INTSR1) {
		uint32_t v = pendingInterrupts & 0xFFFF;
		// Diagnostic log for EINT3 status reads (PSION_LOG_INTSR1=1).
		if (std::getenv("PSION_LOG_INTSR1")) {
			static int loggedN = 0;
			if (loggedN < 200 || (loggedN & 0xFF) == 0) {
				log("INTSR1 read → 0x%04x  EINT3=%d EINT1=%d  pc=%08x lr=%08x",
				    v, (v >> 7) & 1, (v >> 5) & 1, getRealPC(), getGPR(14));
			}
			loggedN++;
		}
		return v;
	} else if (reg == INTMR1) {
		return interruptMask & 0xFFFF;
	} else if (reg == LCDCON) {
		return lcdControl;
	} else if (reg == MEMCFG1) {
		return memCfg1;
	} else if (reg == MEMCFG2) {
		static int n = 0;
		if (n++ < 8 && std::getenv("PSION_S5_MEM_TRACE")) {
			log("readReg32 MEMCFG2 -> 0x%08x  pc=%08x lr=%08x",
			    memCfg2, getRealPC(), getGPR(14));
		}
		return memCfg2;
	} else if (reg == DRFPR) {
		return drfpr;
	} else if (reg == TC1D) {
		return tc1.value;
	} else if (reg == TC2D) {
		return tc2.value;
	} else if (reg == RTCDR) {
		// Match the Windermere/5mx behaviour: every kernel read of
		// RTCDR refreshes from the host clock via getRTC(). That way
		// the kernel always sees the true current time even if it
		// stamped a different default into the register at boot.
		// PSION_S5_RTC_KEEP=1 disables the refresh for diagnostics.
		if (!std::getenv("PSION_S5_RTC_KEEP")) {
			rtc = getRTC();
		}
		static int firstReads = 0;
		if (firstReads++ < 20 && std::getenv("PSION_S5_RTC_TRACE")) {
			log("readReg32 RTCDR #%d -> rtc=0x%08x  pc=%08x lr=%08x",
			    firstReads, rtc, getRealPC(), getGPR(14));
		}
		return rtc;
	} else if (reg == SYNCIO) {
		// DEBUG: confirm we enter the SYNCIO read path
		{
			static int reg32SyncioLogged = 0;
			if (reg32SyncioLogged < 50) {
				log("readReg32 SYNCIO entered #%d  reg=0x%x  pc=%08x lr=%08x",
				    reg32SyncioLogged, reg, getRealPC(), getGPR(14));
				reg32SyncioLogged++;
			}
		}
		// CL-PS7110 datasheet section 1.2.9: reading SYNCIO clears SSEOTI.
		pendingInterrupts &= ~(1u << SSEOTI);
		// Determine ADC chip family. Series 5 uses ADS7843 (S=1 start bit
		// + 3-bit channel select); MC218/Osaris/5mx use ADC1010 with
		// completely different channel codes (0xC1=X, 0x81=Y, 0x91=batt).
		// Discriminator: PS7110 (Series 5) has chipHasSysCon2()==false;
		// PS7111 (MC218/Osaris/5mx) has chipHasSysCon2()==true.
		bool isAds7843 = !chipHasSysCon2();
		uint8_t controlByte = lastSyncioRequest & 0xFF;
		// Device-specific SSI peripheral gets first refusal — see
		// syncioResponse's declaration.
		if (MaybeU32 r = syncioResponse(controlByte); r.has_value())
			return r.value();
		if (!isAds7843) {
			// Legacy ADC1010 mapping for non-Series-5 chips:
			switch (controlByte) {
			case 0xC1: return (touchX * 8) + 305;     // DigitiserX
			case 0x81: return (touchY * 13.53) + 680;  // DigitiserY
			case 0x91: return 3000;                    // MainBattery
			case 0xD1: return 3100;                    // BackupBattery
			case 0xA1: return 1000;                    // Reference
			}
		} else if ((controlByte & 0x80) != 0) {
			// Series 5 (PS7110) ADC reads. The chip family was originally
			// thought to be a TI ADS7843 (S=1 start bit + 3-bit channel),
			// but the actual EPOC R1 kernel HAL was written for the same
			// ADC1010-compatible chip the MC218 / Osaris CLPS7111 uses
			// (see WindEmu-master/WindCore/clps7111.cpp lines 68-80).
			// SYNCIO write trace from the Series 5 ROM:
			//   0x7091  MainBattery   (was misread as Y touch → 0xFFF)
			//   0x70D1  BackupBattery (was misread as X touch → 0xFFF)
			//   0x70A1  Reference     (was misread as MainBattery → 0xBB8)
			//   0x70F1  AUX / pressure
			//   0x6D0C  Y touch (handled by path 3 below — bit 7 clear)
			//   0x6D09  X touch (handled by path 3 below — bit 7 clear)
			//
			// The previous channel-based dispatch (channel = (ctrl >> 4) & 7)
			// conflated 0x81 with 0x91 and 0xC1 with 0xD1 — codes that mean
			// completely different things to the HAL. The kernel polled
			// 0x91 / 0xD1 constantly expecting battery voltages but got
			// touch-probe sentinel 0xFFF = 4.095V, which the HAL flags as
			// above the 2-AA-pack max → "faulty sensor" → reports 0V.
			//
			// 12-bit ADC result, treated as millivolts directly by the HAL.
			bool penDown = (touchX || touchY);
			uint32_t adcVal = 0;
			switch (controlByte) {
			case 0x91:           // MainBattery
				adcVal = 0xBB8;  // 3000 mV → 3.0V
				break;
			case 0xD1:           // BackupBattery
				adcVal = 0xC1C;  // 3100 mV → 3.1V
				break;
			case 0xA1:           // Reference (1.0V)
				adcVal = 0x3E8;  // 1000 mV
				break;
			case 0xF1:           // AUX / pressure channel (kernel polls
				                 // occasionally alongside the battery reads)
				adcVal = penDown ? 0x200 : 0xC1C;
				break;
			case 0xE1:           // PC-card socket Vcc sense (Series 5). EPOC
				                 // R1's PSU power-up samples this channel and
				                 // requires ~3.3 V (sensed through a divider,
				                 // ~1320 mV raw) before it will enumerate the
				                 // CF card. socketVccSenseAdc() returns the live
				                 // sense while the socket is powered, else 0.
				adcVal = socketVccSenseAdc();
				break;
			case 0x81:           // DigitiserY — Series 5 ROM uses 0x0C
				                 // instead, so this is dead-path; preserve
				                 // empirical regression for legacy callers.
				if (penDown) {
					static int yK = -1;
					if (yK < 0) {
						const char *e = std::getenv("PSION_S5_TOUCH_Y_K");
						yK = e ? std::atoi(e) : 3904;
					}
					int32_t v = yK - (int32_t)(touchY * 12.59);
					if (v < 101)  v = 101;
					if (v > 3999) v = 3999;
					adcVal = (uint32_t)v;
				} else {
					adcVal = 0xFFFu;
				}
				break;
			case 0xC1:           // DigitiserX — Series 5 ROM uses 0x09
				                 // instead, so this is dead-path; preserve
				                 // empirical regression for legacy callers.
				if (penDown) {
					static int xK = -1;
					if (xK < 0) {
						const char *e = std::getenv("PSION_S5_TOUCH_X_K");
						xK = e ? std::atoi(e) : 3999;
					}
					int32_t v = xK - (int32_t)(touchX * 5.62);
					if (v < 101)  v = 101;
					if (v > 3999) v = 3999;
					adcVal = (uint32_t)v;
				} else {
					adcVal = 0xFFFu;
				}
				break;
			default:
				adcVal = 0;
				break;
			}
			adcVal &= 0xFFF;
			// Return the raw 12-bit ADC value AS-IS — matching the 5mx
			// ADC1010 code path above which returns (touchX*8)+305
			// directly with no left-shift.
			//
			// Per disasm of FUN_5007C4FC (the post-tap X-handler):
			//   ldr r3, [r4, #8]     ; sample_avg from object[8]
			//   sub r3, r3, #0x65    ; - 101
			//   ldr r2, [pc, ...]    ; 0xF3A = 3898
			//   cmp r3, r2
			//   bhi REJECT           ; unsigned > → reject
			// Valid range: sample_avg ∈ [101, 3999], i.e. a raw 12-bit
			// ADC value. With the previous (adcVal << 3) packing the
			// kernel saw values in [808, 31992] and rejected every tap,
			// even though our X-coord captured correctly through the
			// pipeline (verified: sample_avg=0x2E88 for touchX=200
			// matches 305+200*5.92=1489 left-shifted-by-3, then rejected
			// because 11912 > 3999).
			//
			// PEN-DETECT BIT: the previous code OR'd in bit 0x400 of
			// the shifted response to signal pen-down. Removing the
			// shift means we no longer have a dedicated pen-detect
			// bit position. That's fine — the kernel reads pen state
			// via INTSR1 & 0x80 (FUN_5007cd24), NOT via a bit in the
			// SYNCIO data. The bit-0x400 hypothesis was conjecture
			// from before we'd traced the validation logic.
			uint32_t result16 = adcVal;
			static int ads7843Logged = 0;
			static int ads7843LogLimit = -1;
			if (ads7843LogLimit < 0) {
				const char *e = std::getenv("PSION_ADS7843_LOG_LIMIT");
				ads7843LogLimit = e ? std::atoi(e) : 200;
			}
			if (ads7843Logged < ads7843LogLimit || (ads7843Logged & 0x3F) == 0) {
				log("Series5 ADC read: req=0x%04x ctrl=0x%02x adc=0x%03x → 0x%04x  (pen=%s touchX=%d touchY=%d)",
				    lastSyncioRequest, controlByte, adcVal, result16,
				    penDown ? "down" : "up", touchX, touchY);
			}
			ads7843Logged++;
			return result16;
		}
		// Series 5 EPOC R1 polls an ADS7843 touchscreen controller via
		// SYNCIO. Per FUN_5001D6E0 in the ROM:
		//   read SYNCIO; check bit 0x400; if clear write 0x6D09
		//   (channel-code 9 = X aux), if set write 0x6D0C (channel-code
		//   12 = Y aux). FUN_50018174 forms the request as
		//   (param_1 << 8) | param_2 | 0x6000 — so the channel-code
		//   is in the LOW byte, NOT the high byte.
		//
		// PSION_S5_ADC_EMU=N selects emulation mode (default 1):
		//   0  off (use defaultSyncioReadValue() = 0x800)
		//   1  per-channel response: return scaled ADC value with bit
		//      0x400 as pen-down status (1=pen present)
		//   2  test mode: return alternating bit-0x400 values
		//
		// Was previously named PSION_S5_ADC1010_EMU — kept the old
		// env-var name as an alias since some saved sessions/scripts
		// might use it. (The chip is actually an ADS7843 per Series 5
		// schematics; ADC1010 was an earlier mis-identification.)
		// adcMode must NOT be a function-scope static — switching from
		// Series 5 (PS7110, default 1) to a PS7111 device like Osaris
		// (default 0) would otherwise reuse the cached Series 5 value
		// and route Osaris reads through the wrong emulation. The user
		// reported Osaris showing "Corrupt" launcher icons when loaded
		// after Series 5 in the same browser session — this static
		// caching was the root cause. Cache the env-var override (which
		// doesn't change) but recompute the per-device default from
		// sysFlg1 every read.
		static int adcModeEnv = -2;  // -2 = not checked, -1 = no env, else value
		if (adcModeEnv == -2) {
			const char *e = std::getenv("PSION_S5_ADC_EMU");
			if (!e) e = std::getenv("PSION_S5_ADC1010_EMU"); // legacy alias
			adcModeEnv = e ? std::atoi(e) : -1;
		}
		int adcMode = (adcModeEnv >= 0) ? adcModeEnv
		                                : ((sysFlg1 & 0x20000000u) ? 0 : 1);
		uint32_t result = defaultSyncioReadValue();
		if (adcMode == 1) {
			// Per-channel response. 16-bit SYNCIO read returns:
			//   bits 0-11  : 12-bit ADC data
			//   bit 10 (=0x400) : pen-down status (1 = pen present)
			//   bits 12-15: status / channel-id echo
			//
			// Fix vs earlier broken implementation: the channel-code
			// is the LOW byte of lastSyncioRequest (0x6D09 → code=9),
			// NOT the high byte. Earlier code did `>> 8` and so never
			// matched any case, always returning 0x000 + pen-down bit.
			//
			// Channel-code mapping observed from FUN_5001D6E0 +
			// FUN_5001E568 + chained DFCs FUN_5001D728/_D74C/etc:
			//   0x08  init/calibration request (write-only)
			//   0x09  X-position (pen-up branch reads this)
			//   0x0C  Y-position (pen-down branch reads this)
			//   0xF1  aux / pressure (FUN_5001D7B4)
			//   0xF1+ aux / pressure / battery during chain steps
			//
			// ADS7843 12-bit value range: 0..4095. For our 640x240
			// panel, scale physical coords linearly to typical panel
			// ADC range (~200..3800 X, ~250..3600 Y).
			uint32_t code = lastSyncioRequest & 0xFF;
			uint32_t penDown = (touchX || touchY) ? 0x400u : 0u;
			switch (code) {
			case 0x09: // X-position channel
				// Linear: ADC = 200 + (touchX * (3800-200)) / 640
				//             = 200 + touchX * 5.625 (~5.625 per pixel)
				result = penDown
					? (uint32_t)(200 + touchX * 5.625) & 0xFFF
					: 0xFFFu;
				break;
			case 0x0C: // Y-position channel
				result = penDown
					? (uint32_t)(250 + touchY * 13.96) & 0xFFF
					: 0xFFFu;
				break;
			case 0xF1: // aux / pressure / pen-detect
				// Mid-scale when pen down, max when pen up.
				result = penDown ? 0x800u : 0xFFFu;
				break;
			default:
				// Unknown channel: return mid-scale, bit 0x400 clear.
				// (Reaching here means the kernel sent a code we don't
				// model — likely a battery / VBAT read on a non-X/Y
				// channel; return a plausible default.)
				result = 0x000u;
				break;
			}
			// Force bit 0x400 to reflect pen state regardless of the
			// channel-specific data above.
			result = (result & ~0x400u) | penDown;
		} else if (adcMode == 2) {
			static int toggle = 0;
			toggle ^= 1;
			result = toggle ? (result | 0x400u) : (result & ~0x400u);
		}
		// Log SYNCIO reads with progressive throttling so we can trace
		// touch-driver activity without flooding the log if it polls
		// every cycle. PSION_SYNCIO_LOG_LIMIT=N (default 200) is the
		// upper bound; after that, log every 256th read.
		static int syncioLogged = 0;
		static int syncioLogLimit = -1;
		if (syncioLogLimit < 0) {
			const char *e = std::getenv("PSION_SYNCIO_LOG_LIMIT");
			syncioLogLimit = e ? std::atoi(e) : 200;
		}
		if (syncioLogged < syncioLogLimit || (syncioLogged & 0xFF) == 0) {
			log("SYNCIO read #%d req=%08x → result=%08x pc=%08x lr=%08x mode=%d",
			    syncioLogged, lastSyncioRequest, result, getRealPC(), getGPR(14), adcMode);
		}
		syncioLogged++;
		return result;
	} else if (reg == PALLSW) {
		return lcdPalette & 0xFFFFFFFF;
	} else if (reg == PALMSW) {
		return lcdPalette >> 32;
	} else if (reg == SYSCON2) {
		if (!chipHasSysCon2()) {
			log("PS7110: SYSCON2 read (reserved) pc=%08x", getRealPC());
			return 0xFFFFFFFFu;
		}
		return 0;
	} else if (reg == SYSFLG2) {
		if (!chipHasSysCon2()) {
			log("PS7110: SYSFLG2 read (reserved) pc=%08x", getRealPC());
			return 0xFFFFFFFFu;
		}
		return 0;
	} else if (reg == INTSR2) {
		if (!chipHasSysCon2()) {
			log("PS7110: INTSR2 read (reserved) pc=%08x", getRealPC());
			return 0xFFFFFFFFu;
		}
		return pendingInterrupts >> 16;
	} else if (reg == INTMR2) {
		if (!chipHasSysCon2()) {
			log("PS7110: INTMR2 read (reserved) pc=%08x", getRealPC());
			return 0xFFFFFFFFu;
		}
		return interruptMask >> 16;
	} else if (reg == PMPCON) {
		return pmpcon;
	} else if (reg == RTCMR) {
		// RTC match register stub — kernel reads back what it wrote.
		return rtcMatch;
	} else if (reg == CODR) {
		// 32-bit CODR read — pops one captured ADC sample. EPOC reads
		// CODR as a byte zero-extended into a word, so we widen the
		// int16 ring's signed-8-bit view here. FIFO depth cap mirrors
		// the real CPSR 16-entry hardware FIFO so the record DFC's
		// drain loop terminates after the same number of samples a
		// real codec would surface in one cycle.
		//
		// When the ADC ring is empty (no mic samples queued, host mic
		// off, or guest hasn't enabled capture) we fall back to the
		// last-written CODR value. Real hardware reads "undefined"
		// from an empty RX FIFO but EPOC's driver may RMW the codec
		// register during init and rely on read-back consistency —
		// the legacy code stored codrValue specifically for that case.
		auto r = audio.popAdcSample(passedCycles);
		if (!r.valid) return codrValue;
		if (r.ringEmpty) pendingInterrupts &= ~(1u << CSINT);
		return r.sample;
	} else if (reg == UARTDR1) {
		// UART1 data register. With a host cable attached, pop the next
		// inbound IrDA byte from the bridge RX FIFO and refresh the
		// interrupt latches. Otherwise keep the legacy "no RX" stub
		// (return 0) so boot paths are undisturbed.
		if (uart1.hostAttached) {
			uint8_t b = uart1.popRxByte();
			updateUartIrqs();
			if (uartTraceBudget())
				log("UART1 RX read32 -> %02x (left=%zu) pc=%08x lr=%08x",
				    b, uart1.rxFifoBytes(), getRealPC(), getGPR(14));
			return b;
		}
		if (uartTraceBudget())
			log("UART1 RX read32 (unattached) pc=%08x lr=%08x",
			    getRealPC(), getGPR(14));
		return 0;
	} else if (reg == UBRLCR1) {
		// UART1 baud rate / line control. Osaris boot reads this in
		// early UART init. Return last-written value so RMW loops
		// (e.g. set bit X, read back, verify) work.
		return uart1LineCtl;
	} else if (reg == UARTDR2) {
		if (!chipHasSysCon2()) {
			log("PS7110: UARTDR2 read (reserved) pc=%08x", getRealPC());
			return 0xFFFFFFFFu;
		}
		return 0;
	} else if (reg == UBRLCR2) {
		if (!chipHasSysCon2()) {
			log("PS7110: UBRLCR2 read (reserved) pc=%08x", getRealPC());
			return 0xFFFFFFFFu;
		}
		return uart2LineCtl;
	} else {
		log("RegRead32 unknown:: pc=%08x lr=%08x reg=%03x", getRealPC(), getGPR(14), reg);
		return 0xFFFFFFFF;
	}
}

void Emulator::writeReg8(uint32_t reg, uint8_t value) {
	if (reg == PADR) {
		uint32_t oldPorts = portValues;
		portValues &= 0x00FFFFFF;
		portValues |= (uint32_t)value << 24;
		diffPorts(oldPorts, portValues);
	} else if (reg == PBDR) {
		uint32_t oldPorts = portValues;
		portValues &= 0xFF00FFFF;
		portValues |= (uint32_t)value << 16;
		// Port B bits 0 / 1 are the settings PROM's select and clock on
		// the machines that have one wired here (the Series 5); the
		// default hook does nothing, which is right for the Osaris.
		onPortBWrite(oldPorts, portValues);
		diffPorts(oldPorts, portValues);
	} else if (reg == 0x02) {
		portCData = value;   // Series 5 PortC data
	} else if (reg == PDDR) {
		uint32_t oldPorts = portValues;
		portValues &= 0xFFFF00FF;
		portValues |= (uint32_t)value << 8;
		diffPorts(oldPorts, portValues);
	} else if (reg == PEDR) {
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
	} else if (reg == 0x42) {
		portCDir = value;    // Series 5 PortC direction
	} else if (reg == PDDDR) {
		portDirections &= 0xFFFF00FF;
		portDirections |= (uint32_t)value << 8;
	} else if (reg == PEDDR) {
		portDirections &= 0xFFFFFF00;
		portDirections |= (uint32_t)value;
	} else if (reg == FRBADDR) {
		if (!chipHasFRBADDR()) {
			log("PS7110: FRBADDR write %02x ignored (FB fixed at 0xC0000000)", value);
		} else {
			log("LCD: address write %08x", value << 28);
			lcdAddress = value << 28;
		}
	} else if (reg == DRFPR) {
		drfpr = value;
	} else if (reg == CODR) {
		// 8-bit CODR write — push one signed PCM byte to the host DAC
		// ring + virtual TX FIFO. AudioCodecModel handles the int16
		// expansion, FIFO bookkeeping, and drain scheduling.
		audio.pushDacSample((int8_t)value, passedCycles);
		codrValue = (codrValue & ~0xFFu) | value;
		if (codrWritesLogged < 8) {
			log("CODR write8 %02x pc=%08x lr=%08x", value & 0xFF,
			    getRealPC(), getGPR(14));
			codrWritesLogged++;
		}
	} else if (reg == COEOI) {
		// 8-bit COEOI ack — clears CSINT and resets the per-cycle FIFO
		// read counter so the next CSINT can surface another batch.
		pendingInterrupts &= ~(1u << CSINT);
		audio.ackCsint();
	} else if (reg == UARTDR1) {
		// UART1 transmit data (byte access). Mirror the 32-bit path:
		// enqueue to the host bridge when attached, else discard.
		if (uart1.hostAttached) {
			uart1.pushTxByte(value);
			updateUartIrqs();
		}
		if (uartTraceBudget())
			log("UART1 TX write8 %02x (attached=%d) pc=%08x lr=%08x",
			    value, (int)uart1.hostAttached, getRealPC(), getGPR(14));
		uart1Data = value;
	} else if (reg == UBRLCR1) {
		// UART1 baud / line-control (byte access).
		if (uartTraceOn())
			log("UART1 UBRLCR1 write8 %02x pc=%08x lr=%08x",
			    value, getRealPC(), getGPR(14));
		uart1LineCtl = value;
	} else {
		log("RegWrite8 unknown:: pc=%08x reg=%03x value=%02x", getRealPC(), reg, value);
	}
}
void Emulator::writeReg32(uint32_t reg, uint32_t value) {
	if (reg == SYSCON1) {
		// PS7110: SYSCON1 is 24-bit (bits 24-31 reserved). Mask via the
		// chip-variant virtual so the kernel reads back only the bits real
		// hardware would store. PS7111 default mask is 0xFFFFFFFF, no-op.
		value &= sysConMask();
		uint32_t prevSysCon1 = sysCon1;
		sysCon1 = value;  // preserve full value for read-back
		// UARTEN (bit 8) 0 -> 1: the guest has just switched the UART on,
		// so it starts from a clean interrupt state. A disabled UART has
		// no modem-status change detector running and no transmitter to
		// finish a byte, so neither can have latched anything — but our
		// host bridge does latch: serialAttachHost() sets IntModemStatus
		// the moment a cable is "plugged in", which on a machine that has
		// not opened its port yet sits there until it does and is then
		// delivered as a spurious "the modem lines just changed" the
		// instant the driver unmasks UMSINT.
		//
		// Real hardware reports no such edge: a cable present before the
		// port was enabled is simply a cable that is already there, and
		// SYSFLG1 says so on the first read. Clearing the latches here is
		// what makes the emulated machine agree.
		//
		// IntRx is deliberately left to re-derive from the RX FIFO rather
		// than being cleared with the others: it is a function of queued
		// bytes, not an edge, and dropping it while bytes are waiting
		// would strand them.
		if (!uartNoEnableReset() &&
		    (value & (1u << 8)) && !(prevSysCon1 & (1u << 8))) {
			uart1.interrupts &= ~(UART::IntModemStatus | UART::IntTx);
			pendingInterrupts &= ~((1u << UMSINT) | (1u << UTXINT));
			if (uartTraceOn())
				log("UART1 UARTEN 0->1: interrupt latches reset "
				    "(attached=%d) pc=%08x", (int)uart1.hostAttached,
				    getRealPC());
		}
		kScan = value & 0xF;
		uint8_t tc1cfg = Timer::ENABLED; // always on with PS-7111!
		if (value & 0x10) tc1cfg |= Timer::PERIODIC;
		if (value & 0x20) tc1cfg |= Timer::MODE_512KHZ;
		uint8_t tc2cfg = Timer::ENABLED;
		if (value & 0x40) tc2cfg |= Timer::PERIODIC;
		if (value & 0x80) tc2cfg |= Timer::MODE_512KHZ;
		tc1.setConfig(tc1cfg);
		tc2.setConfig(tc2cfg);
		// Buzzer drive — SYSCON1 bits 9 (BZTOG) and 10 (BZMOD). See
		// CL-PS7110 datasheet 3.2.11 / CL-PS7111 5.5: BZMOD=0 routes
		// BZTOG straight to the buzzer pin; BZMOD=1 auto-toggles the
		// pin on every TC1 underflow. Latch a one-tick "hold" on the
		// 0→1 BZTOG edge so the click pattern (a sub-millisecond high
		// pulse) survives our 64 Hz TINT pump cadence and produces an
		// audible burst.
		bool newBzTog = (value & 0x200) != 0;  // bit 9
		bool newBzMod = (value & 0x400) != 0;  // bit 10
		if (newBzTog && !buzzerBzTog) {
			audio.noteBuzzerRisingEdge();
			// Surface the first few BZTOG-rising edges unconditionally
			// so a tester capturing browser console logs can confirm
			// whether the kernel is actually using the buzzer pin for
			// click feedback on their device. Throttled to eight per
			// session so real gameplay doesn't drown the log.
			if (bztogRisingLogged < 8) {
				log("Buzzer BZTOG 0→1 (BZMOD=%d) pc=%08x lr=%08x",
				    newBzMod, getRealPC(), getGPR(14));
				bztogRisingLogged++;
			}
		}
		buzzerBzTog = newBzTog;
		buzzerBzMod = newBzMod;
		// CDENRX (codec RX enable) 1→0 — kernel is tearing down the
		// recording session. Drop any pending CSINT and reset the codec
		// RX state so writeAudioInput doesn't keep firing CSINT against
		// a torn-down handler.
		bool prevCdenrx = (prevSysCon1 & (1u << 14)) != 0;
		bool nowCdenrx  = (value       & (1u << 14)) != 0;
		if (prevCdenrx && !nowCdenrx) {
			pendingInterrupts &= ~(1u << CSINT);
			audio.resetRx();
		}
		// CDENTX 1→0 — playback session teardown. Reset the virtual
		// TX FIFO so leftover level doesn't keep firing CSINT after
		// the kernel handler has gone away.
		bool prevCdentx = (prevSysCon1 & (1u << 13)) != 0;
		bool nowCdentx  = (value       & (1u << 13)) != 0;
		if (prevCdentx && !nowCdentx) {
			pendingInterrupts &= ~(1u << CSINT);
			audio.resetTx();
		}
		if (std::getenv("PSION_IRQ_TRACE"))
			log("SYSCON1 write: %08x kscan=%x tc1={per=%d,512k=%d} tc2={per=%d,512k=%d} bz={tog=%d,mod=%d} cdenrx=%d",
				value, kScan, !!(value&0x10), !!(value&0x20), !!(value&0x40), !!(value&0x80),
				newBzTog, newBzMod, nowCdenrx);
	} else if (reg == INTMR1) {
		uint32_t prev = interruptMask & 0xFFFF;
		interruptMask &= 0xFFFF0000;
		interruptMask |= (value & 0xFFFF);
		uint32_t now = value & 0xFFFF;
		// Always log INTMR1 changes — small volume (write only on
		// init / IRQ unmask events) and crucial for tracing when the
		// kernel arms specific interrupt sources (e.g., EINT3 = bit 7
		// for the Series 5 pen-IRQ from ADS7843).
		if (prev != now) {
			log("INTMR1 write: %04x -> %04x (delta: %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s)",
				prev, now,
				((prev ^ now) & 0x0001) ? ((now & 0x0001) ? "+EXTFIQ ":"-EXTFIQ ") : "",
				((prev ^ now) & 0x0002) ? ((now & 0x0002) ? "+BLINT ":"-BLINT ") : "",
				((prev ^ now) & 0x0004) ? ((now & 0x0004) ? "+WEINT ":"-WEINT ") : "",
				((prev ^ now) & 0x0008) ? ((now & 0x0008) ? "+MCINT ":"-MCINT ") : "",
				((prev ^ now) & 0x0010) ? ((now & 0x0010) ? "+CSINT ":"-CSINT ") : "",
				((prev ^ now) & 0x0020) ? ((now & 0x0020) ? "+EINT1 ":"-EINT1 ") : "",
				((prev ^ now) & 0x0040) ? ((now & 0x0040) ? "+EINT2 ":"-EINT2 ") : "",
				((prev ^ now) & 0x0080) ? ((now & 0x0080) ? "+EINT3 ":"-EINT3 ") : "",
				((prev ^ now) & 0x0100) ? ((now & 0x0100) ? "+TC1OI ":"-TC1OI ") : "",
				((prev ^ now) & 0x0200) ? ((now & 0x0200) ? "+TC2OI ":"-TC2OI ") : "",
				((prev ^ now) & 0x0400) ? ((now & 0x0400) ? "+RTCMI ":"-RTCMI ") : "",
				((prev ^ now) & 0x0800) ? ((now & 0x0800) ? "+TINT ":"-TINT ") : "",
				((prev ^ now) & 0x1000) ? ((now & 0x1000) ? "+UTXINT1 ":"-UTXINT1 ") : "",
				((prev ^ now) & 0x2000) ? ((now & 0x2000) ? "+URXINT1 ":"-URXINT1 ") : "",
				((prev ^ now) & 0x4000) ? ((now & 0x4000) ? "+UMSINT ":"-UMSINT ") : "",
				((prev ^ now) & 0x8000) ? ((now & 0x8000) ? "+SSEOTI ":"-SSEOTI ") : "");
		}
	} else if (reg == MEMCFG1) {
		log("MEMCFG1 write: 0x%08x -> 0x%08x  pc=%08x", memCfg1, value, getRealPC());
		memCfg1 = value;
	} else if (reg == MEMCFG2) {
		log("MEMCFG2 write: 0x%08x -> 0x%08x  pc=%08x", memCfg2, value, getRealPC());
		memCfg2 = value;
	} else if (reg == LCDCON) {
		// Throttle: Series 5 kernel writes LCDCON twice per ~1 sec
		// heartbeat with the same two values (0x5814e95f / 0x1814e4af).
		// Without throttling this floods the browser stderr.
		static uint32_t lastLcdcon0 = 0xDEADBEEF, lastLcdcon1 = 0xDEADBEEF;
		static int lcdReps = 0;
		if (value != lastLcdcon0 && value != lastLcdcon1) {
			log("LCD: ctl write %08x", value);
			lastLcdcon1 = lastLcdcon0;
			lastLcdcon0 = value;
		} else if (lcdReps < 4) {
			log("LCD: ctl write %08x", value);
			lcdReps++;
			if (lcdReps == 4)
				log("LCD: ctl write (further repeats throttled)");
		}
		lcdControl = value;
	} else if (reg == TC1D) {
		tc1.load(value);
		if (std::getenv("PSION_IRQ_TRACE"))
			log("TC1D load: %04x", value & 0xFFFF);
	} else if (reg == TC2D) {
		tc2.load(value);
		if (std::getenv("PSION_IRQ_TRACE"))
			log("TC2D load: %04x", value & 0xFFFF);
	} else if (reg == RTCDR) {
		if (std::getenv("PSION_S5_RTC_TRACE")) {
			log("writeReg32 RTCDR <- 0x%08x  pc=%08x lr=%08x  (prev rtc=0x%08x)",
			    value, getRealPC(), getGPR(14), rtc);
		}
		// Writes are stored but the next read refreshes from getRTC()
		// anyway (mirroring the Windermere RTCDRL/U behaviour), so any
		// kernel-side stamp gets immediately superseded by the real
		// host time. PSION_S5_RTC_KEEP=1 still works as a diagnostic
		// (forces our initial stored value to stick).
		if (!std::getenv("PSION_S5_RTC_KEEP")) {
			rtc = value;
		}
	} else if (reg == SYNCIO) {
		lastSyncioRequest = value & 0xFFFF;
		// CL-PS7110 datasheet section 1.2.9: writing SYNCIO starts an SSI
		// transfer; SSEOTI fires when the frame finishes. Real ADC1010
		// takes ~16 µs per conversion (~300 cycles at 18.432 MHz).
		//
		// PSION_S5_SSEOTI_DELAY=N (default = 300) — cycles from SYNCIO
		// write to SSEOTI assertion. 0 = fire immediately (legacy).
		// Some kernels poll SSI status and may not handle "instant
		// completion" correctly — Series 5 EPOC R1 likely expects the
		// real conversion delay.
		static int sseotiDelay = -1;
		if (sseotiDelay < 0) {
			const char *e = std::getenv("PSION_S5_SSEOTI_DELAY");
			sseotiDelay = e ? std::atoi(e) : 300;
		}
		if (sseotiDelay > 0) {
			sseotiFireAt = passedCycles + sseotiDelay;
			sseotiPending = true;
		} else {
			pendingInterrupts |= (1u << SSEOTI);
			sseotiPending = false;
		}
	} else if (reg == PALLSW) {
		lcdPalette &= 0xFFFFFFFF00000000;
		lcdPalette |= value;
	} else if (reg == PALMSW) {
		lcdPalette &= 0x00000000FFFFFFFF;
		lcdPalette |= (uint64_t)value << 32;
	} else if (reg == HALT) {
		halted = true;
	} else if (reg == BLEOI) {
		// Battery Low End of Interrupt — clears BLINT.
		pendingInterrupts &= ~(1 << BLINT);
	} else if (reg == MCEOI) {
		// Media Changed End of Interrupt — clears MCINT.
		pendingInterrupts &= ~(1 << MCINT);
	} else if (reg == STFCLR) {
		// Writing STFCLR clears the four start-up reason flags in SYSFLG1
		// (per CL-PS7111 datasheet section 5.26):
		//   bit 15 CLDFLG — cold start (power-on reset)
		//   bit 14 PFFLG  — power fail
		//   bit 13 RSTFLG — reset button (NURESET asserted)
		//   bit 12 NBFLG  — new battery (NBATCHG transition)
		// Previously we cleared only CLDFLG; the EPOC R1 kernel may also
		// dispatch on PFFLG/RSTFLG/NBFLG when deciding cold vs warm boot.
		sysFlg1 &= ~0x0000F000;
	} else if (reg == TEOI) {
		pendingInterrupts &= ~(1 << TINT);
		// WEINT (watchdog) is also cleared by TEOI per datasheet 5.12.
		pendingInterrupts &= ~(1 << WEINT);
	// TEOI = 0x418,
	// STFCLR = 0x41C,
	// E2EOI = 0x420,
	} else if (reg == TC1EOI) {
		pendingInterrupts &= ~(1 << TC1OI);
	} else if (reg == TC2EOI) {
		pendingInterrupts &= ~(1 << TC2OI);
	} else if (reg == RTCEOI) {
		// RTC Match End of Interrupt — clears RTCMI.
		pendingInterrupts &= ~(1 << RTCMI);
	} else if (reg == UMSEOI) {
		// UART Modem Status End of Interrupt — clears UMSINT.
		//
		// The UART model's own IntModemStatus latch has to be cleared
		// here too, not just the interrupt-controller bit. A cable plug
		// sets that latch once (serialAttachHost), and updateUartIrqs()
		// re-derives UMSINT from it — and that runs on every host poll,
		// 50 times a second. Clearing only pendingInterrupts therefore
		// re-raised the interrupt the instant the guest had acknowledged
		// it, and the driver got an endless stream of "the modem lines
		// changed" for a cable that changed state exactly once.
		//
		// EOI means "this edge is handled", and the latch is part of the
		// edge. Found while tracing the Geofox's link (it was not what
		// was breaking it — see modemLinesActiveLow() in clps7111.h for
		// what was), and it costs every CL-PS711x machine the same
		// spurious signalling for as long as a cable is attached.
		uart1.interrupts &= ~UART::IntModemStatus;
		pendingInterrupts &= ~(1 << UMSINT);
	} else if (reg == COEOI) {
		// Codec End of Interrupt — clears CSINT. AudioCodecModel resets
		// the per-cycle FIFO read counter so the next CSINT can surface
		// up to a full FIFO depth of mic samples.
		pendingInterrupts &= ~(1 << CSINT);
		audio.ackCsint();
	} else if (reg == PMPCON) {
		// Pump Power Control — Series 5 EPOC R1 LCD-pump init does many RMW
		// cycles that need read-back semantics. Without persisting the value
		// the kernel's "wait for bit X to clear" loops never terminate.
		pmpcon = value;
	} else if (reg == SYSCON2) {
		if (!chipHasSysCon2()) {
			log("PS7110: SYSCON2 write %08x ignored (reserved) pc=%08x", value, getRealPC());
		} else {
			log("SysCon2 write: %08x", value);
		}
	} else if (reg == INTMR2) {
		if (!chipHasSysCon2()) {
			log("PS7110: INTMR2 write %08x ignored (reserved) pc=%08x", value, getRealPC());
		} else {
			interruptMask &= 0xFFFF;
			interruptMask |= (value << 16);
		}
	} else if (reg == KBDEOI) {
		if (!chipHasSysCon2()) {
			log("PS7110: KBDEOI write ignored (reserved) pc=%08x", getRealPC());
		} else {
			pendingInterrupts &= ~(1 << KBDINT);
		}
	} else if (reg == 0xB00 || reg == 0xB04 || reg == 0xB08) {
		// SSI0 DMA descriptor pointers — accept silently. Firing SSEOTI
		// here was tried and didn't change kernel behaviour because
		// SSEOTI isn't unmasked in INTMR1 (the kernel only unmasks
		// TC2OI); the kernel must consume DMA completions via a
		// different signalling path we haven't identified yet.
		//
		// Log the first eight writes per session so a tester can see
		// whether the Osaris / Series 5 kernel is steering audio
		// through SSI DMA instead of polled CODR (the user-reported
		// "no beeps" symptom on those devices has no CODR-write
		// activity in the logs — if SSI DMA writes appear here while
		// CODR stays silent, that's the path to implement next).
		if (ssiDmaWritesLogged < 8) {
			log("SSI0 DMA write reg=%03x value=%08x pc=%08x lr=%08x",
			    reg, value, getRealPC(), getGPR(14));
			ssiDmaWritesLogged++;
		}
	} else if (reg == RTCMR) {
		// RTC match register — kernel sets target time for alarm.
		// We store but don't fire RTCMI based on it.
		rtcMatch = value;
	} else if (reg == CODR) {
		// 32-bit CODR write: push one signed-8-bit PCM sample into the
		// host DAC ring + virtual TX FIFO. AudioCodecModel does the
		// int16 expansion and FIFO bookkeeping in one place.
		audio.pushDacSample((int8_t)(value & 0xFF), passedCycles);
		codrValue = value;  // preserve readback for RMW loops
		if (codrWritesLogged < 8) {
			log("CODR write32 %08x pc=%08x lr=%08x", value,
			    getRealPC(), getGPR(14));
			codrWritesLogged++;
		}
	} else if (reg == UARTDR1) {
		// UART1 transmit data. With a host cable attached, enqueue the
		// byte to the bridge TX queue (drained by the host poll) and
		// refresh interrupt latches. Always keep uart1Data for readback.
		// When not attached, the byte is discarded (legacy stub).
		if (uart1.hostAttached) {
			uart1.pushTxByte((uint8_t)value);
			updateUartIrqs();
		}
		if (uartTraceBudget())
			log("UART1 TX write32 %02x (attached=%d) pc=%08x lr=%08x",
			    value & 0xFF, (int)uart1.hostAttached, getRealPC(), getGPR(14));
		uart1Data = value;
	} else if (reg == UBRLCR1) {
		// UART1 baud / line-control register.
		if (uartTraceOn())
			log("UART1 UBRLCR1 write32 %08x (brd=%u) pc=%08x lr=%08x",
			    value, value & 0xFFF, getRealPC(), getGPR(14));
		uart1LineCtl = value;
	} else if (reg == UARTDR2) {
		if (!chipHasSysCon2()) {
			log("PS7110: UARTDR2 write %08x ignored (reserved)", value);
		} else {
			uart2Data = value;
		}
	} else if (reg == UBRLCR2) {
		if (!chipHasSysCon2()) {
			log("PS7110: UBRLCR2 write %08x ignored (reserved)", value);
		} else {
			uart2LineCtl = value;
		}
	} else if (reg == STDBY) {
		// Writing to STDBY puts the CPU in standby. Treat as halt
		// (same as HALT register) — we don't model standby separately.
		halted = true;
	} else {
		log("RegWrite32 unknown:: pc=%08x reg=%03x value=%08x", getRealPC(), reg, value);
	}
}

// Region 4 has FOUR sub-apertures in the CL-PS7110/7111 PC-card map:
//   0x40000000-0x43FFFFFF  CF attribute memory (CIS + CCR window)
//   0x44000000-0x47FFFFFF  CF I/O space (ATA task-file registers) (7111)
//   0x48000000-0x4BFFFFFF  unused (CL-PS7111 datasheet says reserved)
//   0x4C000000-0x4FFFFFFF  CLPS7600 PCMCIA controller register window (7111)
// Region 5 is the PC card common-memory window (FAT sector transfers).
//
// Series 5 (CL-PS7110) has NO on-die CLPS7600, so the 0x4C-0x4F sub-aperture
// is repurposed by EPOC R1's CF driver as additional I/O windows. Empirical
// (PSION_CF_TRACE on Series 5 v1.01) shows the kernel splits the ATA task
// file across two bases:
//   0x4E000002/003/006/007/00E  → sector-count, LBA[7:0], drive, cmd/status, devctrl
//   0x4C000000 (V32)            → data register (256-word IDENTIFY drain)
//   0x4C000004/005              → LBA[15:8], LBA[23:16]
// They're really TWO views of the same task file — the chip's bus decode
// aliases. So when there's no CLPS7600 to claim 0x4C, treat 0x4C-0x4F AND
// 0x4E as CF I/O. The offset mask is 0xF (not 0x7) so both primary task
// file (0-7) and secondary (8-F: alt-status / devctrl) reach the card.
static inline bool isCFIOAddr(uint32_t physAddr, bool hasCLPS7600) {
	uint32_t hi = physAddr & 0xFF000000;
	if (hi == 0x44000000) return true;
	if (hi == 0x4E000000) return true;
	// Series 5: 0x4C-0x4F all CF I/O when no CLPS7600 claims 0x4C.
	if (!hasCLPS7600 && (hi == 0x4C000000 || hi == 0x4D000000 || hi == 0x4F000000))
		return true;
	return false;
}
static inline bool isCLPS7600RegisterAddr(uint32_t physAddr, bool hasCLPS7600) {
	return hasCLPS7600 && (physAddr & 0xFF000000) == 0x4C000000;
}

MaybeU32 Emulator::readPhysical(uint32_t physAddr, ValueSize valueSize) {
	// Diagnostic: log SoC-register-area reads (0x80000xxx and 0x58000xxx)
	// when in tap window — helps identify MMU mapping issues for SYNCIO.
	{
		static int probeLogged = 0;
		if (probeLogged < 50 &&
		    ((physAddr >= 0x80000000 && physAddr <= 0x80001FFF) ||
		     (physAddr >= 0x58000000 && physAddr <= 0x58001FFF) ||
		     (physAddr >= 0x88000000 && physAddr <= 0x88001FFF)) &&
		    (physAddr & 0xFFF) == 0x500) {
			log("SoC-reg readPhysical phys=0x%08x size=%d  pc=%08x lr=%08x",
			    physAddr, (int)valueSize, getRealPC(), getGPR(14));
			probeLogged++;
		}
	}
	uint8_t region = (physAddr >> 28);
	// Synthesise halfword reads from two byte reads. The MC218 CF path currently
	// goes through CLPS7600 register space + attribute memory; if a true 16-bit
	// I/O window is wired later, add a direct ataRead16() branch here.
	if (valueSize == V16) {
		MaybeU32 lo = readPhysical(physAddr, V8);
		MaybeU32 hi = readPhysical(physAddr + 1, V8);
		if (lo.has_value() && hi.has_value())
			return (lo.value() & 0xFF) | ((hi.value() & 0xFF) << 8);
		return {};
	}
	if (valueSize == V8) {
		if (region == 0)
			return ROM[physAddr & 0xFFFFFF];
		else if (region == 1)
			return ROM2[physAddr & 0x3FFFF];
		else if (region == 2) {
			// External chip-select nCS1 aperture (0x20000000-0x2FFFFFFF).
			// Delegate to the subclass virtual; fall through to 0x00 if it
			// doesn't claim the access (preserved behaviour for MC218/Osaris).
			MaybeU32 r = readRegion2(physAddr, V8);
			return r.has_value() ? r.value() : 0x00;
		}
		else if (region == 3) {
			// External chip-select nCS2 aperture (0x30000000-0x3FFFFFFF).
			// Unlike nCS1 there is no fall-through default: a device with
			// nothing wired here leaves the access unmapped, so the CPU
			// takes the same bus-error abort it took before this branch
			// existed. Only a subclass that claims the window changes
			// anything.
			return readRegion3(physAddr, V8);
		}
		else if (region == 4) {
			if (isCLPS7600RegisterAddr(physAddr, chipHasCLPS7600()))
				return pcCardController.read(physAddr & 0xFFFFFFF, V8);
			if (isCFIOAddr(physAddr, chipHasCLPS7600())) {
				uint8_t b = cfCard.ataRead8(physAddr & 0xF);
				static int cfIoLogs = 0;
				if (cfIoLogs++ < 50000 && std::getenv("PSION_CF_TRACE"))
					log("CF I/O read[V8]  phys=0x%08x reg=%u -> 0x%02x  pc=%08x lr=%08x",
					    physAddr, physAddr & 0xF, b, getRealPC(), getGPR(14));
				return b;
			}
			uint8_t b = cfCard.readAttributeByte(physAddr & 0xFFFFFF);
			static int cfAttrLogs = 0;
			if (cfAttrLogs++ < 1000 && std::getenv("PSION_CF_TRACE"))
				log("CF attribute read[V8] phys=0x%08x -> 0x%02x  pc=%08x lr=%08x",
				    physAddr, b, getRealPC(), getGPR(14));
			return b;
		}
		else if (region == 5) {
			uint8_t b = cfCard.readByte(physAddr & 0xFFFFFF);
			static int cfDataLogs = 0;
			if (cfDataLogs++ < 500 && std::getenv("PSION_CF_TRACE"))
				log("CF data read[V8] phys=0x%08x -> 0x%02x  pc=%08x lr=%08x",
				    physAddr, b, getRealPC(), getGPR(14));
			return b;
		}
		else if (region == 6) {
			// PS7111 has 2 KB of on-chip SRAM at CS6; PS7110 (Series 5) has
			// nothing wired here (per CL-PS7110 datasheet Table 3-1, CS6 is
			// just a generic external chip-select). Series 5 hardware
			// schematics show no chip on CS6, so return open-bus 0xFF.
			if (!chipHasOnChipSRAM())
				return 0xFF;
			return OnChipSRAM[physAddr & 0x7FF];
		}
		else if (region == 8 && physAddr <= 0x80001FFF)
			return readReg8(physAddr & 0x1FFF);
		else if (region == 0xC)
			return MemoryBlockC0[physAddr & getRamMask()];
		else if (region == 0xD && aliasDRegionToRam())
			return MemoryBlockC0[(physAddr & getRamMask()) + getRegionDRamOffset()];
		else if (region > 0xC)
			return 0xFF; // just throw accesses to unmapped RAM away
	} else {
		uint32_t result;
		if (region == 0)
			LOAD_32LE(result, physAddr & 0xFFFFFF, ROM);
		else if (region == 1)
			LOAD_32LE(result, physAddr & 0x3FFFF, ROM2);
		else if (region == 2) {
			// See V8 branch comment. Word-sized reads of region 2 go through
			// the same virtual so subclasses can treat 8/16/32 uniformly.
			MaybeU32 r = readRegion2(physAddr, V32);
			return r.has_value() ? r.value() : 0x00000000;
		}
		else if (region == 3) {
			// See the V8 branch: unclaimed nCS2 stays unmapped.
			return readRegion3(physAddr, V32);
		}
		else if (region == 4) {
			if (isCLPS7600RegisterAddr(physAddr, chipHasCLPS7600()))
				result = pcCardController.read(physAddr & 0xFFFFFFF, V32);
			else if (isCFIOAddr(physAddr, chipHasCLPS7600())) {
				// 32-bit reads of the CF I/O window: ATA reg 0 (Data) is
				// 16-bit on real CF hardware. Treat 32-bit reads as a
				// 16-bit data read followed by 0xFFFF in the upper half.
				if ((physAddr & 0xF) == 0)
					result = (uint32_t)cfCard.ataRead16() | 0xFFFF0000u;
				else
					result = (uint32_t)cfCard.ataRead8(physAddr & 0xF) | 0xFFFFFF00u;
			}
			else {
				uint32_t off = physAddr & 0xFFFFFF;
				result = (uint32_t)cfCard.readAttributeByte(off)
				       | ((uint32_t)cfCard.readAttributeByte(off + 1) << 8)
				       | ((uint32_t)cfCard.readAttributeByte(off + 2) << 16)
				       | ((uint32_t)cfCard.readAttributeByte(off + 3) << 24);
			}
		}
		else if (region == 5) {
			static int region5Logged = 0;
			if (region5Logged < 20) {
				log("readReg32 region5 phys=0x%08x (cfCard) pc=%08x lr=%08x",
				    physAddr, getRealPC(), getGPR(14));
				region5Logged++;
			}
			result = cfCard.readWord(physAddr & 0xFFFFFF);
		}
		else if (region == 6) {
			if (!chipHasOnChipSRAM())
				return 0xFFFFFFFFu;
			LOAD_32LE(result, physAddr & 0x7FF, OnChipSRAM);
		}
		else if (region == 8 && physAddr <= 0x80001FFF)
			result = readReg32(physAddr & 0x1FFF);
		else if (region == 0xC)
			LOAD_32LE(result, physAddr & getRamMask(), MemoryBlockC0);
		else if (region == 0xD && aliasDRegionToRam())
			LOAD_32LE(result, (physAddr & getRamMask()) + getRegionDRamOffset(), MemoryBlockC0);
		else if (region > 0xC)
			return 0xFFFFFFFF; // just throw accesses to unmapped RAM away
		else
			return {};
		return result;
	}

	return {};
}

bool Emulator::writePhysical(uint32_t value, uint32_t physAddr, ValueSize valueSize) {
	uint8_t region = (physAddr >> 28);
	if (valueSize == V16) {
		bool okLo = writePhysical(value & 0xFF, physAddr, V8);
		bool okHi = writePhysical((value >> 8) & 0xFF, physAddr + 1, V8);
		return okLo && okHi;
	}
	if (valueSize == V8) {
		if (region == 0xC)
			MemoryBlockC0[physAddr & getRamMask()] = (uint8_t)value;
		else if (region == 0xD && aliasDRegionToRam())
			MemoryBlockC0[(physAddr & getRamMask()) + getRegionDRamOffset()] = (uint8_t)value;
		else if (region > 0xC)
			return true; // just throw accesses to unmapped RAM away
		else if (region == 2) {
			// External chip-select nCS1: delegate to subclass.
			writeRegion2(value, physAddr, V8);
			return true;  // always swallow unknown nCS1 writes
		}
		else if (region == 3) {
			// External chip-select nCS2: a subclass that claims the write
			// handles it; anything else stays unmapped and faults, as it
			// did before the window existed.
			return writeRegion3(value, physAddr, V8);
		}
		else if (region == 4) {
			if (isCLPS7600RegisterAddr(physAddr, chipHasCLPS7600()))
				pcCardController.write(value, physAddr & 0xFFFFFFF, V8);
			else if (isCFIOAddr(physAddr, chipHasCLPS7600())) {
				cfCard.ataWrite8(physAddr & 0xF, (uint8_t)value);
				static int cfIoWrLogs = 0;
				if (cfIoWrLogs++ < 1000 && std::getenv("PSION_CF_TRACE"))
					log("CF I/O write[V8] phys=0x%08x reg=%u <- 0x%02x  pc=%08x lr=%08x",
					    physAddr, physAddr & 0xF, (uint8_t)value, getRealPC(), getGPR(14));
			}
			else {
				// Most CF attribute memory is read-only CIS, but the
				// Card Configuration Registers (CCR) sit in this window
				// at offset 0x200 / 0x202 / 0x204 / 0x206 and the host
				// writes them to enable I/O mode + IRQ on the card.
				// Previously these writes were silently dropped, leaving
				// the card stuck in memory-only mode and forcing EPOC's
				// PCCARD-ARM driver into an infinite "retry config" loop.
				cfCard.writeAttributeByte(physAddr & 0xFFFFFF, (uint8_t)value);
				static int cfAttrWrLogs = 0;
				if (cfAttrWrLogs++ < 500 && std::getenv("PSION_CF_TRACE"))
					log("CF attribute write[V8] phys=0x%08x <- 0x%02x  pc=%08x lr=%08x",
					    physAddr, (uint8_t)value, getRealPC(), getGPR(14));
			}
		}
		else if (region == 5)
			cfCard.writeByte(physAddr & 0xFFFFFF, (uint8_t)value);
		else if (region == 6) {
			// PS7111: writes go to on-chip SRAM. PS7110: open-bus, swallow.
			if (chipHasOnChipSRAM())
				OnChipSRAM[physAddr & 0x7FF] = (uint8_t)value;
		}
		else if (region == 8 && physAddr <= 0x80001FFF)
			writeReg8(physAddr & 0x1FFF, value);
		else
			return false;
	} else {
		if (region == 0xC)
			STORE_32LE(value, physAddr & getRamMask(), MemoryBlockC0);
		else if (region == 0xD && aliasDRegionToRam())
			STORE_32LE(value, (physAddr & getRamMask()) + getRegionDRamOffset(), MemoryBlockC0);
		else if (region > 0xC)
			return true; // just throw accesses to unmapped RAM away
		else if (region == 2) {
			writeRegion2(value, physAddr, V32);
			return true;
		}
		else if (region == 3) {
			return writeRegion3(value, physAddr, V32);
		}
		else if (region == 4) {
			if (isCLPS7600RegisterAddr(physAddr, chipHasCLPS7600()))
				pcCardController.write(value, physAddr & 0xFFFFFFF, V32);
			else if (isCFIOAddr(physAddr, chipHasCLPS7600())) {
				// 32-bit writes to CF I/O Data register go via 16-bit ATA write.
				if ((physAddr & 0xF) == 0)
					cfCard.ataWrite16((uint16_t)value);
				else
					cfCard.ataWrite8(physAddr & 0xF, (uint8_t)value);
			}
			else {
				// 32-bit attribute writes split into 4 single-byte CCR writes
				// (the CF CCR is byte-addressed; the chip strobes one byte per
				// attribute access on real hardware).
				cfCard.writeAttributeByte((physAddr    ) & 0xFFFFFF, (uint8_t)(value      ));
				cfCard.writeAttributeByte((physAddr + 1) & 0xFFFFFF, (uint8_t)(value >>  8));
				cfCard.writeAttributeByte((physAddr + 2) & 0xFFFFFF, (uint8_t)(value >> 16));
				cfCard.writeAttributeByte((physAddr + 3) & 0xFFFFFF, (uint8_t)(value >> 24));
			}
		}
		else if (region == 5)
			cfCard.writeWord(physAddr & 0xFFFFFF, value);
		else if (region == 6) {
			if (chipHasOnChipSRAM())
				STORE_32LE(value, physAddr & 0x7FF, OnChipSRAM);
		}
		else if (region == 8 && physAddr <= 0x80001FFF)
			writeReg32(physAddr & 0x1FFF, value);
		else
			return false;
	}
	return true;
}



void Emulator::writeAudioInput(const int16_t *src, size_t count) {
	// Mic-recording path for Series 5 (EPOC R1) and Osaris (EPOC R5).
	// CL-PS7110/7111 doesn't have Windermere's per-channel CONFG=3
	// signal; instead the kernel sets SYSCON1 bit 14 (CDENRX) to enable
	// the codec's RX FIFO and registers a CSINT handler. We drop samples
	// whenever:
	//   * the host mic toggle is off (no live input to deliver), or
	//   * the kernel hasn't enabled CDENRX (no recording session, no
	//     handler — asserting CSINT here would spin the kernel IRQ
	//     dispatcher on an empty slot).
	if (!audio.hostMicEnabled()) return;
	if (!codecRxEnabled()) return;
	if (audio.enqueueMicSamples(src, count)) {
		// Empty→non-empty edge: wake the kernel's CSINT handler.
		pendingInterrupts |= (1u << CSINT);
	}
}

void Emulator::configure() {
	if (configured) return;
	configured = true;

	// Configure the codec FIFO model's timing for our SoC clock so the
	// virtual TX drain rate and deferred-CSINT cadence land at the
	// real 8 kHz sample rate.
	audio.configure(CLOCK_SPEED);

	srand(1000);

	// PSION_RAM_FILL=NN — explicitly fill MemoryBlockC0 (DRAM) with byte
	// 0xNN at boot. Real PS7110 DRAM at power-on is INDETERMINATE (random
	// bit pattern). Some EPOC R1 boot paths read uninitialised memory
	// before the kernel zeros it; the value seen there can affect boot
	// path. Default behaviour (no env var): leave RAM at whatever the
	// allocator gave us (typically 0 from BSS).
	if (const char *fillStr = std::getenv("PSION_RAM_FILL")) {
		uint8_t fill = (uint8_t)std::strtoul(fillStr, nullptr, 0);
		memset(MemoryBlockC0, fill, sizeof(MemoryBlockC0));
		log("RAM: filled MemoryBlockC0 with 0x%02x via PSION_RAM_FILL", fill);
	}

	memset(&tc1, 0, sizeof(tc1));
	memset(&tc2, 0, sizeof(tc1));
	tc1.clockSpeed = CLOCK_SPEED;
	tc2.clockSpeed = CLOCK_SPEED;
	// Real CL-PS7110/CL-PS7111 timers are always running — the SYSCON1
	// bits pick the clock source and periodic vs one-shot, they don't
	// gate the counter itself. Enable both timers at reset so ROMs that
	// expect TC2 to tick before any SYSCON1 write (Series 5 EPOC R1
	// polls INTSR1 bit 9 in a tight loop at 0x1b420 waiting for the
	// first TC2OI fire) still make progress.
	tc1.config = Timer::ENABLED;
	tc2.config = Timer::ENABLED;

	nextTickAt = TICK_INTERVAL;
	tc1.nextTickAt = tc1.tickInterval();
	tc2.nextTickAt = tc2.tickInterval();
	rtc = getRTC();

	applyDeviceQuirks();

	reset();
}

uint8_t *Emulator::getROMBuffer() {
	return ROM;
}
size_t Emulator::getROMSize() {
	return sizeof(ROM);
}
void Emulator::loadROM(uint8_t *buffer, size_t size) {
	// Real flash/ROM defaults to 0xFF for unwritten cells / missing
	// chips. The Series 5 has a removable "personality module" with
	// TWO ROM chips (base OS + language pack); the .bin we have is the
	// 6 MB base only — the missing 2 MB language area must read 0xFF
	// like real hardware, NOT 0x00 (which kernel might misinterpret as
	// a valid empty string or null pointer).
	memset(ROM, 0xFF, sizeof(ROM));
	memcpy(ROM, buffer, std::min(size, sizeof(ROM)));

	// CF accel-timer site detection. The PCCARD-ATA retry callback on
	// Osaris arms a 2,000,000 µs (2 s) NTimer between sectors as a
	// safety net for when the CF IRQ→DFC wake fails. In our emulator
	// the IRQ→DFC path has a fidelity gap so the kernel waits out the
	// full 2 s per sector. We intercept the shared NTimer-arm
	// trampoline and rewrite r1 from 2,000,000 to kCfAccelTimerUs.
	//
	// The Osaris CF driver has FOUR retry-arm sites (PCs 0x50080714,
	// 0x500807b8, 0x50081384, 0x50081434) that all route through the
	// same trampoline at 0x500823a0 with r1=0x001e8480 and
	// r2=0x50081318 (the per-sector retry callback). We intercept once
	// at the trampoline and guard on (r1==2M && r2==callback) so other
	// timer arms are untouched — mirrors the 5mx pattern in
	// windermere.cpp ~line 1745.
	//
	// Variant table maps each known ROM to (callback, trampoline). The
	// earlier site-based scan at 0x50237ec4/0x50238194 was a different
	// driver layer that the kernel never actually reaches during CF
	// drain; those sites are dropped.
	struct CfVariant { uint32_t callback; uint32_t trampoline; };
	static constexpr CfVariant kVariants[] = {
		{0x50081318, 0x500823a0},  // Osaris v1.02(209)
	};
	auto rom32 = [&](uint32_t romOff) -> uint32_t {
		if (romOff + 4 > sizeof(ROM)) return 0;
		return (uint32_t)ROM[romOff] | ((uint32_t)ROM[romOff+1] << 8) |
		       ((uint32_t)ROM[romOff+2] << 16) | ((uint32_t)ROM[romOff+3] << 24);
	};
	cfRomTimerArmPC1 = 0;
	cfRomTimerArmPC2 = 0;
	for (auto &v : kVariants) {
		// Confirm variant by checking that ROM contains the prologue
		// `push {r4-r6, lr}; sub sp, sp, #8` at the callback address —
		// this is the standard PCCARD-ATA retry-callback prologue used
		// across the EPOC R1 family.
		uint32_t cbOff = v.callback - 0x50000000;
		if (rom32(cbOff) == 0xe92d4070 && rom32(cbOff + 4) == 0xe24dd008) {
			cfRomTimerArmPC1 = v.trampoline;
			cfRomTimerArmPC2 = v.callback;  // re-purposed: callback addr (gate)
			break;
		}
	}
	log("CF accel-timer probe: trampoline=%08x callback=%08x",
	    cfRomTimerArmPC1, cfRomTimerArmPC2);
}

void Emulator::executeUntil(int64_t cycles) {
	if (!configured)
		configure();

	// PSION_TICK_INTERVAL_MULT=N — scale the kernel-tick interval. Default 1x
	// matches real hw (64Hz). Larger = slower IRQ rate (kernel has longer
	// uninterrupted windows). Smaller = faster IRQ rate.
	// PSION_FIRST_IRQ_DELAY=N — minimum cycles before first IRQ fires.
	// Default is the device's defaultFirstIrqDelay() (Series 5: 10M cyc;
	// PS7111: 0). Env var overrides.
	//
	// The env-var lookups are cached (env doesn't change during a run) but
	// firstIrqDelay must recompute defaultFirstIrqDelay() each call when no
	// env override is set — otherwise switching between devices in the same
	// process (e.g. browser: Series 5 → Osaris) leaks the previous device's
	// 10M-cycle delay into the next device's boot, breaking Osaris (apps
	// render as "Corrupt") because PS7111-family devices need IRQs enabled
	// from cycle 0.
	static int tickMultCached = -1;
	static int64_t firstIrqDelayOverride = -1;  // -1 = no env override
	static int cfIrqBitCached = -1;             // -1 = disabled / unset
	static int cfPcTraceCached = 0;
	if (tickMultCached < 0) {
		const char *e = std::getenv("PSION_TICK_INTERVAL_MULT");
		tickMultCached = e ? std::max(1, std::atoi(e)) : 1;
		const char *d = std::getenv("PSION_FIRST_IRQ_DELAY");
		firstIrqDelayOverride = d ? (int64_t)std::strtoull(d, nullptr, 0) : -1;
		if (const char *c = std::getenv("PSION_CF_IRQ_LINE")) {
			cfIrqBitCached = !strcmp(c, "eint1") ? EINT1
			               : !strcmp(c, "eint2") ? EINT2
			               : !strcmp(c, "eint3") ? EINT3
			               : !strcmp(c, "mcint") ? MCINT
			               : -1;
		}
		cfPcTraceCached = std::getenv("PSION_CF_PC_TRACE") != nullptr;
	}
	int tickMult = tickMultCached;
	uint64_t firstIrqDelay = (firstIrqDelayOverride >= 0)
	    ? (uint64_t)firstIrqDelayOverride
	    : defaultFirstIrqDelay();
	int64_t scaledTick = (int64_t)TICK_INTERVAL * tickMult;

	while (!asleep && passedCycles < cycles) {
		if (passedCycles >= nextTickAt) {
			// increment RTCDIV
			if (rtcDiv == 0x3F) {
				rtc++;
				rtcDiv = 0;
			} else {
				rtcDiv++;
			}

			nextTickAt += scaledTick;
			// 64-Hz tick. We model this as a "WEINT fires when TINT was not
			// serviced by the next edge" watchdog (per CL-PS7111 datasheet
			// 5.12, both cleared via TEOI) — but the EPOC R1 kernel on these
			// parts does NOT drive its system tick off TINT: it leaves TINT
			// masked and never writes TEOI, so by that rule TINT is "unserviced"
			// at *every* edge and WEINT would be pending continuously. That is
			// normally harmless because the kernel keeps WEINT masked too — but
			// the IR transfer teardown path runs an IRQ-disabled busy-wait (a
			// free-running-TC2 delay loop) during which it briefly enables the
			// FIQ source, and the perpetually-pending spurious WEINT then
			// dispatches as a watchdog FIQ and reboots the device the instant a
			// file finishes beaming. The watchdog can't actually distinguish a
			// wedge from normal operation here (TINT is always "unserviced"), so
			// it only ever produces this false positive. Don't raise it — this
			// matches Windermere/5mx, which models no such tick-watchdog and is
			// unaffected. (TEOI still clears WEINT below in case the kernel ever
			// asserts it via another path.)
			pendingInterrupts |= (1<<TINT);

			// Buzzer synthesiser. Per-device opt-in via enableBuzzerPump():
			// only Series 5 routes its EPOC R1 click feedback through the
			// CL-PS7110 buzzer pin in practice. Osaris and MC218 ran
			// noisy on this code path (kernel writes BZMOD=1 during init
			// for unrelated reasons; we'd emit a sustained tone) so they
			// keep the pump disabled and rely on their existing CODR
			// routing instead. The SYSCON1 BZTOG / BZMOD bits are still
			// tracked above for both chip variants, but the pump only
			// runs here when a subclass opts in.
			if (enableBuzzerPump()) {
				// EPOC R1 (Series 5) / R5 (Osaris) drive click / beep
				// feedback through the CL-PS7110/7111 buzzer pin via
				// SYSCON1 bits 9 (BZTOG) and 10 (BZMOD). BZMOD=0 routes
				// BZTOG straight to the pin; BZMOD=1 auto-toggles on
				// TC1 underflow. The pump itself lives in
				// AudioCodecModel; here we just translate the device-
				// specific register bits into "active" and "toneHz".
				bool bzActive = false;
				int toneHz = 0;
				if (!buzzerBzMod) {
					// BZMOD=0 manual mode: real piezo only sounds on
					// the BZTOG edge (mechanical/capacitive click),
					// NOT while the pin is held high. A steady
					// BZTOG=1 is a DC pin level — silent on hardware.
					// Only the click-hold latch (set on the rising
					// edge by noteBuzzerRisingEdge) emits audio here.
					// This silences the user-visible Osaris "constant
					// beep when speaker enabled" symptom that came
					// from the kernel setting BZTOG=1 at boot and
					// never clearing it.
					bzActive = audio.buzzerHolding();
				} else if (buzzerBzTog) {
					// TC1-driven mode: gate on BZTOG too, otherwise
					// the brief BZMOD=1 phase during early boot pumps
					// a continuous tone into the host speaker.
					bzActive = (tc1.interval > 0);
					if (tc1.interval > 0) {
						int tickRate = (tc1.config & Timer::MODE_512KHZ) ? 512000 : 2000;
						int interval = (int)tc1.interval;
						int underflowHz = tickRate / interval;
						int tc1Hz = underflowHz / 2;
						if (tc1Hz >= 50 && tc1Hz <= 4000) toneHz = tc1Hz;
					}
				}
				audio.emitBuzzerSamples(bzActive, toneHz);
			}
		}
		if (tc1.tick(passedCycles))
			pendingInterrupts |= (1<<TC1OI);
		if (tc2.tick(passedCycles))
			pendingInterrupts |= (1<<TC2OI);

		// Mic-disabled record fallback. When the kernel has enabled
		// CDENRX (recording) but the host mic isn't granted,
		// writeAudioInput drops all input samples and the kernel's
		// drain loop runs against an empty ADC ring — historically a
		// recipe for kernel-side crashes mirroring the Windermere
		// "5mx record crash" symptom. Synthesise silence at the codec
		// rate so the drain loop completes normally with zero-valued
		// samples and the kernel state machine stays sane. The user
		// gets a silent recording, no crash.
		if (codecRxEnabled() && !audio.hostMicEnabled()) {
			constexpr int kSamplesPerTickRx = AudioCodecModel::kAudioSampleRate / 64;
			if (audio.adcRingFill() < (size_t)kSamplesPerTickRx) {
				static const int16_t kSilence[kSamplesPerTickRx] = {};
				if (audio.enqueueMicSamples(kSilence, kSamplesPerTickRx)) {
					pendingInterrupts |= (1u << CSINT);
				}
			}
		}

		// Deferred codec CSINT re-fire. When a kernel CODR-drain cycle
		// hit the 16-sample hardware-FIFO cap with samples still queued,
		// AudioCodecModel scheduled a re-fire ~3 codec frames later.
		// Fire it here AFTER the drain loop has fully returned so the
		// kernel's stack-local drain buffer isn't overrun by a re-entry
		// mid-loop.
		if (audio.tickDeferredCsint(passedCycles) && codecRxEnabled()) {
			pendingInterrupts |= (1u << CSINT);
		}

		// Virtual TX FIFO drain. AudioCodecModel returns true the tick
		// the level crosses below kTxFifoWatermark; we fire CSINT only
		// when the kernel has CDENTX set so we don't poke an
		// unregistered handler during boot.
		if (audio.tickTxFifoDrain(passedCycles) && codecTxEnabled() &&
		    (pendingInterrupts & (1u << CSINT)) == 0) {
			pendingInterrupts |= (1u << CSINT);
		}

		// Deferred SSEOTI: fire when conversion completes (real ADC1010
		// takes ~16 µs / 300 cycles per conversion). Reading SYNCIO
		// clears SSEOTI; if pending and not yet read by the kernel,
		// the bit gets set here.
		if (sseotiPending && passedCycles >= sseotiFireAt) {
			pendingInterrupts |= (1u << SSEOTI);
			sseotiPending = false;
		}

		// CF IREQ# → CLPS7600 RDY input. PCCARD-ARM polls CLPS7600
		// Interrupt Status bit 10 (RDY_CHG) after each ATA cmd to
		// detect "data ready to drain" / "command complete" — even
		// without the SoC-level IRQ line being routed. So we always
		// latch the rising edge into the CLPS7600 so polling drivers
		// (Series 5 EPOC R1 especially) advance past the post-cmd
		// wait. The chain into pendingInterrupts is still gated by
		// PSION_CF_IRQ_LINE because that path is the experimental
		// route via EINT* / MCINT — it can perturb non-CF code paths.
		pcCardController.setCardIreq(cfCard.irqAsserted());
		if (cfIrqBitCached >= 0) {
			if (pcCardController.irqAsserted())
				pendingInterrupts |= (1u << cfIrqBitCached);
			else
				pendingInterrupts &= ~(1u << cfIrqBitCached);
		}

		bool irqsAllowed = (passedCycles >= (int64_t)firstIrqDelay);
		if (irqsAllowed && (pendingInterrupts & interruptMask & FIQ_INTERRUPTS) != 0 && canAcceptFIQ()) {
			requestFIQ();
			halted = false;
		}
		if (irqsAllowed && (pendingInterrupts & interruptMask & IRQ_INTERRUPTS) != 0 && canAcceptIRQ()) {
			requestIRQ();
			halted = false;
		}

		// Subclass hook for low-cost periodic kernel-state polling. Series 5
		// uses this to track EKA1 boot progression (iCurrentThread,
		// iRescheduleNeededFlag, NThread iNState) without the per-instruction
		// trace overhead.
		pollKernelState();

		// CF accel-timer: rewrite the kernel's 2-second sector-retry
		// timeout to ~100 µs at the BL site. PCCARD-ATA's retry callback
		// loads r1=0x1e8480 (=2,000,000 µs) and immediately BLs to the
		// NTimer-arm function. We intercept right after the LDR (when
		// r1 is set, before the BL fires) and overwrite r1.
		// PSION_CF_ACCEL_TIMER_OFF=1 disables the rewrite for diagnostics.
		// Always step the CF card's irq-assert delay down (was previously
		// only stepped when PSION_CF_IRQ_LINE was set). The 1000-cycle
		// kIrqAssertDelay needs to actually decrement for cfCard.irqAsserted()
		// to ever return true, and that signal is consumed by the subclass
		// tickCfBridge() hook below (Series 5 routes it through ETNA).
		if (cfCard.inserted()) cfCard.tickIrqDelay(1);

		// Subclass CF-bridge hook (Series 5 wires CF IREQ# → ETNA →
		// EINT3 here, matching the 5mx Windermere flow). Default no-op
		// keeps Osaris on the original CLPS7600-only path.
		tickCfBridge();

		// PSION_CF_PC_TRACE=1 — log distinct PCs in the CF driver
		// address range (0x50080000-0x50090000) while a card is
		// inserted. Capped at 200 unique PCs. Used to find what
		// kernel code path is active during the per-sector wait.
		if (cfCard.inserted() && cfPcTraceCached) {
			uint32_t pc = (uint32_t)cpu.getRealPC();
			if (pc >= 0x50080000 && pc < 0x50090000) {
				static int pcSamples = 0;
				static uint32_t lastLogged = 0;
				if (pcSamples < 200 && pc != lastLogged) {
					pcSamples++;
					lastLogged = pc;
					log("CF PC sample #%d pc=%08x r1=%08x lr=%08x",
					    pcSamples, pc, cpu.getGPR(1), cpu.getGPR(14));
				}
			}
		}

		// CF accel-timer rewrite — port of the 5mx fix to CL-PS7111.
		// Background: on 5mx the kernel's PCCARD-ATA retry callback
		// arms a 2,000,000 µs (2 s) safety-net NTimer per sector. The
		// IRQ→DFC wake path has a fidelity gap in our emulator so the
		// kernel never sees the "data ready" interrupt and waits out
		// the full timer. Windermere intercepts the BL site (verified
		// PCs in Variant table) and rewrites r1 from 2,000,000 to
		// kCfAccelTimerUs (= 100 µs), turning 2 s/sector into ~1 tick
		// per sector. See windermere.cpp ~ line 1745.
		//
		// On Osaris, the same 2 s gap is observed in the per-sector
		// drain trace. Four ldr-r1=0x001e8480 sites in the PCCARD-ATA
		// driver (0x50080714, 0x500807b8, 0x50081384, 0x50081434) all
		// route through a shared NTimer-arm trampoline at 0x500823a0
		// with r2=0x50081318 (the per-sector retry callback). We
		// intercept once at the trampoline PC and gate on
		// (r1==2,000,000 && r2==callback) so unrelated timer arms are
		// untouched. cfRomTimerArmPC1 holds the trampoline PC,
		// cfRomTimerArmPC2 holds the callback address used as the gate.
		//
		// PSION_CF_ACCEL_TIMER_OFF=1 disables the rewrite for diagnostics.
		if (cfAccelTimer && cfCard.inserted() &&
		    cfRomTimerArmPC1 != 0 && cfRomTimerArmPC2 != 0) {
			uint32_t pc = (uint32_t)cpu.getRealPC();
			if (pc == cfRomTimerArmPC1 &&
			    cpu.getGPR(1) == 0x001e8480u &&
			    cpu.getGPR(2) == cfRomTimerArmPC2 &&
			    !std::getenv("PSION_CF_ACCEL_TIMER_OFF")) {
				cpu.setGPR(1, kCfAccelTimerUs);
				cfAccelTimerHits++;
				if (cfAccelTimerHits <= 8 || (cfAccelTimerHits & 0xFF) == 0)
					log("CF accel-timer hit #%u at trampoline=%08x: r1 2,000,000 → %u µs",
					    cfAccelTimerHits, pc, kCfAccelTimerUs);
			}
		}

		// what's running?
		if (halted) {
			// Idle fast-forward. A halted CPU can only be woken by an
			// interrupt, and every interrupt source serviced by this loop
			// fires at a computable future cycle: the 64 Hz tick
			// (nextTickAt), the TC1/TC2 prescaler edges (their own
			// nextTickAt — we stop at the *edge*, not the underflow, so
			// Timer::tick() semantics are untouched), and a pending
			// SYNCIO conversion (sseotiFireAt). Jump straight to the
			// nearest of those instead of running the full peripheral
			// loop once per cycle — post-boot idle goes from dominating
			// host CPU time to near-free.
			//
			// The audio codec is the one subsystem with sub-edge timing
			// (deferred CSINT re-fires, TX FIFO drain), so while it is
			// enabled we keep the original cycle-by-cycle behaviour.
			// The firstIrqDelay boundary is honoured so the skip can't
			// jump over the moment IRQs first become deliverable.
			int64_t step = 1;
			if (!codecRxEnabled() && !codecTxEnabled()) {
				int64_t target = cycles;
				if (nextTickAt < target) target = nextTickAt;
				if (tc1.nextTickAt < target) target = tc1.nextTickAt;
				if (tc2.nextTickAt < target) target = tc2.nextTickAt;
				if (sseotiPending && sseotiFireAt < target) target = sseotiFireAt;
				if (passedCycles < (int64_t)firstIrqDelay && (int64_t)firstIrqDelay < target)
					target = (int64_t)firstIrqDelay;
				if (target > passedCycles + 1)
					step = target - passedCycles;
			}
			// The per-iteration cfCard.tickIrqDelay(1) above covered one
			// cycle of the card's IRQ-assert countdown; account for the
			// remainder of the skip so an in-flight CF interrupt still
			// asserts after the same number of emulated cycles.
			if (step > 1 && cfCard.inserted())
				cfCard.tickIrqDelay((int)std::min<int64_t>(step - 1, 1 << 30));
			passedCycles += step;
		} else {
			// instructionReady() is a cheap flag check; test it before
			// paying for the PC translation rather than after.
			if (instructionReady()) {
				if (auto v = virtToPhys(getGPR(15) - 0xC); v.has_value())
					debugPC(v.value());
			}
			passedCycles += tick();

			uint32_t new_pc = getGPR(15) - 0xC;
#ifndef __EMSCRIPTEN__
			if (_breakpoints.find(new_pc) != _breakpoints.end()) {
				log("⚠️ Breakpoint triggered at %08x!", new_pc);
				return;
			}
#endif
			if (new_pc >= 0x80000000 && new_pc <= 0x90000000) {
				// PSION_S5_NO_BAD_PC_TRAP=1 disables the early-termination
				// safety check, useful for sweep harnesses that want to see
				// if the kernel recovers via abort/undef handler chain.
				static int noTrap = -1;
				if (noTrap < 0) {
					const char *e = std::getenv("PSION_S5_NO_BAD_PC_TRAP");
					noTrap = (e && e[0] == '1') ? 1 : 0;
				}
				log("BAD PC %08x!!", new_pc);
				logPcHistory();
				if (!noTrap) return;
			}
		}
	}
}


const char *Emulator::identifyObjectCon(uint32_t ptr) {
	if (ptr == readVirtualDebug(0x80000880, V32).value()) return "process";
	if (ptr == readVirtualDebug(0x80000884, V32).value()) return "thread";
	if (ptr == readVirtualDebug(0x80000888, V32).value()) return "chunk";
//	if (ptr == readVirtualDebug(0x8000088C, V32).value()) return "semaphore";
//	if (ptr == readVirtualDebug(0x80000890, V32).value()) return "mutex";
	if (ptr == readVirtualDebug(0x80000894, V32).value()) return "logicaldevice";
	if (ptr == readVirtualDebug(0x80000898, V32).value()) return "physicaldevice";
	if (ptr == readVirtualDebug(0x8000089C, V32).value()) return "channel";
	if (ptr == readVirtualDebug(0x800008A0, V32).value()) return "server";
//	if (ptr == readVirtualDebug(0x800008A4, V32).value()) return "unk8A4"; // name always null
	if (ptr == readVirtualDebug(0x800008AC, V32).value()) return "library";
//	if (ptr == readVirtualDebug(0x800008B0, V32).value()) return "unk8B0"; // name always null
//	if (ptr == readVirtualDebug(0x800008B4, V32).value()) return "unk8B4"; // name always null
	return nullptr;
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
	if (!hasOsarisDebugHooks()) return;
	char objName[1000];
	if (pc == 0x32304) {
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

	if (pc == 0x634) {
		uint32_t virtAddr = getGPR(0);
		uint32_t physAddr = getGPR(1);
		uint32_t btIndex = getGPR(2);
		uint32_t regionSize = getGPR(3);
		log("KERNEL MMU SECTION: v:%08x p:%08x size:%08x idx:%02x",
			virtAddr, physAddr, regionSize, btIndex);
	}
	if (pc == 0x66C) {
		uint32_t virtAddr = getGPR(0);
		uint32_t physAddr = getGPR(1);
		uint32_t btIndex = getGPR(2);
		uint32_t regionSize = getGPR(3);
		uint32_t pageTableA = getGPR(4);
		uint32_t pageTableB = getGPR(5);
		log("KERNEL MMU PAGES: v:%08x p:%08x size:%08x idx:%02x tableA:%08x tableB:%08x",
			virtAddr, physAddr, regionSize, btIndex, pageTableA, pageTableB);
	}
	if (pc == 0x15070) {
		uint32_t virtAddr = getGPR(0);
		uint32_t physAddr = getGPR(1);
		uint32_t regionSize = getGPR(2);
		uint32_t a = getGPR(3);
		log("DPlatChunkHw MAPPING: v:%08x p:%08x size:%08x arg:%08x",
			virtAddr, physAddr, regionSize, a);
	}

	if (pc == 0x16198) {
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
		log("EVENT %s: tick=%d params=%08x,%08x", n, evtTick, evtParamA, evtParamB);
	}
}


const char *Emulator::getDeviceName() const { return "MC218"; }
int Emulator::getDigitiserWidth()  const { return 440; }
int Emulator::getDigitiserHeight() const { return 200; }
int Emulator::getLCDOffsetX()      const { return 60; }
int Emulator::getLCDOffsetY()      const { return 0; }
int Emulator::getLCDWidth()        const { return 320; }
int Emulator::getLCDHeight()       const { return 200; }

void Emulator::readLCDIntoBuffer(uint8_t **lines, bool is32BitOutput) const {
	if (lcdAddress == 0xC0000000) {
		int width = 320, height = 200;
		int bpp = 1;
		if (lcdControl & 0x40000000) bpp = 2;
		if (lcdControl & 0x80000000) bpp = 4;
		int ppb = 8 / bpp;

		// build our image out
		int lineWidth = (width * bpp) / 8;
		for (int y = 0; y < height; y++) {
			int lineOffs = lineWidth * y;
			for (int x = 0; x < width; x++) {
				uint8_t byte = MemoryBlockC0[lineOffs + (x / ppb)];
				int shift = (x & (ppb - 1)) * bpp;
				int mask = (1 << bpp) - 1;
				int palIdx = (byte >> shift) & mask;
				int palValue;
				if (bpp == 1)
					palValue = palIdx * 255;
				else
					palValue = (lcdPalette >> (palIdx * 4)) & 0xF;

				palValue |= (palValue << 4);
				if (is32BitOutput) {
					// Silvery-green tint matching the 5mx (Windermere) render
					// (rgbValues r=0x99, g=0xAA, b=0x88 at max brightness).
					// inv ∈ [0..255]: 255 = unlit pixel (bright background),
					// 0 = max drive (black).
					uint8_t inv = (uint8_t)(palValue ^ 0xFF);
					lines[y][x*4]   = (uint8_t)((0x99 * inv) / 255);
					lines[y][x*4+1] = (uint8_t)((0xAA * inv) / 255);
					lines[y][x*4+2] = (uint8_t)((0x88 * inv) / 255);
					lines[y][x*4+3] = 0xFF;
				} else {
					lines[y][x] = palValue ^ 0xFF;
				}
			}
		}
	}
}



void Emulator::diffPorts(uint32_t oldval, uint32_t newval) {
	uint32_t changes = oldval ^ newval;
	// Throttle per-pin-bit: each toggle pair (low→high→low) is logged
	// up to 4 times then silenced. The Series 5 EPOC R1 kernel toggles
	// PRT B4 / D2 / D6 every ~1 sec heartbeat which floods browser stderr.
	auto logPin = [this](unsigned bitIdx, const char *name, uint32_t bitVal) {
		static int counts[24] = {};  // one per port pin
		if (counts[bitIdx] < 8) {
			log("%s: %d  pc=%08x lr=%08x", name, bitVal,
			    getRealPC(), getGPR(14));
			counts[bitIdx]++;
			if (counts[bitIdx] == 8)
				log("%s: (further repeats throttled)", name);
		}
	};
	if (changes & 1) logPin(0, "PRT E0", newval&1);
	if (changes & 2) logPin(1, "PRT E1", newval&2);
	if (changes & 4) logPin(2, "PRT E2", newval&4);
	if (changes & 0x100) logPin(3, "PRT D0", newval&0x100);
	if (changes & 0x200) logPin(4, "PRT D1", newval&0x200);
	if (changes & 0x400) logPin(5, "PRT D2", newval&0x400);
	if (changes & 0x800) logPin(6, "PRT D3", newval&0x800);
	if (changes & 0x1000) logPin(7, "PRT D4", newval&0x1000);
	if (changes & 0x2000) logPin(8, "PRT D5", newval&0x2000);
	if (changes & 0x4000) logPin(9, "PRT D6", newval&0x4000);
	if (changes & 0x8000) logPin(10, "PRT D7", newval&0x8000);
	if (changes & 0x10000) logPin(11, "PRT B0", newval&0x10000);
	if (changes & 0x20000) logPin(12, "PRT B1", newval&0x20000);
	if (changes & 0x40000) logPin(13, "PRT B2", newval&0x40000);
	if (changes & 0x80000) logPin(14, "PRT B3", newval&0x80000);
	if (changes & 0x100000) logPin(15, "PRT B4", newval&0x100000);
	if (changes & 0x200000) logPin(16, "PRT B5", newval&0x200000);
	if (changes & 0x400000) logPin(17, "PRT B6", newval&0x400000);
	if (changes & 0x800000) logPin(18, "PRT B7", newval&0x800000);
}


uint32_t Emulator::readKeyboard() const {
	if (kScan & 8) {
		// Select one keyboard
		if ((kScan & 7) < 7)
			return keyboardColumns[kScan & 7];
		else
			return 0;
	} else if (kScan == 0) {
		// Report all columns combined
		uint8_t val = 0;
		for (int i = 0; i < 7; i++)
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
	case '1':                KEY(0, 0);
	case '2':                KEY(1, 0);
	case '3':                KEY(2, 0);
	case '4':                KEY(3, 0);
	case '5':                KEY(4, 0);
	case '6':                KEY(5, 0);
	case '7':                KEY(6, 0);

	case '8':                KEY(0, 1);
	case '9':                KEY(1, 1);
	case '0':                KEY(2, 1);
	case 'P':                KEY(3, 1);
	case EStdKeySingleQuote: KEY(4, 1);
	case EStdKeyEnter:       KEY(5, 1);
	case EStdKeyBackspace:   KEY(6, 1);

	case EStdKeyEscape:      KEY(0, 2);
	case 'Q':                KEY(1, 2);
	case 'W':                KEY(2, 2);
	case 'E':                KEY(3, 2);
	case 'R':                KEY(4, 2);
	case 'T':                KEY(5, 2);
	case 'Y':                KEY(6, 2);

	case 'U':                KEY(0, 3);
	case 'J':                KEY(1, 3);
	case 'I':                KEY(2, 3);
	case 'K':                KEY(3, 3);
	case 'O':                KEY(4, 3);
	case 'L':                KEY(5, 3);
	case EStdKeyUpArrow:     KEY(6, 3);

	case EStdKeyTab:         KEY(0, 4);
	case 'A':                KEY(1, 4);
	case 'S':                KEY(2, 4);
	case 'D':                KEY(3, 4);
	case 'F':                KEY(4, 4);
	case 'G':                KEY(5, 4);
	case 'H':                KEY(6, 4);

	case EStdKeySpace:       KEY(0, 5);
	case EStdKeyComma:       KEY(1, 5);
	case 'M':                KEY(2, 5);
	case EStdKeyFullStop:    KEY(3, 5);
	case EStdKeyLeftArrow:   KEY(4, 5);
	case EStdKeyDownArrow:   KEY(5, 5);
	case EStdKeyRightArrow:  KEY(6, 5);

	case 'Z':                KEY(0, 6);
	case 'X':                KEY(1, 6);
	case EStdKeyMenu:        KEY(2, 6);
	case 'C':                KEY(3, 6);
	case 'V':                KEY(4, 6);
	case 'B':                KEY(5, 6);
	case 'N':                KEY(6, 6);

	case EStdKeyLeftShift:   KEY(8, 0);
	case EStdKeyRightShift:  KEY(8, 1);
	case EStdKeyLeftCtrl:    KEY(8, 2);
	case EStdKeyLeftFunc:    KEY(8, 3);
	}

	if (idx >= 0x800) {
		if (value)
			keyboardExtra |= (idx & 0xFF);
		else
			keyboardExtra &= ~(idx & 0xFF);
	} else if (idx >= 0) {
		if (value)
			keyboardColumns[idx >> 8] |= (idx & 0xFF);
		else
			keyboardColumns[idx >> 8] &= ~(idx & 0xFF);
	}
}


void Emulator::updateTouchInput(int32_t x, int32_t y, bool down) {
	pendingInterrupts &= ~(1 << EINT2);
	if (down)
		pendingInterrupts |= (1 << EINT2);
	log("Touch: x=%d y=%d down=%s", x, y, down ? "yes" : "no");
	touchX = x;
	touchY = y;
}

bool Emulator::attachCard(const uint8_t *bytes, size_t size) {
	if (!bytes || size == 0) return false;
	cfCard.attach(bytes, size);
	pcCardController.setCardPresent(true);
	pendingInterrupts |= (1 << MCINT);
	return true;
}

void Emulator::detachCard() {
	cfCard.detach();
	pcCardController.setCardPresent(false);
	pendingInterrupts |= (1 << MCINT);
}

}
