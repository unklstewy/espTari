/**
 * @file esptari_stream.h
 * @brief Low-latency A/V streaming — HW JPEG encoder + WebSocket transport
 *
 * Binary WebSocket protocol:
 *   Video: [0x01][frame_num:4LE][ts_ms:4LE][w:2LE][h:2LE][JPEG data…]
 *   Audio: [0x02][ts_ms:4LE][samples:2LE][ch:1][bits:1][PCM data…]
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Binary packet type tags ─────────────────────────────── */
#define ESPTARI_PKT_VIDEO   0x01
#define ESPTARI_PKT_AUDIO   0x02

/* ── Header sizes (bytes) ────────────────────────────────── */
#define ESPTARI_VIDEO_HDR   13   /* type(1)+frame(4)+ts(4)+w(2)+h(2) */
#define ESPTARI_AUDIO_HDR    9   /* type(1)+ts(4)+samples(2)+ch(1)+bits(1) */

/** Stream statistics */
typedef struct {
    uint32_t frames_sent;       /**< Total video frames sent */
    uint32_t audio_chunks_sent; /**< Total audio chunks sent */
    uint64_t bytes_sent;        /**< Total bytes over WebSocket */
    float    fps;               /**< Measured frames per second */
    uint32_t clients;           /**< Currently connected WS clients */
    uint32_t dropped_frames;    /**< Encode failures / no-client drops */
    uint32_t encode_time_us;    /**< Last JPEG encode duration (µs) */
    uint8_t  jpeg_quality;      /**< Current JPEG quality 1-100 */
} esptari_stream_stats_t;

/**
 * @brief Initialize streaming subsystem
 *
 * Creates the HW JPEG encoder, allocates DMA-aligned output buffers,
 * and registers the `/ws` WebSocket endpoint on @p server.
 *
 * @param server  Running HTTP server handle (from esptari_web)
 * @return ESP_OK on success
 */
esp_err_t esptari_stream_init(httpd_handle_t server);

/**
 * @brief Start the streaming FreeRTOS task
 *
 * Pulls frames from esptari_video, JPEG-encodes them via HW,
 * reads PCM from esptari_audio, and broadcasts both to all
 * connected WebSocket clients.
 */
esp_err_t esptari_stream_start(void);

/**
 * @brief Stop the streaming task (blocks until exited)
 */
void esptari_stream_stop(void);

/**
 * @brief Set JPEG encoding quality
 * @param quality  1 (worst) – 100 (best), default 80
 */
void esptari_stream_set_quality(uint8_t quality);

/**
 * @brief Snapshot current streaming statistics
 */
void esptari_stream_get_stats(esptari_stream_stats_t *stats);

/**
 * @brief Tear down streaming — stops task, frees encoder & buffers
 */
void esptari_stream_deinit(void);

#ifdef __cplusplus
}
#endif
