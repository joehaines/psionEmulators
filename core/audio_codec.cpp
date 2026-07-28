// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "audio_codec.h"

void AudioCodecModel::configure(int clockSpeed) {
	cyclesPerAudioSample_ = clockSpeed / kAudioSampleRate;
	// Deferred CSINT cadence after a per-cycle FIFO cap hit. The "*3"
	// multiplier here matches what Windermere empirically settled on
	// to land the kernel's effective drain rate close to 1:1 wall-
	// clock on Chrome. (1× was too aggressive: the kernel dispatcher
	// re-entered faster than its CSINT handler could clear, driving
	// playback pitch up; 3× is the comfortable steady state.)
	codecCsintIntervalCycles_ = cyclesPerAudioSample_ * 3;
}

void AudioCodecModel::setHostEnabled(bool speaker, bool mic) {
	hostSpeakerEnabled_ = speaker;
	hostMicEnabled_ = mic;
}

size_t AudioCodecModel::readAudioOutput(int16_t *dst, size_t maxSamples) {
	// Drain the DAC ring regardless of host speaker state — when muted
	// we substitute silence so EPOC's TX-FIFO drain observers don't
	// see a stalled queue. The codec state machine doesn't care
	// whether the user is listening.
	size_t written = 0;
	while (written < maxSamples && dacHead_ != dacTail_) {
		int16_t s = dacQueue_[dacHead_];
		dacHead_ = advance(dacHead_);
		dst[written++] = hostSpeakerEnabled_ ? s : (int16_t)0;
	}
	return written;
}

void AudioCodecModel::pushDacSample(int8_t sample, int64_t currentCycle) {
	// Widen to int16 with a <<8 shift so a ±127 codec sample lands at
	// ±32512 in the host stream rather than the inaudible ±0.004 you'd
	// get from a raw 8-bit value.
	int16_t s = (int16_t)sample << 8;
	size_t next = advance(dacTail_);
	if (next == dacHead_) dacHead_ = advance(dacHead_);  // drop oldest
	dacQueue_[dacTail_] = s;
	dacTail_ = next;
	codrWrites++;

	// Reflect into the virtual hardware FIFO. The TX FIFO drain
	// scheduled in the tick loop is what fires CSINT below the
	// watermark — without it the kernel writes a half-FIFO of
	// samples on playback warmup, never sees CSINT, and tears down.
	if (virtualTxFifoLevel_ < kAudioFifoDepth) virtualTxFifoLevel_++;
	if (nextTxFifoDrainAt_ == 0) {
		nextTxFifoDrainAt_ = currentCycle + cyclesPerAudioSample_;
	}
}

AudioCodecModel::PopResult AudioCodecModel::popAdcSample(int64_t currentCycle) {
	PopResult r{};
	if (adcHead_ != adcTail_ && adcFifoReadsSinceRefill_ < kAudioFifoDepth) {
		int8_t s8 = (int8_t)(adcQueue_[adcHead_] >> 8);
		adcHead_ = advance(adcHead_);
		adcFifoReadsSinceRefill_++;
		codrReads++;
		r.sample = (uint8_t)s8;
		r.valid = true;
		r.ringEmpty = (adcHead_ == adcTail_);
		// Per-cycle 16-sample cap: real hardware FIFO of that depth
		// drains in one IRQ; we schedule the next CSINT ~3/8000 s
		// later so the kernel keeps draining at the codec rate
		// (~500 Hz CSINT, 16 samples each → 8 kHz mic input rate).
		// Only schedule once per drain cycle.
		r.capHit = !r.ringEmpty &&
		           adcFifoReadsSinceRefill_ >= kAudioFifoDepth &&
		           codecCsintDeferredAt_ == 0;
		if (r.capHit) {
			codecCsintDeferredAt_ = currentCycle + codecCsintIntervalCycles_;
		}
	}
	return r;
}

void AudioCodecModel::pushDacSample16(int16_t sample) {
	size_t next = advance(dacTail_);
	if (next == dacHead_) dacHead_ = advance(dacHead_);  // drop oldest
	dacQueue_[dacTail_] = sample;
	dacTail_ = next;
	codrWrites++;
}

bool AudioCodecModel::popMicSample16(int16_t &out) {
	if (adcHead_ == adcTail_) return false;
	out = adcQueue_[adcHead_];
	adcHead_ = advance(adcHead_);
	codrReads++;
	return true;
}

void AudioCodecModel::ackCsint() {
	// COEOI write. Reset the per-cycle FIFO read counter so the next
	// CSINT can surface another batch. Don't immediately re-fire
	// CSINT here — doing so previously let the kernel dispatcher
	// chain re-entries faster than our scheduled 8 kHz cadence,
	// driving delivery rates 2-9× too fast and making recordings
	// sound pitched down / stretched.
	adcFifoReadsSinceRefill_ = 0;
}

bool AudioCodecModel::tickTxFifoDrain(int64_t currentCycle) {
	bool watermarkCrossed = false;
	while (nextTxFifoDrainAt_ != 0 &&
	       currentCycle >= nextTxFifoDrainAt_ &&
	       virtualTxFifoLevel_ > 0) {
		virtualTxFifoLevel_--;
		nextTxFifoDrainAt_ += cyclesPerAudioSample_;
		if (virtualTxFifoLevel_ == 0) {
			nextTxFifoDrainAt_ = 0;
		}
		if (virtualTxFifoLevel_ < kTxFifoWatermark) {
			watermarkCrossed = true;
		}
	}
	return watermarkCrossed;
}

bool AudioCodecModel::tickDeferredCsint(int64_t currentCycle) {
	if (codecCsintDeferredAt_ == 0 || currentCycle < codecCsintDeferredAt_) {
		return false;
	}
	codecCsintDeferredAt_ = 0;
	// Caller will check ring + RX-active predicate before firing CSINT.
	// Reset the per-cycle FIFO read counter so the next drain can
	// surface another batch.
	if (adcHead_ != adcTail_) {
		adcFifoReadsSinceRefill_ = 0;
		return true;
	}
	return false;
}

bool AudioCodecModel::enqueueMicSamples(const int16_t *src, size_t count) {
	bool wasEmpty = (adcHead_ == adcTail_);
	for (size_t i = 0; i < count; i++) {
		size_t next = advance(adcTail_);
		if (next == adcHead_) break;  // ring full — drop the rest
		adcQueue_[adcTail_] = src[i];
		adcTail_ = next;
	}
	return wasEmpty && adcHead_ != adcTail_;
}

void AudioCodecModel::noteBuzzerRisingEdge() {
	buzzerHoldTicks_ = buzzerClickHoldTicks;
}

void AudioCodecModel::emitBuzzerSamples(bool active, int toneHz) {
	// Pump 125 samples per TINT tick (8000/64 = 125 samples/s at the
	// codec rate). When idle but with a non-zero phase, emit one tick
	// of zeros to drain the worklet's underflow lastSample to 0
	// instead of holding a DC offset between clicks.
	if (buzzerHoldTicks_ > 0) buzzerHoldTicks_--;
	const int kSamplesPerTick = kAudioSampleRate / 64;
	if (active) {
		int hz = toneHz > 0 ? toneHz : buzzerClickHz;
		const int kAmplitude = 80;
		int halfPeriod = kAudioSampleRate / (hz * 2);
		if (halfPeriod < 1) halfPeriod = 1;
		for (int i = 0; i < kSamplesPerTick; i++) {
			int8_t raw = ((buzzerPhase_ / halfPeriod) & 1) ? kAmplitude : -kAmplitude;
			int16_t sample = (int16_t)raw << 8;
			size_t next = advance(dacTail_);
			if (next == dacHead_) dacHead_ = advance(dacHead_);
			dacQueue_[dacTail_] = sample;
			dacTail_ = next;
			buzzerPhase_++;
			if (buzzerPhase_ >= halfPeriod * 2) buzzerPhase_ = 0;
		}
		codrWrites += kSamplesPerTick;
	} else if (buzzerPhase_ != 0) {
		for (int i = 0; i < kSamplesPerTick; i++) {
			size_t next = advance(dacTail_);
			if (next == dacHead_) dacHead_ = advance(dacHead_);
			dacQueue_[dacTail_] = 0;
			dacTail_ = next;
		}
		buzzerPhase_ = 0;
	}
}

void AudioCodecModel::resetTx() {
	virtualTxFifoLevel_ = 0;
	nextTxFifoDrainAt_ = 0;
	dacHead_ = dacTail_;
}

void AudioCodecModel::resetRx() {
	adcHead_ = adcTail_;
	adcFifoReadsSinceRefill_ = 0;
	codecCsintDeferredAt_ = 0;
}

void AudioCodecModel::resetAll() {
	resetTx();
	resetRx();
	buzzerPhase_ = 0;
	buzzerHoldTicks_ = 0;
}
