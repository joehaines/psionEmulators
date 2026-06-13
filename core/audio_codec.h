// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
// Shared codec FIFO + buzzer state model.
//
// Both Windermere (5mx, MC218, Revo, 5mx Pro) and CL-PS7110/7111
// (Series 5, Osaris) wire their on-chip codec to the same software-visible
// pattern: an 8-bit signed PCM data port (CODR), a 16-entry hardware
// FIFO on each of TX (playback) and RX (recording), a CSINT interrupt
// that fires when the TX FIFO has room or the RX FIFO has data, and a
// COEOI register that acks the interrupt.
//
// Before this struct landed, the two chip families each had their own
// copy of the FIFO drain, deferred-CSINT-re-fire, and buzzer-pump
// machinery. Bugs that needed fixing in both copies (audible-volume
// scaling, TX watermark CSINT firing, etc.) ended up fixed in one and
// regressed in the other. This struct owns the codec/buzzer state and
// the timing-driven plumbing; the device subclasses still own the
// register-decode and the CSINT-gating decisions (since those use
// chip-specific control bits — Windermere CONFG vs CL-PS711x SYSCON1).
//
// Public usage shape (composed into each Emulator):
//
//     AudioCodecModel audio;
//
//     // in the emulator constructor:
//     audio.configure(CLOCK_SPEED);
//
//     // in writeReg, when the kernel writes CODR:
//     audio.pushDacSample((int8_t)value, passedCycles);
//
//     // in readReg, when the kernel reads CODR:
//     auto r = audio.popAdcSample(passedCycles);
//     if (r.ringEmpty) clear pending CSINT;
//     return r.sample;
//
//     // in writeAudioInput (after device-specific gating):
//     bool wasEmptyEdge = audio.enqueueMicSamples(src, count);
//     if (wasEmptyEdge && device-specific-handler-active) fire CSINT;
//
//     // in the TINT tick loop, every tick:
//     if (audio.tickTxFifoDrain(passedCycles) && device-says-tx-active)
//         fire CSINT;
//     if (audio.tickDeferredCsint(passedCycles) && device-says-rx-active)
//         fire CSINT;
//
//     // for buzzer pump (per TINT tick):
//     audio.emitBuzzerSamples(deviceComputedActive, toneHz);
//
//     // on session teardown:
//     audio.resetRx();   // or resetTx() / resetAll()
//
//     // for host-side audio:
//     size_t got = audio.readAudioOutput(dst, maxSamples);
//     audio.setHostEnabled(speaker, mic);
//
// The struct deliberately doesn't know about CONFG / SYSCON1 / channel
// pointers / BZMOD vs BZTOG — those stay in the emulator classes
// because they encode chip-specific semantics. AudioCodecModel only
// covers the parts that are genuinely shared.

#include <array>
#include <cstddef>
#include <cstdint>

class AudioCodecModel {
public:
	// Real codec sample rate. CL-PS7110/7111 and Windermere both run at
	// 8 kHz nominally; the kernels program this and we mirror.
	static constexpr int kAudioSampleRate = 8000;
	// Ring buffer depth ≈ 1 s headroom at 8 kHz. Plenty for browser
	// pump latency.
	static constexpr size_t kAudioQueueSize = 8192;
	// Hardware FIFO depth. The kernel reads CODR in tight loops and
	// stops once the FIFO drains; capping per-cycle reads to this
	// depth keeps the kernel's stack-local drain buffer from
	// overflowing on devices that do unbounded reads.
	static constexpr int kAudioFifoDepth = 16;
	// CSINT fires when the virtual TX FIFO drops below this level —
	// matches the half-empty TX interrupt the real chips fire.
	static constexpr int kTxFifoWatermark = 1;

	// Configure per-device chip parameters. Pass the SoC clock speed
	// in Hz (Windermere = 36.864 MHz, CL-PS711x = 18.432 MHz).
	void configure(int clockSpeed);

	// ── Host-facing API ──────────────────────────────────────────────
	size_t readAudioOutput(int16_t *dst, size_t maxSamples);
	void setHostEnabled(bool speaker, bool mic);
	bool hostSpeakerEnabled() const { return hostSpeakerEnabled_; }
	bool hostMicEnabled() const { return hostMicEnabled_; }

	// ── Kernel-side CODR / COEOI ─────────────────────────────────────

	// Kernel wrote CODR with `sample` (8-bit signed PCM). Schedules
	// TX FIFO drain if idle.
	void pushDacSample(int8_t sample, int64_t currentCycle);

	// Kernel read CODR. Returns the popped sample plus three edges
	// the caller may want to act on:
	//   .ringEmpty  — the ring just transitioned to empty; clear CSINT.
	//   .capHit     — per-cycle 16-sample cap reached with samples
	//                 still pending; caller should schedule the
	//                 deferred CSINT re-fire so the next drain runs
	//                 ~125 µs later.
	//   .valid      — false when there was nothing to pop (caller may
	//                 want to fall back to whatever read-back the
	//                 device's CODR shadow holds).
	struct PopResult {
		uint8_t sample;
		bool valid;
		bool ringEmpty;
		bool capHit;
	};
	PopResult popAdcSample(int64_t currentCycle);

	// Kernel wrote COEOI. Resets the per-cycle FIFO read cap so the
	// next CSINT can surface another batch.
	void ackCsint();

	// ── Tick-loop hooks (call once per TINT tick) ────────────────────

	// Decrement the virtual TX FIFO at the codec sample rate. Returns
	// true when the level just crossed below kTxFifoWatermark — the
	// caller fires CSINT if its device-specific TX-active predicate
	// is satisfied.
	bool tickTxFifoDrain(int64_t currentCycle);

	// Returns true when the deferred CSINT scheduled by an earlier
	// popAdcSample(...).capHit has come due AND samples remain in the
	// ring. Caller fires CSINT if its device-specific RX-active
	// predicate is satisfied.
	bool tickDeferredCsint(int64_t currentCycle);

	// Buzzer pump. Caller computes `active` from device-specific
	// register state (BZCONT for Windermere, SYSCON1 bits 9/10 for
	// CL-PS711x) and the tone frequency from TC1 / a click default.
	// Pump emits 125 square-wave samples per TINT (8 kHz at 64 Hz
	// tick) into the same DAC queue the codec uses. A trailing zero
	// tick on the idle transition cleans up the DC offset.
	void emitBuzzerSamples(bool active, int toneHz);

	// ── Mic input ────────────────────────────────────────────────────

	// Push host mic samples into the ADC ring. Returns true on the
	// empty→non-empty edge so the caller can fire CSINT (gated by
	// device-specific RX-enable predicate).
	bool enqueueMicSamples(const int16_t *src, size_t count);

	// ── Status queries (for SYSFLG / COLFG flags) ────────────────────

	bool isTxFifoFull() const { return virtualTxFifoLevel_ >= kAudioFifoDepth; }
	bool isRxFifoEmpty() const {
		return adcHead_ == adcTail_ || adcFifoReadsSinceRefill_ >= kAudioFifoDepth;
	}
	// Diagnostic accessors
	int debugAdcFifoReadsSinceRefill() const { return adcFifoReadsSinceRefill_; }
	int64_t debugCsintDeferredAt() const { return codecCsintDeferredAt_; }

	// ── Session teardown ─────────────────────────────────────────────

	void resetTx();   // playback teardown (CDENTX 1→0 / CONFG=0 TX bits)
	void resetRx();   // recording teardown (CDENRX 1→0 / CONFG=0 RX bits)
	void resetAll();  // full codec teardown (CONFG=0 etc.)

	// Buzzer click envelope hooks. The device subclass detects the
	// rising edge of its chip-specific buzzer-toggle bit (BZCONT bit 0
	// for Windermere; SYSCON1 bit 9 for CL-PS711x) and calls
	// noteBuzzerRisingEdge(); the pump latches a `buzzerClickHoldTicks`
	// TINT count so sub-tick BZTOG pulses still produce an audible
	// burst. emitBuzzerSamples decrements the latch each call.
	void noteBuzzerRisingEdge();
	bool buzzerHolding() const { return buzzerHoldTicks_ > 0; }

	// Per-device tone defaults (used by emitBuzzerSamples when caller
	// passes toneHz=0). Tweakable per chip / kernel variant.
	int buzzerClickHoldTicks = 3;
	int buzzerClickHz = 3000;

	// Diagnostic counters, kept here so the harness/audio-harness can
	// read them via the host wrapper without poking into private state.
	uint64_t codrReads = 0;
	uint64_t codrWrites = 0;
	uint64_t csintFiredCount = 0;

	int32_t txFifoLevel() const { return virtualTxFifoLevel_; }

	// Direct access to the ring buffers — used by the host-side
	// AudioWorklet bridge and the diagnostic harnesses. Read-only from
	// the emulator side; the mutator API above maintains them.
	const std::array<int16_t, kAudioQueueSize> &dacQueueRef() const { return dacQueue_; }
	const std::array<int16_t, kAudioQueueSize> &adcQueueRef() const { return adcQueue_; }
	size_t adcRingFill() const {
		return (adcTail_ - adcHead_) & (kAudioQueueSize - 1);
	}
	// Discard all buffered mic (ADC) samples and any pending CSINT.  Called
	// when recording stops so leftover host mic data (our ADC ring is far
	// deeper than the real 16-deep RX FIFO) doesn't keep generating RX
	// service interrupts after the recorder has cancelled its read.
	void clearAdcRing() {
		adcHead_ = adcTail_ = 0;
		adcFifoReadsSinceRefill_ = 0;
		codecCsintDeferredAt_ = 0;
	}
	size_t dacRingFill() const {
		return (dacTail_ - dacHead_) & (kAudioQueueSize - 1);
	}

private:
	static inline size_t advance(size_t i) {
		return (i + 1) & (kAudioQueueSize - 1);
	}

	// Sample rings.
	std::array<int16_t, kAudioQueueSize> dacQueue_ = {};
	std::array<int16_t, kAudioQueueSize> adcQueue_ = {};
	size_t dacHead_ = 0, dacTail_ = 0;
	size_t adcHead_ = 0, adcTail_ = 0;

	// Per-cycle RX FIFO read cap.
	int adcFifoReadsSinceRefill_ = 0;

	// Virtual TX FIFO between CODR writes and the host speaker.
	int32_t virtualTxFifoLevel_ = 0;
	int64_t nextTxFifoDrainAt_ = 0;

	// Scheduled deferred CSINT (set by popAdcSample's capHit branch).
	int64_t codecCsintDeferredAt_ = 0;

	// Host gates.
	bool hostSpeakerEnabled_ = false;
	bool hostMicEnabled_ = false;

	// Buzzer pump square-wave phase + sub-tick BZTOG-pulse latch.
	int buzzerPhase_ = 0;
	int buzzerHoldTicks_ = 0;

	// Timing parameters set by configure().
	int cyclesPerAudioSample_ = 0;
	int codecCsintIntervalCycles_ = 0;
};
