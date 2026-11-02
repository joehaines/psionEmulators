// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Minimal driver for the JC3248W535's AXS15231B panel over QSPI.
//
// Over QSPI the controller ignores the row address (RASET), so the only
// reliable update is a whole frame: RAMWR for the first strip of rows,
// RAMWRC to continue, full 320-pixel width throughout. That is all this
// driver does; Screen::renderRows produces the strips.

#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <cstdint>
#include <functional>

class Lcd {
public:
    static constexpr int kWidth = 320, kHeight = 480;
    static constexpr int kStripRows = 8;    // 5 KiB per strip buffer (internal RAM)

    esp_err_t init();
    void setBacklight(int percent);

    // Send one frame. `fill(firstRow, rows, out)` writes `rows` full panel
    // rows of big-endian RGB565 into `out`; it is called kHeight /
    // kStripRows times, top to bottom, while the previous strip transmits.
    void drawFrame(const std::function<void(int, int, uint16_t *)> &fill);

private:
    void command(uint8_t cmd, const uint8_t *data = nullptr, size_t len = 0);
    static bool onTransferDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *,
                               void *ctx);

    esp_lcd_panel_io_handle_t m_io = nullptr;
    uint16_t *m_strip[2] = {};
    SemaphoreHandle_t m_free = nullptr;   // counts strip buffers not in flight
};
