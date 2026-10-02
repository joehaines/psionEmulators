// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "present.h"
#include "rt.h"

typedef unsigned int u32;
typedef unsigned char u8;

// What the screen shows, as far as this program knows. Same layout as g_fb.
static u8 g_shown[SCR_W * SCR_H / 2] __attribute__((aligned(4)));

static inline int px(const u8 *b, int x, int y)
{
    u8 v = b[(y * SCR_W + x) >> 1];
    return (x & 1) ? v >> 4 : v & 15;
}

static inline void setpx(u8 *b, int x, int y, int g)
{
    u8 *p = &b[(y * SCR_W + x) >> 1];
    if (x & 1) *p = (u8)((*p & 0x0f) | (g << 4));
    else       *p = (u8)((*p & 0xf0) | g);
}

static int g_force;         // the next present paints everything in its rectangles

void present_reset(void)
{
    memset(g_shown, 0, sizeof g_shown);
    g_force = 1;
}

void present_diff(const Rect *dirty, int n, PresentEmit emit)
{
    int i, force = g_force;
    g_force = 0;
    for (i = 0; i < n; i++) {
        int x0 = dirty[i].x, y0 = dirty[i].y, x1 = x0 + dirty[i].w, y1 = y0 + dirty[i].h;
        int x, y;
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > SCR_W) x1 = SCR_W;
        if (y1 > SCR_H) y1 = SCR_H;
        for (y = y0; y < y1; y++) {
            for (x = x0; x < x1; x++) {
                int g, xe, ye, xx, yy;
                // Most of the screen is unchanged: pass over equal runs a word, then a byte, at a time.
                if (!force && !(x & 7) && x + 8 <= x1 &&
                    *(const u32 *)&g_fb[(y * SCR_W + x) >> 1] == *(const u32 *)&g_shown[(y * SCR_W + x) >> 1]) { x += 7; continue; }
                if (!force && !(x & 1) && x + 1 < x1 &&
                    g_fb[(y * SCR_W + x) >> 1] == g_shown[(y * SCR_W + x) >> 1]) { x++; continue; }
                g = px(g_fb, x, y);
                if (!force && g == px(g_shown, x, y)) continue;
                // A rectangle may cover pixels that are already right (painting
                // them again is harmless), so it runs as far right as the colour
                // does, and as far down as every pixel of the span is that colour.
                for (xe = x + 1; xe < x1 && px(g_fb, xe, y) == g; xe++) {}
                for (ye = y + 1; ye < y1; ye++) {
                    for (xx = x; xx < xe; xx++)
                        if (px(g_fb, xx, ye) != g) break;
                    if (xx < xe) break;
                }
                for (yy = y; yy < ye; yy++)
                    for (xx = x; xx < xe; xx++) setpx(g_shown, xx, yy, g);
                emit(x, y, xe, ye, g);
                x = xe - 1;
            }
        }
    }
}
