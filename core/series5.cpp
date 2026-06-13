// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "series5.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace Series5 {

// Read an unsigned integer from an env var, accepting decimal or 0x-prefixed
// hex. Returns the supplied default if the var is unset or unparseable.
static uint32_t envU32(const char *name, uint32_t fallback) {
    const char *s = std::getenv(name);
    if (!s || !*s) return fallback;
    char *end = nullptr;
    unsigned long v = std::strtoul(s, &end, 0);
    if (!end || *end != '\0') return fallback;
    return (uint32_t)v;
}

void Emulator::applyDeviceQuirks() {
    // Stash region-2 response shape. Default unchanged (R2_ZERO).
    uint32_t r2 = envU32("PSION_S5_REGION2_TYPE", R2_ZERO);
    if (r2 <= R2_LATCH) region2Type = (int)r2;

    // SYSFLG1 OR-mask. Lets the sweep flip extra status flags the kernel
    // may poll early (battery-OK, low-batt, charge-state etc).
    uint32_t flgOr = envU32("PSION_S5_SYSFLG1_OR", 0);
    if (flgOr) sysFlg1 |= flgOr;
    uint32_t flgAnd = envU32("PSION_S5_SYSFLG1_AND", 0xFFFFFFFFu);
    sysFlg1 &= flgAnd;

    // Initial pending-interrupt mask. Some EKA1 kernels expect a particular
    // wakeup interrupt to be already-asserted at boot.
    uint32_t intsr = envU32("PSION_S5_INTSR1_SEED", 0);
    if (intsr) pendingInterrupts |= (uint16_t)intsr;

    // DRFPR / PMPCON seeds — the kernel's RMW loops poll these for status
    // bits it sets on its own; seeding can shortcut a stuck wait.
    uint32_t drf = envU32("PSION_S5_DRFPR_SEED", 0);
    if (drf) drfpr = (uint8_t)drf;
    uint32_t pmp = envU32("PSION_S5_PMPCON_SEED", 0);
    if (pmp) pmpcon = pmp;

    // RAM mask. 0x3FFFFF = 4 MB (MC218 layout), 0x7FFFFF = 8 MB (default),
    // 0xFFFFFF = 16 MB (full 32-bit aperture). Lets us check whether the
    // kernel's RAM-size probe is sensitive to the wrap point.
    uint32_t ram = envU32("PSION_S5_RAM_MASK", 0x7FFFFF);
    if (ram == 0x3FFFFF || ram == 0x7FFFFF || ram == 0xFFFFFF)
        ramMaskOverride = ram;

    // Diagnostic banner so sweep logs show which mutation each run used.
    if (std::getenv("PSION_S5_SWEEP_BANNER")) {
        std::fprintf(stderr,
            "[series5] quirks: r2=%d sysFlg1=0x%08x intsr=0x%04x drfpr=0x%02x"
            " pmpcon=0x%08x ramMask=0x%07x\n",
            region2Type, sysFlg1, pendingInterrupts, drfpr, pmpcon,
            ramMaskOverride);
    }

    // Kernel-state tracker (PSION_S5_KERNEL_TRACE=1). Samples every 4096
    // outer-loop iterations (~the same cadence as the timer tick check) so
    // overhead stays trivial.
    kernelTrace = std::getenv("PSION_S5_KERNEL_TRACE") != nullptr;
    pollSampleEvery = 4096;
    pollLastSampleAt = 0;
    if (kernelTrace) {
        std::fprintf(stderr,
            "[s5-kernel] tracker enabled — watching iCurrentThread @ 0x8010061C, "
            "iRescheduleNeededFlag @ 0x80100348, idle NThread iNState @ 0x80006e70, "
            "LCDCON, highest CPSR mode\n");
    }

    // Experimental supervisor sub-object vtable fix
    // (PSION_S5_FIX_SUPERVISOR_VTABLE=1). The kernel allocator reuses
    // memory at virt 0x80006F10 that was previously filled with 0xA5A5A5A5
    // by an early-boot debug-fill, and FUN_5000F69C's vtable-write doesn't
    // land there in our boot — see the analysis in commit 41c4a3a's body
    // and the supervisor-block dump output. When this knob is set, the
    // tracker patches the vtable slot to the same value the case-0 thread
    // sub-object uses (0x50028444 = DAT_5000F6E4 in the ROM constant pool)
    // as soon as it's seen as 0xA5A5A5A5. This unblocks the
    // LDR PC, [R3, #0x14] dispatch at 0x5002eb2c so FUN_5002DF60 (the
    // GetName method) runs and FUN_5002EB0C returns, letting Construct
    // complete and FUN_50006758 progress to the Resume vtable call that
    // sets the supervisor thread iNState=EReady.
    fixSupervisorVtable = std::getenv("PSION_S5_FIX_SUPERVISOR_VTABLE") != nullptr;
    if (fixSupervisorVtable) {
        std::fprintf(stderr,
            "[s5-kernel] FIX_SUPERVISOR_VTABLE enabled — will patch 0x80006F10 "
            "to vtable 0x50028444 when detected as 0xA5A5A5A5\n");
    }
}

void Emulator::pollKernelState() {
    // The fix path can run independently of the tracer logging. We check
    // the trigger condition at the same low rate as the tracker.
    if (!kernelTrace && !fixSupervisorVtable) return;

    // Get current cycle approximation from base via the CPU's elapsed cycles.
    // We use a simple "every Nth call" heuristic since clps7111.cpp's
    // executeUntil already gates via passedCycles >= nextTickAt.
    static uint64_t callCount = 0;
    callCount++;
    if (callCount - pollLastSampleAt < pollSampleEvery) return;
    pollLastSampleAt = callCount;

    // Sample the EKA1 kernel globals. readVirtualDebug honours MMU translation
    // so we get the same view the kernel does — important since our virt
    // addresses are kernel-mapped via the page tables it set up.
    auto sample = [this](uint32_t va) -> uint32_t {
        auto v = readVirtualDebug(va, V32);
        return v.has_value() ? v.value() : 0xDEADBEEFu;
    };

    uint32_t curThread       = sample(0x8010061C);   // iCurrentThread
    uint32_t reschedFlag     = sample(0x80100348);   // iRescheduleNeededFlag
    // Idle / loader thread NThread block at 0x80006DAC; iNState at offset 0xC4
    // means virt 0x80006E70.
    uint32_t idleNState      = sample(0x80006DAC + 0xC4);
    // Case-0 initial loader thread NThread at 0x80006074; iNState at +0xC4.
    // iCurrentThread points here once Init1's loader thread becomes current.
    uint32_t loaderNState    = sample(0x80006074 + 0xC4);
    // Bootstrap thread at 0x80003CD0 (per series5.h:464 — the NULL/initial
    // thread on the boot stack). iNState at +0xC4.
    uint32_t bootNState      = sample(0x80003CD0 + 0xC4);
    // 4th NThread, allocated later in boot, candidate for the supervisor
    // or first user-mode thread. Discovered via iCurrentThread tracking
    // at cycle ~247M (= when CPSR escalates to User mode).
    uint32_t mysteryNState   = sample(0x800072D0 + 0xC4);
    uint32_t lcdCtl          = currentLcdControl();

    // Track CPSR mode escalation: 0x13 (SVC) → 0x1B (Undef) → 0x12 (IRQ) →
    // 0x17 (Abort) → 0x10 (User). Boot success requires reaching User mode
    // for EFile.exe / WindowSrv to run. Map them to a numeric "depth" so a
    // simple monotonic max captures progress.
    auto *cpu = getArmCpu();
    uint32_t cpsrMode = cpu ? (cpu->getCPSR() & 0x1F) : 0;
    // Sample User-mode PCs to characterise WHERE user-mode runs once
    // it's reached. Logged sparsely so as not to flood stderr.
    if (cpsrMode == 0x10 && cpu) {
        userModeCounter++;
        // Track unique user-mode PCs in a small fixed-size set so we
        // can dump them at end of boot. 64 slots is plenty — Symbian
        // EUser+kernel ROM regions number maybe 10-30 entry points.
        uint32_t userPc = cpu->getGPR(15) - 0xC;
        bool seen = false;
        for (int i = 0; i < userModePcCount; i++) {
            if (userModePcs[i] == userPc) { seen = true; break; }
        }
        if (!seen && userModePcCount < (int)(sizeof(userModePcs)/sizeof(userModePcs[0]))) {
            userModePcs[userModePcCount++] = userPc;
            std::fprintf(stderr,
                "[s5-kernel]   user-mode NEW PC #%d=0x%08x sample#=%llu\n",
                userModePcCount, userPc, (unsigned long long)userModeCounter);
        }
    }
    auto modeDepth = [](uint32_t m) -> uint32_t {
        switch (m) {
        case 0x13: return 1;  // SVC
        case 0x1B: return 2;  // Undef (post-first-scheduling)
        case 0x12: return 3;  // IRQ
        case 0x17: return 4;  // Abort
        case 0x11: return 5;  // FIQ
        case 0x10: return 6;  // User — boot reached user-mode!
        default:   return 0;
        }
    };
    uint32_t depth = modeDepth(cpsrMode);
    uint32_t highest = modeDepth(highestCpsrMode);
    if (depth > highest) {
        std::fprintf(stderr,
            "[s5-kernel] CPSR escalated to mode 0x%02x (depth=%u) at cycles=~%llu\n",
            cpsrMode, depth, (unsigned long long)callCount);
        highestCpsrMode = cpsrMode;
        if (cpsrMode == 0x10 && firstUserModeAt == 0) {
            firstUserModeAt = (uint32_t)callCount;
            // Log the user-mode PC and a few instruction bytes from
            // there — confirms whether user-mode code is real (i.e.
            // EFile.exe loaded) or zeros (= kernel hasn't actually
            // mapped EFile into user RAM yet).
            uint32_t userPc = cpu->getGPR(15) - 0xC;
            auto insn = readVirtualDebug(userPc, V32);
            auto fb0  = readVirtualDebug(0x80004080, V32);
            auto fb4  = readVirtualDebug(0x80004084, V32);
            std::fprintf(stderr,
                "[s5-kernel]   user-mode PC=0x%08x insn=0x%08x  "
                "*0x80004080=0x%08x *0x80004084=0x%08x\n",
                userPc, insn.value_or(0xDEAD0000),
                fb0.value_or(0xDEAD0000), fb4.value_or(0xDEAD0000));
        }
    }

    // Kernel globals — log on change.
    if (curThread != lastCurrentThread) {
        std::fprintf(stderr,
            "[s5-kernel] iCurrentThread: 0x%08x -> 0x%08x at cycles=~%llu\n",
            lastCurrentThread, curThread, (unsigned long long)callCount);
        lastCurrentThread = curThread;
        if (curThread != 0x80006DAC && curThread != 0 &&
            supervisorThreadFirstSeenAt == 0) {
            supervisorThreadFirstSeenAt = (uint32_t)callCount;
            std::fprintf(stderr,
                "[s5-kernel] *** non-idle thread became current at cycles=~%llu — "
                "supervisor / loader spawn may have succeeded\n",
                (unsigned long long)callCount);
        }
    }
    if (reschedFlag != lastRescheduleFlag) {
        std::fprintf(stderr,
            "[s5-kernel] iRescheduleNeededFlag: %u -> %u at cycles=~%llu\n",
            lastRescheduleFlag, reschedFlag, (unsigned long long)callCount);
        lastRescheduleFlag = reschedFlag;
    }
    if (idleNState != lastIdleNState) {
        std::fprintf(stderr,
            "[s5-kernel] NThread@0x80006DAC (idle/null) iNState: 0x%08x -> 0x%08x at cycles=~%llu\n",
            lastIdleNState, idleNState, (unsigned long long)callCount);
        lastIdleNState = idleNState;
    }
    if (loaderNState != lastLoaderNState) {
        std::fprintf(stderr,
            "[s5-kernel] NThread@0x80006074 (case-0 loader) iNState: 0x%08x -> 0x%08x at cycles=~%llu\n",
            lastLoaderNState, loaderNState, (unsigned long long)callCount);
        lastLoaderNState = loaderNState;
    }
    if (bootNState != lastBootNState) {
        std::fprintf(stderr,
            "[s5-kernel] NThread@0x80003CD0 (boot/init) iNState: 0x%08x -> 0x%08x at cycles=~%llu\n",
            lastBootNState, bootNState, (unsigned long long)callCount);
        lastBootNState = bootNState;
    }
    if (mysteryNState != lastMysteryNState) {
        std::fprintf(stderr,
            "[s5-kernel] NThread@0x800072D0 (4th-alloc/supervisor?) iNState: 0x%08x -> 0x%08x at cycles=~%llu\n",
            lastMysteryNState, mysteryNState, (unsigned long long)callCount);
        lastMysteryNState = mysteryNState;
    }
    if (lcdCtl != lastLcdControl) {
        std::fprintf(stderr,
            "[s5-kernel] *** LCDCON written: 0x%08x -> 0x%08x at cycles=~%llu\n",
            lastLcdControl, lcdCtl, (unsigned long long)callCount);
        lastLcdControl = lcdCtl;
    }

    // Watch the supervisor sub-object's vtable slot (0x80006F10) and the
    // +0x10 slot (0x80006F20) — the latter is read by FUN_5002DF60 line 49750:
    //   if (*(int *)(param_2 + 0x10) == 0) { ... } else { FUN_500384F0(...) }
    // If +0x10 contains a stale uninitialised pointer (e.g., 0x50019280 = ROM
    // panic stub address as data), the else branch dereferences it and the
    // boot diverges to the panic loop.
    uint32_t watch80006F10 = sample(0x80006F10);
    if (watch80006F10 != lastWatch80006F10) {
        std::fprintf(stderr,
            "[s5-kernel] WATCH 0x80006F10 (sub-obj vtable): 0x%08x -> 0x%08x at cycles=~%llu\n",
            lastWatch80006F10, watch80006F10, (unsigned long long)callCount);
        lastWatch80006F10 = watch80006F10;
    }
    uint32_t watch80006F20 = sample(0x80006F20);
    if (watch80006F20 != lastWatch80006F20) {
        std::fprintf(stderr,
            "[s5-kernel] WATCH 0x80006F20 (sub-obj +0x10): 0x%08x -> 0x%08x at cycles=~%llu\n",
            lastWatch80006F20, watch80006F20, (unsigned long long)callCount);
        lastWatch80006F20 = watch80006F20;
    }
    // Watch 0x80006F38 too — that's the value at +0x10 (sub-obj +0x10
    // points there), and FUN_500384F0 line 0x500384fc does LDR R4,[R6]
    // = *param_2 = *(0x80006F38). The first word at the destination
    // determines the kernel's branch into FUN_50019280 panic.
    uint32_t watch80006F38 = sample(0x80006F38);
    if (watch80006F38 != lastWatch80006F38) {
        std::fprintf(stderr,
            "[s5-kernel] WATCH 0x80006F38 (sub-obj+0x28 / target of +0x10): 0x%08x -> 0x%08x at cycles=~%llu\n",
            lastWatch80006F38, watch80006F38, (unsigned long long)callCount);
        lastWatch80006F38 = watch80006F38;
    }

    // Watch the supervisor's saved-context fields. The scheduler dispatcher
    // at 0x500195A4 loads these to restore a thread's CPU state. For a
    // never-run thread these MUST be initialised by the thread-create code
    // (iPc = entry fn, iSpsr = initial mode flags, iSp = stack top, iLr =
    // exit handler). If they stay zero, dispatch jumps to PC=0 — the
    // scheduler refuses to dispatch and the thread never runs.
    //
    // Saved-context layout per FUN_500193F8 (the SAVE function):
    //   +0xE4..+0x114: R0-R12 (13 regs)
    //   +0x118: SP of saved mode
    //   +0x11C: LR of saved mode
    //   +0x120: saved PC (iPc)
    //   +0x124: saved SPSR (iCpsr)
    //
    // For the supervisor (NThread base 0x80006DAC):
    //   iPc   at virt 0x80006ECC (= 0x80006DAC + 0x120)
    //   iCpsr at virt 0x80006ED0 (= 0x80006DAC + 0x124)
    //   iSp   at virt 0x80006EC4 (= 0x80006DAC + 0x118)
    uint32_t watchSupIPc   = sample(0x80006DAC + 0x120);
    uint32_t watchSupICpsr = sample(0x80006DAC + 0x124);
    uint32_t watchSupISp   = sample(0x80006DAC + 0x118);
    if (watchSupIPc != lastSupIPc) {
        std::fprintf(stderr,
            "[s5-kernel] WATCH 0x80006ECC (supervisor iPc):   0x%08x -> 0x%08x at cycles=~%llu\n",
            lastSupIPc, watchSupIPc, (unsigned long long)callCount);
        lastSupIPc = watchSupIPc;
    }
    if (watchSupICpsr != lastSupICpsr) {
        std::fprintf(stderr,
            "[s5-kernel] WATCH 0x80006ED0 (supervisor iCpsr): 0x%08x -> 0x%08x at cycles=~%llu\n",
            lastSupICpsr, watchSupICpsr, (unsigned long long)callCount);
        lastSupICpsr = watchSupICpsr;
    }
    if (watchSupISp != lastSupISp) {
        std::fprintf(stderr,
            "[s5-kernel] WATCH 0x80006EC4 (supervisor iSp):   0x%08x -> 0x%08x at cycles=~%llu\n",
            lastSupISp, watchSupISp, (unsigned long long)callCount);
        lastSupISp = watchSupISp;
    }

    // One-shot dump of the supervisor NThread block once it's been
    // constructed. Triggered when the vtable pointer at offset 0 becomes
    // non-zero (and not the 0xa5a5a5a5 SVC stack fill pattern). Helps
    // identify why vtable+0x14 (called from FUN_5002EB0C in Construct's
    // container-add path) doesn't return for the supervisor thread.
    uint32_t supervisorVtable = sample(0x80006DAC);
    if (!supervisorBlockDumped && supervisorVtable != 0 &&
        supervisorVtable != 0xa5a5a5a5 && supervisorVtable != 0xDEADBEEFu) {
        supervisorBlockDumped = true;
        std::fprintf(stderr, "[s5-kernel] supervisor NThread @ 0x80006DAC dump (vtable=0x%08x):\n",
            supervisorVtable);
        for (int off = 0; off < 0x180; off += 16) {
            uint32_t w0 = sample(0x80006DAC + off);
            uint32_t w1 = sample(0x80006DAC + off + 4);
            uint32_t w2 = sample(0x80006DAC + off + 8);
            uint32_t w3 = sample(0x80006DAC + off + 12);
            std::fprintf(stderr,
                "  +0x%03x  %08x %08x %08x %08x\n",
                off, w0, w1, w2, w3);
        }
        // Also dump 16 vtable entries from supervisorVtable
        std::fprintf(stderr, "[s5-kernel] supervisor vtable @ 0x%08x dump:\n",
            supervisorVtable);
        for (int off = 0; off < 0x80; off += 16) {
            uint32_t w0 = sample(supervisorVtable + off);
            uint32_t w1 = sample(supervisorVtable + off + 4);
            uint32_t w2 = sample(supervisorVtable + off + 8);
            uint32_t w3 = sample(supervisorVtable + off + 12);
            std::fprintf(stderr,
                "  vt+0x%02x  %08x %08x %08x %08x\n",
                off, w0, w1, w2, w3);
        }
        // The container-add path in FUN_5002EB0C calls vtable+0x14 on a
        // sub-object of the NThread at offset +0x164 (= 0x80006F10 for the
        // supervisor). That sub-object has its own vtable pointer at
        // offset 0. Dump it — the divergence point is the LDR PC,[R3,#0x14]
        // at 0x5002eb2c using R3 = *(sub-object).
        uint32_t subObj = 0x80006DAC + 0x164;
        uint32_t subVtable = sample(subObj);
        std::fprintf(stderr, "[s5-kernel] supervisor sub-obj @ 0x%08x vtable=0x%08x\n",
            subObj, subVtable);
        if (subVtable >= 0x50000000 && subVtable < 0x50800000) {
            std::fprintf(stderr, "[s5-kernel] sub-obj vtable @ 0x%08x dump:\n",
                subVtable);
            for (int off = 0; off < 0x40; off += 16) {
                uint32_t w0 = sample(subVtable + off);
                uint32_t w1 = sample(subVtable + off + 4);
                uint32_t w2 = sample(subVtable + off + 8);
                uint32_t w3 = sample(subVtable + off + 12);
                std::fprintf(stderr,
                    "  vt+0x%02x  %08x %08x %08x %08x\n",
                    off, w0, w1, w2, w3);
            }
        } else {
            std::fprintf(stderr,
                "[s5-kernel]   sub-obj vtable 0x%08x is NOT in ROM range — "
                "this is the divergence cause: vtable+0x14 dispatches to garbage\n",
                subVtable);
        }
        // For comparison, also dump the case-0 thread's sub-object — it
        // works (the kernel reaches LCDCON write through it). Same offset
        // pattern: NThread@0x80006074 + 0x68 holds the sub-obj pointer,
        // sub-obj's vtable is at offset 0.
        uint32_t case0SubObjPtr = sample(0x80006074 + 0x68);
        uint32_t case0SubObjVtable = sample(case0SubObjPtr);
        std::fprintf(stderr,
            "[s5-kernel] case-0 thread sub-obj: NThread+0x68=0x%08x ; subobj@0x%08x vtable=0x%08x\n",
            case0SubObjPtr, case0SubObjPtr, case0SubObjVtable);
    }

    // Experimental fix: scrub the supervisor sub-object when its memory
    // shows the SVC-stack debug-fill pattern (0xA5A5A5A5). FUN_5000F69C
    // would normally write the vtable at offset 0 and the allocator
    // (thunk_FUN_50030734) would zero the rest. In our boot the slot at
    // 0x80006F10 is reused memory that previously held the debug fill,
    // and FUN_5000F69C's vtable-write doesn't appear to land there (likely
    // a memory-aliasing or MMU-shadow issue with the supervisor's SVC
    // stack frame). Continuously patching each poll catches the case
    // where the kernel re-fills the slot between dispatches.
    //
    // Zero the entire 0x24-byte sub-object then write the vtable at +0,
    // matching what a freshly-allocated sub-object from FUN_5000F69C
    // looks like. Also scrub 0xA5A5A5A5 fill anywhere in the supervisor
    // NThread block region (0x80006DAC..0x80006F40) since other fields
    // may also have inherited debug-fill from the SVC stack.
    if (fixSupervisorVtable) {
        uint32_t subObjAddr = 0x80006F10;
        uint32_t subVt = sample(subObjAddr);
        if (subVt == 0xA5A5A5A5u) {
            // Scrub 0xA5A5A5A5 across the supervisor block region.
            int scrubbed = 0;
            for (uint32_t addr = 0x80006DAC; addr < 0x80006F40; addr += 4) {
                if (sample(addr) == 0xA5A5A5A5u) {
                    writeVirtual(0u, addr, V32);
                    scrubbed++;
                }
            }
            // Then write the sub-object vtable at offset 0 (DAT_5000F6E4
            // in ROM constant pool).
            writeVirtual(0x50028444u, subObjAddr, V32);
            if (!supervisorVtablePatched) {
                supervisorVtablePatched = true;
                std::fprintf(stderr,
                    "[s5-kernel] PATCHED supervisor region 0x80006DAC..0x80006F40: "
                    "scrubbed %d 0xA5A5A5A5 words + set sub-obj vtable=0x50028444 "
                    "at cycles=~%llu\n",
                    scrubbed, (unsigned long long)callCount);
            }
        }
    }
}

MaybeU32 Emulator::readRegion2(uint32_t physAddr, ValueSize valueSize) const {
    (void)valueSize;
    uint32_t off = physAddr & 0x0FFFFFFF;
    if (std::getenv("PSION_S5_REGION2_LOG")) {
        static int rcnt = 0;
        if (rcnt < 200) {
            const_cast<Emulator*>(this)->log(
                "[r2-read] off=%08x size=%d pc=%08x lr=%08x",
                off, (int)valueSize, cpu.getGPR(15) - 8, cpu.getGPR(14));
            rcnt++;
        }
    }
    // Real Series 5 hardware has an ETNA companion at chip-select nCS1
    // (0x20000000). Trace shows the kernel writes ETNA register offsets
    // (IntClear=0x08, wake1=0x0C, etc.) — same layout as 5mx. Delegate
    // to the same ETNA emulator that 5mx (Windermere) uses.
    if (useRealEtna && off < 0x40) {
        if (valueSize == V8 || valueSize == V32)
            return MaybeU32{etna.readReg8(off)};
    }
    switch (region2Type) {
    case R2_ZERO:  return MaybeU32{0};
    case R2_ONES:  return MaybeU32{0xFFFFFFFFu};
    case R2_COUNT: return MaybeU32{region2ReadCount++};
    case R2_LATCH: {
        return MaybeU32{region2Latch[(off >> 4) & 0xF]};
    }
    }
    return MaybeU32{0};
}

bool Emulator::writeRegion2(uint32_t value, uint32_t physAddr, ValueSize valueSize) {
    (void)valueSize;
    uint32_t off = physAddr & 0x0FFFFFFF;
    if (std::getenv("PSION_S5_REGION2_LOG")) {
        static int wcnt = 0;
        if (wcnt < 200) {
            log("[r2-write] off=%08x value=%08x size=%d pc=%08x lr=%08x",
                off, value, (int)valueSize, cpu.getGPR(15) - 8, cpu.getGPR(14));
            wcnt++;
        }
    }
    if (useRealEtna && off < 0x40) {
        if (valueSize == V8) {
            etna.writeReg8(off, value & 0xFF);
            return true;
        }
        if (valueSize == V32) {
            etna.writeReg8(off,        value        & 0xFF);
            etna.writeReg8(off + 1,   (value >>  8) & 0xFF);
            etna.writeReg8(off + 2,   (value >> 16) & 0xFF);
            etna.writeReg8(off + 3,   (value >> 24) & 0xFF);
            return true;
        }
    }
    if (region2Type == R2_LATCH) {
        region2Latch[(off >> 4) & 0xF] = value;
    }
    return true;
}

// PS7110-generic LCD render lives in core/clps7110.cpp; Series 5 inherits
// it by being a CLPS7110::Emulator subclass. Device-specific behaviour
// (RAM sizing, ETNA-like region 2 companion, sweep knobs) stays here.

// Series-5 specific debugPC. The clps7111 base hooks (Osaris-pinned PCs)
// are gated off via hasOsarisDebugHooks(); these hooks fire on the
// equivalents we've located in the Series 5 ROM.
void Emulator::debugPC(uint32_t pc) {
    // Track first-hit only. Repeated hits log nothing — the cycle stamp
    // on the first hit is enough to know whether the kernel reached the
    // call site at all.
    static bool seenLcdInit = false;
    static bool seenIdleTail = false;

    // PSION_S5_CF_FLOW=1 — PCMCIA socket-driver / PSU flow trace (off by
    // default). Retained diagnostic for the CF power-up path: logs how far
    // socket enumeration gets and the PSU voltage-check decision. This is the
    // instrumentation used to root-cause the socket-Vcc ADC fix; see
    // docs/series5-cf-investigation.md. FUN_5007df00 (0x7df00) is the
    // "configure/enable I/O window" call (the 5mx equivalent of writing
    // SktCtrl=0x41); FUN_5007d078 (0x7d078) sets SktCtrl bit6 (enable).
    // If those never fire, the driver never decided to enable the card.
    static int cfFlow = -1;
    if (cfFlow < 0) {
        const char *e = std::getenv("PSION_S5_CF_FLOW");
        cfFlow = (e && e[0] == '1') ? 1 : 0;
    }
    if (cfFlow) {
        struct Tgt { uint32_t pc; const char *name; };
        static const Tgt tgts[] = {
            {0x7dcf4, "FUN_5007dcf4 socket-prep"},
            {0x7dd90, "FUN_5007dd90 interrogate-socket"},
            {0x7de4c, "FUN_5007de4c reset/power"},
            {0x7dec0, "FUN_5007dec0 reset-line"},
            {0x7df00, "FUN_5007df00 CONFIGURE-IO-WINDOW"},
            {0x7d078, "FUN_5007d078 SktCtrl-bit6-ENABLE"},
            {0x7cf64, "FUN_5007cf64 write-reg0"},
            {0x7e0ac, "FUN_5007e0ac"},
            {0x7dcc0, "FUN_5007dcc0"},
        };
        static unsigned long long hitCount[sizeof(tgts)/sizeof(tgts[0])] = {};
        for (size_t i = 0; i < sizeof(tgts)/sizeof(tgts[0]); i++) {
            if (pc == tgts[i].pc) {
                if (hitCount[i] < 6) {
                    log("S5 CF_FLOW: %-34s pc=%05x r0=%08x r1=%08x lr=%08x (#%llu)",
                        tgts[i].name, pc, getGPR(0), getGPR(1), getGPR(14),
                        hitCount[i] + 1);
                }
                hitCount[i]++;
            }
        }
        // FUN_5002110c (0x2110c): the socket power-up state machine.
        // r0 = state object. [0]=state (0..n), [1]=tick counter,
        // [9]=delay threshold, [2]=? . Log the state each entry so we can
        // watch it advance (or restart). Cap to keep the log bounded.
        if (pc == 0x2110c) {
            static unsigned long long psuHits = 0;
            if (psuHits < 80) {
                uint32_t obj = getGPR(0);
                uint32_t s0 = 0, s1 = 0, s9 = 0, s2 = 0;
                if (auto v = readVirtualDebug(obj,        V32); v.has_value()) s0 = v.value();
                if (auto v = readVirtualDebug(obj + 4,    V32); v.has_value()) s1 = v.value();
                if (auto v = readVirtualDebug(obj + 8,    V32); v.has_value()) s2 = v.value();
                if (auto v = readVirtualDebug(obj + 0x24, V32); v.has_value()) s9 = v.value();
                log("S5 CF_PSU: FUN_5002110c obj=%08x state=%u tick=%u thresh=%u [2]=%08x",
                    obj, s0, s1, s9, s2);
            }
            psuHits++;
        }
        // 0x21134 = `MOV r5,r0` right after FUN_5001f6b8 returns iVar3 (the
        // PSU object). Resolve its [0x2c] sub-vtable and the [0x10] method
        // (= VccVoltCheck) cleanly here. 0x21210 holds the readiness result
        // in r4 (= SUBS r4,r0,#0 at 0x2120c).
        if (pc == 0x21134) {
            static unsigned long long h = 0;
            if (h < 30) {
                uint32_t obj = getGPR(0), subv = 0, method = 0;
                if (auto v = readVirtualDebug(obj + 0x2c, V32); v.has_value()) subv = v.value();
                if (subv) if (auto v = readVirtualDebug(subv + 0x10, V32); v.has_value()) method = v.value();
                log("S5 CF_RDY: iVar3(PSU)=%08x subvtbl=%08x VccVoltCheck=%08x", obj, subv, method);
            }
            h++;
        }
        if (pc == 0x21210) {
            static unsigned long long h = 0;
            if (h < 30) {
                log("S5 CF_RDY: VccVoltCheck -> %d (r4=%08x)", (int32_t)getGPR(4), getGPR(4));
            }
            h++;
        }
        // Resolve the runtime target of the func_0x5002700c veneer
        // (0x27010 = `LDR pc,[r12]`; r12 = GOT slot). And capture the HW
        // voltage read's return value at 0x242d0 (right after the BL).
        if (pc == 0x27010 && cfCard.inserted()) {
            static unsigned long long h = 0;
            if (h < 8) {
                uint32_t r12 = getGPR(12), tgt = 0;
                if (auto v = readVirtualDebug(r12, V32); v.has_value()) tgt = v.value();
                log("S5 CF_RDY: veneer 0x5002700c r12=%08x -> target=%08x  r0(arg)=%08x",
                    r12, tgt, getGPR(0));
            }
            h++;
        }
        if (pc == 0x242d0) {
            static unsigned long long h = 0;
            if (h < 12) {
                log("S5 CF_RDY: HW voltread(0x5002700c) -> %d (r0=%08x)",
                    (int32_t)getGPR(0), getGPR(0));
            }
            h++;
        }
        // Alt-path gate in FUN_500243d8: 0x243e4 = SUBS r1,r0,#0 (r1 =
        // FUN_5001e6d8 result); 0x243f0 = CMP r0,#0 (r0 = *(ctrl+0x1c)).
        // Both must be != 0 for the alternate voltage reader to run.
        if (pc == 0x243e4) {
            static unsigned long long h = 0;
            if (h < 12) log("S5 CF_RDY: gate1 FUN_5001e6d8()=%d", (int32_t)getGPR(1));
            h++;
        }
        if (pc == 0x243f0) {
            static unsigned long long h = 0;
            if (h < 12) log("S5 CF_RDY: gate2 *(ctrl+0x1c)=%08x", getGPR(0));
            h++;
        }
        // 0x21228 = RSB r3,r4,r0 in PSU case 2: r4 = read voltage
        // (VccVoltCheck ret, only reached when >= 0), r0 = target voltage.
        // Advance to state 3 requires (target - read) < target/4.
        if (pc == 0x21228) {
            static unsigned long long h = 0;
            if (h < 20) log("S5 CF_RDY: voltcmp read=%d target=%d (need read>%d)",
                            (int32_t)getGPR(4), (int32_t)getGPR(0),
                            (int32_t)getGPR(0) - (int32_t)getGPR(0)/4);
            h++;
        }
    }

    // Cold-boot date fix lives in clps7111.cpp now — every RTCDR read
    // refreshes from getRTC() so the kernel always sees the host clock.
    // The remaining year-1274 / year-2024 mis-display was traced (via
    // PSION_S5_DATE_TRACE, kept below) to an off-by-one in our ARM710
    // LSL #N carry-out: arm710.cpp:2656 read Rm[31-N] instead of
    // Rm[32-N], breaking 64-bit shift-and-subtract division. EUSER's
    // TTime/DAY_us conversion produced 2_943_449 instead of 740_126
    // days for year-2026 TTime us, which led FUN_50031298 to compute
    // year 8058 (and downstream displays to show "7 Aug 1274" via
    // signed-wrap arithmetic). Fixing the LSL carry-out in arm710.cpp
    // makes the kernel's TTime->TDateTime produce the correct year.

    // PSION_S5_DATE_TRACE=1 — leave the instrumentation that pinned
    // down the LSL bug. Hooks the FUN_50031298 alt-path so r0 (the
    // computed year) and r9 (the days-count produced by FUN_5003e800)
    // get logged; if the year drifts again we can spot it without
    // re-tracing EUSER.
    if (std::getenv("PSION_S5_DATE_TRACE")) {
        static int hits = 0;
        if (pc == 0x31298u && hits < 100) {
            uint32_t r0 = getGPR(0);
            uint32_t r1 = getGPR(1);
            uint32_t lo = 0, hi = 0;
            if (auto v = readVirtualDebug(r1,     V32); v.has_value()) lo = v.value();
            if (auto v = readVirtualDebug(r1 + 4, V32); v.has_value()) hi = v.value();
            log("S5: DATE_TRACE #%d  ENTER FUN_50031298  out=%08x  *in lo=%08x hi=%08x  LR=%08x",
                ++hits, r0, lo, hi, getGPR(14));
        }
        if (pc == 0x31414u && hits < 100) {
            log("S5: DATE_TRACE #%d  year-out = %d, days-in = %d",
                ++hits, (int32_t)getGPR(0), (int32_t)getGPR(9));
        }
    }

    // PSION_S5_CORRUPT_TRACE: log every transition where r0 becomes
    // KErrCorrupt (-20 = 0xFFFFFFEC). debugPC fires AFTER each
    // instruction with pc=that instruction's address, so we compare r0
    // pre/post via a static cache and log whenever it newly takes the
    // value -20. Catches all encodings: literal-pool load, MVN, SUB,
    // RSB, function-return-from-callee, etc.
    static int corruptTrace = -1;
    if (corruptTrace < 0) {
        const char *e = std::getenv("PSION_S5_CORRUPT_TRACE");
        corruptTrace = (e && e[0] == '1') ? 1 : 0;
    }
    if (corruptTrace) {
        static uint32_t prevR0 = 0;
        uint32_t r0 = getGPR(0);
        if (r0 == 0xFFFFFFECu && prevR0 != 0xFFFFFFECu) {
            static uint64_t count = 0;
            ++count;
            if (count <= 200 || (count & 0xFFF) == 0) {
                uint32_t r5 = getGPR(5);
                uint32_t r5p4 = 0;
                if (auto v = readVirtualDebug(r5 + 4, V32); v.has_value())
                    r5p4 = v.value();
                log("S5: r0:=KErrCorrupt @PC=0x%08x #%llu  LR=0x%08x R1=%08x R2=%08x R4=%08x R5=%08x [R5+4]=%08x R6=%08x R7=%08x",
                    pc, (unsigned long long)count, getGPR(14),
                    getGPR(1), getGPR(2), getGPR(4),
                    r5, r5p4, getGPR(6), getGPR(7));
                // Dump 0x40 bytes of the struct at r5 (and at [r5+4]) so
                // we can identify which file/object the read is targeting.
                // Filename strings should appear as ASCII or UCS-2 here.
                if (pc == 0x67dc0u || pc == 0x3741b8u || pc == 0x58868u) {
                    // Dump 96 bytes as hex + ASCII to identify the file/object
                    // by any TDesC name strings stored inside the struct.
                    auto dumpat = [&](uint32_t base, const char *label) {
                        char hexbuf[3 * 96 + 1];
                        char ascbuf[96 + 1];
                        int hp = 0, ap = 0;
                        for (int i = 0; i < 96 && hp < (int)sizeof(hexbuf) - 4; i++) {
                            auto v = readVirtualDebug(base + i, V8);
                            uint8_t b = (uint8_t)(v.value_or(0xAA) & 0xFF);
                            hp += snprintf(hexbuf + hp, sizeof(hexbuf) - hp, "%02x ", b);
                            ascbuf[ap++] = (b >= 0x20 && b < 0x7F) ? (char)b : '.';
                        }
                        ascbuf[ap] = 0;
                        log("S5: corrupt struct %s @0x%08x:", label, base);
                        log("    HEX: %s", hexbuf);
                        log("    ASC: %s", ascbuf);
                    };
                    dumpat(r5, "[r5]");
                    if (r5p4 >= 0x50000000u || r5p4 < 0x80000000u)
                        dumpat(r5p4, "[r5+4]->");
                }
            }
        }
        prevR0 = r0;

        // At PC=0x3e614 (entry to wrapper around int64_add) when dst is the
        // failing file size at 0x502d98, capture inputs + LR so we can see
        // who's calling extend-file-size (the outer caller's address).
        // Also walk back along the stack to find the caller-of-caller.
        if (pc == 0x3e614u && getGPR(0) == 0x502d98u) {
            uint32_t r1 = getGPR(1);
            uint32_t dLo=0, dHi=0;
            if (auto v = readVirtualDebug(r1,     V32); v.has_value()) dLo = v.value();
            if (auto v = readVirtualDebug(r1 + 4, V32); v.has_value()) dHi = v.value();
            uint32_t lr = getGPR(14);
            // Read 4 stack words to see callee-saved LR
            uint32_t sp = getGPR(13);
            uint32_t s0=0,s1=0,s2=0,s3=0;
            if (auto v = readVirtualDebug(sp,      V32); v.has_value()) s0 = v.value();
            if (auto v = readVirtualDebug(sp +  4, V32); v.has_value()) s1 = v.value();
            if (auto v = readVirtualDebug(sp +  8, V32); v.has_value()) s2 = v.value();
            if (auto v = readVirtualDebug(sp + 12, V32); v.has_value()) s3 = v.value();
            log("S5: extend-size  delta=%08x%08x  LR=%08x  SP=%08x  stack:[%08x %08x %08x %08x]",
                dHi, dLo, lr, sp, s0, s1, s2, s3);
        }

        // At PC=0x6c0f4 (function entry of extend-stream), capture LR so
        // we see WHO calls extend-stream with a 0x400 delta. The function
        // takes r0=stream-store, r1=delta in bytes.
        if (pc == 0x6c0f4u) {
            uint32_t r0 = getGPR(0);
            uint32_t r1 = getGPR(1);
            if (r0 == 0x502d7cu) {
                static uint64_t ec = 0; ++ec;
                if (ec <= 64 || (ec & 0x3FF) == 0) {
                    log("S5: extend-stream ENTRY #%llu  r0(store)=%08x r1(delta)=%08x  LR=%08x",
                        (unsigned long long)ec, r0, r1, getGPR(14));
                }
            }
        }
        // At PC=0x67d90 (entry to the failing READ function), capture
        // outer LR + a stack frame snapshot so we can chain up to whoever
        // requested an off-the-end read of size 0x238 / 0x140 / 0xf at
        // the cluster-boundary-crossing offsets.
        // At PC=0x67db4 (the BL to PLT 0x6f3dc), dump:
        //   - what the PLT slot (RAM at 0x50400d24) actually points to
        //   - r0 before the BL (= struct + 0x1c address)
        //   - r4 (= computed read_end)
        // This lets us see exactly what virtual method gets called.
        if (pc == 0x67db4u) {
            uint32_t r0 = getGPR(0);
            uint32_t r5 = getGPR(5);
            // Only log when we're heading to the bounds-fail (r5 matches our struct)
            if (r5 == 0x502e4cu) {
                static uint64_t bn = 0; ++bn;
                if (bn <= 30 || (bn & 0xFFF) == 0) {
                    uint32_t pltSlot = 0;
                    if (auto v = readVirtualDebug(0x50400d24u, V32); v.has_value()) pltSlot = v.value();
                    // Dump the actual function bytes (32 bytes)
                    char buf[3*32+1];
                    int p=0;
                    for (int i = 0; i < 32 && p < (int)sizeof(buf)-4; i++) {
                        auto bv = readVirtualDebug(pltSlot + i, V8);
                        p += snprintf(buf+p, sizeof(buf)-p, "%02x ", (unsigned)(bv.value_or(0xAA)&0xFF));
                    }
                    log("S5: bounds-getter @PC=0x67db4 #%llu  r0(addr)=%08x  PLT_RAM[0x50400d24]=%08x  fn_bytes=%s",
                        (unsigned long long)bn, r0, pltSlot, buf);
                }
            }
        }
        if (pc == 0x67d90u) {
            uint32_t r0 = getGPR(0);
            uint32_t r1 = getGPR(1);
            uint32_t r2 = getGPR(2);
            // Filter: only the bounds-tripping arguments
            if (r0 == 0x502e4cu && (r2 == 0x238u || r2 == 0x140u || r2 == 0xfu)) {
                static uint64_t ec = 0; ++ec;
                uint32_t sp = getGPR(13);
                uint32_t s0=0,s1=0,s2=0,s3=0,s4=0,s5=0;
                if (auto v = readVirtualDebug(sp,      V32); v.has_value()) s0 = v.value();
                if (auto v = readVirtualDebug(sp +  4, V32); v.has_value()) s1 = v.value();
                if (auto v = readVirtualDebug(sp +  8, V32); v.has_value()) s2 = v.value();
                if (auto v = readVirtualDebug(sp + 12, V32); v.has_value()) s3 = v.value();
                if (auto v = readVirtualDebug(sp + 16, V32); v.has_value()) s4 = v.value();
                if (auto v = readVirtualDebug(sp + 20, V32); v.has_value()) s5 = v.value();
                log("S5: read-fn ENTRY #%llu  r0(store)=%08x r1(off)=%08x r2(len)=%08x r3(buf)=%08x  LR=%08x SP=%08x  stk:[%08x %08x %08x %08x %08x %08x]",
                    (unsigned long long)ec,
                    r0, r1, r2, getGPR(3), getGPR(14), sp,
                    s0, s1, s2, s3, s4, s5);
            }
        }
        // At PC=0x68840 (the "extend by 1 cluster" wrapper that calls
        // 0x6c0f4 with r1=cluster_size), capture LR + r0/r1 to see the
        // OUTER caller that decides "extend now". Also dump stack to see
        // the wider chain.
        if (pc == 0x68840u) {
            uint32_t r0 = getGPR(0);
            if (r0 == 0x502d2cu || r0 == 0x502d7cu  // file-struct or its [+4]
                || (r0 >= 0x502d00u && r0 < 0x503000u)) {
                static uint64_t ec = 0; ++ec;
                if (ec <= 30 || (ec & 0x3FF) == 0) {
                    uint32_t sp = getGPR(13);
                    uint32_t s0=0,s1=0,s2=0,s3=0,s4=0;
                    if (auto v = readVirtualDebug(sp,      V32); v.has_value()) s0 = v.value();
                    if (auto v = readVirtualDebug(sp +  4, V32); v.has_value()) s1 = v.value();
                    if (auto v = readVirtualDebug(sp +  8, V32); v.has_value()) s2 = v.value();
                    if (auto v = readVirtualDebug(sp + 12, V32); v.has_value()) s3 = v.value();
                    if (auto v = readVirtualDebug(sp + 16, V32); v.has_value()) s4 = v.value();
                    log("S5: 1cluster-extend ENTRY #%llu  r0=%08x r1=%08x r2=%08x r3=%08x  LR=%08x SP=%08x  stk:[%08x %08x %08x %08x %08x]",
                        (unsigned long long)ec, r0, getGPR(1), getGPR(2), getGPR(3),
                        getGPR(14), sp, s0, s1, s2, s3, s4);
                }
            }
        }
        // At PC=0x6c12c (BL to delta-computing function) capture INPUTS:
        //   r0 = stack slot where delta will be written
        //   r1 = data-stream pointer
        //   r5 = same data ptr (saved earlier in the function)
        //   r6 = stream-store object (its [+0x1c] is the file-size)
        // Also dump 64 bytes at r5 to see what's being written.
        if (pc == 0x6c12cu) {
            uint32_t r0 = getGPR(0);
            uint32_t r1 = getGPR(1);
            uint32_t r5 = getGPR(5);
            uint32_t r6 = getGPR(6);
            uint32_t curSizeLo=0, curSizeHi=0;
            if (auto v = readVirtualDebug(r6 + 0x1c, V32); v.has_value()) curSizeLo = v.value();
            if (auto v = readVirtualDebug(r6 + 0x20, V32); v.has_value()) curSizeHi = v.value();
            // Only log when this is the failing file (curSize matches our pattern)
            if (curSizeLo >= 0x20000u && curSizeLo < 0x40000u) {
                static uint64_t entries = 0; ++entries;
                if (entries <= 30 || (entries & 0x3FF) == 0) {
                    log("S5: delta-compute ENTER #%llu  r0(slot)=%08x r1(dataPtr)=%08x r5=%08x r6=%08x  curSize=%08x%08x",
                        (unsigned long long)entries, r0, r1, r5, r6,
                        curSizeHi, curSizeLo);
                    // Dump 64 bytes at r1 (data buffer / stream descriptor)
                    char hexbuf[3*64+1], ascbuf[65];
                    int hp=0, ap=0;
                    for (int i = 0; i < 64 && hp < (int)sizeof(hexbuf)-4; i++) {
                        auto v = readVirtualDebug(r1 + i, V8);
                        uint8_t b = (uint8_t)(v.value_or(0xAA) & 0xFF);
                        hp += snprintf(hexbuf+hp, sizeof(hexbuf)-hp, "%02x ", b);
                        ascbuf[ap++] = (b >= 0x20 && b < 0x7F) ? (char)b : '.';
                    }
                    ascbuf[ap]=0;
                    log("    r1-data HEX: %s", hexbuf);
                    log("    r1-data ASC: %s", ascbuf);
                }
            }
        }
        // At PC=0x6c130 (right after the BL returns) capture OUTPUTS:
        //   *r4 = delta written by the just-called function
        //   r0 = return value (status?)
        if (pc == 0x6c130u) {
            uint32_t r4 = getGPR(4);
            uint32_t dLo=0, dHi=0;
            if (auto v = readVirtualDebug(r4,     V32); v.has_value()) dLo = v.value();
            if (auto v = readVirtualDebug(r4 + 4, V32); v.has_value()) dHi = v.value();
            uint32_t r6 = getGPR(6);
            uint32_t curSizeLo=0;
            if (auto v = readVirtualDebug(r6 + 0x1c, V32); v.has_value()) curSizeLo = v.value();
            if (curSizeLo >= 0x20000u && curSizeLo < 0x40000u) {
                static uint64_t exits = 0; ++exits;
                if (exits <= 30 || (exits & 0x3FF) == 0) {
                    log("S5: delta-compute EXIT  #%llu  r0(ret)=%08x  delta=%08x%08x  curSize=%08x",
                        (unsigned long long)exits, getGPR(0), dHi, dLo, curSizeLo);
                }
            }
        }
        // has just executed. The instruction is actually "CMP r4, r0"
        // (Rn=r4 in bits 19:16), so the bounds check Leaves when r4 > r0
        // (read_end_offset > file_size). Log every fail-bound case so
        // we can see file-size r0 and which read offset trips it.
        if (pc == 0x67db8u) {
            uint32_t cmpR0 = getGPR(0);
            uint32_t cmpR4 = getGPR(4);
            if ((int32_t)cmpR4 > (int32_t)cmpR0) {
                static uint64_t ccnt = 0;
                ++ccnt;
                if (ccnt <= 64 || (ccnt & 0xFFF) == 0) {
                    uint32_t r5 = getGPR(5);
                    uint32_t r5p4 = 0;
                    if (auto v = readVirtualDebug(r5 + 4, V32); v.has_value())
                        r5p4 = v.value();
                    log("S5: bounds-FAIL @0x67db8 #%llu  fsize=%08x req-end=%08x off=%08x len=%08x  r5=%08x [r5+4]=%08x LR=%08x",
                        (unsigned long long)ccnt, cmpR0, cmpR4,
                        getGPR(6), getGPR(7),
                        r5, r5p4, getGPR(14));
                    // Dump 96 bytes from the second struct ([r5+4]) at the
                    // exact moment the bounds check trips. fsize was computed
                    // by func([r5+4]+0x1c), so the struct + offset 0x1c is the
                    // input. Looking for a "max_size" field or file mapping
                    // base that explains the 0x27200/0x28a00/0x29600 values.
                    auto dumpat = [&](uint32_t base, const char *label) {
                        char hexbuf[3*96+1]; char ascbuf[97];
                        int hp=0, ap=0;
                        for (int i = 0; i < 96 && hp < (int)sizeof(hexbuf)-4; i++) {
                            auto v = readVirtualDebug(base + i, V8);
                            uint8_t b = (uint8_t)(v.value_or(0xAA) & 0xFF);
                            hp += snprintf(hexbuf+hp, sizeof(hexbuf)-hp, "%02x ", b);
                            ascbuf[ap++] = (b >= 0x20 && b < 0x7F) ? (char)b : '.';
                        }
                        ascbuf[ap]=0;
                        log("S5: bnd-struct %s @0x%08x HEX: %s", label, base, hexbuf);
                        log("S5: bnd-struct %s @0x%08x ASC: %s", label, base, ascbuf);
                    };
                    dumpat(r5p4, "[r5+4]");
                    dumpat(r5p4 + 0x1c, "[r5+4]+0x1c");
                }
            }
        }
    }

    // PSION_S5_MEMCPY_TRACE: log every entry to the kernel's memcpy
    // function at PC=0x5004D800. Logs source/dest/count + caller LR.
    // Used to find which ROM→RAM transfers happen during boot for
    // Agenda/Word/Data app data. Filter externally for the Agenda
    // ROM range (0x5047CC40-0x504825FF approx).
    static int memcpyTrace = -1;
    if (memcpyTrace < 0) {
        const char *e = std::getenv("PSION_S5_MEMCPY_TRACE");
        if (e) memcpyTrace = std::atoi(e);
        else memcpyTrace = 0;
    }
    if (memcpyTrace && pc == 0x4D800) {
        uint32_t dst = getGPR(0);
        uint32_t src = getGPR(1);
        uint32_t cnt = getGPR(2);
        uint32_t lr  = getGPR(14);
        // Filter to ROM sources (src in 0x50000000-0x507FFFFF) AND
        // non-trivial size (>= 16 bytes) to cut noise. mode == 2 logs ALL.
        bool isRom = (src >= 0x50000000u && src < 0x50800000u);
        if (memcpyTrace == 2 || (isRom && cnt >= 16)) {
            static uint64_t count = 0;
            ++count;
            if (count <= 4096) {
                log("S5: memcpy #%llu  dst=0x%08x  src=0x%08x  cnt=%u  lr=0x%08x",
                    (unsigned long long)count, dst, src, cnt, lr);
            }
        }
    }
    // PSION_S5_MEMCPY_VERIFY: at memcpy ENTRY, perform the copy ourselves
    // via readVirtualDebug/writeVirtual. Then let the kernel's memcpy run
    // and verify the destination matches. If our RAM model has any bug
    // where the kernel's store→load roundtrip is broken, this will catch
    // it. We use a delayed-verify pattern: capture (src, dst, cnt) at
    // entry and verify on the NEXT memcpy entry (by which time the
    // previous copy should be complete).
    static int memcpyVerify = -1;
    if (memcpyVerify < 0) {
        const char *e = std::getenv("PSION_S5_MEMCPY_VERIFY");
        memcpyVerify = (e && e[0] == '1') ? 1 : 0;
    }
    if (memcpyVerify && pc == 0x4D800) {
        static uint32_t prevDst = 0, prevSrc = 0, prevCnt = 0;
        static uint64_t prevId = 0;
        // Verify the PREVIOUS memcpy now (it should be complete).
        if (prevId > 0 && prevSrc >= 0x50000000u && prevSrc < 0x50800000u
            && prevCnt > 0 && prevCnt <= 4096) {
            int diffs = 0;
            uint32_t firstDiffOff = 0xFFFFFFFFu;
            for (uint32_t i = 0; i < prevCnt && diffs < 5; i++) {
                auto srcByte = readVirtualDebug(prevSrc + i, V8);
                auto dstByte = readVirtualDebug(prevDst + i, V8);
                uint8_t sb = (uint8_t)srcByte.value_or(0xAA);
                uint8_t db = (uint8_t)dstByte.value_or(0xBB);
                if (sb != db) {
                    if (firstDiffOff == 0xFFFFFFFFu) firstDiffOff = i;
                    diffs++;
                }
            }
            if (diffs > 0) {
                log("S5: memcpy VERIFY FAIL #%llu  src=0x%08x dst=0x%08x cnt=%u  "
                    "first diff @+0x%x  (showing first 5)",
                    (unsigned long long)prevId, prevSrc, prevDst, prevCnt,
                    firstDiffOff);
                for (uint32_t i = 0; i < prevCnt && i < 64; i++) {
                    auto srcByte = readVirtualDebug(prevSrc + i, V8);
                    auto dstByte = readVirtualDebug(prevDst + i, V8);
                    uint8_t sb = (uint8_t)srcByte.value_or(0xAA);
                    uint8_t db = (uint8_t)dstByte.value_or(0xBB);
                    if (sb != db) {
                        log("    +0x%x: src=0x%02x dst=0x%02x", i, sb, db);
                    }
                }
            }
        }
        prevDst = getGPR(0); prevSrc = getGPR(1); prevCnt = getGPR(2);
        prevId++;
    }

    // PSION_S5_BOUNDS_FIX — workaround for the EPOC R1 FAT stream-store
    // bounds check that otherwise emits KErrCorrupt for Word/Agenda/Data
    // during boot. The kernel stores the file size in cluster-aligned
    // increments (0x400 bytes at a time) at struct +0x1c. When a write
    // straddles a cluster boundary (e.g. 0x238 bytes at offset 0x27000
    // where the previous EOF was 0x26e00), the kernel allocates one
    // cluster and stamps file_size = 0x27200 — but the write itself
    // really did fill the new cluster, so the bytes at 0x27200..0x27237
    // are valid in RAM. Subsequent byte-precise reads of that range
    // fail the bounds check at PC=0x50067dbc and Leave KErrCorrupt,
    // which propagates as "Corrupt" dialogs for the affected apps.
    //
    // There are FOUR identical bounds-check sites in the kernel — each
    // sits right after a BL to the same file-size-getter PLT slot at
    // 0x6f3dc, and each compares the size against a computed read-end
    // before MVN-ing KErrCorrupt. The four BL sites are at 0x67c84,
    // 0x67ce8, 0x67d48 and 0x67db4 (different methods on the same
    // stream-store object — Read variants and Write probably). All
    // four exhibit the same lag-behind issue. The fix runs immediately
    // after each BL: if the requested read-end (r4) sits inside the
    // next cluster up from the just-fetched file_size (r0), round r0
    // up to that cluster boundary so the bounds check succeeds.
    //
    // The rounded-up value is only used when it IS large enough to
    // cover the read — reads that genuinely overflow stay failed.
    //
    // Default: ON. PSION_S5_BOUNDS_FIX=0 disables for A/B testing.
    static int boundsFix = -1;
    if (boundsFix < 0) {
        const char *e = std::getenv("PSION_S5_BOUNDS_FIX");
        boundsFix = (e && e[0] == '0') ? 0 : 1;
    }
    // PSION_S5_AGENDA_TRACE=1 — log what's happening just before Agenda
    // dereferences its wild pointer at PC=0x50277e74. Capture r7 and
    // [r7+0x54] (the index that gets <<2'd and added to r7 to form
    // the bad pointer).
    // Also log every bounds-check fall-through at the 3 NON-boot sites
    // (PC 0x67c84/0x67ce8/0x67d48) so we can see which reads Agenda
    // makes that the single-site fix won't catch.
    static int agendaTrace = -1;
    if (agendaTrace < 0) {
        const char *e = std::getenv("PSION_S5_AGENDA_TRACE");
        agendaTrace = (e && e[0] == '1') ? 1 : 0;
    }
    if (agendaTrace) {
        if (pc == 0x67c84u || pc == 0x67ce8u || pc == 0x67d48u) {
            uint32_t r0 = getGPR(0);
            uint32_t r4 = getGPR(4);
            uint32_t r5 = getGPR(5);
            uint32_t r6 = getGPR(6);
            uint32_t r7 = getGPR(7);
            uint32_t r5p4 = 0;
            if (auto v = readVirtualDebug(r5 + 4, V32); v.has_value()) r5p4 = v.value();
            if ((int32_t)r4 > (int32_t)r0) {
                static uint64_t failed = 0; ++failed;
                if (failed <= 40)
                    log("S5: bounds-FAIL @runtime PC=%x #%llu  fsize=%08x req-end=%08x off~=%08x len=%08x  r5=%08x [r5+4]=%08x",
                        pc, (unsigned long long)failed,
                        r0, r4, r6, r7, r5, r5p4);
            }
        }
        if (pc == 0x277d44u) {
            // function entry: MOV r7, r0 just executed
            uint32_t r7 = getGPR(7);
            uint32_t v54 = 0;
            if (auto v = readVirtualDebug(r7 + 0x54, V32); v.has_value()) v54 = v.value();
            static uint64_t enters = 0; ++enters;
            if (enters <= 20)
                log("S5: AGENDA fn-entry @277d3c #%llu  r7=%08x  [r7+0x54]=%08x",
                    (unsigned long long)enters, r7, v54);
        }
        if (pc == 0x277e74u) {
            // about to fault: dump r7, r3, [r7+0x54]
            uint32_t r3 = getGPR(3);
            uint32_t r7 = getGPR(7);
            uint32_t v54 = 0;
            if (auto v = readVirtualDebug(r7 + 0x54, V32); v.has_value()) v54 = v.value();
            log("S5: AGENDA pre-fault r7=%08x [r7+0x54]=%08x  r3(=r7+(v54<<2))=%08x  -> reads [r3+0x74]=0x%08x (will fault if unmapped)",
                r7, v54, r3, r3 + 0x74);
        }
    }

    // Patch only the boot-time bounds-check site (0x67db4) so Word/Data
    // launch cleanly. The runtime sites stay unpatched: relaxing them
    // tips Agenda over from a "Corrupt" dialog into a KERN-EXEC 3 crash
    // a few seconds later because the kernel ends up reading
    // uninitialised tail-cluster bytes through them.
    if (boundsFix && pc == 0x67db4u) {
        uint32_t r0 = getGPR(0);
        uint32_t r4 = getGPR(4);
        if ((int32_t)r4 > (int32_t)r0) {
            uint32_t rounded = (r0 + 0x3FFu) & ~0x3FFu;
            if (rounded >= r4) {
                static uint64_t adjusted = 0;
                ++adjusted;
                if (adjusted <= 16 || (adjusted & 0xFFFF) == 0) {
                    log("S5: BOUNDS_FIX round-up file_size %08x -> %08x  (req-end=%08x)",
                        r0, rounded, r4);
                }
                setGPR(0, rounded);
            }
        }
    }

    // PSION_S5_BYTE_VERIFY: hook PC=0x5004D83C (right AFTER strb r3, [r0], #1
    // in the byte-by-byte memcpy loop). At this point, r3 still holds the
    // value just stored, and r0 has been post-incremented so r0-1 is the
    // address that was just written. Read it back and compare with r3.
    // If they differ, the write→read roundtrip is broken for that physical
    // address. There are 4 strb instructions in the loop (at +0x38, +0x48,
    // +0x58, +0x68); hook all 4 following PCs.
    static int byteVerify = -1;
    if (byteVerify < 0) {
        const char *e = std::getenv("PSION_S5_BYTE_VERIFY");
        byteVerify = (e && e[0] == '1') ? 1 : 0;
    }
    if (byteVerify && (pc == 0x4D83C || pc == 0x4D84C
                    || pc == 0x4D85C || pc == 0x4D86C)) {
        uint32_t r0 = getGPR(0);
        uint32_t r3 = getGPR(3);
        uint32_t writtenAddr = r0 - 1;
        uint8_t  expectedByte = (uint8_t)r3;
        auto readBack = readVirtualDebug(writtenAddr, V8);
        if (readBack.has_value()) {
            uint8_t actualByte = (uint8_t)readBack.value();
            if (actualByte != expectedByte) {
                static uint64_t fails = 0;
                ++fails;
                if (fails <= 64 || (fails & 0xFF) == 0) {
                    log("S5: BYTE_VERIFY FAIL #%llu  addr=0x%08x  expected=0x%02x  actual=0x%02x  pc=0x%08x  lr=0x%08x",
                        (unsigned long long)fails, writtenAddr,
                        expectedByte, actualByte, pc, getGPR(14));
                }
            }
        }
    }

    // PSION_S5_STM_VERIFY: hook PCs immediately after each STM in the
    // fast memcpy word-loop. The STM instructions are at 0x5004D90C,
    // 0x5004D914, ..., 0x5004D944 (8 STMs in the loop). Each STM stores
    // 8 registers (r3-r10, 32 bytes) and post-increments r0. So at the
    // PC right after the STM, r3-r10 still hold the stored values, and
    // r0 has advanced by 32. Read back 32 bytes from (r0-32) and compare.
    static int stmVerify = -1;
    if (stmVerify < 0) {
        const char *e = std::getenv("PSION_S5_STM_VERIFY");
        stmVerify = (e && e[0] == '1') ? 1 : 0;
    }
    // debugPC fires AFTER each instruction executes (verified empirically:
    // existing LCD-init hook at pc=0x18C38 fires when that PC has just run).
    // So to verify a STM, we hook AT its own PC — by then the STM has run,
    // r0 was post-incremented by 32, and r3-r10 still hold the stored values
    // (the next LDM hasn't run yet because debugPC fires before the NEXT
    // instruction would execute).
    if (stmVerify && (pc == 0x4D90C || pc == 0x4D914
                   || pc == 0x4D91C || pc == 0x4D924
                   || pc == 0x4D92C || pc == 0x4D934
                   || pc == 0x4D93C || pc == 0x4D944)) {
        uint32_t r0 = getGPR(0);
        uint32_t base = r0 - 32;
        uint32_t expectedRegs[8] = {
            getGPR(3), getGPR(4), getGPR(5), getGPR(6),
            getGPR(7), getGPR(8), getGPR(9), getGPR(10)
        };
        for (int i = 0; i < 8; i++) {
            auto rb = readVirtualDebug(base + i * 4, V32);
            uint32_t actual = rb.value_or(0xDEADBEEF);
            if (rb.has_value() && actual != expectedRegs[i]) {
                static uint64_t fails = 0;
                ++fails;
                if (fails <= 64 || (fails & 0xFF) == 0) {
                    log("S5: STM_VERIFY FAIL #%llu  addr=0x%08x  expected=0x%08x  actual=0x%08x  pc=0x%08x  lr=0x%08x  base=0x%08x",
                        (unsigned long long)fails, base + i * 4,
                        expectedRegs[i], actual, pc, getGPR(14), base);
                }
            }
        }
    }
    // PSION_S5_STM_DUMP=1: at each STM in the memcpy loop, dump r0/r3-r10
    // BEFORE the STM executes. Combined with STM_VERIFY (which fires AFTER),
    // we can see what registers were stored vs what ended up in memory.
    static int stmDump = -1;
    if (stmDump < 0) {
        const char *e = std::getenv("PSION_S5_STM_DUMP");
        stmDump = (e && e[0] == '1') ? 1 : 0;
    }
    if (stmDump && (pc == 0x4D90C || pc == 0x4D914 || pc == 0x4D91C
                 || pc == 0x4D924 || pc == 0x4D92C || pc == 0x4D934
                 || pc == 0x4D93C || pc == 0x4D944)) {
        static uint64_t dumpFails = 0;
        ++dumpFails;
        if (dumpFails <= 32) {
            log("S5: STM_DUMP @PC=0x%08x  r0=0x%08x  r3-r10={0x%08x,0x%08x,0x%08x,0x%08x,0x%08x,0x%08x,0x%08x,0x%08x}",
                pc, getGPR(0),
                getGPR(3), getGPR(4), getGPR(5), getGPR(6),
                getGPR(7), getGPR(8), getGPR(9), getGPR(10));
        }
    }


    if (pc == 0x18c38 && !seenLcdInit) {
        seenLcdInit = true;
        log("Series 5: FUN_50018c38 (LCD init) reached");
    }
    // PSION_S5_TOUCH_INJECT hook: when user is touching, divert execution
    // INTO FUN_5001D6E0 from a known-safe PC. The touch poll reads our
    // SYNCIO emulation (returns pen-down since touchX/Y are set), takes
    // the pen-down branch, and self-schedules — establishing a polling
    // loop.
    //
    // SAFE INJECTION PC: PC=0x5007D2E0 is the instruction RIGHT AFTER
    // the kernel returns from the PCCARD-ARM subroutine `BL 0x5007D300`
    // at 0x5007D2DC. At this PC, the kernel is in a stable state — no
    // half-built stack frame — and the very next instruction is another
    // BL we can safely "replace" by diverting execution. When
    // FUN_5001D6E0 returns it will pop our stashed LR (= 0x5007D2E4)
    // which is the instruction the kernel would have executed next.
    //
    // We can NOT inject inside FUN_X at 0x5007D300 because that function
    // has a STMDB prologue we'd skip, corrupting its stack frame.
    if (pc == 0x7D2E0 && touchInjectPending) {
        touchInjectPending = false;
        uint32_t curPc = getGPR(15) - 0xC;        // = 0x5007D2E0
        uint32_t nextPc = curPc + 4;              // = 0x5007D2E4 (next insn)
        log("Series 5: TOUCH-INJECT diverting PC=0x%08x -> FUN_5001D6E0 "
            "(prior LR=0x%08x, will return to 0x%08x)",
            curPc, getGPR(14), nextPc);
        setGPR(14, nextPc);
        setGPR(15, 0x5001D6E0u + 8);
        return;
    }
    if (pc == 0x1ae04 && !seenIdleTail) {
        seenIdleTail = true;
        log("Series 5: FUN_5001ae04 (idle loop tail) reached");

        // PSION_S5_MMU_CHECK: at idle, dump the phys translation for
        // a set of suspicious virtual addresses. Used to find unintended
        // MMU aliases (two virts mapping to same phys when they shouldn't).
        if (const char *e = std::getenv("PSION_S5_MMU_CHECK"); e && e[0] == '1') {
            uint32_t virts[] = {
                0x00404000, 0x00404100, 0x82204000, 0x82204100,
                0x00402550, 0x82204700, 0x82204800, 0x822056C8,
                0x80003BB8, 0x80003C18, 0xC0FE0000, 0xD0000000,
                0x80100000, 0x80100D94, 0x80100EC8, 0x80100E28,
            };
            for (uint32_t v : virts) {
                auto p = virtToPhys(v);
                if (p.has_value())
                    log("S5: MMU_CHECK virt=0x%08x → phys=0x%08x", v, p.value());
                else
                    log("S5: MMU_CHECK virt=0x%08x → (unmapped)", v);
            }
        }

        // PSION_S5_ROM_DUMP=1: at idle, dump first N bytes of each app's
        // ROFS files via readVirtualDebug. Compare against ROM file content
        // to find mismatches (which would indicate MMU or ROM-read bugs).
        if (const char *e = std::getenv("PSION_S5_ROM_DUMP"); e && e[0] == '1') {
            struct { const char *name; uint32_t virt; uint32_t bytes; } files[] = {
                {"Word.app",   0x504A6520, 64},
                {"Word.aif",   0x504A6E60, 128},
                {"Word.mbm",   0x504A7990, 128},
                {"Sheet.app",  0x5049FE50, 64},
                {"Sheet.aif",  0x504A0750, 64},
                {"Sheet.mbm",  0x504A3290, 64},
                {"Agenda.app", 0x5047CC40, 64},
                {"Agenda.aif", 0x50481F70, 64},
                {"Agenda.rsc", 0x50481120, 64},
                {"Data.app",   0x50479C60, 64},
                {"Data.mbm",   0x5047C480, 64},
            };
            for (auto &f : files) {
                char buf[3*128 + 1] = {0};
                int p = 0;
                for (uint32_t i = 0; i < f.bytes && p < (int)sizeof(buf)-4; i++) {
                    auto b = readVirtualDebug(f.virt + i, V8);
                    p += snprintf(buf+p, sizeof(buf)-p, "%02x ",
                                  (unsigned)(b.value_or(0xAA) & 0xFF));
                }
                log("S5: ROM_DUMP %s @0x%08x: %s", f.name, f.virt, buf);
            }
        }
        // Once the kernel has reached idle, the exception vectors at
        // virt 0x00..0x20 are fully programmed. Dump them so we can see
        // where FIQ/IRQ/etc dispatch to. Particularly: FIQ vector at
        // virt 0x1C is what runs when EXTFIQ fires.
        const char *labels[] = {
            "Reset", "Undef", "SWI", "PrefetchAbort",
            "DataAbort", "(reserved)", "IRQ", "FIQ"
        };
        for (int i = 0; i < 8; i++) {
            uint32_t va = i * 4;
            auto insn = readVirtualDebug(va, V32);
            uint32_t target = 0;
            if (insn.has_value() && (insn.value() & 0xFFFFF000) == 0xe59ff000) {
                uint32_t imm = insn.value() & 0xFFF;
                auto ptr = readVirtualDebug(va + 8 + imm, V32);
                if (ptr.has_value()) target = ptr.value();
            }
            log("  vector[0x%02x %-14s] = 0x%08x  target=0x%08x",
                va, labels[i],
                insn.value_or(0xDEADBEEF), target);
        }
    }
    // Touch-debug traces below were originally added while diagnosing the
    // pen-up debouncer / X-sample DFC paths; they're left in for a future
    // investigation but produce a lot of noise during normal use, so they
    // only fire when PSION_S5_TOUCH_TRACE=1. Boot-progress / FIQ-delivery
    // anchors that are still load-bearing for general debugging stay on
    // unconditionally above this gate (LCD init, idle-loop tail, etc.).
    bool touchTrace = std::getenv("PSION_S5_TOUCH_TRACE") != nullptr;
    if (!touchTrace) return;

    // FIQ handler entry FUN_500198A0 — track whether EXTFIQ is being
    // delivered. If we assert EXTFIQ from updateTouchInput but never see
    // this hit, FIQs are masked (CPSR_F=1) or our IRQ delivery path
    // isn't routing EXTFIQ to the FIQ vector.
    if (pc == 0x198A0) {
        static uint64_t count = 0;
        if (count < 10 || (count & 0x3FF) == 0)
            log("Series 5: FUN_500198A0 (FIQ handler) hit #%llu",
                (unsigned long long)++count);
        else
            ++count;
    }
    // PortE writer FUN_500184B0 — log its caller LR so we can identify
    // what function manipulates Port E during the touch tap. The Port E
    // writes during the tap come from this function; LR tells us who
    // called it.
    if (pc == 0x184B0) {
        static uint64_t count = 0;
        if (count < 30 || (count & 0x3FF) == 0)
            log("Series 5: FUN_500184B0 (PEDR writer) hit #%llu  caller LR=0x%08x r0=0x%08x",
                (unsigned long long)++count, getGPR(14), getGPR(0));
        else
            ++count;
    }
    // Series 5 pen-driver poll loops (per ROM analysis):
    //   FUN_5007C21C reads INTSR1 & 0x80 (= EINT3 = ADS7843 PENIRQ).
    //   If set, advances state machine to "pen detected" path and
    //   schedules next chain step. If clear, takes "pen up" branch.
    //   FUN_5007C2FC is the debounce / counter loop (also reads
    //   INTSR1 & 0x80, increments pen-up counter, schedules next step).
    //   These ARE the kernel's touch driver — when they fire and EINT3
    //   is asserted, the kernel knows pen is down.
    if (pc == 0x7C21C) {
        static uint64_t count = 0;
        ++count;
        uint32_t r0 = getGPR(0);
        auto v110 = readVirtualDebug(r0 + 0x110u, V32);
        auto v118 = readVirtualDebug(r0 + 0x118u, V32);
        if (count <= 20 || (count & 0x3F) == 0)
            log("Series 5: FUN_5007C21C hit #%llu  r0=0x%08x  [r0+0x110]=0x%08x [r0+0x118]=0x%08x",
                (unsigned long long)count, r0,
                v110.value_or(0), v118.value_or(0));
    }
    // FUN_5007C0DC is the STATE-RESET method: writes 0 to [r0+0x118].
    // Called via vtable (entry at 0x5007CE40 in DigitiserX/Y vtable).
    // Hook to see WHEN/WHY this is called — the reset is required for
    // subsequent taps to fire after the first tap leaves +0x118 stuck at 3.
    if (pc == 0x7C0DC) {
        static uint64_t count = 0;
        ++count;
        uint32_t r0 = getGPR(0);
        uint32_t lr = getGPR(14);
        log("Series 5: FUN_5007C0DC (state RESET +0x118=0) hit #%llu  r0=0x%08x LR=0x%08x",
            (unsigned long long)count, r0, lr);
    }
    if (pc == 0x7C2FC) {
        static uint64_t count = 0;
        if (count < 10 || (count & 0xFF) == 0)
            log("Series 5: FUN_5007C2FC (pen debounce) hit #%llu",
                (unsigned long long)++count);
        else
            ++count;
    }
    // PC=0x1D91C is the BL to FUN_50018184 inside FUN_5001D908. If we
    // hit this PC, the BL IS being executed (counter check passed).
    if (pc == 0x1D91C) {
        static uint64_t count = 0;
        if (count < 20 || (count & 0xFF) == 0)
            log("Series 5: FUN_5001D908+0x14 (about to BL FUN_50018184) hit #%llu",
                (unsigned long long)++count);
        else
            ++count;
    }
    // FUN_5001D670 — the post-averaging "deliver result" callback.
    // Disasm: r0 = axis object; writes 0 to *(r0+0x10), then calls
    // *(r0+0x18)[0x10] (= vtable[4]). Sample average is stored at *(r0+8)
    // by FUN_5001D908 just before this call. Hook to identify the
    // vtable[4] target and the value being delivered, per axis. This is
    // the boundary between "sampling works" and "delivery broken".
    if (pc == 0x1D670) {
        static uint64_t count = 0;
        ++count;
        if (count <= 10 || (count & 0x3F) == 0) {
            uint32_t r0 = getGPR(0);
            auto vtablePtr = readVirtualDebug(r0 + 0x18u, V32);
            auto sampleAvg = readVirtualDebug(r0 + 0x08u, V32);
            auto sampleRange = readVirtualDebug(r0 + 0x0Cu, V32);
            uint32_t vt = vtablePtr.value_or(0);
            auto vt4 = readVirtualDebug(vt + 0x10u, V32);
            log("Series 5: FUN_5001D670 hit #%llu  r0=0x%08x vtable=0x%08x vtable[4]=0x%08x "
                "sample_avg=0x%08x range=0x%08x",
                (unsigned long long)count, r0, vt, vt4.value_or(0),
                sampleAvg.value_or(0), sampleRange.value_or(0));
        }
    }
    // FUN_50018184 (read SYNCIO) entry at PC=0x18184. STMDB SP! is the
    // first instruction. Hook before MOV R0, #0x500 so R0 reflects whatever
    // the caller passed in (probably uninteresting since this fn doesn't
    // take args, but worth confirming).
    if (pc == 0x18184) {
        static uint64_t count = 0;
        if (count < 30 || (count & 0xFF) == 0)
            log("Series 5: FUN_50018184 (read SYNCIO) hit #%llu LR=0x%08x R0_in=0x%08x",
                (unsigned long long)++count, getGPR(14), getGPR(0));
        else
            ++count;
    }
    // FUN_5001D908 is the X-sample-averaging DFC callback. It reads
    // SYNCIO via FUN_50018184 when *DAT_5001da50 >= 0. Hook entry to
    // log when called and what *DAT_5001da50 is. This DFC is scheduled
    // during the tap (we see r0=0x80100d94 in schedule logs) but no
    // SYNCIO reads happen — suggests the early-out condition is hit.
    if (pc == 0x1D908) {
        static uint64_t count = 0;
        ++count;
        if (count < 10 || (count & 0xFFF) == 0)
            log("Series 5: FUN_5001D908 (X-sample DFC) hit #%llu",
                (unsigned long long)count);
    }
    // IRQ vector entry (virt 0x50019678 = ROM 0x19678) — counts every
    // time the IRQ vector at 0x18 dispatches. Each entry calls the IRQ
    // dispatcher FUN_5001A770. PSION_S5_IRQ_DUMP_CYC and INTMR1 logs
    // show when masks change; this hook tells us WHEN delivery actually
    // happens. With EINT3 asserted, we want to see if extra entries
    // happen vs not-asserted.
    if (pc == 0x19678) {
        static uint64_t count = 0;
        if (count < 10 || (count & 0xFFF) == 0)
            log("Series 5: IRQ-vector @ FUN_50019678 hit #%llu",
                (unsigned long long)++count);
        else
            ++count;
    }
    // Periodically log touch-poll entry FUN_5001D6E0 — we expect this DFC
    // to fire roughly once per second on real Series 5. If it stops firing
    // after early boot, the scheduler isn't reaching it (e.g., DFC queue
    // empty / wrong thread state / IPC bug).
    if (pc == 0x1D6E0) {
        static uint64_t count = 0;
        if (count < 10 || (count & 0xFF) == 0)
            log("Series 5: FUN_5001D6E0 (touch poll) hit #%llu",
                (unsigned long long)++count);
        else
            ++count;
    }
    // FUN_5001a904 is the TC2OI handler / DFC dispatcher per IRQ table dump.
    // It dequeues 0-tick entries from DAT_50017450 and invokes their
    // callbacks. If this is NEVER hit, TC2OI isn't being delivered. If it
    // IS hit but FUN_5001D6E0 doesn't run, the touch DFC isn't in the queue.
    if (pc == 0x1A904) {
        static uint64_t count = 0;
        if (count < 10 || (count & 0x3FF) == 0)
            log("Series 5: FUN_5001A904 (TC2OI DFC dispatcher) hit #%llu",
                (unsigned long long)++count);
        else
            ++count;
    }
    // FUN_5001728c is the DFC scheduler entry. Log hits with the DFC
    // pointer and — when r0 is the touch-poll DFC at 0x80100ce0 (callback
    // = FUN_5001D6E0) or its pen-up chain successor at 0x80100cb8
    // (callback = FUN_5001D728) — also the entry's [0]..[4] fields so we
    // can see how its delta / total / next pointer look just before the
    // queue insertion.
    //
    // ROM DFC init table at 0x5001E928+ pairs callback addresses with DFC
    // struct addresses; from that table:
    //   0x80100ca4 -> FUN_5001D698  (write SYNCIO 0x6D0C aux)
    //   0x80100cb8 -> FUN_5001D728  (pen-up chain step 2)
    //   0x80100ccc -> FUN_5001D6BC  (chain helper)
    //   0x80100ce0 -> FUN_5001D6E0  (TOUCH POLL ENTRY)
    //   0x80100cf4 -> FUN_5001D74C  (pen-up chain step 3)
    //   0x80100d08 -> FUN_5001D770  (chain step 4 — conditional)
    //   0x80100d1c..0x80100d44 — later chain steps
    if (pc == 0x1728C) {
        static uint64_t count = 0;
        uint32_t r0 = getGPR(0);
        bool isTouchEntry = (r0 == 0x80100ce0u);   // FUN_5001D6E0 DFC
        bool isChain      = (r0 >= 0x80100ca4u && r0 <= 0x80100d50u);
        if (count < 30 || (count & 0x3FF) == 0 || isTouchEntry || isChain) {
            const char *tag = isTouchEntry ? " [TOUCH-POLL-ENTRY]"
                            : isChain      ? " [touch-chain]" : "";
            log("Series 5: FUN_5001728C (DFC schedule) hit #%llu r0=0x%08x%s",
                (unsigned long long)++count, r0, tag);
            if (isTouchEntry) {
                auto w0 = readVirtualDebug(r0 + 0x0, V32);
                auto w1 = readVirtualDebug(r0 + 0x4, V32);
                auto w2 = readVirtualDebug(r0 + 0x8, V32);
                auto w3 = readVirtualDebug(r0 + 0xC, V32);
                auto w4 = readVirtualDebug(r0 + 0x10, V32);
                log("  TOUCH-POLL DFC fields: [0]=0x%08x [1]=0x%08x [2]=0x%08x [3]=0x%08x [4]=0x%08x",
                    w0.value_or(0), w1.value_or(0), w2.value_or(0),
                    w3.value_or(0), w4.value_or(0));
            }
        } else {
            ++count;
        }
    }
    // FUN_5001AA14 programs Timer 2 with a relative delta (param_1 in r0).
    // Log every call so we can see what deltas the kernel is requesting
    // and whether the touch DFC's deadline ever gets programmed.
    if (pc == 0x1AA14) {
        static uint64_t count = 0;
        if (count < 30 || (count & 0x3FF) == 0)
            log("Series 5: FUN_5001AA14 (timer 2 program) hit #%llu r0=%d (0x%08x)",
                (unsigned long long)++count, (int32_t)getGPR(0), getGPR(0));
        else
            ++count;
    }
}

}
