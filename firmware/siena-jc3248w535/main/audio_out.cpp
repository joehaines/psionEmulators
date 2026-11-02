// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "audio_out.h"

#include "board.h"

#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"

namespace audio_out {

namespace {
i2s_chan_handle_t g_tx = nullptr;
}

esp_err_t init(int sampleRate) {
    i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan.dma_desc_num = 4;
    chan.dma_frame_num = 240;           // 4 x 30 ms of buffering at 8 kHz
    chan.auto_clear = true;             // silence, not a loop, on underrun
    esp_err_t err = i2s_new_channel(&chan, &g_tx, nullptr);
    if (err != ESP_OK) return err;

    i2s_std_config_t std = {};
    std.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(uint32_t(sampleRate));
    std.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                       I2S_SLOT_MODE_MONO);
    // Each sample in both slots. The NS4168 plays one channel, picked by
    // the voltage on its CTRL pin (here the battery rail); this way it
    // doesn't matter which.
    std.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
    std.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    std.gpio_cfg.bclk = board::kI2sBclk;
    std.gpio_cfg.ws = board::kI2sWs;
    std.gpio_cfg.dout = board::kI2sDout;
    std.gpio_cfg.din = I2S_GPIO_UNUSED;
    err = i2s_channel_init_std_mode(g_tx, &std);
    if (err != ESP_OK) return err;
    return i2s_channel_enable(g_tx);
}

size_t write(const int16_t *samples, size_t count) {
    if (!g_tx || !count) return 0;
    // A moment's wait for room, rather than dropping: when the Siena runs
    // ahead of the speaker (catching up after boot), this paces it.
    size_t written = 0;
    i2s_channel_write(g_tx, samples, count * sizeof(int16_t), &written, pdMS_TO_TICKS(5));
    return written / sizeof(int16_t);
}

} // namespace audio_out
