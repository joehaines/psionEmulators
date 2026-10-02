// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Turning "g_fb changed inside these rectangles" into as few filled
// rectangles as will do. On a Series 5 the cost of a frame is not the
// pixels but the number of draw calls the window server has to answer, so
// this is worth doing carefully, and worth being able to test on a PC.

#ifndef LEMMINGS_PRESENT_H
#define LEMMINGS_PRESENT_H

#include "platform.h"

// Called once for each rectangle to paint: [x0,x1) x [y0,y1) in palette index g.
typedef void (*PresentEmit)(int x0, int y0, int x1, int y1, int g);

// Forget what the screen shows, so the next present paints everything.
void present_reset(void);

// Compare g_fb to what the screen is known to show, inside the given
// rectangles, and emit the rectangles that bring the screen up to date.
void present_diff(const Rect *dirty, int n, PresentEmit emit);

#endif
