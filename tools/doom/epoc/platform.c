// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// The EPOC end of Doom: the window server (for the keyboard and for being
// the program the machine shows), the panel's own memory (for pixels), the
// kernel's tick count (for time) and the file server (for the WAD).
//
// There is no EPOC SDK here, so none of this is written against a header.
// Every call is an ordinal in a DLL (see doom.spec) found by reading the
// ROM; tools/doom/NOTES.md says how, and tools/lemmings/epoc/platform_epoc.c
// is where the window-server calls were first worked out.
//
// Pixels: the window server paints through one filled rectangle per run of
// pixels, which is hopeless for a 3D game. But UserSvr::ScreenInfo hands any
// program the address of the panel's own memory, and a program can write
// to it directly. On a Series 5 that is 640 x 240, four bits a pixel, two
// pixels to a byte, the left pixel in the low nibble, 0 black and 15 white.

#include <stdio.h>
#include <string.h>
#include "epoc.h"
#include "doomkeys.h"
#include "doomgeneric.h"

typedef unsigned int u32;
typedef int i32;
typedef unsigned char u8;
typedef unsigned short u16;

// ── the machine's calls (doom.spec) ─────────────────────────────────

extern void *U_cleanup(void);
extern i32   U_screenInfo(void *tdes8);
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
extern void  F_close(void *rfile);
extern i32   F_fsConnect(void *rfs, i32 slots);
extern i32   F_open(void *rfile, void *rfs, const void *name, u32 mode);
extern i32   F_readPos(void *rfile, i32 pos, void *tptr8);

extern void  Exec_WaitForAnyRequest(void);
extern void  Exec_After(i32 microseconds, volatile i32 *status);   // svc 0xC0005E
extern u32   Exec_TickCount(void);                                  // svc 0x800070: 1/64 s

#define PEND ((i32)0x80000001u)

// ── descriptors, as this R1 build lays them out ─────────────────────

#define EPtrC 1u
#define EPtr  2u
#define EBuf  3u
#define TYPE_LEN(t, n) (((t) << 28) | ((n) & 0x0fffffffu))
#define DES_LEN(x)     ((x) & 0x0fffffffu)
typedef struct { u32 iTypeLength; const void *iPtr; } TPtrC8_;
typedef struct { u32 iTypeLength; i32 iMaxLength; void *iPtr; } TPtr8_;

// ── state ───────────────────────────────────────────────────────────

static u32 g_sess[256], g_grp[256], g_scr[256], g_win[256];
static void *g_gc;
static u32 ev[64];
static volatile i32 g_evst;
static int g_ev_posted;

static volatile u32 *g_fb;          // the panel's memory, as ScreenInfo gave it
static int g_w, g_h;                // in pixels
static int g_stride;                // bytes a row
static int g_fb_ok;

// What the panel's memory turned out to be (detect_video): 4 bits a pixel and
// grey, or 8 bits a pixel with a palette of 256 sixteen-bit entries sitting
// just in front of the pixels.
static int g_bpp;
static u8 *g_pix;                   // the first pixel
static volatile u16 *g_pal;         // the palette, or 0
static int g_pal565;                // palette entries are RGB565 rather than RGB444
static u16 g_pal_saved[256];        // what the machine had, put back on the way out
static u8 g_black, g_white;         // the bytes the window server wrote for black and white

// ── the console: text on the panel while there is no game yet ───────

extern const unsigned char g_font5x7[64][7];
static int g_con_x, g_con_y, g_con_on = 1;
static int g_rows, g_cols;

// text colours: black on white, whatever the pixel format
static void con_px(int x, int y, int fg)
{
    if (g_bpp == 4) {
        volatile u8 *p = g_pix + y * g_stride + (x >> 1);
        u8 b = *p, v = fg ? 0 : 15;
        if (x & 1) *p = (u8)((b & 0x0f) | (v << 4)); else *p = (u8)((b & 0xf0) | v);
    } else {
        g_pix[y * g_stride + x] = fg ? g_black : g_white;
    }
}

static void con_glyph(int x, int y, int ch)
{
    int r, c;
    const unsigned char *g;
    if (ch >= 'a' && ch <= 'z') ch -= 32;
    if (ch < 32 || ch > 95) ch = '?';
    g = g_font5x7[ch - 32];
    for (r = 0; r < 8; r++)
        for (c = 0; c < 6; c++)
            con_px(x + c, y + r, r < 7 && c < 5 && (g[r] & (0x10 >> c)));
}

static void screen_fill(int white)      // the whole panel, white or black
{
    u8 v = g_bpp == 4 ? (white ? 0xff : 0x00) : (white ? g_white : g_black);
    memset(g_pix, v, (size_t)(g_h * g_stride));
}

static void con_scroll(void)
{
    // up eight rows, and white along the bottom
    u8 v = g_bpp == 4 ? 0xff : g_white;
    memmove(g_pix, g_pix + 8 * g_stride, (size_t)((g_h - 8) * g_stride));
    memset(g_pix + (g_h - 8) * g_stride, v, (size_t)(8 * g_stride));
}

void epoc_con_write(const void *s, int n)
{
    const char *p = s;
    if (!g_fb_ok || !g_con_on) return;
    while (n-- > 0) {
        char c = *p++;
        if (c == '\n' || g_con_x >= g_cols) {
            g_con_x = 0;
            if (g_con_y >= g_rows - 1) con_scroll(); else g_con_y++;
            if (c == '\n') continue;
        }
        if (c == '\r') continue;
        con_glyph(g_con_x * 6, g_con_y * 8, c);
        g_con_x++;
    }
}

// ── the window server ───────────────────────────────────────────────

static int ws_init(int pw, int ph)
{
    i32 e;
    void **vt;
    U_cleanup();                        // the cleanup stack the window server client leaves on
    F_connect();
    W_sessCtor(g_sess);
    if (W_connect(g_sess) != 0) return -1;
    W_grpCtor(g_grp, g_sess);
    W_grpConstruct(g_grp, 1);
    W_scrCtor(g_scr, g_sess);
    if (W_scrConstruct(g_scr) != 0) return -2;
    vt = (void **)g_scr[0];
    e = ((i32 (*)(void *, void **))vt[6])(g_scr, &g_gc);        // CreateContext
    if (e != 0 || !g_gc) return -3;
    W_bwCtor(g_win, g_sess);
    if (W_bwConstruct(g_win, g_grp, 3, 2) != 0) return -4;
    {
        i32 pt[2] = {0, 0}, sz[2];
        sz[0] = pw; sz[1] = ph;
        W_setExt204(g_win, pt, sz);
    }
    W_activate(g_win);
    W_gcActivate(g_gc, g_win);
    vt = (void **)((u32 *)g_gc)[0];
    ((void (*)(void *))vt[8])(g_gc);                            // Reset
    // paint the window white once, so nothing of the desktop shows
    ((void (*)(void *, u32))vt[16])(g_gc, 0);                   // pen: none
    ((void (*)(void *, u32))vt[19])(g_gc, 1);                   // brush: solid
    {
        u32 rgb = 0xffffff;
        i32 r[4];
        r[0] = 0; r[1] = 0; r[2] = pw; r[3] = ph;
        ((void (*)(void *, void *))vt[18])(g_gc, &rgb);
        ((void (*)(void *, void *))vt[34])(g_gc, r);
    }
    W_flush(g_sess);
    return 0;
}

static void __attribute__((noinline)) ws_rect(int x, int y, int w, int h, u32 rgb)
{
    void **vt = (void **)((u32 *)g_gc)[0];
    i32 r[4];
    r[0] = x; r[1] = y; r[2] = x + w; r[3] = y + h;
    ((void (*)(void *, void *))vt[18])(g_gc, &rgb);
    ((void (*)(void *, void *))vt[34])(g_gc, r);
}

// Find out what the panel's memory is. Paint the left half black and the right
// half white through the window server, then look for the long runs of equal
// bytes that makes: how long a run is says how many bits a pixel are (a half
// row of 320 pixels is 160 bytes at 4 bits, 320 at 8), and where the first
// begins says where the pixels start (a palette may come first).
static int detect_video(void)
{
    volatile u8 *p = (volatile u8 *)g_fb;
    int i, k, run, tries, base = -1;
    ws_rect(0, 0, g_w / 2, g_h, 0x000000);
    ws_rect(g_w / 2, 0, g_w - g_w / 2, g_h, 0xffffff);
    W_flush(g_sess);
    for (tries = 0; tries < 15; tries++) DG_SleepMs(100);
    for (i = 0; i + 100 < 1024 * 1024; i++) {
        for (k = 1; k < 100 && p[i + k] == p[i]; k++) {}
        if (k == 100) { base = i; break; }
    }
    if (base < 0) return -1;
    run = 0;
    while (p[base + run] == p[base]) run++;
    g_black = p[base];
    g_white = p[base + run];
    g_bpp = run * 8 / (g_w / 2);
    if (g_bpp != 4 && g_bpp != 8) return -2;
    g_stride = g_w * g_bpp / 8;
    g_pix = (u8 *)g_fb + base;
    g_pal = 0;
    if (g_bpp == 8 && base >= 512) {
        // 256 sixteen-bit entries in front of the pixels (SA-11x0 style)
        g_pal = (volatile u16 *)((u8 *)g_fb + base - 512);
        for (i = 0; i < 256; i++) g_pal_saved[i] = g_pal[i];
        g_pal565 = g_pal[g_white] == 0xffff;
    }
    return 0;
}

// Where is the panel's memory and how big is the panel? (Before the window
// server session: its window has to be as big as the panel.)
static int screen_query(void)
{
    // a TBuf8<24>: length|type, max length, then six words of TScreenInfoV01
    volatile u32 b[2 + 6];
    int i;
    for (i = 0; i < 8; i++) b[i] = 0;
    b[0] = TYPE_LEN(EBuf, 0);
    b[1] = 24;
    U_screenInfo((void *)b);
    if (!b[4] || !b[5]) return -1;                // iScreenAddressValid, iScreenAddress
    g_fb = (volatile u32 *)b[5];
    g_w = (int)b[6]; g_h = (int)b[7];
    return 0;
}

// ... and what format it is in (needs the window server session).
static int screen_init(void)
{
    g_fb_ok = 1;                                  // (the wait in detect_video needs the timer only)
    if (detect_video() != 0) { g_fb_ok = 0; return -2; }
    g_cols = (int)udivmod((unsigned)g_w, 6, 0); g_rows = g_h >> 3;
    return 0;
}

// A record of what the program was last doing, for finding it in a RAM snapshot
// when it stops (search for the magic).
volatile u32 g_dbg[16] = { 0x31474244 /* "DBG1" */, 0, 0, 0, 0, 0, 0, 0 };

volatile u32 g_kdbg[10] = { 0x3159454b /* "KEY1" */, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

// ── time ────────────────────────────────────────────────────────────

uint32_t DG_GetTicksMs(void)
{
    // 64 ticks a second: 15.625 ms each. The count does not start at zero (on a
    // Series 7 it starts somewhere past two billion), so only the differences
    // between readings are used. A reading that goes backwards (the Series 7's
    // does now and then) is not allowed to make time run backwards: Doom's
    // clock treats a step back as "skip the next million tics" and stalls on
    // the title screen. Steps forward are capped at half a minute.
    static u32 last, acc;
    static int have;
    u32 t = Exec_TickCount();
    g_dbg[6] = t; g_dbg[7]++;
    if (!have) { last = t; have = 1; }
    else {
        i32 d = (i32)(t - last);
        if (d > 0) acc += d > 64 * 30 ? 64 * 30 : (u32)d; else if (d < 0) g_dbg[8]++;
        last = t;
    }
    return (acc * 125u) >> 3;
}

// ── keys ────────────────────────────────────────────────────────────

enum { WE_KEY = 1, WE_KEYUP = 2, WE_KEYDOWN = 3, WE_POINTER = 5 };

#define KQ 64
static struct { int pressed; u8 key; } g_kq[KQ];
static int g_kq_head, g_kq_tail;

// EPOC's TStdScanCode to Doom's key codes.
static int map_scan(int sc)
{
    if (sc >= 'A' && sc <= 'Z') return sc + 32;        // letters
    if (sc >= '0' && sc <= '9') return sc;
    switch (sc) {
    case 1:  return KEY_BACKSPACE;
    case 2:  return KEY_TAB;
    case 3:  return KEY_ENTER;
    case 4:  return KEY_ESCAPE;
    case 5:  return KEY_USE;                            // use / open
    case 14: return KEY_LEFTARROW;
    case 15: return KEY_RIGHTARROW;
    case 16: return KEY_UPARROW;
    case 17: return KEY_DOWNARROW;
    case 18: case 19: return KEY_RSHIFT;                // run
    case 20: case 21: case 24: case 25: return KEY_RALT;   // strafe: Alt and Fn
    case 22: case 23: return KEY_FIRE;                  // fire
    }
    return 0;
}

static void kq_put(int pressed, int key)
{
    int n = (g_kq_head + 1) % KQ;
    if (n == g_kq_tail || !key) return;
    g_kq[g_kq_head].pressed = pressed; g_kq[g_kq_head].key = (u8)key;
    g_kq_head = n;
}

static void ws_poll(void)
{
    for (;;) {
        if (!g_ev_posted) {
            g_evst = PEND;
            W_evReady(g_sess, &g_evst);
            W_flush(g_sess);
            g_ev_posted = 1;
        }
        if (g_evst == PEND) return;
        g_ev_posted = 0;
        W_getEvent(g_sess, ev);
        g_kdbg[4] = ev[0]; g_kdbg[5] = ev[4]; g_kdbg[6] = ev[5]; g_kdbg[7] = ev[6]; g_kdbg[8] = ev[7];
        switch ((i32)ev[0]) {
        case WE_KEYDOWN: g_kdbg[1]++; g_kdbg[9] = (g_kdbg[9] << 8) | (ev[5] & 255); kq_put(1, map_scan((i32)ev[5])); break;
        case WE_KEYUP:   g_kdbg[2]++; kq_put(0, map_scan((i32)ev[5])); break;
        default:         g_kdbg[3]++; break;
        }
    }
}

int DG_GetKey(int *pressed, unsigned char *key)
{
    ws_poll();
    if (g_kq_tail == g_kq_head) return 0;
    *pressed = g_kq[g_kq_tail].pressed;
    *key = g_kq[g_kq_tail].key;
    g_kq_tail = (g_kq_tail + 1) % KQ;
    return 1;
}

void DG_SleepMs(uint32_t ms)
{
    g_dbg[5]++; { extern int gametic, gamestate, I_GetTime(void); g_dbg[11] = (u32)gametic; g_dbg[12] = (u32)I_GetTime(); g_dbg[14] = (u32)gamestate; }
    volatile i32 st = PEND;
    if (ms < 1) ms = 1;
    Exec_After((i32)(ms * 1000u), &st);
    while (st == PEND) Exec_WaitForAnyRequest();
}

void DG_SetWindowTitle(const char *t) { (void)t; }

// ── pixels ──────────────────────────────────────────────────────────

// Doom's 256-colour frame to the panel: each Doom pixel is two panel pixels
// across, so one source byte becomes one destination byte, looked up in a
// table of "this colour as a pair of equal greys".
static u8 g_lut[256];
static u8 g_rgb[768];
static int g_force = 1;                 // convert everything the next time a frame is shown

// Put the game's colours where the panel can use them: a grey for each into a
// lookup table (4 bits a pixel), or into the palette in front of the pixels
// (8 bits a pixel: it is the machine's own palette, rewritten for as long as
// the game runs, and put back on the way out).
static void apply_palette(void)
{
    int i;
    for (i = 0; i < 256; i++) {
        u32 r = g_rgb[i * 3], g = g_rgb[i * 3 + 1], b = g_rgb[i * 3 + 2];
        if (g_bpp == 4) {
            u32 v = ((r * 77u + g * 151u + b * 28u) >> 8) >> 4;
            g_lut[i] = (u8)(v | (v << 4));
        } else if (g_pal) {
            u16 e = g_pal565 ? (u16)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
                             : (u16)(((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4));
            if (i == 0) e |= (u16)(g_pal_saved[0] & 0xf000);     // the format bits of entry 0
            g_pal[i] = e;
        }
    }
    if (g_bpp == 8) { g_black = 0; g_white = 4; }      // the game's black and white
    g_force = 1;                                        // everything shown is in the old colours
}

void epoc_set_palette(const unsigned char *rgb)    // 768 bytes, gamma already applied
{
    memcpy(g_rgb, rgb, 768);
    if (g_bpp == 4 || !g_con_on) apply_palette();      // (8 bits: not under the start-up text)
}

static u32 g_speed;                     // see cpu_speed

// A frame-rate readout in the margin above the picture: frames shown in the
// last stretch of ticks (64 to the second), to a tenth. Off with -DNO_STATS.
static void stats_draw(void)
{
    static u32 t0, frames;
    static char text[24];
    u32 t = Exec_TickCount();
    if (!t0) t0 = t;
    frames++;
    if (t - t0 >= 128) {                            // two seconds
        u32 tenths = udivmod(frames * 640u, t - t0, 0);          // frames/s x 10 (64 ticks a second)
        u32 q, r;
        int n = 0, i;
        q = udivmod(tenths, 10, &r);
        if (q >= 10) text[n++] = (char)('0' + udivmod(q, 10, &q));
        text[n++] = (char)('0' + q); text[n++] = '.'; text[n++] = (char)('0' + r);
        text[n++] = ' '; text[n++] = 'F'; text[n++] = 'P'; text[n++] = 'S';
        {   // and the CPU speed figure (see cpu_speed)
            u32 v = g_speed, r2; int d[6], k = 0;
            text[n++] = ' ';
            do { v = udivmod(v, 10, &r2); d[k++] = (int)r2; } while (v && k < 6);
            while (k) text[n++] = (char)('0' + d[--k]);
        }
        text[n] = 0;
        for (i = 0; i < 16; i++) con_glyph(4 + i * 6, 6, i < n && text[i] ? text[i] : ' ');
        t0 = t; frames = 0;
    }
}

// Doom's view window, as the renderer set it (r_main.c / r_draw.c)
extern int viewwindowx, viewwindowy, scaledviewwidth, viewheight;

static u32 g_prev[200 * 320 / 4];       // what was last converted, outside the view
static int g_vgeom[4];

// Four Doom pixels (a word of palette indices) to the panel's. A Doom pixel is
// two panel pixels across. When pixels come in equal pairs (low detail, in
// the 3D view) it is two lookups, not four.
#define PAIRED(w) ((((w) ^ ((w) >> 8)) & 0x00ff00ffu) == 0)

static inline u32 cv4(u32 w)
{
    if (PAIRED(w)) {
        u32 x = (u32)g_lut[w & 255] | ((u32)g_lut[(w >> 16) & 255] << 16);
        return x | (x << 8);
    }
    return (u32)g_lut[w & 255] | ((u32)g_lut[(w >> 8) & 255] << 8)
         | ((u32)g_lut[(w >> 16) & 255] << 16) | ((u32)g_lut[w >> 24] << 24);
}

static inline void cv8(u32 w, u32 *o)           // 8 bits a pixel: eight bytes out
{
    u32 a = w & 255, b = (w >> 8) & 255, c = (w >> 16) & 255, d = w >> 24;
    if (a == b && c == d) {
        a |= a << 8; c |= c << 8;
        o[0] = a | (a << 16); o[1] = c | (c << 16);
    } else {
        o[0] = a | (a << 8) | (b << 16) | (b << 24);
        o[1] = c | (c << 8) | (d << 16) | (d << 24);
    }
}

// A run of words [w0, w1) of one row: converted, and kept in g_prev. With
// `diff`, only those that differ from what was converted last time.
static inline void row_words(const u32 *s, u32 *prev, u32 *dst, int w0, int w1, int diff)
{
    int x;
    if (g_bpp == 4) {
        if (diff) {
            for (x = w0; x < w1; x++) { u32 w = s[x]; if (w != prev[x]) { prev[x] = w; dst[x] = cv4(w); } }
        } else {
            for (x = w0; x < w1; x++) dst[x] = cv4(s[x]);
        }
    } else {
        if (diff) {
            for (x = w0; x < w1; x++) { u32 w = s[x]; if (w != prev[x]) { prev[x] = w; cv8(w, dst + 2 * x); } }
        } else {
            for (x = w0; x < w1; x++) cv8(s[x], dst + 2 * x);
        }
    }
}

// A panel too short for 200 rows (the Revo's is 480 x 160, 4 bits a pixel):
// scaled to fit, 1.5 times across and four rows in five down, by taking the
// nearest source pixel. Four source pixels a b c d become six: a a b c c d,
// which is three bytes.
static void present_small(const unsigned char *src)
{
    int y, oh = g_h < 200 ? g_h : 200;
    u8 *dst = g_pix;
    for (y = 0; y < oh; y++) {
        const u32 *s = (const u32 *)(src + (u32)(y * 5 >> 2) * 320);
        u8 *d = dst + y * g_stride + ((g_w - 480) >> 2);
        int x;
        for (x = 0; x < 80; x++) {
            u32 w = s[x];
            u32 a = g_lut[w & 255] & 15, b = g_lut[(w >> 8) & 255] & 15;
            u32 c = g_lut[(w >> 16) & 255] & 15, e = g_lut[w >> 24] & 15;
            d[0] = (u8)(a | (a << 4));
            d[1] = (u8)(b | (c << 4));
            d[2] = (u8)(c | (e << 4));
            d += 3;
        }
    }
}

// A panel narrower than 640 but tall enough (the Osaris: 320 x 200): one panel
// pixel to a Doom pixel, so two to a byte.
static void present_narrow(const unsigned char *src)
{
    int y;
    for (y = 0; y < 200 && y < g_h; y++) {
        const u32 *s = (const u32 *)(src + y * 320);
        u8 *d = g_pix + y * g_stride + ((g_w - 320) >> 2);
        int x;
        for (x = 0; x < 80; x++) {
            u32 w = s[x];
            d[0] = (u8)((g_lut[w & 255] & 15) | ((g_lut[(w >> 8) & 255] & 15) << 4));
            d[1] = (u8)((g_lut[(w >> 16) & 255] & 15) | ((g_lut[w >> 24] & 15) << 4));
            d += 2;
        }
    }
}

void epoc_present(const unsigned char *src)        // 320 x 200 palette indices
{
    int y, vy0, vy1, vx0, vx1;
    int vs = (g_bpp == 8 && g_h >= 400) ? 2 : 1;       // twice down as well on a tall panel
    u8 *row;
    g_dbg[10]++;
    if (!g_fb_ok) return;
    if (g_con_on) {                                 // the first frame: clear away the start-up text
        g_con_on = 0;
        apply_palette();
        memset(g_pix, 0, (size_t)(g_h * g_stride));    // (index 0 is black in the game's palette too)
        g_force = 1;
    }
    if (g_bpp == 4 && g_w < 640) {
        if (g_h < 200) present_small(src); else present_narrow(src);
#ifndef NO_STATS
        stats_draw();
#endif
        return;
    }
    vy0 = viewwindowy; vy1 = viewwindowy + viewheight;
    vx0 = viewwindowx & ~3; vx1 = (viewwindowx + scaledviewwidth + 3) & ~3;
    if (g_vgeom[0] != vx0 || g_vgeom[1] != vx1 || g_vgeom[2] != vy0 || g_vgeom[3] != vy1) {
        g_vgeom[0] = vx0; g_vgeom[1] = vx1; g_vgeom[2] = vy0; g_vgeom[3] = vy1;
        g_force = 1;                                // the view moved: what was kept is no longer what is shown
    }
    row = g_pix + ((g_h - 200 * vs) / 2) * g_stride + (g_bpp == 4 ? (g_w - 640) / 4 : (g_w - 640) / 2);
    for (y = 0; y < 200; y++) {
        const u32 *s = (const u32 *)(src + y * 320);
        u32 *prev = g_prev + y * 80;
        u32 *dst = (u32 *)row;       // (4 bits: 80 words of output; 8 bits: 160)
        if (y >= vy0 && y < vy1) {
            row_words(s, prev, dst, vx0 >> 2, vx1 >> 2, 0);
            if (vx0 > 0 || vx1 < 320) {
                row_words(s, prev, dst, 0, vx0 >> 2, !g_force);
                row_words(s, prev, dst, vx1 >> 2, 80, !g_force);
                if (g_force) {                       // (keep the margins' copy in step)
                    int x;
                    for (x = 0; x < 80; x++) prev[x] = s[x];
                }
            }
        } else {
            row_words(s, prev, dst, 0, 80, !g_force);
            if (g_force) { int x; for (x = 0; x < 80; x++) prev[x] = s[x]; }
        }
        if (vs == 2) memcpy(row + g_stride, row, 640);
        row += g_stride * vs;
    }
    g_force = 0;
#ifndef NO_STATS
    stats_draw();
#endif
}

void DG_Init(void) {}
void DG_DrawFrame(void) {}

// ── files ───────────────────────────────────────────────────────────

static u32 g_rfs[4];
static int g_fs_ok;

int epoc_file_open(EpocFile *f, const char *path, long *size)
{
    TPtrC8_ name;
    u32 i;
    long lo, hi;
    u8 probe[4];
    TPtr8_ des;
    if (!g_fs_ok) {
        if (F_fsConnect(g_rfs, 4) != 0) return -1;
        g_fs_ok = 1;
    }
    for (i = 0; path[i]; i++) {}
    name.iTypeLength = TYPE_LEN(EPtrC, i);
    name.iPtr = path;
    for (i = 0; i < 4; i++) f->h[i] = 0;
    if (F_open(f, g_rfs, &name, 0) != 0) return -1;
    // The file's length: the largest position a one-byte read still finds.
    lo = 0; hi = 1L << 27;
    while (hi - lo > 1) {
        long mid = (lo + hi) >> 1;
        des.iTypeLength = TYPE_LEN(EPtr, 0); des.iMaxLength = 1; des.iPtr = probe;
        if (F_readPos(f, (i32)mid, &des) == 0 && DES_LEN(des.iTypeLength) == 1) lo = mid; else hi = mid;
    }
    *size = lo + 1;
    return 0;
}

long epoc_file_read(EpocFile *f, long pos, void *buf, long n)
{
    long got = 0;
    g_dbg[1] = (u32)pos; g_dbg[2] = (u32)n; g_dbg[3]++;           // a read begins
    // the server copies a bounded amount a call
    while (got < n) {
        long chunk = n - got > 32768 ? 32768 : n - got;
        TPtr8_ des;
        des.iTypeLength = TYPE_LEN(EPtr, 0); des.iMaxLength = (i32)chunk; des.iPtr = (u8 *)buf + got;
        if (F_readPos(f, (i32)(pos + got), &des) != 0) return got ? got : -1;
        if (!DES_LEN(des.iTypeLength)) break;
        got += (long)DES_LEN(des.iTypeLength);
    }
    g_dbg[4]++;                                                      // ... and ended
    return got;
}

void epoc_file_close(EpocFile *f) { F_close(f); }


// ── a probe, built with -DPROBE: what does this machine's screen memory look
// like? Paint the left half black and the right half white through the window
// server, then report what is in the panel's memory as rows of bars drawn the
// same way (so the report does not depend on the pixel format being worked out).

#ifdef PROBE

static void __attribute__((noinline)) probe_row(int row, u32 v)
{
    int b;
    for (b = 0; b < 32; b++) {
        if ((v >> (31 - b)) & 1) ws_rect(8 + b * 19, 2 + row * 22, 15, 16, 0xffffff);
        else ws_rect(8 + b * 19 + 7, 2 + row * 22 + 7, 2, 2, 0xffffff);
    }
}

u32 g_probe_w[4];
static void probe(void)
{
    volatile u8 *p;
    int i, run, base = -1, runlen = 0, tries;
    u32 first = 0, second = 0;
    ws_rect(0, 0, 320, g_h, 0x000000);
    ws_rect(320, 0, 320, g_h, 0xffffff);
    W_flush(g_sess);
    for (tries = 0; tries < 20; tries++) DG_SleepMs(100);
    p = (volatile u8 *)g_fb;
    // first run of 100 equal bytes, and how long it is
    for (i = 0; i + 100 < 200000; i++) {
        int k;
        for (k = 1; k < 100 && p[i + k] == p[i]; k++) {}
        if (k == 100) { base = i; first = p[i]; break; }
    }
    if (base >= 0) {
        run = 0;
        while (p[base + run] == p[base]) run++;
        runlen = run;
        second = p[base + runlen];
    }
    {   // read what is in the first words now, before the report is painted over them
        extern u32 g_probe_w[4];
        g_probe_w[0] = ((volatile u32 *)g_fb)[0];
        g_probe_w[1] = ((volatile u32 *)g_fb)[1];
        g_probe_w[2] = ((volatile u32 *)g_fb)[127];     // palette entries 254, 255 if there is one
        g_probe_w[3] = ((volatile u32 *)g_fb)[16];
    }
    ws_rect(0, 0, 640, g_h, 0x808080);
    probe_row(0, (u32)g_fb);                     // the address
    probe_row(1, (u32)g_w);
    probe_row(2, (u32)g_h);
    probe_row(3, (u32)base);                     // where the first run starts
    probe_row(4, (u32)runlen);                   // how long it is (160 = 4 bits a pixel, 320 = 8)
    probe_row(5, first);
    probe_row(6, second);
    probe_row(7, g_probe_w[0]);                  // the first words: a palette?
    probe_row(8, g_probe_w[1]);
    probe_row(9, g_probe_w[2]);
    probe_row(10, g_probe_w[3]);
    W_flush(g_sess);
    for (;;) DG_SleepMs(1000);
}
#endif


// How fast is this CPU? Count turns of a small loop that touches memory in the
// space of two kernel ticks (31 ms), in thousands. The Series 5 (an ARM710a at
// 18 MHz) comes out lowest, a 5mx (36 MHz) about double, and the StrongARM
// machines several times that. It only has to tell those classes apart.
// Measured in the emulators: Series 5 about 203, 5mx about 413, netpad about 2490.
#define SPEED_MID  300
#define SPEED_FAST 800

static u32 cpu_speed(void)
{
    volatile u32 cell[4];
    u32 t, n = 0;
    cell[0] = 1;
    t = Exec_TickCount();
    while (Exec_TickCount() == t) {}                // start on a tick edge
    t = Exec_TickCount();
    while (Exec_TickCount() - t < 2) {
        int i;
        for (i = 0; i < 100; i++) cell[i & 3] = cell[(i + 1) & 3] + 1;
        n++;
    }
    g_speed = n;
    return n;
}

// ── start and stop ──────────────────────────────────────────────────

unsigned char epoc_zone[EPOC_ZONE_BYTES] __attribute__((aligned(8)));
const int epoc_zone_size = EPOC_ZONE_BYTES;

void epoc_exit(int code)
{
    char m[48];
    int i;
    g_con_on = 1;
    if (g_pal) for (i = 0; i < 256; i++) g_pal[i] = g_pal_saved[i];      // the machine's own colours
    if (g_bpp == 8) { g_black = 0; g_white = 0xff; }
    snprintf(m, sizeof m, "\nexit %d\n", code);
    epoc_con_write(m, (int)strlen(m));
    // stay on the panel so the message can be read
    for (;;) DG_SleepMs(1000);
}

static char *g_argv[4];
static char g_iwad[64];

static int try_iwad(const char *p)
{
    EpocFile f; long sz;
    if (epoc_file_open(&f, p, &sz) != 0) return 0;
    epoc_file_close(&f);
    return 1;
}

int game_init_and_run(void)
{
    int argc = 0;
    if (screen_query() != 0) return 3;
    if (ws_init(g_w, g_h) != 0) return 1;
    if (screen_init() != 0) return 2;
#ifdef PROBE
    probe();
#endif
    // a first, empty screen to write the console on
    screen_fill(1);
    printf("Doom for EPOC (%d bits a pixel, %d x %d)\n", g_bpp, g_w, g_h);
    printf("cpu speed %d\n", (int)cpu_speed());
    g_argv[argc++] = "doom";
    {   // The WAD is on a card, which may not be mounted yet (or be put in later):
        // look on the card drive first, then the others, and keep looking.
        static const char *const where[] = {
            "D:\\DOOM1.WAD", "E:\\DOOM1.WAD", "C:\\DOOM1.WAD",
            "D:\\Doom\\DOOM1.WAD", "C:\\Doom\\DOOM1.WAD",
            "D:\\Documents\\DOOM1.WAD", "C:\\Documents\\DOOM1.WAD",
            "D:\\System\\Apps\\Doom\\DOOM1.WAD", "C:\\System\\Apps\\Doom\\DOOM1.WAD",
            "D:\\Games\\DOOM1.WAD", "C:\\Games\\DOOM1.WAD"
        };
        int n, k;
        for (n = 0; n < 600 && !g_iwad[0]; n++) {
            for (k = 0; k < (int)(sizeof where / sizeof where[0]) && !g_iwad[0]; k++)
                if (try_iwad(where[k])) strcpy(g_iwad, where[k]);
            if (g_iwad[0]) break;
            if (n == 0) printf("Waiting for DOOM1.WAD (on a card, or in C:\\Doom) ...\n");
            DG_SleepMs(500);
        }
        if (g_iwad[0]) { g_argv[argc++] = "-iwad"; g_argv[argc++] = g_iwad; }
    }
    doomgeneric_Create(argc, g_argv);
    {   // low detail (half as many columns to draw), and a view window to suit the
        // machine: the slower the CPU, the smaller. 10 is the full width of the screen
        // above the status bar, 7 is 224 x 120.
        extern int detailLevel, screenblocks;
        extern void R_SetViewSize(int blocks, int detail);
        u32 speed = cpu_speed();
        detailLevel = 1;
        screenblocks = speed < SPEED_MID ? 7 : speed < SPEED_FAST ? 9 : 10;
        R_SetViewSize(screenblocks, detailLevel);
    }
    for (;;) doomgeneric_Tick();
}
