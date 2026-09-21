# Psion HC120: what `roms/hc120_v172f_2.bin` is, and what is missing

The HC120 is not emulated, and cannot be from what is in this tree: the ROM
dump here is one chip of the machine's ROM, and it is not the chip the CPU
starts executing from. This note records what the file is, how that was
established, and what is needed to finish the job — the hardware side is
mostly a known quantity, because the HC is the same SIBO1 chip set as the
Series 3 and the MC.

## What the file is

`roms/hc120_v172f_2.bin` is 128 KiB of EPOC16 (SIBO) ROM content:

* It ends in the CPU's reset vector. The last paragraph of the image reads
  `EA 00 00 00 A0` — `JMP FAR A000:0000` — followed by the build-date
  string `010170`. The 8086 fetches its first instruction from `FFFF:0000`
  = 0xFFFF0, and a reset vector can only be at the top of the 1 MiB space,
  so **this image maps at 0xE0000-0xFFFFF**.
* The image is linear code, not an interleaved half. Unlike the MC's two
  28F010 chips (which hold the even and odd bytes of a 16-bit bus and are
  joined by `scripts/build-mc200-rom.mts`), this file disassembles and
  reads as continuous 8086 code with legible strings.
* It carries OS and application content — `SYS$FSRV.*`, `SYS$SHLL.IMG`,
  `SYS$NTFY.IMG`, `SYS$NCP.IMG`, `OPL.DYL`, `SCREEN.PIC`, a serial test
  utility ("TTEST … V2.20F (27/11/91)"), and a print utility
  ("PPRINT V1.00F") — so it is genuinely part of an HC ROM and not a
  datapak or an SSD image.

## Why it cannot boot on its own

The reset vector jumps to **A000:0000 = 0xA0000**, which is 256 KiB below
the bottom of this image. The first instruction the machine executes is in
a part of the ROM that is not in this tree.

Two further things confirm the missing part rather than a mapping mistake
on our side:

* **No SIBO boot block.** Every SIBO1 ROM in this tree opens its boot block
  with the same shape: three `E9 xx xx` near jumps followed by a vector
  table (`00 F7 00 1D 01 2A 01 …`). On the MC400 that block is at ROM
  offset 0 (mapped at 0xC0000); on the Series 3 it is at ROM offset
  0x40000 (also mapped at 0xC0000). It appears nowhere in this file.
* **No ROM:: directory.** The Series 3 and MC400 images carry a directory
  of 6-byte entries (address + size) against names like `SYS$SHLL.IMG` and
  `OPL.DYL`. This file has the names only where program code references
  them as strings — the directory itself is in the missing part.

## What is missing

Published HC specifications give the machine 256 KiB of internal Flash ROM
(HC110: 256 KiB RAM; HC120: 512 KiB RAM). Two readings fit the evidence,
and both need the same thing:

1. **256 KiB in two 128 KiB chips**, with the top window mirroring the
   upper half. This file is then the second chip, and the dump is missing
   the first — which is what the `_2` suffix suggests, with
   `hc120_v172f_1.bin` (or `_0`) still to come.
2. **384 KiB at 0xA0000-0xFFFFF in three chips.** This file is the third,
   and both earlier chunks are missing.

Either way the chunk that holds 0xA0000 — the boot block the reset vector
jumps to — is the one to find. With it in hand, joining the parts is the
same one-line job `scripts/build-mc200-rom.mts` does for the MC200 (a
concatenation here, not an interleave, since these chunks are linear).

## What is already known about the hardware

The HC is a SIBO1 machine like the Series 3 and the MC, so it should fall
out of `core/series3.{h,cpp}` the way the MC200 did — a `Series3::Config`
and a registry entry, no new chip models:

| | |
|---|---|
| CPU | NEC V30H (not the MC's 80C86 — so `V30Variant::V30`, no `i8086` cycle scaling) at 3.84 MHz |
| Chip set | ASIC1 + ASIC2, as the Series 3 |
| LCD | 160x80 mono |
| RAM | HC100 128 KiB, HC110 256 KiB, HC120 512 KiB |
| ROM | 256 KiB Flash, top of the image at 0xFFFFF, boot entry at 0xA0000 |
| Packs | two SSD slots |

The frontend already has a photo skin for the machine at
`frontend/public/device-skins/hc120.png` (492x1084). Its LCD glass sits at
left 0.187, top 0.180, width 0.632, height 0.170 of the image — measured
from the image, and ready for an `EmulatorView` entry once there is a
device to point it at. A 160x80 panel is 2:1, so the active area inside
that glass is the full width with a 2:1 box centred in it.
