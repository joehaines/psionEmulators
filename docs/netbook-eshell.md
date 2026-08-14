# netBook: booting an OS image that isn't the stock one

`roms/ESHELL/OS.IMG` is a netBook build of ESHELL — EPOC's text console,
the thing Symbian's own engineers booted when they wanted a command
prompt instead of the Eikon desktop. It boots on real netBook hardware
from a CF card exactly like the stock `OS.IMG` does, and it now boots
here — and you can type at its prompt. Getting it there turned up three
places where the emulator had quietly bound itself to one particular OS
build rather than to the machine, which is what this note is about.
Booting a second image for the same hardware turns out to be a good way
of finding them: anything the emulator "knows" that is really a fact
about one ROM shows up immediately. The third one had been hiding a
whole working hardware path.

## What the image is

A 876,800-byte `EPOCARM ROM` image: the same 256-byte wrapper the stock
netBook `OS.IMG` carries, followed by an EPOC R5 ROM whose header
declares `iRomBase = 0x50000000`, `iRomSize = 0x00800000` and version
0.01. It prints

    ESHELL 0.01(213)   CFG=UREL
    Cold Reset

    Copyright (c) 1998 Symbian Ltd

    C:\>

on the 640×480 panel. Note that the declared `iRomSize` (8 MB) is the
size of the ROM *area* the image was linked for, not the length of the
file — the image is only 0.84 MB. Nothing should infer the file's
length from that field.

## 1. The MMU-enable hooks were overwriting the guest's page tables

The SA-1100 machines carry a set of L1 "gap fillers" in
`core/sa1100.cpp`: at the moment a guest enables its MMU we write
section descriptors for the PCMCIA windows, the relocated-kernel alias
at `0x88000000`, and a few identity mappings. They exist because a real
netBook's first-stage bootloader has already made those mappings before
the EPOC bootstrap runs, and in some of our boot paths nothing has.

They were written unconditionally, which meant they also replaced
mappings the guest had made for itself. For the stock netBook v1.05(450)
OS that was invisible: its own descriptors for those slots agree with
ours. ESHELL's don't. Its bootstrap builds a **coarse page table** at
L1[0] holding the exception-vector page; our filler replaced that slot
with an identity section onto SDRAM, so the SWI vector at VA 8 resolved
to whatever happened to be at physical `0xC0000008`. The first
executive call ESHELL made — a `svc` out of the stub table at
`0x5005dd4c` — landed on the word `0x0f000c00`, which decodes as
another `svc`, and the machine spun in the vector forever at a black
screen.

The fix is `writeL1IfUnmapped()`: a filler now only writes into an L1
slot the guest left as a translation fault. That keeps the fillers
doing the job they were added for — supplying what a pre-OS loader
would have supplied — without ever contradicting a running kernel. All
23 devices in `tests/devices.txt` still pass, the stock netBook OS
included.

`PSION_MMU_DUMP_L1=1` prints the guest's L1 at each MMU-enable (runs of
identically-offset sections coalesced), and `PSION_MMU_FILL_TRACE=1`
says which fillers were applied and which were declined. Between them
you can diff two ROMs' idea of the machine's memory map directly. For
the record, ESHELL's own map is:

    VA 00000000  coarse   (exception vectors)
    VA 40000000  coarse   (Eiger / ASIC14 window)
    VA 41000000  coarse
    VA 42000000-423fffff  coarse ×4
    VA 43000000-431fffff  coarse ×2
    VA 50000000-507fffff  section -> PA 0xC8000000   (the ROM image in SDRAM bank 1)
    VA 58000000  coarse   (SoC peripherals)
    VA 80000000  coarse   (kernel data)
    VA 80300000  coarse

## 2. The faithful-CF completion gate was sized for one image

The netBook's default boot here is the faithful one: the v0.11
bootloader mounts the FAT16 card and reads `D:\OS.IMG` through its own
medata/ATA driver, progress bar and all. What the emulator can't follow
is the bootloader's post-read restart (see
`docs/netbook-cf-bootloader-faithful.md`), so once the read has clearly
finished we complete the boot ourselves from the bytes the loader
actually pulled off the card.

"Clearly finished" was two hard-coded sector counts — 28000 for both
the idle gate and the hard gate — which were 94% of the 29894 sectors
the stock 14 MB `OS.IMG` drains, and nothing more general than that.
ESHELL's image is ~1700 sectors. It could never reach 28000, the
handoff never fired, and the bootloader carried on past the point we
model, into a fault storm.

Both gates are now a percentage of the image actually in the slot:
`attachCard` reads `D:\OS.IMG`'s length out of the card's own FAT16
root directory (`cfRootFileSize()`) and latches its sector count. The
stock OS gate lands at 26898 sectors instead of 28000 — the same
boot, slightly earlier — and ESHELL's at 1610. A card whose directory
we can't read falls back to the old absolutes.
`PSION_NB_FAITHFUL_BOOT_SECTORS` / `_HARD_SECTORS` still override with
absolute counts.

## 3. The keyboard was modelled at the wrong registers entirely

Key delivery on the netBook and Series 7 went through
`s7InjectRawEvent()`, which calls the kernel's `Kern::AddEvent` and
manipulates the event ring — at addresses that are literals from the
v1.05(450) / v254 builds (`0x500195d4`, `0x80000b54`, `0x800007f0`, …).
Against ESHELL those addresses hold uninitialised heap fill, so the
injection was calling a random function and writing kernel globals at
random.

That path exists because the hardware one didn't work, and the reason it
didn't work is that the emulator had the wrong registers. It modelled
the matrix as a halfword at Eiger `0x08`: low byte the column drive,
high byte the rows. No ROM reads that. The real pair, and every image
for this machine agrees on it, is:

| Register | Role |
|---|---|
| **Eiger `0x30`**, low nibble | column drive — `8+n` drives column *n*, `0` drives all columns at once, anything else drives none |
| **Eiger `0x04`**, low byte | the eight row lines read back |

The netBook OS and the Series 7 ROM both reach it through the same two
kernel helpers — netBook `0x50004ed8` "drive column"
(`modifyReg16(0x30, …)`) and `0x500048e0` "read rows"
(`readReg16(0x04)`) — the v0.11 bootloader carries the same routines,
and ESHELL's `ekeyb.dll` inlines them against VA `0x58030030` /
`0x58030004`, which are those offsets.

**No interrupt is involved.** Both drivers poll the matrix from a tick
callback (ESHELL's ekeyb installs a periodic one every 2 ticks), read
`0x04` with all columns driven as an "is anything down" probe, and only
walk the eight columns when that comes back non-zero. The long-standing
hunt for a keyboard bit among the sixteen Eiger IRQ slots — the
`PSION_S7_KBD_EIGER_BIT` sweep knob — was looking for something that
isn't there.

### The key map was wrong too

The `EpocKey → (column, row)` table was reconstructed from netBSD's
`epockbdmap.h` and did not match this keyboard: with the registers
fixed, typing `DIR` into ESHELL produced `4Ji`, and Enter produced
nothing at all. The machine's real map is a 64-entry table of
`{EPOC scan code, flags}` indexed by `column*8 + row`, and it is
**byte-for-byte identical** in all four ROMs we ship (ESHELL at
`0x5003e6c0`, the netBook OS, the Series 7 ROM, the bootloader) — as
you would expect of a property of the keyboard membrane rather than of
any one build. `setKeyboardKey` now carries a transcription of it:

```
        row0     row1   row2   row3    row4   row5   row6  row7
col0    Enter    Right  Tab    Y       Left   Down   N     LShift
col1    Bksp     –      -      =       0      P      ;     RShift
col2    Off      K      I      8       9      O      L     Ctrl
col3    –        ,      '      M       J      U      7     Fn
col4    Space    R      4      5       T      G      B     \
col5    Menu     F      V      C       D      E      3     /
col6    DictStop Q      A      Z       S      W      X     –
col7    Escape   1      2      6       .      Up     H     –
```

### Which path carries input

The two paths are mutually exclusive — run both and every press arrives
twice, which is what the old "skip the matrix update while synthetic
injection is on" comment was defending against. `s7SynthEventsUsable()`
is now the arbiter: it sanity-checks the kernel event ring, so the
netBook OS and Series 7 keep the synthetic path they are tuned for, and
a ROM it doesn't recognise drives the matrix instead.

**The stock netBook OS no longer needs the synthetic path either.** With
`PSION_S7_NO_SYNTH_TRAWEVENT=1` the v1.05(450) desktop now takes keys
through the hardware: the trace shows its kernel scanning at
`0x5000474c`, finding Menu at column 5 row 0, and the File menu opens —
and *stays* open, without the "suppress EKeyUp on Menu so the menu
doesn't close again" workaround the synthetic path needs. That was never
possible before, because the registers it was scanning weren't the ones
the emulator modelled.

Switching the default over is the obvious next step and deliberately not
part of this change: `s7InjectRawEvent` also carries **pointer** events,
so the kill switch turns off touch as well, and the netBook/Series 7 key
handling has a layer of workarounds (deferred `EKeyUp`, the Menu
suppression above) tuned for synthetic delivery that want unpicking
together rather than piecemeal. The finding is recorded here so whoever
does it starts from a known-good baseline.

## Testing

`tests/devices.txt` gains `netbook_eshell`: the bootloader plus a
synthesised FAT16 card carrying `roms/ESHELL/OS.IMG`, which is the exact
path the browser takes (`osCardSpec('netbook', 'eshell')`). It gates
both boot fixes at once — the small image exercises the scaled
completion gate, and the console only paints if the vector page
survived. The console is two grey levels, so its variance settles around
700 rather than the desktop's ~5000.

`tests/integration/test-netbook-eshell-keyboard.sh` covers input, which
no boot gate can see. It types `dir` + Enter at the prompt and requires
three things: the console changed against an untouched boot (ESHELL
echoes the command and prints its listing), the ROM read a non-zero row
byte back from `Eiger[0x04]` under a real column drive (so the keys went
through the registers, not a shortcut), and the synthetic path declined
this ROM rather than writing into a kernel it doesn't know. Shift and
Backspace are hardware keys in the same table and work the same way:
holding LShift across `A` `B` prints `AB`.

Worth knowing when reading the output: on a cold-booted ESHELL, `dir`
reports `0 Files, 0 Directories` — C: is the empty RAM drive.
