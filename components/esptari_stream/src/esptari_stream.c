/**
 * @file esptari_stream.c
 * @brief Low-latency A/V streaming — HW JPEG encoder + WebSocket broadcast
 *
 * Architecture
 * ────────────
 *  ┌──────────┐  get_frame   ┌────────────┐  /ws binary   ┌─────────┐
 *  │ esptari  │─────────────▸│  JPEG HW   │──────────────▸│ Browser │
 *  │  video   │              │  encoder   │               │ Canvas  │
 *  └──────────┘              └────────────┘               └─────────┘
 *  ┌──────────┐  read        ┌────────────┐  /ws binary   ┌─────────┐
 *  │ esptari  │─────────────▸│  packetize │──────────────▸│ Browser │
 *  │  audio   │              │  PCM       │               │WebAudio │
 *  └──────────┘              └────────────┘               └─────────┘
 *
 * The streaming FreeRTOS task runs on core 1 at priority 5.
 * It pulls the current video front-buffer, hardware-JPEG-encodes it,
 * builds a binary packet, and broadcasts to every connected WS client.
 * Audio PCM chunks (20 ms) are read from the ring buffer and sent
 * similarly with their own header.
 *
 * SPDX-License-Identifier: MIT
 */
#include "esptari_stream.h"
#include "esptari_video.h"
#include "esptari_audio.h"

#include "driver/jpeg_encode.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "esptari_stream";

/* ── tunables ────────────────────────────────────────────── */
#define STREAM_TASK_STACK   (10 * 1024)
#define STREAM_TASK_PRIO    5
#define STREAM_TASK_CORE    1

#define MAX_JPEG_OUT        (256 * 1024)   /* 256 KB — generous for 640×400 */
#define AUDIO_PKT_MAX       (4096 + ESPTARI_AUDIO_HDR)
#define AUDIO_CHUNK_MS      20             /* 20 ms of PCM per packet */
#define MAX_WS_CLIENTS      8
#define FPS_WINDOW_US       1000000        /* 1-second FPS window */
#define DEFAULT_QUALITY     80
#define TARGET_FRAME_MS     40             /* ~25 fps */

/* ── state ───────────────────────────────────────────────── */
static httpd_handle_t        s_server;
static jpeg_encoder_handle_t s_jpeg;
static TaskHandle_t          s_task;
static volatile bool         s_running;

static uint8_t              *s_jpeg_buf;        /* cache-aligned JPEG output   */
static size_t                s_jpeg_buf_size;
static uint8_t              *s_video_pkt;       /* header + JPEG for broadcast */
static uint8_t               s_audio_pkt[AUDIO_PKT_MAX];  /* reused each loop */

static esptari_stream_stats_t s_stats;
static uint8_t                s_quality = DEFAULT_QUALITY;
static int64_t                s_fps_t0;
static uint32_t               s_fps_cnt;

/* ── little-endian helpers ───────────────────────────────── */
static inline void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
}
static inline void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* ── client helpers ──────────────────────────────────────── */

/** Count currently connected WebSocket clients. */
static int count_ws_clients(void)
{
    int  fds[MAX_WS_CLIENTS];
    size_t n = MAX_WS_CLIENTS;
    if (httpd_get_client_list(s_server, &n, fds) != ESP_OK) return 0;

    int cnt = 0;
    for (size_t i = 0; i < n; i++) {
        if (httpd_ws_get_fd_info(s_server, fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET)
            cnt++;
    }
    return cnt;
}

/**
 * Broadcast a binary WebSocket frame to every connected WS client.
 * Returns the number of clients the frame was successfully sent to.
 */
static int broadcast_binary(const uint8_t *data, size_t len)
{
    httpd_ws_frame_t ws = {
        .final   = true,
        .type    = HTTPD_WS_TYPE_BINARY,
        .payload = (uint8_t *)data,
        .len     = len,
    };

    int  fds[MAX_WS_CLIENTS];
    size_t n = MAX_WS_CLIENTS;
    if (httpd_get_client_list(s_server, &n, fds) != ESP_OK) return 0;

    int sent = 0;
    for (size_t i = 0; i < n; i++) {
        if (httpd_ws_get_fd_info(s_server, fds[i]) != HTTPD_WS_CLIENT_WEBSOCKET)
            continue;
        esp_err_t e = httpd_ws_send_data(s_server, fds[i], &ws);
        if (e == ESP_OK) {
            sent++;
        } else {
            ESP_LOGD(TAG, "WS send fd=%d err=%s", fds[i], esp_err_to_name(e));
        }
    }
    return sent;
}

/* ── streaming task ──────────────────────────────────────── */

static void stream_task(void *arg)
{
    ESP_LOGI(TAG, "Streaming task started (quality=%d, target=%d ms)",
             s_quality, TARGET_FRAME_MS);
    s_fps_t0  = esp_timer_get_time();
    s_fps_cnt = 0;

    while (s_running) {
        int64_t loop_t0 = esp_timer_get_time();

        /* How many WS clients right now? */
        int clients = count_ws_clients();
        s_stats.clients = (uint32_t)clients;

        if (clients == 0) {
            /* Nobody watching — sleep longer */
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        /* ─── Video ─────────────────────────────────────── */
        esptari_frame_t vf;
        if (esptari_video_get_frame(&vf) == ESP_OK) {

            jpeg_encode_cfg_t enc = {
                .width         = vf.width,
                .height        = vf.height,
                .src_type      = JPEG_ENCODE_IN_FORMAT_RGB565,
                .sub_sample    = JPEG_DOWN_SAMPLING_YUV420,
                .image_quality = s_quality,
            };

            uint32_t jpeg_sz = 0;
            int64_t  t0 = esp_timer_get_time();
            esp_err_t rc = jpeg_encoder_process(
                    s_jpeg, &enc,
                    vf.data, vf.width * vf.height * vf.bpp,
                    s_jpeg_buf, (uint32_t)s_jpeg_buf_size, &jpeg_sz);
            s_stats.encode_time_us = (uint32_t)(esp_timer_get_time() - t0);

            esptari_video_release_frame();          /* unlock front buffer */

            if (rc == ESP_OK && jpeg_sz > 0) {
                /* Assemble video packet:
                 *   [0x01][frame:4][ts_ms:4][w:2][h:2][JPEG…] */
                uint8_t *p = s_video_pkt;
                p[0] = ESPTARI_PKT_VIDEO;
                put_u32(p + 1,  vf.frame_num);
                put_u32(p + 5,  (uint32_t)(vf.timestamp_us / 1000));
                put_u16(p + 9,  (uint16_t)vf.width);
                put_u16(p + 11, (uint16_t)vf.height);
                memcpy(p + ESPTARI_VIDEO_HDR, s_jpeg_buf, jpeg_sz);

                size_t pkt_len = ESPTARI_VIDEO_HDR + jpeg_sz;
                int ok = broadcast_binary(s_video_pkt, pkt_len);
                if (ok > 0) {
                    s_stats.frames_sent++;
                    s_stats.bytes_sent += pkt_len;
                }

                /* FPS measurement */
                s_fps_cnt++;
                int64_t now = esp_timer_get_time();
                int64_t dt  = now - s_fps_t0;
                if (dt >= FPS_WINDOW_US) {
                    s_stats.fps = (float)s_fps_cnt * 1e6f / (float)dt;
                    s_fps_cnt = 0;
                    s_fps_t0  = now;
                }
            } else {
                s_stats.dropped_frames++;
                if (rc != ESP_OK) {
                    ESP_LOGW(TAG, "JPEG encode err: %s (q=%d %ux%u)",
                             esp_err_to_name(rc), s_quality,
                             (unsigned)vf.width, (unsigned)vf.height);
                }
            }
        }

        /* ─── Audio ─────────────────────────────────────── */
        const esptari_audio_fmt_t *af = esptari_audio_get_format();
        if (af && af->sample_rate > 0) {
            int frame_bytes = af->channels * (af->bits / 8);
            int chunk_bytes = (af->sample_rate * frame_bytes * AUDIO_CHUNK_MS) / 1000;
            if (chunk_bytes > AUDIO_PKT_MAX - ESPTARI_AUDIO_HDR)
                chunk_bytes = AUDIO_PKT_MAX - ESPTARI_AUDIO_HDR;

            int avail = esptari_audio_available();
            while (avail >= chunk_bytes && s_running) {
                int got = esptari_audio_read(
                        s_audio_pkt + ESPTARI_AUDIO_HDR, chunk_bytes);
                if (got <= 0) break;

                int samples = got / frame_bytes;
                uint32_t ts = (uint32_t)(esp_timer_get_time() / 1000);

                /* [0x02][ts:4][samples:2][ch:1][bits:1][PCM…] */
                s_audio_pkt[0] = ESPTARI_PKT_AUDIO;
                put_u32(s_audio_pkt + 1, ts);
                put_u16(s_audio_pkt + 5, (uint16_t)samples);
                s_audio_pkt[7] = af->channels;
                s_audio_pkt[8] = af->bits;

                size_t alen = ESPTARI_AUDIO_HDR + (size_t)got;
                int ok = broadcast_binary(s_audio_pkt, alen);
                if (ok > 0) {
                    s_stats.audio_chunks_sent++;
                    s_stats.bytes_sent += alen;
                }
                avail = esptari_audio_available();
            }
        }

        /* ─── Rate limiter ──────────────────────────────── */
        int64_t elapsed_ms = (esp_timer_get_time() - loop_t0) / 1000;
        int     delay_ms   = TARGET_FRAME_MS - (int)elapsed_ms;
        if (delay_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        } else {
            taskYIELD();
        }
    }

    ESP_LOGI(TAG, "Streaming task exited");
    s_task = NULL;
    vTaskDelete(NULL);
}

/* ── WebSocket endpoint handler ──────────────────────────── */

static esp_err_t ws_handler(httpd_req_t *req)
{
    /* Upgrade handshake — just accept */
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WS client connected (fd=%d)", httpd_req_to_sockfd(req));
        return ESP_OK;
    }

    /* Incoming client frame */
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(ws_pkt));
    esp_err_t rc = httpd_ws_recv_frame(req, &ws_pkt, 0);   /* get length */
    if (rc != ESP_OK) return rc;

    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        ESP_LOGI(TAG, "WS client disconnected");
        return ESP_OK;
    }

    /* Read small control messages (future: quality change, subscribe) */
    if (ws_pkt.len > 0 && ws_pkt.len <= 128) {
        uint8_t buf[128];
        ws_pkt.payload = buf;
        rc = httpd_ws_recv_frame(req, &ws_pkt, sizeof(buf));
        if (rc == ESP_OK) {
            ESP_LOGD(TAG, "WS rx %d bytes type=%d", (int)ws_pkt.len, ws_pkt.type);
        }
    }
    return ESP_OK;
}

/* ── Public API ──────────────────────────────────────────── */

esp_err_t esptari_stream_init(httpd_handle_t server)
{
    if (!server) {
        ESP_LOGE(TAG, "NULL server handle");
        return ESP_ERR_INVALID_ARG;
    }
    s_server = server;

    /* ---- hardware JPEG encoder ---- */
    jpeg_encode_engine_cfg_t eng = {
        .intr_priority = 0,
        .timeout_ms    = 100,
    };
    esp_err_t rc = jpeg_new_encoder_engine(&eng, &s_jpeg);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "JPEG encoder create failed: %s", esp_err_to_name(rc));
        return rc;
    }

    /* Cache-aligned output buffer (may land in PSRAM via DMA allocator) */
    jpeg_encode_memory_alloc_cfg_t mem = {
        .buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER,
    };
    s_jpeg_buf = jpeg_alloc_encoder_mem(MAX_JPEG_OUT, &mem, &s_jpeg_buf_size);
    if (!s_jpeg_buf) {
        ESP_LOGE(TAG, "JPEG output buffer alloc failed (%d KB)",
                 MAX_JPEG_OUT / 1024);
        jpeg_del_encoder_engine(s_jpeg);
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "JPEG output buffer: %u KB allocated",
             (unsigned)(s_jpeg_buf_size / 1024));

    /* Packet assembly buffer (PSRAM) for header + JPEG payload */
    s_video_pkt = heap_caps_malloc(ESPTARI_VIDEO_HDR + MAX_JPEG_OUT,
                                   MALLOC_CAP_SPIRAM);
    if (!s_video_pkt) {
        ESP_LOGE(TAG, "Video packet buffer alloc failed");
        free(s_jpeg_buf);
        jpeg_del_encoder_engine(s_jpeg);
        return ESP_ERR_NO_MEM;
    }

    /* ---- register /ws endpoint ---- */
    httpd_uri_t ws_uri = {
        .uri       = "/ws",
        .method    = HTTP_GET,
        .handler   = ws_handler,
        .user_ctx  = NULL,
        .is_websocket            = true,
        .handle_ws_control_frames = true,
    };
    rc = httpd_register_uri_handler(s_server, &ws_uri);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "/ws register failed: %s", esp_err_to_name(rc));
        heap_caps_free(s_video_pkt);
        free(s_jpeg_buf);
        jpeg_del_encoder_engine(s_jpeg);
        return rc;
    }

    memset(&s_stats, 0, sizeof(s_stats));
    s_stats.jpeg_quality = s_quality;
    s_running = false;
    s_task    = NULL;

    ESP_LOGI(TAG, "Stream subsystem ready (quality=%d)", s_quality);
    return ESP_OK;
}

esp_err_t esptari_stream_start(void)
{
    if (s_running) {
        ESP_LOGW(TAG, "Already streaming");
        return ESP_OK;
    }
    if (!s_server || !s_jpeg) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    s_running = true;
    BaseType_t ok = xTaskCreatePinnedToCore(
            stream_task, "av_stream",
            STREAM_TASK_STACK, NULL,
            STREAM_TASK_PRIO, &s_task,
            STREAM_TASK_CORE);
    if (ok != pdPASS) {
        s_running = false;
        ESP_LOGE(TAG, "Task create failed");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Streaming started");
    return ESP_OK;
}

void esptari_stream_stop(void)
{
    if (!s_running) return;
    ESP_LOGI(TAG, "Stopping stream…");
    s_running = false;

    /* Wait up to 1 s for task to exit */
    for (int i = 0; i < 50 && s_task; i++)
        vTaskDelay(pdMS_TO_TICKS(20));

    ESP_LOGI(TAG, "Streaming stopped");
}

void esptari_stream_set_quality(uint8_t quality)
{
    if (quality < 1)   quality = 1;
    if (quality > 100) quality = 100;
    s_quality = quality;
    s_stats.jpeg_quality = quality;
    ESP_LOGI(TAG, "JPEG quality → %d", quality);
}

void esptari_stream_get_stats(esptari_stream_stats_t *stats)
{
    if (!stats) return;
    s_stats.clients = (uint32_t)count_ws_clients();
    *stats = s_stats;
}

void esptari_stream_deinit(void)
{
    esptari_stream_stop();

    if (s_server)
        httpd_unregister_uri(s_server, "/ws");

    if (s_video_pkt) { heap_caps_free(s_video_pkt); s_video_pkt = NULL; }
    if (s_jpeg_buf)  { free(s_jpeg_buf);             s_jpeg_buf  = NULL; }
    if (s_jpeg)      { jpeg_del_encoder_engine(s_jpeg); s_jpeg  = NULL; }

    s_server = NULL;
    ESP_LOGI(TAG, "Stream subsystem shut down");
}
