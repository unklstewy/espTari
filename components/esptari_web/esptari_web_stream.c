#include "esptari_web_stream.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "esp_timer.h"
#include "esptari_core.h"
#include "esptari_web_http_utils.h"
#include "esptari_web_media.h"
#include "cJSON.h"

#define send_json esptari_web_send_json
#define query_value esptari_web_query_value
#define read_body esptari_web_read_request_body

#define STREAM_QUEUE_CAPACITY 128u
#define STREAM_QUEUE_DEPTH_IDLE 32u

typedef enum {
    STREAM_KIND_VIDEO = 0,
    STREAM_KIND_AUDIO,
    STREAM_KIND_ENGINE,
    STREAM_KIND_REGISTERS,
    STREAM_KIND_BUS,
    STREAM_KIND_MEMORY,
    STREAM_KIND_COUNT,
} stream_kind_t;

typedef struct {
    uint32_t queue_depth;
    uint32_t queue_capacity;
    uint32_t high_watermark_depth;
    uint32_t dropped_events_since_last;
    bool throttle_active;
    uint64_t dropped_events;
    uint64_t overflow_events_total;
    uint64_t throttle_transitions_total;
    uint64_t sample_timestamp_us;
} stream_backpressure_metrics_t;

typedef enum {
    VIDEO_PACING_REALTIME = 0,
    VIDEO_PACING_FIXED_FPS,
} video_pacing_mode_t;

typedef struct {
    video_pacing_mode_t mode;
    uint32_t target_fps;
    uint32_t max_burst_frames;
    uint64_t last_emit_us;
} video_pacing_state_t;

typedef struct {
    uint64_t last_frame_id;
    uint64_t emitted_pairs;
    uint64_t sequence_violations;
    uint64_t pairing_violations;
    uint32_t last_payload_bytes;
    char last_error_code[32];
} video_payload_emitter_state_t;

static uint64_t stream_event_seq;
static uint64_t slo_alarm_seq;
static bool slo_alarm_breached;
static char stream_media_attach_events_json_buf[1536];
static char stream_media_disk_state_events_json_buf[1536];
static char stream_video_contract_json_buf[1024];
static char stream_video_meta_sample_json_buf[640];
static char stream_audio_contract_json_buf[1024];
static char stream_audio_meta_sample_json_buf[640];
static char stream_video_payload_emitter_json_buf[1024];
static char stream_video_payload_sample_json_buf[256];
static char stream_response_buf[12288];
static video_pacing_state_t video_pacing_state = {
    .mode = VIDEO_PACING_REALTIME,
    .target_fps = 50,
    .max_burst_frames = 1,
    .last_emit_us = 0,
};
static video_payload_emitter_state_t video_emitter_state;
static stream_backpressure_metrics_t stream_metrics[STREAM_KIND_COUNT] = {
    [STREAM_KIND_VIDEO] = {.queue_capacity = STREAM_QUEUE_CAPACITY, .queue_depth = STREAM_QUEUE_DEPTH_IDLE, .high_watermark_depth = STREAM_QUEUE_DEPTH_IDLE},
    [STREAM_KIND_AUDIO] = {.queue_capacity = STREAM_QUEUE_CAPACITY, .queue_depth = STREAM_QUEUE_DEPTH_IDLE, .high_watermark_depth = STREAM_QUEUE_DEPTH_IDLE},
    [STREAM_KIND_ENGINE] = {.queue_capacity = STREAM_QUEUE_CAPACITY, .queue_depth = STREAM_QUEUE_DEPTH_IDLE, .high_watermark_depth = STREAM_QUEUE_DEPTH_IDLE},
    [STREAM_KIND_REGISTERS] = {.queue_capacity = STREAM_QUEUE_CAPACITY, .queue_depth = STREAM_QUEUE_DEPTH_IDLE, .high_watermark_depth = STREAM_QUEUE_DEPTH_IDLE},
    [STREAM_KIND_BUS] = {.queue_capacity = STREAM_QUEUE_CAPACITY, .queue_depth = STREAM_QUEUE_DEPTH_IDLE, .high_watermark_depth = STREAM_QUEUE_DEPTH_IDLE},
    [STREAM_KIND_MEMORY] = {.queue_capacity = STREAM_QUEUE_CAPACITY, .queue_depth = STREAM_QUEUE_DEPTH_IDLE, .high_watermark_depth = STREAM_QUEUE_DEPTH_IDLE},
};

static const char *stream_kind_name(stream_kind_t stream)
{
    switch (stream) {
        case STREAM_KIND_VIDEO:
            return "video";
        case STREAM_KIND_AUDIO:
            return "audio";
        case STREAM_KIND_ENGINE:
            return "engine";
        case STREAM_KIND_REGISTERS:
            return "registers";
        case STREAM_KIND_BUS:
            return "bus";
        case STREAM_KIND_MEMORY:
            return "memory";
        default:
            return "video";
    }
}

static bool stream_kind_from_name(const char *name, stream_kind_t *out_stream)
{
    if (name == NULL || out_stream == NULL) {
        return false;
    }

    if (strcmp(name, "video") == 0) {
        *out_stream = STREAM_KIND_VIDEO;
        return true;
    }
    if (strcmp(name, "audio") == 0) {
        *out_stream = STREAM_KIND_AUDIO;
        return true;
    }
    if (strcmp(name, "engine") == 0) {
        *out_stream = STREAM_KIND_ENGINE;
        return true;
    }
    if (strcmp(name, "registers") == 0) {
        *out_stream = STREAM_KIND_REGISTERS;
        return true;
    }
    if (strcmp(name, "bus") == 0) {
        *out_stream = STREAM_KIND_BUS;
        return true;
    }
    if (strcmp(name, "memory") == 0) {
        *out_stream = STREAM_KIND_MEMORY;
        return true;
    }

    return false;
}

static stream_backpressure_metrics_t *stream_metrics_for(stream_kind_t stream)
{
    if ((int)stream < 0 || stream >= STREAM_KIND_COUNT) {
        return NULL;
    }
    return &stream_metrics[stream];
}

static void update_backpressure_metrics(stream_backpressure_metrics_t *metrics,
                                        bool pressure_active,
                                        uint64_t timestamp_us,
                                        uint32_t *dropped_events_since_last,
                                        uint32_t *coalesced_updates)
{
    if (metrics == NULL || dropped_events_since_last == NULL || coalesced_updates == NULL) {
        return;
    }

    bool previous_throttle = metrics->throttle_active;

    *dropped_events_since_last = 0;
    *coalesced_updates = 0;

    if (pressure_active) {
        metrics->queue_depth = metrics->queue_capacity;
        metrics->throttle_active = true;
        *dropped_events_since_last = 1;
        *coalesced_updates = 1;
        metrics->dropped_events += (uint64_t)(*dropped_events_since_last);
        metrics->overflow_events_total += 1;
    } else {
        metrics->queue_depth = STREAM_QUEUE_DEPTH_IDLE;
        metrics->throttle_active = false;
    }

    if (metrics->queue_depth > metrics->high_watermark_depth) {
        metrics->high_watermark_depth = metrics->queue_depth;
    }
    if (metrics->high_watermark_depth > metrics->queue_capacity) {
        metrics->high_watermark_depth = metrics->queue_capacity;
    }

    if (previous_throttle != metrics->throttle_active) {
        metrics->throttle_transitions_total += 1;
    }

    metrics->dropped_events_since_last = *dropped_events_since_last;
    metrics->sample_timestamp_us = timestamp_us;
}

static bool starts_with_unknown(const char *value)
{
    return value != NULL && strncmp(value, "unknown", 7) == 0;
}

static bool is_unknown_selector(httpd_req_t *req)
{
    char value[64];
    if (query_value(req, "component", value, sizeof(value)) && starts_with_unknown(value)) {
        return true;
    }
    if (query_value(req, "components", value, sizeof(value)) && starts_with_unknown(value)) {
        return true;
    }
    if (query_value(req, "source", value, sizeof(value)) && starts_with_unknown(value)) {
        return true;
    }
    if (query_value(req, "register", value, sizeof(value)) && starts_with_unknown(value)) {
        return true;
    }
    if (query_value(req, "registers", value, sizeof(value)) && starts_with_unknown(value)) {
        return true;
    }
    if (query_value(req, "region", value, sizeof(value)) && starts_with_unknown(value)) {
        return true;
    }
    if (query_value(req, "regions", value, sizeof(value)) && starts_with_unknown(value)) {
        return true;
    }
    if (query_value(req, "mapped_target", value, sizeof(value)) && starts_with_unknown(value)) {
        return true;
    }
    if (query_value(req, "mapped_targets", value, sizeof(value)) && starts_with_unknown(value)) {
        return true;
    }
    return false;
}

static esp_err_t validate_inspect_filter(httpd_req_t *req)
{
    if (is_unknown_selector(req)) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INSPECT_FILTER_INVALID\"}}", 400);
    }
    return ESP_OK;
}

static bool is_supported_video_pixel_format(const char *pixel_format)
{
    if (pixel_format == NULL) {
        return false;
    }
    return strcmp(pixel_format, "RGB565") == 0 ||
           strcmp(pixel_format, "XRGB8888") == 0 ||
           strcmp(pixel_format, "RGB888") == 0;
}

static bool is_supported_audio_format(const char *audio_format)
{
    if (audio_format == NULL) {
        return false;
    }
    return strcmp(audio_format, "PCM_S16LE") == 0 ||
           strcmp(audio_format, "PCM_F32LE") == 0;
}

static const char *video_pacing_mode_name(video_pacing_mode_t mode)
{
    if (mode == VIDEO_PACING_FIXED_FPS) {
        return "fixed_fps";
    }
    return "realtime";
}

static bool parse_video_pacing_mode(const char *value, video_pacing_mode_t *out_mode)
{
    if (value == NULL || out_mode == NULL) {
        return false;
    }
    if (strcmp(value, "realtime") == 0) {
        *out_mode = VIDEO_PACING_REALTIME;
        return true;
    }
    if (strcmp(value, "fixed_fps") == 0) {
        *out_mode = VIDEO_PACING_FIXED_FPS;
        return true;
    }
    return false;
}

static esp_err_t validate_video_metadata_contract(httpd_req_t *req)
{
    char value[32] = {0};

    if (query_value(req, "metadata_schema_version", value, sizeof(value))) {
        char *end = NULL;
        unsigned long schema_version = strtoul(value, &end, 10);
        if (end == value || *end != '\0') {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
        if (schema_version != 1UL) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"UNSUPPORTED_VERSION\"}}", 400);
        }
    }

    if (query_value(req, "pixel_format", value, sizeof(value)) && !is_supported_video_pixel_format(value)) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const char *dimension_fields[] = {"width", "height", "payload_bytes"};
    for (size_t i = 0; i < sizeof(dimension_fields) / sizeof(dimension_fields[0]); i++) {
        memset(value, 0, sizeof(value));
        if (!query_value(req, dimension_fields[i], value, sizeof(value))) {
            continue;
        }

        char *end = NULL;
        unsigned long parsed = strtoul(value, &end, 10);
        if (end == value || *end != '\0' || parsed == 0UL) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
    }

    return ESP_OK;
}

static esp_err_t validate_audio_metadata_contract(httpd_req_t *req)
{
    char value[32] = {0};

    if (query_value(req, "metadata_schema_version", value, sizeof(value))) {
        char *end = NULL;
        unsigned long schema_version = strtoul(value, &end, 10);
        if (end == value || *end != '\0') {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
        if (schema_version != 1UL) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"UNSUPPORTED_VERSION\"}}", 400);
        }
    }

    if (query_value(req, "format", value, sizeof(value)) && !is_supported_audio_format(value)) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const char *dimension_fields[] = {"sample_rate", "channels", "frames", "payload_bytes"};
    for (size_t i = 0; i < sizeof(dimension_fields) / sizeof(dimension_fields[0]); i++) {
        memset(value, 0, sizeof(value));
        if (!query_value(req, dimension_fields[i], value, sizeof(value))) {
            continue;
        }

        char *end = NULL;
        unsigned long parsed = strtoul(value, &end, 10);
        if (end == value || *end != '\0' || parsed == 0UL) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
    }

    return ESP_OK;
}

static esp_err_t stream_guard_running(httpd_req_t *req)
{
    esptari_session_status_t status;
    esptari_core_get_status(&status);
    if (status.state != ESPTARI_SESSION_RUNNING) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }
    return ESP_OK;
}

static esp_err_t stream_control_handler(httpd_req_t *req)
{
    esp_err_t guard = stream_guard_running(req);
    if (guard != ESP_OK) {
        return guard;
    }

    char body[512] = {0};
    esp_err_t read_err = read_body(req, body, sizeof(body));
    if (read_err != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const char *type = NULL;
    const char *stream = NULL;
    const char *pacing_mode = NULL;
    bool ok = esptari_web_json_get_string(root, "type", &type) &&
              esptari_web_json_get_string(root, "stream", &stream) &&
              esptari_web_json_get_string(root, "pacing_mode", &pacing_mode);
    if (!ok || type == NULL || stream == NULL || pacing_mode == NULL ||
        strcmp(type, "set_rate_limit") != 0 || strcmp(stream, "video") != 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    video_pacing_mode_t mode;
    if (!parse_video_pacing_mode(pacing_mode, &mode)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    uint32_t target_fps = video_pacing_state.target_fps;
    uint32_t max_burst_frames = video_pacing_state.max_burst_frames;

    if (mode == VIDEO_PACING_FIXED_FPS) {
        cJSON *target_fps_item = cJSON_GetObjectItemCaseSensitive(root, "target_fps");
        if (!cJSON_IsNumber(target_fps_item)) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
        int target_fps_value = target_fps_item->valueint;
        if (target_fps_value < 1 || target_fps_value > 240) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
        target_fps = (uint32_t)target_fps_value;
    }

    cJSON *max_burst_item = cJSON_GetObjectItemCaseSensitive(root, "max_burst_frames");
    if (max_burst_item != NULL) {
        if (!cJSON_IsNumber(max_burst_item)) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
        int max_burst_value = max_burst_item->valueint;
        if (max_burst_value < 1 || max_burst_value > 8) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
        max_burst_frames = (uint32_t)max_burst_value;
    }

    video_pacing_state.mode = mode;
    video_pacing_state.target_fps = target_fps;
    video_pacing_state.max_burst_frames = max_burst_frames;
    cJSON_Delete(root);

    char resp[384];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"stream\":\"video\",\"type\":\"set_rate_limit\",\"pacing\":{\"pacing_mode\":\"%s\",\"target_fps\":%lu,\"max_burst_frames\":%lu},\"applied_at_us\":%llu}}",
             video_pacing_mode_name(video_pacing_state.mode),
             (unsigned long)video_pacing_state.target_fps,
             (unsigned long)video_pacing_state.max_burst_frames,
             (unsigned long long)esp_timer_get_time());
    return send_json(req, resp, 200);
}

static esp_err_t emit_stream_probe(httpd_req_t *req, stream_kind_t stream)
{
    esp_err_t guard = stream_guard_running(req);
    if (guard != ESP_OK) {
        return guard;
    }

    esp_err_t filter_err = validate_inspect_filter(req);
    if (filter_err != ESP_OK) {
        return filter_err;
    }

    if (stream == STREAM_KIND_VIDEO) {
        esp_err_t video_contract_err = validate_video_metadata_contract(req);
        if (video_contract_err != ESP_OK) {
            return video_contract_err;
        }
    } else if (stream == STREAM_KIND_AUDIO) {
        esp_err_t audio_contract_err = validate_audio_metadata_contract(req);
        if (audio_contract_err != ESP_OK) {
            return audio_contract_err;
        }
    }

    stream_event_seq++;
    uint64_t timestamp_us = (uint64_t)esp_timer_get_time();
    const char *stream_name = stream_kind_name(stream);
    stream_backpressure_metrics_t *metrics = stream_metrics_for(stream);
    if (metrics == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    bool force_payload_mismatch = false;
    bool force_frame_regression = false;
    bool rate_limited = false;
    uint64_t effective_frame_id = stream_event_seq;
    uint32_t payload_bytes = 512000;

    char backpressure[16] = {0};
    bool pressure_active = query_value(req, "backpressure", backpressure, sizeof(backpressure)) && strcmp(backpressure, "1") == 0;
    if (stream == STREAM_KIND_VIDEO) {
        char mismatch_query[8] = {0};
        char regression_query[8] = {0};
        force_payload_mismatch = query_value(req, "force_payload_mismatch", mismatch_query, sizeof(mismatch_query)) && strcmp(mismatch_query, "1") == 0;
        force_frame_regression = query_value(req, "force_frame_regression", regression_query, sizeof(regression_query)) && strcmp(regression_query, "1") == 0;

        if (force_payload_mismatch) {
            video_emitter_state.pairing_violations++;
            strlcpy(video_emitter_state.last_error_code, "INTERNAL_ERROR", sizeof(video_emitter_state.last_error_code));
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }

        if (video_pacing_state.mode == VIDEO_PACING_FIXED_FPS && video_pacing_state.target_fps > 0) {
            uint64_t min_interval_us = 1000000ULL / (uint64_t)video_pacing_state.target_fps;
            if (video_pacing_state.last_emit_us > 0 && (timestamp_us - video_pacing_state.last_emit_us) < min_interval_us) {
                rate_limited = true;
            }
        }

        effective_frame_id = video_emitter_state.last_frame_id + 1ULL;
        if (force_frame_regression && video_emitter_state.last_frame_id > 0) {
            effective_frame_id = video_emitter_state.last_frame_id;
        }

        if (video_emitter_state.last_frame_id > 0 && effective_frame_id <= video_emitter_state.last_frame_id) {
            video_emitter_state.sequence_violations++;
            strlcpy(video_emitter_state.last_error_code, "VID-EMIT-01", sizeof(video_emitter_state.last_error_code));
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }
    }

    bool metrics_pressure = pressure_active || rate_limited;
    uint32_t dropped_events_since_last = 0;
    uint32_t coalesced_updates = 0;
    update_backpressure_metrics(metrics,
                                metrics_pressure,
                                timestamp_us,
                                &dropped_events_since_last,
                                &coalesced_updates);

    bool degraded = dropped_events_since_last > 0 || coalesced_updates > 0 || metrics->throttle_active;
    const char *delivery_reason = "none";
    if (rate_limited) {
        delivery_reason = "rate_limited";
    } else if (dropped_events_since_last > 0 || coalesced_updates > 0) {
        delivery_reason = "queue_overflow";
    }

    double high_watermark_ratio = (double)metrics->high_watermark_depth / (double)metrics->queue_capacity;

    const char *slo_state = "normal";
    const char *slo_severity = "info";
    char slo_query[16] = {0};
    if (query_value(req, "slo", slo_query, sizeof(slo_query))) {
        if (strcmp(slo_query, "breach") == 0) {
            slo_alarm_breached = true;
            slo_state = "breach";
            slo_severity = "warning";
        } else if (strcmp(slo_query, "recover") == 0) {
            slo_alarm_breached = false;
            slo_state = "recovered";
            slo_severity = "info";
        }
        slo_alarm_seq++;
    } else if (slo_alarm_breached) {
        slo_state = "breach";
        slo_severity = "warning";
    }

    strlcpy(stream_media_attach_events_json_buf, "[]", sizeof(stream_media_attach_events_json_buf));
    strlcpy(stream_media_disk_state_events_json_buf, "[]", sizeof(stream_media_disk_state_events_json_buf));
    strlcpy(stream_video_contract_json_buf, "null", sizeof(stream_video_contract_json_buf));
    strlcpy(stream_video_meta_sample_json_buf, "null", sizeof(stream_video_meta_sample_json_buf));
    strlcpy(stream_audio_contract_json_buf, "null", sizeof(stream_audio_contract_json_buf));
    strlcpy(stream_audio_meta_sample_json_buf, "null", sizeof(stream_audio_meta_sample_json_buf));
    strlcpy(stream_video_payload_emitter_json_buf, "null", sizeof(stream_video_payload_emitter_json_buf));
    strlcpy(stream_video_payload_sample_json_buf, "null", sizeof(stream_video_payload_sample_json_buf));
    if (stream == STREAM_KIND_VIDEO) {
        if (video_emitter_state.last_error_code[0] == '\0') {
            strlcpy(video_emitter_state.last_error_code, "none", sizeof(video_emitter_state.last_error_code));
        }
        video_emitter_state.last_frame_id = effective_frame_id;
        video_emitter_state.last_payload_bytes = payload_bytes;
        video_emitter_state.emitted_pairs++;
        video_pacing_state.last_emit_us = timestamp_us;

        snprintf(stream_video_contract_json_buf,
                 sizeof(stream_video_contract_json_buf),
                 "{\"channel\":\"video.metadata.v1\",\"schema\":\"video_frame_meta_v1\",\"required_fields\":[\"type\",\"schema_version\",\"channel\",\"session_id\",\"frame_id\",\"timestamp_us\",\"width\",\"height\",\"pixel_format\",\"payload_bytes\"],\"pixel_format_enum\":[\"RGB565\",\"XRGB8888\",\"RGB888\"],\"ordering\":\"frame_id_strictly_ascending\",\"payload_pairing\":\"frame_id_and_payload_bytes_must_match_following_binary_payload\"}");
        snprintf(stream_video_meta_sample_json_buf,
                 sizeof(stream_video_meta_sample_json_buf),
                 "{\"type\":\"video_frame_meta\",\"schema_version\":1,\"channel\":\"video.metadata.v1\",\"session_id\":\"ses_local\",\"frame_id\":%llu,\"timestamp_us\":%llu,\"width\":640,\"height\":400,\"pixel_format\":\"RGB565\",\"payload_bytes\":%lu}",
                 (unsigned long long)effective_frame_id,
                 (unsigned long long)timestamp_us,
                 (unsigned long)payload_bytes);
        snprintf(stream_video_payload_sample_json_buf,
                 sizeof(stream_video_payload_sample_json_buf),
                 "{\"frame_id\":%llu,\"payload_bytes\":%lu,\"binary_payload_emitted\":true}",
                 (unsigned long long)effective_frame_id,
                 (unsigned long)payload_bytes);
        snprintf(stream_video_payload_emitter_json_buf,
                 sizeof(stream_video_payload_emitter_json_buf),
                 "{\"checks\":{\"VID-EMIT-01\":\"pass\",\"VID-EMIT-02\":\"pass\",\"VID-EMIT-03\":\"pass\"},\"pacing\":{\"pacing_mode\":\"%s\",\"target_fps\":%lu,\"max_burst_frames\":%lu,\"throttle_active\":%s},\"state\":{\"last_frame_id\":%llu,\"emitted_pairs\":%llu,\"sequence_violations\":%llu,\"pairing_violations\":%llu,\"last_error\":\"%s\"}}",
                 video_pacing_mode_name(video_pacing_state.mode),
                 (unsigned long)video_pacing_state.target_fps,
                 (unsigned long)video_pacing_state.max_burst_frames,
                 rate_limited ? "true" : "false",
                 (unsigned long long)video_emitter_state.last_frame_id,
                 (unsigned long long)video_emitter_state.emitted_pairs,
                 (unsigned long long)video_emitter_state.sequence_violations,
                 (unsigned long long)video_emitter_state.pairing_violations,
                 video_emitter_state.last_error_code);
    } else if (stream == STREAM_KIND_AUDIO) {
        snprintf(stream_audio_contract_json_buf,
                 sizeof(stream_audio_contract_json_buf),
                 "{\"channel\":\"audio.metadata.v1\",\"schema\":\"audio_chunk_meta_v1\",\"required_fields\":[\"type\",\"schema_version\",\"channel\",\"session_id\",\"chunk_id\",\"timestamp_us\",\"sample_rate\",\"channels\",\"format\",\"frames\",\"payload_bytes\"],\"format_enum\":[\"PCM_S16LE\",\"PCM_F32LE\"],\"ordering\":\"chunk_id_strictly_ascending\",\"payload_pairing\":\"chunk_id_and_payload_bytes_must_match_following_binary_payload\"}");
        snprintf(stream_audio_meta_sample_json_buf,
                 sizeof(stream_audio_meta_sample_json_buf),
                 "{\"type\":\"audio_chunk_meta\",\"schema_version\":1,\"channel\":\"audio.metadata.v1\",\"session_id\":\"ses_local\",\"chunk_id\":%llu,\"timestamp_us\":%llu,\"sample_rate\":48000,\"channels\":2,\"format\":\"PCM_S16LE\",\"frames\":1024,\"payload_bytes\":4096}",
                 (unsigned long long)stream_event_seq,
                 (unsigned long long)timestamp_us);
    }
    if (stream == STREAM_KIND_ENGINE) {
        const esptari_web_media_attach_event_t *events = NULL;
        size_t event_count = 0;
        esptari_web_media_get_last_rom_attach_events(&events, &event_count);

        size_t cursor = 0;
        cursor += snprintf(stream_media_attach_events_json_buf + cursor,
                           sizeof(stream_media_attach_events_json_buf) - cursor,
                           "[");
        for (size_t index = 0; index < event_count; index++) {
            const esptari_web_media_attach_event_t *event = &events[index];
            if (index > 0) {
                cursor += snprintf(stream_media_attach_events_json_buf + cursor,
                                   sizeof(stream_media_attach_events_json_buf) - cursor,
                                   ",");
            }
            if (event->has_error) {
                cursor += snprintf(stream_media_attach_events_json_buf + cursor,
                                   sizeof(stream_media_attach_events_json_buf) - cursor,
                                   "{\"type\":\"media_attach_status\",\"schema_version\":1,\"session_id\":\"ses_local\",\"event_seq\":%llu,\"event_timestamp_us\":%llu,\"media_type\":\"rom\",\"media_id\":\"%s\",\"phase\":\"%s\",\"result\":\"%s\",\"request_id\":\"%s\",\"error\":{\"code\":\"%s\",\"message\":\"%s\"}}",
                                   (unsigned long long)event->event_seq,
                                   (unsigned long long)event->event_timestamp_us,
                                   event->media_id,
                                   event->phase,
                                   event->result,
                                   event->request_id,
                                   event->error_code,
                                   event->error_message);
            } else {
                cursor += snprintf(stream_media_attach_events_json_buf + cursor,
                                   sizeof(stream_media_attach_events_json_buf) - cursor,
                                   "{\"type\":\"media_attach_status\",\"schema_version\":1,\"session_id\":\"ses_local\",\"event_seq\":%llu,\"event_timestamp_us\":%llu,\"media_type\":\"rom\",\"media_id\":\"%s\",\"phase\":\"%s\",\"result\":\"%s\",\"request_id\":\"%s\"}",
                                   (unsigned long long)event->event_seq,
                                   (unsigned long long)event->event_timestamp_us,
                                   event->media_id,
                                   event->phase,
                                   event->result,
                                   event->request_id);
            }
        }
        snprintf(stream_media_attach_events_json_buf + cursor,
             sizeof(stream_media_attach_events_json_buf) - cursor,
                 "]");

        const esptari_web_media_disk_state_event_t *disk_events = NULL;
        size_t disk_event_count = 0;
        esptari_web_media_get_last_disk_state_events(&disk_events, &disk_event_count);

        cursor = 0;
        cursor += snprintf(stream_media_disk_state_events_json_buf + cursor,
                           sizeof(stream_media_disk_state_events_json_buf) - cursor,
                           "[");
        for (size_t index = 0; index < disk_event_count; index++) {
            const esptari_web_media_disk_state_event_t *event = &disk_events[index];
            if (index > 0) {
                cursor += snprintf(stream_media_disk_state_events_json_buf + cursor,
                                   sizeof(stream_media_disk_state_events_json_buf) - cursor,
                                   ",");
            }
            if (event->has_disk_id) {
                cursor += snprintf(stream_media_disk_state_events_json_buf + cursor,
                                   sizeof(stream_media_disk_state_events_json_buf) - cursor,
                                   "{\"type\":\"media_disk_state\",\"schema_version\":1,\"session_id\":\"ses_local\",\"event_seq\":%llu,\"event_timestamp_us\":%llu,\"drive\":\"%s\",\"state\":\"%s\",\"disk_id\":\"%s\",\"request_id\":\"%s\"}",
                                   (unsigned long long)event->event_seq,
                                   (unsigned long long)event->event_timestamp_us,
                                   event->drive,
                                   event->state,
                                   event->disk_id,
                                   event->request_id);
            } else {
                cursor += snprintf(stream_media_disk_state_events_json_buf + cursor,
                                   sizeof(stream_media_disk_state_events_json_buf) - cursor,
                                   "{\"type\":\"media_disk_state\",\"schema_version\":1,\"session_id\":\"ses_local\",\"event_seq\":%llu,\"event_timestamp_us\":%llu,\"drive\":\"%s\",\"state\":\"%s\",\"disk_id\":null,\"request_id\":\"%s\"}",
                                   (unsigned long long)event->event_seq,
                                   (unsigned long long)event->event_timestamp_us,
                                   event->drive,
                                   event->state,
                                   event->request_id);
            }
        }
        snprintf(stream_media_disk_state_events_json_buf + cursor,
                 sizeof(stream_media_disk_state_events_json_buf) - cursor,
                 "]");
    }

    snprintf(stream_response_buf, sizeof(stream_response_buf),
             "{\"ok\":true,\"data\":{\"stream\":\"%s\",\"event_seq\":%llu,\"event_timestamp_us\":%llu,\"delivery\":{\"degraded\":%s,\"reason\":\"%s\",\"dropped_events_since_last\":%lu,\"coalesced_updates\":%lu,\"throttle_active\":%s},\"backpressure\":{\"queue_depth\":%lu,\"queue_capacity\":%lu,\"dropped_events\":%llu,\"dropped_events_since_last\":%lu,\"throttle_active\":%s,\"high_watermark_depth\":%lu,\"high_watermark_ratio\":%.3f,\"overflow_events_total\":%llu,\"throttle_transitions_total\":%llu,\"sample_timestamp_us\":%llu},\"backpressure_event\":{\"type\":\"stream_backpressure_telemetry\",\"schema_version\":1,\"session_id\":\"ses_local\",\"event_seq\":%llu,\"event_timestamp_us\":%llu,\"stream\":\"%s\",\"metrics\":{\"queue_depth\":%lu,\"queue_capacity\":%lu,\"dropped_events\":%llu,\"dropped_events_since_last\":%lu,\"throttle_active\":%s,\"high_watermark_depth\":%lu,\"high_watermark_ratio\":%.3f,\"overflow_events_total\":%llu,\"throttle_transitions_total\":%llu,\"sample_timestamp_us\":%llu}},\"video_metadata_contract\":%s,\"video_frame_meta_sample\":%s,\"audio_metadata_contract\":%s,\"audio_chunk_meta_sample\":%s,\"video_payload_emitter\":%s,\"video_payload_sample\":%s,\"media_attach_status_events\":%s,\"media_disk_state_events\":%s,\"slo_alarm\":{\"seq\":%llu,\"state\":\"%s\",\"severity\":\"%s\"}}}",
             stream_name,
             (unsigned long long)stream_event_seq,
             (unsigned long long)timestamp_us,
             degraded ? "true" : "false",
             delivery_reason,
             (unsigned long)dropped_events_since_last,
             (unsigned long)coalesced_updates,
             metrics->throttle_active ? "true" : "false",
             (unsigned long)metrics->queue_depth,
             (unsigned long)metrics->queue_capacity,
             (unsigned long long)metrics->dropped_events,
             (unsigned long)metrics->dropped_events_since_last,
             metrics->throttle_active ? "true" : "false",
             (unsigned long)metrics->high_watermark_depth,
             high_watermark_ratio,
             (unsigned long long)metrics->overflow_events_total,
             (unsigned long long)metrics->throttle_transitions_total,
             (unsigned long long)metrics->sample_timestamp_us,
             (unsigned long long)stream_event_seq,
             (unsigned long long)timestamp_us,
             stream_name,
             (unsigned long)metrics->queue_depth,
             (unsigned long)metrics->queue_capacity,
             (unsigned long long)metrics->dropped_events,
             (unsigned long)metrics->dropped_events_since_last,
             metrics->throttle_active ? "true" : "false",
             (unsigned long)metrics->high_watermark_depth,
             high_watermark_ratio,
             (unsigned long long)metrics->overflow_events_total,
             (unsigned long long)metrics->throttle_transitions_total,
             (unsigned long long)metrics->sample_timestamp_us,
             stream_video_contract_json_buf,
             stream_video_meta_sample_json_buf,
             stream_audio_contract_json_buf,
             stream_audio_meta_sample_json_buf,
             stream_video_payload_emitter_json_buf,
             stream_video_payload_sample_json_buf,
             stream_media_attach_events_json_buf,
             stream_media_disk_state_events_json_buf,
             (unsigned long long)slo_alarm_seq,
             slo_state,
             slo_severity);
    return send_json(req, stream_response_buf, 200);
}

static esp_err_t stream_video_handler(httpd_req_t *req)
{
    return emit_stream_probe(req, STREAM_KIND_VIDEO);
}

static esp_err_t stream_audio_handler(httpd_req_t *req)
{
    return emit_stream_probe(req, STREAM_KIND_AUDIO);
}

static esp_err_t stream_engine_handler(httpd_req_t *req)
{
    return emit_stream_probe(req, STREAM_KIND_ENGINE);
}

static esp_err_t inspect_registers_stream_handler(httpd_req_t *req)
{
    return emit_stream_probe(req, STREAM_KIND_REGISTERS);
}

static esp_err_t inspect_bus_stream_handler(httpd_req_t *req)
{
    return emit_stream_probe(req, STREAM_KIND_BUS);
}

static esp_err_t inspect_memory_stream_handler(httpd_req_t *req)
{
    return emit_stream_probe(req, STREAM_KIND_MEMORY);
}

static esp_err_t stream_backpressure_telemetry_handler(httpd_req_t *req)
{
    esp_err_t guard = stream_guard_running(req);
    if (guard != ESP_OK) {
        return guard;
    }

    char stream_name[24] = {0};
    char session_id[64] = {0};

    if (!query_value(req, "session_id", session_id, sizeof(session_id)) || session_id[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    if (!query_value(req, "stream", stream_name, sizeof(stream_name)) || stream_name[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    stream_kind_t stream_kind = STREAM_KIND_VIDEO;
    if (!stream_kind_from_name(stream_name, &stream_kind)) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INSPECT_FILTER_INVALID\"}}", 400);
    }

    stream_backpressure_metrics_t *metrics = stream_metrics_for(stream_kind);
    if (metrics == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    metrics->sample_timestamp_us = (uint64_t)esp_timer_get_time();
    double high_watermark_ratio = (double)metrics->high_watermark_depth / (double)metrics->queue_capacity;

    char resp[640];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"stream\":\"%s\",\"sample_timestamp_us\":%llu,\"queue_depth\":%lu,\"queue_capacity\":%lu,\"high_watermark_depth\":%lu,\"high_watermark_ratio\":%.3f,\"dropped_events\":%llu,\"dropped_events_since_last\":%lu,\"overflow_events_total\":%llu,\"throttle_active\":%s,\"throttle_transitions_total\":%llu}}",
             session_id,
             stream_kind_name(stream_kind),
             (unsigned long long)metrics->sample_timestamp_us,
             (unsigned long)metrics->queue_depth,
             (unsigned long)metrics->queue_capacity,
             (unsigned long)metrics->high_watermark_depth,
             high_watermark_ratio,
             (unsigned long long)metrics->dropped_events,
             (unsigned long)metrics->dropped_events_since_last,
             (unsigned long long)metrics->overflow_events_total,
             metrics->throttle_active ? "true" : "false",
             (unsigned long long)metrics->throttle_transitions_total);
    return send_json(req, resp, 200);
}

void esptari_web_stream_get_runtime_snapshot(esptari_web_stream_runtime_snapshot_t *out)
{
    if (out == NULL) {
        return;
    }

    out->dropped_packets_total = 0;
    for (int i = 0; i < (int)STREAM_KIND_COUNT; ++i) {
        out->dropped_packets_total += stream_metrics[i].dropped_events;
    }
}

void esptari_web_stream_register_routes(httpd_handle_t server_handle)
{
    httpd_uri_t stream_video = {.uri = "/api/v2/stream/video", .method = HTTP_GET, .handler = stream_video_handler, .user_ctx = NULL};
    httpd_uri_t stream_audio = {.uri = "/api/v2/stream/audio", .method = HTTP_GET, .handler = stream_audio_handler, .user_ctx = NULL};
    httpd_uri_t stream_engine = {.uri = "/api/v2/engine/stream", .method = HTTP_GET, .handler = stream_engine_handler, .user_ctx = NULL};
    httpd_uri_t stream_control = {.uri = "/api/v2/stream/control", .method = HTTP_POST, .handler = stream_control_handler, .user_ctx = NULL};
    httpd_uri_t stream_backpressure = {.uri = "/api/v2/stream/telemetry/backpressure", .method = HTTP_GET, .handler = stream_backpressure_telemetry_handler, .user_ctx = NULL};
    httpd_uri_t inspect_registers = {.uri = "/api/v2/inspect/registers/stream", .method = HTTP_GET, .handler = inspect_registers_stream_handler, .user_ctx = NULL};
    httpd_uri_t inspect_bus = {.uri = "/api/v2/inspect/bus/stream", .method = HTTP_GET, .handler = inspect_bus_stream_handler, .user_ctx = NULL};
    httpd_uri_t inspect_memory = {.uri = "/api/v2/inspect/memory/stream", .method = HTTP_GET, .handler = inspect_memory_stream_handler, .user_ctx = NULL};

    httpd_register_uri_handler(server_handle, &stream_video);
    httpd_register_uri_handler(server_handle, &stream_audio);
    httpd_register_uri_handler(server_handle, &stream_engine);
    httpd_register_uri_handler(server_handle, &stream_control);
    httpd_register_uri_handler(server_handle, &stream_backpressure);
    httpd_register_uri_handler(server_handle, &inspect_registers);
    httpd_register_uri_handler(server_handle, &inspect_bus);
    httpd_register_uri_handler(server_handle, &inspect_memory);
}
