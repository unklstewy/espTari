/**
 * @file esptari_core.c
 * @brief Core emulation framework — machine lifecycle, timing, component bus
 *
 * SPDX-License-Identifier: MIT
 */
#include "esptari_core.h"
#include "esp_log.h"

static const char *TAG = "esptari_core";

static esptari_state_t s_state = ESPTARI_STATE_STOPPED;

esp_err_t esptari_core_init(void)
{
    ESP_LOGI(TAG, "Core emulation framework initialized");
    s_state = ESPTARI_STATE_STOPPED;
    return ESP_OK;
}

esp_err_t esptari_core_load_machine(esptari_machine_t machine)
{
    ESP_LOGI(TAG, "Loading machine profile %d", (int)machine);
    /* TODO: Parse machine JSON, load EBIN components via loader */
    return ESP_OK;
}

esp_err_t esptari_core_start(void)
{
    ESP_LOGI(TAG, "Starting emulation");
    s_state = ESPTARI_STATE_RUNNING;
    /* TODO: Start CPU execution task, video/audio timing */
    return ESP_OK;
}

void esptari_core_pause(void)
{
    if (s_state == ESPTARI_STATE_RUNNING) {
        s_state = ESPTARI_STATE_PAUSED;
        ESP_LOGI(TAG, "Emulation paused");
    }
}

void esptari_core_resume(void)
{
    if (s_state == ESPTARI_STATE_PAUSED) {
        s_state = ESPTARI_STATE_RUNNING;
        ESP_LOGI(TAG, "Emulation resumed");
    }
}

void esptari_core_stop(void)
{
    s_state = ESPTARI_STATE_STOPPED;
    ESP_LOGI(TAG, "Emulation stopped");
    /* TODO: Release EBIN components, free memory */
}

esptari_state_t esptari_core_get_state(void)
{
    return s_state;
}

void esptari_core_reset(void)
{
    ESP_LOGI(TAG, "Machine reset");
    /* TODO: Reset CPU, clear RAM, reload TOS vectors */
}
