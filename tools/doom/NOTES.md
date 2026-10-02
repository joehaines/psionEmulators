# Doom for EPOC: working notes

## Getting pixels onto the screen (Series 5, EPOC R1)

`UserSvr::ScreenInfo` is `EUser` ordinal 913 (exec call `0x80007c`). It takes a
`TDes8` holding a `TScreenInfoV01` (a 3<<28 | length word, a max-length word,
then six data words) and fills in:

    iWindowHandleValid  0
    iWindowHandle       0
    iScreenAddressValid 1
    iScreenAddress      0x7FE00000   (640 x 240)
    iScreenSize         640, 240

`0x7FE00000` is a user-mode alias of the panel's own memory: the emulator's
write watch shows window-server writes there landing at physical
`0xC0000000`, which is the CL-PS7110's fixed frame buffer. A user program
can write it directly (`tools/doom/spike`): 4 bits a pixel, two to a byte, the
left pixel in the low nibble, 0 black and 15 white, 320 bytes a row.

This avoids the window server's one-filled-rectangle-per-run path that
Lemmings is limited by.

Still to check: the same call on the 5mx / Revo (ER5), the netpad and the
Series 7 / netBook (colour, 8 bits a pixel?).

## Tools
* `tools/doom/runexe.sh EXE OUT.pgm SECS` runs an EXE on an emulated Series 5.
* Harness traces added while finding the above (core/arm710.cpp, env gated):
  `PSION_SWI_TRACE=1` (every exec call), `PSION_SWI_FOLLOW=<hex swi>` (the next
  400 instructions after that call from user code). The emulator already had
  `PSION_WRITE_WATCH=lo-hi` and `PSION_PHYS_WATCH=lo-hi`.
