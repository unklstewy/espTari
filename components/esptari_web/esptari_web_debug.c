#include "esptari_web_debug.h"

#include <stdint.h>
#include <stdio.h>

#include "esp_timer.h"
#include "esptari_core.h"
#include "esptari_web_auth.h"
#include "esptari_web_debug_mode.h"
#include "esptari_web_debug_state.h"
#include "esptari_web_debug_step.h"
#include "esptari_web_http_utils.h"

#define send_json esptari_web_send_json

static void advance_runtime_scheduler(void)
{
    uint64_t now_us = (uint64_t)esp_timer_get_time();
    if (esptari_web_debug_timestamp_origin_us == 0) {
        esptari_web_debug_timestamp_origin_us = now_us;
        esptari_web_debug_timestamp_last_emitted_us = now_us;
        esptari_web_debug_last_advance_at_us = now_us;
    }

    esptari_session_status_t status;
    esptari_core_get_status(&status);
    if (status.state != ESPTARI_SESSION_RUNNING) {
        esptari_web_debug_last_advance_at_us = now_us;
        return;
    }

    if (strcmp(esptari_web_debug_clock_mode, "single_step") == 0) {
        esptari_web_debug_last_advance_at_us = now_us;
        return;
    }

    if (esptari_web_debug_last_advance_at_us == 0) {
        esptari_web_debug_last_advance_at_us = now_us;
        return;
    }

    if (now_us < esptari_web_debug_last_advance_at_us) {
        esptari_web_debug_timestamp_regressions++;
        esptari_web_debug_last_advance_at_us = now_us;
        return;
    }

    uint64_t elapsed_us = now_us - esptari_web_debug_last_advance_at_us;
    esptari_web_debug_last_advance_at_us = now_us;
    if (elapsed_us == 0) {
        return;
    }

    double ticks_exact = ((double)elapsed_us * (double)esptari_web_debug_scheduler_hz * esptari_web_debug_clock_effective_ratio) / 1000000.0;
    ticks_exact += esptari_web_debug_tick_accumulator;
    if (ticks_exact < 1.0) {
        esptari_web_debug_tick_accumulator = ticks_exact;
        return;
    }

    uint64_t ticks_advanced = (uint64_t)ticks_exact;
    esptari_web_debug_tick_accumulator = ticks_exact - (double)ticks_advanced;
    esptari_web_debug_tick_counter += ticks_advanced;
    esptari_web_debug_cycle_counter += ticks_advanced * 12ULL;
    esptari_web_debug_arbitration_round += (uint32_t)ticks_advanced;

    uint64_t candidate_timestamp = esptari_web_debug_timestamp_origin_us + esptari_web_debug_tick_counter;
    if (candidate_timestamp < esptari_web_debug_timestamp_last_emitted_us) {
        esptari_web_debug_timestamp_regressions++;
    } else {
        esptari_web_debug_timestamp_last_emitted_us = candidate_timestamp;
        esptari_web_debug_timestamp_emit_seq++;
    }
}

void esptari_web_debug_get_runtime_snapshot(esptari_web_debug_runtime_snapshot_t *out)
{
    if (out == NULL) {
        return;
    }

    advance_runtime_scheduler();

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
    esptari_web_debug_runtime_snapshot_t snapshot = {0};
    esptari_web_debug_get_runtime_snapshot(&snapshot);

    char resp[896];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"mode\":\"%s\",\"effective_ratio\":%.6f,\"mode_transition_seq\":%llu,\"last_transition_at_us\":%llu,\"scheduler_hz\":%lu,\"tick_counter\":%llu,\"cycle_counter\":%llu,\"timestamp_origin_us\":%llu,\"timestamp_last_emitted_us\":%llu,\"timestamp_regressions\":%llu,\"arbitration_round\":%lu,\"arbitration\":{\"hook_layer\":\"arb_pre_tick,arb_component_step,arb_post_tick\",\"round\":%lu,\"last_bus_owner\":\"cpu\"},\"timestamp_emitter\":{\"emit_seq\":%llu,\"source\":\"tick_counter\",\"monotonic\":true}}}",
             snapshot.run_mode,
             esptari_web_debug_clock_effective_ratio,
             (unsigned long long)esptari_web_debug_clock_mode_transition_seq,
             (unsigned long long)esptari_web_debug_clock_last_transition_at_us,
             (unsigned long)snapshot.scheduler_hz,
             (unsigned long long)snapshot.tick_counter,
             (unsigned long long)snapshot.cycle_counter,
             (unsigned long long)snapshot.timestamp_origin_us,
             (unsigned long long)snapshot.timestamp_last_emitted_us,
             (unsigned long long)snapshot.timestamp_regressions,
             (unsigned long)esptari_web_debug_arbitration_round,
             (unsigned long)esptari_web_debug_arbitration_round,
             (unsigned long long)esptari_web_debug_timestamp_emit_seq);
    return send_json(req, resp, 200);
}

void esptari_web_debug_register_routes(httpd_handle_t server_handle)
{
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/debug/clock/mode", HTTP_POST, esptari_web_debug_clock_mode_handler, "engine:control"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/debug/clock/step", HTTP_POST, esptari_web_debug_clock_step_handler, "engine:control"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/debug/clock/state", HTTP_GET, esptari_web_debug_clock_state_handler, "inspect:read"));
}
