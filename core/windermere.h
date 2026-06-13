// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2019 Ash Wolf (WindEmu); modifications (c) 2024-2026 Joe Haines.
// Subject to the Mozilla Public License v2.0 (http://mozilla.org/MPL/2.0/).

#pragma once
#include "emubase.h"
#include "windermere_cpu.h"
#include "wind_defs.h"
#include "hardware.h"
#include "etna.h"
#include "vcfcard.h"
#include "audio_codec.h"
#include <array>
#include <cstddef>

namespace Windermere {

// Which member of the Windermere family this Emulator is acting as.
// All four devices share the SoC (Windermere = ARM710T + on-chip
// peripherals), but the ROM-level behaviour differs in ways that
// matter for audio gating and a few other emulator-side workarounds.
// Each per-device factory in device_registry.cpp calls setVariant()
// after construction so the audio code paths can branch explicitly
// rather than inferring device identity from runtime flags. New
// device-specific quirks should be guarded on `variant() == X`
// (or via the helper predicates below) so each device's behaviour
// stays self-contained and a fix on one variant can't accidentally
// regress another.
enum class Variant {
    Mx5,     // Psion Series 5mx (mask ROM v1.05(260))
    Mc218,   // Ericsson MC218 — rebadged 5mx, same SoC + ROM family,
             // but the touch-driven REC button takes a different
             // CONFG=3 code path that lacks the channel pointer in r5.
    Revo,    // Psion Revo (480×160 LCD, no CF slot, no Dictaphone key).
    Mx5Pro,  // Psion Series 5mx Pro — patched OS loaded from CF by a
             // separate bootloader Flash; codec channel struct has a
             // different layout so the +0x1c state probe doesn't read
             // the original 5mx 1-3 active phases.
};

class Emulator : public EmuBase {
public:
    // Composed ARM710 (T-variant on Windermere — 5mx, Revo, MC218 are all
    // ARM710T parts). Public so helpers like Etna and VCFCard, which take a
    // raw ARM710* in their constructors, can be wired to it directly.
    WindermereBridge cpu{this, true};
    ARM710 *getArmCpu() override { return &cpu; }
    const ARM710 *getArmCpu() const override { return &cpu; }

    // ── Per-device variant ────────────────────────────────────────────
    // Set by the per-device factory in device_registry.cpp immediately
    // after construction. The shared Windermere code reads this to
    // pick the right gate / workaround for the running device. Default
    // Mx5 keeps the original 5mx behaviour for legacy call sites that
    // construct directly without going through the factory.
    void setVariant(Variant v) { variant_ = v; }
    Variant variant() const { return variant_; }
    // Convenience predicates for audio gates. Mx5Pro is the patched-
    // ROM variant that needs the bootloader-pseudo-DFC fallback;
    // every other variant follows the strict 5mx-style gates.
    bool isMx5Pro() const { return variant_ == Variant::Mx5Pro; }
    bool isMc218() const { return variant_ == Variant::Mc218; }
    bool isRevo()  const { return variant_ == Variant::Revo;  }

    // PRT bit 12 (0x1000) is the LCD EL-backlight enable pin (see
    // diffPorts in windermere.cpp). EPOC drives it in response to
    // Fn+Space and tracks the backlight-on timeout itself, so reading
    // the live pin gives the frontend a feed that's already correct
    // for auto-off / Control Panel brightness / etc.
    bool getBacklight() const override { return (portValues & 0x1000) != 0; }

    uint8_t ROM[0x1000000];
	uint8_t ROM2[0x40000];
    uint8_t MemoryBlockC0[0x800000];
    uint8_t MemoryBlockC1[0x800000];
    uint8_t MemoryBlockD0[0x800000];
    uint8_t MemoryBlockD1[0x800000];
    enum { MemoryBlockMask = 0x7FFFFF };

    // Scratch backing for SoC registers we don't explicitly model. Reads of
    // unmapped registers return the last value written here (default 0); the
    // 5mx Pro bootloader stashes a DRAM scratch pointer in reg 0x218 during
    // its init pass and reads it back later, so without this it crashes. The
    // 5mx OS doesn't write-then-read any unmodelled register, so this is a
    // no-op for the existing devices.
    uint8_t scratchRegs[0x1000] = {0};

private:
    Variant variant_ = Variant::Mx5;
    uint16_t pendingInterrupts = 0;
    uint16_t interruptMask = 0;
    uint32_t portValues = 0;
    uint32_t portDirections = 0;
    // PWRSR bit 7 (LC2V4) — codec voltage regulator OK. Cleared here
    // initially; see FUN_5009ff64 for the dictaphone DFC's use of it.
    // PWRSR (Power/State Register) at 0x80000400. Bit 12 = CldFlg (cold-boot
    // flag) — set on a true cold boot (full power-off → on transition). The
    // 5mx Pro bootloader's Main() only takes the OS-load path when this bit
    // is set; without it, Main jumps straight to its idle/halt-wait loop and
    // never tries to read SYS$ROM.BIN from the CF card. The 5mx OS doesn't
    // condition any boot logic on this bit, so setting it is a no-op for
    // existing devices.
    uint32_t pwrsr = 0x00003080;
    uint32_t lcdControl = 0;
    uint32_t lcdAddress = 0;
    uint32_t rtc = 0;
	uint16_t lastSSIRequest = 0;
	int ssiReadCounter = 0;

	uint32_t kScan = 0;
	uint8_t keyboardColumns[8] = {0,0,0,0,0,0,0};
	int32_t touchX = 0, touchY = 0;
	bool penDown = false;
	// Tap latch: a tap that comes-and-goes while the kernel can't service
	// EINT3 (most visibly while a freshly-inserted CF card's FAT mount
	// briefly wedges the kernel with EINT3 masked) would otherwise vanish
	// without the pen driver ever sampling it. Key presses survive such
	// windows — the kernel queues them and they all land when it wakes —
	// but taps were silently lost, which is why "keys start landing
	// before touch works again" after a CF insert. updateTouchInput
	// defers the host's pen-up when the digitiser was sampled fewer than
	// kPenLatchMinSamples times during the touch (zero when the kernel
	// was wedged the whole time; near-zero when the host delivered the
	// down/up pair with no sim frames in between, as a browser tap does
	// while the frontend bursts frames through a CF mount): penDown
	// stays asserted (keeping EINT3 pending) until the pen ISR has taken
	// kPenLatchMinSamples X-channel conversions — enough of a sample run
	// for the driver's debounce to register a short tap (a 4-frame
	// stress-test tap measures ~6) — and only then is the release
	// delivered (see the deferred-release block in the tick loop).
	// penSamplesSinceDown counts completed X-channel (0xD0D3)
	// conversions since the last pen-down edge; a normal serviced tap
	// takes ~36, so its release is immediate and behaviour is unchanged.
	// kPenLatchTimeoutCycles bounds the latch so a tap can't land
	// absurdly long after the user gave up (e.g. if the kernel never
	// unmasks EINT3 again).
	bool penUpLatched = false;
	uint32_t penSamplesSinceDown = 0;
	int64_t penLatchDeadline = 0;
	static constexpr uint32_t kPenLatchMinSamples = 4;
	static constexpr int64_t kPenLatchTimeoutCycles = (int64_t)CLOCK_SPEED * 10;
	// PD bit 3 on 5mx is the CF bay-door sense line — independent of whether
	// a card is physically present. Default closed (0 == active-low asserted
	// == door shut); the UI has no door-open control, but keeping the state
	// separate matches real hardware and avoids surprising EPOC's
	// media-change handlers when only card-presence changes.
	bool doorOpen = false;

    Timer tc1, tc2;
    UART uart1, uart2;
	Etna etna;
	VCFCard cfCard;
	int cfProbeLogs = 0; // rate-limit for CF window access logging
	int cfIntLogs = 0;   // rate-limit for media-change interrupt logging
	bool mcintPrev = false;      // previous mediaChangePending() state
	int mcintTransitionLogs = 0; // rate-limit for MCINT transition logs
	bool cfIrqPrev = false;           // previous cfCard.irqAsserted() level
	int cfIrqTransitionLogs = 0;      // rate-limit for CF IREQ transition logs
	int fiqDeliveryLogs = 0;     // rate-limit for FIQ-delivery logs
	uint32_t fiqDeliveryCount = 0; // running total of FIQ deliveries
	// Per-cause FIQ/IRQ counters (one slot per pendingInterrupts bit) plus a
	// rolling 500 ms dump so we can distinguish "nothing firing" (CPU idle
	// in HALT, 0 interrupts/window) from "TINT firing but handler is
	// lightweight" (~32 IRQs/window at 64 Hz) during the 2 s CF-poll gap.
	uint32_t fiqCauseCount[16] = {0};
	uint32_t irqCauseCount[16] = {0};
	uint32_t irqDeliveryCount = 0;
	int eint3DispatchLogs = 0;   // rate-limit for EINT3 dispatch PC/LR logs
	int intMaskLogs = 0;         // rate-limit for INTENS/INTENC mask-change logs
	bool halted = false, asleep = false;
	// Edge-triggered MCINT request latched by attachCard() / detachCard() to
	// simulate the door-switch fire real hardware raises on an insert/remove
	// event. Consumed once the FIQ is acked via MCEOI.
	bool mcintEdgePending = false;
	// Experimental: route CF IREQ# (via ETNA::mediaChangePending) to this
	// pendingInterrupts bit. -1 disables routing (default). Used by the
	// harness to sweep candidate Windermere interrupt lines and identify
	// the one the 5mx BSP wires ETNA to.
	int cfIrqLine = -1;
	// Experimental: CF-IRQ mask re-enable strategy. See runCfReenableStrategy
	// in windermere.cpp for each mode's behaviour.
	//   0 = off (baseline; multi-sector reads fall through to the 2s
	//       socket-watchdog path)
	//   1 = intenc-block: drop INTENC=0x80 writes from the kernel dispatcher
	//       (lr=50011a04). Bit 7 stays enabled across dispatch; the CPU's
	//       own CPSR.I gate prevents re-entry during the handler.
	//   2 = deassert-user: re-enable mask bit 7 on the CF IREQ# 1->0 edge,
	//       but only when the CPU is in User32 mode (avoids re-IRQing into
	//       a kernel critical section).
	//   3 = deassert-nonirq: re-enable on the 1->0 edge when the CPU is NOT
	//       in FIQ/IRQ/Abort/Undefined mode (i.e. User or Supervisor).
	//   4 = intclear: re-enable on the next tick after ETNA IntClear clears
	//       bit 0 (the driver's deterministic ack point).
	//   5 = level: force mask bit 7 to stay set whenever the driver has
	//       asserted it once (clamps INTENC=0x80 as a no-op after first
	//       INTENS=0x80). Stronger than intenc-block — no LR check.
	//   6 = delayed: re-enable N=~100us after the 1->0 edge, giving the
	//       kernel dispatcher time to return to the pre-IRQ context.
	//   7 = user-reenable: continuously re-enable mask bit 7 whenever the
	//       CPU is in User32 mode and bit 7 has ever been armed. The first
	//       dispatches that lead to hangs are the ones that fire into
	//       kernel/IRQ context (pc=5000b568) because the kernel dispatcher
	//       can't tolerate nested CF IRQs. Re-enabling only in User mode
	//       guarantees the dispatch happens from a safe context.
	//   8 = dispatch-gate-user: pair any re-enable strategy (defaults to
	//       level / mode 5) with a filter that suppresses bit-7 dispatch
	//       unless CPSR.mode == User32. The pending bit stays set; the
	//       dispatch defers until the CPU returns to User.
	//   9 = tc2-accelerate: while CF IRQ# is asserted and mask bit 7 is
	//       off, fire TC2 ~50x natural rate inside the main tick loop.
	//       NTimerQ's 2s CF watchdog matures 50x faster => ~40ms/sector
	//       instead of 2s/sector. Doesn't touch IRQ routing so it can't
	//       hang the CPU the way bit-7 re-enable does.
	//  10 = end-of-dfc-reenable: intercept the end-of-DFC store at
	//       pc=0x5000f1b8 (the cleanup that clears DAT_5000f11c /
	//       0x80000820) inside FUN_5000f124 and re-enable mask bit 7.
	//       That store marks the exact moment the CF timer-queue DFC has
	//       finished its state-machine step and is about to return to
	//       idle, waiting for the next CF IRQ to advance. Without the
	//       re-enable the card's next IREQ# never reaches the CPU; with
	//       it, the 2 s socket-watchdog gap collapses into the natural
	//       3 ms TC2 cadence. Does NOT fix the CF throughput — the
	//       chicken-and-egg INTSR ACK path means FUN_500072cc sees the
	//       dispatch but never runs the bound handler.
	//  11 = rom-sync-call (ABANDONED — see windermere.cpp):
	//       attempted to invoke FUN_5000f124 directly via
	//       ARM710::callRomFunctionSync. The call fires and mutates the
	//       CF timer queue head on first invocation but doesn't advance
	//       sector_drains; subsequent calls are no-ops. Static analysis
	//       also shows FUN_5000f124 has zero BL callers in the ROM, so
	//       it's not a real standalone function entry. The primitive is
	//       still worth keeping in ARM710 for future work that finds the
	//       correct DFC body address.
	//  12 = cf-dfc-direct-call (DOES NOT YET FIX THROUGHPUT, but the
	//       addresses are now CORRECTLY identified and the direct call
	//       executes the DFC body). At stuck-state detection, locate
	//       DPcCardMediaDriverAta on the kernel heap (0x80309f20 in
	//       5mx v1.05(260) runs), pull its two TDfc iFunction pointers
	//       (CardIreqDfcFunction=0x500891c0 at driver+0x44, and
	//       TimerDfcFunction=0x5008914c at driver+0x30), and call
	//       CardIreqDfcFunction(driver) synchronously via
	//       ARM710::callRomFunctionSync. In practice the direct call
	//       executes (observed re-enabling EINT3 via INTENS from
	//       lr=50089200) but reads iCardStatus at driver+0xa8 as
	//       ECardIdle (0) — suggesting either the real driver lives at
	//       a different heap address in the Kern code paths we missed,
	//       or iCardStatus has a different offset in EKA1. The fix
	//       baseline is preserved; mode 12 is a scaffold for follow-up.
	int cfReenableMode = 0;
	uint32_t cfReenableDelayCyc = 0; // counter for mode=6
	bool cfLevelArmed = false;       // latched true after INTENS=0x80; gates mode=5
	// Mode 10: set by writePhysical when it sees the store at pc=0x5000f1b8
	// that clears DAT_5000f11c (end-of-DFC cleanup). Consumed by the
	// strategy function once the CPU has been continuously idle in the
	// null thread for ~5 ms (cfDfcIdleThresholdCyc) — indicating the CF
	// state machine has quiesced and further progress requires an
	// external IRQ we can safely re-fire.
	bool cfDfcCleanupSeen = false;
	int64_t cfIdleSinceCyc = -1; // passedCycles when null-thread idle started, -1 = not idle
	uint32_t cfMode10ReenableCount = 0; // rate-limit counter for mode-10 re-enable logs
	uint32_t cfMode11CallCount = 0;     // count of synchronous FUN_5000f124 invocations in mode 11
	int64_t cfMode11NextAllowedCyc = 0; // cooldown to prevent call-storms
	uint32_t cfMode12ReenableCount = 0; // retained; unused in cf-dfc-direct-call flavour
	int64_t cfMode12NextAllowedCyc = 0; // retained; unused
	// Mode 12 (cf-dfc-direct-call): direct-call the CF DFC body via
	// ARM710::callRomFunctionSync at stuck-state detection. Populated once
	// by scanForCfDfcs() and reused on every subsequent stuck event.
	uint32_t cfDfcDriverPtr = 0;           // DPcCardMediaDriverAta* (r0 arg)
	uint32_t cfDfcCardIreqFn = 0;          // CardIreqDfcFunction entry PC
	uint32_t cfDfcTimerFn = 0;             // TimerDfcFunction entry PC
	uint32_t cfMode12DfcInvocations = 0;
	int64_t cfMode12DfcNextAllowedCyc = 0; // cooldown to prevent call storm
	// Live-driver-this capture: the static heap object that scanForCfDfcs
	// finds (0x80309f20 in this ROM) is an uninitialised-enough placeholder
	// whose iCardStatus stays at ECardIdle=0, so direct-calling
	// CardIreqDfcFunction on it takes the idle-cleanup branch and never
	// drains. The LIVE driver instance lives at a different heap address
	// reached via ATA-command execution. We capture it by inspecting r4 at
	// pc=0x5005531c (our log's CF-io-write position; the actual strb is at
	// 0x50055314 and uses r4 as the base for a field load at +0x2c — i.e.
	// r4 is `this` at that moment). First capture wins; scanForCfDfcs then
	// uses this value instead of the static scan result.
	uint32_t cfDfcLiveDriverPtr = 0;       // captured from r4 at CF cmd write
	uint32_t prevIntClearCount = 0;  // tracks ETNA IntClear writes
	uint32_t prevAtaStatusReads = 0; // tracks VCFCard ATA Status reads
	uint32_t cfStatIrqAssertions = 0;
	uint32_t cfStatIrqDeassertions = 0;
	uint32_t cfStatEint3Dispatches = 0;
	uint32_t cfStatReschedulePokes = 0;
	// Deep-diagnostic window around the first CF EINT3 dispatch. When the
	// PSION_CF_DIAG env var is set at boot, we flip ARM710::cfDiagEnabled
	// on when cfStatEint3Dispatches transitions 0->1, count down
	// cfDiagCyclesRemaining, and flip it off. Gives us a focused trace of
	// the CF ISR path without flooding the log with boot-time noise.
	bool cfDiagEnabled = false;
	int64_t cfDiagCyclesRemaining = 0;

	// Workaround for a known kernel-fidelity bug: TDfc::Add() called from
	// the CF ISR context silently fails to wake the DFC thread in our
	// ARM710 / EKA1 emulator pairing. That leaves the CPU spinning in
	// the null-thread idle loop at 0x5000b55c with EINT3 masked off, and
	// only the 2 s socket-watchdog rescues the sector. When enabled, the
	// executeUntil() loop detects that stuck state and force-writes 1 to
	// the kernel's reschedule-needed flag at physical 0xd07e5878
	// (virtual 0x80000878). The IRQ handler at 0x50004980 checks this
	// flag on exit and, if set, runs the full rescheduler — picking up
	// the CF DFC thread. See the stuck-detection block in executeUntil.
	bool cfReschedulePokeEnabled = false;
	// --cf-fast-watchdog: attempt to accelerate the CF driver's
	// iBusyTimeout polling loop (67 iterations at 30 ms, or 400 at 5 ms
	// in the release build — total 2010 ms) by scaling TC2LOAD writes
	// from the FUN_5000aa38 setter down by kCfFastWatchdogDiv.
	// EMPIRICALLY A NO-OP for sector_drains (and a wall-clock
	// regression) because the per-sector 2 s gap is driven by an
	// instruction-count watchdog in guest memory, not by TC2 cadence —
	// see the long note at the TC2LOAD handler site in windermere.cpp
	// and commit b62bd3f. Kept as infrastructure (flag, counters) for
	// the next attempt that finds the correct timer-queue Add callsite.
	// Off by default; baseline (10 sector_drains / 15 s) preserved.
	bool cfFastWatchdog = false;
	static constexpr uint32_t kCfFastWatchdogDiv = 30;
	uint32_t cfFastWatchdogHits = 0;
	int cfFastWatchdogLogs = 0;
	// CF 2s-timer accelerator: the PCCARD-ATA socket callback at ROM
	// 0x50083178 arms a 2,000,000 us (2 s) safety-net timer on itself via
	// the kernel import trampoline at 0x500842fc (resolves at runtime to
	// the NTimer-queue Add at 0x5002250c). Four distinct BL sites arm
	// this same timer (0x50082508, 0x500825b8, 0x500831fc, 0x500832b8),
	// all with (r1=2,000,000, r2=0x50083178); intercepting the shared
	// trampoline entry catches every arm in one place. On real hardware
	// the CF IRQ rescues the transfer long before the 2 s fires; in our
	// emulator the IRQ->DFC wake path has a fidelity gap we have not yet
	// closed (see EMULATOR_FIDELITY_ANALYSIS.md), so every sector waits
	// out the full 2 s. Rewriting r1 to kCfAccelTimerUs rounds up to one
	// 64 Hz kernel tick (15.625 ms), turning 2 s/sector into
	// ~15 ms/sector — ~40x more sector drains over the same sim-time.
	// Guarded tightly on both r1 and r2 so other kernel timer work is
	// untouched. Defaults ON; the bug is structural and the workaround
	// is strictly local to the CF retry callback.
	bool cfAccelTimer = true;
	static constexpr uint32_t kCfAccelTimerUs = 100;
	uint32_t cfAccelTimerHits = 0;
	// ROM-specific PCCARD-ATA addresses resolved at loadROM time.
	// Two variants supported:
	//   5mx v1.05(260)    — callback 0x50083178, trampoline 0x500842fc
	//   5mx Pro v1.05(319) — callback 0x500a5108, trampoline 0x500a628c
	// The MC218 ROM is byte-compatible with 5mx at these offsets so it
	// picks up the 5mx values. Other variants leave these zero and the
	// hooks simply no-op, falling back to baseline CF behaviour.
	uint32_t cfRomCallbackAddr  = 0;
	uint32_t cfRomTrampolinePC  = 0;
	// CF direct-invoke accelerator: once the PCCARD-ATA retry callback's
	// `this` pointer is captured (first time PC hits 0x50083178 with a
	// valid RAM-range r0), we can synchronously invoke the callback from
	// emulator C++ via ARM710::callRomFunctionSync. This bypasses the
	// kernel's 64 Hz NTimer queue grain entirely — the ceiling left by
	// the r1-rewrite workaround — and drives a sector drain whenever the
	// card is asserting IREQ# but no emulator-guest code is about to get
	// to the callback. Guarded on gap-state (card inserted, IRQ
	// asserted, EINT3 masked) so we never force-drive the driver when
	// the natural path is making progress.
	//
	// Defaults ON: with the accel-timer alone every sector still costs a
	// full 64 Hz kernel tick, so a card insert wedges the whole OS for
	// ~6.5 s of sim time while the FAT mount drains (~390 sectors) —
	// taps during that window were lost and the desktop froze. With
	// direct-invoke the mount drains in ~1-1.5 s, close to the brief
	// pause a real 5mx shows on insertion. Invocations are gated on the
	// CPU being in UND32 (null thread) or USR mode — hijacking a live
	// ISR (IRQ32) wedged the 5mx Pro's bootloader→OS boot, where the
	// card is present from t≈0 and the OS re-mounts it mid-boot; see
	// the mode-gate comment at the invoke site and the 5mxpro_blboot
	// row in tests/devices.txt. Kill switches:
	// PSION_NO_CF_DIRECT_INVOKE=1 env or --no-cf-direct-invoke on the
	// harness (setCfDirectInvoke(false)).
	bool cfDirectInvoke = true;
	uint32_t cfDirectInvokeThis = 0;      // captured at 0x50083178 entry
	int64_t cfDirectInvokeLastCyc = 0;
	uint32_t cfDirectInvokeHits = 0;
	static constexpr int64_t kCfDirectInvokeInterval = 10000;  // ~270 us
	// Time budget: we only poke after the stuck state has been stable
	// for >= ~5 ms. Prevents firing on legitimate short idle windows
	// unrelated to CF.
	int64_t cfStuckSinceCycles = -1;        // -1 = not currently stuck
	int64_t cfNextPokeAllowedAt = 0;        // passedCycles at which next poke may fire
	// Tracks last sector-drain count so we can detect "no CF progress for
	// >= N ms" as an additional precondition. Avoids interfering with the
	// natural polling path while it's making progress.
	uint32_t cfLastDrainCountAtCheck = 0;
	int64_t cfLastDrainProgressCycles = 0;

	void runCfReenableStrategyEdge(bool cfIrqPrevLatched, bool cfIrqNow, int passedCycles);
	void maybePokeReschedule();
	void scanForCfDfcs();
	void maybeCallCfDfc();

	// Reflect each UART's masked interrupt state onto pendingInterrupts
	// UART1/UART2. Called after any UART register access (which can change
	// `interrupts` or `interruptMask`) and after host-side bridge calls
	// (which inject/consume bytes that flip IntRx/IntTx). Safe to call
	// repeatedly — purely a state mirror.
	void updateUartIrqs();

	// ── Audio codec ───────────────────────────────────────────────────────
	// Windermere's PCM codec (registers at 0xA00–0xA10, IrqCodec/CSINT).
	// Implemented as two ring buffers of normalised int16 samples:
	//   dacQueue — EPOC writes a CODR sample, we push it here, the host
	//              drains it to the browser speaker.
	//   adcQueue — the host pushes mic samples here, EPOC reads a CODR
	//              sample, we pop one off.
	// The register-level format (bits per sample, rate) is inferred from
	// whatever EPOC programs into CONFG. Until a live trace tells us more,
	// we assume 8 kHz / 16-bit mono linear PCM, which is what the ROM's
	// dictaphone driver expects on real hardware.
	//
	// CRITICAL: the real CPSR codec has a 16-entry hardware FIFO. EPOC's
	// dictaphone DFC (FUN_5009f4ec state 1) reads CODR in a while-loop
	// gated on COLFG bit 0 and stashes each sample into a 16-byte local
	// buffer (`local_24[4]`). On real hardware the loop can never see
	// more than 16 samples because that's all the FIFO will hold. If we
	// let COLFG advertise "not empty" for the full content of our (much
	// deeper) adcQueue, the DFC overruns its stack buffer and corrupts
	// the saved PC — every recording attempt reboots the OS. Model the
	// 16-entry watermark by capping how many samples we'll surface per
	// drain cycle via adcFifoReadsSinceRefill / kAudioFifoDepth.
	// Shared codec FIFO + buzzer state. Owns the sample rings, virtual
	// TX FIFO, deferred CSINT scheduling, buzzer pump and host enable
	// gates. Implemented identically with the CL-PS711x copy in
	// core/audio_codec.{h,cpp}; the chip-specific bits below (CONFG
	// register decode, BZCONT, recordChannelPtr, sawBootloaderPseudoDfc)
	// are how Windermere hands the model the predicates it needs.
	AudioCodecModel audio;
	uint32_t codecConfig = 0;   // CONFG mirror
	uint32_t codecLeftGain = 0; // COLFG mirror
	bool codecEnabled = false;    // PADR bit 0 (driven by guest)
	bool audioAmpEnabled = false; // PADR bit 1 (driven by guest)
	// dacHistory is a Windermere-only rolling diagnostic ring the
	// browser audio test scrapes via debugDacHistory*. AudioCodecModel
	// doesn't expose this because it's specific to this device's
	// debug harness.
	mutable int8_t dacHistory[1024] = {};
	mutable size_t dacHistoryHead = 0;
	mutable size_t dacHistoryCount = 0;
	// CLPS711x BZCONT — drives the system buzzer (keyboard clicks,
	// alarms, calculator beeps, app navigation sounds). Distinct from
	// the codec at 0xA00 which only handles the dictaphone. Bit layout:
	//   bit 0: BZTOG — manual buzzer state (used when BZMOD=1)
	//   bit 1: BZMOD — 0: TC1-driven tone, 1: manual via BZTOG
	// EPOC's "click on keypress" pattern is a brief BZCONT=1 / =0 pair
	// while TC1 is configured to a click frequency (~2 kHz).
	uint8_t buzzerCtrl = 0;
	// Channel-struct pointer captured at the moment FUN_5009fcb0 writes
	// CONFG=3 — its R5 holds the channel pointer thanks to the mov r5,r0
	// prologue at 5009fcb8. We use this to drive FUN_5009f4ec from the
	// emulator because EPOC's DFC scheduler never runs our codec DFC on
	// its own (post-IRQ drain hook at *DAT_50005174+0x10 is null in
	// RAM after boot — see docs/codec-recording-analysis.md).
	uint32_t recordChannelPtr = 0;
	// "Bootloader pseudo-DFC" mode: the 5mx Pro bootloader writes a 32-bit
	// CONFG=3 without any EPOC dictaphone driver around it (no channel
	// struct, no DFC queue), then expects the codec's CSINT to keep firing
	// as its virtual TX FIFO drains so its own CSINT handler can run its
	// wake callback. Flagging this state separately from the global
	// audioRegisterHandlingEnabled means the OS that loads AFTER the
	// bootloader can still re-enter normal EPOC-driver behaviour (writing
	// CONFG=3 with R5 holding a real channel pointer) without permanently
	// losing the audio path. Cleared on CONFG=0.
	bool bootloaderPseudoDfc = false;
	// Sticky companion: once the bootloader-pseudo-DFC path has ever
	// fired, we know we're running the 5mx Pro bootloader+CF-loaded OS
	// rather than 5mx's mask-ROM OS. The patched 5mx Pro v1.05(319) ROM
	// lays the dictaphone channel struct out differently so the kernel's
	// state-1 write never lands at +0x1c. We use this sticky flag to
	// allow a codec-config-based CSINT-firing fallback on 5mx Pro
	// recording paths while preserving the strict state==1-3 gate on the
	// original 5mx, where the broader gate caused recording to crash.
	bool sawBootloaderPseudoDfc = false;
	// Wall-clock cycle (matches `passedCycles`) at which the codec was
	// last activated (CONFG transitioned 0 → non-zero via V8 or V32).
	// Used by the tick-loop's sawBootloaderPseudoDfc-only fallback to
	// add a settling delay before firing the FIRST CSINT — the 5mx Pro
	// patched OS installs its CSINT handler AFTER writing CONFG=3 (the
	// inverse of stock 5mx FUN_5009fcb0 ordering), so firing CSINT
	// inside the handler-install window dispatches into an un-installed
	// handler chain and crashes the OS to splash. ~100 ms settling at
	// the 64 Hz TINT cadence works out to 6-7 ticks of headroom past
	// the handler-install window.
	int64_t codecOnAtCycles = -1;
	// Snapshot of audio.codrReads at the codec 0→non-zero CONFG transition.
	// On the Mx5Pro patched-OS recording path the edge-fired CSINT can be
	// missed during the ~100 ms handler-install settle window, so the drain
	// never starts. We "kick" CSINT after settle only until codrReads
	// advances past this baseline, then hand off to the edge+deferred path.
	uint64_t recordingCodrBaseline = 0;
	// True once installDfcDrainThunk() has written both the thunk bytes
	// and the hook pointer. Subsequent calls are still safe (they
	// rewrite the same bytes) but skip the log.
	bool dfcThunkInstalled = false;
	// Cycle-scheduled sample tick for CSINT generation. The codec is
	// considered "servicing" whenever codecEnabled && (CONFG.enable) and
	// the guest has enabled CSINT in interruptMask.
	bool codecIntLoggedOnce = false;

    uint32_t getRTC();

    uint32_t readReg8(uint32_t reg);
    uint32_t readReg32(uint32_t reg);
    void writeReg8(uint32_t reg, uint8_t value);
    void writeReg32(uint32_t reg, uint32_t value);

public:
	MaybeU32 readPhysical(uint32_t physAddr, ValueSize valueSize) override;
	bool writePhysical(uint32_t value, uint32_t physAddr, ValueSize valueSize) override;
	// PSION_TRACE_PERIPH=1 — emit a per-access trace line.  See
	// implementation in windermere.cpp.  Lives on the class so non-static
	// state (`log`, `passedCycles`, register values) can be reached.
	void tracePeriph(const char *op, uint32_t physAddr, int sz, uint32_t value);

private:
    bool configured = false;
    void configure();

    const char *identifyObjectCon(uint32_t ptr);
    void fetchStr(uint32_t str, char *buf);
    void fetchName(uint32_t obj, char *buf);
    void fetchProcessFilename(uint32_t obj, char *buf);
    void debugPC(uint32_t pc);
	void diffPorts(uint32_t oldval, uint32_t newval);
	void diffInterrupts(uint16_t oldval, uint16_t newval);
	uint32_t readKeyboard();

public:
	Emulator();
	void setPromDeviceName(const char *name) { etna.setDeviceName(name); }
	uint8_t *getROMBuffer() override;
	size_t getROMSize() override;
	void loadROM(uint8_t *buffer, size_t size) override;
	void executeUntil(int64_t cycles) override;
	int32_t getClockSpeed() const override { return CLOCK_SPEED; }
	const char *getDeviceName() const override;
	int getDigitiserWidth() const override;
	int getDigitiserHeight() const override;
	int getLCDOffsetX() const override;
	int getLCDOffsetY() const override;
	int getLCDWidth() const override;
	int getLCDHeight() const override;
	void readLCDIntoBuffer(uint8_t **lines, bool is32BitOutput) const override;
	void setKeyboardKey(EpocKey key, bool value) override;
	void updateTouchInput(int32_t x, int32_t y, bool down) override;

	// Plant a small ARM thunk into an unused zero region of the ROM, then
	// write its address into *(DAT_50005174+0x10) — the post-IRQ DFC drain
	// slot. The kernel's IRQ epilogue (FUN_50004980) reads that slot each
	// IRQ; if non-null it calls the hook to drive EPOC's DFC scheduler
	// forward. The kernel never populates that slot in our boot, so our
	// codec DFC (FUN_5009f4ec) never runs. The thunk loads the DFC
	// manager pointer from kernel RAM at virtual 0x80000c28 (DAT_5001b9dc's
	// target), calls FUN_5001ba70 with it, and returns 0 (no reschedule).
	//
	// Must be called AFTER the kernel has populated virtual 0x80000c28
	// (the DFC manager pointer) — typically anytime after boot completes.
	// Idempotent: repeated calls overwrite with the same bytes.
	// Returns false if the virtual-to-physical mapping for the slot is
	// not yet available (call again later).
	bool installDfcDrainThunk();

	// Write `recordChannelPtr` to the channel-ptr slot the DFC drain
	// thunk reads from (virtual 0x80000030). No-op if the thunk hasn't
	// been installed yet, or if the slot's virt→phys translation hasn't
	// been set up. Called from the CONFG write handler whenever
	// recordChannelPtr changes.
	void publishChannelPtrToThunk();

	// Debug: force the dictaphone channel state to `s` via the MMU. The
	// driver normally transitions states through FUN_5009fad8 (→3) and
	// FUN_5009fcb0 (→1), but the latter is triggered by a specific
	// Voice Notes UI interaction we can't easily synthesise from the
	// native harness. This bypass lets the harness exercise the DFC's
	// state-1 RX-drain path directly. Returns true if the write
	// reached the channel struct.
	bool debugForceChannelState(uint32_t s);
	// Push a half-second 440 Hz tone directly into the host DAC queue
	// so the user can verify the JS→WASM→worklet pipeline works
	// independently of the guest's BZCONT / codec activity. Useful
	// for "speaker doesn't work" reports — if this beep is audible,
	// the engine is fine and the issue is the guest not driving
	// audio; if it's silent, the host audio path is broken.
	void debugInjectTestTone();

	// Harness-only observability: lifetime counts of CODR traffic plus
	// a snapshot of the ADC / DAC ring fill levels.
	uint64_t debugCodrReads() const { return audio.codrReads; }
	uint64_t debugCodrWrites() const { return audio.codrWrites; }
	uint64_t debugCsintFired() const { return audio.csintFiredCount; }
	uint64_t debugTxDrainTicks() const { return audio.csintFiredCount; }
	uint32_t debugTxFifoLevel() const { return (uint32_t)audio.txFifoLevel(); }
	size_t debugAdcFill() const { return audio.adcRingFill(); }
	size_t debugDacFill() const { return audio.dacRingFill(); }
	uint32_t debugCodecConfig() const { return codecConfig; }
	uint32_t debugRecordChannelPtr() const { return recordChannelPtr; }
	// Rolling capture of the last kDacHistorySize CODR writes (int8
	// pre-scale values EPOC wrote, before our <<8 normalisation). Lets
	// the in-browser audio test inspect what the guest actually pushed
	// to the codec, independent of whether the host drained it. Index
	// ordering is oldest→newest.
	static constexpr size_t kDacHistorySize = 1024;
	size_t debugDacHistoryCopy(int8_t *out) const {
		size_t n = dacHistoryCount < kDacHistorySize ? dacHistoryCount : kDacHistorySize;
		size_t start = dacHistoryCount >= kDacHistorySize
		             ? (dacHistoryHead + kDacHistorySize) % kDacHistorySize
		             : 0;
		for (size_t i = 0; i < n; ++i)
			out[i] = dacHistory[(start + i) % kDacHistorySize];
		return n;
	}
	// Pick the backing MemoryBlock for a physical address. Returns
	// nullptr for non-RAM regions (I/O, CF, etc). HISTORICAL BUG:
	// before 2026-05-19 this code accepted any of {C0,C1,D0,D1} as the
	// region check but always read from MemoryBlockC0 — so reads of a
	// virtual address whose physical translation landed in the D0 bank
	// (the patched 5mx Pro OS puts the dictaphone channel struct
	// there) saw garbage from C0 instead of the real bytes. Symptom
	// was "Voice Notes REC press doesn't advance the record timer":
	// the kernel WAS setting channel-state to 1, but our gating logic
	// read 0x20202020 (some ASCII string that happened to sit at the
	// matching C0 offset) and concluded recording wasn't active.
	uint8_t *memoryBlockForPhys(uint32_t physAddr) {
		switch ((physAddr >> 24) & 0xFF) {
		case 0xC0: return MemoryBlockC0;
		case 0xC1: return MemoryBlockC1;
		case 0xD0: return MemoryBlockD0;
		case 0xD1: return MemoryBlockD1;
		default:   return nullptr;
		}
	}
	const uint8_t *memoryBlockForPhys(uint32_t physAddr) const {
		switch ((physAddr >> 24) & 0xFF) {
		case 0xC0: return MemoryBlockC0;
		case 0xC1: return MemoryBlockC1;
		case 0xD0: return MemoryBlockD0;
		case 0xD1: return MemoryBlockD1;
		default:   return nullptr;
		}
	}
	// Read 4 bytes (little-endian) from RAM via virt-to-phys translation.
	// Returns {} if translation fails or the target is non-RAM. Uses the
	// correct backing block for the physical region (fixes the
	// MemoryBlockC0-only bug described above).
	std::optional<uint32_t> readRamVirt32(uint32_t virtAddr) {
		auto p = virtToPhys(virtAddr);
		if (!p.has_value()) return std::nullopt;
		uint32_t phys = p.value();
		uint8_t *blk = memoryBlockForPhys(phys);
		if (!blk) return std::nullopt;
		uint32_t off = phys & MemoryBlockMask;
		return (uint32_t)blk[off]
		     | ((uint32_t)blk[off+1] << 8)
		     | ((uint32_t)blk[off+2] << 16)
		     | ((uint32_t)blk[off+3] << 24);
	}
	uint32_t debugChannelState() {
		auto v = (recordChannelPtr == 0) ? std::nullopt
		                                  : readRamVirt32(recordChannelPtr + 0x1c);
		return v.value_or(0);
	}
	// Raw peek of the D0 RAM block by physical address. The kernel page
	// tables map virtual 0x80000000 → physical 0xd07e5000 on 5MX, so
	// e.g. debugReadD0(0x7e5020) gives you virtual 0x80000020.
	uint32_t debugReadD0(size_t off) const {
		if (off + 3 >= 0x800000) return 0;
		return MemoryBlockD0[off] |
		       (MemoryBlockD0[off+1] << 8) |
		       (MemoryBlockD0[off+2] << 16) |
		       (MemoryBlockD0[off+3] << 24);
	}
	uint32_t debugReadC0(size_t off) const {
		if (off + 3 >= 0x800000) return 0;
		return MemoryBlockC0[off] |
		       (MemoryBlockC0[off+1] << 8) |
		       (MemoryBlockC0[off+2] << 16) |
		       (MemoryBlockC0[off+3] << 24);
	}
	uint32_t debugReadC1(size_t off) const {
		if (off + 3 >= 0x800000) return 0;
		return MemoryBlockC1[off] |
		       (MemoryBlockC1[off+1] << 8) |
		       (MemoryBlockC1[off+2] << 16) |
		       (MemoryBlockC1[off+3] << 24);
	}
	uint32_t debugReadD1(size_t off) const {
		if (off + 3 >= 0x800000) return 0;
		return MemoryBlockD1[off] |
		       (MemoryBlockD1[off+1] << 8) |
		       (MemoryBlockD1[off+2] << 16) |
		       (MemoryBlockD1[off+3] << 24);
	}
	// Translate a virtual address via the real MMU. Writes `phys` if the
	// translation succeeds so the caller can see where it went. Returns
	// 0xDEADBEEF on translation failure to distinguish it from legitimately
	// zero memory.
	//
	// NB: each region (C0/C1/D0/D1) has a SEPARATE backing block. An
	// earlier comment here claimed they were aliased — that was wrong
	// (the writePhysical path stores into MemoryBlockD0 for region 0xD0,
	// see windermere.cpp). Reading from the wrong block returns garbage,
	// not just-different-bytes-from-the-same-RAM. This was the root
	// cause of "patched 5mxPro Voice Notes REC doesn't advance" — the
	// channel-state probe lived in D0 but our gating code read C0.
	uint32_t debugReadVirt(uint32_t virtAddr, uint32_t *phys = nullptr) {
		auto p = virtToPhys(virtAddr);
		if (!p.has_value()) { if (phys) *phys = 0; return 0xDEADBEEF; }
		if (phys) *phys = p.value();
		uint32_t region = (p.value() >> 24) & 0xFF;
		uint32_t off = p.value() & 0x7fffff;
		// Read from the CORRECT memory bank based on physical region.
		// Previously this read MemoryBlockC0 unconditionally — which
		// silently returned wrong data for region 0xD0/0xD1 in
		// INCLUDE_D mode, where D-region maps to MemoryBlockD0.
		if (region == 0xC0 || region == 0xC1)
			return debugReadC0(off);
		if (region == 0xD0 || region == 0xD1)
			return debugReadD0(off);
		return 0xDEADBEEF;
	}

	bool attachCard(const uint8_t *bytes, size_t size) override;
	void detachCard() override;
	bool isCardInserted() const override { return cfCard.inserted(); }
	size_t getCardImageSize() const override { return cfCard.imageSize(); }
	const uint8_t *getCardImageData() const override { return cfCard.data(); }
	void setCfIrqLine(int bit) override { cfIrqLine = bit; }
	void setCfReenableMode(int mode) override { cfReenableMode = mode; }
	void setCfReschedulePoke(bool enable) override { cfReschedulePokeEnabled = enable; }
	void setCfFastWatchdog(bool enable) override { cfFastWatchdog = enable; }
	void setCfAccelTimer(bool enable) override { cfAccelTimer = enable; }
	void setCfDirectInvoke(bool enable) override { cfDirectInvoke = enable; }
	CfStats getCfStats() const override {
		return { cfCard.ataCommandCount, cfCard.sectorBoundaryCount,
		         cfStatIrqAssertions, cfStatIrqDeassertions,
		         cfStatEint3Dispatches, cfStatReschedulePokes,
		         cfAccelTimerHits };
	}
	// Gap condition: card asserting IREQ# while the kernel has EINT3 (bit 7)
	// masked off. In this state the CF driver falls through to the 2 s
	// socket-watchdog path and the sector won't drain until the watchdog
	// matures — so we ask the frontend to burst extra frames per RAF.
	bool cfGapActive() const override {
		return cfCard.inserted() && cfCard.irqAsserted() &&
		       (interruptMask & (1u << EINT3)) == 0;
	}

	// Audio surface — enables speaker/mic routing to/from the host.
	bool hasAudio() const override { return audioRegisterHandlingEnabled; }
	// Lets a guest-side path (or main.cpp) opt out of codec register
	// emulation at runtime. With this flag false, CONFG / COLFG /
	// CODR / COEOI fall through to the unhandled-register path and
	// reads return 0xFFFFFFFF, so EPOC's "(CONFG & 3) != 0" probe
	// sees "codec in use" and the dictaphone driver aborts cleanly.
	// Auto-flipped to false from writeReg8 CONFG=3 if r5 isn't a
	// valid channel pointer — a path observed on MC218 v1.05(259)
	// that crashes the kernel if we leave the codec enabled.
	void setAudioRegisterHandlingEnabled(bool e) { audioRegisterHandlingEnabled = e; }
private:
	bool audioRegisterHandlingEnabled = true;
public:
	int getAudioSampleRate() const override { return AudioCodecModel::kAudioSampleRate; }
	size_t readAudioOutput(int16_t *dst, size_t maxSamples) override {
		return audio.readAudioOutput(dst, maxSamples);
	}
	void writeAudioInput(const int16_t *src, size_t count) override;
	void setHostAudioEnabled(bool speaker, bool mic) override {
		audio.setHostEnabled(speaker, mic);
	}

	// ── Host serial bridge ────────────────────────────────────────────────
	// Attach/detach a virtual host-side cable to UART1 or UART2 (uartIndex
	// 1 or 2 — matches the SoC's naming and the UART1/UART2 interrupt
	// numbers). When attached, bytes the guest writes to the UART data
	// register accumulate in the UART's txQueue (drained via
	// serialReadToHost); bytes pushed via serialWriteFromHost arrive in
	// the UART's rxFifo and surface as IntRx → UARTn pendingInterrupt.
	// While unattached, the original stub behaviour stands so other boot
	// paths (notably the 5mx Pro bootloader's UART2 DSR poll) are
	// unaffected.
	bool serialAttachHost(int uartIndex);
	bool serialDetachHost(int uartIndex);
	size_t serialWriteFromHost(int uartIndex, const uint8_t *data, size_t len);
	size_t serialReadToHost(int uartIndex, uint8_t *dst, size_t cap);
	size_t serialHostTxAvailable(int uartIndex) const;
	bool serialIsAttached(int uartIndex) const;

protected:
	// Exposed for subclasses that override readLCDIntoBuffer (e.g. Revo whose
	// LCD is 480x160). The framebuffer layout is identical across all
	// EPOC R5 devices; only the dimensions differ.
	uint32_t currentLcdAddress() const { return lcdAddress; }
};
}
