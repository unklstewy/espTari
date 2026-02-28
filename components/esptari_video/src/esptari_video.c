/**
 * @file esptari_video.c
 * @brief Video subsystem — double-buffered framebuffer in PSRAM
 *
 * Two 640×400 RGB565 buffers live in PSRAM.  The emulation core writes
 * into the "back" buffer, then calls esptari_video_swap() to atomically
 * promote it to the "front" buffer.  The stream component reads the
 * front buffer via esptari_video_get_frame().
 *
 * When the emulator is not running, generate_test_pattern() can be
 * called to paint colour bars for end-to-end pipeline verification.
 *
 * SPDX-License-Identifier: MIT
 */
#include "esptari_video.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "esptari_video";

/* ── State ─────────────────────────────────────────────────────────── */

static uint8_t *s_buf[2];              /* double-buffer in PSRAM        */
static uint8_t  s_write_idx;           /* index of current back-buffer  */
static uint32_t s_frame_num;
static int64_t  s_last_swap_us;
static esptari_resolution_t s_resolution;
static SemaphoreHandle_t s_lock;       /* protects swap / read          */
static bool s_initialised;
static bool s_frame_ready;             /* at least one swap has occurred */

/* ── Init / Deinit ─────────────────────────────────────────────────── */

esp_err_t esptari_video_init(void)
{
    if (s_initialised) return ESP_OK;

    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < 2; i++) {
        s_buf[i] = heap_caps_calloc(1, ESPTARI_VIDEO_MAX_FRAME_SIZE,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_buf[i]) {
            ESP_LOGE(TAG, "PSRAM alloc failed for buffer %d (%d bytes)",
                     i, ESPTARI_VIDEO_MAX_FRAME_SIZE);
            /* Clean up on partial failure */
            if (s_buf[0]) { heap_caps_free(s_buf[0]); s_buf[0] = NULL; }
            vSemaphoreDelete(s_lock); s_lock = NULL;
            return ESP_ERR_NO_MEM;
        }
    }

    s_write_idx   = 0;
    s_frame_num   = 0;
    s_last_swap_us = 0;
    s_resolution  = ESPTARI_RES_LOW;
    s_frame_ready = false;
    s_initialised = true;

    ESP_LOGI(TAG, "Video: 2× %dx%d RGB565 (%d KB each) in PSRAM",
             ESPTARI_VIDEO_MAX_WIDTH, ESPTARI_VIDEO_MAX_HEIGHT,
             ESPTARI_VIDEO_MAX_FRAME_SIZE / 1024);
    return ESP_OK;
}

void esptari_video_deinit(void)
{
    if (!s_initialised) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (int i = 0; i < 2; i++) {
        heap_caps_free(s_buf[i]);
        s_buf[i] = NULL;
    }
    s_initialised = false;
    s_frame_ready = false;
    xSemaphoreGive(s_lock);
    vSemaphoreDelete(s_lock);
    s_lock = NULL;
    ESP_LOGI(TAG, "Video subsystem shut down");
}

/* ── Write path (emulation core) ───────────────────────────────────── */

esp_err_t esptari_video_get_write_buffer(uint8_t **buf, size_t *size)
{
    if (!s_initialised) return ESP_ERR_INVALID_STATE;
    if (!buf || !size) return ESP_ERR_INVALID_ARG;
    *buf  = s_buf[s_write_idx];
    *size = ESPTARI_VIDEO_MAX_FRAME_SIZE;
    return ESP_OK;
}

void esptari_video_swap(esptari_resolution_t res)
{
    if (!s_initialised) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_write_idx   = 1 - s_write_idx;   /* flip back ↔ front */
    s_frame_num++;
    s_last_swap_us = esp_timer_get_time();
    s_resolution   = res;
    s_frame_ready  = true;
    xSemaphoreGive(s_lock);
}

void esptari_video_set_resolution(esptari_resolution_t res)
{
    s_resolution = res;
}

/* ── Read path (stream component) ──────────────────────────────────── */

esp_err_t esptari_video_get_frame(esptari_frame_t *frame)
{
    if (!frame) return ESP_ERR_INVALID_ARG;
    if (!s_initialised || !s_frame_ready) return ESP_ERR_NOT_FOUND;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    uint8_t front = 1 - s_write_idx;   /* read from the front buffer */
    frame->data         = s_buf[front];
    frame->width        = ESPTARI_VIDEO_MAX_WIDTH;
    frame->height       = ESPTARI_VIDEO_MAX_HEIGHT;
    frame->bpp          = ESPTARI_VIDEO_BPP;
    frame->resolution   = s_resolution;
    frame->frame_num    = s_frame_num;
    frame->timestamp_us = s_last_swap_us;
    /* NOTE: lock held until release_frame() is called */
    return ESP_OK;
}

void esptari_video_release_frame(void)
{
    if (!s_initialised) return;
    xSemaphoreGive(s_lock);
}

/* ── Test pattern ──────────────────────────────────────────────────── */

/**
 * RGB565 colour bars: white, yellow, cyan, green, magenta, red, blue, black
 * Matches standard SMPTE-like pattern.
 */
static const uint16_t COLOUR_BARS[] = {
    0xFFFF, /* white   */
    0xFFE0, /* yellow  */
    0x07FF, /* cyan    */
    0x07E0, /* green   */
    0xF81F, /* magenta */
    0xF800, /* red     */
    0x001F, /* blue    */
    0x0000, /* black   */
};
#define NUM_BARS (sizeof(COLOUR_BARS) / sizeof(COLOUR_BARS[0]))

void esptari_video_generate_test_pattern(void)
{
    if (!s_initialised) return;

    uint8_t *buf;
    size_t size;
    if (esptari_video_get_write_buffer(&buf, &size) != ESP_OK) return;

    uint16_t *pixels = (uint16_t *)buf;
    const uint16_t w = ESPTARI_VIDEO_MAX_WIDTH;
    const uint16_t h = ESPTARI_VIDEO_MAX_HEIGHT;
    const uint16_t bar_w = w / NUM_BARS;

    for (uint16_t y = 0; y < h; y++) {
        for (uint16_t x = 0; x < w; x++) {
            uint16_t bar_idx = x / bar_w;
            if (bar_idx >= NUM_BARS) bar_idx = NUM_BARS - 1;

            uint16_t colour = COLOUR_BARS[bar_idx];

            /* Bottom 20%: gradient ramp for testing grey levels */
            if (y >= h * 4 / 5) {
                uint8_t grey5 = (x * 31) / w;  /* 0..31 */
                colour = (grey5 << 11) | (grey5 * 2 << 5) | grey5;
            }

            pixels[y * w + x] = colour;
        }
    }

    esptari_video_swap(s_resolution);
    ESP_LOGD(TAG, "Test pattern generated (frame %lu)", (unsigned long)s_frame_num);
}
