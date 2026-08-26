# Application benchmarks

A boot is a bad benchmark for anything that has to amortise a cost: the guest
runs each stretch of initialisation once and then idles. These drive the guest's
own UI instead, so the timed window is an application actually doing something.

Run one with `scripts/wasm-bench.mjs --script`:

    PSION_RTC_SEED=0 node scripts/wasm-bench.mjs frontend/public/psion.js \
        roms/netBook_BL_v011_eng.bin netbook 0 \
        --script scripts/bench/netbook-sheet.json --shot-dir /tmp

`PSION_RTC_SEED=0` pins the clock, without which the on-screen time makes the
framebuffer fingerprint differ between runs for no interesting reason.

Paths inside a script are relative to the working directory, so run them from
the repository root. `--shot-dir` is where `{ "shot": ... }` writes its PGMs;
looking at those is how you check a launch sequence still lands where it did.

## netbook-sheet.json

Boots the netBook bootloader, hands it the stock OS image on a CF card, waits
for the desktop, opens Sheet from it (the `S` key selects it, Enter opens it),
and then pages through the grid forty times with the timer running.

This is the workload the emulator is actually judged on, and it is nothing like
a boot: **4.8 simulated seconds take about 5.5 wall seconds — 0.87x real time**,
which is the "nearly unusable" the performance work started from. A boot, by
contrast, runs at 3-13x real time because it is mostly idle.

The framebuffer fingerprint at the end (`FRAMEBUFFER: ... fnv=`) is stable
across engines, so it doubles as a correctness check: any two configurations
that reach the same screen agree.
