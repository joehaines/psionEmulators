# Tests

All emulator validation lives here. Build the native harness first
(`bash harness/build.sh`); most tests drive `harness/run` directly.

## Layout

| Path | What it covers |
|------|----------------|
| `boot/test-boot.sh` | Per-device boot validation against committed golden screenshots — the primary regression gate (run in CI). |
| `integration/` | Feature/end-to-end tests: FAT16, FEFS, SSD pack writes, infrared, remote-link (PLP), SIBO app launch, Series 3c apps, netBook CF bootloader, netpad MMC card, app-library cold delivery, EPOC app-folder install, netpad app-library SIS install. |
| `stress/` | Touch / event-binding reliability stress runs for the Series 7 and netBook, plus the netpad's end-to-end stylus (`netpad_touch.sh`), key-delivery / OS-timer-rate (`netpad_keys.sh`) and screen-orientation (`netpad_orientation.sh`) tests. |
| `unit/` | Standalone C++ unit tests (`ssd_smoke`, `cf_write_test`, `mmc_card_test`, `machine_id_test`, `v30_smoke`); all but `v30_smoke` are built by `harness/build.sh`. |
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
bash tests/stress/netpad_orientation.sh       # netpad "Switch orientation"
tests/unit/mmc_card_test                      # MMC-over-SPI card protocol
tests/unit/machine_id_test                    # EPOC Unique id through the ETNA identity PROM
```

## Frontend unit tests

The TypeScript suites stay colocated with the code under
`frontend/src/lib/__tests__/` (relative imports + Node's native type
stripping) and run from `frontend/`:

```sh
cd frontend
npm run test:plp  test:irda  test:modem  test:modem-e2e \
        test:converters  test:printer  test:wprt  test:state  test:applib \
        test:rotation  test:sizing  test:machine-id
```

Real-browser suites (Playwright; `PW_EXE` points at a Chromium if it isn't in
the default location):

```sh
cd frontend
node test/keyboard-focus/run.mjs      # host UI text fields vs. emulator key capture
node test/plp-browser/run.mjs         # Remote Link transfer in a real worker
```
