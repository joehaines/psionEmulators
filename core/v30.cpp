// license:BSD-3-Clause
// copyright-holders: Bryan McPhail, ASG (MAME NEC port)
//                    + adaptation by the Psion emulator project, 2026.
//
// NEC V30 CPU core — main loop + dispatch.
//
// The instruction set is split across four sibling files so the per-
// category work can fan out across parallel workstreams without merge
// conflicts on a single large switch:
//
//   v30_ops_mov.cpp   — MOV / LEA / LDS / LES / XLAT
//   v30_ops_arith.cpp — ADD / SUB / CMP / OR / AND / XOR / Grp1 / Grp3 /
//                        INC / DEC
//   v30_ops_ctrl.cpp  — JMP / Jcc / CALL / RET / LOOP / INT / IRET
//   v30_ops_misc.cpp  — stack, string ops, shifts, flags, IN / OUT,
//                        segment prefixes
//
// Each exports one `dispatch<Category>(V30&, op)` entry that returns a
// cycle count, or -1 if the opcode isn't handled in that category.
// step() tries each category in turn; an unhandled opcode logs to stderr
// and halts the CPU with enough context for a port-by-port iteration.
//
// Shared helpers (ModR/M decoder, flag maths, stack, condition codes)
// live in v30_internals.h as inline functions.

#include "v30.h"
#include "v30_internals.h"

#include <cstdio>
#include <cstdlib>

using namespace V30Detail;

void V30::reset() {
    for (int i = 0; i < 8; ++i) regs.w[i] = 0;
    sregs[0] = 0x0000; // ES (DS1)
    sregs[1] = 0xFFFF; // CS (PS) -- reset vector at FFFF:0000
    sregs[2] = 0x0000; // SS
    sregs[3] = 0x0000; // DS (DS0)
    ip = 0x0000;
    signVal = auxVal = overVal = carryVal = 0;
    zeroVal = 1;         // ZF starts clear
    parityVal = 1;
    tf = iflag = df = 0;
    md = 1;
    irqLine = nmiLine = false;
    halted = false;
    intInhibit = 0;
    segOverride = -1;
}

// Handle a pending NMI or maskable IRQ at an instruction boundary.
// Returns true if an interrupt was serviced (so step() can count the
// cycles appropriately).
static bool serviceInterrupts(V30& c) {
    if (c.nmiLine) {
        c.nmiLine = false;
        push16(c, buildPSW(c));
        push16(c, c.sregs[1]);
        push16(c, c.ip);
        c.iflag = 0;
        c.tf    = 0;
        c.ip       = c.busRef().readMemWord(0x00008);
        c.sregs[1] = c.busRef().readMemWord(0x0000A);
        c.halted = false;
        return true;
    }
    if (c.iflag && c.irqLine) {
        uint8_t vec = c.busRef().ackInterrupt();
        uint32_t vecAddr = uint32_t(vec) * 4;
        uint16_t newIp = c.busRef().readMemWord(vecAddr);
        uint16_t newCs = c.busRef().readMemWord(vecAddr + 2);
        if (std::getenv("PSION_WAKE_TRACE")) {
            static uint64_t svcCount = 0;
            if (++svcCount <= 20 || svcCount % 200 == 0)
                std::fprintf(stderr, "[svc] #%llu vec=%02x isr=%04X:%04X\n",
                    (unsigned long long)svcCount, vec, newCs, newIp);
        }
        push16(c, buildPSW(c));
        push16(c, c.sregs[1]);
        push16(c, c.ip);
        c.iflag = 0;
        c.tf    = 0;
        c.ip       = newIp;
        c.sregs[1] = newCs;
        c.halted = false;
        return true;
    }
    return false;
}

int64_t V30::step() {
    const int i8086Scale = i8086CycleScale(variant);
    // Check pending interrupts at instruction boundary. Clears halted.
    // The one-instruction shadow after MOV SS / POP SS / STI defers the
    // check so the kernel's stack-switch pairs complete atomically.
    // A latched segment-override prefix means we are MID-instruction
    // (the prefix is dispatched as its own step): delivering here would
    // push a resume IP between the prefix and its instruction, so the
    // override is lost on IRET and e.g. `call *%cs:tbl(bx)` reloads its
    // pointer from DS — the 3c kernel's syscall dispatcher does exactly
    // that, and a codec interrupt in that window wild-called into the
    // dispatcher re-entrantly and panicked the kernel (0x3D) about 1 s
    // into every sound recording.
    if (intInhibit) {
        intInhibit--;
    } else if (segOverride < 0 && serviceInterrupts(*this)) {
        if (std::getenv("PSION_IRQ_TRACE")) {
            static uint64_t irqCount = 0;
            static uint64_t nextReport = 10'000'000;
            irqCount++;
            extern int64_t g_v30_passed;
            // Report via a module-static counter; caller compares against
            // the harness cycle clock.
            if (irqCount % 500 == 0) {
                std::fprintf(stderr, "[irq] count=%llu (sampled every 500)\n",
                             (unsigned long long)irqCount);
            }
            (void)nextReport;
        }
        return 25 * i8086Scale;
    }

    // Debug: trace a specific PC (PSION_PC_TRACE=<hex>) on each visit.
    static uint32_t s_pcTraceTarget = [](){
        const char *e = std::getenv("PSION_PC_TRACE");
        return e ? uint32_t(std::strtoul(e, nullptr, 16)) : 0xFFFFFFFFu;
    }();
    static uint32_t s_pcTraceTarget2 = [](){
        const char *e = std::getenv("PSION_PC_TRACE2");
        return e ? uint32_t(std::strtoul(e, nullptr, 16)) : 0xFFFFFFFFu;
    }();
    // PSION_PC_GATE_TRACE=<hex>: at every visit to that PC, log the
    // full register set + SP/SS plus the word at [SS:SP] (= the
    // return address pushed by the caller's CALL). Used to bisect
    // which caller is responsible for a divergent DX/AX/... input.
    // Caps at 200 hits per session to keep the log small.
    static uint32_t s_pcGateTraceTarget = [](){
        const char *e = std::getenv("PSION_PC_GATE_TRACE");
        return e ? uint32_t(std::strtoul(e, nullptr, 16)) : 0xFFFFFFFFu;
    }();
    {
        uint32_t pc = lin(sregs[1], ip);
        if (s_pcTraceTarget != 0xFFFFFFFFu && pc == s_pcTraceTarget) {
            static uint64_t n = 0;
            if (n++ < 200)
            std::fprintf(stderr, "[pc-trace] hit %05X cs=%04X ip=%04X ax=%04X bx=%04X cx=%04X dx=%04X psw=%04X zf=%d cf=%d\n",
                pc, sregs[1], ip, regs.w[0], regs.w[3], regs.w[1], regs.w[2],
                buildPSW(*this), zf(*this), cf(*this));
        }
        if (s_pcTraceTarget2 != 0xFFFFFFFFu && pc == s_pcTraceTarget2) {
            static uint64_t n = 0;
            if (n++ < 200)
            std::fprintf(stderr, "[pc-trace2] hit %05X cs=%04X ip=%04X ax=%04X bx=%04X cx=%04X dx=%04X psw=%04X zf=%d cf=%d\n",
                pc, sregs[1], ip, regs.w[0], regs.w[3], regs.w[1], regs.w[2],
                buildPSW(*this), zf(*this), cf(*this));
        }
        if (s_pcGateTraceTarget != 0xFFFFFFFFu && pc == s_pcGateTraceTarget) {
            static uint64_t n = 0;
            if (n++ < 200) {
                uint32_t ssSp = uint32_t(sregs[2]) * 16u + regs.w[4];
                uint16_t retIp = busRef().readMemWord(ssSp);
                std::fprintf(stderr,
                    "[gate-trace] hit %05X "
                    "ax=%04X bx=%04X cx=%04X dx=%04X si=%04X di=%04X bp=%04X sp=%04X "
                    "cs=%04X ds=%04X es=%04X ss=%04X "
                    "[ss:sp]=%04X (retIP, caller probably at cs:%04X)\n",
                    pc,
                    regs.w[0], regs.w[3], regs.w[1], regs.w[2],
                    regs.w[6], regs.w[7], regs.w[5], regs.w[4],
                    sregs[1], sregs[3], sregs[0], sregs[2],
                    retIp, uint16_t(retIp - 3));
            }
        }
    }

    // When halted, return a batch of cycles so the driver's tick loop
    // can advance peripherals by the same amount in one go, instead of
    // stepping through the HLT one cycle at a time. The ASIC scheduler
    // fires its pending timers inside tick() so a large batch here is
    // safe (tick uses a `while (remaining <= 0)` loop to catch up).
    // 64 cycles ~= 8 μs at 7.68 MHz — fine granularity for the kernel's
    // tick interrupt handler to still wake promptly.
    if (halted) return 64 * i8086Scale;

    uint32_t pc = lin(sregs[1], ip);
    uint8_t  op = busRef().readMemByte(pc);
    ip = uint16_t(ip + 1);

    // Wild-jump forensics (PSION_OPCODE_DEBUG): report the first few
    // control transfers into segment 0 — kernel code never legitimately
    // executes the IVT/data page, so any entry is a corrupt far pointer.
    static const bool s_opDbg = std::getenv("PSION_OPCODE_DEBUG") != nullptr;
    if (s_opDbg) {
        static uint16_t prevCs = 0xFFFF, prevIp = 0;
        if (sregs[1] < 0x0040 && prevCs >= 0x0040 && prevCs != 0xFFFF) {
            static int n = 0;
            if (n++ < 8) {
                std::fprintf(stderr,
                    "[wildjump] %04x:%04x -> %04x:%04x (sp=%04x ss=%04x)\n",
                    prevCs, prevIp, sregs[1], uint16_t(ip - 1),
                    regs.w[4], sregs[2]);
                // Just-popped words (frame content is still in memory).
                std::fprintf(stderr, "[wildjump]   below-sp:");
                for (int i = 12; i >= 1; i--) {
                    uint32_t a = lin(sregs[2], uint16_t(regs.w[4] - i * 2));
                    std::fprintf(stderr, " %04x", busRef_.readMemWord(a));
                }
                std::fprintf(stderr, "\n[wildjump]   target bytes:");
                uint32_t t = lin(sregs[1], uint16_t(ip - 1));
                for (int i = -8; i < 24; i++)
                    std::fprintf(stderr, " %02x", busRef_.readMemByte(t + i));
                std::fprintf(stderr, "\n");
            }
        }
        prevCs = sregs[1];
        prevIp = uint16_t(ip - 1);
    }

    // Try each category in turn. Return value:
    //   >= 0   opcode handled, cycle count
    //   -2     segment-override prefix handled (dispatchMisc only); do NOT
    //           clear segOverride — it's consumed by the NEXT instruction.
    //   -1     not our category, fall through
    //
    // The dispatch handlers all return V30-tuned cycle counts. For an
    // I8086 instance we scale those by `i8086CycleScale(variant)` so the
    // host-cycle budget per instruction matches MAME's i86_device. The
    // V30 was a faster drop-in for the i8086 (~2x throughput at the same
    // clock); without this scale the MC400 boot ROM's peripheral races
    // (watchdog clear deadline, boot beep duration vs timer IRQ window)
    // close at the wrong sim time.
    const int scale = i8086CycleScale(variant);
    int64_t cycles;
    if ((cycles = dispatchMov  (*this, op)) >= 0) { clearSegOverride(*this); return cycles * scale; }
    if ((cycles = dispatchArith(*this, op)) >= 0) { clearSegOverride(*this); return cycles * scale; }
    if ((cycles = dispatchCtrl (*this, op)) >= 0) { clearSegOverride(*this); return cycles * scale; }
    cycles = dispatchMisc(*this, op);
    if (cycles == -2) return 2 * scale;       // segment-override prefix
    if (cycles >= 0) { clearSegOverride(*this); return cycles * scale; }

    // No category claimed it: log + halt. The diagnostic is the iteration
    // signal — the next session (or the same one) implements whatever
    // opcode is reported and reruns.
    uint16_t failedIp = uint16_t(ip - 1);
    std::fprintf(stderr,
        "V30: unimplemented opcode 0x%02X at %04X:%04X (linear %05X), halting\n",
        op, sregs[1], failedIp, lin(sregs[1], failedIp));
    if (std::getenv("PSION_OPCODE_DEBUG")) {
        // Wild-jump forensics: the stack's return addresses identify the
        // code that jumped/called into garbage.
        std::fprintf(stderr,
            "  ax=%04x bx=%04x cx=%04x dx=%04x si=%04x di=%04x bp=%04x sp=%04x\n"
            "  ds=%04x es=%04x ss=%04x stack:",
            regs.w[0], regs.w[3], regs.w[1], regs.w[2],
            regs.w[6], regs.w[7], regs.w[5], regs.w[4],
            sregs[3], sregs[0], sregs[2]);
        for (int i = 0; i < 24; i++) {
            uint32_t a = lin(sregs[2], uint16_t(regs.w[4] + i * 2));
            std::fprintf(stderr, " %04x", busRef_.readMemWord(a));
        }
        std::fprintf(stderr, "\n");
    }
    halted = true;
    return 1;
}

int64_t V30::run(int64_t budget) {
    int64_t cycles = 0;
    while (cycles < budget && !halted) {
        cycles += step();
    }
    return cycles;
}
