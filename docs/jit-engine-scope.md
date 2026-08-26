# JIT engine scope

Referenced from `core/arm710.h`, `core/sa1100.h` and `core/sa1100.cpp` — the
file those comments point at did not exist. This is it.

Read `docs/netbook-s7-performance.md` first: it establishes why this work is
needed and what the ceiling is.

---

## The target, and why a JIT

The Series 7 and netBook need ~94.5 M emulated ARM instructions/second to run
at 1:1. The interpreter delivers ~16 M/s natively. Nothing about that gap is
closable by tuning the interpreter — it needs a code generator.

But a JIT alone does not get there either. Splitting the measured profile by
what a code generator actually removes:

| | share of runtime | removed by a JIT? |
| --- | --- | --- |
| fetch, decode, dispatch, per-instruction framework | 65.7% | yes |
| instruction semantics | 11.7% | collapses to a few native instructions |
| outer servicing loop | 6.1% | partly |
| **MMU / TLB path for guest loads and stores** | **15.7%** | **no** |

Even a free JIT caps at ~6.4× against that last row, which is 0.17× → ~1.1×
natively and under 1× in a browser. **The memory subsystem is the ceiling, and
it has to be fixed before the code generator, not after.**

---

## Stage 0 — verification (landed)

Every stage after this one changes how instructions are executed. None of it
is safe without a way to prove behaviour is unchanged, and "the boot
screenshot still looks right" is not that: a screenshot only catches
divergence that survives to the panel, and only after millions of instructions
have washed over it.

**`core/state_trace.h`** folds the architectural state at every executed-
instruction boundary into a running 64-bit FNV-1a hash and samples it to a
file. Run a workload twice, diff the traces: identical files prove the two
engines agreed on all 16 registers, on CPSR and on the cycle count at every
sampled boundary. A differing line brackets the first divergence to one
interval, and `PSION_STATE_TRACE_FROM/TO` then dumps full per-instruction
register records across that window.

```
PSION_STATE_TRACE=<path>        write the trace
PSION_STATE_TRACE_INTERVAL=N    sample every N instructions (default 65536)
PSION_STATE_TRACE_FROM=N        full per-instruction records...
PSION_STATE_TRACE_TO=N          ...for indices in [FROM, TO)
```

Two things had to be fixed before it could work at all:

**Determinism.** The SA-1100 seeded its RTC from `std::time(nullptr)`, so two
runs of the same workload diverged in guest register state from the first RTC
read onward. `PSION_RTC_SEED` now pins it, mirroring the convention
`psion_asic9.cpp` already used (`unset`/`1` = host time, `0` = epoch, anything
else = a hex literal). Default behaviour is unchanged.

**The engine gate.** `initFastPathGate()` turns every fast path off if any
other `PSION_*` variable is set, so that per-instruction diagnostic traces
still run when someone is debugging. That rule made the engines untestable —
the harness has to run *with* an engine on, and needs the RTC pinned. There is
now an explicit `isEngineNeutralVar()` list holding exactly those two names,
so exempting a variable is a deliberate act rather than a pattern match.

---

## Stage 1 — burst execution (landed, DEFAULT ON)

`ARM710::tickPageLoop()` — the existing Tier-2 page-anchored loop — was dead
code: `initFastPathGate()` compared 21 characters of the 22-character prefix
`"PSION_DECODE_PAGELOOP="` and offset by 21, so the value it read was `"=1"`
and the `== '1'` test never held. `pageLoop_` was permanently false and the
loop had never run through its documented switch. One character.

Un-breaking it exposed three real divergences, each found by the harness
above. All three are properties of *any* engine that retires more than one
instruction per call — the JIT included — not of the page loop specifically.

### 1. Frozen device clock (found at instruction 127,673)

`passedCycles` was advanced by the outer loop after the stepping call
returned, so during a burst it stayed frozen. The guest read OSCR mid-burst
and got a value 5 ticks stale.

Compensating inside `readOscr()` fixed that one read and moved the divergence
to instruction 1,041,345 — because the problem is not OSCR, it is every
peripheral whose answer depends on the clock. Patching read sites does not
scale and misses the ones nobody thought of.

The fix is a per-instruction **cycle sink**: `ARM710::cycleSink_` points at the
device's own counter, a multi-instruction engine advances it as it retires
each instruction, and the device stops advancing it itself. Peripherals then
never observe a frozen clock, whichever engine is driving. Deliberately not
used by `tick()` — single-instruction callers such as `callRomFunctionSync()`
do their own accounting and would double-count.

### 2. Bursting past the next scheduled event

The outer loop bounds the interpreter by the soonest scheduled SoC event. A
burst ignored that bound and overshot it by up to its own length, so interrupt
delivery slid — and because idle time is fast-forwarded to the next event, a
few instructions of slip amplified into **12 ms of simulated time** by
instruction 1,041,345.

`stepCpu()` and `tickPageLoop()` now take a cycle budget and a tick budget and
report what they consumed, so a burst stops exactly where the interpreter's
batch loop would have. `batch_i` counts tick-equivalents rather than calls, so
a batch covers the same number of emulated instructions either way.

This is the same bound a block-chaining JIT needs, and `nextSocEventCycle()`
already exists to supply it.

### 3. Cycles lost on the fallback path

`tickPageLoop()` falls back to `tick()` when it cannot resolve a page or makes
no progress. Those paths did not advance the sink, so their cycles vanished
from `passedCycles` and simulated time ran slow. The contract is now explicit
and stated at the top of the function: **the burst advances the sink for every
cycle it reports, and the caller adds nothing.** Miss one path and time
silently runs slow — which is exactly what happened.

### The wrong-dispatch bug

The three divergences above were all cycle-accounting. A fourth was not, and it
is the one that mattered.

The dispatch kind was carried alongside each instruction through the prefetch
pipeline, and it could get out of step with the word it described. On the
netBook Quartz image an IRQ left `prefetchKind[]` shifted one slot against
`prefetch[]`, so `0a000001` — `BEQ`, with Z set — was dispatched as
data-processing. It executed as `AND r0,r0,#1`, wrote 0 to r0 instead of
branching, and the guest hit a prefetch abort four instructions later. The
device never idled again (`idle_frac` 96.3% → 4.0%) and the boot screen
changed. The boot suite caught it; two hand-picked spot checks had not.

Both engines held *identical architectural state* at that instruction and were
still about to execute different things, so the register hash could not see it
coming. Widening the harness to cover the prefetch pipeline located it exactly.
(The decoded kinds are dumped but deliberately **not** hashed: they are an
engine's internal selector, and the reference interpreter never computes them —
hashing them would make the two engines incomparable by construction.)

The fix is to derive the kind from the instruction word **at the point of use**,
in `tickPageLoop()` and in `tick()`'s fast-dispatch path alike. A selector that
can disagree with its instruction silently runs the wrong handler; deriving it
makes the disagreement impossible. `decodeKind()` moved into the header for
this — as an out-of-line call, made once per instruction, it cost ~25% of the
burst engine's advantage.

This is a strong candidate for the **Pac-Man KERN-EXEC 3 regression** that kept
`decodeFast_` opt-in: same carried kind, same wrong handler, and it only bites
once an interrupt lands mid-stream — which is why boot never hit it and an
intensive app did. There is no Pac-Man fixture in the tree to confirm it, so
`decodeFast_` is left opt-in; it is also worth almost nothing now that the page
loop is on (+0.4 MHz on the Series 7, measured).

### Result

| device | interpreter | burst engine | |
| --- | --- | --- | --- |
| Series 7 (20 s desktop boot) | 36.8 MHz / 0.166× | 54.7 MHz / **0.247×** | **+49%** |
| netBook (full OS off card) | 26.0 MHz / 0.117× | 40.1 MHz / **0.181×** | **+54%** |

Verified:

* **State hash identical at every sample point** — Series 7 146 M instructions,
  netBook stock OS 260 M, netBook Quartz 145 M, for the page loop and the fast
  path independently.
* **All 25 boot-suite devices pass**, in the default configuration, with the
  kill switch, and with `decodeFast_`.
* **Every one of those 25 screenshots is byte-identical** to the interpreter's.
* `executed_cycles` matches to the digit — the engines did not merely reach the
  same place, they charged the same cycles getting there.

**Default ON**, with `PSION_DECODE_PAGELOOP=0` as the kill switch, following the
same pattern as `PSION_DEADLINE_BATCH`. Any other `PSION_*` variable still
forces the interpreter, so a diagnostic run keeps its per-instruction traces.

---

## Stage 2 — the memory subsystem and the WASM build (landed)

### Measuring the thing that actually ships

The whole analysis up to this point was native profiling, because there was no
way to run the WebAssembly build here. `scripts/build-wasm.sh` also no longer
worked: it used `emcc` for the C++ translation units and the link, and from
Emscripten 4 on the C driver stops pulling in libc++ when the inputs are object
files, so the link fails with `operator new`, `__cxa_throw` and
`std::to_string` undefined. It needs `em++`.

With that fixed and `scripts/wasm-bench.mjs` added, the shipped artifact can be
benchmarked and profiled directly — and the first thing it showed is that
**native measurements over-predict WASM ones**, sometimes badly:

| change | native | WASM |
| --- | --- | --- |
| `-flto` on the WASM build | +13% | **0%** |
| data-side micro-TLB | +40% | +4.6% |
| burst engine | +49% | +38% |
| fetch-span + inlined IRQ check | 0% | +6.5% |

`-flto` was the obvious win and is worth nothing here — wasm-ld and Binaryen
already do the whole-program work at link time, so it stays off. The last row
is the mirror image: two changes gcc had already inlined, which mattered only
in WASM. **Measure the artifact you ship.**

### The data-side micro-TLB

`lastFastTlbEntry` was a single MRU entry shared by every data access, and a
typical instruction stream touches stack, globals and heap in turn — so it
ping-ponged: 51,886,944 `readVirtual` calls on a Series 7 boot, 33,083,397
falling through to the full `translateFast()` + `checkPermsfast()` walk. A
**63.8% miss rate**, against the "80-95%" the header claimed.

It is now a small direct-mapped set indexed on VA bits 12+. Entries live in the
fixed `fastTlb[][]` array and are never freed, and every use re-checks
`addrMask`/`addr`, which is authoritative — so a slot is only ever a *hint*: it
can miss, it can never mis-serve.

### The two calls the burst loop did not need

Profiling the WASM build put 6.5% in `resolveFetchPage` and 4.2% in
`sampleAndDispatchPendingExceptions`.

`resolveFetchPage` pinned its resolved span to the 1 KB subpage containing the
fetch. That is only required for a **small page**, whose AP field is selected by
`va>>10`. A section's permissions are uniform across its whole megabyte, and a
large page's across 16 KB — both can run to the 4 KB boundary of the
decoded-page validity token instead. Kernel and ROM code is largely
section-mapped, so that is four times fewer re-resolves.

`sampleAndDispatchPendingExceptions` does nothing unless an IRQ or FIQ is
pending. The flag test is now inlined at the call site.

### Result

**WebAssembly**, Series 7, 8 simulated seconds, interleaved against the
pre-session build on the pinned Emscripten 6.0.1:

| | wall time | |
| --- | --- | --- |
| before this work | 13.62 s | |
| after | **8.52 s** | **1.60× faster** |

Plus two per-frame costs that do not scale with core speed: the LCD blit
1.569 → 0.212 ms/frame (**7.4×**), and the worker's duty cycle 75.5% → 99.9%
(**1.32×**) on a CPU-bound guest.

Native, Series 7 20 s desktop boot: 36.8 MHz / 0.166× → **81.8 MHz / 0.370×**,
**2.23×**.

Verified throughout: `executed_cycles` identical to the digit at every step,
state hash identical against the interpreter, `PSION_MEM_FASTPATH_CHECK` clean,
and all 25 boot-suite devices passing with byte-identical screenshots in both
the default configuration and under the kill switch.

### The frontend, which turned out to matter more than the memory path

Two things outside the emulator core, both measured rather than reasoned about.

**The LCD blit.** `readLCDIntoBuffer` runs once per displayed frame — up to 60
times a second whatever the emulated CPU is doing — and cost **1.57 ms/frame**
in WASM on a Series 7: 9.4% of a 16.7 ms frame, none of it scaling with core
speed. It resolved every pixel through `resolvePixelIndex → pxByteAt → byteAt`,
re-deriving the SDRAM bank, range-checking the region and bounds-checking the
offset *per pixel*, and cleared all 307,200 pixels before overwriting every one.

None of that varies within a row. The 8 bpp / 32-bit case (Series 7, netBook,
netpad) now resolves the bank and row offset once per row and folds palette and
contrast into one 256-entry RGBA lookup per frame: **1.569 → 0.212 ms/frame,
7.4×**. All 25 boot-suite screenshots stay byte-identical.

**The worker's scheduling yield.** The loop ends in `setTimeout(tick, sleep)`,
and `sleep` is 0 exactly when the core is behind real time. A chained
`setTimeout(0)` is clamped to 4 ms by the HTML timer-nesting rule. Measured in
real Chromium inside a Worker:

| | rate | per iteration |
| --- | --- | --- |
| `setTimeout(0)` chain | 249/sec | 4.02 ms |
| `MessageChannel` chain | 90,228/sec | 0.011 ms |

With the loop's actual shape (12 ms of emulation per tick, then reschedule):
**75.5% → 99.9% duty, 1.32×** on a CPU-bound guest.

### A third round, after re-profiling

Re-profiling the shipped build found two more, both on the burst loop's
per-instruction path:

**The resolved code page was a local.** `FetchPage` lived in `tickPageLoop`, so
every burst re-resolved from scratch — and a burst is at most 16 instructions,
so roughly one instruction in sixteen paid a full `resolveFetchPage`. Holding it
in a member and re-validating on entry with the loop's own checks (span,
privilege, decoded-page token): **+3.3%**. This is the same 7.9% of the profile
the two failed attempts below were aiming at; they attacked the *cost* of a
resolve when the problem was its *frequency*.

**Condition codes came from a switch the compiler would not inline.** Each
table entry is the truth table of one condition over all 16 NZCV states,
indexed by `CPSR >> 28`, so evaluating one is a load, a shift and a mask:
**+4.5%**. Checked exhaustively against the switch, 256 of 256 combinations.

> **A measurement error, and its correction.** These two were first reported as
> +11% and +15%. They are not. The A/B copied each previous `psion.js` into a
> scratch directory to compare against — but an emscripten `psion.js` loads a
> **hard-coded `"psion.wasm"`** filename, not one derived from its own name, so
> every one of those runs silently loaded a stale `psion.wasm` that happened to
> be sitting in that directory from hours earlier. The "before" side was not the
> build it claimed to be.
>
> Corrected by giving every build its own directory containing its own
> `psion.wasm`, then interleaving. The cumulative figure was never affected —
> it always compared whole worktrees, each with a matching pair.
>
> The lesson generalises: an A/B harness needs to prove it loaded what it says
> it loaded. Interleaving controls for ambient noise but not for measuring the
> wrong artifact.

**Stripping the per-instruction diagnostics is not worth doing.** The
`pcHistory` ring write and index update, removed entirely: **+0.4%**, i.e.
nothing. With the burst engine on, the ~46 env gates in `tick()`'s slow body
are already skipped for almost every instruction, and the ring write is two
stores the store buffer absorbs. The native profile's 1.6% did not survive
into WASM. Left in place — it is what `logPcHistory()` prints at an abort.

### A fourth round: the shadow array nobody read

The wrong-dispatch fix made the dispatch selector derive from the instruction
word at the point of use. What it did *not* do was remove the machinery it
replaced. `prefetchKind[]` — the per-slot decode kind carried alongside
`prefetch[]` — was still maintained on every fetch, in both engines, and
nothing functional read it any more. Only the state trace did.

So the burst loop was calling `decodeKind()` **twice per instruction**: once on
the freshly fetched word to feed the shadow array, and once again on the word
being executed to pick the handler. `fetchVirtual()` paid a third on the
tick() path.

Deleting `prefetchKind[]` and `lastFetchKind` outright, and having the trace
classify at its own call site (it is behind an unlikely-branch gate, so it is
free when tracing is off):

| device | before | after | |
| --- | --- | --- | --- |
| Series 7, 8 sim s | 6.93 s | 6.02 s | **+13.2%** |

Three interleaved runs each, one directory per build. This is the largest
single win since the LCD blit, and it came from deleting code rather than
adding any: the removed decode was pure overhead left behind by an earlier
fix, which is the most reliable kind of speedup there is.

State hash identical over 2,218 (Series 7) and 233 (netBook) sample points
against the interpreter, screenshots bit-identical, 25/25 boot-suite devices
pass.

### A fifth round: measuring the burst engine instead of guessing at it

Two more experiments went in on the strength of a profile, and both were worth
roughly nothing:

| attempt | measured |
| --- | --- |
| burst cap 16 → 64 instructions | ~1%, inside noise, and it slips the per-batch hook cadence — reverted |
| drop the dead `DecodedPage::kind[]` (1 KB/slot) | performance-neutral; kept for the 262 KB of heap and the dead code it removes |

The burst-cap result was the useful one, because it said the cap was not the
binding constraint. So the next step was to stop guessing and count. A
compile-time-gated counter block (`PSION_PAGELOOP_STATS`) in `tickPageLoop()`
and `resolveFetchPage()`, over an 8-second Series 7 boot:

```
entries=26,898,663  insns=192,837,464  insns/entry=7.17
  entry-resolve-fail=8,538,478  in-loop-reresolve=24,766,200
  exits: slow=1,517,041  reresolve-fail=7,755,833  budget-insns=9,087,017

resolveFetchPage: calls=42,251,564  ok=26,135,258
  fail: perm-not-cached=16,010,487  xlate-fault=1  no-host-ptr=0
```

One page resolution per 4.5 instructions, and **38% of them failed** — all for
the same reason, and not the one anyone would have guessed. Translation was
fine (one fault in 42 million). The host pointer was always there. The only
thing missing was the permission-cache byte, and `resolveFetchPage` responded
to a cold byte by giving up on the page entirely.

`checkPermsfast()` backfills that byte, but only the **data** paths call it, so
a code page that was executed and never read as data stayed permanently
unresolvable: the burst engine bailed to `tick()`, `tick()` fetched through the
slow path, and the next burst bailed again — for the life of the page.

Filling the byte in instead of giving up (the authoritative walk, on first touch
of a sub-page, proceeding only if it returns `NoFault` *and* the byte it wrote
agrees):

```
insns/entry            7.17  →  14.52
entry-resolve-fail     8.4 M →  105,819
in-loop reresolve-fail 7.7 M →  0
perm-not-cached fails 16.0 M →  0
resolveFetchPage calls 42.3 M → 26.6 M
```

The burst engine now retires twice as many instructions per entry and
essentially never fails to resolve a page.

**And it is worth about 3%.** Native Series 7, six deterministic runs per build
with `PSION_RTC_SEED=0` pinning `executed_cycles` to the digit: 105.2 → 108.5
MHz median. In WASM it is inside the noise (5.62 → 5.64 s median, twenty
interleaved runs).

That is the finding, not a disappointment to be explained away: **the failed
resolves were cheap.** A failure costs an MRU-hit translation and a few masks,
and then `tick()` executes the instruction anyway. Halving the work the burst
engine wastes barely moves the total, because the burst engine's per-instruction
advantage over `tick()` is itself modest — which is the same conclusion the
`pcHistory` and `-flto` experiments reached from other directions, and the
clearest statement yet of why the remaining factor needs a code generator rather
than a faster interpreter.

Kept anyway: it is cycle-for-cycle identical (`executed_cycles` matches to the
digit), it removes 16 M redundant page-table walks per boot, and Stage 3 needs
a burst engine that actually engages on every code page — a codegen that only
ever sees the pages the permission cache happened to warm is not a codegen.

### A sixth round: counting the memory path, and four more that did not work

The same counter treatment applied to `readVirtual`/`writeVirtual` and
`execBlockDataTransfer`, over the same 8-second Series 7 boot:

```
reads=46,848,907  fast-path=32,129,852 (68.6%)  authoritative=14,612,817
  read fast-path misses: mru-mismatch=8,430,158  no-host-ptr=5,140,648
                         perm-not-computed=968,098  mru-null=73,913
writes=22,075,251  authoritative=1,237,957 (5.6%)
ldm/stm=8,442,067  words=28,993,120  words/op=3.43  page-crossing=2,131
```

Three things fall out of that, and each pointed at an experiment:

**The read fast path misses 31% of the time**, and 8.4 M of those misses are
plain conflicts in an 8-entry direct-mapped table indexed by `va>>12`. Widening
it to 64 ways cut conflict misses to 5.7 M — and throughput went 111.9 → 110.7
MHz, i.e. slightly *worse*. The miss path is not expensive: `translateFast`
almost always hits the fast TLB (page-table walks over the whole boot: **zero**
reached `translateAddressUsingTlb` from this path), so an "MRU miss" costs a
call and a repeat of checks the fast path had already done, not a page walk.

**5.1 M reads have no host pointer at all** — memory-mapped device registers,
which have to go the long way round by definition. Nothing to win there.

**LDM/STM is 42% of all memory traffic** — 29.0 M words across 8.4 M ops — and
a block crosses a page in 2,131 of those 8.4 M. That is as close to a free lunch
as a profile ever offers: resolve the page once per instruction instead of once
per word, and ~20.5 M virtual `readVirtual` calls disappear. Implemented as
`blockReadBase()` (one 1 KB block, so a single sub-page permission byte governs
every word; nullptr falls back to the per-word path unchanged). Native: 114.0 →
113.5 MHz, nothing. WASM, where a `call_indirect` is genuinely dearer than an
x86 indirect call and where this should therefore have paid: **5.48 → 5.71 s,
4% worse.** Reverted.

`-fno-threadsafe-statics` on the WASM build (every `PSION_ENV_*` gate is a
function-local static, so each use is a guard load and branch): 5.67 → 5.65 s.
Noise. Not taken.

**The pattern is now unambiguous.** Nine contained optimisations aimed at the
memory path and the burst engine have been measured across three rounds:

| | measured |
| --- | --- |
| widen the resolved fetch span 1 KB → 4 KB | nothing |
| cache the last four page resolutions | 7% worse |
| `readVirtualFast`, register return instead of sret | 5% worse |
| strip the per-instruction `pcHistory` diagnostics | +0.4% |
| WASM `-flto` | nothing |
| burst cap 16 → 64 instructions | ~1%, inside noise |
| drop the dead `DecodedPage::kind[]` | neutral (kept: 262 KB of heap) |
| widen the data MRU 8 → 64 ways | 1% worse |
| batch LDM/STM through one page resolution | 4% worse in WASM |

The one contained change that did pay in this whole sweep paid because it
deleted work that was **redundant**, not because it made work cheaper: the
second `decodeKind()` per instruction, +13.2%. Everything aimed at making the
*remaining* work cheaper has come back at zero or negative, and the reason is
consistent across all of them — the interpreter's per-instruction cost is not
overhead sitting on top of the real work, it very nearly *is* the real work.

Where that leaves the Series 7 and netBook, measured rather than modelled: ~110
MHz of emulated SA-1100 clock natively against a 221 MHz part, so **~0.5× real
time while the guest is actually busy**. Boot as a whole runs at 1.6× real time
only because it is 73% idle and idle is fast-forwarded — which is exactly why
booting feels fine and Pac-Man does not.

Closing a 2× gap does not need a heroic JIT. It needs the thing all of this has
been circling: instructions that are decoded once and re-executed from a block
cache, rather than re-derived from the word every time. That is Stage 3, and
this round is the evidence that nothing short of it is left.

### Three things that did not work

Worth recording, because the reasoning behind each was sound and all three were
wrong. The pattern: a function's *self time* in a profile is not recoverable
time — much of this work is intrinsic, not overhead.

| attempt | expectation | measured |
| --- | --- | --- |
| widen the resolved fetch span 1 KB → 4 KB | fewer re-resolves | 9.34 → 9.32 s, **nothing** |
| cache the last four page resolutions | branch-and-return hits | 9.32 → 10.0 s, **7% worse** |
| `readVirtualFast`, register return instead of sret | ~12% of runtime | 8.98 → 9.42 s, **5% worse** |

The sret was real — the generated wasm confirms `readVirtual` takes four i32
params and returns nothing, while `writeVirtual` returns `i64`. Removing it
still lost, because the new accessor is another `call_indirect` and the fast
path it duplicates has to be re-run on a miss. Re-resolves turned out to be
driven by *branches* leaving the page, not by straight-line code crossing a
boundary, so span width barely matters.

The memory path is still ~21% of the WASM profile and still the JIT's ceiling.
It has simply not yielded to anything contained. What is left there is
structural: making the accessors non-virtual and inlinable, which means moving
the host-pointer/permission cache up into `ARM710` rather than bolting another
entry point onto `SA1100Bridge`.

## Stage 3 — WASM code generation

### Sizing the prize before building anything

Callgrind says operand extraction and call marshalling (`arm710.cpp:2788-2790`)
are **46% of `tickPageLoop`'s self cost**, and `tickPageLoop` is 38.7% of the
WASM profile. Read naively that is 18% of runtime sitting in bit-field
extraction, and it makes a predecoded-operand block cache look irresistible.

It is wrong, and it is wrong in a way worth naming: **callgrind counts
instructions, not cycles**, and shift-and-mask extractions are the cheapest
instructions there are — no dependencies, no memory, four per cycle.

Measured instead of inferred. Each probe computes a second, identical copy of
one per-instruction cost, feeds it to an empty `asm volatile` consumer so the
compiler can neither CSE it against the real one nor delete it, and the
slowdown is that cost's true marginal price (native Series 7, ten interleaved
deterministic runs per build):

| per-instruction cost | marginal price |
| --- | --- |
| one full set of 9 operand extractions | **3.8%** |
| one 6-argument call/return | **5.8%** |

So a predecoded block cache — whose entire purpose is to delete the first row —
has a ceiling of about 4%, not 18%. That is the same size as everything else
tried in rounds three to six, and it would have cost a week. **The interpretive
framework as a whole (extraction, dispatch, call, condition check, prefetch
shuffle, `pcHistory`) is worth somewhere near 17-20% in total** — remove every
last bit of it and the result is ~1.25x, not the 2x that is needed.

### Which means the win has to come from inside the handlers

The other ~80% is `execDataProcessing`'s barrel shifter and flag computation,
`execSingleDataTransfer`'s addressing, `execBlockDataTransfer`'s loop. An
interpreter pays all of that generically because the instruction is a *variable*.
A code generator pays almost none of it because for one specific instruction
word the shifter type and amount are constants, the opcode is a constant, the
register numbers are constants, and — this is the big one — the flags are often
**dead**:

```
insns=144,508,962  data-processing=68,826,862 (47.6% of all executed)
  of which S=0 (flags computed and thrown away): 44,726,573 — 65%
```

`ADD r1, r2, r3` compiled is four wasm instructions: load, load, add, store.
Interpreted it is a six-argument call, nine extractions, a switch through the
barrel shifter, four flag computations that 65% of the time nobody reads, and a
switch on the opcode. That ratio — not the dispatch overhead — is where 2x
lives.

### The shape the code generator has to take

Two more measurements decide the architecture:

```
distinct block-start PCs = 115,409   block executions = 45,182,950
  → amortisation 391x
mean basic block = 3.20 instructions
  (1:6.9M  2:13.8M  3:11.3M  4:4.4M  5:3.4M  6:1.9M  7:1.5M  8+:2.0M)
```

**391x amortisation** says compiling is affordable — a block start is executed
391 times on average, so even an expensive compile pays for itself.

**3.20 instructions per basic block** says the unit of *execution* cannot be the
basic block. A generated function entered for 3.2 instructions is dominated by
its own call overhead — and 5.8% is what one call per instruction costs, so one
call per 3.2 instructions is not obviously better than the interpreter. Blocks
must chain: a block's terminating branch has to reach the next block's code
directly rather than returning to a dispatcher.

That rules out the architecture the existing seams were written for — the
comment on `getRegFileAddr()` imagines "a JS-side JIT", but a JS-side dispatcher
crossing the JS/WASM boundary every 3.2 instructions cannot win. The generator
runs in C++, emits a module, and hands the bytes to a small JS helper that
instantiates it against the emulator's own memory and table; the exported
function lands at a table index, which in Emscripten *is* a C function pointer,
so C++ calls it with an ordinary indirect call and no boundary crossing at all.

### The first subset, and why it is drawn where it is

Instructions the first generator accepts: data-processing, immediate or
immediate-shifted-register operand, `Rd != 15`, `Rn != 15`, `Rm != 15`, no
register-specified shift amount. Everything else terminates the block.

That subset is not arbitrary — every exclusion removes a hazard:

* **No `r15` anywhere.** During execution `GPRs[15]` is the executing address
  plus 12, and the handlers compensate by -4 when `r15` is read as an operand.
  Excluding `r15` removes the entire prefetch-convention hazard from the first
  generator.
* **No register-specified shifts.** Those cost an extra cycle, so the compiled
  cycle count would have to be computed rather than constant-folded.
* **No memory access.** No faults are possible, so a block can never exit early
  and its post-state is entirely constant-foldable.

With those exclusions each accepted instruction costs exactly one cycle and the
block's exit state — `GPRs[15]`, `prefetch[0]`, `prefetch[1]`, `prefetchCount`
— is known at compile time and can be stored as constants.

**Interrupts are the one thing that cannot be constant-folded.** The interpreter
calls `samplePendingExceptionsFast()` before every instruction, and it is
`if (pendingIRQ || pendingFIQ) slow()`. A block that runs N instructions without
checking would deliver interrupts up to N instructions late — precisely the
class of divergence that cost three debugging rounds in Stage 1. So generated
code loads those two bytes inline before each instruction and bails to the
interpreter if either is set. Two loads and a branch, no call.

### Validation

The state-hash harness is native and generated WASM is not, so the ladder is:

1. **The emitter** is testable natively: emit a module, check it with
   `WebAssembly.validate`.
2. **The compiler** is testable natively and differentially: for a given ARM
   word and register file, run the generated function in node and run
   `execDataProcessing` in the harness, and compare every GPR and every CPSR
   flag. This is the level at which codegen bugs are cheap to find.
3. **The dispatcher** is validated in the WASM build the way everything else in
   this document was: same workload with the JIT on and off, diff the state
   traces, and let the first differing line name the instruction.

Default OFF (`PSION_JIT=1` to enable) until step 3 is clean on the Series 7 and
the netBook.

### What is built, and the measurement that says what to build next

Landed and validated:

* `core/wasm_emit` — a WASM module writer.
* `core/arm_jit` — the block compiler for the subset above.
* `tests/unit/arm_jit_test{.cpp,.mjs}` — **91,520 cases**, each run once through
  `ARM710::execDataProcessing` and once through the generated WASM from the same
  register file, comparing every GPR and every CPSR flag. Every opcode, both
  operand forms, all four shift types at 0/1/7/31, S both ways, eight
  conditions, five register seeds, all 16 NZCV states, and 8,960
  multi-instruction blocks of length 2 to 8. It caught a real bug on its first
  run: the arithmetic V computation lands the overflow in bit 31, but V is bit
  28, so masking without shifting first produced a V flag that was never set.

Then the same discipline applied to the dispatcher, before writing it — how
much of the executed stream does this subset actually cover, and in what run
lengths? Over an 8-second Series 7 boot:

```
accepts() covers 58,924,180 of 144,508,962 (40.8%)
  runs=38,793,441  mean=1.52
  1:26.9M  2:7.0M  3:3.0M  4:1.1M  5:0.4M  6:0.2M  7+:0.13M

+ LDR/STR + LDM/STM: 103,308,844 (71.5%)
  runs=39,626,692  mean=2.61
```

**Mean run 1.52, and 69% of runs are a single instruction.** A generated
function entered for 1.52 instructions loses: one call costs ~5.8% per
instruction, so a call every 1.52 instructions spends ~3.8% of runtime on
dispatch alone, against a body that saves perhaps 8-10%. Widening to memory
instructions only reaches 2.61.

So **a block compiler that stops at the first instruction it cannot handle is
not worth dispatching to**, however correct it is. That is not a defect in the
code generator — the generator is right, and the tests prove it. It is a
statement about what the *region* selector has to be.

### Stage 3b — region compilation (landed, opt-in, DEFAULT OFF)

Two changes turn this into something that can pay, and the coverage histogram
is what identifies them:

1. **Do not terminate at a branch — follow it.** Inside a resolved page a
   direct branch target is a compile-time constant, so a backward branch
   becomes a wasm `loop` and a forward conditional becomes a wasm `if`. The
   unit of compilation stops being the basic block (mean 3.20) and becomes the
   loop body, which is where the 391x amortisation is concentrated.
2. **Do not terminate at an unsupported instruction — bridge it.** Emit a call
   back into the interpreter for that one instruction and carry on compiling.
   A region then spans whatever a loop contains, and only genuinely hard
   instructions cost a call — which is exactly what the interpreter already
   pays for them, so the bridge is free relative to today.

With both, region length is bounded by control flow rather than by the first
gap in the instruction subset, and the 47.6% of executed instructions that are
data processing get compiled into four wasm instructions apiece instead of a
six-argument call.

Worth noting what this reframes: the memory path, which resisted four separate
contained optimisations in rounds three to six, becomes tractable inside
generated code. Every one of those attempts failed because it *added* a call —
`readVirtualFast` was a second `call_indirect`, `blockReadBase` another. Inlined
into a generated block, the MRU check, permission-byte test and host-pointer
load are straight-line wasm with no call at all, and only a miss calls out.

### The dispatcher, and four things that were wrong with it

Getting a compiled region *called* turned out to be harder than compiling it,
and every problem was in the integration rather than the generated code. A
microbenchmark settles that up front: a 24-instruction region, called through a
cross-module `call_indirect`, runs in **15 ns** — 0.6 ns per emulated
instruction, against roughly 18 ns for the interpreter. The code generator is
not the bottleneck and never was.

**1. One module per region does not scale.** The first working dispatcher
compiled a `WebAssembly.Module` per region. It ran the host out of an 8 GB heap.
With a hotness threshold added it survived, and was **4.5x slower than the
interpreter** — and the profile said why: **62.8% of runtime inside
`WebAssembly.Instance`**, ~4.4 ms per instantiation. Regions are now linked in
batches of 128 into one module, which turned thousands of instantiations into
tens.

**2. Growing the table per region deoptimises the whole emulator.** Every
`wasmTable.grow()` invalidates the engine's indirect-call state for the entire
module, so growing it once per region repeatedly threw away the optimised code
for all 1.1 MB of emulator. Measured at ~13 us per region *entry*. The table
range is now reserved once.

**3. The slot table aliased.** Indexing by `(va >> 2) & (kSlots - 1)` maps only
an 8 KB window of address space to distinct slots; guest code spans hundreds of
KB. 1.07 M evictions against 1,024 live regions. A multiplicative hash over
16,384 slots took that to 66 K.

**4. Cold code must not be compiled at all.** An entry point has to be hit 64
times before it earns a region. Over a Series 7 boot there are 115,409 distinct
block starts against 45.2 M executions, so waiting costs almost nothing.

### Two real code-generation bugs, and how they were found

Both were found by running every region against the interpreter on the real
workload (`PSION_JIT_CHECK`) — not by the unit test, which passes 90,912 cases
and did not catch either.

**A computed jump that lands on the next instruction.** The Series 7 ROM
dispatches into an unrolled byte-copy with

```
50051a30: ADD r12, pc, #0x78
50051a34: SUB pc, r12, r2, LSL #3
```

The region bridges the second instruction and then checks whether the
interpreter moved the PC. For one value of `r2` the computed target is exactly
`pc+12` — the same address a fall-through reaches — so the PC test said
"nothing happened". But writing `r15` set `prefetchCount` to 0, and the
interpreter owes two pipeline-refill ticks before it can execute again. The
region carried on without them and ran two instructions ahead, at 2 cycles each
instead of two refills' worth of 1. The check reported it as exactly a 2-cycle
gap. **A bridge now tests the pipeline count as well as the PC.**

**A bridge that changes processor mode.** A region is compiled against one
resolved code page, and that resolution is permission-checked for one privilege
level — `tickPageLoop` re-checks `fp.priv != isPrivileged()` before every
instruction. A region cannot re-resolve, so after a bridged `MSR`, `MRS` or
`MOVS pc, lr` it may keep reading instructions from a page the guest is no
longer allowed to fetch from, where the interpreter would take a permission
fault. **A mode-changing instruction is still bridged, but now ends the trace.**

Finding the second one first required fixing the *check*: `MOVS pc, lr` reloads
CPSR from the SPSR and switches bank, and a register snapshot captures neither,
so marking it re-runnable did not merely weaken the check — it made it unsound.

### Two dispatcher bugs, and the workload that found them

The self-check (`PSION_JIT_CHECK`) reported clean on a Series 7 boot while the
run itself was visibly wrong — finishing six times too fast, which is what a
divergence looks like from outside: the guest wanders into an idle loop and
`executeUntil` fast-forwards, so wall time collapses while simulated time still
reaches its target. Two things had to be built before the cause was reachable.

**A region stamps the address it was compiled for.** With that in place, every
one of the 29 divergences turned out to be the same thing: *a slot serving a
region compiled for a different address*.

**Bug 1 — a slot changes hands between the compile and the flush.** Regions are
batched, so a compile only records its table index; the index is written into
the slot when the batch lands. In between, another address hashing to the same
slot can claim it — and the flush then stamped the old occupant's index onto the
new one. The new occupant's own tag and word-hash both agreed, so nothing else
caught it, and it ran entirely the wrong instructions. A pending region now
carries the address it was compiled for and the flush skips a slot that has
moved on.

**Bug 2 — a recompiling slot stays runnable with its old code.** When a region
goes stale its replacement is compiled immediately and the slot's validity
metadata — token, word hash, span — is updated there and then, while the code
only arrives at the next flush. Until then every check passed (right tag, right
token, and a hash that had just been updated to match the new words) and the
OLD compiled code ran. Guest code loaded from a card lives in RAM and is
rewritten, so this fired on an application workload and never on a ROM-only
boot. A recompiling slot is now unrunnable until its code lands.

That second one is why the bench grew `--card`: a boot is a poor workload for
anything that has to amortise a cost, and it is also ROM-only, so it cannot
reach a whole class of bug. And `FRAMEBUFFER:` — an FNV of the final LCD
contents — is the end-to-end check the register-level self-check cannot be:
one proves each region right in isolation, the other proves the whole run
landed in the same place.

### Making a bridged instruction cheaper than the interpreter's own

A bridge used to call `ARM710::tick()`, which re-fetches the instruction
through the MMU and shuffles the prefetch pipeline — both of which the burst
engine exists to avoid. A bridged instruction therefore cost MORE than simply
interpreting it, and since a region is roughly 60% bridges, more regions meant a
slower emulator.

`ARM710::jitExecOne(insn)` is the burst loop's per-instruction body and nothing
else: no fetch, no shuffle, no r15 advance. The region already holds the word as
a compile-time constant and maintains the pipeline only at its exits, so the
only state a bridge has to write is r15. The full `tick()` form survives for
`DK_SLOW`, which ends a trace anyway and so is paid at most once per region.

### Where it actually stands

Honest status, with the measurements behind each line.

**Correct.** Framebuffer bit-identical to the interpreter on both workloads —
Series 7 boot and netBook running a Quartz card — and the self-check verifies
every re-runnable region against the interpreter on the real workload: 1.4 M
regions on the card workload, 317 K on the boot, zero divergences, zero wrong
regions. 91,372 differential cases in the unit test, including the exact
instruction sequences that diverged in the live emulator.

**Neutral at the default settings, and 2.7x away from where it wants to be.**

| Series 7 boot, 8 sim s | netBook + Quartz card, 30 sim s | |
| --- | --- | --- |
| 6.10 s | 6.05 s | interpreter |
| 6.09 s | 6.18 s | JIT, default tick cap |
| 6.33 s | **2.24 s** | JIT, `PSION_JIT_MAX_TICKS=2048` |

The application workload is 2.7x faster with a raised tick cap. It is not the
default because at that cap the netBook stops mounting its card and ends on the
bootloader screen.

### The thing standing in the way, named exactly

A region retires hundreds of tick-equivalents per `stepCpu()` call where the
burst engine retires at most sixteen. The SoC's batch loop samples
`cpu.getRealPC()` against literal OS addresses — `nbCfMountHook` alone does ten
such comparisons, and its own comment says it is "keyed on literal PCs from the
netBook v1.05(450) OS". A hook of that shape only ever fires if a sample lands
on the address it is watching for, and a region that runs five hundred ticks
without returning changes which addresses are observable at all.

This is not a defect in the code generator, and it is not a surprise: the Stage
3 seams note above says exactly this — the live hooks are "block-boundary
constraints or deopt points", and converting them to a PC-keyed table is "part
of the Stage 2 cleanup, not an afterthought". It was not done, and this is the
bill.

Measured tolerance, on the netBook with a Quartz card: identical up to a 64-tick
cap, divergent from 128. Series 7 without a card is identical to 2048 — it has
no CF hooks — which is what identifies the hooks as the mechanism rather than
anything about tick granularity itself.

**So the next step is not in the code generator.** It is to convert the live
exact-PC hooks in `core/sa1100.cpp` into a PC-keyed set the compiler can read,
and end a trace at any address in it. Then the tick cap comes off and the 2.7x
is available on the devices that need it. Everything else — bigger inline
coverage, LDR/STR with the memory fast path inlined — is worth less than that
and should wait for it.

### That was tried, and it is wrong

The conversion was built: 378 addresses extracted from every `== 0x50…`
comparison in `executeUntil` (a deliberate superset of what the hooks key on),
bound to the burst engine so a burst hands back before executing any of them,
with the batch loop then running its housekeeping on exactly that address. It
reproduces the bootloader-splash failure it was meant to fix, at *any* batch
length, and bisecting the table shows three separate quarters of it doing so
independently — so it is not one rogue address.

The reason is that the hooks were never the mechanism. Turning all eight of
them off (`PSION_NB_HOOK_MASK=0`) leaves the netBook + Quartz boot working at a
batch of 128 and failing at 4, exactly as with them on. What the raised tick cap
actually broke was the CF card's interrupt: it was not in
`nextSocEventCycle()`, so a guest sleeping on it woke on the next 64 Hz tick
instead — 3.46 M cycles late, every sector — and the faithful OS.IMG read went
from 0.63 to 31 simulated seconds. That is fixed (see
`docs/netbook-s7-performance.md` §9), and with it the generated code renders the
right framebuffer at every tick cap, self-check clean.

Stopping at hook addresses was still the wrong lever: it samples the hooks
**more** often, which is the direction the bottom of the cadence window fails
in, and it bought nothing at the top once the real bug was found.

What the window is worth was measured instead, and it is not small: raising
`kBatchTicks` and the burst cap from 16 to 128 — no code generator involved —
is **1.96x** on the netBook + Quartz workload and 1.06x on the Series 7 boot,
framebuffer bit-identical, boot suite green on all 25 devices. That is now the
default; see `docs/netbook-s7-performance.md` §8.

### And with the ceiling gone, it still does not pay

The tick cap is no longer what stands in the way, so what does is now measurable
rather than arguable. Over a netBook + Quartz run the dispatcher reports 375,123
entries running 13.7 M cycles of generated code — about 5% of what the guest
executes — at **36.5 cycles, or 18 instructions, per entry**. Raising
`PSION_JIT_TRACE_INSNS` from 24 to 64, 128 or 256 moves that number by nothing:
traces are not hitting the limit, they end at the first control-flow edge the
compiler will not follow. A further 944,764 bursts could not be entered at all,
because `jitEntryStateClean()` needs a full prefetch pipeline and half of all
bursts resume mid-refill after a branch.

Eighteen instructions is a basic block, and the burst engine now retires around
a hundred per entry. So a region covers a fifth of a burst, hands the rest back,
and pays a call to do it — which is precisely the arithmetic that said a
basic-block compiler could not pay, now confirmed on the real workload.

### Chaining, and what it settled

Both of those were built. A trace now follows an unconditional branch and keeps
going at its target, so the chain is materialised into one function at compile
time (`Layout::allowChain`; `Region::runs` carries the resulting non-contiguous
guest-address ranges so the dispatcher can still verify the words it compiled
from). And the dispatcher is offered at the first instruction boundary of every
burst rather than only before one, so the half of all bursts that resume with a
flushed pipeline stop being skipped.

They worked: bursts never seen 944,764 → 0, generated-code coverage 13.7 M → 68.5
M cycles, average span 18 → 26 instructions. Validated at 2,166,079 regions
verified against the interpreter with zero divergences, and 91,512 differential
cases including followed branches, chained loop edges and bridges on the far
side of a hop.

The run still got slower, and the reason is one line of the dispatcher's own
stats:

    [jit] compiled span avg=26.2 insns (58% bridged), 0.53 branches followed

`acceptsInline` takes data-processing instructions and nothing else, so every
load, store, multiply and halfword transfer is a `call_indirect` into
`psionJitExecOne` — the interpreter's own work with a call in front of it. With
`PSION_JIT_LOOPS_ONLY=1` and a raised tick cap the dispatch cost vanishes (3,244
cycles per entry, 25 M instructions in generated code) and the result is 4.04 s
against the interpreter's 4.04 s: dead even, at 29% bridged.

**So the next step is the inline subset: LDR/STR compiled against the fast TLB
rather than bridged.** Neither coverage nor call overhead is the constraint any
more; the generated code calling out for most of what it runs is.

### Which was then done, and it crossed over

Loads compile against the micro-TLB now (`Layout::mem`, filled from
`ARM710::jitMemSeam`), bridging on any miss so the interpreter still sees every
unmapped page, uncached permission, device address and unaligned word.
PC-relative loads fold to the constant they read, since both the address and the
word are known at compile time and the word lives in the page the region already
guards. And a trace stops at an unconditional write to r15, which was compiling
thousands of positions that could never run.

    bridged   58% -> 51% (loads) -> 38% (literals) -> 33% (PC-write stop)
    cycles/entry  32 -> 93

With dispatch amortised away — loops only, raised tick cap — the generated code
now runs the netBook + Quartz workload in **3.72 s against the interpreter's
3.92 s**, where before it was 4.04 against 4.04. It is the faster of the two
where it gets to run.

What is left is the fixed cost of entering and leaving a region, and stores,
which are now the largest bridged category.

### Stores, and the hash that did not need to exist

Stores compile inline too now, with the code-coherency drop emitted rather than
avoided — `physAddrFromTlbEntry` followed by `invalidateDecodePage`, about twenty
WASM instructions, which is what the interpreter's own write fast path carries
for the same reason. And the per-entry word hash is off by default, because
every site that replaces guest code host-side now calls
`ARM710::onGuestCodeReplaced()`; `Runtime::flushAll()` existed for exactly that
and had never been called from anywhere, which was a latent bug as well as a
0.4 s cost.

    bridged   58% -> 33% (loads, literals, PC-write stop) -> 19% (stores)
              7% inside loop regions
    bridged ldr/str: store=0

netBook + Quartz over eight interleaved runs: interpreter 3.91–4.40 s, JIT
4.01–4.22 s — level, inside the noise, from about 8% behind. With dispatch
amortised away, 3.81 s against 4.02 s.

The JIT stays default-off. Correct: 154,522 regions verified live with zero
divergences; 94,377 differential cases including 2,205 inlined stores checked
byte-for-byte on the memory they left; and under `PSION_JIT_CHECK_ALL`, whose
double-execution has a noise floor of its own, inlining loads and stores gives
**the same 7,477 divergences as a control with everything bridged** — it adds
none. What is left is the entry and exit sequence, not the instruction set.

### Profiled, at last

`node --cpu-prof` against a `--profiling-funcs` build, diffed between an
interpreter run and a JIT run, settles in one pass what several rounds of
reasoning had only narrowed — and says the first thing to know is that **at the
shipping hot threshold the generated code is 0.40% of runtime**. Every argument
about whether it beat the interpreter was being had over that slice.

With coverage turned up it is legible: the generated code runs 30 M instructions
in 87 ms — **2.9 ns each against the interpreter's ~16 ns** — and saves 446 ms.
Against that, **164 ms goes to V8 compiling and instantiating the modules and
72 ms to this compiler emitting them**.

So the lever is emitted bytes. The micro-TLB guard, the flush and the seven-store
pipeline write were being inlined into every region: 26.9 MB of WebAssembly for
6,093 regions. They are module-internal functions the batch shares now
(`kHelper*`), and a load's region-side code is ten instructions instead of sixty.
**26.9 MB → 15.5 MB.**

It still does not pay on this benchmark, and the arithmetic says why: a region
costs ~23 µs to compile and saves ~13 ns per instruction, so it must run ~1,800
instructions to break even. A netBook desktop's average compiled region runs
~3,000, and its profile is flat. Restricted to regions that loop — which clear
that bar comfortably — the JIT runs 3.81 s against 4.02 s.

So that is the policy now: **compile only regions with a loop edge**
(`PSION_JIT_LOOPS_ONLY=0` compiles everything), and **give a looping region a
2,048-tick cap** (`PSION_JIT_LOOP_TICKS`) while a straight-line one keeps the
caller's. A loop cut off after the caller's 128 ticks is the compile cost
without the benefit — 4.26–4.42 s against 3.94–4.05.

Six interleaved pairs on netBook + Quartz: interpreter mean 4.090 s, JIT mean
4.092 s. **Parity**, from four to eight per cent behind. Series 7 boot is parity
too; the ESHELL card image is slightly ahead. Every framebuffer identical, and
the benchmark's own run-to-run spread is ±7%, which is wider than anything left
to argue about. The next measurement that would say more is a CPU-bound
application, not another round against an idling desktop.

---

### Seams

Only worth starting once Stage 2 has moved the ceiling.

The seams already exist: `SA1100::Emulator::stepCpu()` is the dispatch point,
`ARM710::gprFileAddr()` / `cpsrAddr()` expose the register file as linear-memory
offsets a generated block can target in place, `stepOneInstruction` in
`wasm/main.cpp` is the interpreter-fallback primitive to validate blocks
against, and `nextSocEventCycle()` bounds a block chain.

Two facts worth having:

* The emulator runs in a Worker, where `new WebAssembly.Module(bytes)` is
  **synchronous at any size** — the 4 KB sync-compile limit is main-thread
  only. Generated modules need no async round trip.
* `core/sa1100.cpp` carries 281 exact-PC comparisons and `core/arm710.cpp`
  another 85. Most are diagnostics that can compile out, but the live ones —
  netBook timer-queue fixups, CF mount hooks, ROM patches — are block-boundary
  constraints or deopt points. Converting them to a PC-keyed table is part of
  the Stage 2 cleanup, not an afterthought.

Whatever the code generator emits gets validated the same way everything above
was: run it against the interpreter, diff the hashes, and let the first
differing line name the instruction.
