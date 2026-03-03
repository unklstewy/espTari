#include "esptari_web_debug.h"

#include <stdint.h>
#include <stdio.h>

#include "esp_timer.h"
#include "esptari_web_debug_mode.h"
#include "esptari_web_debug_state.h"
#include "esptari_web_debug_step.h"
#include "esptari_web_http_utils.h"

#define send_json esptari_web_send_json

void esptari_web_debug_get_runtime_snapshot(esptari_web_debug_runtime_snapshot_t *out)
{
    if (out == NULL) {
        return;
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    if (esptari_web_debug_timestamp_origin_us == 0) {
        esptari_web_debug_timestamp_origin_us = now_us;
        esptari_web_debug_timestamp_last_emitted_us = now_us;
    }

    out->run_mode = esptari_web_debug_clock_mode;
    out->tick_counter = esptari_web_debug_tick_counter;
    out->cycle_counter = esptari_web_debug_cycle_counter;
    out->scheduler_hz = esptari_web_debug_scheduler_hz;
    out->timestamp_origin_us = esptari_web_debug_timestamp_origin_us;
    out->timestamp_last_emitted_us = esptari_web_debug_timestamp_last_emitted_us;
    out->timestamp_regressions = esptari_web_debug_timestamp_regressions;
}

static esp_err_t esptari_web_debug_clock_state_handler(httpd_req_t *req)
{
    char resp[640];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"mode\":\"%s\",\"effective_ratio\":%.6f,\"mode_transition_seq\":%llu,\"last_transition_at_us\":%llu,\"scheduler_hz\":%lu,\"tick_counter\":%llu,\"cycle_counter\":%llu,\"timestamp_origin_us\":%llu,\"timestamp_last_emitted_us\":%llu,\"timestamp_regressions\":%llu,\"arbitration_round\":%lu}}",
             esptari_web_debug_clock_mode,
             esptari_web_debug_clock_effective_ratio,
             (unsigned long long)esptari_web_debug_clock_mode_transition_seq,
             (unsigned long long)esptari_web_debug_clock_last_transition_at_us,
             (unsigned long)esptari_web_debug_scheduler_hz,
             (unsigned long long)esptari_web_debug_tick_counter,
             (unsigned long long)esptari_web_debug_cycle_counter,
             (unsigned long long)esptari_web_debug_timestamp_origin_us,
             (unsigned long long)esptari_web_debug_timestamp_last_emitted_us,
             (unsigned long long)esptari_web_debug_timestamp_regressions,
             (unsigned long)esptari_web_debug_arbitration_round);
    return send_json(req, resp, 200);
}

void esptari_web_debug_register_routes(httpd_handle_t server_handle)
{
    httpd_uri_t debug_clock_mode = {
        .uri = "/api/v2/debug/clock/mode",
        .method = HTTP_POST,
        .handler = esptari_web_debug_clock_mode_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t debug_clock_step = {
        .uri = "/api/v2/debug/clock/step",
        .method = HTTP_POST,
        .handler = esptari_web_debug_clock_step_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t debug_clock_state = {
        .uri = "/api/v2/debug/clock/state",
        .method = HTTP_GET,
        .handler = esptari_web_debug_clock_state_handler,
        .user_ctx = NULL,
    };

    httpd_register_uri_handler(server_handle, &debug_clock_mode);
    httpd_register_uri_handler(server_handle, &debug_clock_step);
    httpd_register_uri_handler(server_handle, &debug_clock_state);
}
