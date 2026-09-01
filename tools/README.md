# tools/

Programs that run **on** the emulated machines, and the small toolchain
that builds and installs them. Nothing here is part of the emulator or the
web app; `scripts/` holds the build and asset tooling for those.

```
e32/       EPOC32 binary tooling
  e32link.mts   flat ARM binary (clang + ld.lld) -> E32Image .exe
  romfs.mts     list / extract / replace files inside an EPOC32 ROM image
romdump/   ROMDUMP.EXE — dumps an EPOC R5 Unicode machine's ROM to its own
           disk, with a console UI; HOW-TO-USE.md is the guide for
           someone holding the machine
```

## Building an EPOC binary here

There is no Symbian SDK and `petran` does not run on Linux, so
`e32link.mts` writes the E32Image container itself: header, import address
table, import section and relocations, all of it checked against a real
ER5u binary that ships in the Conan ROM. See its file header for the
layout and [`docs/conan-rom-dumping.md`](../docs/conan-rom-dumping.md) for
how the format and the DLL ordinals were established.

```sh
bash tools/romdump/build.sh          # clang + ld.lld + e32link -> ROMDUMP.EXE
```

The build needs clang with the ARM target, `ld.lld`, `llvm-objcopy`,
`llvm-nm` and node 22+. The packed binary is committed next to its source
so the tests and the docs do not depend on that toolchain being present;
`tools/romdump/build/` is not.

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
