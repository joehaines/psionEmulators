// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "sync_service.h"

#include "ram_store.h"
#include "wifi_link.h"

#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <cstdio>
#include <ctime>
#include <set>

namespace sync_service {

namespace {

const char *TAG = "sync";
constexpr size_t kMaxIcs = 4 * 1024 * 1024;   // a calendar's whole history
constexpr int kDaysBefore = 1, kDaysAfter = 31;
constexpr TickType_t kEvery = pdMS_TO_TICKS(30 * 60 * 1000);

Hooks             g_hooks;
SemaphoreHandle_t g_lock;
std::string       g_url;                     // under g_lock
TaskHandle_t      g_task;
bool              g_sntp = false;

// Made on first use: setCalendarUrl may come before start.
SemaphoreHandle_t lock() {
    if (!g_lock) g_lock = xSemaphoreCreateMutex();
    return g_lock;
}

void status(const std::string &s) {
    ESP_LOGI(TAG, "%s", s.c_str());
    if (g_hooks.status) g_hooks.status(s);
}

// The calendar, by HTTPS GET.
bool fetch(const std::string &url, std::string &out) {
    esp_http_client_config_t cfg = {};
    cfg.url = url.c_str();
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.timeout_ms = 20000;
    cfg.buffer_size = 4096;
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return false;
    bool ok = false;
    if (esp_http_client_open(c, 0) == ESP_OK) {
        esp_http_client_fetch_headers(c);
        const int code = esp_http_client_get_status_code(c);
        if (code == 200) {
            char buf[2048];
            int n;
            while ((n = esp_http_client_read(c, buf, sizeof buf)) > 0 && out.size() < kMaxIcs)
                out.append(buf, size_t(n));
            ok = n >= 0;
        } else {
            status("Calendar: the server said " + std::to_string(code));
        }
    }
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    return ok;
}

std::set<std::string> loadSynced(const std::string &path) {
    std::set<std::string> keys;
    FILE *f = path.empty() ? nullptr : std::fopen(path.c_str(), "r");
    if (!f) return keys;
    char line[512];
    while (std::fgets(line, sizeof line, f)) {
        std::string k(line);
        while (!k.empty() && (k.back() == '\n' || k.back() == '\r')) k.pop_back();
        if (!k.empty()) keys.insert(k);
    }
    std::fclose(f);
    return keys;
}

void saveSynced(const std::string &path, const std::set<std::string> &keys) {
    if (path.empty()) return;
    FILE *f = std::fopen(path.c_str(), "w");
    if (!f) return;
    for (const std::string &k : keys) std::fprintf(f, "%s\n", k.c_str());
    std::fclose(f);
}

void syncCalendar() {
    xSemaphoreTake(lock(), portMAX_DELAY);
    const std::string url = g_url;
    xSemaphoreGive(lock());
    if (url.empty()) return;
    if (!haveTime()) {
        status("Calendar: waiting for the time");
        return;
    }
    std::string ics;
    if (!fetch(url, ics)) {
        if (ics.empty()) status("Calendar: couldn't fetch it");
        return;
    }
    const time_t now = std::time(nullptr);
    std::vector<calendar_sync::Event> events =
        calendar_sync::parseIcs(ics, now - kDaysBefore * 86400, now + kDaysAfter * 86400);
    ics.clear();
    ics.shrink_to_fit();

    // Only what this profile's Agenda hasn't had; and forget what has
    // passed out of the window, so the list doesn't grow for ever.
    const std::string path = ram_store::profileFile("CALSYNC.TXT");
    std::set<std::string> synced = loadSynced(path), kept;
    std::vector<calendar_sync::Event> fresh;
    for (const calendar_sync::Event &e : events) {
        if (synced.count(e.key())) kept.insert(e.key());
        else fresh.push_back(e);
    }
    char line[64];
    std::snprintf(line, sizeof line, "Calendar: %u events, %u new", unsigned(events.size()),
                  unsigned(fresh.size()));
    status(line);
    if (fresh.empty()) {
        if (kept.size() != synced.size()) saveSynced(path, kept);
        return;
    }
    for (const calendar_sync::Event &e : fresh) kept.insert(e.key());
    saveSynced(path, kept);
    if (g_hooks.deliver) g_hooks.deliver(std::move(fresh));
}

void task(void *) {
    bool everConnected = false;
    for (;;) {
        // Woken by syncNow, or every half hour.
        const bool asked = ulTaskNotifyTake(pdTRUE, everConnected ? kEvery : pdMS_TO_TICKS(2000)) > 0;
        if (!wifi_link::connected()) {
            if (asked) status("Sync: Wi-Fi isn't connected");
            continue;
        }
        if (!g_sntp) {
            esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
            g_sntp = esp_netif_sntp_init(&cfg) == ESP_OK;
            esp_netif_sntp_sync_wait(pdMS_TO_TICKS(15000));
        }
        everConnected = true;
        syncCalendar();
    }
}

} // namespace

void start(const Hooks &hooks) {
    g_hooks = hooks;
    // Its stack in PSRAM: TLS wants a deep one, and the task never writes
    // flash (what it keeps goes to the SD card).
    xTaskCreatePinnedToCoreWithCaps(task, "sync", 10240, nullptr, 2, &g_task, 0, MALLOC_CAP_SPIRAM);
}

void setCalendarUrl(const std::string &url) {
    xSemaphoreTake(lock(), portMAX_DELAY);
    g_url = url;
    xSemaphoreGive(lock());
}

void syncNow() {
    if (g_task) xTaskNotifyGive(g_task);
}

bool haveTime() {
    return std::time(nullptr) > 1600000000;   // after 2020: SNTP has answered
}

} // namespace sync_service
