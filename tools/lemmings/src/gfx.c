// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "gfx.h"
#include "rt.h"

extern const unsigned char g_font5x7[64][7];

static Rect g_cl;       // zero until gfx_clip() — the image has no initialised data

void gfx_clip(int x, int y, int w, int h)
{
    int x1 = x + w, y1 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > SCR_W) x1 = SCR_W;
    if (y1 > SCR_H) y1 = SCR_H;
    g_cl.x = x; g_cl.y = y;
    g_cl.w = x1 > x ? x1 - x : 0;
    g_cl.h = y1 > y ? y1 - y : 0;
}

void gfx_clip_rect(const Rect *r) { gfx_clip(r->x, r->y, r->w, r->h); }
const Rect *gfx_getclip(void) { return &g_cl; }

static inline int in_clip(int x, int y)
{
    return x >= g_cl.x && y >= g_cl.y && x < g_cl.x + g_cl.w && y < g_cl.y + g_cl.h;
}

void gfx_pixel(int x, int y, int c)
{
    unsigned char *p;
    if (!in_clip(x, y)) return;
    p = &g_fb[(y * SCR_W + x) >> 1];
    if (x & 1) *p = (unsigned char)((*p & 0x0f) | (c << 4));
    else       *p = (unsigned char)((*p & 0xf0) | (c & 15));
}

int gfx_get(int x, int y)
{
    unsigned char v;
    if (x < 0 || y < 0 || x >= SCR_W || y >= SCR_H) return 0;
    v = g_fb[(y * SCR_W + x) >> 1];
    return (x & 1) ? (v >> 4) : (v & 15);
}

void gfx_fill(int x, int y, int w, int h, int c)
{
    int x1 = x + w, y1 = y + h, xx, yy;
    if (x < g_cl.x) x = g_cl.x;
    if (y < g_cl.y) y = g_cl.y;
    if (x1 > g_cl.x + g_cl.w) x1 = g_cl.x + g_cl.w;
    if (y1 > g_cl.y + g_cl.h) y1 = g_cl.y + g_cl.h;
    c &= 15;
    for (yy = y; yy < y1; yy++) {
        unsigned char *row = &g_fb[(yy * SCR_W) >> 1];
        xx = x;
        if (xx & 1 && xx < x1) {                       // leading odd pixel
            row[xx >> 1] = (unsigned char)((row[xx >> 1] & 0x0f) | (c << 4));
            xx++;
        }
        for (; xx + 1 < x1; xx += 2)                   // whole bytes
            row[xx >> 1] = (unsigned char)(c | (c << 4));
        if (xx < x1)                                   // trailing even pixel
            row[xx >> 1] = (unsigned char)((row[xx >> 1] & 0xf0) | c);
    }
}

void gfx_frame(int x, int y, int w, int h, int c)
{
    gfx_fill(x, y, w, 1, c);
    gfx_fill(x, y + h - 1, w, 1, c);
    gfx_fill(x, y, 1, h, c);
    gfx_fill(x + w - 1, y, 1, h, c);
}

static const unsigned char HEXV[128] = {
    [0 ... 127] = 255,
    ['0'] = 0, ['1'] = 1, ['2'] = 2, ['3'] = 3, ['4'] = 4, ['5'] = 5, ['6'] = 6, ['7'] = 7,
    ['8'] = 8, ['9'] = 9, ['a'] = 10, ['b'] = 11, ['c'] = 12, ['d'] = 13, ['e'] = 14, ['f'] = 15
};

static inline void put(int x, int y, int v)
{
    unsigned char *p = &g_fb[(y * SCR_W + x) >> 1];
    if (x & 1) *p = (unsigned char)((*p & 0x0f) | (v << 4));
    else       *p = (unsigned char)((*p & 0xf0) | v);
}

// Draw a sprite at 1x or 2x, clipped, straight into the frame buffer:
// the clip is worked out once for the whole sprite, not per pixel.
void gfx_sprite_scaled(const Sprite *s, int x, int y, int flip, int scale)
{
    int r, c, cx0, cx1, cy0, cy1;
    // Cheap rejection first: the game draws every sprite for every dirty
    // rectangle and most of them are nowhere near it.
    if (x >= g_cl.x + g_cl.w || y >= g_cl.y + g_cl.h ||
        x + s->w * scale <= g_cl.x || y + s->h * scale <= g_cl.y) return;
    cx0 = g_cl.x; cx1 = g_cl.x + g_cl.w;
    cy0 = g_cl.y; cy1 = g_cl.y + g_cl.h;
    for (r = 0; r < s->h; r++) {
        const char *row = s->rows[r];
        int py = y + r * scale, k;
        if (py + scale <= cy0 || py >= cy1) continue;
        for (c = 0; c < s->w; c++) {
            int v = HEXV[(unsigned char)row[flip ? s->w - 1 - c : c] & 127], px0 = x + c * scale;
            if (v == 255 || px0 + scale <= cx0 || px0 >= cx1) continue;
            for (k = 0; k < scale; k++) {
                int yy = py + k, xx;
                if (yy < cy0 || yy >= cy1) continue;
                for (xx = px0; xx < px0 + scale; xx++)
                    if (xx >= cx0 && xx < cx1) put(xx, yy, v);
            }
        }
    }
}

void gfx_sprite(const Sprite *s, int x, int y, int flip)
{
    gfx_sprite_scaled(s, x, y, flip, 1);
}

int gfx_text_w(const char *s, int scale)
{
    int n = 0;
    while (*s++) n++;
    return n * 6 * scale;
}

void gfx_text(int x, int y, const char *s, int c, int scale)
{
    for (; *s; s++, x += 6 * scale) {
        int ch = (unsigned char)*s, r, b;
        const unsigned char *g;
        if (ch >= 'a' && ch <= 'z') ch -= 32;
        if (ch < 32 || ch > 95) ch = '?';
        g = g_font5x7[ch - 32];
        if (x + 6 * scale <= g_cl.x || x >= g_cl.x + g_cl.w) continue;
        for (r = 0; r < 7; r++) {
            for (b = 0; b < 5; b++) {
                if (g[r] & (0x10 >> b)) {
                    if (scale == 1) gfx_pixel(x + b, y + r, c);
                    else gfx_fill(x + b * scale, y + r * scale, scale, scale, c);
                }
            }
        }
    }
}

int fmt_int(char *buf, int v)
{
    char tmp[12];
    int n = 0, i = 0;
    unsigned u;
    if (v < 0) { buf[i++] = '-'; u = (unsigned)-v; } else u = (unsigned)v;
    if (u == 0) tmp[n++] = '0';
    while (u) {
        unsigned rem;
        u = udivmod(u, 10, &rem);
        tmp[n++] = (char)('0' + rem);
    }
    while (n) buf[i++] = tmp[--n];
    buf[i] = 0;
    return i;
}

void fmt_2d(char *buf, int v)
{
    unsigned rem, q = udivmod((unsigned)v, 10, &rem);
    if (q > 9) q = 9;
    buf[0] = (char)('0' + q);
    buf[1] = (char)('0' + rem);
    buf[2] = 0;
}
