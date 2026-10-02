// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#ifndef LEMMINGS_GAME_H
#define LEMMINGS_GAME_H

#include "platform.h"
#include "levels.h"

// One frame of the game: feed it what happened, and it plays `ticks`
// game ticks, repaints what changed and asks the platform to show it.
// Returns 1 when the player has asked to leave.
void game_init(unsigned seed);
int  game_frame(const InEvent *ev, int n, int ticks);

// ── The rules, driven directly, for the tests ───────────────────────
void game_start_level(int level);
void game_sim(int ticks);                       // advance the rules only
int  game_assign_near(int skill, int x, int y); // level coordinates; 1 on success
void game_set_rate(int rate);
void game_nuke(void);
void game_render_all(void);                     // repaint the whole screen

enum { INFO_OUT, INFO_SAVED, INFO_DEAD, INFO_SPAWNED, INFO_TIME, INFO_ACTIVE,
       INFO_OVER, INFO_WON, INFO_MODE, INFO_LEVEL, INFO_RATE, INFO_SCROLL };
int  game_info(int what);

// Where the n-th active lemming is, and what it is doing, for the tests.
int  game_lem_state(int n, int *x, int *y, int *dir);
int  game_terrain(int x, int y);                // 0 empty, 1-7 earth, 8+ steel

enum { MODE_TITLE, MODE_PLAY, MODE_RESULT };
enum { ST_NONE, ST_FALL, ST_WALK, ST_CLIMB, ST_FLOAT, ST_BLOCK, ST_BUILD, ST_SHRUG,
       ST_BASH, ST_MINE, ST_DIG, ST_OHNO, ST_POP, ST_SPLAT, ST_DROWN, ST_EXIT };

#endif
