/**
 * @file esptari_video.h
 * @brief Video subsystem — double-buffered framebuffer in PSRAM
 *
 * The framebuffer is written by the emulation core (Shifter) and read by
 * the streaming subsystem.  All frames use RGB565 little-endian and are
 * stored in PSRAM to keep internal SRAM free for the CPU core.
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
#define ESPTARI_VIDEO_MAX_WIDTH   640
#define ESPTARI_VIDEO_MAX_HEIGHT  400
#define ESPTARI_VIDEO_BPP         2      /* RGB565 = 2 bytes/pixel */
#define ESPTARI_VIDEO_MAX_FRAME_SIZE \
    (ESPTARI_VIDEO_MAX_WIDTH * ESPTARI_VIDEO_MAX_HEIGHT * ESPTARI_VIDEO_BPP)

/*---------------------------------------------------------------------
 * Types
 *-------------------------------------------------------------------*/

/** ST display resolution */
typedef enum {
    ESPTARI_RES_LOW  = 0,   /**< 320×200, 16 colors — upscaled 2× to 640×400 */
    ESPTARI_RES_MED  = 1,   /**< 640×200,  4 colors — line-doubled to 640×400 */
    ESPTARI_RES_HIGH = 2,   /**< 640×400, monochrome                          */
} esptari_resolution_t;

/** Read-only reference to a completed frame */
typedef struct {
    const uint8_t *data;            /**< RGB565 LE pixel data in PSRAM         */
    uint16_t       width;           /**< Frame width  (always 640 for stream)  */
    uint16_t       height;          /**< Frame height (always 400 for stream)  */
    uint8_t        bpp;             /**< Bytes per pixel (2 = RGB565)          */
    esptari_resolution_t resolution;/**< Source ST resolution                  */
    uint32_t       frame_num;       /**< Monotonic counter                     */
    int64_t        timestamp_us;    /**< esp_timer_get_time() when swapped     */
} esptari_frame_t;

/*---------------------------------------------------------------------
 * API
 *-------------------------------------------------------------------*/

/**
 * @brief Initialize video subsystem — allocate double buffers in PSRAM.
 * @return ESP_OK on success, ESP_ERR_NO_MEM if PSRAM allocation fails
 */
esp_err_t esptari_video_init(void);

/**
 * @brief Get a writeable pointer to the back-buffer.
 *
 * The emulation core calls this, draws a frame, then calls
 * esptari_video_swap() to publish it.
 *
 * @param[out] buf  Receives pointer to writeable RGB565 buffer
 * @param[out] size Receives size in bytes (always MAX_FRAME_SIZE)
 * @return ESP_OK or ESP_ERR_INVALID_STATE if not initialised
 */
esp_err_t esptari_video_get_write_buffer(uint8_t **buf, size_t *size);

/**
 * @brief Swap buffers — publish the back-buffer as the latest frame.
 *
 * @param res  Resolution of the frame just rendered
 */
void esptari_video_swap(esptari_resolution_t res);

/**
 * @brief Get a read-only reference to the most recent completed frame.
 *
 * Call esptari_video_release_frame() when done.
 *
 * @param[out] frame Filled with frame metadata + data pointer
 * @return ESP_OK, or ESP_ERR_NOT_FOUND if no frame available yet
 */
esp_err_t esptari_video_get_frame(esptari_frame_t *frame);

/**
 * @brief Release a frame obtained via get_frame().
 */
void esptari_video_release_frame(void);

/**
 * @brief Set the current resolution (affects test pattern).
 */
void esptari_video_set_resolution(esptari_resolution_t res);

/**
 * @brief Generate a test-pattern frame into the back-buffer and swap it.
 *
 * Useful for verifying the full video→encode→stream→browser pipeline
 * without the emulator running.
 */
void esptari_video_generate_test_pattern(void);

/**
 * @brief De-initialise video subsystem and free buffers.
 */
void esptari_video_deinit(void);

#ifdef __cplusplus
}
#endif
