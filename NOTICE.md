# NOTICE — third-party components

This repository is a combination of original work and third-party works under
several licenses. The project as a whole is offered under the terms in `LICENSE`,
**except** for the components listed below, which remain governed by their own
licenses. When you reuse, modify, or redistribute any of these components, you
must honour the corresponding license.

## Emulator core — WindEmu (Mozilla Public License 2.0)

Part of the ARM-based emulation core derives from **WindEmu** by Ash Wolf
("Treeki") — https://github.com/Treeki/WindEmu. The Psion-specific code is
copyright (c) 2019 Ash Wolf; the ARM instruction decoder/emitter is copyright
(c) 2013-2014 Jeffrey Pfau. Both are licensed under the Mozilla Public License,
v. 2.0 (http://mozilla.org/MPL/2.0/).

- Used essentially verbatim (MPL-2.0, original authors only): the ARM
  decoder/emitter/ISA set (`core/decoder*.{c,h}`, `core/emitter-*.h`,
  `core/isa-*.h`), `core/common.h`, `core/macros.h`, `core/clps7111_defs.h`.
- Derived and substantially modified for this project (MPL-2.0, © Ash Wolf with
  modifications © Joe Haines): `core/arm710.cpp`, `core/windermere.{cpp,h}`,
  `core/clps7111.{cpp,h}`, `core/clps7600.{cpp,h}`, `core/etna.{cpp,h}`,
  `core/hardware.h`.

Each such file carries its own MPL-2.0 header; consult the file for the precise
attribution. Per MPL-2.0, these files remain under the MPL regardless of the
terms in `LICENSE`.

## Device models — MAME (BSD-3-Clause)

Several CPU and ASIC models are ports of code from the **MAME** project and are
distributed under the BSD-3-Clause license, retaining their original
copyright-holders (incl. Nigel Barnes, Bryan McPhail, Aaron Giles, Sandro Ronco),
with adaptation by this project. Files carry a `// license:BSD-3-Clause` header
naming the upstream authors — for example the NEC V30 core (`core/v30*`), the
Psion ASICs (`core/psion_asic1/2/3/9.*`), the HD6303 core (`core/hd6303.*`), and
the SIBO / StrongARM device models. See each file's header for details.

## Reference material (development only, not distributed)

The `reference/` directory contains datasheets, scanned manuals, decompiled
ROMs, and third-party source (incl. Symbian/EPOC and NetBSD code) used only for
reverse-engineering during development. It is excluded from the published
repository and is not required to build or run the application.

## App library — 3-Lib collection (`applib/`)

`applib/` preserves Steve Litchfield's **3-Lib** shareware/freeware library,
used as the source for the site's app catalogue. The software is shareware or
freeware by its original authors, preserved for historical interest. If you are
an author and would like an item amended or removed, please open an issue.

## ROM images (`roms/`)

The Psion/EPOC ROM images in `roms/` are the property of their respective
copyright owners and are included to allow the emulator to run. They are not
covered by `LICENSE`.
