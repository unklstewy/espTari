#include "esptari_web_stream.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_timer.h"
#include "esptari_core.h"
#include "esptari_web_http_utils.h"

#define send_json esptari_web_send_json
#define query_value esptari_web_query_value

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

static uint64_t stream_event_seq;
static uint64_t slo_alarm_seq;
static bool slo_alarm_breached;
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

static esp_err_t stream_guard_running(httpd_req_t *req)
{
    esptari_session_status_t status;
    esptari_core_get_status(&status);
    if (status.state != ESPTARI_SESSION_RUNNING) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }
    return ESP_OK;
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

    stream_event_seq++;
    uint64_t timestamp_us = (uint64_t)esp_timer_get_time();
    const char *stream_name = stream_kind_name(stream);
    stream_backpressure_metrics_t *metrics = stream_metrics_for(stream);
    if (metrics == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char backpressure[16] = {0};
    bool pressure_active = query_value(req, "backpressure", backpressure, sizeof(backpressure)) && strcmp(backpressure, "1") == 0;
    uint32_t dropped_events_since_last = 0;
    uint32_t coalesced_updates = 0;
    update_backpressure_metrics(metrics,
                                pressure_active,
                                timestamp_us,
                                &dropped_events_since_last,
                                &coalesced_updates);

    bool degraded = dropped_events_since_last > 0 || coalesced_updates > 0 || metrics->throttle_active;
    const char *delivery_reason = (dropped_events_since_last > 0 || coalesced_updates > 0)
                                      ? "queue_overflow"
                                      : "none";

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

    char resp[1792];
    snprintf(resp, sizeof(resp),
             "{\"ok\":true,\"data\":{\"stream\":\"%s\",\"event_seq\":%llu,\"event_timestamp_us\":%llu,\"delivery\":{\"degraded\":%s,\"reason\":\"%s\",\"dropped_events_since_last\":%lu,\"coalesced_updates\":%lu,\"throttle_active\":%s},\"backpressure\":{\"queue_depth\":%lu,\"queue_capacity\":%lu,\"dropped_events\":%llu,\"dropped_events_since_last\":%lu,\"throttle_active\":%s,\"high_watermark_depth\":%lu,\"high_watermark_ratio\":%.3f,\"overflow_events_total\":%llu,\"throttle_transitions_total\":%llu,\"sample_timestamp_us\":%llu},\"backpressure_event\":{\"type\":\"stream_backpressure_telemetry\",\"schema_version\":1,\"session_id\":\"ses_local\",\"event_seq\":%llu,\"event_timestamp_us\":%llu,\"stream\":\"%s\",\"metrics\":{\"queue_depth\":%lu,\"queue_capacity\":%lu,\"dropped_events\":%llu,\"dropped_events_since_last\":%lu,\"throttle_active\":%s,\"high_watermark_depth\":%lu,\"high_watermark_ratio\":%.3f,\"overflow_events_total\":%llu,\"throttle_transitions_total\":%llu,\"sample_timestamp_us\":%llu}},\"slo_alarm\":{\"seq\":%llu,\"state\":\"%s\",\"severity\":\"%s\"}}}",
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
             (unsigned long long)slo_alarm_seq,
             slo_state,
             slo_severity);
    return send_json(req, resp, 200);
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
    httpd_uri_t stream_backpressure = {.uri = "/api/v2/stream/telemetry/backpressure", .method = HTTP_GET, .handler = stream_backpressure_telemetry_handler, .user_ctx = NULL};
    httpd_uri_t inspect_registers = {.uri = "/api/v2/inspect/registers/stream", .method = HTTP_GET, .handler = inspect_registers_stream_handler, .user_ctx = NULL};
    httpd_uri_t inspect_bus = {.uri = "/api/v2/inspect/bus/stream", .method = HTTP_GET, .handler = inspect_bus_stream_handler, .user_ctx = NULL};
    httpd_uri_t inspect_memory = {.uri = "/api/v2/inspect/memory/stream", .method = HTTP_GET, .handler = inspect_memory_stream_handler, .user_ctx = NULL};

    httpd_register_uri_handler(server_handle, &stream_video);
    httpd_register_uri_handler(server_handle, &stream_audio);
    httpd_register_uri_handler(server_handle, &stream_engine);
    httpd_register_uri_handler(server_handle, &stream_backpressure);
    httpd_register_uri_handler(server_handle, &inspect_registers);
    httpd_register_uri_handler(server_handle, &inspect_bus);
    httpd_register_uri_handler(server_handle, &inspect_memory);
}
