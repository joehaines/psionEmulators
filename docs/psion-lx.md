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
| netBook Pro board: display controller, NAND, power controller, keyboard, codec and touch panel, RTC, EEPROM, the two card sockets, sound output; the boot handoff | `core/netbookpro.{h,cpp}` |
| A PC Card / CF card as a socket sees it, and the CompactFlash storage card | `core/pccard.h` (over the shared `core/vcfcard.*`) |
| Intersil PRISM 2.5 802.11b PC Card (the Hermes MAC orinoco_cs drives) | `core/hermes_card.{h,cpp}` |
| The network behind its access point: DHCP, ARP, DNS and TCP/UDP to host sockets | `core/virtnet.{h,cpp}` |
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

## Card sockets

The netBook Pro has two: a CompactFlash slot (socket 0) and a PC Card slot
(socket 1). `drivers/pcmcia/pxa2xx_netbookpro.c` gives each a card-detect,
a READY / IREQ# and a RESET line on GPIOs (73/71/70 for the CF slot,
75/77/74 for the PC Card), and the PXA255 decodes each socket's 256 MB:
I/O at +0, attribute memory at +0x08000000, common memory at +0x0C000000,
from 0x20000000 and 0x30000000. The kernel's soc_common layer maps the I/O
space statically, so a card sees its own I/O address in the low bits.
Detect is active low and both edges interrupt, so a card inserted or
removed at run time reaches cardmgr as a real hot-plug event. READY is
also the card's interrupt request once it is in I/O mode, and the socket
driver sets it falling-edge — which matters below.

### CompactFlash

The CF card is the emulator's shared ATA model (`core/vcfcard.*`, the one
the EPOC machines use) in contiguous I/O mode, wrapped in `CfAtaCard`
(`core/pccard.h`) for what Linux's IDE driver needs on top of EPOC's: the
device-control register's nIEN gating the interrupt and SRST resetting the
drive, device 1 reading as absent (otherwise the probe finds a phantom
`hdb`), and RECALIBRATE completing like the other no-data commands. The
image's own cardmgr binds `ide-cs` and its `/etc/pcmcia/ide.opts` mounts
the first partition at `/media/cf`, with a "My CF Card" link in the `lx`
user's home. The frontend's card images (partitioned FAT16) are exactly
what that expects; the card dialog is the same one every CF machine uses.

### Wi-Fi

The netBook Pro had no radio on the board — Wi-Fi was a PC Card — so the
PC Card slot holds one: an Intersil PRISM 2.5 802.11b card running
station firmware 1.4.9, which is what `orinoco_cs` (0.13e, a module in the
initrd) drives. Its CIS carries manfid 0x0156/0x0002, which the image's
`/etc/pcmcia/config` binds to orinoco_cs; the driver reads the station
identity and treats it as Intersil firmware with WEP and IBSS support.

`HermesCard` (`core/hermes_card.*`) is the MAC as the driver programs it:
the command register with its parameters and responses, the event status /
enable / acknowledge registers, two buffer access paths (select, offset
and an auto-incrementing data port), frame buffers (FIDs) and
configuration records (RIDs). Once the port is enabled the card associates
with one access point, ESSID "PsionNet" on channel 6, and reports the link
in a link-status information frame; `iwconfig` shows it with its rate,
signal and quality. Frames go out as Ethernet frames and come back in the
receive buffers' 802.11 + 802.3 + SNAP layout.

Frames are paced at 11 Mb/s air time. That is realism, but also
necessary: the socket's interrupt is falling-edge and the card's request
is a level, and orinoco's handler gives up after ten passes. A card that
refills its receive buffer the instant the last one is acknowledged keeps
the request asserted across the handler's exit, and no edge ever comes
again (the transmit watchdog then fires with `EVSTAT=000b`). With the air
time between frames, a ~1 MB/s transfer runs without a lost interrupt.

The image's network scripts then do the rest: cardmgr runs
`/etc/pcmcia/network`, which DHCPs on `eth0`.

### The network behind the access point

`VirtualNetwork` (`core/virtnet.*`) is a user-mode router in the style of
QEMU's slirp. The guest is 10.0.2.15, the router 10.0.2.2, the DNS server
10.0.2.3; ARP, DHCP and ping of the router are answered in the core. TCP
connections end in a small TCP endpoint per connection (in-order receive,
the guest's window and MSS, go-back-N retransmission) and become plain
byte streams to a `NetHost`; UDP datagrams go to it as they are. 10.0.2.2
means the host machine itself, and DNS to 10.0.2.3 goes to the host's
resolver.

Who the `NetHost` is depends on where the emulator runs:

- **Desktop app** — the emulator's worker drains the requests after every
  frame and posts them up; the renderer hands them to
  `window.psionHost.net`; the main process opens them with Node's `net`
  and `dgram` (`desktop/src/main/netHost.ts`) and the events come back the
  same way. The LX is on the internet, through this computer.
- **Browser** — a web page cannot open sockets, so there is no host. The
  card still associates and the guest still gets its lease; connections
  past the router are refused at once and DNS answers SERVFAIL, so
  applications report that there is no network rather than hanging.
- **Native harness** — BSD sockets (`harness/lx-harness.cpp`), which is
  how the tests reach a server on the runner.

The Wi-Fi button on the toolbar (the Drives panel in the desktop app)
takes the card out of its slot and puts it back.

## Sound

The OSS driver (`sound/oss/pxa-audio.c`) plays through a chain of DMA
descriptors: one zero-length memory-to-memory marker per fragment, whose
end interrupt tells the driver a fragment is done, then the fragment's
data with flow control by the target, the AC97 PCM transmit FIFO. The
PXA255 model paces the channel DRCMR12 maps at the codec's front-DAC rate
(variable rate is enabled, so 44.1 kHz from the GPE sound server, 48 kHz
by default), one stereo frame per sample period, raising the end
interrupt when each descriptor drains. The board mixes the frames to mono
and resamples them to the host's 48 kHz stream.

The speaker amplifier is enabled by clearing GPIO 65, which the kernel
never configures as an output — the bootloader did, so the emulated
handoff does. Volume is the codec's master and PCM-out attenuators; the
mixer's default level plays at full scale on the host, a quieter setting
is attenuated from there, and either mute bit silences it.

`tests/integration/test-lx-peripherals.sh` checks all three from inside
the guest: it logs in on the serial console, reads, writes and hot-plugs a
CF card, plays a tone from it into `/dev/dsp`, and fetches a file over
Wi-Fi from a web server on the test machine.

## Not emulated (yet)

The SL811HS USB host, the microphone (the codec's ADC path), the battery
monitor stream the PCON sends over SSP (the driver logs that it times out
and retries), and suspend. The Wi-Fi card's WEP is accepted and ignored —
the access point is open — and 0.13e's driver has no scanning, so the card
only ever sees the one network.
