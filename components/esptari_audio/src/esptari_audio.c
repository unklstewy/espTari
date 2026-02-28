/**
 * @file esptari_audio.c
 * @brief Audio subsystem — sample generation and ring buffer
 *
 * SPDX-License-Identifier: MIT
 */
#include "esptari_audio.h"
#include "esp_log.h"

static const char *TAG = "esptari_audio";

esp_err_t esptari_audio_init(const esptari_audio_fmt_t *fmt)
{
    if (!fmt) return ESP_ERR_INVALID_ARG;
    ESP_LOGI(TAG, "Audio initialized: %luHz %dch %dbit",
             (unsigned long)fmt->sample_rate, fmt->channels, fmt->bits);
    /* TODO: Allocate ring buffer in PSRAM */
    return ESP_OK;
}

int esptari_audio_read(uint8_t *buf, int len)
{
    (void)buf; (void)len;
    /* TODO: Read from ring buffer */
    return 0;
}

int esptari_audio_available(void)
{
    /* TODO: Return bytes in ring buffer */
    return 0;
}

void esptari_audio_deinit(void)
{
    ESP_LOGI(TAG, "Audio subsystem shut down");
}
