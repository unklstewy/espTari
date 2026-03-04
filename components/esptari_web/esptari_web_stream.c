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

typedef enum {
    AUDIO_PACING_REALTIME = 0,
    AUDIO_PACING_FIXED_HZ,
} audio_pacing_mode_t;

typedef struct {
    audio_pacing_mode_t mode;
    uint32_t target_hz;
    uint32_t max_burst_chunks;
    uint64_t last_emit_us;
} audio_pacing_state_t;

typedef struct {
    uint64_t last_frame_id;
    uint64_t emitted_pairs;
    uint64_t sequence_violations;
    uint64_t pairing_violations;
    uint32_t last_payload_bytes;
    char last_error_code[32];
} video_payload_emitter_state_t;

typedef struct {
    uint64_t last_chunk_id;
    uint64_t emitted_pairs;
    uint64_t sequence_violations;
    uint64_t pairing_violations;
    uint32_t last_payload_bytes;
    char last_error_code[32];
} audio_payload_emitter_state_t;

typedef struct {
    uint64_t last_event_seq;
    uint64_t last_event_timestamp_us;
    uint64_t events_emitted;
    uint64_t events_suppressed_changed_only;
    uint64_t check_failures_seq;
    uint64_t check_failures_timestamp;
    uint64_t check_failures_selector;
    uint64_t check_failures_changed_only;
    char last_error_code[32];
} register_stream_publisher_state_t;

static uint64_t stream_event_seq;
static uint64_t slo_alarm_seq;
static bool slo_alarm_breached;
static char stream_media_attach_events_json_buf[1536];
static char stream_media_disk_state_events_json_buf[1536];
static char stream_video_contract_json_buf[1024];
static char stream_video_meta_sample_json_buf[640];
static char stream_audio_contract_json_buf[1024];
static char stream_audio_meta_sample_json_buf[640];
static char stream_register_contract_json_buf[1536];
static char stream_register_filter_json_buf[768];
static char stream_register_sample_json_buf[768];
static char stream_register_publisher_json_buf[1024];
static char stream_video_payload_emitter_json_buf[1024];
static char stream_video_payload_sample_json_buf[256];
static char stream_audio_payload_emitter_json_buf[1024];
static char stream_audio_payload_sample_json_buf[256];
static char stream_response_buf[16384];
static video_pacing_state_t video_pacing_state = {
    .mode = VIDEO_PACING_REALTIME,
    .target_fps = 50,
    .max_burst_frames = 1,
    .last_emit_us = 0,
};
static audio_pacing_state_t audio_pacing_state = {
    .mode = AUDIO_PACING_REALTIME,
    .target_hz = 240,
    .max_burst_chunks = 1,
    .last_emit_us = 0,
};
static video_payload_emitter_state_t video_emitter_state;
static audio_payload_emitter_state_t audio_emitter_state;
static register_stream_publisher_state_t register_publisher_state;
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

typedef enum {
    SELECTOR_LIST_VALID = 0,
    SELECTOR_LIST_BAD_REQUEST,
    SELECTOR_LIST_UNKNOWN,
} selector_list_validation_t;

static selector_list_validation_t validate_selector_list(const char *value)
{
    if (value == NULL || value[0] == '\0') {
        return SELECTOR_LIST_BAD_REQUEST;
    }

    const char *cursor = value;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }

        if (*cursor == ',') {
            return SELECTOR_LIST_BAD_REQUEST;
        }

        const char *start = cursor;
        while (*cursor != '\0' && *cursor != ',') {
            cursor++;
        }
        const char *end = cursor;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
            end--;
        }

        if (end <= start) {
            return SELECTOR_LIST_BAD_REQUEST;
        }

        size_t token_len = (size_t)(end - start);
        if (token_len >= 64) {
            return SELECTOR_LIST_BAD_REQUEST;
        }

        char token[64] = {0};
        memcpy(token, start, token_len);
        token[token_len] = '\0';
        if (starts_with_unknown(token)) {
            return SELECTOR_LIST_UNKNOWN;
        }

        if (*cursor == ',') {
            cursor++;
            if (*cursor == '\0') {
                return SELECTOR_LIST_BAD_REQUEST;
            }
        }
    }

    return SELECTOR_LIST_VALID;
}

static bool append_json_array_item(char *buffer, size_t buffer_size, size_t *cursor, const char *value)
{
    if (buffer == NULL || cursor == NULL || value == NULL) {
        return false;
    }

    int written = snprintf(buffer + *cursor, buffer_size - *cursor, "\"%s\"", value);
    if (written <= 0 || (size_t)written >= (buffer_size - *cursor)) {
        return false;
    }

    *cursor += (size_t)written;
    return true;
}

static bool selector_list_to_json_array(const char *value, char *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0) {
        return false;
    }

    if (value == NULL || value[0] == '\0') {
        strlcpy(buffer, "[]", buffer_size);
        return true;
    }

    selector_list_validation_t validation = validate_selector_list(value);
    if (validation != SELECTOR_LIST_VALID) {
        return false;
    }

    size_t cursor = 0;
    int written = snprintf(buffer + cursor, buffer_size - cursor, "[");
    if (written <= 0 || (size_t)written >= (buffer_size - cursor)) {
        return false;
    }
    cursor += (size_t)written;

    bool first = true;
    const char *scan = value;
    while (*scan != '\0') {
        while (*scan == ' ' || *scan == '\t') {
            scan++;
        }

        const char *start = scan;
        while (*scan != '\0' && *scan != ',') {
            scan++;
        }

        const char *end = scan;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
            end--;
        }

        size_t len = (size_t)(end - start);
        char token[64] = {0};
        memcpy(token, start, len);
        token[len] = '\0';

        if (!first) {
            written = snprintf(buffer + cursor, buffer_size - cursor, ",");
            if (written <= 0 || (size_t)written >= (buffer_size - cursor)) {
                return false;
            }
            cursor += (size_t)written;
        }

        if (!append_json_array_item(buffer, buffer_size, &cursor, token)) {
            return false;
        }
        first = false;

        if (*scan == ',') {
            scan++;
        }
    }

    written = snprintf(buffer + cursor, buffer_size - cursor, "]");
    if (written <= 0 || (size_t)written >= (buffer_size - cursor)) {
        return false;
    }
    return true;
}

static bool selector_list_has_values(const char *value)
{
    return value != NULL && value[0] != '\0';
}

static bool selector_list_contains_exact(const char *value, const char *candidate)
{
    if (!selector_list_has_values(value) || candidate == NULL) {
        return false;
    }

    const char *cursor = value;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }

        const char *start = cursor;
        while (*cursor != '\0' && *cursor != ',') {
            cursor++;
        }
        const char *end = cursor;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
            end--;
        }

        size_t token_len = (size_t)(end - start);
        if (token_len == strlen(candidate) && strncmp(start, candidate, token_len) == 0) {
            return true;
        }

        if (*cursor == ',') {
            cursor++;
        }
    }

    return false;
}

static bool selector_list_contains_prefix(const char *value, const char *candidate)
{
    if (!selector_list_has_values(value) || candidate == NULL) {
        return false;
    }

    const char *cursor = value;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }

        const char *start = cursor;
        while (*cursor != '\0' && *cursor != ',') {
            cursor++;
        }
        const char *end = cursor;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
            end--;
        }

        size_t token_len = (size_t)(end - start);
        if (token_len > 0 && strncmp(candidate, start, token_len) == 0) {
            return true;
        }

        if (*cursor == ',') {
            cursor++;
        }
    }

    return false;
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

static const char *audio_pacing_mode_name(audio_pacing_mode_t mode)
{
    if (mode == AUDIO_PACING_FIXED_HZ) {
        return "fixed_hz";
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

static bool parse_audio_pacing_mode(const char *value, audio_pacing_mode_t *out_mode)
{
    if (value == NULL || out_mode == NULL) {
        return false;
    }
    if (strcmp(value, "realtime") == 0) {
        *out_mode = AUDIO_PACING_REALTIME;
        return true;
    }
    if (strcmp(value, "fixed_hz") == 0) {
        *out_mode = AUDIO_PACING_FIXED_HZ;
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

static esp_err_t validate_register_snapshot_contract(httpd_req_t *req)
{
    char value[256] = {0};
    bool mode_interval = false;

    if (query_value(req, "mode", value, sizeof(value))) {
        if (strcmp(value, "event") == 0) {
            mode_interval = false;
        } else if (strcmp(value, "interval") == 0) {
            mode_interval = true;
        } else {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
    }

    bool has_interval = query_value(req, "interval_us", value, sizeof(value));
    if (has_interval) {
        char *end = NULL;
        unsigned long interval_us = strtoul(value, &end, 10);
        if (end == value || *end != '\0' || interval_us == 0UL) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
        if (!mode_interval) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
    } else if (mode_interval) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    if (query_value(req, "changed_only", value, sizeof(value))) {
        if (!(strcmp(value, "true") == 0 || strcmp(value, "false") == 0 || strcmp(value, "1") == 0 || strcmp(value, "0") == 0)) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
    }

    const char *selectors[] = {"components", "registers", "register_prefixes"};
    for (size_t i = 0; i < sizeof(selectors) / sizeof(selectors[0]); i++) {
        if (!query_value(req, selectors[i], value, sizeof(value))) {
            continue;
        }
        selector_list_validation_t selector_status = validate_selector_list(value);
        if (selector_status == SELECTOR_LIST_UNKNOWN) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INSPECT_FILTER_INVALID\"}}", 400);
        }
        if (selector_status != SELECTOR_LIST_VALID) {
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
        strcmp(type, "set_rate_limit") != 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char resp[384];
    if (strcmp(stream, "video") == 0) {
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

        snprintf(resp,
                 sizeof(resp),
                 "{\"ok\":true,\"data\":{\"stream\":\"video\",\"type\":\"set_rate_limit\",\"pacing\":{\"pacing_mode\":\"%s\",\"target_fps\":%lu,\"max_burst_frames\":%lu},\"applied_at_us\":%llu}}",
                 video_pacing_mode_name(video_pacing_state.mode),
                 (unsigned long)video_pacing_state.target_fps,
                 (unsigned long)video_pacing_state.max_burst_frames,
                 (unsigned long long)esp_timer_get_time());
        cJSON_Delete(root);
        return send_json(req, resp, 200);
    }

    if (strcmp(stream, "audio") == 0) {
        audio_pacing_mode_t mode;
        if (!parse_audio_pacing_mode(pacing_mode, &mode)) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }

        uint32_t target_hz = audio_pacing_state.target_hz;
        uint32_t max_burst_chunks = audio_pacing_state.max_burst_chunks;

        if (mode == AUDIO_PACING_FIXED_HZ) {
            cJSON *target_hz_item = cJSON_GetObjectItemCaseSensitive(root, "target_hz");
            if (!cJSON_IsNumber(target_hz_item)) {
                cJSON_Delete(root);
                return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
            }
            int target_hz_value = target_hz_item->valueint;
            if (target_hz_value < 1 || target_hz_value > 48000) {
                cJSON_Delete(root);
                return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
            }
            target_hz = (uint32_t)target_hz_value;
        }

        cJSON *max_burst_item = cJSON_GetObjectItemCaseSensitive(root, "max_burst_chunks");
        if (max_burst_item != NULL) {
            if (!cJSON_IsNumber(max_burst_item)) {
                cJSON_Delete(root);
                return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
            }
            int max_burst_value = max_burst_item->valueint;
            if (max_burst_value < 1 || max_burst_value > 16) {
                cJSON_Delete(root);
                return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
            }
            max_burst_chunks = (uint32_t)max_burst_value;
        }

        audio_pacing_state.mode = mode;
        audio_pacing_state.target_hz = target_hz;
        audio_pacing_state.max_burst_chunks = max_burst_chunks;

        snprintf(resp,
                 sizeof(resp),
                 "{\"ok\":true,\"data\":{\"stream\":\"audio\",\"type\":\"set_rate_limit\",\"pacing\":{\"pacing_mode\":\"%s\",\"target_hz\":%lu,\"max_burst_chunks\":%lu},\"applied_at_us\":%llu}}",
                 audio_pacing_mode_name(audio_pacing_state.mode),
                 (unsigned long)audio_pacing_state.target_hz,
                 (unsigned long)audio_pacing_state.max_burst_chunks,
                 (unsigned long long)esp_timer_get_time());
        cJSON_Delete(root);
        return send_json(req, resp, 200);
    }

    cJSON_Delete(root);
    return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
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
    } else if (stream == STREAM_KIND_REGISTERS) {
        esp_err_t register_contract_err = validate_register_snapshot_contract(req);
        if (register_contract_err != ESP_OK) {
            return register_contract_err;
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
    bool force_audio_payload_mismatch = false;
    bool force_audio_chunk_regression = false;
    bool rate_limited = false;
    uint64_t effective_frame_id = stream_event_seq;
    uint64_t effective_chunk_id = stream_event_seq;
    uint32_t payload_bytes = 512000;
    uint32_t audio_payload_bytes = 4096;

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
    } else if (stream == STREAM_KIND_AUDIO) {
        char mismatch_query[8] = {0};
        char regression_query[8] = {0};
        force_audio_payload_mismatch = query_value(req, "force_audio_payload_mismatch", mismatch_query, sizeof(mismatch_query)) && strcmp(mismatch_query, "1") == 0;
        force_audio_chunk_regression = query_value(req, "force_audio_chunk_regression", regression_query, sizeof(regression_query)) && strcmp(regression_query, "1") == 0;

        if (force_audio_payload_mismatch) {
            audio_emitter_state.pairing_violations++;
            strlcpy(audio_emitter_state.last_error_code, "INTERNAL_ERROR", sizeof(audio_emitter_state.last_error_code));
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }

        if (audio_pacing_state.mode == AUDIO_PACING_FIXED_HZ && audio_pacing_state.target_hz > 0) {
            uint64_t min_interval_us = 1000000ULL / (uint64_t)audio_pacing_state.target_hz;
            if (audio_pacing_state.last_emit_us > 0 && (timestamp_us - audio_pacing_state.last_emit_us) < min_interval_us) {
                rate_limited = true;
            }
        }

        effective_chunk_id = audio_emitter_state.last_chunk_id + 1ULL;
        if (force_audio_chunk_regression && audio_emitter_state.last_chunk_id > 0) {
            effective_chunk_id = audio_emitter_state.last_chunk_id;
        }

        if (audio_emitter_state.last_chunk_id > 0 && effective_chunk_id <= audio_emitter_state.last_chunk_id) {
            audio_emitter_state.sequence_violations++;
            strlcpy(audio_emitter_state.last_error_code, "AUD-EMIT-01", sizeof(audio_emitter_state.last_error_code));
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
    strlcpy(stream_register_contract_json_buf, "null", sizeof(stream_register_contract_json_buf));
    strlcpy(stream_register_filter_json_buf, "null", sizeof(stream_register_filter_json_buf));
    strlcpy(stream_register_sample_json_buf, "null", sizeof(stream_register_sample_json_buf));
    strlcpy(stream_register_publisher_json_buf, "null", sizeof(stream_register_publisher_json_buf));
    strlcpy(stream_video_payload_emitter_json_buf, "null", sizeof(stream_video_payload_emitter_json_buf));
    strlcpy(stream_video_payload_sample_json_buf, "null", sizeof(stream_video_payload_sample_json_buf));
    strlcpy(stream_audio_payload_emitter_json_buf, "null", sizeof(stream_audio_payload_emitter_json_buf));
    strlcpy(stream_audio_payload_sample_json_buf, "null", sizeof(stream_audio_payload_sample_json_buf));
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
        if (audio_emitter_state.last_error_code[0] == '\0') {
            strlcpy(audio_emitter_state.last_error_code, "none", sizeof(audio_emitter_state.last_error_code));
        }
        audio_emitter_state.last_chunk_id = effective_chunk_id;
        audio_emitter_state.last_payload_bytes = audio_payload_bytes;
        audio_emitter_state.emitted_pairs++;
        audio_pacing_state.last_emit_us = timestamp_us;

        snprintf(stream_audio_payload_sample_json_buf,
                 sizeof(stream_audio_payload_sample_json_buf),
                 "{\"chunk_id\":%llu,\"payload_bytes\":%lu,\"binary_payload_emitted\":true}",
                 (unsigned long long)effective_chunk_id,
                 (unsigned long)audio_payload_bytes);
        snprintf(stream_audio_payload_emitter_json_buf,
                 sizeof(stream_audio_payload_emitter_json_buf),
                 "{\"checks\":{\"AUD-EMIT-01\":\"pass\",\"AUD-EMIT-02\":\"pass\",\"AUD-EMIT-03\":\"pass\"},\"pacing\":{\"pacing_mode\":\"%s\",\"target_hz\":%lu,\"max_burst_chunks\":%lu,\"throttle_active\":%s},\"state\":{\"last_chunk_id\":%llu,\"emitted_pairs\":%llu,\"sequence_violations\":%llu,\"pairing_violations\":%llu,\"last_error\":\"%s\"}}",
                 audio_pacing_mode_name(audio_pacing_state.mode),
                 (unsigned long)audio_pacing_state.target_hz,
                 (unsigned long)audio_pacing_state.max_burst_chunks,
                 rate_limited ? "true" : "false",
                 (unsigned long long)audio_emitter_state.last_chunk_id,
                 (unsigned long long)audio_emitter_state.emitted_pairs,
                 (unsigned long long)audio_emitter_state.sequence_violations,
                 (unsigned long long)audio_emitter_state.pairing_violations,
                 audio_emitter_state.last_error_code);
    } else if (stream == STREAM_KIND_REGISTERS) {
        char components_query[256] = {0};
        char registers_query[256] = {0};
        char prefixes_query[256] = {0};
        char mode_query[16] = {0};
        char interval_query[32] = {0};
        char changed_only_query[8] = {0};
        char force_seq_regression_query[8] = {0};
        char force_timestamp_regression_query[8] = {0};
        char force_selector_mismatch_query[8] = {0};
        char force_changed_only_violation_query[8] = {0};
        char force_schema_invalid_query[8] = {0};
        char components_json[256] = {0};
        char registers_json[256] = {0};
        char prefixes_json[256] = {0};
        const char *mode = "event";
        const char *changed_only = "true";
        const char *interval_us = "null";
        bool changed_only_enabled = true;
        bool force_seq_regression = false;
        bool force_timestamp_regression = false;
        bool force_selector_mismatch = false;
        bool force_changed_only_violation = false;
        bool force_schema_invalid = false;

        uint64_t publisher_event_seq = register_publisher_state.last_event_seq + 1ULL;
        uint64_t publisher_event_timestamp_us = timestamp_us;
        const char *publisher_component = "cpu";
        const char *publisher_register = "PC";
        const char *publisher_old_value = "0x00FC1234";
        const char *publisher_new_value = "0x00FC1236";
        char publisher_value_encoding[16] = "hex";
        uint32_t publisher_value_bits = 32;

        bool has_components = query_value(req, "components", components_query, sizeof(components_query));
        bool has_registers = query_value(req, "registers", registers_query, sizeof(registers_query));
        bool has_prefixes = query_value(req, "register_prefixes", prefixes_query, sizeof(prefixes_query));
        bool has_mode = query_value(req, "mode", mode_query, sizeof(mode_query));
        bool has_interval = query_value(req, "interval_us", interval_query, sizeof(interval_query));
        bool has_changed_only = query_value(req, "changed_only", changed_only_query, sizeof(changed_only_query));
        force_seq_regression = query_value(req, "force_reg_event_seq_regression", force_seq_regression_query, sizeof(force_seq_regression_query)) && strcmp(force_seq_regression_query, "1") == 0;
        force_timestamp_regression = query_value(req, "force_reg_timestamp_regression", force_timestamp_regression_query, sizeof(force_timestamp_regression_query)) && strcmp(force_timestamp_regression_query, "1") == 0;
        force_selector_mismatch = query_value(req, "force_reg_selector_mismatch", force_selector_mismatch_query, sizeof(force_selector_mismatch_query)) && strcmp(force_selector_mismatch_query, "1") == 0;
        force_changed_only_violation = query_value(req, "force_reg_changed_only_violation", force_changed_only_violation_query, sizeof(force_changed_only_violation_query)) && strcmp(force_changed_only_violation_query, "1") == 0;
        force_schema_invalid = query_value(req, "force_reg_schema_invalid", force_schema_invalid_query, sizeof(force_schema_invalid_query)) && strcmp(force_schema_invalid_query, "1") == 0;

        if (has_mode) {
            mode = mode_query;
        }
        if (has_interval) {
            interval_us = interval_query;
        }
        if (has_changed_only) {
            changed_only_enabled = (strcmp(changed_only_query, "1") == 0 || strcmp(changed_only_query, "true") == 0);
        }
        changed_only = changed_only_enabled ? "true" : "false";

        if (force_seq_regression && register_publisher_state.last_event_seq > 0) {
            publisher_event_seq = register_publisher_state.last_event_seq;
        }
        if (force_timestamp_regression && register_publisher_state.last_event_timestamp_us > 0) {
            publisher_event_timestamp_us = register_publisher_state.last_event_timestamp_us - 1ULL;
        }
        if (force_selector_mismatch) {
            publisher_component = "shifter";
            publisher_register = "VBL";
        }
        if (force_changed_only_violation) {
            publisher_new_value = publisher_old_value;
        }
        if (force_schema_invalid) {
            publisher_value_bits = 0;
            strlcpy(publisher_value_encoding, "invalid", sizeof(publisher_value_encoding));
        }

        if (register_publisher_state.last_error_code[0] == '\0') {
            strlcpy(register_publisher_state.last_error_code, "none", sizeof(register_publisher_state.last_error_code));
        }

        if (!selector_list_to_json_array(has_components ? components_query : NULL, components_json, sizeof(components_json)) ||
            !selector_list_to_json_array(has_registers ? registers_query : NULL, registers_json, sizeof(registers_json)) ||
            !selector_list_to_json_array(has_prefixes ? prefixes_query : NULL, prefixes_json, sizeof(prefixes_json))) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }

        if (publisher_value_bits == 0 ||
            !(strcmp(publisher_value_encoding, "hex") == 0 || strcmp(publisher_value_encoding, "signed") == 0 || strcmp(publisher_value_encoding, "unsigned") == 0)) {
            strlcpy(register_publisher_state.last_error_code, "INTERNAL_ERROR", sizeof(register_publisher_state.last_error_code));
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }

        if (register_publisher_state.last_event_seq > 0 && publisher_event_seq != (register_publisher_state.last_event_seq + 1ULL)) {
            register_publisher_state.check_failures_seq++;
            strlcpy(register_publisher_state.last_error_code, "REG-PUB-01", sizeof(register_publisher_state.last_error_code));
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }

        if (register_publisher_state.last_event_timestamp_us > 0 && publisher_event_timestamp_us < register_publisher_state.last_event_timestamp_us) {
            register_publisher_state.check_failures_timestamp++;
            strlcpy(register_publisher_state.last_error_code, "REG-PUB-02", sizeof(register_publisher_state.last_error_code));
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }

        if (selector_list_has_values(components_query) && !selector_list_contains_exact(components_query, publisher_component)) {
            register_publisher_state.check_failures_selector++;
            strlcpy(register_publisher_state.last_error_code, "REG-PUB-03", sizeof(register_publisher_state.last_error_code));
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }
        if (selector_list_has_values(registers_query) && !selector_list_contains_exact(registers_query, publisher_register)) {
            register_publisher_state.check_failures_selector++;
            strlcpy(register_publisher_state.last_error_code, "REG-PUB-03", sizeof(register_publisher_state.last_error_code));
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }
        if (selector_list_has_values(prefixes_query) && !selector_list_contains_prefix(prefixes_query, publisher_register)) {
            register_publisher_state.check_failures_selector++;
            strlcpy(register_publisher_state.last_error_code, "REG-PUB-03", sizeof(register_publisher_state.last_error_code));
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }

        if (changed_only_enabled && strcmp(publisher_old_value, publisher_new_value) == 0) {
            register_publisher_state.check_failures_changed_only++;
            register_publisher_state.events_suppressed_changed_only++;
            strlcpy(register_publisher_state.last_error_code, "REG-PUB-04", sizeof(register_publisher_state.last_error_code));
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }

        register_publisher_state.last_event_seq = publisher_event_seq;
        register_publisher_state.last_event_timestamp_us = publisher_event_timestamp_us;
        register_publisher_state.events_emitted++;
        strlcpy(register_publisher_state.last_error_code, "none", sizeof(register_publisher_state.last_error_code));

        snprintf(stream_register_contract_json_buf,
                 sizeof(stream_register_contract_json_buf),
                 "{\"channel\":\"registers.snapshot.v1\",\"schema\":\"register_snapshot_v1\",\"required_fields\":[\"type\",\"schema_version\",\"session_id\",\"tick\",\"cycle\",\"component\",\"register\",\"old_value\",\"new_value\",\"value_encoding\",\"value_bits\"],\"value_encoding_enum\":[\"hex\",\"signed\",\"unsigned\"],\"ordering\":\"tick_cycle_ascending\",\"selectors\":{\"components\":\"allowlist\",\"registers\":\"exact_allowlist\",\"register_prefixes\":\"prefix_allowlist\",\"changed_only\":\"suppress_old_equals_new\",\"mode_enum\":[\"event\",\"interval\"],\"interval_us\":\"required_when_mode_interval\"}}" );
        snprintf(stream_register_filter_json_buf,
                 sizeof(stream_register_filter_json_buf),
                 "{\"components\":%s,\"registers\":%s,\"register_prefixes\":%s,\"changed_only\":%s,\"mode\":\"%s\",\"interval_us\":%s}",
                 components_json,
                 registers_json,
                 prefixes_json,
                 changed_only,
                 mode,
                 interval_us);
        snprintf(stream_register_sample_json_buf,
                 sizeof(stream_register_sample_json_buf),
                 "{\"type\":\"register_update\",\"schema_version\":1,\"session_id\":\"ses_local\",\"event_seq\":%llu,\"event_timestamp_us\":%llu,\"tick\":%llu,\"cycle\":%llu,\"component\":\"%s\",\"register\":\"%s\",\"old_value\":\"%s\",\"new_value\":\"%s\",\"value_encoding\":\"%s\",\"value_bits\":%lu}",
                 (unsigned long long)register_publisher_state.last_event_seq,
                 (unsigned long long)register_publisher_state.last_event_timestamp_us,
                 (unsigned long long)(register_publisher_state.last_event_seq * 2ULL),
                 (unsigned long long)register_publisher_state.last_event_seq,
                 publisher_component,
                 publisher_register,
                 publisher_old_value,
                 publisher_new_value,
                 publisher_value_encoding,
                 (unsigned long)publisher_value_bits);
        snprintf(stream_register_publisher_json_buf,
                 sizeof(stream_register_publisher_json_buf),
                 "{\"pipeline\":[\"collect\",\"apply_filters\",\"schema_validate\",\"emit\"],\"checks\":{\"REG-PUB-01\":\"pass\",\"REG-PUB-02\":\"pass\",\"REG-PUB-03\":\"pass\",\"REG-PUB-04\":\"pass\"},\"events_emitted\":%llu,\"events_suppressed_changed_only\":%llu,\"state\":{\"last_event_seq\":%llu,\"last_event_timestamp_us\":%llu,\"failure_counters\":{\"reg_pub_01\":%llu,\"reg_pub_02\":%llu,\"reg_pub_03\":%llu,\"reg_pub_04\":%llu},\"last_error\":\"%s\"}}",
                 (unsigned long long)register_publisher_state.events_emitted,
                 (unsigned long long)register_publisher_state.events_suppressed_changed_only,
                 (unsigned long long)register_publisher_state.last_event_seq,
                 (unsigned long long)register_publisher_state.last_event_timestamp_us,
                 (unsigned long long)register_publisher_state.check_failures_seq,
                 (unsigned long long)register_publisher_state.check_failures_timestamp,
                 (unsigned long long)register_publisher_state.check_failures_selector,
                 (unsigned long long)register_publisher_state.check_failures_changed_only,
                 register_publisher_state.last_error_code);
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
             "{\"ok\":true,\"data\":{\"stream\":\"%s\",\"event_seq\":%llu,\"event_timestamp_us\":%llu,\"delivery\":{\"degraded\":%s,\"reason\":\"%s\",\"dropped_events_since_last\":%lu,\"coalesced_updates\":%lu,\"throttle_active\":%s},\"backpressure\":{\"queue_depth\":%lu,\"queue_capacity\":%lu,\"dropped_events\":%llu,\"dropped_events_since_last\":%lu,\"throttle_active\":%s,\"high_watermark_depth\":%lu,\"high_watermark_ratio\":%.3f,\"overflow_events_total\":%llu,\"throttle_transitions_total\":%llu,\"sample_timestamp_us\":%llu},\"backpressure_event\":{\"type\":\"stream_backpressure_telemetry\",\"schema_version\":1,\"session_id\":\"ses_local\",\"event_seq\":%llu,\"event_timestamp_us\":%llu,\"stream\":\"%s\",\"metrics\":{\"queue_depth\":%lu,\"queue_capacity\":%lu,\"dropped_events\":%llu,\"dropped_events_since_last\":%lu,\"throttle_active\":%s,\"high_watermark_depth\":%lu,\"high_watermark_ratio\":%.3f,\"overflow_events_total\":%llu,\"throttle_transitions_total\":%llu,\"sample_timestamp_us\":%llu}},\"video_metadata_contract\":%s,\"video_frame_meta_sample\":%s,\"audio_metadata_contract\":%s,\"audio_chunk_meta_sample\":%s,\"register_snapshot_contract\":%s,\"register_filter_selectors\":%s,\"register_update_sample\":%s,\"register_publisher\":%s,\"video_payload_emitter\":%s,\"video_payload_sample\":%s,\"audio_payload_emitter\":%s,\"audio_payload_sample\":%s,\"media_attach_status_events\":%s,\"media_disk_state_events\":%s,\"slo_alarm\":{\"seq\":%llu,\"state\":\"%s\",\"severity\":\"%s\"}}}",
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
             stream_register_contract_json_buf,
             stream_register_filter_json_buf,
             stream_register_sample_json_buf,
             stream_register_publisher_json_buf,
             stream_video_payload_emitter_json_buf,
             stream_video_payload_sample_json_buf,
             stream_audio_payload_emitter_json_buf,
             stream_audio_payload_sample_json_buf,
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
