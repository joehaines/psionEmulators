# ROM audit: language variants, unexposed boot parameters, Easter eggs

An audit of every image in `roms/` (September 2026) for three things the
emulator might be hiding from its users:

1. **Language variants**: a ROM that carries more than one locale and picks
   one at boot.
2. **Unexposed boot parameters**: anything the ROM reads at boot (a PROM
   field, a strap pin, a file on a card, a serial handshake) that changes what
   comes up, and that the emulator does not let the user set.
3. **Easter eggs**: hidden games, cheats, test modes and credits.

The Geofox One and the Siena already have a language picker (see
`core/geofox.h` and the locale section of `core/series3c.cpp`). This audit
covers everything else.

## Summary

A follow-up change acted on most of this. The **Now** column says where
each item stands after it.

| Machine / ROM | What's there | Now |
|---|---|---|
| **Conan v0.10(17)** | 7 locales: English ×2, **French, German, Spanish, Italian, Dutch** | **Language picker** (edits the ROM directory; see below) |
| **MC218 v1.05(259)** | 5 locales (UK, Sweden, USA, +2 export) × 7 keyboards | **Language picker** (Etna PROM byte 2) |
| **5mx Pro v1.05(319)** | Same 5 locales × 7 keyboards as MC218 | **Language picker** (Etna PROM byte 2) |
| **Revo v1.06(390)** | 3 locales (UK, Scandinavian, USA) × 3 keyboards | **Language picker** (edits the ROM directory) |
| **5mx v1.05(260)** | 2 locales (UK, Scandinavian) × 2 keyboards | **Language picker** (Etna PROM byte 2) |
| **Series 5 v1.01(144)** | 2 locales × 3 keyboards | **Language picker** (its PROM is now wired) |
| **Series 3a v3.22f** | 2 locales: UK (44), USA (1) | Not a registered device; unchanged |
| **Workabout / Workabout MX** | 4 keyboard tables (2 alpha, 2 numeric) | Unchanged: needs a numeric key map |
| All ER1/ER5 machines | `ETest.exe` CF-card overrides: `D:\ewsrv.exe`, `D:\final.exe` | Documented; no payload in the tree to boot |
| All ER5 machines | `ELOCL0` / `EKDATA0` locale override DLLs | Documented (the Revo picker uses `ELOCL0`) |
| netBook | ESHELL and **Quartz** OS images | **Boot ESHELL / Boot Quartz** buttons |
| netBook bootloader | `D:\LOADER.MBM` splash override; factory test suite in ROM | Listed on the Easter eggs page |
| Revo, Conan ×2 | Built-in self test (BIST) | Listed; not reachable yet |
| **Series 3c / 3mx** | **Hidden credits played to "Jerusalem"** | **Hint + Easter eggs page** (verified) |
| 11 EPOC ROMs | **Bombs "Cheat mode"** | **Hint + Easter eggs page** (verified on the 5mx) |
| 5mx Pro | Bootloader credits (`about`) | Hint + Easter eggs page (already known) |
| Workabout, WA MX, HC120 | Factory test programs in the ROM drive | Listed on the Easter eggs page |

The rest of this note gives the evidence for each row. All addresses are
virtual (ROM base `0x50000000`) unless marked as file offsets.

---

## 1. Language variants

### How an EPOC machine picks its locale

Every ER1/ER5 image here uses the same chain, which the Geofox work first
decoded (`core/geofox.h`):

1. The variant reads a factory settings PROM into a buffer whose pointer
   is at `0x4000003C`, and validates it (XOR of the bytes must be `0x42`).
2. `LanguageIndex()` and `KeyboardIndex()` return fields from that PROM. A
   failed checksum makes both return 0.
3. EKern stores them in `TMachineInfoV1` at `+0xE0` / `+0xE4`, the pair of
   `str r0,[r4,#0xe0]` / `str r0,[r4,#0xe4]` stores found in every kernel:
   5mx `0x5000DD44`, MC218 `0x5000DD44`, 5mx Pro `0x5000FD44`, Revo
   `0x50010BD0`, Series 5 `0x50016004`, Osaris `0x50013720`, Series 7
   `0x5000F720`, netBook `0x500117AC`, netpad `0x50012844`.
4. The window server first tries to load **`ELOCL0`** (and **`EKDATA0`**)
   from the file system. Only if that fails does it load `ELOCL<n>` /
   `EKDATA<n>` for n in 1..7, or the unsuffixed DLL otherwise (MC218
   `EwSrv` at `0x50160180` and `0x5014D230`).

**Where the fields live differs from the Geofox.** On the Geofox they are
16-bit words at PROM bytes 4–5 and 6–7. On the Psion-built machines both
indices are **3-bit fields in the first 32-bit word of the PROM**:

```
language = (word0 >> 18) & 7      ; PROM byte 2, bits 2..4
keyboard = (word0 >> 21) & 7      ; PROM byte 2, bits 5..7
```

This holds for the Series 5 (`VArmP2.dll` `0x5007E5DC` / `0x5007E5F4`) and
for the Windermere variant object on the 5mx, 5mx Pro and MC218 (vtable
slots `+0x80` / `+0x84`, e.g. MC218 `0x50087528` / `0x50087540`). The
Windermere check covers all 128 bytes; the Series 5 check covers 32.

**Verified in the emulator.** With byte 2 programmed (checksum re-folded),
`LanguageIndex()` / `KeyboardIndex()` called through the harness REPL
return the programmed values (5mx 1/1, 5mx Pro 4/6, MC218 3/3), and after
boot the chosen `ELocl<n>`'s `TLocale` record is the one live in RAM.
`tests/integration/test-epoc-language.sh` checks exactly that. (A RAM
string such as `EKDATA2.DLL` is *not* evidence on its own: the ROM's own
file names end up in RAM directory caches.) Writing bytes 4/6, the Geofox
offsets, has no effect on these machines.

**The Series 5 reads its PROM sequentially.** `VArmP2.dll` sends one READ
command for word 0 (a 10-bit frame: one padding zero, start bit, opcode
`10`, address 0) and then clocks out all 16 words without reselecting, the
93Cxx sequential-read mode. The Windermere machines reselect for every
word. Etna now continues to the next word after 16 data bits, and port B
bits 0/1 on the CL-PS7110 now drive its select and clock.

**The Revo board is different.** The Revo's variant reads the language
and keyboard index from the **low nibbles of PROM bytes `0x3A` / `0x3B`**
(`0x500804B8` / `0x500804CC`). It clocks the PROM with port B bits 0/1 but
moves the data over Windermere port E (bit 2 out, bit 3 in, `0x500801F8` /
`0x50080260`), which the emulator doesn't route, so the PROM stays
unread, as `core/revo.h` says. Routing it was tried: a *valid* PROM
changes other board settings too, and the Revo came up to a blank
screen. So the picker leaves the Revo's PROM alone. Its window server
tries `ELOCL0` before anything else, and with index 0 loads no locale DLL
at all, so the emulator renames the ROM directory's `ELocl1.dll` entry
to `ELocl0.dll` in place (same length; these directories are searched
linearly) and points it at the chosen DLL. The `Ekdata.dll` entry the
kernel loads at boot is pointed at the matching keyboard table.

**The Conan doesn't use an index at all.** Its window server asks the HAL
for attribute `0x44` (`ELanguageIndex`), formats `ELOCL.%02d`, and falls
back to `ELOCL.LOC` (`EwSrv.exe`, `0x502A39A8`). No `ELOCL.nn` is in the
ROM, so it always boots the UK `ELocl.loc`, and the French, German,
Spanish, Italian and Dutch DLLs are never loaded. A localised Conan build
would ship its locale as `ELOCL.LOC`, so the picker points the
`ELocl.loc` directory entry at the chosen `ELocl<n>.dll`. Verified: a
French boot has "Janvier" in RAM and no "January".

### What each variant is

Decoded from each `ELocl<n>.dll`'s `TLocale` block (language, country code,
UTC offset, date and time format, currency):

| DLL | 5mx | 5mx Pro / MC218 | Revo | Series 5 v1.01 |
|---|---|---|---|---|
| `ELocl` (0) | English, UK 44, £ | English, UK 44, £ | English, UK 44, £ | English, UK 44, £ |
| `ELocl1` | "Scandinavian English": country 44, **kr**, 24 h, `.` date sep | 5mx Pro: as 5mx. **MC218: Sweden 46**, kr, 24 h | as 5mx | as 5mx |
| `ELocl2` | — | **American**, USA 1, $, mm/dd, GMT−6 | American, USA 1, $ | — |
| `ELocl3` | — | UK formats, country **351** (Portugal) | — | — |
| `ELocl4` | — | UK formats, country **36** (Hungary) | — | — |
| `Ekdata<n>` | 0–1 | 0–6 | 0–2 | 0–2 |

`ELocl3` and `ELocl4` differ from the UK DLL *only* in the country code:
English-UK builds tagged for export markets.

The keyboard tables, identified from their shifted digit rows and letter
layout: `Ekdata` is UK, `Ekdata1` Nordic (`æøåäö`), `Ekdata2` **US**
(`)!@#$…`, on both MC218 and Revo), `Ekdata3` QWERTY with `ç` and `« »`
(Portuguese, matching `ELocl3`), `Ekdata4` a second Nordic layout,
`Ekdata5` **QWERTZ** and `Ekdata6` **French AZERTY**. So the locale and
keyboard indices only pair one-to-one for 0–2. The picker pairs locale *n*
with keyboard *n* for UK, Scandinavian and US, and keeps the UK keyboard
for the two export-tagged UK locales rather than guess a table.

### Conan v0.10(17): the multilingual one

`Z:\System\Libs` holds `ELocl.dll` … `ELocl6.dll` plus `ELocl.loc`, with
day names in the DLLs themselves:

| DLL | Language | Country | First weekday |
|---|---|---|---|
| `ELocl` / `ELocl.loc` | English | UK 44 | Monday |
| `ELocl1` | English | UK 44 | Monday |
| `ELocl2` | French | France 33 | Lundi |
| `ELocl3` | German | Germany 49 | Montag |
| `ELocl4` | Spanish | Spain 34 | Lunes |
| `ELocl5` | Italian | Italy 39 | Lunedì |
| `ELocl6` | Dutch | Netherlands 31 | Maandag |

Only one `Ekdata.dll`, and the application resources are English-only, so
switching gives localised dates, days, months and number formats, not a
translated UI. How the Conan chooses (it doesn't: `ELOCL.LOC` every
time) and how the picker gets round that is under "How an EPOC machine
picks its locale" above. Its BIST (`bistdll.dll`, section 3) has a menu
that writes a 0–15 language and keyboard index into the E2PROM (not
traced further: whatever it feeds, with no `ELOCL.nn` file in the ROM it
can't change the boot locale on its own).
`conan_s2_2201.engbuild.IMG` has a single English locale.

### SIBO: locale blocks and straps

Each SIBO2 image ends with eight segment words at `0xFFFE0` indexing its
locale blocks. The kernel picks one from strap pins; an entry of `0xFFFF`
sends it looking for a `SYS$CTRY` file instead.

| ROM | Blocks | Strap read |
|---|---|---|
| Siena v4.20f | UK 44, USA 1, Sweden 46, Spain 34 | port C bits 1/0/5 (already exposed) |
| **Series 3a v3.22f** | **UK 44, USA 1** | index = port 0x21 bit 7 → bit 0; port C bits 0–1 → bits 1–2 (`A000:4297`) |
| Series 3a v3.40f, 3c v5.20f, 3mx v6.16f, Pocket Book II | 1 (UK) | — |
| **Workabout v2.40f / MX v7.20f** | 4 blocks, all country 44, **different keyboard tables** | port 0x21 bit 1 → index bit 0, bit 7 → index bit 1 (`A000:2855`) |
| Series 3 v1.91f, Pocket Book, HC120, MC200/400 | none (all `0xFFFF`) | falls back to `SYS$CTRY` |

- **Series 3a v3.22f** is only used by `scripts/build-mame-3a.sh`; the
  registered Series 3a device boots v3.40f, which has one block. Wiring it
  would be the Siena's port C reader plus port 0x21 bit 7.
- **Workabout**: blocks 0 and 2 are alphabetic keypads, blocks 1 and 3 are
  numeric keypads with function-key rows (the Workabout shipped as both
  alpha and numeric models). Byte `+0x20` of each block is its index.
  Port 0x21 is the high byte of the key-row read, so rows 9 and 15 would
  have to be strapped. The emulator never asserts them, so it is always
  the alpha model. Exposing the numeric model needs a matching frontend
  key map too.
- The MC400 V2.60F ROM disk carries `SYS$CTRY.CFO`, the file form of the
  same thing.

---

## 2. Unexposed boot parameters

### `ETest.exe`: the ER1/ER5 boot coordinator (all Psion ER1/ER5 ROMs)

The 764-byte `ETest.exe` (5mx `0x5006AAC4`) does, in order:

1. Start **`D:\ewsrv.exe`** from the CF card. If that works, start
   **`D:\final.exe`** (the factory final-test hook) and stop.
2. Otherwise start `Z:\System\Libs\elink.exe` (present only on prototype
   Series 5 ROMs; `test-er1-romdump.sh` already uses this).
3. Otherwise start `Z:\System\Libs\ewsrv.exe`, and if that fails, fall
   back to **`eshell.exe`**, the text console.

So a CF card carrying `ewsrv.exe` in its root replaces the whole GUI at
boot. That's a legitimate way to boot a replacement shell or test program
without patching the ROM.

### Locale override DLLs (all ER5 ROMs)

As described in section 1, the window server tries `ELOCL0` and `EKDATA0`
before the built-in ones. Installing a renamed copy of any `ELocl<n>.dll`
as `C:\System\Libs\ELOCL0.DLL` switches locale with no PROM change.

### Revo: serial factory download

The Revo's `ETest.exe` is 5,436 bytes. Before starting the GUI it opens
`EUART2`, sends `'A'`, and waits 500 ms (`0x7A120` µs) for a factory PC.
If one answers, it receives files into `C:\System\` / `C:\System\Libs\`
(`0x50067374`). It then starts `ewsrv.exe` followed by `bist.exe`.

### netBook bootloader (`netBook_BL_v011_eng.bin`)

The 2 MB bootloader is itself a small EPOC ROM (37 files):
- Its `eshell.exe` ("Bootloader v2") loads **`D:\LOADER.MBM`** in
  preference to the built-in `Z:\LOADER.MBM`, so a card can replace the
  boot splash, before reading `D:\OS.IMG`.
- `Z:\Test\` holds a factory suite:
  - `T_CALIB` (touch calibration)
  - `T_SERIAL` (loopback, handshake and xon/xoff tests)
  - `T_HAL` (LED test, "Flashy flashy")
  - `T_INF` (prints `LanguageIndex` / `KeyboardIndex`, backlight and
    click settings)
  - `T_HW` ("A - Access Asic14" register poker)
  - `T_pccdsr` (CF sector stress test)

### Series 5 PROM

The Series 5's `VArmP2.dll` bit-bangs 16 words from a serial PROM
(`0x5007E45C`) into the block at `0x40000FE0`. The emulator reads it back
as all zeros, so the checksum fails and every setting in it (language,
keyboard, and the touch/other fields at bytes 3–6) falls back to defaults.
`docs/epoc-machine-id.md` already notes the PROM isn't wired on the
CL-PS7110.

### 5mx Pro bootloader

`core/windermere.cpp` already records that bit 14 of `[stash+0x8D4]`,
derived from the EEPROM, chooses between the cold-boot animation plus the
YModem wait and an immediate CF boot. It's modelled as always-clear.

---

## 3. Easter eggs, hidden programs and test modes

### Bombs "Cheat mode" (11 ROMs)

`Z:\System\Apps\Bombs\Bombs.app` ships on:
- Series 5 v1.00 and v1.01
- 5mx, 5mx Pro
- MC218
- Geofox
- Osaris
- Series 7 (both builds)
- netBook (both builds)

Decompiled with [opolua](https://github.com/inseven/opolua)'s
`dumpopo.lua --decompile`, the `PROCESS` procedure reads:

```
ELSEIF (KEYCODE& >= 10000) AND (KEYCODE& <= 10004)
    CHEATEVENT& = ((CHEATEVENT& AND 65535) * 16) + (KEYCODE& - 10000)
    IF (CHEATEVENT& = 274960) AND (CHEAT% = 0)
        CHEATON:                      REM gIPRINT "Cheat mode on"
```

Key codes 10000–10004 are the **silkscreen sidebar icons**
(`KKeySidebarMenu%=10000` in the ROM's own `Const.oph`; then Clipboard,
Infrared, Zoom in, Zoom out). The last five are packed into a hex number,
and `0x43210` (274960) is the icons in reverse order. So:

> **In Bombs, tap the sidebar icons from the bottom up: Zoom out, Zoom in,
> Infrared, Clipboard, Menu.** "Cheat mode on" appears.

With the cheat on, after you step on a bomb, a tap on the clock (within
the game's first hour) covers the bombs again and play carries on
(`PROCESSTIMEWIN`: `SHOWBOMB:(0)`, `MODE% = 1`). It resets with every new
game. **Verified on the emulated 5mx.** `Bombs.rsc` puts the strings
`Series 5mx` (`Series 5` or `Psion` in other builds) and `5000` right
before "Cheat mode on", which looked like a typed cheat code, but neither
is part of it: the first is the default name in the best-times table and
the code never reads the second.

The cheat can be entered on the Series 5, 5mx, 5mx Pro and MC218. The
Geofox, Osaris, Series 7 and netBook carry the same code, but their
silkscreens have no Infrared icon (or no sidebar), so the sequence can't
be entered.

### Built-in self test (Revo, Conan)

- **Revo**: `bist.exe`, `bistdrv.dll`, `bistdrv.ldd`. Manual and automatic
  modes, "Select Test Mode", E2PROM and comms tests, and a
  "Error Loading elocl?.dll" path that enumerates `ELocl*.dll`.
- **Conan v0.10**: `bist.exe` ("BUILT IN SELF TEST V%X.%03X"), with
  `bistdll.dll` loaded from `D:\`, `C:\` or `Z:\` in that order. This is
  another card-override hook. The DLL includes:
  - a 0–15 language picker naming the languages
    ("… PORTUGUESE, TURKISH, … INTER. FRENCH, CZECK …")
  - "Keyboard index"
  - RAM and ROM size and version reports
  - a Bluetooth loopback test whose payload is
    *"Symbian EPOC Bluetooth to Bluetooth test 20/07/99 Hello World!!"*
    and *"the quick brown fox jumped over the lazy dog"*.
  `EStart.exe` references `Z:\System\Libs\BIST.EXE`.
- **Conan S2 engineering build**: the same BIST, plus `EMonitor.dll`,
  `D_EXC.exe`, `EShell.exe` and `Z:\System\Programs\gdbstub.exe` (a GDB
  remote-debug stub).

### Hidden programs in SIBO ROM drives

These are present in `ROM::` but have no System-screen icon:

- **Workabout**: `LCDTEST.IMG`, `BEEPTEST.IMG`, `SYS$SOAK.IMG` (soak
  test), `SYS$ALOK.IMG`, `SYS$GSYS.IMG`.
- **Workabout MX**: `LCDTEST.IMG`, `SYS$SOAK.IMG`, `IRSNIFF.APP`,
  `IRDEM.APP`, `FTPDEM.APP`, `DEMMAN.APP`, `IRPRINT.APP`, and
  `SYS$SLOG.IMG` ("Psion TCP/IP sockets log dumper").
- **HC120**: `TTEST.IMG` (serial terminal tester: Baud, Echo, Hand, Parity,
  Word_size), `BATCHK.IMG`.
- **Pocket Book II**: `PLOTTER.APP` (a function plotter; also an app).
- **Series 3a v3.40f / 3c / 3mx**: `PATIENCE.APP` (card game), not in the
  older 3a v3.22f.

### Credits

**Series 3c / 3mx: "Jerusalem" and the rogues' gallery.** On the System
screen press Psion+V (Info › About), then type `!Mrs T Bogan!`. The buzzer
plays "Jerusalem" (about twelve seconds), then a credits table appears
with the columns *Accused*, *Main Charges* and *Other Offences*
("ROM builds, Duty pedant", "Thought policing", …). The phrase and the
tune's frequency table are stored with every byte shifted by +3 (3c file
offset `0xABE50`, 3mx `0xABD20`) and the credits by +4, which is why a
plain string search misses them. The trigger was known publicly for the
3c and 3mx; **verified on the emulated 3c.** A shift-and-XOR scan of every
other image for the same trick found nothing.

**5mx Pro bootloader.** Typing `about` at the bootloader lists its authors
(the matcher is at `0x10C4C` in `5mxPRO_BL_v1.09_ger.bin`, with the word
stored in plain text beside it). The emulator already hinted at this one.

Otherwise the only team sign-offs are the ends of the welcome documents
("The Psion Series 5 Team", "The Osaris Team").

---

## Follow-ups

Done since the audit: language pickers for every multi-locale EPOC ROM
(Series 5, 5mx, 5mx Pro, MC218, Revo, Conan); the Series 5's settings PROM
wired, including sequential reads; the Bombs and Series 3c/3mx eggs
decoded and verified; per-device hints and an Easter eggs page; and a
Boot Quartz button beside Boot ESHELL on the netBook. Still open:

1. **Workabout numeric model**: strap port 0x21 rows 9/15 and add the
   numeric keypad key map.
2. **Series 3a v3.22f** as an alternative 3a ROM, which would bring its USA
   locale (port 0x21 bit 7 and port C bits 0–1).
3. **Revo PROM**: route port E bits 2/3 *and* build a Revo-layout image
   that keeps the board's other settings working. That would replace the
   directory edit and make the Revo's machine ID settable.
4. **BIST** on the Revo and Conan: find the flag that makes `bist.exe`
   stay up rather than exit.
5. **Hidden SIBO programs**: confirm a way to start `LCDTEST`, `SYS$SOAK`
   and the rest from the running machine.

## How this was done

- EPOC file systems were listed with a small walker handling ER1 (8-bit
  names, direct root), ER5 (8-bit names, root-dir list) and ER5u (UTF-16),
  including images with a 0x100 wrapper or a bootloader in front. The
  Series 7 image's header copy sits at file offset `0xBFFF00`, but it maps
  from offset 0.
- ARM code was read with Capstone, and x86 (SIBO) code with Capstone's
  16-bit mode.
- Runtime facts were checked on the native harness (`harness/build.sh`)
  with `--fork-repl` `peek` / `call`, `--save-ram-snapshot`,
  `--press-key` and `--tap-seq`. The first PROM pokes used a local-only
  environment hook in `core/etna.cpp`; the pickers replaced it.
- OPL was decompiled with opolua's `dumpopo.lua`.
