# Tests

All emulator validation lives here. Build the native harness first
(`bash harness/build.sh`); most tests drive `harness/run` directly.

## Layout

| Path | What it covers |
|------|----------------|
| `boot/test-boot.sh` | Per-device boot validation against committed golden screenshots — the primary regression gate (run in CI). |
| `integration/` | Feature/end-to-end tests: FAT16, FEFS, SSD pack writes, infrared, remote-link (PLP), SIBO app launch, Series 3c apps, netBook CF bootloader, netpad MMC card, app-library cold delivery, EPOC app-folder install, netpad app-library SIS install, Conan ROM dump (including that it resumes), Geofox mouse pad, Geofox, Siena and EPOC R5 ROM language, LCD backlight. |
| `stress/` | Touch / event-binding reliability stress runs for the Series 7 and netBook, plus the netpad's end-to-end stylus (`netpad_touch.sh`), key-delivery / OS-timer-rate (`netpad_keys.sh`) and screen-orientation (`netpad_orientation.sh`) tests. |
| `unit/` | Standalone C++ unit tests (`ssd_smoke`, `cf_write_test`, `mmc_card_test`, `machine_id_test`, `v30_smoke`); all but `v30_smoke` are built by `harness/build.sh`. Plus two node tests (no harness needed): `device-lists-sync.mts`, the two device lists outside the C++ registry — track.php's analytics allowlist and the id → display-name map — checked against it, because both fail silently; and `e32-format.mts`, which holds `tools/e32`'s EPOC32 image packer against a binary Psion's own tools produced. |
| `golden/` | Committed reference LCD screenshots (one PGM per device); the boot gate diffs against these. |
| `fixtures/` | Test inputs (SSD images, CF image, EPOC document samples for the converter tests). |
| `devices.txt`, `devices-ssd.txt` | Device boot manifests consumed by `test-boot.sh`. |

Transient output (`actual/`, `logs/`, `results/`, `sweeps/`, `cards/`) is
regenerated per run and gitignored.

## Common runs

```sh
bash harness/build.sh                         # build harness/run + unit tests
bash tests/boot/test-boot.sh --all            # all devices (regression gate)
bash tests/boot/test-boot.sh revo             # one device
bash tests/boot/test-boot.sh --all --update-golden   # refresh golden PGMs
bash tests/integration/test-remote-link.sh    # PLP remote-link end-to-end
bash tests/integration/test-epocdir-install.sh  # app-folder install → 5mx, then launch it
bash tests/integration/test-epocdir-install.sh --device netpad  # same, over the netpad's cable
bash tests/integration/test-netpad-mmc.sh     # netpad MMC card, read + write
bash tests/integration/test-netpad-app-install.sh  # app library → netpad: install a .SIS, see it in Extras
bash tests/integration/test-netpad-app-install.sh epocgames/fred  # …and a 3-Lib EPOC app on it
bash tests/integration/test-ssd-write.sh      # SIBO SSD pack format + write + read back
bash tests/integration/test-conan-romdump.sh  # tools/romdump: dump the Conan's ROM on the machine
bash tests/integration/test-lemmings-series5.sh  # tools/lemmings: the game on a Series 5, level 1 in play
bash tests/integration/test-lemmings-netpad.sh   # …and on a netpad from an MMC card, in colour
bash tools/lemmings/host/test.sh              # the game's rules on a PC: all twelve levels solved, frame budget
bash tests/integration/test-geofox-mouse.sh   # Geofox mouse pad: pointer tracking, tap-to-open, drag-select
bash tests/integration/test-geofox-language.sh  # Geofox ROM language: UK vs USA resources, locale + keyboard DLL
bash tests/integration/test-siena-language.sh   # Siena ROM locale: UK / USA / Swedish / Spanish via the port C straps
bash tests/integration/test-epoc-language.sh    # Series 5 / 5mx / 5mx Pro / MC218 / Revo / Conan ROM locale: PROM index or ROM directory
bash tests/integration/test-backlight.sh      # LCD backlight: every backlit machine, switched by its own key
bash tests/integration/test-backlight.sh --slow  # …and the netBook, booted from its OS card
bash tests/stress/netpad_orientation.sh       # netpad "Switch orientation"
tests/unit/mmc_card_test                      # MMC-over-SPI card protocol
tests/unit/machine_id_test                    # EPOC Unique id through the ETNA identity PROM
node --experimental-strip-types tests/unit/device-lists-sync.mts   # device lists vs. the registry
node --experimental-strip-types tests/unit/e32-format.mts          # E32Image packer vs the ROM's own binary
```

## Frontend unit tests

The TypeScript suites stay colocated with the code under
`frontend/src/lib/__tests__/` (relative imports + Node's native type
stripping) and run from `frontend/`:

```sh
cd frontend
npm run test:plp  test:irda  test:modem  test:modem-e2e \
        test:converters  test:printer  test:wprt  test:state  test:applib \
        test:rotation  test:sizing  test:machine-id  test:backlight
```

Real-browser suites (Playwright; `PW_EXE` points at a Chromium if it isn't in
the default location):

```sh
cd frontend
node test/keyboard-focus/run.mjs      # host UI text fields vs. emulator key capture
node test/plp-browser/run.mjs         # Remote Link transfer in a real worker
```

`frontend/src/lib/__tests__/_plp_audit.mts <device>` is the end-to-end Remote
Link audit: the real `PlpClient` against the real ROM over the native harness
(connect, drives, directory, 40 KB upload + byte-exact download, delete, then a
second session after a cable re-plug or a parked-session adoption). It needs the
harness built and the ROMs present, and runs in real time (20-130 s per device);
its header lists the options. The `_plp_*_repro.mts` files beside it are the
single-purpose probes it grew out of.
