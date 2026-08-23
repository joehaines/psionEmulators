# netBook: booting the Quartz build

`roms/OS.IMG` is a netBook build of **Quartz** — EPOC's pen-oriented UI
(the `Qik*` / `Qikon` layer, the ancestor of UIQ) linked for netBook
hardware instead of the Crystal desktop the stock `OS.IMG` carries. It
boots on a real netBook off a CF card exactly like the stock image does,
and it now does the same here: the bootloader reads `D:\OS.IMG` through
its own medata/ATA driver, the machine comes up on the Quartz v6.0
splash and goes on to the Quartz app screen.

It did neither before this change, and both reasons are the same *shape*
of bug the EShell image turned up (see
[netbook-eshell.md](netbook-eshell.md)): places where the emulator had
bound itself to one particular OS build rather than to the machine.
Booting a third image for the same hardware found two more, and they
sat one behind the other — fixing the first got the machine to the
splash, and only then did the second have a chance to fire.

## What the image is

A 3,653,888-byte `EPOCARM ROM` image: the same 256-byte wrapper the
stock netBook `OS.IMG` carries, followed by an EPOC R5 ROM whose header
declares `iRomBase = 0x50000000`, `iRomSize = 0x00800000` and
`iKernelLimit = 0x80400000` — the stock netBook OS and the EShell test
ROM both stop their kernel region at `0x80300000`, and that one word of
difference is enough to move every kernel global.

It is a debug build. Its BSP narrates the whole boot over the netBook's
debug serial port (SA-1100 `Ser3`, kernel VA `0x58005000`) — board
markers, an SoC register dump before and after init, the Eiger ASIC
state, the PC-Card power-up — and it carries EPOC's kernel **debug
monitor**, the thing that prints `*** DEBUG MONITOR ***` and a
full register dump when the kernel faults. Both turned out to be the
tools that found the bug; see "How it was found" below.

Once booted it paints the Quartz v6.0 splash (the EPOC hand logo,
`Quartz`, `v6.0`) into a 240×320 portrait area of the netBook's 640×480
panel — Quartz is a pen-machine UI and lays itself out for a pen-machine
screen even on this hardware.

## Where it gets to

The image's own boot, all the way through:

| t | screen |
|---|---|
| 2 s | bootloader splash |
| 4–26 s | blank white — the OS booting, nothing drawn |
| 28 s | Quartz v6.0 splash (variance ~11200) |
| 40 s | **Quartz app screen** — a mostly-white content area under a status
  bar carrying the shift and on-screen-keyboard buttons, the clock, the
  free-memory figure and the mail/battery icons (variance ~450) |
| 60 s+ | still there, with the clock advancing |

Getting from the splash to the app screen was the second of the two bugs
below. Before that fix the machine parked on the splash: one unhandled
data abort about a fifth of a second after the splash was painted, into
the kernel's debug monitor, which waits for a keystroke on the debug
serial port that under emulation never comes.

## Bug 1: a UCB1200 mutex write aimed at one ROM's kernel data

`core/sa1100.cpp` clears a word at kernel VA `0x8000001C` every time an
Eiger SPI command completes. That address is where the Series 7
v1.05(254) and netBook v1.05(450) BSPs keep the UCB1200 SPI command
mutex; real silicon clears it from the UCB1200's SPI-complete IRQ,
which we don't run, and without the release the recording heartbeat's
mutex claim never succeeds. It fired from three places (the ASIC[0x40]
busy-window handshake, its legacy poll-counter twin, and the ASIC[0x4c]
codec-command path), each writing zero unconditionally.

`0x8000001C` is a fact about those two builds, not about the machine.
In the Quartz build the kernel keeps something else there: entry 5
(`IrqGpioEdge5`) of the kernel's interrupt-name table — a
**NULL-terminated** array of `TPtrC8*` at VA `0x80000008`, in interrupt-ID
order, that `Interrupt::IdFromName()` walks to turn a name into an
interrupt id. Zeroing entry 5 truncated that scan after four entries.

Everything downstream follows from that one word:

- `Interrupt::Bind(handler, "IrqExtCfCardIreq")` — the PC-Card driver's
  first socket-0 binding — walked the table, hit the NULL at index 5,
  and returned `KErrNotFound`.
- The driver treats a failed interrupt bind as fatal:
  `Kern::Fault("PCCARD-ARM", 4)`.
- The kernel entered its debug monitor, which took over the panel and
  sat waiting for a keystroke on the debug serial port. Nothing ever
  arrives there under emulation, so the machine spun in the monitor's
  "wait for a character" loop (86% of samples in two instructions at
  ROM `0x50008368`/`0x50008370`) behind a screen EPOC had cleared to
  white and never drawn on.

The fix is `releaseUcb1200CommandMutex()`: the release now only clears
the word **while it still looks like that flag** — the mutex is a 0/1
flag, and it reads 1 on both the Series 7 ROM and the stock netBook OS
every time the release fires, so requiring `value <= 1` keeps those two
byte-identical while declining to touch a ROM whose kernel keeps a
pointer (`0x80000408`) there. Same principle as `writeL1IfUnmapped()`:
supply what the hardware would have supplied, never contradict a
running kernel.

## Bug 2: a PC-Card hook fired at a literal PC from one ROM

With the panel lit, the machine painted its splash and then took a data
abort — a section translation fault dereferencing `0xe59ff018`, which is
not an address but an instruction word (`ldr pc,[pc,#0x18]`, the ROM's
exception-vector template). The ROM's own record of it, which the debug
monitor would have printed:

    Exception: Type 12  Code 5001cc60  Data e59ff018  Extra 5

`0x5001cc60` is `ldr r2,[r3]`, four instructions into a locale
string-collation loop:

    5001cc50   ldr r3, [pc, #164]     ; r3 = &TheCollationTable  (0x800001cc)
    5001cc54   ldr r3, [r3]           ; r3 = TheCollationTable   (0x50081b80)
    5001cc58   add r0, sp, #28
    5001cc5c   mov r1, r10
    5001cc60   ldr r2, [r3]           ; ← faulted, with r3 = 0xe59ff018

Tracing the loop's own loads showed the loop running correctly for
hundreds of iterations and then, on one pass, `ldr r3,[r3]` reading
**VA 0** instead of `0x800001cc` — the previous instruction's result
never reached r3, and a read of VA 0 returns exactly the vector word the
fault then choked on. Nothing had written the global, the translation was
correct, no exception landed between the two instructions, and neither
the memory fast path nor the decoded-op cache was in play (the boot
fails identically with `PSION_NO_MEM_FASTPATH=1`).

The emulator was writing the register itself. `nbCfMountHook` in
`core/sa1100.cpp` drives the netBook OS's PC-Card mount by reaching into
it at literal PCs — and one of them is:

```
if (pc == 0x5001cc54u) {                       // "caps handler"
    if (cpu.getGPR(3) != 0u) cpu.setGPR(3, 0u);   // force param_4 = 0
    return;
}
```

In the netBook v1.05(450) OS `0x5001cc54` is the entry of the medata
caps handler, and forcing its fourth argument to 0 makes the driver
re-run `OpenMediaDriver` after a card insert instead of short-circuiting
on `KErrNotReady`. In the Quartz build the same address is
`ldr r3,[r3]` in the middle of that collation loop, and the hook
destroyed the pointer the next instruction dereferenced. The hook was
gated on `isNetBookRom_ && cfCard.inserted()` — true for any netBook OS
image with a card in the slot, which is precisely the case it must not
fire in.

The fix is `netBookOsPcHooksValid()`: before any of that group of hooks
runs, check that the code they are aiming at is actually there. Six of
the hooked sites are verified against the image mirrored into `ROM[]` at
the OS handoff — four function prologues, an `add r1,r0,r1,lsl #2` and
an `ldr r3,[sp]`. All six matching is conclusive; any mismatch stands the
whole group down, with a line in the log saying so. Latched once and
re-armed at each handoff, so it costs nothing per instruction.

The general lesson is the same one as bug 1 and as the EShell fixes:
a literal address out of one ROM is a fact about that build, and the
emulator has to prove the build before acting on it.

## How it was found

Worth recording, because the image hands you the tools.

`PSION_UART_TX=1` echoes the guest's UART3 transmit bytes to stderr.
(It used to echo only while nothing was attached to the port, and
`netBookLoadOsFromCard()` marks UART3 host-attached at the OS handoff —
so on the one boot path worth watching the knob silently did nothing.
It now echoes either way; the bytes still reach the Remote Link queue.)
That is what showed the boot narration stopping mid-stream, right after
the PC-Card power-up line:

    [R42]KE[pb 08=0000 12=0080 ->08=0008 12=0000 cd=0000 vs=0303]

The stall was in the ROM's serial `PutChar`, waiting on a received
character — EPOC's debug monitor asking for its password with nothing
on the other end of the cable. Feeding the port a character got as far
as a `Password: ` prompt; the monitor compares the typed line against a
literal in the image, and in this build that literal is `replacement`.
Typing it prints the banner, the fault category (`PCCARD-ARM`), the
fault reason, and the saved registers of every processor mode — from
which the faulting `Kern::Fault` call site, and then the failing
`Interrupt::Bind`, and then the NULL table entry, fall out in order.

Two other knobs did the rest: `--fork-repl` / `--repl-at-cycle` in
`harness/run` (fork a child at a cycle and `peek`/`run` in it, so
bisecting *when* a word changed costs one boot rather than one boot per
probe), and `PSION_WATCH=<va>[-<va>]` for virtual writes.

Worth knowing for next time: `PSION_WATCH` lives in `ARM710::writeVirtual`,
which `SA1100Bridge` **overrides** — so on these machines it sees nothing,
and "no hits" means "not instrumented", not "no writes". Bug 2 was finally
pinned by logging the *reads* a given PC range issues, which showed the
same instruction reading two different addresses on two passes of the same
loop and pointed straight at the register being overwritten from outside
the guest.

## Testing

`tests/devices.txt` gains `netbook_quartz`: the v0.11 bootloader plus a
synthesised FAT16 card carrying `roms/OS.IMG`, read the faithful way —
the same path the browser takes for any netBook OS image.

Its variance gate is a **band**, not a floor, because the two bugs fail
on either side of the app screen's ~450:

- bug 1 leaves a blank white panel, variance 0 — caught by the floor;
- bug 2 parks the machine on the splash, variance ~11,200 — which a floor
  alone waves through, so the row also carries `--max-variance 2000`
  (a new harness flag added for exactly this).
