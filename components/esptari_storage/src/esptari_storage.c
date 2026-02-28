/**
 * @file esptari_storage.c
 * @brief Storage management — SD card, ROM loading
 *
 * SPDX-License-Identifier: MIT
 */
#include "esptari_storage.h"
#include "esp_log.h"

static const char *TAG = "esptari_storage";

esp_err_t esptari_storage_init(void)
{
    ESP_LOGI(TAG, "Storage subsystem initialized");
    /* TODO: Mount SD card, scan ROM directory */
    return ESP_OK;
}

int esptari_storage_list_roms(esptari_rom_info_t *list, int max)
{
    (void)list; (void)max;
    /* TODO: Scan /sdcard/roms/tos/ directory */
    return 0;
}

int esptari_storage_load_rom(const char *path, uint8_t *buf, uint32_t buf_sz)
{
    if (!path || !buf) return -1;
    ESP_LOGI(TAG, "Loading ROM: %s", path);
    /* TODO: Read file from SD card */
    return -1;
}

bool esptari_storage_sd_ready(void)
{
    /* TODO: Check mount status */
    return false;
}

void esptari_storage_deinit(void)
{
    ESP_LOGI(TAG, "Storage subsystem shut down");
}
