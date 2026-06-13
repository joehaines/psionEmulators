// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "sibo_audio.h"

void SiboAudio::configure(int busClockHz) {
	cyclesPerSample_ = busClockHz / kAudioSampleRate;
}

void SiboAudio::setHostEnabled(bool speaker, bool mic) {
	hostSpeakerEnabled_ = speaker;
	hostMicEnabled_     = mic;
}

size_t SiboAudio::readAudioOutput(int16_t *dst, size_t maxSamples) {
	// Drain the DAC ring regardless of host speaker state so producer-side
	// pumps don't observe a stalled queue; substitute silence when muted.
	size_t written = 0;
	while (written < maxSamples && dacHead_ != dacTail_) {
		int16_t s = dacRing_[dacHead_];
		dacHead_ = advance(dacHead_);
		dst[written++] = hostSpeakerEnabled_ ? s : int16_t(0);
	}
	return written;
}

void SiboAudio::noteBuzzerRisingEdge() {
	buzzerHoldTicks_ = buzzerClickHoldTicks;
}

void SiboAudio::emitBuzzerSamples(bool active, int toneHz) {
	// 125 samples = 8 kHz / 64 Hz. Caller is expected to invoke this once
	// per 64 Hz tick from the device's executeUntil loop.
	constexpr int kSamplesPerTick = kAudioSampleRate / 64;

	bool gateActive = active || buzzerHoldTicks_ > 0;
	if (buzzerHoldTicks_ > 0) buzzerHoldTicks_--;

	if (!gateActive) {
		// Push one trailing zero to clean up the DC offset on the
		// idle-transition tick, then short-circuit.
		size_t next = advance(dacTail_);
		if (next == dacHead_) dacHead_ = advance(dacHead_);
		dacRing_[dacTail_] = 0;
		dacTail_ = next;
		buzzerPhase_ = 0;
		return;
	}

	if (toneHz <= 0) toneHz = buzzerClickHz;
	// Half-period in samples for the requested tone (≥1 to avoid divide-by-zero
	// degenerating into a DC level).
	int halfPeriod = kAudioSampleRate / (2 * toneHz);
	if (halfPeriod < 1) halfPeriod = 1;

	constexpr int16_t kAmplitude = 0x2000;  // ~25% full-scale; comfortable click level
	for (int i = 0; i < kSamplesPerTick; i++) {
		int16_t s = (buzzerPhase_ < halfPeriod) ? kAmplitude : int16_t(-kAmplitude);
		size_t next = advance(dacTail_);
		if (next == dacHead_) dacHead_ = advance(dacHead_);
		dacRing_[dacTail_] = s;
		dacTail_ = next;
		buzzerPhase_++;
		if (buzzerPhase_ >= 2 * halfPeriod) buzzerPhase_ = 0;
	}
}

int16_t SiboAudio::alawExpand(uint8_t raw) {
	// MAME psion3a_codec_device::pcm_in, verbatim: XOR with 0x55, then
	// expand the 3-bit segment + 4-bit mantissa to a 13-bit magnitude.
	// Bit 7 (after the XOR) is the sign, 1 = negative.
	uint8_t data = raw ^ 0x55;
	uint8_t seg  = (data & 0x70) >> 4;
	int16_t mag  = 0;
	if (seg) {
		mag = 0x10;
		seg--;
	}
	mag = int16_t((((mag + (data & 0x0f)) << 1) + 1) << seg);
	int16_t out = (data & 0x80) ? int16_t(-mag) : mag;
	// MAME treats the 13-bit value as a fraction of 4096 full-scale;
	// scale by 8 so ±4032 lands at ±32256 in int16.
	return int16_t(out * 8);
}

uint8_t SiboAudio::alawCompress(int16_t sample) {
	// Exact inverse of alawExpand: 16-bit → 13-bit magnitude, segment
	// search, then the same XOR-0x55 transmission format with bit 7
	// (pre-XOR bit 7, post-XOR unchanged: 0x55 has bit 7 clear) = sign.
	uint8_t sign = (sample < 0) ? 0x80 : 0x00;
	int mag = (sample < 0) ? -int(sample) : int(sample);
	mag >>= 3;  // back to the 13-bit domain alawExpand scales from
	if (mag > 0x0FFF) mag = 0x0FFF;
	uint8_t data;
	if (mag < 0x20) {
		data = uint8_t(mag >> 1);                 // segment 0
	} else {
		uint8_t seg = 1;
		while (seg < 7 && mag >= (0x20 << seg)) seg++;
		data = uint8_t((seg << 4) | ((mag >> seg) & 0x0f));
	}
	return uint8_t(data | sign) ^ 0x55;
}

void SiboAudio::pushPcmSample(uint8_t alawByte) {
	int16_t s = alawExpand(alawByte);
	size_t next = advance(dacTail_);
	if (next == dacHead_) dacHead_ = advance(dacHead_);
	dacRing_[dacTail_] = s;
	dacTail_ = next;
}

SiboAudio::PopResult SiboAudio::popPcmSample() {
	if (adcHead_ == adcTail_) return { alawCompress(0), false };
	int16_t s = adcRing_[adcHead_];
	adcHead_ = advance(adcHead_);
	return { alawCompress(s), true };
}

void SiboAudio::enqueueMicSamples(const int16_t *src, size_t count) {
	if (!hostMicEnabled_) return;
	for (size_t i = 0; i < count; i++) {
		size_t next = advance(adcTail_);
		if (next == adcHead_) adcHead_ = advance(adcHead_);
		adcRing_[adcTail_] = src[i];
		adcTail_ = next;
	}
}
