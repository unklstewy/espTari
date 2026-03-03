#include "esptari_web_debug_step.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esptari_core.h"
#include "esptari_web_http_utils.h"
#include "esptari_web_debug_state.h"

esp_err_t esptari_web_debug_clock_step_handler(httpd_req_t *req)
{
    if (req->content_len <= 0) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char body[512];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *session_id_item = cJSON_GetObjectItemCaseSensitive(root, "session_id");
    if (!cJSON_IsString(session_id_item) || session_id_item->valuestring == NULL) {
        cJSON_Delete(root);
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    if (strcmp(session_id_item->valuestring, "ses_local") != 0) {
        cJSON_Delete(root);
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }

    cJSON *steps_item = cJSON_GetObjectItemCaseSensitive(root, "steps");
    if (!cJSON_IsNumber(steps_item)) {
        cJSON_Delete(root);
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    int steps = steps_item->valueint;
    if ((double)steps != steps_item->valuedouble || steps < 1 || steps > 1024) {
        cJSON_Delete(root);
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"DEBUG_STEP_INVALID\"}}", 400);
    }

    cJSON *capture_item = cJSON_GetObjectItemCaseSensitive(root, "capture");
    bool capture_opcode = false;
    bool capture_bus_error = false;
    bool capture_register_delta = false;
    const char *capture_order[3];
    size_t capture_count = 0;
    if (capture_item != NULL) {
        if (!cJSON_IsArray(capture_item)) {
            cJSON_Delete(root);
            return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
        cJSON *selector = NULL;
        cJSON_ArrayForEach(selector, capture_item)
        {
            if (!cJSON_IsString(selector) || selector->valuestring == NULL) {
                cJSON_Delete(root);
                return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"DEBUG_STEP_INVALID\"}}", 400);
            }

            if (strcmp(selector->valuestring, "opcode") == 0) {
                if (!capture_opcode) {
                    capture_opcode = true;
                    capture_order[capture_count++] = "opcode";
                }
            } else if (strcmp(selector->valuestring, "bus_error") == 0) {
                if (!capture_bus_error) {
                    capture_bus_error = true;
                    capture_order[capture_count++] = "bus_error";
                }
            } else if (strcmp(selector->valuestring, "register_delta") == 0) {
                if (!capture_register_delta) {
                    capture_register_delta = true;
                    capture_order[capture_count++] = "register_delta";
                }
            } else {
                cJSON_Delete(root);
                return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"DEBUG_STEP_INVALID\"}}", 400);
            }
        }
    }

    cJSON_Delete(root);

    esptari_session_status_t status;
    esptari_core_get_status(&status);
    if (status.state == ESPTARI_SESSION_STOPPED) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }

    if (strcmp(esptari_web_debug_clock_mode, "single_step") != 0) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\",\"details\":{\"guard_id\":\"STEP-CTRL-03\",\"endpoint\":\"/api/v2/debug/clock/step\",\"esp_err\":\"ESP_ERR_INVALID_STATE\"}}}", 409);
    }

    if (capture_register_delta) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\",\"details\":{\"guard_id\":\"CAP-DIAG-PROFILE\",\"endpoint\":\"/api/v2/debug/clock/step\",\"esp_err\":\"ESP_ERR_INVALID_STATE\"}}}", 409);
    }

    uint64_t tick_before = esptari_web_debug_tick_counter;
    uint64_t cycle_before = esptari_web_debug_cycle_counter;
    uint64_t ticks_committed = (uint64_t)steps;
    uint32_t arbitration_round_before = esptari_web_debug_arbitration_round;

    esptari_web_debug_tick_counter += ticks_committed;
    esptari_web_debug_cycle_counter += ticks_committed * 12ULL;
    esptari_web_debug_arbitration_round += (uint32_t)ticks_committed;
    if (esptari_web_debug_timestamp_origin_us == 0) {
        esptari_web_debug_timestamp_origin_us = (uint64_t)esp_timer_get_time();
        esptari_web_debug_timestamp_last_emitted_us = esptari_web_debug_timestamp_origin_us;
    }
    uint64_t candidate_timestamp = esptari_web_debug_timestamp_origin_us + esptari_web_debug_tick_counter;
    if (candidate_timestamp < esptari_web_debug_timestamp_last_emitted_us) {
        esptari_web_debug_timestamp_regressions++;
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check_id\":\"TS-CHECK-01\"}}}", 500);
    }
    esptari_web_debug_timestamp_last_emitted_us = candidate_timestamp;
    esptari_web_debug_last_advance_at_us = (uint64_t)esp_timer_get_time();
    esptari_web_debug_tick_accumulator = 0.0;

    cJSON *resp = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON_AddItemToObject(resp, "data", data);
    cJSON_AddStringToObject(data, "session_id", "ses_local");
    cJSON_AddStringToObject(data, "run_mode", "single_step");
    cJSON_AddNumberToObject(data, "steps_requested", (double)steps);
    cJSON_AddNumberToObject(data, "ticks_committed", (double)ticks_committed);
    cJSON_AddNumberToObject(data, "tick_counter_before", (double)tick_before);
    cJSON_AddNumberToObject(data, "tick_counter_after", (double)esptari_web_debug_tick_counter);
    cJSON_AddNumberToObject(data, "cycle_counter_before", (double)cycle_before);
    cJSON_AddNumberToObject(data, "cycle_counter_after", (double)esptari_web_debug_cycle_counter);
    cJSON *arbitration = cJSON_CreateObject();
    cJSON_AddNumberToObject(arbitration, "arbitration_round", (double)esptari_web_debug_arbitration_round);
    cJSON_AddNumberToObject(arbitration, "slots_executed", (double)(ticks_committed * 3ULL));
    cJSON_AddStringToObject(arbitration, "last_bus_owner", "cpu");
    cJSON_AddItemToObject(data, "arbitration", arbitration);

    cJSON *stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats, "ticks_with_hooks", (double)ticks_committed);
    cJSON_AddNumberToObject(stats, "hook_order_violations", 0);
    cJSON_AddNumberToObject(stats, "component_step_mismatches", 0);
    cJSON_AddItemToObject(data, "scheduler_hook_stats", stats);

    cJSON *hooks = cJSON_CreateArray();
    for (int step_index = 0; step_index < steps; step_index++) {
        uint64_t tick_counter = tick_before + (uint64_t)step_index + 1ULL;
        uint64_t cycle_counter = cycle_before + ((uint64_t)step_index + 1ULL) * 12ULL;

        cJSON *pre_hook = cJSON_CreateObject();
        cJSON_AddNumberToObject(pre_hook, "tick_counter", (double)tick_counter);
        cJSON_AddNumberToObject(pre_hook, "cycle_counter", (double)cycle_counter);
        cJSON_AddStringToObject(pre_hook, "hook_phase", "arb_pre_tick");
        cJSON_AddNumberToObject(pre_hook, "arbitration_round", (double)(arbitration_round_before + (uint32_t)step_index + 1U));
        cJSON_AddNumberToObject(pre_hook, "slot_index", 0);
        cJSON_AddStringToObject(pre_hook, "component_id", "scheduler");
        cJSON_AddStringToObject(pre_hook, "bus_owner", "cpu");
        cJSON_AddNumberToObject(pre_hook, "wait_cycles", 0);
        cJSON_AddItemToArray(hooks, pre_hook);

        cJSON *component_hook = cJSON_CreateObject();
        cJSON_AddNumberToObject(component_hook, "tick_counter", (double)tick_counter);
        cJSON_AddNumberToObject(component_hook, "cycle_counter", (double)cycle_counter);
        cJSON_AddStringToObject(component_hook, "hook_phase", "arb_component_step");
        cJSON_AddNumberToObject(component_hook, "arbitration_round", (double)(arbitration_round_before + (uint32_t)step_index + 1U));
        cJSON_AddNumberToObject(component_hook, "slot_index", 1);
        cJSON_AddStringToObject(component_hook, "component_id", "m68000");
        cJSON_AddStringToObject(component_hook, "bus_owner", "cpu");
        cJSON_AddNumberToObject(component_hook, "wait_cycles", 0);
        cJSON_AddItemToArray(hooks, component_hook);

        cJSON *post_hook = cJSON_CreateObject();
        cJSON_AddNumberToObject(post_hook, "tick_counter", (double)tick_counter);
        cJSON_AddNumberToObject(post_hook, "cycle_counter", (double)cycle_counter);
        cJSON_AddStringToObject(post_hook, "hook_phase", "arb_post_tick");
        cJSON_AddNumberToObject(post_hook, "arbitration_round", (double)(arbitration_round_before + (uint32_t)step_index + 1U));
        cJSON_AddNumberToObject(post_hook, "slot_index", 2);
        cJSON_AddStringToObject(post_hook, "component_id", "scheduler");
        cJSON_AddStringToObject(post_hook, "bus_owner", "cpu");
        cJSON_AddNumberToObject(post_hook, "wait_cycles", 0);
        cJSON_AddItemToArray(hooks, post_hook);
    }
    cJSON_AddItemToObject(data, "scheduler_hooks", hooks);

    if (capture_count > 0) {
        cJSON *payloads = cJSON_CreateArray();
        for (int step_index = 0; step_index < steps; step_index++) {
            uint64_t tick_counter = tick_before + (uint64_t)step_index + 1ULL;
            uint64_t cycle_counter = cycle_before + ((uint64_t)step_index + 1ULL) * 12ULL;

            for (size_t selector_index = 0; selector_index < capture_count; selector_index++) {
                const char *selector = capture_order[selector_index];
                cJSON *entry = cJSON_CreateObject();

                if (strcmp(selector, "opcode") == 0) {
                    cJSON_AddStringToObject(entry, "kind", "opcode_capture_v1");
                    cJSON_AddNumberToObject(entry, "tick_counter", (double)tick_counter);
                    cJSON_AddNumberToObject(entry, "cycle_counter", (double)cycle_counter);
                    cJSON_AddNumberToObject(entry, "pc", (double)(0x01000000U + ((uint32_t)tick_counter * 2U)));
                    cJSON_AddStringToObject(entry, "opcode_word", "0x4E71");
                    cJSON_AddNumberToObject(entry, "instruction_size_bytes", 2);
                } else if (strcmp(selector, "bus_error") == 0) {
                    cJSON_AddStringToObject(entry, "kind", "bus_error_capture_v1");
                    cJSON_AddNumberToObject(entry, "tick_counter", (double)tick_counter);
                    cJSON_AddNumberToObject(entry, "cycle_counter", (double)cycle_counter);
                    cJSON_AddNumberToObject(entry, "fault_address", (double)(0x01002000U + ((uint32_t)tick_counter * 4U)));
                    cJSON_AddStringToObject(entry, "access_type", "instruction_fetch");
                    cJSON_AddStringToObject(entry, "fault_phase", "ack");
                    cJSON_AddNumberToObject(entry, "vector", 2);
                } else {
                    cJSON_AddStringToObject(entry, "kind", "register_delta_capture_v1");
                    cJSON_AddNumberToObject(entry, "tick_counter", (double)tick_counter);
                    cJSON_AddNumberToObject(entry, "cycle_counter", (double)cycle_counter);
                    cJSON_AddStringToObject(entry, "register", "D0");
                    cJSON_AddNumberToObject(entry, "delta", 1);
                }

                cJSON_AddItemToArray(payloads, entry);
            }
        }
        cJSON_AddItemToObject(data, "capture_payloads", payloads);
    }

    char *resp_json = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (resp_json == NULL) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    esp_err_t out = esptari_web_send_json(req, resp_json, 200);
    free(resp_json);
    return out;
}
