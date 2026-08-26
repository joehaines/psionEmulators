// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "rtc_seed.h"
#include <cstdlib>
#include <ctime>

uint32_t psionInitialRtcSeconds() {
	const char *seed = std::getenv("PSION_RTC_SEED");
	if (!seed || seed[0] == 0 || (seed[0] == '1' && seed[1] == 0))
		return (uint32_t)(std::time(nullptr) - 946684800);
	if (seed[0] == '0' && seed[1] == 0)
		return 0;
	return (uint32_t)std::strtoul(seed, nullptr, 16);
}
