#include "esptari_web_core_status.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_timer.h"
#include "esptari_core.h"
#include "esptari_web_debug.h"
#include "esptari_web_http_utils.h"
#include "esptari_web_lifecycle_session.h"
#include "esptari_web_stream.h"

#define send_json esptari_web_send_json
#define query_value esptari_web_query_value

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
    const char *active_machine = esptari_web_lifecycle_active_machine();
    const char *active_profile = esptari_web_lifecycle_active_profile();

    char resp[1536];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"state\":\"%s\",\"run_mode\":\"%s\",\"machine\":\"%s\",\"profile\":\"%s\",\"snapshot_at_us\":%llu,\"uptime_ms\":%llu,\"cycle_counter\":%llu,\"tick_counter\":%llu,\"loaded_modules\":[],\"runtime\":{\"scheduler_hz\":%lu,\"timestamp_origin_us\":%llu,\"timestamp_last_emitted_us\":%llu,\"timestamp_regressions\":%llu,\"last_transition_at_us\":%llu,\"last_error\":null},\"stream_health\":{\"video\":{\"connected_clients\":0,\"dropped_packets\":%llu},\"audio\":{\"connected_clients\":0,\"dropped_packets\":%llu}}}}",
             session_id,
             esptari_core_state_to_string(status.state),
             debug_snapshot.run_mode,
             active_machine,
             active_profile,
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

void esptari_web_core_status_register_routes(httpd_handle_t server_handle)
{
    httpd_uri_t health = {.uri = "/api/v2/engine/health", .method = HTTP_GET, .handler = health_handler, .user_ctx = NULL};
    httpd_uri_t status = {.uri = "/api/v2/engine/status", .method = HTTP_GET, .handler = status_handler, .user_ctx = NULL};
    httpd_uri_t session_state = {.uri = "/api/v2/engine/session", .method = HTTP_GET, .handler = session_state_handler, .user_ctx = NULL};

    httpd_register_uri_handler(server_handle, &health);
    httpd_register_uri_handler(server_handle, &status);
    httpd_register_uri_handler(server_handle, &session_state);
}
