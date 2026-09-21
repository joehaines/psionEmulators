# Psion Emulator

A web-based emulator for Psion handheld computers, spanning three CPU
architectures and fifteen years of devices. Play any of them in the browser at
**[joehaines.com/psion](https://joehaines.com/psion)**.

- **Hitachi HD6301X0 / HD6303X** — the 1984 [Psion Organiser I](https://en.wikipedia.org/wiki/Psion_Organiser)
  and the 1986 Organiser II.
- **NEC V30 / V30H** (with Psion's ASIC1 / ASIC2 / ASIC9) — the SIBO family:
  Series 3, 3a, 3c, 3mx, Siena, Workabout, MC200/MC400/MC218.
- **ARM710 (CL-PS7110 / Windermere) and StrongARM SA-1100** — the ARM-era
  machines: Series 5, 5mx, Revo, Series 7, netBook, and the licensed
  Geofox One.

Every CPU core, peripheral and OS path is emulated natively in C++, compiled to
WebAssembly via Emscripten and driven from a React + TypeScript + Vite frontend.

## Supported devices

| Device | Year | CPU | Status | CF | SSD | Speaker | Mic | Touch | IR | Link |
|--------|------|-----|--------|----|-----|---------|-----|-------|----|------|
| Organiser I | 1984 | HD6301X0 | ✅ | — | — | ❌ | — | — | — | ❌ |
| Organiser II (LZ/LZ64) | 1986 | HD6303X | ✅ | — | — | ❌ | — | — | — | ❌ |
| MC200 | 1989 | Intel 80C86A | ✅ | — | ✅ ×4 | ✅ | — | — | — | ❌ |
| MC400 | 1989 | Intel 80C86A | ✅ | — | ✅ ×4 | ✅ | — | — | — | ❌ |
| Series 3 | 1991 | NEC V30 | ✅ | — | ✅ ×2 | ✅ | — | — | — | ❌ |
| Acorn Pocket Book | 1992 | NEC V30 | ✅ | — | ✅ ×2 | ✅ | — | — | — | ❌ |
| Series 3a | 1993 | NEC V30H | ✅ | — | ✅ ×2 | ✅ | ✅ | — | — | — |
| Workabout | 1995 | NEC V30H | ✅ | — | ✅ ×2 | ✅ | ✅ | — | — | ❌ |
| Series 3c | 1996 | NEC V30H | ✅ | — | ✅ ×2 | ✅ | ✅ | — | ✅ | ✅ |
| Acorn Pocket Book II | 1996 | NEC V30H | ✅ | — | ✅ ×2 | ✅ | ✅ | — | — | ❌ |
| Siena | 1996 | NEC V30H | ✅ | — | ✅ ×1 | ✅ | ✅ | — | ✅ | ✅ |
| Series 5 | 1997 | ARM710 (CL-PS7110) | ✅ | ✅ | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| Geofox One | 1997 | ARM710 (CL-PS7110) | ✅ | ❌ | — | ❓ | — | — | ❓ | ✅ |
| Series 3mx | 1998 | NEC V30H | ✅ | — | ✅ ×2 | ✅ | ✅ | — | ✅ | ✅ |
| Osaris | 1998 | ARM710 (CL-PS7111) | ✅ | ✅ | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| WorkaboutMX | 1998 | NEC V30MX | ✅ | — | ✅ ×2 | ✅ | ✅ | — | ❌ | ✅ |
| Series 5mx | 1999 | ARM710 (Windermere) | ✅ | ✅ | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| Ericsson MC218 | 1999 | ARM710 (Windermere) | ✅ | ✅ | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| Revo | 1999 | ARM710 (Windermere) | ✅ | — | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| Series 7 | 1999 | StrongARM SA-1100 | ✅ B| ✅ | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| netBook | 1999 | StrongARM SA-1100 | ✅ | ✅ | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| Series 5mx Pro | 2000 | ARM710 (Windermere) | ✅ | ✅ | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| Revo (Conan) | 2001 | ARM710 (Windermere) | ✅ | — | — | ✅ | ✅ | ✅ | ✅ | ❌ |
| netpad | 2001 | StrongARM SA-1110 | ✅ | ✅ MMC | — | ⚠️ | ✅ | ✅ | ✅ | ✅ |

✅ Supported · ⚠️ Partial / not fully booted · ❌ Not yet emulated · — Hardware not present · ❓ Hardware present, not yet verified.
**CF** = removable storage card (CompactFlash, or MMC on the netpad).
**Link** = Remote Link / PsiWin file access over the serial cable — ❌ on the
Conan means the port is there but its ROM speaks a later link protocol this
emulator's PsiWin client can't.

### Feature notes

- **Series 7 / netBook** boot to a real EPOC desktop. The netBook loads its OS
  the faithful way — the bootloader reads `D:\OS.IMG` off the FAT16 CompactFlash
  card through its own driver. CompactFlash, microphone capture/playback and
  Remote Link all run through the machines' own kernel driver stacks. The
  netBook boots an OS image that *isn't* its stock one too: **Boot ESHELL**
  puts Symbian's EPOC text console on the card instead, the bootloader reads
  and starts it by the same faithful path, and you can type at its `C:\>`
  prompt — the keys go through the Eiger/ASIC14 keyboard matrix into the
  image's own `ekeyb.dll`, with no help from the emulator. See
  [docs/netbook-eshell.md](docs/netbook-eshell.md). A third image boots by the
  same path: `roms/OS.IMG` is a netBook build of EPOC's pen-oriented **Quartz**
  UI, and it runs through the Quartz v6.0 splash to the Quartz app screen. See
  [docs/netbook-quartz.md](docs/netbook-quartz.md), which is also the record of
  the two emulator-side bugs it found — both places where a workaround had
  bound itself to one particular OS build (its kernel-data layout, and its
  literal code addresses) rather than to the machine.
- **netpad** — the SA-1110 sibling of the netBook, booting its own EPOC R5 ROM
  to the netpad desktop on a 640×240 8 bpp colour panel (a 6×6×6 palette cube).
  Its board peripherals are emulated (bit-banged I2C board controller, nCS4
  serial device, GPIO handshakes); EPOC boots into its "off" state, so an
  emulated power-button press switches the machine on through the SA-1110
  sleep-mode-reset path. The stylus runs through the machine's own driver
  stack: the SoC's SSP clocks an ADS7846-class board codec (touch X/Y/pressure
  plus the battery and temperature channels) with the pen-detect line on
  GPIO 14, so taps reach EPOC via Exyin.dll exactly as on hardware. It runs
  its OS timer at the spec-correct 3.6864 MHz (its Series 7 / netBook
  siblings still need a 10× rate to boot), so guest-measured time is
  honest: key auto-repeat no longer fires mid-press, the board codec's
  transfers settle before the driver reads them back, and the clock keeps
  real time. Being a pen machine it has no keyboard — text is typed on
  EPOC's on-screen Psiboard, and the case carries just two assignable
  buttons — so host keys (the frontend's Menu button above all) are
  delivered as synthetic key events through the kernel's own
  `Kern::AddEvent`. The touch plate is wider than the panel, and the
  overhang carries the five silkscreen keys printed down the right-hand
  side of the screen (Menu, brightness, Zoom, on-screen keyboard, Extras);
  the frontend draws that column in both the skinned and skinless views,
  and taps on it reach the ROM. Infrared rides the SoC's ICP on UART2,
  the same wiring as its SA-1100 siblings, and takes a full Eikon-IR beam.
  Control panel → Screen drives the board's 5-bit contrast DAC, which the
  render path applies to the panel, and the PIC board controller on the
  I2C bus reports a healthy main battery to Information → Battery.
  The removable-media slot takes an **MMC card** — attach a FAT16 image
  and EPOC mounts it as drive D:, browsable and writable from the System
  screen. The card is an MMC in SPI mode on the board FPGA's own port
  (not the SoC's SSP), and it comes up through the machine's real driver
  stack: card-detect and a media-change interrupt on the FPGA's
  interrupt controller, the variant's CMD0/CMD1 identification state
  machine, then `medmmc.pdd` reading and writing 512-byte blocks.
  **Switch orientation** on the Tools menu turns the machine from a
  landscape slab into a portrait one: EPOC redraws the whole desktop
  rotated inside the same 640×240 framebuffer without touching the LCD
  controller, so the emulator reads the state out of the screen driver
  and the frontend turns the device — case photo, silkscreen column and
  all — a quarter-turn anticlockwise to match, mapping pen taps back
  through the same rotation. **Audio** is an AC'97 codec on a controller
  in the same board FPGA as the card slot — five halfword registers at
  nCS4 0x200 — and the microphone runs the whole way through it: host
  mic samples clock into the controller's receive FIFO at the rate the
  codec's own rate register is programmed to, surface to the machine at
  the PCM data register, and raise the FIFO service interrupt the sound
  PDD waits on. Playback shares the same FIFO register and works the
  same way. The board's separate key-click piezo is not wired, which is
  why the speaker column above reads partial. See
  [`docs/netpad-rom-and-mmc.md`](docs/netpad-rom-and-mmc.md) for the
  register map, which also explains why the ROM's only document-creating
  app is Data.
- **Geofox One** — a 1997 EPOC32 clamshell from Geofox Ltd, a Psion
  licensee, on the same ARM710 + CL-PS711x platform and the same EPOC
  Release 1 kernel generation as the Series 5 (ROM 1.01(146) against the
  Series 5's 1.01(144)). It **boots to the EPOC desktop** on its 640×320
  panel — icon column, epoc hand, System pane and running clock — with no
  CPU exceptions over a 40-second boot, and its applications open and run:
  Word, Sheet, Data, Agenda, Calc, Time (with the world map) and the Extras
  bar. The twelve keys etched down the keyboard deck go straight to their
  applications, and the machine is navigable from the keyboard throughout.
  Mail and Web answer "Not in ROM" — this image carries Geofox's
  `MailDum.app` / `WebDum.app` stubs, so that is the ROM's own behaviour.

  Four things had to be fixed to get there, each measured rather than
  guessed. The first is a CPU bug, not a Geofox one:
  - **ARM exception entry must mask IRQs.** The core only set CPSR.I on
    IRQ and FIQ entry, following WindEmu; real ARM hardware sets it on
    every exception, SWI included, and the Geofox is the clearest case
    anywhere in this tree of why that matters. Its EKern hands SVC mode
    and IRQ mode *the same 1 KB stack* — the mode-stack setup at ROM
    0x50019BC8 loads SP_svc and SP_irq from one pointer — which is only
    safe because a SWI is entered with interrupts off. With them left on,
    the first timer IRQ to land inside a fast-path executive call pushed
    r0-r3/ip/lr straight over the live SVC frame; the call then returned
    into a stale handler address, and the wreckage surfaced seconds later
    as a USER 19 (bad descriptor type) panic, a prefetch abort into EPOC's
    own 0xBB stack poison, and a hang inside the 64-bit divide at
    0x5004D65C entered mid-loop with a garbage iteration count. The window
    server never ran, so the panel stayed blank. Series 5 already carried
    the fix under its own flag; the Geofox now sets it directly, and
    `tests/devices.txt` gates it — with `PSION_ALL_EXC_IBIT=0` the boot
    goes back to five exceptions and a variance of zero.
  - **The configuration PROM on the SSI bus.** The variant driver reads a
    32-byte block over SYNCIO at boot and XORs it to 0x42 or gives up.
    Unanswered it read as zeros, failed, and the boot stopped with EFile
    up and no GUI.
  - **The power/battery chip on chip-select nCS2.** The driver reads one
    status byte a second from 0x30000000 and splits it into four fields
    (main-battery-low flag, main level, backup level). Unmapped, the read
    took a bus-error abort; mapped with the wrong bits it stops at the
    media drivers. Bits 1-2 are the "power is fine" flags — the ROM's own
    strings are "Replace main batteries" and "Main batteries too low for
    PC Card" — and the value shipped was picked by sweeping all 256.
  - **The keyboard.** Eight columns of twelve keys, selected through
    SYSCON1's KBDSCAN and read back across port A and port B's low
    nibble — wider than the seven-column, seven-row matrix the shared
    CL-PS711x code assumed. The scancode table is the ROM's own, copied
    from 0x5007CD0C, so the emulated keyboard sends exactly what the real
    one does.

  The **mouse pad** works. The Geofox has no touchscreen: it points with a
  capacitive pad in the keyboard deck that reports relative motion the way
  a mouse does, and the ROM's `Exyin.dll` reads it as a PS/2-shaped
  packet — status byte with the two sign bits in it, then X, then Y — over
  two SSI frames, 40 times a second, whenever the pad raises EINT2. The
  emulator answers those frames, so the on-screen arrow moves, taps click,
  double-taps open, and a drag rubber-band-selects. Because the host's
  mouse and touchscreen both report a *position* and the pad reports
  *motion*, the two are bridged by a closed loop: the emulator keeps a
  shadow of the pointer the driver is holding and sends the deltas that
  walk it there, staying below the driver's acceleration threshold so the
  shadow stays exact rather than approximate. A hovering mouse moves the
  pointer, a press clicks where it lands, and a right-click sends the Menu
  key — which is exactly what the real pad's top-right-corner tap sends.

  The **Remote Link** works, over the serial cable on UART1. Getting there
  came down to one wire. The machine ships with Remote Link set to Cable
  at 115200, so its `RemoteLinkServer` opens UART1 by itself about eleven
  seconds into boot — and then, in this emulator, sat there and never
  transmitted a byte, which read from the outside exactly like a machine
  with no link at all. It was waiting on its modem lines. Psion's own
  CL-PS711x boards present CTS/DSR/DCD to `SYSFLG1` active high, and the
  emulator asserted them that way for every machine on the SoC; the
  Geofox, built by a different company, reads them inverted. Told the
  Psion way that a cable had arrived, its link server heard the opposite
  and stayed quiet. With the polarity right it does what the Series 5
  does — answers a cable plug with a `Req_Req_Pdu` burst, takes the host's
  `Req_Con_Pdu`, and goes on to NCP Info. Driven by the real client the
  browser uses, plugging in twenty seconds after the desktop is up
  connects in a tenth of a second and lists the machine's drives —
  `C: D: E: F: G: Z:`. `tests/integration/test-remote-link.sh` gates the
  handshake and the row fails outright if the polarity is put back;
  `_plp_repro.mts geofox` runs the whole stack against the ROM.

  One piece of hardware is still not emulated, and the table above says so
  rather than the emulator pretending otherwise: a **PC Card** attached
  through the shared CompactFlash path does not mount. With a card
  inserted the guest never touches the PC-card window at all, so the
  socket's detect and power are on hardware still to be found. See
  [`core/geofox.h`](core/geofox.h), which records the addresses.
- **Remote Link** (serial cable) is emulated via a host serial bridge + PLP /
  PsiWin client (link → NCP → RFSV drive/dir/file), verified end-to-end on the
  Windermere machines and the SA-1100 Series 7 / netBook / netpad. The one
  EPOC32 machine it does *not* work on is the Conan, whose ROM carries the
  later ER5u connectivity stack — the generation that needed a new PsiWin on
  real hardware — so the emulator does not offer Remote Link there. See
  [`docs/conan-remote-link.md`](docs/conan-remote-link.md).
- **Infrared send** beams a host file into the device's inbox over an emulated
  IrDA stack (SIR → IrLAP → IrLMP/IAS → Tiny TP → EPOC Eikon-IR), verified on all
  EPOC32 machines. The EPOC16 "Psion IRLink" SIBO machines are not yet supported.
- **Printing** — the Printer dialog captures jobs either *via PC* (PsiWin print
  spooler → positioned PDF, EPOC32) or by *serial capture* (`.txt` / Courier PDF
  / raw `.prn`). Run `npm run test:wprt` / `test:printer`.
- **Audio** — SIBO1 (Series 3 / MC400 / Pocket Book) is a piezo buzzer; SIBO2
  (3a onward) adds an 8-bit PCM codec with mic input. The EPOC32 machines
  each carry a codec of their own: an on-chip 8-bit one on CL-PS711x and
  Windermere, a UCB1200 on the SA-1100 Series 7 / netBook, and an AC'97
  part on the netpad's board FPGA. Run `harness/netpad-audio-harness` (or
  `bash tests/integration/test-netpad-audio.sh`) to check the netpad's
  microphone and speaker paths against the register contract its own ROM
  drivers use.
- **SSD packs** (SIBO) — the SSD dialog builds a FEFS Flash pack, fills it
  with host files and inserts it; packs mount read/write, so the Psion can
  save to them and format them. Files the machine writes appear in the
  dialog's listing while the pack is still inserted, and **download** takes
  any of them off the device (the whole pack image can be saved too). See
  [`docs/sibo-ssd-packs.md`](docs/sibo-ssd-packs.md).
- **MC400 ROMs** — the MC400 defaults to the v2.60F boot ROM; a discreet
  header link swaps to the older v1.26F ROM (and back) without leaving a
  second entry in the device picker.
- **MC200** — the MC400's smaller sibling: the same SIBO1 board and the same
  V2.12F boot ROM, with a 640×200 panel in place of the 640×400 one, a
  single-plate framebuffer and 128 KiB of RAM. It cold-boots with its own
  factory **ROM:: System Disk** in Pack D (`roms/MC200_V2.12F_system.ssd`,
  the real pack dumped), so it goes from the *Psion Graphic User Interface*
  splash through the first-run dialogs to the desktop. The ROM image is
  built from the machine's two flash-chip dumps in
  `roms/MC200_V2.12F_ROM_disk/` by `scripts/build-mc200-rom.mts` — see the
  README in that folder.
- **Organiser I** — the 1984 original, and the smallest machine here by
  some distance: a Hitachi HD6301X0 running out of the CPU's own 4 KiB mask
  ROM, 2 KiB of external RAM, and one row of 16 characters on an HD44780.
  It boots to its clock (which ticks), takes the alphabetical keypad, and
  cycles ENTER / OFF / CALC on MODE — in CALC the letter keys type the
  digits printed under them, so `1 + 2 EXECUTE` leaves `CALC:1+2=3` on the
  panel. Cold boot ends with the ROM switching the machine off again,
  exactly as the hardware does; the driver presses ON for you so it comes
  up ready (`core/organiser1.h`).
- **HC120** — not yet emulated. The dump in the tree is one chip of the
  machine's ROM and not the one the CPU starts from, so there is nothing to
  boot; [`docs/hc120-rom.md`](docs/hc120-rom.md) records what the file is,
  what is missing and what the rest of the machine needs.
- **Revo (Conan)** — "Conan" is the Revo's successor, emulated from
  `roms/conan_v0.10(17)_eng.IMG`: the ROM of a real machine, dumped off it
  with `tools/romdump` (TRomHeader version 0.10(17), built 2001-06-20,
  16 MB where the shipping Revo's is 8). The machine names itself on its
  own splash — **Psion Conan © Psion Digital 2001 / EPOC Release 6 ©
  Copyright Symbian LTD 2001**, over a CONAN wordmark with ARM, EPOC and
  Bluetooth badges — so the codename this repository has always used is the
  ROM's own, and the machine is an **EPOC R6** (Symbian OS 6.0) build.
  The earlier engineering image `roms/conan_s2_2201.engbuild.IMG` (EPOC R5,
  version 0.01(22), built 2001-05-12, 12 MB) is kept alongside it as
  `conanv001`, reached by a discreet header link the way the MC400's two
  ROMs are; it paints the *Revo's* splash rather than Conan's own.
  It is the Revo's board: the images' HAL tables name the same parts —
  the `LAP 53` panel, `MLM 650` digitiser, `ARM 710T`, and the `REVO` /
  `REVO-PRO` device types, byte-for-byte the same run in both — and both
  paint their splash and the Revo's desktop layout at 480×160, so Conan
  runs the whole Revo profile and needs no new hardware modelling.
  `core/conan.h` is `Revo::Emulator` under another name, and records what
  was read out of the images to establish that.
  What is new is the software in the extra megabytes: WAP (`WAPSTKSRV.EXE`)
  and Bluetooth (`btmanserver.exe`, `sdp.exe`, `thci.exe`) stacks the
  shipping Revo ROM has neither of — and the case agrees, its lid badged
  **revo Bluetooth**. The machine ROM puts a **Bluetooth on** tab in the
  desktop toolbar and adds an Opera browser besides. Neither stack is
  reachable here: there is no modelled Bluetooth radio, and the WAP stack
  has no bearer.
  Its connectivity is new too, and that one is user-visible: the image has no
  `PlpDL.prt`, the module carrying the PLP data link every other supported
  EPOC ROM speaks, and registers the Unicode link services (`SYS$RFSVU.*`,
  `SYS$RPCSU.*`) rather than `SYS$RFSV.*` / `SYS$RPCS.*`. On the cable it
  talks an escaped, XON/XOFF-safe stream (`ESC` = `0x19`, ENQ `19 23` / ACK
  `19 24`) and answers no PLP frame at all, so Remote Link, the printer
  dialog's *via PC* tab and cable app installs are switched off for this
  machine — exactly the wall PsiWin hit before its 2.3 release.
  [`docs/conan-remote-link.md`](docs/conan-remote-link.md) has the wire
  evidence and what supporting it would take.
  Both images boot to an interactive desktop, and both leave a dialog on it
  that is the image's own doing rather than the emulation's. The machine ROM
  spends its first minute on the splash and then reports "Problem
  initialising Mail / Not found" (`tests/golden/conan.pgm`); the engineering
  build puts EShell and `D_EXC` on the desktop and its own Agenda panics with
  `CONE 14` on every cold boot, leaving the "Program closed" dialog you see
  in `tests/golden/conanv001.pgm` — rerun that one with the host clock back
  in mid-2000 and the frame comes out byte-identical to its golden apart from
  the clock cell. Behind either dialog is a working desktop.
  **Close Lid** in the control bar shuts the case: the machine carries on
  running behind it (as a real one does — EPOC's own "off" is a standby that
  keeps the clock), it just takes the screen, touchscreen and keyboard away
  until you open it again.
  Being a Unicode build also puts this machine out of reach of the EPOC R5
  ROM extractors, which are non-Unicode binaries whose imports its DLLs
  cannot bind — so `tools/romdump` is a ROM dumper written for ER5u/ER6
  instead. It is an ordinary EPOC executable, built here without an SDK
  (`tools/e32`), and it copies the ROM to the machine's own disk in 2 MB
  parts, because a 16 MB ROM does not fit on a Revo-class RAM disk in one
  piece. It asks how many parts to write now, shows their progress, reads
  every one back and compares it with the ROM before moving on, and picks
  up from where it stopped the next time it is opened — so you dump what
  fits, copy those parts off, and run it again for the rest.
  **`roms/conan_v0.10(17)_eng.IMG` is what it produced**, off a real
  machine, in eight parts: the tool is no longer just tested against an
  image, it has delivered one.
  [`tools/romdump/HOW-TO-USE.md`](tools/romdump/HOW-TO-USE.md) is the guide
  for someone holding the machine.
  `bash tests/integration/test-conan-romdump.sh` runs it on the real ROM
  and checks the result;
  [`docs/conan-rom-dumping.md`](docs/conan-rom-dumping.md) records how the
  ER5u file-server ordinals and the E32Image format were established.

Every device has a committed golden screenshot in `tests/golden/`; run
`bash tests/boot/test-boot.sh --all` to re-verify locally.

## Project structure

```
core/       C++ emulation engine (CPU cores, ASICs, device models)
wasm/       Emscripten WASM layer with Embind exports
frontend/   React + TypeScript web app (Vite)
applib/     Source for the in-app software library (see applib/README.md)
scripts/    Build / asset scripts, plus the public-mirror sync
desktop/    Electron shell for the native Windows / macOS apps
harness/    Native (non-WASM) host driver for the core
tools/      Programs that run on the emulated machines (see tools/README.md)
tests/      Boot validation, integration, unit tests (see tests/README.md)
docs/       Per-device investigation notes
roms/       ROM images (property of their owners; see NOTICE)
.github/    GitHub Actions CI/CD
```

## Building

Requires the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
(activated) and Node.js 20+.

```sh
source <path-to-emsdk>/emsdk_env.sh
bash scripts/build-all.sh        # full build -> dist/
bash scripts/build-wasm.sh       # WASM only
bash scripts/build-frontend.sh   # frontend only (needs WASM built)
cd frontend && npm run dev        # local dev server
```

App icons / share images are generated from `favicon.svg` and committed; only
re-run `cd frontend && npm run icons` if the logo changes.

## Desktop apps (Windows / macOS)

An Electron shell in `desktop/` turns the emulator into a native app: a
borderless, resizable window that is nothing but the machine, locked to its
aspect ratio, with a device switcher on `Ctrl/Cmd+K`.

```sh
bash scripts/build-desktop.sh dir     # unpacked app for this platform
bash scripts/build-desktop.sh mac     # .dmg + .zip  (needs a macOS host)
bash scripts/build-desktop.sh win     # NSIS .exe + .zip
cd desktop && npm run dev             # run it framed, with devtools
```

The renderer is the same web app, built by `frontend/vite.desktop.config.ts`
and served over a registered `app://` scheme — not `file://`, which blocks the
Web Worker the emulator runs in. ROMs ship outside the app archive and are
streamed from there.

What the desktop build adds beyond a window:

- **State that saves itself.** A snapshot about once a minute when the machine
  is idle, plus on device switch, blur and quit, kept as a few generations per
  device under the app's data directory in the same `PSIONST1` format the web
  app's Upload button accepts. Earlier sessions are restorable from the
  switcher. IndexedDB is still the live store; disk is a mirror and a history.
- **A shared card folder.** One host folder, projected into whatever removable
  medium the current machine takes — a FAT16 CompactFlash image for the ARM
  machines, an MMC image for the netpad, a FEFS24 SSD pack for the Series 3
  family — and read back when you switch away, so a file dropped in follows you
  between machines. Not available on the Organiser II: per-file projection there
  needs a Datapak filesystem writer that does not exist yet.
- **A folder synced to the machine's own drive**, over the emulated Remote Link
  cable, for the link-capable machines. Measured at 0.81 KB/s on a real 5mx, so
  it suits documents and not large files — see
  [docs/desktop-drive-sync.md](docs/desktop-drive-sync.md), which also records
  two things about real ROMs that a mock filesystem will not tell you.

Tests: `cd frontend && npm run test:desktop` for the pure logic,
`cd desktop && npm test` for the main process and the Electron gates (needs a
display; `xvfb-run` on Linux), and
`node --experimental-strip-types tests/integration/test-drive-sync.mts` to drive
the sync against a real ROM through `harness/run`.

Builds are unsigned for now; signing and notarisation are a later step.

## App library

The site ships a browsable app library (`#/apps`) built from Steve Litchfield's
3-Lib shareware collection, preserved in `applib/`.
`scripts/build-app-library.mts` converts the catalogue into `dist/apps`
(`manifest.json` + one zip per app, with icons extracted from EPOC `.AIF`
resources). CI regenerates it on every deploy; nothing under `dist/` is
committed. Each app can be downloaded or tried in one click — delivered onto the
right device the way the machine actually had it (a Remote Link `.SIS` upload on
EPOC32, or a FEFS SSD pack on the SIBO machines). A bundle that is an already-
installed EPOC32 app folder rather than an installer is copied straight into
`C:\System\Apps\<App>\` over the link instead, along with anything else it
carries for the drive (a shared library in `\System\Libs`, say). Run
`npm run test:applib`, and `bash tests/integration/test-epocdir-install.sh` for
the folder path end-to-end on the 5mx ROM (`--device netpad` for the same
delivery to the netpad).

The library route carries what's on screen, so any view of it can be linked to.
`#/apps?app=<category>/<slug>` opens an app's details popup — the URL the page
puts in the address bar as soon as you open one, which is therefore also the link
to send someone. Opening an app pushes a history entry, so Back closes the popup
and Close undoes its own push; arriving on a shared link, Close falls back to the
plain library.

`#/apps?device=<deviceId>` opens the library filtered to one machine, and
`#/apps?category=<genre>` to one category (the values in the library's own
dropdown, e.g. `Games`); the two combine.

The netpad uses both, though its own software no longer goes through the library
at all. Psion Teklogix shipped that tablet with a bare EPOC R5 ROM and its
applications on a support CD, so the emulator's netpad carries a yellow
**Install standard apps** button that reproduces the CD: one click downloads
every app in `applib/netpad` — Word, Sheet, Agenda, Opera, the OPL editor and the
rest — writes all of their installers onto a single MMC card image and inserts
it, narrating the download in a progress bar as it goes. What's left to do is
what a real netpad owner did with the CD in hand: open the installers from drive
D:. That set is not the limit of what the machine runs, though: the netpad is an
EPOC R5 ARM machine with the 5mx's own 640×240 panel, so the library's whole EPOC
catalogue is listed for it and installs on it one app at a time, at
`#/apps?device=netpad`.

The netpad takes delivery on its MMC card (drive D:) rather than the cable: the
slot mounts as D: with nothing switched on first, where its Remote Link wants
enabling from the device's own Tools menu on real hardware.
`bash tests/integration/test-netpad-app-install.sh` drives that end to end on the
real ROM — deliver, run the device's own installer, and require the app to show
up in Extras. It takes an app id, so any EPOC app works:
`bash tests/integration/test-netpad-app-install.sh epocgames/fred` installs a
3-Lib game onto the netpad the same way.

The build also emits a PWA service worker (`vite-plugin-pwa`): the app shell is
precached, ROMs and skins are runtime-cached, and updates activate on the next
visit.

## Boot-validation harness

`harness/run.cpp` (built via `bash harness/build.sh`) loads any ROM and emulates
it without WebAssembly. `tests/boot/test-boot.sh` turns it into a pass/fail gate
that diffs the end-of-boot LCD against the committed `tests/golden/<id>.pgm`:

```sh
bash harness/build.sh
bash tests/boot/test-boot.sh revo                  # one device
bash tests/boot/test-boot.sh --all                 # everything in tests/devices.txt
bash tests/boot/test-boot.sh --all --update-golden # refresh committed PGMs
```

A device passes when, after its boot window: LCD variance ≥ threshold (blank
screens fail), unique sampled PCs ≥ threshold (tight wait loops fail), and no
aborts / undefined instructions were logged. CI runs the full suite before the
slow WASM build so regressions surface fast. See `tests/README.md`.

## Adding a new device

The cleanest template is the Revo (`core/revo.{h,cpp}`):

1. If the device reuses an existing CPU + peripheral core, subclass it in
   `core/<device>.{h,cpp}` and override only what differs (LCD size, digitiser,
   name).
2. Add the source filename to both `harness/build.sh` and `scripts/build-wasm.sh`.
3. Add a factory and `DeviceProfile` entry in `core/device_registry.cpp`.
4. Drop a skin SVG into `frontend/public/skins/` (and a `SKIN_LAYOUTS` entry in
   `frontend/src/components/EmulatorView.tsx` if the aspect ratio differs).
5. Add a line to `tests/devices.txt` and run
   `bash tests/boot/test-boot.sh <id> --update-golden`; commit the golden PGM.
6. Add the id to the two device lists that live outside the registry:
   `$ALLOWED_DEVICES` in `frontend/public/api/track.php` and the id → name map
   in `frontend/public/device-names.json`. Both fail *silently* when a machine
   is missing — analytics events come back 400 and the client swallows them, so
   the device just never appears on the usage leaderboard — which is why
   `node --experimental-strip-types tests/unit/device-lists-sync.mts` asserts
   both against the registry on every CI run.

A fundamentally different CPU (SA-1100, V30/V30H, HD6303X) needs a new core
first. The existing cores — `core/arm710.*`, `core/sa1100.*`, `core/v30.*`
(+ `v30_ops_*`), `core/hd6303.*` — each expose a small synchronous bus interface
the device driver implements.

## Deployment

Automated via GitHub Actions on push to `main`. Configure the repository secrets
`FTP_HOST`, `FTP_USER`, `FTP_PASS`, `FTP_SERVER_DIR`. For local config, copy
`.env.example` to `.env`.

## Public mirror

[`joehaines/psionEmulators`](https://github.com/joehaines/psionEmulators) is a
public snapshot of this repository. `scripts/sync-public.sh` republishes it on
demand:

```sh
bash scripts/sync-public.sh --dry-run          # what would change
bash scripts/sync-public.sh                    # publish HEAD to main
bash scripts/sync-public.sh -m "Netpad support"  # …with your own commit subject
```

Each run replaces the mirror's whole tree with the tracked files of one commit
here (`--ref` picks another) and commits the difference as a single commit on
`main`, so files deleted here are deleted there. It is a snapshot, not a fork of
this history — private commits, branch names and untracked working files never
cross over, because the content comes from `git archive`. If the mirror already
matches, the script says so and pushes nothing. Anything tracked here that must
never be published goes in the script's `EXCLUDE` list; development-only
material (`reference/`, `.env`, the local API config) is untracked and so can
never reach the archive in the first place.

The mirror clone lives in `.public-sync/` (gitignored) and is reused between
runs, so only the first run pays for the clone.

The same sync also runs from CI, so you don't need a local clone of the mirror
at all: **Actions → Sync Public Mirror → Run workflow**, picking the branch or
tag to publish, with optional inputs for the commit subject and a dry run.
Manual trigger only — a push to `main` deploys the site but never touches the
mirror. The workflow needs one repository secret, `PUBLIC_MIRROR_TOKEN`: a
fine-grained PAT scoped to `joehaines/psionEmulators` with **Contents: read and
write**. `GITHUB_TOKEN` cannot stand in for it, because it only ever grants
access to the repository the workflow runs in.

`.github/workflows/sync-public.yml` is itself in the script's `EXCLUDE` list, so
the mirror never carries the workflow that publishes it.

## License

Copyright (c) 2024–2026 Joe Haines. Original work is released under the terms in
[`LICENSE`](LICENSE) (reuse is permitted with attribution; self-hosted
deployments must link back to https://joehaines.com/psion). Portions of the
emulation core derive from [WindEmu](https://github.com/Treeki/WindEmu)
(MPL-2.0) and MAME (BSD-3-Clause), and ROM images and the bundled software
library belong to their respective owners — see [`NOTICE.md`](NOTICE.md).
