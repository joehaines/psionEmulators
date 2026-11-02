// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Follows ESP-IDF's examples/bluetooth/esp_hid_host, cut down to the one
// job of finding and keeping a BLE keyboard.

#include "ble_keyboard.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_check.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_hidh.h"
#include "esp_hidh_gattc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "esp_heap_caps.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_timer.h"

#include <cinttypes>
#include <cstring>
#include <initializer_list>

namespace ble_keyboard {

namespace {

const char *TAG = "ble-kbd";

constexpr uint16_t kHidServiceUuid = 0x1812;
constexpr uint16_t kAppearanceHidGeneric  = 0x03C0;
constexpr uint16_t kAppearanceHidKeyboard = 0x03C1;
constexpr uint32_t kScanSeconds = 30;
constexpr int      kBondsKept = 4;      // of the stack's 15

struct Candidate { esp_bd_addr_t bda; esp_ble_addr_type_t addrType; };

Callbacks       g_cb;
QueueHandle_t   g_candidates;
esp_hidh_dev_t *g_dev = nullptr;
volatile bool   g_connected = false;
volatile bool   g_scanning = false;
volatile bool   g_candidatePending = false;
// The open in progress (managerTask blocks in it), for the watchdog.
volatile int64_t g_openingSinceUs = 0;   // 0: none
esp_bd_addr_t    g_openingBda;
int              g_nearbySeen = 0;       // per scan, for logNearby

bool isBonded(const esp_bd_addr_t bda) {
    int n = esp_ble_get_bond_device_num();
    if (n <= 0) return false;
    esp_ble_bond_dev_t list[8];
    if (n > 8) n = 8;
    if (esp_ble_get_bond_device_list(&n, list) != ESP_OK) return false;
    for (int i = 0; i < n; ++i)
        if (std::memcmp(list[i].bd_addr, bda, sizeof(esp_bd_addr_t)) == 0) return true;
    return false;
}

// Says it's a keyboard, or advertises the HID service without saying it's
// a mouse or gamepad. Some keyboards (the M7) only list the HID service
// while pairing: reconnecting, they advertise their appearance alone.
bool looksLikeKeyboard(uint8_t *adv, uint16_t len) {
    uint8_t l = 0;
    const uint8_t *a = esp_ble_resolve_adv_data_by_type(adv, len, ESP_BLE_AD_TYPE_APPEARANCE, &l);
    const int appearance = a && l >= 2 ? (a[0] | (a[1] << 8)) : -1;
    if (appearance == kAppearanceHidKeyboard) return true;
    bool hid = false;
    for (esp_ble_adv_data_type t : { ESP_BLE_AD_TYPE_16SRV_CMPL, ESP_BLE_AD_TYPE_16SRV_PART }) {
        const uint8_t *p = esp_ble_resolve_adv_data_by_type(adv, len, t, &l);
        for (int i = 0; p && i + 1 < l; i += 2)
            if ((p[i] | (p[i + 1] << 8)) == kHidServiceUuid) hid = true;
    }
    return hid && (appearance < 0 || appearance == kAppearanceHidGeneric);
}

// Keyboards that re-pair on a new address each time (the M7 does) leave
// their old bonds behind, and a full bond table stops new pairings. Once
// it's filling up, keep only the keyboard that just paired.
void pruneBonds(const esp_bd_addr_t keep) {
    int n = esp_ble_get_bond_device_num();
    if (n <= kBondsKept) return;
    esp_ble_bond_dev_t list[16];
    if (n > 16) n = 16;
    if (esp_ble_get_bond_device_list(&n, list) != ESP_OK) return;
    int removed = 0;
    for (int i = 0; i < n; ++i) {
        if (std::memcmp(list[i].bd_addr, keep, sizeof(esp_bd_addr_t)) == 0) continue;
        esp_ble_remove_bond_device(list[i].bd_addr);
        ++removed;
    }
    ESP_LOGI(TAG, "forgot %d old pairings", removed);
}

// Each device heard close by (a keyboard on the desk), once per scan, so a
// keyboard that won't reconnect shows what it is advertising.
void logNearby(const esp_ble_gap_cb_param_t::ble_scan_result_evt_param &r) {
    static uint8_t seen[16][6];
    if (r.rssi < -60) return;
    for (int i = 0; i < g_nearbySeen; ++i)
        if (std::memcmp(seen[i], r.bda, 6) == 0) return;
    if (g_nearbySeen < 16) std::memcpy(seen[g_nearbySeen++], r.bda, 6);
    uint8_t *adv = const_cast<uint8_t *>(r.ble_adv);
    const uint16_t len = r.adv_data_len + r.scan_rsp_len;
    uint8_t l = 0;
    char name[20] = "";
    if (const uint8_t *n = esp_ble_resolve_adv_data_by_type(adv, len, ESP_BLE_AD_TYPE_NAME_CMPL, &l)) {
        if (l > 19) l = 19;
        std::memcpy(name, n, l);
        name[l] = 0;
    }
    const uint8_t *a = esp_ble_resolve_adv_data_by_type(adv, len, ESP_BLE_AD_TYPE_APPEARANCE, &l);
    ESP_LOGI(TAG, "near: " ESP_BD_ADDR_STR " rssi %d adv type %d addr type %d%s appearance %04x%s '%s'",
             ESP_BD_ADDR_HEX(r.bda), r.rssi, r.ble_evt_type, r.ble_addr_type,
             isBonded(r.bda) ? " bonded" : "", a && l >= 2 ? (a[0] | (a[1] << 8)) : 0,
             looksLikeKeyboard(adv, len) ? " keyboard" : "", name);
}

// Every 5 s. A connection that hangs (the keyboard gone back to sleep
// half-way through the GATT discovery, say) would hold managerTask in
// esp_hidh_dev_open for good, and nothing would scan again: after 40 s
// drop the link, which ends the open; if even that doesn't, restart (the
// Siena resumes from the SD card). While no keyboard is connected, say
// what the stack is doing now and then.
void watchdog(TimerHandle_t) {
    static int ticks = 0;
    const int64_t since = g_openingSinceUs;
    if (since) {
        const int64_t s = (esp_timer_get_time() - since) / 1000000;
        if (s >= 40 && s < 45) {
            ESP_LOGW(TAG, "connecting for %llds: dropping the link", (long long)s);
            esp_ble_gap_disconnect(g_openingBda);
        } else if (s >= 120) {
            ESP_LOGE(TAG, "connecting for %llds: Bluetooth is stuck, restarting", (long long)s);
            esp_restart();
        }
    }
    if (!g_connected && ++ticks % 6 == 0)
        ESP_LOGI(TAG, "no keyboard: %s, %d bonded", since ? "connecting" :
                 g_scanning ? "scanning" : "idle", esp_ble_get_bond_device_num());
}

void startScan() {
    esp_ble_scan_params_t params = {};
    params.scan_type = BLE_SCAN_TYPE_ACTIVE;
    params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
    params.scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
    params.scan_interval = 0x50;
    params.scan_window = 0x30;
    params.scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE;
    g_scanning = true;
    esp_ble_gap_set_scan_params(&params);   // scanning starts on its completion
}

void gapEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        g_nearbySeen = 0;
        esp_ble_gap_start_scanning(kScanSeconds);
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        auto &r = param->scan_rst;
        if (r.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT) {
            g_scanning = false;
            break;
        }
        if (r.search_evt != ESP_GAP_SEARCH_INQ_RES_EVT || g_candidatePending || g_connected)
            break;
        logNearby(r);
        if (!isBonded(r.bda) && !looksLikeKeyboard(r.ble_adv, r.adv_data_len + r.scan_rsp_len))
            break;
        ESP_LOGI(TAG, "keyboard " ESP_BD_ADDR_STR " (rssi %d), connecting",
                 ESP_BD_ADDR_HEX(r.bda), r.rssi);
        Candidate c;
        std::memcpy(c.bda, r.bda, sizeof c.bda);
        c.addrType = r.ble_addr_type;
        g_candidatePending = true;
        esp_ble_gap_stop_scanning();
        xQueueSend(g_candidates, &c, 0);
        break;
    }
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        g_scanning = false;
        break;
    case ESP_GAP_BLE_SEC_REQ_EVT:
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;
    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
        // We display, the keyboard types: the usual keyboard pairing.
        ESP_LOGW(TAG, "pairing: type %06" PRIu32 " on the keyboard, then Enter",
                 param->ble_security.key_notif.passkey);
        if (g_cb.onPasskey) g_cb.onPasskey(int(param->ble_security.key_notif.passkey));
        break;
    case ESP_GAP_BLE_NC_REQ_EVT:
        esp_ble_confirm_reply(param->ble_security.key_notif.bd_addr, true);
        break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
        if (g_cb.onPasskey) g_cb.onPasskey(-1);
        if (param->ble_security.auth_cmpl.success) {
            ESP_LOGI(TAG, "paired");
            pruneBonds(param->ble_security.auth_cmpl.bd_addr);
        } else {
            ESP_LOGW(TAG, "pairing failed, reason 0x%x", param->ble_security.auth_cmpl.fail_reason);
        }
        break;
    default:
        break;
    }
}

void hidhEvent(void *, esp_event_base_t, int32_t id, void *data) {
    auto *p = static_cast<esp_hidh_event_data_t *>(data);
    switch (esp_hidh_event_t(id)) {
    case ESP_HIDH_OPEN_EVENT:
        if (p->open.status == ESP_OK) {
            g_dev = p->open.dev;
            g_connected = true;
            if (g_scanning) esp_ble_gap_stop_scanning();
            ESP_LOGI(TAG, "connected: %s", esp_hidh_dev_name_get(p->open.dev));
            if (g_cb.onConnection) g_cb.onConnection(true);
        }
        break;
    case ESP_HIDH_INPUT_EVENT:
        if (p->input.usage == ESP_HID_USAGE_KEYBOARD && g_cb.onReport)
            g_cb.onReport(p->input.data, p->input.length);
        break;
    case ESP_HIDH_CLOSE_EVENT:
        g_dev = nullptr;
        g_connected = false;
        ESP_LOGI(TAG, "disconnected (reason 0x%x)", p->close.reason);
        if (g_cb.onPasskey) g_cb.onPasskey(-1);
        if (g_cb.onConnection) g_cb.onConnection(false);
        break;
    default:
        break;
    }
}

// Scans while disconnected and opens whatever the scan turns up.
// esp_hidh_dev_open blocks until the GATT discovery is done, so it can't
// run in the GAP callback.
void managerTask(void *) {
    for (;;) {
        if (!g_connected && !g_scanning && !g_candidatePending) startScan();
        Candidate c;
        if (xQueueReceive(g_candidates, &c, pdMS_TO_TICKS(1000)) != pdTRUE) continue;
        // Count as connected as soon as the open succeeds: the open event
        // can arrive after we loop, and a scan started in that gap starves
        // the new link until the keyboard times out.
        if (!g_connected) {
            std::memcpy(g_openingBda, c.bda, sizeof g_openingBda);
            g_openingSinceUs = esp_timer_get_time();
            const bool opened = esp_hidh_dev_open(c.bda, ESP_HID_TRANSPORT_BLE, c.addrType);
            g_openingSinceUs = 0;
            if (opened) g_connected = true;
            else        ESP_LOGW(TAG, "open failed");
        }
        g_candidatePending = false;
    }
}

} // namespace

esp_err_t start(const Callbacks &callbacks) {
    g_cb = callbacks;
    g_candidates = xQueueCreate(2, sizeof(Candidate));

    esp_bt_controller_config_t bt = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_bt_controller_init(&bt), TAG, "controller init");
    ESP_RETURN_ON_ERROR(esp_bt_controller_enable(ESP_BT_MODE_BLE), TAG, "controller enable");
    ESP_RETURN_ON_ERROR(esp_bluedroid_init(), TAG, "bluedroid init");
    ESP_RETURN_ON_ERROR(esp_bluedroid_enable(), TAG, "bluedroid enable");
    ESP_RETURN_ON_ERROR(esp_ble_gap_register_callback(gapEvent), TAG, "gap cb");
    ESP_RETURN_ON_ERROR(esp_ble_gattc_register_callback(esp_hidh_gattc_event_handler), TAG, "gattc cb");

    // Bond, with MITM protection where the keyboard can do it. IO
    // capability "display only": the keyboard types the passkey we show.
    esp_ble_auth_req_t auth = ESP_LE_AUTH_REQ_SC_MITM_BOND;
    esp_ble_io_cap_t iocap = ESP_IO_CAP_OUT;
    uint8_t keys = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t keySize = 16;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth, sizeof auth);
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof iocap);
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &keys, sizeof keys);
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &keys, sizeof keys);
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &keySize, sizeof keySize);

    esp_hidh_config_t hidh = {};
    hidh.callback = hidhEvent;
    hidh.event_stack_size = 4096;
    ESP_RETURN_ON_ERROR(esp_hidh_init(&hidh), TAG, "hidh init");

    // Its stack in PSRAM: it never writes flash (the stack does, in its own
    // tasks), and internal RAM is short.
    xTaskCreatePinnedToCoreWithCaps(managerTask, "ble-kbd", 4096, nullptr, 3, nullptr, 0,
                                    MALLOC_CAP_SPIRAM);
    xTimerStart(xTimerCreate("ble-kbd-wd", pdMS_TO_TICKS(5000), pdTRUE, nullptr, watchdog), 0);
    return ESP_OK;
}

void forgetAll() {
    int n = esp_ble_get_bond_device_num();
    if (n > 0) {
        esp_ble_bond_dev_t list[8];
        if (n > 8) n = 8;
        if (esp_ble_get_bond_device_list(&n, list) == ESP_OK)
            for (int i = 0; i < n; ++i) esp_ble_remove_bond_device(list[i].bd_addr);
    }
    if (g_dev) esp_hidh_dev_close(g_dev);
    ESP_LOGW(TAG, "forgot %d bonded keyboard(s)", n > 0 ? n : 0);
}

} // namespace ble_keyboard
