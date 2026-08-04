# netpad: what the ROM ships, how the MMC slot and audio work, and which way up it draws

Findings from working on the netpad that aren't bugs in the emulator but
shape what the device can do, recorded here so they don't have to be
re-derived. The MMC section below started as a list of open questions;
it is now the map of the hardware the emulator models.

## The ROM has only one document-creating app

**Symptom.** On the netpad desktop, "New file" offers exactly one entry
in its Program list: *Data*. Browsing `Z:` over Remote Link shows a
`\System\Apps\` full of familiar names — Word, Sheet, Agenda, Contacts,
Jotter, Record, Paint, Calc, Spell, TimeW, Comms, Msgapp, TextEd — which
makes it look as though the ROM isn't loading properly on boot.

**It is loading properly.** `roms/Netpad.img` is a complete EPOC R5 image:
a 0xC0000 boot partition followed by an OS ROM whose header (at file
offset 0xC0000) declares `iRomBase = 0x50000000` and `iRomSize =
0xB00000`, and every file the ROM directory lists resolves inside the
image we ship. Walking that directory from `iRomRootDirectoryList` gives
498 entries and exactly **eleven** `.app` executables:

    \System\Apps\PsiBoard\Psiboard.app     \System\Apps\Shell\Shell.app
    \System\Apps\TextCv\TextCv.app         \System\Apps\Data\Data.app
    \System\Apps\ImEd\ImEd.app             \System\Apps\MSWordCv\MSWordCv.app
    \System\Apps\FxVw\FxVw.app             \System\Apps\OPL\OPL.app
    \System\Apps\FxEd\FxEd.app             \System\Apps\InstApp\InstApp.app
    \System\Apps\SMEd\SMEd.app

The other twenty-odd `\System\Apps\` directories carry only the app's
resources — `.aif`, `.mbm`, `.rsc`, `.hlp` — with no executable. Their
engines *are* in `\System\Libs` (`WpEng.dll`, `ShEng.dll`, `AgnModel.dll`,
`CNTModel.dll`, …), and their document templates are in
`\System\Templates`, but the application binaries are simply not in the
image. `\System\Install` holds four SIS files (TcpIp, PRINTDRV, Stdlib,
VIEWERS) and none of them carries a missing app either.

So Apparc's list is right and the Shell is showing what the ROM has.
`Data` is the only ROM app that creates documents; getting the rest onto
the machine means installing them, the same way the app library already
delivers SIS packages over Remote Link.

What can be installed is not limited to the machine's own support CD
(`applib/netpad`, behind the **Install standard apps** button). The
netpad is an EPOC R5 ARM machine with the 5mx's own 640×240 panel, so
every ER5 app in the library is catalogued for it and installs by the
same route — `bash tests/integration/test-netpad-app-install.sh
epocgames/fred` puts a 3-Lib game in its Extras bar on the real ROM.
Bundles that are an installed app folder rather than an installer take
the cable instead of the card (a card carries 8.3 names only); the
netpad answers that from a cold boot here, because `sa1100.cpp` parks
its single boot-time `Req_Req_Pdu` for the host bridge —
`bash tests/integration/test-epocdir-install.sh --device netpad`.

To re-check this against another netpad image, walk the ROM directory
from the header's `iRomRootDirectoryList` (file offset 0xC0000 + 0x94).
Entries are `{ TInt iSize; TLinAddr iAddr; TUint8 iAtt; TUint8 iNameLen;
char iName[] }`, padded to four bytes, with `iAtt & 0x10` marking a
directory whose `iAddr` points at the child `TRomDir` (`TInt iSize`
followed by the entries).

## MMC

The netpad's removable-media slot takes an **MMC card in SPI mode**, and
it now works: attach a FAT16 image and EPOC mounts it as drive `D:`, the
System screen browses it, and files written from the machine land in the
image. `medmmc.pdd` names its transport `Media.MmcSpi`, which was the
first clue; the rest of this section is the hardware map behind it, all
of it derived from the ROM's own code.

### The card is on the board FPGA, not the SoC's SSP

The obvious guess — that the card hangs off the SA-1110's SSP alongside
the ADS7846-class touch/ADC codec — is wrong. A full boot's SSP trace
(`PSION_NETPAD_SSP_TRACE=1`) contains only the codec's control bytes and
never an MMC `0x40 | cmd`, because the MMC port is a separate block in
the board FPGA, on nCS4 (physical `0x40000000`, reached through VA
`0x58030000`). The ROM ships the FPGA's own driver, `d_fpga.ldd`, and
its bitstream as `\System\Programs\FPGA$ROM.BIN`.

Five registers make up the port. The kernel's ASSP wraps each one in a
tiny accessor, which is how they can be named with confidence:

| Reg | ASSP accessor | Meaning |
|-----|---------------|---------|
| `0x100` | `0x50005078` | control: bit 0 enable, bit 3 selects 8-bit frames over 16-bit, bit 5 is the card's chip select |
| `0x102` | `0x500050a0` | status: bit 0 busy, bit 3 port ready (masked to the low nibble) |
| `0x104` | `0x500050b8` | clock divider — the variant programs 400 kHz for card identification |
| `0x106` | `0x500050d4` / `0x500050e4` | 16-bit data register |
| `0x108` | `0x500050f0` / `0x50005100` | 8-bit data register |

Two more FPGA blocks matter. `0x10` / `0x12` are the interrupt status and
enable for sixteen lines EPOC names `IrqExternal0`..`15`; the ASSP finds
the lowest pending line at ROM `0x500a7004`, masks at `0x500a6f74` and
acknowledges by writing the bit back at `0x500a6fac`. The FPGA's
interrupt output feeds **SA-1110 GPIO 10** — ECust binds `IrqGpioEdge10`,
and GPIO 10 is the only pin besides the power button, GPIO 0 and the pen
line whose rising edge the OS ever arms.

### Card detect is two bits of register `0x1a`, read with opposite senses

Both have to move together or nothing happens:

* **bit 6, inverted.** The variant's socket-info card-present field
  (ROM `0x500a8014`: present == `!(reg & 0x40)`), which the peripheral
  bus's socket poller samples.
* **bit 3, straight.** The bus controller gates on this before it will
  look at the socket at all (ROM `0x500a8044` for socket 0:
  `(reg >> 3) & 1`). This is the one the earlier investigation was
  missing — with only bit 6 modelled the socket reports a card and is
  then ignored, which is exactly the dead end that looked like "presence
  isn't a level".

Presence is *not* a plain SA-1110 GPIO level: pulling GPIO 11..13 high
does nothing, which is what the first pass found and why it looked as
though the PIC board controller had to be involved.

### Sockets, media ids and who does what

EPOC's peripheral bus (`EpBus.dll`) sees two sockets, and the variant
hardcodes their types at ROM `0x500a7730`: **socket 0 → card type 2
(MMC)**, socket 1 → type 8 (PC-Card). Media ids map to sockets at
`0x500a80f8` (2→0, 3→1, 4→2, 5→3), and `DPccdMediaDriver::DoCreate`
(`0x5005b75c`) refuses a media whose socket type doesn't match the
driver's own — `medmmc.pdd` answers 2, so it only ever binds to media 2 /
socket 0. Media change for socket 0 is `IrqExternal11`, socket 1's is
`IrqExternal12` (ROM `0x500a8088`), which is what `d_mmc.ldd`'s
`PIC MEDIA CHANGE BOUND OK` / `PCMCIA MEDIA CHANGE BOUND OK` log lines
are naming.

The card-identification handshake is in the **variant**, not the media
driver: ROM `0x500a81c8` is a ten-state machine that deasserts chip
select and clocks ten 0xFF fill bytes (the spec's 80 idle clocks),
selects the card, sends CMD0 with the correct `0x95` CRC, polls CMD1
until R1 reads 0, sends CMD13, and only then reports the socket ready.
`medmmc.pdd` takes over from there with CMD9 (SEND_CSD) and CMD17 /
CMD24 for the blocks, switching the port to 16-bit frames for the
512-byte payloads and back to 8-bit for the CRC.

### What the emulator models

`core/netpad_mmc.{h,cpp}` is the card: a byte-level MMC SPI slave
(CMD0/1/9/10/12/13/16/17/18/24/25/32-38/58, R1/R2/R3, data tokens, CRC7
and CRC16) over a raw FAT16 image. `core/sa1100.cpp` models the FPGA
port around it, including one inference worth recording: **the port has
an eight-entry receive FIFO.** The two ROM drivers pace it differently —
`medmmc.pdd` pairs every data-register write with one read, while the
variant's state machine writes eight fill bytes in a row and only then
reads eight results — and that only adds up if received frames are
buffered. Eight is the depth the state machine assumes.

Two other details the drivers pin down: the response latency has to be at
least one idle byte (medmmc clocks a trailing 0xFF of its own after each
six-byte command and throws the result away, so answering immediately
would hide the R1), and the 16-bit data register carries the **first byte
on the wire in its low half** — the block reader stores the low half at
`buffer[0]` and the writer packs `buffer[0]` into it.

### Diagnostics

* `PSION_NETPAD_MMC_TRACE=1` — every SPI frame, chip-select edge and
  FPGA interrupt.
* `PSION_NETPAD_NCS4_TRACE=N` — every board-register access (N caps the
  read lines, 0 = unlimited).
* `PSION_NETPAD_FPGA_READ=<reg>=<hex>,…` — pin a board register's read
  value, for probing a signal whose sense isn't known yet.
* `PSION_NETPAD_PC_RANGE=<lo>-<hi>` — log the first execution of every
  PC in a range (a range of 0x20 bytes or less becomes a watchpoint that
  logs every visit with r0/r1). Point it at a ROM executable's address
  range — walk the ROM directory for its `iCodeAddress` — to see whether
  a driver ran and how far it got.
* `PSION_NETPAD_LR_WATCH=<addr>` — name the callee of an indirect
  branch by logging PCs seen while `lr` holds a given return address.
  This is what identified the `0x500a8044` presence gate.

`bash tests/integration/test-netpad-mmc.sh` drives the whole chain and
`tests/unit/mmc_card_test` checks the card model on its own.


## "Switch orientation" happens entirely in software

The Tools menu on the netpad's System screen carries **Switch
orientation**, and choosing it turns the machine from a landscape slab
into a portrait one. Nothing about it is visible on the bus.

Across the switch the LCD controller is not reprogrammed at all — a
`--log-file` run shows `LCCR1=01010a70 LCCR2=00000cef DBAR1=c0000000`
before and after, i.e. the same 640x240 panel scanned out of the same
framebuffer — and no board register, FPGA register or I2C transfer moves
either. EPOC's screen driver simply starts drawing the whole UI rotated
inside the existing framebuffer, the way Symbian's `CFbsDrawDevice`
orientation support has always worked. Screenshots confirm it: the
desktop comes out sideways within an unchanged 640x240 image, and turning
that image a quarter-turn **anticlockwise** makes it read upright, with
the title bar along the top and the System toolbar along the bottom.

Two consequences for a host:

* **It has to read the state out of the guest.** The orientation lives in
  the screen driver's draw device — an `ScDv.dll` object (the DLL is at
  ROM `0x502d3ce0`) whose tail is `+0x20` width 640, `+0x24` height 240,
  `+0x28` framebuffer VA + 0x200 (past the palette the LCD DMA reads
  first), `+0x30` the orientation quadrant, `+0x34` framebuffer VA
  `0x58040000`. Scanning RAM bank 0 for the (640, 240, fb + 0x200)
  triple finds exactly one match, and `+0x30` reads 0 landscape / 1
  rotated, flipping on every use of the menu entry. That is what
  `Emulator::getScreenOrientation` (core/sa1100.cpp) reports, and it is
  the only thing the frontend needs in order to turn the device to match.
* **Pen coordinates do not rotate.** The digitiser keeps reporting panel
  coordinates whichever way up EPOC is drawing — the window server
  applies the rotation to incoming pointer events itself. A host that
  rotates its display therefore has to rotate taps back before sending
  them, and nothing in the emulator's touch path changes.
  `tests/stress/netpad_orientation.sh` proves this end to end: the tap
  that switches the machine back is placed at the panel coordinate the
  menu entry occupies *while rotated*, computed through the same
  transform.


## Audio is an AC'97 codec on the board FPGA

The netpad's sound hardware is not the SA-1110's MCP port that its
Series 7 / netBook siblings use for their UCB1200. It is an **AC'97
codec** on a controller in the same board FPGA that carries the MMC
port, reached through the same nCS4 window at VA `0x58030000`
(PA `0x40000000`), and it is now emulated: `core/sa1100.cpp`'s
`netpadAc97*` functions model the controller and the codec's register
file, with the receive FIFO fed from the host microphone and the
transmit FIFO drained to the host speaker.

### Three drivers, one register map

Three pieces of the ROM drive the block and they agree on every detail,
which is where the map below comes from:

* the **ASSP accessors linked into `EKern.exe`** — the leaf functions
  every other driver calls;
* **`\System\Libs\Esdrv.pdd`**, the sound PDD, which registers itself as
  `Sound.Ac97` and is what `ESound.ldd` (`RMdaDevSound`, device name
  `Sound`) binds to;
* **`\System\Libs\d_ac97.ldd`**, a test driver published as
  `Codec.Ac97`, whose `DoControl` dispatch (ROM `0x5036cb20`) exposes
  ten entries — write / read a codec register, play a buffer, power up,
  power down, a square-wave tone test, record a buffer, start a play
  stream, set volume, and report the residue.

| Reg | ASSP accessor | Meaning |
|-----|---------------|---------|
| `0x200` | `0x500051a0` / `0x500051b0` | PCM data. Write pushes a playback sample into the transmit FIFO, read pops a recorded one out of the receive FIFO. 16-bit signed, one sample per access |
| `0x202` | `0x50005158` / `0x50005180` | codec register index. Bit 0 picks direction: an even index commits the value staged at `0x204`, `index\|1` starts a read whose result appears there |
| `0x204` | `0x50005194` | codec register data |
| `0x206` | `0x50005118` | control: bit 0 releases the codec's reset, bit 1 enables the AC-link, bit 2 flushes the receive FIFO |
| `0x208` | `0x50005140` | status, masked to `0x9f`: bit 0 receive FIFO empty, bit 4 transmit FIFO full, bit 7 codec command busy |

Both FIFOs are **16 deep**: the play path pre-fills exactly sixteen
samples before it enables its interrupt (ROM `0x502e79d8`), and the
record ISR drains eight per service request (`0x502e7a5c`).

The codec itself is a stock AC'97 part. `Esdrv.pdd`'s record
configuration (ROM `0x502e7ac8`) mutes master (`0x02`), PC beep (`0x0a`)
and PCM out (`0x18`), sets the variable-rate bit in extended audio
(`0x2a`), programs the **PCM L/R ADC rate** (`0x32`) to 8000, selects
the microphone in record select (`0x1a`), picks MIC1 by clearing bit 9
of general purpose (`0x20`), and finally writes the mic volume (`0x0e`)
and record gain (`0x1c`) from the values Control panel → **AC97 Record**
stores in the publish-and-subscribe keys `SYS$SND::REC::GAIN`,
`SYS$SND::REC::MIXER` and `SYS$SND::REC::BOOST`. The play path
(`0x502e795c`) is the mirror image, programming the **PCM front DAC
rate** (`0x2c`) instead. Samples reaching the LDD are converted to 8-bit
µ-law by the encoder at ROM `0x502e757c`.

### Two things the map does not say, and what the emulator does instead

* **Power-up is a reset pulse, not a powerdown-register walk.** The
  variant parks the codec at the end of boot by walking the powerdown
  register `0x26` up to `0x3b80` (ROM `0x500a6ab4`), but neither
  driver's power-up sequence clears those bits again: both just drive
  the control register's reset bit low and then high, wait, and spin
  until `0x26`'s low nibble — a read-only "REF / ANL / DAC / ADC ready"
  status — reads `0xf`. So the reset bit has to reload the codec's
  register file with its power-on defaults, and the emulator models it
  that way. (The low nibble also has to read `0xf`: the permissive nCS4
  default used to answer `0x80` for these registers, which is exactly
  the *busy* bit, so every codec access the variant made during boot
  spun its wait loop until the driver's own retry count ran out —
  around 440 K cycles apiece, ~43 000 status reads over a boot.)
* **Which interrupt line is which direction is not decidable from the
  ROM.** Both drivers carry the strings `IrqExternal3` and
  `IrqExternal4` — lines 3 and 4 of the FPGA's interrupt controller at
  `0x10` / `0x12` — and both bind both, and neither is ever opened
  during an ordinary session, so no trace pins the assignment down. It
  does not have to be pinned down: the sound PDD runs one direction at a
  time and arms only the line it is waiting on, so
  `netpadAc97ServiceIrq` raises whichever of the two is actually
  enabled and is correct under either mapping.

### The ROM ships no recorder

Nothing in `Netpad.img` opens the sound device during normal use — the
key and screen clicks the Sound control panel governs go to a separate
board register (nCS4 `0x0c`), not through the codec, and the only
audio-related app in the image is Control panel → **AC97 Record**, which
just stores the three gain settings. `harness/netpad-audio-harness`
therefore drives the block over the emulated bus with the exact register
sequence `Esdrv.pdd`'s record path performs, checks that a tone pushed
in through the host microphone bridge comes back out of the PCM data
register unaltered and at the codec's programmed rate, and checks the
playback direction and the service interrupt the same way. Run it with
`bash tests/integration/test-netpad-audio.sh`.

`PSION_NETPAD_AC97_TRACE=1` logs codec register traffic, control-register
changes and service interrupts.
