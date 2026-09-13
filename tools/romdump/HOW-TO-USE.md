# ROMDUMP — how to use it

`ROMDUMP.EXE` copies a Psion's ROM into files on the Psion's own disk, so
you can take a copy of the machine's operating system off it.

It is written for **EPOC Release 5 Unicode (ER5u)** machines — the Conan /
Revo-Bluetooth generation — because the ROM extractors written for
EPOC R5 are non-Unicode binaries and this generation's DLLs will not load
them. It does not need a card, a cable, or anything installed first.

A ROM does not fit in one piece on a machine whose RAM is the same size,
so the dump comes off in **2 MB parts** and you can stop and start it:
write what fits, copy those parts off, delete them, run it again for the
rest. Nothing you have not asked for is ever deleted. (The Conan this was
written for has a 16 MB ROM, so it comes off in eight parts —
`roms/conan_v0.10(17)_eng.IMG` in this repository is one such dump.)

---

## 1. Put ROMDUMP.EXE on the machine

Any way you already move files onto it:

* **Beam it** over infrared from another machine or a PC with IrDA, and
  save it out of the Inbox.
* **Copy it** with the PsiWin release that talks to this generation of
  machine (2.3 or later — earlier ones do not connect to it at all).

Put it wherever your files live — `C:\Documents` is the obvious place,
and it is what the System screen shows you first.

## 2. Run it

On the System screen, **tap the file once to select it, then tap it
again** (or press Enter). A text screen opens:

```
Psion EPOC ER5u ROM dump
ROM at 0x50000000 is 16384 KB: 8 parts of 2048 KB
Next is part 1 of 8, written to C:\ROMDUMP.nnn
Write how many of the 8 left?  1-8, Q = quit:
```

The prompt always names how many parts are actually left, so it counts
down as you go — `1-8`, then `1-7`, and so on. Answer with:

| Key | What happens |
|-----|--------------|
| a digit in the range shown | write that many parts now |
| `A` | write every part that is left (offered when more than 9 are) |
| `Q` | stop; it prints what to do next |

Anything else just asks again, so a stray key cannot cancel a dump.

Pick a number that fits your free space: each part needs 2 MB, and the
machine needs room to work in besides. If you are not sure, ask for one
part and look at how much space is left afterwards.

Each part shows its progress and is then read back off the disk and
compared with the ROM, so you know it arrived intact:

```
Part 1 of 6 ........ written and checked.
```

When a batch finishes it asks again, showing the next part to write. At
the end it prints what to do next and waits for **Esc** — no other key
closes it, so the instructions stay put even if you were still holding
the last key you pressed.

## 3. Copy the parts off, and get the rest

The dump lives in:

| File | What it is |
|------|-----------|
| `C:\ROMDUMP.001`, `.002`, … | the parts, 2 MB each (the last one is shorter) |
| `C:\ROMDUMP.PRG` | four bytes remembering which part is next |
| `C:\ROMDUMP.TXT` | a summary of the last run, in plain text |

1. Copy `C:\ROMDUMP.*` to a PC (PsiWin, or beam them).
2. Delete the parts you have copied — that is what frees the space.
3. Run ROMDUMP again. It reads `ROMDUMP.PRG` and offers the *next*
   part, not the first one, so you never write the same part twice.
4. Repeat until it says every part is written.

## 4. Join the parts back together

On a PC, in part order:

```
Windows      copy /b ROMDUMP.001+ROMDUMP.002+ROMDUMP.003 rom.img
macOS/Linux  cat ROMDUMP.0* > rom.img
```

(`ROMDUMP.0*` sorts correctly as long as you have fewer than 100 parts,
which you will.)

The result is the ROM image: the same bytes the machine runs from. Check
its size against the `ROM size` line in `ROMDUMP.TXT`.

## 5. Check the dump

`C:\ROMDUMP.TXT` is written after every run, and can be read on the
machine (open it in Word, or `type romdump.txt` at an EShell prompt):

```
Psion EPOC ER5u ROM dump

ROM base   0x50000000
ROM size   0x01000000 (16384 KB)
Part size  2048 KB
Parts      8
Last batch parts 1..8, 16384 KB
Next part  none, the ROM is all written
Verified   8 parts read back, all match the ROM
Result     complete
```

`Verified … all match the ROM` is the line that matters: it means every
part was read back off the disk and compared, byte for byte, with the
ROM it came from.

---

## If something goes wrong

**It will not start a second time.** On the System screen a tap only
*selects* a file that was not already selected. Tap it again, or press
Enter, to open it.

**"cannot write it - the disk is probably full."** Free some space —
copy off and delete the parts you already have — and run it again. The
part it stopped on is written again from the beginning next time, so
nothing is left half-done *on the machine*.

> **Delete that half-written part before you copy anything off.** It is
> still sitting on `C:` under its ordinary name, and step 1 above says to
> copy `C:\ROMDUMP.*` — so it will travel to the PC looking exactly like
> a finished part, just shorter. Joined in with the rest it produces an
> image that still *opens* — the ROM's file directory is at the front, so
> tools will happily list every file in it — while the bytes the missing
> tail should have held are simply absent, and the machine built from it
> will not boot. Check `C:\ROMDUMP.TXT` before copying: `Next part` names
> the part that still has to be written, and only `Result complete` means
> you have the whole ROM. Every part except the last should be exactly
> 2,097,152 bytes.
>
> This is not hypothetical: the first dump taken with this tool lost
> 2.8 MB exactly this way, and looked valid until it was booted.

**"DOES NOT MATCH THE ROM".** The part on the disk is not what was read
out of the ROM. Delete that part, free some space and try again; if it
happens twice in the same place, do not trust the dump.

**Nothing appears on screen.** It carries on without a screen if the
machine will not give it one — look at `C:\ROMDUMP.TXT` afterwards.

**Start over from part 1.** Delete `C:\ROMDUMP.PRG`. (While it exists,
ROMDUMP will not overwrite parts you may not have copied off yet.)

---

## Running it under the emulator

The Conan in this repository has no cable or card route for getting
files onto it, so the test rig puts the binary in the ROM instead, over
a file the engineering build already shows on its desktop:

```sh
node --experimental-strip-types tools/e32/romfs.mts \
    replace roms/conan_s2_2201.engbuild.IMG \
    'Z:\System\Samples\D_EXC.exe' tools/romdump/ROMDUMP.EXE /tmp/conan-romdump.IMG

bash tests/integration/test-conan-romdump.sh     # the whole thing, checked
```

## Building it

`bash tools/romdump/build.sh` — needs clang with the ARM target, ld.lld,
llvm-objcopy, llvm-nm and node 22+. The built binary is committed next to
the source, so you only need this if you change it. See
[`../README.md`](../README.md) for the toolchain and
[`../../docs/conan-rom-dumping.md`](../../docs/conan-rom-dumping.md) for
how it works and how it was established.
