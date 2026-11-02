// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Psion Siena on a Guition JC3248W535.
//
//   core 1  the emulator: V30H + ASIC9, paced to wall-clock time, plus the
//           keyboard player and the speaker feed
//   core 0  the panel (whole frames, only when the Siena's LCD changed),
//           the Bluetooth keyboard, the USB terminal and the BOOT button
//
// Everything that touches the emulator runs on its own task; the others
// hand it input through a queue and take finished LCD frames back.

#include "app_store.h"
#include "audio_out.h"
#include "ble_keyboard.h"
#include "board.h"
#include "hid_keymap.h"
#include "key_player.h"
#include "lcd_axs15231b.h"
#include "profiler.h"
#include "ram_store.h"
#include "screen.h"
#include "settings_store.h"
#include "sync_service.h"
#include "siena_machine.h"
#include "terminal_keys.h"
#include "ui.h"
#include "wifi_link.h"

#include "emubase.h"
#include "v30.h"

#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_rom_crc.h"
#include "esp_cpu.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "nvs_flash.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <string>
#include <new>
#include <utility>
#include <vector>

namespace {

const char *TAG = "siena";

constexpr int     kLcdW = SienaMachine::kLcdW, kLcdH = SienaMachine::kLcdH;
constexpr int64_t kSliceMs = 10;       // emulation granularity
constexpr int64_t kFrameMs = 33;       // LCD sampled at ~30 Hz
constexpr int64_t kMaxLagMs = 250;     // beyond this, drop time rather than race
constexpr int     kForgetHoldMs = 3000;
// The RAM goes to the SD card once typing has paused this long (files only
// change when the user does something), or this long into steady typing.
constexpr int64_t kSaveIdleMs = 3000;
constexpr int64_t kSaveMaxDelayMs = 30000;
// Turbo lets a busy Siena run up to this far ahead of real time; it waits
// for the clock to catch up once it's idle again.
constexpr int64_t kTurboAheadMs = 2000;
// Room for this many store entries from the start: allocations under 4 KiB
// go to internal RAM, which Bluetooth needs; this size goes to PSRAM.
constexpr size_t  kStoreReserve = 128;

// Input for the emulator task.
struct Input {
    enum Kind : uint8_t { HidReport, TerminalByte, ReleaseAll } kind;
    uint8_t len;
    uint8_t data[8];
};

SienaMachine     *g_machine;
Lcd               g_lcd;
Screen            g_screen;
QueueHandle_t     g_input;
TaskHandle_t      g_displayTask;
SemaphoreHandle_t g_frameLock;
uint8_t          *g_frame;             // latest LCD levels, under g_frameLock
std::atomic<int>  g_passkey{ -1 };
unsigned          g_lcdUpdates = 0;     // Siena LCD changes seen (emulator task)
bool              g_resumed = false;    // carrying on from the SD card's state
Ui::Settings      g_settings;           // as loaded; then the display task's copy
// From the emulator task to the display task, under g_frameLock.
Overlay           g_overlay;
Ui::Settings      g_newSettings;
// The app store (a pack in flash, in the Siena's slot), and the names of
// the apps installed from it, newline separated.
std::vector<app_store::Entry> g_store;
const uint8_t    *g_pack;               // its pack, mapped from flash
std::string       g_installed;          // as loaded; then the display task's copy
int               g_installedProfile;   // whose list that is
std::string       g_newInstalled;       // under g_frameLock, with:
int               g_newInstalledProfile;

// What a Siena is built from, for building another (a profile switch).
const uint8_t    *g_rom;                // mapped from flash, for good
void             *g_emuRam;             // where the emulator object lives
bool              g_audio = false;
Ui::Account       g_account;            // as loaded at start
bool              g_wifi = false;       // started
// Calendar events from the sync task for the emulator task to type in.
SemaphoreHandle_t g_eventsLock;
std::vector<calendar_sync::Event> g_events;
std::string       g_syncStatus;         // the last sync's, under g_eventsLock

// A fresh Siena, as at power-on: the ROM loaded, the app store's pack in
// its slot (so its boot finds drive A:), sound on.
void buildMachine() {
    delete g_machine;
    g_machine = new SienaMachine(g_emuRam);
    g_machine->loadRom(g_rom, 0x100000, CONFIG_SIENA_LANGUAGE);
    if (g_pack) g_machine->attachPack(g_pack, 0x800000);
    if (g_audio) g_machine->emu().setHostAudioEnabled(true, false);
}

// Before its first instruction, the current profile's Siena from the SD
// card: with the machine state too it carries on where it was; with the
// RAM alone it warm-boots, its M: drive as it was. True if it resumed.
bool restoreMachine() {
    if (!ram_store::available()) return false;
    std::vector<uint8_t> state, packWrites;
    if (!ram_store::load(g_machine->emu().getRamBuffer(), state, packWrites)) return false;
    bool resumed = false;
    if (!state.empty()) {
        resumed = g_machine->loadState(state.data(), state.size());
        if (!resumed) ESP_LOGE(TAG, "saved machine state doesn't fit this build");
    }
    if (!resumed && !packWrites.empty() &&
        !g_machine->loadPackWrites(packWrites.data(), packWrites.size()))
        ESP_LOGE(TAG, "saved writes to the app pack don't fit it");
    return resumed;
}

void post(const Input &in) { xQueueSend(g_input, &in, 0); }

// Built in PSRAM: internal RAM, where the task's stack is, is short.
template <class T, class... Args> T &inPsram(Args &&...args) {
    return *new (heap_caps_malloc(sizeof(T), MALLOC_CAP_SPIRAM)) T(std::forward<Args>(args)...);
}

void emulatorTask(void *) {
    KeyPlayer &player = inPsram<KeyPlayer>();
    HidKeymap &hid = inPsram<HidKeymap>(player);
    TerminalKeys &terminal = inPsram<TerminalKeys>(player);
    Ui &ui = inPsram<Ui>();
    ui.setSettings(g_settings);
    int64_t nowWall = 0;
    hid.filter = [&](int key, uint8_t mods) { return ui.onKey(key, mods, nowWall); };
    terminal.filter = hid.filter;
    ui.onCaptureChanged = [&](bool capturing) { hid.setCaptured(capturing); };
    {
        std::vector<Ui::App> apps;
        apps.reserve(kStoreReserve);
        for (const app_store::Entry &e : g_store) {
            apps.push_back(e.app);
            const std::string name = "\n" + std::string(e.app.name) + "\n";
            apps.back().installed = ("\n" + g_installed + "\n").find(name) != std::string::npos;
        }
        ui.setApps(std::move(apps));
    }
    ui.onInstall = [&](int app) {
        ESP_LOGI(TAG, "app store: installing %.*s", int(g_store[app].program.size()),
                 g_store[app].program.data());
        app_store::queueInstall(player, g_store[app].program);
    };
    ui.onAppsChanged = [&] {
        std::string names;
        for (const Ui::App &a : ui.apps())
            if (a.installed) names += std::string(a.name) + "\n";
        xSemaphoreTake(g_frameLock, portMAX_DELAY);
        g_newInstalled = names;
        g_newInstalledProfile = ram_store::profile();
        xSemaphoreGive(g_frameLock);
    };
    // Profile changes happen between slices, not inside the keyboard code
    // that asked for them.
    int pendingProfile = -1;
    if (ram_store::available()) ui.setProfiles(ram_store::profiles(), ram_store::profile());
    ui.onSwitchProfile = [&](int profile) { pendingProfile = profile; };
    ui.onNewProfile = [&] {
        const int made = ram_store::createProfile();
        if (made > 0) pendingProfile = made;
        return made;
    };
    ui.onDeleteProfile = [&](int profile) { return ram_store::deleteProfile(profile); };
    ui.setAccount(g_account);
    hid.textFilter = [&](char ch) { return ui.onChar(ch, nowWall); };
    terminal.textFilter = hid.textFilter;
    ui.onAccountChanged = [&](const Ui::Account &a) {
        const bool rejoin = a.wifiName != g_account.wifiName || a.wifiPassword != g_account.wifiPassword;
        g_account = a;
        settings_store::saveAccount(a);
        if (rejoin && g_wifi && !a.wifiName.empty()) wifi_link::join(a.wifiName, a.wifiPassword);
        sync_service::setCalendarUrl(a.calendarUrl);
    };
    ui.onSyncNow = [] { sync_service::syncNow(); };
    int typingEvents = 0;     // calendar events being typed into Agenda
    std::string statusShown;
    int64_t nextStatusMs = 0;
    unsigned overlayShown = ui.overlayVersion();
    uint8_t *levels = static_cast<uint8_t *>(malloc(kLcdW * kLcdH));
    int16_t samples[256];

    // Wall time starts where the Siena is: a resumed machine is well past 0.
    int64_t startMs = g_machine->nowMs();
    int64_t t0 = esp_timer_get_time() - startMs * 1000;
    int64_t lagMs = 0;            // wall time given up while running slow
    int64_t nextFrameMs = 0, statsAtMs = startMs, statsEmuMs = startMs;
    // A Siena that didn't resume saves itself once started, so even after
    // new firmware the next power-off resumes rather than boots.
    int64_t lastInputMs = startMs, unsavedSinceMs = g_resumed ? -1 : startMs;   // wall time
    unsigned reports = 0;
    std::vector<uint8_t> state, packWrites;
    // Sound since the last progress line: samples, those the I2S queue
    // had no room for, and the loudest.
    unsigned soundSamples = 0, soundDropped = 0;
    int soundPeak = 0;
    // Turbo keeps to real time while sound plays: anything longer than a
    // key click, which would otherwise outrun the speaker.
    int64_t soundUntilMs = 0, soundForMs = 0;
    const int soundRate = g_machine->emu().getAudioSampleRate();
    unsigned slices = 0, haltedSlices = 0;
    // What a busy slice costs: host CPU cycles per Siena cycle. Under 31.25
    // (240 MHz / 7.68 MHz) is real time; turbo's headroom is 31.25 / this.
    uint64_t busyHostCycles = 0, busySienaCycles = 0;
    int64_t maxAheadMs = 0;       // turbo's lead on real time, at most
    for (;;) {
        const int64_t wallMs = (esp_timer_get_time() - t0) / 1000 - lagMs;
        nowWall = wallMs + lagMs;
        int64_t emuMs = g_machine->nowMs();
        const bool busy = !g_machine->emu().getV30Cpu()->halted;
        const bool ahead = ui.settings().turbo && busy && nowWall >= soundUntilMs;
        if (emuMs >= wallMs + (ahead ? kTurboAheadMs : 0)) {
            vTaskDelay(1);
            continue;
        }
        if (wallMs - emuMs > kMaxLagMs) lagMs += wallMs - emuMs - kMaxLagMs;

        Input in;
        while (xQueueReceive(g_input, &in, 0) == pdTRUE) {
            lastInputMs = wallMs + lagMs;
            if (unsavedSinceMs < 0) unsavedSinceMs = lastInputMs;
            switch (in.kind) {
            case Input::HidReport:    hid.onReport(in.data, in.len); break;
            case Input::TerminalByte: terminal.onByte(in.data[0], emuMs); break;
            case Input::ReleaseAll:   hid.releaseAll(); break;
            }
        }
        if (pendingProfile > 0) {
            // Keep the Siena being left as it is now, then bring up the
            // other, and start the clocks again from where it is.
            const int profile = pendingProfile;
            pendingProfile = -1;
            ram_store::flush();
            state.clear();
            g_machine->saveState(state);
            packWrites.clear();
            if (g_pack) g_machine->savePackWrites(packWrites);
            ram_store::save(g_machine->emu().getRamBuffer(), state, packWrites);
            ram_store::flush();
            ram_store::setProfile(profile);
            settings_store::saveProfile(profile);
            hid.releaseAll();
            player = KeyPlayer();
            buildMachine();
            const bool resumed = restoreMachine();
            ESP_LOGI(TAG, "profile %d: %s", profile, resumed ? "resumed" : "starting");
            const std::string installed = settings_store::loadInstalled(profile);
            xSemaphoreTake(g_frameLock, portMAX_DELAY);
            g_newInstalled = installed;
            g_newInstalledProfile = profile;
            g_installed = installed;
            g_installedProfile = profile;
            xSemaphoreGive(g_frameLock);
            ui.setInstalled(installed);
            ui.setProfiles(ram_store::profiles(), profile);
            startMs = g_machine->nowMs();
            t0 = esp_timer_get_time() - startMs * 1000;
            lagMs = 0;
            nextFrameMs = 0;
            statsAtMs = statsEmuMs = lastInputMs = nowWall = startMs;
            unsavedSinceMs = resumed ? -1 : startMs;
            ui.notify("Profile " + std::to_string(profile), nowWall);
            continue;
        }
        terminal.poll(emuMs);
        player.update(g_machine->emu(), emuMs);
        // Calendar events: typed in one go, the keyboard held meanwhile.
        if (typingEvents && player.idle()) {
            ui.endBusy(std::to_string(typingEvents) + " events added to Agenda", nowWall);
            unsavedSinceMs = unsavedSinceMs < 0 ? nowWall : unsavedSinceMs;
            typingEvents = 0;
        } else if (!typingEvents && !ui.capturing() && player.idle()) {
            std::vector<calendar_sync::Event> events;
            xSemaphoreTake(g_eventsLock, portMAX_DELAY);
            events.swap(g_events);
            xSemaphoreGive(g_eventsLock);
            if (!events.empty()) {
                typingEvents = int(events.size());
                ui.beginBusy("Adding " + std::to_string(typingEvents) + " events to Agenda...", nowWall);
                for (const calendar_sync::Event &e : events) calendar_sync::queueAgendaEntry(player, e);
            }
        }
        if (ui.installing() && player.idle()) {
            ui.installDone(nowWall);
            ESP_LOGI(TAG, "app store: install typed");
        }
        ui.poll(nowWall);
        if (nowWall >= nextStatusMs) {
            nextStatusMs = nowWall + 500;
            xSemaphoreTake(g_eventsLock, portMAX_DELAY);
            const std::string sync = g_syncStatus;
            xSemaphoreGive(g_eventsLock);
            const std::string status =
                (!g_wifi ? "Wi-Fi didn't start"
                 : g_account.wifiName.empty() ? "Wi-Fi: no network set"
                 : wifi_link::connected() ? "Wi-Fi: connected to " + g_account.wifiName
                                          : "Wi-Fi: joining " + g_account.wifiName + "...") +
                (sync.empty() ? "" : "\n" + sync);
            if (status != statusShown) ui.setStatus(statusShown = status);
        }
        if (ui.overlayVersion() != overlayShown) {
            overlayShown = ui.overlayVersion();
            xSemaphoreTake(g_frameLock, portMAX_DELAY);
            g_overlay = ui.overlay();
            g_newSettings = ui.settings();
            xSemaphoreGive(g_frameLock);
            xTaskNotifyGive(g_displayTask);
        }

        const uint32_t hostBefore = esp_cpu_get_cycle_count();
        const int64_t sienaBefore = g_machine->emu().currentCycles();
        g_machine->runUntilMs(emuMs + kSliceMs);
        if (busy && !g_machine->emu().getV30Cpu()->halted) {
            busyHostCycles += uint32_t(esp_cpu_get_cycle_count() - hostBefore);
            busySienaCycles += uint64_t(g_machine->emu().currentCycles() - sienaBefore);
        }
        emuMs = g_machine->nowMs();
        ++slices;
        haltedSlices += g_machine->emu().getV30Cpu()->halted;
        maxAheadMs = std::max(maxAheadMs, emuMs - wallMs);

        size_t n;
        while ((n = g_machine->emu().readAudioOutput(samples, std::size(samples))) > 0) {
            soundDropped += n - audio_out::write(samples, n);
            soundSamples += n;
            int peak = 0;
            for (size_t i = 0; i < n; ++i) peak = std::max(peak, std::abs(int(samples[i])));
            soundForMs = peak ? soundForMs + int64_t(n) * 1000 / soundRate : 0;
            if (soundForMs >= 200) soundUntilMs = nowWall + 500;
            soundPeak = std::max(soundPeak, peak);
        }

        if (emuMs >= nextFrameMs) {
            nextFrameMs = emuMs + kFrameMs;
            if (g_machine->readLcd(levels)) {
                xSemaphoreTake(g_frameLock, portMAX_DELAY);
                std::memcpy(g_frame, levels, kLcdW * kLcdH);
                xSemaphoreGive(g_frameLock);
                ++g_lcdUpdates;
                xTaskNotifyGive(g_displayTask);
            }
        }

        // Save once input has paused and every key is played out, at a
        // moment the CPU is halted (as it nearly always is when idle).
        nowWall = (esp_timer_get_time() - t0) / 1000;
        if (unsavedSinceMs >= 0 && player.idle() && g_machine->emu().getV30Cpu()->halted &&
            (nowWall - lastInputMs >= kSaveIdleMs ||
             (nowWall - unsavedSinceMs >= kSaveMaxDelayMs && nowWall - lastInputMs >= 1000))) {
            state.clear();
            g_machine->saveState(state);
            packWrites.clear();
            if (!g_store.empty()) g_machine->savePackWrites(packWrites);
            if (ram_store::save(g_machine->emu().getRamBuffer(), state, packWrites))
                unsavedSinceMs = -1;
        }

        // Progress every 5 s of real time: emulated time, and how much of it
        // ran, so a slow machine and a stuck one look different.
        if (nowWall - statsAtMs >= 5000) {
            ESP_LOGI(TAG, "emulated %.1f s so far; speed %.0f%% of a real Siena; %u LCD updates; "
                     "idle at %u%% of slice ends",
                     emuMs / 1000.0,
                     100.0 * double(emuMs - statsEmuMs) / double(nowWall - statsAtMs),
                     unsigned(g_lcdUpdates), slices ? 100 * haltedSlices / slices : 0);
            if (maxAheadMs > kSliceMs) ESP_LOGI(TAG, "turbo: up to %lld ms ahead", (long long)maxAheadMs);
            if (busySienaCycles > 1000000)
                ESP_LOGI(TAG, "busy: %.1f host cycles per Siena cycle",
                         double(busyHostCycles) / double(busySienaCycles));
            busyHostCycles = busySienaCycles = 0;
            slices = haltedSlices = 0;
            maxAheadMs = 0;
            if (soundPeak)
                ESP_LOGI(TAG, "sound: %u samples, %u dropped, peak %d", soundSamples,
                         soundDropped, soundPeak);
            soundSamples = soundDropped = 0;
            soundPeak = 0;
            statsEmuMs = emuMs;
            statsAtMs = nowWall;
            if (++reports % 2 == 0) profiler::report();
        }
    }
}

void displayTask(void *) {
    // PSRAM, like the other frame copies: internal RAM is short once the
    // Bluetooth stack is up, and drawing is bound by the panel's bus.
    uint8_t *levels = static_cast<uint8_t *>(calloc(kLcdW * kLcdH, 1));
    int shownPasskey = -2;
    bool backlightOn = false;
    unsigned framesDrawn = 0;
    for (;;) {
        // Redraw when the Siena's LCD changes, or the passkey does.
        const bool newFrame = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)) > 0;
        const int passkey = g_passkey.load();
        if (!newFrame && passkey == shownPasskey) continue;
        shownPasskey = passkey;
        g_screen.setPasskey(passkey);
        xSemaphoreTake(g_frameLock, portMAX_DELAY);
        std::memcpy(levels, g_frame, kLcdW * kLcdH);
        g_screen.setOverlay(g_overlay);
        const Ui::Settings s = g_newSettings;
        const std::string installed = g_newInstalled;
        const int installedProfile = g_newInstalledProfile;
        if (installed != g_installed || installedProfile != g_installedProfile) {
            g_installed = installed;
            g_installedProfile = installedProfile;
            xSemaphoreGive(g_frameLock);
            settings_store::saveInstalled(installedProfile, installed);
        } else {
            xSemaphoreGive(g_frameLock);
        }
        g_screen.setTheme(s.theme);
        g_screen.setSmooth(s.smooth);
        // Kept from here, off the emulator's core: NVS writes stall flash.
        if (s.theme != g_settings.theme || s.smooth != g_settings.smooth ||
            s.turbo != g_settings.turbo) {
            g_settings = s;
            settings_store::save(s);
        }
        const int64_t t = esp_timer_get_time();
        g_lcd.drawFrame([&](int row, int rows, uint16_t *out) {
            g_screen.renderRows(levels, row, rows, out);
        });
        if (++framesDrawn <= 2)
            ESP_LOGI(TAG, "frame %u drawn in %lld ms", framesDrawn,
                     (long long)((esp_timer_get_time() - t) / 1000));
        if (!backlightOn) {
            g_lcd.setBacklight(100);
            backlightOn = true;
        }
    }
}

// Keys typed in a serial terminal on the laptop, and the BOOT button.
void usbTask(void *) {
    gpio_config_t button = {};
    button.pin_bit_mask = 1ULL << board::kBootButton;
    button.mode = GPIO_MODE_INPUT;
    button.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&button);
    int heldMs = 0;
    uint8_t buf[32];
    for (;;) {
        const int n = usb_serial_jtag_read_bytes(buf, sizeof buf, pdMS_TO_TICKS(20));
        for (int i = 0; i < n; ++i) post({ Input::TerminalByte, 1, { buf[i] } });
        // Hold BOOT for 3 s to forget paired keyboards and pair a new one.
        if (gpio_get_level(board::kBootButton) == 0) {
            heldMs += 20;
            if (heldMs == kForgetHoldMs) ble_keyboard::forgetAll();
        } else {
            heldMs = 0;
        }
    }
}

const uint8_t *mapRom(esp_partition_mmap_handle_t &handle) {
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, esp_partition_subtype_t(0x40), "siena_rom");
    if (!part) return nullptr;
    const void *ptr = nullptr;
    if (esp_partition_mmap(part, 0, part->size, ESP_PARTITION_MMAP_DATA, &ptr, &handle) != ESP_OK)
        return nullptr;
    return static_cast<const uint8_t *>(ptr);
}

// Map the app store: its pack (g_pack) and list (g_store). Returns the
// pack's id, or 0 with no store flashed. The mapping stays for good: the
// Siena reads the pack from it.
uint32_t mapStore() {
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, esp_partition_subtype_t(0x41), "siena_apps");
    constexpr size_t kPack = 0x800000, kIndex = 0x10000;
    if (!part || part->size < kPack + kIndex) return 0;
    const void *ptr = nullptr;
    esp_partition_mmap_handle_t handle;
    if (esp_partition_mmap(part, 0, kPack + kIndex, ESP_PARTITION_MMAP_DATA, &ptr, &handle) != ESP_OK) {
        ESP_LOGW(TAG, "couldn't map the app store");
        return 0;
    }
    const uint8_t *store = static_cast<const uint8_t *>(ptr);
    uint32_t packId = 0;
    const std::vector<app_store::Entry> entries =
        app_store::parseIndex(reinterpret_cast<const char *>(store + kPack), kIndex, packId);
    if (entries.empty()) {
        esp_partition_munmap(handle);
        ESP_LOGW(TAG, "no app store flashed");
        return 0;
    }
    g_store.reserve(kStoreReserve);
    g_store.assign(entries.begin(), entries.end());
    g_pack = store;
    ESP_LOGI(TAG, "app store: %u apps on drive A:", unsigned(g_store.size()));
    return packId;
}

} // namespace

extern "C" void app_main() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    usb_serial_jtag_driver_config_t usb = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb));
    usb_serial_jtag_vfs_use_driver();

    ESP_ERROR_CHECK(g_lcd.init());
#if CONFIG_SIENA_SCREEN_FLIPPED
    g_screen.setFlipped(true);
#endif

    esp_partition_mmap_handle_t romHandle;
    const uint8_t *rom = g_rom = mapRom(romHandle);
    if (!rom || rom[0xFFFF0] != 0xEA) {
        // No ROM flashed (or not a SIBO image: the reset vector at
        // FFFF:0000 is a far JMP).
        ESP_LOGE(TAG, "no Siena ROM in the siena_rom partition - flash one with idf.py flash");
        return;
    }
    g_emuRam = heap_caps_malloc(SienaMachine::emulatorSize(), MALLOC_CAP_INTERNAL);
    // The saved RAM is only good for the same ROM in the same language.
    const uint32_t romId = esp_rom_crc32_le(CONFIG_SIENA_LANGUAGE, rom, 0x100000);
    const uint32_t packId = mapStore();
    buildMachine();
    if (g_pack) ESP_LOGI(TAG, "app store: %u apps on drive A:", unsigned(g_store.size()));
    // The profile in use last time, as it was.
    const int profile = settings_store::loadProfile();
    ram_store::start(romId, g_machine->emu().getRamSize(), packId, profile);
    g_resumed = restoreMachine();
    ESP_LOGI(TAG, "ROM loaded, %u KiB internal / %u KiB PSRAM free",
             unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));

    if (audio_out::init(g_machine->emu().getAudioSampleRate()) == ESP_OK) {
        g_audio = true;
        g_machine->emu().setHostAudioEnabled(true, false);
    } else
        ESP_LOGW(TAG, "no I2S audio");

    g_settings = g_newSettings = settings_store::load();
    g_account = settings_store::loadAccount();
    g_installedProfile = g_newInstalledProfile = ram_store::profile();
    g_installed = g_newInstalled = settings_store::loadInstalled(g_installedProfile);
    g_screen.setTheme(g_settings.theme);
    g_screen.setSmooth(g_settings.smooth);
    g_input = xQueueCreate(64, sizeof(Input));
    g_frameLock = xSemaphoreCreateMutex();
    g_eventsLock = xSemaphoreCreateMutex();
    g_frame = static_cast<uint8_t *>(calloc(kLcdW * kLcdH, 1));

    xTaskCreatePinnedToCore(displayTask, "display", 4096, nullptr, 4, &g_displayTask, 0);
    TaskHandle_t emuTask;
    xTaskCreatePinnedToCore(emulatorTask, "siena", 8192, nullptr, 5, &emuTask, 1);
    profiler::start(emuTask, 1);
    xTaskCreatePinnedToCoreWithCaps(usbTask, "usb-keys", 3072, nullptr, 3, nullptr, 0,
                                    MALLOC_CAP_SPIRAM);

    ble_keyboard::Callbacks cb = {};
    cb.onReport = [](const uint8_t *r, size_t len) {
        Input in = { Input::HidReport, uint8_t(std::min<size_t>(len, 8)), {} };
        std::memcpy(in.data, r, in.len);
        post(in);
    };
    cb.onPasskey = [](int passkey) {
        g_passkey = passkey;
        xTaskNotifyGive(g_displayTask);
    };
    cb.onConnection = [](bool connected) {
        if (!connected) post({ Input::ReleaseAll, 0, {} });
    };
    // Wi-Fi first: started after Bluetooth it may not find the internal
    // RAM it needs in one piece. It joins a network once one is set.
    // UK time, which the calendar's times are put in (and the Siena's
    // locale is English UK).
    setenv("TZ", "GMT0BST,M3.5.0/1,M10.5.0", 1);
    tzset();
    g_wifi = wifi_link::start() == ESP_OK;
    if (!g_wifi) ESP_LOGE(TAG, "Wi-Fi failed to start");
    else if (!g_account.wifiName.empty()) wifi_link::join(g_account.wifiName, g_account.wifiPassword);
    if (g_wifi) {
        sync_service::setCalendarUrl(g_account.calendarUrl);
        sync_service::Hooks hooks;
        hooks.deliver = [](std::vector<calendar_sync::Event> events) {
            xSemaphoreTake(g_eventsLock, portMAX_DELAY);
            for (calendar_sync::Event &e : events) g_events.push_back(std::move(e));
            xSemaphoreGive(g_eventsLock);
        };
        hooks.status = [](const std::string &s) {
            xSemaphoreTake(g_eventsLock, portMAX_DELAY);
            g_syncStatus = s;
            xSemaphoreGive(g_eventsLock);
        };
        sync_service::start(hooks);
    }
    if (ble_keyboard::start(cb) != ESP_OK) ESP_LOGE(TAG, "Bluetooth failed to start");
    // The Bluetooth stack takes internal RAM, and so does every function
    // placed in IRAM: watch this when moving more code there.
    ESP_LOGI(TAG, "started, %u KiB internal RAM free",
             unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
}
