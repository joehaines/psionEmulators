// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include <stdint.h>

// Intel StrongARM SA-1100 SoC — register / memory layout used by the
// Psion Series 7 and netBook.
//
// References:
//   - Intel SA-1100 Microprocessor Developer's Manual (1999).
//   - Psion netBook service manual and schematics.
//   - NetBSD arch/epoc32 and Linux/SA-1100 kernel sources.
//
// The SoC integrates the CPU and all peripherals on one chip. The
// physical address space is split into 3-bit regions:
//
//   region 0 (0x00000000)  nCS0 — boot ROM / flash (up to 64 MB)
//   region 1 (0x10000000)  nCS1
//   region 2 (0x20000000)  nCS2 — on the netBook this is a PCMCIA slot
//   region 3 (0x30000000)  nCS3
//   region 4 (0x40000000)  nCS4
//   region 5 (0x50000000)  nCS5
//   region 8 (0x80000000)  on-chip peripherals: UART0-3, MCP/SSP
//   region 9 (0x90000000)  system control: OS timer, RTC, power, GPIO, intc
//   region A (0xA0000000)  memory controller
//   region B (0xB0000000)  LCD + DMA controller
//   region C (0xC0000000)  SDRAM bank 0
//   region D (0xC8000000)  SDRAM bank 1 (alias inside same 256 MB slot)
//   region E (0xE0000000)  Cache zero / cache clean (NOT a RAM region)
//
// The Psion netBook / Series 7 has a single 32 MB SDRAM bank populated
// at 0xC0000000. The ROM image is 16 MB at 0x00000000.
namespace SA1100 {

// SA-1100 CPU clock on netBook / Series 7. The BSP programs PPCR=0x12
// which selects the 221.184 MHz PLL setting (60 × 3.6864 MHz, derived
// from the same crystal that drives the 3.6864 MHz OS timer). Older
// notes said "we pick 200 MHz as a round number" — but boot-timing
// gates that compare passedCycles to CLOCK_SPEED multiples then fire
// ~10% earlier in real-time than the BSP expects, which matters for
// any kernel logic that polls a peripheral on a wall-clock budget.
// Using the real clock keeps sim-time and real-time aligned 1:1.
enum : int32_t {
    CLOCK_SPEED = 221'184'000,
    TICK_INTERVAL = CLOCK_SPEED / 64,   // 64 Hz tick, matches Windermere
};

// ─── UARTs / Serial channels ──────────────────────────────────────────
// Each serial port has its own 64 KB register window. The SA-1100 has
// five serial channels:
//   Ser0 = USB Device Controller (UDC) at 0x80000000 — NOT a UART
//   Ser1 = SDLC / UART at 0x80010000 (mode-selectable)
//   Ser2 = ICP / IrDA / UART at 0x80030000 (mode-selectable)
//          (0x80020000 is SDLC for Ser1; Ser2 base is 0x80030000.)
//   Ser3 = UART3 at 0x80050000 (console on the netBook)
//   Ser4 = MCP (Multimedia Communications Port) at 0x80060000 — audio
//   HSSP = High-Speed Synchronous Serial Port at 0x80080000
// EPOC uses Ser3 (0x80050000) as the console on the netBook. The UART
// register layout is identical across ports that run in UART mode.
enum : uint32_t {
    UDC_BASE  = 0x80000000,     // Ser0 — USB Device Controller (NOT a UART)
    SER0_BASE = 0x80000000,     // kept for legacy callers; aliases UDC
    SER1_BASE = 0x80010000,     // Ser1 — SDLC / UART
    SDLC_BASE = 0x80020000,     // Ser1 SDLC alternate window (legacy SER2 alias)
    SER2_BASE = 0x80030000,     // Ser2 — ICP / IrDA / UART (corrected)
    SER3_BASE = 0x80050000,     // Ser3 — UART (console)
    MCP_BASE  = 0x80060000,     // Ser4 — MCP (audio codec bus)
    SER4_BASE = 0x80060000,     // legacy alias for MCP_BASE
    HSSP_BASE = 0x80080000,     // High-Speed Synchronous Serial Port

    // UART register offsets (bytes from port base)
    UTCR0 = 0x00,   // control register 0 (frame/stop/parity)
    UTCR1 = 0x04,   // control register 1 (baud MSB)
    UTCR2 = 0x08,   // control register 2 (baud LSB)
    UTCR3 = 0x0C,   // control register 3 (enables)
    UTDR  = 0x14,   // data register
    UTSR0 = 0x1C,   // status 0 (ack-on-write)
    UTSR1 = 0x20,   // status 1 (read-only)

    // IrDA-specific control register (Ser2 in IrDA mode)
    ICCR_REG = 0x10,    // IrDA Control Register

    // USB Device Controller (UDC) register offsets — Ser0
    UDCCR  = 0x00,  // control register
    UDCAR  = 0x04,  // address register
    UDCOMP = 0x08,  // OUT maximum packet
    UDCIMP = 0x0C,  // IN maximum packet
    UDCCS0 = 0x10,  // control/status endpoint 0
    UDCCS1 = 0x14,  // control/status endpoint 1
    UDCCS2 = 0x18,  // control/status endpoint 2
    UDCD0  = 0x20,  // data endpoint 0

    // MCP (Multimedia Communications Port) register offsets — Ser4
    MCCR0 = 0x00,   // control register 0 (clock divisors, audio rates)
    MCDR0 = 0x04,   // audio data register 0
    MCDR1 = 0x08,   // audio data register 1
    MCDR2 = 0x0C,   // telecom data register
    MCSR  = 0x10,   // status register (FIFO flags)
};

// ─── System peripherals (region 9) ────────────────────────────────────
// All register windows are 64 KB-aligned.
enum : uint32_t {
    OSTIMER_BASE = 0x90000000,      // OS timer + watchdog
    OSMR0  = 0x90000000,            // match register 0
    OSMR1  = 0x90000004,
    OSMR2  = 0x90000008,
    OSMR3  = 0x9000000C,
    OSCR   = 0x90000010,            // counter register (R/W)
    OSSR   = 0x90000014,            // status register
    OWER   = 0x90000018,            // watchdog enable
    OIER   = 0x9000001C,            // interrupt enable

    RTC_BASE = 0x90010000,
    RTAR   = 0x90010000,            // alarm register
    RCNR   = 0x90010004,            // counter register (seconds)
    RTTR   = 0x90010008,            // trim
    RTSR   = 0x90010010,            // status

    PWR_BASE = 0x90020000,
    PMCR   = 0x90020000,            // power mode control
    PSSR   = 0x90020004,            // sleep status
    PSPR   = 0x90020008,            // scratch pad
    PWER   = 0x9002000C,            // wake enable
    PCFR   = 0x90020010,            // general config
    PPCR   = 0x90020014,            // CPU clock
    PGSR   = 0x90020018,            // GPIO sleep state
    POSR   = 0x9002001C,            // oscillator status

    RESET_BASE = 0x90030000,
    RSRR   = 0x90030000,            // reset controller
    RCSR   = 0x90030004,            // reset cause

    TUCR   = 0x90030008,            // test unit control

    GPIO_BASE = 0x90040000,
    GPLR   = 0x90040000,            // level register (R)
    GPDR   = 0x90040004,            // direction
    GPSR   = 0x90040008,            // set (W1S)
    GPCR   = 0x9004000C,            // clear (W1C)
    GRER   = 0x90040010,            // rising-edge detect enable
    GFER   = 0x90040014,            // falling-edge detect enable
    GEDR   = 0x90040018,            // edge detect status (W1C)
    GAFR   = 0x9004001C,            // alternate function

    INTC_BASE = 0x90050000,
    ICIP   = 0x90050000,            // IRQ pending
    ICMR   = 0x90050004,            // IRQ mask
    ICLR   = 0x90050008,            // IRQ level (0=IRQ, 1=FIQ)
    ICCR   = 0x9005000C,            // idle mask bit
    ICFP   = 0x90050010,            // FIQ pending
    ICPR   = 0x90050020,            // pending (raw)

    PPC_BASE = 0x90060000,          // peripheral pin controller
    PPDR   = 0x90060000,
    PPSR   = 0x90060004,
    PPAR   = 0x90060008,
    PSDR   = 0x9006000C,
    PPFR   = 0x90060010,
};

// ─── Memory controller (region A) ─────────────────────────────────────
enum : uint32_t {
    MEMCFG_BASE = 0xA0000000,
    MDCNFG = 0xA0000000,
    MDCAS00 = 0xA0000004,
    MDCAS01 = 0xA0000008,
    MDCAS02 = 0xA000000C,
    MSC0   = 0xA0000010,
    MSC1   = 0xA0000014,
    MECR   = 0xA0000018,            // expansion memory config
    MDREFR = 0xA000001C,            // DRAM refresh config
    MDCAS20 = 0xA0000020,
    MDCAS21 = 0xA0000024,
    MDCAS22 = 0xA0000028,
    MSC2   = 0xA000002C,
    SMCNFG = 0xA0000030,
};

// ─── DMA + LCD controller (region B) ──────────────────────────────────
enum : uint32_t {
    DMA_BASE   = 0xB0000000,
    DDAR0 = 0xB0000000, DSR0  = 0xB0000004, DBSA0 = 0xB000000C,
    DBTA0 = 0xB0000010, DBSB0 = 0xB0000014, DBTB0 = 0xB0000018,

    LCD_BASE   = 0xB0100000,
    LCCR0 = 0xB0100000,             // LCD control 0
    DBAR1 = 0xB0100010,             // DMA base address channel 1
    DCAR1 = 0xB0100014,
    DBAR2 = 0xB0100018,
    DCAR2 = 0xB010001C,
    LCCR1 = 0xB0100020,             // control 1 (horizontal timing)
    LCCR2 = 0xB0100024,             // control 2 (vertical timing)
    LCCR3 = 0xB0100028,             // control 3 (bpp + polarity + PCD)
    LCSR  = 0xB0100038,             // LCD status
};

// Interrupt causes (bit position in ICIP/ICMR/ICPR).
enum : int {
    IRQ_GPIO0 = 0,
    IRQ_GPIO1 = 1,
    IRQ_GPIO2 = 2,
    IRQ_GPIO3 = 3,
    IRQ_GPIO4 = 4,
    IRQ_GPIO5 = 5,
    IRQ_GPIO6 = 6,
    IRQ_GPIO7 = 7,
    IRQ_GPIO8 = 8,
    IRQ_GPIO9 = 9,
    IRQ_GPIO10 = 10,
    IRQ_GPIO11_27 = 11,             // aggregated detect for GPIO 11..27
    // SA-1100 interrupt-controller bit assignments, per Intel SA-1100
    // Developer's Manual chapter 11 (cross-verified against MAME's
    // SA-1110 model at reference/mame-code/.../sa1110.h:633-663).
    IRQ_LCD       = 12,             // LCD controller
    IRQ_UDC       = 13,             // USB Device Controller (Ser0)
    // bit 14 = Ser1 SDLC (not modelled — empty slot)
    IRQ_SER1_UART = 15,             // Ser1 UART mode
    IRQ_SER2_ICP  = 16,             // Ser2 IrDA / UART
    IRQ_SER3_UART = 17,             // Ser3 UART (system console)
    IRQ_MCP_AUDIO = 18,             // MCP audio TX/RX FIFO
    IRQ_SSP       = 19,             // SSP / UCB1200 codec on Series 7
    IRQ_DMA_CH0   = 20,             // DMA channel 0 (was 21)
    IRQ_DMA_CH1   = 21,
    IRQ_DMA_CH2   = 22,
    IRQ_DMA_CH3   = 23,
    IRQ_DMA_CH4   = 24,
    IRQ_DMA_CH5   = 25,
    IRQ_OST0      = 26,             // OS timer match 0
    IRQ_OST1      = 27,
    IRQ_OST2      = 28,
    IRQ_OST3      = 29,
    IRQ_RTC_TICK  = 30,             // 1 Hz tick
    IRQ_RTC_ALARM = 31,
    // Legacy aliases for code paths that haven't been retargeted yet.
    // Ser0 on real SA-1100 silicon is the USB Device Controller, not a
    // serial port — the bit-13 name was misleading.  Map it to IRQ_UDC.
    IRQ_SER0_SDLC = IRQ_UDC,
    // MCP and Ser4 are the same block (MCP audio bus).
    IRQ_SER4_MCP  = IRQ_MCP_AUDIO,
};

// Symbolic names for ICMR / ICIP / ICPR bit positions.  Mirrors the
// per-bit diffInterrupts decode that Windermere uses in
// windermere.cpp:2996.  NULL entries mean "no source on this bit" — we
// keep them so the array index matches the IRQ_* enum directly.
inline const char *kIrqNames[32] = {
    "GPIO0",  "GPIO1",  "GPIO2",     "GPIO3",      // 0..3
    "GPIO4",  "GPIO5",  "GPIO6",     "GPIO7",      // 4..7
    "GPIO8",  "GPIO9",  "GPIO10",    "GPIO11_27",  // 8..11
    "LCD",    "UDC",    nullptr,     "SER1_UART",  // 12..15  (14 = Ser1 SDLC, not modelled)
    "SER2",   "SER3",   "MCP_AUDIO", "SSP",        // 16..19
    "DMA0",   "DMA1",   "DMA2",      "DMA3",       // 20..23
    "DMA4",   "DMA5",   "OST0",      "OST1",       // 24..27
    "OST2",   "OST3",   "RTC_TICK",  "RTC_ALARM",  // 28..31
};

}
