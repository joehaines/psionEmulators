# Psion Emulator

A web-based emulator for Psion handheld computers, spanning three CPU
architectures and fifteen years of devices. Play any of them in the browser at
**[joehaines.com/psion](https://joehaines.com/psion)**.

- **Hitachi HD6303X** — the 1986 [Psion Organiser II](https://en.wikipedia.org/wiki/Psion_Organiser).
- **NEC V30 / V30H** (with Psion's ASIC1 / ASIC2 / ASIC9) — the SIBO family:
  Series 3, 3a, 3c, 3mx, Siena, Workabout, MC400/MC218.
- **ARM710 (CL-PS7110 / Windermere) and StrongARM SA-1100** — the ARM-era
  machines: Series 5, 5mx, Revo, Series 7, netBook.

Every CPU core, peripheral and OS path is emulated natively in C++, compiled to
WebAssembly via Emscripten and driven from a React + TypeScript + Vite frontend.

## Supported devices

| Device | Year | CPU | Status | CF | SSD | Speaker | Mic | Touch | IR | Link |
|--------|------|-----|--------|----|-----|---------|-----|-------|----|------|
| Organiser II (LZ/LZ64) | 1986 | HD6303X | ✅ | — | — | ❌ | — | — | — | ❌ |
| MC400 | 1989 | Intel 80C86A | ✅ | — | ✅ ×4 | ✅ | — | — | — | ❌ |
| Series 3 | 1991 | NEC V30 | ✅ | — | ✅ ×2 | ✅ | — | — | — | ❌ |
| Acorn Pocket Book | 1992 | NEC V30 | ✅ | — | ✅ ×2 | ✅ | — | — | — | ❌ |
| Series 3a | 1993 | NEC V30H | ✅ | — | ✅ ×2 | ✅ | ✅ | — | — | — |
| Workabout | 1995 | NEC V30H | ✅ | — | ✅ ×2 | ✅ | ✅ | — | — | ❌ |
| Series 3c | 1996 | NEC V30H | ✅ | — | ✅ ×2 | ✅ | ✅ | — | ✅ | ✅ |
| Acorn Pocket Book II | 1996 | NEC V30H | ✅ | — | ✅ ×2 | ✅ | ✅ | — | — | ❌ |
| Siena | 1996 | NEC V30H | ✅ | — | ✅ ×1 | ✅ | ✅ | — | ✅ | ✅ |
| Series 5 | 1997 | ARM710 (CL-PS7110) | ✅ | ✅ | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| Series 3mx | 1998 | NEC V30H | ✅ | — | ✅ ×2 | ✅ | ✅ | — | ✅ | ✅ |
| Osaris | 1998 | ARM710 (CL-PS7111) | ✅ | ✅ | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| WorkaboutMX | 1998 | NEC V30MX | ✅ | — | ✅ ×2 | ✅ | ✅ | — | ❌ | ✅ |
| Series 5mx | 1999 | ARM710 (Windermere) | ✅ | ✅ | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| Ericsson MC218 | 1999 | ARM710 (Windermere) | ✅ | ✅ | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| Revo | 1999 | ARM710 (Windermere) | ✅ | — | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| Series 7 | 1999 | StrongARM SA-1100 | ✅ B| ✅ | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| netBook | 1999 | StrongARM SA-1100 | ✅ | ✅ | — | ✅ | ✅ | ✅ | ✅ | ✅ |
| Series 5mx Pro | 2000 | ARM710 (Windermere) | ✅ | ✅ | — | ✅ | ✅ | ✅ | ✅ | ✅ |

✅ Supported · ⚠️ Partial / not fully booted · ❌ Not yet emulated · — Hardware not present.
**Link** = Remote Link / PsiWin file access over the serial cable.

### Feature notes

- **Series 7 / netBook** boot to a real EPOC desktop. The netBook loads its OS
  the faithful way — the bootloader reads `D:\OS.IMG` off the FAT16 CompactFlash
  card through its own driver. CompactFlash, microphone capture/playback and
  Remote Link all run through the machines' own kernel driver stacks.
- **Remote Link** (serial cable) is emulated via a host serial bridge + PLP /
  PsiWin client (link → NCP → RFSV drive/dir/file), verified end-to-end on the
  Windermere machines and the SA-1100 Series 7 / netBook.
- **Infrared send** beams a host file into the device's inbox over an emulated
  IrDA stack (SIR → IrLAP → IrLMP/IAS → Tiny TP → EPOC Eikon-IR), verified on all
  EPOC32 machines. The EPOC16 "Psion IRLink" SIBO machines are not yet supported.
- **Printing** — the Printer dialog captures jobs either *via PC* (PsiWin print
  spooler → positioned PDF, EPOC32) or by *serial capture* (`.txt` / Courier PDF
  / raw `.prn`). Run `npm run test:wprt` / `test:printer`.
- **Audio** — SIBO1 (Series 3 / MC400 / Pocket Book) is a piezo buzzer; SIBO2
  (3a onward) adds an 8-bit PCM codec with mic input.
- **MC400 ROMs** — the MC400 defaults to the v2.60F boot ROM; a discreet
  header link swaps to the older v1.26F ROM (and back) without leaving a
  second entry in the device picker.

Every device has a committed golden screenshot in `tests/golden/`; run
`bash tests/boot/test-boot.sh --all` to re-verify locally.

## Project structure

```
core/       C++ emulation engine (CPU cores, ASICs, device models)
wasm/       Emscripten WASM layer with Embind exports
frontend/   React + TypeScript web app (Vite)
applib/     Source for the in-app software library (see applib/README.md)
scripts/    Build / asset scripts
harness/    Native (non-WASM) host driver for the core
tests/      Boot validation, integration, unit tests (see tests/README.md)
roms/       ROM images (property of their owners; see NOTICE)
.github/    GitHub Actions CI/CD
```

## Building

Requires the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
(activated) and Node.js 20+.

```sh
source <path-to-emsdk>/emsdk_env.sh
bash scripts/build-all.sh        # full build -> dist/
bash scripts/build-wasm.sh       # WASM only
bash scripts/build-frontend.sh   # frontend only (needs WASM built)
cd frontend && npm run dev        # local dev server
```

App icons / share images are generated from `favicon.svg` and committed; only
re-run `cd frontend && npm run icons` if the logo changes.

## App library

The site ships a browsable app library (`#/apps`) built from Steve Litchfield's
3-Lib shareware collection, preserved in `applib/`.
`scripts/build-app-library.mts` converts the catalogue into `dist/apps`
(`manifest.json` + one zip per app, with icons extracted from EPOC `.AIF`
resources). CI regenerates it on every deploy; nothing under `dist/` is
committed. Each app can be downloaded or tried in one click — delivered onto the
right device the way the machine actually had it (a Remote Link `.SIS` upload on
EPOC32, or a FEFS SSD pack on the SIBO machines). Run `npm run test:applib`.

The build also emits a PWA service worker (`vite-plugin-pwa`): the app shell is
precached, ROMs and skins are runtime-cached, and updates activate on the next
visit.

## Boot-validation harness

`harness/run.cpp` (built via `bash harness/build.sh`) loads any ROM and emulates
it without WebAssembly. `tests/boot/test-boot.sh` turns it into a pass/fail gate
that diffs the end-of-boot LCD against the committed `tests/golden/<id>.pgm`:

```sh
bash harness/build.sh
bash tests/boot/test-boot.sh revo                  # one device
bash tests/boot/test-boot.sh --all                 # everything in tests/devices.txt
bash tests/boot/test-boot.sh --all --update-golden # refresh committed PGMs
```

A device passes when, after its boot window: LCD variance ≥ threshold (blank
screens fail), unique sampled PCs ≥ threshold (tight wait loops fail), and no
aborts / undefined instructions were logged. CI runs the full suite before the
slow WASM build so regressions surface fast. See `tests/README.md`.

## Adding a new device

The cleanest template is the Revo (`core/revo.{h,cpp}`):

1. If the device reuses an existing CPU + peripheral core, subclass it in
   `core/<device>.{h,cpp}` and override only what differs (LCD size, digitiser,
   name).
2. Add the source filename to both `harness/build.sh` and `scripts/build-wasm.sh`.
3. Add a factory and `DeviceProfile` entry in `core/device_registry.cpp`.
4. Drop a skin SVG into `frontend/public/skins/` (and a `SKIN_LAYOUTS` entry in
   `frontend/src/components/EmulatorView.tsx` if the aspect ratio differs).
5. Add a line to `tests/devices.txt` and run
   `bash tests/boot/test-boot.sh <id> --update-golden`; commit the golden PGM.

A fundamentally different CPU (SA-1100, V30/V30H, HD6303X) needs a new core
first. The existing cores — `core/arm710.*`, `core/sa1100.*`, `core/v30.*`
(+ `v30_ops_*`), `core/hd6303.*` — each expose a small synchronous bus interface
the device driver implements.

## Deployment

Automated via GitHub Actions on push to `main`. Configure the repository secrets
`FTP_HOST`, `FTP_USER`, `FTP_PASS`, `FTP_SERVER_DIR`. For local config, copy
`.env.example` to `.env`.

## License

Copyright (c) 2024–2026 Joe Haines. Original work is released under the terms in
[`LICENSE`](LICENSE) (reuse is permitted with attribution; self-hosted
deployments must link back to https://joehaines.com/psion). Portions of the
emulation core derive from [WindEmu](https://github.com/Treeki/WindEmu)
(MPL-2.0) and MAME (BSD-3-Clause), and ROM images and the bundled software
library belong to their respective owners — see [`NOTICE.md`](NOTICE.md).
