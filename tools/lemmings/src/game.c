// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The game: terrain, lemmings, the eight skills, the panel, and the title
// and result screens. It knows nothing about the machine it runs on —
// platform.h is the whole of that — so the same file plays on a PC in the
// tests and on a Series 5.
//
// Two rules of the house, both from the ARM710a: no division except by a
// power of two (or through udivmod), and no 16-bit loads, so every array
// here is of bytes or ints.

#include "game.h"
#include "gfx.h"
#include "sprites.h"
#include "rt.h"
#include "palette.h"

#define VIEW_W   640
#define VIEW_H   160
#define PANEL_Y  160
#define MAXLEM   100
#define TPS      16                     // game ticks a second
#define FATAL_FALL 60                   // pixels; more than this and it is a splat
#define BRICKS   12

// ── Terrain ─────────────────────────────────────────────────────────
//
// Four bits a pixel. 0 is empty air, 1-7 is earth of one kind or another
// (which is only what colour it is drawn), and 8 up is steel, which
// nothing can dig through.

static unsigned char g_terr[LEVEL_W * LEVEL_H / 2];

static inline int t_get(int x, int y)
{
    unsigned char v;
    if ((unsigned)x >= LEVEL_W) return 15;              // the sides of the world are steel
    if ((unsigned)y >= LEVEL_H) return 0;               // above and below are air
    v = g_terr[(y * LEVEL_W + x) >> 1];
    return (x & 1) ? (v >> 4) : (v & 15);
}

static inline void t_put(int x, int y, int n)
{
    unsigned char *p;
    if ((unsigned)x >= LEVEL_W || (unsigned)y >= LEVEL_H) return;
    p = &g_terr[(y * LEVEL_W + x) >> 1];
    if (x & 1) *p = (unsigned char)((*p & 0x0f) | (n << 4));
    else       *p = (unsigned char)((*p & 0xf0) | n);
}

static inline int solid(int x, int y) { return t_get(x, y) != 0; }
static inline int steel(int x, int y) { return t_get(x, y) >= 8; }

// ── The level being played ──────────────────────────────────────────

typedef struct {
    int state, x, y, dir;
    int frame;              // animation counter
    int t;                  // ticks in the current state
    int n;                  // bricks left, dig cycles, and so on
    int climber, floater;
    int fall;               // pixels fallen so far
    int bomb;               // ticks until it goes off; 0 for not a bomber
} Lem;

static Lem g_lem[MAXLEM];

static int g_mode, g_level;
static int g_scroll;
static int g_out, g_saved, g_dead, g_spawned, g_time, g_ticks;
static int g_rate, g_rate_min, g_next_spawn, g_door_open;
static int g_skill_left[SK_COUNT];
static int g_sel_skill;
static int g_paused, g_fast, g_nuked, g_over, g_won;
static int g_water;
static int g_nuke_arm;                  // ticks left in which a second press means it
static int g_cx, g_cy;                  // the cursor, in screen pixels
static int g_hover;                     // lemming under the cursor, or -1
static int g_menu_sel;
static int g_keys[8];                   // held: left right up down
static int g_pen_down, g_pen_minimap;
static int g_result_ticks;
static unsigned g_status_sig;
static unsigned g_seed;

static unsigned rnd(void)
{
    g_seed = g_seed * 1103515245u + 12345u;
    return g_seed >> 16;
}

// ── Dirty rectangles ────────────────────────────────────────────────

#define MAXDIRTY 48
static Rect g_dirty[MAXDIRTY];
static int g_ndirty, g_full;

static void dirty_add(int x, int y, int w, int h)
{
    int i, x1 = x + w, y1 = y + h;
    if (g_full) return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > SCR_W) x1 = SCR_W;
    if (y1 > SCR_H) y1 = SCR_H;
    if (x1 <= x || y1 <= y) return;
    for (i = 0; i < g_ndirty; i++) {
        Rect *d = &g_dirty[i];
        // Touching or overlapping: grow the old one to take in the new.
        if (x <= d->x + d->w && x1 >= d->x && y <= d->y + d->h && y1 >= d->y) {
            int nx0 = x < d->x ? x : d->x, ny0 = y < d->y ? y : d->y;
            int nx1 = x1 > d->x + d->w ? x1 : d->x + d->w;
            int ny1 = y1 > d->y + d->h ? y1 : d->y + d->h;
            d->x = nx0; d->y = ny0; d->w = nx1 - nx0; d->h = ny1 - ny0;
            return;
        }
    }
    if (g_ndirty >= MAXDIRTY) { g_full = 1; return; }
    g_dirty[g_ndirty].x = x; g_dirty[g_ndirty].y = y;
    g_dirty[g_ndirty].w = x1 - x; g_dirty[g_ndirty].h = y1 - y;
    g_ndirty++;
}

static void dirty_all(void) { g_full = 1; }

// Something changed in the level, at level coordinates.
static void mark_lvl(int lx, int ly, int w, int h)
{
    int sx = lx - g_scroll;
    if (sx >= VIEW_W || sx + w <= 0 || ly >= VIEW_H || ly + h <= 0) return;
    dirty_add(sx, ly, w, h);
}

static void mark_lem(const Lem *l)
{
    // The sprite, 15 pixels tall at most and 14 wide for the pop — and the
    // fuse's digit above the head when it has one.
    int up = l->bomb > 0 ? 21 : 16;
    mark_lvl(l->x - 8, l->y - up, 17, up + 3);
}

// ── Painting the terrain ────────────────────────────────────────────

static int earth_at(int y) { return 1 + ((y >> 3) & 1); }

static void paint_rect(int x, int y, int w, int h, int kind)
{
    int xx, yy;
    for (yy = y; yy < y + h; yy++)
        for (xx = x; xx < x + w; xx++) {
            int n = kind == 0 ? 0 : kind == 8 ? 8 + ((xx >> 3) & 1) : earth_at(yy);
            t_put(xx, yy, n);
        }
}

static void paint_ramp(int x, int y, int w, int h, int up)
{
    int i, yy;
    for (i = 0; i < w; i++) {
        int rise = udivmod((unsigned)((i + 1) * h), (unsigned)w, 0);
        int top = up ? y + h - rise : y + h - (h - rise) - 0;
        if (!up) top = y + rise;
        for (yy = top; yy < y + h; yy++) t_put(x + i, yy, earth_at(yy));
    }
}

static void paint_disc(int cx, int cy, int r, int kind)
{
    int xx, yy;
    for (yy = -r; yy <= r; yy++)
        for (xx = -r; xx <= r; xx++)
            if (xx * xx + yy * yy <= r * r)
                t_put(cx + xx, cy + yy, kind ? earth_at(cy + yy) : 0);
}

static void build_level(const LevelDef *ld)
{
    const Prim *p;
    int x, y;
    memset(g_terr, 0, sizeof g_terr);
    for (p = ld->prims; p->kind != P_END; p++) {
        switch (p->kind) {
        case P_RECT:    paint_rect(p->a, p->b, p->c, p->d, 1); break;
        case P_CARVE:   paint_rect(p->a, p->b, p->c, p->d, 0); break;
        case P_STEEL:   paint_rect(p->a, p->b, p->c, p->d, 8); break;
        case P_RAMP_UP: paint_ramp(p->a, p->b, p->c, p->d, 1); break;
        case P_RAMP_DN: paint_ramp(p->a, p->b, p->c, p->d, 0); break;
        case P_DISC:    paint_disc(p->a, p->b, p->c, 1); break;
        case P_CDISC:   paint_disc(p->a, p->b, p->c, 0); break;
        }
    }
    // A pale top to the earth, so the ground reads as ground.
    for (x = 0; x < LEVEL_W; x++)
        for (y = 1; y < LEVEL_H; y++) {
            int n = t_get(x, y);
            if (n >= 1 && n <= 2 && !solid(x, y - 1)) {
                t_put(x, y, 3);
                if (t_get(x, y + 1) >= 1 && t_get(x, y + 1) <= 2) t_put(x, y + 1, 3);
            }
        }
}

// Remove earth — never steel — from a rectangle. Returns how much steel got in the way.
static int carve_rect(int x, int y, int w, int h)
{
    int xx, yy, hit = 0;
    for (yy = y; yy < y + h; yy++)
        for (xx = x; xx < x + w; xx++) {
            int n = t_get(xx, yy);
            if (n >= 8) hit++;
            else if (n) t_put(xx, yy, 0);
        }
    mark_lvl(x, y, w, h);
    return hit;
}

static int count_steel(int x, int y, int w, int h)
{
    int xx, yy, c = 0;
    for (yy = y; yy < y + h; yy++)
        for (xx = x; xx < x + w; xx++) if (steel(xx, yy)) c++;
    return c;
}

static int count_solid(int x, int y, int w, int h)
{
    int xx, yy, c = 0;
    for (yy = y; yy < y + h; yy++)
        for (xx = x; xx < x + w; xx++) if (solid(xx, yy)) c++;
    return c;
}

// ── Lemmings ────────────────────────────────────────────────────────

static void lem_set(Lem *l, int state)
{
    l->state = state;
    l->t = 0;
    l->frame = 0;
}

static int interval_for(int rate)   // ticks between lemmings at a release rate
{
    return 4 + (((99 - rate) * 13) >> 5);
}

static void spawn(void)
{
    int i;
    const LevelDef *ld = &g_levels[g_level];
    for (i = 0; i < MAXLEM; i++) {
        if (g_lem[i].state == ST_NONE) {
            Lem *l = &g_lem[i];
            memset(l, 0, sizeof *l);
            l->x = ld->entr_x;
            l->y = ld->entr_y + 7;
            l->dir = 1;
            l->state = ST_FALL;
            g_spawned++;
            g_out++;
            g_door_open = 12;
            mark_lem(l);
            mark_lvl(ld->entr_x - 16, ld->entr_y - 2, 32, 14);
            return;
        }
    }
}

static void lem_leave(Lem *l, int saved)
{
    mark_lem(l);
    l->state = ST_NONE;
    g_out--;
    if (saved) g_saved++; else g_dead++;
}

static int blocker_near(const Lem *me, int nx)
{
    int i;
    for (i = 0; i < MAXLEM; i++) {
        const Lem *b = &g_lem[i];
        if (b == me || b->state != ST_BLOCK) continue;
        if (nx - b->x >= -5 && nx - b->x <= 5 && b->y - me->y >= -9 && b->y - me->y <= 9)
            return 1;
    }
    return 0;
}

static void land(Lem *l, int floating)
{
    if (!floating && l->fall > FATAL_FALL) {
        lem_set(l, ST_SPLAT);
        l->t = 0;
    } else {
        lem_set(l, ST_WALK);
    }
    l->fall = 0;
}

static void start_fall(Lem *l)
{
    lem_set(l, ST_FALL);
    l->fall = 0;
}

// The lemming's own explosion: a shallow crater, and the end of it.
static void explode(Lem *l)
{
    int cx = l->x, cy = l->y - 5, xx, yy;
    for (yy = -10; yy <= 10; yy++)
        for (xx = -13; xx <= 13; xx++)
            if (xx * xx * 100 + yy * yy * 169 <= 169 * 100 && t_get(cx + xx, cy + yy) != 0
                && !steel(cx + xx, cy + yy))
                t_put(cx + xx, cy + yy, 0);
    mark_lvl(cx - 14, cy - 11, 29, 23);
}

static int lem_at(const Lem *l) { return (int)(l - g_lem); }

static void step_walk(Lem *l)
{
    int nx = l->x + l->dir, h = 0, d;
    l->frame++;
    if (blocker_near(l, nx)) { l->dir = -l->dir; return; }
    while (h <= 6 && solid(nx, l->y - h)) h++;
    if (h > 6) {
        if (l->climber) { lem_set(l, ST_CLIMB); }
        else l->dir = -l->dir;
        return;
    }
    l->x = nx;
    l->y -= h;
    if (!solid(l->x, l->y + 1)) {
        for (d = 1; d <= 3 && !solid(l->x, l->y + 1 + d); d++) {}
        if (d <= 3) l->y += d;
        else start_fall(l);
    }
}

static void step_fall(Lem *l, int floating)
{
    int k, per = floating ? 2 : 3;
    l->frame++;
    for (k = 0; k < per; k++) {
        if (solid(l->x, l->y + 1)) { land(l, floating); return; }
        l->y++;
        l->fall++;
        if (l->y >= LEVEL_H) { lem_leave(l, 0); return; }
    }
    if (!floating && l->floater && l->fall >= 24) { lem_set(l, ST_FLOAT); }
}

static void step_climb(Lem *l)
{
    l->frame++;
    if (solid(l->x, l->y - 10)) {               // hit the roof: let go
        l->dir = -l->dir;
        start_fall(l);
        return;
    }
    if (!solid(l->x + l->dir, l->y)) {          // over the top
        l->x += l->dir;
        lem_set(l, ST_WALK);
        return;
    }
    l->y--;
}

static void step_build(Lem *l)
{
    l->frame++;
    if (l->t == 0) {
        int i;
        // Is there room for the brick and for the lemming above it?
        if (solid(l->x + l->dir * 3, l->y - 10) || solid(l->x + l->dir * 6, l->y - 9)) {
            l->dir = -l->dir;
            lem_set(l, ST_WALK);
            return;
        }
        for (i = 1; i <= 6; i++) t_put(l->x + l->dir * i, l->y, 4);
        mark_lvl((l->dir > 0 ? l->x : l->x - 6), l->y, 7, 1);
    }
    if (l->t == 5) {
        l->x += l->dir * 2;
        l->y -= 1;
        l->n++;
    }
    l->t++;
    if (l->t >= 8) {
        l->t = 0;
        if (l->n >= BRICKS) lem_set(l, ST_SHRUG);
    }
}

static void step_bash(Lem *l)
{
    int fx = l->dir > 0 ? l->x + 1 : l->x - 5;
    l->frame++;
    if (count_steel(fx, l->y - 8, 5, 9)) { l->dir = -l->dir; lem_set(l, ST_WALK); return; }
    if (!count_solid(fx, l->y - 8, 5, 9)) { lem_set(l, ST_WALK); return; }
    if ((l->frame & 1) == 0) {
        carve_rect(fx, l->y - 8, 5, 9);
        l->x += l->dir;
    }
    if (!solid(l->x, l->y + 1)) {
        int d;
        for (d = 1; d <= 3 && !solid(l->x, l->y + 1 + d); d++) {}
        if (d <= 3) l->y += d; else start_fall(l);
    }
}

static void step_mine(Lem *l)
{
    l->frame++;
    l->t++;
    if ((l->t & 3) == 0) {
        int nx = l->x + l->dir * 2;
        int cx = nx - 4;
        if (count_steel(cx, l->y - 8, 9, 10)) { l->dir = -l->dir; lem_set(l, ST_WALK); return; }
        carve_rect(cx, l->y - 8, 9, 10);
        l->x = nx;
        l->y += 1;
        if (!solid(l->x, l->y + 1)) {
            int d;
            for (d = 1; d <= 3 && !solid(l->x, l->y + 1 + d); d++) {}
            if (d <= 3) l->y += d; else start_fall(l);
        }
    }
}

static void step_dig(Lem *l)
{
    l->frame++;
    l->t++;
    if ((l->t & 3) == 0) {
        if (count_steel(l->x - 4, l->y + 1, 9, 1)) { lem_set(l, ST_WALK); return; }
        carve_rect(l->x - 4, l->y + 1, 9, 1);
        l->y += 1;
        if (!solid(l->x, l->y + 1)) {
            if (count_solid(l->x - 4, l->y + 1, 9, 3) == 0) start_fall(l);
        }
    }
}

static void lem_step(Lem *l)
{
    const LevelDef *ld = &g_levels[g_level];
    int i;

    if (l->state == ST_NONE) return;

    // The bomber's clock runs whatever else it is doing.
    if (l->bomb > 0 && l->state != ST_OHNO && l->state != ST_POP) {
        if (--l->bomb == 0) { lem_set(l, ST_OHNO); return; }
    }

    switch (l->state) {
    case ST_WALK:   step_walk(l);  break;
    case ST_FALL:   step_fall(l, 0); break;
    case ST_FLOAT:  step_fall(l, 1); break;
    case ST_CLIMB:  step_climb(l); break;
    case ST_BUILD:  step_build(l); break;
    case ST_BASH:   step_bash(l);  break;
    case ST_MINE:   step_mine(l);  break;
    case ST_DIG:    step_dig(l);   break;
    case ST_BLOCK:
        l->frame++;
        if (!solid(l->x, l->y + 1)) start_fall(l);
        break;
    case ST_SHRUG:
        l->frame++;
        if (++l->t >= 10) lem_set(l, ST_WALK);
        break;
    case ST_OHNO:
        l->frame++;
        if (++l->t >= 10) { lem_set(l, ST_POP); explode(l); }
        break;
    case ST_POP:
        if (++l->t >= 8) lem_leave(l, 0);
        return;
    case ST_SPLAT:
        if (++l->t >= 16) lem_leave(l, 0);
        return;
    case ST_DROWN:
        l->frame++;
        if (++l->t >= 16) lem_leave(l, 0);
        return;
    case ST_EXIT:
        l->frame++;
        if (++l->t >= 12) lem_leave(l, 1);
        return;
    }
    if (l->state == ST_NONE) return;

    // Water and the edge of the world.
    if (ld->water && l->y >= ld->water && l->state != ST_POP) {
        lem_set(l, ST_DROWN);
        return;
    }
    if (l->y >= LEVEL_H - 1 && l->state == ST_FALL) { lem_leave(l, 0); return; }

    // The way out.
    i = l->x - ld->exit_x;
    if ((l->state == ST_WALK || l->state == ST_FALL || l->state == ST_FLOAT)
        && i >= -3 && i <= 3 && l->y >= ld->exit_y - 2 && l->y <= ld->exit_y + 2) {
        lem_set(l, ST_EXIT);
    }
}

static void sim_tick(void)
{
    int i;
    const LevelDef *ld = &g_levels[g_level];

    if (g_over) return;
    g_ticks++;

    if (g_time > 0 && (g_ticks & (TPS - 1)) == 0) {
        g_time--;
        dirty_add(0, 161, 460, 16);
    }

    if (g_nuke_arm > 0) g_nuke_arm--;

    // The trapdoor.
    if (g_door_open > 0) {
        g_door_open--;
        if (g_door_open == 0) mark_lvl(ld->entr_x - 16, ld->entr_y - 2, 32, 14);
    }
    if (g_spawned < ld->total && !g_nuked && g_ticks >= 30) {
        if (--g_next_spawn <= 0) {
            spawn();
            g_next_spawn = interval_for(g_rate);
        }
    }

    for (i = 0; i < MAXLEM; i++) {
        Lem *l = &g_lem[i];
        if (l->state == ST_NONE) continue;
        mark_lem(l);
        lem_step(l);
        if (l->state != ST_NONE) mark_lem(l);
    }

    // Out of time, or nobody left to come.
    if (g_time <= 0 || (g_spawned >= ld->total && g_out == 0) || (g_nuked && g_out == 0)) {
        g_over = 1;
        g_won = g_saved >= ld->required;
        g_result_ticks = 0;
    }
}

// ── Giving out skills ───────────────────────────────────────────────

static int lem_can(const Lem *l, int skill)
{
    switch (skill) {
    case SK_CLIMB: return !l->climber && l->state != ST_POP && l->state != ST_OHNO;
    case SK_FLOAT: return !l->floater && l->state != ST_POP && l->state != ST_OHNO;
    case SK_BOMB:  return l->bomb == 0 && l->state != ST_OHNO && l->state != ST_POP
                          && l->state != ST_SPLAT && l->state != ST_DROWN && l->state != ST_EXIT;
    case SK_BLOCK: return l->state == ST_WALK;
    case SK_BUILD: return l->state == ST_WALK;
    case SK_BASH:  return l->state == ST_WALK;
    case SK_MINE:  return l->state == ST_WALK;
    case SK_DIG:   return l->state == ST_WALK;
    }
    return 0;
}

static void lem_apply(Lem *l, int skill)
{
    switch (skill) {
    case SK_CLIMB: l->climber = 1; break;
    case SK_FLOAT: l->floater = 1; break;
    case SK_BOMB:  l->bomb = 5 * TPS; break;
    case SK_BLOCK: lem_set(l, ST_BLOCK); break;
    case SK_BUILD: lem_set(l, ST_BUILD); l->n = 0; break;
    case SK_BASH:  lem_set(l, ST_BASH); break;
    case SK_MINE:  lem_set(l, ST_MINE); break;
    case SK_DIG:   lem_set(l, ST_DIG); break;
    }
}

// The lemming nearest a spot, in level coordinates, within reach of a fingertip.
static int lem_near(int lx, int ly, int skill)
{
    int i, best = -1, bd = 1 << 30;
    for (i = 0; i < MAXLEM; i++) {
        const Lem *l = &g_lem[i];
        int dx, dy, d;
        if (l->state == ST_NONE) continue;
        if (skill >= 0 && !lem_can(l, skill)) continue;
        if (skill < 0 && (l->state == ST_POP || l->state == ST_SPLAT || l->state == ST_EXIT
                          || l->state == ST_DROWN)) continue;
        dx = lx - l->x; dy = ly - (l->y - 4);
        if (dx < -8 || dx > 8 || dy < -9 || dy > 9) continue;
        d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

int game_assign_near(int skill, int x, int y)
{
    int i;
    if (skill < 0 || skill >= SK_COUNT || g_skill_left[skill] <= 0) return 0;
    i = lem_near(x, y, skill);
    if (i < 0) return 0;
    lem_apply(&g_lem[i], skill);
    g_skill_left[skill]--;
    mark_lem(&g_lem[i]);
    dirty_add(0, PANEL_Y, VIEW_W, SCR_H - PANEL_Y);
    return 1;
}

void game_set_rate(int rate)
{
    if (rate < g_rate_min) rate = g_rate_min;
    if (rate > 99) rate = 99;
    if (rate != g_rate) { g_rate = rate; dirty_add(0, PANEL_Y, VIEW_W, SCR_H - PANEL_Y); }
}

void game_nuke(void)
{
    int i, k = 0;
    if (g_nuked) return;
    g_nuked = 1;
    for (i = 0; i < MAXLEM; i++) {
        Lem *l = &g_lem[i];
        if (l->state != ST_NONE && l->bomb == 0 && l->state != ST_POP && l->state != ST_OHNO
            && l->state != ST_SPLAT && l->state != ST_DROWN && l->state != ST_EXIT) {
            l->bomb = 3 * TPS + (k & 7) * 3;
            k++;
        }
    }
    dirty_add(0, PANEL_Y, VIEW_W, SCR_H - PANEL_Y);
}

// ── Starting a level ────────────────────────────────────────────────

void game_start_level(int level)
{
    const LevelDef *ld;
    int i;
    if (level < 0) level = 0;
    if (level >= g_nlevels) level = g_nlevels - 1;
    g_level = level;
    ld = &g_levels[level];
    build_level(ld);
    memset(g_lem, 0, sizeof g_lem);
    g_out = g_saved = g_dead = g_spawned = g_ticks = 0;
    g_time = ld->seconds;
    g_rate = g_rate_min = ld->rate;
    g_next_spawn = 1;
    g_door_open = 0;
    for (i = 0; i < SK_COUNT; i++) g_skill_left[i] = ld->skills[i];
    g_sel_skill = -1;
    for (i = 0; i < SK_COUNT; i++) if (ld->skills[i]) { g_sel_skill = i; break; }
    if (g_sel_skill < 0) g_sel_skill = SK_DIG;
    g_paused = g_fast = g_nuked = g_over = g_won = 0;
    g_nuke_arm = 0;
    g_water = ld->water;
    g_scroll = ld->scroll;
    g_cx = VIEW_W / 2; g_cy = VIEW_H / 2;
    g_hover = -1;
    memset(g_keys, 0, sizeof g_keys);
    g_pen_down = 0;
    g_mode = MODE_PLAY;
    dirty_all();
}

// ── Info for the tests ──────────────────────────────────────────────

int game_info(int what)
{
    int i, n = 0;
    switch (what) {
    case INFO_OUT:     return g_out;
    case INFO_SAVED:   return g_saved;
    case INFO_DEAD:    return g_dead;
    case INFO_SPAWNED: return g_spawned;
    case INFO_TIME:    return g_time;
    case INFO_ACTIVE:  for (i = 0; i < MAXLEM; i++) if (g_lem[i].state != ST_NONE) n++; return n;
    case INFO_OVER:    return g_over;
    case INFO_WON:     return g_won;
    case INFO_MODE:    return g_mode;
    case INFO_LEVEL:   return g_level;
    case INFO_RATE:    return g_rate;
    case INFO_SCROLL:  return g_scroll;
    }
    return 0;
}

int game_lem_state(int n, int *x, int *y, int *dir)
{
    int i, k = 0;
    for (i = 0; i < MAXLEM; i++) {
        if (g_lem[i].state == ST_NONE) continue;
        if (k++ == n) {
            if (x) *x = g_lem[i].x;
            if (y) *y = g_lem[i].y;
            if (dir) *dir = g_lem[i].dir;
            return g_lem[i].state;
        }
    }
    return ST_NONE;
}

int game_terrain(int x, int y) { return t_get(x, y); }

void game_sim(int ticks)
{
    while (ticks-- > 0) sim_tick();
}

// ── Drawing ─────────────────────────────────────────────────────────
//
// Everything is drawn with palette indices (palette.h); the platform
// decides whether they are colours or greys.

static const unsigned char TLUT[16] = {
    C_SKY1, C_EARTH1, C_EARTH2, C_GRASS, C_BRICK, C_EARTH1, C_EARTH2, C_EARTH2,
    C_STEEL1, C_STEEL2, C_STEEL1, C_STEEL2, C_STEEL1, C_STEEL2, C_STEEL1, C_STEEL2
};

// The sky is two blues with a hard join. (A dithered join looks better and
// costs a window-server call for nearly every pixel of it; see present.c.)
static inline int sky_at(int x, int y)
{
    (void)x;
    return y < 44 ? C_SKY2 : C_SKY1;
}

static void draw_terrain(int x0, int y0, int x1, int y1)
{
    int x, y;
    for (y = y0; y < y1; y++) {
        unsigned char *frow = &g_fb[(y * SCR_W) >> 1];
        const unsigned char *trow = &g_terr[(y * LEVEL_W) >> 1];
        int wet = g_water && y >= g_water;
        for (x = x0; x < x1; x++) {
            int lx = x + g_scroll;
            int nb = (lx & 1) ? (trow[lx >> 1] >> 4) : (trow[lx >> 1] & 15);
            int g = TLUT[nb];
            if (nb == 0) {
                if (wet) {
                    g = C_WATER;
                    if (y == g_water && (((lx + (g_ticks >> 2)) & 7) < 3)) g = C_SKY1;
                } else g = sky_at(x, y);
            } else if (nb == 4) {
                // bricks: mortar between the courses and along each one
                if ((y & 3) == 3 || ((lx + ((y >> 2) & 1) * 4) & 7) == 0) g = C_WOOD;
            } else if (nb >= 8) {
                if ((lx & 7) == 3 && (y & 7) == 3) g = C_BLACK;        // a rivet
            }
            if (x & 1) frow[x >> 1] = (unsigned char)((frow[x >> 1] & 0x0f) | (g << 4));
            else       frow[x >> 1] = (unsigned char)((frow[x >> 1] & 0xf0) | g);
        }
    }
}

// Clouds drift at half the speed of the ground and are drawn only where
// the sky shows, so they pass behind the land.
static const char *const r_cloud[] = {
    "......aaaa..........",
    "....aaffffaa....aa..",
    "..aaffffffffaaaaffaa",
    ".affffffffffffffffffa",
    "aaffffffffffffffffffa",
    ".aaaaaaaaaaaaaaaaaaa."
};
static const Sprite SPR_CLOUD = { 21, 6, r_cloud };
static const int CLOUD_X[5] = { 60, 380, 700, 960, 1180 };
static const int CLOUD_Y[5] = { 8, 22, 6, 20, 10 };

static void draw_clouds(void)
{
    int i, r, c;
    for (i = 0; i < 5; i++) {
        int cx = CLOUD_X[i] - (g_scroll >> 1), cy = CLOUD_Y[i];
        if (cx + 21 <= gfx_getclip()->x || cx >= gfx_getclip()->x + gfx_getclip()->w) continue;
        if (cy + 6 <= gfx_getclip()->y || cy >= gfx_getclip()->y + gfx_getclip()->h) continue;
        for (r = 0; r < 6; r++)
            for (c = 0; c < 21; c++) {
                char ch = SPR_CLOUD.rows[r][c];
                int px = cx + c, py = cy + r;
                if (ch == '.') continue;
                if (t_get(px + g_scroll, py) != 0) continue;
                gfx_pixel(px, py, ch == 'f' ? C_WHITE : C_SKY1);
            }
    }
}

static void lem_draw(const Lem *l)
{
    const Sprite *s;
    int sx, sy, flip = l->dir < 0;
    switch (l->state) {
    case ST_FALL:   s = &SPR_FALL[(l->frame >> 2) & 1]; break;
    case ST_WALK:   s = &SPR_WALK[(l->frame >> 1) & 3]; break;
    case ST_CLIMB:  s = &SPR_CLIMB[(l->frame >> 2) & 1]; break;
    case ST_FLOAT:  s = &SPR_FLOAT[(l->frame >> 2) & 1]; break;
    case ST_BLOCK:  s = &SPR_BLOCK[(l->frame >> 3) & 1]; break;
    case ST_BUILD:  s = &SPR_BUILD[(l->frame >> 1) & 3]; break;
    case ST_SHRUG:  s = &SPR_SHRUG[(l->frame >> 2) & 1]; break;
    case ST_BASH:   s = &SPR_BASH[(l->frame >> 1) & 3]; break;
    case ST_MINE:   s = &SPR_MINE[(l->frame >> 2) & 3]; break;
    case ST_DIG:    s = &SPR_DIG[(l->frame >> 2) & 1]; break;
    case ST_OHNO:   s = &SPR_OHNO[(l->frame >> 2) & 1]; break;
    case ST_POP:    s = &SPR_POP[0]; break;
    case ST_SPLAT:  s = &SPR_SPLAT[0]; break;
    case ST_DROWN:  s = &SPR_DROWN[(l->frame >> 2) & 1]; break;
    case ST_EXIT:   s = &SPR_EXITING[(l->frame >> 1) & 1]; break;
    default: return;
    }
    sx = l->x - g_scroll - s->w / 2;
    sy = l->y - s->h + 1;
    gfx_sprite(s, sx, sy, flip);
    if (l->bomb > 0 && l->state != ST_POP && l->state != ST_OHNO) {
        char d[2];
        int bx = l->x - g_scroll - 3;
        d[0] = (char)('0' + ((l->bomb + 15) >> 4));
        d[1] = 0;
        gfx_fill(bx - 1, sy - 10, 7, 9, C_BLACK);
        gfx_text(bx, sy - 9, d, C_WHITE, 1);
    }
}

static void draw_objects(void)
{
    const LevelDef *ld = &g_levels[g_level];
    int fl = (g_ticks >> 2) & 1;
    // trapdoor
    gfx_sprite(&SPR_TRAP[g_door_open > 0 ? 1 : 0], ld->entr_x - g_scroll - 14, ld->entr_y, 0);
    // the exit and its flames
    gfx_sprite(&SPR_DOOR[0], ld->exit_x - g_scroll - 11, ld->exit_y - 16, 0);
    gfx_sprite(&SPR_FLAME[fl], ld->exit_x - g_scroll - 20, ld->exit_y - 5, 0);
    gfx_sprite(&SPR_FLAME[1 - fl], ld->exit_x - g_scroll + 15, ld->exit_y - 5, 0);
}

static void draw_cursor(void)
{
    int x = g_cx, y = g_cy;
    // white with a black edge reads on sky and on earth alike
    gfx_fill(x - 8, y - 1, 7, 3, C_BLACK);
    gfx_fill(x + 2, y - 1, 7, 3, C_BLACK);
    gfx_fill(x - 1, y - 8, 3, 7, C_BLACK);
    gfx_fill(x - 1, y + 2, 3, 7, C_BLACK);
    gfx_fill(x - 7, y, 5, 1, C_WHITE);
    gfx_fill(x + 3, y, 5, 1, C_WHITE);
    gfx_fill(x, y - 7, 1, 5, C_WHITE);
    gfx_fill(x, y + 3, 1, 5, C_WHITE);
    if (g_hover >= 0) {
        const Lem *l = &g_lem[g_hover];
        int sx = l->x - g_scroll - 6, sy = l->y - 12;
        gfx_frame(sx - 1, sy - 1, 15, 16, C_BLACK);
        gfx_frame(sx, sy, 13, 14, C_ORANGE);
    }
}

// ── The panel ───────────────────────────────────────────────────────

#define BTN_Y   186
#define BTN_H   52
#define BTN_W   48
#define BTN_X0  12
#define BTN_STEP 50
#define NBTN    12
#define MAP_X   468
#define MAP_Y   162

static const char *const SKILL_NAME[SK_COUNT] = { "CLIMB", "FLOAT", "BOMB", "BLOCK", "BUILD", "BASH", "MINE", "DIG" };

// A skill's picture is the lemming doing it, at twice the size.
static void draw_skill_icon(int k, int x, int y)
{
    const Sprite *s;
    switch (k) {
    case SK_CLIMB: s = &SPR_CLIMB[0]; break;
    case SK_FLOAT: s = &SPR_FLOAT_TOP; break;
    case SK_BOMB:  s = &SPR_OHNO[0]; break;
    case SK_BLOCK: s = &SPR_BLOCK[0]; break;
    case SK_BUILD: s = &SPR_BUILD[2]; break;
    case SK_BASH:  s = &SPR_BASH[0]; break;
    case SK_MINE:  s = &SPR_MINE[0]; break;
    default:       s = &SPR_DIG[0]; break;
    }
    gfx_sprite_scaled(s, x + 24 - s->w, y, 0, 2);
}

static void draw_button(int i)
{
    int x = BTN_X0 + i * BTN_STEP, on = 0, c, face;
    char num[12];
    if (i >= 2 && i < 2 + SK_COUNT) on = (g_sel_skill == i - 2);
    if (i == 10) on = g_paused;
    if (i == 11) on = g_nuke_arm > 0 || g_nuked;
    face = on ? C_SHIRT : C_STEEL2;
    c = on ? C_WHITE : C_BLACK;
    gfx_fill(x, BTN_Y, BTN_W, BTN_H, face);
    // a raised edge when up, a sunk one when down
    gfx_fill(x, BTN_Y, BTN_W, 1, on ? C_STEEL1 : C_WHITE);
    gfx_fill(x, BTN_Y, 1, BTN_H, on ? C_STEEL1 : C_WHITE);
    gfx_fill(x, BTN_Y + BTN_H - 1, BTN_W, 1, on ? C_WHITE : C_STEEL1);
    gfx_fill(x + BTN_W - 1, BTN_Y, 1, BTN_H, on ? C_WHITE : C_STEEL1);
    gfx_frame(x - 1, BTN_Y - 1, BTN_W + 2, BTN_H + 1, C_BLACK);
    if (i == 0 || i == 1) {
        gfx_text(x + 8, BTN_Y + 5, "RATE", c, 1);
        gfx_fill(x + 14, BTN_Y + 22, 20, 4, c);
        if (i == 1) gfx_fill(x + 22, BTN_Y + 14, 4, 20, c);
        fmt_int(num, i == 0 ? g_rate_min : g_rate);
        gfx_text(x + 24 - (int)(gfx_text_w(num, 1) >> 1), BTN_Y + 40, num, c, 1);
    } else if (i >= 2 && i < 2 + SK_COUNT) {
        int k = i - 2;
        const char *nm = SKILL_NAME[k];
        fmt_int(num, g_skill_left[k]);
        gfx_text(x + 24 - (gfx_text_w(nm, 1) >> 1), BTN_Y + 4, nm, c, 1);
        draw_skill_icon(k, x, BTN_Y + 13);
        gfx_text(x + 24 - (gfx_text_w(num, 2) >> 1), BTN_Y + 36, num, c, 2);
    } else if (i == 10) {
        gfx_text(x + 9, BTN_Y + 5, "PAUSE", c, 1);
        gfx_fill(x + 15, BTN_Y + 16, 6, 22, c);
        gfx_fill(x + 27, BTN_Y + 16, 6, 22, c);
    } else {
        gfx_text(x + 12, BTN_Y + 5, "NUKE", c, 1);
        gfx_sprite_scaled(&SPR_POP[0], x + 10, BTN_Y + 16, 0, 2);
    }
}

static void draw_status(void)
{
    char b[24], *p = b;
    const LevelDef *ld = &g_levels[g_level];
    int m = g_time > 0 ? g_time : 0, mm, ss, pct;
    unsigned rem;
    gfx_fill(0, 161, 466, 22, C_STEEL2);
    gfx_text(6, 164, ld->name, C_BLACK, 1);
    gfx_text(gfx_text_w(ld->name, 1) + 14, 164, ld->rating, C_SHIRT, 1);
    gfx_text(6, 174, "OUT", C_BLACK, 1);
    fmt_int(b, g_out);
    gfx_text(30, 174, b, C_BLACK, 1);
    gfx_text(60, 174, "IN", C_BLACK, 1);
    pct = ld->total ? (int)udivmod((unsigned)(g_saved * 100), (unsigned)ld->total, 0) : 0;
    p = b;
    p += fmt_int(p, pct);
    *p++ = '%'; *p = 0;
    gfx_text(76, 174, b, C_BLACK, 1);
    gfx_text(118, 174, "NEED", C_BLACK, 1);
    pct = ld->total ? (int)udivmod((unsigned)(ld->required * 100), (unsigned)ld->total, 0) : 0;
    p = b;
    p += fmt_int(p, pct);
    *p++ = '%'; *p = 0;
    gfx_text(148, 174, b, C_BLACK, 1);
    gfx_text(196, 174, "TIME", C_BLACK, 1);
    mm = (int)udivmod((unsigned)m, 60, &rem);
    ss = (int)rem;
    p = b;
    p += fmt_int(p, mm);
    *p++ = ':';
    fmt_2d(p, ss);
    gfx_text(226, 174, b, m <= 30 ? C_ORANGE : C_BLACK, 1);
    gfx_text(270, 174, "RATE", C_BLACK, 1);
    fmt_int(b, g_rate);
    gfx_text(300, 174, b, C_BLACK, 1);
    if (g_paused) gfx_text(392, 174, "PAUSED", C_SHIRT, 1);
    else if (g_fast) gfx_text(392, 174, "FAST", C_SHIRT, 1);
}

static void draw_minimap(void)
{
    int mx, my, i;
    gfx_fill(MAP_X - 1, MAP_Y - 1, 162, 22, C_BLACK);
    for (my = 0; my < 20; my++)
        for (mx = 0; mx < 160; mx++) {
            int n = t_get(mx * 8 + 4, my * 8 + 4);
            gfx_pixel(MAP_X + mx, MAP_Y + my,
                      n == 0 ? (g_water && my * 8 + 4 >= g_water ? C_WATER : C_SKY1)
                             : n >= 8 ? C_STEEL1 : n == 3 ? C_GRASS : C_EARTH2);
        }
    for (i = 0; i < MAXLEM; i++)
        if (g_lem[i].state != ST_NONE)
            gfx_pixel(MAP_X + (g_lem[i].x >> 3), MAP_Y + (g_lem[i].y >> 3), C_WHITE);
    gfx_frame(MAP_X + (g_scroll >> 3), MAP_Y, VIEW_W >> 3, 20, C_ORANGE);
}

static int hits(const Rect *r, int x, int y, int w, int h)
{
    return r->x < x + w && r->x + r->w > x && r->y < y + h && r->y + r->h > y;
}

// Draw only the parts of the panel that the rectangle touches. The panel
// is the dear part of a frame — the minimap alone is 3,200 lookups — and
// most frames change none of it.
static void draw_panel(const Rect *r)
{
    int i;
    gfx_fill(0, PANEL_Y, VIEW_W, SCR_H - PANEL_Y, C_STEEL2);
    gfx_fill(0, PANEL_Y, VIEW_W, 1, C_BLACK);
    if (hits(r, 0, 161, 466, 22)) draw_status();
    if (hits(r, MAP_X - 1, MAP_Y - 1, 162, 22)) draw_minimap();
    for (i = 0; i < NBTN; i++)
        if (hits(r, BTN_X0 + i * BTN_STEP, BTN_Y, BTN_W, BTN_H)) draw_button(i);
}

// ── The title screen and the result ─────────────────────────────────

#define MENU_X0  24
#define MENU_Y0  82
#define MENU_ROW 15

static void draw_title(void)
{
    int i, x, y;
    const char *tag = "A GAME OF SAVING THE LITTLE FELLOWS";
    // sky, and a range of hills along the bottom; only as much of it as the clip needs
    {
        const Rect *cl = gfx_getclip();
        for (y = cl->y; y < cl->y + cl->h; y++)
            for (x = cl->x; x < cl->x + cl->w; x++)
                gfx_pixel(x, y, sky_at(x, (y * 171) >> 8));
    }
    for (x = 0; x < SCR_W; x += 4) {
        int h = 16 + (((x * 3) >> 2) % 37) - (((x >> 3) & 7) * 2);
        int top = 200 - h;
        gfx_fill(x, top, 4, 200 - top, (x >> 6) & 1 ? C_GRASS : C_EARTH2);
    }
    gfx_fill(0, 200, SCR_W, 40, C_EARTH1);
    gfx_fill(0, 200, SCR_W, 4, C_GRASS);
    for (x = 0; x < SCR_W; x += 8) gfx_fill(x + ((x >> 3) & 3), 210 + ((x >> 4) & 15), 3, 2, C_EARTH2);
    // the name, with a shadow
    x = SCR_W / 2 - gfx_text_w("LEMMINGS", 6) / 2;
    gfx_text(x + 3, 11, "LEMMINGS", C_BLACK, 6);
    gfx_text(x, 8, "LEMMINGS", C_ORANGE, 6);
    gfx_text(SCR_W / 2 - gfx_text_w(tag, 1) / 2 + 1, 59, tag, C_WHITE, 1);
    gfx_text(SCR_W / 2 - gfx_text_w(tag, 1) / 2, 58, tag, C_BLACK, 1);
    // the level list on a panel
    gfx_fill(MENU_X0 - 12, MENU_Y0 - 10, 616, 118, C_STEEL2);
    gfx_frame(MENU_X0 - 12, MENU_Y0 - 10, 616, 118, C_BLACK);
    gfx_fill(MENU_X0 - 11, MENU_Y0 - 9, 614, 1, C_WHITE);
    for (i = 0; i < g_nlevels; i++) {
        int col = i >= 6, row = i - col * 6, sel = i == g_menu_sel;
        char b[8];
        x = MENU_X0 + col * 300;
        y = MENU_Y0 + row * MENU_ROW;
        if (sel) gfx_fill(x - 4, y - 3, 288, 13, C_SHIRT);
        fmt_int(b, i + 1);
        gfx_text(x, y, b, sel ? C_WHITE : C_BLACK, 1);
        gfx_text(x + 20, y, g_levels[i].name, sel ? C_WHITE : C_BLACK, 1);
        gfx_text(x + 220, y, g_levels[i].rating, sel ? C_ORANGE : C_STEEL1, 1);
    }
    gfx_text(24, 176, "ARROWS CHOOSE    ENTER PLAY    ESC QUIT", C_BLACK, 1);
    // a few lemmings out for a stroll along the hilltop
    for (i = 0; i < 4; i++) {
        unsigned rem;
        udivmod((unsigned)(g_ticks * 2 + i * 190), SCR_W + 40, &rem);
        gfx_sprite(&SPR_WALK[((g_ticks >> 1) + i) & 3], (int)rem - 20, 192, 0);
    }
}

static void draw_result(void)
{
    const LevelDef *ld = &g_levels[g_level];
    char b[40], *p;
    int x = 120, y = 40, w = 400, h = 120, pct;
    gfx_fill(x + 4, y + 4, w, h, C_BLACK);
    gfx_fill(x, y, w, h, C_STEEL2);
    gfx_frame(x, y, w, h, C_BLACK);
    gfx_frame(x + 2, y + 2, w - 4, h - 4, C_WHITE);
    gfx_text(x + w / 2 - gfx_text_w(ld->name, 2) / 2, y + 10, ld->name, C_BLACK, 2);
    p = b;
    p += fmt_int(p, g_saved);
    *p++ = ' '; *p++ = 'O'; *p++ = 'F'; *p++ = ' ';
    p += fmt_int(p, ld->total);
    *p++ = ' '; *p++ = 'S'; *p++ = 'A'; *p++ = 'V'; *p++ = 'E'; *p++ = 'D'; *p = 0;
    gfx_text(x + w / 2 - gfx_text_w(b, 2) / 2, y + 36, b, C_BLACK, 2);
    pct = ld->total ? (int)udivmod((unsigned)(ld->required * 100), (unsigned)ld->total, 0) : 0;
    p = b;
    *p++ = 'Y'; *p++ = 'O'; *p++ = 'U'; *p++ = ' '; *p++ = 'N'; *p++ = 'E'; *p++ = 'E'; *p++ = 'D'; *p++ = 'E'; *p++ = 'D'; *p++ = ' ';
    p += fmt_int(p, pct);
    *p++ = '%'; *p = 0;
    gfx_text(x + w / 2 - gfx_text_w(b, 1) / 2, y + 62, b, C_BLACK, 1);
    if (g_won) gfx_text(x + w / 2 - gfx_text_w("WELL DONE", 2) / 2, y + 76, "WELL DONE", C_GRASS, 2);
    else       gfx_text(x + w / 2 - gfx_text_w("NOT THIS TIME", 2) / 2, y + 76, "NOT THIS TIME", C_ORANGE, 2);
    gfx_text(x + w / 2 - gfx_text_w("PRESS ENTER", 1) / 2, y + 102, "PRESS ENTER", C_SHIRT, 1);
}

// ── Putting it together ─────────────────────────────────────────────

static void compose(const Rect *r0)
{
    Rect r = *r0;
    int i;
    if (r.x < 0) { r.w += r.x; r.x = 0; }
    if (r.y < 0) { r.h += r.y; r.y = 0; }
    if (r.x + r.w > SCR_W) r.w = SCR_W - r.x;
    if (r.y + r.h > SCR_H) r.h = SCR_H - r.y;
    if (r.w <= 0 || r.h <= 0) return;

    if (g_mode == MODE_TITLE) {
        gfx_clip_rect(&r);
        draw_title();
        return;
    }
    if (r.y < VIEW_H) {
        int vh = r.h < VIEW_H - r.y ? r.h : VIEW_H - r.y;
        gfx_clip(r.x, r.y, r.w, vh);
        draw_terrain(r.x, r.y, r.x + r.w, r.y + vh);
        draw_clouds();
        draw_objects();
        for (i = 0; i < MAXLEM; i++) if (g_lem[i].state != ST_NONE) lem_draw(&g_lem[i]);
        if (!g_over) draw_cursor();
    }
    if (r.y + r.h > PANEL_Y) {
        gfx_clip_rect(&r);
        draw_panel(&r);
    }
    if (g_mode == MODE_RESULT || g_over) {
        gfx_clip_rect(&r);
        if (g_mode == MODE_RESULT) draw_result();
    }
}

void game_render_all(void)
{
    Rect all;
    all.x = 0; all.y = 0; all.w = SCR_W; all.h = SCR_H;
    compose(&all);
    g_ndirty = 0; g_full = 0;
}

static void flush_display(void)
{
    int i;
    if (g_full) {
        Rect all;
        all.x = 0; all.y = 0; all.w = SCR_W; all.h = SCR_H;
        compose(&all);
        plat_present(&all, 1);
    } else if (g_ndirty) {
        for (i = 0; i < g_ndirty; i++) compose(&g_dirty[i]);
        plat_present(g_dirty, g_ndirty);
    }
    g_ndirty = 0;
    g_full = 0;
}

void game_init(unsigned seed)
{
    gfx_clip(0, 0, SCR_W, SCR_H);
    g_seed = seed ? seed : 12345;
    g_mode = MODE_TITLE;
    g_menu_sel = 0;
    g_ticks = 0;
    g_level = 0;
    dirty_all();
}

static void set_scroll(int s)
{
    if (s < 0) s = 0;
    if (s > LEVEL_W - VIEW_W) s = LEVEL_W - VIEW_W;
    if (s != g_scroll) {
        g_scroll = s;
        dirty_add(0, 0, VIEW_W, VIEW_H);
        dirty_add(MAP_X - 1, MAP_Y - 1, 162, 22);
    }
}

static void update_hover(void)
{
    int h = lem_near(g_cx + g_scroll, g_cy, -1);
    if (h != g_hover) {
        if (g_hover >= 0) mark_lem(&g_lem[g_hover]);
        g_hover = h;
        if (h >= 0) mark_lem(&g_lem[h]);
    }
}

static void move_cursor(int nx, int ny)
{
    if (nx < 0) nx = 0;
    if (nx > VIEW_W - 1) nx = VIEW_W - 1;
    if (ny < 0) ny = 0;
    if (ny > VIEW_H - 1) ny = VIEW_H - 1;
    if (nx != g_cx || ny != g_cy) {
        dirty_add(g_cx - 9, g_cy - 9, 19, 19);
        g_cx = nx; g_cy = ny;
        dirty_add(g_cx - 9, g_cy - 9, 19, 19);
    }
}

static void select_skill(int k)
{
    if (k < 0 || k >= SK_COUNT || g_sel_skill == k) return;
    g_sel_skill = k;
    dirty_add(0, BTN_Y, VIEW_W, BTN_H);
}

static void press_button(int i)
{
    if (i == 0) game_set_rate(g_rate - 5 < g_rate_min ? g_rate_min : g_rate - 5);
    else if (i == 1) game_set_rate(g_rate + 5);
    else if (i < 2 + SK_COUNT) select_skill(i - 2);
    else if (i == 10) {
        g_paused = !g_paused;
        dirty_add(0, PANEL_Y, VIEW_W, SCR_H - PANEL_Y);
    } else {
        if (g_nuke_arm > 0) game_nuke();
        else g_nuke_arm = TPS * 2;
        dirty_add(0, PANEL_Y, VIEW_W, SCR_H - PANEL_Y);
    }
}

static void do_assign_at_cursor(void)
{
    if (g_sel_skill >= 0) game_assign_near(g_sel_skill, g_cx + g_scroll, g_cy);
}

static void pen_at(int x, int y, int down)
{
    if (y >= PANEL_Y) {
        if (down) {
            int i;
            if (y >= MAP_Y && y < MAP_Y + 20 && x >= MAP_X && x < MAP_X + 160) {
                g_pen_minimap = 1;
                set_scroll(((x - MAP_X) << 3) - VIEW_W / 2);
                return;
            }
            for (i = 0; i < NBTN; i++) {
                int bx = BTN_X0 + i * BTN_STEP;
                if (x >= bx && x < bx + BTN_W && y >= BTN_Y && y < BTN_Y + BTN_H) { press_button(i); return; }
            }
        }
        return;
    }
    move_cursor(x, y);
    if (down) {
        update_hover();
        do_assign_at_cursor();
    }
}

static void play_event(const InEvent *e)
{
    switch (e->type) {
    case EV_KEYDOWN:
        switch (e->a) {
        case SC_LEFT:  g_keys[0] = 1; break;
        case SC_RIGHT: g_keys[1] = 1; break;
        case SC_UP:    g_keys[2] = 1; break;
        case SC_DOWN:  g_keys[3] = 1; break;
        case SC_ENTER: case SC_SPACE:
            if (g_over) { g_mode = MODE_RESULT; dirty_all(); }
            else do_assign_at_cursor();
            break;
        case SC_ESC:
            g_mode = MODE_TITLE;
            dirty_all();
            break;
        case SC_TAB:
            select_skill((g_sel_skill + 1) & 7);
            break;
        }
        break;
    case EV_KEYUP:
        switch (e->a) {
        case SC_LEFT:  g_keys[0] = 0; break;
        case SC_RIGHT: g_keys[1] = 0; break;
        case SC_UP:    g_keys[2] = 0; break;
        case SC_DOWN:  g_keys[3] = 0; break;
        }
        break;
    case EV_CHAR: {
        int c = e->a;
        if (c >= 'A' && c <= 'Z') c += 32;
        if (c >= '1' && c <= '8') select_skill(c - '1');
        else if (c == 'p') { g_paused = !g_paused; dirty_add(0, PANEL_Y, VIEW_W, SCR_H - PANEL_Y); }
        else if (c == 'f') { g_fast = !g_fast; dirty_add(0, PANEL_Y, VIEW_W, SCR_H - PANEL_Y); }
        else if (c == '+' || c == '=') game_set_rate(g_rate + 5);
        else if (c == '-' || c == '_') game_set_rate(g_rate - 5);
        else if (c == 'n') press_button(11);
        else if (c == 'r') game_start_level(g_level);
        break;
    }
    case EV_PEN_DOWN: g_pen_down = 1; g_pen_minimap = 0; pen_at(e->a, e->b, 1); break;
    case EV_PEN_MOVE:
        if (g_pen_down) {
            if (g_pen_minimap && e->b >= PANEL_Y) set_scroll(((e->a - MAP_X) << 3) - VIEW_W / 2);
            else if (e->b < PANEL_Y) pen_at(e->a, e->b, 0);
        }
        break;
    case EV_PEN_UP:   g_pen_down = 0; g_pen_minimap = 0; break;
    case EV_FOCUS:
        if (!e->a && !g_paused) { g_paused = 1; dirty_add(0, PANEL_Y, VIEW_W, SCR_H - PANEL_Y); }
        break;
    }
}

static void title_event(const InEvent *e, int *quit)
{
    int rows = 6;
    switch (e->type) {
    case EV_KEYDOWN:
        switch (e->a) {
        case SC_UP:    if (g_menu_sel > 0) g_menu_sel--; dirty_all(); break;
        case SC_DOWN:  if (g_menu_sel < g_nlevels - 1) g_menu_sel++; dirty_all(); break;
        case SC_LEFT:  if (g_menu_sel >= rows) g_menu_sel -= rows; dirty_all(); break;
        case SC_RIGHT: if (g_menu_sel + rows < g_nlevels) g_menu_sel += rows; dirty_all(); break;
        case SC_ENTER: case SC_SPACE: game_start_level(g_menu_sel); break;
        case SC_ESC:   *quit = 1; break;
        }
        break;
    case EV_PEN_DOWN: {
        int i;
        for (i = 0; i < g_nlevels; i++) {
            int col = i >= 6, row = i - col * 6;
            int x = MENU_X0 + col * 300 - 4, y = MENU_Y0 + row * MENU_ROW - 3;
            if (e->a >= x && e->a < x + 288 && e->b >= y && e->b < y + 13) {
                if (g_menu_sel == i) game_start_level(i);
                else { g_menu_sel = i; dirty_all(); }
            }
        }
        break;
    }
    case EV_QUIT: *quit = 1; break;
    }
}

int game_frame(const InEvent *ev, int n, int ticks)
{
    int i, quit = 0, t;

    for (i = 0; i < n; i++) {
        if (ev[i].type == EV_QUIT) quit = 1;
        if (g_mode == MODE_TITLE) title_event(&ev[i], &quit);
        else if (g_mode == MODE_PLAY) play_event(&ev[i]);
        else if (g_mode == MODE_RESULT) {
            if (ev[i].type == EV_KEYDOWN && (ev[i].a == SC_ENTER || ev[i].a == SC_SPACE)) {
                if (g_won && g_level + 1 < g_nlevels) game_start_level(g_level + 1);
                else if (g_won) { g_mode = MODE_TITLE; dirty_all(); }
                else game_start_level(g_level);
            } else if (ev[i].type == EV_KEYDOWN && ev[i].a == SC_ESC) {
                g_mode = MODE_TITLE; dirty_all();
            } else if (ev[i].type == EV_PEN_DOWN) {
                if (g_won && g_level + 1 < g_nlevels) game_start_level(g_level + 1);
                else if (g_won) { g_mode = MODE_TITLE; dirty_all(); }
                else game_start_level(g_level);
            }
        }
    }
    if (quit) return 1;

    if (g_mode == MODE_TITLE) {
        g_ticks += ticks;
        if (ticks) dirty_add(0, 186, SCR_W, 20);
    } else if (g_mode == MODE_PLAY) {
        int dx = g_keys[1] - g_keys[0], dy = g_keys[3] - g_keys[2];
        if (ticks > 0) {
            if (dx || dy) {
                int nx = g_cx + dx * 5 * ticks, ny = g_cy + dy * 4 * ticks;
                move_cursor(nx, ny);
                if (nx < 24) set_scroll(g_scroll - 24 * ticks);
                if (nx > VIEW_W - 25) set_scroll(g_scroll + 24 * ticks);
            }
            if (!g_paused) {
                int steps = ticks * (g_fast ? 3 : 1);
                for (t = 0; t < steps; t++) {
                    sim_tick();
                    if ((g_ticks & 15) == 0) dirty_add(MAP_X - 1, MAP_Y - 1, 162, 22);
                }
                // The counters change slowly; the rest is marked as it moves.
                {
                    unsigned sig = (unsigned)g_out * 7u + (unsigned)g_saved * 131u + (unsigned)g_time * 4099u
                                   + (unsigned)g_rate * 65537u + (g_paused ? 1u : 0u) + (g_fast ? 2u : 0u);
                    if (sig != g_status_sig) { g_status_sig = sig; dirty_add(0, 161, 466, 22); }
                }
            }
            update_hover();
            if (g_over && g_mode == MODE_PLAY) {
                if ((g_result_ticks += ticks) >= TPS * 2) { g_mode = MODE_RESULT; dirty_all(); }
            }
        }
    }

    flush_display();
    return 0;
}
