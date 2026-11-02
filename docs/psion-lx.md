# Psion LX

The Psion LX was Psion Teklogix's Linux prototype: the netBook Pro's
hardware (late 2004) running a Linux 2.6 system that Simtec Electronics
built with OpenEmbedded, under a "PSION LX BETA" splash. It never shipped.
The files in `roms/netbook_LX_prototype/` are the prototype's OS image, its
root filesystem tree and the sources for both; this emulator boots them on
emulated netBook Pro hardware to the GPE desktop, where the stylus, the
keyboard and the image's applications (Word, Sheet, Calendar, Contacts and
the rest) work.

Device id `psionlx` (`core/device_registry.cpp`). It is the only machine on
its own core:

| Part | Where |
|---|---|
| Intel XScale CPU, ARMv5TE, ARM + Thumb, MMU, CP15/CP14/CP0 | `core/xscale.{h,cpp}` |
| Intel PXA255 on-chip peripherals | `core/pxa255.{h,cpp}` |
| netBook Pro board: display controller, NAND, power controller, keyboard, codec and touch panel, RTC, EEPROM; the boot handoff | `core/netbookpro.{h,cpp}` |
| zlib/gzip decoder for the OS image and the NAND image | `core/inflate.{h,cpp}` |
| Builds the factory NAND image from the rootfs tree | `scripts/build-lx-nand.mts` |

## What is in the image files

`roms/netbook_LX_prototype/Psion LX - Linux prototype/psion-netbookpro-linux-sources/nBkProOs.img`
(copied, byte for byte, to `roms/netbook_LX_prototype/LX_boot/` so the site
and the desktop app can ship it without the source tree around it) is what
the netBook Pro bootloader loads:

- a 256-byte header: an ARM branch over it, `nBk2` at 0x0c, the payload
  length at 0x10, the load offset into SDRAM (0x408000) at 0x18, the name
  and "NETBOOKPRO Linux26";
- then a big-endian uncompressed length and a zlib stream: 10.8 MB that is
  the kernel `Image` ("Linux version 2.6.9-rc1-bk18-nb1 … Tue Jan 4 02:27:32
  GMT 2005"), a loader stub, and an 8 MB ext2 initrd.

The first word of the kernel was replaced with a `bl` to the stub, which
does the bootloader's handoff itself: it moves the kernel down four bytes
into place, reads the SDRAM size the bootloader leaves at 0xA00000F4, writes
the ATAGs at 0xA0400100 (machine 503, `MACH_NETBOOKPRO`; command line
`root=/dev/ram rw console=ttySA0,115200 init=/linuxrc`; the initrd at
0xA1000000), copies the initrd there and jumps to the kernel. So the
emulator does exactly what the bootloader did and no more: inflate the
payload to 0xA0408000, write the RAM size, and start it with the MMU off.

The initrd's `/linuxrc` runs cardmgr, offers a CompactFlash installer
(`NBLXLoad.sh` on `hda1`), then mounts the root filesystem — JFFS2 on the
internal NAND's first partition, falling back to `hda2` — and pivots into
it.

## The root filesystem

The NAND holds the OpenEmbedded `soho-image` tree in the same folder. It
reaches the emulator as an image of the whole 64 MB NAND — 131072 pages of
512 data bytes, each followed by its 16 spare bytes — which
`scripts/build-lx-nand.mts` builds from the tree: JFFS2 as `mkfs.jffs2`
made it for this machine (32 KB erase blocks, no cleanmarkers) at the
kernel's partition offset (4 MB), with the software ECC the NAND layer
checks on read in every page's spare area. The build is deterministic and
takes about ten seconds, so the image is generated rather than committed
(`.gitignore`); CI builds it before the deploy staging
(`scripts/stage-deploy-roms.mts` runs it when it is missing) and before
desktop packaging, and `tests/boot/test-boot.sh` builds it on first use.

The tree in git is not quite what Psion's build produced, and the builder
puts back what is missing: the empty directories (git keeps none — `/sys`,
`/initrd`, `/media/*`, `/etc/gtk-2.0`, `/usr/lib/locale` and the others the
boot scripts write into), the device nodes (from the image's own
`/etc/device_table`), and ownership (root, with the `lx` user's home
directory). It holds 100 MB of files in 50 MB of JFFS2, in the 56 MB
partition.

### First boot

The image was built to finish installing on its first boot, as a real LX
did after it was flashed: `/etc/rcS.d/S98configure` runs every package's
post-install script — generating the locales, the GTK and Pango module
lists, the MIME database — and then deletes itself. That takes about 100
emulated seconds, and the desktop appears at about 160 s. Later boots skip
it and reach the desktop in about 70 s. In the browser a saved session
resumes where it was, so this is a one-off.

## The hardware

**PXA255** (`core/pxa255.cpp`): interrupt controller, the four OS timers at
3.6864 MHz, RTC, GPIO with edge detection, the four UARTs (16550-style with
transmit timing modelled — the loader stub's own print routine depends on
it), I2C as master and slave, AC97 with the codec-access handshake the
driver waits on, the MMC controller (no card: every command times out, which
is what the driver needs to see — without it the card probe blocks the
kernel's shared workqueue and the boot never leaves `rcS`), SSP and DMA
register files with memory-to-memory transfers, and the clock, power and
memory-controller registers. Time is counted in instructions, 200 million to
the emulated second.

**XScale core** (`core/xscale.cpp`): the full ARMv5TE instruction set in
ARM and Thumb state, the MMU (sections, coarse and fine tables, large,
small, tiny and XScale extended pages, domains, S/R protection, alignment
faults, base-restored aborts), FCSE, CP15, CP14 (clock configuration, idle,
performance monitor) and the CP0 DSP accumulator. FPA instructions trap, as
on the real chip, and the kernel's NWFPE emulates them. ARM-state code runs
from a cache of decoded instructions, one record per instruction per
physical page, dispatched through specialised handlers; pages that hold
decoded code lose their direct write path in the TLB so a store to them
drops the stale records. `tests/unit/run-xscale-test.sh` checks the core
against the host: one C program built for the host and for ARMv5TE (ARM and
Thumb), run on both execution paths, must print the same.

**Board** (`core/netbookpro.cpp`):

- Epson S1D13806 on nCS5: registers, the 1.25 MB display buffer (mapped
  straight into the CPU's fast path) and the BitBLT engine, which the
  image's X server (`Xepson`, kdrive) uses for fills and copies. 800×600 at
  16 bpp; the backlight level is the controller's GPIO register.
- 64 MB x16 Samsung NAND on nCS1, its CLE/ALE/nCE/WP on GPIOs. The kernel's
  driver never sets those pins' directions — the bootloader did — so the
  emulated handoff does.
- The PCON power controller at I2C 0x13: power-state requests, which it
  acknowledges by writing the new state into the PXA255's own I2C slave
  (0x70), and the keyboard, whose scan codes reach the kernel the same way.
- Ricoh R2051 RTC at I2C 0x32, set from the host clock; the guest can set it.
- The configuration EEPROM at I2C 0x53, carrying the factory touch
  calibration that `xtscal -e2cal` reads on the first X start (five points,
  written from the emulated panel's own mapping, so taps land exactly).
- Wolfson WM9705 AC97 codec with its touch digitiser: pen-down on GPIO 4,
  X/Y/pressure samples streamed into AC97 slot 5 (the driver reads them from
  `MODR`), evdev and tslib up to X.

## Input

The keyboard is the netBook Pro's, as `drivers/input/keyboard/netbookpro.c`
maps it, with the X keymap from `/etc/X11/xmodmap-netbookpro`: Fn (host
Meta / Windows key) is the AltGr layer for `£ € { } [ ] |` and the Fn
combinations the kernel turns into Home, End, Page Up/Down, Caps Lock and
F1. The frontend's `netbookpro` layout (`frontend/src/lib/keymap.ts`) sends
those chords. The stylus is the host pointer.

## Not emulated (yet)

The CompactFlash and PC-Card sockets (the image probes them and finds them
empty), the SL811HS USB host, sound output (the codec answers, no samples
leave it), Wi-Fi, the battery monitor stream the PCON sends over SSP (the
driver logs that it times out and retries), and suspend.
