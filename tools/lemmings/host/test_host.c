// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Runs the game headless on a PC: plays scripted solutions through the
// rules and checks the outcome, and can dump the screen as a PGM.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/game.h"
#include "../src/gfx.h"
#include "../src/palette.h"
#include "../src/present.h"

unsigned char g_fb[SCR_W * SCR_H / 2];
int plat_init(void) { return 0; }
void plat_shutdown(void) {}
int plat_wait(InEvent *e, int m, int *t) { (void)e; (void)m; *t = 1; return 0; }
// The present path the Series 5 uses, counted: how many filled rectangles a frame costs.
static long g_rects, g_frames, g_maxrects;
static int g_rects_frame;
static int g_hist[600];
static void count_emit(int x0, int y0, int x1, int y1, int g) { (void)x0; (void)y0; (void)x1; (void)y1; (void)g; g_rects++; g_rects_frame++; }
void plat_present(const Rect *d, int n)
{
    g_rects_frame = 0;
    present_diff(d, n, count_emit);
    if (g_frames < 600) g_hist[g_frames] = g_rects_frame;
    g_frames++;
    if (g_rects_frame > g_maxrects) g_maxrects = g_rects_frame;
}
unsigned plat_seed(void) { return 1; }
int plat_should_stop(void) { return 0; }

// A screenshot of g_fb: colour (PPM) if the path says .ppm, else the grey panel's view (PGM).
static void dump(const char *path)
{
    FILE *f = fopen(path, "wb");
    int x, y, ppm = strlen(path) > 4 && !strcmp(path + strlen(path) - 4, ".ppm");
    if (!f) return;
    fprintf(f, "%s\n%d %d\n255\n", ppm ? "P6" : "P5", SCR_W, SCR_H);
    for (y = 0; y < SCR_H; y++)
        for (x = 0; x < SCR_W; x++) {
            unsigned char v = g_fb[(y * SCR_W + x) >> 1];
            v = (x & 1) ? v >> 4 : v & 15;
            if (ppm) fwrite(PAL_RGB[v], 1, 3, f);
            else fputc(PAL_GREY[v] * 17, f);
        }
    fclose(f);
}

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

// A script step: when some lemming reaches x (walking right) give it a skill.
typedef struct { int skill, x; } Step;

static int play(int level, const Step *steps, int nsteps, int maxticks)
{
    int t, si = 0;
    game_start_level(level);
    for (t = 0; t < maxticks && !game_info(INFO_OVER); t++) {
        game_sim(1);
        if (si < nsteps) {
            int i, x, y, d, st;
            for (i = 0; (st = game_lem_state(i, &x, &y, &d)) != ST_NONE; i++) {
                if (st == ST_WALK && d > 0 && x >= steps[si].x) {
                    if (game_assign_near(steps[si].skill, x, y - 4)) { if (getenv("LT_DEBUG")) printf("   step %d at t%d x%d y%d\n", si, t, x, y); si++; break; }
                }
            }
        }
    }
    return game_info(INFO_SAVED);
}

int main(int argc, char **argv)
{
    int s;
    static const Step none[1] = {{0,0}};
    static const Step l2[] = {{SK_BUILD, 500},{SK_BUILD, 532}};
    static const Step l3[] = {{SK_DIG, 340}};
    static const Step l5[] = {{SK_CLIMB,500},{SK_CLIMB,500},{SK_CLIMB,500},{SK_CLIMB,500},{SK_CLIMB,500},{SK_CLIMB,500},{SK_CLIMB,500},{SK_CLIMB,500},{SK_CLIMB,500},{SK_CLIMB,500},{SK_CLIMB,500},{SK_CLIMB,500},{SK_CLIMB,500}};
    static const Step l9[] = {{SK_BASH, 397},{SK_BASH, 747}};
    game_init(1);
    s = play(0, none, 0, 16 * 300);
    printf("L1 saved %d/20\n", s);
    CHECK(s >= 10, "level 1 should be won");
    s = play(1, l2, 2, 16 * 300);
    printf("L2 saved %d\n", s);
    CHECK(s >= 12, "L2 should be won");
    s = play(2, l3, 1, 16 * 300);
    printf("L3 saved %d\n", s);
    CHECK(s >= 12, "L3 should be won");
    s = play(4, l5, 13, 16 * 300);
    printf("L5 saved %d\n", s);
    CHECK(s >= 12, "L5 should be won");
    s = play(8, l9, 2, 16 * 300);
    printf("L9 saved %d\n", s);
    CHECK(s >= 15, "L9 should be won");
    {
        static const Step l4[] = {{SK_BLOCK, 850}};
        static Step l6[8], l10[13], l12[5];
        static const Step l7[] = {{SK_BOMB, 418}};
        static const Step l8[] = {{SK_DIG, 500}};
        static const Step l11[] = {{SK_BUILD, 500}, {SK_BUILD, 524}};
        int i;
        for (i = 0; i < 8; i++) { l6[i].skill = SK_FLOAT; l6[i].x = 150; }
        for (i = 0; i < 12; i++) { l10[i].skill = SK_CLIMB; l10[i].x = 480; }
        l10[12].skill = SK_BASH; l10[12].x = 797;
        l12[0].skill = SK_BUILD; l12[0].x = 435;
        l12[1].skill = SK_BUILD; l12[1].x = 458;
        l12[2].skill = SK_BASH; l12[2].x = 897;
        s = play(3, l4, 1, 16 * 300);  printf("L4 saved %d\n", s);  CHECK(s >= 20, "L4 should be won");
        s = play(5, l6, 8, 16 * 300);  printf("L6 saved %d\n", s);  CHECK(s >= 8, "L6 should be won");
        s = play(6, l7, 1, 16 * 360);  printf("L7 saved %d\n", s);  CHECK(s >= 10, "L7 should be won");
        s = play(7, l8, 1, 16 * 360);  printf("L8 saved %d\n", s);  CHECK(s >= 8, "L8 should be won");
        s = play(9, l10, 13, 16 * 420); printf("L10 saved %d\n", s); CHECK(s >= 12, "L10 should be won");
        s = play(10, l11, 2, 16 * 420); printf("L11 saved %d\n", s); CHECK(s >= 10, "L11 should be won");
        s = play(11, l12, 3, 16 * 480); printf("L12 saved %d\n", s); CHECK(s >= 20, "L12 should be won");
    }
    {   // a busy level through the real frame loop, counting what the window server would be asked
        int t;
        game_init(1);
        present_reset();
        game_start_level(0);
        g_rects = g_frames = g_maxrects = 0;
        for (t = 0; t < 500; t++) game_frame(0, 0, 1);
        if (getenv("LT_HIST")) for (t = 0; t < 120; t++) printf("%d ", g_hist[t]);
        printf("\npresent: %ld frames, %ld rectangles, %.1f a frame, worst %ld\n",
               g_frames, g_rects, g_frames ? (double)g_rects / g_frames : 0.0, g_maxrects);
        // What a frame costs on a Series 5 is the window server's answer to each of these,
        // about half a millisecond apiece, so they are budgeted.
        CHECK(g_rects / g_frames <= 250, "an average frame should cost 250 rectangles or fewer");
        CHECK(g_maxrects <= 8000, "even a full repaint should cost 8000 rectangles or fewer");
    }
    game_start_level(0);
    game_sim(200);
    game_render_all();
    if (argc > 1) dump(argv[1]);
    if (argc > 2) {     // the title, and a busier level, for looking at
        char name[256];
        game_init(1);
        game_sim(0);
        game_render_all();
        snprintf(name, sizeof name, "%s-title.ppm", argv[2]);
        dump(name);
        game_start_level(9);
        game_sim(600);
        game_render_all();
        snprintf(name, sizeof name, "%s-l10.ppm", argv[2]);
        dump(name);
    }
    printf(fails ? "FAILED\n" : "OK\n");
    return fails != 0;
}
