// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "profiler.h"

#include "esp_freertos_hooks.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "xtensa_context.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>

// FreeRTOS's running task per core (tasks.c); the port's context switch
// reads it the same way.
extern "C" void *volatile pxCurrentTCBs[];

namespace profiler {

namespace {

const char *TAG = "profile";

constexpr int kMaxSamples = 10240;   // 10 s at the 1 kHz tick
constexpr int kTop = 60;

// Samples are kept as 16-bit bucket numbers, to spare internal RAM (the
// Bluetooth stack needs what is left): 32-byte buckets of IRAM, coarser
// 128-byte ones of flash, and one for anywhere else.
constexpr uint32_t kIramBase = 0x40370000, kIramBucket = 32;
constexpr uint32_t kFlashBase = 0x42000000, kFlashBucket = 128;
constexpr uint16_t kFlashCodes = 0x4000, kElsewhere = 0xFFFF;

inline uint16_t IRAM_ATTR bucketOf(uint32_t pc) {
    if (pc >= kIramBase && pc < kIramBase + kFlashCodes * kIramBucket)
        return uint16_t((pc - kIramBase) / kIramBucket);
    if (pc >= kFlashBase && pc < kFlashBase + (kElsewhere - kFlashCodes) * kFlashBucket)
        return uint16_t(kFlashCodes + (pc - kFlashBase) / kFlashBucket);
    return kElsewhere;
}

uint32_t bucketStart(uint16_t b) {
    if (b == kElsewhere) return 0;
    if (b < kFlashCodes) return kIramBase + b * kIramBucket;
    return kFlashBase + (b - kFlashCodes) * kFlashBucket;
}

TaskHandle_t g_task;
int          g_core;
uint16_t    *g_samples;              // emulator-task buckets, in internal RAM
std::atomic<int>      g_count{ 0 };
std::atomic<uint32_t> g_otherTicks{ 0 };   // the core was doing something else

// Runs in the tick interrupt on the emulator's core, which also happens
// while the other core is writing flash with the caches off: so nothing
// here may touch flash or PSRAM (FreeRTOS's task-handle getters live in
// flash). Interrupt entry stores the interrupted task's stack pointer in
// its TCB (pxTopOfStack, the TCB's first field), and the saved frame
// there holds its PC.
void IRAM_ATTR onTick() {
    if (pxCurrentTCBs[g_core] != g_task) {
        g_otherTicks.store(g_otherTicks.load(std::memory_order_relaxed) + 1,
                           std::memory_order_relaxed);
        return;
    }
    const XtExcFrame *frame = *reinterpret_cast<XtExcFrame *const *>(g_task);
    const int n = g_count.load(std::memory_order_relaxed);
    if (n < kMaxSamples) {
        g_samples[n] = bucketOf(uint32_t(frame->pc));
        g_count.store(n + 1, std::memory_order_relaxed);
    }
}

} // namespace

void start(TaskHandle_t task, int core) {
    g_task = task;
    g_core = core;
    g_samples = static_cast<uint16_t *>(
        heap_caps_malloc(kMaxSamples * sizeof(uint16_t), MALLOC_CAP_INTERNAL));
    if (g_samples) esp_register_freertos_tick_hook_for_cpu(onTick, UBaseType_t(core));
}

void report() {
    // Marking the buffer full stops the tick hook writing while we sort.
    const int n = std::min(g_count.exchange(kMaxSamples), kMaxSamples);
    const uint32_t other = g_otherTicks.exchange(0);
    if (n == 0) {
        g_count.store(0);
        return;
    }
    // Sort to count each bucket; the sample buffer is reused.
    std::sort(g_samples, g_samples + n);
    struct Hot { uint16_t bucket; int hits; };
    Hot top[kTop] = {};
    for (int i = 0; i < n;) {
        int j = i;
        while (j < n && g_samples[j] == g_samples[i]) ++j;
        Hot h = { g_samples[i], j - i };
        for (int k = 0; k < kTop; ++k) {
            if (h.hits > top[k].hits) std::swap(h, top[k]);
        }
        i = j;
    }
    ESP_LOGI(TAG, "%d samples in the emulator, %u elsewhere (idle/other tasks); busiest code:",
             n, unsigned(other));
    for (const Hot &h : top) {
        if (!h.hits) break;
        ESP_LOGI(TAG, "%5.1f%%  0x%08lx", 100.0 * h.hits / n, (unsigned long)bucketStart(h.bucket));
    }
    g_count.store(0);
}

} // namespace profiler
