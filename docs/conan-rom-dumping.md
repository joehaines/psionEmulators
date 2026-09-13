# Dumping the Conan's ROM from the machine itself

The Psion ROM extractors written for EPOC Release 5 — PsiROMx and its kin —
are ER5 (non-Unicode) binaries. The Conan is an **ER5u** machine, a separate
ABI: every descriptor-taking export takes 16-bit text, and the DLLs those
programs import are different builds with their own ordinal numbering. An ER5
binary's imports cannot bind against an ER5u ROM, so the extractor never gets
as far as running.

`tools/romdump` is a ROM dumper written for ER5u instead. It is original code —
nothing was taken from, or disassembled out of, anyone's extractor; what it
needed to know it read out of Psion's own ROM, which this repository already
disassembles at length. It is under 8 KB, imports ten functions, and talks to
whoever is holding the machine through the same console the ROM's own EShell
uses.

Everything below was read out of `roms/conan_s2_2201.engbuild.IMG` unless it
says otherwise, and `tools/e32/romfs.mts` and `tests/integration/test-conan-romdump.sh`
reproduce it.

## The machine really is a Unicode build

The ROM's own file system says so before any binary does: `TRomHeader` at the
image base gives `iRomBase` 0x50000000 (+0x8c), `iRomSize` 0x00C00000 (+0x90)
and `iRomRootDirectoryList` (+0x94), and every `TRomEntry` name below it is
UCS-2 — 475 files that only decode as UTF-16LE. The shipping Revo's image next
to it stores the same kind of names as 8-bit text. (`core/conan.h` records the
same split in the two ROMs' HAL string tables.)

## What a ROM dumper actually needs

The ROM is memory-mapped and readable from user mode at `iRomBase`, so dumping
it is a file write of a descriptor that points into it. No driver, no
kernel-mode code, nothing device-specific: the whole program is the file-server
client, six imports from `EFSRV[100039e4].DLL`, plus four from
`EUSER[100039e5].DLL` for the screen it talks to the owner through.

| Ordinal | Function | How it was identified |
|---------|----------|-----------------------|
| 18 | `RFs::Connect(TInt)` | The only export that loads the literal `"FileServer"` (UCS-2 at 0x500a1dd4) and passes it, with a `TVersion(1,2,234)`, to EUser's `RSessionBase::CreateSession` (EUSER ordinal 285). |
| 121 | `RFile::Open(RFs&, const TDesC16&, TUint)` | `CreateSubSession` (EUSER 286) with function code 0x1b and args `{name, mode}`. |
| 136 | `RFile::Read(TDes8&)` | `SendReceive` (EUSER 974) 0x1f, args `{&des, des.iMaxLength, 0x80000000}` — the last being "at the current position". |
| 151 | `RFile::Replace(RFs&, const TDesC16&, TUint)` | `CreateSubSession` 0x1d, args `{name, mode}`. |
| 194 | `RFile::Write(const TDesC8&)` | `SendReceive` 0x20, args `{&des, des.Length(), 0x80000000}`, returning early when the length masks to zero. |
| 15 | `RFile::Close()` | `mov r1, #0x1a; b CloseSubSession` (EUSER 170), which clears the two words of the subsession handle. |

The console four came out of `Z:\System\Samples\EShell.exe`, which is where
the ROM's own text prompt comes from, and they are the whole of how it draws
itself:

| Ordinal | Function | How it was identified |
|---------|----------|-----------------------|
| EUSER 731 | `Console::NewL(const TDesC16&, TSize)` | EShell builds a `TPtrC` on `"ESHELL"`, pushes `TSize(-1,-1)` and calls it; the result is the console pointer it keeps in a global and passes to everything below. Inside, it allocates and calls `Create(aTitle,aSize)` through the new object's vtable. |
| EUSER 837 | `CConsoleBase::Printf(TRefByValue<const TDesC16>, …)` | Every line EShell writes goes `TPtrC(literal)` → `Printf(console, &ptr, …)`; the export opens with the `push {r1,r2,r3}` of an ARM variadic and formats into a 0x210-byte buffer. |
| EUSER 511 | `CConsoleBase::Getch()` | Called on the console with no arguments before EShell compares the result against `'Y'` and `'N'`; inside, it calls `Read` through vtable+0x10 and waits. |
| EUSER 746 | `CTrapCleanup::New()` | The first thing EShell's `E32Main` does; a null return is its `KErrNoMemory`. The console's allocations need it underneath. |

The function codes are what make the identification safe rather than a guess.
Four exports create a subsession with consecutive codes 0x1b, 0x1c, 0x1d, 0x1e:
the first three take `(RFs&, name, mode)` and the fourth takes an extra
out-parameter, which is `RFile::Temp`'s signature and nothing else's — so the
run is `Open, Create, Replace, Temp` and 0x1d is `Replace`. Four exports send
0x1f and four send 0x20 with the same argument shapes, which are `RFile::Read`'s
and `RFile::Write`'s four overloads; 0x1f's are the ones that fill a descriptor
and 0x20's are the ones that consume one.

Two cross-checks agree with all of it. `Z:\System\Samples\D_EXC.exe`, EPOC's own
crash dumper — a program that connects to the file server, replaces a file,
writes it and closes it — imports EFSRV ordinals 15, 18, 151, 184, 194, 203 and
216, which contains exactly the four this program uses to write a file
(`Connect`, `Replace`, `Write`, `Close`) and nothing that contradicts them. And
the descriptor layout the table implies
(a length word whose top nibble is the type, `TPtrC` = 1, `TPtr` = 2 with
`iMaxLength` at +4) is exactly what EUser's own `TPtrC16` constructor builds:
it counts a NUL-terminated run, ORs in 0x10000000 and stores `{length, ptr}`.

Ordinals are frozen per EPOC release, so these are the ER5u numbers rather than
this image's numbers — but only this image was available to check.

## The container

There is no SDK here and `petran` does not run on Linux, so `tools/e32/e32link.mts`
writes the E32Image itself. Its worked example is in the ROM:
`Z:\System\Samples\D_EXC.exe` is a RAM-format E32Image stored as a plain file,
not an execute-in-place ROM image, so it shows exactly what the loader wants.

```
0x00  iUid1 iUid2 iUid3 iUidChecksum      0x1000007a for an EXE
0x10  'EPOC'  iCpuIdentifier (0x2000)
0x18  iCheckSumCode iCheckSumData
0x20  iVersion(tools) iTime(TInt64) iFlags (2 = EXE, 3 = DLL)
0x30  iCodeSize iDataSize iHeapSizeMin iHeapSizeMax iStackSize iBssSize
0x48  iEntryPoint iCodeBase iDataBase iDllRefTableCount
0x58  iExportDirOffset iExportDirCount iTextSize
0x64  iCodeOffset iDataOffset iImportOffset iCodeRelocOffset
0x74  iDataRelocOffset iPriority                          (header = 0x7c bytes)
```

* **`iUidChecksum`** is a CCITT CRC-16 (poly 0x1021, zero seed) of the three
  UIDs' even-numbered bytes in the low half and their odd-numbered bytes in the
  high half. Verified against four ROM binaries: `(1000007a,0,0)` → `045ac39e`,
  `(10000079,1000008d,100039e4)` → `ea0535b6`.
* **`iCheckSumCode`** is not a CRC at all — it is the plain 32-bit sum of the
  code section's words. D_EXC's 4444 code bytes sum to its stored `36c3f743`.
* **The code section** is text, then the import address table, then constants;
  `iTextSize` is the IAT's offset, so nothing may come between them. Each IAT
  slot holds its import's *ordinal number* until the loader overwrites it with
  the address that ordinal resolves to, and the slots are filled in import-table
  order, one per import, with a terminating zero word. Code reaches an import
  through a three-instruction thunk that loads its slot and jumps.
* **Relocations** are `(type << 12) | offset-in-page` halfwords, in per-page
  blocks; the ROM's own binaries use type 3 ("inferred": the loader decides from
  the value whether it is code- or data-relative). `iSize` in the relocation
  section excludes its own 8-byte header; in the import section it includes
  everything.

Finding the relocations without a linker that knows the format is the one part
that is a technique rather than a fact: `tools/romdump/build.sh` links the same
objects twice, at 0x400000 and 0x900000, and every word that moved by the delta
held an absolute address. A word that moved by anything else fails the build.

## Two things the machine taught us at run time

**Where RAM-loaded code goes.** A build whose entry point was a four-instruction
loop showed the harness's PC sampler sitting at 0xfff00000, 0xfff00008 and
0xfff0000c — so this kernel maps a RAM-loaded image at 0xfff00000, and the
image had been read, relocated and entered.

**UID3 decides what "open" means.** Opening the binary from the System screen
runs it only when its UID3 is zero, as D_EXC's is. Built with a UID3 of its own,
the same program is not run: the Shell opens EShell instead, presumably having
looked the UID up, found no application registered for it, and fallen back.
`tools/romdump/build.sh` therefore ships `--uid3 0`.

## Using it

The dump cannot be one file. The Conan's ROM is 12 MB and it would be written
to a RAM disk on a machine with 16 MB of RAM in total, so `romdump` cuts the
ROM into 2 MB parts and can be resumed:

```
C:\ROMDUMP.001 …   the parts, 2 MB each (the last one short)
C:\ROMDUMP.PRG     four bytes: the part to write next
C:\ROMDUMP.TXT     what the run did
```

The program asks how many parts to write now — the prompt names how many are
actually left, counting down as they are written, with `A` offered only when
more than nine remain and `Q` to stop — prints a row of dots as each one is
written, says whether it read back clean, and ends with what to do next. Every batch re-reads the progress file
before it starts, recording it again after every part. Copy the parts off the machine, delete them, run
it again and it carries on from where it stopped; join them in order (`cat`,
`copy /b`) to rebuild the image. Nothing is ever deleted by the program, and a
part interrupted by a full disk is written again from the start next time.

Every part is read back off the disk and compared with the ROM before the run
moves on — a dump nobody checked is worth very little, and this is the one thing
the machine can tell you that a size in a directory listing cannot. It costs
only what the read costs, on the same six imports the write already needs, and
it is what the report's `Verified` line answers.

Resuming is tested rather than asserted (see below). The one path still
designed rather than exercised is the disk filling up — the emulated machine has
room for the whole ROM — where a failed write stops the run and leaves the parts
already written intact, so the progress file sends the next run back to the part
that failed. A run that finds the dump already finished writes its report and
stops without touching the parts on the disk, since they may be ones nobody has
copied off yet; deleting `ROMDUMP.PRG` asks for a fresh dump.

On a real machine, get `ROMDUMP.EXE` onto the device — beamed over IrDA, or
copied with the PsiWin release that speaks to this generation — and open it from
the System screen. It exits silently; `C:\ROMDUMP.TXT` is the report, and it is
UCS-2 because the machine is, so the machine's own text can display it:

```
Psion EPOC ER5u ROM dump

ROM base   0x50000000
ROM size   0x00C00000 (12288 KB)
Part size  2048 KB
Parts      6
This run   parts 1..6, 12288 KB
Next run   part 7
Verified   6 parts read back, all match the ROM
Result     complete
```

It reads the ROM's base and size out of `TRomHeader` rather than assuming the
Conan's, so a shorter ER5u ROM dumps only what it has, and a header that does
not check out stops the run instead of dumping nonsense.

## More things the machine taught us

**Opening a file twice.** On the System screen a tap *selects* a file that was
not already selected, and only a tap on the selected one opens it. That is why
the dumper appeared not to be runnable a second time: Enter alone, or a single
tap, was landing on a desktop whose selection had moved while the program ran.
Tap-to-select then Enter opens it again every time, which is what the resume run
below does and what [`HOW-TO-USE.md`](../tools/romdump/HOW-TO-USE.md) tells the
owner of a real machine to do.

**"Press any key" is the wrong way to end.** A key held long enough to repeat
leaves the extra presses in the console's queue, and the first thing the closing
prompt did was take one — so the instructions appeared and the window shut in
the same instant. It waits for Esc now, which a repeat of the key that answered
the menu cannot be. (In the harness, Esc is scan code 4; the key code the
program compares against is 27.)

**A stack smash looks like KERN-EXEC 3.** The first console build wrote its
closing instructions — about 300 characters — through a helper with a 256-entry
buffer, and the machine put up "Program closed / Main / KERN-EXEC 3" instead.
`Say()` now copies in bounded pieces, so the length of what it prints cannot
matter.

## How this is tested

`bash tests/integration/test-conan-romdump.sh` runs the whole thing against the
real ROM. `tools/e32/romfs.mts` swaps the built binary into the image over
`D_EXC.exe` — a file the engineering build puts on the desktop, and one the
loader treats exactly as it would treat a copy on C: — then the harness boots
the machine, dismisses the image's own Agenda panic dialog and opens the dumper
(a binary too big to fit where D_EXC's bytes were goes after the end of the
image instead, inside the 12 MB its header declares, with the directory entry
pointed at it). It does that twice, for two different questions.

**Is it resumable?** The first run asks the dumper for one part, quits, and
opens it again — a second process, from the desktop — which offers *part 2*,
writes it, and then offers part 3. Nothing but the four bytes in
`C:\ROMDUMP.PRG` connects the two, which is the point
(`tests/golden/conan-romdump-resume.pgm`):

```
ROM at 0x50000000 is 12288 KB: 6 parts of 2048 KB
Next is part 2 of 6, written to C:\ROMDUMP.nnn
Write how many of the 5 left?  1-5, Q = quit: 1
Part 2 of 6 ........ written and checked.

ROM at 0x50000000 is 12288 KB: 6 parts of 2048 KB
Next is part 3 of 6, written to C:\ROMDUMP.nnn
Write how many of the 4 left?  1-4, Q = quit:
```

**Is the dump right?** The second run asks for all six parts, waits them out, and
then asks the machine to show its own work — `dir`, then `type romdump.txt`:

```
Directory of C:\
Documents        <DIR>
ROMDUMP.001      2097152    12/05/2031  12:18:08.000000
…
ROMDUMP.006      2097152    12/05/2031  12:18:08.000000
ROMDUMP.PRG            4
ROMDUMP.TXT          662
System           <DIR>
    8 Files  12583578 bytes
```

6 × 2097152 is 12582912, the ROM's declared length exactly, and the report
printed under it says all six parts were read back and match. That console is
`tests/golden/conan-romdump.pgm` — the RTC is pinned, so the timestamps and
therefore the whole screen are the same on every run.

Nothing in that comes from outside the machine, so the run is checked from
outside too: for every 64 KB step through the first three parts, that run of ROM
bytes is found in a snapshot of the machine's RAM. It stops at part 3 because
`--save-ram-snapshot` dumps one 8 MB bank and the back half of a 12 MB dump
lands in the other one.

## What it produced on a real machine

Everything above was established against `roms/conan_s2_2201.engbuild.IMG`
and reproduced under the emulator. The tool has since been run on an actual
Conan, and `roms/conan_v0.10(17)_eng.IMG` is what came off it — eight
2 MB parts, joined in order, `ROMDUMP.TXT` reporting `Parts 8`,
`Next part none, the ROM is all written`, `Result complete`.

The machine's ROM is not the image the tool was written against, and the
differences are worth recording:

| | engineering image | real machine |
|---|---|---|
| TRomHeader version | 0.01(22) | 0.10(17) |
| Built | 2001-05-12 | 2001-06-20 |
| `iRomSize` | 0x00C00000 (12 MB) | 0x01000000 (16 MB) |
| Image delivered | 0xBB2000 (short, padding only) | 0x1000000 (all of it) |
| Files | 475 | 614 |
| Splash | the Revo's, "EPOC Release 5" | its own, "Psion Conan … EPOC Release 6" |

Two things follow from that.

**The machine is an EPOC R6 build.** Its own splash reads "Psion Conan ©
Psion Digital 2001 / EPOC Release 6 © Copyright Symbian LTD 2001", over a
CONAN wordmark carrying ARM, EPOC and Bluetooth badges. The engineering
image reports R5 and paints the Revo's splash, so "Conan" was this
repository's name for the device until the machine supplied its own.

**The ER5u ordinals hold on R6.** The file-server and console ordinals in
the tables above were read out of an R5u image; the binary built from them
ran unmodified on the R6 machine, wrote every part and verified each one
against the ROM. Ordinals are frozen per release, so that is a result about
the two releases as much as about this program.

### The first attempt lost 2.8 MB, and looked fine

Worth recording because the failure is silent. The first run stopped partway
through part 7 — the RAM disk filled — and `WritePart` leaves the part it
failed on where it is. Its name is an ordinary part name, so when the parts
were copied off with `copy C:\ROMDUMP.*` the half-written one came too. The
joined image parsed perfectly: the ROM's directory sits at the front, so
`tools/e32/romfs.mts` listed all 614 files. But 72 of them were wholly
past the end of the delivered bytes — including `Shell.RSC`, `Shell.mbm`
and `Splash.mbm` — and the machine booted to a blank screen.

What gave it away, in order:

- the joined file was 0xD28000 where the header's `iRomSize` said
  0x1000000;
- the directory placed file data as far as 0xF0BA03, past the end of the
  file, where the engineering image's own file data stops at 0xBB15D3,
  *inside* its short image — that image's missing tail really is padding,
  this one's was not;
- `ROMDUMP.007` was 1,212,416 bytes, exactly 37 × `CHUNK_BYTES` — the
  write loop's step, and therefore the only size a stopped write can leave
  behind.

The parts themselves were sound: every ROM binary in all seven had a valid
UID1 at exactly the address the directory gave. The dump was not corrupt,
just short — and the re-run rewrote part 7 from its beginning, byte-for-byte
over the abandoned prefix, exactly as the resume is meant to.

`tools/romdump/HOW-TO-USE.md` now warns about this under "cannot write it".
The tool could also delete a part it failed to write, which would remove the
trap at the cost of one more EFSRV import.
