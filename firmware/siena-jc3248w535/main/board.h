// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Guition JC3248W535 (ESP32-S3-WROOM-1 N16R8, 3.5" 320x480 AXS15231B QSPI
// IPS panel). Pin map from the vendor's GPIO table and the BSP in
// github.com/NorthernMan54/JC3248W535EN (src/esp_bsp.h).

#pragma once

#include "driver/gpio.h"

namespace board {

// LCD, QSPI on SPI2.
constexpr gpio_num_t kLcdCs   = GPIO_NUM_45;
constexpr gpio_num_t kLcdClk  = GPIO_NUM_47;
constexpr gpio_num_t kLcdD0   = GPIO_NUM_21;
constexpr gpio_num_t kLcdD1   = GPIO_NUM_48;
constexpr gpio_num_t kLcdD2   = GPIO_NUM_40;
constexpr gpio_num_t kLcdD3   = GPIO_NUM_39;
constexpr gpio_num_t kLcdBacklight = GPIO_NUM_1;

// I2S amplifier (speaker on P6). Its CTRL pin is strapped to the battery
// rail, so it is always enabled.
constexpr gpio_num_t kI2sBclk = GPIO_NUM_42;
constexpr gpio_num_t kI2sWs   = GPIO_NUM_2;
constexpr gpio_num_t kI2sDout = GPIO_NUM_41;

// microSD slot, 1-bit SDMMC (the vendor's demo: SD_MMC_CLK/CMD/D0).
constexpr gpio_num_t kSdClk = GPIO_NUM_12;
constexpr gpio_num_t kSdCmd = GPIO_NUM_11;
constexpr gpio_num_t kSdD0  = GPIO_NUM_13;

// The BOOT button, free once the chip is running.
constexpr gpio_num_t kBootButton = GPIO_NUM_0;

} // namespace board
