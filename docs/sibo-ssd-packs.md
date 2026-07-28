# SIBO SSD packs: why a pack mounted read-only, and what the bus actually does

Findings from making Solid State Disks writable on the SIBO machines
(Series 3/3a/3c/3mx, Siena, Workabout, MC400), recorded so the protocol
details don't have to be re-derived from traces. Everything below was
established against `roms/series3a_v3.40f_eng.bin` running under
`harness/run`, with `PSION_SSD_TRACE=1` logging every SIBO frame.

## The symptom

Insert any pack and the Psion mounts it read-only. Disk → Directory
labels drive A: **"Disk [A], Protected, 127K free"**, no app can save to
it, and Disk → Format disk on a blank pack fails with **"Write failed"**.

Two separate causes, one on top of the other.

## Cause 1: Port A on a Flash pack is a command bus, not memory

A RAM SSD is SRAM — a Port A write stores the byte. A Flash SSD is a
bank of Intel 28F0xx-class devices, and Port A is that chip's bus:
bytes written to it are **commands**, and only the second cycle of a
program command carries data. EPOC16 speaks the first-generation
("Quick-Pulse") command set:

| Command | Cycle 1 | Cycle 2 |
|---|---|---|
| Read Memory | `0x00` or `0xFF` | read |
| Read Intelligent Identifier | `0x90` | read (even addr = maker, odd = device) |
| Program | `0x40` (or `0x10`) | write the data byte |
| Program Verify | `0xC0` | read |
| Erase | `0x20` | `0x20` |
| Erase Verify | `0xA0` | read |

The emulator used to store every Port A write as data. Two consequences:

* **Every write verify failed.** Formatting programs a byte with
  `0x40`, data, then reads it back with `0xC0` — and got `0xC0` back,
  because the verify opcode had just been stored over the data. Hence
  "Write failed".
* **The read-array command corrupted the pack.** EPOC16 opens its
  slot scan with `0x00` at offset 0. Stored as data that clears the
  `0xA5` of the `0xF1A5` FEFS magic, so an attached image would be
  quietly damaged. (The old code dodged this by dropping Port A writes
  on write-protected packs entirely — treating a symptom.)

`PsionSSD` now models the chip: a command state machine, programming
that only clears bits (`&=`, as real flash does — FEFS relies on it, a
file delete clears the entry's valid bit in place), and bulk erase of
the addressed device. On a `Protected` pack the commands are accepted
and program/erase do nothing, which is what a real part does with its
programming voltage strapped off.

Formatting a blank Flash pack on the device now runs the authentic
sequence — program every byte to `0x00`, bulk erase, erase-verify each
byte, then program the FEFS header — and lands a valid volume.

## Cause 2: SIBO control bit 4 is the address auto-increment

With the flash chip modelled, the pack still mounted as
"Unformatted!". The header read is where it showed:

```
protected pack (0xE3)            flash pack (0x23)
CONTROL 0x93, D=0, C=0           CONTROL 0x93, D=0, C=0
                                 CONTROL 0x80, DATA 0x00   ← read-array command
CONTROL 0xC0 → read 0xA5         CONTROL 0xD0 → read 0xF1  ← byte 1, not byte 0!
```

The command write consumed address 0, because the ASIC5 port-B counter
was stepping on *every* Port A access. The driver's header read
therefore started one byte late and the magic came back as `0x11F1`.

Bit 4 of the SIBO control byte is what distinguishes the two: it marks a
"multi" access that auto-increments. EPOC16 relies on the distinction —
it bursts file data through Port A with `0x90` / `0xD0` so the address
walks itself, and issues one-off accesses (flash commands with `0x80`,
the byte a verify reads back with `0xC0`) with bit 4 clear so they land
on the address already set up and leave it alone. The erase-verify loop
makes it plainest: verify command (no step), read the byte (no step),
read the port-B counter (steps) — exactly one byte per iteration.

MAME's `psion_asic5_device` steps unconditionally, but its SSD device
reports every Flash pack as hardware write-protected, so the path that
issues these commands is never taken there.

## Pack types and what EPOC16 does with them

The info byte's top three bits (D7-D5) are the memory type, set by
straps on the pack PCB — independent of the contents. `PsionSSD::Type`
maps to them:

| Type | D7-D5 | 128K info byte | EPOC16 |
|---|---|---|---|
| `Ram` | 000 | 0x03 | read/write; formats as a FAT volume ("PSION1.0" boot record), not FEFS |
| `Flash` | 001 | 0x23 | read/write FEFS; drive shows as "Flash" |
| `Protected` | 111 | 0xE3 | read-only; drive shows as "Protected" |

Types 3-5 Flash (D7-D5 = 011/100/101) exist in the info-byte table but
the Series 3a rejects them outright — the drive reads
"Unrecognised!". Type 1 is what Psion's own branded Flash SSDs used and
what the emulator presents.

The frontend attaches user packs as `Flash`; `Protected` is reserved for
factory system disks (the MC400's ROM:: disk in Pack D), which is how
that pack is strapped on real hardware.

## Header layout, for reference

Offsets are decimal in the Psionics documentation, hex here:

```
0x00  2   magic 0xF1A5
0x02  4   unique id
0x06  2   unknown (always 1)
0x08  3   unknown (always 1)
0x0B  3   pointer to the root directory entry
0x0E  8   volume name        0x16  3  volume extension
0x19  4   count of times formatted; 0xFFFFFFFF on a factory ROM image
0x1D  ..  identity string    (ROM images and erased flash cards)
      or  2-byte size in 256-byte units, 2 bytes 0xFFFF, then the
          identity string at 0x21  (field-formatted flash cards)
```

Both forms mount. The Series 3a's own formatter writes the first form
with the count programmed and `"PSION1.0"` as the identity string, and
puts the root directory entry immediately after it at 0x25. The packs
`frontend/src/lib/fefs.ts` builds use the factory layout (count erased,
root at 0x45); EPOC16 accepts either on a Flash or write-protected pack,
so the header is *not* what decided read-only — the two causes above
were.

## Getting files back off a pack

Once the Psion can write to a pack, the files it writes have to be able
to leave the machine. `frontend/src/lib/fefs.ts` gained
`readFileFromPack(image, path)` for that: it resolves a path the way
`listFiles` reports it (`HELLO.TXT`, `WRD\REPORT.WRD`) and joins the
entry's data-record chain — the first record lives in the 31-byte file
entry, continuations are 17-byte records, and a file over 64K spans
several because the length field is 16-bit.

Nothing in the reader may assume our own writer's layout. Our writer
lays a file's data down contiguously after its entry; EPOC16's allocator
places records wherever it likes, so the chain pointers are the only
thing to follow. A truncated image is clamped rather than rejected —
salvaging what is readable beats refusing the download.

The SSD dialog exposes this as a per-file **download** next to each file
in the pack listing. Two details make it work on a pack that is *in* the
machine:

* the dialog polls `getSSDBytes` every 1.5 s while a pack is inserted
  (the same trick the CF dialog uses), so a document saved on the Psion
  appears in the list without an eject;
* the download itself re-reads the live bytes rather than the polled
  copy, so a file saved seconds earlier comes out complete.

The listing is driven by the image contents, not the pack strap, so a
factory `Protected` pack (the MC400 ROM:: disk) lists and downloads its
files too — it just can't be edited from the dialog.

Case 3 of `tests/integration/test-ssd-write.sh` covers the whole path:
the Series 3a creates a Data file on drive A (File → New file, disk
chosen by *typing* the drive letter — the choice field's arrows
auto-repeat at the harness's default 8-frame hold and overshoot), and
the dumped pack is then listed and extracted with the frontend reader
via `tests/integration/ssd-extract.mts`. What comes out is a genuine
`OPLDatabaseFile` written by EPOC16's own filing system.

## Reproducing

```sh
bash harness/build.sh
tests/unit/ssd_smoke                       # protocol + flash command unit test
bash tests/boot/test-boot.sh --ssd         # per-device boot with packs attached

# Watch the bus (very verbose — a format is ~2.4 M frames):
PSION_SSD_TRACE=1 ./harness/run roms/series3a_v3.40f_eng.bin --device series3a \
    --boot-seconds 20 --skip-card --quiet-logs \
    --ssd-a tests/fixtures/fefs-128k.ssd --ssd-type-a flash 2>trace.log
```

Driving the System screen from the harness is how the writable path was
validated end to end: `--press-key AT_SEC EPOC_KEY` reaches the menus
(Menu = 148, arrows 14-17, Enter = 3, letters = ASCII), and
`--ssd-dump-a PATH` writes the pack image back out afterwards so the
guest's writes can be inspected on the host.
