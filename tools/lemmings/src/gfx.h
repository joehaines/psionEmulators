// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Drawing into g_fb. Everything is clipped to the rectangle set with
// gfx_clip(), which is how the game repaints only what changed: it sets
// the clip to a dirty rectangle and draws the whole scene, and only the
// pixels inside that rectangle are touched.

#ifndef LEMMINGS_GFX_H
#define LEMMINGS_GFX_H

#include "platform.h"

// A sprite is rows of characters, one per pixel: '.' is see-through and
// '0'..'9','a'..'f' are the sixteen grey levels, black to white.
typedef struct {
    int w, h;
    const char *const *rows;
} Sprite;

void gfx_clip(int x, int y, int w, int h);
void gfx_clip_rect(const Rect *r);
const Rect *gfx_getclip(void);

void gfx_pixel(int x, int y, int c);
int  gfx_get(int x, int y);
void gfx_fill(int x, int y, int w, int h, int c);
void gfx_frame(int x, int y, int w, int h, int c);    // one-pixel outline
void gfx_sprite(const Sprite *s, int x, int y, int flip);
void gfx_sprite_scaled(const Sprite *s, int x, int y, int flip, int scale);   // scale 1 or 2

// 5x7 text, 6 pixels to the character; 'scale' 1 or 2.
void gfx_text(int x, int y, const char *s, int c, int scale);
int  gfx_text_w(const char *s, int scale);

// Decimal, since the runtime has no printf.
int  fmt_int(char *buf, int v);
void fmt_2d(char *buf, int v);       // two digits, zero padded

#endif
