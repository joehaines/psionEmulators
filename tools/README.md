# tools/

Programs that run **on** the emulated machines, and the small toolchain
that builds and installs them. Nothing here is part of the emulator or the
web app; `scripts/` holds the build and asset tooling for those.

```
e32/       EPOC32 binary tooling
  e32link.mts   flat ARM binary (clang + ld.lld) -> E32Image .exe
  romfs.mts     list / extract / replace files inside an ER5u ROM image
  romfs1.mts    the same for an EPOC Release 1 ROM, whose names are 8-bit
                and whose root directory sits somewhere else
  er1check.mts  which EFSRV export is which in an R1 ROM, and whether an
                E32Image will load on it
  armv3check.mts  refuse a binary holding an instruction a Series 5's
                ARM710a cannot execute (BX, the long multiplies, the
                halfword loads)
romdump/     ROMDUMP.EXE for EPOC R5 Unicode machines (Conan / Revo
             Bluetooth), with a console UI
romdump-er1/ ROMDUMP.EXE for EPOC Release 1 machines (Series 5, including
             the prototype builds, and the Geofox). Imports six functions
             from one DLL, says nothing on the screen, checks those six
             against the machine's own ROM before using them, and keeps
             a log that survives a panic. Built twice: ROMDUMP.EXE, and
             ROMDUMP0.EXE with no import table at all — it carries the
             file server's client side inside it, over the kernel's own
             executive calls, for a machine that has no library it can
             borrow one from
```

Each has a HOW-TO-USE.md written for someone holding the machine.

## Building an EPOC binary here

There is no Symbian SDK and `petran` does not run on Linux, so
`e32link.mts` writes the E32Image container itself: header, import address
table, import section and relocations, all of it checked against a real
ER5u binary that ships in the Conan ROM. See its file header for the
layout and [`docs/conan-rom-dumping.md`](../docs/conan-rom-dumping.md) for
how the format and the DLL ordinals were established.

```sh
bash tools/romdump/build.sh          # clang + ld.lld + e32link -> ROMDUMP.EXE
bash tools/romdump-er1/build.sh      # the same, against R1's DLL and ordinals
```

The build needs clang with the ARM target, `ld.lld`, `llvm-objcopy`,
`llvm-nm` and node 22+. The packed binary is committed next to its source
so the tests and the docs do not depend on that toolchain being present;
each `build/` directory is not.

## Running one on an emulated machine

`romfs.mts replace` swaps a binary into a ROM image over a file already in
it, which is how `tests/integration/test-conan-romdump.sh` gets the dumper
onto the Conan: its engineering ROM shows `Z:\System\Samples\D_EXC.exe` on
the desktop, so a binary put there is one keypress from running, and the
loader treats it exactly as it would treat a copy installed on C:. A
replacement that does not fit where the original's bytes were is appended
past the end of the image — inside the length the ROM header declares —
and the directory entry is pointed at it, so nothing else moves either
way.

```sh
node --experimental-strip-types tools/e32/romfs.mts list roms/conan_s2_2201.engbuild.IMG
bash tests/integration/test-conan-romdump.sh
```

The same route works on an EPOC Release 1 machine, with one wrinkle: the
Shell decides what a file is from its name as well as its UID, so
`romfs1.mts --rename` gives the entry a name ending `.EXE` (the same
length as the old one — a ROM directory has no slack). That is how
`tests/integration/test-er1-romdump.sh` runs the R1 dumper on an
emulated Series 5, over `Z:\System\Samples\Welcome to Series 5`, and
gets the machine's whole 6 MB ROM onto its own RAM disk.

```sh
bash tests/integration/test-er1-romdump.sh
bash tests/unit/run-er1-romdump-test.sh
node --experimental-strip-types tests/unit/er1-romdump-image.mts
node --experimental-strip-types tools/e32/er1check.mts ordinals 'roms/S5_v1.00(113)_eng.bin'
node --experimental-strip-types tools/e32/armv3check.mts --e32 tools/romdump-er1/ROMDUMP.EXE
```

Note for anything else built for these machines: a Series 5 is **ARMv3**.
It has no BX and no 64-bit multiply, and it does not fault on them — it
executes something else instead, so a compiler's division by a constant
silently returns nonsense. `armv3check.mts` is in `build.sh` for that
reason.
