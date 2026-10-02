// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
//
// Spike: what does UserSvr::ScreenInfo (exec 0x80007c) give a user program?
// Reuses the Lemmings window-server scaffolding to put text on screen.
#include "../../lemmings/src/platform.h"
#include "../../lemmings/src/gfx.h"

extern int ScreenInfo(unsigned *info);      // svc 0x80007c; start.S
extern unsigned TickCountProbe(void);       // svc 0x8000be, a guess
extern void Exec_WaitForAnyRequest(void);
extern void Exec_After(int microseconds, volatile int *status);
const unsigned char *spike_pad(void);
extern const unsigned char g_pad[];

int game_init_and_run(void)
{
    volatile unsigned buf[2 + 6 + 8];           // a TBuf8<24>: length|type, max length, data
    Rect all = {0, 0, SCR_W, SCR_H};
    int i, b;
    if (plat_init() != 0) return 1;
    for (i = 0; i < 16; i++) buf[i] = 0;
    buf[0] = 3u << 28;                          // EBuf, length 0
    buf[1] = 24;
    gfx_clip(0, 0, SCR_W, SCR_H);
    gfx_fill(0, 0, SCR_W, SCR_H, 15);
    plat_present(&all, 1);
    {
        // can a program write to its own code section (where .rodata lives)?
        volatile unsigned char *p = (volatile unsigned char *)g_pad;
        unsigned r[2];
        p[0] = 99;
        p[1] = p[0] + 1;
        r[0] = p[0]; r[1] = p[1];
        gfx_fill(0, 0, SCR_W, SCR_H, 15);
        for (i = 0; i < 2; i++)
            for (b = 0; b < 32; b++)
                if ((r[i] >> (31 - b)) & 1) gfx_fill(60 + b * 16, 10 + i * 30, 12, 24, 0);
                else gfx_fill(60 + b * 16 + 5, 10 + i * 30 + 11, 2, 2, 0);
        { Rect bars = {0, 0, SCR_W, 80}; plat_present(&bars, 1); }
    }
    for (;;) { InEvent e[4]; int t; plat_wait(e, 4, &t); }
}

// Padding: tools/e32/romfs1.mts replaces a bigger file than the one it
// takes over by borrowing a donor's space (the way the Lemmings test does);
// an EXE smaller than "Welcome to Series 5" goes in place, and that image
// does not boot. Referenced so the linker keeps it.
const unsigned char g_pad[9000] = { 1 };
const unsigned char *spike_pad(void) { return g_pad; }
