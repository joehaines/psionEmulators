#pragma once

// NEC V30 CPU core (and V30H superset) — scaffold only.
//
// This header establishes the interface for a stand-alone V30 implementation
// derived from MAME's cpu/nec/*.{cpp,h,hxx,ipp} (BSD-3-Clause, copyright
// Bryan McPhail). The implementation is intentionally a stub: a real port is
// several focused engineer-days of work. See reference/mame-psion/PORTING_PLAN.md
// for the full porting plan and sequencing.
//
// Host usage (once ported):
//   class Series3Emulator : public EmuBaseV30 {
//     V30 cpu{*this};
//     uint8_t readByte(uint32_t linear) override { ... }
//     ...
//   };
//
// The V30 needs only a linear address + I/O port interface. Segment/offset
// combination is handled internally. V30H differs from V30 only in adding a
// handful of opcodes and allowing a faster clock — see `chip_type` below.

#include <cstdint>
#include <functional>

class V30Bus {
public:
    virtual ~V30Bus() = default;

    // 20-bit linear (segment<<4 + offset) memory access. The CPU already
    // handles segment wrap at the 1 MiB boundary; implementations just need
    // to map `linear` through whatever address decoder the machine has.
    virtual uint8_t  readMemByte(uint32_t linear) = 0;
    virtual uint16_t readMemWord(uint32_t linear) = 0;
    virtual void     writeMemByte(uint32_t linear, uint8_t v) = 0;
    virtual void     writeMemWord(uint32_t linear, uint16_t v) = 0;

    // I/O port space (16-bit port numbers on V30; real Psion SIBOs only
    // decode the low 12 or so bits).
    virtual uint8_t  readIoByte(uint16_t port) = 0;
    virtual uint16_t readIoWord(uint16_t port) = 0;
    virtual void     writeIoByte(uint16_t port, uint8_t v) = 0;
    virtual void     writeIoWord(uint16_t port, uint16_t v) = 0;

    // Interrupt acknowledge. On a maskable IRQ the CPU reads the vector
    // number from the programmable interrupt controller; the PIC (the top
    // half of ASIC1/ASIC9) drives this. Return 0xFF if no vector is
    // available (treated as spurious).
    virtual uint8_t  ackInterrupt() { return 0xFF; }
};

enum class V30Variant {
    V30   = 0,   // used by Series 3, HC, MC (laptop kernel boots fine on V30)
    V30H  = 1,   // used by Series 3a/3c/3mx, Siena, Workabout, WorkaboutMX
    I8086 = 2,   // used by Psion MC200/MC400/MC Word per MAME mc400.cpp.
                 // Binary-compatible with V30 (same opcode encoding) but
                 // ~2x slower per cycle and lacks the V30 0x0F-prefix
                 // extended opcodes — 0x0F is `POP CS` on the i8086. See
                 // i8086CycleScale() and the 0x0F handler in v30_ops_misc.cpp.
};

// Returns 2 for I8086 variants and 1 otherwise. The V30 was a faster
// drop-in replacement for the i8086 — Intel positioned it as ~2x the
// instruction throughput at the same clock. Each opcode handler that
// returns a cycle count multiplies its V30-tuned count by this scale
// so an I8086 instance executes the same NUMBER of instructions per
// host cycle as MAME's i86_device, keeping the MC400's boot-time
// peripheral races (watchdog clear deadline, boot beep duration vs
// timer IRQ) in their original windows.
constexpr int i8086CycleScale(V30Variant v) {
    return v == V30Variant::I8086 ? 2 : 1;
}

class V30 {
public:
    // Basic register state. MAME's nec_common_device keeps these split out
    // across several unions; we mirror the canonical layout so a port can be
    // near-mechanical. Names follow Intel 8086 conventions, with the NEC
    // aliases noted (AW=AX, CW=CX, DW=DX, BW=BX, IX=SI, IY=DI, DS0=DS,
    // DS1=ES, PS=CS).
    union Regs {
        uint16_t w[8];   // [AW, CW, DW, BW, SP, BP, IX, IY]
        uint8_t  b[16];  // [AL, AH, CL, CH, DL, DH, BL, BH, ...]
    } regs = {};

    uint16_t sregs[4] = {}; // [DS1/ES, PS/CS, SS, DS0/DS]
    uint16_t ip       = 0;

    // Flags (kept split for fast modify, collapsed into the 16-bit PSW on
    // PUSHF/INT). See MAME necmacro.h for the canonical flag derivation.
    int32_t  signVal   = 0;
    uint32_t auxVal    = 0;
    uint32_t overVal   = 0;
    uint32_t zeroVal   = 0;
    uint32_t carryVal  = 0;
    uint32_t parityVal = 0;
    uint8_t  tf = 0, iflag = 0, df = 0, md = 1;

    // Pending interrupt / NMI state.
    bool irqLine  = false;
    bool nmiLine  = false;
    bool halted   = false;
    // One-instruction interrupt shadow (x86 semantics): loading SS via
    // MOV SS,r/m or POP SS inhibits ALL interrupts (IRQ and NMI) until
    // the following instruction completes, so `mov ss / mov sp` pairs
    // are atomic; STI inhibits maskable IRQs the same way. The SIBO
    // kernel's reschedule path switches stacks with exactly that pair —
    // without the shadow, a codec interrupt landing between the two
    // loads pushes its frame onto new-SS:old-SP and corrupts memory
    // (seen as a wild IRET to segment 0 during 3c sound recording).
    uint8_t intInhibit = 0;
    // MAME NEC_INPUT_LINE_POLL: when false the WAIT instruction (0x9B)
    // re-executes itself (spins). ASIC9 clears this via setPollLine(false)
    // at the start of each SIB frame transfer and reasserts it 24 bus
    // cycles later (12 ticks at SIB clock = bus_clock/2) via the busy
    // timer. Starts asserted so WAIT is a NOP before any SIB activity.
    bool pollLine = true;

    // Active segment override set by a 0x26/0x2E/0x36/0x3E prefix. -1 means
    // no override in effect; values 0-3 index sregs[] (ES/CS/SS/DS).
    int  segOverride = -1;

    // Exact cycle count for the instruction currently being dispatched,
    // overriding the scaled V30 figure step() would otherwise use. -1 (the
    // resting value) means "no override"; step() consumes and clears it.
    //
    // i8086CycleScale()'s flat 2x is a fair average, but a handful of
    // instructions are nowhere near it — the V30 does LOOP in 6 cycles
    // where the 8086 needs 17, so doubling the V30 figure overstates an
    // 8086 loop by half again. That matters on the MC: its boot ROM
    // checksums all 256 KiB of itself in a LODSW/ADD/LOOP loop with no
    // watchdog pet anywhere in it, and has to finish before ASIC1's
    // watchdog NMI (three unacknowledged 4 Hz ticks). The real machine
    // makes it with ~20% to spare; at 2x-the-V30 timing the emulated one
    // ran ~19% slow and was NMI'd mid-checksum, into an interrupt vector
    // the boot ROM has not written yet. Ops that know their real 8086
    // figure set it here through i8086Exact().
    int32_t absCycles = -1;

    V30(V30Bus& bus, V30Variant v = V30Variant::V30) : busRef_(bus), variant(v) {}

    V30Bus& busRef() { return busRef_; }

    void reset();

    // Executes one instruction. Returns cycles consumed (1 cycle ~ one internal
    // clock; Series 3 V30 runs at 3.84 MHz, 3a/3c at 7.68 / 13.82 / 27.68 MHz).
    // Not implemented in this scaffold — see PORTING_PLAN.md.
    int64_t step();

    // Executes until the cycle budget is exhausted. Default implementation
    // just calls step() in a loop.
    int64_t run(int64_t budget);

    void setIrqLine(bool state)  { irqLine  = state; }
    void setNmiLine(bool state)  { nmiLine  = state; }
    void setPollLine(bool state) { pollLine = state; }

    V30Variant getVariant() const { return variant; }

private:
    V30Bus&    busRef_;
    // Where the instruction currently executing was fetched from. Only
    // read by the PSION_OPCODE_DEBUG report in step(), where it is the
    // difference between "this opcode is unimplemented" and "we are
    // running off the end of the map": a CS:IP that wrapped past the top
    // of the 1 MiB space reports as the reset vector but fetched from
    // somewhere else entirely.
    uint32_t   dbgFetchPc_ = 0;
    uint16_t   dbgFetchCs_ = 0, dbgFetchIp_ = 0;
    V30Variant variant;
};
