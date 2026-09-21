These two files are the MC200's **boot ROM**, not a ROM:: disk — the
directory name is how the dump arrived, and it is kept so the files stay
byte-identical to the originals.

`v212f_0.bin` and `v212f_1.bin` are the two 28F010 128 KiB flash chips of
the V2.12F (081090) boot ROM, dumped separately. They sit on the V30's
16-bit bus, so chip 0 holds the even bytes of the image and chip 1 the odd
ones — which is why neither file contains readable code on its own. They
are byte-identical to MAME's `mc200` ROM set (CRC32 `ff346271` /
`4f266410`), where the same pair also serves as one of the MC400's BIOS
options: the two laptops shipped the same boot ROM.

Interleaving them reconstructs the 256 KiB image the CPU sees at
0xC0000-0xFFFFF:

```sh
node --experimental-strip-types scripts/build-mc200-rom.mts
```

writes `roms/MC200_v2.12F.bin` — the file the `mc200` device profile loads —
and checks the reset vector (`EA 00 00 00 C0` at 0xFFFF0, a far jump to
C000:0000) came out right. The joined image is committed, so the script
only needs re-running if these dumps are ever replaced.

The machine's ROM:: filing system — window server, shell, OPL, fonts —
lives on the System Disk pack instead, `roms/MC200_V2.12F_system.ssd`,
which the frontend pre-inserts in Pack D on cold boot.
