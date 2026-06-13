// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
// SIBO-only audio output model.
//
// Deliberately separate from core/audio_codec.h (which serves Windermere and
// CL-PS7110/7111) so the EPOC16 SIBO chips (ASIC2 piezo buzzer on Series 3 /
// MC400; ASIC9 buzzer + 8-bit PCM codec on Series 3a / 3c / 3mx / Siena /
// Workabout) can evolve independently from the EPOC32 codec FIFO machinery.
// The two chip families never share a binary at runtime — each Emulator
// owns its own audio member — but keeping the class itself separate means
// a fix in one path can never accidentally regress the other.
//
// What this model owns:
//   - DAC output ring shared by the buzzer pump and the ASIC9 PCM playback
//   - ADC input ring fed by writeAudioInput() and drained by the PCM read
//     callback (ASIC9 only — Series 3 has no mic)
//   - 64 Hz buzzer square-wave generator with a click-hold latch
//   - Host-side speaker / mic enable gates
//
// What this model deliberately does NOT have (because SIBO doesn't need it):
//   - Virtual TX FIFO drain scheduling (ASIC9's own Snd timer handles draining)
//   - Deferred CSINT re-fire (ASIC9 asserts A9IntSnd autonomously)
//   - Per-cycle FIFO read cap
//   - CONFG / SYSCON1 chip-control plumbing
//
// Public usage:
//
//     SiboAudio audio;
//     audio.configure(BUS_CLOCK_HZ);
//     audio.buzzerClickHoldTicks = 2;
//
//     // From the chip's buz_cb:
//     if (level && !prev) audio.noteBuzzerRisingEdge();
//
//     // From executeUntil on a 64 Hz cadence:
//     audio.emitBuzzerSamples(active, audio.buzzerClickHz);
//
//     // From ASIC9 PCM callbacks:
//     audio.pushPcmSample(byte);                 // playback
//     auto r = audio.popPcmSample();             // capture
//
//     // From EmuBase audio hooks:
//     audio.readAudioOutput(dst, maxSamples);
//     audio.enqueueMicSamples(src, count);
//     audio.setHostEnabled(speaker, mic);

#include <array>
#include <cstddef>
#include <cstdint>

class SiboAudio {
public:
	static constexpr int    kAudioSampleRate = 8000;
	static constexpr size_t kRingSize        = 8192;  // ~1 s headroom @ 8 kHz

	void configure(int busClockHz);

	// ── Host-facing API ─────────────────────────────────────────────────
	size_t readAudioOutput(int16_t *dst, size_t maxSamples);
	void   setHostEnabled(bool speaker, bool mic);
	bool   hostSpeakerEnabled() const { return hostSpeakerEnabled_; }
	bool   hostMicEnabled()     const { return hostMicEnabled_; }

	// Ring occupancy (tests / debugging).
	size_t dacFill() const { return (dacTail_ - dacHead_) & (kRingSize - 1); }
	size_t adcFill() const { return (adcTail_ - adcHead_) & (kRingSize - 1); }

	// ── Buzzer pump ─────────────────────────────────────────────────────
	// Caller computes `active` (pin-high OR still inside click hold) and
	// passes a tone in Hz; pump pushes a square-wave burst sized to one
	// 64 Hz tick (125 samples at 8 kHz).
	void emitBuzzerSamples(bool active, int toneHz);
	// Latch a click-hold envelope. Subsequent emitBuzzerSamples calls
	// treat the buzzer as active for buzzerClickHoldTicks ticks even if
	// the pin level is low.
	void noteBuzzerRisingEdge();
	bool buzzerHolding() const { return buzzerHoldTicks_ > 0; }

	// ── ASIC9 PCM codec hooks ───────────────────────────────────────────
	// The ASIC9-attached codec (M7702 on 3c, M7542 on 3a) is A-law, not
	// linear PCM: each byte on the FIFO is an A-law-companded sample. The
	// expand formula is ported verbatim from MAME's psion3a_codec_device::
	// pcm_in (reference/mame-psion/psion/psion3a.cpp:69-86), which yields a
	// 13-bit magnitude (±4032); we scale by 8 to fill int16 range. The
	// compress side is its exact inverse so mic capture round-trips.
	static int16_t alawExpand(uint8_t raw);
	static uint8_t alawCompress(int16_t sample);

	// Push one raw A-law playback byte into the DAC ring (mixed with
	// any concurrent buzzer output — last-writer-wins on the ring slot).
	void pushPcmSample(uint8_t alawByte);

	struct PopResult { uint8_t sample; bool valid; };
	// Pop one ADC sample (as a raw A-law byte) for the PCM-in callback.
	// `valid` is false when the ring is empty, in which case `sample`
	// holds the A-law silence byte (alawCompress(0) = 0x55) so the kernel
	// records silence rather than a DC ramp.
	PopResult popPcmSample();

	// Host mic input. No-op when host mic is disabled — the caller is
	// expected to gate on hostMicEnabled() before calling, but the
	// internal check keeps stale samples from accumulating.
	void enqueueMicSamples(const int16_t *src, size_t count);

	// Per-device tone defaults — exposed so device drivers can override.
	int buzzerClickHoldTicks = 2;
	int buzzerClickHz        = 3000;

private:
	static inline size_t advance(size_t i) { return (i + 1) & (kRingSize - 1); }

	std::array<int16_t, kRingSize> dacRing_ = {};
	std::array<int16_t, kRingSize> adcRing_ = {};
	size_t dacHead_ = 0, dacTail_ = 0;
	size_t adcHead_ = 0, adcTail_ = 0;

	bool hostSpeakerEnabled_ = false;
	bool hostMicEnabled_     = false;

	int buzzerPhase_     = 0;
	int buzzerHoldTicks_ = 0;

	int cyclesPerSample_ = 0;  // reserved for future timing decisions
};
