// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "wifi_link.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include <algorithm>
#include <atomic>
#include <cstring>

namespace wifi_link {

namespace {
const char *TAG = "wifi";
std::atomic<bool> g_connected{ false };
bool g_wantJoin = false;

void onEvent(void *, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        g_connected = false;
        if (g_wantJoin) esp_wifi_connect();   // keep trying
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto *e = static_cast<ip_event_got_ip_t *>(data);
        ESP_LOGI(TAG, "connected, address " IPSTR, IP2STR(&e->ip_info.ip));
        g_connected = true;
    }
}
} // namespace

esp_err_t start() {
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "init");
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, onEvent, nullptr);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, onEvent, nullptr);
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "storage");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "mode");
    return esp_wifi_start();
}

std::vector<std::string> scan() {
    std::vector<std::string> out;
    if (esp_wifi_scan_start(nullptr, true) != ESP_OK) return out;
    uint16_t n = 0;
    esp_wifi_scan_get_ap_num(&n);
    std::vector<wifi_ap_record_t> aps(n);
    esp_wifi_scan_get_ap_records(&n, aps.data());
    std::sort(aps.begin(), aps.begin() + n,
              [](const wifi_ap_record_t &a, const wifi_ap_record_t &b) { return a.rssi > b.rssi; });
    for (int i = 0; i < n; ++i) {
        const std::string ssid(reinterpret_cast<const char *>(aps[i].ssid));
        if (!ssid.empty() && std::find(out.begin(), out.end(), ssid) == out.end())
            out.push_back(ssid);
    }
    return out;
}

void join(const std::string &ssid, const std::string &password) {
    wifi_config_t cfg = {};
    // Both may fill their fields exactly, with no terminator.
    std::memcpy(cfg.sta.ssid, ssid.data(), std::min(ssid.size(), sizeof cfg.sta.ssid));
    std::memcpy(cfg.sta.password, password.data(), std::min(password.size(), sizeof cfg.sta.password));
    esp_wifi_disconnect();
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    g_wantJoin = true;
    esp_wifi_connect();
}

bool connected() { return g_connected; }

} // namespace wifi_link
