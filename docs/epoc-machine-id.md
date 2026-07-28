# The EPOC machine ID (Machine information → Unique id)

The System screen's **Information → Machine…** dialog shows a 64-bit
`Unique id` in four hex groups. It is `TMachineInfoV1::iMachineUniqueId`,
which the kernel takes from the Superpage; the variant fills it from the
machine's identity chip at boot. EPOC software that licenses itself to one
machine keys off it.

The two halves come from different places. The **low half** is the identity
chip; the **high half** is the model's own UID, a constant compiled into the
ROM image:

| Device | Unique id with the chip word set to `CAFEBABE` (stock ROM half) |
|---|---|
| Series 5mx | `1000-118A-CAFE-BABE` |
| MC218 | `1000-118A-CAFE-BABE` (ROM reports the same model UID) |
| 5mx Pro | `1000-118A-CAFE-BABE` |
| Series 7 | `0908-0001-CAFE-BABE` |
| netBook | `0908-0001-CAFE-BABE` (same model UID as the Series 7) |

`EmuBase::{has,get,set}MachineId` exposes the identity-chip half.
`getMachineIdPrefix()` reports the ROM-side half (0 when that machine's dialog
hasn't been read — now only the netpad, whose System screen cooperates poorly
with synthetic key delivery), and `setMachineIdPrefix()` patches it **where
the constant can be found in the image the machine actually boots** — which on
two machines is not the ROM at all, see below. `canSetMachineIdPrefix()`
reports that up front.

The frontend's "Show debugging" → **Unique id** panel and the harness's
`--machine-id HEX` (up to 16 digits, EPOC's dashes accepted) both drive it.
The value is written before the first instruction runs, and a running EPOC has
already cached the old one, so the machine must be reset for the OS to report a
change. Note the high half is the machine's model UID, not just a display
field: patching it means anything else that identifies the machine sees the new
value too.

| Machine | chip half | ROM half |
|---|---|---|
| 5mx, MC218 | settable | **settable** — one copy of the constant per ROM |
| Series 7 | settable | **settable** — ten copies, all rewritten together (see below) |
| 5mx Pro | settable | **settable** — one copy, on the CF card carrying `SYS$ROM.BIN` |
| netBook | settable | **settable** — ten copies, on the CF card carrying `D:\OS.IMG` |
| netpad | settable | fixed, and not shown: its model UID has never been read out of its own dialog, so there's no value to search the image for |

A 5mx, 5mx Pro, Series 7 or netBook asked for `0BADF00D-DEADBEEF` reports
exactly that.

## Where the word lives

**Series 5mx / 5mx Pro / MC218** — the ETNA identity PROM (`core/etna.cpp`),
128 bytes, little-endian at offset `0x18`. The kernel sweeps all 64 words at
boot and XORs the image against `0x42`; if that check fails it discards the
PROM entirely and falls back to built-in defaults, so the ID reads as zero
*and* the PROM's device-name field (shown as `Type`) is ignored. Any write
into the image must therefore re-fold the checksum byte at `0x7F`.

**Series 7 / netBook / netpad** — the Eiger serial EEPROM (`core/sa1100.cpp`),
little-endian at offset `0x18`. `initEepromImage`'s defaults give that word
other meanings as well (byte `0x18` doubles as the panel orientation, bits
31-30 as a machine-type selector), so an arbitrary id overwrites them. Measured
rather than assumed: a Series 7 and a netpad both cold-boot cleanly with the
word set to `DEADBEEF` — the netpad's boot-check variance is byte-identical to
its golden run and its screen orientation still reports 0 — so the whole word
is written verbatim.

### The ROM half

`getMachineIdPrefix()` / `setMachineIdPrefix()`. The constant is a plain 32-bit
literal, so each device records **every** word-aligned copy at load
(`EmuBase::findRomWords`) and rewrites them as a set. Recording the offsets
rather than re-scanning for the current value matters: a user-supplied UID can
be a byte pattern that occurs all over the ROM (`00000000` would match
thousands of words), and a re-scan would then splatter it.

Copies per ROM: one on the 5mx family (a literal pool in the variant code);
**ten** on the Series 7, seven of them in what looks like a repeated
module-header field. All ten have to go together — patching all of them boots
cleanly and changes the id, while patching a subset black-screens the machine,
which is why they are never touched individually.

#### …when the ROM isn't where the OS is

The 5mx Pro and the netBook have no OS in their own flash. They boot a
bootloader — 128 KB on the 5mx Pro, the 2 MB YModem loader on the netBook —
which reads the real image off a CompactFlash card (`SYS$ROM.BIN`, `D:\OS.IMG`)
and runs it from DRAM. So `ROM[]` never holds the constant that decides what
EPOC prints, and patching it changed nothing: the first eight digits snapped
straight back to the ROM value. Worse on the netBook, whose bootloader flash
carries eight copies of its own — offsets the OS overwrites wholesale at the
handoff, so patching them afterwards would have scribbled the id into eight
arbitrary words of the running OS.

Both machines therefore track the constant **on the card**
(`findCardMachineIdWords`, called from `attachCard`): the copies are located
and the programmed value stamped on before the guest reads a single sector, so
it makes no difference whether the netBook takes its faithful CF boot (the
bootloader's own medata/ATA driver reads the card) or the synthetic
`netBookLoadOsFromCard` handoff. Both report settable with an empty slot,
because the frontend ejects the OS card on every reset — the id is normally
programmed with nothing in the bay and applied to whichever card goes in next.

The card scan matches two values, the factory constant *and* whatever is
programmed now, because a card patched in an earlier session no longer carries
the factory one. That is the one place a re-scan is unavoidable; everywhere
else the recorded offsets are reused, so a subsequent id can't splatter.

The netBook's OS image carries the Series 7's model UID, `09080001`, the same
ten times in the same module-header field — confirmed end to end, not assumed:
a netBook booted with `--machine-id 0BADF00D-DEADBEEF` reports
`0BAD-F00D-DEAD-BEEF` in its own Machine information dialog, and the assembled
64-bit word appears nine times in bank 0 with no copy of the old value left.
The netpad's EPOC R5 image has **eleven** copies, one more than that field
accounts for, and its dialog has never been read to say which value is the
model UID — so it alone still reports 0 (unknown).

**Revo** — not available. Its ROM never drives the ETNA PROM lines: with the
whole image filled with a positional pattern, `Type` still reads the ROM's own
`REVO` string and the ID stays zero. The real machine is CL-PS711x-family and
reads its identity chip over pins this pairing doesn't route.
`Revo::Emulator::hasMachineId()` returns false so the UI hides the control.

**Series 5 / Osaris** — not available either: an ETNA-shaped register block
with no PROM bit-bang wiring (`core/clps7111.cpp` has the calls commented out).

## The PROM serial protocol (and how it was silently broken)

ECust bit-bangs a 93Cxx-style serial read over Windermere's port B. Per word:
port B bit 0 high starts the transfer, then each command bit is written into
ETNA register `0x0C` bit 2 and clocked in by pulsing port B bit 1; the 16 data
bits are clocked out the same way and read back from register `0x0C` bit 3.

The command frame is **start bit 1, READ opcode `10`, 6-bit word index** — but
the ROMs disagree on the idle padding in front of the start bit:

| ROM | pulses per word |
|---|---|
| 5mx v1.05(260) | 25 (no padding) |
| MC218 v1.05(259) | 26 (one leading zero) |

The original decoder counted a fixed 10-bit address. On the 5mx that ate the
first *data* clock, so the index came back as `(index << 1 | index & 1)` and
every word was shifted — the checksum failed and EPOC threw the PROM away.
That is why `Unique id` read `0000-0000-0000-0000`. Fixing it at 9 bits
instead would have broken the MC218 the same way, so `Etna::setPromBit1High`
ignores clocks until the start bit arrives, exactly as the real chip does.

## Verifying

```sh
# Boot, open Information → Machine…, screenshot the dialog.
harness/run "roms/5mx_v1.05(260)_eng.bin" --device 5mx --boot-seconds 46 \
  --machine-id CAFEBABE \
  --press-key 35 148 --press-key 36 15 --press-key 37 15 \
  --press-key 38 15 --press-key 39 15 --press-key 40 17 --press-key 41 3 \
  --screenshot /tmp/id.pgm
node scripts/pgm2png.mjs /tmp/id.pgm      # Unique id: 1000-118A-CAFE-BABE

# The two card-booted machines. Both take the whole 16 digits; the card is
# what gets patched, so it has to be attached (the boot suite synthesises
# tests/cards/5mxpro-osboot.img on first use).
harness/run "roms/5mxPRO_BL_v1.09_ger.bin" --device 5mxpro --boot-seconds 2 \
  --card-path tests/cards/5mxpro-osboot.img --post-attach-seconds 95 \
  --machine-id 0BADF00D-DEADBEEF \
  --press-key 80 148 --press-key 82 15 --press-key 83 15 --press-key 84 15 \
  --press-key 85 15 --press-key 86 17 --press-key 88 3 \
  --screenshot /tmp/id.pgm                # Unique id: 0BAD-F00D-DEAD-BEEF

harness/run "roms/netBook_BL_v011_eng.bin" --device netbook --boot-seconds 3 \
  --card-path roms/netbook_os.img --post-attach-seconds 115 \
  --machine-id 0BADF00D-DEADBEEF \
  --press-key 96 148 --press-key 98 15 --press-key 99 15 --press-key 100 15 \
  --press-key 101 15 --press-key 102 17 --press-key 104 3 \
  --screenshot /tmp/id.pgm                # Unique id: 0BAD-F00D-DEAD-BEEF

tests/unit/machine_id_test                # protocol + checksum, no ROM needed
```

`tests/unit/machine_id_test` drives the PROM through the guest-side bit-bang
only — both frame paddings, the full 64-word sweep, and the checksum — so a
regression in the decoder fails there rather than silently reverting the
machine ID to zero. It also covers the OS-card scan above: a fresh card, one
already patched in a previous session, and a half-patched one all have to
resolve to the same set of offsets.

On the SA-1100 machines `--save-ram-snapshot` gives an independent read: the
kernel caches the assembled 64-bit id in bank 0, so the low word followed by
the high word (`DEADBEEF`, `0BADF00D` little-endian) should be findable there
and the old model UID should not be.
