// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "clps7110.h"

#include <cstring>

namespace CLPS7110 {

Emulator::Emulator() {
    // Per CL-PS7111 datasheet section 5.9 (and CL-PS7110 datasheet
    // section 3.2.12): SYSFLG1 bit 29 (ID) reads '1' for the CL-PS7111
    // and '0' for the CL-PS7110. The base class default is 0x20008000
    // (CLPS7111 ID + CLDFLG); we want 0x00008000 (CLDFLG only).
    sysFlg1 = 0x00008000;
}

// PS7110 LCD render. Per CL-PS7110 datasheet section 1.2.10 the frame
// buffer is FIXED at physical 0xC0000000 — there is no FRBADDR register.
// The LCDCON layout is identical to PS7111 (bits 31=GSMD, 30=GSEN,
// 29:25 AC prescale, 24:19 pixel prescale, 18:13 line length, 12:0 video
// buffer size), so we read bpp / line length from it instead of
// hardcoding values.
//
// When LCDCON == 0 (controller never programmed) we render solid black
// rather than reading uninitialised RAM at 0xC0000000 — anything else
// surfaces kernel scratch / stack as stripe-noise.
//
// Palette-aware rendering: the CL-PS7110 has a 64-bit grayscale palette
// (PALLSW/PALMSW = 16 × 4-bit entries) that maps a framebuffer pixel
// value (0..15 in 4 bpp) to a physical grayscale level (0..15). On a
// real reflective greyscale LCD, palette level 0 is the LIGHTEST (no
// pixel drive = ambient light reflected back = silvery/transparent
// background) and level 15 is the DARKEST (max pixel drive = blocks
// reflection = solid grey). Pixel value 0 in the kernel's
// pre-painted framebuffer is the splash background which on real
// hardware appears as the bright silvery LCD background, not pure
// black. So we INVERT the level→intensity mapping when sending bytes
// to the host RGB output.
//
// (Earlier comment said "level 0 is darkest" — that was wrong for
// reflective LCDs. The kernel programs the palette but it's the
// physical display that determines what a viewer SEES: undriven
// segments reflect ambient light; driven segments absorb it.)
void Emulator::readLCDIntoBuffer(uint8_t **lines, bool is32BitOutput) const {
    uint32_t lcdCtl = currentLcdControl();
    uint64_t pal   = currentLcdPalette();
    int width = getLCDWidth();
    int height = getLCDHeight();
    int rowStride = is32BitOutput ? width * 4 : width;

    if (lcdCtl == 0) {
        // LCDCON not yet programmed — render the unlit-LCD background
        // colour. 32-bit output gets the silvery-green tint that matches
        // the 5mx (Windermere) render (r=0x99, g=0xAA, b=0x88); 8-bit
        // (PGM) output stays pure white for harness screenshots.
        for (int y = 0; y < height; y++) {
            if (is32BitOutput) {
                for (int x = 0; x < width; x++) {
                    lines[y][x*4]     = 0x99;
                    lines[y][x*4 + 1] = 0xAA;
                    lines[y][x*4 + 2] = 0x88;
                    lines[y][x*4 + 3] = 0xFF;
                }
            } else {
                std::memset(lines[y], 255, rowStride);
            }
        }
        return;
    }

    int bpp = 1;
    if (lcdCtl & 0x40000000) bpp = 2;
    if (lcdCtl & 0x80000000) bpp = 4;
    int ppb = 8 / bpp;

    int lcdLineLenPixels = (((int)((lcdCtl >> 13) & 0x3F)) + 1) * 16;
    int linePixels = lcdLineLenPixels < width ? lcdLineLenPixels : width;

    int vbufBytes = (((int)(lcdCtl & 0x1FFF)) + 1) * 16;
    int fbBytes   = (width * height * bpp) / 8;
    int validBytes = vbufBytes < fbBytes ? vbufBytes : fbBytes;

    const uint8_t *src = MemoryBlockC0;

    // If the kernel hasn't programmed the palette, default to a sane
    // identity mapping (palIdx → palIdx grayscale level) so the render
    // doesn't go solid black on a working device whose kernel just hasn't
    // written PALLSW/PALMSW yet (unlikely but defensive).
    auto paletteEntry = [&](int idx) -> uint8_t {
        if (pal == 0) return (uint8_t)idx;   // identity fallback
        return (uint8_t)((pal >> (idx * 4)) & 0xF);
    };

    // 32-bit output: silvery-green tinted grayscale matching the 5mx
    // (Windermere) render (r=0x99, g=0xAA, b=0x88 at max brightness).
    // 8-bit (PGM) output: pure grayscale, level 0 = white, level 15 = black.
    for (int y = 0; y < height; y++) {
        int lineOffs = ((width * bpp) / 8) * y;
        for (int x = 0; x < width; x++) {
            uint8_t pixel = 0xFF;  // default: undriven = white
            if (x < linePixels) {
                int idx = lineOffs + (x / ppb);
                if (idx < validBytes) {
                    uint8_t byte = src[idx];
                    int shift = (x & (ppb - 1)) * bpp;
                    int mask = (1 << bpp) - 1;
                    int palIdx = (byte >> shift) & mask;
                    uint8_t level = paletteEntry(palIdx);
                    uint8_t palValue = (uint8_t)(level | (level << 4));
                    pixel = palValue ^ 0xFF;
                }
            }
            if (is32BitOutput) {
                // pixel ∈ [0..255]: 255 = unlit (bright), 0 = max drive (dark).
                lines[y][x*4]     = (uint8_t)((0x99 * pixel) / 255);
                lines[y][x*4 + 1] = (uint8_t)((0xAA * pixel) / 255);
                lines[y][x*4 + 2] = (uint8_t)((0x88 * pixel) / 255);
                lines[y][x*4 + 3] = 0xFF;
            } else {
                lines[y][x] = pixel;
            }
        }
    }
}

}
