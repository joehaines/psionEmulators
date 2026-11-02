// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The Siena's sound (buzzer and 8 kHz codec, mixed by SiboAudio) out of the
// board's I2S amplifier and the speaker on P6.

#pragma once

#include "esp_err.h"

#include <cstddef>
#include <cstdint>

namespace audio_out {

esp_err_t init(int sampleRate);
// Queue mono samples, waiting at most a few ms for room; whatever still
// doesn't fit is dropped. Returns how many were queued.
size_t write(const int16_t *samples, size_t count);

} // namespace audio_out
