// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "../src/platform.h"
#include "../src/game.h"

int game_init_and_run(void)
{
    InEvent evs[16];
    int n, ticks;
    if (plat_init() != 0) return 1;
    game_init(plat_seed());
    for (;;) {
        n = plat_wait(evs, 16, &ticks);
        if (game_frame(evs, n, ticks)) break;
    }
    plat_shutdown();
    return 0;
}
