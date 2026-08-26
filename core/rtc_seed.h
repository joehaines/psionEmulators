// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include <cstdint>

// Initial RTC seconds count, shared by every device family.
//
// Seeded from host time so a cold boot lands on today's date. That makes two
// runs of the same workload diverge in guest state from the first RTC read
// onward, which the differential verification harness (core/state_trace.h)
// cannot work around — so PSION_RTC_SEED pins it:
//   unset / "1"  -> seed from std::time(nullptr) (default; unchanged)
//   "0"          -> start at the epoch itself
//   any other    -> hex-parsed literal
uint32_t psionInitialRtcSeconds();
