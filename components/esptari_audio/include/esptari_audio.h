/**
 * @file esptari_audio.h
 * @brief Audio subsystem — ring-buffered PCM samples for streaming
 *
 * The emulation core (YM2149 + optional DMA audio) pushes PCM samples
 * via esptari_audio_write().  The streaming component pulls them via
 * esptari_audio_read().  A lock-free ring buffer in PSRAM bridges the
 * two sides.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------
 * Constants
 *-------------------------------------------------------------------*/
#define ESPTARI_AUDIO_RING_SIZE  (32 * 1024)   /* 32 KB ≈ 185 ms @ stereo 44.1 kHz */

/*---------------------------------------------------------------------
 * Types
 *-------------------------------------------------------------------*/

/** Audio format descriptor */
typedef struct {
    uint32_t sample_rate;   /**< Samples per second (e.g., 44100)  */
    uint8_t  channels;      /**< 1 = mono, 2 = stereo              */
    uint8_t  bits;          /**< Bits per sample (8 or 16)         */
} esptari_audio_fmt_t;

/*---------------------------------------------------------------------
 * API
 *-------------------------------------------------------------------*/

/**
 * @brief Initialise audio subsystem and allocate ring buffer.
 * @param fmt Desired audio format
 * @return ESP_OK on success
 */
esp_err_t esptari_audio_init(const esptari_audio_fmt_t *fmt);

/**
 * @brief Write PCM samples into the ring buffer (producer / emulation core).
 * @param data  Source PCM data
 * @param len   Number of bytes to write
 * @return Number of bytes actually written (may be less if buffer full)
 */
int esptari_audio_write(const uint8_t *data, int len);

/**
 * @brief Read PCM samples from the ring buffer (consumer / stream).
 * @param[out] buf  Destination buffer
 * @param      len  Maximum bytes to read
 * @return Number of bytes actually read
 */
int esptari_audio_read(uint8_t *buf, int len);

/**
 * @brief Get number of buffered audio bytes available for reading.
 */
int esptari_audio_available(void);

/**
 * @brief Get the current audio format.
 */
const esptari_audio_fmt_t *esptari_audio_get_format(void);

/**
 * @brief Generate a test tone (440 Hz sine wave) into the ring buffer.
 *
 * Writes one frame's worth of samples (~882 @ 44.1 kHz / 50 Hz).
 */
void esptari_audio_generate_test_tone(void);

/**
 * @brief Shut down audio subsystem and free ring buffer.
 */
void esptari_audio_deinit(void);

#ifdef __cplusplus
}
#endif
