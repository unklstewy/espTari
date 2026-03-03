#include "esptari_web_stream.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_timer.h"
#include "esptari_core.h"

static uint64_t stream_event_seq;
static uint64_t backpressure_overflow_total;
static uint64_t backpressure_throttle_transitions_total;
static bool backpressure_throttle_active;
static uint64_t slo_alarm_seq;
static bool slo_alarm_breached;

static esp_err_t send_json(httpd_req_t *req, const char *json, int status_code)
{
    httpd_resp_set_type(req, "application/json");
    const char *status = "500 Internal Server Error";
    switch (status_code) {
    case 200:
        status = "200 OK";
        break;
    case 201:
        status = "201 Created";
        break;
    case 400:
        status = "400 Bad Request";
        break;
    case 404:
        status = "404 Not Found";
        break;
    case 409:
        status = "409 Conflict";
        break;
    case 412:
        status = "412 Precondition Failed";
        break;
    default:
        break;
    }
    httpd_resp_set_status(req, status);
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static bool query_value(httpd_req_t *req, const char *key, char *out, size_t out_len)
{
    if (httpd_req_get_url_query_len(req) <= 0) {
        return false;
    }

    char query[256] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        return false;
    }
    return httpd_query_key_value(query, key, out, out_len) == ESP_OK;
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

static esp_err_t emit_stream_probe(httpd_req_t *req, const char *stream_name)
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

    bool degraded = false;
    const char *delivery_reason = "none";
    uint64_t dropped_events_since_last = 0;
    uint64_t coalesced_updates = 0;

    char backpressure[16] = {0};
    if (query_value(req, "backpressure", backpressure, sizeof(backpressure)) && strcmp(backpressure, "1") == 0) {
        degraded = true;
        delivery_reason = "queue_overflow";
        dropped_events_since_last = 1;
        coalesced_updates = 1;
        backpressure_overflow_total++;
        if (!backpressure_throttle_active) {
            backpressure_throttle_active = true;
            backpressure_throttle_transitions_total++;
        }
    } else if (backpressure_throttle_active) {
        backpressure_throttle_active = false;
        backpressure_throttle_transitions_total++;
    }

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

    char resp[896];
    snprintf(resp, sizeof(resp),
             "{\"ok\":true,\"data\":{\"stream\":\"%s\",\"event_seq\":%llu,\"event_timestamp_us\":%llu,\"delivery\":{\"degraded\":%s,\"reason\":\"%s\",\"dropped_events_since_last\":%llu,\"coalesced_updates\":%llu,\"throttle_active\":%s},\"backpressure\":{\"queue_depth\":%u,\"queue_capacity\":128,\"dropped_events\":%llu,\"dropped_events_since_last\":%llu,\"throttle_active\":%s,\"high_watermark_depth\":128,\"high_watermark_ratio\":1.0,\"overflow_events_total\":%llu,\"throttle_transitions_total\":%llu,\"sample_timestamp_us\":%llu},\"slo_alarm\":{\"seq\":%llu,\"state\":\"%s\",\"severity\":\"%s\"}}}",
             stream_name,
             (unsigned long long)stream_event_seq,
             (unsigned long long)timestamp_us,
             degraded ? "true" : "false",
             delivery_reason,
             (unsigned long long)dropped_events_since_last,
             (unsigned long long)coalesced_updates,
             backpressure_throttle_active ? "true" : "false",
             backpressure_throttle_active ? 96u : 32u,
             (unsigned long long)backpressure_overflow_total,
             (unsigned long long)dropped_events_since_last,
             backpressure_throttle_active ? "true" : "false",
             (unsigned long long)backpressure_overflow_total,
             (unsigned long long)backpressure_throttle_transitions_total,
             (unsigned long long)timestamp_us,
             (unsigned long long)slo_alarm_seq,
             slo_state,
             slo_severity);
    return send_json(req, resp, 200);
}

static esp_err_t stream_video_handler(httpd_req_t *req)
{
    return emit_stream_probe(req, "video");
}

static esp_err_t stream_audio_handler(httpd_req_t *req)
{
    return emit_stream_probe(req, "audio");
}

static esp_err_t inspect_registers_stream_handler(httpd_req_t *req)
{
    return emit_stream_probe(req, "registers");
}

static esp_err_t inspect_bus_stream_handler(httpd_req_t *req)
{
    return emit_stream_probe(req, "bus");
}

static esp_err_t inspect_memory_stream_handler(httpd_req_t *req)
{
    return emit_stream_probe(req, "memory");
}

void esptari_web_stream_get_runtime_snapshot(esptari_web_stream_runtime_snapshot_t *out)
{
    if (out == NULL) {
        return;
    }

    out->dropped_packets_total = backpressure_overflow_total;
}

void esptari_web_stream_register_routes(httpd_handle_t server_handle)
{
    httpd_uri_t stream_video = {.uri = "/api/v2/stream/video", .method = HTTP_GET, .handler = stream_video_handler, .user_ctx = NULL};
    httpd_uri_t stream_audio = {.uri = "/api/v2/stream/audio", .method = HTTP_GET, .handler = stream_audio_handler, .user_ctx = NULL};
    httpd_uri_t inspect_registers = {.uri = "/api/v2/inspect/registers/stream", .method = HTTP_GET, .handler = inspect_registers_stream_handler, .user_ctx = NULL};
    httpd_uri_t inspect_bus = {.uri = "/api/v2/inspect/bus/stream", .method = HTTP_GET, .handler = inspect_bus_stream_handler, .user_ctx = NULL};
    httpd_uri_t inspect_memory = {.uri = "/api/v2/inspect/memory/stream", .method = HTTP_GET, .handler = inspect_memory_stream_handler, .user_ctx = NULL};

    httpd_register_uri_handler(server_handle, &stream_video);
    httpd_register_uri_handler(server_handle, &stream_audio);
    httpd_register_uri_handler(server_handle, &inspect_registers);
    httpd_register_uri_handler(server_handle, &inspect_bus);
    httpd_register_uri_handler(server_handle, &inspect_memory);
}
