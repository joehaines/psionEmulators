# Psion HC120: the ROM's layout and the keyboard matrix

The HC120 is the industrial handheld of the SIBO1 generation — the same
V30H + ASIC1 + ASIC2 as the Series 3, in a rubber-sealed case with a
160x80 panel and a 54-key keypad. It is emulated by `Series3::Emulator`
with an HC config (`makeHC120` in `core/device_registry.cpp`); no new chip
modelling was needed.

The machine was first emulated in MAME, by Nigel Barnes: his `psionhc`
driver (`src/mame/psion/psionhc.cpp`, covering the HC100, HC110 and
HC120) is the reference for everything below — the V1.72F chip pair and
where it sits in the address space, the RAM sizes of the three models, and
the keypad matrix in its `psionhc_uk` input ports. This note records how
those facts map onto this emulator, and what was checked against the ROM
here.

## The ROM is two chips, and the top of memory shows one of them twice

`roms/hc120/` holds the machine's two 128 KiB flash chips, dumped one per
file — the same `v172f_1.bin` / `v172f_2.bin` pair MAME's `psionhc120`
ROM set names. They are linear halves, not the even/odd interleave the
MC's pair uses, so `scripts/build-hc120-rom.mts` joins them by
concatenation into the 256 KiB `roms/hc120_v1.72F.bin`.

MAME's driver maps the flash into the top half of the address space with
each chip appearing twice (`ROM_RELOAD`): chip 1 at 0x80000 and 0xA0000,
chip 2 at 0xC0000 and 0xE0000. The part of that the ROM actually uses is:

```
0x00000-0x7FFFF  RAM (512 KiB on the HC120; 256 on the HC110, 128 on the HC100)
0x80000-0x9FFFF  chip 1 in MAME's map; left unmapped here — the ROM never reads it
0xA0000-0xBFFFF  ROM chip 1 — opens with the SIBO boot block
0xC0000-0xDFFFF  ROM chip 2
0xE0000-0xFFFFF  ROM chip 2 again — the reset vector lives here
```

Checked against the image:

* Chip 1 starts `E9 D3 D3 E9 35 D6 E9 37 D6 00 F9 00 …` — the three near
  jumps and vector table that open every SIBO1 boot block (the MC400's
  ROM opens the same way at its offset 0, the Series 3's at 0x40000).
* Chip 2 ends in `EA 00 00 00 A0` — `JMP FAR A000:0000`. A reset vector
  can only be at 0xFFFF0, so chip 2 answers at the top of memory, and the
  first instruction the machine runs is at 0xA0000: the boot block at the
  bottom of chip 1.
* 0xC0000-0xDFFFF is chip 2, as MAME has it. Swapping chip 1 in there
  instead stops the machine before it lights the panel.

So the top 128 KiB window is an alias of the upper chip rather than a
third chip. The emulator models that as an address decode
(`Series3::Config::romAliasSize`) rather than baking it into the image, so
the committed ROM stays exactly what the two chips hold.

## What it boots into

`EPOC/Os V3.95F`, `Rom V1.72F`, `Shell V2.10F`. The ROM holds the OS and a
command shell — `version`, `free`, `copy`, `delete`, `format`, `type`,
`date`, `lproc`, `backlight` and the rest — but no applications: on this
machine those live on an SSD pack. With no pack in it the shell looks for
`A:AUTOEXEC.BTF`, does not find it, and settles on

```
(c) Psion PLC 1991
Insert Pack
 and press enter
```

which is the machine working, not the machine stuck. Put a pack in drive A
with an `AUTOEXEC.BTF` on it and the shell runs it at startup;
`tests/fixtures/hc120-autoexec.ssd` is exactly that, and the
`hc120-shell` row in `tests/devices-ssd.txt` boots with it.

## The keyboard

The key matrix is MAME's: `psionhc_uk` in `psionhc.cpp` lists all eight
columns, one per physical keypad row, with the six keys of that row on
bits 0x20 (left) down to 0x01 (right):

| col | 0x20 | 0x10 | 0x08 | 0x04 | 0x02 | 0x01 | 0x40 | 0x80 |
|-----|------|------|------|------|------|------|------|------|
| 0 | ESC | MENU | PgUp | PgDn | ← Task | → Info | `+` | OFF |
| 1 | A | B | C | D | E | F | `.` | — |
| 2 | G | H | I | J | K | L | `0` | — |
| 3 | M | N | O | P | Q | R | SPACE | — |
| 4 | S | T | U | V | W | X | SHIFT | — |
| 5 | TAB (↔) | 7 | 8 | 9 | `/` | Y | — | — |
| 6 | DEL | 4 | 5 | 6 | `*` | Z | Backlight | — |
| 7 | PSION LOCK | 1 | 2 | 3 | `-` | ENTER | Contrast | — |

The 0x40 bits of columns 0-4 are the keypad's bottom row (SHIFT SPACE 0 .
+), running right to left as the column index rises.

`core/series3.cpp::hc120KeyMatrix` carries that table, less three keys
it has no EPOC key code for — OFF (col 0, 0x80), Backlight and Contrast.
Every key it does map was also checked on the emulated machine, using
the shell as an oracle: with `PSION3_TRACE_KB` confirming the ROM scans
the columns, each slot was pressed at the `$` prompt and the echo read off the panel — letters and
digits by the character that came back, SPACE by the gap it left, SHIFT by
turning `a` into `A`, DEL by removing the character before it, ENTER by
running the line, and ESC by clearing the input line. The rest of the
top row gives nothing readable back at a `$` prompt, so for those keys
MAME's names are the authority.

One consequence for the driver: on the Series 3 and the MC, Esc *is* the
ON key, wired to ASIC2's OnClr so it can wake the machine from standby,
and `setKeyboardKey` routes it there instead of to the matrix. The HC has
both — ESC on the keypad, and a separate ON/OFF button on the case (MAME's
`ON_OFF` port, which drives OnClr) — so its config clears `escIsOnKey`
and Esc goes to the matrix like any other key.
