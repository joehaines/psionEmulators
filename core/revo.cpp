// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "revo.h"

namespace Revo {

// Mirrors Windermere::readLCDIntoBuffer but with width/height taken from the
// virtual overrides so each line's byte stride matches the Revo framebuffer
// layout (480 x 160 at 4 bpp). The palette + pixel-encoding format is the
// same as 5mx — just a different canvas size.
void Emulator::readLCDIntoBuffer(uint8_t **lines, bool is32BitOutput) const {
    static bool initRgbValues = false;
    static uint32_t rgbValues[16];
    if (!initRgbValues) {
        initRgbValues = true;
        for (int i = 0; i < 16; i++) {
            int r = (0x99 * i) / 15;
            int g = (0xAA * i) / 15;
            int b = (0x88 * i) / 15;
            rgbValues[15 - i] = r | (g << 8) | (b << 16) | 0xFF000000;
        }
    }

    uint32_t lcdAddress = currentLcdAddress();
    if ((lcdAddress >> 24) == 0xC0 || (lcdAddress >> 24) == 0xD0) {
        const uint8_t *lcdBuf = &MemoryBlockC0[lcdAddress & MemoryBlockMask];
        int width = getLCDWidth();
        int height = getLCDHeight();

        int bpp = 1 << (lcdBuf[1] >> 4);
        int ppb = 8 / bpp;
        uint16_t palette[16];
        for (int i = 0; i < 16; i++)
            palette[i] = lcdBuf[i * 2] | ((lcdBuf[i * 2 + 1] << 8) & 0xF00);

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

}
