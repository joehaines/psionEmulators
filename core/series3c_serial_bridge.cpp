// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Host serial bridge for SIBO2 devices (Series 3a / 3c / 3mx / Siena /
// Workabout / WorkaboutMX). Exposes the same method names as
// Windermere::Emulator so the WASM dispatcher in wasm/main.cpp can
// pick a host bridge by dynamic_cast without caring about the
// underlying chipset.
//
// The bridge rewires PsionCondor's TXD callback to drain into a host-
// side TX buffer (instead of the Honda slot) and asserts the modem-
// status lines (CTS / DSR / DCD) so the kernel-side PLP server sees a
// live cable. RX from the host pushes directly into the Condor's RX
// FIFO via pushRx().
//
// Kept in a separate translation unit from series3c.cpp so the chipset
// emulation stays unchanged and this new feature is easy to audit or
// revert in isolation. The class declaration in series3c.h hosts the
// method signatures.

#include "series3c.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Series3c {

bool Emulator::serialAttachHost(int uartIndex) {
    if (m_hostAttached) return m_hostUartIndex == uartIndex;
    m_hostAttached = true;
    m_hostUartIndex = uartIndex;
    PsionCondor &u = hostUart(uartIndex);
    m_hostTxBuf.clear();
    // Rewire the bound UART's TXD callback so the kernel's outbound
    // bytes land in our host buffer (instead of going to the Honda
    // slot, which would otherwise route them at the SSD pack which
    // doesn't expect serial data).
    u.setTxdCb([this](uint8_t b) { m_hostTxBuf.push_back(b); });
    // Generate a "cable plugged in" EDGE on the modem-status lines —
    // not just a level. The lines idle high (the boot probe expects
    // MSR=0xF0), so drop them first and re-raise: each transition
    // latches a 16550-style delta bit and, with EDSSI enabled, fires
    // the modem-status interrupt the kernel's link driver uses as its
    // cable-detect trigger. (The IR port has no cable, but the edges
    // are harmless there — the IrLAP driver ignores modem lines.)
    u.setCts(false);
    u.setDsr(false);
    u.setDcd(false);
    u.setCts(true);
    u.setDsr(true);
    u.setDcd(true);
    return true;
}

bool Emulator::serialDetachHost(int /*uartIndex*/) {
    if (!m_hostAttached) return true;
    m_hostAttached = false;
    PsionCondor &u = hostUart(m_hostUartIndex);
    m_hostTxBuf.clear();
    m_hostRxStage.clear();
    // Restore the original wiring depending on the device variant.
    // Siena: Condor TXD goes to Honda. Others: TXD drops on the floor
    // (matches the cable-unplugged carrier state).
    if (m_cfg.model == Model::Siena) {
        m_condor.setTxdCb([this](uint8_t b) { m_honda.writeTxd(b); });
    } else {
        u.setTxdCb([](uint8_t) {});
    }
    // Drop modem-status lines to indicate cable removed.
    u.setCts(false);
    u.setDsr(false);
    u.setDcd(false);
    return true;
}

bool Emulator::serialIsAttached(int uartIndex) const {
    return m_hostAttached && m_hostUartIndex == uartIndex;
}

size_t Emulator::serialWriteFromHost(int /*uartIndex*/, const uint8_t *data, size_t len) {
    if (!m_hostAttached || data == nullptr || len == 0) return 0;
    // Stage rather than pushing straight into the UART: the hardware
    // RX FIFO is 8 entries deep and pushRx drops on full, so any host
    // frame longer than 8 bytes would lose its tail. The executeUntil
    // pump trickles staged bytes in as the kernel drains the FIFO.
    for (size_t i = 0; i < len; i++) m_hostRxStage.push_back(data[i]);
    pumpHostRxStage();
    return len;
}

void Emulator::pumpHostRxStage() {
    PsionCondor &u = hostUart(m_hostUartIndex);
    while (!m_hostRxStage.empty() && u.rxHasRoom()) {
        u.pushRx(m_hostRxStage.front());
        m_hostRxStage.pop_front();
    }
}

size_t Emulator::serialReadToHost(int /*uartIndex*/, uint8_t *dst, size_t cap) {
    if (!m_hostAttached || dst == nullptr || cap == 0 || m_hostTxBuf.empty()) return 0;
    size_t n = std::min(cap, m_hostTxBuf.size());
    std::memcpy(dst, m_hostTxBuf.data(), n);
    m_hostTxBuf.erase(m_hostTxBuf.begin(), m_hostTxBuf.begin() + n);
    return n;
}

size_t Emulator::serialHostTxAvailable(int /*uartIndex*/) const {
    return m_hostTxBuf.size();
}

} // namespace Series3c
