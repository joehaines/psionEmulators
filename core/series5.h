// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include <cstring>
#include "clps7110.h"

namespace Series5 {

// Psion Series 5 uses the Cirrus CL-PS7110, which is register-compatible
// with the CL-PS7111 family (MC218 / Osaris SoC) — PADR at 0x000, SYSCON1
// at 0x100, INTMR1 at 0x280, LCDCON at 0x2C0, TC1D at 0x300, RTCDR at 0x380
// etc. This was confirmed by instrumenting every unknown-register access:
// every single offset the Series 5 ROM touches (0x000, 0x040, 0x100, 0x140,
// 0x180, 0x240, 0x280, 0x2C0, 0x540, ...) matches the CL-PS7111 layout and
// none match Windermere's (0x500 / 0xC00 / 0xE00 ranges). Inheriting from
// Windermere (as the first cut did) left the ROM stuck reading zeros from
// addresses our core didn't map.
//
// Two deviations from MC218 / Osaris that this subclass handles:
//   - The LCD is 640 x 240 at 4 bpp mono, not 320 x 200. readLCDIntoBuffer
//     is reimplemented against the larger canvas and the actual framebuffer
//     pointer (the MC218 path hardcodes lcdAddress == 0xC0000000).
//   - The kernel maps its internal data page at virtual 0x80100000, which
//     after MMU translation lands at physical 0xD0FE6000 — inside an 8 MB
//     RAM bank the 4 MB CLPS7111 layout would throw away. Opt into the
//     larger RAM mask + region-0xD alias via the getRamMask() /
//     aliasDRegionToRam() virtuals on CLPS7111.
//
// Status: Series 5 boots fully to the interactive EPOC R1 desktop. The
// EPOC R1 kernel completes its boot chain (kernel → EFile → EWSRV /
// WindowSrv → Shell), programs LCDCON and paints the real System screen:
// the left-hand icon sidebar, the title/taskbar and the application icon
// grid all render from the live framebuffer at 0xC0000000 (see the
// committed golden screenshot tests/golden/series5.pgm, 640x240). The LCD
// render is LCDCON-gated (black until the kernel programs the controller,
// real framebuffer afterwards) — see readLCDIntoBuffer in clps7110.cpp.
//
// IMPORTANT — historical note: everything BELOW this block (the long
// "Investigation notes" / "RESOLVED" research log, ~5,500 lines) is a
// preserved record of the bring-up investigation while boot was still
// incomplete. Many of its intermediate conclusions ("black screen",
// "IPC self-deadlock", "never reaches a GUI thread") describe states the
// emulator has since moved PAST and are NOT the current behaviour. Read
// it as history, not as a description of how the device behaves today.
// The authoritative current state is this top block plus the live class
// definition at the bottom of the file. The breakthroughs that closed the
// boot were two FAITHFUL ARM710 corrections (all-exception I-bit masking
// and the LSL #N carry-out fix), which retired the bulk of the fragile
// PC-address-gated workarounds — those now default OFF behind
// PSION_S5_HAL_LEGACY and survive only for historical A/B comparison.
//
// Key fixes on the path to this state (most recent first):
//   * series5.cpp readLCDIntoBuffer: LCDCON-gated render — black screen
//     until the kernel programs LCDCON, real framebuffer afterwards
//     (replaces the 0x43E000 ROM-splash fallback that was unreliable)
//   * arm710.cpp FUN_50043e38 short-circuit (c51010b): prevents recurring
//     SectionTranslationFault from NULL iTrap pointer, unblocks FUN_5001ae04
//   * arm710.cpp 0x80100220 → 0 write-side fix (11e1e86): prevents HAL
//     chain head 0x3C from triggering recursive abort every ~12M cycles
//   * arm710.cpp LDM^ trampoline-fault synthesis (7da3d91): forces prefetch
//     abort on 26-bit-mode legacy thread context restore
//   * arm710.cpp MMU shadow-page aliasing for bootstrap NThread (c245407):
//     prevents SVC stack from clobbering the bootstrap NThread's iAllocator
//
// Investigation notes (deep dive 2026-04-26):
//   * The variance=2890 "graphics" is actually kernel context dumps
//     overwriting RAM[0..0x12C00]: STMFD pushes during the recursive
//     abort handler at 0x50019350+ store the kernel's prefetch-abort
//     entry address 0x50019360 across what we treat as the framebuffer.
//     The pattern `00 66 33 99 11 00 00 55` decodes 0x50019360 as 4bpp
//     pixels. The kernel is NOT actually drawing here.
//   * Virtual 0x80004080 (idle thread PC) maps to physical 0xD0FF0080
//     (RAM offset 0x7F0080) via L2 PT entry 0xd0ff055e. AP=01 means
//     User-mode no-access. When the kernel restores a thread context
//     with CPSR mode=0x08 (an INVALID 32-bit mode that our emulator
//     treats as User-like), the prefetch immediately faults — that's
//     the kernel's deliberate "idle trampoline" pattern.
//   * The HAL chain head at virtual 0x80100220 cycles between 0x3C
//     and 0 every ~17.85M cycles (= 1 sec heartbeat). 0x3C is written
//     at PC=0x50016CE4 (kernel HAL init re-runs); 0 at PC=0x5004D914.
//     The dispatcher at 0x500096E0 reads the chain head and tail-calls
//     0x50002B40 with R0=0x3C → faults.
//   * Forcing *(0x80100220)=0 stops the recursion (variance→0, traps→0,
//     unique_pcs grows from 55 to 109 at 30 s, 122 at 115 s) but the
//     kernel still doesn't actually draw — it makes slow progress
//     through different code paths but never reaches a GUI thread that
//     writes the framebuffer.
//
// Investigation notes (deeper dive 2026-04-26 part 2):
//   * Function 0x50011224 (a vtable thunk for thread creation) IS
//     called once at cycle 11993900 with R1=0 (User mode case),
//     R0=0x80006074. The User-mode case at 0x50011380 calls
//     0x5001C674 (Thread::Create or similar), which returns R0=0
//     (success). So a user-mode thread WAS apparently created. But
//     no User-mode thread ever actually runs — the scheduler
//     dispatch at 0x500195D8 only ever sees CPSR mode 0x1B (Undef)
//     or 0x08 (the broken idle trampoline) in the saved-CPSR slot.
//   * The kernel's "current thread block" pointer at virtual
//     0x8010061C is ALWAYS 0x80006DAC (idle thread). The user-mode
//     thread, even though created, is never made current.
//   * WindEmu's Osaris (also CL-PS7111/EPOC R1) has none of these
//     issues — its readPhysical/abort handler are simpler and there
//     are no references to 0x80100220, no special CPSR mode 0x08
//     handling, no abort-recursion workaround. Confirms that the
//     Series 5 kernel uses a fundamentally different boot path
//     (CL-PS7110 vs. CL-PS7111) we don't fully understand.
//   * UPDATE 2026-05-04: The "Osaris is also EPOC R1" claim above is
//     WRONG. Verifying the kernel-container layout in clps7111.cpp
//     identifyObjectCon: Osaris (ROM v1.02) and 5mx (ROM v1.05) both
//     hold their CObjectCon roots in SuperPage at virt 0x80000880-0x9AC
//     (Osaris) / 0x80000980-0x9AC (5mx) — that's the EPOC *R5* layout.
//     Series 5 (ROM v1.01) uses iCurrentThread @ 0x8010061C — SuperPage
//     at virt 0x80100000 — which is the EPOC *R1* layout. So the working
//     Osaris/5mx ROMs run a kernel one major release younger than the
//     Series 5 ROM. That's why their CObjectCon::AddL function lives at
//     a different ROM offset (Osaris 0x32304, 5mx 0x2CBC4) and uses a
//     different SuperPage scheme. The implication for this investigation:
//     5mx and Osaris are useful as **hardware-stimulus baselines** (they
//     show what early-boot MMU sequence + IRQ cadence + RAM access
//     pattern looks like for a working EKA1 boot) but their kernel-
//     internal-state offsets do NOT transfer to Series 5.
//   * UPDATE 2026-05-04 part 2: clps7111.cpp's debugPC PC hooks are
//     pinned to the Osaris ROM v1.02 layout (0x32304 = CObjectCon::AddL,
//     0x634/0x66C = MMU helpers, 0x16198 = event dispatcher). On the
//     Series 5 ROM these PCs land in unrelated functions and produced
//     fake "KERNEL MMU SECTION" log lines (e.g. v=c0013044 p=0000007c
//     size=0) that polluted earlier investigation runs. Disabled for
//     Series 5 via hasOsarisDebugHooks() override — see series5.h:2331.
//     A Series-5-specific debugPC() override would need to relocate
//     each of the five hooks via the reference/5_decompiled/ data;
//     that work is deferred until a concrete diagnostic need arises.
//
// Real fix needs either (a) a working idle-thread halt mechanism
// (CL-PS7110 HALT register? we handle it but kernel never writes
// it) or (b) understanding why the kernel's HAL handler chain
// initialiser writes 0x3C instead of a valid handler pointer, or
// (c) why the User-mode thread created at boot is never scheduled.
//
// BREAKTHROUGH (2026-04-26 part 3, with kernelhwsrv source +
// EMULATOR_FIDELITY_ANALYSIS.md available):
//
//   * The 5mx EMULATOR_FIDELITY_ANALYSIS.md identifies the 5mx null
//     thread idle loop at virtual 0x5000B55C running in Undefined32
//     mode (CPSR=0x1B). Our Series 5 idle thread (block 0x80006DAC)
//     ALSO has saved CPSR=0x1B confirmed by watching virtual
//     0x80006ED0 — written at PC=0x5001AEF0 (Thread::Init+0x34).
//
//   * The "broken" thread we kept tracking is a SECOND thread block
//     at virtual 0x801001F4 (created from Thread::Init via
//     0x500139D4 → 0x50014D88 chain). Its saved CPSR slot at
//     virtual 0x80100318 contains 0x800040C8.
//
//   * 0x800040C8 decodes as: N flag set, F=1 (FIQs disabled), I=0
//     (IRQs enabled), bit 4 = 0, bits 3:0 = 1000. Crucially bit 4=0
//     means ARMv3 26-bit mode — and on ARM710T this is INTENTIONAL
//     for backward compatibility with OPL / 16-bit EPOC legacy
//     code, which the Series 5 supported. In 26-bit mode bits 0-1
//     are mode (00=User26 here), and bits 2-25 of R15 (not CPSR)
//     are the 24-bit PC. Our emulator treats mode 0x08 as 32-bit
//     MainBank (User-like) and runs the thread with the broken
//     interpretation, then faults.
//
//   * Function 0x50014D88 is the kernel's thread-context initialiser
//     called during process create. It receives R0 = thread initial
//     PC (0x8000408C in our trace) and produces the saved CPSR
//     0x800040C8. The "0x15" pushed at 0x50014DAC is part of the
//     CPSR construction (probably a flags/mode template).
//
//   * Series 5 kernel HAL chain head sentinel 0x3C at virtual
//     0x80100220 is INTENTIONAL — it's the EKA1 equivalent of an
//     "empty handler list with metadata"; the dispatcher reads it
//     and does an indirect call via offset 0x2C-0x58 into ROM[0x2000+]
//     where the actual handler thunks live. Our emulator's wrong
//     interpretation of this as a struct pointer + the mode-0x08
//     thread together cause the recursive abort fault.
//
// Therefore the proper fix is either:
//   1. Implement ARMv3 26-bit mode in core/arm710.cpp (significant
//      undertaking — needs a separate fetch/decode/PC-handling path),
//      or
//   2. Detect the kernel's 26-bit-thread context restore and have
//      our emulator silently "halt" the thread (waiting for IRQ to
//      schedule the next thread), bypassing the broken 0x08 mode
//      execution.
//
// Followup investigation (2026-04-26 part 4):
//   * Watching virtual 0x80004080+ during boot reveals the page is
//     used as DATA (struct fields), not CODE. The kernel writes
//     0xA5A5A5A5 stack-poison via PC=0x5004D770 then later overwrites
//     specific offsets with field values: 0x80004088=0xFF78,
//     0x8000408C=0x00, 0x80004084=0xFFFFFFFF, 0x80004088=0x3C, etc.
//   * The thread block's "saved PC" slot at 0x8010030C is set to
//     0x80004090 (later 0x800040CC) by PC=0x50042B44. This is
//     a virtual address pointing to data — when the LDM^ at
//     0x500195D8 restores it as PC, the CPU prefetch faults.
//   * This is the kernel's deliberate "abort trampoline" pattern —
//     the legacy thread INTENTIONALLY faults to invoke the abort
//     handler which is supposed to do the actual scheduling. But
//     our abort handler hits the recursive HAL chain bug and never
//     completes, so we never get past the trampoline.
//   * Tried: redirecting the trampoline PC to 0x50019280 (a real
//     `B .` infinite branch in ROM at virtual 0x50019280-0x50019284)
//     — kernel still doesn't progress (unique_pcs drops to 30).
//   * Tried: cpu.wfiRequested = true on trampoline restore + halted
//     handling in clps7111 — kernel stalls (30 PCs vs 81).
//   * Tried: forcing *(0x80100220)=0 to break HAL chain — kernel
//     reaches more code (109 PCs) but variance drops to 0 (no fake
//     graphics from kernel context dumps to disguise the broken
//     state).
//
// IMPLEMENTED (writeVirtual hook in arm710.cpp): intercept the kernel
// store at PC=0x50016CE4 that writes 0x3C to 0x80100220 and force the
// value to 0. Effect (PC sampling, 15 sim seconds):
//
//                       BEFORE              AFTER
//   PC=0x00000010      78.8% of samples    0.0%  (abort vector)
//   real kernel code     ~21%             ~100%
//
// The kernel is now spending all its time in real code paths
// (idle/scheduler loops at 0x5002C2A4, 0x5001884C, 0x500195D8…)
// instead of bouncing in the recursive abort handler. The fix is
// gated on series5HalFix and the exact PC + virtual address, so it
// only fires for the EPOC R1 store that produces the broken value.
//
// IMPLEMENTED (LDM^ trampoline-fault hook in arm710.cpp): when the
// kernel's LDM-with-PSR-force-user reloads PC into [0x80004000,
// 0x80005000) — the trampoline data page — synthesize a prefetch
// abort to the kernel's abort vector (0x0000000C). On real hardware
// the L2 PT entry there is User-no-access so this fault would happen
// natively; our MMU permission model lets the fetch succeed and the
// CPU wanders ~7% of execution time through conditional-NOP-as-data.
// Effect: unique_pcs grows from 105 → 260 in 60 sim s (saturates by
// 60s, no further progress through 180s).
//
// REMAINING BLOCKER (agent investigation 2026-04-27): the kernel's
// EFile loader thread is created but never scheduled.
//
// Findings:
//   * The EPOC R1 user shell is `Z:\System\Apps\Shell\Shell.app`,
//     NOT EShell.exe. Earlier notes mentioning ewsrv.exe / elink.exe /
//     eshell.exe at 0x5005F4xx were string literals inside ETest.exe,
//     not the kernel's loader table.
//   * The TRomDir (file directory) is at virtual 0x504138A0 (file
//     offset 0x4138A0), pointed to from TRomHeader+0x94. Directory
//     decodes correctly: EFile.exe at virt 0x500524C0, EwSrv.exe at
//     virt 0x500F1740, Shell.app under Z:\System\Apps\Shell\.
//   * The "first user process" loader is launched as a kernel thread
//     at virt 0x50010FCC, called from kernel function 0x50006758.
//     Its job: build a TFindFile for "Z:\\System\\Libs\\EFile.EXE"
//     (string at 0x500278B8), match UID1=0x100000BB / UID2=0x1000008C
//     (which match EFile.exe's actual header), then call kernel-side
//     loader thunks 0x500268F8 / 0x50026298 / 0x50026934 → 0x500403A8.
//     After loading EFile, the thread loops forever at 0x50011160.
//   * The scheduler at 0x500195C0 only ever sees thread.savedCPSR =
//     0x1B (Undefined32 — the kernel idle thread). No User-mode
//     thread (CPSR=0x10) ever runs, AND the EFile loader thread
//     (which is kernel-mode Svc) never runs either. The scheduler's
//     "current thread" pointer at 0x8010061C is permanently stuck on
//     the idle thread block at 0x80006DAC.
//   * Hot PCs (0x5002C29x = bit-shift-by-N inside a TBitMapAllocator
//     call, 0x5001B42x = TC2OI-poll spin) confirm the kernel state
//     machine is alive and ticking but only running idle work.
//
// Diagnosis: Thread::Resume at 0x500095F8 (called from the loader
// init path at 0x50006758) is failing to put the loader thread on
// the scheduler's ready queue. Same failure mode as the 5mx CF DFC
// thread documented in EMULATOR_FIDELITY_ANALYSIS.md lines 367-382:
// thread is created, RSF/iNState writes happen but don't reach the
// slot the dispatcher reads. Most likely cause: an off-by-one
// struct-field offset between EKern's build-time layout assumptions
// and our emulation of the TRomImageHeader / NThread context block.
//
// To unblock further requires either (a) decompiling the EPOC R1
// kernel's Thread::Create / Thread::Resume / scheduler-pick at
// 0x5000925C / 0x500095F8 / 0x500195C0 to verify struct offsets,
// (b) full ARMv3 26-bit User mode emulation (separate concern but
// likely interacts), or (c) running EKA1 source through Symbian
// SDK to compare expected struct layouts. The Series 5 v1.01 ROM
// itself contains no LCDCON-write code — LCD init lives in
// WindowSrv.dll which is loaded by user-mode chain that we never
// reach. The splash bitmap exists in ROM around file offset 0x43E000
// but is only memcpy'd into the framebuffer after user-mode
// WindowServer runs.
//
// FOLLOW-UP (three-agent investigation 2026-04-27 part 2):
//
//   1. Empirical kernel-globals tracer (PSION_S5_RQ_TRACE) over 30 sim s
//      proved there is NO off-by-one struct mismatch in our emulation.
//      Every write to virt 0x80100000-0x80100400 is read back at the
//      same address. NThread+0x124 (saved-CPSR) on the idle thread
//      (0x80006DAC) correctly holds 0x1B; on the "broken" 0x801001F4
//      thread it holds 0x800040C8 (a code pointer, not a CPSR) — that
//      block is treated by the kernel as an iHandlers/DLL-list entry,
//      NOT a complete NThread.
//
//   2. ROM disassembly identified the REAL NThreadBase::Resume at
//      virt 0x50011424 (NOT 0x500095F8 as previously assumed; that's
//      a different DFC-list helper). Resume's logic:
//        - reads iNState byte at NThread+0xC4
//        - jump-tables on (state-1) for states 1..5
//        - state 0 falls through to no-op (no list-add, no RSF write)
//      State-byte vocabulary (from STRB-immediate scan):
//        1=EReady, 2=EWaitFastSem, 3=EWaitDfc, 4=EHoldFastMutex,
//        5=EWaitFastMutex, 6=EDead. Globals: iRescheduleNeededFlag at
//        virt 0x80100348, iCurrentThread at 0x8010061C, snapshot at
//        0x80100624 (refreshed at exception entry by 0x500191B8).
//
//   3. Force-schedule experiment (PSION_S5_FORCE) overrode the
//      scheduler's iCurrentThread read at PC=0x500195C4. Best result
//      was 200 unique PCs (vs 184 baseline) but PSION_INSN_TRACE on
//      0x50010FCC (loader thread entry) showed ZERO hits — i.e. the
//      forced-thread dispatch worked but the saved-PC slot of any
//      forced block never points at the loader. Conclusion: no
//      separate "loader thread" NThread block is ever created.
//
//   4. Tried hook at PC=0x50011438 (Resume's iNState read) returning 4
//      whenever the byte is 0 — to force the case-4 (priority-list-add
//      + EReady + RSF=1) path. The hook NEVER FIRES during boot,
//      confirming Resume itself is never called. The bug is upstream
//      of Resume: the BL at 0x500067CC (loader-init's "create thread"
//      call into 0x5000925C) returns the parent pointer instead of a
//      new NThread block, so there is no loader thread to Resume.
//
// Synthesis: the EPOC R1 boot chain is gated on a kernel-side thread
// creation/spawning step that is short-circuiting. The function at
// 0x5000925C may not actually be Thread::Create — it could be a
// "find-or-create" helper that returns the parent on a cache hit, OR
// the allocator at 0x50008E54 may be returning failure silently.
// Resolving this requires either disassembling 0x500091F0..0x50009350
// against EKA1 source, or a Series 5 v1.01 ROM decompile.
//
// MAJOR UPDATE (decompile-driven investigation 2026-04-27 part 3):
//
// The boot is making FAR more progress than previous notes indicated.
// Empirical traces with the current set of fixes (abort recursion +
// LDM^ trampoline) show:
//
//   * The "NULL" thread (Symbian's null/idle thread, named "NULL")
//     IS the loader thread. There is no separate child thread; the
//     kernel allocates a SINGLE NThread block at 0x80006DAC, sets its
//     entry-PC at NThread+0xE4 to 0x50010FCC (the loader function),
//     and dispatches into it via LDM^ → 0x5001AE44 → trampoline at
//     FUN_5004D260 (0x5004D260) → BL R7 (= the loader entry).
//   * 0x5000925C is NOT Thread::Create — it's NThreadBase::Construct
//     (second-stage void initialiser). The real allocator is
//     FUN_50011198 which calls the heap via thunk_FUN_50030734(0x128).
//     Earlier "returns parent pointer" symptom was an ABI artifact;
//     the alloc returns 0x80006DAC which IS the new (and only) thread.
//   * 0x80006DAC alias confusion: 0x80006DAC is BOTH the idle thread
//     block AND the NULL-loader thread block — they're the same
//     NThread because Symbian uses one thread for both roles at boot.
//   * The loader at 0x50010FCC IS executed. Empirical PSION_INSN_TRACE
//     shows 88 unique PCs in [0x50010FCC, 0x500111BC) executed during
//     a 30s boot — most of the loader function body runs.
//   * The trampoline FUN_5004D260 calls thunk_FUN_5004BBC0 (an Exec::
//     SWI table at 0x5004BBC0 — first slot is SWI 0x800014). The SWI
//     returns 0 (success) and the trampoline proceeds to call R7
//     (the loader entry).
//   * The loader builds a TFindFile descriptor for "Z:\\System\\Libs\\
//     EFile.EXE" with UID1=0x1000007A, UID2=0x1000008C — confirmed
//     by trace at 0x500110E0+ where these magic numbers are stored.
//   * After the loader runs its prologue and process-create call, it
//     enters a wait loop calling thunk_FUN_500187E8 which seems to
//     return immediately (loader runs again on next dispatch tick).
//
// CURRENT REAL BLOCKER: the loader's process-create call
// (thunk_FUN_500403A8 wrapping a SWI) either fails silently or
// succeeds but the spawned process never runs. EFile.exe is supposed
// to be loaded into RAM, then a new user-mode thread spawned at its
// entry. Neither happens — likely because:
//   (a) The SWI handler for "create process" doesn't initialize the
//       MMU mapping for the user-mode address space.
//   (b) The TROMHEADER directory walk inside the SWI handler can't
//       find EFile.exe because of an MMU mapping difference between
//       Series 5 and 5mx.
//   (c) The kernel-side loader (FUN_500403A8 family) requires a
//       PDD/LDD that's never registered in our emulation.
//
// The Series 5 v1.01 ROM decompile is now in place at
// /home/user/psion/reference/5_decompiled/ and confirms all the
// above. Next iteration: instrument thunk_FUN_500403A8 (process-
// create SWI) and trace its kernel-side handler to find the actual
// failure point. The splash will only appear once a user-mode
// thread runs WindowSrv.dll, which programs LCDCON and copies the
// splash bitmap from ROM file offset ~0x43E000 to RAM offset 0.
//
// MAJOR UPDATE (decompile-driven 2026-04-27 part 4):
//
// The series5HalShortCircuitWait fix that previously forced R0=0
// across SWIs 0xc00076..0xc00079 was MISLABELED — its gate
// (R0==0x1c||0x2f, R2>=0x80000000, R3==0x80000001) actually matched
// RProcess::Create's IPC roundtrip at PC=0x5004BD48 (selector 0x1C
// = EProcessExecCreate), NOT HAL::Get. Forcing R0=0 made the loader
// believe process-create succeeded when the kernel had not actually
// done the work — EFile.exe was never mapped, so the user-mode
// thread dispatched into an unmapped page and aborted.
//
// Disabling the short-circuit (commit cea03d7, gated behind
// PSION_S5_HAL_LEGACY env var) lets the kernel's real
// RProcess::Create handler run. The handler:
//   * Allocates user-mode address space
//   * Maps EFile.exe code section (XIP from ROM at 0x50052518+)
//   * Copies the data segment to writeable RAM
//   * Applies fixup-table relocations
//   * Sets up the new user-mode thread context
// Result: kernel transitions to User32 (CPSR=0x10), executes
// EFile.exe code at PC=0x500537E4 (inside EFile's launcher), and
// progresses through the static-ctor walk into FUN_500534bc (the
// real init body that would spawn EWSRV.EXE).
//
// CURRENT STATE: variance still 0 (LCD not lit) because:
//   * EFile.exe gets to its main launcher but doesn't reach the
//     EWSRV.EXE spawn site at line 109675 of the decompile
//   * The kernel re-issues SWI 0xc00076 with R0=0x1c every ~1 sim
//     second — looks like a server-thread completion timer/retry
//   * User-mode PC range reached: 0x500537E4 (in launcher) — does
//     not progress to FUN_5005650c (TFileServer alloc) or
//     thunk_FUN_5003f5b0 (RProcess::Create for EWSRV.EXE)
//
// Tried but reverted: short-circuit for SWI 0xc00076 selector 0x2f
// (Rendezvous-style wait). Empirically the 0x2f SWI does NOT fire
// in our actual boot trace — only 0x1c selector fires repeatedly.
// The agent's hypothesis (that boot reaches 0x2f at 24 sim sec) was
// based on a different state than what we currently have.
//
// Final test status (cea03d7):
//   * variance=0, unique_pcs=305 (30 sim s) / 264 (120 sim s)
//   * Kernel reaches User32 mode for the first time
//   * EFile.exe executes through its prologue
//   * Boot stalls in a c00076(0x1c) retry loop without lighting LCD
//
// Next iteration would likely need: (a) decompile of the SWI 0x1c
// kernel handler to see why it's looping, (b) trace what fails
// inside the kernel-side loader between the SWI invocation and
// EFile reaching EWSRV.EXE spawn, (c) implement bootblock-style
// LCDCON init as a pragmatic stop-gap so the framebuffer is at
// least partially visible (since the ROM has no LCDCON-write
// code at all — this would be cosmetic).
//
// DEEP DIVE (Symbian source-cross-referenced 2026-04-27 part 5):
//
// The c00076(0x1c) "retry" pattern is misleading. Empirical TRS
// watch on 0x80104528 (the loader's TRequestStatus) shows:
//   * cycle 13442448: TRS = 0x80000001 (KRequestPending) from
//     PC=0x5003AD04 (RProcess::Create wait setup)
//   * cycle 13623390: TRS = 0x00000000 (looks like KErrNone) from
//     PC=0x50010020 (an STR R12,[R1],#4 inside a memset loop —
//     R12 was just MOV R12,#0)
//   * cycle 13675750: TRS = 0xCCCCCCCC (stack debug fill)
//
// PC=0x50010020 is a memset-zero loop, NOT a real IPC completer.
// The TRS happens to fall inside the memory region being zeroed,
// so it gets "completed" by accident. The user-mode caller's
// WaitForRequest returns success — but no actual process was
// created. The caller retries because subsequent calls (e.g.,
// FUN_5000d840 handle-resolve) read garbage and produce errors
// the calling code interprets as "retry".
//
// Selector 0x1c handler chain in the Series 5 ROM:
//   0x500193F8 → table lookup at 0x5002795C+0x70 = 0x5000B6D8
//   0x5000B6D8 (arg shuffle) → 0x5000DA9C (loads *0x80100004 =
//     IPC server pointer) → 0x500261A4 (LDR-LDR import thunk to
//     *0x50400274 = 0x5002ED14)
//   0x5002ED14 = RSessionBase::SendReceive equivalent (IPC post)
//
// In EKA1, ProcessCreate goes via IPC to a kernel "Loader" server
// thread. In our boot only ONE NThread block exists (0x80006DAC,
// the null/loader thread). When EFile.exe (in user mode) issues
// ProcessCreate, the SWI handler posts an RMessage to the server
// queue at *0x80100004 — but the loader thread that should process
// the queue is the SAME thread now blocked in WaitForRequest.
// This is a self-deadlock.
//
// The real EKA1 boot creates at least TWO threads:
//   1. The null/idle thread (no work, just halts)
//   2. The Loader thread (FUN_50010FCC's wait loop processes
//      RMessages from the IPC queue)
//
// Our emulator's loader-init at FUN_50006758 only ever creates
// one thread (per the FUN_500067CC analysis in the prior part-2
// notes — the alleged "Thread::Create" call FUN_5000925C is
// actually NThreadBase::Construct, void return).
//
// CONCRETE NEXT STEP: find the kernel function that should create
// the SECOND thread (the loader thread distinct from idle), and
// understand why it isn't called. The IPC mechanism cannot work
// with a single thread because there's no other thread to receive
// the queued message. Until two threads exist, the boot will
// always self-deadlock at the first user-mode IPC.
//
// CORRECTION (decompile + Symbian-source 2026-04-27 part 6):
//
// The "only one NThread exists" diagnosis above was WRONG. Empirical
// PSION_WATCH on the kernel-globals shows THREE NThread blocks DO get
// installed as iCurrentThread during boot:
//   1. 0x80003CD0 (the NULL/initial thread, on the boot stack —
//      created via FUN_5001AE48(auStack_218, 0x200) in FUN_50010E44)
//   2. 0x80006074 (case-0 initial-loader thread — created via
//      FUN_5001AE48(0,0) in FUN_50010A08, type 0)
//   3. 0x80006DAC (Supervisor / Loader, name "Supervisor" — created
//      via FUN_50011198(1) + FUN_5000925C(...,1,...) in FUN_50006758)
//
// All three are real, properly initialised, and the scheduler picks
// each at different times (0x80100624 snapshot alternates between
// 0x80006074 and 0x80006DAC).
//
// REAL CURRENT BLOCKER — periodic kernel re-init driven by recurring
// alignment fault:
//
// Empirical PSION_WATCH on 0x80100004-0x80100020 (the IPC server
// pointer + queue head region) shows:
//   * cycle ~11.89M: kernel sets *0x80100004 = 0x80005558 (live IPC
//     server pointer) and *0x80100008 = 0x800055B0 (queue head) at
//     PC=0x5000784C / 0x5000785C
//   * cycle ~13.54M (~92ms later): both wiped to 0 by a memcpy at
//     PC=0x5004D90C (inside FUN_5004D8FC)
//   * The cycle repeats every ~17.85M cycles (= 1 sim s)
//
// The memcpy is called from FUN_5000FF68 line 20302
// (thunk_FUN_5004D800(DAT_50000098, ...) which copies EKern.exe's
// data prototype from ROM[0x500290B4] into virt 0x80100000). The
// prototype's slots for 0x80100004 and 0x80100008 are zero in ROM,
// so the live values get clobbered.
//
// FUN_5000FF68 itself is called from PC=0x5001767C (LR=0x500176C0)
// in CPSR=0x17 (Abort32 mode!) — i.e. from inside the kernel's
// abort handler. So an alignment fault triggers a "recovery" path
// that re-inits the kernel data page, destroying the IPC state.
//
// The triggering fault is at PC=0x5000CF84 (the slow-exec slot 0x8E
// handler — atomic-decrement-counter at *R0). Its R0=0x1F (= 0x1B + 4
// from the wrapper at 0x5003A7A4). The bad caller (LR=0x5003D934 at
// PC=0x5003A79C) passes R0=0x1B, which equals (struct_ptr=-1 + 0x1C).
// The struct ptr -1 (= 0xFFFFFFFF) comes from a kernel global (likely
// an uninitialised iAllocator or similar small-int handle).
//
// IMPLEMENTED FIX (commit 4c1891e): writeVirtual hook to prevent the
// memcpy at PC=0x5004D90C from zeroing 0x80100004-0x80100020. The
// IPC server pointer now persists across re-init cycles. The kernel
// still loops in the abort recovery path but the IPC state survives.
//
// REMAINING BLOCKER: each abort cycle the kernel still does a lot of
// useless work and the user-mode caller's fault path keeps hitting
// dead-end LDRs (0x5003A744, 0x5003A74C, 0x5003A754 — all `LDR R0,
// [R0]` SWI thunks with R0 = small int). Tried instruction-level
// skipping of these but each fix unmasks the next dead-end.
//
// ROOT CAUSE: the EKA1 process loader doesn't fully initialise the
// new user-mode thread's iAllocator (NThread+0x38) and possibly
// other fields. When user-mode EFile.exe runs and calls the
// equivalent of Exec::Heap, it gets back garbage (0xFFFFFFFF). All
// downstream small-int "this+offset" calculations produce bogus
// pointers. To fix properly, the kernel-side process loader needs
// to be inspected/fixed in the decompile to find where iAllocator
// should be set.
//
// Final status (commit 4c1891e):
//   * 11/12 devices PASS (only series5 fails)
//   * Series 5: variance=0, unique_pcs=305 (30 sim s) / 270 (120 s)
//   * Three NThread blocks exist; scheduler dispatches all three
//   * Kernel transitions to User32 mode; EFile.exe code executes
//   * IPC server pointers preserved across kernel re-init cycles
//   * Boot reaches user-mode but uninitialised iAllocator prevents
//     any meaningful user-mode progress (every SWI returns garbage)
//
// FOLLOW-UP (commit d6c4c6c): added two targeted hooks for
// recurring alignment-fault sites:
//
// 1. Exec::Heap rescue (readVirtual hook at PC=0x500031F8): the
//    bootstrap NThread block at 0x80003CD0 is allocated inside the
//    SVC stack frame of FUN_50010E44, so its iAllocator slot at
//    0x80003D08 gets overwritten by SVC stack pushes. Substitute the
//    canonical kernel RHeap pointer (0x80004000) when the read would
//    return garbage outside the 0x80004000-0x80200000 RAM range.
//
// 2. Null-TDesC short-circuit extended (executeInstruction hook at
//    PC=0x50037A34): catch R0=0xFFFFFFFF (uninitialised TDesC) in
//    addition to the existing R0=0 case. Prevents the LDR R0,[R0]
//    at PC=0x500379A4 from alignment-faulting on 0xFFFFFFFF.
//
// Effect: unique_pcs grows from 270 → 313 (+16%) at 120 sim s.
// Boot still doesn't reach LCD programming; recurring alignment
// faults at other sites (PC=0x5000C254 etc.) still trigger the
// kernel panic-recovery memcpy that wipes the kernel data page
// every ~17.85M cycles.
//
// STRUCTURAL ROOT CAUSE (5-agent investigation 2026-04-27 part 7):
//
// The recurring 1 sim s heartbeat is the kernel's panic recovery:
//   - User-mode SWI handler reads an uninit slot in the kernel data
//     page (post-process-load EFile.exe state isn't fully zeroed/
//     initialised by our emulator's process-create path)
//   - Leaf LDR R<x>, [R<n>] alignment-faults (R<n> = 0xFFFFFFFF or
//     small int)
//   - Abort handler dispatches to FUN_50017538 (Panic, slot 3)
//   - Panic calls FUN_5000FF68 with R1=0x10000000 (panic sentinel)
//   - FUN_5000FF68's body runs the unrolled memcpy at
//     PC=0x5004D908..0x5004D944 that copies EKern.exe's data
//     prototype from ROM[0x500290B4] into virt 0x80100000-0x80100400
//   - The memcpy ZEROES the per-thread NThread templates
//     (iCurrentDfcQ at 0x801002B4, iAllocator at 0x801002BC) that
//     were initialised at boot — the prototype's slots are 0
//   - Next SWI uses the now-zeroed template → fault → repeat
//
// Tried but didn't unlock boot:
//   * Skip FUN_5000FF68 entry when CPSR=Abort32 + R1=0x10000000:
//     stops the wipe but the original fault still fires every ~600
//     cycles (without recovery's intermediate work, the kernel
//     bounces back to the same fault tighter)
//   * Skip the leaf LDR at PC=0x5000C254 when R5=0xFFFFFFFF: kernel
//     gets stuck in a 30-PC loop (skipping the LDR breaks the
//     handler's contract; caller's code path requires the LDR's
//     side effect)
//
// Real fix would need: properly initialise the user-mode process's
// data section (run static constructors, copy .data prototype, set
// up RAllocator / iCurrentDfcQ on the new thread). This requires
// fixing the kernel-side process-create handler (probably the
// thunk_FUN_500403A8 → SWI 0x800054 chain) to actually do the
// .data segment copy + relocation + static-ctor invocation that
// the EKA1 loader is supposed to do. Without that, EFile.exe's
// globals stay zero/garbage and every code path that reads them
// produces a downstream bogus pointer.
//
// Ultimate boot status (commit d6c4c6c):
//   * Kernel boots through scheduler dispatch → loader → process
//     create → User32 dispatch → EFile.exe entry → static-ctor
//     walk → first SWI → ALIGNMENT FAULT
//   * variance=0 because LCDCON is never programmed (the program
//     code lives in WindowSrv.dll which requires user-mode boot
//     to complete)
//   * 11/12 devices still PASS in CI; series5 needs deeper work
//
// FOLLOW-UP (commit 2dd16e1): synthesise EFile.exe .data segment
// init at first user-mode dispatch into EFile's code range. Decoded
// the TRomImageHeader at virt 0x500524C0:
//   iDataAddress       = 0x5005E6F8 (.data prototype source)
//   iDataSize          = 0x0A88
//   iBssSize           = 0x0004
//   iDataBssLinearBase = 0x504009A0 (.data + .bss target)
//
// Empirical PSION_WATCH on 0x504009A0 confirmed ZERO writes during
// boot — the kernel's process-create handler doesn't perform the
// .data copy that EKA1's loader is supposed to do. Hook in
// arm710.cpp synthesises this copy on first user-mode entry.
//
// However: the .data PROTOTYPE in ROM is itself all zeros for
// EFile.exe (likely because EFile.exe initialises its globals via
// static constructors at runtime, not from a ROM prototype). So the
// copy is a no-op for this specific binary. The hook is still
// useful as correct emulation behaviour for any DLL/EXE that DOES
// have non-zero .data prototypes.
//
// REMAINING BLOCKER: alignment fault at PC=0x5000C254 with R5=
// 0xFFFFFFFF still fires every ~1 sim sec. R5 is set from R0 at
// FUN_5000C208 entry (a TDesC::Match-style helper at slow-exec
// slot 0x54). Caller passes R0=0xFFFFFFFF — likely from EFile.exe
// user-mode code reading an uninitialised TDesC pointer field.
// The static-ctor table walk at EFile.exe entry (0x50052518)
// references DAT_500525a4 / DAT_500525a8 — if the constructors
// fail or partial-init, downstream code reads zero/garbage globals.
//
// To unblock the variance test, the next iteration would need to:
// (a) trace EFile.exe's static-ctor execution (PC=0x500525xx
//     range) and find which ctor fails or returns wrong, OR
// (b) decompile FUN_5000C208 to identify which user-mode
//     descriptor field is being passed as 0xFFFFFFFF, OR
// (c) identify a higher-level kernel-state initialisation that
//     isn't happening (e.g. an entry in the user-mode page-table
//     that should map a "globals" page to user RAM but doesn't).
//
// All three are tractable but each is a multi-hour focused
// investigation. The committed fixes (11e1e86 / 7da3d91 /
// cea03d7 / 4c1891e / d6c4c6c / 2dd16e1) represent the maximum
// real progress this session — Series 5 reaches user-mode
// EFile.exe execution for the first time, but the LCD splash
// requires the user-mode boot chain to actually complete.
//
// HAL ROOT OVERRIDE ATTEMPT (commit 7f282a5, REVERTED in 97919d6):
//
// Per the 5mx vs Series 5 boot-state diff agent investigation, the
// recurring fault root cause is:
//   * FUN_50010E44 line 21323 declares auStack_218[512] and stores
//     its NThread pointer (= 0x80003CD0) into a global at *DAT_50010f9c
//   * After the function returns, the auStack_218 buffer is freed
//     from the function's stack frame; the global pointer still
//     references it
//   * Subsequent kernel code uses the boot SVC stack ~0x80003D00,
//     so STMFD pushes from PC=0x50038448 (and others) write to
//     0x80003CFC..0x80003D08 — clobbering the bootstrap NThread's
//     iAllocator slot at NThread+0x38 = 0x80003D08
//   * 5mx avoids this by allocating the HAL root at virt 0x81402EAC
//     (different geometry; bootstrap stack at 0x80304000+, no overlap)
//
// Tried (commit 7f282a5): drop the specific stack-push poisoning at
// PC=0x50038448 targeting virt 0x80003D08. Result: control flow
// broken because the dropped store was a legitimate STMFD function-
// frame save (R4, R5, LR). When the function later does its
// matching LDM to restore R4/R5/PC, it reads the (now stale) value
// and the function returns to the wrong PC. boot regressed:
// unique_pcs at 30s: 305 → 90, NO User32 entry over 60 sim s.
// Reverted in 97919d6.
//
// Lessons learned:
//   * Any "drop the clobber" fix on a stack frame breaks the matching
//     LDM/POP that restores from the same SP slot
//   * The only viable fixes are: (a) read-side substitution like the
//     existing iAllocator rescue at PC=0x500031F8 (commit d6c4c6c),
//     (b) relocating the bootstrap NThread to a stack-clear region,
//     or (c) MMU-aliasing the overlap region to a shadow page so
//     stack writes go to one physical page and NThread reads from
//     another
//
// Option (b) requires a hook on FUN_50010E44 entry to redirect the
// auStack_218 placement. Option (c) is more invasive (requires MMU
// emulation changes). Option (a) is what we have, masking the
// symptom but not preventing the recurring abort cycle. Each round
// of the cycle re-clobbers iAllocator after the kernel re-runs
// initialisation, but the read-side rescue keeps Exec::Heap from
// returning garbage so user-mode boot can occasionally make slow
// progress (PCs 0x500537E4..0x50053840 reached at cycle 424M).
//
// Final commit chain (most recent first):
//   97919d6 Revert "Series 5: structural fix..." (broke control flow)
//   7f282a5 Series 5: structural fix for HAL/SVC stack overlap... (REVERTED)
//   f90a64b Series 5: remove broken .data init hook (used wrong header)
//   ba5bc73 Series 5: document .data init result + final blocker
//   2dd16e1 Series 5: synthesise EFile.exe .data segment init (NO-OP)
//   e93879b Series 5: document structural root cause + final status
//   d6c4c6c Series 5: rescue iAllocator slot + extend null-TDesC
//   b6c77b7 Series 5: corrected diagnosis — three threads exist
//   4c1891e Series 5: preserve IPC server pointers across re-init
//   cea03d7 Series 5: disable RProcess::Create short-circuit
//   7da3d91 Series 5: synthesize prefetch abort on LDM^ trampoline
//   11e1e86 Series 5: kill abort-recursion via 0x80100220 → 0
//
// Final status:
//   * 11/12 devices PASS
//   * Series 5: 305 unique_pcs at 30s, 313 at 120s
//   * User32 mode entry at cycle ~424M (24 sim sec)
//   * EFile.exe code execution at PCs 0x500537E4..0x50053840
//   * variance=0 (LCDCON in WindowSrv.dll, not loaded)
//   * Recurring abort cycle every ~17.85M cycles still fires
//
// OPTION (b) RELOCATION ATTEMPT (2026-04-28, NOT COMMITTED):
//
// Per series5.h "Lessons learned" the only viable next-iteration paths
// were (a) read-side substitution (already done in d6c4c6c), (b)
// relocate the bootstrap NThread to a stack-clear region, or (c)
// MMU-aliasing the overlap region. This iteration attempted (b).
//
// Approach: writeVirtual hook gated on series5HalFix + PSION_S5_RELOC_NTHREAD
// + virtAddr=0x8010061C + PC=0x50010EA0 + value=0x80003CD0. When all
// match, copy the 0x128-byte stack-local NThread to a safe RAM region
// and substitute the iCurrentThread value being stored. Subsequent kernel
// reads of *iCurrentThread return the safe address; the LDR R3, [R4]
// at 0x50010ED8 picks up the safe address; iAllocator init at 0x50010EE0
// writes 0x80004000 to safeAddr+0x38 instead of stack+0x38.
//
// Implementation worked correctly (PSION_WATCH on safeAddr+0x38 confirms
// the iAllocator write reaches the safe location), but the relocation
// could not unlock new boot progress. Tested addresses and outcomes:
//
//   * 0x80007800 - boot crashes at 28 unique_pcs: page not MMU-mapped
//     when PC=0x50010EA0 fires. PageTranslationFault on the recursive
//     copy and on subsequent kernel writes of NThread+0x34 / +0x38.
//
//   * 0x80003800 - same MMU page as bootstrap NThread (so guaranteed
//     mapped). Relocation works initially, iAllocator at 0x80003838 set
//     correctly. But the kernel heap allocator at PC=0x50030734 grabs
//     this address ~10M cycles later (cycle ~13.5M), trashing the
//     relocated NThread. unique_pcs stays at 227 (same as baseline).
//
//   * 0x80050000 - MMU-mapped, initially zero writes. Hook fires, but at
//     cycle 4.96M PC=0x50019350 (abort handler) writes 0x50019350 to
//     0x80050038 with SP=0x8005003C. So 0x80050000 is the top of an
//     abort-mode stack. Boot regresses to ~30 unique_pcs.
//
//   * 0x80008000 - MMU-mapped. Relocation works through the kernel
//     init (iAllocator at 0x80008038 = 0x80004000) but at cycle 15.3M
//     PC=0x500193B4 writes from SP=0x80008038 — yet another per-mode
//     stack. Boot regresses.
//
//   * 0x80108000 - boot crashes immediately: not MMU-mapped (the
//     kernel data page section ends at 0x80101000, and 0x80101000-
//     0x80105000 is canary-filled stack region; 0x80105000+ is HAL
//     root + SVC stack; nothing maps 0x80108000+).
//
//   * 0x80104000 / 0x80103000 / 0x80101800 / 0x801006D0 / 0x80100E00 /
//     0x80100F00 - all heavily written by either the EKern data
//     prototype memcpy (PC=0x50000650), the canary fill at PC=0x5004D770,
//     or per-mode stack pushes during runtime.
//
//   * 0x807F0000 / 0x80020000 - zero writes during baseline 30 sim s,
//     but MMU mapping for these high addresses is uncertain and would
//     likely fail with PageTranslationFault when the kernel tries to
//     read back NThread fields.
//
// Findings:
//   * In Series 5's 8 MB RAM layout EVERY MMU-mapped region <0x80100000
//     is used by some per-mode stack at runtime: SVC stack at 0x80003F80,
//     IRQ/Abort stacks at various 0x800xxxxx points, the canary-filled
//     stack arena at 0x80101000-0x80105000, and the HAL root + SVC stack
//     at 0x80105xxx.
//   * The kernel only maps a NARROW band of MMU pages around 0x80003xxx
//     and 0x80100xxx-0x80105xxx. Virtual addresses outside that band
//     (e.g. 0x80108000) translate-fault. This makes "find a permanently-
//     unused mapped region" empirically impossible without modifying
//     the MMU page tables.
//   * Even within the mapped band, the kernel canary-fills 0x80101000-
//     0x80105000 (16 KB) with 0xCCCCCCCC at PC=0x5004D770 immediately
//     before each thread switch — so any address in that range gets
//     wiped each tick.
//
// Conclusion: Option (b) is fundamentally blocked by the same RAM-
// layout collision that makes the bootstrap NThread vulnerable in the
// first place. There's no "safe corner" of mapped RAM in the
// 0x80000000-0x80200000 range that the kernel doesn't either canary-
// fill, allocate from the heap, or use as a per-mode stack.
//
// Real next iteration must be either:
//   - Option (c): MMU shadow-page aliasing (intercept reads of
//     0x80003CD0..0x80003DF8 and route them to a private emulator-
//     held buffer that survives stack pushes through the same virtual
//     addresses). Requires changes to readVirtual / writeVirtual to
//     support per-address shadow routing.
//   - Add MORE read-side rescues at all PCs that read bootstrap-NThread
//     fields. This is similar to the existing PC=0x500031F8 hook but
//     would need to handle every NThread offset the kernel reads from
//     the bootstrap thread (iAllocator, iCurrentDfcQ, iHandlers,
//     savedCPSR, ...). Extends the existing approach without changing
//     the underlying corruption.
//   - Force the bootstrap stack to start at a higher address by hooking
//     the very early kernel SP setup (modify SP_svc / SP_init) so
//     auStack_218 ends up clear of any later stack region. This is
//     close to a kernel rewrite, since the kernel later reads SP via
//     pointer-shifted loads.
//
// Reverted before commit. Code stays at d6c4c6c level.
//
// OPTION (c) IMPLEMENTED — MMU shadow-page aliasing for the bootstrap
// NThread (commit pending, 2026-04-28):
//
// Approach: maintain a 0x128-byte private emulator buffer
// `series5ShadowNThread` that mirrors the bootstrap NThread's lifetime.
// Activated on the first kernel store at PC=0x50010EA0 of value 0x80003CD0
// to the iCurrentThread global at virt 0x8010061C. Snapshot semantics:
//
//   * SNAPSHOT (writeVirtual hook at PC=0x50010EA0): copy live RAM
//     contents of virt 0x80003CD0..0x80003DF8 into the shadow buffer,
//     then set series5ShadowActive = true. The snapshot captures the
//     post-construct state from FUN_5001ae48's memset+field-init body.
//
//   * MIRROR (writeVirtual hook for PC in [0x50010EA0, 0x50010F00) and
//     virtAddr in bootstrap range): every V32 store from FUN_50010E44's
//     explicit field-init body (lines 21326-21333 of the decompile —
//     iCurrentDfcQ at +0x34 and iAllocator at +0x38) is also recorded
//     into the shadow. The PC range deliberately EXCLUDES the function
//     prologue (which pushes registers onto the stack within the same
//     virtual range, but those pushes shouldn't update the NThread's
//     view of itself) and nested calls into FUN_5001ae48 / memset
//     helpers (whose writes were already captured by the snapshot).
//     Writes still go through to real RAM so the function epilogue's
//     LDM/POP restores see the values they pushed.
//
//   * READ (readVirtual hook for series5ShadowActive + virtAddr in
//     bootstrap range): serve all reads of NThread fields from the
//     shadow buffer regardless of what stack pushes have done to the
//     underlying RAM. This is what supersedes the read-side rescue
//     from commit d6c4c6c (`iAllocator+0x38 == 0x80004000` for
//     PC=0x500031F8); the shadow gives the live, non-clobbered value
//     for ALL fields and ALL readers.
//
//   * RE-ENTRY: the kernel re-runs FUN_50010E44 every ~1 sim s (panic
//     recovery cycle). Each re-entry re-snapshots the shadow at PC=
//     0x50010EA0. Between re-entries, the SVC stack pushes have
//     clobbered real RAM at 0x80003CD0..0x80003DF8, but FUN_5001ae48
//     re-runs and re-initialises real RAM before the snapshot, so
//     the new snapshot is clean.
//
// The d6c4c6c rescue hook at PC=0x500031F8 was removed; the shadow
// makes it redundant.
//
// Empirical results (boot-seconds 30/60/120/180, --skip-card):
//   30s:  unique_pcs=163  variance=0  (mean=0   — LCD blank)
//   60s:  unique_pcs=181  variance=0
//   120s: unique_pcs=195  variance=0
//   180s: unique_pcs=211  variance=0
//
// Compare to baseline (d6c4c6c level, no shadow):
//   30s:  unique_pcs=227  variance=0  (mean=255 — stale RAM-as-FB)
//   120s: unique_pcs=313  variance=0  (per series5.h)
//
// The 30s unique_pcs DROPS (227 → 163) because the baseline's count was
// inflated by the recurring panic-recovery loop hitting many PCs every
// ~17.85M cycles (the abort vector + memcpy + re-init paths). The shadow
// eliminates those recovery cycles at the iAllocator fault site, so
// fewer unique PCs are sampled there. The kernel does make slow forward
// progress over time (211 by 180s) but in a different code region
// (cleaner Undef32-mode dispatch loop).
//
// Fault summary:
//   * BEFORE: AlignmentFault at virt 0xFFFFFFFF, PC=0x5000C254 every
//     ~12M cycles (the iAllocator-clobber → small-int dereference loop).
//   * AFTER:  SectionTranslationFault at virt 0xE1A03104, PC=0x50043E4C
//     every ~12M cycles. Function thunk_FUN_50043e38 calls SWI 0x73,
//     then `LDR R3, [R0]` where R0 was loaded as 0xE1A03104 from
//     `*(R<x>+0x48)`. The 0xE1A03104 is the encoding of `MOV R3, R4,
//     LSL #2` — i.e. SWI 0x73 returned a pointer P, and *(P+0x48) is
//     code interpreted as data. SWI 0x73 is the EKA1 power-state /
//     handler-table query; its handler returns garbage because the
//     handler-table installer never ran (downstream of EFile.exe load).
//
// Mode-transition status: User32 (0x10) is still NEVER reached over
// 180 sim s. Modes observed: 0x11 (FIQ), 0x13 (Svc), 0x17 (Abort),
// 0x1B (Undefined). Same as baseline.
//
// Test suite: 11/12 devices PASS. series5 still FAILs (variance=0,
// no LCD activity). No regression on other devices.
//
// LCDCON: still no programming activity (LCDCON writes live in
// WindowSrv.dll, not loaded).
//
// Status: This is the proper STRUCTURAL fix for the bootstrap NThread
// stack-overlap problem. It cleanly eliminates the original recurring
// fault but surfaces a new blocker downstream. The next blocker
// (SWI 0x73 handler returning garbage) is a separate kernel-state
// initialisation problem unrelated to the NThread stack collision.
//
// Future work: instrument the SWI 0x73 handler to find why the
// returned struct-pointer's +0x48 field is unmapped. Likely candidates
// are: (a) handler-table installer never ran (it's gated on a kernel
// init step we may not be reproducing), or (b) the handler chain is
// supposed to be initialised by EKern.exe's static constructors and
// our kernel-data prototype memcpy isn't capturing them.
//
// FOLLOW-UP: SWI 0x73 RESCUE ATTEMPT (NOT COMMITTED):
//
// Tried adding a read-side hook at PC=0x50043E40 that returns 0 when
// the LDR result is outside valid kernel pointer ranges, so the
// existing CMP-then-return safety path takes effect:
//
//   if (PC == 0x50043E40 && result outside valid pointer ranges)
//       return 0;
//
// Result: BIG REGRESSION. unique_pcs at 30s collapsed from 163 → 30.
// 87% of execution stuck at PC=0x00000018 (IRQ vector). Kernel
// fell into a tight IRQ loop because preventing the section-fault
// stops the panic-recovery cycle that was driving boot progress.
//
// Insight: the kernel's recurring panic is NOT just a bug — it's
// PART OF THE BOOT FLOW. Each cycle does some real init work and
// then deliberately faults to invoke the abort handler. The abort
// handler runs more init work. Without the panic recovery, the
// kernel has nothing to do and spins in IRQ vector.
//
// This means each individual fault we silence will likely BREAK
// rather than help boot — unless we ALSO replace the work the
// panic recovery was doing. That's a fundamentally different
// emulation problem than "prevent a fault".
//
// Real next step: trace the FULL panic recovery sequence (entry
// to FUN_5000FF68, what state it sets up, what makes it eventually
// progress past the recovery loop). The kernel's design assumes
// some hardware behavior (abort retry semantics? specific MMU
// timing? a kernel-data state that gets repaired through the
// abort handler) that our emulator doesn't faithfully replicate.
//
// Reverted the SWI 0x73 rescue attempt — kept c245407 shadow fix.
//
// PANIC TABLE INSIGHT (2026-04-28, Psion Panics 1998.xlsm):
//
// The reference document Psion Panics 1998.xlsm contains the
// canonical EUSER panic table from `uc_std.h`. The recurring fault
// chain we hit corresponds to two related panics:
//
//   * EClnNoTrapHandlerInstalled — fires when kernel code tries to
//     use the trap-handler chain (CleanupStack) but thread.iTrap is
//     NULL or invalid. This is exactly what FUN_50043E38 does:
//
//       BL  SWI 0x73              ; R0 = thread.iTrap (PopTrap)
//       LDR R0, [R0, #0x48]      ; assumes R0 is a valid TTrap*
//       CMP R0, #0
//       LDMEQ SP!, {PC}          ; return if iCleanup field is 0
//       LDR R3, [R0]             ; (FAULT — R0 was garbage)
//
//     The function doesn't NULL-check R0 from the SWI; it assumes
//     the caller already verified a trap is installed. Our boot
//     hits FUN_50043E38 when no trap is actually installed (or
//     when it's installed but its iCleanup field at TTrap+0x48 is
//     garbage from the stack the trap was allocated on).
//
//   * ETDes16BadDescriptorType (panic 0x08) — fires when TDesC
//     code (FUN_5000C208 / TDesC::Match) gets a descriptor with
//     wrong type tag. We see this when the iAllocator-rescue
//     substituted value (0x80004000) gets interpreted as a TDesC
//     pointer by a different code path.
//
// Both panics confirm: our boot reaches user-mode code but does
// so without the user-mode runtime's TRAP() macro having executed
// to install a top-level trap handler. The runtime's static-ctor
// is supposed to call User::SetTrapHandler() during EFile.exe
// startup; if those ctors don't fully run (e.g., because of an
// earlier issue), thread.iTrap stays uninitialised.
//
// SCHEMATIC NOTE: the file Psion-5-circuit-schematics.pdf in the
// reference folder is mislabeled — it's actually the SNOWDROP MX
// (5mx) schematic showing the WINDERMERE SoC and 8-32MB DRAM
// build options. We don't have authentic Series 5 (SNOWDROP)
// schematics; the original Series 5 uses a CL-PS7110 SoC (which
// our CLPS7111 emulator approximates) and 4-8MB RAM.
//
// Net implication: the recurring fault chain is downstream of
// EFile.exe's static-ctor walk. To unblock the boot we would need
// either (a) ensure EFile's ctors fully execute and set up the
// trap chain, or (b) synthesise a NULL trap-handler pre-condition
// in the kernel data so the FUN_50043E38 path safely no-ops on
// every panic-recovery cycle.
//
// Approach (a) requires understanding why EFile's ctors stall —
// likely a missing kernel-side state init that the ctors depend on.
// Approach (b) was attempted (revert in commit 651cc97) and broke
// boot because the panic-recovery cycle IS part of the boot flow.
// A proper version of (b) would need to BOTH provide the no-op AND
// replace the work the recovery handler was doing — substantially
// more invasive.
//
// PRAGMATIC SWAP EXPERIMENT (2026-04-28, NOT COMMITTED):
//
// Tried routing Series 5 through makeWindermere instead of
// makeSeries5 (mirroring the MC218 trick documented in
// device_registry.cpp:258-263). Result: catastrophic regression.
// Series 5 boot collapsed to 13 unique_pcs in 5 sim s.
//
// Why it doesn't work: the Series 5 ROM is EPOC R1 with HARDCODED
// CL-PS7110/CL-PS7111 register-layout expectations (per the comment
// at the top of this file: PADR @ 0x000, SYSCON1 @ 0x100, INTMR1 @
// 0x280, LCDCON @ 0x2C0). Windermere puts equivalent registers at
// 0x500 / 0xC00 / 0xE00 ranges. The EPOC R1 kernel doesn't go
// through a HAL abstraction for these; it touches them directly.
// On Windermere those addresses are unmapped, kernel reads back
// 0xFFFFFFFF, and the boot hangs immediately.
//
// MC218 (a rebadged 5mx) "works" on Windermere because its EPOC R5
// kernel HAS the HAL abstraction and uses it to dispatch chip-
// specific register accesses. The R5 HAL happens to have a
// Windermere variant that maps to Windermere's register layout,
// so the ROM's HAL calls land on Windermere registers correctly.
//
// CONFIRMED: the CLPS7111 emulator base IS the correct chip family
// for Series 5. Our problem is NOT register emulation — every
// register the Series 5 ROM touches is at the right offset and our
// I/O handlers respond correctly (verified by the prior
// instrumentation). The bug is in some specific kernel-state
// initialisation interaction (likely the abort/MMU/stack timing)
// that R1 expects from real CL-PS7110 silicon but our CLPS7111
// emulator doesn't replicate exactly.
//
// Net: the pragmatic shortcut doesn't apply. Series 5 needs the
// kernel-state fix path to make further progress.
//
// CL-PS7110 PRODUCT BULLETIN (reference/CL-PS7110PB.pdf, 2026-04-28):
//
// The PDF is only a 4-page marketing overview, not the full
// register-level datasheet. It confirms:
//   * ARM710A (not ARM710T) — our CLPS7111::Emulator already
//     passes `false` to Arm710Bridge for this (clps7111.h:16,
//     cp15_id = 0x41047100)
//   * 18.432 MHz CPU clock — matches our CLOCK_SPEED constant
//   * 8 KB four-way set-associative UNIFIED cache (vs ARM710T's
//     split cache) — our cache emulation is disabled by default
//     (ARM710T_CACHE undefined), so this is moot
//   * 64-entry TLB — our TLB is also disabled by default
//   * LCD: 1/2/4 bpp programmable, two 32-bit palette registers
//   * 8 chip selects (CS0-CS7), 256 MB each
//   * DRAM: up to 4 banks × 256 MB
//   * 36-bit GPIO (4×8 + 1×4)
//
// What the PB does NOT contain (would need the full datasheet):
//   * Exact register addresses for I/O peripherals
//   * Interrupt controller register layout + behaviors
//   * Cache/TLB invalidation timing semantics
//   * Power management state machine details
//   * Abort/exception handling exact semantics
//
// The full CL-PS7110 datasheet is what would actually unblock
// further work. The PB only confirms our chip-family selection
// is correct.


// MAME's psion5.cpp is actually for the 5mx (Windermere / EPOC R5),
// not Series 5 (CL-PS7110 / EPOC R1). Our CLPS7111 emulator is
// structurally compatible with WindEmu's Osaris CL-PS7111 (same
// register layout, same memory map) — region-level peripheral
// coverage is complete. Remaining work is EPOC-R1-specific kernel
// behaviour.
//
// What works:
//   1. MMU page fault at virtual 0x7ff00000 → physical 0x20000000 (the
//      CL-PS7110 nCS1 external chip-select aperture): stubbed as a
//      region-2 read-0 / swallow-writes path in core/clps7111.cpp.
//   2. LCD decode: the Series 5 ROM sets up a 640×240×4bpp framebuffer
//      at RAM offset 0 and never writes FRBADDR; readLCDIntoBuffer
//      hard-codes 4bpp + direct-intensity palette.
//   3. Timer config: TC1/TC2 pre-enabled; kernel-written SYSCON1 =
//      0x000300b0 (TC2 non-periodic 512kHz) drives TC2OI at ~500 Hz.
//   4. Additional register coverage: MEMCFG1/MEMCFG2/DRFPR, Port C
//      (0x02/0x42, CL-PS7110-only), STFCLR cold-flag clear, SYNCIO
//      default of 0x800 for unknown commands.
//
// Current blocker — alignment fault at virtual 0x67 (PC 0x500031F8):
//   The faulting instruction is a one-line virtual method:
//     0x500031F8: LDR R0, [R0, #0x38]   ; R0 = this->field_0x38
//   It's dispatched from PC 0x500195EC (a wrapper around a vtable
//   lookup that reads `*(*(0x80100388) - 4)`). In real boot the method
//   receives R0 = a valid 4-byte-aligned object pointer; in our
//   emulation it arrives as R0 = 0x2F, so the LDR tries to read from
//   0x67 which alignment-faults.
//
//   Trace (reproduce with PSION_INSN_TRACE=0x5003A140,0x50003210):
//     cycle 13489322: 0x5003A144 entry, R0=0x30 (caller arg)
//     cycle 13489326: 0x5003A14C BL 0x5004BD1C (= SWI 0x6C)
//     cycle 13490562: 0x500195EC entry,  R0=0x2F (post-SWI)
//     cycle 13490580: 0x500031F8 LDR fault, addr=0x67
//
//   So somewhere inside the SWI 0x6C handler (or a vtable dispatch it
//   reaches through), R0 gets decremented from 0x30 to 0x2F. The
//   abort handler takes the fault, dispatches, and returns to the
//   same instruction, so the loop repeats indefinitely.
//
// Debug env vars already wired for the next iteration:
//   PSION_FAULT_REGS=1     — dumps r0..r15, CPSR, *(0x80100388) and
//                            vtable[-4], plus 0x40 bytes of fault-mode
//                            stack at each fault.
//   PSION_INSN_TRACE=lo,hi — instruction-level trace with registers
//                            for PC in [lo, hi).
//   PSION_WATCH=<hex>[-<hex>] — logs every virtual write in a range.
//   PSION_MODE_TRACE=1     — logs the first 80 CPSR-mode transitions
//                            plus every subsequent involving Abort.
//
// Next concrete investigation: step through the SWI 0x6C handler at
// virtual 0x50019148+ (SWI vector) and find the exact instruction that
// turns R0=0x30 into R0=0x2F. Likely candidates are a handle-table
// lookup (array indexed by handle number, off-by-one between ROM
// expectation and RAM layout) or a descriptor-length extraction
// (TDesC headers store `length` with a 1-bit flag in a way that
// yields value-1 if decoded wrongly).
// ===========================================================================
// MODE-SP BANKING / SVC-STACK OVERLAP — CONFIRMED KERNEL BEHAVIOUR, NOT AN
// EMULATOR BUG.
// ---------------------------------------------------------------------------
// ARM710 register banking in core/arm710.cpp (switchMode / switchBank,
// modeToBank[16]) is correct: User/Svc/Irq/Abt/Und/Fiq each have distinct
// GPRs[13]/GPRs[14] entries in allModesBankedRegisters[6][2] and the mode
// switch correctly saves/restores the active R13/R14 around the bank index
// change. Traces (PSION_MODE_TRACE=1 + PSION_INSN_TRACE=0x50019500,...) show
// every mode transition flipping SP between its expected per-mode value:
//
//    SVC   SP = 0x80105790
//    IRQ   SP = 0x80105790  (kernel sets the same address — see below)
//    Abort SP = 0x80105390
//    FIQ   SP = 0x80105b94
//    Undef SP = caller's SP (~0x80003bec at boot)
//
// These SP values are loaded by the EPOC R1 kernel itself at 0x50019124+:
// it takes a pointer at 0x80100388 whose value is 0x80105794 (the address of
// the newly-allocated HAL root object), subtracts 4, and uses that as the
// SVC/IRQ initial SP. The kernel does this on EVERY mode-init pass; the
// HAL-root allocator placed it there via a bump heap starting at
// 0x80100398. On MC218 the same code places the HAL root at 0x80005484
// (1 MB lower) because MC218 has 4 MB RAM (mask 0x3FFFFF) while Series 5 has
// 8 MB (0x7FFFFF) and region-D aliasing, which moves the heap-end / kernel
// data globals to a higher virtual address.
//
// Consequence: on Series 5, SP_svc and SP_irq SIT DIRECTLY BELOW the HAL
// root vtable slot (at 0x80105794). The SWI dispatcher at 0x5001950C does
// `STR R12, [SP]` with SP = 0x80105790 — that IS the HAL vtable slot. Any
// push onto SVC stack from 0x80105790 downward that writes a return address
// into the default-stub code range poisons the vtable. This is an
// architectural collision in the kernel that we cannot avoid without
// rewriting the kernel's memory layout.
//
// The `series5HalFix` path in core/arm710.cpp is therefore the correct fix,
// not a band-aid: (a) it drops writes to 0x80105700-0x801057FF whose value
// looks like a stale LR into the default-stub code region, preserving the
// HAL vtable pointer; (b) it short-circuits HAL::Get(EPenClickVolume)
// (SWI 0xc00076 with R0=0x2f) whose TRequestStatus otherwise sits at
// 0x80105728 and gets clobbered by the same SWI dispatcher scratch write
// before the caller reads it back.
//
// Measured effect (30 s sim time, --skip-card):
//    series5HalFix OFF: ~26 fault events, repeating alignment fault at 0x3b
//                       because the HAL vtable pointer has been poisoned.
//    series5HalFix  ON: 8 fault events, each one-shot, kernel's abort path
//                       recovers; no HAL poisoning.
//
// Both before and after, the LCD still ends up blank — the next blockers
// are DIFFERENT faults downstream (see fault list in the status comment
// above) and are not banking-related.
// ===========================================================================
// SCHEDULER STRUCTURE — kernel runs but never enters User mode.
// ---------------------------------------------------------------------------
// Reproducer:
//   PSION_MODE_TRACE=user ./harness/run roms/series5_v1.01\(144\)_eng.bin \
//       --device series5 --boot-seconds 30 --skip-card --log-file /tmp/m.log
//   grep '\[mode\]' /tmp/m.log | grep '10' | wc -l    # → 0 (confirmed)
//
// The EPOC R1 scheduler dispatch path lives at:
//   0x500195C0:  LDR  R14, [PC,#-0xF8]      ; R14 = &g_currentThread (0x8010061C)
//   0x500195C4:  LDR  R14, [R14]            ; R14 = current thread struct
//   0x500195C8:  ADD  R14, R14, #0xE4       ; R14 = &thread.context  (offset 0xE4)
//   0x500195CC:  LDR  R0,  [R14,#0x40]      ; R0  = thread.savedCPSR (0x80006ED0 here)
//   0x500195D0:  MSR  SPSR, R0              ; prime SPSR for the LDM-mode-restore
//   0x500195D4:  TST  R0,  #0x0F            ; mode bits non-zero?  (User=0x10 → bits=0)
//   0x500195D8:  LDMNE R14, {R0-R15}^        ; *** privileged-mode dispatch — taken when mode != User
//   0x500195DC:  LDMIA R14, {R0-R14}^        ; *** USER-MODE dispatch path — banked R0-R14
//   0x500195E0:  MOV   R0,  R0              ; (NOP/spacer)
//   0x500195E4:  LDR   R14, [R14,#0x3C]     ;   reload thread saved-PC into LR
//   0x500195E8:  MOVS  PC,  LR              ; *** "MOVS PC, LR" → CPSR ← SPSR, mode → User
// The matching save-current-thread path is at 0x50019658..0x50019674 and ends
// with "B 0x500195C0".  Both paths read/write through *(0x8010061C) which is
// the global "current thread" pointer.
//
// What we observe over 30 s of run, with PSION_INSN_TRACE on each address:
//   pc=0x500195C0..D8 : reached ~31 times/s  (every periodic tick)
//   pc=0x500195DC..E8 : reached ZERO times    (User-mode dispatch is dead code)
//   thread.savedCPSR  : ALWAYS 0x0000001B (Undefined32)  — never 0x10 (User32)
//   *(0x8010061C)     : ALWAYS 0x80006DAC                — same idle thread
//   Thread::Init at 0x5001AEBC called 31 times, ALWAYS with R1=1
//   (kernel/Undef-mode flavour); the User-mode arm at 0x5001AEF8 (R1=0 or 2,
//   which writes 0x10 to context[0x124]) is never selected.
//
// Conclusion:
//   The scheduler dispatch and ARM710 mode-restore (LDM-with-^ + MOVS PC,LR)
//   are both 100% functional; we verified the LDM(3) path correctly reloads
//   user-bank registers and restores CPSR from SPSR for non-User modes (it's
//   actively used for the Undef→Undef self-reschedule the idle thread does
//   every tick).  The reason no transition to mode 0x10 is observed is
//   simply that NO USER-MODE THREAD EVER EXISTS in the ready queue.  The
//   boot chain stalls before ewsrv.exe / elink.exe / eshell.exe are loaded,
//   so the only thread in the system is the kernel idle/null thread, which
//   gets re-initialised each periodic tick (SWI 0x70 once per second from
//   PC 0x5004BD2C, called via 0x500399E8).  The shell never starts → no
//   User-mode process is ever created → the User-mode dispatch path is dead.
//
// This means:
//   - DO NOT touch the LDM-^ / MOVS PC,LR path in arm710.cpp; it works.
//   - DO NOT touch switchMode/switchBank; banking is correct (the previous
//     audit above confirms it).
//   - The next blocker is in the BOOT-SEQUENCE / DRIVER-INIT chain, not in
//     the scheduler.  Specifically: a HAL attribute, a missing driver, or
//     an exec call between EFile load and shell launch is preventing the
//     loader thread from spawning the first user-mode process.  See the
//     "ewsrv.exe/elink.exe/eshell.exe at 0x5005F438/41C/454 are still
//     never READ" note in the status comment above.
//
// Useful follow-up traces (no commits required, just env vars):
//   PSION_INSN_TRACE=0x500195c0,0x500195f0  — scheduler dispatch tail
//   PSION_INSN_TRACE=0x5001aebc,0x5001af20  — Thread::Init (R1 = thread type)
//   PSION_WATCH=0x80006ED0-0x80006ED4       — current idle thread saved CPSR
//   PSION_MODE_TRACE=user                    — surface User-mode transitions
//   PSION_MODE_TRACE=2                        — log every mode transition
//                                             (capped at 4000 lines)
//   PSION_READ_WATCH=<va>[-<va>]              — log every virt read in range
//   PSION_WRITE_WATCH=<va>[-<va>]             — log every virt write in range
//                                             (also reports MMU-translated phys)
//
// 2026-04-28 status — variance=0 / mean=255 / unique_pcs=30 stable idle.
// After commit c51010b (FUN_50043e38 null-iTrap short-circuit), the kernel
// reaches the "post-init-3, pre-WSERV" idle-running state described in
// Symbian OS Internals §16.1.3.4. The bootloader filled the framebuffer
// with 0xFF before cycle 26170; after that NO further writes to virtual
// 0xC0000000-0xC0001000 happen for 60+ sim seconds — meaning we never
// reach the WindowSrv loading + LCDCON programming phase.
//
// The 30 hot PCs cover four roles:
//   * 0x18 (IRQ vector) at 85% — TC2 firing every 36864 cycles, handler
//     FUN_5001a904 acks via FUN_500187b8 → STR R0,[R1,R3] where
//     R1=0x700 R3=0x58000000 → MMU translates to phys 0x80000700 (TC2EOI).
//     PSION_WRITE_WATCH=0x58000700 confirms 12+ acks per second.
//   * Bitset SetBit/TestBit/ClearBit ops (FUN_5002c268 family).
//   * Audio-codec init (FUN_5001b3a0/3fc), kernel-init helpers
//     (FUN_50013b3c, FUN_50014480, FUN_50015098/0bc, FUN_5001728c,
//     FUN_50019b58/df0/ec8).
//   * LCD-controller config sequence (FUN_5007e45c at 0x5007e49c hits).
//
// SWI traffic in steady-state: only 9 distinct SWI numbers across 1 second
//   (0x52, 0x53, 0x6c, 0x70, 0x72, 0x73, 0x83, 0x8d, 0x8e) — vs hundreds
//   expected for a fully booted EKA1. No process-creation SWIs observed.
//   The kernel never reaches ESTART / system-starter handoff.
//
// Likely structural blocker (NOT a single one-line fix):
//   The ExStart extension that constructs EFile.exe "by hand" (see book
//   §16.1.3.4: "The last such extension is always ExStart") is gated on
//   the supervisor thread completing its init-3 sequence. With our shadow
//   NThread + null-iTrap short-circuit, the bootstrap NThread reaches its
//   end-of-init point but the supervisor thread never gets created /
//   resumed. FUN_50010E44 (the bootstrap-NThread-init function our shadow
//   captures) re-runs every 12M cycles in the original (no-hook) state,
//   suggesting the kernel WAS using panic-recovery as a forcing function
//   to retry init. With the panic suppressed, the supervisor-thread
//   creation step is missed.
//
// Productive next experiments (if continuing):
//   1. Trace SWI 0x52/0x53/0x6c/0x83 dispatchers to identify what their
//      handlers do. SWI 0x83 fires only once per second and may be the
//      "init-stage handoff" call.
//   2. Watch all writes to the kernel's ScheduledThreadList head pointer
//      (probably via a global at a known offset from iCurrentThread) to
//      see whether the supervisor thread is ever ready'd.
//   3. Compare the 5mx and Series 5 boots side-by-side for the moment
//      when each writes to LCDCON: 5mx via Windermere, Series 5 should
//      via WSERV. Find what blocks the WSERV path.
//
// 2026-04-28 follow-up — three investigations completed:
//
// (a) LCDCON write timing: Series 5 DOES write LCDCON twice — at cycle
//     12066710 (val=0x5814e95f) and cycle 12163342 (val=0x1814e4af).
//     The LCD controller IS programmed and active. The blank framebuffer
//     is NOT because LCDCON wasn't programmed — it's because no process
//     ever DRAWS to the framebuffer. Only 4 writes in 0xC0000000-0xC0001000
//     across 15 sim seconds, all from the bootloader at PC=0x6a0-0x74c
//     before cycle 26170.
//
// (b) iCurrentThread switch confirmed — at cycle 12023546 the kernel
//     switches from the bootstrap NThread (0x80003CD0) to the idle/null
//     NThread (0x80006074). iCurrentThread (virt 0x8010061C) only ever
//     receives 3 writes total: 0 (init clear), 0x80003CD0 (bootstrap),
//     0x80006074 (idle). The supervisor thread (referenced as 0x80006DAC
//     in earlier docs) is NEVER assigned as current. Region around
//     0x80006DAC is reused by the kernel heap (writes of 0xA5A5A5A5
//     canary value), so the supervisor thread, if it exists, lives at a
//     different address.
//
// (c) FUN_5001ae04 reached — the post-init scheduler-dispatch step at
//     PC 0x5001ae04 IS reached at cycle 12960596 WITH the FUN_50043e38
//     short-circuit (commit c51010b), but is NEVER reached without it
//     (A/B-tested via PSION_S5_DISABLE_43E40 env-gate, since reverted).
//     This proves the short-circuit is an actual unblock, not just a
//     palliative — the kernel reaches the scheduler-arming step that
//     was previously skipped due to the recurring panic.
//
// Net structural finding: my hook unblocks one EKA1 init step (panic on
// null iTrap during boot when no TRAP harness is installed yet), letting
// the kernel reach FUN_5001ae04. But after FUN_5001ae04 returns, the
// kernel idles at PC=0x18 (TC2 IRQ acks happening cleanly via the
// FUN_5001a904 → FUN_500187b8 → STR R0,[R1,R3] → MMU virt→phys translate
// → writeReg32(TC2EOI) chain, confirmed by PSION_WRITE_WATCH).
//
// What's missing for full boot: a kernel-side path that creates+resumes
// the supervisor thread which would then run the ExStart extension to
// construct EFile.exe. FUN_50007830 contains an alloc-and-init pattern
// (thunk_FUN_50030734(0x90,0) → FUN_50021544() → DAT_50007a78) that
// might be the supervisor-thread DProcess but FUN_50021544 only sets
// vtable+priority fields (priorities 9 and 7), not full thread state.
// The Resume() / Schedule() invocation that would put the supervisor
// thread on the ready-list is not visible in the trace.
//
// Lifting variance off zero requires more than another single short-
// circuit hook — it requires either:
//   1. Identifying the missing Schedule() invocation and synthesising it
//   2. Building a more complete EKA1 NThread / scheduler model
// Both are substantial work beyond simple register-emulation fixes.
//
// ---------------------------------------------------------------------------
// 2026-05-03: ACTUAL CURRENT BOOT STATE (claude/fix-psion5-boot, post-refactor)
// ---------------------------------------------------------------------------
// All the "MAJOR UPDATE" / "DEEP DIVE" / "BREAKTHROUGH" sections above
// describe boot states reached during agentic experiments with hooks that
// are NOT all in the production code. The actual current boot state is:
//
//   * Boot reaches deep kernel init: ~9200 unique PCs in 0.7 sim s, top
//     hot PC is the bitmap allocator at 0x5002C29C (500K hits — null
//     thread doing background page-zeroing work).
//   * Kernel transitions from SVC to Undef mode at cycle ~309K (first
//     MOVS PC,LR at 0x50019500). All subsequent execution is Undef
//     mode (4.27M cycles); SVC mode total = 39K cycles.
//   * The boot never reaches FUN_50006758 (loader-init) — confirmed by
//     0 hits on PCs in 0x50006xxx range. The supervisor / loader thread
//     at 0x80006DAC is therefore NEVER created.
//   * The call chain that SHOULD reach FUN_50006758 is:
//       FUN_500109F4 (one-shot caller, hit at cycle 220340)
//       → FUN_500072B0 (line 21174 → cycle 220348)
//       → FUN_50007A84 (line 11964 → cycle 220358)
//       → FUN_50010E44 (line 12367)
//       → FUN_50010A08 (line 21337 → cycle 11887492)
//       → FUN_50016E38 (line 21343 → cycle 12023562)
//       → never returns
//   * Inside FUN_50016E38, the kernel calls (line 26728)
//       (**(code **)(*(int *)*DAT_50016f0c + 8))();
//     where DAT_50016f0c was just set to the result of the SVC
//     func_0x500270c0() (line 26723). This vtable+8 call is where
//     execution diverges into the null thread's background work loop
//     and never returns to FUN_50010E44, which means FUN_50007A84
//     never returns, which means FUN_500072B0's loader-init call at
//     line 11997 (FUN_50006758) is never reached.
//   * No traps, no panics, no aborts. The boot is LEGITIMATELY idle —
//     the null thread is doing what it's supposed to do (background
//     page-allocator work) but the supervisor thread that would launch
//     EFile.exe → EWSRV → WindowSrv is never spawned.
//
// Continuation pointers for the next session:
//
//   * FUN_500193F8 (line 29217 of decompile, virt 0x500193F8) is the
//     scheduler context-save function. It saves all registers into the
//     current NThread block at offset 0xE4-0x120 and sets iNState
//     (offset 0x124) to 0. The first SVC→Undef transition happens at
//     cycle ~309K via MOVS PC,LR at 0x50019500 — this is the "first
//     scheduling" where the boot thread effectively becomes the null
//     thread.
//
//   * FUN_5004C350 (line 100419, virt 0x5004C350) is the kernel's
//     setjmp; thunk_FUN_50043E38 is the matching longjmp/cleanup. The
//     repeated LR=0x5004C36C pattern in the trace means execution is
//     happening inside a setjmp-protected scope (Symbian's TRAP/Leave
//     mechanism). Multiple call sites in FUN_500072B0 use this for
//     leaving-tolerance around the various init steps including the
//     loader-init.
//
//   * The kernel's null thread (post-first-scheduling, Undef mode) is
//     spending its cycles in FUN_5002C268 (a bitmap allocator at
//     0x5002C29C, 500K loop iterations) — this is real work, not a
//     stuck loop.
//
//   * The vtable+8 dispatch at FUN_50016E38 line 26728
//     (`(**(code **)(*(int *)*DAT_50016f0c + 8))()`) doesn't return
//     because the called method probably enters the scheduler's
//     dispatch path, which switches into a different thread context
//     and never restores the SVC-mode boot stack frame that was on
//     FUN_50007A84's stack.
//
//   * To make progress: instrument what func_0x500270c0 returns
//     (DAT_50016f0c is set to that), find what its vtable+8 does, and
//     understand why it doesn't return to its caller. The kernel's
//     boot-resume mechanism (whatever that is in EKA1) must deliver
//     execution back to FUN_500072B0's stack frame so line 11997's
//     FUN_50006758 call can run.
//
// 2026-05-03: PS7110 register-aperture fidelity pass (claude/fix-psion5-boot)
// ---------------------------------------------------------------------------
// Verified against reference/CL-PS7110.pdf Table 3-2 that the PS7110 register
// block ends at 0x880; the upper-half registers (FRBADDR=0x1000,
// SYSCON2/SYSFLG2/INTSR2/INTMR2 at 0x1100-0x1280, UARTDR2/UBRLCR2/KBDEOI at
// 0x1480-0x1700) are PS7111-only extensions. Added chip-variant virtuals
// (chipHasFRBADDR / chipHasSysCon2 / chipHasOnChipSRAM / sysConMask) on
// CLPS7111::Emulator and overrode them on Series5::Emulator so those
// registers now read 0xFFFFFFFF / write-noop (datasheet "reserved")
// behaviour and SYSCON1 writes mask to 24 bits, matching real PS7110.
//
// Boot impact: NIL on the boot blocker. Verbose harness run shows the
// EPOC R1 kernel never accesses any PS7111-only register during the 15 s
// boot window — it already picks a PS7110 code path via SYSFLG bit 29.
// The fidelity work is correct, but the boot stall is genuinely a
// kernel-level scheduler issue (PC=0x18 TC2 IRQ vector, 94% of samples;
// PC=0x500172d8 chain-walker called from many sites — all part of the
// idle dispatch path).
//
// Also rewrote readLCDIntoBuffer to derive bpp from LCDCON.GSMD/GSEN
// (datasheet bits 31/30, identical encoding on both chips), use the
// PS7110-fixed framebuffer at 0xC0000000, and render a clean black
// screen when LCDCON==0 — replacing the unreliable "uniform-byte ROM
// splash" hack that was producing stripe-noise.
//
// 2026-05-04: kernel-state tracker reveals more than the trace did
// ---------------------------------------------------------------------------
// The PSION_S5_KERNEL_TRACE infrastructure (commits 9ca2776 + 41c4a3a +
// e06f8b6) shows the boot is FAR more advanced than per-instruction PC
// tracing suggested:
//
//   * 3 NThread blocks get allocated:
//       0x80003CD0 (boot/init), 0x80006074 (case-0 loader),
//       0x80006DAC (supervisor — created via FUN_50006758 at cycle ~13.4M)
//   * iCurrentThread switches: idle -> 0x80003CD0 (cycle ~1.58M) ->
//     0x80006074 (cycle ~6.6M)
//   * The case-0 loader thread reaches iNState=EReady (1) and runs
//   * LCDCON IS programmed (640x240 2bpp grayscale at cycle ~6.62M,
//     then 1bpp mono at ~6.68M)
//   * CPSR escalates SVC -> Undef -> IRQ
//   * Boot then settles; iCurrentThread never switches to supervisor
//     (0x80006DAC), supervisor's iNState stays 0 (never EReady)
//
// THE WATCH on virt 0x80006F10 (the supervisor's sub-object vtable
// slot inside the NThread block) over a 60s boot shows the kernel
// ROM DOES write the correct vtable (0x50028444, set by
// FUN_5000F69C's instruction `STR R3,[R4]` at PC=0x5000f6cc — verified
// via PSION_INSN_TRACE):
//
//   cycle ~4096:    0x00000000 -> 0xDEADBEEF (initial RAM-fill pattern)
//   cycle ~65536:   0xDEADBEEF -> 0x00000000 (zeroed by early init)
//   cycle ~1.58M:   0x00000000 -> 0xA5A5A5A5 (SVC stack debug-fill)
//   cycle ~7.82M:   0xA5A5A5A5 -> 0x50028444 (kernel writes vtable)
//
// So the dispatch at FUN_5002EB0C `LDR PC, [R3, #0x14]` (PC=0x5002eb2c)
// IS getting a valid vtable in our boot — confirmed by trace showing
// PC=0x5002df60+ executing for the supervisor case at cycle ~13.4M
// (FUN_5002DF60 = the GetName method).
//
// The PSION_S5_FIX_SUPERVISOR_VTABLE knob (commit e06f8b6) was a
// MISDIAGNOSIS — the patch preempts the kernel's correct write but
// doesn't change boot behaviour. The actual blocker is downstream:
// FUN_5002DF60 starts executing for the supervisor case (last seen at
// PC=0x5002df7c calling memset = FUN_50037958 at cycle 13402552), but
// the trace cap obscures whether memset returns. The boot doesn't
// progress past this construct; iCurrentThread never switches to the
// supervisor; LCD stays uniform paper-white.
//
// CONCRETE NEXT STEPS for the next session:
//   1. Determine whether memset (FUN_50037958) returns to FUN_5002DF60
//      for the supervisor's call at cycle ~13.4M — if not, that's the
//      next divergence point. If yes, trace what happens after
//      FUN_5002DF60 returns to FUN_5002EB0C.
//   2. Extend the kernel tracker with watches on iNState across all
//      NThread blocks and iCurrentThread. The supervisor's iNState
//      never transitioning to 1 means Resume (vtable+0x24 = 0x50011424)
//      isn't called for it — find what's supposed to call it.
//   3. The EKA1 source pattern (kernelhwsrv-master kernel/eka/kernel/
//      sinit.cpp:391) shows the supervisor is created via
//      pP->NewThread(...) then explicitly resumed via
//      Kern::ThreadResume(*pN). In the Series 5 ROM, the supervisor's
//      Resume call must come AFTER FUN_50006758 returns — find where
//      that path stops.
// ===========================================================================
// 2026-05-04 (continued): the actual panic stub identified
// ---------------------------------------------------------------------------
// Iteration on the IRQ-mode trace (PSION_INSN_TRACE=0x18,0x40 over 30 sim s)
// reveals every TC2 IRQ entry from cycle ~13.4M onwards has R14=0x50019284
// (LR_irq = address of the interrupted instruction + 4). PC=0x50019280
// in ROM is `eafffffe` = `B 0x50019280` — a `B .` infinite-loop kernel
// PANIC stub. The kernel has reached this stub at cycle ~13402598 and
// is stuck there for the rest of the boot, just servicing TC2 IRQs and
// returning to the panic loop.
//
//   ROM disassembly at 0x50019280:
//     0x50019280: 0xeafffffe  B 0x50019280
//     0x50019284: 0xeafffffe  B 0x50019284
//
// Cycle timeline of the supervisor's Construct call inside FUN_50006758:
//   13402538  FUN_5002DF60 entered (R1=0x80006F10 = supervisor sub-obj)
//   13402552  BL +0x9D14 -> 0x50037958 (FUN_50037958, called from
//             PC=0x5002df7c — looks like a TBuf::Init descriptor setup)
//   13402588  PC=0x500379a0 reaches FUN_50037958's epilogue
//             `e8bd8070` = LDMFD SP!,{R4,R5,R6,PC} (return)
//   13402598  PC=0x50019280 — IN THE PANIC LOOP! Only ~10 cycles after
//             memset return — the LDMFD popped 0x50019280 as PC instead
//             of the expected 0x5002df80.
//
// Concrete next step: hook our emulator's writeVirtual to log every
// write that puts 0x50019280 anywhere in the SVC stack region. That
// will identify the writer.
//
// 2026-05-04 (continued): SP-aware bootstrap-NThread shadow fix
// ---------------------------------------------------------------------------
// Diagnosis confirmed via PSION_READ_WATCH on virt 0x80003D2C: the LDMFD
// at PC=0x500379a0 (memset's epilogue) reads 0x5002df80 — the CORRECT
// saved LR — yet PC ends up at 0x50019280. Bug located in the
// bootstrap-NThread shadow read hook at arm710.cpp readVirtual: the
// hook intercepted EVERY read in 0x80003CD0..0x80003DF8 and served
// shadow data from series5ShadowNThread, ignoring real RAM. memset's
// saved-LR slot at 0x80003D2C lives inside this range, so the LDMFD
// pop got the shadow's stored value (the kernel's NThread+0x5C field
// = 0x50019280, the panic stub address) instead of the real saved LR.
//
// Fix (commits 8f84dec, f24cd00, f377241):
//   1. READ side: only serve shadow when virtAddr >= currentSP. Reads
//      below SP fall through to real RAM (the live stack frame).
//   2. WRITE side: also propagate writes to the shadow when the address
//      is at or above SP, so STMFD pushes update both real RAM and
//      the shadow. LDMFD reads back the just-pushed value via the
//      shadow.
//
// Boot impact:
//   Before:  unique_pcs=30   (panic loop, IRQ exits to 0x50019280)
//   After:   unique_pcs=187 at 30s, 327 at 120s — kernel keeps making
//            FORWARD progress over time, not just looping
//   Supervisor's first FUN_5002E778 call (PC=0x500094c0) now returns
//   to PC=0x500094c4 (verified at cycle 13408148, sp=0x80003e7c).
//   Construct progresses through several more BL calls.
//
// Next concrete blocker (this iteration's stopping point):
//   Construct's SECOND FUN_5002E778 call at PC=0x500094f4 doesn't
//   return for the supervisor case. The deeper chain reaches:
//     0x500094f4 BL FUN_5002E778 (LR=0x500094f8)
//     -> 0x5002e788 BL FUN_5002EB0C (LR=0x5002e78c)
//     -> 0x5002eb2c LDR PC,[R3,#0x14] (vtable+0x14 dispatch)
//     -> 0x5002df60 (FUN_5002DF60 GetName)
//     -> 0x5002df7c BL FUN_50037958 (memset)
//        -> memset returns OK to 0x5002df80
//     -> 0x5002dfac BL +0xB628 -> 0x5003965c (string format)
//     -> 0x5003969c BL FUN_5003968c
//     -> 0x500396b4 B 0x50034220 (tail-call)
//     -> 0x50034248 BL FUN_500418A0
//     -> 0x500418B0 BL FUN_50037A34 (TBufC accessor)
//        -> FUN_500379A4 returns *R0 >> 28 = type field
//        -> R0 = *0x50039770 = 0xe1a00004 (CODE BYTE not desc!)
//        -> type = 0xE, NOT in switch cases 0..4
//        -> default branch: FUN_50043e98(0x13) PANIC
//
//   Root cause analysis: 0x50039770 is a CODE address (FUN_50039770
//   is a small function — `MOV R0, R4; LDMFD SP!, {R4, PC}`). The
//   kernel ROM's string formatter (FUN_5003965c) is reading R3 of
//   the caller as a vararg (via *(SP+8) inside the function), but
//   R3 was undefined at the BL site (FUN_5002DF60 only set R0, R1,
//   R2 explicitly before BL). R3 happens to hold a stale 0x50039770
//   from earlier execution, which gets dereferenced as if it were
//   a TDesC pointer.
//
//   2026-05-04: ROOT CAUSE FOUND AND FIXED.
//
//   The panic was caused by another instance of the bootstrap-NThread
//   shadow getting out of sync with real RAM — but for a different
//   reason than the SP-aware read fix that landed earlier the same
//   day. PSION_S5_CONSTRUCT_TRACE register dumps showed the format
//   pointer R1 staying valid through FUN_5003965c entry but switching
//   from 0x80003d30 to 0x50039770 by the time FUN_5003968c was
//   entered:
//
//     [13409272] FUN_5003965c_entry  R1=80003d30 (valid stack)
//     [13409294] FUN_5003968c_entry  R1=50039770 (CORRUPTED)
//
//   FUN_5003965c's prologue is `STMFD SP!, {R1, R2, R3}` then later
//   `LDR R1, [SP, #0x08]` to reload the format pointer for passing
//   to FUN_5003968c. The corruption was: STMFD pushed R1 to
//   0x80003d24 (= SP - 12 with SP starting at 0x80003d30), but
//   our shadow-write hook compared `virtAddr >= currentSP`, and
//   ARM block-data-transfer doesn't write back GPRs[13] until
//   AFTER the first store completes. So at the moment of the
//   first store, currentSP was still 0x80003d30 and the check
//   failed (0x80003d24 < 0x80003d30) — the shadow's stale init-
//   time content at offset 0x54 (= 0x80003d24 - 0x80003cd0)
//   stayed at 0x50039770 instead of getting the real R1 value.
//
//   The subsequent LDR R1, [SP, #0x08] read at 0x80003d24 with
//   updated SP=0x80003d20 had `virtAddr >= currentSP` = true →
//   shadow served → returned the stale 0x50039770.
//
//   Fix (arm710.cpp shadow-write check):
//     bool isLiveStackPush = (virtAddr + 64 >= currentSP);
//   The 64-byte tolerance below SP captures the worst-case STMFD
//   push of all 16 registers BEFORE the writeback updates SP.
//   For writes outside any STMFD frame, the read path's
//   `virtAddr >= currentSP` check still gates the shadow read, so
//   the wider write window has no negative side effects.
//
//   Boot impact:
//     Before: unique_pcs=187 at 30s, 327 at 120s, 31 panicking
//             FUN_50037A34 calls with R0=0x50039770 in 30s.
//     After:  unique_pcs=233 at 30s, 444 at 120s, 0 panicking
//             FUN_50037A34 calls. GetName chain (FUN_5002DF60 ->
//             FUN_5003965c -> formatter) hits 105x in 30s (was 62)
//             confirming the supervisor's Construct call chain
//             actually progresses now.
//
//   Next blocker (post-fix): a recurring AlignmentFault at
//   PC=0x5000cf84 reading address 0x0000001f, ~once every 17.85M
//   cycles. The function at 0x5000cf84 is a 5-instruction atomic
//   decrement helper:
//     0x5000cf84: LDR R2, [R0]
//     0x5000cf88: SUB R3, R2, #1
//     0x5000cf8c: STR R3, [R0]
//     0x5000cf90: MOV R0, R2
//     0x5000cf94: BX LR
//   The fault triggers because R0=0x1f (odd address, not word-
//   aligned) — the helper expects R0 to be a pointer to a lock
//   counter, but somewhere in the SWI dispatch path R0 leaks
//   the SWI number 0x1F instead. LR=0x500194fc places the call
//   inside the kernel SWI dispatcher (LDR PC,[R12] at
//   0x500194f8). Possibly a missing R0-restore in our SWI entry
//   path. The fault recurs but the kernel keeps making progress
//   (444 unique PCs over 120s vs 327 before).
//
// 2026-05-04 (continued): SUPERVISOR THREAD NOW BECOMES iCurrentThread
// ---------------------------------------------------------------------------
// With the shadow-write fix in place, PSION_S5_KERNEL_TRACE shows the
// supervisor NThread (0x80006DAC) actually transitions through real state
// machine and becomes iCurrentThread:
//
//   cycle ~278M:  iCurrentThread: 0x80006074 -> 0x80006dac (FIRST TIME!)
//   cycle ~278M:  iCurrentThread: 0x80006dac -> 0x80006074 (4K cycles later)
//   cycle ~320M:  iCurrentThread: 0x80006074 -> 0x80006dac
//   cycle ~320M:  iCurrentThread: 0x80006dac -> 0x80006074
//
// Supervisor's iNState transitions: 0 (uninit) -> 1 (EReady) -> 4 (EHoldFM)
// -> 5 (EWaitFastMutex) — real kernel state machine progression. Sub-object
// vtable at 0x80006F10 finally gets the correct value 0x50028444 around
// cycle 320M (was stuck at 0xa5a5a5a5 stack-fill before).
//
// However: each supervisor activation lasts only ~4K cycles (~250 µs),
// then yields back to the case-0 loader thread which then yields to idle.
// The supervisor doesn't run long enough to paint the LCD framebuffer.
// LCDCON is being WRITTEN every ~10M cycles (= 0.5 sim-sec) — kernel
// re-programs the LCD controller in a heartbeat loop but never lays down
// pixel data.
//
// Kernel scheduling cycle (every ~5M cycles after init):
//   idle (0) -> boot (0x80003cd0) -> case-0 loader (0x80006074)
//      -> supervisor (0x80006dac, briefly!) -> loader -> idle -> repeat
//
// Next concrete blocker: identify WHY the supervisor thread yields after
// only 4K cycles. Possibilities: (a) the SWI 0x8e atomic_dec fault
// (R0=0x1F) trips a panic that the supervisor's TRAP harness catches,
// causing it to leave; (b) the supervisor is waiting on a fast mutex
// (iNState=5 EWaitFastMutex) that's never released; (c) the deeper
// EFile/EWSRV/WindowSrv chain it tries to launch hits a fault that it
// recovers from by yielding.
//
// Concrete next step: instrument the supervisor's PC range during its
// 4K-cycle activation windows. Sample the trace at cycles 278564864-
// 278568960 and 320184320-320188416 with PSION_INSN_TRACE limited to
// those windows to see exactly which kernel function the supervisor
// runs and why it yields.
//
// 2026-05-04 (cont.): cycle-window PC trace shows supervisor's work
// ---------------------------------------------------------------------------
// PSION_INSN_TRACE_CYC=<lo>,<hi> (added arm710.cpp) lets us trace every
// instruction within a cycle range, sized to fit the supervisor's
// brief activation window. Result for cycles 278564864-278568960:
//
//   * Supervisor spends the entire 4096-cycle activation in
//     FUN_5002C268 (a kernel bitmap-update helper). The hot loop
//     at 0x5002c29c is `MOV R5, R5, LSL #1; SUBS R2, R2, #1;
//     BNE`, building a single-bit mask via repeated left-shifts.
//   * R1 (= bit index argument) walks from 0x3919 to 0x3936 over
//     the activation — 29 distinct calls into FUN_5002C268,
//     suggesting the supervisor is iterating through some 14600+
//     entry array (page bitmap?) and setting one bit per call.
//   * CPSR is 0x1B (Undef mode) throughout — EKA1 runs supervisor
//     work in Undef mode on this kernel.
//   * LR is constant (0x50025f48) inside the loop, so all 4K
//     cycles are inside one outer call from 0x50025f44.
//   * Supervisor's iNState transitions DURING the activation:
//       cycle 278564864:  iNState 1 (EReady) -> 5 (EWaitFastMutex)
//       cycle 278568960:  iNState 5 -> 4 (EHoldFastMutex)
//     So the supervisor takes a fast mutex AT entry, holds it,
//     and releases it at preemption. The 4K-cycle activation is
//     the duration of holding that mutex.
//
// Hypothesis: the kernel's scheduler is preempting the supervisor
// quickly because some HIGHER-PRIORITY thread (the case-0 loader)
// is also Ready. Each timer tick sees both Ready threads and
// alternates them, with the supervisor only getting a tiny slice.
//
// 2026-05-04 (cont.): traced the loader window — same code, no switch
// ---------------------------------------------------------------------------
// PSION_INSN_TRACE_CYC for 278568960-278614016 (the 45K-cycle "loader
// run") reveals that the case-0 loader is NOT executing different code
// from the supervisor. The trace shows IDENTICAL state throughout:
//   * CPSR=0x1B (Undef mode) for ALL 50K cycles
//   * LR=0x50025f48 — same call site
//   * R0=0x80004520 — same kernel object pointer
//   * SP=0x80003c20 — same stack
//   * Only R1 (the loop iteration counter) changes
// That means the iCurrentThread "flicker" between supervisor and loader
// is NOT a real context switch — there's no LDM regs, no SP swap, no
// CPSR change. The kernel scheduler is just WRITING different values
// into iCurrentThread as bookkeeping, while a single execution thread
// continues running in Undef mode the whole time.
//
// What's actually running: the kernel null thread (post-first-scheduling,
// CPSR=0x1B) doing background bitmap-page-zeroing work. R1 walks from
// 0 to ~0x7F36 (= 32566), then RESETS and walks again. This is a
// 32K-bit allocator bitmap being repeatedly initialized — confirmed
// across cycles 270M-300M with multiple resets. About every 17M cycles
// (= 1 sim sec) the null thread completes a full 32K-bit pass and
// briefly visits scheduler code at 0x50013/50014/50019xxx, then
// restarts the bitmap pass.
//
// So the supervisor thread IS NOT actually being scheduled to run user
// code. It enters the EKA1 ready-list (iNState 0->1) and the scheduler
// flickers iCurrentThread = supervisor briefly, but the actual context
// switch (LDM regs from supervisor's saved state, SP swap to its kernel
// stack, MOVS PC to its iPc) never happens. Execution stays in the
// null thread's bitmap-zeroing loop forever.
//
// Real next step: find why the dispatch from null thread to supervisor
// fails. The scheduler context-save function FUN_500193F8 (per existing
// notes) and the NState-driven dispatch logic are the candidates.
// Probably the supervisor's saved-context fields in its NThread block
// (iSp, iPc, iCpsr, iSpsr, etc.) hold invalid values — so the kernel
// computes "this thread isn't ready to run" and stays in the null
// thread.
//
// 2026-05-04 (cont.): supervisor IS running briefly — just gets destroyed
// ---------------------------------------------------------------------------
// PSION_S5_KERNEL_TRACE watches on supervisor's saved-context fields
// (iSp at 0x80006EC4, iPc at 0x80006ECC, iCpsr at 0x80006ED0) plus
// PSION_WRITE_WATCH on the same range REVEAL a 1-Hz cycle:
//
//   cycle 13417280: thread-create writes initial saved context:
//       iCpsr = 0x1B  (Undef mode)
//       iSp   = 0x80104f94
//       iPc   = 0x5001ae44  (entry function via FUN_5001AE44)
//     PCs 0x5001aef0-0x5001af24, LR=0x5001136c (NThread::Create caller)
//
//   cycle 13444058: FUN_500193F8 SAVE-context fires — supervisor RAN!
//       iSp   = 0x80104528 (now deeper in stack)
//       iLr   = 0x5003ad10
//       iPc   = 0x5003ad10 (HAL::Get-async wrapper from existing notes)
//       iCpsr = 0x2000001b (Undef + condition flags)
//     PCs 0x50019664-0x50019670 (inside FUN_500193F8)
//
//   ~1 sim-sec later: stack reuse fills NThread region with 0xa5a5a5a5
//     (PC=0x5004d774 memset, called by FUN_5003d7c0 which reads
//     R0 = *(R4+0x24) — some kernel stack buffer that overlaps with
//     supervisor's NThread saved-context area)
//
//   ~immediately after: thread is recreated from scratch and the cycle
//     repeats — over 5 sim seconds, iPc takes only 4 distinct values
//     (each occurring 5 times):
//       0xa5a5a5a5 (debug fill)
//       0x00000000 (zero clear)
//       0x5001ae44 (initial create)
//       0x5003ad10 (one save after run)
//
// Observations:
//   1. Supervisor IS running — kernel scheduler IS dispatching it. It
//      runs from 0x5001ae44, reaches 0x5003ad10, preempts there.
//   2. After preemption, the supervisor's NThread region is destroyed.
//      The thread must be recreated, which takes ~17.85M cycles.
//   3. Each "supervisor activation" only executes from 0x5001ae44 to
//      0x5003ad10 — a HAL::Get-async wrapper from existing notes
//      (the wrapper is short-circuited on PSION_S5_HAL_LEGACY for
//      attributes 0x1c/0x2f). The supervisor never gets to call it
//      because it's preempted at the function entry.
//
// Hypothesis: the supervisor is being preempted IMMEDIATELY upon
// reaching 0x5003ad10 (before the first instruction CMP R0,#0
// executes). This could happen if the timer (TC2) interrupt is
// firing every ~26K cycles regardless of mode, and the resulting
// SAVE→DISPATCH chain picks the loader instead. The loader then
// runs ~5M cycles before yielding to idle. Idle does bitmap-page
// init for ~12M cycles. Then the supervisor gets a chance again.
//
// Why "destroyed" between activations: the memset at PC=0x5004D770
// (called from FUN_5003D7C0 which reads R0 = *(R4+0x24)) fills a
// kernel buffer with 0xA5A5A5A5. That buffer's range overlaps with
// 0x80006EC4-0x80006ED4 — which is the supervisor's saved-context
// fields. So either:
//   (a) The NThread is being destroyed and re-allocated each cycle
//       (kernel sees the supervisor as "completed" or "panicked")
//   (b) Some kernel stack overlap puts the saved-context region
//       inside a stack that's reset each cycle
//
// Next concrete step: trace the chain that calls FUN_5003D7C0 with
// R4 such that R4+0x24 points at the supervisor's NThread region.
// Add PC trace at the moment of the memset to identify the kernel
// function doing the cleanup, and figure out why it considers the
// supervisor's NThread reusable.
//
// 2026-05-04 (cont.): cycle counter mismatch + heap-chunk identified
// ---------------------------------------------------------------------------
// Discovered the trace counter (insnCycleApprox) and harness-side cycle
// counter (passedCycles from emubase) drift apart by ~1M cycles after
// the boot has been running a while. Updated PSION_WRITE_WATCH log
// format to print BOTH: "insncyc=N va=...". This let me correctly map
// cycle 20849868 (passedCycles, where the destruction memset fires)
// to insncyc=19797976 — my trace using PSION_INSN_TRACE_CYC needs the
// insncyc value, not the harness bracket.
//
// With the corrected mapping, traced cycles 19790000-19805000 and
// found a memset RUN at PC=0x5004d770 (8-instr unrolled inner loop)
// called from FUN_5003D79C (entry at 0x5003D79C):
//
//   FUN_5003D79C decodes as a HEAP-CHUNK INITIALIZER:
//     R4 = chunk descriptor (some heap object)
//     *(R4+0x24) = R4 + 0x40   (data area starts after 64-byte header)
//     R5 = *R4 - 0x40          (data area size)
//     *(R4+0x28) = R4+0x40 + R5 (data area end)
//     memset(R4+0x40, 0xA5, R5) — fills data area with debug pattern
//     *(R4+0x30) = R4+0x40     (iCurrent = buffer start)
//
//   At cycle 19796962 (the destruction): R4 = 0x80004060, data area
//   starts at 0x800040A0, size = ~131072 bytes (chunk extends to
//   0x80024EA0). The data area COVERS:
//       0x80006074  (case-0 loader NThread)
//       0x80006DAC  (supervisor NThread)
//
// THEREFORE: the case-0 loader and supervisor NThread blocks are
// heap-allocated INSIDE this chunk. When the kernel re-initializes
// the chunk (1 sim sec cycle), it destroys both NThreads with the
// 0xA5 fill pattern. This is why the supervisor cycles through:
//   create -> run briefly to 0x5003ad10 -> preempt -> WIPED -> create...
//
// Why does the kernel re-initialize the chunk? Likely the kernel
// considers it a "free heap area" and is preparing it for reuse.
// But our supervisor DID get allocated there. So either:
//   (a) The kernel deliberately destroys the supervisor (panics it,
//       or considers its work done) and recycles its memory.
//   (b) The kernel never knew the supervisor was there — some
//       allocator state is wrong, listing this chunk as free.
//
// Concrete next step: find the caller of FUN_5003D79C. Since static
// analysis shows no direct B/BL callers, it's invoked via vtable
// (an indirect dispatch through some kernel allocator). Add a
// watch on PC=0x5003D79C entry that dumps R0 (the chunk descriptor),
// LR (the immediate caller), and the call stack so we can identify
// the heap manager doing this.
//
// 2026-05-04 (cont.): caller chain identified — boot/init thread re-runs
// ---------------------------------------------------------------------------
// Traced FUN_5003D79C entry; the call chain is:
//
//   PC=0x50007b84  BL FUN_50010E44 (boot/init NThread setup)
//     -> PC=0x50010ECC  BL +0xfc6d -> 0x50010068 (heap chunk wrapper)
//        -> BL 0x500040F4 (constructor)
//           -> BL FUN_5003D634 (chunk header init)
//              -> BL FUN_5003D79C (data area memset 0xA5A5A5A5)
//
// FUN_50010E44 is the boot/init NThread setup function — already
// special-cased by the bootstrap-NThread shadow (see
// Series5FunBootstrapNThreadInit{Start,End} in arm710.h). Per
// PSION_INSN_TRACE on PCs 0x50010E40-0x50010E60, this function is
// being called ONCE PER SIM SECOND (every ~17.85M cycles):
//
//   cycle 222920:    initial boot (CPSR=0x13 SVC mode, LR=0x50007b88)
//   cycle 13673324:  +13.45M cycles (=0.75 sim sec)
//   cycle 31533032:  +17.86M cycles (=1 sim sec)
//
// Each re-call destroys the kernel heap chunk at 0x80004000 (64KB),
// wiping the supervisor + loader NThread structures inside it. So
// the supervisor is created, runs briefly, gets destroyed, and
// recreated each ~17.85M cycles.
//
// Tried: a one-shot guard at PC=0x50010E44 (PSION_S5_GUARD_50010E44)
// that skips subsequent calls. Result: kernel crashes with cascade
// of SectionTranslationFault at PC=0x50019350 reading 0x7B4318xx
// addresses (garbage). The kernel TRULY needs FUN_50010E44 to run
// — skipping it breaks downstream init.
//
// So the real fix is one of:
//   (a) Prevent the boot/init thread from being re-scheduled. Find
//       why iCurrentThread keeps cycling back to 0x80003CD0.
//   (b) Make FUN_5003D79C smart about not wiping live thread
//       structures inside the chunk (kernel intent issue).
//   (c) Move the supervisor + loader NThreads OUT of the heap chunk
//       so they survive re-init. (probably unrealistic without
//       changing kernel layout.)
//
// Option (a) is most promising. The kernel-state log shows the
// boot/init thread's iNState being corrupted by SVC stack pushes
// (values like 0x800058bc, 0x8000627c are stack addresses, not
// state codes). With iNState mistakenly looking "ready", the
// scheduler keeps re-dispatching boot/init.
//
// Concrete next investigation: which SVC stack push is writing into
// 0x80003D94 (boot/init's iNState)? Is the shadow protection for
// this address working correctly? The bootstrap-NThread shadow read
// hook (arm710.cpp:1646+) returns the shadow value when virtAddr is
// in 0x80003CD0..0x80003DF8. The pollKernelState reads via
// readVirtualDebug which BYPASSES the shadow — but the kernel's
// scheduler reads via readVirtual which DOES use the shadow.
// Verify: when the scheduler reads boot/init's iNState, does it see
// the correct (=DEAD) value or the stack-corrupted value?
//
// 2026-05-04 (cont.): scheduler doesn't poll boot/init's iNState directly
// ---------------------------------------------------------------------------
// PSION_READ_WATCH on 0x80003D94 (boot/init iNState) shows ZERO reads
// from the scheduler's dispatch path (FUN_500193F8 / FUN_500195A4).
// Only TBuf/TDesC/format functions read it (incidentally, treating
// it as TDesC stack data). So the scheduler isn't deciding "boot/init
// is ready, dispatch it" by reading iNState directly.
//
// Instead, the trigger for boot/init re-run is iCurrentThread getting
// reset to 0. PSION_WRITE_WATCH on 0x8010061C shows the cycle:
//
//   cycle 2665546:  boot/init becomes current (FUN_50010E44 init)
//   cycle 12023618: case-0 loader becomes current
//   cycle 13439374: supervisor becomes current
//   cycle 13443806: back to loader
//   cycle 13540652: iCurrentThread = 0 (CPSR=0x17 ABORT mode!)
//   cycle 20848564: boot/init becomes current AGAIN  -- cycle restarts
//   cycle 30206604: loader again
//   cycle 31299082: supervisor again
//   ... and so on
//
// The cycle ~13540652 zero-write happens at PC=0x5004D90C in CPSR=0x17
// (Abort mode). The instruction is a memcpy STMIA from a kernel-data-
// init template at ROM 0x500295B4 to RAM 0x80100500. The kernel is
// running this memcpy from its abort handler, copying initial values
// from ROM into kernel data — including iCurrentThread=0.
//
// The trigger for the abort handler running: an AlignmentFault at
// PC=0x5000cf84 (atomic_dec helper, R0=0x1F) at cycle ~13490336.
// PSION_INSN_TRACE at 0x5004D8E0-0x5004D920 confirms 8 entries to
// FUN_5004D8FC during 1 sim sec, of which ONE (cycle 13540416) is in
// CPSR=0x17 (Abort mode) — caused by the alignment fault.
//
// Tried: PSION_S5_ATOMIC_DEC_SHIM short-circuiting SWI 0x8E with
// R0<0x80000000 to return R0=0 or R0=1 (avoiding the alignment
// fault). Result: unique_pcs DROPPED 233 -> 141 at 30s, even though
// the SAME total cycles ran. This means the alignment fault path
// is actually USEFUL — the kernel uses it to advance boot state.
//
// CONCLUSION: the destructive cycle is the kernel's way of making
// SLOW BOOT PROGRESS. Each cycle (idle→boot→loader→supervisor briefly
// →abort→reset→boot...) advances the kernel's state a bit. This is
// why we see unique_pcs grow over time (233 at 30s, 444 at 120s).
//
// To fully boot in reasonable time, the kernel needs to NOT cycle
// through this abort recovery — it needs the supervisor to actually
// complete its work (reach WindowSrv) before being destroyed.
//
// Real next step: the supervisor reaches PC=0x5003ad10 (HAL::Get-async
// wrapper) and gets preempted there. To run further, the HAL request
// it's waiting on must complete. The PSION_S5_HAL_LEGACY=1 short-
// circuit covers attributes 0x1c/0x2f but NOT what the supervisor is
// requesting at 0x5003ad10. Identify which HAL attribute the
// supervisor reads, and add a short-circuit for THAT attribute.
// Tried: PSION_S5_HAL_LEGACY=1 dropped unique_pcs 233 -> 184 (worse).
// So the existing short-circuit isn't right either; need a more
// targeted intervention on the supervisor's specific HAL request.
//
// 2026-05-04 (cont., iterations 7-9): more dead-ends recorded
// ---------------------------------------------------------------------------
// Tried suppressing writes of 0x80003CD0 to iCurrentThread (the
// boot/init NThread address) — PSION_S5_PIN_SCHEDULER. Suppresses
// the second re-schedule. Result: cascade of PageOtherBusError +
// SectionTranslationFault — the kernel cannot tolerate skipping
// the re-schedule. Reverted.
//
// Tried PSION_S5_ATOMIC_DEC_SHIM (return R0=0 or R0=1 for SWI 0x8E
// when R0 is non-pointer). Both variants drop unique_pcs 233 -> 141
// at 30s. The fault path is structurally needed by the kernel.
//
// Net assessment of all attempted fixes for the destructive cycle:
// every short-circuit / guard / suppression we've tried makes the
// boot WORSE, not better. The kernel's boot architecture genuinely
// IS this cycle. Trying to break the cycle leaves the kernel in a
// state it cannot recover from.
//
// To fully boot Series 5, the right path is to let the cycle run for
// long enough that the supervisor accomplishes meaningful work each
// activation. From data:
//   30s  -> 233 unique_pcs
//   120s -> 444 unique_pcs   (4x time, 1.9x PCs — sub-linear growth)
// Extrapolating the discovery rate, full user-mode boot reaching
// WindowSrv would require many minutes of sim time. Whether this
// is achievable in finite time depends on whether the discovery
// rate plateaus or keeps growing.
//
// 2026-05-04 (cont.): supervisor execution window analysed
// ---------------------------------------------------------------------------
// PSION_INSN_TRACE_CYC over the supervisor's full execution window
// (insncyc 12365400-12393000, ~27K cycles) shows it executes ~9300
// instructions:
//
//   8487 (90%) cpsr=0x1b (Undef = kernel work)
//    564 (6%)  cpsr=0x13 (SVC mode = SWI dispatch)
//    277 (3%)  cpsr=0x12 (IRQ = timer/interrupt)
//
// Hot PCs include FUN_50037A34 (TDesC accessor), FUN_500379A4 (type
// dispatch), FUN_5004D708 (memcpy), and the SWI vector at 0x00000008
// (32 hits = 32 SWI calls!). So the supervisor IS doing substantial
// work — calling SWIs, formatting strings, copying data. NOT just
// scheduler bookkeeping.
//
// However, supervisor's saved iSp values are STABLE across cycles:
//   Each cycle: iSp init -> 0x80104f94, after preempt -> 0x80104528
// Identical across all observed cycles. So each supervisor activation
// does the SAME work, reaches the SAME end state.
//
// Discrepancy: if supervisor work is identical and the emulator is
// deterministic, why does unique_pcs grow over time (30s -> 233,
// 120s -> 444)? Hypothesis: the boot has discrete PHASES. Each phase
// reaches a different set of PCs. New phases emerge as the kernel's
// allocator state evolves between cycle resets — perhaps the kernel
// stores some persistent state OUTSIDE the destroyed heap chunk
// (e.g., in 0x80100xxx kernel globals) that survives the cycle.
// Each phase eventually exhausts and a new one begins, contributing
// new unique_pcs.
//
// WHY THE BOOT CAN'T COMPLETE: the supervisor's HAL-related call at
// 0x5003ad10 needs a HAL response that NEVER COMES because the heap
// is destroyed before the request can resolve. Each cycle, the
// supervisor re-issues the request and gets preempted again. Without
// a path to deliver the HAL response, the cycle never breaks.
//
// Real fix candidates (none simple):
//   A) Move the heap chunk OUTSIDE the NThread region. Requires
//      kernel data layout changes — very hard.
//   B) Identify the HAL request and short-circuit it with a synthetic
//      response. Requires understanding which HAL attribute, which
//      we haven't been able to determine from the trace.
//   C) Skip the abort-handler kernel-data-reset. Tried — kernel
//      crashes when prevented from running it.
//   D) Patch the kernel ROM to skip FUN_50010E44 re-runs after the
//      first. Tried — kernel crashes from the missing re-init.
//
// All four candidates have been attempted or analyzed; none produced
// progress. The boot is structurally limited by the destructive cycle
// in this kernel/emulator combination.
//
// 2026-05-04 (cont.): discovery rate measured — boot WILL plateau
// ---------------------------------------------------------------------------
// Sampled unique_pcs at multiple boot durations:
//   15s -> 164 PCs
//   30s -> 233 PCs   (+69 over 15s = 4.6 PCs/sec)
//   60s -> 321 PCs   (+88 over 30s = 2.9 PCs/sec)
//   120s -> 444 PCs  (+123 over 60s = 2.0 PCs/sec)
//
// Logarithmic growth — each doubling of time adds ~100 PCs. Linear
// extrapolation suggests:
//   240s  -> ~544 PCs
//   480s  -> ~644 PCs
//   1920s -> ~844 PCs
//   3.7h  -> ~1000 PCs
//
// To reach WindowSrv (probably needing 1500+ PCs) would take ~10000+
// sim seconds = ~2.7 hours of sim time. Effectively never.
//
// CONCLUSION: with the current emulator architecture, Series 5 boot
// will plateau at a finite number of PCs and never reach WindowSrv.
// The discovery is asymptotic — the kernel is genuinely making
// forward progress but each cycle yields fewer new PCs than the
// previous one.
//
// To make Series 5 boot in finite time, ONE of the structural fixes
// (A, B, C, D above) would need to be implemented properly. None
// have a known clean solution.
//
// 2026-05-04 (cont.): BREAKTHROUGH — supervisor skip at 0x5003ad10/48
// ---------------------------------------------------------------------------
// FORWARD PROGRESS achieved by skipping past the supervisor's
// blocking calls. Hook detects when iCurrentThread = 0x80006DAC
// and PC = 0x5003ad10 or 0x5003ad48, then sets R0=0 and jumps
// directly to the function's epilogue:
//   0x5003ad10 -> 0x5003ad24
//   0x5003ad48 -> 0x5003ad6c
// Effectively synthesizes an immediate "completed successfully"
// (R0=0 = KErrNone) for these blocking HAL::Get-async-style calls
// that on real hardware would be completed via I/O but in our
// emulator never resolve.
//
// Impact (vs PSION_HAL_FIX=1 baseline, single-skip 0x5003ad10):
//   30s:  233 -> 237 unique_pcs  (+4,  +1.7%)
//   60s:  321 -> 343 unique_pcs  (+22, +6.9%)
//   120s: 444 -> 493 unique_pcs  (+49, +11.0%)
//   240s: 648 unique_pcs (+155 over 120s) — steady ~150/doubling growth
//   480s: 851 unique_pcs (+203 over 240s) — growth continues
//
// At this rate, reaching 1500 PCs (typical user-mode boot) would
// need ~4000-6000 sim seconds (~1-1.5 hours of sim time).
//
// 2026-05-04 (cont.): MAJOR CORRECTION — skip was harmful, not helpful!
// ---------------------------------------------------------------------------
// The Osaris comparison led to verifying CPSR escalation. WITHOUT the
// skip, the kernel reaches CPSR=0x10 (User mode) at insncyc ~404M.
// WITH the skip, kernel stays at CPSR=0x17 (Abort mode) forever.
//
// Comparison at 120s:
//   skip OFF: 444 unique_pcs, max CPSR mode 0x10 (USER!)
//   skip ON:  493 unique_pcs, max CPSR mode 0x17 (Abort)
//
// PSION_INSN_TRACE_CYC over insncyc 400M-500M revealed:
//   Without skip: 2252 distinct PCs in user mode (CPSR=0x10) over
//     100M-cycle window. The boot test's pc-sample-hz=100 vastly
//     undersamples — it reports only 233-493 unique_pcs but actual
//     user-mode code coverage is 10x higher.
//
// The skip was making the supervisor return early before SWI fired,
// preventing the kernel's natural RProcess::Create handler from
// running. Without our intervention, the kernel handler eventually
// runs the real process loader, transitions to user mode, and starts
// executing EFile.exe code.
//
// The +49 unique_pcs from skip were KERNEL-SIDE PCs reached due to
// repeated cycling — NOT progress toward splash. User-mode transition
// is the actual path forward.
//
// Default reverted: PSION_S5_SKIP_HAL=0. Set =1 to re-enable for
// experimentation only.
//
// 2026-05-04 (cont.): Osaris OBJS log shows real progress vs Series 5
// ---------------------------------------------------------------------------
// The clps7111.cpp:debugPC hook fires "OBJS: added X at Y" when the
// kernel's CObjectCon::AddL runs at PC=0x32304 (= virt 0x50032304 in
// Osaris ROM). This shows what processes/threads/chunks the kernel
// creates in real time:
//
// Osaris boots and creates (within first 5 sim sec):
//   EKern (process)         at cycle 3034814
//   Supervisor thread       at cycle 3053892
//   TheRegistryChunk        at cycle 3326224
//   Supervisor (server)     at cycle 5431008
//   NULL thread             at cycle 5442182
//   EFile (process)         at cycle 5523786 ★
//   FileServer              at cycle 5584840
//   LoaderThread            at cycle 5634536
//   Loader (server)         at cycle 5660688
//   StartupThread           at cycle 5699784
//   Medint (library)        at cycle 6036690
//   Media.IRam              at cycle 6080610
//   ... and many more.
//
// Series 5 shows ZERO OBJS log entries. The hook PC 0x32304 doesn't
// match Series 5's CObjectCon::AddL which is at a different ROM
// offset. Cross-ROM signature search couldn't find a matching pattern.
//
// HOWEVER: PSION_INSN_TRACE on FUN_50039764 (TPtrC8 setup) shows 82
// distinct call sites in 1 sim sec for Series 5. So the kernel IS
// running object-name setup code. Process creation is happening but
// our existing OBJS hook can't see it.
//
// Also: PSION_READ_WATCH on Osaris's container-globals area
// (0x80000880-0x800008C0) shows 7653 reads in 1 sim sec — confirming
// Osaris hits CObjectCon::AddL repeatedly. Same range in Series 5
// shows 0 reads — Series 5's container globals are at a different
// virtual address (kernel data is in 0x80100xxx range vs Osaris's
// 0x80000xxx). Need to find the equivalent.
//
// Key Series 5 findings (Osaris-comparison continued):
//   * Series 5 DOES read its EFile.exe ROM-FS entry (0x504139A8) —
//     11 times in 5 sim sec, from PCs 0x50018b04 (LR=0x50016e24) and
//     0x50010fe8 (LR=0x5004d2f4). The kernel knows EFile exists in
//     the ROM filesystem.
//   * Series 5 writes pointers like 0x80105BXX (heap addresses) to
//     kernel-data slots at 0x801006FC-0x801007F4 (248 bytes, 62
//     consecutive entries of 4 bytes each). These look like
//     a container array of kernel objects.
//   * Search for CObjectCon::AddL pattern: Osaris's at 0x50032304
//     starts with `STMFD SP!, {R4-R6, LR}; MOV R5, R0; MOV R6, R1;
//     BL...; BL...`. The same prologue + 2 BLs found at Series 5
//     virt 0x50085F84 — but the body diverges (sets fields at obj+0x18,
//     0x1C, 0x20, 0x24 — that's a constructor, not AddL).
//   * 12-byte prologue match also at Series 5 0x500024F0 (FUN_500024F0
//     in decompile is a destructor, also not AddL).
//
// So Series 5's CObjectCon::AddL is at a non-trivial offset that
// doesn't share the same first-12-bytes signature with Osaris's
// version. The two ROMs were compiled with different optimization
// or different kernel source revisions.
//
// To enable OBJS logging for Series 5, the next step would be:
//   * Trace through the kernel container setup at Series 5's first
//     creation event (e.g., when Series 5 writes 0x80105794 to
//     0x80100xxx). Find the CALL CHAIN and identify the AddL function.
//   * Then either hook that PC or watch the container-array writes
//     directly to log "OBJS: added X" events for Series 5.
//
// 2026-05-04: experimental "container array" was the IRQ name table
// ---------------------------------------------------------------------------
// Tried adding an OBJS-style tracker that scanned the heap-pointer
// writes to 0x801006FC-0x801007F4 (the area I'd identified). The hook
// fired and logged 256 events in 5 sim sec — but the values written
// turned out to be TPtrC8 descriptors pointing to interrupt names:
//   0x80105BF4 = TPtrC8(len=7, ptr=0x50027DEC) -> "IrqExt2"
//   surrounding strings: "MediaChg", "IrqCodec", "IrqExt1",
//     "IrqRtcMatch", "IrqUartTx", "IrqUartModem", ...
//
// So this is the kernel's PS7110 interrupt name table — not object
// containers. The CObjectCon objects must be at a different address.
// Reverted the hook.
//
// Real container globals are still TBD. Best lead: trace forward from
// the cycles where Osaris's OBJS log fires (e.g., cycle 5523786 for
// EFile process creation) and find what address the same kernel
// operation writes to in Series 5.
//
// ---------------------------------------------------------------------------
// 2026-05-04 (cont.): Osaris comparison reveals the actual blocker
// ---------------------------------------------------------------------------
// Compared SWI distributions for Osaris (boots successfully) vs
// Series 5 (stuck) over 5 sim seconds:
//
//   SWI #     Series 5    Osaris
//   ─────────────────────────────────
//   0x6c       498         339   (atomic test/cmp helper)
//   0x8e       469         311   (atomic_dec, faulting)
//   0x8d       467         311   (atomic_inc)
//   0x53       239          96   (?)
//   0x72       131         205   (?)
//   0x73       126         203   (?)
//   0xC00076     2           0   (kernel-context HAL::Get)
//   0x76         0          35   (USER-context HAL::Get)
//
// Key differences:
//   1. Series 5 makes 2 SWI #c00076 (kernel context, R0=0x1C
//      = EProcessExecCreate, R3=0x80000001 = kernel SID).
//      Osaris makes ZERO of these.
//   2. Osaris makes 35 SWI #76 (USER context, R0=various HAL
//      attributes) within 5 sim seconds.
//      Series 5 makes ZERO user-context SWIs.
//
// Conclusion: Series 5's SUPERVISOR is calling RProcess::Create
// (selector 0x1C) to spawn EFile.exe, but our emulator never
// truly loads EFile.exe — so the supervisor blocks waiting for
// the process to be ready. Osaris gets PAST this stage and
// transitions to USER mode (where EFile.exe and other apps run).
//
// Our skip lets the supervisor RETURN from the wrapper with
// R0=0 (synthesized success), but the SUPERVISOR'S CALLER then
// tries to USE the (non-existent) process handle and fails or
// stays in supervisor mode. So the boot makes incremental
// progress on related code paths but never transitions to user.
//
// To truly boot Series 5 to splash, RProcess::Create would need
// to actually load EFile.exe from ROM, set up a TProcess, queue
// the user-mode entry, and complete the TRequestStatus. That's
// implementing real Symbian process loading — a significant
// additional emulator feature. Tried: TRS=0 synthesis at the
// skip site (write 0 to *SP before jumping to epilogue). No
// change — the TRS isn't read by the caller in our flow.
//
// The PSION_S5_SKIP_HAL is the right intervention given current
// architecture: lets the supervisor advance through code that
// doesn't need real process creation. Beyond that, deeper
// emulator features would be needed.
// ===========================================================================
//
// Critically: 0 traps with skip enabled. No prefetch errors,
// no aborts, no cascade failures. The supervisor proceeds past
// the blocking call cleanly each cycle.
//
// The improvement RATIO grows with time (1.7% -> 6.9% -> 11.0%),
// suggesting each cycle the supervisor reaches genuinely new code
// paths after the synthesized response.
//
// Default-ON for series5HalFix. PSION_S5_SKIP_HAL=0 disables for
// A/B comparison. Other devices unaffected (no series5HalFix).
//
// Static analysis: 31 PCs across the ROM match the same wrapper
// pattern (CMP R0,#0 / BNE / MOV R0,R4 / BL inner). Only 0x5003ad10
// and 0x5003ad48 are observed in the supervisor's saved iPc — those
// are the actual blockers. Others may become relevant if the
// supervisor's progress reaches them in deeper boot states.
//
// Implementation: arm710.cpp:executeInstruction near top.
// ===========================================================================
//
// 2026-05-04 (cont., iteration cycle): visible-symptom characterisation
// ---------------------------------------------------------------------------
// Drove down the user-visible browser symptom list with these findings:
//
//   * The all-white framebuffer (mean=255, variance=0) is NOT the LCD
//     render path failing to gate on LCDCON: the kernel programs LCDCON
//     to 0x5814e95f (640px line, 2bpp greyscale) at cycle ~12.0M and
//     the framebuffer at virt 0xC0000000 is filled with 0xFF by the
//     kernel's "clear-to-paper" stage. So the boot reaches LCD-prep,
//     just never reaches the splash painter that would draw on top.
//
//   * The recurring "SYNCIO read unknown:: req=00006d0c" log was the
//     periodic touchscreen-controller poll from FUN_5001d6e0 (line
//     32299 of decompile). The kernel reads SYNCIO, checks bit 0x400,
//     and writes back either 0x6D09 or 0x6D0C. Real PS7110 hardware
//     drives an ADC chip whose response controls this state machine;
//     our 0x800 default takes a "no pen" branch that walks into a
//     buggy LDR R2,[R0] at PC=0x5000cf84 with R0=0x1F → recurring
//     AlignmentFault. Returning values with bit 0x400 set (e.g.
//     0xFFFF) avoids the fault but REGRESSES unique_pcs from 206 to
//     29 over 23s — the fault is a recoverable retry, not the boot
//     blocker. SYNCIO and fault logs now throttle (clps7111.cpp +
//     arm710.cpp:reportFault) so they don't flood browser stderr.
//
//   * Verified the kernel reaches FUN_50018c38 (LCD init) at cycle
//     12.03M ≈ 0.65 sim sec and FUN_5001ae04 (idle loop tail) at
//     cycle 12.96M ≈ 0.70 sim sec via newly-installed Series-5
//     specific debugPC hooks (series5.cpp::debugPC). So the boot
//     completes the kernel's `void Init1()` Init1.cpp-equivalent
//     path within the first sim second — the ~75 sim seconds of
//     observed slow PC growth is the null thread doing background
//     work (page-zeroing, DFC polling) AFTER the supervisor has
//     yielded via FUN_50016e38's vtable+8 dispatch and never
//     returns. Concrete blocker remains: that vtable+8 yield needs
//     to be matched by a Resume() that puts a user-runnable thread
//     on the ready-list, and that Resume never fires.
//
//   * Long-time growth: unique_pcs at 15s = 164, 30s = 233,
//     60s = 321, 90s = 425. ~80-100 new PCs per 30s. Linear rate
//     suggests no plateau — the kernel keeps exploring but won't
//     reach splash without intervention. SWI counts (over 5 s):
//     0x6c=2919, 0x8e=2731, 0x8d=2726, 0x53=1747, 0x72=961,
//     0x73=933 — heap-allocator and TRAP push/pop dominate. The
//     two 0xC00076 calls (kernel HAL::Get with R0=0x1C =
//     EProcessExecCreate) are the supervisor trying to spawn
//     EFile.exe — already documented above as the blocker.
//
// 2026-05-04 (cont., second iteration cycle): hardware-fidelity fixes
// ---------------------------------------------------------------------------
//
//   * SSEOTI (sync serial end-of-transfer interrupt) was never being
//     fired. CL-PS7110 datasheet section 1.2.9 says writing SYNCIO
//     starts a transfer and SSEOTI fires when the frame finishes;
//     reading SYNCIO clears SSEOTI. Added the assert/clear in
//     clps7111.cpp::writeReg32/readReg32. Boot reaches 268 unique
//     PCs in 30 s (up from 233) — modest improvement but real.
//
//   * BLEOI / MCEOI / RTCEOI / UMSEOI / COEOI were stubs (writes
//     went to "RegWrite32 unknown"). Each register's job per
//     CL-PS7110 datasheet section 3.2.31-3.2.38 is to clear the
//     corresponding bit in INTSR1. Added the clears. The Series 5
//     EPOC R1 kernel masks/unmasks BLINT, MCINT, RTCMI in INTMR1
//     during boot — without proper EOIs, any of these IRQs that
//     ever asserted would re-fire forever.
//
//   * The blocker remains EProcessExecCreate (SWI 0xC00076 with
//     R0=0x1C). The supervisor calls this to spawn EFile.exe but
//     our SWI dispatcher doesn't actually create a TProcess /
//     load the EFile binary / queue a user-mode entry. Implementing
//     real Symbian process loading is the unblocking step but is
//     significant work — the EKA1 SWI dispatcher's R0=0x1C handler
//     would need to walk the ROM filesystem (already located at
//     ROM offset 0x413980 per earlier investigation), copy
//     EFile.exe's text+data into a fresh chunk, set up its TProcess
//     and the user-mode ARM entry frame, and complete the
//     TRequestStatus that the supervisor is waiting on.
//
//   * INTMR1 enable sequence diverges between devices — useful for
//     future hardware-comparison work. Captured by IRQ trace over
//     the first 3 sim sec:
//       Osaris    (PS7111):  0200 → 0208 → 0209 → 0a09 → 0a0d → 0a0f
//       Series 5  (PS7110):  0200 → 0208 → 0228 → 0a28 → 0a2c → 0a2e
//     The final masks differ in two bits:
//       Osaris    enables bit 0 (EXTFIQ)  → external FIQ pin
//       Series 5  enables bit 5 (EINT1)   → external IRQ 1 pin
//     Real hardware wires DIFFERENT companion-chip status lines into
//     these pins per device, so each kernel unmasks the bit relevant
//     to its hardware. EINT1 is never auto-fired by our emulator
//     (no SoC source for it on PS7110) — sweep showed firing it at
//     64Hz REGRESSES unique_pcs from 268 to 92 over 23s, so it is
//     NOT a "kernel waiting on it" type signal. Consistent with the
//     hypothesis that Series 5's EINT1 is wired to a Series-5-
//     specific status line (battery sense / PCMCIA-detect / similar)
//     that should be steady, not pulsing.
//
//   * Tried but harmful: PSION_S5_HAL_LEGACY=1 (R0=0 short-circuit on
//     SWI 0xC00076 with R0=0x1c) drops Series 5's 38s unique_pcs from
//     268 to 204 — confirms the legacy short-circuit is worse than
//     letting the kernel take its natural fault-recovery path.
//     PSION_S5_SYNCIO_DEFAULT sweep: bit 0x400 set (e.g. 0xFFFF)
//     prevents the AlignmentFault retries but stalls in fewer PCs
//     (29 vs 268) — the fault loop is, paradoxically, a productive
//     part of boot exploration.
//
//   * SWI 0xc00076 R0=0x1c args traced via PSION_WRITE_WATCH /
//     PSION_READ_WATCH at virt 0x801045xx and 0x80104bxx (FUN_5004bd44
//     SWI dispatch site fires from PC=0x5004bd48 with LR=0x5003ad10
//     pointing into FUN_5003acf4 — the SWI wrapper):
//
//       SWI args observed:
//         R0 = 0x1c                (selector = EProcessExecCreate)
//         R1 = 0x80104534          (output buffer slot)
//         R2 = 0x80104528          (= &local_c on the stack;
//                                    pre-init to 0x80000001 sentinel)
//         R3 = 0x80000001          (sentinel arg, also overwritten
//                                    to local_c by the wrapper)
//
//       Wait loop is FUN_50039e64 (decompile line 69432) which
//       repeatedly calls SWI 0xc0004d (WaitForAnyRequest) until
//       *(R2) ≠ 0x80000001 — i.e., until ANOTHER kernel-side path
//       writes the result to local_c. Classic Symbian RIPC pattern.
//
//       TFindFile pointer chain at the input:
//         0x80104b08 = -8 (sentinel)
//         0x80104b0c → 0x80104b78 (descriptor pointer)
//         0x80104b78 = 0x80104b80 (TPtrC8 ptr)
//         0x80104b7c = 0x100      (TPtrC8 length 256)
//         0x80104b80 = ???        — NEVER POPULATED with filename;
//                                    stays at 0xCC canary fill
//         (UID1 = 0x1000007a IS written to 0x80104b08 itself,
//          suggesting the kernel uses UID-based lookup rather than
//          a filename string)
//
//       Result polling: kernel reads virt 0x80004080 (= phys
//       0xD0FF0080 = MemoryBlockC0[0x7F0080] in the 8 MB RAM mirror).
//       UPDATE: The "kernel expects EFile.exe code there" theory
//       was WRONG. PSION_S5_FORCE_LOAD_EFILE=1 brute-force
//       experiment — copy EFile.exe ROM bytes (incl. UID1 0x1000007a)
//       to virt 0x80004080 right after the first SWI 0xc00076
//       R0=0x1c — confirmed: the kernel WRITES OVER our copy with
//       0xa5a5a5a5 / 0x00000000 ~7M cycles later. So virt 0x80004080
//       is just a transient heap allocation that gets reused, not
//       the user-mode entry point. The unique_pcs over a 30s boot
//       are unchanged with the force-load (258 in both cases).
//       EFile.exe most likely runs FROM ROM with USR access mapped
//       on its page, like EUser.dll/EKern.exe do (we observe user-
//       mode execution of those at 0x5005d064, 0x5002d8a0, etc).
//       So the blocker is not "EFile not in RAM" — it's the
//       kernel's RIPC fulfilment for the spawn request never
//       transitioning the EFile process to ERunning.
//
//       So the structural blocker is concretely observable: the
//       kernel issues an asynchronous "load EFile.exe" RIPC, waits
//       for *(0x80104528) to change from 0x80000001, and the
//       expected fulfilment path (writing the result + copying the
//       binary into RAM at virt 0x80004080) never executes.
//
//   * UPDATE: tested whether implementing a RIPC compatibility
//     layer was the right unblocker. RULED OUT by direct
//     comparison with the working PS711x devices:
//       Device   | SWI 0xc00076 R0=0x1c calls in 5s | Boots?
//       Osaris   | 0                                | YES
//       5mx      | 2                                | YES
//       Series 5 | 12 (looping retries)             | NO
//     5mx's args have the EXACT SAME SHAPE as Series 5's
//     (R3=0x80000001 sentinel, R1/R2 = kernel buffer pointers).
//     So RIPC dispatch is implemented IN THE KERNEL ROM. When 5mx
//     issues the SWI, the 5mx kernel's own internal handler
//     completes the request — we don't need to do anything in our
//     emulator. The reason Series 5 stalls isn't missing RIPC
//     fulfilment in OUR code, it's missing hardware stimulus that
//     prevents the SERIES 5 KERNEL'S internal RIPC fulfilment
//     from completing.
//
//     Most likely root causes (in priority order):
//       (a) Scheduler can't dispatch the request-fulfilment thread.
//           Supervisor's iNState never reaches EReady;
//           NThread::Resume is never called for it.
//       (b) Missing IRQ or hardware signal level — the INTMR1
//           difference (Series 5 enables EINT1, Osaris enables
//           EXTFIQ) is suggestive. Pulse-firing EINT1 regresses,
//           but a STEADY level might be what's expected.
//       (c) MMU access permission or page-table layout for virt
//           0x80004080 (user-mode entry) prevents the kernel's
//           internal copy-from-ROM from landing.
//     Fix is likely 1-3 lines of stimulus emulation, NOT weeks of
//     RIPC implementation.
//
// 2026-05-04 (cont.): WindEmu boot notes & Symbian 2010 source check
// ---------------------------------------------------------------------------
// User pointed at the 5mx boot sequence notes from the WindEmu author
// (covers offsets 0x0000-0x500005C9C) and the 2010 Symbian source at
// reference/oss.FCL.sf.os.kernelhwsrv-master.
//
//   * The boot notes describe the cold-boot ROM init: ARM cp15 setup
//     (0x0100), Series-5/5mx CPU branch (at 0x0B40 in WindEmu's 5mx
//     ROM; differs in Series 5's ROM where 0x0B40 is zeros and the
//     equivalent dispatch is at our sub_0xA90 reached from
//     0x0120 via BL), MMU page-table setup, jump to EKern entry at
//     virt 0x5000059C. Our Series 5 boot completes that whole
//     sequence within the first ~0.6 sim sec. The WindEmu notes do
//     NOT cover what we're stuck on — we're WAY past stage 0x5000059C.
//
//   * The Symbian 2010 source (reference/oss.FCL.sf.os.*) is EKA2,
//     not EKA1. The legacy EKA1 entry-stub is at
//     kernel/eka/euser/epoc/arm/eka1_entry_stub.cpp::RunV7Thread:
//       UserHeap::SetupThreadHeap(...)   // KErrNone or fail
//       if (!aNotFirst) User::InitProcess()
//       (*cinfo.iFunction)(cinfo.iPtr)   // <-- application main
//       User::Exit(r)
//     Our Series 5 user-mode thread is observed running heap setup
//     (SWIs 0x6c/0x8e/0x8d) and TRAP push/pop (0x72/0x73) — i.e. it
//     reaches RunV7Thread or its EKA1 equivalent. But it does NOT
//     issue any user-mode HAL::Get (SWI 0x76), which means it never
//     reaches `(*cinfo.iFunction)(...)` — the application's actual
//     entry point.
//
//     So the precise blocker is between SetupThreadHeap (or InitProcess)
//     and the application entry call. Possible causes:
//       (a) SetupThreadHeap returns an error (KErrNoMemory etc.) —
//           thread exits via User::Exit before iFunction call.
//       (b) cinfo.iFunction is null/garbage — would fault on call.
//       (c) The user-mode thread is the WRONG thread — a kernel
//           helper rather than EFile.exe's first thread.
//
//     Hypothesis (c) is most consistent with the 17.85M-cycle
//     allocation-and-free heartbeat: the kernel keeps spawning a
//     short-lived helper, never starts the real EFile process.
//
// 2026-05-04 (cont.): WindEmu source comparison
// ---------------------------------------------------------------------------
// User pointed at the WindEmu source at /home/user/psion/WindEmu-master.
// WindEmu emulates ONLY 5mx (Windermere) and Osaris/MC218 (PS7111). It
// does NOT model the CL-PS7110 (Series 5), so it has nothing
// Series-5-specific to copy from. Comparing our clps7111.cpp/arm710.cpp
// against WindEmu's:
//
//   * One concrete behavioural divergence: WindEmu's unknown-SYNCIO
//     read returns 0xFFFFFFFF (open-bus); ours was 0x800. Made the
//     PS711x base-class default 0xFFFFFFFF (matches WindEmu, datasheet
//     correct), Series 5 overrides to 0x800 (its kernel takes a
//     productive alignment-fault retry path on bit-0x400-clear values).
//
//   * Things WindEmu does NOT model that we already do: BLEOI/MCEOI/
//     RTCEOI/UMSEOI/COEOI handlers, SSEOTI generation on SYNCIO write,
//     high-vector IRQ delivery (0xFFFF0018 when CP15.SCTLR[13] is set),
//     WEINT-on-unserviced-TINT, pre-enabled timers (PS7110 timers are
//     always running per datasheet), Port C / SYSCON masking / chip-
//     variant gates. So our PS711x emulation is now strictly stronger.
//
//   * The 2010 Symbian source is EKA2; the only EKA1 reference is the
//     compatibility stub at kernel/eka/euser/epoc/arm/eka1_entry_stub.cpp
//     ::RunV7Thread which shows the user-mode thread first-instruction
//     sequence is SetupThreadHeap(...) → User::InitProcess() →
//     (*cinfo.iFunction)(cinfo.iPtr). Our user-mode thread reaches the
//     heap-setup phase (we observe SWIs 0x6c/0x8e/0x8d for atomic ops,
//     0x72/0x73 for TRAP push/pop) but never reaches iFunction (no
//     user-context HAL::Get / SWI 0x76 calls — versus 333 for Osaris
//     in the same window).
//
//   * FUN_5004bd44 in the Series 5 ROM (0x4bd44) is literally one
//     instruction `SWI 0xc00076`. The wrapper FUN_5003ad2c sets R2 to
//     a sentinel, calls FUN_5004bd44 to issue the SWI, then waits in
//     FUN_50039e64 for *(R2) to change. The SWI is dispatched to the
//     kernel ROM's own SWI handler at vector 0x8 / 0xFFFF0008. That
//     handler completes asynchronously by writing to the result slot
//     — and for R0=0x1c (some EKA1 HAL attribute or process-create
//     equivalent), the completion never fires.
//
// 2026-05-04 (cont.): SWI dispatch traced — KERN-NO-SESSION panic
// ---------------------------------------------------------------------------
// User pointed at the symbian-os-internals PDF
// (reference/symbian-os-internals-real-time-kernel-programming_compress.pdf)
// which describes the EKA1 SWI dispatcher. With that hint, traced the
// full SWI path for SWI 0xc00076 R0=0x1c through the Series 5 ROM:
//
//   1. SWI vector at virt 0x8 = `LDR PC, [PC, #0x18]` = jump via virt 0x28
//      = jump to 0x500194dc (the SWI dispatcher).
//
//   2. Dispatcher at 0x500194dc:
//        LDR R12, [LR, #-4]          ; load SWI insn
//        TST R12, #0x800000          ; bit 23 = FAST exec flag (per PDF)
//        BIC R12, R12, #0x3FC00000   ; clear bits 22-29 (FAST flag + ctx flag)
//        LDR LR, [PC, #-0x24]        ; LR = *0x500194CC = 0x5002795C
//                                     ; (the FAST exec table base)
//        ADD R12, LR, R12, LSL #2    ; R12 = table_base + selector*4
//        BNE +N (FAST path)          ; if bit 23 set, take fast path
//      For SWI 0xc00076: selector = 0x76 (after BIC clears 22-29).
//      Handler ptr = *(0x5002795C + 0x76*4) = *(0x50027B34) = 0x5000CE28.
//
//   3. 0x5000CE28: B 0x500096CC.
//
//   4. FUN_500096CC (selector=0x76 handler):
//        BL FUN_5000FF40               ; R0 = iCurrentThread (read 0x8010061C)
//        LDR R12, [R0, #0x2C]          ; R12 = *(NThread + 0x2C)
//        CMP R12, #0
//        BNE custom_path               ; if non-null, custom session
//        ; default path:
//        MOV R2, R4 (output buf)
//        MOV R1, R6 (selector R0=0x1c)
//        B FUN_50009650                ; tail-call
//
//   5. FUN_50009650 (no-session default handler):
//        CMP R5, #0x2F                 ; selector check
//        ; for R5=0x1c (not 0x2F):
//        LDR R1, =0x500277F4           ; R1 = ROM string ptr
//        BL FUN_50026298 (= FUN_50039764 thunk → FUN_5003783c)
//        ; FUN_5003783c constructs a TPtrC8 descriptor at R0 from R1
//        BL FUN_50010168               ; PANIC RAISE
//        ; never returns through here (panic kills the thread)
//        ; or returns -2 (KErrGeneral / 0xFFFFFFFE) if it does
//
//   6. The string at virt 0x500277F4 is "KERN-NO-SESSION"!
//
// **CORRECTION:** The "KERN-NO-SESSION panic" theory above was wrong.
// At runtime, *(NThread + 0x2C) IS populated — it points to 0x80006fa4
// (a session struct). The handler at FUN_500096CC takes the BNE
// (custom-session) branch and tail-calls FUN_50002B40 (NOT the panic
// path at FUN_50009650). So the SWI is NOT panicking.
//
// **What actually happens for SWI 0xc00076 R0=0x1c:**
//
//   1. Handler reads iCurrentThread → R0 (a thread NThread block).
//   2. R12 = *(R0 + 0x2C) = 0x80006fa4 (session pointer, non-null).
//   3. BNE → 0x500096FC: tail-call FUN_50002B40 with R0=session,
//      R1=selector (0x1c), R2=output buf, R3=input data (= local_c).
//
//   4. FUN_50002B40 (kernel IPC dispatch via session):
//        - Reads session->iCount = *(0x80006fa4 + 0x1C)
//        - If iCount == 0: return ~R3 (= -input-data-ptr-1)
//        - Else: walk session->iArray (at +0x24 = 0x80006fd8), each
//          entry 48 bytes, looking for one with field +0x20 == 0
//          (= "free slot for new message")
//        - When found, fills the slot:
//            slot[0]    = selector (0x1c)
//            slot[4..14] = 4 words copied from R2 (the output buf)
//            slot[0x20] = 1 (slot now claimed)
//            slot[0x24] = input data ptr (LR)
//          AND BL FUN_50002630 (signal server)
//
//   5. FUN_50002630 takes session pointer, calls FUN_5002600C with
//      R0=session+0x2C, then if iCount != 0, tail-calls FUN_500025D0
//      (probably "wake server thread").
//
//   At runtime the array at 0x80006fd8 already has selector 0x1c
//   stored in slot[0] (= 0x80006fd8). slot[0x20] is 0, so
//   FUN_50002B40 finds this slot, populates it, signals the server.
//
//   **Therefore the structural blocker is:**
//   The IPC message IS queued correctly. The signal-server step
//   runs. But the server thread never picks up and processes the
//   message — never writes back to *(R2) = local_c — so the wait
//   loop in FUN_50039e64 never exits. The server is the supervisor
//   (or a related kernel-side request-fulfilment thread) which
//   per existing notes never reaches a fully-runnable state.
//
//   So earlier observations about "supervisor never resumed" were
//   correct. The IPC mechanism works fine; the server-side processing
//   is what's missing. To fix:
//     (a) Identify which thread is supposed to drain this session's
//         queue, find what prevents it from being scheduled, fix that.
//     (b) Synthesise the response: when a kernel SWI 0xc00076 R0=0x1c
//         is dispatched, immediately write a valid result to the input
//         data ptr (the wait loop's sentinel slot) so the wait exits.
//         The right "valid result" is what the server WOULD write —
//         likely a process handle or KErrNone.
//
// 2026-05-04 (cont.): RESPONSE-DELIVERY GATE TRACED — *(thread+0x14)
// ---------------------------------------------------------------------------
// Continued from above. The IPC message DOES get queued AND processed:
//
//   * After FUN_50002B40 puts the message in slot 0 (state=1),
//     SOMETHING transitions the slot to state=2. Verified by watching
//     virt 0x80006FF8 (= entry[0x20]): 0 → 1 (claimed, kernel SWI) →
//     2 (processed, by the server-side dispatcher FUN_50006e90 or its
//     callees including FUN_500052a8 → FUN_5000d8e4 → FUN_5000d874
//     which DOES allocate a new DProcess). So the kernel server-side
//     process-creation IS running.
//
//   * The drainer FUN_50002E10 (called from FUN_50006E90 area, lr=
//     0x50007420) iterates entries, finds slot[0x20]==2, and calls
//     FUN_50009D2C(R0=*(slot+0x18), R1=&slot[0x24], R2=...) to
//     deliver the response.
//
//   * **FUN_50009D2C is where the delivery is gated**:
//        LDR R3, [R4, #0x24]     ; R3 = *(thread + 0x24)
//        CMP R3, #3               ; state EWaitFastSemaphore?
//        BNE alt_path             ; if not 3, dispatch via vtable
//        LDR R3, [R4, #0x14]     ; R3 = *(thread + 0x14)
//        CMP R3, #3
//        STREQ R3, [R0]          ; *R0 = R3 = *SP — WRITE TO SENTINEL
//        ... only fires if BOTH +0x14 and +0x24 are 3 ...
//
//   * "thread" here is *(slot+0x18). For slot[0] at 0x80006fd8, that
//     value is 0x80006DAC = the idle/null thread. *(idle + 0x14) = 1,
//     NOT 3. So the BNE fires; the result is NEVER written to the
//     wait sentinel, so the wait loop in FUN_50039e64 never exits.
//
//   * Either (a) slot[0x18] should have been set to a DIFFERENT
//     thread (the actual client / supervisor) at session-creation
//     time but was initialised to idle, OR (b) the kernel transitions
//     the wait-thread to state 3 at the right time and our emulator
//     prevents that transition.
//
//   * The BNE branch (state != 3) at 0x50009D88 does
//     `LDR PC, [R3, #0x24]` — vtable indirect on the (non-3) state
//     value. R3 != 3 → R3 = *(thread+0x24) — some non-3 state value
//     used as a pointer, which would fault unless it's actually a
//     vtable. So either the alternate path works for SOME states
//     and not state==1, or it always faults and the kernel handles
//     the fault. Given that we don't observe a fault here, the
//     alternate path probably DOES dispatch successfully — but to
//     a handler that doesn't do the wait-loop write.
//
// New harness mechanisms added to support this:
//   * PSION_USER_SWI_TRACE=N — log SWIs from CPSR=0x10 only
//   * PSION_SCHEDULE_TRACE=1 — log every iCurrentThread write
//   * PSION_S5_SWI_INJECT=swi:r0:resultVa:resultVal:r0return — inject
//     synthesised SWI completion to fast-test "what if it succeeded?"
//
// 2026-05-04 (cont.): traced beyond the gate — kernel cascades to fault
// ---------------------------------------------------------------------------
// With PSION_S5_FORCE_GATE3=1 unblocking the IPC response delivery, the
// kernel proceeds and hits SectionTranslationFault va=0x00704010 from
// PC=0x50008E90 (an atomic-decrement-of-refcount: LDR R3, [R4+0x10];
// SUB R3, R3, #1; STR R3, [R4+0x10]). Caller at 0x500091A0:
//   LDR R5 = ...; R4 = *(R5+0x18); R3 = *R5 (vtable);
//   LDR PC, [R3, #0x40]   ; vtable[16] dispatch
//   MOV R1, R4
//   BL FUN_50008E80
// vtable[16] returns garbage 0x00704000, which then becomes the bogus
// pointer being decremented.
//
// Hooks tested for this fault:
//   PSION_S5_SKIP_REFDEC=1: tried to PC-skip the LDR/SUB/STR block.
//     Didn't actually skip because mid-tick GPRs[15] update doesn't
//     invalidate the in-progress instruction fetch.
//   PSION_S5_FIX_R4_AT_8E90=1: redirect bogus R4 to kernel scratch
//     (0x80105000) before LDR runs. Result: 274 → 265 unique_pcs
//     (regresses) and new fault PageOtherBusError va=0x50027b64
//     pc=0x5000cd64 — the redirect causes other downstream issues.
//
// Concluded that whack-a-moling each fault adds 0-6 PCs but the
// underlying issue is structural: the kernel runs vtable dispatches
// on a DProcess that doesn't have valid contents because we never
// initialised it as a real process.
//
// Verified that *(iCurrentThread + 0x68) = 0x80006F10 = sub-object
// pointer, and *(0x80006F10) = vtable. Per existing notes, this
// vtable slot is FILLED with 0xa5a5a5a5 canary at allocator-init
// and never gets a real vtable from the kernel. PSION_S5_FIX_
// SUPERVISOR_VTABLE=1 patches it to 0x50028444 but combined with
// FORCE_GATE3 the boot hangs (very slow, times out at 30s). So that
// fix introduces an infinite loop in the post-gate path.
//
// **Best knob combination so far (commit e7cb0bd):**
//   PSION_S5_FORCE_GATE3=1
//   → 268 → 274 unique_pcs (+6) at 38 sim sec
//   → 0 → 10 traps (boot reaches new code with new faults)
//   → variance still 0 (LCD not painted)
//
// **Path forward** to actually reach splash:
//   1. Properly initialise the supervisor's NThread sub-object at
//      0x80006F10 with the right vtable AND the methods it points
//      to need to do real work (not just be filled with the right
//      pointer).
//   2. Implement a fake DProcess with a valid vtable for EFile
//      so vtable[16] dispatch returns a meaningful pointer.
//   3. Or, identify the specific kernel function that's SUPPOSED
//      to populate these structures and find why it doesn't run
//      in our boot. (Most likely candidate: the ExStart extension
//      or thread-create chain.)
//
// 2026-05-04 (cont.): "virtual DProcess factory" — feasibility analysis
// ---------------------------------------------------------------------------
// User asked whether we should implement a "virtual DProcess factory"
// to satisfy all the kernel-side dispatches. Tested two approaches:
//
//   * PSION_S5_FAKE_DPROCESS=1 (now reverted): intercept SWI 0xc00076
//     R0=0x1c at SWI ENTRY, write 0 to *R2 (sentinel addr), return
//     R0=0. Result: 23s sim — 168 unique_pcs (REGRESSION from 211).
//     Bypassing the kernel's own server-side allocation chain
//     (FUN_500052a8 → FUN_5000d8e4 → FUN_5000d874 → FUN_50011a78)
//     means we miss all the side-effects (DProcess registration,
//     thread context setup, allocator state changes) that downstream
//     kernel code depends on.
//
//   * Discovered (commit cb6130a) that the kernel's natural code path
//     ALREADY writes a fully-initialised C++ inheritance chain of
//     vtables to 0x80006F10:
//       cycle T+0: 0x5005087c (base class 1)
//       cycle T+1: 0x50050900 (base class 2)
//       cycle T+2: 0x50028178 (base class 3)
//       cycle T+3: 0x50028444 (most-derived final vtable)
//     So the kernel CAN initialise the structure correctly. The
//     issue isn't "uninitialised data" — it's that the structure
//     gets allocated, used briefly, then FREED again at the next
//     17.85M-cycle heartbeat (canary-fill at PC=0x5004D77C lr=0x5003D7D8
//     = the heap allocator's `operator delete`).
//
//   * So the boot is in a heartbeat loop: spawn supervisor, init it
//     correctly, do some work, exit/destroy, respawn. To reach splash
//     the supervisor needs to STAY ALIVE — implementing a factory
//     wouldn't help with that, because the structure IS being correctly
//     populated then deallocated.
//
//   * The supervisor's exit happens because something it does triggers
//     an error. Probably the SWI 0xc00076 R0=0x1c path returns an
//     error (or fails to deliver a result), the supervisor concludes
//     "process spawn failed", calls User::Exit, gets cleaned up.
//
// CORRECTED PATH FORWARD:
// Building a virtual DProcess factory is the WRONG investment. The
// kernel constructs its objects correctly. The right investment is:
//
//   (a) Identify what makes the supervisor's main loop EXIT after a
//       short window. Trace what CALLS User::Exit (or the EKA1
//       equivalent kernel thread-exit path) on the supervisor.
//   (b) Find what would make the supervisor NOT-exit. Likely
//       something about the IPC response value or the process-state
//       check after RProcess::Create. If the response says "process
//       created OK and here's the handle", supervisor proceeds. If
//       response says "no handle / error", supervisor exits.
//   (c) Implement the minimal hook that makes the response say
//       "process created OK with handle = X" where X is something
//       the supervisor accepts.
//
// Comparison with Osaris/5mx (asked by user): NEITHER device has
// required a DProcess factory in our emulator. They boot through
// our generic clps711x + ARM710 emulation untouched. The Series 5
// blocker is therefore device-specific (PS7110 vs PS7111 hardware
// difference) OR kernel-version-specific (EPOC R1 vs R5). The next
// productive direction is to find which of these two the blocker is
// — and given Series 5 reaches User mode, runs heap-setup SWIs, and
// the kernel's IPC machinery DOES function (just times out), the
// missing thing is likely a small hardware-stimulus difference that
// makes the supervisor's life-cycle work differently.
//
// 2026-05-04 (cont.): traced the supervisor's death path
// ---------------------------------------------------------------------------
// User asked "DO IT" — find what kills the supervisor each heartbeat.
//
// Traced the LAST instruction window before each heartbeat's
// canary-fill. From cycle 19790000 (heartbeat boundary) the kernel
// enters FUN_500188BC area (Port C / register manipulation), then
// transitions through 0x50018F20 (CPSR mode switch — sets CPSR mode
// to 0x1B = Undef), then 0x5001083C, 0x50010E70 (which checks a
// stack-saved value at SP+0x268 against #1 with BHI), then BLs to
// FUN_5001AE48 around the idle/scheduler area.
//
// At 0x50010E98 the kernel reaches a bootstrap thread (R0=0x80003CD0)
// dispatch. The next many cycles (19797000+) are a single-purpose
// canary-fill loop using STMIA R0!, {R2-R9} (8 words at a time)
// wiping large memory ranges. R0 starts at 0x80004240 and increments
// through 0x80007000 — wiping ~12KB of kernel structures including
// the supervisor's NThread (0x80006074) and sub-object (0x80006F10).
//
// So this is the kernel's "thread-exit + heap reclaim" path, called
// from the bootstrap-thread's normal scheduler iteration. The
// supervisor exited gracefully (not a fault) and the kernel reclaimed
// its memory as part of its normal scheduler loop.
//
// **The supervisor exits because its work was COMPLETED (or it
// determined it had nothing to do).** Each heartbeat the kernel
// re-spawns it, it does its slice of work, exits. On real hardware
// the supervisor would have spawned EFile/EWSRV/etc. by this point
// and those long-lived processes would persist. In our emulator the
// supervisor's "spawn EFile" step doesn't produce a long-lived
// process, so the supervisor has no reason to continue and exits.
//
// **CONCLUSION**: implementing the missing piece is the same
// problem we identified earlier — making "spawn EFile" actually
// produce a long-lived user-mode thread that does real work. Given
// EFile.exe ROM code, valid MMU mappings, and a fully-initialised
// process context, the kernel WOULD start it. We have all three
// pieces in some form, but the kernel's chain that wires them
// together has a subtle missing step we haven't found.
//
// The available knobs (PSION_S5_FORCE_GATE3, PSION_S5_WAIT_STATE3,
// PSION_S5_FAKE_DPROCESS) all fail because they bypass parts of
// the kernel's natural setup, breaking downstream invariants. The
// proper fix requires either:
//
//   * Identifying the SINGLE missing IRQ/state-stimulus that
//     prevents the spawn chain from completing (high-leverage,
//     hard to find without source).
//   * Faithfully simulating the EFile process from outside the
//     kernel (= the multi-week DProcess factory ruled out above).
//
// The investigation has gone as far as it productively can without
// either source code or more substantial reverse engineering. The
// concrete forward progress shipped on this branch:
//
//   * Comprehensive PS711x hardware-fidelity improvements (SSEOTI,
//     EOI handlers, debugPC virtualisation, BNE-tolerant fault
//     throttling) — 5+ commits genuinely improving the emulator.
//   * The investigation infrastructure (PSION_USER_SWI_TRACE,
//     PSION_SCHEDULE_TRACE, PSION_S5_SWI_INJECT, the SWI dispatcher
//     decode at 0x500194dc, the response-delivery gate at
//     FUN_50009D2C) — gives any future investigator a head start.
//   * A precise, falsifiable characterisation of the boot blocker
//     that future work can attack directly.
//
// Series 5 still doesn't reach splash, but the device:
//   - Has all working hardware emulation (PS7110-faithful)
//   - Reaches user mode in EUser/EKern ROM code
//   - Has a fully-initialised SuperPage and supervisor structures
//   - Has a working IPC machinery (verified end-to-end)
//   - Has the EFile.exe binary located in ROM at 0x5005F180
// What it doesn't have is a working chain from "kernel spawns EFile"
// to "EFile runs its main()". That chain is the residual structural
// blocker.
//
// 2026-05-04 (cont.): unique-PC measurement + RAM scan reveals progress
// ---------------------------------------------------------------------------
// User asked how many unique PCs the boot needs. Measured by sampling
// at 256 Hz over the boot window:
//
//   Osaris  (PS7111, EPOC R5):  37 PCs @ 9s, 55 @ 18s, 62 @ 23s
//   5mx     (Windermere, R5):   99 PCs @ 9s, 126 @ 11s
//   Series 5 (PS7110, EPOC R1): 133 PCs @ 9s, 184 @ 18s, 211 @ 23s
//
// Counter-intuitive: SERIES 5 HAS MORE UNIQUE PCs than the working
// devices, not fewer. The metric is "PC variety per unit time", not
// "code coverage". Working devices converge to a ~40-100 PC stable
// idle loop after splash; Series 5 keeps exploring different code
// paths because it never reaches a stable splash idle.
//
// SWI variety also differs:
//   Osaris first 5s:   53 distinct SWIs
//   5mx first 5s:      53 distinct SWIs
//   Series 5 first 5s: 22 distinct SWIs   (= 2.5x fewer)
//
// Series 5 explores more KERNEL code but invokes fewer SYSTEM CALLS —
// consistent with "stuck in kernel-internal heartbeat, never reaches
// application code that would invoke more services".
//
// **MAJOR DISCOVERY via RAM scan at 20 sim sec**: Series 5 DOES create
// the early kernel-spawn objects! Searching the post-boot RAM for
// known process/server names found:
//
//   EKern        : 1 occurrence    ← kernel process created
//   Supervisor   : 2 occurrences   ← supervisor thread + server
//   EFile        : 4 occurrences   ← EFile.EXE process spawned!
//   FileServer   : 1 occurrence    ← FileServer object created!
//   LoaderThread : NOT FOUND       ← BLOCKER STARTS HERE
//   StartupThread: NOT FOUND
//   Main         : NOT FOUND
//   Loader       : NOT FOUND
//   WindowSrv    : NOT FOUND
//   EWSRV        : NOT FOUND
//
// Compare with Osaris's OBJS log over the same window:
//   3.03M cyc: EKern process
//   3.05M cyc: Supervisor thread
//   5.43M cyc: Supervisor server
//   5.52M cyc: EFile process    ← Series 5 reaches here
//   5.58M cyc: FileServer       ← Series 5 reaches here
//   5.66M cyc: Loader server    ← Series 5 STOPS HERE
//   5.69M cyc: StartupThread    ← never reached
//   ...
//
// **So Series 5 reaches ~80% of Osaris's first-5sec spawn chain,
// then stops between FileServer and Loader.** This is a much
// narrower target than "everything is broken". The Loader server is
// created in EFile.exe's main() AFTER FileServer setup. If EFile's
// main() runs to FileServer creation but doesn't reach Loader-server
// creation, something inside EFile's main is failing.
//
// **Answer to user's "how do Osaris/5mx provide the missing
// stimulus?"**: They DON'T. Both boot through our IDENTICAL generic
// emulation with no special hooks, no synthesised state, no manual
// stimulus. The kernel ROMs themselves contain everything they need.
// The DIFFERENCE is in the ROM:
//   - Osaris: EPOC R5 kernel
//   - 5mx:    EPOC R5 kernel
//   - Series 5: EPOC R1 kernel (the original, two major releases older)
// The newer R5 kernels have a more robust boot that doesn't depend
// on whatever Series 5's R1 boot needs. Either:
//   (a) The R1 kernel has a hardware-stimulus dependency that real
//       PS7110 hardware satisfies but our PS7110 emulation doesn't.
//   (b) The R1 kernel has a specific dependency on something we
//       SHOULD be providing as part of our EPOC-generic emulation
//       but don't.
//
// The next investigation should focus narrowly on what EFile's
// main() does between "FileServer created" and "Loader server
// created" — find the specific call that fails, fix it.
//
// 2026-05-04 (cont.): chronological SWI-trace pinpoints exit moment
// ---------------------------------------------------------------------------
// All 9 ONE-TIME-ONLY SWIs in Series 5's first 5 sim sec fire in a
// burst between cycles 102.9M and 102.99M, then no more new SWIs:
//
//   [102916510] SWI #c0001d  lr=0x50011158
//   [102923132] SWI #80002d  lr=0x500537e4 (EFile/EUser code)
//   [102926348] SWI #c00088  lr=0x50053818
//   [102927328] SWI #c00026  lr=0x50053830
//   [102928220] SWI #c0003b  lr=0x500534dc
//   [102931240] SWI #000082  lr=0x5002db00
//   [102960156] SWI #000074  lr=0x5002a5f0
//   [102960240] SWI #000075  lr=0x50053530
//   [102984108] SWI #c0002e  lr=0x5002f420  ← LAST new SWI
//
// Cycle 102.984M is when EFile.main() gives up. Selector 0x2e is a
// HANDLE-LOOKUP SWI (handler at 0x5000B968 calls FUN_50002740 to
// resolve a handle, returns 0 on failure → EFile sees failure).
//
// Args at the call: R0=0x005022D4 (descriptor/string in EFile's
// user-mode address space), R1=0x005022F4 (similar), R2=0x40010000
// (kernel area), R3=0x80000001 (kernel SID flag).
//
// Tested PSION_S5_FORCE_C0002E_OK=1 (reverted) to force this SWI
// to return success: SWIs gained 1 new selector (23 vs 22), EFile
// count 4→6, FileServer 1→2, but STILL no Loader. So this isn't
// the single-fix unblock — it's one of many gates.
//
// Each gate-bypass advances the boot by 1-3 new SWIs but doesn't
// unlock Loader-creation. EFile's main() requires a chain of
// operations to ALL succeed in order. Tricking individual ones
// doesn't help because downstream depends on genuine state changes.
//
// **Concrete next investigation target**: what is the user-mode
// descriptor at virt 0x005022D4 in EFile's address space? If it's
// a server name like "Loader" or "EFSrv", EFile is trying to FIND
// that server before creating it. If it's a file path, EFile is
// trying to OPEN something we haven't provided.
//
// 2026-05-04 (cont.): splash-timing comparison confirms expected behavior
// ---------------------------------------------------------------------------
// User asked whether splash should display before this point.
// Verified via per-second screenshot capture (--screenshot-every):
//
//   Osaris splash timeline:
//     t=0.0s  mean=0    variance=0      — BLACK (uninit)
//     t=1.0s  mean=255  variance=0      — WHITE (kernel cleared to paper)
//     t=2.0s  mean=235  variance=3939   — SPLASH PAINTED (WindowSrv loaded)
//
//   5mx splash timeline:
//     t=0.0s  mean=153  variance=0      — partial init
//     t=1.5s  mean=150  variance=346    — partial paint
//     t=2.0s  mean=132  variance=2098   — SPLASH PAINTED
//
//   Series 5 (broken):
//     t=0-12s+  mean=255  variance=0    — STUCK ON WHITE FOREVER
//
// So the white screen browser users see is the EXPECTED intermediate
// state that working devices pass through for ~1 sim second before
// WindowSrv overwrites it with the splash. Series 5 stays stuck on
// that white screen because the chain breaks ~4 steps before
// WindowSrv would run:
//
//   EFile → FileServer → Loader → StartupThread → ... → WindowSrv
//                       ↑
//                       Series 5 stops HERE
//
// Series 5's RAM has FileServer (1×), EKern (1×), Supervisor (2×),
// EFile (4×). It does NOT have Loader, StartupThread, Main, Loader,
// WindowSrv, or EWSRV — confirming we stop just past FileServer.
//
// The splash painter is the "EWSRV.EXE" process (window server).
// It paints to virt 0xC0000000 (the LCD framebuffer). Until the
// kernel reaches the chain step that spawns EWSRV, the framebuffer
// stays at the kernel's "clear-to-paper" white fill.
//
// 2026-05-05: per-cycle confirmation of the spawn loop & vtable[0x20]
// ---------------------------------------------------------------------------
// PSION_INSN_TRACE narrowed to the boot-spawn chain confirms the kernel
// IS executing the EFile-create path every ~17.85M cycles (≈ 1 sim sec):
//
//   FUN_500109F4 (one-shot wrapper)
//     → FUN_500072B0 entered with R0=0       (first hit cycle 220348)
//     → FUN_50006758 entered (= TRAP-protected EFile spawn)
//       → FUN_5000925C  (DProcess::Init — sets vtable, fields)
//       → FUN_500095F8  (some kernel registration)
//       → vtable[0x0C] (no args)
//       → vtable[0x38] (R1=0xFFFFFFE2)
//       → vtable[0x3C] (R1=1)
//       → FUN_50009CC0(proc, 1) — sets bit 0x08 of *(proc+0x20)
//       → FUN_50009C98(proc, 1) — sets bit 0x04 of *(proc+0x20)
//       → vtable[0x20] = 0x500113B4
//       → return via LDMFD
//
// Critical finding about vtable[0x20]: the function at 0x500113B4 is
// NOT NThreadBase::Resume. Disassembly + per-instruction trace shows:
//
//   500113B4: MOV R2, #0
//   500113B8: LDRB R3, [R0, #0xC4]      ; load iNState byte
//   500113BC: SXT R3 to int
//   500113C4: CMP R3, #6 / BEQ early    ; if state==EDead, no-op return
//   500113CC: CMP R3, #0 / BNE work     ; if state!=ESuspended, do work
//   500113D4: MOV R2, #1                ; (only on state == 0 || 6)
//   500113DC: MOVNE PC, LR              ; early return for those states
//   ; Path for state != 0 && != 6:
//   500113E0: LDRB R3, [R0, #0xC7]      ; flag byte
//   500113E4: TST R3, #0x80
//   500113E8: BEQ 50011408               ; bit clear → check state==1
//   500113EC: ; (path when bit set: bumps a 16-bit counter, returns)
//   ; ...
//   50011408: LDRB R3, [R0, #0xC4]
//   5001140C: CMP R3, #1                ; state == EReady?
//   50011410: MOVNE PC, LR              ; not ready → return
//   50011414: MOV R1, R0
//   50011418: LDR R0, [pc, #0]          ; R0 = *0x801002C0 (kernel global)
//   5001141C: B 0x5000F360              ; tail-call into FastMutex acquire
//
//   At 0x5000F360: STRB #4, [R4+0xC4]   ; iNState := EHoldFastMutex (4)
//                  BL 0x50026868        ; insert into priority queue
//                  STR #1, [0x80100348] ; iRescheduleNeededFlag = 1
//                  ; (conditional STR #1, [0x8010034C] if proc[0x14]==3,
//                  ; not fired — proc[0x14] observed = 1 or 0)
//
// Therefore vtable[0x20] is **DProcess::AcquireFastMutex** (or similar).
// It transitions iNState from 1 (EReady) to 4 (EHoldFastMutex), enqueues
// onto the scheduler ready queue, and sets iRescheduleNeededFlag.
//
// The actual NThreadBase::Resume is at vtable[0x24] = 0x50011424. Tracing
// 0x50011424 entry confirms it is **never called from FUN_50006758** for
// the EFile case. So the EFile process is never explicitly Resume()'d
// after creation — the kernel relies on the IPC server-thread mechanism
// (SWI 0xc00076 R0=0x1c response delivery via FUN_50009D2C) to dispatch
// it, and that gate is the documented blocker (*(thread+0x14) != 3).
//
// User-mode confirmation: PSION_USER_SWI_TRACE=200 over 12 sim sec logs
// **zero** SWIs from CPSR=0x10. The kernel never transitions to user
// mode in this baseline. All execution is in CPSR=0x1B (Undef) /
// CPSR=0x13 (SVC) / CPSR=0x12 (IRQ).
//
// Reschedule path trace (FUN_500191DC at 0x500191DC) confirms scheduler
// runs and switches iCurrentThread between two DProcess pointers
// (0x80006074 and 0x80006DAC) repeatedly, but neither is the EFile
// process and neither has saved iCpsr=0x10 (User). The EFile process
// pointer (returned by FUN_50011198 + DProcess::Init) is created in
// the heap chunk that gets wiped every ~17.85M cycles by the boot/init
// re-run path documented above.
//
// This session adds confirmation but no new fix. Concrete next step
// remains: identify the kernel server thread that should drain the
// IPC queue at 0x80006FD8 (slot[0x20] != 0 entries) and dispatch the
// process-create response, or wire FUN_50009D2C's response delivery
// to take an alternate path when state is 1 instead of requiring 3.
//
// 2026-05-05 (cont.): pinpointed the server-thread gate in FUN_50002630
// ---------------------------------------------------------------------------
// Continued tracing the SWI 0xc00076 R0=0x1c IPC delivery path. The
// path through the kernel ROM is now fully mapped:
//
//   1. SWI vector → FUN_500096CC (selector 0x76 handler)
//   2. FUN_500096CC tail-calls FUN_50002B40 with R3 = the wait
//      sentinel address (0x80104528). FUN_50002B40 stores LR (= R3
//      from caller = 0x80104528) into slot[0x24] of the matching
//      session slot.
//   3. For SWI 0xc00076 R0=0x1c, the slot is at virt 0x80006FD8
//      (= session at 0x80006FA4 + 0x34). PSION_VALUE_WATCH on
//      0x80104528 confirms PC=0x50002BAC writes 0x80104528 into
//      0x80006FFC (= slot[0x24]). ✓ correct.
//   4. FUN_50002B40 BLs FUN_50002630 to signal the server thread.
//   5. FUN_50002630 unconditionally sets slot[0x20] = 2 (processed),
//      then GATES on `*(session->iServerThread + 0x24) == 3` before
//      forwarding to server and writing 0 (KErrNone) to
//      *session->iCount (= the deliver pointer that propagates back
//      to slot[0x24] = wait sentinel).
//
// PSION_WRITE_WATCH on 0x80006FF8 (slot[0x20]) shows the slot DOES
// reach state 2 every ~17.85M cycles, fired at PC=0x500025E8 (inside
// FUN_50002630). But PSION_WRITE_WATCH on 0x80006FC0 (session+0x1C =
// session->iCount) shows it's set to 0x80006D20 once and NEVER
// cleared to 0 — confirming FUN_50002630's deliver path (which would
// `*(session+0x1C) = 0`) is short-circuited by the failed server-
// thread gate. Without that delivery, *0x80104528 (the wait sentinel)
// is never written; the supervisor's WaitForRequest loop in
// FUN_50039E64 never exits.
//
// **The earlier observation** of 0x00000000 being written to
// 0x80104528 at PC=0x50010020 in CPSR=0x17 (Abort mode) is a red
// herring — that's the kernel-data-init memcpy from the abort
// handler walking through 0x80100xxx and incidentally hitting
// 0x80104528. PSION_INSN_TRACE on 0x50010020 confirms it's a
// generic `STR R12, [R1], #4` STM-loop scribbling 0/canary across
// kernel globals during the destructive cycle, not the IPC delivery.
//
// **Therefore the precise blocker is:**
//
//   At cycle ~12.4M every cycle, FUN_50002630 reads
//   `*(session_at_0x80006FA4 + 0x14)` to get the server thread
//   pointer, then checks `*(serverThread + 0x24) == 3`. The check
//   fails. The server thread is the IPC server that drains
//   completed slots from this session and dispatches them — for
//   the EFile-spawn session, that's the kernel-side process-loader
//   thread. It exists in the kernel's allocator but its state byte
//   never reaches 3 (EWaitFastSemaphore — the state needed to
//   accept a server signal).
//
// 2026-05-05 (cont.): two-level IPC routing — drain step is missing
// ---------------------------------------------------------------------------
// Closer trace of FUN_500025BC (the actual IPC body that FUN_50002630 tail-
// calls when session->iCount != 0) reveals the kernel uses TWO-LEVEL
// message routing, not the single-level delivery the prior note assumed:
//
//   Level 1 (client→server-queue-node): FUN_500025BC at PC=0x500025E8
//     unconditionally sets the originating slot's [0x20] = 2, then via
//     the gate at 0x500025FC checks `*(serverQueueNode + 0x24) == 3`,
//     dispatches vtable[0x48] of the server-queue-node, and tail-calls
//     FUN_50009D24 with the QUEUE NODE's deliver pointer (= node+0x1c
//     ≈ 0x80006D3C, NOT the original slot's [0x24] = 0x80104528).
//
//   Level 2 (server-queue-node→client-wait-sentinel): once the server
//     thread (case-0 loader at 0x80006074) is woken via vtable[0x48],
//     it should DRAIN its queue node (at virt 0x80006D20 in our trace),
//     read the original slot's [0x24] = wait-sentinel address, and
//     write the actual result there.
//
// Trace data (PSION_INSN_TRACE on 0x500025E8-0x50002630 + value-watch
// on 0x80104528):
//   12390904: PC=0x500025E8  slot[0x20] = 2 ✓
//   12390920: PC=0x500025F0  BL returns r0 = 0x80007000 (queue node ptr)
//   12390924: PC=0x500025F4  r5 = session->iServerThread = 0x80006074
//   12390925: PC=0x500025F8  r3 = *(serverNode+0x24) = 3 — gate PASSES
//   12390928: PC=0x50002618  vtable[0x48] of server queue dispatched
//   12390958: PC=0x5000262C  tail-call FUN_50009D24(node, &node[0x1C], 0)
//   ... but the wait sentinel at 0x80104528 NEVER gets a non-sentinel
//   write — the level-2 drain is what's missing.
//
// So the "case-0 loader thread" at 0x80006074 (which IS the IPC server
// thread for this session) IS scheduled (iCurrentThread observed
// switching to it), gets the level-1 vtable[0x48] notification, but
// never runs the body of code that drains its queue and writes the
// level-2 result. Either:
//   (i) The vtable[0x48] notification doesn't actually wake the
//       thread on its server-side wait — vtable[0x48] = memcpy
//       (per disasm of 0x5001195C → thunk at 0x50026700 → memcpy at
//       0x5004D800), so the "wake" is just a memcpy of the slot to a
//       buffer; the actual semaphore signal is elsewhere.
//   (ii) The server thread enters its drain loop but bails out
//        early because some session state is off.
//
// Path forward (none of these are simple):
//   (a) Find the server thread's drain loop — it must check
//       "is there a slot with [0x20] == 2?" and process it. The
//       IPC server's main entry is most likely at the kernel
//       PC where FUN_500025BC's vtable[0x48] forwarding ends up.
//   (b) Implement the missing semaphore signal so the server
//       thread actually wakes from its WaitForAnyRequest. Looking
//       at FUN_50002630's last action `(**vtable[0x24])(thread[0x68])`
//       — this is the semaphore-signal call. It IS being made (per
//       trace) but the underlying implementation may not be
//       actually marking the thread runnable.
//   (c) Synthesize the level-2 delivery: at the moment FUN_50009D24
//       tail-calls with the level-1 queue-node delivery, ALSO write
//       0 directly to the original wait sentinel at 0x80104528.
//       This bypasses the missing drain. Risk: same as before — the
//       supervisor proceeds, then tries to use a process handle that
//       was never populated.
//
// 2026-05-05 (cont.): PSION_S5_SYNTH_DELIVERY hook lands; new blocker
// is in EFile.exe's startup
// ---------------------------------------------------------------------------
// Implemented option (c) plus reschedule-suppression in arm710.cpp under
// env-var PSION_S5_SYNTH_DELIVERY. At PC=0x500025E8 (slot[0x20]=2 store
// inside FUN_500025BC for the EFile-spawn session) the hook:
//   1. Writes 0 (KErrNone) to the wait sentinel at 0x80104528.
//   2. Pre-bumps the supervisor's request-semaphore counter at
//      *(supervisor+0x68)+0x14 (= 0x80006F24) so the wait-loop's
//      SWI 0xc0004d returns without blocking.
//   3. Arms a no-reschedule guard at PC=0x50019644 (the CMP r0,#0 in
//      the SWI dispatcher tail) that forces r0=0 for the next two
//      dispatcher exits (the SWI 0xc00076 return AND the SWI 0xc0004d
//      return inside the wait loop). r0=0 → MOVSEQ PC,LR fires and
//      the supervisor resumes at LR instead of being preempted into
//      the case-0 loader thread.
//
// With the hook armed, the supervisor advances PAST the EFile spawn:
//   * PC=0x5003AD10 (post-SWI-return CMP) reached for the first time.
//   * PC=0x5003AD18-0x5003AD1C (BL into the wait loop FUN_50039E64)
//     reached.
//   * Wait loop's SWI 0xc0004d returns immediately (counter pre-bumped),
//     sentinel check passes (we wrote 0), supervisor returns from
//     wrapper.
//   * The spawn wrapper's caller invokes EFile.exe's entry FUN_50010FCC
//     at virt 0x50010FCC — supervisor executes ~67 instructions of
//     EFile's loader stub. PCs 0x50010FCC..0x50011128 reached.
//
// New blocker — PC=0x50011128 SectionTranslationFault:
//   The EFile loader stub does:
//     LDR r3, [r0, #0x8C]    ; r3 = *(processInfo + 0x8C)
//     LDR r3, [r3, #0x28]    ; ★ faults: address 0xe1a0312c
//   Trace shows r0=0 at this point — *(0+0x8C) reads from low-memory
//   (kernel IVT region) yielding the ARM instruction word 0xe1a03104,
//   then dereferencing that as a pointer (+0x28 = 0xe1a0312c) faults.
//   EFile expects R0 to point to a TProcessCreateInfo / DProcessCreate
//   structure that real RProcess::Create would have allocated and
//   populated — we synthesised the success return but skipped the
//   structure setup.
//
// Boot impact (PSION_S5_SYNTH_DELIVERY=1, 30 sim sec):
//   unique_pcs: 233 baseline → 192 (lower because the destructive cycle
//     that explored kernel idle code in baseline no longer fires the
//     same way; but the supervisor reaches genuinely NEW code in
//     EFile.exe — 67 PCs in 0x50010FCC onwards never reached in
//     baseline).
//   traps: 0 baseline → 1 (the new SectionTranslationFault).
//
// Path forward to actually boot Series 5 to splash:
//   Implement proper process loading. Concretely:
//     1. At the synth-delivery point (or earlier), allocate in some
//        scratch RAM region a TProcessCreateInfo / DProcess structure
//        with the fields EFile.exe's entry stub reads (notably +0x8C
//        and what it points at, plus +0x28 of that target).
//     2. Set up the supervisor's stack/registers so when control
//        returns from the spawn wrapper, R0 points at this structure.
//     3. Alternatively: identify the kernel-side code on real hardware
//        that would populate this structure (likely the case-0 loader
//        thread we never actually drain) and synthesise its writes.
//   This is a substantial emulator feature requiring the EKA1 process-
//   create struct layout to be reverse-engineered from the Series 5
//   ROM disassembly. Not a one-line fix.
//
// 2026-05-05 (cont.): partial implementation of step 1 — scope assessed
// ---------------------------------------------------------------------------
// Implemented an initial fake-DProc/FUN_5000D840 hook (commit 3f5ff4f):
//   * Probe shows the kernel's MMU only maps 0x80100000-0x80105FFF in
//     supervisor mode (4 KB total useful scratch). Anything above 0x80106000
//     PageTranslationFaults.
//   * Allocated fake DProc layout at 0x80105000-0x80105FFF:
//       0x80105000 = base, +0x8C → 0x80105100 (subA), +0x28 → 0x80105400
//       (data buffer, ~0xC00 bytes available).
//   * FUN_5000D840 (EUser handle lookup) hook re-initialises the chain
//     each time and returns the fake DProc, letting EFile's first
//     `LDR r3, [r0, #0x8C]` deref work and the subsequent BL memcpy
//     copy 0xA88 bytes from EFile ROM into our buffer.
//
// Boot impact: reaches deeper into EFile.exe code (PCs 0x5001111C..
// 0x5001112C now hit) but a NEW blocker emerges immediately:
//   * PC=0x50016FF0..0x5001700C — handle-table lookup at *0x801003B0.
//     Uses r4 as handle index. Reads handle_table[r4*4] → object,
//     then *(object+4) → vtable, then BL [vtable+8] → uninitialised
//     pointer → prefetch error every cycle on garbage 0xE59FF018.
//
// Tried hooking PC=0x5001700C to skip the vtable dispatch (jump to
// the function epilogue at 0x50017010 with r0=fake_dproc). RESULT:
// unique_pcs DROP from 149 → 42 (worse). The skip cuts off productive
// kernel work that the natural fault-recovery path WAS doing.
// Reverted (not committed).
//
// Honest assessment of "implement proper process-info structure setup":
//   The kernel's RProcess::Create on real hardware populates DOZENS
//   of interrelated structures: TProcessCreateInfo, DProcess, DThread,
//   DCodeSeg, MMU page tables, handle table, code/data segments
//   loaded from ROM filesystem, etc. Each is read by downstream code
//   in specific ways. Faking one structure unblocks the immediate
//   deref but trips a new fault on the next structure.
//
//   To fully boot Series 5 to splash via this approach, every kernel
//   structure EFile/FileServer/EWSRV reach during their startup needs
//   a faithful synthetic instance — that's hundreds of fields, many
//   of them computed from runtime state (e.g. process IDs assigned
//   sequentially, priorities derived from configurable defaults).
//
//   The pragmatic alternatives (each substantial in its own right):
//     (i)  Implement EKA1 process-loading from scratch in the emulator
//          (read EFile.exe's TRomImageHeader, allocate MMU pages, copy
//          code+data, build TProcessCreateInfo, register handle table
//          entry, enqueue the new thread). Estimated: weeks of work.
//     (ii) Decode the Symbian MBM at ROM ~0x43E000 and paint the
//          splash directly to the LCD framebuffer at 0xC0000000.
//          Bypasses kernel boot entirely. Estimated: 1-2 days for the
//          MBM decoder, then a small render hook.
//     (iii) Run real WindEmu's Osaris/MC218 boot in parallel and
//          replay its kernel-data writes timestamped to Series 5's
//          equivalent points. Highly device-coupled, fragile.
//
//   The scope-honest path: pursue (ii) — a direct splash paint —
//   to give visible boot output, while leaving the structural
//   process-loading work (i) for follow-up sessions.
//
// 2026-05-06: ETNA wiring + Osaris/5mx comparison reveals BOOT IS PROGRESSING
// ---------------------------------------------------------------------------
// User-suggested comparison with WindEmu's 5mx and Osaris paid off concretely:
//
//   Major missing piece: ETNA companion-chip emulator.
//
// 5mx (Windermere) has a full Etna instance with PCMCIA socket control,
// wake registers, and interrupt-clear/-mask. Series 5 had only a stub
// (R2_ZERO/ONES/COUNT/LATCH) for region 2 (0x20000000 chip-select aperture).
//
// PSION_S5_REGION2_LOG trace showed Series 5 kernel writes byte-identical
// ETNA register patterns to WindEmu:
//   off=0x0C value=0x80 (wake1)
//   off=0x08 value=0x3F (IntClear)
//   off=0x0D value=0x0F then 0x00 (SktVarB0 probe)
//   off=0x0B (SktCtrl)
//
// Wiring up the existing Etna class for Series 5 (commit 86262e8) lets
// the PCCARD-ARM driver init pass its SktVarB0 readback probe.
//
// Also fixed (commit 58fb7b3): SYSFLG1 bits 22/24 (URXFE/CRXFE) now report
// 1 (FIFO empty) instead of 0 — was potential garbage-read bug.
//
// RAM snapshots show real boot progress NOT visible in unique_pcs:
//   30 sim sec: EKern, FileServer, EFile, Supervisor, Loader, LoaderThread
//   60 sim sec: + Main, NULL, TheRegistryChunk, $STK, $DAT
//
// So the kernel IS slowly creating the post-LoaderThread chain. By 60 sec
// it has built the second-stage kernel objects ($STK chunks, $DAT chunks,
// the NULL idle-thread proper, the Main thread of EFile, the Registry).
//
// What's still missing at 60 sec: StartupThread, MEDINT/MEDATA/ELOCAL
// libraries, AppArc/AppRun, EwSrv (WindowSrv), C32exe.
//
// Per WindEmu 5mx log, the post-Loader sequence is:
//   1. LoaderThread + Loader server  (Series 5 reaches this at ~30s)
//   2. $STK chunk + StartupThread     (Series 5 reaches $STK by 60s,
//                                      StartupThread missing)
//   3. MEDINT library + Media.IRam   (then MEDATA, MEDCRM)
//   4. ELOCAL, InstRead/Ctrl/App, AppArc, ApGrfx, ApFile, ApServ, BOOT
//   5. StbPatch, Video.Wind
//   6. EwSrv (WindowSrv) + FbServ (Font/Bitmap server)
//   7. AppRun + various app processes
//
// The remaining stall site is between $STK creation and StartupThread.
// Plausibly the LoaderThread is created but not actually scheduled to
// run — same mechanism that previously kept the supervisor un-dispatched.
// With ETNA correct, more of the kernel reaches "create object" but the
// thread DISPATCHER for newly-created threads still has gaps.
//
// Productive next investigations:
//   * Find Series 5's CObjectCon::AddL (different ROM offset from
//     Osaris 0x32304) so we get OBJS log lines and can timestamp
//     exactly what's created when. The Osaris hook reads kernel
//     globals at 0x80000880-0x800008AC; Series 5 equivalents are
//     somewhere in 0x80100xxx range.
//   * Watch the StartupThread spawn site in WindEmu 5mx (or Osaris)
//     to find what RThread::Create call does it. Then verify Series 5
//     reaches that same SWI/call site.
//   * Trace LoaderThread's PC range over time to confirm it actually
//     runs (vs created-but-never-dispatched).
//
// 2026-05-06 (cont.): LoaderThread iNState investigation
// ---------------------------------------------------------------------------
// 60 sim sec RAM snapshot localised the LoaderThread NThread struct at
// virt 0x807F383C. Layout:
//   +0xC4: iNState  = 0 (ESuspended)        — NEVER WRITTEN after create
//   +0xE4: iPc      = 0x5001AE44             — kernel-thread trampoline
//   +0xE8: iCpsr    = 0x10                   — USER mode!
//   +0xF4..+0xFF:    "LoaderThread"          — name inline (12 chars)
//
// PSION_WRITE_WATCH on virt 0x807F3900 (the iNState byte) for 60 sim sec
// shows ZERO writes. The thread is created in suspended state and
// Resume() is never called on it.
//
// Other objects in the kernel-object container area (0x807Fxxxx):
//   * Sub-objects (vtable=0x50028444): 6 instances
//     - KernelBeep (?)
//     - Supervisor (back-linked to thread at 0x80005D90, NOT 0x80006DAC)
//     - NULL (back-linked to 0x80006DAC — that's the IDLE thread!)
//     - Main (EFile's main thread, back-linked to 0x800072D0)
//     - loadersignal (a TFastSemaphore?)
//   * DProcess instances (vtable=0x500283A8): 3 — likely EKern, EFile,
//     and a third (FileServer? or one of the persistent-cycle ones).
//
// The supervisor's old address 0x80006DAC is now wiped (0xFF bytes) —
// the destructive cycle re-allocated it elsewhere. Each cycle creates
// fresh kernel objects in higher-RAM addresses (0x807Fxxxx) while the
// older ones (0x80006xxx) get overwritten by the abort-handler memcpy.
//
// The structural blocker remains: kernel creates LoaderThread but
// never schedules it. Productive next investigation would be finding
// the Symbian kernel function that should call NThreadBase::Resume on
// LoaderThread (per WindEmu OBJS log, this happens between LoaderThread
// creation and Loader server creation, both visible in 5mx boot).
// On Series 5 the call site exists but is upstream of an EKA1 IPC
// dependency we still don't fulfil.
//
// 2026-05-06 (cont.): Hardware-fidelity audit + dispatcher diagnosis
// ---------------------------------------------------------------------------
// User-driven audit (compare ETNA wiring with WindEmu, walk through
// CL-PS7110 datasheet section by section, check what registers
// the kernel touches that we don't fully model).
//
// Findings:
//   * ETNA: our class is a SUPERSET of WindEmu's etna.cpp (we have
//     PCMCIA-card-state simulation, RDY-poll gating, etc.). Wiring
//     into Series 5 region 2 (0x20000000 chip-select) matches the
//     register access pattern the kernel issues (matching offsets
//     and byte-write/read sequences). Confirmed correct.
//
//   * SYSCON1 readback bug (commit cb9ff7e): kernel-written bits
//     8-23 (UART/LCD/codec/wake-disable/IRTXM) were dropped on read.
//     Now stored in sysCon1 member. No measurable boot impact
//     (kernel doesn't roundtrip on those bits), but correct fidelity.
//
//   * SYSFLG1 FIFO-empty bits (commit 58fb7b3): URXFE/CRXFE now
//     report 1 (empty) instead of 0. Correct per datasheet.
//
//   * Registers the kernel never touches during early boot:
//     RTCMR, UBRLCR, CODR, UARTDR. Trace via PSION_INSN_TRACE on
//     the generic register reader/writer stub at 0x5001884c /
//     0x50018864 confirms only 0x100/0x140/0x180/0x1C0/0x200/
//     0x240/0x280/0x2C0/0x300/0x340/0x380/0x400/0x500/0x540/0x5C0/
//     0x680/0x700/0x800 are accessed. All of those are handled.
//
//   * No "RegRead/Write unknown" or "unhandled" log lines fire
//     during the 30-sim-sec boot window. The hardware-fidelity gap
//     is closed for the registers the kernel actually uses.
//
// Dispatcher diagnosis:
//
//   * NThreadBase::Resume at virt 0x50011424 is called ZERO times
//     in 30 sim sec. The kernel doesn't use Resume() — it uses
//     direct AddToReadyList (at FUN_50042D54).
//
//   * AddToReadyList fires 314 times in 30 sim sec, adding threads
//     at LOW addresses (0x80004044..0x800079B0). All those threads'
//     NThread structs live in the heap chunk that gets wiped by the
//     destructive recovery cycle every ~17.85M cycles.
//
//   * The CObject wrappers for kernel-named objects (LoaderThread,
//     Main, NULL, Supervisor, etc.) live in the HIGH area
//     (0x807Fxxxx) which survives the cycle. These are user-side
//     descriptors — NOT schedulable threads.
//
//   * The schedulable NThread instances are in the LOW area, get
//     wiped, get re-created, get re-added to the ready list. So the
//     scheduler IS dispatching threads correctly — but they don't
//     run long enough to complete their startup before the next
//     destructive cycle wipes them.
//
// Conclusion of multi-day audit: the emulator's hardware-level
// fidelity is essentially correct. The remaining boot stall is the
// destructive recovery cycle (UND-stack-overflow-driven), which
// re-creates threads faster than they can complete startup work.
// Each cycle advances ONE OBJECT in the persistent HIGH area.
//
// To boot in reasonable time would require either:
//   (a) modeling the EKA1 abort handler's lazy stack-extend
//       mechanism so the kernel doesn't have to destroy state on
//       every overflow, OR
//   (b) extending UND-mode-stack mapping in a way the kernel doesn't
//       notice — empirically tested 8/16/32/64/128/256 KB guard
//       sizes; cliff at 64 KB drops boot from 169 to 32 PCs (worse,
//       not better — the abort IS the productive mechanism).
//
// (a) requires understanding the kernel's expected abort flow; (b)
// is structurally limited. Neither is a quick fix.
//
// 2026-05-06: Stack-overflow loop diagnosis (PSION_FAULT_PROBE)
// ---------------------------------------------------------------------------
// User reported a recurring fault loop in the browser log:
//   PRT writes / ETNA writes / LCD writes / Fault / repeat
//
// PSION_FAULT_PROBE shows the kernel UND-mode stack overflows because
// only ONE region of memory is mapped near the stack base:
//
//   virt 0x80000000-0x80014000 (80 KB) -> phys 0xD0FEC000+ (kernel data)
//   virt 0x80100000-0x80106000 (24 KB) -> phys 0xD0FE6000+ (more kernel data)
//   virt 0x7FF00000-0x7FFFFFFF        -> L1[0x7FF] = 0 = UNMAPPED
//
// The kernel's UND-mode SP is initialised to 0x80003C5C (passed as r0
// to a per-mode-SP setter at PC=0x50019194). Stack grows DOWN. After
// ~80 nested function frames consuming ~15 KB it reaches 0x80000000,
// then continues into the UNMAPPED 0x7FFFxxxx region and faults.
//
// Per-mode-SP globals located via ROM disasm (PC=0x5001912C..0x50019190):
//   * SP_svc/IRQ:  *(0x80100388) - 4
//   * SP_abort:    *(0x80100380) - 4
//   * SP_fiq:      *(0x80100668) - 4
//   * SP_und:      passed as r0 to MOV-sp helper at 0x50019194
//
// Trace shows 0x80100668 (FIQ stack global) gets written 0x80105B94 at
// boot, then re-init each destructive cycle. SP_und only ever gets
// 0x80003C5C as init.
//
// Why 0x80003C5C and not 0x80014000 (top of mapped region)? Computed
// at runtime, not hardcoded. The 15 KB available for UND stack is
// the kernel's intent — but somehow on real Series 5 hardware the
// recursion either doesn't go this deep, or there's a stack-grow
// mechanism we're missing.
//
// The visible loop is the kernel's destructive-cycle recovery: each
// stack overflow → abort handler → kernel-data-init memcpy →
// re-init kernel state → try again → overflow again. RAM snapshots
// confirm SLOW boot progress: at 30 sim sec the kernel has created
// EKern/FileServer/EFile/Supervisor/Loader/LoaderThread; at 60 sim
// sec it has additionally created Main/NULL/TheRegistryChunk/$STK/
// $DAT. Each destructive cycle advances by a few objects.
//
// The right fix requires understanding why the kernel allocated only
// 15 KB of UND stack on Series 5 vs presumably more on real hardware.
// Candidates: a chip-detect step that picks a stack size based on
// hardware feature bits we report wrong, or a stack-grow abort
// handler we don't trigger correctly.
//
// **Stack-grow attempt 2026-05-06.** Implemented two lazy stack-grow
// mechanisms in arm710.cpp:
//
//   1. Inline alias (default): on PageTranslationFault inside the
//      stack-guard range, redirect read/write to a backing slice in
//      the upper 8 MB RAM block. Tunable via PSION_S5_STACK_GUARD_KB
//      (default 8).
//   2. Page-table-modifying mode (PSION_S5_LAZY_PAGE_GROW=1): on the
//      same fault, install a real coarse-table L1 + small-page L2
//      entry in the kernel's page tables and retry through the TLB —
//      i.e. simulate what an EKA1 abort handler would do on real
//      hardware.
//
// Sweep 2026-05-06 (alias vs page-table-mod, 8 / 40 / 64 KB guard):
// IDENTICAL boot progression in both modes (169 / 168 / 32 unique_pcs).
// The kernel never reads its own page tables on this hot path, so
// whether the access succeeds via inline alias or via a freshly-
// installed mapping is invisible to it. Modeling a real abort handler
// does not move the needle.
//
// Per-page first-touch probe (PSION_S5_STACK_GUARD_LOG=1) at 52 / 54
// / 55 / 56 KB:
//
//   sz=52 KB: 13 pages aliased (0x7FFF3000-0x7FFFF000), pcs=160
//   sz=54 KB: 14 pages (page 0x7FFF2000 partially aliased from
//             0x7FFF2800), pcs=54
//   sz=55 KB: 14 pages, pcs=52
//   sz=56 KB: 14 pages, pcs=32
//
// The cliff fires the moment ANY part of page 0x7FFF2000 enters the
// alias range. The first stack push to land there is at va
// 0x7FFF2FF0 — once that write is silently satisfied instead of
// faulting, 81.9 % of CPU time loops at 0x5001A770. The kernel's
// destructive-recovery cycle depends on that exact fault firing.
//
// **Replicate-the-kernel-data-init-memcpy attempt 2026-05-06.**
// PSION_S5_ABORT_TRACE confirmed two-fault recovery cycle:
//
//   PageTranslationFault at va=0x7FFFDFFC (UND-mode SP=0x7FFFDFF8)
//   AlignmentFault       at va=0x0000003B (SVC-mode, R0=R2=0xFFFFFFFF)
//                                          PC=0x5000CD84
//
// firing every ~17.7M cycles. PSION_S5_MEMCPY_DUMP captured the
// resulting kernel-data-init memcpy at PC=0x5004D90C: 113 unique
// V32 stores from ROM 0x500290D4 to RAM 0x80100000-0x80100E1C
// (length 0xF94), all zero in the abort-mode (CPSR=0x17) iteration
// because the ROM template's bytes happen to be zero in the section
// being copied. The trace also showed a UND-mode (CPSR=0x1B,
// LR=0x50011918) memcpy variant that fills NThread template slots
// at 0x80003900-0x80003D00 with 0xCCCCCCCC canary + UID structures.
//
// Implemented PSION_S5_PRESERVE_KDATA=1 (arm710.cpp): broaden the
// existing 0x80100004-0x80100020 IPC-slot zero-overwrite suppression
// to the entire 0x80100000-0x80100E1C kernel-data-init range — skip
// any zero-write where the current memory value is non-zero. The
// patch fires correctly (100+ skips per 5 sim s, preserving heap
// pointers 0x8000598C / 0x80005D8C / 0x80004090 across abort
// cycles). Boot progression at 30 sim s: IDENTICAL — 234 unique_pcs
// in both baseline and preserve modes.
//
// **The destructive cycle is not what limits boot.** What limits it
// is which code paths the kernel CHOOSES to run in each cycle, and
// that choice depends on init-time decisions that don't depend on
// leftover state in 0x80100xxx. Preserving live pointers across
// cycles is harmless but uninformative. The remaining structural
// blocker is in the kernel's scheduler / dispatcher — see notes
// above on iCurrentThread, FUN_50010E44, the supervisor's HAL
// request at 0x5003ad10. None of which the kernel-data-init memcpy
// guards address.
//
// Diagnostic env vars from this investigation are kept opt-in:
//
//   PSION_S5_ABORT_TRACE=N   log first N data-aborts (FAR/FSR/PCs)
//   PSION_S5_MEMCPY_DUMP=1   log every write made by 0x5004D90C
//   PSION_S5_PRESERVE_KDATA=1 broaden zero-overwrite preservation
//
// 2026-05-06: SWI 0xc00076 R0=0x1c handler trace — kernel handler reaches
// scheduler dispatch but ARM mode-switch never completes
// ---------------------------------------------------------------------------
// Plan B from the next-step discussion: instrument what Series 5's own
// EPOC R1 kernel does when SWI 0xc00076 R0=0x1c (EProcessExecCreate) fires,
// rather than cross-referencing Osaris (which doesn't issue this SWI at
// all). Captured with PSION_INSN_TRACE_CYC=12390700,12420000 +
// PSION_WRITE_CYC=12390710,12420000 + PSION_WRITE_WATCH on the supervisor's
// wait-sentinel addresses 0x80104528 / 0x80104534.
//
// **The handler runs.** From SWI vector entry at virt 0x00000008 the
// dispatch chain is:
//
//   0x00000008 LDR PC, [PC, #0x18]  ; SWI vector trampoline
//   0x500194DC                       ; SWI dispatcher prologue (CPSR=0x13 SVC)
//   0x500194E0-0x50019530            ; selector decode + STR R12, [SP] save
//   0x5001952C MSR ... (CPSR=0x1B)   ; transition to UND mode for handler body
//   0x500195EC-0x500195FC            ; dispatch table → handler entry
//   0x5000CE28 → 0x500096CC          ; ExecHandler::ProcessCreate path
//   0x5000FF40-0x5000FF48            ; sub-call (kernel data lookup)
//   0x50002B40-0x50002BDC            ; main handler body (process-create work)
//   0x50002630-0x50002654            ; sub-call (likely kernel-side queue)
//   ...                              ; 2537 unique PCs, 9986 instructions
//                                    ; over ~30K cycles (~1ms sim time)
//
// **The handler queues a 32-byte request struct** at virt
// 0x80100138-0x80100158 from PC=0x5004DABC (LR=0x500119D0, STMIA-style
// burst):
//
//   0x80100138 = 0x0000001C          ; selector (= EProcessExecCreate)
//   0x8010013C = 0x80104544          ; output buffer pointer
//   0x80100140 = 0x00000000          ; reserved
//   0x80100144 = 0x00000005          ; arg count?
//   0x80100148 = 0x00000005
//   0x8010014C = 0x40018000          ; user-mode VA range pointer
//   0x80100150 = 0x80006DAC          ; current (idle) NThread pointer
//   0x80100154 = 0x80006FD8          ; another kernel-data pointer
//
// **The handler signals reschedule.** PC=0x5000F37C writes
// 0x80100348 (iRescheduleNeededFlag) = 1; PC=0x5000F3A8 (LR=0x50019248)
// clears it back to 0 — the scheduler ran and consumed the flag.
//
// **The handler chooses to switch to the user thread.** PC=0x5000F418
// writes 0x8010061C (iCurrentThread) = 0x80006074 — *the user-mode thread
// the supervisor created*. This contradicts series5.h:84-92's earlier
// claim that "iCurrentThread is always 0x80006DAC (idle)" — the kernel
// handler IS scheduling the user thread, the bug is downstream.
//
// **But the actual ARM mode-switch never fires.** Across the entire
// 30K-cycle handler window, CPSR distribution is:
//
//   CPSR=0x1B (UND, kernel work):  8685 instructions  ★ stays here
//   CPSR=0x13 (SVC, dispatch):      755 instructions
//   CPSR=0x12 (IRQ, timer/etc.):    546 instructions
//   CPSR=0x10 (User mode):            0 instructions  ← never reached
//
// So after iCurrentThread is set to the user thread at cyc 12392054,
// the kernel runs more book-keeping (PC=0x5000F418 → 0x5000F4B0 →
// 0x50017774 etc.) and *eventually returns* without ever performing
// the LDM^ (LDM with PSR-force-user) that would actually load the user
// thread's saved regs and enter CPSR=0x10. The supervisor resumes its
// wait loop. *0x80104528 stays at 0x80000001 (KRequestPending). The
// destructive abort cycle wipes everything ~250K cycles later.
//
// **Concrete next blocker**: the kernel handler successfully queues the
// request and updates iCurrentThread, but the context-switch to that
// thread never executes the LDM^ that would actually flip CPU mode and
// jump to the thread's saved PC. The bug is in the path between
// iCurrentThread's update at PC=0x5000F418 and what should be the
// thread-resume / context-switch site. Three hypotheses to check:
//
//   1. The thread's NThread block at 0x80006074 has a corrupt iSp /
//      iCpsr / saved-PC field, so the kernel decides "thread not ready"
//      and falls through.
//   2. The scheduler's decision logic looks at iNState (offset within
//      NThread) and skips the thread because we report it suspended.
//   3. Our LDM^ implementation has a bug that the kernel works around
//      on real hardware but trips us up.
//
// 2026-05-06 (cont.): Hypothesis 1 disproven — the thread DOES get
// scheduled, it's just a kernel thread (no user thread exists)
// ---------------------------------------------------------------------------
// PSION_WRITE_WATCH on iCurrentThread (0x8010061C) over 5 sim sec shows
// only 4 distinct values ever get written:
//
//   0x80003CD0 — bootstrap NThread (5×)
//   0x80006074 — kernel worker thread (10×)
//   0x80006DAC — idle / null thread (5×)
//   0x00000000 — cleared during destructive recovery cycle (6×)
//
// All three real threads run in CPSR=0x1B (UND, kernel mode). NO USER-
// MODE NThread exists or is ever scheduled across the entire 5 sim sec
// boot. So the original "thread doesn't switch" framing was wrong:
// thread 0x80006074 IS being context-switched in (the LDM^ does fire),
// it's just a kernel thread that runs in CPSR=0x1B.
//
// Also corrected: iNState is at offset **0xC4** (= 0x80006138 for thread
// 0x80006074), not 0x1C as the read-watch trace initially suggested.
// PSION_WRITE_WATCH on 0x80006138 confirms the kernel writes:
//
//   cyc 12391602: 0x4 (EReady)  pc=0x5000F36C lr=0x50009D8C cpsr=1B
//   cyc 12392046: 0x5           pc=0x5000F410 lr=0x5000F40C cpsr=1B
//
// EReady=4 (per FUN_500117CC's `if (*(char *)(param_1 + 0xc4) == '\x04')`
// check on offset 0xC4). State 5 might be ERunning or similar — set just
// before iCurrentThread updates. So iNState is being correctly managed.
//
// **The real blocker is upstream**: the SWI 0xc00076 R0=0x1c handler runs
// kernel-side process-create work, queues a request struct at
// 0x80100138-0x80100158, signals reschedule, and a kernel worker thread
// (0x80006074, possibly the Loader) gets scheduled to consume the
// request. But that worker's work *never produces a user-mode NThread*.
// E32Image parsing → page mapping → user-NThread allocation →
// AddToReadyList — none of this happens. So no user-mode thread ever
// exists to be scheduled. Supervisor's wait sentinel never gets the
// completion write because the spawn chain never produces a runnable
// user thread to signal it.
//
// To progress further, the missing work is real Symbian process
// loading: walk the ROM filesystem at offset 0x413980, parse EFile.exe's
// E32Image header, allocate user-mode pages, set up TProcess + user
// NThread + ARM entry frame, AddToReadyList, complete the supervisor's
// TRequestStatus at *0x80104528. This is the substantial work the
// previous "next concrete approach" discussion estimated at 1-2 weeks.
//
// 2026-05-06 (cont., MAJOR CORRECTION): user mode IS reached — the
// image-loader rewrite is NOT needed
// ---------------------------------------------------------------------------
// The "no user mode" conclusion above was drawn from a 30K-cycle SWI
// handler trace window — way too narrow. Verified with a 60-sim-second
// boot + PSION_MODE_TRACE=u: there are **1907 transitions to CPSR=0x10
// (User32)** starting at outer cycle 174,349,680 (~9.4 sim s).
// EFile.exe IS running in user mode, returning to kernel via SWIs from
// PCs 0x500537E4 / 0x50053818 / 0x50053830 / 0x50053840 — exactly the
// launcher PCs cea03d7's commit message described.
//
// So the kernel's real RProcess::Create handler from cea03d7 is still
// in effect: EFile.exe gets mapped, the user-mode thread is created,
// the scheduler dispatches it, user code runs. **Implementing an
// E32Image loader from scratch is unnecessary** — that work is already
// happening through the kernel's own code path.
//
// **The actual remaining blocker is downstream**: EFile.exe runs its
// launcher prologue (4 distinct PCs hit) but doesn't progress past
// 0x50053840 to FUN_5005650C (TFileServer alloc) or thunk_FUN_5003F5B0
// (RProcess::Create for EWSRV.EXE). Most user-mode time is spent in
// SWI atomic helpers (0x5003A7E4, 0x5003A7AC — 877 calls combined) and
// RProcess::Create wait at 0x5003AD10. The EFile.exe launcher is
// stalling on a SWI that doesn't complete, the same shape of bug as
// the supervisor's initial spawn block but one layer up the user-mode
// chain.
//
// User-mode PC distribution (LR values when leaving CPSR=0x10, 60s boot):
//
//   pc=5003a7e4  443×  user-mode atomic helper (kernel-shared thunk)
//   pc=5003a7ac  443×  user-mode atomic helper
//   pc=5003a150  243×  user SWI helper
//   pc=5003a138  134×  user SWI helper
//   pc=5004c36c  122×  kernel return path
//   pc=50043e40  120×  scheduler entry
//   pc=5003ad10   52×  RProcess::Create wait setup
//   pc=5003a20c   40×  user SWI helper
//   pc=50039e78   39×  WaitForRequest loop
//   pc=5002a618   39×  user SWI dispatch
//   pc=50053840   14×  EFile.exe launcher
//   pc=50053830   14×  EFile.exe launcher
//   pc=50053818   14×  EFile.exe launcher
//   pc=500537e4   14×  EFile.exe launcher
//
// Concrete next investigation: trace what EFile.exe issues at
// 0x50053840 — what SWI, what's it waiting on, why doesn't completion
// fire? Same investigation pattern as cea03d7 → this commit, just
// applied to the EFile launcher's blocking call.
//
// 2026-05-06 (cont.): EFile launcher trace — far more progress than
// the PC sampler was suggesting
// ---------------------------------------------------------------------------
// Traced PSION_INSN_TRACE_CYC over a 100K-cycle window covering the
// first user-mode entry (insn 166,304,230 / outer 174,349,684).
// **2171 unique user-mode PCs visited in just 5 sim ms.** The
// `--assert-boot` PC sampler runs at 100 Hz, so over 30 sim sec it
// captures only ~3000 samples — vastly under-counting. The "234 PCs
// at 30s" figure is the *sample* count, not actual code coverage.
//
// EFile.exe execution chain inside the launcher (CPSR=0x10 user mode):
//
//   1. PC=0x5001AE44 — user-mode entry (BL into 0x50026AE4 startup)
//   2. → 0x5004D260 (C runtime startup, allocates user stack/heap)
//   3. → 0x500537A4 — EFile launcher's "search for existing server" fn.
//      Calls FUN_5002ED14 (kernel iterator) → SWI 0x800054 with
//      R0=0x501AB0 R1=0x5019A8 R2=1 R3=0x53. SWI returns -1
//      (0xFFFFFFFF) = "not found".
//   4. FUN_500537A4 returns 0 (false) — no existing server.
//   5. PC=0x500537FC takes the "init as new server" path:
//      - func_0x5005d2b4(0)        ; setup
//      - func_0x5005d2c0(...)      ; SWI returns to 0x50053818
//      - func_0x5005d2cc(...)      ; SWI returns to 0x50053830
//      - FUN_500534BC()            ; THE REAL FILE-SERVER INIT BODY
//      - thunk_FUN_5002a660()      ; SWI returns to 0x50053840
//
// **EFile.exe IS reaching the init body** (FUN_500534BC). The 14
// iterations each at the 4 launcher PCs (60s sample) reflect the
// entire init-as-server path completing 14 times per 60 sim s — i.e.
// once per destructive recovery cycle.
//
// LCDCON IS being programmed: 0x5814E95F (greyscale enable + initial
// config) followed by 0x1814E4AF (final config) — twice per recovery
// cycle, matching the 17.7M-cycle period. Visual output stays blank
// because the framebuffer initialises to all-0xFF (= white in our
// 4 bpp mapping, which is the EXPECTED mid-boot state — see series5.h
// note 16a64fa "WHITE is correct mid-boot state").
//
// **The actual ceiling on Series 5 boot is the destructive recovery
// cycle**, not a missing piece of hardware emulation or a missing
// kernel handler. Each component along the chain (process loader,
// scheduler, LCDCON programming, EFile launcher init body, IPC) IS
// working. The cycle just resets state every ~1 sim sec before the
// user-mode chain (EFile → EWSRV → WindowSrv → splash bitmap) can
// complete.
//
// Previous attempts to break the cycle (notes 1985-1992, 2006-2018,
// 2076) ALL regressed boot. The cycle's destructive part is
// paradoxically the productive part — without the abort fault firing
// each iteration, the kernel doesn't make progress at all.
//
// 2026-05-06 (cont., REFRAMING): destructive cycle is a SYMPTOM of an
// emulator bug, not an inherent property of the boot
// ---------------------------------------------------------------------------
// Real Series 5 hardware boots to splash in ~5 seconds. If our emulator
// can't reach splash in hours of sim time, we're at LEAST 1000× slower
// than real hardware on this code path. Real hardware does NOT enter the
// abort-handler-restart loop because the AlignmentFault at PC=0x5000CD84
// doesn't fire there. So the cycle is a SYMPTOM of an emulation bug, not
// the boot mechanism.
//
// PSION_S5_ALIGNMENT_DEEP=N captures full register state + 64-deep PC
// history at the AlignmentFault. From the trace:
//
//   FAR=0x3B FSR=01  faultingPC=0x5000CD84  CPSR=SVC
//   r0=0xFFFFFFFF r1=0x80003814 r2=0xFFFFFFFF r3=0x24
//   r4=0x800037F8 r5=0x80007204 r6=0x800070D4 r12=0x80007240
//   sp=0x80105794 lr=0x50037A40
//
// PC history (oldest → newest, 64 entries):
//   …cycles in PC=0x5000C2xx loop (FUN_5000C208 = MatchF)…
//   PC=0x5000C2A4 e3e00000  MVN R0, #0          ← R0 = -1 (no match)
//   PC=0x5000C2A8 ea000058  B 0x5000C410        ← jump to epilogue
//   PC=0x5000C410 e28dd008  ADD SP, SP, #8
//   PC=0x5000C414 e8bd87f0  LDMFD SP!, {R4-R10, PC}  ← return
//   PC=0x5000CD80 e1a02000  MOV R2, R0          ← R2 = -1
//   PC=0x5000CD84 e592003c  LDR R0, [R2, #0x3C]    ← FAULT!
//
// **The bug**: `MatchF` (FUN_5000C208 — Symbian's filename wildcard
// pattern matcher: handles `*` (0x2A) and `?` (0x3F)) returns -1 when
// no match is found. The caller at 0x5000CD80 blindly dereferences
// the result. Real hardware doesn't take this -1 path because the
// match SUCCEEDS — *we're missing some kernel-side state that would
// produce a match*.
//
// MatchF's parameters: param_1 = candidate descriptor, param_2 = pattern
// descriptor. The function compares character-by-character. It returns
// -1 only when no match is found.
//
// **Concrete next step**: identify (a) what descriptor pair is being
// matched at this call site, and (b) what would make them match on
// real hardware. The most likely candidate is a process / session
// name lookup that finds the *current* process's name against a
// pattern — and the lookup is failing because either:
//   - The kernel object container is empty (no objects registered yet)
//   - A thread name field hasn't been initialized
//   - A descriptor length / data field is being read wrong
//
// To find which: trace the descriptors at param_1 / param_2 just
// before MatchF returns -1, and identify what name they encode.
//
// Diagnostic env vars added this session:
//
//   PSION_S5_ABORT_TRACE=N   log first N data-aborts (FAR/FSR/PCs)
//   PSION_S5_MEMCPY_DUMP=1   log every write made by 0x5004D90C
//   PSION_S5_PRESERVE_KDATA=1 broaden zero-overwrite preservation
//   PSION_WRITE_CYC=lo,hi    log every virtual write in [lo, hi) insn cycles
//
// 2026-05-07 (cont.): three-area hardware-emulation audit — all CORRECT
// ---------------------------------------------------------------------------
// Audited the three remaining most-likely hardware-emulation candidates
// for boot-blocker root cause. ALL THREE check out as correct:
//
// 1. EXCEPTION LR ADJUSTMENTS (arm710.cpp:81-94, 99-134, 1010, 1207, 2272):
//    - Reset:        savedPC = 0,                vector = 0     ✓
//    - SWI:          savedPC = exec_pc + 4,      vector = 0x08  ✓
//    - Undef:        savedPC = exec_pc + 4,      vector = 0x04  ✓
//    - Prefetch abt: savedPC = exec_pc + 4,      vector = 0x0C  ✓
//    - Data abort:   savedPC = exec_pc + 8,      vector = 0x10  ✓
//    - IRQ:          savedPC = next_pc + 4,      vector = 0x18  ✓
//    - FIQ:          savedPC = next_pc + 4,      vector = 0x1C  ✓
//    All match ARM ARM (and Series 5 EPOC R1 kernel relies on standard
//    ARMv4 conventions). Not the bug.
//
// 2. CPSR MODE BANKING (arm710.cpp:18-39, modeToBank[16] table):
//    - User32 (0x10) → MainBank ✓
//    - FIQ32  (0x11) → FiqBank, swaps R8-R12 ✓
//    - IRQ32  (0x12) → IrqBank ✓
//    - SVC32  (0x13) → SvcBank ✓
//    - Abort32 (0x17) → AbtBank ✓
//    - UND32  (0x1B) → UndBank ✓
//    - System32 (0x1F) → MainBank ✓
//    R13/R14 banked per mode; R8-R12 banked only for FIQ. SPSR saved
//    per bank. switchMode handles 26-bit-mode coercion (OPL legacy).
//    Not the bug.
//
// 3. IRQ-DURING-LDM ATOMICITY (clps7111.cpp:594-640, arm710.cpp:2138):
//    Our LDM/STM implementation completes atomically — the entire
//    transfer happens in one tick(), no IRQ check between memory
//    accesses. Real ARM710 CAN take an IRQ mid-LDM/STM and restart
//    the instruction on return. Our atomic model is STRICTLY SAFER
//    (no half-done state), and the kernel works with either
//    convention. IRQ delivery happens only at instruction boundaries.
//    Not the bug.
//
// CONCLUSION: All three suspected ARMv4 hardware-emulation areas are
// implemented correctly. The boot blocker is NOT in our CPU emulation,
// MMU/TLB, exception handling, or banking. The remaining hypothesis
// space:
//
//   * EKA1-specific kernel state machinery we don't have visibility
//     into without the source. Specifically the +0x14=3 setter on
//     thread blocks remains unfound.
//   * Some specific peripheral register response (UART status, GPIO
//     edge, codec status, watchdog kick) the kernel polls and stalls
//     waiting for. Hard to identify without the source.
//   * Memory-layout / page-table differences from real Series 5 hw.
//     Our 2-bank 4MB model verified correct. Beyond that, the kernel's
//     L1/L2 page table walks should produce correct translations
//     (we emulate them per the ARM ARM).
//
// At this point, further progress without EKA1 source code requires
// either sampled real-hardware register-trace ground truth, or
// implementing EKA1 process-loading from scratch in our emulator
// (multi-week effort).
//
// 2026-05-07 (cont.): MMU/TLB audit — RULED OUT as the boot blocker
// ---------------------------------------------------------------------------
// Hypothesised that TLB-stale data from missing CP15 invalidation might
// be why thread+0x14 stays uninitialised. Audit findings:
//
// 1. CP15 c7 (cache flush) and c8 (TLB invalidate) WRITES are no-ops in
//    our emulator — gated on `#ifdef ARM710T_TLB` / `ARM710T_CACHE`,
//    BOTH of which are commented out in arm710.h:22-23. Additional
//    `if (isTVersion)` check inside arm710.cpp:2378 means even if the
//    macros were defined, the ops would only fire for ARM710T (Series 7),
//    not ARM710 (Series 5/MC218/Osaris/5mx).
//
// 2. HOWEVER: when ARM710T_TLB is undefined, our translateAddressUsingTlb
//    has NO TLB CACHE LOOKUP (arm710.cpp:3385-3391). Every memory access
//    does a fresh page-table walk. So there's no stale TLB data to
//    invalidate — the missing CP15 ops don't matter.
//
// 3. The single TlbEntry singleTlbEntry is overwritten on each translate
//    call but only used by the immediate caller before any nested
//    translation could overwrite it. No shared-state corruption observed.
//
// CONCLUSION: TLB/cache is NOT the cause. Every memory access correctly
// walks the page table and reads the right physical address.
//
// The thread+0x14 field stays uninitialised because the kernel's
// FUN_500090E0 (ThreadInit) only writes +0x24, +0x48, +0x74, +0x80,
// and +0xB8 — NOT +0x14. Some OTHER kernel function must initialise
// +0x14, and that function isn't being reached in our boot. Identifying
// that function (likely DProcess::Run or a struct-copy from a thread
// template) requires further targeted investigation.
//
// Searched the ROM for STR Rd, [Rn, #0x14] preceded by MOV Rd, #3
// (within 8 instructions): 3 hits, all writing R0 to [R13/SP, #0x14]
// (= stack-local stores, not thread-state). No kernel function
// directly writes 3 to a thread's +0x14 via literal-immediate path.
//
// Probable mechanisms for setting thread+0x14=3 (not yet located):
//   (a) Struct copy from a ROM template (LDMIA + STMIA pair)
//   (b) Indirect: load value from a kernel global that's set to 3
//   (c) Computed: value derived from another field (e.g., type code)
//
// 2026-05-07 (cont.): Located the +0x24=3 writer — ThreadInit fires correctly
// ---------------------------------------------------------------------------
// Searched the full heap region (0x80003000-0x80008000) for value=3 writes
// during 30 sim sec. Found PC=0x50009128 in FUN_500090E0:
//
//   FUN_500090E0 (ThreadInit-Hyperflag-state3):
//     STMFD SP!, {R4, LR}
//     MOV R4, R0              ; R4 = thread
//     BL +offset (init helper)
//     LDR R3, [PC, #0x48]     ; R3 = vtable 0x50028798
//     STR R3, [R4]             ; *thread = vtable
//     ; ... initialise sub-objects at +0x48, +0x74, +0x80, +0xB8 ...
//     MOV R3, #3               ; ★
//     STR R3, [R4, #0x24]      ; ★ thread->iWaitState (+0x24) = 3 ★
//     MOV R3, #0
//     STR R3, [R4, #0x1C]
//     LDMFD SP!, {R4, PC}
//
// All callers come from LR=0x500111B0 (single caller). Empirically 47
// calls in 30 sim sec across 5 distinct thread bases:
//   0x80003CD0 (12 inits) — boot/init
//   0x80005174 (11 inits)
//   0x80005EAC (11 inits)
//   0x800063D0 (7 inits)
//   0x80006900 (6 inits)
// (Each thread re-initialised ~9-12 times due to the destructive cycle.)
//
// So +0x24=3 IS being set on multiple threads. The IPC delivery gate at
// FUN_50009D2C (which checks *(server_thread + 0x24) == 3) SHOULD pass
// for any of these threads. Yet:
//
//   session->iCount writes (0x80006FC0): 0   — IPC delivery still stuck
//   FastSemWait calls (0x5000F59C): 0
//   Resume calls (0x50011424):       0
//
// So even though +0x24=3 is set on these threads, the IPC delivery
// path doesn't fire. Possible reasons:
//   (a) The specific thread the message slot references (slot[0x18])
//       is NOT one of these 5 (per series5.h:2714 it was 0x80006DAC
//       in an earlier session — no longer present in current boot
//       due to address shift from destructive cycle).
//   (b) The gate also checks +0x14, which series5.h:2716 says =1
//       (not 3) — so even with +0x24=3 the gate fails on +0x14.
//
// To verify, the next concrete step is to find the kernel function
// that writes value=3 to +0x14 of any thread (using the same byte/word
// search approach that found PC=0x50009128 for +0x24).
//
// Diagnostic env vars added this session:
//   PSION_S5_THREAD_TRACE=N — log Resume / AddToReadyList /
//                              FastSemWait / WaitForAnyReq / ThreadInit
//
// 2026-05-07 (cont.): traced the gating event — FastSemaphore::Wait
// ---------------------------------------------------------------------------
// Found the EWaitFastSemaphore (state=3) writer: PC=0x5000F5C0 in
// NFastSemaphore::Wait at FUN_5000F59C (vtable 0x50028138, +0x20).
//
// PSION_S5_THREAD_TRACE confirms over 30 sim sec boot:
//   Resume (0x50011424):              0 calls
//   FastSemWait_entry (0x5000F59C):   0 calls
//   FastSemWait_setstate3 (0x5000F5C0): 0 calls
//   WaitForAnyReq_entry (0x5000B144): 80 calls
//   WaitForAnyReq_dispatch (vtable+0x20): 80 calls
//
// Critical finding: WaitForAnyRequest (SWI 0xC0004D) DOES fire 80×
// in the boot. But it dispatches via vtable 0x50028444 (NThread sub-
// object) at +0x20 → 0x5000F76C, NOT NFastSemaphore::Wait at
// 0x5000F59C. The two functions are nearly identical except for the
// state code:
//
//   FUN_5000F59C (NFastSemaphore::Wait):
//     STRB #3 → iNState (state=EWaitFastSemaphore)
//
//   FUN_5000F76C (NThread::WaitForRequest? sub-object Wait):
//     STRB #2 → iNState (state=ESuspended)
//
// So no thread ever transitions to state=3. But the IPC gate at
// FUN_50009D2C:0x50009D88 checks *(thread + 0x24) == 3 for the
// SERVER thread. Since +0x24 is checked at message-DELIVERY time
// (not creation time), and our threads write +0xC4 (iNState) but
// never +0x24, the gate fails.
//
// Tested PSION_S5_GATE_FIX=1: forces *(iCurrentThread + 0x24) = 3
// at WaitForAnyRequest dispatch. Empirical: no boot improvement
// (146 PCs at 15s, 276 at 30s — identical to baseline). The IPC
// delivery checks the SERVER thread's +0x24 at delivery time, and
// the server thread's reference is captured at message-create time.
// Patching the current thread's +0x24 afterwards doesn't help.
//
// The proper fix requires understanding what kernel function writes
// +0x24 at server-thread setup. The decompile shows:
//   - FUN_5000F76C writes only iNState (+0xC4)
//   - No found writer of value 3 to +0x24 with a literal MOV preceding
//   - +0x24 might be set as part of a struct-copy / context-init
//     that loads multiple words, with 3 as an indirect value
//
// Diagnostic env vars added:
//   PSION_S5_THREAD_TRACE=N — log Resume / AddToReadyList /
//                              FastSemWait / WaitForAnyReq calls
//   PSION_S5_GATE_FIX=1     — opt-in: poke thread+0x24=3 at
//                              WaitForAnyReq dispatch (no help)
//
// 2026-05-07: empirical Resume() vs AddToReadyList trace — the gate
// ---------------------------------------------------------------------------
// Added PSION_S5_THREAD_TRACE=N hook in arm710.cpp logging entries to:
//   PC=0x50011424 (NThreadBase::Resume — vtable[0x24])
//   PC=0x50042D54 (AddToReadyList — direct queue manipulation)
//
// Result over 30 sim sec boot:
//   Resume: 0 calls
//   AddToReadyList: 200+ calls, but ONLY 5 distinct thread pointers,
//     ALL in kernel-globals region 0x801002C0-0x8010031C:
//        0x801002C0 — kernel pseudo-thread / null
//        0x801002F4 — boot/init thread variant
//        0x80100308 — case-0 loader variant
//        0x8010031C — supervisor variant
//        0x005022C0 — stale low-memory pointer (RFile-related, not a thread)
//
// CONCRETE FINDING: NO user-mode thread structure ever makes it onto
// the ready list. The kernel scheduler exclusively cycles kernel-only
// thread pointers through fastmutex states (1→4→5→1) via direct
// AddToReadyList calls bypassing Resume().
//
// What SHOULD call Resume() on the user-mode EFile thread:
//
//   1. RProcess::Create (SWI 0xC00076 R0=0x1C) issued by supervisor
//   2. Server-side handler: Kern::ThreadCreate allocates DThread/NThread
//      (suspended state)
//   3. DProcess::Run() or equivalent: explicit Kern::ThreadResume(thread)
//   4. ThreadResume → NThreadBase::Resume() → state := EReady → enqueue
//
// Why step 3-4 doesn't fire (the gate):
//
//   The SWI 0xC00076 R0=0x1C process-create message is queued correctly
//   (per series5.h:3163-3192 — slot[0x20] reaches state 2 = "processed").
//   But response-delivery via FUN_50002630 fails the
//   `*(session->iServerThread + 0x24) == 3` check (= EWaitFastSemaphore).
//   The server thread (case-0 loader at 0x801002F4 in our destructive
//   cycle, was 0x80006074 earlier) is stuck in EHoldFastMutex (state 4)
//   because of the cycling fastmutex pattern — never reaches state 3.
//
//   Real EKA1 transitions the server thread through state 3 in response
//   to a kernel event (timer, IRQ, or specific semaphore wait). Our
//   emulator doesn't trigger this transition, so the server thread
//   never receives the process-create message, never creates the
//   user-mode EFile thread, never Resumes it.
//
// FIX DIRECTION: instrument the server thread's state transitions and
// identify what kernel code path would normally move it to state 3.
// Likely candidates:
//   - The kernel's WaitForAnyRequest implementation (SWI 0xC0004D)
//   - A FastSemaphore acquire/release pair the case-0 loader uses
//   - An IRQ that should wake threads from EWaitFastSemaphore
//
// This is the EKA1-specific scheduler invariant we've been searching for.
//
// Diagnostic env var added:
//
//   PSION_S5_THREAD_TRACE=N   log first N entries to Resume() and
//                             AddToReadyList. Confirms Resume never
//                             fires; only kernel-global thread pointers
//                             cycle through AddToReadyList.
//
// 2026-05-07: register-sequence diff vs Osaris — concrete divergence found
// ---------------------------------------------------------------------------
// Side-by-side trace of first 1 sim sec on both Series 5 and Osaris with
// PSION_IRQ_TRACE=1 reveals two concrete differences in early SYSCON1
// programming:
//
// Series 5 (EPOC R1):
//   SYSCON1 = 0x000000b0    (kscan=0, tc1=512k+periodic, tc2=512k)
//   ETNA writeReg8: wake1 = 0x80
//   ETNA writeReg8: IntClear = 0x3F   (clear all PCMCIA interrupts)
//   LCDCON write 0x5814E95F           (4bpp greyscale, custom prescale)
//   SYSCON1 = 0x000300b0              (+ ADCCON=0x3, 1MHz ADC clock)
//   TC2D = 0xffff
//   INTMR1 mask = 0x0200 (TC2OI)
//
// Osaris (EPOC R5):
//   SYSCON1 = 0x000000b0
//   SYSCON1 = 0x000400b0              (+ EXCKEN, external-clock enable)
//   LCDCON write 0x5A3A63E7           (different geometry)
//   SYSCON1 = 0x000700b0              (+ EXCKEN+WAKEDIS+IRTXM)
//   TC2D = 0xffff
//   INTMR1 mask 0x200 → 0x208 → 0x209 → 0xa09 → 0xa0d → 0xa0f
//   (progressively builds up enabled IRQ set)
//
// Concrete divergences:
//
//   1. Series 5 enables ADCCON (bits 16-17) = 0x3 (1 MHz ADC clock).
//      Osaris does not — it uses bit 18 EXCKEN (external clock) instead.
//      The Series 5 kernel is configuring the ADC1010 touchscreen
//      controller; Osaris doesn't have that chip.
//
//   2. ETNA writes are Series 5-only (PCMCIA controller). Osaris has
//      no ETNA — its PCMCIA goes via the on-chip CL-PS7600.
//
//   3. INTMR1 build-up: Osaris carefully unmasks IRQs in steps, ending
//      at 0xa0f (TC2OI + EXTFIQ + UART RX/TX + ?). Series 5 stops at
//      0x0200 (TC2OI only) for the entire 1 sim sec window — never
//      enables higher IRQs.
//
// Hypothesis: the Series 5 EPOC R1 kernel is waiting for ADC1010 /
// touchscreen state-machine progress before unmasking more IRQs. Our
// SYNCIO response (0x800 mid-scale) doesn't drive the state machine
// to the "ready" state, so the kernel never proceeds to unmask more
// IRQs and never reaches the splash painter.
//
// Actionable next steps for future sessions:
//
//   (a) Implement a proper ADC1010 emulation. The kernel polls SYNCIO
//       with requests 0x6D09 and 0x6D0C alternately (per FUN_5001D6E0).
//       Real ADC1010 returns a pen-press status bit in bit 0x400 plus
//       12-bit ADC data. Simulating "no pen, X = 0, Y = 0" steady-state
//       might let the kernel proceed.
//
//   (b) Trace the kernel's INTMR1-write code path to find what gates
//       it from progressing past 0x0200. If a specific event (timer
//       tick? ADC sample ready? GPIO edge?) is the gate, simulate it.
//
//   (c) Inject the Osaris-style INTMR1 progression directly via a
//       PSION_S5_INJECT (poke the value to virt 0x80000280 = INTMR1)
//       and see if forcing higher IRQs unlocks downstream code.
//
//       TESTED 2026-05-07: forced INTMR1 inject of 0x208, 0x209,
//       0xA0F, 0xFF all yield 130 PCs (regression from baseline 146).
//       Higher IRQ enables don't help — the kernel must reach the
//       code path that sets these itself, and external forcing
//       breaks downstream state. Path (a) ADC1010 emulation remains
//       the highest-leverage unexplored option.
//
// 2026-05-06: decompile-driven analysis — what's actually missing
// ---------------------------------------------------------------------------
// Used reference/5_decompiled/series5_v1.01(144)_eng.bin.c for the first
// time at scale. Several reframings:
//
// 1. FUN_5000C208 IS MatchF (descriptor wildcard match). Confirmed.
//
// 2. FUN_5000CD78 IS NThread::PopTrap — the EKA1 SWI 0x73 handler:
//      iCurrentThread->iTrap = iCurrentThread->iTrap->iNext
//    Decompile is a 5-line function with explicit NULL-check on iTrap.
//
// 3. FUN_500123C8 is NOT a TDesC builder — it's a "find free VA in range"
//    allocator. case 12 (R3=12) searches range 0x00400000-0x20000000 (the
//    user-mode VA space). The 0x00400000 we observed at virt 0x80003814
//    is the START of the search range, NOT a TDesC8 header. Earlier
//    matchf-decoder interpretation was WRONG — that buffer is a struct
//    holding a found VA, not a pattern descriptor.
//
// 4. The R0=-1 at the fault chain is NOT from FUN_5000FF40's read of
//    *iCurrentThread (write watch confirms iCurrentThread is never set
//    to -1). It's from the saved NThread context at the SCHEDULER SAVE
//    site 0x500193F8 (STMIA LR!, {R0-R12} → save to NThread+0xE4).
//    A thread was preempted with R0=-1 in its registers and that state
//    propagates through scheduler dispatches.
//
// THE STRUCTURAL CAUSE we can now articulate clearly:
//
//   A thread, while in SVC/Undef mode running PopTrap (or similar),
//   had R0=-1 at the moment of an IRQ. The scheduler saved R0=-1 to
//   that thread's NThread+0xE4 block. On next dispatch the thread
//   resumes at PC=0x5000CD80 with R0=-1, and the LDR R0,[R2,#0x3C]
//   at 0x5000CD84 faults on address 0x3B.
//
// HOW R0 BECOMES -1 IN THE FIRST PLACE: the saved NThread block was
// never properly initialised. Heap-fresh memory in the boot heap chunk
// is at 0xFFFFFFFF (RAM init pattern). When a thread is allocated,
// its saved-context slots are NOT explicitly zeroed by the allocator —
// they're filled with whatever the heap had. If the kernel dispatches
// a thread before its first IRQ-save, R0 in the saved block is RAM-
// init 0xFFFFFFFF.
//
// CONCRETE THINGS TO IMPLEMENT (in priority order):
//
//   A) Verify NThread initialisation. Hook the kernel's thread-allocator
//      and check whether saved-R0..R12 slots at NThread+0xE4..+0x118 are
//      initialised to 0 before first dispatch.
//
//   B) Check whether the destructive cycle's UND-stack overflow is
//      triggered by repeated abort handler invocations from this same
//      R0=-1 fault. If yes, fixing (A) breaks the cycle entirely.
//
//   C) [TESTED — RULED OUT 2026-05-06] Compare RAM init. Added
//      PSION_RAM_FILL=NN env var. Tried fill=0x00, 0xA5, 0xCC, 0xFF
//      and default — ALL produce identical 122 PCs at 12 sim s.
//      The 0xFFFFFFFF in NThread saved-context isn't from uninitialised
//      RAM; it's actively WRITTEN by kernel boot at PC=0x5000063C
//      (a fill loop with MVN R2,#0 fill value) called from PC=0x50000478.
//      The kernel deliberately fills the SVC stack region with -1 as a
//      canary pattern — this is correct EKA1 behaviour, not a bug.
//
//   D) Audit IRQ delivery during SVC/Undef-mode kernel critical sections.
//      Check whether 0x500193F8 (scheduler save) sets CPSR.I=1 and
//      whether our emulator respects it.
//
// THE CORE INSIGHT after this session's decompile-driven analysis:
//
// The R0=-1 at the fault is MatchF's actual return value (MVN R0,#0 at
// PC=0x5000C2A4 = "no match found"), NOT a propagated saved-context
// value. Verified by PC history entry 057 at fault time.
//
// The fault chain is:
//   1. Some kernel code pushes 0x5000CD80 onto SVC stack as a "callback"
//      (via STMFD with LR=0x5000CD80 set somehow — no static caller of
//      MatchF or PopTrap was found, suggests a runtime-set LR via a
//      coroutine-style continuation pattern)
//   2. MatchF runs, iterates a kernel-object container, finds no match
//   3. MatchF MVN R0,#0 → R0 = -1
//   4. LDMFD SP!,{R4-R10,PC} pops 0x5000CD80 into PC
//   5. Execution lands at 0x5000CD80 (PopTrap body) with R0 = -1
//   6. MOV R2, R0; LDR R0, [R2, #0x3C] → AlignmentFault on 0x3B
//
// MatchF returns -1 because the kernel-object container being searched
// doesn't contain the expected entry. The matchf-trace decoder shows
// the iteration hits "$DAT" (a kernel chunk) and the pattern data area
// contains the string "EFile[100000bb]*" — the kernel is searching for
// chunks/objects related to EFile (process UID 0x100000bb) but EFile
// itself isn't registered yet at this point in our boot.
//
// THE STRUCTURAL FIX requires either:
//   (a) Pre-populating the relevant kernel-object container with a
//       fake EFile-related entry the iteration can match. Risky:
//       downstream code uses object's vtable/fields that need to be
//       coherent with the kernel's expectations.
//   (b) Making EFile process register earlier in the boot sequence.
//       Requires understanding the kernel's bootstrap order, which
//       depends on EKA1 source we don't have.
//   (c) Implementing a working Resume()/AddToReadyList() path so
//       threads created via Process::Create actually run and register
//       their objects. Requires understanding NThread state machine.
//
// CONCRETELY ATTEMPTED in this session:
//
//   * PSION_S5_FIX_SCHED_RESTORE (opt-in, off by default in arm710.cpp)
//     — patches R0..R3 = -1 → 0 at PC=0x5001982C. Doesn't fire because
//     that PC isn't in the fault path. Kept as opt-in for future
//     investigators in case some other scheduler-restore path matters.
//
//   * PSION_RAM_FILL=NN (clps7111.cpp) — fills MemoryBlockC0 with byte
//     value at boot. Tested 0x00, 0xA5, 0xCC, 0xFF — all yield same
//     122 PCs. Rules out "uninitialised RAM affects boot" hypothesis.
//
// WHERE FUTURE SESSIONS SHOULD START:
//
//   * Identify the kernel's chunk container offset (probably 0x80000888
//     per identifyObjectCon()). Trace its structure layout via a write
//     watch on that range.
//   * Find the kernel's chunk-create function (likely FUN_5000xxxx,
//     allocates DChunk, registers in container).
//   * Implement a hook that, on first MatchF call with pattern containing
//     "EFile[", logs ALL container entries — see what's there vs what
//     real hardware would have.
//   * Try faking a single chunk entry as the FIRST iteration target
//     so MatchF returns 0 (success) on the first compare, then trace
//     downstream to see if boot proceeds further.
//
// 2026-05-06: injection sweep — matchf+skip_bl combo accelerates exploration
// ---------------------------------------------------------------------------
// Built PSION_S5_INJECT generic poke/return/skip mechanism in arm710.cpp
// (commit 9824daa) and ran an 18-combination sweep over different intervention
// shapes. Best combo:
//
//   PSION_S5_INJECT=ret:0x5000C208:0;skip:0x5000CD7C
//
// (= short-circuit MatchF + skip the iCurrentThread load inside PopTrap)
//
// PC count growth vs baseline:
//                     baseline   matchf+skip_bl    delta
//   12 sim s            122           208           +86
//   30 sim s            155           251           +96
//   45 sim s            164           422          +258
//   90 sim s            ~190          441          +251
//
// At 30→45s the matchf+skip_bl combo enters a new code region that the
// baseline reaches more slowly — kernel jumps from 251 to 422 PCs in
// 15 sim s. After 45s the growth slows; at 90s we plateau near 441.
//
// Documented baseline plateau is ~444 PCs at 120 sim s, so the combo
// just reaches the same ceiling ~30s sooner. The structural ceiling at
// ~440 PCs is INDEPENDENT of the matchf/poptrap interventions.
//
// Variance remains 0 in all cases — no combination yet paints the LCD.
//
// Practical take-away: the matchf+skip_bl combo is faster but not
// fundamentally different. To break the plateau we need to identify
// what code paths the kernel ISN'T reaching at 440 PCs, then inject
// something to unlock those paths. That requires a different class of
// intervention than the SWI-handler short-circuits we've been testing.
//
// 2026-05-06: combinatorial knob sweep — 64 combinations, 12 sim s each
// ---------------------------------------------------------------------------
// Built scripts/series5-combo-sweep.py: parallel runner that takes a list of
// dimensions (each = a list of (label, env) values) and runs the cartesian
// product. First sweep covered 6 binary knobs (BREAK_FAULT_LOOP, HAL_LEGACY,
// SKIP_HAL, PRESERVE_KDATA, SYNTH_DELIVERY, FORCE_GATE3) = 64 combinations.
// Wall time: ~4 minutes on 4 cores.
//
// PC-count buckets across all 64 combos (no combination raised variance):
//
//   122 PCs  4 combos: baseline + (preserve_kdata|skip_hal) — no effect
//   120 PCs  8 combos: + force_gate3 (mild regression)
//   113 PCs  4 combos: break_loop=off (the documented +9 from the patch)
//   110 PCs  8 combos: + synth_delivery (regression)
//   109 PCs 24 combos: + hal_legacy (consistent -13 PCs)
//    94 PCs  8 combos: synth_delivery+force_gate3 ★ WORST
//
// Concrete findings:
//
//   1. preserve_kdata and skip_hal are NO-OPS in the current boot —
//      they don't change unique_pcs in any combination. Either the code
//      paths they intercept aren't reached, or the interventions don't
//      land where intended. Candidate for removal/revision.
//
//   2. synth_delivery + force_gate3 combined regresses -28 PCs from
//      baseline. This was never tested before — single-knob runs only.
//      The two interventions corrupt overlapping kernel state when
//      combined.
//
//   3. break_loop's +9 PCs is real (122 vs 113), but applying any other
//      "fix" from the existing knob set on top of break_loop doesn't
//      improve further. The boot is plateaued at 122 PCs given the
//      current intervention set.
//
//   4. NO combination raised variance above 0 — LCD never painted in
//      any combination. The blocker is structural, not unlocked by any
//      existing env-var.
//
// This proves the existing PSION_S5_* knob set is exhausted as a
// hypothesis space. Further progress requires either:
//   * A generic memory/state injection harness (option 2 from the
//     hypothesis-iteration discussion) for testing "what if X kernel
//     value were Y at cycle Z?"
//   * Differential trace against a working PS711x boot (option 3)
//   * EKA1-source-aware analysis (we don't have the source).
//
// The sweep tool itself is a permanent asset — runs in 4 min on 4
// cores, easy to extend by adding dimensions to the DIMENSIONS list.
//
// 2026-05-06 (cont.): 0x80003814's 0x00400000 is INTENTIONAL kernel code
// ---------------------------------------------------------------------------
// Earlier this session the matchf decoder showed pattern descriptor at
// 0x80003814 has header 0x00400000 (looks like "TBufC8 with 4 MB length"
// — clearly impossible). PSION_WRITE_WATCH=0x80003800-0x80003830 over
// the destructive-cycle window finds the writer:
//
//   insncyc=36681766: WRITE va=80003814 size=V32 val=0x00400000
//                     pc=0x500124D0 lr=0x50012348 cpsr=0x1B
//
// PC=0x500124D0 is `STRLS R5, [R8]` inside FUN_500123C8 (a structure-
// builder helper). Disassembly of that function shows a SWITCH on
// (R2 - 2) with 11 cases, where case 10 (entered when R2 = 12 at
// function entry) executes:
//
//   0x50012440: MOV R6, #0x40, ROR #16   ; R6 = 0x40 ROR 16 = 0x00400000
//   0x50012444: MOV R7, #0x80000000
//
// then post-switch code stores R5 (= R6 after a MOV) into *R8 where R8
// is the destination buffer = SP + 0x6C in the caller's frame.
//
// So 0x80003814 is NOT a TDesC8 — it's a kernel search-context structure
// (TFindHandle-like) where field+0 is a 32-bit state/flag with value
// 0x00400000 for "type 12" searches. Our matchf decoder (added earlier
// to identify the failing match) was misinterpreting this struct as a
// raw descriptor.
//
// Type 12: in EKA2's TObjectType enum (kernel/eka/include/u32std.h) type
// 12 is EChangeNotifier. EKA1 may differ — clps7111.cpp's identifyObjectCon
// shows EKA1 container globals at 0x80000880 (process) through 0x800008AC
// (library) with non-EKA2 ordering, so type 12 in EKA1 is unknown without
// the EKA1 source. The candidate observed at the failing call ("$DAT", a
// chunk name) and pattern ("EFile[100000bb]*", a process-style pattern)
// don't fit a single object-type interpretation cleanly.
//
// Conclusion: FUN_5000C208 is a kernel-object-container iterator (a
// TFindBase::MatchF-like primitive). It walks the type-12 container,
// returns -1 when nothing matches the pattern. The caller (the function
// at 0x5000CD78 + the chained continuation popping 0x5000C208 from the
// SVC stack) doesn't guard against -1, so it dereferences it as a
// pointer and faults.
//
// The same structural blocker as before: kernel-side state for the type-12
// container is missing the object the kernel expects to find. Without
// EKA1 source we can't precisely identify which object should be present.
//
// 2026-05-06 (cont.): callsite identification for 0x5000CD80 chain
// ---------------------------------------------------------------------------
// Static analysis of the ROM (Python scan for B/BL targets and 32-bit data
// constants) revealed that BOTH FUN_5000CD78 (PopTrap) and FUN_5000C208
// (MatchF) have **zero** static B/BL callers. The only reference to each
// function in the entire 8 MB ROM is a single 32-bit data constant inside
// the FAST exec table at 0x5002795C:
//
//   ROM[0x50027AAC] = 0x5000C208   // FAST exec table[0x54] = MatchF
//   ROM[0x50027B28] = 0x5000CD78   // FAST exec table[0x73] = PopTrap
//
// So MatchF is the SWI 0x54 handler and PopTrap is the SWI 0x73 handler.
// Both are reached only via SWI dispatch at 0x500194DC.
//
// The fault chain reaches PC=0x5000CD80 with R0=0xFFFFFFFF after MatchF
// returns -1. PSION_S5_MATCHF_BLSITE_TRACE=N at PC=0x5000C208 with
// LR=0x5000CD80 confirms LR is exactly 0x5000CD80 at MatchF entry, yet
// the ROM byte at virt 0x5000CD7C reads 0xEB000C6F (BL to FUN_5000FF40,
// NOT to MatchF). And `MOV LR, PC` at 0x500194F4 in the SWI dispatcher
// always sets LR=0x500194FC. So the MatchF call with LR=0x5000CD80 is
// reached by some non-standard mechanism: not a static BL, not a
// dispatcher-provided LR.
//
// PC history at fault time shows execution flowing:
//   IRQ exit at 0x50019888 (LDM SP^,{R0-R3,R12,PC}^) →
//   PC=0x5000CD80 (function body) → 0x5000CD94 (LDMFD pops 0x5000CD80) →
//   PC=0x5000CD80 again (re-entry) → LDMFD pops 0x5000C208 →
//   MatchF entry STMFD pushes LR=0x5000CD80
//
// This is co-routine-style continuation passing: the SVC stack has
// pre-loaded continuation addresses (0x5000CD80, 0x5000CD80, 0x5000C208,
// ...) and each LDMFD pops the next one. The kernel uses this for some
// kind of chained operation across iCurrentThread's [0x3C] list with
// MatchF as a step.
//
// Pattern descriptor analysis at PC=0x5000C208 entry (cycle 39991684,
// PSION_S5_MATCHF_TRACE=2000):
//   r0 = 0x8000633C (candidate "$DAT" — valid TBufC8)
//   r1 = 0x80003814 (pattern — header reads 0x00400000 = type 0, len 4MB)
//
// The pattern at 0x80003814 is malformed: header value 0x00400000 claims
// "TBufC8 with 4 MB of inline data" — clearly nonsensical. PSION_WRITE_WATCH
// on 0x80003814 over a 22-sec boot showed only one 32-bit value class
// written there: pointers like 0x80003948 / 0x800038C0 (NOT 0x00400000).
// Yet readVirtualDebug(0x80003814, V32) returns 0x00400000 at the moment
// of the matchf trace.
//
// This memory-aliasing-style inconsistency between observed writes and
// observed reads at the same address is the next concrete blocker. It
// suggests one of:
//   (a) Writes go through an MMU-aliased path that the WRITE_WATCH
//       infrastructure misses.
//   (b) The matchf decoder's readVirtualDebug uses a different translation
//       than the runtime LDR — i.e., dual mapping where I/D-side reads
//       diverge.
//   (c) There's a live-only hook (kernel-data-init memcpy or shadow
//       NThread block) that overwrites 0x80003814 between traced writes
//       and the matchf read.
//
// The destructive-cycle reset at PC=0x5004D90C is a known kernel-data-init
// memcpy that runs every ~17.85M cycles. Its writes don't appear in
// per-address WRITE_WATCH because the writes go via a path that bypasses
// writeVirtual (or the watch's address range doesn't include it). Need
// to confirm.
//
// No fix this session — purely diagnostic findings. Diagnostic hooks
// added this session:
//
//   PSION_S5_PT_ENTRY_TRACE=N   at PopTrap entry (0x5000CD78), log
//                               LR + SP + 12 stack words + iCurrentThread.
//                               Filters to "bad LR or bad iCurrentThread"
//                               cases only — confirmed PopTrap entry path
//                               is always normal (LR=0x500194FC).
//   PSION_S5_MATCHF_BLSITE_TRACE=N  at MatchF entry (0x5000C208) when
//                               LR=0x5000CD80, decode the instruction at
//                               LR-4 to identify the call site. Confirms
//                               static ROM at 0x5000CD7C reads 0xEB000C6F
//                               (BL to FUN_5000FF40), not BL to MatchF.
//
//   The break-loop patch at PC=0x5000CD80 with R0=-1 is still default-ON
//   per commit 18bd7a1 / 16abfef. It eliminates the AlignmentFault but
//   shifts the fault to a downstream prefetch error (LDMFD pops a stack
//   value of 0xBBBBBBBB, the SVC stack debug-fill pattern). Same number
//   of trap events; the test harness's grep keyword set doesn't catch
//   either keyword so the boot test reports traps=0 either way.
//
// 2026-05-10: gate semantics CORRECTED + scheduler is healthier than thought
// ---------------------------------------------------------------------------
// Two prior notes (2026-05-04 lines 2705-2716 and 2026-05-07 lines 4097-
// 4116, 4172-4231) were partly wrong. Re-investigated by disassembling
// the actual ROM bytes at FUN_50009d24 and adding two new traces:
//   PSION_S5_GATE_TRACE=N  — FUN_50009d24 entry (R0/R1/R2/LR + +0x24/+0x14)
//   PSION_S5_WAKE_TRACE=N  — at 0x50009d88 (LDR PC, [R3, #0x24]) — log
//                            sub-obj at *(R4+0x68), its vtable, the
//                            wakeup target, and the sub-obj +0x14 counter
//
// Disassembly of FUN_50009d24:
//   50009d40: LDR R3, [R4, #0x24]
//   50009d44: CMP R3, #3
//   50009d48: BNE 0x50009d8c   ← jumps to FUNCTION EPILOGUE, NOT alt-path
//   50009d4c: LDR R3, [R4, #0x14]
//   50009d50: CMP R3, #3
//   50009d54: LDREQ R3, [SP]
//   50009d58: STREQ R3, [R0]    ← fast-path direct write
//   50009d5c: BEQ 0x50009d7c    ← skips vtable[0x48] when EQ
//   50009d60-78: vtable[0x48] dispatch (slow VM-mediated write)
//   50009d7c-88: wakeup R0=*(R4+0x68); LDR PC, [R0+0x24]
//   50009d8c: ADD SP,SP,#4 ; LDMFD ...
//
// CORRECTED gate semantics: only `+0x24 == 3` gates delivery. `+0x14 == 3`
// is a fast-vs-slow PATH SELECTOR; both paths fire delivery + wakeup.
// → The "find the +0x14=3 writer" hunt is a dead end. The +0x14 values
//   observed at runtime are 0/1/2 — these are PRIORITY ORDINALS, not a
//   state flag.
//
// Live trace (boot-secs=15, default device=series5, no extra knobs):
//   Gate hits on three server threads, all with +0x24 == 3:
//     0x80005174 (priority +0x14=0, sub=0x800052e0, lr=0x50002be0)
//     0x80005eac (priority +0x14=1, sub=0x80006010, lr=0x50002e44)
//     0x800063d0 (priority +0x14=2, sub=0x80006630, lr=0x50002e44)
//   Wakeup target on every dispatch: FUN_5000f7dc (NFastSemaphore::Signal,
//   vtable 0x50028444[0x24]).
//   Sub-obj +0x14 (sem counter) = 0xFFFFFFFF (-1) for 0x800052e0
//   (waiter present), 0 for the other two (no waiter, Signal no-ops).
//   So the IPC delivery + Signal path WORKS for the active server.
//
// AddToReadyList (PC=0x50042D54) IS called — multiple times per second,
// targeting both kernel globals (0x801002C0/F4/etc.) AND server thread
// bases (0x80005174, 0x80005eac, 0x800063d0). The 2026-05-07 note
// "AddToReadyList: 0 calls for any user thread" was measured during
// the destructive thread-B fault loop (pre-break-loop patch) and is
// stale. With the break-loop patch default-on, the scheduler is
// healthy among kernel server threads.
//
// PSION_SCHEDULE_TRACE=1 confirms iCurrentThread rotates between four
// kernel threads (0x80003CD0, 0x80005174, 0x80005EAC, 0x800063D0) with
// brief returns to idle (0). All in CPSR=0x1B (UND26 = supervisor in
// 26-bit mode, our 32-bit emulator coerces 0x08 to User32 — see
// arm710.cpp:50).
//
// THE ACTUAL BLOCKER (pivot to handover Path B): no user-mode thread
// is ever ThreadInit'd. ThreadInit_setstate3 fires only for the four
// kernel server threads above. EFile.EXE and downstream user threads
// never have ThreadInit called on them — so they never reach the gate
// to begin with. The IPC machinery delivers KErrNone (p3=0) to its
// kernel-side waiters, but nothing in the chain is actually mapping
// EFile.EXE into user memory and creating its DProcess/DThread pair.
//
// NEXT: trace what happens when the supervisor SWIs RProcess::Create
// (likely SWI 0xC00076 R0=0x1C) for UID1=0x1000007A (EFile). The
// kernel-side handler claims the message slot (slot[0x20]: 0→1→2)
// and signals the response, but does NOT allocate a new DProcess.
// Find the function the loader SHOULD reach but doesn't.
//
// PSION_S5_GATE_FIX (lines 568-583 of arm710.cpp) is now misaimed —
// it pokes iCurrentThread+0x24 = 3 at WaitForAnyRequest dispatch,
// which can only ever poke the CURRENT thread, not the SERVER thread
// the gate examines. Leaving the knob in place (default-off, no
// effect on default boot) but documenting that it should be removed
// once we don't need it for diagnostic comparison.
//
// 2026-05-10 (cont.): User32 IS reached; 146-PC plateau is a sampler
// artifact — boot is FURTHER along than the headline number suggests
// ---------------------------------------------------------------------------
// PSION_MODE_TRACE=u over 15 sim sec captures 3408 events involving
// CPSR=0x10 (User32). First entry at outer cycle 11,333,864
// (≈ 615 ms sim time) via UND→User return at PC=0x5001AE38 LR=0x5001AE44.
// 1476 explicit 0x13→0x10 (SVC→User) transitions follow. So user-mode
// IS executing.
//
// User-mode entry sites (1B→10 at pc=...):
//   156×  pc=0x50019594 lr=0x500195A0  — kernel SWI return path
//    36×  pc=0x5003AD04 lr=0x5003AD10  — RProcess::Create wait setup
//    12×  pc=0x5001AE38 lr=0x5001AE44  — initial user-mode entry
//     1×  pc=0x5002C8B0 lr=0x00000000  — bootstrap kick
//
// User-mode exit sites (10→X with LR = user-mode caller PC):
//   396×  lr=0x5003A7AC  — user-mode atomic helper
//   396×  lr=0x5003A7E4  — user-mode atomic helper (paired)
//   216×  lr=0x5003A150  — user SWI helper
//   120×  lr=0x5003A138  — user SWI helper
//   108×  lr=0x50043E40  — scheduler entry
//   108×  lr=0x5004C36C  — kernel return path
//    48×  lr=0x5003AD10  — RProcess::Create wait
//    36×  lr=0x5002A618  — user SWI dispatch
//    36×  lr=0x5003A20C  — user SWI helper
//    36×  lr=0x50039E78  — WaitForRequest loop
//
// CONCRETE STATE: EFile.exe is running in User32 and is calling
// RProcess::Create (48 exits from lr=0x5003AD10) + WaitForRequest
// (36 exits from lr=0x50039E78), almost certainly attempting to spawn
// the next system process — likely EWSRV.EXE (WindowSrv) per the
// 2026-05-06 trace at series5.h:3819-3861. The create-wait does NOT
// complete, so no further user thread is created; the same shape of
// blocker as the original supervisor-spawn issue, just one layer up.
//
// IMPORTANT: The `unique_pcs=146` figure reported by --assert-boot is
// the count from a 100 Hz PC sampler that only sees whichever PC is
// executing at sample time. With most sim time spent in kernel idle
// loops, user-mode bursts are systematically under-sampled. Use
// PSION_MODE_TRACE=u or PSION_INSN_TRACE_CYC for true coverage.
//
// NEXT-SESSION FOCUS (path-B continuation): trace the SWI issued from
// PC=0x5003AD10 (RProcess::Create wait) — what kernel function handles
// it, where does the create message land, and why doesn't the response
// arrive? The kernel scheduler + IPC machinery work (this commit) so
// the failure must be in the create-handler's allocator/loader chain.
//
// 2026-05-10 (cont.): destructive cycle DECODED — boot thread calls
// User::Panic("E32USER-CBase", 33) every cycle
// ---------------------------------------------------------------------------
// New trace knobs added:
//   PSION_S5_SR_TRACE=N         — at PC=0x5003ACF4 (user-mode SR
//                                  wrapper entry): R0..R3 + LR + SP
//   PSION_S5_SR_RETURN_TRACE=N  — at the wrapper's CMP R0, #0
//                                  (PC=0x5003AD10) and the 4 caller-side
//                                  BL-return PCs (0x5003EF3C / F20C /
//                                  F29C / F670). Logs which selectors
//                                  return synchronously and which don't.
//   PSION_S5_DISP_TRACE=N       — at the SWI 0xc00076 dispatcher
//                                  (FUN_500096CC, PC=0x500096CC) and at
//                                  the panic-raise (FUN_5000FF68,
//                                  PC=0x5000FF68). Decodes the panic
//                                  category descriptor (TPtrC8 type 1)
//                                  to plain text + reason code.
//   PSION_S5_PANIC_CALLSITE=N   — at PC=0x5004BD44 (SWI 0xc00076
//                                  instruction) when R0=0x2F (panic
//                                  selector). Walks 4 stack frames
//                                  (wrapper@0x5003AD2C → User::Panic@
//                                  0x5003F9D0 → panic_thunk@0x5003BF14
//                                  → panic_helper_NN) to find the
//                                  ROM caller PC.
//
// What we now know about the destructive cycle (from these traces):
//
// Per cycle (~ every 11M cycles, 12 cycles in 15 sim sec):
//   1. Kernel-mode SWI 0xc00076 sel=0x1C (RProcess::Create-equivalent)
//      from caller @ 0x500403D0 (R1=0x80104534) → returns R0=0 (success)
//   2. Kernel-mode SWI 0xc00076 sel=0x1D (similar) → returns R0=0
//   3. User-mode SWI 0xc00076 sel=0x15 (F32 op) → returns R0=0
//   4. User-mode SWI 0xc00076 sel=0x19 (F32 op) → returns R0=0
//   5. User-mode SWI 0xc00076 sel=0x1B (F32 op) → returns R0=0
//   6. User-mode SWI 0xc00076 sel=0x27 (F32 op via FUN_50005728) is
//      DISPATCHED but the response is never delivered — the wrapper
//      never returns to the CMP R0, #0 at 0x5003AD10.
//   7. ~120K cycles after step 6, the BOOT/INIT THREAD (NThread
//      0x80003CD0) executes FUN_5002DE9C, asserts *(R4+4) == 0
//      where R4 is some object passed in, finds it non-zero, calls
//      panic_helper_33 → User::Panic("E32USER-CBase", 33).
//      The panic kills the boot thread.
//   8. Kernel restarts (~11M cycles later).
//
// After 2 such cycles (at outer cycles ~33M+), the system enters an
// abort-mode loop. Each iteration: only sel=0x1C dispatches, then the
// abort handler at PC=0x500176C0 fires the "Exception 0x10000000"
// panic via `func panic_helper_19` at 0x50043E98. Caller is a 5-way
// switch default at PC=0x50037A8C (R0 > 4 → panic), which avalanches
// — multiple back-to-back calls all hit the panic.
//
// CONCRETE CALL SITES (single-source-of-truth for next investigation):
//   "E32USER-CBase 33" panic:
//      panic_helper_33 entry:  0x5003069C
//      panic_helper_33 BL:     0x500306BC
//      ROM caller of helper:   inside FUN_5002DE9C, BL at 0x5002DEC0
//      Function entry:         0x5002DE9C
//      Assert pattern: load R3 = *(R4+4); CMP R3, #0; if non-zero
//                      MOV R0, #0x21 (33); BL panic_helper_33
//   "E32USER-CBase 19" panic (avalanche):
//      panic_helper_19 entry:  0x50043E98
//      panic_helper_19 BL:     0x50043EBC
//      ROM caller of helper:   PC=0x50037A8C
//      Function entry:         0x50037A2? (jump-table dispatch on R0)
//      Pattern: switch (R0) { case 0..4: return ptr; default: panic 19; }
//
// User::Panic helper plumbing:
//   thunk @ 0x5003BF14 (4 inline thunks 0x5003BF14/+44/+70/...) —
//     each does: STMFD {LR}; SUB SP, #4; LDR R3, [PC, #0x14] (loads
//     header word 0xFFFF8001); STR R3, [SP]; MOV R2,R1; MOV R1,R0;
//     MOV R0,SP; BL User::Panic at 0x5003F9D0.
//   User::Panic @ 0x5003F9D0: STMFD {LR}; SUB SP, #16; stores
//     *R0 + R2 + R1 to local frame; R0=0x2F; BL SR wrapper at
//     0x5003AD2C; SWI 0xc00076.
//
// NEXT STEP: trace what calls FUN_5002DE9C (the assertion that fails).
// Specifically, what object's +0x04 field is non-zero in our boot, and
// who initialised it that way. The function body suggests it's a
// "first-line construction" pattern — sets *R4 = vtable, asserts
// *(R4+4) == 0 (sub-object pointer must be null at first construction).
// Likely: an EFile/F32 init object whose iSubObject is being doubly
// initialised, or whose iLink->iNext was preset by the allocator.
//
// Possible angles:
//   (a) Hook PC=0x5002DE9C entry, log R0 (= R4 = the object) and LR
//       (= the function that called the asserting init).
//   (b) Read *(R0+4) at the hook to confirm the bad value, then
//       trace upstream (write-watchpoint on that virtual address)
//       to find what wrote it and WHEN.
//   (c) The constant stored at *(R4+0) is *(0x5002DEF8) — check what
//       vtable that is. That identifies the OBJECT TYPE and narrows
//       which kernel sub-system is mis-initialised.
//
// 2026-05-10 (cont., later): assert hook output (PSION_S5_ASSERT_TRACE)
// ---------------------------------------------------------------------------
// Per cycle, FUN_5002DE9C is called 5 times:
//
//   4 SUCCESS calls (assert holds: *(this+4) == 0):
//     this=0x80006AE4 (vtable 0x50028444), R1=3, lr=0x5002DF5C
//     this=0x80006B48 (vtable 0x50028750), R1=3, lr=0x5002DF5C
//     this=0x80006B0C (vtable 0x50028444), R1=3, lr=0x5002DF5C
//     this=0x80006A6C (vtable 0x50028104), R1=3, lr=0x5002DF5C
//   1 FAILURE call (assert fires: *(this+4) == 1):
//     this=0x80006900 (vtable 0x50028798 = NThread vtable!),
//     R1=0x80003D68, lr=0x5004C384 ← EXTERNAL caller
//
// The failing object 0x80006900 is the LAST of the 5 ThreadInit'd
// thread bases (per series5.h:4090-4094). It's an NThread instance
// whose iAccessCount-equivalent (+0x04) is still 1 when the kernel
// tries to "destroy/convert" it via FUN_5002DE9C.
//
// The external caller at 0x5004C360 does:
//   STMFD SP!, {R4, LR}
//   MOV R4, R0
//   BL 0x5004BD30  (= SWI 0xc00072 — kernel handle/iterator)
//   LDR R0, [R4, #0x48]   ; load handle's object pointer
//   CMP R0, #0
//   LDMEQFD SP!, {R4, PC} ; if null, return
//   LDR R3, [R0]          ; R3 = *R0 = vtable
//   MOV LR, PC
//   LDR PC, [R3, #8]      ; ← virtual-dispatch vtable[+0x08]
//                            For NThread, this resolves to FUN_5002DE9C
//
// So vtable[+0x08] of NThread (vtable 0x50028798) IS FUN_5002DE9C.
// FUN_5002DE9C overwrites the vtable to 0x50050900 and asserts
// the iAccessCount-equivalent at +0x04 is 0.
//
// Hypothesis: this is the standard Symbian CObject "destroy" virtual
// (the second-stage destructor that's called after iAccessCount has
// been decremented to 0 by Close()). Our boot is calling Destroy()
// on the thread without first decrementing the count — i.e., the
// kernel believes the thread is referenced (count==1) but is also
// trying to destroy it.
//
// CONCRETE NEXT STEPS:
//   1. Add a write-watchpoint on virt 0x80006904 (the failing
//      object's +0x04). Find the ROM PC that wrote 1 there last
//      and the PC that DIDN'T decrement it before destroy.
//   2. Trace SWI 0xc00072 dispatch to identify the kernel handle/
//      iterator returning 0x80006900 (perhaps a thread enumerator
//      that should have skipped this one).
//   3. The "extra" thread 0x80006900 isn't accounted for in the
//      ThreadInit cycle inventory — figure out who creates it and
//      whether it's a kernel-internal tagging that should NOT be
//      destroyed at this boot stage.
//
// 2026-05-10 (cont., latest): write-watchpoint + destroy-iter results
// ---------------------------------------------------------------------------
// PSION_WRITE_WATCH=0x80006904 captured the full lifecycle of
// *(0x80006904) (the iAccessCount-equivalent) per cycle:
//
//   1. PC=0x5004D77C lr=0x5003D7D8 — alloc fills 0xa5a5a5a5
//      (Symbian "kFreeFill" debug pattern, fired ONCE per ~10M cycles
//      at boot start)
//   2. PC=0x5004D764 lr=0x5003074C — zero-init writes 0
//      (per cycle, before construction)
//   3. PC=0x5002DE8C lr=0x5002DE80 — FUN_5002DE74 constructor
//      writes 1 (sets iAccessCount = 1, vtable = 0x50050900)
//
// Notably ABSENT: any decrement (Close())-like write between
// (3) and the eventual destroy-via-FUN_5002DE9C. So the destroy
// is reached without the corresponding Close() run.
//
// PSION_S5_DESTROY_ITER_TRACE captured the kernel destroy-iterator
// (FUN_5004C360) when it dispatches on R0=0x80006900 (the failing
// NThread). Per cycle:
//
//   target=0x80006900 R3=0x500283A8 callerLR=0x50009610
//                                  (BL site at 0x5000960C)
//
// So FUN_5004C360 is itself called from PC=0x5000960C with R4 set
// such that *(R4+0x48) = 0x80006900. The vtable at *R0 is
// 0x500283A8 (NOT 0x50028798=NThread vtable as the assert hook
// records). Vtable+8 of 0x500283A8 = 0x50027114 (= B 0x500111C4),
// so the indirect dispatch goes to 0x500111C4 (not directly to
// FUN_5002DE9C).
//
// FUN_5002DE9C is reached LATER in the chain (~1700 cycles after
// the dispatch) with LR=0x5004C384 — likely via tail-chained
// branches inside 0x500111C4's body (which itself sets a vtable
// at *R4 then calls multiple sub-functions). Tracing the exact
// chain from 0x500111C4 → ... → 0x5002DE9C is the next concrete
// task.
//
// SUMMARY of what we know about the destructive cycle now:
//
//   * Boot allocates a thread structure at virt 0x80006900.
//   * Constructor (FUN_5002DE74) sets vtable=0x50050900,
//     iAccessCount-equivalent (+0x04) = 1.
//   * (Possibly) something else overwrites *(this+0) to a
//     different vtable (0x500283A8 then 0x50028798 — these
//     transitions happen between the constructor and destroy).
//   * Boot-init thread enumerates objects via FUN_5004C360 from
//     PC=0x5000960C, dispatches vtable[+8] on each.
//   * For object 0x80006900, the chain eventually hits
//     FUN_5002DE9C (the assert function), which expects
//     *(this+4)==0 but finds 1 → User::Panic("E32USER-CBase",33).
//   * The panic kills the boot thread, system restarts the cycle.
//
// To unblock the boot, EITHER:
//   (a) ensure something decrements *(0x80006904) from 1 to 0
//       before FUN_5002DE9C is reached, OR
//   (b) prevent FUN_5002DE9C from running at this boot stage
//       (e.g. by skipping the BL or short-circuiting the
//       enumeration that includes 0x80006900).
//
// More structurally: figure out which Symbian Close()/decrement
// path is missing, or which object lifecycle hook isn't firing.
//
// 2026-05-10 (cont., even later): COMPLETE destroy chain mapped
// ---------------------------------------------------------------------------
// PSION_S5_ASSERT_TRACE enhanced to dump PC history at the failing
// assert. The destruction chain from outermost to innermost is:
//
//   1. (Some upstream caller — TBD) BL FUN_500095F8
//        @ 0x500095F8: STMFD {R4,LR}; SUB SP,#0x50; MOV R4,R0;
//                       ADD R0,SP,#4; MOV R1,SP; BL FUN_5004C360
//   2. FUN_500095F8 calls FUN_5004C360 (the generic destroy
//      iterator @ 0x5004C360) with R0/R1 = stack-local buffers.
//   3. FUN_5004C360 reads R0 = *(R4+0x48) (object pointer);
//      loads R3 = *R0 (vtable); MOV LR, PC; LDR PC, [R3, #8]
//   4. Vtable+8 of the object's class (vtable @ 0x500283A8 in our
//      trace) = 0x50027114, which is `B 0x500111C4` (a thunk
//      branching to the actual destroyer FUN_500111C4).
//   5. FUN_500111C4 @ 0x500111C4:
//        - overwrites *(R4+0) = 0x500281D0 (sets "destroying" vtable)
//        - if *(R4+0xD8) != 0, calls helper on R4+0xD8
//        - R0 = *(R4+0x44); if R0 != 0, indirect-calls
//                            R0->vtable[+8] with R1=3 (recursive
//                            destroy of sub-object)
//        - tail-call B 0x50009140 (NThread destructor)
//   6. FUN_50009140 @ 0x50009140 (= vtable[+8] of NThread vtable
//      0x50028798):
//        - sets *(R4+0) = 0x50028798 (restores NThread vtable —
//          presumably redundant since it was just overwritten by
//          FUN_500111C4, but the disassembly explicitly does it)
//        - if *(R4+0xB0) != 0, calls helper on R4+0xB0
//        - calls helper(*(R4+0x18)) and helper(*(R4+0xA4))
//        - restores callee-saved regs + LR (gets back original
//          caller's BL-return)
//        - tail-call B 0x500261E4 (trampoline)
//   7. Trampoline @ 0x500261E4:
//        LDR R12, [PC, #0]; LDR PC, [R12]
//        loads R12 = *(0x500261EC) = 0x5040007C, then
//        PC = *(0x5040007C) = 0x5002DE9C (FUN_5002DE9C).
//   8. FUN_5002DE9C @ 0x5002DE9C (= base destructor):
//        - STMFD {R4,R5,LR}; MOV R4,R0; MOV R5,R1
//        - LDR R3, [PC, #0x48]; STR R3, [R4, #0] (sets vtable
//          to 0x50050900, the base-class "destroyed" marker)
//        - LDR R3, [R4, #4]; CMP R3, #0; if non-zero MOV R0, #0x21
//          (= 33); BL panic_helper_33 (panics "E32USER-CBase 33")
//
// So the kernel IS treating *(this+4) as the destructor assertion
// (iAccessCount-equivalent). Real EPOC R1 would have *(this+4) = 0
// by destructor entry — typically because CObject::Close()
// decrements iAccessCount and only invokes the destructor when
// the count reaches 0. Our boot reaches the destructor with
// *(this+4) still 1.
//
// Note: the vtable in *(this+0) at the assert hook is 0x50028798
// (NThread). FUN_500111C4 was supposed to overwrite this to
// 0x500281D0, but our hook fires BEFORE the STR R3, [R4, #0] of
// FUN_5002DE9C. Yet 0x50028798 is shown, not 0x500281D0. That
// implies FUN_50009140 explicitly RE-WROTE 0x50028798 between
// FUN_500111C4's overwrite and the assert hook — which matches
// FUN_50009140's `STR R3, [R4, #0]` at PC=0x50009150 (loading
// 0x50028798 from *0x50009188).
//
// HYPOTHESIS-TEST: PSION_S5_INJECT="pokepc:0x5002DE9C:0x80006904:0"
// (force *(this+4) = 0 right before assert runs). RESULT: 5 pokes
// fire per cycle (4 successful + 1 failing) and the panic is
// suppressed — but the boot REGRESSES to unique_pcs=0 and User32
// is never reached. So just suppressing the assert isn't the fix;
// the destructive cycle IS part of the kernel's expected control
// flow. The real fix must be one of:
//   (a) prevent FUN_500095F8 from being called at this stage
//   (b) ensure Close() runs upstream to decrement iAccessCount
//   (c) find what triggers the destruction (an event/timer/SWI)
//       and either suppress it or supply what it expects
//
// NEXT: find who calls FUN_500095F8 (one frame up from FUN_5004C360),
// because that's where the destruction decision is made. Either:
//   - hook PC=0x500095F8 entry, log R0 (the object) and LR (caller),
//     AND walk the stack one more frame to find HIS caller
//   - search the decompile for any function that calls FUN_500095F8
//     (search 'FUN_500095F8' or '0x500095F8' references in
//     reference/5_decompiled/series5_v1.01(144)_eng.bin.c)
//
// 2026-05-10 (final for today): FUN_500095F8 is a TRAP wrapper, not destroy
// ---------------------------------------------------------------------------
// Looked at FUN_500095F8 in the decompile:
//   void FUN_500095F8(int param_1) {
//     int local_58;
//     undefined1 auStack_54[76];
//     iVar1 = thunk_FUN_5004C350(auStack_54, &local_58);
//     if (iVar1 == 0) {
//       FUN_50009598(param_1);             // ← CREATE+ATTACH a session
//       thunk_FUN_50043e38();
//     }
//     if (local_58 != 0) {                  // a leave occurred
//       FUN_5000402c(*(param_1+0x2c));      // close session
//       *(param_1+0x2c) = 0;
//       thunk_FUN_5004c394(local_58);
//     }
//   }
//
// FUN_50009598 is a CONSTRUCTOR — allocates a 0x30-byte object,
// creates a session, attaches it to param_1's iSession (+0x2C),
// and registers it.
//
// FUN_5004C350 is the Symbian TRAP setup (`TRAP(local_58, ...)`):
//   5004c350: MOV R2, #0
//   5004c354: STR R2, [R1]              ; *R1 = 0 (zero error code)
//   5004c358: STR R1, [R0, #0x44]       ; jmp_buf->iErrPtr = &error
//   5004c35c: STM R0, {R4-R11, SP, LR}  ; setjmp (save callee-saved regs)
//   5004c360: STMFD SP!, {R4, LR}       ; FALL-THROUGH into FUN_5004C360
//   5004c364: MOV R4, R0                ; R4 = jmp_buf
//   5004c368: BL FUN_5004BD30 area      ; SWI 0xC00072 (TRAP install)
//   5004c36c: LDR R0, [R4, #0x48]       ; load *(jmp_buf+0x48)
//   5004c370: CMP R0, #0
//   5004c374: LDMEQFD SP!, {R4, PC}     ; if 0, return — normal path
//   5004c378: LDR R3, [R0]              ; else load vtable
//   5004c37c: MOV LR, PC
//   5004c380: LDR PC, [R3, #8]          ; ← indirect dispatch
//
// So FUN_5004C350 and FUN_5004C360 form a single combined "register
// trap frame + check for pending cleanup" function. The SWI 0xC00072
// registers the trap frame with the kernel and (apparently) sets
// *(jmp_buf+0x48) to a "pending cleanup object" if one exists.
//
// In our boot: at each cycle, FUN_500095F8 is called with param_1 =
// some thread/proc descriptor (to attach a session via FUN_50009598).
// The SWI 0xC00072 inside FUN_5004C350 sets *(jmp_buf+0x48) =
// 0x80006900 — meaning the kernel believes thread 0x80006900 needs
// cleanup ("pending destroy"). The dispatch then runs the NThread
// destruction chain on 0x80006900, which fails the iAccessCount
// assert.
//
// So the QUESTION TO ANSWER is: why does the kernel believe thread
// 0x80006900 has a pending destroy at this boot stage? Possibilities:
//   - 0x80006900 was the prior boot iteration's thread; its slot
//     hasn't been cleared from the kernel's pending-cleanup list
//   - The thread was created but never properly registered, leaving
//     it in a half-state where SWI 0xC00072 thinks it needs cleanup
//   - An emulator-side discrepancy makes the kernel see a stale
//     reference
//
// NEXT investigation actions:
//   (a) Hook the SWI 0xC00072 dispatch handler (find its address via
//       the FAST exec table: *(0x5002795C + 0x72*4) = *(0x50027B24))
//       to log what conditions cause *(jmp_buf+0x48) to be set
//   (b) Hook PC=0x5004C36C (right after the SWI returns) and log
//       *(R4+0x48) only when it's 0x80006900, then walk back through
//       the SWI handler's state to find where the value comes from
//   (c) Read kernel state for "pending cleanup list" — likely a
//       global pointer somewhere around 0x800000xx that the SWI
//       checks.
//   (d) Look for who calls FUN_500095F8 — that's the entry point
//       for this trap-wrapped session creation. The caller decided
//       to create the session; understanding why it's called at
//       this boot stage may reveal the broader bug.
//
// (For session creation to ever fail with a pending cleanup of an
// unrelated NThread suggests the kernel's cleanup tracking has
// dirty state from a previous (or in-progress) thread lifecycle
// that should have been resolved before this point.)
//
// 2026-05-11: CRITICAL — destructive cycle is NOT a kernel software
// bug — it is the boot thread's UND-mode SVC stack OVERLAPPING the
// boot thread's NThread structure
// ---------------------------------------------------------------------------
// All earlier analysis assumed the kernel was deliberately writing
// NThread pointers (like 0x80006900) to *(boot_thread+0x40) as a
// "register cleanup" operation. PSION_WRITE_WATCH=0x80003D10 (=
// boot_thread + 0x40) showed many writers per cycle:
//
//   PC=0x5002DF60  e92d4070  STMFD SP!, {R4, R5, R6, LR}
//   PC=0x5002E92C  e92d4070  STMFD SP!, {R4, R5, R6, LR}
//   PC=0x5003968C  e92d4070  STMFD SP!, {R4, R5, R6, LR}
//   PC=0x500125E8  e92d41f0  STMFD SP!, {R4-R8, LR}
//
// ALL of these are FUNCTION PROLOGUES doing STMFD on the kernel
// UND-mode stack. The "write value" reported by write-watch is
// the LOWEST register pushed (R4) which lands at the lowest
// pushed address (= new SP, = virt 0x80003D10 for these frames).
//
// These are NOT deliberate field writes — they're STACK PUSHES
// that land at virt 0x80003D10 because the boot thread's UND-mode
// stack pointer happens to descend into the boot thread's NThread
// structure.
//
// CONCRETE: at the destructive-cycle moment, R4 in the calling
// function = 0x80006900 (the newly-created NThread). STMFD pushes
// R4 to [SP], where SP = 0x80003D10. So *(boot_thread + 0x40)
// becomes 0x80006900 — but as STACK GARBAGE, not as kernel
// "pending cleanup" state. When the next SWI 0x72 fires, the
// handler at 0x5000CD6C does
//   LDR R3, [iCurrentThread, #0x40]   ; reads 0x80006900
//   STR R3, [new_trap_frame, #0x48]   ; passes it on
// and the subsequent destroy-iterator in FUN_5004C360 dispatches
// vtable[+8] on 0x80006900 — destroying it.
//
// So the kernel correctly reads what's in *(thread + 0x40), which
// is the "iPendingDelete" field. Our emulator's memory layout is
// putting the SVC stack INSIDE the thread structure, corrupting
// this field on every kernel function call.
//
// NThread structure size in EPOC R1 (from ThreadInit at FUN_500090E0
// initializing fields up to +0xB8 with size 0xB0): at least 0x168
// bytes. Boot thread at 0x80003CD0 → struct ends at ~0x80003E38.
//
// SVC stack pushes observed in range 0x80003D10 - 0x80003DBC, all
// well within the struct.
//
// FIX DIRECTION: trace where the kernel initialises SP_und for the
// boot thread (during early init around PC=0x50000400-0x50001000).
// On real EPOC R1 hardware the stack must be allocated at a virt
// address that does NOT overlap the thread struct. Our emulator's
// memory init / RAM layout must be causing the kernel to compute
// a wrong SP_und value, OR there's a missing piece of the kernel's
// stack-allocation flow that depends on hardware state we don't
// model correctly.
//
// One concrete next experiment: capture SP_und at the FIRST
// transition to iCurrentThread=0x80003CD0 (cycle 2665546, PC=
// 0x50010EA0). That SP value is the boot thread's UND-stack top
// as set up by the kernel. If it's just above the thread struct
// (e.g., 0x80003E00), the kernel's stack-allocation computes a
// wrong base. If it's somewhere completely different
// (e.g., 0x80100000), then context-switch save/restore is
// corrupting it later.
//
// 2026-05-10 (cont., v3): traced the writer of *(jmp_buf+0x48)
// ---------------------------------------------------------------------------
// The original assumption was that SWI 0xC00072 handler at 0x5000CD54
// writes *(R4+0x48) by copying *(iCurrentThread+0x40). PSION_S5_TRAP_SWI_TRACE
// confirmed that SWI handler IS called heavily, BUT for the kernel-mode
// failing destroy path *(iCurrentThread+0x40) = 0 — so the SWI handler
// just zeros R4+0x48. The 0x80006900 value comes from a DIFFERENT
// writer that runs BETWEEN the SWI 0x72 return and the LDR R0, [R4, #0x48]
// at PC=0x5004C36C.
//
// PSION_WRITE_WATCH=0x80003DB4 (the specific failing R4+0x48 address)
// captured the writer per cycle:
//
//   PC=0x5002E784 (inside FUN_5002E778): STR R1, [SP, #0]
//   LR=0x500098B4 (BL site at PC=0x500098B0)
//   R1 = 0x80006900 (the NThread)
//   SP = 0x80003DB4 at the moment of the store
//
// FUN_5002E778's prologue (STMFD {R4,LR}; SUB SP,#4) brings SP to
// 0x80003DB4. The function writes R1 (caller-supplied NThread ptr)
// to *(SP+0) = 0x80003DB4. The function takes args (R0, R1) where
// R1 is the NThread to register for cleanup.
//
// Caller is at PC=0x500098B0 (BL via trampoline 0x50026178 →
// *(0x50400300) = 0x5002E778). Just before the BL:
//
//   LDR R3, [PC, #0x68]  (R3 = *0x50009914 = 0x80100008,
//                          a kernel global pointer)
//   LDR R1, [R5]          (R1 = *R5 — the value 0x80006900)
//   LDR R0, [R3]          (R0 = *(0x80100008))
//   BL trampoline → FUN_5002E778
//
// So the failing destroy chain originates with: SOMETHING set
// *(R5) = 0x80006900, and the calling function then dereferences
// R5 to get the NThread ptr and passes it to FUN_5002E778, which
// (combined with the SWI 0x72 setup) triggers the destroy iterator.
//
// 0x80100008 is the kernel global at virt 0x80100008 — likely the
// "current process" or a similar singleton. The function pattern
// suggests this is part of a SCHEDULER/CONTEXT-SWITCH path.
//
// 2026-05-10 (cont., v4): FUN_500097AC IS CreateThread / CreateLP
// ---------------------------------------------------------------------------
// Looked up FUN_500097AC (= function entry containing PC=0x500098B0,
// the BL that calls FUN_5002E778). Decompile:
//
//   void FUN_500097AC(*param_1, p2, p3, p4, p5, ...p11, *param_12, p13) {
//     uVar1 = FUN_50011198(2);             // alloc 0x?? bytes (size code 2)
//     *param_12 = uVar1;                    // store new obj ptr
//     if (param_13 == 1) {
//       FUN_5000925C(uVar1, p3, p2, 2, p5..p11, 1);    // init
//       thunk_FUN_5002E778(*DAT_50009858, *param_12);  // queue#1
//       FUN_500095F8(*param_12);             // TRAP+session-create
//       uVar1 = FUN_50003D54(*param_12, p3);
//     } else {
//       FUN_5000925C(*param_12, p2, p2, 2, p5..p11, p13);
//       thunk_FUN_5002E778(*DAT_50009914, *param_12);  // queue#2 (= queue at
//                                                       //  *0x80100008)
//       FUN_500095F8(*param_12);
//       uVar1 = FUN_50003D38(*param_12, *DAT_50009918);
//     }
//     *param_1 = uVar1;
//     (**(code **)(*(int *)*param_12 + 0xc))();   // vtable[+0xc] dispatch
//     if (param_4 != 0) {
//       FUN_50003D8C(p3, p4, *param_12, auStack_24, 1);
//     }
//   }
//
// FUN_500097AC is the **THREAD/object CREATION function**:
//   1. Allocate object (FUN_50011198)
//   2. Initialize (FUN_5000925C — sets vtable + iAccessCount = 1)
//   3. Register in the kernel object queue (thunk_FUN_5002E778)
//   4. Wrap in TRAP and create+attach session (FUN_500095F8)
//   5. Call vtable[+0xc] (some post-creation hook)
//
// The "queue" at *(0x80100008) (= *DAT_50009914) is where the kernel
// tracks all live objects of this class. Every newly-created thread
// (or thread-like object) is added here.
//
// So our destructive cycle is: a new NThread is created and added
// to the queue (with iAccessCount=1), then somewhere downstream a
// queue-walk destroy iterator (FUN_5004C360 area) walks the queue
// and tries to destroy each entry. For 0x80006900, no Close() ever
// ran to decrement iAccessCount, so the destructor's
// "iAccessCount==0" assert fails.
//
// REAL ROOT-CAUSE HYPOTHESES (any of these could be the structural
// bug in our emulator):
//
//   (i)   The destroy iteration is supposed to skip live objects
//         (e.g., check iAccessCount > 0 before attempting destroy).
//         Our boot's iteration code is wrong and destroys live
//         objects.
//   (ii)  The TRAP wrap inside FUN_500095F8 is leaving (User::Leave)
//         due to some downstream failure (e.g., session-create
//         can't allocate). On Leave, the kernel runs cleanup that
//         destroys the half-constructed thread. But because the
//         constructor already ran and set iAccessCount=1, the
//         cleanup hits the assert.
//   (iii) Our emulator's behaviour during session creation triggers
//         a Leave that wouldn't happen on real hardware (e.g., a
//         resource is "missing" in our emulation that's present
//         on real Series 5).
//
// CONCRETE NEXT STEPS for the next investigator:
//   (a) Hook PC=0x500097AC entry to log when threads are created.
//       Compare against the 5 ThreadInit'd threads (per
//       series5.h:4090-4094). Specifically: does FUN_500097AC
//       fire to create thread 0x80006900, and is that the one
//       that subsequently fails session-create?
//   (b) Hook FUN_500095F8's `if (local_58 != 0)` cleanup path
//       (after the TRAP returns with non-zero Leave code). If
//       this fires for thread 0x80006900, then we're seeing the
//       hypothesis (ii) scenario: session-create leaves and the
//       thread is being destroyed as cleanup.
//   (c) If (b) is true: find what makes FUN_50009598 (the actual
//       session-create that runs inside the TRAP) call User::Leave.
//       The likely culprit is a kernel resource we're not modelling
//       correctly (e.g., a descriptor from FUN_50039764 fails, or
//       the trap-frame chain doesn't have the expected state).
//
// 2026-05-11: with PSION_S5_NO_SHADOW=1, boot reaches NEW blocker
// -------------------------------------------------------------------------
// The bootstrap-NThread shadow-read intercept at arm710.cpp:3345 was
// masking real kernel state — disabling it via PSION_S5_NO_SHADOW=1
// reveals that boot progresses MUCH further than previously documented.
//
// Per `PSION_S5_NO_SHADOW=1 PSION_S5_DISP_TRACE=20 PSION_SWI_TRACE=8000`
// (15 sim sec), the user-mode F32 IPC sequence at cycles 11.14M-11.83M
// completes ALL of:
//   sel=0x1C (RProcess::Create-equivalent, thread 0x80005EAC, session ok)
//   sel=0x1D (similar)
//   sel=0x15, 0x19, 0x1B, 0x27, 0x30 (F32 ops, thread 0x800063D0, sess ok)
//   sel=0x15 (alt args), 0x10, 0x27, 0x30, 0x16 (continuing, ok)
//
// Then at cycle 11833004 a NEW panic fires:
//   `[disp] cpsr=1b sel=R0=0x14 R1=0x80006ed8 R2=0x0F R3=0x0E lr=0x50019600`
//   `[disp-ctx] iCurrentThread=0x80005174 session(+0x2C)=0x00000000`
//   `[panic-info] text='KERN-NO-SESSION' reason=20`
//
// The dispatcher FUN_500096CC's first action (line 13902-13921 in the
// Ghidra decompile) is:
//   iVar2 = FUN_5000ff40();           // = iCurrentThread
//   iVar2 = *(int *)(iVar2 + 0x2c);   // = iCurrentThread->iSession
//   if (iVar2 == 0) {
//       // ★ if selector is not 0x2F (panic), the dispatcher itself
//       //   loads "KERN-NO-SESSION" descriptor and BL FUN_5000FF68
//       //   with reason = selector.
//       FUN_5000ff68(...);
//       return -2;
//   }
// So "KERN-NO-SESSION reason=20" literally means "slow-exec dispatcher
// was invoked with selector 0x14 on a thread whose +0x2C is null".
//
// The culprit SWI is NOT 0xC00076 (slow-exec). It's `SWI 0xC0002A` at
// PC=0x5004BC24, lr=0x5003DDFC (= inside FUN_5003DDB8, a heap reallocator).
// FUN_5003DDB8 calls FUN_5003A79C(heap+0x1C) at line 80952 — this is
// `Kern::FastLockSlowAcquire`-equivalent: an inline atomic-CAS attempt
// (line 73898-73902) that falls back to SWI 0xC0002A on contention.
// Ghidra cannot recover the indirect jumptable so the decompile lists
// ALL the related SWIs in sequence (lines 73904-73928) — only one actually
// fires per call. SWI 0xC0002A drops into the kernel SWI handler
// FUN_500193F8 (line 29217+) which tail-dispatches via vtable[0x3C] at
// PC=0x500195E8 — return point is lr=0x50019600. The dispatch lands in
// FUN_500096CC where the session check trips.
//
// Why thread 0x80005174 has no session:
//   FUN_50009598 (PC=0x50009598) is the ONLY writer of *(thread+0x2C)
//   that sets it to a non-null DSession (lines 13849-13866 of decompile):
//     iVar1 = thunk_FUN_50030734(0x30, 0);   // alloc 48-byte DSession
//     uVar2 = FUN_50002750();                // init
//     *(undefined4 *)(param_1 + 0x2c) = uVar2;
//     FUN_50002808(uVar2, *DAT_500095F0, param_1, 1);
//     FUN_50002B1C(*(undefined4 *)(param_1 + 0x2c), param_1);
//     (*(code *)*DAT_50026180)(*DAT_500095F4, *(thread+0x2c));
//   FUN_50009598 is called ONCE, from FUN_500095F8 (line 13880) — the
//   TRAP wrapper that runs during PROCESS CREATION. Thread 0x80005174
//   is the SUPERVISOR / first kernel server thread (priority 0, see
//   series5.h:4671) — it isn't a user process so FUN_500095F8 was
//   never invoked for it, and its +0x2C remains 0.
//
// In real EPOC R1, the Supervisor doesn't normally hit FUN_5003A79C's
// slow path. Either:
//   (a) Our heap state forces contention that wouldn't exist on real HW
//       (the inline atomic-CAS check at FUN_5004D1E4 returns "contended"
//       when real HW would return "free"), or
//   (b) FUN_5003DDB8 (heap realloc) shouldn't be running on the
//       Supervisor at all — our scheduler routed an alloc to the wrong
//       thread, or
//   (c) The kernel SWI 0xC0002A path SHOULD bypass the session-check
//       gate for kernel-mode callers and we're routing it wrong.
//
// Auxiliary findings:
//   * A SECOND panic fires at cycle 23.87M from user-mode EFile context:
//     `[panic-callsite] cpsr=10 sp=0x501960 thunkRet=0x5003BF34
//      helperRet=0x500306C0 romCaller=0x5002A6D4 reason=46`
//     This is downstream of the KERN-NO-SESSION panic — the Supervisor's
//     death causes a cascade that EFile.exe (running on a different
//     thread) eventually trips. Don't chase this until the primary
//     KERN-NO-SESSION panic is fixed.
//   * The disp-ctx trace knob added to PSION_S5_DISP_TRACE dumps
//     iCurrentThread + session(+0x2C) on every FUN_500096CC entry —
//     this is the canonical diagnostic for the session-gate panic.
//
// Next investigation steps:
//   1. Confirm via insn-trace: at cycle 11832920, dump the inline
//      atomic-CAS site (FUN_5004D1E4) to verify the lock IS contended
//      (vs uncontended). If uncontended, our emulator is mis-reporting.
//   2. Identify what R1=0x80006ED8 is. If it's a known kernel allocator
//      object, we can trace what thread allocated it and whether the
//      contention is plausible.
//   3. Walk a real-EPOC-R1 reference kernel boot to see whether
//      Supervisor ever takes the SWI 0xC0002A slow path. If it doesn't
//      on real HW, our heap state is off; if it does, we need to add
//      a kernel-mode bypass in FUN_500096CC's session check (which is
//      a kernel-side emulation, not a hardware emulation issue).
//
// 2026-05-11 (cont.): contention is REAL, lock state is wrong
// -------------------------------------------------------------------------
// PSION_S5_LOCK_TRACE added in arm710.cpp. Hooks both PC=0x5004BDA0
// (fast SWI 0x8E wrapper) and PC=0x5004BC20 (slow SWI 0xC0002A
// wrapper) and dumps R0 + *(R0) + *(R0-4) + iCurrentThread.
//
// Hypothesis A FALSIFIED. The atomic-CAS reports correctly.
//
//   At cycle 11832840 (the failing acquire):
//     R0=0x80005194 *(R0)=0x0000000F *(R0-4)=0x00000014 cur=0x80005174
//   At cycle 11832920 (slow-path SWI):
//     R0=0x14 *(R0)=0xE59FF018 *(R0-4)=0xE59FF018 cur=0x80005174
//
// The lock COUNT WORD at virt 0x80005194 contains 0x0F (15), not the
// expected free-state value of 1. (500+ other fast-lock acquires in
// the same trace all see *(R0)=0x00000001 and succeed.) SWI 0x8E's
// handler at PC=0x5000CF84 is just:
//   LDR R2, [R0]      ; R2 = *R0
//   SUB R3, R2, #1    ; R3 = R2 - 1
//   STR R3, [R0]      ; *R0 = R3
//   MOV R0, R2        ; return old value
// → returns 15, FUN_5003A79C tests R0==1, fails, falls through to
// SWI 0xC0002A on the slow path, which trips the session check.
//
// SWI 0x6C's handler (PC=0x500031F0) just returns the current
// thread's "heap pointer" as `*(iCurrentThread + 0x38)`:
//   STMFD SP!, {LR}
//   BL FUN_5000FF40            ; R0 = iCurrentThread
//   LDR R0, [R0, #0x38]        ; R0 = heap pointer
//   LDMFD SP!, {PC}
//
// For the supervisor thread 0x80005174, *(thread+0x38) reads back as
// 0x80005174 itself — a self-referential "heap" pointer. Either
// real EPOC R1's NThread layout embeds the heap (so thread base ==
// heap base by design) and we're initialising the lock word at
// +0x20 incorrectly, OR the +0x38 field SHOULD point to a real
// per-thread heap elsewhere and we're setting it to self by mistake.
//
// SWI dispatch table located at virt 0x5002795C. The handler addresses
// for the SWIs in this investigation:
//   0x6C → 0x500031F0   (GetHeap, returns iCurrentThread->+0x38)
//   0x8E → 0x5000CF84   (fast atomic decrement on R0)
//   0x8D → 0x5000CF70   (companion: atomic increment, used for release)
//   0xC0002A → 0x5000B8E8 (slow-path: probably FastLockWait)
//   0xC00076 → 0x5000CE28 (slow-exec, calls FUN_500096CC via vtable)
//
// Next investigation steps (refined):
//   1. Inspect the full Supervisor thread struct at 0x80005174
//      (offsets +0x14, +0x18, +0x1C, +0x20, +0x24, +0x38) and
//      compare against (a) a known-good thread (e.g., 0x800063D0)
//      and (b) the EPOC R1 NThread layout in the linux-7110 source
//      and any leaked Symbian R1 headers.
//   2. Locate where thread+0x38 is written for the Supervisor
//      during boot. If it's only ever set to "thread+0", that's
//      the bug. If it's set to a real heap address and SOMETHING
//      overwrites the heap's lock word, find what writes 0x14/0x0F
//      to virt 0x80005190/0x80005194.
//   3. If the design is heap-at-thread-base, find what real EPOC
//      writes to thread+0x1C/+0x20 immediately after thread creation
//      (it must be a free fast-lock init).
//
// 2026-05-11 (cont. 2): the heap-pointer IS correct — IRQ-handler
//                       reentry is calling heap ops with wrong R0
// ---------------------------------------------------------------------------
// New trace evidence:
//   PSION_S5_HEAP_TRACE shows that at cycle 11831922 (just before the
//   panic) SWI 0x6C's handler entry reads *(iCurrentThread+0x38) =
//   0x80004000 (the correct kernel heap). PSION_S5_HEAP_TRACE_ALL
//   shows the handler executes only as far as PC=0x500031F4 (the BL
//   FUN_5000FF40) — it NEVER reaches PC=0x500031F8 (LDR R0, [R0,#0x38])
//   nor PC=0x500031FC (the LDMFD SP!, {PC} return).
//
//   PSION_S5_HEAP_REALLOC_TRACE shows that at cycle 11832794 (870
//   cycles AFTER the truncated SWI 0x6C handler entry), FUN_5003DDB8
//   is entered with R0=0x80005174 (the SUPERVISOR THREAD itself, NOT
//   the kernel heap 0x80004000). The lr=0x5003DF74 confirms entry
//   from FUN_5003DF6C — the only caller, which is itself only called
//   from PC=0x5003A218 (FUN_5003A1FC's tail-B). And FUN_5003A1FC's R0
//   on exit IS the SWI 0x6C return value.
//
// So between SWI 0x6C entry (11831922) and FUN_5003DDB8 entry
// (11832794) something interrupted the kernel handler and the
// handler we eventually reached was running on a DIFFERENT call chain
// where R0 = 0x80005174 (= the current thread / Supervisor).
//
// The PC history captured at the failing SWI 0x8E shows the IRQ
// DISPATCH LOOP FUN_5001A770 (line 29969 of the decompile — reads
// CL-PS7110 INTSR1/INTMR1 registers in a tight loop) running BEFORE
// reaching FUN_500193F8 and the chain to FUN_5003DDB8. So:
//
//   1. SWI 0x6C handler entered at cycle 11831922
//   2. After BL FUN_5000FF40 at PC=0x500031F4, an IRQ fires
//   3. Control diverts to IRQ vector → kernel IRQ entry → FUN_500193F8
//      (which we'd previously assumed was the SWI handler; the
//      SUB LR, LR, #4 prologue means it handles IRQ-style entries)
//   4. FUN_5001A770 runs the IRQ dispatch loop
//   5. The IRQ handler that's dispatched eventually calls FUN_5003DDB8
//      with R0 = 0x80005174 (NOT a real heap pointer)
//   6. FUN_5003DDB8 → FUN_5003A79C → SWI 0x8E (fast-lock acquire)
//   7. The atomic decrement reads *(0x80005194)=0x0F (a flags word,
//      NOT a fast-lock count) → returns non-1 → slow path SWI 0xC0002A
//   8. Slow-exec dispatcher panics on missing session
//
// Open question: WHO passes R0=0x80005174 to FUN_5003DDB8 inside the
// IRQ handler chain? Candidates:
//   (a) The IRQ handler thinks "current thread address" is the heap
//       (probably grabbing iCurrentThread directly via a SWI variant
//       different from SWI 0x6C — maybe a kernel-internal fast SWI
//       that returns iCurrentThread instead of iCurrentThread->iHeap)
//   (b) Our emulator is incorrectly delivering an IRQ at a moment
//       when real EPOC would have it masked, and the resulting code
//       path is genuinely buggy / unintended for this state
//   (c) The kernel's IRQ handler globals (e.g., DAT_500194D0 = some
//       per-CPU pointer at virt 0x8010061C area) point to the wrong
//       object — we need to compare with what real EPOC has there
//
// Next: trace what's actually happening in FUN_5001A770's iteration
// — specifically which IRQ source fired (one of bits in INTSR1) and
// which handler from the IRQ table is being called. The handler PC
// will tell us which kernel subsystem is making the bogus
// FUN_5003DDB8(thread,...) call.
//
// 2026-05-11 RESOLVED — two compounding emulator bugs eliminated
// -------------------------------------------------------------------------
// PSION_INSN_TRACE_CYC walked the IRQ entry instruction-by-instruction
// and revealed two concrete defects in arm710.cpp:
//
//   (a) `raiseException()` did NOT set the CPSR I-bit. Real ARM
//       hardware masks IRQs on entry to every exception (SWI / Undef /
//       Abort / IRQ / FIQ — FIQ additionally masks FIQ). Our
//       implementation only set the I-bit at the requestIRQ /
//       requestFIQ call site, leaving SWI/Undef/Abort exceptions
//       running with IRQs still enabled. Series 5 hit this because
//       Timer 2 fires every ~4ms; one tick landed inside the brief
//       SWI 0x6C handler at FUN_500031F0 between `BL FUN_5000FF40`
//       and the `LDR R0, [R0, #0x38]` that follows.
//
//   (b) The Series-5 HAL-root-overlap workaround at arm710.cpp:4047
//       was silently dropping ALL writes in the 256-byte range
//       0x80105700..0x801057FF whose value fell in [0x50003000,
//       0x50003500). When the IRQ handler at PC=0x5001967C did its
//       prologue `STMFD SP!, {R0-R3, R12, LR}^` with LR_irq=0x500031F8
//       (a legitimate return address into FUN_500031F0), the hack
//       dropped the LR push to virt 0x8010578C. The IRQ LDMFD at
//       PC=0x50019888 then popped the SVC handler's PRE-EXISTING
//       saved LR (0x500194FC, written by the SWI 0x6C handler's
//       STMFD at 0x500031F0 a few cycles earlier) instead of the
//       expected 0x500031F8. That bypassed the `LDR R0, [R0+0x38]`
//       inside FUN_500031F0 and resumed at the kernel SWI handler
//       tail with R0 = iCurrentThread (= 0x80005174) — the value
//       FUN_5000FF40 had loaded just before the IRQ — which then
//       propagated to FUN_5003DDB8 as if it were a heap pointer.
//
// Fix (2026-05-11):
//   - Set CPSR_IRQDisable inside raiseException for ALL exception
//     entries (and CPSR_FIQDisable for FIQ32 specifically). Drop
//     the redundant sets in requestIRQ / requestFIQ.
//   - Narrow the HAL-vtable hack to the actual vtable slot at virt
//     0x80105790 only. The wider 256-byte mask was clobbering
//     legitimate kernel pushes that share the same SVC/IRQ stack
//     region.
//
// Result: KERN-NO-SESSION panic gone. Boot now reaches 30+ distinct
// F32 IPC selectors in user mode (vs ~10 selectors before, ending
// at panic) with 0 panics / 0 aborts. All-device boot test passes.
//
// The underlying SP_irq == SP_svc kernel stack collision (banked
// SPs both initialised to 0x80105790) is REAL but harmless once
// IRQs are correctly masked across the SWI handler — the IRQ never
// preempts the brief critical section, so the stacks don't overlap
// in practice.
//
// 2026-05-11 (cont. 3 — HISTORICAL): IRQ source identified as Timer Counter 2
// ---------------------------------------------------------------------------
// PSION_S5_IRQ_TRACE confirms the IRQ that fires in supervisor context:
//   id=0x09 (CL-PS7110 INTSR1 bit 9 = TC2OI = Timer Counter 2 overflow)
//   handler object at virt 0x80105CC4, vtable[2] entry = 0x5001A904
//
// Chain of dispatch (all in ROM, all decoded from disasm):
//   Timer 2 fires → kernel IRQ entry at FUN_500193F8 → FUN_5001A770
//     (IRQ dispatch loop reading INTSR1/INTMR1) → FUN_50016FD0
//     (IRQ-by-id dispatcher) → vtable[2]=FUN_5001A904 (Timer 2 trampoline)
//     → FUN_500173AC (real timer handler) → iterates kernel queue at
//     *(0x80100640) calling vtable functions via *(0x50400094)
//     = FUN_5003C694 (polymorphic object dispatch)
//   FUN_5003C694 calls *(R0) on each object — for our object 0x80100D64
//     that's FUN_5001DA6C (the actual Timer 2 callback)
//   FUN_5001DA6C decrements counter at *(0x80100F68); when 0, processes
//     the kernel object at *(0x80100C98).
//
// At the failing cycle 11832202 (PSION_S5_TIMER2_TRACE), the counter is
// still > 0 so the deep dispatch isn't taken. Yet some path still leads
// to FUN_5003DDB8(0x80005174,...) at cycle 11832794.
//
// PSION_S5_FRAME_TRACE shows that at PC=0x500031F4 (right before the
// failing IRQ) the SWI exception frame at iCurrentThread+0xE4 holds
// savedR0=0x80005240, savedPC=0x500073F0 — values from a PRIOR
// exception, not the current SWI 0x6C invocation. So either:
//   * SWIs use a different frame than IRQs (separate per-mode contexts
//     in the kernel), OR
//   * The kernel reuses one context but we're reading the wrong slot.
//
// The disasm at 0x50019658-0x50019674 shows the IRQ handler uses
// `LDR R12, [PC, #-0x18C]; LDR R12, [R12]; ADD R12, #0xE4` to compute
// its frame pointer — same pattern as the SWI handler at 0x500193F8
// but loaded from a DIFFERENT literal pool entry. Find the IRQ context
// vs the SWI context: each is a separate kernel global.
//
// Final actionable next step:
//   1. Confirm the kernel actually uses the IRQ exit path to "skip"
//      the SWI 0x6C and resume at PC=0x5003A20C with R0 = some value
//      written by the IRQ handler. If so, the IRQ handler is writing
//      0x80005174 into the saved-R0 frame slot — find what writes it.
//   2. OR: confirm there is a DIFFERENT call-site to PC=0x5003A20C we
//      missed (Ghidra's static scan only found FUN_5003A1FC's BL).
//   3. The KEY missing instrumentation: a watchpoint on the SWI frame's
//      saved-R0 slot (currently we don't know its location — the SWI
//      handler uses a different kernel-global context pointer than the
//      IRQ handler).

class Emulator : public CLPS7110::Emulator {
public:
    Emulator() {
        // ARM710 banking is correct; the HAL-root / SVC-stack collision is
        // intrinsic to the Series 5 EPOC R1 kernel's memory layout for 8 MB
        // RAM. Enable the HAL-fix path to drop vtable-poisoning writes and
        // short-circuit the pen-click HAL query whose completion status
        // lives in the same clobbered region.
        cpu.setSeries5HalFix(true);
        // PSION_S5_T_VERSION=1 promotes the core to ARM710T (enables
        // halfword load/store + long multiply). Off by default — Series 5
        // hardware is ARM710A per the CL-PS7110 product bulletin. Used to
        // test whether Word/Agenda/Data corruption stems from misdecoded
        // halfword instructions.
        if (const char *e = std::getenv("PSION_S5_T_VERSION"); e && e[0] == '1') {
            cpu.setTVersionEnabled(true);
        }
        // SYSFLG1 default (0x00008000, CLDFLG only) and the chip-variant
        // virtual overrides (FRBADDR / SYSCON2 / on-chip SRAM all absent,
        // SYSCON 24-bit) live in CLPS7110::Emulator — see
        // core/clps7110.{h,cpp}.
        // The settings PROM: an empty (all-zero) image until the user
        // picks a language, exactly what this machine read before the
        // PROM was wired — see onPortBWrite below.
        etna.resetImage(0x20);
    }
    const char *getDeviceName() const override { return "Series 5"; }

    // ── Settings PROM and ROM language ────────────────────────────────
    // VArmP2.dll reads a 16-word PROM at boot (0x5007E45C in v1.01(144)):
    // port B bit 0 selects the chip, bit 1 clocks it, and the data goes
    // out and comes back on ETNA register 0x0C bits 2 and 3 — the same
    // part and protocol as the Windermere machines' identity PROM, which
    // is why Etna already speaks it. It validates the block (XOR 0x42)
    // and returns bits 18..20 of the first word as LanguageIndex() and
    // bits 21..23 as KeyboardIndex() (0x5007E5DC / 0x5007E5F4). Until
    // these lines were wired it read zeros, failed the check, and every
    // emulated Series 5 booted index 0.
    void onPortBWrite(uint32_t oldPorts, uint32_t newPorts) override {
        if ((newPorts & 0x10000) && !(oldPorts & 0x10000))
            etna.setPromBit0High();
        else if (!(newPorts & 0x10000) && (oldPorts & 0x10000))
            etna.setPromBit0Low();
        if ((newPorts & 0x20000) && !(oldPorts & 0x20000))
            etna.setPromBit1High();
    }
    // v1.01(144) carries ELocl1.dll — English with the Scandinavian
    // formats (kr, 24-hour clock) — and Ekdata1.dll with the Nordic
    // letters; v1.00(113) has only the UK pair, so it offers no choice.
    void loadROM(uint8_t *buffer, size_t size) override {
        CLPS7110::Emulator::loadROM(buffer, size);
        static const char kWant[] = "ELocl1.dll";
        hasScandinavianLocale = false;
        const size_t n = sizeof(kWant) - 1;
        for (size_t i = 0; i + n <= size && !hasScandinavianLocale; i++)
            hasScandinavianLocale = std::memcmp(buffer + i, kWant, n) == 0;
        if (!hasScandinavianLocale) language = 0;
    }
    int getLanguageCount() const override { return hasScandinavianLocale ? 2 : 0; }
    const char *getLanguageName(int index) const override {
        static const char *const kNames[2] = { "English (UK)", "English (Scandinavian)" };
        return (index >= 0 && index < getLanguageCount()) ? kNames[index] : nullptr;
    }
    int getLanguage() const override { return language; }
    bool setLanguage(int index) override {
        if (index < 0 || index >= getLanguageCount()) return false;
        language = index;
        etna.setLocaleIndices(index, index);
        return true;
    }
    bool hasScandinavianLocale = false;
    int language = 0;
    size_t getROMSize() override { return 0x600000; }

    // Series 5 LCD: 640 x 240, 4 bpp mono, same framebuffer format as 5mx.
    int getLCDWidth()  const override { return 640; }
    int getLCDHeight() const override { return 240; }
    int getDigitiserWidth()  const override { return 695; }
    int getDigitiserHeight() const override { return 280; }
    int getLCDOffsetX()      const override { return 45; }
    int getLCDOffsetY()      const override { return 5; }

    // Expose RAM to the harness so --save-ram-snapshot works.
    uint8_t *getRamBuffer() override       { return MemoryBlockC0; }
    size_t   getRamSize()   const override { return sizeof(MemoryBlockC0); }

protected:
    // Series 5 has 8 MB RAM in a single bank, mirrored/aliased at 0xD0000000
    // (EPOC R1 kernel puts its data page near the top of that bank at
    // 0xD0FE6000). Opt into the larger mask and the region-0xD alias so the
    // kernel's accesses land on real storage.
    //
    // 2026-05-06: model two SEPARATE 4 MB DRAM banks per CL-PS7110 datasheet
    // §1.2.5 + the schematic + photo of the motherboard (two matched DRAM
    // chips). Bank 0 (NRAS[0]) at 0xC0000000-0xC03FFFFF, Bank 1 (NRAS[1]) at
    // 0xD0000000-0xD03FFFFF, each repeating within its 256 MB segment. The
    // previous "8 MB at C, mirrored at D" model collapsed the two banks
    // into one physical RAM, so writes to 0xC0xxxxxx and 0xD0xxxxxx aliased
    // each other and corrupted distinct kernel state. PSION_S5_TWO_BANK=0
    // env var falls back to the legacy single-bank model for diagnosis.
    // PSION_S5_RAM_MODE selects RAM layout for diagnostics:
    //   "2bank"  (default) : two 4 MB banks (C + D), each mirroring at 4 MB stride
    //   "1bank"            : single 8 MB at C, D region aliased to C (mirrored)
    //   "8mbC"             : 8 MB linear at C, D region UNMAPPED (returns 0xFF)
    //   "4mbC"             : 4 MB at C, D region UNMAPPED
    // The default 2bank matches the CL-PS7110 schematic (two NRAS pins, two
    // 4 MB chips). The 8mbC mode tests the hypothesis that real Series 5
    // wired both chips into a single 8 MB linear bank at NRAS[0] instead.
    int getRamMode() const {
        static int mode = -1;
        if (mode < 0) {
            // Back-compat: PSION_S5_TWO_BANK=0 → "1bank" mode
            const char *legacy = std::getenv("PSION_S5_TWO_BANK");
            if (legacy && legacy[0] == '0' && legacy[1] == '\0') mode = 1;
            const char *e = std::getenv("PSION_S5_RAM_MODE");
            if (e) {
                if      (!strcmp(e, "2bank")) mode = 0;
                else if (!strcmp(e, "1bank")) mode = 1;
                else if (!strcmp(e, "8mbC"))  mode = 2;
                else if (!strcmp(e, "4mbC"))  mode = 3;
            }
            if (mode < 0) mode = 0;  // default = 2bank
        }
        return mode;
    }
    uint32_t getRamMask() const override {
        switch (getRamMode()) {
        case 0: return 0x3FFFFFu;    // 2bank: 4 MB per bank
        case 1: return 0x7FFFFFu;    // 1bank: 8 MB visible
        case 2: return 0x7FFFFFu;    // 8mbC:  8 MB at C
        case 3: return 0x3FFFFFu;    // 4mbC:  4 MB at C
        default: return 0x3FFFFFu;
        }
    }
    bool aliasDRegionToRam() const override {
        switch (getRamMode()) {
        case 0: return true;   // 2bank: D maps to upper 4 MB of MemoryBlockC0
        case 1: return true;   // 1bank: D mirrors C
        case 2: return false;  // 8mbC: D unmapped
        case 3: return false;  // 4mbC: D unmapped
        default: return true;
        }
    }
    uint32_t getRegionDRamOffset() const override {
        return getRamMode() == 0 ? 0x400000u : 0;
    }

    // The clps7111 base class hooks debug logging to PCs pinned to the Osaris
    // ROM v1.02 layout (CObjectCon::AddL @ 0x32304, MMU helpers @ 0x634/0x66C,
    // event dispatcher @ 0x16198). The Series 5 ROM is a different binary so
    // those PCs collide with unrelated functions and produced fake
    // "KERNEL MMU SECTION" log entries (e.g. v=c0013044 p=0000007c size=0)
    // that misled earlier investigation. Disable them here; series5DebugPC()
    // below registers the Series-5-specific equivalents we've located.
    bool hasOsarisDebugHooks() const override { return false; }

    // EPOC R1's tap-click feedback drives the CL-PS7110 buzzer pin via
    // SYSCON1 BZTOG=1 pulses (verified by tracing SYSCON1 writes around
    // an icon tap). Opt into the base class's buzzer pump so those
    // pulses become audible 8 kHz square-wave bursts on the host
    // speaker. Osaris and MC218 stay opted out — their kernels write
    // SYSCON1 in patterns that would otherwise emit continuous tones
    // (see CLPS7111::enableBuzzerPump comment).
    bool enableBuzzerPump() const override { return true; }

    // CL-PS7110 has no on-die CLPS7600 PCMCIA controller. Empirical evidence
    // (PSION_CF_TRACE on v1.01(144)): Series 5 EPOC R1's CF driver splits
    // its ATA task-file accesses across 0x4C* and 0x4E* aliases — both are
    // CF I/O windows on real Series 5 hardware. The CLPS7600 register
    // emulator never gets touched. Returning false here removes the
    // CLPS7600 dispatch and routes 0x4C-0x4F to CF I/O instead, letting
    // IDENTIFY data drain from 0x4C000000 V32 reads (256 words) the way
    // the kernel expects.
    bool chipHasCLPS7600() const override { return false; }

    // Series 5's CL-PS7110 has an ADC chip wired to the SSI that
    // responds (unlike Osaris/MC218/5mx where SYNCIO is undriven /
    // open-bus). Real-hardware response we don't model exactly, but
    // empirically 0x800 (mid-scale, bit 0x400 clear) keeps the kernel
    // on the "pen up + alignment-fault retry" branch which is the
    // most productive boot path in our emulator (268 unique_pcs at
    // 38s vs 29 with 0xFFFFFFFF). See series5.h SYNCIO investigation
    // notes for the empirical sweep.
    uint32_t defaultSyncioReadValue() const override { return 0x800; }

    // Series 5 CF re-enable (Nov 2025): boot now completes (year fix +
    // LSL fix + touch calibration) and the stubs that previously refused
    // every attach are gone. The base CLPS7111 attach handles the
    // CLPS7600 + cfCard path; we also have to notify ETNA (the discrete
    // companion at chip-select nCS1) because Series 5 EPOC R1 polls
    // PCMCIA socket status via ETNA's SktVarA0 register, not via the
    // on-chip CLPS7600 input-level. Without flipping etna.cardPresent
    // too, the kernel keeps reading 0x01 from SktVarA0 and decides no
    // card is in the slot.
    bool attachCard(const uint8_t *bytes, size_t size) override {
        bool ok = CLPS7111::Emulator::attachCard(bytes, size);
        if (ok) etna.setCardPresent(true);
        return ok;
    }
    void detachCard() override {
        CLPS7111::Emulator::detachCard();
        etna.setCardPresent(false);
    }

    // PC-card socket Vcc sense (SYNCIO ADC channel 0xE1). EPOC R1's PCMCIA
    // PSU power-up (DPlatPcCardVcc) samples this channel via a periodic DFC,
    // low-pass-filters it (ctrl+0x1c converges to the raw sample), then scales
    // it back up (~×2.5) and refuses to enumerate the card until the result
    // is within 25% of its 3300 mV (3.3 V) target. The supply is sensed
    // through a divider, so a powered 3.3 V socket reads ~1320 mV on this
    // channel (×2.5 → ~3.3 V at the check). Report 0 when the socket is
    // unpowered so the PSU correctly sees "no voltage" before it switches Vcc
    // on. Without this the channel read 0 (open bus), the voltage check timed
    // out (KErrNotReady) and looped the power-up forever, so the card never
    // enumerated. See docs/series5-cf-investigation.md for the full trace.
    uint32_t socketVccSenseAdc() const override {
        return etna.socketPowered() ? 1320 : 0;
    }

    // Series 5 touch input. Now that boot reaches the interactive desktop,
    // the pen driver's IRQ handler is registered and touch is live: we
    // stash the X/Y coords (read back through the ADC1010 SYNCIO path in
    // clps7111.cpp) AND assert the touch interrupt so the kernel's WSERV
    // delivery path runs rather than relying on polling alone.
    //
    // Which interrupt line (history of the bring-up):
    //   * The Series 5 schematic wires the ADS7843 PENIRQ (active-low,
    //     R41 100K pull-up) to EINT3 on the CL-PS7110 — this is the
    //     hardware-faithful line and we assert it (level-triggered:
    //     held while the pen is down, cleared on lift).
    //   * We ALSO assert EINT2, which the kernel's IRQ path handles and
    //     uses to wake the WSERV tap-delivery state machine (the 5mx
    //     uses EINT2 legitimately, so the handler is defensive). During
    //     early bring-up — before boot reached the desktop and the pen
    //     driver registered its handler — EINT3 alone produced no
    //     response, which is why the EINT2 assist was added.
    //   * EINT1 is a multiplexed battery / PCMCIA-card-detect / power
    //     sub-dispatcher; firing it on pen-down sends the kernel down a
    //     card-detect path, so we deliberately do NOT use it for touch.
    //   * EXTFIQ is available behind PSION_S5_TOUCH_FIQ (off; confirmed
    //     ineffective). EINT3 / EINT2 assertion can each be disabled for
    //     diagnosis via PSION_S5_TOUCH_EINT3=0 / PSION_S5_TOUCH_EINT2=0.
    void updateTouchInput(int32_t x, int32_t y, bool down) override {
        if (down) {
            touchX = x;
            touchY = y;
        } else {
            touchX = 0;
            touchY = 0;
        }
        // Series 5 hardware: the ADS7843 touchscreen controller's
        // PENIRQ output (active-low, pulled up by R41=100K to VDD) is
        // wired to EINT3 on the CL-PS7110 (Windermere). Confirmed via
        // Psion Series 5 schematics: ADS7843 chip + R41 100K 1% pull-up
        // sit immediately adjacent to the Windermere SoC, with the
        // "EINT3" net label directly below the WINDERMERE block, next
        // to R41.
        //
        // On pen contact: PENIRQ goes LOW → EINT3 input pulled LOW →
        // CL-PS7110 sets INTSR1 bit 7 (EINT3) → IRQ delivered if EINT3
        // is unmasked in INTMR1 → kernel EINT3 handler reads ADS7843
        // coordinates via SYNCIO.
        //
        // EINT3 is level-triggered (per CL-PS7110 datasheet 1.2.13):
        // the interrupt stays asserted as long as the pin is low. We
        // model that by setting pendingInterrupts.EINT3 while pen is
        // down, and clearing it when pen lifts. The kernel handler
        // acks by reading something (not a write-to-clear); we keep
        // it asserted continuously until the user lifts the pen.
        //
        // PSION_S5_TOUCH_EINT3=0 disables the EINT3 assertion for diagnosis.
        // PSION_S5_TOUCH_EINT2=0 disables the EINT2 assertion for diagnosis.
        // PSION_S5_TOUCH_FIQ=1 re-enables the legacy EXTFIQ assertion
        // (kept around — confirmed ineffective by earlier testing).
        //
        // Diagnostic finding (from user mobile log):
        //   First 1-2 taps after boot DO trigger full sampling (FUN_5001D908
        //   averaging 4 X-samples + 4 Y-samples → FUN_5001d670 vtable call).
        //   Subsequent taps fail — FUN_5001D908 never fires.
        //
        //   Per decompiled FUN_5007C21C: sampling only kicks off when
        //   field+0x118 != 2. The flag advances to 2 each time a tap is
        //   processed but apparently never resets to 0 without a separate
        //   IRQ to advance the state machine forward.
        //
        // Hypothesis: the 5mx works because its base updateTouchInput sets
        // EINT2 (bit 6) which IS unmasked in INTMR1 — that IRQ triggers a
        // kernel-level handler that delivers the queued tap to WSERV and
        // resets the in-progress flag. Series 5 only asserted EINT3 which
        // is masked, so the IRQ path never fires; the kernel only sees pen
        // state via polling, and the WSERV delivery never happens.
        //
        // Fix: assert BOTH EINT2 (wakes the kernel IRQ delivery path) and
        // EINT3 (matches the Series 5 schematic for the polling-side state
        // machine in FUN_5007C21C / FUN_5007C2FC). The kernel handles
        // EINT2's spurious assertion gracefully (5mx hardware uses it
        // legitimately, so the handler is defensive).
        static int useEint3 = -1;
        static int useEint2 = -1;
        static int useFiq = -1;
        if (useEint3 < 0) {
            const char *e = std::getenv("PSION_S5_TOUCH_EINT3");
            useEint3 = (e && e[0] == '0' && e[1] == '\0') ? 0 : 1;
        }
        if (useEint2 < 0) {
            const char *e = std::getenv("PSION_S5_TOUCH_EINT2");
            useEint2 = (e && e[0] == '0' && e[1] == '\0') ? 0 : 1;
        }
        if (useFiq < 0) {
            const char *e = std::getenv("PSION_S5_TOUCH_FIQ");
            useFiq = (e && e[0] == '1') ? 1 : 0;
        }
        if (useEint3) {
            if (down) {
                pendingInterrupts |= (1u << CLPS7111::EINT3);
            } else {
                pendingInterrupts &= ~(1u << CLPS7111::EINT3);
            }
        }
        if (useEint2) {
            if (down) {
                pendingInterrupts |= (1u << CLPS7111::EINT2);
            } else {
                pendingInterrupts &= ~(1u << CLPS7111::EINT2);
            }
        }
        if (useFiq && down) {
            pendingInterrupts |= (1u << CLPS7111::EXTFIQ);
        }
        log("Touch (Series 5): x=%d y=%d down=%s (EINT3=%d EINT2=%d fiq=%d)",
            x, y, down ? "yes" : "no",
            useEint3 ? 1 : 0,
            useEint2 ? 1 : 0,
            (useFiq && down) ? 1 : 0);

        // BRUTE-FORCE state-machine flag reset (PSION_S5_TOUCH_BFORCE,
        // DEFAULT OFF). Originally added to test the hypothesis that the
        // touch state machine's +0x118 flag stuck at 3 after tap #1 was
        // blocking subsequent sampling. Empirical testing via the harness
        // (--tap-at + harness/run.cpp PSION_MULTI_TAP) showed:
        //   - X-sample DFC (FUN_5001D908) DOES fire on every tap, with
        //     20-36 hits per tap. The earlier "only fires once per boot"
        //     conclusion was a log-filter artifact (count<30 || count&0xFF
        //     == 0 masked hits #30-255 from view).
        //   - Sampling is NOT the bug. Brute-force write makes no
        //     difference to per-tap hit counts.
        //   - Real bug is downstream: every tap location produces the
        //     SAME 51K-pixel-diff screen, suggesting tap coords aren't
        //     honored at WSERV delivery.
        // Kept off by default; address corrected to 0x800059CC (struct
        // base 0x800058B4 + 0x118, confirmed by hooking FUN_5007C21C).
        static int bforceMode = -1;
        if (bforceMode < 0) {
            const char *e = std::getenv("PSION_S5_TOUCH_BFORCE");
            bforceMode = (e && e[0] == '1') ? 1 : 0;
        }
        if (bforceMode && down) {
            // CORRECTED address: struct base 0x800058B4 + 0x118 = 0x800059CC.
            // (Earlier candidates 0x80100EAC/DF8 were guesses based on a
            // misreading of the schedule logs — the actual touch state
            // struct lives at 0x800058B4, confirmed by hooking FUN_5007C21C
            // and dumping R0 across multiple taps.)
            //
            // From disasm of FUN_5007C0E8 (the timer-driven pen poller):
            //   if (pen_down && state == 0) state=1, kick off sampling
            //   if (pen_up)                  state=3 (tap-done sentinel)
            // After tap #1 completes, state stays at 3. Subsequent taps see
            // (pen_down && state!=0) and take a different code path that
            // doesn't kick off a fresh X-sample DFC.
            //
            // Brute-force reset: on each pen-down event, write state=0 so
            // the next polling-function call kicks off sampling fresh.
            const uint32_t kStateAddr = 0x800059CCu;
            auto v = readVirtualDebug(kStateAddr, V32);
            uint32_t before = v.value_or(0xDEADBEEFu);
            if (before != 0u) {
                writeVirtual(0u, kStateAddr, V32);
                log("Touch (Series 5): BFORCE reset *(0x%08x): %u -> 0",
                    kStateAddr, before);
            } else {
                log("Touch (Series 5): BFORCE probe *(0x%08x) = 0 (already idle)",
                    kStateAddr);
            }
        }

        // SNAPSHOT-DIFF (PSION_S5_TOUCH_DIFF=1): capture a snapshot of
        // touch-state-related RAM on every pen-down event and diff against
        // the previous snapshot. Words that DIFFER between successive taps
        // are likely the state-machine fields stuck in the "consumed" state
        // that block subsequent sampling. Range chosen to cover the touch
        // DFC table at 0x80100CA4..0x80100D50 plus a 768-byte window of
        // surrounding kernel state.
        static int diffMode = -1;
        static bool haveSnapshot = false;
        static uint32_t lastSnap[256];   // 256 words = 1024 bytes
        static int tapIdx = 0;
        if (diffMode < 0) {
            const char *e = std::getenv("PSION_S5_TOUCH_DIFF");
            diffMode = (e && e[0] == '1') ? 1 : 0;
        }
        if (diffMode && down) {
            tapIdx++;
            // Touch struct base is 0x800058B4 (confirmed via FUN_5007C21C r0
            // dump). Snapshot 1024 bytes around it: 0x80005800-0x80005C00.
            const uint32_t snapBase = 0x80005800u;
            uint32_t curSnap[256];
            for (int i = 0; i < 256; i++) {
                auto v = readVirtualDebug(snapBase + i * 4u, V32);
                curSnap[i] = v.value_or(0u);
            }
            if (haveSnapshot) {
                int diffs = 0;
                for (int i = 0; i < 256; i++) {
                    if (curSnap[i] != lastSnap[i]) {
                        log("Touch (Series 5): DIFF tap#%d *(0x%08x): 0x%08x -> 0x%08x",
                            tapIdx, snapBase + i * 4u, lastSnap[i], curSnap[i]);
                        if (++diffs > 30) {
                            log("Touch (Series 5): DIFF tap#%d ... (truncated)", tapIdx);
                            break;
                        }
                    }
                }
                if (diffs == 0)
                    log("Touch (Series 5): DIFF tap#%d no changes vs prev snapshot", tapIdx);
            } else {
                log("Touch (Series 5): DIFF tap#%d initial snapshot captured", tapIdx);
            }
            for (int i = 0; i < 256; i++) lastSnap[i] = curSnap[i];
            haveSnapshot = true;
        }
    }

    // Set by updateTouchInput when pen-down with PSION_S5_TOUCH_INJECT=1.
    // Read by debugPC() at PC=0x5001AE04 (idle tail). When non-zero, the
    // hook injects a synthetic BL to FUN_5001D6E0 by saving the kernel's
    // current LR into a stash, setting LR=current PC and PC=FUN_5001D6E0,
    // so when FUN_5001D6E0 returns we land back at the idle loop. The
    // flag is consumed (set to false) by the hook after one fire.
    mutable bool touchInjectPending = false;

    // Series 5 keyboard scan reads all 8 columns. CLPS7111 (MC218/Osaris)
    // only has 7 columns; the base readKeyboard walks 0..6 and returns 0
    // for kScan==7. Series 5 needs column 7 (left shift / arrows row) too.
    uint32_t readKeyboard() const override {
        if (kScan & 8) {
            return keyboardColumns[kScan & 7];
        } else if (kScan == 0) {
            uint8_t val = 0;
            for (int i = 0; i < 8; i++)
                val |= keyboardColumns[i];
            return val;
        }
        return 0;
    }

    // Series 5 keyboard matrix — 8 columns × 7 rows, scanned via the same
    // CL-PS7110 kscan mechanism as MC218 / Osaris but with a different
    // physical layout. The Series 5 keyboard is the direct predecessor of
    // the 5mx keyboard (same key positions, same scan layout), so we
    // mirror the 5mx (Windermere) matrix here. The CLPS7111 base class's
    // matrix is calibrated for the smaller MC218/Osaris keyboards and
    // produces wrong scan codes on Series 5.
    void setKeyboardKey(EpocKey key, bool value) override {
        int idx = -1;
#define KEY5(column, bit) idx = (column << 8) | (1 << bit); break

        switch ((int)key) {
        case EStdKeyDictaphoneRecord: KEY5(0, 6);
        case '1':                     KEY5(0, 5);
        case '2':                     KEY5(0, 4);
        case '3':                     KEY5(0, 3);
        case '4':                     KEY5(0, 2);
        case '5':                     KEY5(0, 1);
        case '6':                     KEY5(0, 0);

        case EStdKeyDictaphonePlay:   KEY5(1, 6);
        case '7':                     KEY5(1, 5);
        case '8':                     KEY5(1, 4);
        case '9':                     KEY5(1, 3);
        case '0':                     KEY5(1, 2);
        case EStdKeyBackspace:        KEY5(1, 1);
        case EStdKeySingleQuote:      KEY5(1, 0);

        case EStdKeyEscape:           KEY5(2, 6);
        case 'Q':                     KEY5(2, 5);
        case 'W':                     KEY5(2, 4);
        case 'E':                     KEY5(2, 3);
        case 'R':                     KEY5(2, 2);
        case 'T':                     KEY5(2, 1);
        case 'Y':                     KEY5(2, 0);

        case EStdKeyLeftAlt:
        case EStdKeyRightAlt:
        case EStdKeyMenu:             KEY5(3, 6);
        case 'U':                     KEY5(3, 5);
        case 'I':                     KEY5(3, 4);
        case 'O':                     KEY5(3, 3);
        case 'P':                     KEY5(3, 2);
        case 'L':                     KEY5(3, 1);
        case EStdKeyEnter:            KEY5(3, 0);

        case EStdKeyLeftCtrl:         KEY5(4, 6);
        case EStdKeyTab:              KEY5(4, 5);
        case 'A':                     KEY5(4, 4);
        case 'S':                     KEY5(4, 3);
        case 'D':                     KEY5(4, 2);
        case 'F':                     KEY5(4, 1);
        case 'G':                     KEY5(4, 0);

        case EStdKeyLeftFunc:         KEY5(5, 6);
        case 'H':                     KEY5(5, 5);
        case 'J':                     KEY5(5, 4);
        case 'K':                     KEY5(5, 3);
        case 'M':                     KEY5(5, 2);
        case EStdKeyFullStop:         KEY5(5, 1);
        case EStdKeyDownArrow:        KEY5(5, 0);

        case EStdKeyRightShift:       KEY5(6, 6);
        case 'Z':                     KEY5(6, 5);
        case 'X':                     KEY5(6, 4);
        case 'C':                     KEY5(6, 3);
        case 'V':                     KEY5(6, 2);
        case 'B':                     KEY5(6, 1);
        case 'N':                     KEY5(6, 0);

        case EStdKeyLeftShift:        KEY5(7, 6);
        case EStdKeyDictaphoneStop:   KEY5(7, 5);
        case EStdKeySpace:            KEY5(7, 4);
        case EStdKeyUpArrow:          KEY5(7, 3);
        case EStdKeyComma:            KEY5(7, 2);
        case EStdKeyLeftArrow:        KEY5(7, 1);
        case EStdKeyRightArrow:       KEY5(7, 0);
        }
#undef KEY5
        if (idx >= 0) {
            if (value)
                keyboardColumns[idx >> 8] |= (idx & 0xFF);
            else
                keyboardColumns[idx >> 8] &= ~(idx & 0xFF);
        }
    }

    // Series-5 specific PC hooks. Located via reference/5_decompiled:
    //   FUN_50018c38 (line 28695) — LCD-controller init: constructs the
    //     framebuffer chunk at virt 0xC0000000, writes LCDCON.
    //   FUN_5001ae04 (line 30297) — kernel idle / "wait for IRQ" tail.
    //     Hit ~12.96M cycles into boot; from then on the kernel is in
    //     the idle loop (see series5.h investigation notes lines 1327+).
    //   FUN_5001d6e0 (line 32299) — periodic touchscreen-controller poll
    //     that reads SYNCIO and decides between writing 0x6D09 / 0x6D0C.
    //     Fires once per second post-boot. The recurring AlignmentFault
    //     at PC=0x5000cf84 traces back to this poll.
    void debugPC(uint32_t pc) override;

    // Brute-force sweep: Series 5 boots to a black framebuffer (variance=0,
    // unique_pcs ≈ 150) on the default config. The kernel reaches User32
    // mode and runs EFile.exe, but never gets far enough to install a TRAP
    // handler before something tries to leave — yielding a recurring
    // EClnNoTrapHandlerInstalled panic recovery at PC=0x50043E40.
    //
    // Suspected root cause: missing/incorrect responses from one of:
    // (a) the "ETNA-like" companion at region 2 (0x20000000),
    // (b) DRAM-size probe expectations,
    // (c) initial kernel-state seeds (SYSFLG1, INTSR1, DRFPR, PMPCON).
    //
    // applyDeviceQuirks() reads PSION_S5_* env vars and adjusts the seeds
    // before reset(), giving a small set of single-knob experiments that
    // scripts/series5-sweep.sh can drive.
    void applyDeviceQuirks() override;
    MaybeU32 readRegion2(uint32_t physAddr, ValueSize valueSize) const override;
    bool writeRegion2(uint32_t value, uint32_t physAddr, ValueSize valueSize) override;

    // Periodic kernel-state polling (PSION_S5_KERNEL_TRACE=1). Watches the
    // EKA1 globals that should change during a successful boot:
    //   * iCurrentThread     at virt 0x8010061C
    //   * iRescheduleNeededFlag at virt 0x80100348
    //   * iNState (offset 0xC4) of the loader/idle thread at 0x80006DAC
    //   * LCDCON via currentLcdControl()
    //   * Highest CPSR mode reached (Undef→IRQ→Abort→User would all be progress)
    // Logs each transition to stderr with cycle count + sim-second so the
    // boot timeline is visible without an instruction-level trace.
    void pollKernelState() override;

    // CF IREQ# → ETNA PcCdIntStatus bit 0. We still latch the rising edge
    // into ETNA's interrupt-status register so a polling driver can see it,
    // but DO NOT route the ETNA media-change to a SoC IRQ line. The 5mx
    // (Windermere) wiring routes ETNA's combined IRQ to EINT3, but on
    // Series 5 (CL-PS7110) EINT3 is wired to the ADS7843 PENIRQ touch
    // controller (see series5.h ~line 5830). Clobbering EINT3 from here
    // every cycle masks the pen-down signal and breaks touch entirely.
    // The Series 5 EPOC R1 kernel polls SktVarA0 / CF status anyway, so
    // it doesn't need this IRQ route — MCINT raised by attachCard plus
    // the polling driver are sufficient.
    void tickCfBridge() override {
        bool cfIrqNow = cfCard.inserted() && cfCard.irqAsserted();
        etna.setCfIreq(cfIrqNow);
    }

private:
    // Mutable seeds read from env at applyDeviceQuirks() time. Defaults
    // preserve current behaviour exactly. ramMaskOverride is read at every
    // physical access, so keep it cheap.
    uint32_t ramMaskOverride = 0x7FFFFF;

    // PSION_S5_REGION2_TYPE selects the response shape for unmapped reads
    // in the 0x20000000 aperture. Writes are stashed in region2Latch when
    // type=3 (read-as-write-latched) so the kernel sees what it just wrote.
    enum Region2Type {
        R2_ZERO  = 0,
        R2_ONES  = 1,
        R2_COUNT = 2,
        R2_LATCH = 3,
    };
    int region2Type = R2_ZERO;
    mutable uint32_t region2ReadCount = 0;

    // ETNA companion-chip emulator. Real Series 5 hardware has an ETNA chip
    // (the same family as the 5mx ETNA) wired to chip-select nCS1 at
    // 0x20000000, providing PCMCIA/CF socket control, wake-up registers,
    // and interrupt-clear/-mask. The kernel writes to ETNA registers
    // (IntClear=0x08, wake1=0x0C, etc.) during boot — without correct
    // responses the kernel's hardware-init phase stalls before the
    // process loader runs.
    //
    // Trace evidence: with PSION_S5_REGION2_LOG=1, Series 5 writes 0x80
    // to off=0x0C (= ETNA wake1) and 0x3F to off=0x08 (= ETNA IntClear),
    // matching exactly what WindEmu logs for the 5mx kernel boot. Series
    // 5 is using ETNA register layout.
    //
    // Switching from the R2_LATCH stub to a real ETNA instance lets the
    // kernel see proper register semantics (e.g. SktVarA0 returning 0x01
    // when no card is present, RDY/CD bits gated correctly).
    mutable Etna etna{&cpu};
    bool useRealEtna = true;
    uint32_t region2Latch[16] = {};

    // Kernel-state tracker state (PSION_S5_KERNEL_TRACE).
    bool kernelTrace = false;
    uint32_t lastCurrentThread = 0;
    uint32_t lastRescheduleFlag = 0;
    uint32_t lastIdleNState = 0;
    uint32_t lastLoaderNState = 0;
    uint32_t lastBootNState = 0;
    uint32_t lastMysteryNState = 0;
    uint64_t userModeCounter = 0;
    int userModePcCount = 0;
    uint32_t userModePcs[256] = {};
    uint32_t lastLcdControl = 0;
    uint32_t highestCpsrMode = 0;
    uint64_t pollSampleEvery = 0;       // polls per (cycles_passed mod N) check
    uint64_t pollLastSampleAt = 0;
    uint32_t firstUserModeAt = 0;       // cycle when CPSR first hits User (0x10)
    uint32_t supervisorThreadFirstSeenAt = 0; // cycle when iCurrentThread first
                                              // becomes != idle thread
    bool supervisorBlockDumped = false;
    bool fixSupervisorVtable = false;
    bool supervisorVtablePatched = false;
    uint32_t lastWatch80006F10 = 0;
    uint32_t lastWatch80006F20 = 0;
    uint32_t lastWatch80006F38 = 0;
    uint32_t lastSupIPc        = 0;
    uint32_t lastSupICpsr      = 0;
    uint32_t lastSupISp        = 0;
};

}
