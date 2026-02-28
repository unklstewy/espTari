/**
 * @file esptari_video.h
 * @brief Video subsystem — frame buffer management and Shifter/VIDEL interface
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

/** ST display resolutions */
typedef enum {
    ESPTARI_RES_LOW  = 0,   /* 320x200, 16 colors */
    ESPTARI_RES_MED  = 1,   /* 640x200, 4 colors  */
    ESPTARI_RES_HIGH = 2,   /* 640x400, mono       */
} esptari_resolution_t;

/** Frame buffer descriptor */
typedef struct {
    uint8_t            *data;       /**< Pixel data (RGB565 or RGB888) */
    uint16_t            width;
    uint16_t            height;
    uint8_t             bpp;        /**< Bits per pixel (16 or 24) */
    esptari_resolution_t resolution;
    uint32_t            frame_num;  /**< Monotonic frame counter */
} esptari_frame_t;

/**
 * @brief Initialize video subsystem
 * @return ESP_OK on success
 */
esp_err_t esptari_video_init(void);

/**
 * @brief Get pointer to the current completed frame
 * @param[out] frame Filled with frame info
 * @return ESP_OK if a frame is available
 */
esp_err_t esptari_video_get_frame(esptari_frame_t *frame);

/**
 * @brief Notify that the consumer is done with the frame
 */
void esptari_video_release_frame(void);

/**
 * @brief Shut down video subsystem and free buffers
 */
void esptari_video_deinit(void);

#ifdef __cplusplus
}
#endif
