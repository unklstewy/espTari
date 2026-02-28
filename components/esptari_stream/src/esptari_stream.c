/**
 * @file esptari_stream.c
 * @brief Low-latency A/V streaming — WebSocket transport
 *
 * SPDX-License-Identifier: MIT
 */
#include "esptari_stream.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "esptari_stream";

esp_err_t esptari_stream_init(void)
{
    ESP_LOGI(TAG, "Streaming subsystem initialized");
    /* TODO: Set up WebSocket server, frame encoder, audio packetizer */
    return ESP_OK;
}

esp_err_t esptari_stream_start(void)
{
    ESP_LOGI(TAG, "Streaming started");
    /* TODO: Begin sending frames + audio to connected clients */
    return ESP_OK;
}

void esptari_stream_stop(void)
{
    ESP_LOGI(TAG, "Streaming stopped");
}

void esptari_stream_get_stats(esptari_stream_stats_t *stats)
{
    if (stats) {
        memset(stats, 0, sizeof(*stats));
    }
}

void esptari_stream_deinit(void)
{
    ESP_LOGI(TAG, "Streaming subsystem shut down");
}
