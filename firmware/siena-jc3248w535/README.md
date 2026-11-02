# Psion Siena on a JC3248W535

Firmware that turns a Guition **JC3248W535** (ESP32-S3, 3.5" 320x480
touch panel) into a Psion Siena, for fitting inside a Siena case. It runs
the emulator core from this repo, the same V30H + ASIC9 code the website
uses for the Siena, natively on the ESP32-S3.

- The Siena's 240x160 screen fills the panel, landscape at 2x.
- You type on a **Bluetooth LE keyboard**, or from a terminal on a computer
  over USB.
- Sound (buzzer and codec) plays through the board's speaker connector (P6).

## Build and flash

You need [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/index.html) v5.4 or v6.1 (both build it).
Connect the board's USB-C port, then:

```sh
cd firmware/siena-jc3248w535
idf.py set-target esp32s3        # first time only
idf.py -p PORT flash monitor
```

`PORT` is `/dev/ttyACM0` on Linux, `/dev/cu.usbmodem…` on macOS, or `COMn`
on Windows. `flash` writes the firmware, plus the Siena ROM
(`roms/Siena/vine_v4.20f_eng`) and the app store (below) into partitions of
their own. To boot a different image, add `-DSIENA_ROM=/path/to/rom.bin`
before `flash`. When only the firmware changed, `app-flash` is quicker.

`idf.py menuconfig` → **Psion Siena** has two settings:

- **Locale.** The v4.20f ROM carries English (UK), English (USA), Swedish and
  Spanish.
- **Flip.** Turns the picture 180° if the panel sits the other way up in the
  case.

## Keyboard

The ESP32-S3 radio only speaks **Bluetooth Low Energy**. Most current
multi-device keyboards support it, but some older Bluetooth keyboards only
use Bluetooth "classic" and won't be seen.

1. Put the keyboard into pairing mode. The board looks for one whenever no
   keyboard is connected.
2. If the keyboard asks for a passkey, a six-digit number appears in a box on
   the screen. Type it on the keyboard, then press Enter.
3. The pairing is remembered, so the keyboard reconnects by itself after a
   power cycle. **Hold BOOT for 3 seconds** to forget it and pair another.

| Keyboard                  | Siena                                                    |
|---------------------------|----------------------------------------------------------|
| F1 – F8                   | System, Data, Word, Agenda, Time, World, Calc, Sheet     |
| F9 / F10                  | Menu / Help                                              |
| Alt, Win/Cmd or F11       | Psion; tapped on its own, ◆ (diamond: changes view)      |
| Ctrl tapped on its own    | Menu (held with another key, it's Ctrl)                  |
| Caps Lock                 | ◆ (diamond)                                              |
| Backspace / Delete        | Del / Shift+Del                                          |
| Home / End                | Psion+← / Psion+→                                        |
| Page Up / Page Down       | Psion+↑ / Psion+↓                                        |
| Shift, Ctrl, arrows, Esc, Enter, Tab | the same keys                                 |

Keys are held down for as long as you hold them, so auto-repeat and chords
work. The keyboard is assumed to have a US layout. The Siena has no keys for
characters like `! " @ # $ % & ( ) [ ]`. The firmware types those with the
Siena's own Ctrl + three-digit-code entry, so they still arrive as typed.

## Board shortcuts

The board's own functions sit on Shift and a function key, from the
Bluetooth keyboard or a terminal. Settings are kept on the board.

| Keys       | What                                                              |
|------------|-------------------------------------------------------------------|
| Shift+F1   | Colour theme: Siena, Paper, Backlit, Amber, Green, Night          |
| Shift+F2   | Edge smoothing on/off (Scale2x: rounds jagged diagonals and text) |
| Shift+F3   | The app store                                                      |
| Shift+F4   | Turbo on/off: the Siena runs ahead of real time while it is busy, catching up while idle (never while sound plays) |
| Shift+F5   | The next profile                                                   |
| Shift+F6   | Profiles: Enter switches, N adds a new one, Del (twice) deletes    |
| Shift+F10  | A list of these                                                   |

### Profiles

Each profile is a Siena of its own, for different people or uses: its
files, its open apps, the apps installed from the store. Switching saves
the one you leave and resumes the other where it was; a new profile starts
as a Siena fresh from the box. They live on the microSD card (profile 1 in
`SIENA/`, the others in `SIENA/P2/` and so on, up to 9), so profiles need
the card. Display settings are shared.

### App store

All 67 installable Siena apps from the 3-Lib collection (`applib/siena`)
are flashed with the firmware on one Flash SSD, which sits in the Siena's
slot as drive A:. Shift+F3 lists them; Enter installs the selected one by
typing the System screen's own Install for you (it takes ~15 s: watch it
go). Its icon then appears on the System screen. Apps marked `*` are
installed already; Shift+Enter installs one again (if you removed it on
the Siena).

Apps run from A: and can save there (1.5 MB free); what they save is kept
with the Siena on the microSD card. `tools/build_app_store.py` builds the
pack: the readmes are left off to make room, and each app's files are
laid out the way the website's "Try it" packs do it.

### Typing from a computer

With the board on USB, `idf.py monitor` (or any serial terminal) works as a
keyboard. Characters, Enter, Tab, Backspace and the arrow and function keys
are passed through, with the same mapping as above.

## Notes and limits

- **The Siena is kept on the microSD card**, which stands in for its
  backup battery. A few seconds after you stop typing, the board saves the
  whole machine (its RAM plus the CPU, timers and other chips) to
  `SIENA/RAM_A.BIN` or `RAM_B.BIN`, taking turns between the two so that
  unplugging mid-write can't lose both. At power-on it puts the newest
  back and the Siena carries on exactly where it was: same app, same
  screen, unsaved text included. Give it ~5 s after your last key before
  unplugging.
  - The machine state belongs to the firmware build that saved it. After
    flashing new firmware, only the RAM comes back and the Siena warm-boots,
    as from its reset button: the M: drive and its files are kept, apps
    start afresh. The next save is in the new build's form.
  - The Siena's clock stands still while the board is off (see Clock).
  - The card must be FAT32 (cards over 32 GB usually come as exFAT); the
    board never formats it. Without a usable card the Siena cold-boots with
    an empty M: drive, as before.
  - To start from scratch, delete the `SIENA` folder on the card.
  - A save only comes back into the same ROM and language.
- **Clock.** The ESP32 has no battery-backed clock. On a fresh start the
  Siena's clock starts from 1 January; after a resume it carries on from
  when the board was switched off. Set the time in the Time app.
- **Touch** is not used.
- **Speed.** The emulator paces itself to real time and logs its speed over
  USB every 10 seconds (`speed NN% of a real Siena`). If it can't keep up,
  the Siena runs slow rather than skipping.

## Layout

| Path | What |
|---|---|
| `main/app_main.cpp` | Tasks: emulator on core 1, display / keyboard / USB on core 0 |
| `main/siena_machine.*` | The Siena profile of `Series3c::Emulator`, LCD readout |
| `main/screen.*` | Siena LCD → panel rows (rotation, 2x, passkey box) |
| `main/lcd_axs15231b.*` | AXS15231B QSPI panel driver (whole frames only) |
| `main/ble_keyboard.*` | BLE HID host: scan, pair, bond, reconnect |
| `main/hid_keymap.*`, `main/terminal_keys.*`, `main/key_player.*` | Keyboard → Siena key matrix |
| `main/audio_out.*` | I2S speaker output |
| `main/ram_store.*` | The Siena (RAM and machine state) on the microSD card |
| `host/` | Builds the portable half on a PC: boots the Siena, types through both keyboard paths, renders the panel |

The panel's set-up sequence and pin map come from the board vendor's demo,
via [NorthernMan54/JC3248W535EN](https://github.com/NorthernMan54/JC3248W535EN).

### Checking on a PC

```sh
bash host/build.sh
host/host_sim ../../roms/Siena/vine_v4.20f_eng/vine_v4.20f_eng.bin /tmp
```

This writes the screen as the panel shows it at each step: `boot.ppm`
after boot, `word.ppm` after typing into Word, `menu.ppm` after a lone Ctrl
tap, `agenda.ppm` after a lone Alt tap (Week view), `passkey.ppm` with a
pairing passkey up, and `restored.ppm` with Word reopened on a fresh
machine whose RAM was carried over, as after a reflash.

```sh
bash host/resume_check.sh ../../roms/Siena/vine_v4.20f_eng/vine_v4.20f_eng.bin
```

checks that a saved machine resumes exactly: a session saved part-way and
resumed in a fresh process must end identical to one run straight through.
