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
`run7card.sh` runs it the way the app library's Try it delivers it to a Series 7 (written
onto the card already in the slot, started from E:); `tests/integration/test-doom.sh`
checks all five.

## Starting a game

Doom opens on its title screen and demos. Press Esc (or any key) for the menu, choose
New Game with Up/Down and Enter, then the episode and skill. While a demo plays behind the
menu the 3D view is darkened so the lettering can be read.

## Emulator notes

Doom's WAD load is a heavy CF workload, and it has shaken out two emulator issues:

- 5mx family (fixed in `core/windermere.cpp`): the CF direct-invoke accelerator called the
  driver's retry callback while the kernel's DFC runner had IRQs off, which lost the read's
  completion DFC; about one boot in two hung at a fixed point of the load. Runs are made
  repeatable with `PSION_RTC_SEED`.
- Series 7 (not fixed): on a few RTC seeds the card-detect is delivered late and the CF
  driver stalls after two ATA commands, so the card never mounts and Doom waits for the WAD.
  It is not specific to Doom.

`DOOM_HARNESS=path/to/run` makes the runner scripts use another harness build.
