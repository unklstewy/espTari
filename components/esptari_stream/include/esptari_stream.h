/**
 * @file esptari_stream.h
 * @brief Low-latency A/V streaming — WebSocket transport, frame encoding
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

/** Stream statistics */
typedef struct {
    uint32_t frames_sent;
    uint32_t audio_chunks_sent;
    uint32_t bytes_sent;
    uint32_t fps;               /**< Current frames per second */
    uint32_t clients;           /**< Connected WebSocket clients */
    uint32_t dropped_frames;
} esptari_stream_stats_t;

/**
 * @brief Initialize streaming subsystem
 * @return ESP_OK on success
 */
esp_err_t esptari_stream_init(void);

/**
 * @brief Start streaming to connected clients
 */
esp_err_t esptari_stream_start(void);

/**
 * @brief Stop streaming
 */
void esptari_stream_stop(void);

/**
 * @brief Get streaming statistics
 */
void esptari_stream_get_stats(esptari_stream_stats_t *stats);

/**
 * @brief Shut down streaming subsystem
 */
void esptari_stream_deinit(void);

#ifdef __cplusplus
}
#endif
