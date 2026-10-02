// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Twelve levels, all original. Each one is a handful of shapes painted
// into the 1280x160 map, an entrance, an exit and a budget of skills.
//
// Numbers worth knowing while reading them: a lemming is 10 pixels tall
// and walks at one pixel a tick; it can step up a ledge of up to six
// pixels and down up to three; a fall of more than 60 pixels kills it
// (unless it has a floater); a builder's bridge climbs one pixel for every
// two it advances and is 12 bricks long, so it spans about 30 pixels and
// rises 12; an explosion cuts a hole about 26 across and only 5 deep,
// shallow enough to walk out of.

#include "levels.h"

#define RECT(x, y, w, h)    { P_RECT,    1, x, y, w, h }
#define CARVE(x, y, w, h)   { P_CARVE,   0, x, y, w, h }
#define STEEL(x, y, w, h)   { P_STEEL,   0, x, y, w, h }
#define RAMPU(x, y, w, h)   { P_RAMP_UP, 1, x, y, w, h }   // rises to the right
#define RAMPD(x, y, w, h)   { P_RAMP_DN, 1, x, y, w, h }   // falls to the right
#define DISC(x, y, r)       { P_DISC,    1, x, y, r, 0 }
#define CDISC(x, y, r)      { P_CDISC,   0, x, y, r, 0 }
#define END                 { P_END, 0, 0, 0, 0, 0 }

// 1: nothing to do but watch, and learn what the exit looks like.
static const Prim p1[] = {
    RECT(40, 130, 1200, 30),
    RAMPU(500, 118, 60, 12), RECT(560, 118, 80, 12), RAMPD(640, 118, 60, 12),
    RECT(24, 60, 16, 70),
    END };

// 2: the first builder. A gap you cannot walk across.
static const Prim p2[] = {
    RECT(40, 130, 500, 30), RECT(558, 130, 682, 30),
    RECT(24, 60, 16, 70),
    END };

// 3: dig through a roof into the room beneath.
static const Prim p3[] = {
    RECT(40, 100, 1200, 60), CARVE(300, 116, 320, 34),
    RECT(24, 40, 16, 60),
    END };

// 4: the only way out is back the way they came. Blockers.
static const Prim p4[] = {
    RECT(160, 110, 760, 50), RECT(150, 50, 10, 110),
    END };

// 5: a wall too high to step over.
static const Prim p5[] = {
    RECT(40, 130, 1200, 30), RECT(24, 60, 16, 70),
    RECT(560, 70, 40, 60),
    END };

// 6: a long drop to the floor.
static const Prim p6[] = {
    RECT(40, 40, 400, 120), RECT(440, 150, 840, 10),
    RECT(24, 0, 16, 40),
    END };

// 7: a wall of earth to blow a way through — time the fuse to the walk.
static const Prim p7[] = {
    RECT(40, 130, 1200, 30), RECT(24, 60, 16, 70),
    RECT(500, 20, 10, 110),
    END };

// 8: a tunnel down to a room inside the hill.
static const Prim p8[] = {
    RECT(40, 60, 1200, 100), CARVE(480, 110, 140, 40),
    RECT(24, 20, 16, 40),
    END };

// 9: two walls, bash both.
static const Prim p9[] = {
    RECT(40, 130, 1200, 30), RECT(24, 60, 16, 70),
    RECT(400, 50, 24, 80), RECT(750, 50, 30, 80),
    END };

// 10: a tower of steel that no pick can dent, and a wall of earth after it.
static const Prim p10[] = {
    RECT(40, 130, 1200, 30), RECT(24, 60, 16, 70),
    STEEL(500, 80, 30, 50), RECT(800, 60, 26, 70),
    END };

// 11: water. Two builders, one after the other, to cross it.
static const Prim p11[] = {
    RECT(40, 130, 480, 30), RECT(552, 130, 688, 30),
    RECT(24, 60, 16, 70),
    END };

// 12: a gap, then a wall to bash.
static const Prim p12[] = {
    RECT(40, 130, 420, 30), RECT(478, 130, 762, 30),
    RECT(24, 60, 16, 70),
    RECT(900, 70, 40, 60),
    END };

//            climb float bomb block build bash mine dig
const LevelDef g_levels[] = {
  { "JUST WALK",           "FUN",    20, 10, 50, 240, 180,  72, 1150, 129,   0, {0,0,0,0,0,0,0,0}, 0, p1 },
  { "MIND THE GAP",        "FUN",    20, 12, 45, 300, 140,  72, 1100, 129,   0, {0,0,0,0,3,0,0,0}, 0, p2 },
  { "DIG DEEP",            "FUN",    20, 12, 50, 300, 150,  40,  560, 149,   0, {0,0,0,0,0,0,0,3}, 0, p3 },
  { "TURN BACK",           "FUN",    30, 20, 50, 300, 600,  60,  200, 109,   0, {0,0,0,2,0,0,0,0}, 300, p4 },
  { "UP AND OVER",         "FUN",    20, 12, 50, 300, 200,  72,  900, 129,   0, {15,0,0,0,0,0,0,0}, 0, p5 },
  { "PARACHUTE PLEASE",    "FUN",    10,  8, 40, 300, 100,  10,  760, 149,   0, {0,8,0,0,0,0,0,0}, 0, p6 },
  { "BLAST OFF",           "TRICKY", 20, 10, 50, 360, 200,  72,  900, 129,   0, {0,0,3,3,0,0,0,0}, 0, p7 },
  { "TUNNEL VISION",       "TRICKY", 20,  8, 50, 360, 150,  20,  560, 149,   0, {0,0,0,0,0,0,3,1}, 0, p8 },
  { "BASH ROAD",           "TRICKY", 20, 15, 50, 360, 150,  72, 1150, 129,   0, {0,0,0,0,0,3,0,0}, 0, p9 },
  { "STEEL YOURSELF",      "TRICKY", 20, 12, 50, 420, 200,  72, 1150, 129,   0, {12,0,0,1,0,2,0,0}, 0, p10 },
  { "SPLASH",              "TRICKY", 20, 10, 50, 420, 150,  72,  900, 129, 145, {0,0,0,0,4,0,0,0}, 0, p11 },
  { "THE LONG WAY ROUND",  "TAXING", 30, 20, 50, 480, 150,  72, 1100, 129,   0, {0,0,0,0,2,3,0,0}, 0, p12 },
};

const int g_nlevels = (int)(sizeof(g_levels) / sizeof(g_levels[0]));
