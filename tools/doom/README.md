# Doom for EPOC

A port of Doom (via [doomgeneric](https://github.com/ozkl/doomgeneric), GPL-2) to the ARM EPOC
machines, built as a native E32 EXE with clang and a small hand-written libc. No SDK is needed.

| Machine | Screen | Verified in the emulator |
|---|---|---|
| Series 5 (ARM710a, ARMv3) | 640x240 4-bpp grey | yes, ~7 fps (3D view reduced) |
| Series 5mx / MC218-class | 640x240 4-bpp grey | 5mx yes, ~10-13 fps |
| netpad (SA-1100) | 640x240 8-bpp colour | yes, ~60 fps |
| Series 7 / netBook | 640x480 8-bpp colour | Series 7 yes |
| Osaris, Revo, 5mxPro, Geofox, Conan | | code paths exist, not verified (see NOTES.md) |

The screen format is detected at run time. The machine's frame buffer is reached through
`UserSvr::ScreenInfo`; keys come from the window server.

## Using it

Put `DOOM.EXE` anywhere and `DOOM1.WAD` (shareware, `tools/Doom1.WAD`) on a CF/MMC card in
`\Doom`, `\Documents`, `\Games` or the root. Keys: arrows move, Ctrl fire, Space use,
Shift run, Alt/Fn strafe, Esc menu. It is in the app library as "Doom (shareware, EPOC port)".

## Building

    bash tools/doom/build.sh        # -> tools/doom/DOOM.EXE (checked to be ARMv3-clean)

clang cannot target ARMv3, so `epoc/armv3fix.py` rewrites halfword loads/stores in the assembly.
`runs5.sh`, `run5mx.sh`, `runnp.sh`, `run7.sh` run it in the emulator;
`tests/integration/test-doom.sh` checks all four.
