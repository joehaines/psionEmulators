// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// What the game needs from a machine, and nothing else. There are two
// implementations: platform_epoc.c, which talks to an EPOC R1/R5 window
// server, and host/platform_host.c, which runs the same game headless on
// a PC so the rules can be tested without booting an emulated Psion.

#ifndef LEMMINGS_PLATFORM_H
#define LEMMINGS_PLATFORM_H

#define SCR_W 640
#define SCR_H 240

// The screen as the game sees it: 4 bits per pixel, two pixels to a byte,
// the left pixel in the low nibble. 0 is black and 15 white — the same 16
// grey levels a Series 5's panel has. Every drawing routine writes here;
// plat_present() then makes the real screen match.
extern unsigned char g_fb[SCR_W * SCR_H / 2];

typedef struct { int x, y, w, h; } Rect;

// Input, as the game wants to hear about it.
enum {
    EV_KEYDOWN,     // a = scan code, b = character code (0 if none yet)
    EV_KEYUP,       // a = scan code
    EV_CHAR,        // a = character code, b = scan code
    EV_PEN_DOWN,    // a = x, b = y (screen pixels)
    EV_PEN_MOVE,    // a = x, b = y
    EV_PEN_UP,      // a = x, b = y
    EV_FOCUS,       // a = 1 gained, 0 lost
    EV_QUIT         // the machine wants us gone
};
typedef struct { int type, a, b; } InEvent;

// Scan codes the game cares about (EPOC's TStdScanCode).
#define SC_ENTER  3
#define SC_ESC    4
#define SC_SPACE  5
#define SC_TAB    2
#define SC_LEFT   14
#define SC_RIGHT  15
#define SC_UP     16
#define SC_DOWN   17

int  plat_init(void);
void plat_shutdown(void);

// Block until the next game tick is due or something is pending. Fills
// `evs` (up to `max`) and returns how many, and says in *ticks how many
// whole ticks have gone by since the last call (usually 1).
int  plat_wait(InEvent *evs, int max, int *ticks);

// Make the screen show g_fb inside the given rectangles (everything
// outside them is unchanged since the last call).
void plat_present(const Rect *dirty, int n);

// A number that changes: for the random generator's seed.
unsigned plat_seed(void);

// Debug/test hook: 1 when the platform was told to stop after N ticks.
int  plat_should_stop(void);

#endif
