/**
 * @file esptari_video.c
 * @brief Video subsystem — frame buffer management
 *
 * SPDX-License-Identifier: MIT
 */
#include "esptari_video.h"
#include "esp_log.h"

static const char *TAG = "esptari_video";

esp_err_t esptari_video_init(void)
{
    ESP_LOGI(TAG, "Video subsystem initialized");
    /* TODO: Allocate double-buffered frame buffers in PSRAM */
    return ESP_OK;
}

esp_err_t esptari_video_get_frame(esptari_frame_t *frame)
{
    if (!frame) return ESP_ERR_INVALID_ARG;
    /* TODO: Return pointer to last completed frame */
    return ESP_ERR_NOT_FOUND;
}

void esptari_video_release_frame(void)
{
    /* TODO: Mark frame buffer as available for next render */
}

void esptari_video_deinit(void)
{
    ESP_LOGI(TAG, "Video subsystem shut down");
}
