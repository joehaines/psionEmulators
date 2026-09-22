# Psion HC120: the ROM's layout, and how its keyboard was mapped

The HC120 is the industrial handheld of the SIBO1 generation — the same
V30H + ASIC1 + ASIC2 as the Series 3, in a rubber-sealed case with a
160x80 panel and a 54-key keypad. It is emulated by `Series3::Emulator`
with an HC config (`makeHC120` in `core/device_registry.cpp`); no new chip
modelling was needed. Two things about it did need working out, and this
note records both, because neither is documented anywhere and neither is
guessable from the files alone.

## The ROM is two chips, and the top of memory shows one of them twice

`roms/hc120/` holds the machine's two 128 KiB flash chips, dumped one per
file. They are linear halves, not the even/odd interleave the MC's pair
uses, so `scripts/build-hc120-rom.mts` joins them by concatenation into
the 256 KiB `roms/hc120_v1.72F.bin`.

Where those 256 KiB sit is the interesting part:

```
0x00000-0x7FFFF  RAM (512 KiB on the HC120; 256 on the HC110, 128 on the HC100)
0x80000-0x9FFFF  unmapped
0xA0000-0xBFFFF  ROM chip 1 — opens with the SIBO boot block
0xC0000-0xDFFFF  ROM chip 2
0xE0000-0xFFFFF  ROM chip 2 again — the reset vector lives here
```

The evidence for it:

* Chip 1 starts `E9 D3 D3 E9 35 D6 E9 37 D6 00 F9 00 …` — the three near
  jumps and vector table that open every SIBO1 boot block (the MC400's
  ROM opens the same way at its offset 0, the Series 3's at 0x40000).
* Chip 2 ends in `EA 00 00 00 A0` — `JMP FAR A000:0000`. A reset vector
  can only be at 0xFFFF0, so chip 2 answers at the top of memory, and the
  first instruction the machine runs is at 0xA0000: the boot block at the
  bottom of chip 1.
* That leaves 0xC0000-0xDFFFF, which is one chip or the other. Both
  arrangements were built and run: with chip 2 there the machine boots,
  with chip 1 there it does not get as far as lighting the panel.

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

## The keyboard, read off the machine

There is no MAME driver for the HC, so the key matrix was mapped
empirically. The shell echoes what you type, which makes the machine its
own oracle: with `PSION3_TRACE_KB` confirming the ROM scans all ten
columns, each of the 80 matrix slots was pressed in turn and the character
that came back read off the panel. The answer is a tidy grid — one matrix
column per physical keypad row, six keys per row on bits 0x20 (left) down
to 0x01 (right):

| col | 0x20 | 0x10 | 0x08 | 0x04 | 0x02 | 0x01 | 0x40 |
|-----|------|------|------|------|------|------|------|
| 0 | ESC | MENU | PgUp | PgDn | ← | INFO/→ | `+` |
| 1 | A | B | C | D | E | F | `.` |
| 2 | G | H | I | J | K | L | `0` |
| 3 | M | N | O | P | Q | R | SPACE |
| 4 | S | T | U | V | W | X | SHIFT |
| 5 | ↔ | 7 | 8 | 9 | `/` | Y | — |
| 6 | DEL | 4 | 5 | 6 | `*` | Z | — |
| 7 | LOCK | 1 | 2 | 3 | `-` | ENTER | — |

The 0x40 bits are the keypad's bottom row (SHIFT SPACE 0 . +), running
right to left as the column index rises. Between the eight rows of six and
those five, every one of the machine's 54 keys is accounted for.

Each entry above was confirmed by what it produced — letters and digits by
the character echoed, SPACE by the gap it left between two letters, SHIFT
by turning `a` into `A`, DEL by removing the character before it, ENTER by
running the line. The top row is the exception: the shell gives little
back on it. Its leftmost key visibly edits the input line — type `ab`,
press it, and the characters before the cursor go — two slots (col0 0x10
and col0 0x80) echo a glyph that is not in the ASCII font, and the rest do
nothing you can see at a `$` prompt. None of that identifies a key, so the
row is mapped by position, in the same left-to-right bit order every other
row uses: ESC, MENU, PgUp, PgDn, ←, →, which at least puts ESC on the key
that edits the line.

One consequence for the driver: on the Series 3 and the MC, Esc *is* the
ON key, wired to ASIC2's OnClr so it can wake the machine from standby,
and `setKeyboardKey` routes it there instead of to the matrix. The HC has
both — ESC on the keypad, and a separate ON/OFF button on the case — so
its config clears `escIsOnKey` and Esc goes to the matrix like any other
key.
