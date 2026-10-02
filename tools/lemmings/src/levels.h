// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#ifndef LEMMINGS_LEVELS_H
#define LEMMINGS_LEVELS_H

// The level is a 1280 x 160 map of terrain. Rather than store a bitmap
// per level, a level is a short list of shapes that are painted into the
// map when it starts.
#define LEVEL_W 1280
#define LEVEL_H 160

enum { P_END, P_RECT, P_CARVE, P_STEEL, P_RAMP_UP, P_RAMP_DN, P_DISC, P_CDISC };

typedef struct {
    unsigned char kind, tex;
    int a, b, c, d;
} Prim;

// The eight skills, in the order the panel shows them.
enum { SK_CLIMB, SK_FLOAT, SK_BOMB, SK_BLOCK, SK_BUILD, SK_BASH, SK_MINE, SK_DIG, SK_COUNT };

typedef struct {
    const char *name;
    const char *rating;
    int total, required, rate, seconds;
    int entr_x, entr_y;          // trapdoor: centre x, top y
    int exit_x, exit_y;          // door: centre x, the row a lemming's feet are on
    int water;                   // y of the water's surface, or 0 for none
    int skills[SK_COUNT];
    int scroll;                  // where the view starts
    const Prim *prims;
} LevelDef;

extern const LevelDef g_levels[];
extern const int g_nlevels;

#endif
