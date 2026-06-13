// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Host serial bridge for the CL-PS7111 family (Osaris / MC218) and, via
// inheritance, the CL-PS7110 (Series 5). Exposes the same method names as
// Windermere::Emulator and Series3c::Emulator so the WASM / harness
// dispatchers can pick a host bridge by dynamic_cast without caring about
// the underlying chipset.
//
// IrDA SIR framing on these SoCs is a UART1-only property (SYSCON1 SIREN /
// IRTXM), so the bridge only accepts uartIndex == 1. Bytes the guest writes
// to UARTDR1 land in uart1.txQueue (drained via serialReadToHost); bytes the
// host pushes via serialWriteFromHost arrive in uart1.rxFifo and surface as
// URXINT1 (interrupt bit 13) through updateUartIrqs().
//
// Kept in a separate translation unit from clps7111.cpp (mirroring
// core/series3c_serial_bridge.cpp) so the chipset emulation stays unchanged
// and this feature is easy to audit or revert in isolation. The method
// signatures live on the CLPS7111::Emulator class in core/clps7111.h.

#include "clps7111.h"
#include "clps7111_defs.h"

namespace CLPS7111 {

// Map the UART's discrete interrupt latches onto pendingInterrupts. The
// CL-PS7110/7111 interrupt controller has individual UART lines (unlike
// Windermere which collapses onto one UART bit per port), so we set/clear
// each discretely: IntRx→URXINT1(13), IntTx→UTXINT(12),
// IntModemStatus→UMSINT(14).
void Emulator::updateUartIrqs() {
	if (uart1.interrupts & UART::IntRx)          pendingInterrupts |=  (1u << URXINT1);
	else                                         pendingInterrupts &= ~(1u << URXINT1);
	// UTXINT (TX FIFO service request) is a LEVEL interrupt on real CL-PS711x:
	// asserted whenever the transmit FIFO has room, deasserted when it's full.
	// The guest's SIR TX driver unmasks UTXINT (INTMR1) only while it has a
	// frame to send and re-masks when done; it expects unmasking to deliver an
	// immediate "FIFO ready" interrupt so its ISR can write the first byte.
	//
	// Modelling it as a set-on-write / clear-on-drain latch (the obvious but
	// wrong choice) races the host bridge's drain: if the host drains the TX
	// queue empty between the guest's frames, the latch clears, and the next
	// time the guest unmasks UTXINT no interrupt fires — its ISR never runs,
	// the transmit stalls, and (for IrLAP) it can no longer return its TinyTP
	// credit / F-frames, so a sustained beam (e.g. a 17 KB file) hangs partway
	// and the watchdog reboots. So drive UTXINT from "queue has room" instead:
	// it's essentially always assertable here (4 KiB soft cap), and the guest's
	// own INTMR1 mask gates when it actually fires.
	if (uart1.hostAttached && uart1.txQueuedBytes() < UART::kTxQueueCap)
	                                             pendingInterrupts |=  (1u << UTXINT);
	else                                         pendingInterrupts &= ~(1u << UTXINT);
	if (uart1.interrupts & UART::IntModemStatus) pendingInterrupts |=  (1u << UMSINT);
	else                                         pendingInterrupts &= ~(1u << UMSINT);
}

bool Emulator::serialAttachHost(int uartIndex) {
	if (uartIndex != 1) return false;  // IrDA SIR is UART1-only on PS711x
	uart1.hostAttached = true;
	uart1.rxFifo.clear();
	uart1.txQueue.clear();
	// Tell EPOC's serial driver "a cable just plugged in" — without this
	// IRQ the kernel won't notice CTS/DSR/DCD just went high.
	uart1.interrupts |= UART::IntModemStatus;
	updateUartIrqs();
	cpu.log("serial: host attached to UART%d", uartIndex);
	return true;
}

bool Emulator::serialDetachHost(int uartIndex) {
	if (uartIndex != 1) return false;
	uart1.hostAttached = false;
	// Preserve rxFifo across detach (see Windermere::serialDetachHost for
	// the Disc_Pdu rationale) — the CPU drains it on its next tick and it's
	// cleared on the next attach so stale bytes never carry across sessions.
	uart1.txQueue.clear();
	uart1.interrupts &= ~UART::IntTx;
	if (uart1.rxFifo.empty()) uart1.interrupts &= ~UART::IntRx;
	uart1.interrupts |= UART::IntModemStatus;
	updateUartIrqs();
	cpu.log("serial: host detached from UART%d (rxFifo retained, %zu B)",
	        uartIndex, uart1.rxFifo.size());
	return true;
}

size_t Emulator::serialWriteFromHost(int uartIndex, const uint8_t *data, size_t len) {
	if (uartIndex != 1 || !uart1.hostAttached) return 0;
	size_t n = uart1.pushRxFromHost(data, len);
	updateUartIrqs();
	return n;
}

size_t Emulator::serialReadToHost(int uartIndex, uint8_t *dst, size_t cap) {
	if (uartIndex != 1 || !uart1.hostAttached) return 0;
	size_t n = uart1.drainTxToHost(dst, cap);
	updateUartIrqs();
	return n;
}

size_t Emulator::serialHostTxAvailable(int uartIndex) const {
	if (uartIndex != 1 || !uart1.hostAttached) return 0;
	return uart1.txQueuedBytes();
}

bool Emulator::serialIsAttached(int uartIndex) const {
	if (uartIndex != 1) return false;
	return uart1.hostAttached;
}

} // namespace CLPS7111
