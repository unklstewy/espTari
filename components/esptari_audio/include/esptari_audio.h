/**
 * @file esptari_audio.h
 * @brief Audio subsystem — sample generation and buffering
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

/** Audio format descriptor */
typedef struct {
    uint32_t sample_rate;   /**< Samples per second (e.g., 44100) */
    uint8_t  channels;      /**< 1=mono, 2=stereo */
    uint8_t  bits;          /**< Bits per sample (8 or 16) */
} esptari_audio_fmt_t;

/**
 * @brief Initialize audio subsystem
 * @param fmt Desired audio format
 * @return ESP_OK on success
 */
esp_err_t esptari_audio_init(const esptari_audio_fmt_t *fmt);

/**
 * @brief Read audio samples from the ring buffer
 * @param[out] buf   Destination buffer
 * @param      len   Maximum bytes to read
 * @return Number of bytes actually read
 */
int esptari_audio_read(uint8_t *buf, int len);

/**
 * @brief Get number of buffered audio bytes available
 */
int esptari_audio_available(void);

/**
 * @brief Shut down audio subsystem
 */
void esptari_audio_deinit(void);

#ifdef __cplusplus
}
#endif
