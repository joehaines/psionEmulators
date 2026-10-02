#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-PsionWebEmulator
# Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.
#
# Build the game for a PC and run its tests: scripted solutions of all twelve
# levels through the real rules, and the window-server budget of a frame.
#
#   bash tools/lemmings/host/test.sh [OUT] [SCENES]
#
# With OUT (a .pgm or .ppm path) the screen after 200 ticks of level 1 is
# written there; with SCENES too, the title and a busy level are written as
# SCENES-title.ppm and SCENES-l10.ppm, in colour.
set -eu
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp)"
trap 'rm -f "$OUT"' EXIT
# -fno-builtin: src/rt.c defines memset and friends, as the Series 5 build needs.
cc -O1 -w -fno-builtin -o "$OUT" "$HERE/host/test_host.c" "$HERE"/src/{game,gfx,sprites,levels,font,rt,palette,present}.c
"$OUT" "$@"
