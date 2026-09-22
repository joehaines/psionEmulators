# Dumping a Series 5's ROM from the machine itself

The Psion ROM extractors that circulate — PsiROMx and its kin — are built
against a later EPOC than a Series 5 runs. On a Series 5 they do not fail
part way through: they never start. An EPOC32 image names each library it
imports from by UID as well as by name, and on an EPOC Release 1 machine
the file server client is `EFSRV[100000bd].DLL` where the later ones ask
for `EFSRV[100039e4].DLL`. The loader looks for a library that is not in
the ROM and stops there. The ordinals inside those libraries are
different numbers too, and R1's text is 8-bit where ER5u's is 16-bit.

`tools/romdump-er1` is a ROM dumper written for R1 instead. It is
original code: what it needed to know it read out of Psion's own ROMs,
which this repository already disassembles at length, and out of real R1
binaries of the period that happen to be in `applib/`. It is 7.5 KB,
imports six functions from one DLL, and — unlike its ER5u sibling in
`tools/romdump` — says nothing on the screen at all.

Everything below was read out of `roms/S5_v1.00(113)_eng.bin`, a
pre-release Series 5 build, unless it says otherwise, and
`tools/e32/er1check.mts` reproduces the identifications on demand for
that ROM, for the shipping `roms/series5_v1.01(144)_eng.bin` and for the
Geofox's `roms/Geofox_v1.01(146)_eng.bin`.

## The machine really is a non-Unicode R1 build

Three independent things say so before any program runs:

* **The ROM's own file system.** `TRomHeader` at the image base gives
  `iRomBase` 0x50000000 (+0x8c), `iRomSize` 0x00600000 (+0x90) and
  `iRomRootDirectoryList` (+0x94) — the same offsets as every later
  release — but each `TRomEntry`'s name is `iNameLength` *bytes*, not
  that many 16-bit characters. The 191 names only decode as 8-bit text.
  R1 also puts a `TRomDir` for `Z:\` straight at `iRomRootDirectoryList`
  where ER5u puts a root-directory list (a count, a hardware variant and
  an address) in front of it; reading an R1 ROM the ER5u way silently
  loses the `System` level of every path. `tools/e32/romfs1.mts` is the
  R1 reader, `tools/e32/romfs.mts` the ER5u one.
* **EFSRV's own literals.** The string the file server client connects
  by, `"FileServer"`, is at 0x50065338 as ten bytes, not twenty.
* **EUser's descriptor constructors.** `TPtrC8(const TText8*)` counts a
  NUL-terminated run, ORs 0x10000000 into the length and stores
  `{length, pointer}` (at 0x500368f8); the `TPtr8` constructor next to it
  masks that top nibble off and ORs 0x20000000 in. So the descriptor
  layout is the familiar one — a length word whose top nibble is the
  type, `TPtrC` = 1, `TPtr` = 2 with `iMaxLength` at +4 — over 8-bit
  text.

## What a ROM dumper actually needs

The ROM is memory-mapped and readable from user mode at `iRomBase`, so
dumping it is a file write of a descriptor that points into it. No
driver, no kernel-mode code, nothing device-specific: the whole program
is a file-server client, and this one imports six functions and nothing
else.

| Ordinal | Function | How it was identified |
|---------|----------|-----------------------|
| 14 | `RFile::Close()` | `mov r1, #0x1a; b CloseSubSession` — and see below, because two exports have exactly that body. |
| 17 | `RFs::Connect(TInt)` | The only export that loads the address of the `"FileServer"` literal. Beside it, a helper builds `TVersion(1, 0, 78)` — F32's major, minor and build numbers — and both go to EUser's `CreateSession`. |
| 105 | `RFile::Open(RFs&, const TDesC8&, TUint)` | `CreateSubSession` with function code 0x1b and args `{name, mode}`. |
| 119 | `RFile::Read(TDes8&)` | `SendReceive` 0x1f, args `{&des, des.iMaxLength, 0x80000000}` — the last being "at the current position". |
| 133 | `RFile::Replace(RFs&, const TDesC8&, TUint)` | `CreateSubSession` 0x1d, args `{name, mode}`. |
| 170 | `RFile::Write(const TDesC8&)` | `SendReceive` 0x20, args `{&des, des.Length(), 0x80000000}`, returning early when the length masks to zero. |

The function codes are what make the identification safe rather than a
guess, exactly as they were for the ER5u dumper. Four exports create a
subsession with the consecutive codes 0x1b, 0x1c, 0x1d, 0x1e: the first
three take `(RFs&, name, mode)` and the fourth takes an extra
out-parameter, which is `RFile::Temp`'s signature and nothing else's — so
the run is `Open, Create, Replace, Temp` (105, 23, 133, 165) and 0x1d is
`Replace`. Eight exports send 0x1f and eight send 0x20; the ones that
put a `TRequestStatus&` in r3 are the asynchronous halves, the ones that
take a position put it where 0x80000000 otherwise goes, and what is left
of each eight is the plain synchronous call this program wants.

**The two closes.** Exports 14 and 15 have the same two-instruction
body and send the same code to the same function: they are the closes of
two different subsession classes. The ROM settles it. Of the 108
binaries in the prototype ROM, **22 import ordinal 14 — including every
one that opens, replaces, reads or writes a file — and none import
ordinal 15**. `tools/e32/er1check.mts` does that count rather than
hard-coding the answer. Two further things agree: `eptable.app`, a real
Series 5 application in `applib/`, imports 17, 105 and 14 and no other
close, and it must close the files it opens; and in the ER5u ROM the
same pair sits at 15 and 16, where the ER5u dumper's 15 — the one proven
on a real machine — is the lower of the two, as 14 is here.

## Why there is no console

The ER5u dumper talks to whoever is holding the machine through a
console, and that is four more imports from EUser. This one does not,
for reasons that are all about what can fail on an early machine:

* **EFSrv.dll is the same build in all three R1 ROMs** — 205 exports at
  identical offsets in the prototype, the shipping Series 5 and the
  Geofox. **EUser.dll is not**: 1699 exports in the prototype against
  1701 in the other two. Importing nothing from EUser means the one
  library whose numbering this repository cannot prove is stable is one
  the program never asks for.
* A console needs `Econs.dll` to load and the window server to be
  running. In the prototype ROM, `Econs.dll` is present but *nothing
  references it* — no binary in the image loads it by name or by UID —
  and the `eshell.exe` that the ROM's own `ETest.exe` tries to start is
  not in the image at all. That is not a machine to ask for a console
  on.
* `Console::NewL` leaves on failure, and handling a leave means a
  cleanup stack and a `TRAP`, which is more EUser.

What replaces it is a report file, rewritten after every part, so the
machine's own editor can show a dump in progress. The trade is real and
it is the deliberate one: no interactive prompt, in exchange for a
program that cannot fail for want of a screen.

## The machine's CPU is ARMv3, and does not say so

This is the part that cost the first build of this program, and it has
nothing to do with EPOC.

A Series 5's ARM710a is **ARMv3**. Three families of instruction that a
compiler emits freely are not in it:

* **BX** — ARMv4T, for interworking with a Thumb an ARM710a does not
  have. `-march=armv4t` ends every function with `bx lr`, and the
  obvious shape for an import thunk ends in `bx r12`. (The ER5u dumper's
  thunks do exactly that, and are right to: a Conan is ARMv4T.)
* **UMULL / SMULL / UMLAL / SMLAL** — the 64-bit multiplies, which
  arrived with ARMv3M. A compiler turns *every division by a constant*
  into one of these, so an innocent `n / 10` in C is enough.
* **LDRH / STRH / LDRSB / LDRSH** — ARMv4.

And the machine does not refuse them. Those encodings sit in
data-processing space, so an ARM710a quietly executes something else
instead. Asked to run `SMULL r3, r4, r0, r2` with 30 and 0x66666667 —
which should give 0x0000000C_00000012 — a Series 5 leaves both result
registers exactly as they were, while a plain `MUL` beside it gives 42.
The instruction is being taken as an `SBC` with a register-specified
shift of 103 bits, which shifts everything out and subtracts nothing.

So `n / 10` returns 0 and `n % 10` returns n. The first build of this
program printed its own progress markers as "0N" instead of "30", and
then panicked with **KERN-EXEC 3** a few steps later, when the garbage
those divisions produced was used for something that had to be a real
number. On the machine that looks exactly like a program that will not
load — which is what it was reported as.

clang has no armv3 target. What the build does instead:

* asks for `-march=armv4`, which stops the compiler ending functions
  with `bx lr` (it uses `mov pc, lr`);
* writes the import thunks as `ldr r12,[pc]; ldr pc,[r12]` — which is
  what the ROM's own import stubs are (EFSRV's are at 0x50064cf0), for
  the same reason;
* does its own division: `DivTen` in romdump.c is shifts and adds;
* and then **checks the finished binary**. `tools/e32/armv3check.mts`
  decodes every word of the code section, skipping the words the image's
  own relocation table marks as addresses, and fails the build if one
  instruction an ARM710a cannot execute got in. The build runs it; so
  does `tests/unit/er1-romdump-image.mts`.

## What it does when it cannot tell you anything

There is no console, the machine's own error dialog says only
"KERN-EXEC 3", and a panic takes the process's memory with it. So
everything this program might want to say afterwards has to be on the
disk before it happens.

`ROMDUMP.LOG` is rewritten from a buffer after every line. Whatever
went wrong, its last line is where it went wrong — and its first lines
are what the machine is:

```
ROMDUMP for EPOC R1 - 2026-09-22
imports linked: EFSRV[100000bd].DLL 14,17,105,119,133,170
connect to the file server rc=0
log on drive C
rom hdr base=0x50000000 size=0x00600000 root=0x504138A0
rom hdr +0x80: 00910101 5AF66000 00DFF5C3 50000000 00600000 504138A0 80100000 80000000
              50413978 504139A0 5EBDD3D0 00000000 00000000 00000000 00000280 000000F0
EFSrv.dll at 0x5005F470 uid1=0x10000079 uid3=0x100000BD exports=205
  code 0x5005F4C8..0x500668D0 exportdir=0x5006659C
  iClose linked=14 found=14 matches=2 confirmed at 0x50061330
  iConnect linked=17 found=17 matches=1 confirmed at 0x50061480
  iOpen linked=105 found=105 matches=1 confirmed at 0x5006040C
  iReplace linked=133 found=133 matches=1 confirmed at 0x5006044C
  iRead linked=119 found=119 matches=1 confirmed at 0x50060494
  iWrite linked=170 found=170 matches=1 confirmed at 0x50060718
try drive D (needs room for a whole part)
  wrote a report there and reopened it, rc=-18
  will not take a file - next drive
try drive C (needs room for a whole part)
  wrote a report there and reopened it, rc=0
part 1 from ROM 0x50000000 want 1024 KB
  wrote it, rc=0
  read back 1024 KB of 1024 KB, rc=0
  progress written, rc=0
```

The file is always written at its full 8 KB, padded. A file that already
occupies its clusters can be rewritten on a disk with nothing left on
it; one that has to grow cannot — and a log that disappears exactly when
the disk fills would be worse than no log.

## The program checks its own numbers against the machine

The six ordinals above were read out of three R1 ROMs, all of which
carry the same EFSrv.dll. A prototype earlier than any of them need not.
So the program does not trust them: on the machine it is running on, it
walks that machine's ROM directory, finds its EFSrv.dll, reads its
export table, and recognises each call by the same fingerprints
`tools/e32/er1check.mts` uses on a PC — the function code the call sends
the file server, or the literal it loads. What it finds is what it
calls. The `confirmed` lines above are that check agreeing with the
build; a machine where it disagrees gets

```
  iRead linked=119 found=126 matches=1 ** NOT THE LINKED ORDINAL ** using 0x5006xxxx
```

and the program uses what it found. Anything less clear than exactly one
match is left alone and logged, because a wrong address is worse than an
old one.

Two things make this safe. An R1 ROM's EFSrv.dll has no data and no bss
(its ROM image header says `iDataSize` 0, `iBssSize` 0) and its own
imports are resolved in ROM at build time, so its code is complete where
it stands and can be called at the address it sits at. And every read
the walk makes is bounds-checked against the ROM's declared extent: a
machine whose directory is laid out differently gets a line in the log
saying so, not a data abort.

### And a build that imports nothing at all

Finding the calls in the ROM removes the need for the ordinals to be
right. It does not, by itself, remove the loader's binding: the image
still imports six ordinals from `EFSRV[100000bd].DLL`, and a machine
with no DLL of that UID would refuse it before it ran.

So `build.sh` builds the same source a second way. `ROMDUMP0.EXE` has
**no import table** — `iDllRefTableCount` is 0 — so there is nothing for
the loader to look for and nothing it can fail to find.

That build cannot call EFSrv, so it does not: the file server's client
side is written out inside the program, over the kernel's own executive
calls. The next section is what that is. The ROM's library is still
looked for and still written into the log, because it is the most useful
thing a log off an unknown machine can say — but nothing calls it unless
the calls in this program turn out not to work.

Both builds therefore have two ways to reach the file server and take
whichever answers, in opposite orders: `ROMDUMP.EXE` tries the library
the loader bound and falls back to the kernel; `ROMDUMP0.EXE` starts
with the kernel and falls back to whatever library it can find. And
"answers" is not taken on trust — the log is the first thing written,
and a machine where no drive will take it gets the other set of calls
tried before the dump starts rather than after it has silently failed.

Both work: on the emulated Series 5 each loads and dumps the whole 6 MB
ROM, and `bash tests/integration/test-er1-romdump.sh [--no-imports]` is
those two runs.

This is also the answer to whether the libraries could be "packaged into
the exe". A *library* cannot: EPOC32 has no static linking, an E32Image
resolves imports by (name, UID3) against the ROM's own file system at
load time, and a copy of EFSRV carried along would still have to be
loaded and bound like any other. What can be carried is the only part of
it that matters here — the few hundred bytes of client code that turn a
call into a message — and that is what the no-import build does.

## Talking to the file server with nothing but the kernel

Everything a `DLL` does for a caller of `RFile::Write` is assemble a
message and hand it to the kernel. The kernel is not a DLL: it is
reached by an `SVC` instruction, which is in the instruction set and
cannot be missing from a ROM. So the client side can be written out in
the program, and then the program needs no library at all.

The shapes below were read out of an R1 machine's own `EUser.dll` and
`EKern.exe` — `tools/e32/romfs1.mts extract` and a disassembler — not
from documentation:

* **A message** is `SVC #0xC00031` with the function in `r0`, a
  four-word argument block in `r1`, a `TRequestStatus *` in `r2` and the
  session handle in `r3` (EUser's `RSessionBase::DoSend`). The handler
  returns to the caller's `lr`, not to the instruction after the `SVC` —
  which is why EUser's whole executive table is one `SVC` per call, with
  a different call in the very next word.
* **A request status** holds `0x80000001` while its request is
  outstanding. EUser writes it before every send and compares against it
  in `User::WaitForRequest`, both as `mov`/`cmp rN, #96, #6`.
* **Waiting** is `SVC #0xC0004D` (`WaitForAnyRequest`), and each one
  consumes one of the thread's request signals — not necessarily this
  request's. EUser counts the surplus and hands it back with
  `SVC #0xC0004E`, and so does this program.
* **A subsession** — an open file — is created by an ordinary message
  whose fourth argument is a `TBuf8<4>` the server writes the new handle
  into; every later message on it carries that handle as its fourth
  argument. `RSubSessionBase` keeps it at offset 12.
* **A session** is `SVC #0xC00076` with selector 22 and four
  descriptors: where to put the handle, the server's name, a `TBuf8<8>`
  holding the version asked for and the message-slot count, and a second
  out-parameter.

That last one is where this went wrong, and it is worth writing down
because nothing about it is visible from the outside.

**Creating a session is only half of connecting.** `SVC #0xC00076`
returns `KErrNone` and a perfectly good handle, and at that point the
*server* has not been told anything. What tells it is an ordinary
message with **function −1**, carrying the word the kernel handed back
through that second out-parameter and a descriptor over the four bytes
of the version — the last thing `RFs::Connect` does (EUser
`0x5004288c` in a Series 5 ROM), and easy to miss precisely because it
is not part of the executive call.

Leave it out and the session looks right: a handle, no error, a log line
saying `rc=0`. The first real request is then answered by the file
server killing the thread that sent it. No panic dialog, no log line, no
exit code — the program simply stops inside an `SVC` that never returns,
which is the least informative failure an EPOC machine can produce.

Finding that took a breadcrumb: a small block in the program's `.bss`,
behind the magic `KCL1`, written *before* each kernel call rather than
after it. A call that never comes back leaves nothing else behind, and
that block — the stage, the function code, the handle and the four
argument words — is the only account of it there is. It survives in a
RAM snapshot of a machine that stopped, which is how this was read, and
it is left in the program for the same reason the log is.

## The container

`tools/e32/e32link.mts` writes the E32Image by hand, as it does for the
ER5u dumper; the R1 differences are the import name and ordinals above
and `--tools-version 0x560001`. Real R1 binaries were the worked
example this time rather than a ROM file: `applib/epocvault/tads/eshell.exe`
and its neighbours are RAM-format E32Images built by Psion's own tools
for this generation, and they confirm field for field what the packer
writes — `'EPOC'` at +0x10, `iCpuIdentifier` 0x2000, UID checksum
0x045ac39e for `(0x1000007a, 0, 0)`, the import address table holding
the ordinals in import-table order with a terminating zero at
`iTextSize`, relocations of type 3 ("inferred"), `iVersion` between
0x00560001 and 0x006e0001, a 0x1000/0x100000 heap and an 8 KB stack.

**What the loader checks is not a guess either.** EFile.exe's own
E32Image validator is at 0x500578a4 in the prototype ROM, and it is
short: the `'EPOC'` signature at +0x10, then every size and offset in
the header tested for being non-negative and inside the file, with
`iHeapSizeMax >= iHeapSizeMin` and `iCodeSize >= iTextSize`; anything
else is `KErrCorrupt`. It does not look at `iVersion`, at the UID
checksum or at the code checksum. `tools/e32/er1check.mts check` applies
that list — plus the two things that actually stop the later dumpers, a
DLL named by a UID the ROM does not have and an ordinal past the end of
a DLL's export table — to the built binary against a real ROM.

**UID3 decides what "open" means.** `Z:\System\Recogs\RecExe.rdl`, the
ROM's own executable recogniser, matches a file by its UID1 being
`0x1000007a` rather than by its name, so the System screen will offer to
run this whatever it is called. It ships with UID3 zero, like every
console-style EXE of the period, so the Shell runs it rather than
looking for an application registered under that UID.

## What the program does

```
D:\ROMDUMP.001 …   the parts, 1 MB each
D:\ROMDUMP.PRG     32 bytes: which byte the next run starts at
D:\ROMDUMP.TXT     what the run did, rewritten after every part
```

* **It tries `D:` before `C:`.** A Series 5's RAM disk cannot hold 6 MB
  of ROM; a CompactFlash card can. `D:` being the removable drive is the
  ROM's own convention — `ETest.exe` looks for `D:\ewsrv.exe` and
  `D:\final.exe` at boot before falling back to the ones in `Z:`.
* **A drive has to earn the dump.** Writing the first report is how a
  drive is tried, so a drive that is not there fails harmlessly; and a
  drive that cannot take one whole part is passed over for the next one,
  so a card with a few hundred bytes free does not take the dump away
  from the RAM disk beside it. Only if no drive can take a whole part
  does it settle for whatever the first one will accept, so a very small
  disk still gets somewhere, one run at a time.
* **Progress is a byte offset, not a part number.** This is the lesson
  of the ER5u dumper's one real failure (`docs/conan-rom-dumping.md`):
  on a real Conan, a part cut short by a full RAM disk was left looking
  like any other part, was copied off with the rest, and the joined
  image was 2.8 MB short while parsing perfectly. Here a short part is
  *expected* — it is what a full disk produces — and the next run starts
  at the byte the last one really reached.
* **What counts as written is what the file holds.** Every part is read
  back off the disk and compared with the ROM before the dump moves
  past it, and the dump advances by the number of bytes that matched,
  not by the number the writes said they accepted: a write rejected for
  want of space can still have put some of its bytes in the file, and
  those bytes are the ROM's too. A part that comes back *different*
  stops the run, and the offset does not move.
* **The progress file is written before each part as well as after it**,
  so that its 32 bytes are already on the disk when the part fills it.
  Rewriting a file that exists costs no more room than it already has;
  creating one on a full disk is not possible at all, and without that
  the run before this one repeated the same part for ever.

## How it was found, and how it is tested now

The program is run on an emulated Psion Series 5 —
`bash tests/integration/test-er1-romdump.sh` — and it dumps that
machine's whole 6 MB ROM: six 1 MB parts, each read back off the RAM
disk and compared with the ROM before the next one starts. The report it
leaves behind is read out of a snapshot of the machine's RAM, and the
run is checked from outside as well: a 64-byte run of the real ROM
image, sampled every 64 KB, has to be findable in that RAM, which it can
only be if the parts really are the ROM.

**Getting it onto the machine is the awkward part**, and what works is
worth writing down. The emulated Series 5's CompactFlash slot does not
mount a card yet (its PC-card driver loops on the socket-enable
sequence), and its R1 file server does not answer the RFSV requests the
Remote Link client sends — `listDrives` works, `listDirectory` times
out. So the test puts the binary *in the ROM*, over
`Z:\System\Samples\Welcome to Series 5` — a file the System screen
shows on the desktop — with `romfs1.mts --rename` giving the entry a
name that ends `.EXE`. Opening it from the desktop then goes through
`RProcess::Create` and the loader like any other program: the image is
read, relocated, its imports are bound by ordinal, and it runs. Without
the rename the Shell shows it with a question-mark icon and will not
open it.

That is a launch path, not a loading exemption: a binary swapped over
`Z:\System\Libs\EwSrv.exe`, which the ROM's own `ETest.exe` starts by
name at boot, does not execute a single instruction — neither this
program nor a four-instruction probe built the same way — while the
machine still paints its desktop. Whatever the kernel does for the
programs it starts at boot, it is not the loader path the Shell uses.

**How the KERN-EXEC 3 was found**, since the method generalises: a
panic takes the process's memory with it, so nothing written to .bss
survives to be read out of a RAM snapshot. What does survive is a file.
A scratch build with a `Mark(n)` that rewrites `C:\RDMARK.TXT` with the
number of the last step reached put the fault inside `WriteReport` —
and the marker it left said `RDMARK0N` where it should have said
`RDMARK30`, which is `'0' + 30` for a digit that should have been
`30 % 10`. That is the division, and everything above follows from it.

Three things run in CI beside the end-to-end test.

**`bash tests/unit/run-er1-romdump-test.sh`** compiles `romdump.c`
unchanged for the host, against a stand-in for the six EFSRV calls and a
stand-in ROM mapped at 0x50000000 — and, for the self-check, the *real*
ROM images mapped there instead, so the export-recognising above is put
to every R1 ROM in the repository in a millisecond rather than a
three-minute emulator run. It then puts the program through what a
Psion puts it through: a card with room for all of it; a RAM disk with room
for two parts and a bit, where the whole ROM has to come off in three
runs and join up; a disk smaller than a single part; a card that is
there but takes nothing; a disk that gives back something other than
what was written; a finished dump run again; a ROM header that does not
check out. Every one of those cases found something the first time it
was run.

**`node --experimental-strip-types tests/unit/er1-romdump-image.mts`**
asks each of the three R1 ROMs, independently, which EFSRV export is
which, requires the same eight answers from all three, runs the built
`ROMDUMP.EXE` through the checks R1's own loader applies against each
ROM, and requires the binary to be ARMv3-clean.

**`node --experimental-strip-types tests/unit/e32-format.mts`** is the
existing check that the E32Image packer agrees with Psion's own tools.

Nothing here has been run on a real Series 5 — still less on a Protea.
If you run it on one, `ROMDUMP.TXT` is what to send back.
