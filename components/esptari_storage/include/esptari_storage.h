/**
 * @file esptari_storage.h
 * @brief Storage management — SD card, ROM management, machine profiles
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** ROM types */
typedef enum {
    ESPTARI_ROM_TOS = 0,        /**< TOS ROM image */
    ESPTARI_ROM_CARTRIDGE,      /**< Cartridge ROM */
    ESPTARI_ROM_BIOS,           /**< Other BIOS ROMs */
} esptari_rom_type_t;

/** ROM descriptor */
typedef struct {
    char               path[128];
    esptari_rom_type_t type;
    uint32_t           size;
    uint32_t           crc32;
    char               version[32];
} esptari_rom_info_t;

/**
 * @brief Initialize storage subsystem (SD card, SPIFFS)
 * @return ESP_OK on success
 */
esp_err_t esptari_storage_init(void);

/**
 * @brief List available TOS ROMs
 * @param[out] list  Array to fill
 * @param      max   Maximum entries
 * @return Number of ROMs found
 */
int esptari_storage_list_roms(esptari_rom_info_t *list, int max);

/**
 * @brief Load a ROM into memory
 * @param path     Path to ROM file on SD card
 * @param[out] buf Destination buffer
 * @param buf_sz   Buffer size
 * @return Actual bytes loaded, or negative on error
 */
int esptari_storage_load_rom(const char *path, uint8_t *buf, uint32_t buf_sz);

/**
 * @brief Check if SD card is mounted and accessible
 */
bool esptari_storage_sd_ready(void);

/**
 * @brief Shut down storage subsystem
 */
void esptari_storage_deinit(void);

#ifdef __cplusplus
}
#endif
