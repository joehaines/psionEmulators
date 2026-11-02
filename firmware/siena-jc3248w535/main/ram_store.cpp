// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "ram_store.h"

#include "board.h"

#include "driver/sdmmc_host.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_rom_crc.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "sdmmc_cmd.h"

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <sys/stat.h>
#include <unistd.h>

namespace ram_store {

namespace {

const char *TAG = "ram-store";
const char *kMount = "/sd";
const char *kDir = "/sd/SIENA";
constexpr int kMaxProfiles = 9;

// The current profile's slots: profile 1 in SIENA/ itself (where they were
// before there were profiles), profile n in SIENA/P<n>/.
int  g_profile = 1;
char kSlots[2][32];

void profileDir(int profile, char *out, size_t n) {
    if (profile == 1) std::snprintf(out, n, "%s", kDir);
    else              std::snprintf(out, n, "%s/P%d", kDir, profile);
}

bool profileExists(int profile) {
    char dir[32];
    profileDir(profile, dir, sizeof dir);
    struct stat st;
    return stat(dir, &st) == 0 && S_ISDIR(st.st_mode);
}
constexpr uint32_t kMagic = 0x4D415253;   // "SRAM"

// Version 1 slots hold the RAM alone; version 2 adds the machine state,
// tagged with the firmware build that wrote it; version 3 adds what the
// Siena wrote to the app store's pack, tagged with that pack. The RAM
// follows the header, then the state, then the pack's writes; `crc`
// covers them all.
struct Header {
    uint32_t magic, version, seq, size, romId, crc;   // version 1 ends here
    uint32_t stateSize;
    uint8_t  build[8];                                 // app ELF SHA-256, leading bytes
    uint32_t packWritesSize, packId;                   // version 3
};
constexpr size_t kHeaderV1 = offsetof(Header, stateSize);
constexpr size_t kHeaderV2 = offsetof(Header, packWritesSize);

size_t headerBytes(const Header &h) {
    return h.version == 1 ? kHeaderV1 : h.version == 2 ? kHeaderV2 : sizeof(Header);
}

uint32_t g_romId, g_packId;
size_t   g_size;
uint8_t  g_build[8];
uint32_t g_seq = 0;          // newest slot's sequence number
int      g_next = 0;         // the slot the next save overwrites
uint8_t *g_shadow;           // the RAM being written, in PSRAM
std::vector<uint8_t> g_stateShadow, g_packShadow;
TaskHandle_t g_writer;
std::atomic<bool> g_busy{ false };

uint32_t crcOf(const uint8_t *ram, size_t n, const std::vector<uint8_t> &state,
               const std::vector<uint8_t> &pack) {
    uint32_t crc = esp_rom_crc32_le(esp_rom_crc32_le(0, ram, n), state.data(), state.size());
    return esp_rom_crc32_le(crc, pack.data(), pack.size());
}

// A slot's header, if it is complete and belongs to this machine. The
// payload's CRC is checked only for the slot actually restored.
bool readHeader(int slot, Header &h) {
    FILE *f = std::fopen(kSlots[slot], "rb");
    if (!f) return false;
    h = {};
    bool ok = std::fread(&h, kHeaderV1, 1, f) == 1 && h.magic == kMagic &&
              h.version >= 1 && h.version <= 3;
    if (ok && h.version >= 2)
        ok = std::fread(reinterpret_cast<uint8_t *>(&h) + kHeaderV1, headerBytes(h) - kHeaderV1, 1, f) == 1;
    std::fclose(f);
    return ok && h.size == g_size && h.romId == g_romId;
}

bool writeSlot(int slot, uint32_t seq) {
    // Overwrite in place: the payload goes first under a blank header, and
    // the real header only once the payload is on the card.
    FILE *f = std::fopen(kSlots[slot], "r+b");
    if (!f) f = std::fopen(kSlots[slot], "w+b");
    if (!f) return false;
    const size_t n = g_stateShadow.size(), p = g_packShadow.size();
    Header h = {};
    bool ok = std::fwrite(&h, sizeof h, 1, f) == 1 &&
              std::fwrite(g_shadow, 1, g_size, f) == g_size &&
              std::fwrite(g_stateShadow.data(), 1, n, f) == n &&
              std::fwrite(g_packShadow.data(), 1, p, f) == p &&
              std::fflush(f) == 0 && fsync(fileno(f)) == 0;
    if (ok) {
        h = { kMagic, 3, seq, uint32_t(g_size), g_romId,
              crcOf(g_shadow, g_size, g_stateShadow, g_packShadow), uint32_t(n), {},
              uint32_t(p), g_packId };
        std::memcpy(h.build, g_build, sizeof h.build);
        ok = std::fseek(f, 0, SEEK_SET) == 0 && std::fwrite(&h, sizeof h, 1, f) == 1 &&
             std::fflush(f) == 0 && fsync(fileno(f)) == 0;
    }
    return std::fclose(f) == 0 && ok;
}

void writerTask(void *) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const int64_t t = esp_timer_get_time();
        const uint32_t seq = g_seq + 1;
        if (writeSlot(g_next, seq)) {
            ESP_LOGI(TAG, "saved #%lu to %s in %lld ms", (unsigned long)seq, kSlots[g_next],
                     (long long)((esp_timer_get_time() - t) / 1000));
            g_seq = seq;
            g_next ^= 1;
        } else {
            ESP_LOGW(TAG, "saving to %s failed", kSlots[g_next]);
        }
        g_busy = false;
    }
}

} // namespace

bool start(uint32_t romId, size_t ramSize, uint32_t packId, int profile) {
    g_profile = profile;
    g_romId = romId;
    g_packId = packId;
    g_size = ramSize;

    // 1-bit SDMMC, as wired on the board (and as the vendor demo drives it).
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 1;
    slot.clk = board::kSdClk;
    slot.cmd = board::kSdCmd;
    slot.d0 = board::kSdD0;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    esp_vfs_fat_sdmmc_mount_config_t mount = {};
    mount.format_if_mount_failed = false;   // never wipe the user's card
    mount.max_files = 1;
    sdmmc_card_t *card;
    const esp_err_t err = esp_vfs_fat_sdmmc_mount(kMount, &host, &slot, &mount, &card);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "no usable SD card (%s): the Siena's RAM won't be kept",
                 esp_err_to_name(err));
        return false;
    }
    mkdir(kDir, 0777);
    std::memcpy(g_build, esp_app_get_description()->app_elf_sha256, sizeof g_build);

    g_shadow = static_cast<uint8_t *>(heap_caps_malloc(g_size, MALLOC_CAP_SPIRAM));
    if (!g_shadow) return false;
    if (!profileExists(g_profile)) g_profile = 1;   // deleted from the card, say
    setProfile(g_profile);
    // Its stack in PSRAM: it writes the card, never flash.
    xTaskCreatePinnedToCoreWithCaps(writerTask, "ram-store", 4096, nullptr, 1, &g_writer, 0,
                                    MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "SD card mounted (%llu MB)",
             (unsigned long long)card->csd.capacity * card->csd.sector_size / (1024 * 1024));
    return true;
}

bool load(uint8_t *ram, std::vector<uint8_t> &state, std::vector<uint8_t> &packWrites) {
    if (!g_shadow) return false;
    Header h[2];
    const bool ok[2] = { readHeader(0, h[0]), readHeader(1, h[1]) };
    // Newest first; fall back to the other if its payload doesn't check out.
    int order[2] = { 0, 1 };
    if (ok[1] && (!ok[0] || h[1].seq > h[0].seq)) std::swap(order[0], order[1]);
    for (int slot : order) {
        if (!ok[slot]) continue;
        FILE *f = std::fopen(kSlots[slot], "rb");
        if (!f) continue;
        state.resize(h[slot].stateSize);
        packWrites.resize(h[slot].packWritesSize);
        const bool read = std::fseek(f, headerBytes(h[slot]), SEEK_SET) == 0 &&
                          std::fread(ram, 1, g_size, f) == g_size &&
                          std::fread(state.data(), 1, state.size(), f) == state.size() &&
                          std::fread(packWrites.data(), 1, packWrites.size(), f) == packWrites.size();
        std::fclose(f);
        if (!read || crcOf(ram, g_size, state, packWrites) != h[slot].crc) {
            ESP_LOGW(TAG, "%s is damaged, skipping it", kSlots[slot]);
            continue;
        }
        g_seq = h[slot].seq;
        g_next = slot ^ 1;
        // The machine state's layout is the build's own, and it holds the
        // writes to the pack it ran with.
        const bool samePack = h[slot].version >= 3 && h[slot].packId == g_packId;
        const bool sameBuild = !state.empty() && samePack &&
                               std::memcmp(h[slot].build, g_build, sizeof g_build) == 0;
        if (!sameBuild) state.clear();
        // The writes to the pack are good for that pack, whatever the build.
        if (!samePack || g_packId == 0) packWrites.clear();
        ESP_LOGI(TAG, "restored the Siena from %s (#%lu): %s", kSlots[slot],
                 (unsigned long)g_seq,
                 sameBuild ? "resuming where it was"
                           : "RAM only (saved by other firmware), so it warm-boots");
        return true;
    }
    // Nothing usable: carry on past any slot numbers already on the card.
    for (int s = 0; s < 2; ++s)
        if (ok[s] && h[s].seq > g_seq) g_seq = h[s].seq;
    state.clear();
    packWrites.clear();
    ESP_LOGI(TAG, "no saved RAM for this ROM: the Siena starts fresh");
    return false;
}

bool available() { return g_shadow != nullptr; }

void flush() {
    while (g_busy) vTaskDelay(pdMS_TO_TICKS(10));
}

int profile() { return g_profile; }

std::string profileFile(const char *name) {
    if (!g_shadow) return "";
    char dir[32];
    profileDir(g_profile, dir, sizeof dir);
    return std::string(dir) + "/" + name;
}

bool setProfile(int profile) {
    if (profile < 1 || profile > kMaxProfiles) return false;
    flush();
    char dir[32];
    profileDir(profile, dir, sizeof dir);
    if (g_shadow) mkdir(dir, 0777);
    g_profile = profile;
    std::snprintf(kSlots[0], sizeof kSlots[0], "%s/RAM_A.BIN", dir);
    std::snprintf(kSlots[1], sizeof kSlots[1], "%s/RAM_B.BIN", dir);
    g_seq = 0;
    g_next = 0;
    return true;
}

std::vector<int> profiles() {
    std::vector<int> out = { 1 };
    for (int p = 2; g_shadow && p <= kMaxProfiles; ++p)
        if (profileExists(p)) out.push_back(p);
    return out;
}

int createProfile() {
    for (int p = 2; g_shadow && p <= kMaxProfiles; ++p) {
        if (profileExists(p)) continue;
        char dir[32];
        profileDir(p, dir, sizeof dir);
        return mkdir(dir, 0777) == 0 ? p : -1;
    }
    return -1;
}

bool deleteProfile(int profile) {
    if (profile == 1 || profile == g_profile || !profileExists(profile)) return false;
    char dir[32], path[48];
    profileDir(profile, dir, sizeof dir);
    for (const char *f : { "RAM_A.BIN", "RAM_B.BIN" }) {
        std::snprintf(path, sizeof path, "%s/%s", dir, f);
        unlink(path);
    }
    return rmdir(dir) == 0;
}

bool save(const uint8_t *ram, const std::vector<uint8_t> &state,
          const std::vector<uint8_t> &packWrites) {
    if (!g_shadow || g_busy) return false;
    g_busy = true;
    std::memcpy(g_shadow, ram, g_size);
    g_stateShadow = state;
    g_packShadow = packWrites;
    xTaskNotifyGive(g_writer);
    return true;
}

} // namespace ram_store
