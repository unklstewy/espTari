/**
 * @file esptari_audio.c
 * @brief Audio subsystem — ring-buffered PCM in PSRAM
 *
 * Simple lock-free single-producer / single-consumer ring buffer.
 * The write position is only advanced by the emulator task and the
 * read position only by the streaming task, so no mutex is needed
 * (volatile + compiler barriers suffice on a cache-coherent system
 * like the ESP32-P4).
 *
 * SPDX-License-Identifier: MIT
 */
#include "esptari_audio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "esptari_audio";

/* ── State ─────────────────────────────────────────────────────────── */

static uint8_t *s_ring;                 /* ring buffer in PSRAM        */
static volatile uint32_t s_wr;          /* write position              */
static volatile uint32_t s_rd;          /* read position               */
static esptari_audio_fmt_t s_fmt;
static bool s_initialised;
static float s_tone_phase;              /* for test-tone generation    */

#define RING_MASK  (ESPTARI_AUDIO_RING_SIZE - 1)  /* power-of-2 wrap */
_Static_assert((ESPTARI_AUDIO_RING_SIZE & (ESPTARI_AUDIO_RING_SIZE - 1)) == 0,
               "Ring size must be power of 2");

/* ── Init / Deinit ─────────────────────────────────────────────────── */

esp_err_t esptari_audio_init(const esptari_audio_fmt_t *fmt)
{
    if (!fmt) return ESP_ERR_INVALID_ARG;
    if (s_initialised) return ESP_OK;

    s_ring = heap_caps_calloc(1, ESPTARI_AUDIO_RING_SIZE,
                              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_ring) {
        ESP_LOGE(TAG, "PSRAM alloc failed for audio ring (%d bytes)",
                 ESPTARI_AUDIO_RING_SIZE);
        return ESP_ERR_NO_MEM;
    }

    s_fmt  = *fmt;
    s_wr   = 0;
    s_rd   = 0;
    s_tone_phase = 0.0f;
    s_initialised = true;

    ESP_LOGI(TAG, "Audio: %lu Hz %dch %d-bit, ring %d KB",
             (unsigned long)fmt->sample_rate, fmt->channels, fmt->bits,
             ESPTARI_AUDIO_RING_SIZE / 1024);
    return ESP_OK;
}

void esptari_audio_deinit(void)
{
    if (!s_initialised) return;
    heap_caps_free(s_ring);
    s_ring = NULL;
    s_initialised = false;
    ESP_LOGI(TAG, "Audio subsystem shut down");
}

/* ── Write path (emulation core) ───────────────────────────────────── */

int esptari_audio_write(const uint8_t *data, int len)
{
    if (!s_initialised || !data || len <= 0) return 0;

    uint32_t wr = s_wr;
    uint32_t rd = s_rd;
    /* Available space = total - used - 1 (one slot reserved) */
    uint32_t used  = (wr - rd) & RING_MASK;
    uint32_t space = ESPTARI_AUDIO_RING_SIZE - 1 - used;
    if ((uint32_t)len > space) len = (int)space;
    if (len <= 0) return 0;

    /* Copy in up to two chunks (wrap around) */
    uint32_t pos   = wr & RING_MASK;
    uint32_t chunk = ESPTARI_AUDIO_RING_SIZE - pos;
    if (chunk > (uint32_t)len) chunk = (uint32_t)len;

    memcpy(s_ring + pos, data, chunk);
    if ((uint32_t)len > chunk) {
        memcpy(s_ring, data + chunk, (uint32_t)len - chunk);
    }

    s_wr = (wr + (uint32_t)len) & RING_MASK;
    return len;
}

/* ── Read path (stream component) ──────────────────────────────────── */

int esptari_audio_read(uint8_t *buf, int len)
{
    if (!s_initialised || !buf || len <= 0) return 0;

    uint32_t wr = s_wr;
    uint32_t rd = s_rd;
    uint32_t avail = (wr - rd) & RING_MASK;
    if ((uint32_t)len > avail) len = (int)avail;
    if (len <= 0) return 0;

    uint32_t pos   = rd & RING_MASK;
    uint32_t chunk = ESPTARI_AUDIO_RING_SIZE - pos;
    if (chunk > (uint32_t)len) chunk = (uint32_t)len;

    memcpy(buf, s_ring + pos, chunk);
    if ((uint32_t)len > chunk) {
        memcpy(buf + chunk, s_ring, (uint32_t)len - chunk);
    }

    s_rd = (rd + (uint32_t)len) & RING_MASK;
    return len;
}

int esptari_audio_available(void)
{
    if (!s_initialised) return 0;
    return (int)((s_wr - s_rd) & RING_MASK);
}

const esptari_audio_fmt_t *esptari_audio_get_format(void)
{
    return s_initialised ? &s_fmt : NULL;
}

/* ── Test tone (440 Hz sine wave) ──────────────────────────────────── */

void esptari_audio_generate_test_tone(void)
{
    if (!s_initialised) return;

    /* Generate one video-frame's worth of audio (20 ms @ 50 Hz) */
    const float freq = 440.0f;
    int samples_per_frame = (int)(s_fmt.sample_rate / 50);
    int bytes_per_sample  = (s_fmt.bits / 8) * s_fmt.channels;
    int total_bytes       = samples_per_frame * bytes_per_sample;

    /* Heap buffer — stack is too small on the main task */
    uint8_t *tmp = malloc(4096);
    if (!tmp) return;
    if (total_bytes > 4096) total_bytes = 4096;

    int16_t *out = (int16_t *)tmp;
    int sample_count = total_bytes / bytes_per_sample;

    for (int i = 0; i < sample_count; i++) {
        float t = s_tone_phase / (float)s_fmt.sample_rate;
        int16_t val = (int16_t)(sinf(2.0f * M_PI * freq * t) * 16000.0f);
        s_tone_phase += 1.0f;
        if (s_tone_phase >= (float)s_fmt.sample_rate) s_tone_phase -= (float)s_fmt.sample_rate;

        if (s_fmt.channels == 2) {
            *out++ = val;   /* L */
            *out++ = val;   /* R */
        } else {
            *out++ = val;
        }
    }

    esptari_audio_write(tmp, sample_count * bytes_per_sample);
    free(tmp);
}
