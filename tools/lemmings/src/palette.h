// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The game draws in sixteen palette indices, and what they look like is the
// platform's business: a colour panel (the netpad, the netBook, the 5mx
// family's successors) shows PAL_RGB; a Series 5's sixteen-grey panel
// shows PAL_GREY, which is chosen so that things that have to be told
// apart — a lemming against earth, a button against the panel — still can be.
//
// Sprites are written with the index as a hex digit, so the digits below
// are the ones sprites.c uses: hair '6', shirt '3', skin 'e', and so on.

#ifndef LEMMINGS_PALETTE_H
#define LEMMINGS_PALETTE_H

enum {
    C_BLACK = 0,
    C_EARTH1 = 1,       // dark band of earth
    C_EARTH2 = 2,       // light band of earth
    C_SHIRT = 3,        // a lemming's blue robe
    C_BRICK = 4,        // a builder's bricks, tan
    C_GRASS = 5,
    C_HAIR = 6,         // a lemming's green hair
    C_WOOD = 7,
    C_STEEL1 = 8,       // dark steel, and the dark grey of the panel
    C_STEEL2 = 9,       // light steel, and the panel itself
    C_SKY1 = 10,        // sky at the horizon
    C_WATER = 11,
    C_ORANGE = 12,
    C_SKY2 = 13,        // sky overhead
    C_SKIN = 14,
    C_WHITE = 15
};

extern const unsigned char PAL_RGB[16][3];
extern const unsigned char PAL_GREY[16];

#endif
