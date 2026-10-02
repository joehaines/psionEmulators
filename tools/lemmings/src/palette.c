// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "palette.h"

const unsigned char PAL_RGB[16][3] = {
    {0x00, 0x00, 0x00},     // 0  black
    {0x5c, 0x38, 0x1c},     // 1  earth, dark band
    {0x86, 0x54, 0x2c},     // 2  earth, light band
    {0x2c, 0x4c, 0xd4},     // 3  robe
    {0xd8, 0xa8, 0x58},     // 4  brick
    {0x34, 0xa0, 0x30},     // 5  grass
    {0x28, 0xc8, 0x28},     // 6  hair
    {0x94, 0x64, 0x30},     // 7  wood
    {0x60, 0x68, 0x80},     // 8  steel, dark
    {0xb4, 0xbc, 0xcc},     // 9  steel, light
    {0xc4, 0xe0, 0xff},     // a  sky at the horizon
    {0x30, 0x78, 0xe0},     // b  water
    {0xf0, 0x98, 0x18},     // c  orange
    {0x78, 0xac, 0xf4},     // d  sky overhead
    {0xf8, 0xcc, 0xa4},     // e  skin
    {0xff, 0xff, 0xff}      // f  white
};

// For a panel of sixteen greys, as 0 (black) to 15 (white).
const unsigned char PAL_GREY[16] = {
    0,      // black
    5,      // earth, dark
    7,      // earth, light
    3,      // robe
    9,      // brick
    11,     // grass
    1,      // hair
    6,      // wood
    8,      // steel, dark
    12,     // steel, light
    14,     // sky at the horizon
    10,     // water
    9,      // orange
    13,     // sky overhead
    15,     // skin
    15      // white
};
