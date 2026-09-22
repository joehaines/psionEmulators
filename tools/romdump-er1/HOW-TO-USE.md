# ROMDUMP (EPOC Release 1) — how to use it

`ROMDUMP.EXE` copies a Psion's own ROM into files on the machine, so you
can take a copy of its operating system off it.

This one is built for **EPOC Release 1** machines: the **Psion Series 5**,
including the pre-release prototype builds, and the **Geofox One**. The
ROM extractors that circulate are built against later EPOC releases, and
on an R1 machine they do not fail while running — they never start. An
EPOC program names the libraries it needs by UID as well as by name, and
on an R1 machine the file server client is `EFSRV[100000bd].DLL` where
the later ones ask for `EFSRV[100039e4].DLL`. The loader looks for a
library the machine does not have and gives up. This program asks for
R1's, by R1's numbers.

It needs no card, no cable and nothing installed first, and it never
deletes anything it did not write.

**There are two of them, and you want the first:**

| File | When |
|------|------|
| `ROMDUMP.EXE` | always try this one first |
| `ROMDUMP0.EXE` | only if the first will not open at all |

They are the same program. The difference is that `ROMDUMP.EXE` asks the
machine's loader for six functions from the file server's library by
number, and `ROMDUMP0.EXE` asks for nothing at all: it carries its own
way of talking to the file server, over calls that are in the machine's
instruction set rather than in any of its files. The second cannot be
turned away by the loader, which is the one way the first can fail on a
machine nobody has tried it on. Copy both across in one go; you will
almost certainly only need the first.

Either way nothing has to be installed and no library has to be present:
whichever of the two runs, it needs only the machine's file server, and
both of them know two different ways to reach that.

---

## 1. Put ROMDUMP.EXE on the machine

Any way you already move a file onto it:

* **Beam it** over infrared from another Psion, or from a PC with IrDA,
  and save it out of the Inbox;
* **copy it** with PsiWin, or with any of the tools that talk to a
  Series 5 over the serial cable;
* **put it on a CompactFlash card** and read the card in the machine.

`C:\Documents` is the obvious place — it is what the System screen shows
you first — but anywhere will do.

## 2. Run it

On the System screen, **tap the file once to select it, then tap it
again** (or press Enter). On the selected file a single tap only selects
it; it is the second one that opens it.

**Nothing appears on the screen.** That is deliberate: this program
leaves the display and the window server alone, because those are the
parts most likely to be different on an early machine, and a program
that cannot draw is a program that cannot fail to draw. It writes what
it is doing to a file instead, and updates that file after every part —
so you can open it while the dump is still running.

Give it a minute or two. A 6 MB ROM is six files.

## 3. Read what it did

Open **`ROMDUMP.TXT`** — on the card if you have one in, otherwise on
`C:`. It looks like this:

```
Psion EPOC R1 ROM dump

Result      complete - the dump is the whole ROM
Next run    nothing - the whole ROM is written

ROM base    0x50000000
ROM size    0x00600000 (6144 KB)
Written to  D:\ROMDUMP.nnn
Done so far 6144 KB of 6144 KB

This run
  ROMDUMP.001  ROM 0x000000..0x0FFFFF  1024 KB  checked
  ROMDUMP.002  ROM 0x100000..0x1FFFFF  1024 KB  checked
  ...
  6 parts, 6144 KB, every one read back and matching
```

The first two lines are the whole answer; the rest is the detail.

`checked` means that part was read back off the disk and compared with
the ROM, byte for byte, before the dump moved past it. If `ROMDUMP.TXT`
is not there at all, the program did not run — see *If nothing happens*
below.

## 4. If the disk fills up

A Series 5's RAM disk is usually far too small for a whole ROM, so the
run stops when the disk does and the report says so:

```
Result      stopped - the disk is full
Next run    part 3, from ROM 0x00280000
```

Then:

1. copy `ROMDUMP.*` off the machine;
2. delete the **parts** you have copied — keep `ROMDUMP.PRG`, which is
   the four-line note of where to carry on from;
3. run `ROMDUMP.EXE` again. It picks up at exactly the byte it stopped
   at, however short the last part was.

Repeat until the report says `complete`.

**A CompactFlash card makes this one step.** The program writes to `D:`
if a card is there and will take the dump, and only falls back to `C:`
if it will not — a 16 MB card holds a Series 5's whole ROM with room to
spare.

## 5. Put the parts back together

On a PC, join the parts **in order, lowest number first**:

```
copy /b ROMDUMP.001+ROMDUMP.002+ROMDUMP.003+... rom.img     (Windows)
cat ROMDUMP.0* > rom.img                                    (macOS, Linux)
```

The joined file should be **exactly the ROM size the report gives** —
6144 KB for a Series 5. If it is short, a part is missing or one of them
was copied off before it was finished; the report's part list gives the
exact byte range each file holds, so you can tell which.

---

## The files it writes

| File | What it is |
|------|------------|
| `ROMDUMP.001`, `.002`, … | the ROM, 1 MB at a time, in order |
| `ROMDUMP.PRG` | 32 bytes: which byte the next run starts at |
| `ROMDUMP.TXT` | the report above, rewritten after every part |
| `ROMDUMP.LOG` | every step it took, rewritten after every line |

Deleting `ROMDUMP.PRG` starts the dump again from the beginning.

**`ROMDUMP.LOG` is the one to send back if anything goes wrong.** It is
written after every single line, so whatever happened, its last line is
where it happened. It opens with what the machine is and what the
program found on it:

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
  iConnect linked=17 found=17 matches=1 confirmed at 0x500614B0
  ...
part 1 from ROM 0x50000000 want 1024 KB
  wrote it, rc=0
  read back 1024 KB of 1024 KB, rc=0
  progress written, rc=0
```

Those `confirmed` lines are the program checking itself against the
machine it is on: it finds the file server's own library in the ROM and
recognises each call it makes by what only that call does, rather than
trusting the numbers it was built with. On a prototype whose library is
older, a line will say `** NOT THE LINKED ORDINAL **` instead — and the
program will use what it found.

## If nothing happens

No `ROMDUMP.TXT` anywhere means the program never got as far as the file
server. Worth trying:

* **Try `ROMDUMP0.EXE`.** If the machine will not open `ROMDUMP.EXE`,
  this is exactly what that second file is for: it imports nothing, so
  there is nothing for the loader to look for and fail to find, and it
  brings its own file server client rather than borrowing the ROM's.
* **Check what you copied over.** `ROMDUMP.EXE` is 23,692 bytes and
  `ROMDUMP0.EXE` is 23,168. A transfer that mangles them — anything
  that treats them as text — leaves a file the loader will not touch.
* **Check the machine.** This is an EPOC Release 1 program. It will not
  load on a Series 5mx, Revo, Series 7 or netBook: those are later
  releases with different libraries, and they need a different build.
* **Try running it from `C:`** rather than from a card, in case the card
  is the problem rather than the program.

## If it closes with "KERN-EXEC 3"

That dialog — *Program closed / Main / KERN-EXEC / 3* — means the
program ran and then hit something the machine would not do. The build
you have is not the one that did that: the first build divided, and a
Series 5's ARM710a has no instruction for division, so the compiler's
stand-in for one quietly returned nonsense until the program fell over.
This build contains no instruction an ARM710a cannot execute, and the
build refuses to produce one that does
([`docs/series5-prototype-rom-dumping.md`](../../docs/series5-prototype-rom-dumping.md)).

If you see it anyway, **send `ROMDUMP.LOG`**. It is written after every
line, so its last line names the step that did not finish, and its first
lines say what the machine's ROM and file-server library are. That is
everything needed to make the next build the right one — no guessing,
and no second trip to find out.

If there is no `ROMDUMP.LOG` at all on either drive, the program stopped
before it could reach the file server, which is itself the answer: say
so, and say whether `ROMDUMP.TXT` is there either.

## What it does not do

It reads the ROM and writes files. It does not touch the machine's own
files, it does not write to the ROM (it could not — the ROM is
read-only), and it does not need the machine to be in any particular
state. Running it twice is safe: a finished dump is left alone.
