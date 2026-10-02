// EPOC port of Doom's video layer.
//
// This replaces doomgeneric's i_video.c, which is written for a Linux
// framebuffer. Here the finished 320 x 200 frame of palette indices goes
// straight to tools/doom/epoc/platform.c, which turns it into whatever the
// panel wants and writes it into the panel's own memory.
//
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include "config.h"
#include "v_video.h"
#include "m_argv.h"
#include "d_event.h"
#include "d_main.h"
#include "i_video.h"
#include "i_system.h"
#include "z_zone.h"
#include "tables.h"
#include "doomkeys.h"
#include "doomgeneric.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

extern void epoc_present(const unsigned char *src);
extern void epoc_set_palette(const unsigned char *rgb);

int fb_scaling = 1;
int usemouse = 0;

boolean palette_changed;
struct color colors[256];

void I_GetEvent(void);

// The screen buffer; this is modified to draw things to the screen
byte *I_VideoBuffer = NULL;

boolean screensaver_mode = false;
boolean screenvisible = true;

// No mouse on this machine; the variables stay because the configuration
// table names them.
float mouse_acceleration = 2.0;
int mouse_threshold = 10;

// Gamma correction level to use
int usegamma = 0;

void I_InitGraphics (void)
{
    I_VideoBuffer = (byte *) Z_Malloc (SCREENWIDTH * SCREENHEIGHT, PU_STATIC, NULL);
    screenvisible = true;
}

void I_ShutdownGraphics (void)
{
    Z_Free (I_VideoBuffer);
}

void I_StartFrame (void)
{
}

void I_StartTic (void)
{
    I_GetEvent();
}

void I_UpdateNoBlit (void)
{
}

void I_FinishUpdate (void)
{
    epoc_present(I_VideoBuffer);
}

void I_ReadScreen (byte* scr)
{
    memcpy (scr, I_VideoBuffer, SCREENWIDTH * SCREENHEIGHT);
}

void I_SetPalette (byte* palette)
{
    byte rgb[768];
    int i;

    for (i = 0; i < 256; ++i)
    {
        rgb[i * 3]     = gammatable[usegamma][*palette++];
        rgb[i * 3 + 1] = gammatable[usegamma][*palette++];
        rgb[i * 3 + 2] = gammatable[usegamma][*palette++];
        colors[i].a = 0;
        colors[i].r = rgb[i * 3];
        colors[i].g = rgb[i * 3 + 1];
        colors[i].b = rgb[i * 3 + 2];
    }
    palette_changed = true;
    epoc_set_palette(rgb);
}

// Given an RGB value, find the closest matching palette index.
int I_GetPaletteIndex (int r, int g, int b)
{
    int best = 0, best_diff = 0x7fffffff, diff, i;

    for (i = 0; i < 256; ++i)
    {
        diff = (r - colors[i].r) * (r - colors[i].r)
             + (g - colors[i].g) * (g - colors[i].g)
             + (b - colors[i].b) * (b - colors[i].b);
        if (diff < best_diff)
        {
            best = i;
            best_diff = diff;
        }
        if (diff == 0)
        {
            break;
        }
    }
    return best;
}

void I_BeginRead (void) { }
void I_EndRead (void) { }
void I_SetWindowTitle (char *title) { DG_SetWindowTitle(title); }
void I_GraphicsCheckCommandLine (void) { }
void I_SetGrabMouseCallback (grabmouse_callback_t func) { }
void I_EnableLoadingDisk(void) { }
void I_BindVideoVariables (void) { }
void I_DisplayFPSDots (boolean dots_on) { }
void I_CheckIsScreensaver (void) { }
void I_InitWindowTitle (void) { }
void I_InitWindowIcon (void) { }
