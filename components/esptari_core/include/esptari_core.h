/**
 * @file esptari_core.h
 * @brief Core emulation framework — machine lifecycle, timing, component bus
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

/** Machine model identifiers */
typedef enum {
    ESPTARI_MACHINE_ST = 0,
    ESPTARI_MACHINE_STFM,
    ESPTARI_MACHINE_MEGA_ST,
    ESPTARI_MACHINE_STE,
    ESPTARI_MACHINE_MEGA_STE,
    ESPTARI_MACHINE_TT030,
    ESPTARI_MACHINE_FALCON030,
    ESPTARI_MACHINE_COUNT
} esptari_machine_t;

/** Emulation state */
typedef enum {
    ESPTARI_STATE_STOPPED = 0,
    ESPTARI_STATE_RUNNING,
    ESPTARI_STATE_PAUSED,
    ESPTARI_STATE_ERROR,
} esptari_state_t;

/**
 * @brief Initialize the core emulation framework
 * @return ESP_OK on success
 */
esp_err_t esptari_core_init(void);

/**
 * @brief Load a machine profile (JSON) and instantiate components
 * @param machine Machine model to load
 * @return ESP_OK on success
 */
esp_err_t esptari_core_load_machine(esptari_machine_t machine);

/**
 * @brief Start emulation
 * @return ESP_OK on success
 */
esp_err_t esptari_core_start(void);

/**
 * @brief Pause emulation
 */
void esptari_core_pause(void);

/**
 * @brief Resume emulation after pause
 */
void esptari_core_resume(void);

/**
 * @brief Stop emulation and release resources
 */
void esptari_core_stop(void);

/**
 * @brief Get current emulation state
 */
esptari_state_t esptari_core_get_state(void);

/**
 * @brief Reset the emulated machine (warm reset)
 */
void esptari_core_reset(void);

#ifdef __cplusplus
}
#endif
