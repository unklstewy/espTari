#include "esptari_web_debug.h"

#include <stdint.h>

#include "esp_timer.h"
#include "esptari_web_debug_mode.h"
#include "esptari_web_debug_state.h"
#include "esptari_web_debug_step.h"

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

    httpd_register_uri_handler(server_handle, &debug_clock_mode);
    httpd_register_uri_handler(server_handle, &debug_clock_step);
}
