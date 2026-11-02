// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "settings_store.h"

#include "nvs.h"

#include <string>

namespace settings_store {

namespace {
const char *kNamespace = "siena";
}

Ui::Settings load() {
    Ui::Settings s;
    nvs_handle_t h;
    if (nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) return s;
    uint8_t v;
    if (nvs_get_u8(h, "theme", &v) == ESP_OK) s.theme = v % Screen::kThemeCount;
    if (nvs_get_u8(h, "smooth", &v) == ESP_OK) s.smooth = v;
    if (nvs_get_u8(h, "turbo", &v) == ESP_OK) s.turbo = v;
    nvs_close(h);
    return s;
}

void save(const Ui::Settings &s) {
    nvs_handle_t h;
    if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "theme", uint8_t(s.theme));
    nvs_set_u8(h, "smooth", s.smooth);
    nvs_set_u8(h, "turbo", s.turbo);
    nvs_commit(h);
    nvs_close(h);
}

namespace {
std::string getStr(nvs_handle_t h, const char *key) {
    size_t n = 0;
    std::string s;
    if (nvs_get_str(h, key, nullptr, &n) == ESP_OK && n > 0) {
        s.resize(n);
        nvs_get_str(h, key, &s[0], &n);
        s.resize(n - 1);   // the terminator
    }
    return s;
}
}

Ui::Account loadAccount() {
    Ui::Account a;
    nvs_handle_t h;
    if (nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) return a;
    a.wifiName = getStr(h, "wifi_name");
    a.wifiPassword = getStr(h, "wifi_pass");
    a.gmailAddress = getStr(h, "gmail_addr");
    a.gmailAppPassword = getStr(h, "gmail_pass");
    a.calendarUrl = getStr(h, "cal_url");
    nvs_close(h);
    return a;
}

void saveAccount(const Ui::Account &a) {
    nvs_handle_t h;
    if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, "wifi_name", a.wifiName.c_str());
    nvs_set_str(h, "wifi_pass", a.wifiPassword.c_str());
    nvs_set_str(h, "gmail_addr", a.gmailAddress.c_str());
    nvs_set_str(h, "gmail_pass", a.gmailAppPassword.c_str());
    nvs_set_str(h, "cal_url", a.calendarUrl.c_str());
    nvs_commit(h);
    nvs_close(h);
}

// Profile 1's list keeps the key it had before there were profiles.
std::string installedKey(int profile) {
    return profile == 1 ? "installed" : "installed" + std::to_string(profile);
}

std::string loadInstalled(int profile) {
    nvs_handle_t h;
    if (nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) return "";
    const std::string key = installedKey(profile);
    size_t n = 0;
    std::string s;
    if (nvs_get_str(h, key.c_str(), nullptr, &n) == ESP_OK && n > 0) {
        s.resize(n);
        nvs_get_str(h, key.c_str(), &s[0], &n);
        s.resize(n - 1);   // the terminator
    }
    nvs_close(h);
    return s;
}

void saveInstalled(int profile, const std::string &names) {
    nvs_handle_t h;
    if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, installedKey(profile).c_str(), names.c_str());
    nvs_commit(h);
    nvs_close(h);
}

int loadProfile() {
    nvs_handle_t h;
    uint8_t v = 1;
    if (nvs_open(kNamespace, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u8(h, "profile", &v);
        nvs_close(h);
    }
    return v;
}

void saveProfile(int profile) {
    nvs_handle_t h;
    if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "profile", uint8_t(profile));
    nvs_commit(h);
    nvs_close(h);
}

} // namespace settings_store
