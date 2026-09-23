// license:BSD-3-Clause
// copyright-holders:Nigel Barnes (MAME psion_asic9_device)
//                   + adaptation by the Psion emulator project, 2026.
//
// PsionAsic9 — standalone port of MAME's psion_asic9_device.
//
// This is a PHASE-0 skeleton: the chip's register file, interrupt
// aggregation, timer scheduling, LCD scanout, and I/O read/write
// dispatch are all wired up well enough that the Series 3c host
// driver can talk to the chip without the V30 halting on a silent
// read/write. The higher-fidelity behaviour — page-select address
// translation, sound-FIFO playback, RTC ticking, SIBO serial frames,
// keyboard COL drive — is stubbed in place with the canonical
// register semantics from MAME's source but the side effects (sound
// samples actually reaching a host callback, serial frames actually
// moving bytes) are TODOs marked in comments.
//
// Reference: reference/mame-psion/machine/psion_asic9.cpp (MAME,
// ~950 lines). This port preserves the canonical register layout
// and bit meanings so the follow-up sessions can fill in behaviour
// without having to re-derive the MMIO map.

#include "psion_asic9.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace {

// Interrupt status/mask bit numbers (mirrors MAME's A9InterruptX enum,
// matching the io_w 0x08 docstring):
//   b0 A9MSound
//   b1 A9MTimer (Tick)
//   b2 A9MSlave (Sds)
//   b3 A9MExpIntC (EINT0)
//   b4 A9MExpIntA (EINT1)
//   b5 A9MExpIntB (EINT2)
//   b6 A9MFrc1
//   b7 A9MFrc2
enum A9IntBit : uint16_t {
    A9IntSnd     = 0x01,
    A9IntTick    = 0x02,
    A9IntSds     = 0x04,
    A9IntEint0   = 0x08, // ExpIntC
    A9IntEint1   = 0x10, // ExpIntA
    A9IntEint2   = 0x20, // ExpIntB
    A9IntFrc1    = 0x40,
    A9IntFrc2    = 0x80,
    // ASIC9MX only: integrated 16550 UARTs (see setMxUartInt).
    A9IntMxUart0 = 0x0100,
    A9IntMxUart1 = 0x0200,
};

// A9WControl bits (per MAME docstring):
//   b3-b4 A9MRamDeviceSize
//   b10   A9MLcdEnable
//   b11   A9MSoundEnable
//   b12   A9MFrc1PreScale
//   b13   A9MFrc1Is512KHzOr1Hz
//   b14   A9MFrc2PreScale
//   b15   A9MFrc2Is512KHzOr1KHz
constexpr uint16_t A9CtrlFrc1Pre   = 0x1000;
constexpr uint16_t A9CtrlFrc1Hi    = 0x2000; // 512 kHz vs 1 Hz
constexpr uint16_t A9CtrlFrc2Pre   = 0x4000;
constexpr uint16_t A9CtrlFrc2Hi    = 0x8000;
constexpr uint16_t A9CtrlLcdEnable = 0x0400;

// A9WControlExtra bits.
constexpr uint16_t A9CtrlExtraProtOn  = 0x0100;

} // namespace

PsionAsic9::PsionAsic9() {
    reset();
}

void PsionAsic9::setBusClock(int64_t hz) {
    if (hz > 0) m_bus_clock = hz;
}

void PsionAsic9::setAccurateTick(bool enabled) { m_accurate_tick = enabled; }
void PsionAsic9::enableSibHeartbeat(bool enabled) { m_sib_heartbeat = enabled; }

void PsionAsic9::setMemReader(std::function<uint8_t(uint32_t)> r) {
    m_mem_reader = std::move(r);
}

void PsionAsic9::reset() {
    m_post = 0;
    m_a9_control = 0;
    // m_a9_ram_type intentionally NOT cleared here — see setRamTypeDefault
    // doc in the header. The host driver seeds it once at construction
    // time so it survives device_reset (matching MAME, where machine_reset
    // doesn't re-call configure_ram).
    // A9Status reset bits. The value is set by m_boot_status, which the
    // host driver configures via setBootStatus() before loadROM():
    //   0x0020  A9MMainsPresent — main power-supply rail is OK
    //   0x2000  A9MReset        — set every reset; cleared on first read
    //   0x4000  A9MPowerFail    — power was removed unexpectedly
    //   0x8000  A9MCold         — first power-on after battery insertion
    //
    // Default: 0x0020|0x2000|0x8000 (MainsPresent|Reset|Cold). PowerFail
    // is NOT set by default because the Series 3c kernel's recovery path
    // probes the Condor UART at I/O 0x0100-0x011F; our stub never drives
    // the ready bit the kernel waits for, causing an infinite poll loop.
    //
    // For the Series 3a / Workabout / WorkaboutMX the host driver uses
    // 0x2000|0x8000 (Reset|Cold, no MainsPresent). If MainsPresent is
    // set, the v3.40f EPOC16 shell's session-restore path auto-launches
    // the last-used app on a freshly formatted M: drive whose data
    // directories don't yet exist → "Media is corrupt" dialog.
    m_a9_status = m_boot_status;
    // Diagnostic env-vars: PSION_A9_STATUS_OR=NNNN ORs additional bits
    // into the reset A9Status; PSION_A9_STATUS_AND=NNNN masks bits out.
    // Applied in that order (OR then AND). Hex.
    if (const char *v = std::getenv("PSION_A9_STATUS_OR")) {
        m_a9_status |= uint16_t(std::strtoul(v, nullptr, 16) & 0xFFFF);
    }
    if (const char *v = std::getenv("PSION_A9_STATUS_AND")) {
        m_a9_status &= uint16_t(std::strtoul(v, nullptr, 16) & 0xFFFF);
    }
    m_a9_lcd_size = 0;
    m_a9_interrupt_status = 0;
    m_a9_interrupt_mask = 0;
    m_frc1_count = 0;
    m_frc1_reload = 0;
    m_frc2_count = 0;
    m_frc2_reload = 0;
    m_buz_toggle = 0;
    m_watchdog_count = 0;
    m_a9_protection_mode = false;
    m_a9_protection_upper = 0;
    m_a9_protection_lower = 0;
    m_a9_port_ab_ddr = 0;
    m_a9_port_c_ddr = 0;
    m_a9_port_d_ddr = 0;
    m_a9_port_cd_data = 0;
    m_a9_psel_6000 = 0;
    m_a9_psel_7000 = 0;
    m_a9_psel_8000 = 0;
    m_a9_psel_9000 = 0;
    m_a9_control_extra = 0;
    // Seed the 1 Hz RTC counter from the host clock. The ASIC9 RTC is a free-
    // running 32-bit second counter whose epoch is 1/1/1980 00:00:00 UTC on
    // SIBO/EPOC16 (see reference/3a_decompiled/defined_strings.txt:22 — the
    // Agenda string "Cannot position outside range of Agenda (1980-2049)"
    // pins down the lower bound; the matching CMP word ptr [SI+0x8],0x7BC
    // (1980) / JG sequence in 5_dissassembly.txt requires the year register
    // to be STRICTLY > 1980, so a counter of 0 — which decodes back to
    // exactly 1/1/1980 — fails the check the first time Agenda is launched).
    //
    // MAME's psion_asic9_device sets m_rtc = 0 at reset and relies on its
    // NVRAM-backed RAM to preserve the kernel's stored real-world time
    // across runs. We don't persist NVRAM between sessions, so without a
    // host-time seed every cold boot would land on 1/1/1980 and the
    // Agenda app would refuse to open. PSION_RTC_SEED can override:
    //   unset / "1"  → seed from std::time(nullptr) (default)
    //   "0"          → leave m_rtc at zero (matches MAME's reset behaviour)
    //   any other    → hex-parsed literal value
    {
        m_rtc = 0;
        const char *seed = std::getenv("PSION_RTC_SEED");
        if (!seed || (seed[0] == '1' && seed[1] == 0) || seed[0] == 0) {
            // 315532800 = Unix time for 1/1/1980 00:00:00 UTC = SIBO epoch.
            const std::time_t now = std::time(nullptr);
            if (now > 315532800LL) {
                m_rtc = uint32_t(now - 315532800LL);
            }
        } else if (!(seed[0] == '0' && seed[1] == 0)) {
            m_rtc = uint32_t(std::strtoul(seed, nullptr, 16));
        }
    }
    m_snd_fifo.fill(0);
    m_snd_fifo_head = m_snd_fifo_tail = m_snd_fifo_size = 0;
    m_snd_missed = 0;
    m_a9_serial_data = 0;
    m_a9_serial_control = 0;
    m_a9_channel_select = 0;
    m_int_line = m_nmi_line = false;
    m_sib_scan_remaining = 75'000;

    reloadTickTimer();
    // MAME's device_reset() starts both FRC timers at 512 kHz unconditionally,
    // independent of A9WControl bits 13 and 15. Our reloadFrc1/2Timer() reads
    // those control bits — which are 0 at reset — giving 1 Hz (FRC1) and 1024 Hz
    // (FRC2) instead of MAME's 512 kHz. The ioWrite handler correctly retunes on
    // bit-change, so only the reset-time rate is wrong. Fix: set periods directly.
    {
        const int64_t hz = 512000;
        m_frc1_period    = m_bus_clock / hz;
        if (m_frc1_period < 1) m_frc1_period = 1;
        m_frc1_remaining = m_frc1_period;
    }
    {
        const int64_t hz = 512000;
        m_frc2_period    = m_bus_clock / hz;
        if (m_frc2_period < 1) m_frc2_period = 1;
        m_frc2_remaining = m_frc2_period;
    }
    reloadWatchdogTimer();
    reloadRtcTimer();
    reloadSndTimer();
}

void PsionAsic9::reloadTickTimer() {
    // MAME uses attotime::from_hz(32.768) = 32768/1000 Hz. Series 3a
    // enables accurate mode; other drivers keep the legacy 32 Hz value
    // (which was in place when they were validated) to avoid regressions.
    if (m_accurate_tick) {
        m_tick_period = (m_bus_clock * 1000LL) / 32768LL;
    } else {
        m_tick_period = m_bus_clock / 32;
    }
    if (m_tick_period < 1) m_tick_period = 1;
    m_tick_remaining = m_tick_period;
}

void PsionAsic9::reloadFrc1Timer() {
    // MAME's device_reset starts Frc1 at 512 kHz, and then the ioWrite of
    // A9Control at offset 0x02 retunes it on bit-13 change:
    //   bit 13 set (A9MFrc1Is512KHz) -> 512 kHz
    //   bit 13 clear                 -> 1 Hz
    // Until the kernel writes Control the bit is 0, but the timer is
    // pre-armed at 512 kHz — so we honour bit 13 on reload, and the
    // A9Control ioWrite handler calls reloadFrc1Timer when the bit
    // toggles.
    int64_t hz = (m_a9_control & A9CtrlFrc1Hi) ? 512000 : 1;
    m_frc1_period = m_bus_clock / hz;
    if (m_frc1_period < 1) m_frc1_period = 1;
    m_frc1_remaining = m_frc1_period;
}

void PsionAsic9::reloadFrc2Timer() {
    // MAME honours bit 15 (A9MFrc2Is512KHzOr1KHz): set -> 512 kHz,
    // else 1024 Hz.
    int64_t hz = (m_a9_control & A9CtrlFrc2Hi) ? 512000 : 1024;
    m_frc2_period = m_bus_clock / hz;
    if (m_frc2_period < 1) m_frc2_period = 1;
    m_frc2_remaining = m_frc2_period;
}

void PsionAsic9::reloadWatchdogTimer() {
    // MAME sets the watchdog timer to a fixed 4 Hz in device_reset and
    // never retunes it — it is independent of the tick rate.
    int64_t hz = 4;
    m_watchdog_period = m_bus_clock / hz;
    if (m_watchdog_period < 1) m_watchdog_period = 1;
    m_watchdog_remaining = m_watchdog_period;
}

void PsionAsic9::reloadRtcTimer() {
    // 1 Hz RTC tick.
    m_rtc_period = m_bus_clock;
    m_rtc_remaining = m_rtc_period;
}

void PsionAsic9::reloadSndTimer() {
    // 8 kHz PCM.
    int64_t hz = 8000;
    m_snd_period = m_bus_clock / hz;
    if (m_snd_period < 1) m_snd_period = 1;
    m_snd_remaining = m_snd_period;
}

void PsionAsic9::tick(int64_t cycles) {
    if (cycles <= 0) return;

    if (m_tick_period > 0) {
        m_tick_remaining -= cycles;
        while (m_tick_remaining <= 0) { fireTick(); m_tick_remaining += m_tick_period; }
    }
    if (m_frc1_period > 0) {
        m_frc1_remaining -= cycles;
        while (m_frc1_remaining <= 0) { fireFrc1(); m_frc1_remaining += m_frc1_period; }
    }
    if (m_frc2_period > 0) {
        m_frc2_remaining -= cycles;
        while (m_frc2_remaining <= 0) { fireFrc2(); m_frc2_remaining += m_frc2_period; }
    }
    if (m_watchdog_period > 0) {
        m_watchdog_remaining -= cycles;
        while (m_watchdog_remaining <= 0) { fireWatchdog(); m_watchdog_remaining += m_watchdog_period; }
    }
    if (m_rtc_period > 0) {
        m_rtc_remaining -= cycles;
        while (m_rtc_remaining <= 0) { fireRtc(); m_rtc_remaining += m_rtc_period; }
    }
    // SIB keyboard-scan heartbeat: periodically set A9MNoBattery (bit 12)
    // so kernels that poll it for a "SIB scan ready" signal see it fire.
    // Disabled for Series 3a (MAME never sets this bit there; setting it
    // triggers the kernel's "no backup battery" path and corrupts boot).
    // Enabled for Series 3c / Siena / Workabout where the real hardware
    // drives the bit via actual SIB slave devices and our stub substitutes.
    if (m_sib_heartbeat) {
        m_sib_scan_remaining -= cycles;
        while (m_sib_scan_remaining <= 0) {
            m_a9_status |= 0x1000;
            m_sib_scan_remaining += 75'000;
        }
    }
    // MAME runs the sound timer unconditionally at 8 kHz; the per-fire
    // handler gates on A9MSoundEnable (A9WControl bit 11 = 0x0800), NOT
    // bit 7 (which is A9MZeroIsGrayMode). The previous gate here used
    // 0x0080 — A9MSoundEnable was effectively never seen as "on", so the
    // codec FIFO drained nothing and the kernel's "sound DMA done"
    // interrupt never fired even when audio playback was actually
    // configured. Match MAME: tick the timer unconditionally and let
    // fireSnd() consult A9WControl bit 11.
    if (m_snd_period > 0) {
        m_snd_remaining -= cycles;
        while (m_snd_remaining <= 0) { fireSnd(); m_snd_remaining += m_snd_period; }
    }
}

void PsionAsic9::fireTick() {
    m_a9_interrupt_status |= A9IntTick;
    updateInterrupts();
}

void PsionAsic9::fireFrc1() {
    // MAME semantics: pre-decrement, then branch on new value.
    //   new == 0x0000 -> underflow is imminent, fire interrupt / buzzer
    //   new == 0xffff -> just wrapped; reload from the reload register
    //                   if prescale (A9MFrc1PreScale, bit 12) is enabled
    uint16_t next = uint16_t(m_frc1_count - 1);
    m_frc1_count = next;
    if (next == 0x0000) {
        if (m_a9_control_extra & 0x0020) {
            m_buz_toggle ^= 1;
            if (m_buz_cb) m_buz_cb(m_buz_toggle != 0);
        }
        m_a9_interrupt_status |= A9IntFrc1;
        if (std::getenv("PSION_IRQ_TRACE")) {
            static uint64_t n = 0;
            if (++n % 20 == 0) std::fprintf(stderr,
                "[asic9] fireFrc1 #%llu status=%02x mask=%02x ctl=%04x\n",
                (unsigned long long)n, m_a9_interrupt_status, m_a9_interrupt_mask, m_a9_control);
        }
        updateInterrupts();
    } else if (next == 0xFFFF) {
        if (m_a9_control & A9CtrlFrc1Pre) m_frc1_count = m_frc1_reload;
    }
}

void PsionAsic9::fireFrc2() {
    // Mirror MAME's pre-decrement semantics (psion_asic9.cpp frc2 callback):
    //   switch (--m_frc2_count) { case 0x0000: interrupt; case 0xffff: reload; }
    // Our previous post-decrement (prev = m_frc2_count--) checked the OLD value,
    // which fired the interrupt when count wrapped FROM 0 TO 0xFFFF (one tick late)
    // and triggered a spurious interrupt on the very first fire when count=0 at reset.
    uint16_t next = uint16_t(m_frc2_count - 1);
    m_frc2_count = next;
    if (next == 0x0000) {
        m_a9_interrupt_status |= A9IntFrc2;
        updateInterrupts();
    } else if (next == 0xFFFF) {
        if (m_a9_control & A9CtrlFrc2Pre) m_frc2_count = m_frc2_reload;
    }
}

void PsionAsic9::fireWatchdog() {
    m_watchdog_count = (m_watchdog_count + 1) & 3;
    if (m_watchdog_count == 3) {
        if (std::getenv("PSION_SND_TRACE"))
            std::fprintf(stderr, "[snd] watchdog NMI (ctl=%04x)\n", m_a9_control);
        m_a9_status |= 0x0001; // A9MWatchDogNMI
        updateInterrupts();
    }
}

void PsionAsic9::fireRtc() {
    ++m_rtc;
}

void PsionAsic9::fireSnd() {
    // Mirror MAME's psion_asic9_device::snd (psion_asic9.cpp:307). When
    // A9MSoundEnable (A9WControl bit 11 = 0x0800) is clear, the timer is
    // a no-op — the codec is powered off and no FIFO movement happens.
    // When set, A9MSoundDir (A9WControlExtra bit 7 = 0x0080) selects:
    //   0 = capture: pull a sample from the codec into the FIFO
    //   1 = playback: dequeue a sample and push to the codec
    // In either case A9MFifoFull (A9WStatus bit 11 = 0x0800) tracks the
    // FIFO state, and A9IntSnd is asserted on every fire so the kernel's
    // sample-rate ISR can refill / drain the FIFO.
    if (!(m_a9_control & 0x0800)) return;
    // A9IntSnd cadence: MAME asserts the interrupt on EVERY 8 kHz fire,
    // but that starves the V30 — the 3c v5.20f kernel services CSINT
    // with a "drain until empty" loop and relies on its idle task still
    // getting CPU time to reset the watchdog. With per-sample interrupts
    // the idle task never runs during recording, the watchdog NMI fires
    // within 750 ms, and the power-fail handler switches the machine
    // off mid-recording. The drain-until-empty ISR shape also implies
    // the real chip batches: interrupt when the capture FIFO needs
    // EMPTYING (full) or the playback FIFO needs REFILLING (empty),
    // i.e. an effective 500 Hz service rate at the 16-byte FIFO depth.
    // The interrupt must be EDGE-triggered on the service threshold, not
    // re-asserted on every fire past it: A9IntSnd is the highest-priority
    // interrupt bit, so a continuously re-asserting CSINT starves the
    // Tick interrupt whose ISR is the only thing that resets the
    // watchdog while the kernel sits in its quiet record/playback HLT
    // loop. (A starved Tick ISR means a watchdog NMI ~0.75 s into every
    // recording, which the v5.20f kernel treats as a fatal fault — its
    // NMI handler panics with code 0x3d after re-arming Tick.)
    bool wantInt;
    if (m_a9_control_extra & 0x0080) {
        // Playback: pop one sample to the codec. Interrupt on the
        // just-ran-empty edge so the kernel refills a FIFO's worth.
        bool hadData = m_snd_fifo_size > 0;
        if (m_snd_fifo_size > 0) {
            uint8_t s = sndFifoDequeue();
            if (m_pcm_out) m_pcm_out(s);
        }
        if (m_snd_fifo_size < int(m_snd_fifo.size())) m_a9_status &= ~0x0800;
        wantInt = hadData && (m_snd_fifo_size == 0);
    } else {
        // Capture: push one sample from the codec into the FIFO.
        //
        // A9MFifoFull (A9WStatus bit 0x0800) is direction-sensitive: it
        // means "the FIFO has no more service to offer the kernel".
        // MAME sets it on FIFO-FULL in capture mode, but the 3c v5.20f
        // CSINT handler (kernel @ a000:8dea) drains with
        //     in al,0x1a ; stosb ; in ax,0x04 ; test ax,0x800 ; je loop
        // i.e. it keeps reading while the bit is CLEAR and stops when it
        // is SET — so in capture the bit must mean "FIFO EMPTY". With
        // MAME's full-means-set polarity the ISR free-runs at I/O speed,
        // fills the driver's buffer halves with garbage in milliseconds,
        // and deadlocks in IRQ context waiting for the consumer process
        // (which can never run) to release a buffer slot. The drain side
        // of this lives in ioRead(0x1a): the bit is set again when a
        // read empties the FIFO.
        if (m_snd_fifo_size < int(m_snd_fifo.size())) {
            uint8_t s = m_pcm_in ? m_pcm_in() : 0;
            sndFifoEnqueue(s);
        } else if (m_snd_missed < 4096) {
            // Fire arrived with the FIFO full. Real hardware drops the
            // sample; our emulated CSINT latency is far larger than the
            // real kernel's (whole emulated milliseconds between the
            // FIFO filling and the ISR draining), so dropping here
            // cost ~55% of every recording's samples — a 2 s recording
            // came out 0.9 s long. Instead remember how many fires were
            // missed and replay them as the kernel drains (see
            // ioRead 0x1a): the FIFO still looks 16 deep and the
            // status-bit protocol is unchanged, but the capture stream
            // delivers the full 8 kHz of audio.
            m_snd_missed++;
        }
        if (m_snd_fifo_size > 0) m_a9_status &= ~0x0800;
        // Interrupt on the half-full EDGE (headroom for the kernel's
        // drain loop) with a one-shot full-edge fallback in case a drain
        // stopped between half and full — both fire exactly once per
        // crossing, so Tick never starves.
        wantInt = (m_snd_fifo_size == int(m_snd_fifo.size()) / 2) ||
                  (m_snd_fifo_size == int(m_snd_fifo.size()) && m_snd_missed == 0);
    }
    if (wantInt) {
        m_a9_interrupt_status |= A9IntSnd;
        updateInterrupts();
    }
}

void PsionAsic9::updateInterrupts() {
    uint16_t active = m_a9_interrupt_status & m_a9_interrupt_mask;
    bool nextInt = active != 0;
    if (nextInt != m_int_line) {
        m_int_line = nextInt;
        if (m_int_cb) m_int_cb(nextInt);
    }
    // NMI from status low-nibble (watchdog/protected-mode/door/lowbat).
    bool nmiNow = (m_a9_status & 0x000F) != 0;
    if (nmiNow != m_nmi_line) {
        m_nmi_line = nmiNow;
        if (m_nmi_cb) m_nmi_cb(nmiNow);
    }
    static const char *irqDbg = std::getenv("PSION_IRQ_DEBUG");
    if (irqDbg && irqDbg[0] && irqDbg[0] != '0' && nextInt) {
        std::fprintf(stderr, "[IRQ] asserted status=%04X mask=%04X active=%04X\n",
                     m_a9_interrupt_status, m_a9_interrupt_mask, active);
    }
}

bool PsionAsic9::isProtected(uint32_t offset) {
    if (!m_a9_protection_mode) return false;
    // MAME: offset <= lower || offset > upper (note <=, not <)
    return offset <= m_a9_protection_lower || offset > m_a9_protection_upper;
}

uint32_t PsionAsic9::translateAddress(uint32_t offset) const {
    // TODO: honour psel_6000/7000/8000/9000 pages for 0x60000..0x9FFFF.
    // For now, return the offset unchanged so Series 3c driver's naive
    // decode (RAM/ROM split at 0x80000) keeps working.
    return offset;
}

uint16_t PsionAsic9::memRead(uint32_t offset, uint16_t mask) {
    (void)mask;
    uint32_t a = translateAddress(offset);
    if (!m_mem_reader) return 0xFFFF;
    uint16_t lo = m_mem_reader(a);
    uint16_t hi = m_mem_reader(a + 1);
    return uint16_t(lo | (hi << 8));
}

void PsionAsic9::memWrite(uint32_t offset, uint16_t data, uint16_t mask) {
    (void)data; (void)mask;
    if (isProtected(offset)) {
        // Protected-mode violation sets the NMI status bit in MAME.
        m_a9_status |= 0x0002; // A9MProtectedModeNMI
        updateInterrupts();
        return;
    }
    // TODO: write-back through host. No-op for now.
}

// ──────────────────────────────────────────────────────────────────────
// I/O register file. Offsets mirror MAME's psion_asic9_device::io_r /
// io_w (reference/mame-psion/machine/psion_asic9.cpp). MAME dispatches
// on (word_offset << 1), i.e. the byte offset within the 0x00..0xFF
// register window, so our case labels are byte offsets and match MAME's
// literal case labels one-for-one.

uint16_t PsionAsic9::ioRead(uint32_t offset, uint16_t mask) {
    (void)mask;
    switch (offset & 0xFE) {
    case 0x00: // A9Index — not used
        return 0;
    case 0x02: // A9WControl
        return m_a9_control;
    case 0x04: { // A9WStatus — side-effect: reading clears A9MReset
        uint16_t v = m_a9_status;
        m_a9_status &= ~0x2000;
        if (std::getenv("PSION_IRQ_TRACE")) {
            static uint64_t n = 0;
            if (++n % 100 == 0) std::fprintf(stderr, "[asic9] R status #%llu = %04x\n",
                (unsigned long long)n, v);
        }
        return v;
    }
    case 0x06: // A9BInterruptStatus (16-bit on the ASIC9MX)
        if (std::getenv("PSION_IRQ_TRACE")) {
            std::fprintf(stderr, "[asic9] R isStatus mask=%04x status=%04x masked=%04x\n",
                m_a9_interrupt_mask, m_a9_interrupt_status,
                m_a9_interrupt_status & m_a9_interrupt_mask);
        }
        return uint16_t(m_a9_interrupt_status & m_a9_interrupt_mask);
    case 0x08: // A9BInterruptMask
        return m_a9_interrupt_mask;
    case 0x12: // A9WFrc1Data
        return m_frc1_count;
    case 0x14: // A9BProtectionOff (reading disables protection)
        m_a9_protection_mode = false;
        return 0;
    case 0x1a: { // A9BSoundData (read)
        // Dequeue one captured sample. Direction-sensitive status bit:
        // in capture mode 0x0800 means "FIFO empty, stop reading" (see
        // the fireSnd comment — derived from the 3c kernel's CSINT
        // drain loop, which reads until the bit goes high). In playback
        // mode keep MAME's semantics (bit = FIFO full, cleared here once
        // a read makes room — the kernel never reads in playback, but
        // the codec self-test pokes both directions).
        m_dbg_snd_data_reads++;
        const bool capture = !(m_a9_control_extra & 0x0080);
        if (m_snd_fifo_size == 0) {
            if (capture) m_a9_status |= 0x0800;
            return 0;
        }
        uint8_t v = sndFifoDequeue();
        if (capture) {
            // Replay one missed fire per drained slot (see fireSnd's
            // capture backlog comment) so the kernel reads the complete
            // 8 kHz stream despite emulated CSINT latency.
            if (m_snd_missed > 0) {
                uint8_t s = m_pcm_in ? m_pcm_in() : 0;
                sndFifoEnqueue(s);
                m_snd_missed--;
            }
            if (m_snd_fifo_size == 0) m_a9_status |= 0x0800;
        } else {
            if (!sndFifoFull()) m_a9_status &= ~0x0800;
        }
        return v;
    }
    case 0x1e: // A9WFrc2Data
        return m_frc2_count;
    case 0x20: // A9WPortABData
        return uint16_t((m_port_ab_r ? m_port_ab_r() : 0) & ~m_a9_port_ab_ddr);
    case 0x22: // A9WPortABDDR
        return m_a9_port_ab_ddr;
    case 0x24: // A9WPortCDData — 0 unless the host wires a reader
        return m_port_cd_r ? m_port_cd_r(m_a9_port_cd_data) : 0;
    case 0x26: // A9BPortCDDDR
        return uint16_t(m_a9_port_c_ddr | (m_a9_port_d_ddr << 8));
    case 0x28: // A9BPageSelect6000 / 7000
        return uint16_t(m_a9_psel_6000 | (m_a9_psel_7000 << 8));
    case 0x2a: // A9BPageSelect8000 / 9000
        return uint16_t(m_a9_psel_8000 | (m_a9_psel_9000 << 8));
    case 0x2c: // A9WControlExtra
        return m_a9_control_extra;
    case 0x80: // A9WRtcLSW
        return uint16_t(m_rtc & 0xFFFF);
    case 0x82: // A9WRtcMSW
        return uint16_t((m_rtc >> 16) & 0xFFFF);
    case 0x8a: { // A9BSerialData
        uint8_t v = m_a9_serial_data;
        if ((m_a9_serial_control & 0x10) == 0x10)
            m_a9_serial_data = receiveFrame();
        return v;
    }
    case 0x8e: // A9BChannelSelect
        return m_a9_channel_select;
    default:
        return 0;
    }
}

void PsionAsic9::ioWrite(uint32_t offset, uint16_t data, uint16_t mask) {
    const bool accLo = (mask & 0x00FF) != 0;
    const bool accHi = (mask & 0xFF00) != 0;
    switch (offset & 0xFE) {
    case 0x00: // A9WPost
        if (accLo) m_post = uint8_t(data);
        break;
    case 0x02: { // A9WControl (16-bit register)
        // MAME only retunes the FRC timers here (and only on rate-bit
        // changes); it does NOT touch the tick or watchdog timers. Our
        // previous code unconditionally reset both on every A9WControl
        // write, which could starve the tick interrupt if the ROM
        // rewrote A9WControl more often than 1/32 s.
        uint16_t prev = m_a9_control;
        uint16_t merged = (prev & ~mask) | (data & mask);
        m_a9_control = merged;
        // A9MSoundEnable (bit 11) edge: discard any capture backlog so a
        // stale missed-fire count can't bleed into the next codec
        // session. PSION_SND_TRACE logs the edge for record/playback
        // flow debugging.
        if ((merged ^ prev) & 0x0800) {
            m_snd_missed = 0;
            if (std::getenv("PSION_SND_TRACE"))
                std::fprintf(stderr, "[snd] SoundEnable %d->%d (ctl=%04x extra=%04x fifo=%d)\n",
                             (prev >> 11) & 1, (merged >> 11) & 1,
                             merged, m_a9_control_extra, m_snd_fifo_size);
        }
        // Mirror MAME's "only reconfigure RAM on bits-3-4 change" gate
        // (psion_asic9.cpp line 830). We carry m_a9_ram_type separately
        // from m_a9_control so the seeded default survives writes that
        // happen to have bits 3-4 = 0 (which is the kernel's very first
        // A9WControl write on the 3a/3c/Siena).
        uint8_t prevType = uint8_t((prev   >> 3) & 0x3);
        uint8_t newType  = uint8_t((merged >> 3) & 0x3);
        if (newType != prevType) {
            m_a9_ram_type = newType;
        }
        if ((merged & A9CtrlFrc1Hi) != (prev & A9CtrlFrc1Hi)) reloadFrc1Timer();
        if ((merged & A9CtrlFrc2Hi) != (prev & A9CtrlFrc2Hi)) reloadFrc2Timer();
        break;
    }
    case 0x04: // A9WLcdSize (16-bit)
        //  b0-b10 A9MLcdNumberOfPixels - (end-of-frame addr / 16) - 1
        // b11-b15 A9MLcdLineLength     - (pixels per line / 32) - 1
        m_a9_lcd_size = (m_a9_lcd_size & ~mask) | (data & mask);
        break;
    case 0x06: // A9WLcdControl (pixel rate, AC line rate, mode bits)
        // Acknowledged but not simulated at phase 0.
        break;
    case 0x08: // A9BInterruptMask (lo) / A9BNmiClear (hi)
        if (accLo) { m_a9_interrupt_mask = uint16_t((m_a9_interrupt_mask & 0xFF00) | (data & 0xFF)); }
        // MAME clears NMI status on any write to bits 8-15 (ACCESSING_BITS_8_15),
        // regardless of the data value. The previous guard `if (data & 0xFF00)` was
        // wrong: it prevented clearing when the kernel wrote e.g. 0x0002 (new mask
        // in low byte, zero NMI-clear byte), leaving the watchdog NMI asserted.
        // On the ASIC9MX the high byte ALSO carries mask bits 8/9 for the
        // integrated UARTs — the v6.16f kernel programs them with 16-bit
        // RMW cycles of this register (in ax,8 / or / out 8,ax), so a
        // 16-bit write loads the full mask. Classic kernels write zero
        // high bytes, which store zeros into bits that have no sources.
        if (accHi) {
            m_a9_status &= 0xFFF0;
            m_a9_interrupt_mask = uint16_t((m_a9_interrupt_mask & 0x00FF) | (data & 0xFF00));
        }
        if (std::getenv("PSION_IRQ_TRACE")) {
            std::fprintf(stderr, "[asic9] mask <= %04x (data=%04x accLo=%d accHi=%d)\n",
                         m_a9_interrupt_mask, data, accLo, accHi);
        }
        updateInterrupts();
        break;
    case 0x0a: // A9BNonSpecificEoi (lo) / A9BStartFlagClear (hi)
        // MAME: pure no-op for both bytes. When the SIB heartbeat is
        // active (3c / Siena), the hi byte clears bit 12 so the kernel
        // sees a clean 0→1 edge on the next heartbeat pulse.
        if (accHi && m_sib_heartbeat) m_a9_status &= ~0x1000;
        break;
    case 0x0c: // A9BTimerEoi (lo) / A9BSerialSlaveEoi (hi)
        if (accLo) m_a9_interrupt_status &= ~A9IntTick;
        if (accHi) m_a9_interrupt_status &= ~A9IntSds;
        updateInterrupts();
        break;
    case 0x0e: // A9Frc1Eoi (lo) / A9Frc2Eoi (hi)
        if (accLo) m_a9_interrupt_status &= ~A9IntFrc1;
        if (accHi) m_a9_interrupt_status &= ~A9IntFrc2;
        updateInterrupts();
        break;
    case 0x10: // A9WResetWatchDog
        if (std::getenv("PSION_SND_TRACE")) {
            static uint64_t n = 0;
            if (++n % 64 == 1) std::fprintf(stderr, "[snd] wd reset #%llu (count was %d)\n",
                                            (unsigned long long)n, m_watchdog_count);
        }
        m_watchdog_count = 0;
        break;
    case 0x12: // A9WFrc1Data (16-bit)
        m_frc1_reload = (m_frc1_reload & ~mask) | (data & mask);
        m_frc1_count  = m_frc1_reload;
        break;
    case 0x14: // A9BProtectionOn (lo) / A9BProtectionOff (hi)
        if (accLo) m_a9_protection_mode = true;
        if (accHi) m_a9_protection_mode = false;
        break;
    case 0x16: // A9WProtectionUpper (16-bit)
        {
            uint16_t upper = uint16_t(((m_a9_protection_upper >> 4) & ~mask) | (data & mask));
            m_a9_protection_upper = (uint32_t(upper) << 4) | 0x0F;
        }
        break;
    case 0x18: // A9WProtectionLower (16-bit)
        {
            uint16_t lower = uint16_t(((m_a9_protection_lower >> 4) & ~mask) | (data & mask));
            m_a9_protection_lower = uint32_t(lower) << 4;
        }
        break;
    case 0x1a: // A9BSoundData
        // Mirror MAME (psion_asic9.cpp:960): enqueue without checking
        // full (the kernel polls A9MFifoFull before pushing), then set
        // the FifoFull status bit if the new state is full.
        if (accLo) {
            m_dbg_snd_data_writes++;
            sndFifoEnqueue(uint8_t(data));
            if (sndFifoFull()) m_a9_status |= 0x0800;
        }
        break;
    case 0x1c: // A9BSoundEoi
        if (accLo) {
            m_dbg_snd_eoi_writes++;
            m_a9_interrupt_status &= ~A9IntSnd;
            updateInterrupts();
        }
        break;
    case 0x1e: // A9WFrc2Data
        m_frc2_reload = (m_frc2_reload & ~mask) | (data & mask);
        m_frc2_count  = m_frc2_reload;
        break;
    case 0x20: // A9WPortABData (16-bit)
        if (m_port_ab_w) m_port_ab_w(data);
        break;
    case 0x22: // A9WPortABDDR (16-bit)
        m_a9_port_ab_ddr = (m_a9_port_ab_ddr & ~mask) | (data & mask);
        break;
    case 0x24: // A9WPortCDData — latched for the port C/D reader
        m_a9_port_cd_data = (m_a9_port_cd_data & ~mask) | (data & mask);
        if (std::getenv("PSION_PACK_TRACE"))
            std::fprintf(stderr, "[a9] PortCD <= %04x mask=%04x\n", data, mask);
        break;
    case 0x26: // A9BPortCDDDR
        if (accLo) m_a9_port_c_ddr = uint8_t(data & 0xFF);
        if (accHi) m_a9_port_d_ddr = uint8_t((data >> 8) & 0xFF);
        break;
    case 0x28: // A9BPageSelect6000 (lo) / 7000 (hi)
        if (accLo) {
            uint8_t nv = uint8_t(data & 0xFF);
            m_a9_psel_6000 = nv;
        }
        if (accHi) m_a9_psel_7000 = uint8_t((data >> 8) & 0xFF);
        break;
    case 0x2a: // A9BPageSelect8000 (lo) / 9000 (hi)
        if (accLo) m_a9_psel_8000 = uint8_t(data & 0xFF);
        if (accHi) m_a9_psel_9000 = uint8_t((data >> 8) & 0xFF);
        // PSION_PSEL_TRACE=1 logs every write that touches psel_8000 or
        // psel_9000 with the resulting full word — used to bisect a
        // mismatch in I/O port 0x2a's read value vs MAME.
        if (std::getenv("PSION_PSEL_TRACE")) {
            std::fprintf(stderr,
                "[psel] OUT 0x2a data=%04x mask=%04x → psel_8000=%02x psel_9000=%02x\n",
                data, mask, m_a9_psel_8000, m_a9_psel_9000);
        }
        break;
    case 0x2c: { // A9WControlExtra (16-bit)
        uint16_t newExtra = (m_a9_control_extra & ~mask) | (data & mask);
        if ((newExtra ^ m_a9_control_extra) & 0x0080) {
            if (std::getenv("PSION_SND_TRACE"))
                std::fprintf(stderr, "[snd] SoundDir -> %s (extra=%04x fifo=%d)\n",
                             (newExtra & 0x0080) ? "playback" : "capture",
                             newExtra, m_snd_fifo_size);
        }
        m_a9_control_extra = newExtra;
        // Keyboard COL strobe: MAME decodes the low nibble into a 1-hot
        // byte mask, 0 meaning "no columns selected".
        if (m_col_cb) {
            uint8_t col = 0;
            switch (newExtra & 0x0F) {
            case 0x0: col = 0xFF; break;
            case 0x8: col = 0x01; break;
            case 0x9: col = 0x02; break;
            case 0xA: col = 0x04; break;
            case 0xB: col = 0x08; break;
            case 0xC: col = 0x10; break;
            case 0xD: col = 0x20; break;
            case 0xE: col = 0x40; break;
            case 0xF: col = 0x80; break;
            default:  col = 0x00; break;
            }
            m_col_cb(col);
        }
        // BuzzTog is bit 4; drive the buzzer when BuzzFromFrc1 (bit 5) is 0.
        if (m_buz_cb && !(newExtra & 0x0020)) m_buz_cb((newExtra & 0x0010) != 0);
        break;
    }
    case 0x2e: // A9WPumpControl — not simulated
        if (std::getenv("PSION_PACK_TRACE"))
            std::fprintf(stderr, "[a9] PumpCtl <= %04x mask=%04x\n", data, mask);
        break;
    case 0x80: { // A9WRtcLSW (16-bit)
        uint16_t lsw = uint16_t(m_rtc & 0xFFFF);
        lsw = (lsw & ~mask) | (data & mask);
        m_rtc = (m_rtc & 0xFFFF0000u) | lsw;
        break;
    }
    case 0x82: { // A9WRtcMSW (16-bit)
        uint16_t msw = uint16_t((m_rtc >> 16) & 0xFFFF);
        msw = (msw & ~mask) | (data & mask);
        m_rtc = (m_rtc & 0x0000FFFFu) | (uint32_t(msw) << 16);
        break;
    }
    case 0x84: // A9WNullFrame
        transmitFrame(NULL_FRAME);
        break;
    case 0x8a: // A9BSerialData
        if (accLo && (m_a9_serial_control & 0xC0) == 0x80)
            transmitFrame(uint16_t(DATA_FRAME | (data & 0xFF)));
        break;
    case 0x8c: // A9BSerialControl
        if (accLo) {
            m_a9_serial_control = uint8_t(data & 0xFF);
            transmitFrame(uint16_t(CONTROL_FRAME | m_a9_serial_control));
            if ((m_a9_serial_control & 0x40) == 0x40)
                m_a9_serial_data = receiveFrame();
        }
        break;
    case 0x8e: // A9BChannelSelect
        if (accLo) m_a9_channel_select = uint8_t(data & 0xFF);
        break;
    default:
        // Unknown register; ignore silently.
        break;
    }
}

// ──────────────────────────────────────────────────────────────────────
// External interrupt inputs

void PsionAsic9::setMxUartInt(int uart, bool state) {
    // Level inputs from the ASIC9MX's integrated 16550s: UART0
    // (I/O 0x40-0x4E) on status bit 8, UART1 (0x50-0x5E, the Remote
    // Link port) on bit 9. The kernel's UART ISR services the 16550
    // (IIR reads) until the line drops and finishes with a
    // non-specific EOI — there is no per-source EOI for these bits.
    const uint16_t bit = (uart == 0) ? A9IntMxUart0 : A9IntMxUart1;
    if (state) m_a9_interrupt_status |= bit;
    else       m_a9_interrupt_status &= ~bit;
    updateInterrupts();
}

void PsionAsic9::setSdsInt(bool state) {
    if (state) m_a9_interrupt_status |= A9IntSds;
    else       m_a9_interrupt_status &= ~A9IntSds;
    updateInterrupts();
}
void PsionAsic9::setEint0(bool state) {
    if (state) m_a9_interrupt_status |= A9IntEint0;
    else       m_a9_interrupt_status &= ~A9IntEint0;
    updateInterrupts();
}
void PsionAsic9::setEint1(bool state) {
    if (state) m_a9_interrupt_status |= A9IntEint1;
    else       m_a9_interrupt_status &= ~A9IntEint1;
    updateInterrupts();
}
void PsionAsic9::setEint2(bool state) {
    if (state) m_a9_interrupt_status |= A9IntEint2;
    else       m_a9_interrupt_status &= ~A9IntEint2;
    updateInterrupts();
}
void PsionAsic9::setMedChng(bool state) {
    // MAME: medchng_w sets/clears A9MDoorNMI (bit 2 of A9WStatus) and
    // defers NMI line management to update_interrupts() which ORs all
    // NMI-status bits (A9WStatus & 0x000f). Driving the NMI line
    // directly bypassed the status bit, so the CPU's NMI acknowledge
    // path could not distinguish door NMI from watchdog NMI, and a
    // subsequent A9BNmiClear write could not re-evaluate the door bit.
    if (state) m_a9_status |= 0x04;   // A9MDoorNMI
    else       m_a9_status &= ~0x04;
    updateInterrupts();
}

uint8_t PsionAsic9::inta() {
    // Vector is 0x78 + highest-priority unmasked pending bit for the
    // classic 8 sources. The ASIC9MX UARTs (bits 8/9) do NOT extend
    // the vector range upward — 0x80+ are EPOC16 software-interrupt
    // vectors (the int 0x8b/0x8e syscalls live there). They fire
    // vectors 0x76/0x77 instead: the v6.16f kernel builds hardware
    // dispatch stubs there (same push ds/es/bp + chain-list shape as
    // the 0x78-0x7F block, chain heads at 0x6122/0x6126 vs the
    // classic block's 0x0452-0x046E) and installs the UART ISRs into
    // them via the expansion-interrupt service (int 0x8b ah=0x19,
    // channels 0x0D/0x0E).
    uint16_t active = m_a9_interrupt_status & m_a9_interrupt_mask;
    if (active == 0) return 0x78;
    uint8_t vec = 0x78;
    for (int i = 0; i < 8; ++i) {
        if (active & (1 << i)) { vec = uint8_t(0x78 + i); break; }
    }
    if (vec == 0x78 && !(active & 0x01)) {
        if      (active & A9IntMxUart0) vec = 0x76;
        else if (active & A9IntMxUart1) vec = 0x77;
    }
    if (std::getenv("PSION_WAKE_TRACE")) {
        static uint64_t n = 0;
        if (++n <= 20 || n % 200 == 0)
            std::fprintf(stderr,
                "[inta] #%llu vec=%02x psel6=%02x psel7=%02x status=%04x mask=%04x\n",
                (unsigned long long)n, vec,
                m_a9_psel_6000, m_a9_psel_7000,
                m_a9_interrupt_status, m_a9_interrupt_mask);
    }
    return vec;
}

void PsionAsic9::setSibChannelReader(int ch, std::function<uint8_t()> r) {
    if (ch >= 0 && ch < 8) m_data_r[ch] = std::move(r);
}

void PsionAsic9::setSibChannelWriter(int ch, std::function<void(uint16_t)> w) {
    if (ch >= 0 && ch < 8) m_data_w[ch] = std::move(w);
}

// ──────────────────────────────────────────────────────────────────────
// LCD scanout.
//
// MAME's ASIC9 panel is a dual-plane greyscale panel where the two
// planes select one of four greyscales; we render a two-bit image into
// dst. The framebuffer base in RAM is nominally at the top of the RAM
// area selected by A9WLcdSize.

bool PsionAsic9::readLCD(uint8_t *dst, int width, int height) const {
    if (!m_mem_reader) {
        std::memset(dst, 0, size_t(width) * size_t(height));
        return false;
    }
    // MAME's screen_update reads the framebuffer from offset 0x0400 in
    // ASIC9's RAM space (hardcoded — not paged via psel registers).
    //     lineWidthPx = (bits11_15(lcdSize) + 1) * 32   (pixels)
    //     size        = (bits0_10(lcdSize) + 1) * 16    (bytes per plane)
    // Falls back to 480x160 stride if LcdSize is still zero (pre-init).
    uint32_t vramBase = 0x0400;
    int lineWidthPx = ((m_a9_lcd_size >> 11) & 0x1F) ? (((m_a9_lcd_size >> 11) & 0x1F) + 1) * 32 : width;
    int stride = (lineWidthPx + 7) / 8;
    uint16_t planeSize = uint16_t(((m_a9_lcd_size & 0x07FF) + 1) * 16);
    if (planeSize == 16) planeSize = uint16_t(stride * height); // pre-init fallback
    for (int y = 0; y < height; ++y) {
        for (int xByte = 0; xByte < stride; ++xByte) {
            uint8_t black = m_mem_reader(vramBase + y * stride + xByte);
            uint8_t grey  = m_mem_reader(vramBase + planeSize + y * stride + xByte);
            int baseX = xByte * 8;
            for (int bit = 0; bit < 8; ++bit) {
                int x = baseX + bit;
                if (x >= width) break;
                uint8_t pixel = 0;
                if (black & (1 << bit))       pixel = 1;
                else if (grey & (1 << bit))   pixel = 2;
                dst[y * width + x] = pixel;
            }
        }
    }
    return true;
}

// ──────────────────────────────────────────────────────────────────────
// SIBO helpers (stubs)

bool PsionAsic9::channelActive(int ch) const {
    return (m_a9_channel_select & (1 << ch)) != 0;
}
void PsionAsic9::transmitFrame(uint16_t data) {
    for (int ch = 0; ch < 8; ++ch) {
        if (channelActive(ch) && m_data_w[ch]) m_data_w[ch](data);
    }
}
uint8_t PsionAsic9::receiveFrame() {
    for (int ch = 0; ch < 8; ++ch) {
        if (channelActive(ch) && m_data_r[ch]) return m_data_r[ch]();
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Sound FIFO helpers

void PsionAsic9::sndFifoEnqueue(uint8_t v) {
    if (m_snd_fifo_size >= int(m_snd_fifo.size())) return;
    m_snd_fifo[m_snd_fifo_tail] = v;
    m_snd_fifo_tail = (m_snd_fifo_tail + 1) % m_snd_fifo.size();
    ++m_snd_fifo_size;
}

uint8_t PsionAsic9::sndFifoDequeue() {
    if (m_snd_fifo_size == 0) return 0;
    uint8_t v = m_snd_fifo[m_snd_fifo_head];
    m_snd_fifo_head = (m_snd_fifo_head + 1) % m_snd_fifo.size();
    --m_snd_fifo_size;
    return v;
}
