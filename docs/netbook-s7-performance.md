# Why the netBook and Series 7 are slow

Measured on this branch, native build (`bash harness/build.sh`, `-O3 -flto
-DPSION_PROFILE_CYCLES`, g++ 13.3, x86-64). Every number below is from a run
you can repeat; the commands are in each section. No emscripten was available
in the measuring environment, so the WASM figures are reasoned from the native
ones and from the build flags, and are marked as such.

---

## 1. The short version

Two things multiply, and neither is a bug:

1. **The SA-1100 machines are six times the machine every other device is.**
   The Series 7 and netBook run at 221.184 MHz (`core/sa1100_defs.h`). The
   Series 5mx / Revo family runs at 36.864 MHz (`core/wind_defs.h`) — exactly
   1/6 — and the Series 5 at 18.432 MHz. The interpreter's throughput is a
   property of the *interpreter*, not of the guest clock, so the same engine
   lands at roughly 1:1 on a Windermere device and roughly 1:6 on an SA-1100
   one.

2. **The interpreter costs ~500 host instructions per emulated ARM
   instruction.** That is about an order of magnitude more than a tight ARM
   interpreter costs, so there is no headroom to absorb (1).

The largest single fixable line item, for reference, is neither of those: 9.1%
of all host instructions are spent evaluating `PSION_*` diagnostic gates that
are never set, and another 1.6% maintaining a PC-history ring nothing reads.

Together the two factors put a CPU-bound app on the Series 7 at
**0.172× real time** and on the netBook at **0.117× real time** — *natively*,
before WASM and before the browser's frame pacing take their cut. In a browser
the delivered figure will be lower again.

Everything else in this document is either evidence for those two statements
or a list of the things that are costing multiples on top of them.

> **Since this was written**, stages 0–2 of the execution-engine work have
> landed and are on by default, measured on the **WebAssembly build** with the
> pinned Emscripten 6.0.1 that CI actually ships:
>
> | | before | after | |
> | --- | --- | --- | --- |
> | Series 7 core, 8 sim s | 13.62 s | 8.52 s | **1.60×** |
> | LCD blit, 640×480 | 1.569 ms/frame | 0.212 ms/frame | **7.4×** |
> | worker duty cycle (CPU-bound) | 75.5% | 99.9% | **1.32×** |
>
> Since then the Series 7 core has taken a further **+13.2%** in WASM (6.93 →
> 6.02 s over the same 8 sim seconds) from deleting a redundant per-instruction
> `decodeKind()`, and the burst engine now retires 14.5 instructions per entry
> instead of 7.2 after the fetch path was taught to fill a cold permission byte
> rather than give up on the page.
>
> **The interpreter is now at its practical floor.** Nine further contained
> optimisations aimed at the memory path and the burst engine have been built
> and measured; eight came back at zero or negative, and the ninth was a
> deletion. The remaining gap is ~2×: **~110 MHz of emulated SA-1100 clock
> against a 221 MHz part, so roughly 0.5× real time while the guest is busy.**
> Boot runs at 1.6× real time only because it is 73% idle. Closing that 2×
> needs instructions decoded once and re-executed from a block cache — Stage 3
> in `docs/jit-engine-scope.md` — not more interpreter tuning.
>
> Two cautions for anyone reading the native numbers below. Native
> over-predicts WASM — `-flto` is +13% native and **0%** in WASM. And three
> memory-path changes that the profile said should pay measured neutral or
> worse. Two more were first reported at three times their real size because
> the A/B loaded a stale `psion.wasm`; see `docs/jit-engine-scope.md`.
>
> **That "practical floor" was wrong, and by a factor of two on the netBook.**
> The interpreter was not the limit; the batch loop around it was. See §8.
>
> Profiling the application benchmark then found a real one: LDM/STM was paying
> a virtual call per register, and giving it one host pointer per block is worth
> **6–9%** of the netBook Sheet workload. The same section records what did NOT
> work — fronting the per-batch assist hooks with their own guards, which reads
> like an obvious win and measures at zero. See §17.

---

## 2. Real-time factor, as measured

```
./harness/run "roms/series7_v1.05(254)_b756_eng.bin" \
    --boot-seconds 20 --skip-card --quiet-logs
./harness/run roms/netBook_BL_v011_eng.bin --boot-seconds 3 \
    --card-path roms/netbook_os.img --post-attach-seconds 20 --quiet-logs
```

| device | executed cycles / wall sec | guest clock | busy : real-time |
| --- | --- | --- | --- |
| Series 7 (desktop) | 38.1 MHz | 221.2 MHz | **0.172×** |
| netBook (full OS off card) | 25.9 MHz | 221.2 MHz | **0.117×** |

`busy_realtime_ratio` counts only cycles the CPU actually *ticked*; idle time
fast-forwarded through WFI is excluded. It is therefore exactly the number
that bounds an app which never idles — a game, a redraw storm, a recalc.

An idle desktop looks fine (both devices track real time while ~70–85% of
sim cycles are being skipped through WFI). The moment an app stops idling,
the ratio above is what the user gets. That is why it presents as "fine until
you open something".

### The same engine on the other devices

```
gdb -batch -ex 'break exit' -ex run \
    -ex 'printf "%llu\n", *(unsigned long long*)&g_armInsn' \
    --args ./harness/run <rom> --boot-seconds 10 --skip-card --quiet-logs
```

| device | emulated ARM instructions / wall sec | needed for 1:1 (clock ÷ CPI 2.34) |
| --- | --- | --- |
| Series 7 (SA-1100) | 16.3 M/s | **94.5 M/s** |
| Revo (Windermere) | 11.9 M/s | 15.8 M/s |
| Series 5 (CLPS7111) | 3.4 M/s | 7.9 M/s |

These rates are measured over a whole boot, idle included, but idle costs
almost no wall time: on the Series 7 the rate computed this way (16.3 M/s)
and the rate computed from busy cycles alone (38.1 MHz ÷ CPI 2.34 =
16.3 M/s) agree exactly, so the fast-forward through WFI is effectively free
and these are peak interpreter rates.

The SA-1100 path is in fact the *fastest* per instruction of the three — it
has the fast-TLB bridge the others do not. It is 5.8× short of its target
purely because its target is 6× higher.

(The Series 5 line is a separate problem: 3.4 M/s against a 7.9 M/s target
means the CLPS7111 device layer costs roughly 4× per instruction what the
SA-1100 one does. Out of scope here, but worth its own look.)

---

## 3. Where the host instructions actually go

```
valgrind --tool=callgrind ./harness/run \
    "roms/series7_v1.05(254)_b756_eng.bin" --boot-seconds 6 --skip-card --quiet-logs
```

Totals for 6 simulated seconds of Series 7 desktop:

* **87,031,123,063** host instructions
* **174,863,894** calls to `ARM710::tick()` — i.e. emulated instructions attempted
* **124,485,954** calls to `ARM710::executeInstruction()` — instructions actually run
* 407,669,482 emulated SA-1100 cycles (CPI 2.33)

So:

* **497 host instructions per emulated ARM instruction**
* **699 host instructions per instruction that actually executed**

For comparison, a competent ARM interpreter typically sits at 30–80. This engine is
roughly 10× off that, and the gap is not in the instruction semantics.

### Self cost by function

| function | self | per `tick()` |
| --- | --- | --- |
| `ARM710::tick()` | 26.25% | 131 |
| `SA1100Bridge::fetchVirtual()` | 18.18% | 90 |
| `ARM710::executeInstruction()` | 12.41% | 87 (per executed insn) |
| `SA1100::Emulator::stepCpu()` | 6.23% | 31 |
| `SA1100::Emulator::executeUntil()` | 6.07% | 30 |
| `SA1100Bridge::readVirtual()` | 5.63% | |
| `ARM710::execDataProcessing()` | 5.05% | |
| `ARM710::execSingleDataTransfer()` | 4.00% | |
| `SA1100Bridge::translateFast()` | 3.36% | |
| `SA1100Bridge::checkPermsfast()` | 3.17% | |
| `ARM710::execBlockDataTransfer()` | 2.63% | |
| `ARM710::sampleAndDispatchPendingExceptions()` | 2.61% | 13 |
| `SA1100Bridge::writeVirtual()` | 2.05% | |
| `ARM710::translateAddressUsingTlb()` | 1.18% | |

The handlers that implement what the guest asked for — `execDataProcessing`,
`execSingleDataTransfer`, `execBlockDataTransfer` — add up to **11.7%**.
Roughly seven eighths of the host work is the machinery around them: fetch,
dispatch, the prefetch-pipeline model, the per-instruction diagnostic gates,
and the stepping seams.

That is the answer to "why didn't the optimisation rounds help": they were
aimed at the eighth.

### Line level

Same workload, 2 simulated seconds, built with `-g` so callgrind can attribute
lines (47,828,620,998 Ir over 96,273,922 ticks — 497 host instructions per
emulated instruction, matching the 6-second run exactly).

**Roughly a tenth of all runtime is diagnostics that are switched off.**

| what | Ir | share |
| --- | --- | --- |
| `PSION_ENV_BOOL` / `PSION_ENV_CSTR` gates (guard test + branch) | 4,364,810,889 | **9.13%** |
| `pcHistory[]` ring write + index update | 681,960,440 | 1.43% |
| `g_armInsn++` | 96,273,922 | 0.20% |

The env macros expand to a function-local `static const` initialised from
`getenv()`. Each use is a guard-byte load, a test and a branch — three host
instructions — and there are 56 of them on the hot path, evaluated on every
instruction, for flags that cannot change after startup. They are cheap
individually and enormous in aggregate.

**Entering and leaving `tick()` costs more than executing most instructions.**

| line | Ir | per call |
| --- | --- | --- |
| `uint32_t ARM710::tick() {` (prologue) | 1,251,560,986 | 13.0 |
| its closing `}` (epilogue) | 1,155,287,064 | 12.0 |
| `SA1100Bridge::fetchVirtual(uint32_t) {` (prologue) | 1,251,560,986 | 13.0 |
| `ARM710::executeInstruction(uint32_t) {` (prologue) | 818,352,528 | 12.0 |

`tick()` is a 2,169-line function, so the compiler spills and restores a large
register set on every one of the 96 M calls. Prologue plus epilogue for the
three hot functions is ~9% of the program before any work happens.

**`fetchVirtual()`'s "fast" path is 65–90 instructions, and only ~2 of them are
the fetch.** Per call, on a hit:

```
13   function prologue
 6   two static getenv guards (kNoMemFast, kMemFastCheck)
 4   if ((cp15_control & 1) && !kNoMemFast)
 9   MRU compare chain (addrMask / addr / hostReadPtr)
 9   sub-page index for the permission byte
 4   isPrivileged()
 9   the two permission-bit tests
 2   *** the actual instruction load ***
 4   decodeCache_ / decodeFast_ gates (both false in the shipping build)
 3   construct and return pair<optional<uint32_t>, MMUFault>
12   function epilogue
```

That is the shape of the whole engine in miniature: the work is 2 instructions
and the frame around it is 70.

---

## 4. The five specific things costing the most

### 4.1 The fast paths that exist are switched off in the shipping build

`ARM710::initFastPathGate()` (`core/arm710.cpp:48`) reads three environment
variables and leaves `decodeFast_`, `decodeCache_` and `pageLoop_` false
unless they are set. `wasm/main.cpp` exposes `setEnvVar()` for exactly this,
but **nothing in `frontend/` ever calls it** — the only reference is the type
declaration in `frontend/src/types/emulator.ts:175`. So in the browser all
three are off, and every instruction takes the full path.

`decodeFast_` is off deliberately (the header at `core/arm710.h:675` says it
is opt-in "while the Pac-Man KERN-EXEC 3 regression is investigated"), which
is a correctness call, not an oversight. But it does mean the work is sitting
in the tree unshipped.

Measured, on the Series 7, with everything else default:

| configuration | exec MHz | vs default |
| --- | --- | --- |
| default (what ships) | 38.1 | — |
| `PSION_DECODE_FASTPATH=1` | 42.0 | +10% |
| `PSION_DECODE_FASTPATH=1 PSION_DECODE_CACHE=1` | 40.2 | +5.5% |
| `PSION_NO_MEM_FASTPATH=1` | 29.5 | −23% |
| `PSION_DEADLINE_BATCH=0` | 31.9 | −16% |

Turning on everything that can be turned on buys about 10%. It does not move
0.172× anywhere near 1×, because — see §3 — the dispatch chain was never where
the time was.

### 4.2 `PSION_DECODE_PAGELOOP` cannot be enabled at all

`core/arm710.cpp:63`:

```c
else if (std::strncmp(*ep, "PSION_DECODE_PAGELOOP=", 21) == 0) pageLoopOptIn = *ep + 21;
```

`"PSION_DECODE_PAGELOOP="` is 22 characters. The compare stops one short (so
it matches the name without the `=`), and the offset stops one short too — so
`pageLoopOptIn` points at `"=1"`, and the `pageLoopOptIn[0] == '1'` test on
line 71 is comparing against `'='`. `pageLoop_` is therefore always false, and
`ARM710::tickPageLoop()` is dead code.

The sibling flags are correct (`FASTPATH` 22/22, `CACHE` 19/19,
`CACHE_CHECK` 25/25) — it is only this one. Confirmed by measurement: adding
`PSION_DECODE_PAGELOOP=1` to a run changes throughput by less than noise
(40.2 → 40.5 MHz).

Because `decodeCache_` is also derived from `pageLoop_`, the Tier-2 page loop
has never run via its documented switch.

### 4.3 The WASM build has no LTO — and needs it more than the native one does

`harness/build.sh` compiles with `-flto` and its own comment records what that
is worth: *"Measured +13% Series 7 / +3% netBook native throughput"*.
`scripts/build-wasm.sh` does not pass `-flto` at all.

In WASM the loss is worse in kind than the native 13%, because the hot path is
built out of cross-translation-unit *virtual* calls:

* `ARM710::tick()` (arm710.cpp) → `fetchVirtual()` (sa1100_cpu.cpp) — once per
  instruction, 174.9 M times in the profile above
* `execSingleDataTransfer()` / `execBlockDataTransfer()` → `readVirtual()` /
  `writeVirtual()` (sa1100_cpu.cpp) — 51.9 M and 19.0 M times

Without LTO, emcc cannot inline or devirtualise any of these, so each becomes
a `call_indirect` with a runtime signature check. Native `-flto` collapses most
of them.

On top of that, all of these return
`std::pair<std::optional<uint32_t>, MMUFault>` **by value**. On wasm32 that is
an `sret` return: a stack slot written by the callee and read back by the
caller, once per instruction fetch and once per load/store. `fetchVirtual`'s
90 host instructions of self cost — for what is, on a hit, a masked compare, a
permission-bit test and a load — is largely this.

### 4.4 The netBook is structurally excluded from the batching win

`core/sa1100.cpp:21077`:

```c
const bool hooksActive =
    recEdge_ || recDmaEng_ || recRateFix_
    || isNetBookRom_ || isNetBookBootloader_
    || (isSeries7Rom_ && cfCard.inserted());
```

`hooksActive` is true for **every** netBook run, always, and for any Series 7
run with a card in. It does two things:

* it runs eight emulator-assist hooks (`dqGuardHook`, `tmrHook`,
  `tmrStartFix`, `s7CfMountHook`, `nbCfMountHook`, `cascadeTrace`,
  `pwrUpTrace`, `recFixHook`) per outer iteration, and
* it disables deadline batching (`core/sa1100.cpp:21173`), collapsing the
  batch ceiling from 4096 instructions to `kBatchTicks` = 16.

So on the netBook the ~18,700-line body of `executeUntil()` runs once per 16
emulated instructions, where the Series 7 without a card runs it once per
batch bounded by the next scheduled SoC event.

Measured, netBook full-OS boot:

| configuration | exec MHz | vs default |
| --- | --- | --- |
| default | 25.9 | — |
| `PSION_BATCH_TICKS=64` | 32.5 | **+26%** |

That 26% is available today with no new code — but the right fix is to make
the hooks PC-triggered (they all begin by comparing `cpu.getRealPC()` against
a fixed address) so `hooksActive` stops being a permanent "off" switch for
deadline batching.

### 4.5 The data-side TLB MRU misses two times out of three

`core/sa1100_cpu.h:10` advertises a "`lastTlbEntry` single-entry cache (80-95%
hit rate)". From the call-graph edges in the same profile:

| path | calls | falls through to the slow walk | hit rate |
| --- | --- | --- | --- |
| instruction fetch (`lastFetchEntry`) | 174,863,894 | 11,764,401 | **93.3%** |
| data access (`lastFastTlbEntry`) | 51,886,944 | 33,083,397 | **36.2%** |

The fetch side got its own MRU entry and hits 93%. The data side still shares
one entry across stack, globals and heap, so it ping-pongs — and each miss
pays `translateFast()` + `checkPermsfast()`, together 6.5% of total runtime.
Giving the data side the same treatment (a small direct-mapped set, or
separate read/write MRUs) is a contained change.

### 4.6 Roughly 29% of `tick()` calls execute nothing

174,863,894 ticks produced 124,485,954 executed instructions. The other
**50,377,940 (28.8%)** are prefetch-pipeline refills: after any branch,
exception or PC write, `prefetchCount` is reset to 0 and two further `tick()`
calls are needed before an instruction issues again.

Each of those bubbles still pays the whole per-instruction framework —
`tick()` self (131), `fetchVirtual()` (90), `sampleAndDispatchPendingExceptions()`
(13), the `stepCpu()` seam (31) — about 265 host instructions to do nothing but
move two words along an array. That is **≈15% of total runtime**.

The fetches themselves are real work; the three trips through the framework
are not.

---

## 5. What the browser adds on top

None of this could be measured here (no emscripten in the environment), so
these are read off the code rather than timed.

**The worker loop gives up about a quarter of its wall time.**
`frontend/public/emulator-worker.js:459` ends each tick with
`setTimeout(tick, sleep)`. On a CPU-bound guest `sleep` is 0 — but a
`setTimeout` chained from inside a timer callback hits the HTML spec's
nesting clamp at 4 ms. With `TICK_WALL_CAP = 12` ms of emulation per tick,
the duty cycle is ~12/16 ≈ 75%. A `MessageChannel` ping yields without the
clamp.

**The main-thread path gives up about half.** `stepFrame()`
(`wasm/main.cpp:105`) stops after `PSION_FRAME_BUDGET_MS`, default 8 ms, of a
16.7 ms frame. That is the right trade for keeping the tab alive, but it is a
2× on top of everything above. It applies to the non-worker fallback and to
embed mode.

**Even at infinite speed the emulator would run 6% slow.** `stepFrame()`
advances `clockSpeed / 64` per call and is driven from a 60 Hz
`requestAnimationFrame` (or a 64 Hz `setTimeout` that the clamp above makes
slower). 60/64 = 0.9375.

**The LCD blit is not free at 640×480.** `readLCDIntoBuffer()`
(`core/sa1100.cpp:27755`) measures **1.125 ms/frame natively** on the Series 7
— 6.7% of a 16.7 ms frame before WASM's multiplier. It:

* clears all 307,200 RGBA pixels to opaque black, then immediately overwrites
  every one of them;
* resolves each pixel through `resolvePixelIndex` → `pxByteAt` → `byteAt`,
  which re-derives the SDRAM bank from the framebuffer base address, range-
  checks the region and bounds-checks the offset **per pixel**, rather than
  resolving the bank once per row and walking a pointer;
* applies a three-entry contrast LUT per pixel.

Then JS copies the 1.2 MB out of the heap and `putImageData`s another 1.2 MB.
And it runs on every tick even though the guest is advancing at 0.1× — the
frame being painted is usually the same one that was painted last tick.

(For the record: `applyDeviceModePixels`, the 307k-iteration JS loop in the
same path, does *not* run on these devices — `EmulatorView.tsx:641` gates it
on `!isColourDevice`.)

---

## 6. What would actually move it

Ordered by how much they are worth, not by effort.

1. **A JIT.** Nothing else closes a 6× gap. An interpreter that hit the
   30–80 host-instructions-per-guest-instruction that good ones hit would be
   ~7× faster than this one and would land the Series 7 near 1:1; a JIT gets
   there with margin. The seam already exists: `SA1100::Emulator::stepCpu()`
   is documented as the dispatch point, and `ARM710::gprFileAddr()` /
   `cpsrAddr()` already expose the register file as linear-memory offsets for
   generated code.

2. **Rebuild the interpreter loop, if a JIT is too far.** The profile says the
   wins are: kill the per-instruction `fetchVirtual` call (a decoded-block
   cache keyed on physical page, which is what `tickPageLoop` was reaching
   for), stop paying the framework for prefetch bubbles, return memory
   results in registers instead of `pair<optional<u32>, MMUFault>`, and get
   the diagnostic gates out of the hot body (9.1% of runtime, measured) rather
   than relying on the branch predictor to skip them. Splitting `tick()` so the
   hot path is a small function would also recover most of the ~9% currently
   spent on prologues and epilogues.

3. **Compile the diagnostics out.** The `PSION_*` gates cost 9.1% of runtime
   and the `pcHistory` ring another 1.6%, on every instruction, for features
   that are off. Putting the whole diagnostic set behind a build-time
   `PSION_DIAGNOSTICS` macro — enabled for the harness, disabled for the
   shipping WASM build — is a contained change worth ~11%.

4. **Ship what is already written.** `-flto` in `scripts/build-wasm.sh`; the
   one-character fix in `initFastPathGate()`; resolve the Pac-Man KERN-EXEC 3
   regression so `decodeFast_`/`decodeCache_` can default on; call
   `setEnvVar` from the frontend (or, better, make the flags compile-time
   defaults rather than env lookups).

5. **Un-wedge the netBook.** Make the eight assist hooks PC-triggered so
   `hooksActive` stops disabling deadline batching. Worth ~26% on netBook
   today.

6. **Give the data TLB the same MRU treatment the fetch side got.** ~6.5% of
   runtime is currently the slow walk on a 64% miss rate.

7. **Frontend:** `MessageChannel` instead of `setTimeout(0)` in the worker
   (~25%); blit only when the guest has actually advanced a sim frame;
   row-wise LCD conversion instead of the per-pixel lambda chain.

Compounding the measured deltas, items 3–7 are worth roughly 2× on the
Series 7 and 2.5× on the netBook (diagnostics 1.11 × LTO 1.13 × decode fast
path 1.10 × netBook batching 1.26 × data TLB 1.07 × worker clamp 1.33, minus
the overlaps). Real, and worth doing — but that takes 0.12–0.17× to about
0.3–0.4×. The gap between that and 1× is item 1.

---

## 8. The batch loop, not the interpreter (the netBook's missing 2×)

Every measurement above was taken with the SoC's batch loop set to retire
**16** tick-equivalents per outer iteration. That constant turned out to be
worth more than every interpreter optimisation of the last several rounds put
together.

`SA1100::Emulator::executeUntil()` runs the CPU in batches and does its device
housekeeping — timer compares, RTC, the LCD frame IRQ, and eight netBook /
Series-7 assist hooks — once per batch. Two constants bound a batch:

* `kBatchTicks` — tick-equivalents per outer iteration. 16 when a
  per-instruction hook is watching (`hooksActive`: the netBook always, the
  Series 7 with a card in); 4096 otherwise, where a deadline ceiling bounds it
  instead.
* the burst cap inside `stepCpu()` — how many ticks one `tickPageLoop()` call
  may retire. Also 16, "so the per-batch hook cadence is preserved".

The second is redundant with the first: the caller already passes the ticks
left in its batch. All 16 did was guarantee an extra call per batch, and on the
deadline-batched path cap the burst at a 256th of what the batch allowed.

Both are now **128**. Measured in WASM (Emscripten 6.0.1), framebuffer
bit-identical either way:

| | 16 (before) | 128 (now) | |
| --- | --- | --- | --- |
| netBook + Quartz card, 30 sim s | 7.69 s / 7.47 s | **3.84 s** | **1.96×** |
| Series 7 boot, 8 sim s | 6.85 s | **6.48 s** | **1.06×** |

The netBook gets nearly all of it because it is the machine that had no
deadline batching at all; the Series 7 already batched to 4096 and only picks
up the burst cap.

### How far it goes, in both directions

The cadence is not free to choose. On the netBook booting its Quartz image off
a CF card — the workload that exercises the assist hooks hardest — the rendered
framebuffer is byte-identical at 64, 128, 256 and 384, and **changes at 512**;
the guest stops mounting the card entirely at 2048. So 128 keeps a factor of
four in hand.

It also breaks in the *other* direction: at `PSION_BATCH_TICKS` of 1, 4 or 8 the
same guest never loads its OS and sits on the bootloader splash, under the
reference interpreter (`PSION_DECODE_PAGELOOP=0`) as much as under the burst
engine. That end is still open — see §9, which closed the top end and says what
is known about the bottom one.

### What this leaves for the code generator

The netBook's 2.7× JIT result was obtained by running regions of 2048
tick-equivalents — outside the window above, which is why its framebuffer was
wrong. Inside the window the generated code is worth roughly nothing over the
burst engine (netBook 3.99 s vs 3.84 s; Series 7 6.37 s vs 6.48 s). §9 removes
the ceiling; §10 says why the JIT still does not pay once it is gone.

---

## 9. The CF card's interrupt was not a wake event

The ceiling in §8 was not the assist hooks, and the first version of this
section said it was. It is worth recording how that was ruled out, because the
theory was plausible and wrong: `PSION_NB_HOOK_MASK=0` turns off all eight
hooks, and with none of them running the netBook + Quartz boot still works at a
batch of 128 and still fails at 4. The hooks are not in it at either end.

What the top end actually was: **`nextSocEventCycle()` did not know about the CF
card's IREQ#.**

The netBook bootloader reads OS.IMG through medata's async multi-sector path: it
arms the read, then the guest sleeps until the card says a chunk is ready. The
emulator's idle skip jumps `passedCycles` to the soonest *scheduled* event — and
the card's interrupt was not one of them, so the guest slept past the assertion
and woke on the next 64 Hz tick instead. The injection that delivers that
interrupt is sampled once per outer iteration, so this only bites once a batch
runs long enough for the guest to reach its WFI before the batch loop next looks
at the card. Which is exactly what raising the batch does.

The trace is unambiguous. Interrupt deliveries during the read, at a batch of
128 against 2048:

```
128:   698792493  698802832  698813087  698823612  698833861   (~10,300 cycles apart)
2048:  701568024  701577457  705024024  705033447  708480024   (3,456,000 apart)
```

3,456,000 is `CLOCK_SPEED / 64` exactly — the tick the guest woke on, not
anything the card did. Cost: the faithful read took 0.63 simulated seconds at a
batch of 128 and **31 seconds** at 2048, for the identical 400 ATA commands and
6708 sectors. The boot then ran out of window and the screen was wrong.

The fix is one entry in `nextSocEventCycle()`, next to the audio-tick and
Eiger-ADC entries that exist for the same reason, requested only while an edge
is actually owed so a delivered interrupt cannot spin the idle loop. With it the
netBook + Quartz framebuffer is byte-identical at 128, 512 and 2048, and the
read completes in every case. The default (128) is bit-identical to before, on
this and every other device.

The bottom end (batches of 1, 4, 8) is a different failure and is still open:
there the read barely starts — 13 ATA commands in 60 simulated seconds against
400 — so it is an enumeration problem rather than a delivery one. Nothing needs
a batch that short, so it is not on anyone's path; it is recorded here because
it says the cadence window has two edges, not one.

---

## 10. Why the code generator still does not pay

With the ceiling gone the JIT runs correctly at any tick cap — self-check clean
(2363 regions verified, zero divergences, zero wrong regions at a cap of 2048)
and framebuffer-identical. It is still worth nothing:

| netBook + Quartz card, 30 sim s | |
| --- | --- |
| 3.95 s / 4.18 s / 4.25 s | interpreter |
| 3.80 s | JIT, default cap |
| 4.03 s / 4.22 s | JIT, cap 512 / 2048 |

The reason is coverage, and it is now measured rather than guessed. Over that
run the dispatcher reports:

* **375,123 entries** running **13.7 M cycles** of generated code — against
  order 200 M cycles executed. About 5%.
* **cycles/entry = 36.5**, i.e. **18 instructions per region**.
* **944,764 bursts the dispatcher could not be entered from at all**, because
  `jitEntryStateClean()` needs a full prefetch pipeline and half of all bursts
  resume mid-refill after a branch.

Eighteen instructions is a basic block. Raising the trace limit from 24 to 64,
128 or 256 changes `cycles/entry` by nothing at all — traces are not hitting the
limit, they are ending at the first control-flow edge the compiler will not
follow. Meanwhile the burst engine now retires around a hundred instructions per
entry, so a region covers a fifth of a burst and hands the rest straight back,
paying a call to do it.

Both of those were then built. Neither is what stands in the way.

---

## 11. Chaining and full-coverage entry, and what they proved

Two changes, both landed, both validated (2,166,079 regions verified against the
interpreter on the live workload, zero divergences; 91,512 differential cases;
25/25 boot devices):

* **A trace now follows an unconditional branch** instead of ending at it, so a
  region spans both sides. The chain is materialised into one function at compile
  time, so there is no cross-function dispatch to pay for.
* **The dispatcher is offered every burst**, not only bursts that begin with a
  full prefetch pipeline. Half of them do not — a branch flushed it and the
  previous burst ended there — and those were being skipped entirely.

Together they did what they were supposed to. Bursts the dispatcher never saw:
944,764 → **0**. Cycles run in generated code: 13.7 M → **68.5 M**, five times
the coverage. Average compiled span: 18 → **26 instructions**.

And the run got *slower*: netBook + Quartz, 4.02 / 4.08 s interpreter against
4.23 / 4.38 s with the JIT on. Series 7 boot 7.03 s against 6.90 s.

### The number that explains it

```
[jit] compiled span avg=26.2 insns (58% bridged), 0.53 branches followed
```

**Fifty-eight per cent of every region is a call back into the interpreter.**
`acceptsInline` compiles data-processing instructions and nothing else, so every
load, store, multiply and halfword transfer becomes a `call_indirect` into
`psionJitExecOne` — which does exactly the interpreter's work, plus the call. A
region like that cannot beat the interpreter, because it mostly *is* the
interpreter with an indirect call in front of it.

The clinching experiment separates that from dispatch overhead entirely. With
`PSION_JIT_LOOPS_ONLY=1` and a raised tick cap, only looping regions compile and
each entry runs the loop to its budget:

```
[jit] entries=15669  cycles=50834572  cycles/entry=3244.3
[jit] compiled span avg=14.5 insns (29% bridged)
```

3,244 cycles per entry — dispatch amortised more than two hundred times over, 25
million instructions executed in generated code — and the wall time is 4.04 s
against the interpreter's 4.04 s. **Dead even.** Not slower because of dispatch;
level because the code itself is no better, and that region set is still 29%
bridged.

### So the next step is the inline subset, not the dispatch

Chaining and full-coverage entry were the right things to build and they are
done. What they show is that neither coverage nor call overhead is the binding
constraint: the generated code has to stop calling out for the majority of what
it runs.

That means **inlining LDR/STR against the fast TLB** — the memory fast path
compiled into the region rather than bridged. Loads and stores are the bulk of
the 58%, they are what `psionJitExecOne` is called for most often, and they are
the reason a region is mostly interpreter today. Multiply and the halfword
transfers are worth having after that, and are much smaller jobs.

---

## 12. Compiling the loads

Three pieces of code generation, aimed by the histogram above rather than by
guesswork:

* **Loads against the micro-TLB.** The region emits the same lookup
  `SA1100Bridge::readVirtual`'s fast path makes — MRU way, address tag, cached
  permission byte, host read pointer — and bridges the instruction whenever any
  term misses. The inline path never has to be complete, only to agree: an
  unmapped page, an uncached or denied permission, a device address with no host
  pointer and an unaligned word all still reach the interpreter.
* **Constant-pool loads folded to constants.** `LDR Rd, [pc, #imm]` has a
  compile-time address, and the word at it lies in the very page the region was
  compiled from — so it becomes a store of an immediate, with no memory access
  at all. The literal is added to the region's hashed words so the staleness
  check covers it.
* **The trace stops at an unconditional write to r15.** `LDR pc, [...]`
  redirects, the bridge's post-check makes the region leave, and everything
  after it in the trace was dead code: 4,733 such positions compiled in one run,
  none of which could run.

Aimed correctly, as it turned out. The bridged fraction of a region:

| | bridged |
| --- | --- |
| before | 58% |
| after inlined loads | 51% |
| after folded literals | 38% |
| after stopping at PC writes | **33%** |

And `cycles/entry` went from 32 to **93** — the PC-write stop is most of that,
because a region that ends where execution actually leaves is re-entered at a
real target instead of part-way down dead code.

### The generated code is now faster than the interpreter

The loops-only configuration is the clean read, because it amortises dispatch
away: only looping regions compile, each entry runs its loop to its budget.

```
[jit] entries=18679  cycles=57359284  cycles/entry=3070.8
[jit] compiled span avg=20.0 insns (16% bridged, 19% inlined loads, 4% literals)
```

**3.72 s against the interpreter's 3.92 s** on the netBook + Quartz workload —
where before all of this it was 4.04 s against 4.04 s, dead even. The code
generator has crossed over: where it gets to run, it is now the faster of the
two.

### What is left is the per-entry cost

On the general workload the JIT is still level-to-slightly-behind (interpreter
3.82 / 3.95 / 4.00 s; JIT 3.97 / 4.02 / 4.33 s; Series 7 boot 6.63 s against
6.55 s, marginally ahead). The dispatcher itself is cheap — 2.4 M calls that
compile nothing cost 0.09 s, about 37 ns each — so what is left is the fixed
cost of *entering and leaving a region*: the loop-head guard, the word hash
(0.4 s of a 4.4 s run on its own), the flush, and the seven stores every exit
makes to rebuild the prefetch pipeline. At 93 cycles per entry that is spread
over about forty instructions, and it is still not thin enough.

Two things would move it, in order:

1. **Stores.** They are now the largest bridged category by a distance (1,724
   against 1,158 data-processing and 393 LDM/STM). The obstacle is real rather
   than incidental: a store into a cached code page has to invalidate its
   decoded ops, and that invalidation lives inside the interpreter's write path.
2. **The entry and exit sequence.** The word hash exists only because
   `Runtime::flushAll()` is never wired to the host-side image loads that
   motivated it; wiring it would let the hash go. The fault-slot stores at an
   exit are only needed after a bridge has run, which the compiler knows.

---

## 13. Both of those, done

**Stores compile inline**, coherency included. The invalidation a store owes —
a guest write into a cached code page makes its decoded ops stale, and the
host-pointer path never reaches `writePhysical` where the drop would otherwise
happen — is emitted rather than avoided: `physAddrFromTlbEntry` and
`invalidateDecodePage` are twenty WASM instructions between them, and a store
that lands on no cached page pays one indexed compare, exactly as the
interpreter's own fast path does.

**The word hash is off by default**, because the hole it covered is now closed
at the source. It existed because the page-validity token cannot see a host-side
write — a card or ROM image copied straight into the RAM/ROM buffer — and a ROM
page is marked immutable and never dropped at all. Every such site now calls
`ARM710::onGuestCodeReplaced()`, which drops every compiled region and the
decoded pages with it, immutable ones included. That is a latent bug fixed as
well as a cost removed: `Runtime::flushAll()` existed for exactly this and had
never been called from anywhere.

Where that leaves the shape of a region:

| | bridged |
| --- | --- |
| before any of this | 58% |
| after loads, literals, PC-write stop | 33% |
| **after stores** | **19%** |
| in loop regions | **7%** |

`bridged by kind` now reads `dataproc=1408 ldm/stm=384 ldr/str=384 slow=128`,
and `bridged ldr/str: store=0`.

### Where it stands

netBook + Quartz, 30 simulated seconds, eight interleaved runs: interpreter
3.91 – 4.40 s, JIT 4.01 – 4.22 s. **Level, inside the noise** — up from about 8%
behind when this started. Series 7 boot: 6.53 s against 6.61 – 6.83 s, still
marginally behind.

With dispatch amortised away — loops only, raised tick cap, 3,071 cycles per
entry — **3.81 s against the interpreter's 4.02 s**, and those regions are only
7% bridged.

So the generated code is comfortably the faster of the two; what stops that
showing up on the general workload is still the per-entry cost, spread over
regions that average 93 cycles. The remaining bridges are data-processing forms
the inline subset refuses (register-specified shifts, MRS/MSR), LDM/STM, and
the halfword transfers. None of them is now the obvious next lever — the
entry and exit sequence is.

---

## 14. What the profile says, which is not what the guessing said

`node --cpu-prof` against an emscripten build linked with `--profiling-funcs`
gives per-function self time for the whole emulator, generated regions included.
Two runs, interpreter and JIT, then a per-function diff. It should have been the
first thing done rather than the last, because it settles in one pass what
several rounds of reasoning had only narrowed.

**At the shipping hot threshold the generated code is 0.40% of runtime.** Not
0.4% faster — 0.4% *of the time*. 24.9 ms of a 6.16 s profile. Every argument
about whether a region beats the interpreter was being had over a slice that
small, which is why the wall times looked like noise: they were noise.

At `PSION_JIT_HOT=1000`, where the JIT covers about 12% of what the guest
executes, the diff is legible:

```
  slower                              faster
   +91.7 ms  WebAssembly.Instance      -140.2 ms  stepCpu (the burst loop)
   +87.0 ms  generated regions         -110.4 ms  execDataProcessing
   +72.1 ms  WebAssembly.Module         -62.7 ms  execSingleDataTransfer
   +38.4 ms  psionJitExecOne (bridges)  -50.0 ms  writeVirtual
   +72   ms  the region compiler        -46.1 ms  execBlockDataTransfer
   +22.8 ms  the dispatcher
```

The generated code runs **30 M instructions in 87 ms — 2.9 ns each, against the
interpreter's ~16 ns**. It is five times faster, exactly as the loops-only
measurement said. And it saves 446 ms of interpreter time to do it.

But **164 ms goes to V8 compiling and instantiating the modules, and 72 ms to
this compiler emitting them**. 236 ms of compile against 298 ms of net saving,
and the wall times come out level.

### So the lever is bytes, not instructions

The dispatcher had been emitting the whole micro-TLB guard — sixty instructions
for a load, a hundred and fifty for a store — into every region that made an
access, plus a full flush and a seven-store pipeline write at every exit and
every bridge. Measured: **26.9 MB of WebAssembly** for 6,093 regions, 4.4 KB
each.

Those sequences are identical in every region, so they are now module-internal
functions the batch shares — `hostReadW/B`, `storeW/B`, `flush`, `exitState` —
reached by a direct `call`. The region-side code for a load is ten instructions
instead of sixty; for a store, eight instead of a hundred and fifty.

**26.9 MB → 15.5 MB**, a 42% cut, with the differential test unchanged at
94,377/94,377 and the same 4,210 hand-overs to the interpreter.

### And it still does not pay on this workload

Four interleaved pairs at the shipping threshold: interpreter 4.00–4.19 s, JIT
4.16–4.51 s. A threshold sweep with the smaller modules — 8000, 2000, 500 —
finds nothing that wins either.

That is a property of the workload, and it is worth naming precisely. A region
costs about 23 µs to compile (V8's share plus this compiler's) and saves about
13 ns per instruction it executes, so it has to run **roughly 1,800
instructions** before it breaks even. On a netBook desktop idling under a Quartz
UI the average compiled region runs about 3,000 — barely over the line, and the
marginal ones are far under it. The profile is flat: there is no hot loop to
find.

The loops-only configuration is the same fact from the other side. Restricted to
regions that actually loop, the JIT runs 3.81 s against the interpreter's
4.02 s, because those are the regions that clear 1,800 instructions comfortably.
A CPU-bound application — the case that started this whole investigation — is
that shape. This benchmark is not.

**So: the code generator works, is five times faster per instruction than the
interpreter, and is correct. What it needs is a workload with hot code in it,
or a compiler cheap enough that 3,000-instruction regions pay.** The remaining
compile cost is mostly V8's, not ours, so the second is bounded by what a
browser will do with a module. The first is a matter of benchmarking a game
rather than a desktop.

---

## 15. Making the policy match the arithmetic

If a region has to run ~1,800 instructions to repay its compile, then compiling
everything is the wrong policy, and the measurement already said so:
`PSION_JIT_LOOPS_ONLY` beat compiling everything by a distance. So that is the
default now, along with the other half of the same idea — **a looping region
gets a long tick cap** (2,048, `PSION_JIT_LOOP_TICKS`), because a loop cut off
after the caller's 128 ticks is the compile cost without the benefit. Measured:
loops at the caller's cap 4.26–4.42 s, loops at 2,048 3.94–4.05 s.

A straight-line region still honours the caller's cap, which is what keeps it
indistinguishable from the burst engine to the batch loop around it. What makes
the long cap safe for a loop is that a loop stays where it is: the SoC's
per-batch housekeeping is delayed, but no address that loop watches for goes
past unobserved, because the guest is not going anywhere.

### Where that lands, measured properly this time

This benchmark's run-to-run spread is about ±7% — two identical configurations
measured 4.28 s and 4.00 s in the same session — which is wider than every
difference argued about in the sections above. Six interleaved pairs:

| netBook + Quartz, 30 sim s | mean | median | best |
| --- | --- | --- | --- |
| interpreter | 4.090 s | 4.075 s | 3.920 s |
| JIT | 4.092 s | 4.080 s | 3.870 s |

**Parity.** Series 7 boot, three pairs, is parity too (6.90 s against 6.80 s).
The netBook booting the ESHELL image off a card is slightly ahead (1.22 s
against 1.19 s, and 1.13 s at a lower threshold). All framebuffers identical.

That is up from four to eight per cent behind, and it is as far as these two
benchmarks can take it: neither has enough hot code for a compiler to beat a
good interpreter, and neither can resolve a difference smaller than its own
noise. The next honest measurement is a CPU-bound application — the case that
started this investigation — not another round of tuning against a desktop that
is mostly idle.

---

## 16. An application benchmark, and what it says

`scripts/wasm-bench.mjs --script` drives the guest's own UI: a JSON list of
steps that boots, inserts a card, taps, types, screenshots and — between
`{"timer":"start"}` and `{"timer":"stop"}` — times a window. Everything outside
the timer is untimed, so boot, mount and the whole launch sequence stay out of
the number. `--shot-dir` writes PGMs, which is how a launch sequence gets
checked: run it, look at the screen.

`scripts/bench/netbook-sheet.json` is the first one. It boots the netBook
bootloader, hands it the stock OS on a CF card, waits for the desktop, opens
Sheet from it, and pages through the grid forty times with the timer running.

**It runs at 0.8× real time.** 4.8 simulated seconds take about 6.2 wall
seconds. That is the "nearly unusable" this whole investigation started from,
finally on a stopwatch — and it is nothing like a boot, which runs at 7× real
time because it is mostly idle. Every measurement in sections 8–15 was taken on
a workload seven times easier than the one that matters.

### The JIT on it: parity, and the reason is structural

Six interleaved pairs: interpreter mean 6.197 s, JIT mean 6.227 s. Parity, same
as on the boots. What is new is that the dispatcher's own counters say exactly
why:

```
[jit] regions=128 in 1 modules, 117 KB of wasm
[jit] compiled span avg=6.0 insns (17% bridged), 0.00 branches followed
[jit] bridged by kind: slow=128
[jit] traces ended on: mode change=128
```

**Every single compiled region is six instructions long and ends at a mode
change**, and every one of those is a `DK_SLOW` — a SWI. EPOC is a microkernel:
application code round-trips through `Exec::` calls constantly, and a SWI ends a
trace because it changes mode and the region cannot follow it. There is no long
straight-line run and no loop to find, so there is nothing for a region compiler
to amortise its 23 µs over.

That is not a tuning problem. On this guest, application code is *shaped* wrong
for this design.

### The slot table was also thrashing

The same run showed 420,309 evictions against 128 regions compiled and 6.7 M
entries that never got hot, because a conflicting address took the slot and
reset the count. A slot is won rather than taken now — a conflicting address
knocks the incumbent down by one and goes away — which drops evictions to 27,957
and protects a compiled region for free, since its count sits at the threshold.
It did not change the wall time here (nothing was going to get hot enough
anyway), but it is the right policy and it removes a confound from any future
measurement.

### One measurement worth keeping

A `std::getenv` on the region-entry path, added while wiring the per-region tick
cap, cost 4.36–4.61 s against 3.94–4.05 with it hoisted into a static. In
Emscripten a getenv is a JS-side string lookup of ~50–100 ns; on a path taken
once per region entry it was larger than everything the code generator saves.
The emulator's own hot paths have carried that warning for years
(`PSION_ENV_CSTR` exists for it) and it still caught this.

The JIT stays default-off, and is correct: 154,522 regions verified against the
interpreter live with zero divergences; 94,377 differential cases, of which
2,205 are inlined stores checked byte-for-byte on the memory they left behind;
and under `PSION_JIT_CHECK_ALL` — which double-executes and so has a noise floor
of its own — inlining loads and stores produces **exactly the same 7,477
divergences as a control run with everything bridged**, which is to say it adds
none. All 25 boot-suite devices pass and four SA-1100 configurations render
byte-identically.

Reproduce:

```
node scripts/wasm-bench.mjs frontend/public/psion.js \
    roms/netBook_BL_v011_eng.bin netbook 3 \
    --card tests/cards/quartz-netbook.img --post-attach 30
PSION_BATCH_TICKS=16 PSION_BURST_TICKS=16 node scripts/wasm-bench.mjs ...
```

## 17. Profiling the application benchmark, and the two things it found

Section 16 put a real workload on the clock. That also makes it the first
workload worth profiling: the boots that sections 8–15 were measured on run at
3–13× real time because they are mostly idle, so their profile is largely the
SoC's event loop waiting for something to happen. The Sheet benchmark is 0.8×
real time and the CPU is the whole of it.

`node --cpu-prof` over the timed window, with the WASM relinked
`--profiling-funcs` so generated and C++ functions both get names
(`PSION_WASM_EXTRA_LINK="--profiling-funcs"`), interpreter only:

```
  34.7%  SA1100::Emulator::stepCpu           (tickPageLoop is inlined into it)
  12.5%  SA1100Bridge::readVirtual
   8.9%  ARM710::execDataProcessing
   8.5%  SA1100::Emulator::executeUntil
   8.0%  ARM710::execBlockDataTransfer
   7.1%  SA1100Bridge::writeVirtual
   5.0%  ARM710::execSingleDataTransfer
   2.8%  SA1100Bridge::resolveFetchPage
   2.4%  ARM710::translateAddressUsingTlb
```

Two of those lines are not the interpreter doing arithmetic.

### LDM/STM was paying a virtual call per register

`execBlockDataTransfer` calls `readVirtual()` or `writeVirtual()` once per
register in the list. Both are virtual, so each is a `call_indirect` with a type
check, and the read hands back a `pair<optional<uint32_t>, MMUFault>` — three
words, returned through memory. A `push {r4-r8,r10,lr}` pays that seven times.

It does not have to. Those seven words are contiguous, and on a stack frame push
or pop — which is nearly every block transfer a compiler emits — they are in one
page, at one privilege, with one cached permission byte and one host pointer.
`ARM710::fastSpanPtr(va, bytes, write)` resolves that once for the whole block
and returns the biased host pointer, or `nullptr`, in which case every access
falls back to the per-word path exactly as before.

The SA-1100 implementation is deliberately not new logic: every test in it is
one of `readVirtual`/`writeVirtual`'s own inlined fast-path tests, in the same
order, reading the same fields. It adds one bound of its own — the whole span
must lie inside one 1 KB region — and that bound is what makes a single
resolution sound. A small page carries four access-permission fields selected by
`va[11:10]`, so permission is only uniform within 1 KB; staying inside 1 KB also
keeps the span inside one 4 KB decoded code page, so a store's code-coherency
drop is one page, done once. A 16-register transfer is 64 bytes, so the bound
costs essentially no coverage.

A span whose permission byte says *not permitted* is refused, not served: the
fault is the per-word path's to raise, and it has to raise it on the right word.

### The assist hooks: an obvious win that isn't one

`executeUntil` runs eight netBook / Series 7 assist hooks once per batch. They
are lambdas, so they are inlined straight into the loop, and `nbCfMountHook`
alone is 400+ lines. In steady state every block inside them is switched off —
the traces by an env var, the CF swap fixups by a pending flag, the rest by a PC
that has to be one of a handful of literals — and they walk the lot, once per
batch, for the whole session, to find that out. Calling all eight against
`PSION_NB_HOOK_MASK=0` is **7%** of this benchmark.

The obvious move is to ask first: front each hook with a copy of its own leading
guard, so one that cannot fire costs a test rather than a walk. That was built,
validated and measured, and it is worth **nothing**. Four interleaved pairs,
gated against ungated in the same binary:

```
gated     5.57  5.66  5.72  5.50
ungated   5.54  5.57  5.76  5.68
```

A 2–2 split. It has been reverted, and the reason it fails is worth writing
down, because the same shape will come up again.

The 7% is not the hook bodies. Turning off the one hook that does real work
(`dqGuardHook`, which walks the kernel delta queue with two `readVirtualDebug`
page-table walks one batch in 64) is worth 1% and the pairs split 3–1. Running
*only* that hook measures the same as running none. What costs is **entering the
block at all**, about thirty million times:

```
PSION_NB_HOOK_MASK   0xFF ~5.53   0xFD ~5.47   0x02 ~5.06   0x00 ~5.15
```

A gate has to read the PC and evaluate its own terms every batch, which costs
about what calling the guards costs — so it moves the work rather than removing
it. Fifty lines of conditions that have to stay in step with eight hooks, for
that, is a drift hazard and nothing else.

One piece was kept, because it is exact rather than a copy: `dqGuardHook` acts
one batch in 64 and returns on the other 63, and the counter is the only state
it touches on those, so the throttle moved to its call sites and the 63 calls
are gone.

**What this really points at** is the batch count, not the hook block.
`hooksActive` is permanently true on the netBook, and that is also what disables
deadline batching (§8's `kDeadlineBatch`), pinning the outer loop to
`kBatchTicks` = 128 instructions instead of letting it run to the next scheduled
SoC event. Everything in the loop body — the hooks, the gate, the housekeeping —
is paid thirty million times because of that, and the way to stop paying it is
to run the loop fewer times. That is the next thing to try, and it is a change
to interrupt timing on the netBook rather than a contained one, so it wants its
own measurement and its own validation pass.

### What it is worth

Six interleaved pairs of the Sheet benchmark, clean full builds either side, on
the tree as it ships:

```
before   6.01  5.92  5.83  5.88  5.72  5.80   mean 5.86 s
after    5.52  5.45  5.53  5.58  5.56  5.37   mean 5.50 s
```

**6.1%**, and the new side wins every pair. Switching the span off in the new
build instead (`PSION_NO_BLOCK_SPAN=1`, four pairs) puts it at 8.5% — 5.385 s
against 5.8875 s, winning all four. Both are honest; the gap between them is
this benchmark's between-run drift, which is larger than its within-run spread,
so the figure is somewhere in 6–9% and the direction is not in doubt. The
workload goes from about 0.81× real time to about 0.87×.

The two boot-shaped benchmarks do not move — netBook + Quartz card 4.68 → 4.71 s,
Series 7 boot 10.07 → 9.52 s — which is the expected shape rather than a
disappointment: both run at 3–13× real time because they are mostly idle, so
there is very little block-transfer traffic to save. The win lands on the
workload that was "nearly unusable", which is where it was aimed.

Every screen is byte-identical across the change and under each switch: Sheet
`fnv=6425b4c0` with the span on, with `PSION_NO_BLOCK_SPAN=1`, and with
`PSION_JIT=1`; Quartz card `fnv=c5534c9a`; Series 7 boot `fnv=2a40ac1b`. The
25-device boot suite passes, the 94,377-case generator differential test is green
at the same 4,210 hand-overs, and the code generator's own numbers on the Sheet
workload are unchanged to the last digit — 128 regions, 4,073 entries, 27,957
evictions — so nothing about the JIT's behaviour moved either.

### One build lesson, since it cost an hour

`fastSpanPtr` is a new virtual on `ARM710`, so it shifts every vtable index
after it. Rebuilding only the three translation units that changed left the
other 43 objects holding the old indices, and the emulator ran fine on the
interpreter path but died in the code generator with `null function or function
signature mismatch` — a `call_indirect` landing on a function of the wrong type.
It looked exactly like a code-generator bug and was not one. Adding or removing
a virtual means `rm -rf wasm/obj` before `scripts/build-wasm.sh`.

### Switches

```
PSION_NO_BLOCK_SPAN=1   LDM/STM goes back to one readVirtual/writeVirtual per
                        register, leaving the per-word fast path in place
PSION_NO_MEM_FASTPATH=1 drops both, back to the authoritative walk
PSION_NB_HOOK_MASK=<b>  runs only the assist hooks whose bits are set
```

Reproduce:

```
rm -rf wasm/obj && bash scripts/build-wasm.sh
PSION_RTC_SEED=0 node scripts/wasm-bench.mjs frontend/public/psion.js \
    roms/netBook_BL_v011_eng.bin netbook 0 \
    --script scripts/bench/netbook-sheet.json
```
