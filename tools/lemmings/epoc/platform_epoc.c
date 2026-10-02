// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The Series 5 end of the game: a window server session, one full-screen
// backed-up window, and a graphics context to draw into it.
//
// There is no EPOC SDK here, so none of this is written against a header.
// Every call is an ordinal in a DLL (see lemmings.spec) that was found by
// reading the ROM, and every structure is a block of words sized with
// room to spare. What each call needs is written next to it.
//
// Pixels: a Series 5 program would normally BitBlt a bitmap to the
// screen. Bitmaps made with CFbsBitmap::Create cannot be Duplicate()d in
// this environment's font-and-bitmap server, so that road is closed;
// instead each changed run of pixels is painted as a filled rectangle,
// which is what plat_present() does, keeping a copy of what the window
// shows so that only differences are sent.

#include "../src/platform.h"
#include "../src/rt.h"
#include "../src/palette.h"
#include "../src/present.h"

typedef unsigned int u32;
typedef int i32;
typedef unsigned char u8;

extern void *U_cleanup(void);
extern i32   F_connect(void);
extern void *W_sessCtor(void *);
extern i32   W_connect(void *);
extern void *W_grpCtor(void *, void *);
extern i32   W_grpConstruct(void *, u32);
extern void *W_scrCtor(void *, void *);
extern i32   W_scrConstruct(void *);
extern void *W_bwCtor(void *, void *);
extern i32   W_bwConstruct(void *, void *, i32, u32);
extern i32   W_setExt204(void *, void *, void *);
extern void  W_activate(void *);
extern void  W_gcActivate(void *, void *);
extern void  W_flush(void *);
extern void  W_evReady(void *, volatile i32 *);
extern void  W_getEvent(void *, void *);

extern void  Exec_WaitForAnyRequest(void);
extern void  Exec_After(i32 microseconds, volatile i32 *status);   // svc 0xC0005E

#define PEND ((i32)0x80000001u)
// ... of two game ticks each: 16 a second, but drawn eight times a second, because
// what a frame costs on this machine is the window server's answer to each
// filled rectangle and there is not the time to draw sixteen.
#ifndef TICKS_PER_FRAME
#define TICKS_PER_FRAME 2
#endif
#ifndef TICK_US
#define TICK_US 125000                  // a frame every 1/8 s ...
#endif

// Window server event types (TEventCode) and the layout of an event as
// this window server hands it out, found by pressing keys at it: words
// are type, (something), time, window handle, then the payload — for a
// key, the character code, the scan code, modifiers and repeat count.
#ifndef EVLOG
#define EVLOG 0
#endif
enum { WE_KEY = 1, WE_KEYUP = 2, WE_KEYDOWN = 3, WE_POINTER = 5,
       WE_FOCUSLOST = 9, WE_FOCUSGAINED = 10 };

// The window server's client objects, sized well past what this ROM's need: the
// R5 Unicode builds' are larger than R1's, and an overflow here is silent.
static u32 g_sess[256], g_grp[256], g_scr[256], g_win[256];
static void *g_gc;
static u32 ev[64];
static volatile i32 g_evst, g_tmst;
static int g_ev_posted, g_tm_armed;
static u32 g_rgb[16];               // what each palette index is sent to the window server as
static int g_colour;                // 1 on a colour panel
unsigned char g_fb[SCR_W * SCR_H / 2] __attribute__((aligned(4)));

#if EVLOG
static volatile struct { u32 magic; u32 n; u32 v[400]; } __attribute__((aligned(512))) L;
static void lg(u32 x) { if (L.n < 400) L.v[L.n++] = x; }
#else
#define lg(x) ((void)0)
#endif

static inline int px(const u8 *b, int x, int y)
{
    u8 v = b[(y * SCR_W + x) >> 1];
    return (x & 1) ? v >> 4 : v & 15;
}

int plat_init(void)
{
    i32 e;
    void **vt;
#if EVLOG
    L.magic = 0x31584647 ^ 0x01010101;
#endif
    U_cleanup();                        // the cleanup stack the window server client leaves on
    F_connect();

    W_sessCtor(g_sess);
    if (W_connect(g_sess) != 0) return -1;
    W_grpCtor(g_grp, g_sess);
    W_grpConstruct(g_grp, 1);
    W_scrCtor(g_scr, g_sess);
    if (W_scrConstruct(g_scr) != 0) return -2;

    vt = (void **)g_scr[0];
    // Nothing in the screen device says whether the panel is colour: its
    // display-mode word reads the same on a 5mx and a netpad, which the
    // emulator draws through one 16-grey path and one 8-bit colour one.
    // What does differ is the panel's size in twips (words 5 and 6 of the
    // device): a Series 5 or 5mx is 7620 across, the netpad's larger panel
    // 8476. The netpad is the colour machine of this 640 x 240 family, so
    // a panel wider than 8000 twips is taken to be colour.
    {
        int i;
        lg(0xD15A); for (i = 0; i < 9; i++) lg(g_scr[i]);
        g_colour = (i32)g_scr[5] > 8000;
    }
    { int i;
      for (i = 0; i < 16; i++)
          g_rgb[i] = g_colour ? ((u32)PAL_RGB[i][0] | ((u32)PAL_RGB[i][1] << 8) | ((u32)PAL_RGB[i][2] << 16))
                              : (u32)PAL_GREY[i] * 0x111111u;
    }
    e = ((i32 (*)(void *, void **))vt[6])(g_scr, &g_gc);        // CreateContext
    if (e != 0 || !g_gc) return -3;

    W_bwCtor(g_win, g_sess);
    if (W_bwConstruct(g_win, g_grp, 3, 2) != 0) return -4;      // backed-up, so drawing persists
    {
        i32 pt[2] = {0, 0}, sz[2] = {SCR_W, SCR_H};
        W_setExt204(g_win, pt, sz);
    }
    W_activate(g_win);
    W_gcActivate(g_gc, g_win);
    vt = (void **)((u32 *)g_gc)[0];
    ((void (*)(void *))vt[8])(g_gc);                            // Reset
    ((void (*)(void *, u32))vt[16])(g_gc, 0);                   // pen: none
    ((void (*)(void *, u32))vt[19])(g_gc, 1);                   // brush: solid

    // What the window shows to begin with is unknown; make the first
    // present() paint everything.
    present_reset();
    memset(g_fb, 0, sizeof g_fb);
    g_ev_posted = 0;
    g_tmst = 0;
    return 0;
}

void plat_shutdown(void)
{
}

unsigned plat_seed(void)
{
    volatile i32 t = PEND;
    return (unsigned)(u32)&t * 2654435761u;
}

int plat_should_stop(void) { return 0; }

// ── Painting ────────────────────────────────────────────────────────

static void fillrect(int x0, int y0, int x1, int y1, int g, int *cur)
{
#if EVLOG
    L.v[397]++; L.v[398] += (u32)((x1 - x0) * (y1 - y0));
#endif
    void **vt = (void **)((u32 *)g_gc)[0];
    i32 r[4];
    if (g != *cur) {
        u32 rgb = g_rgb[g];
        ((void (*)(void *, void *))vt[18])(g_gc, &rgb);
        *cur = g;
    }
    r[0] = x0; r[1] = y0; r[2] = x1; r[3] = y1;
    ((void (*)(void *, void *))vt[34])(g_gc, r);
}

static int g_emit_cur, g_emit_count;

static void emit_rect(int x0, int y0, int x1, int y1, int g)
{
    fillrect(x0, y0, x1, y1, g, &g_emit_cur);
    if (++g_emit_count >= 150) { W_flush(g_sess); g_emit_count = 0; }
}

void plat_present(const Rect *dirty, int n)
{
#if EVLOG
    u32 r0 = L.v[397];
#endif
    g_emit_cur = -1; g_emit_count = 0;
    present_diff(dirty, n, emit_rect);
#if EVLOG
    L.v[399]++; L.v[394] = L.v[397] - r0; if (L.v[394] > L.v[393]) L.v[393] = L.v[394];
    L.v[392] = (u32)n;
#endif
    W_flush(g_sess);
}

// ── Waiting, and what happened meanwhile ────────────────────────────

static int translate(InEvent *o)
{
    switch ((i32)ev[0]) {
    case WE_KEYDOWN: o->type = EV_KEYDOWN; o->a = (i32)ev[5]; o->b = (i32)ev[4]; return 1;
    case WE_KEYUP:   o->type = EV_KEYUP;   o->a = (i32)ev[5]; o->b = 0;          return 1;
    case WE_KEY:     o->type = EV_CHAR;    o->a = (i32)ev[4]; o->b = (i32)ev[5]; return 1;
    case WE_POINTER: {
        i32 k = (i32)ev[4];
        o->type = k == 0 ? EV_PEN_DOWN : k == 1 ? EV_PEN_UP : EV_PEN_MOVE;
        o->a = (i32)ev[6]; o->b = (i32)ev[7];
        return 1;
    }
    case WE_FOCUSLOST:   o->type = EV_FOCUS; o->a = 0; o->b = 0; return 1;
    case WE_FOCUSGAINED: o->type = EV_FOCUS; o->a = 1; o->b = 0; return 1;
    }
    return 0;
}

int plat_wait(InEvent *evs, int max, int *ticks)
{
    int n = 0;
#if EVLOG
    L.v[396]++;
#endif
    // The timer for this frame was started when the last one began, so that
    // the time a frame takes is counted inside its period and not added to it.
    if (!g_tm_armed) { g_tmst = PEND; Exec_After(TICK_US, &g_tmst); g_tm_armed = 1; }
    for (;;) {
        if (!g_ev_posted) {
            g_evst = PEND;
            W_evReady(g_sess, &g_evst);
            W_flush(g_sess);
            g_ev_posted = 1;
        }
        if (g_evst != PEND) {
            g_ev_posted = 0;
            W_getEvent(g_sess, ev);
#if EVLOG
            { int i; lg(0x3000); for (i = 0; i < 10; i++) lg(ev[i]); }
#endif
            if (n < max && translate(&evs[n])) n++;
            continue;
        }
        if (g_tmst != PEND) break;
        Exec_WaitForAnyRequest();
    }
    g_tmst = PEND;
    Exec_After(TICK_US, &g_tmst);           // the next frame's period starts now
    *ticks = TICKS_PER_FRAME;
    return n;
}
