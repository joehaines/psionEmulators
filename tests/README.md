# Tests

All emulator validation lives here. Build the native harness first
(`bash harness/build.sh`); most tests drive `harness/run` directly.

## Layout

| Path | What it covers |
|------|----------------|
| `boot/test-boot.sh` | Per-device boot validation against committed golden screenshots — the primary regression gate (run in CI). |
| `integration/` | Feature/end-to-end tests: FAT16, FEFS, infrared, remote-link (PLP), SIBO app launch, Series 3c apps, netBook CF bootloader, app-library cold delivery. |
| `stress/` | Touch / event-binding reliability stress runs for the Series 7 and netBook. |
| `unit/` | Standalone C++ unit tests (`ssd_smoke`, `cf_write_test`, `v30_smoke`); `ssd_smoke` and `cf_write_test` are built by `harness/build.sh`. |
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
```

## Frontend unit tests

The TypeScript suites stay colocated with the code under
`frontend/src/lib/__tests__/` (relative imports + Node's native type
stripping) and run from `frontend/`:

```sh
cd frontend
npm run test:plp  test:irda  test:modem  test:modem-e2e \
        test:converters  test:printer  test:wprt  test:state  test:applib
```
