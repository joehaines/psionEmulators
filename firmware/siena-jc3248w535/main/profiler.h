// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Where the emulator spends its time, on the board itself. The FreeRTOS
// tick (1 kHz) on the emulator's core records the program counter the
// emulator task was interrupted at; report() prints the busiest addresses.
// `idf.py monitor` decodes every 0x4... address it sees into a function
// name and source line, so the report reads as a profile.

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace profiler {

void start(TaskHandle_t task, int core);
// Print the samples taken since the last report, busiest first.
void report();

} // namespace profiler
