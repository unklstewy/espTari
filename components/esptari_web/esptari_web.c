#include "esptari_web.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "esptari_core.h"
#include "esptari_input.h"
#include "esptari_web_catalog.h"
#include "esptari_web_debug.h"
#include "esptari_web_lifecycle.h"
#include "esptari_web_mappings.h"
#include "esptari_web_stream.h"

static const char *TAG = "esptari_web";
static httpd_handle_t server_handle;

static bool query_value(httpd_req_t *req, const char *key, char *out, size_t out_len);

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

static esp_err_t health_handler(httpd_req_t *req)
{
    return send_json(req,
        "{\"ok\":true,\"data\":{\"service\":\"esptari\",\"health\":\"ok\"}}",
        200);
}

static esp_err_t status_handler(httpd_req_t *req)
{
    esptari_session_status_t status;
    esptari_core_get_status(&status);

    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"ok\":true,\"data\":{\"session_state\":\"%s\",\"transition_count\":%llu,\"last_transition_us\":%llu}}",
             esptari_core_state_to_string(status.state),
             (unsigned long long)status.transition_count,
             (unsigned long long)status.last_transition_us);
    return send_json(req, buf, 200);
}

static esp_err_t session_state_handler(httpd_req_t *req)
{
    char session_id[64] = "ses_local";
    char requested_session_id[64] = {0};
    if (query_value(req, "session_id", requested_session_id, sizeof(requested_session_id))) {
        if (requested_session_id[0] == '\0') {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
        strlcpy(session_id, requested_session_id, sizeof(session_id));
    }

    esptari_session_status_t status;
    esptari_core_get_status(&status);

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    uint64_t uptime_ms = 0;
    if (status.state == ESPTARI_SESSION_RUNNING ||
        status.state == ESPTARI_SESSION_PAUSED ||
        status.state == ESPTARI_SESSION_SUSPENDED) {
        if (now_us > status.last_transition_us) {
            uptime_ms = (now_us - status.last_transition_us) / 1000ULL;
        }
    }

    esptari_web_debug_runtime_snapshot_t debug_snapshot = {0};
    esptari_web_debug_get_runtime_snapshot(&debug_snapshot);
    esptari_web_stream_runtime_snapshot_t stream_snapshot = {0};
    esptari_web_stream_get_runtime_snapshot(&stream_snapshot);

    char resp[1536];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"state\":\"%s\",\"run_mode\":\"%s\",\"machine\":\"atari_st\",\"profile\":\"st_520_pal\",\"snapshot_at_us\":%llu,\"uptime_ms\":%llu,\"cycle_counter\":%llu,\"tick_counter\":%llu,\"loaded_modules\":[],\"runtime\":{\"scheduler_hz\":%lu,\"timestamp_origin_us\":%llu,\"timestamp_last_emitted_us\":%llu,\"timestamp_regressions\":%llu,\"last_transition_at_us\":%llu,\"last_error\":null},\"stream_health\":{\"video\":{\"connected_clients\":0,\"dropped_packets\":%llu},\"audio\":{\"connected_clients\":0,\"dropped_packets\":%llu}}}}",
             session_id,
             esptari_core_state_to_string(status.state),
             debug_snapshot.run_mode,
             (unsigned long long)debug_snapshot.timestamp_last_emitted_us,
             (unsigned long long)uptime_ms,
             (unsigned long long)debug_snapshot.cycle_counter,
             (unsigned long long)debug_snapshot.tick_counter,
             (unsigned long)debug_snapshot.scheduler_hz,
             (unsigned long long)debug_snapshot.timestamp_origin_us,
             (unsigned long long)debug_snapshot.timestamp_last_emitted_us,
             (unsigned long long)debug_snapshot.timestamp_regressions,
             (unsigned long long)status.last_transition_us,
             (unsigned long long)stream_snapshot.dropped_packets_total,
             (unsigned long long)stream_snapshot.dropped_packets_total);
    return send_json(req, resp, 200);
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

void esptari_web_init(uint16_t port)
{
    if (server_handle != NULL) {
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 48;
    config.stack_size = 10240;

    if (httpd_start(&server_handle, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start web server");
        server_handle = NULL;
        return;
    }

    httpd_uri_t health = {.uri = "/api/v2/engine/health", .method = HTTP_GET, .handler = health_handler, .user_ctx = NULL};
    httpd_uri_t status = {.uri = "/api/v2/engine/status", .method = HTTP_GET, .handler = status_handler, .user_ctx = NULL};
    httpd_uri_t session_state = {.uri = "/api/v2/engine/session", .method = HTTP_GET, .handler = session_state_handler, .user_ctx = NULL};

    httpd_register_uri_handler(server_handle, &health);
    httpd_register_uri_handler(server_handle, &status);
    httpd_register_uri_handler(server_handle, &session_state);
    esptari_web_lifecycle_register_routes(server_handle);
    esptari_web_mappings_register_routes(server_handle);
    esptari_web_stream_register_routes(server_handle);
    esptari_web_debug_register_routes(server_handle);
    esptari_web_catalog_register_routes(server_handle);

    ESP_LOGI(TAG, "Web API ready on port %u", (unsigned)port);
}

bool esptari_web_is_running(void)
{
    return server_handle != NULL;
}

httpd_handle_t esptari_web_get_server(void)
{
    return server_handle;
}

void esptari_web_start_file_server(void)
{
}
