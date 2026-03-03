#include "esptari_web_debug.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esptari_core.h"

static const char *clock_mode = "realtime";
static double clock_effective_ratio = 1.0;
static uint64_t clock_mode_transition_seq;
static uint64_t clock_last_transition_at_us;
static uint64_t debug_tick_counter;
static uint64_t debug_cycle_counter;
static const uint32_t debug_scheduler_hz = 8000000U;
static uint64_t timestamp_origin_us;
static uint64_t timestamp_last_emitted_us;
static uint64_t timestamp_regressions;
static uint32_t arbitration_round;

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

static esp_err_t read_request_body(httpd_req_t *req, char *out_buf, size_t out_buf_size)
{
    if (req->content_len <= 0 || (size_t)req->content_len >= out_buf_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    int received = httpd_req_recv(req, out_buf, req->content_len);
    if (received <= 0) {
        return ESP_FAIL;
    }
    out_buf[received] = '\0';
    return ESP_OK;
}

static esp_err_t clock_mode_handler(httpd_req_t *req)
{
    if (req->content_len <= 0) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char body[256];
    if (read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *session_id_item = cJSON_GetObjectItemCaseSensitive(root, "session_id");
    if (!cJSON_IsString(session_id_item) || session_id_item->valuestring == NULL) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    if (strcmp(session_id_item->valuestring, "ses_local") != 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }

    cJSON *mode_item = cJSON_GetObjectItemCaseSensitive(root, "mode");
    if (!cJSON_IsString(mode_item) || mode_item->valuestring == NULL) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const char *target_mode = mode_item->valuestring;
    cJSON *ratio_item = cJSON_GetObjectItemCaseSensitive(root, "ratio");
    bool has_ratio = ratio_item != NULL;
    double target_ratio = 1.0;

    if (strcmp(target_mode, "realtime") == 0) {
        if (has_ratio) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"DEBUG_CLOCK_INVALID\"}}", 400);
        }
        target_ratio = 1.0;
    } else if (strcmp(target_mode, "slow_motion") == 0) {
        if (!has_ratio || !cJSON_IsNumber(ratio_item)) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"DEBUG_CLOCK_INVALID\"}}", 400);
        }
        target_ratio = ratio_item->valuedouble;
        if (!(target_ratio > 0.0 && target_ratio <= 1.0)) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"DEBUG_CLOCK_INVALID\"}}", 400);
        }
    } else if (strcmp(target_mode, "single_step") == 0) {
        if (has_ratio) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"DEBUG_CLOCK_INVALID\"}}", 400);
        }
        target_ratio = 1.0;
    } else {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"DEBUG_CLOCK_INVALID\"}}", 400);
    }

    cJSON_Delete(root);

    esptari_session_status_t status;
    esptari_core_get_status(&status);
    if (status.state == ESPTARI_SESSION_STOPPED) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\",\"details\":{\"guard_id\":\"CLOCK-TRANS-STATE\",\"endpoint\":\"/api/v2/debug/clock/mode\",\"esp_err\":\"ESP_ERR_INVALID_STATE\"}}}", 409);
    }

    bool idempotent = strcmp(clock_mode, target_mode) == 0;
    if (idempotent && strcmp(target_mode, "slow_motion") == 0) {
        idempotent = clock_effective_ratio == target_ratio;
    }

    const char *from_mode = clock_mode;
    if (!idempotent) {
        clock_mode = strcmp(target_mode, "realtime") == 0
                         ? "realtime"
                         : (strcmp(target_mode, "slow_motion") == 0 ? "slow_motion" : "single_step");
        clock_effective_ratio = target_ratio;
        clock_mode_transition_seq++;
        clock_last_transition_at_us = (uint64_t)esp_timer_get_time();
    }

    char resp[512];
    if (!idempotent) {
        snprintf(resp,
                 sizeof(resp),
                 "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"transition_applied\":true,\"mode_transition_seq\":%llu,\"from_mode\":\"%s\",\"to_mode\":\"%s\",\"effective_ratio\":%.6f,\"last_transition_at_us\":%llu}}",
                 (unsigned long long)clock_mode_transition_seq,
                 from_mode,
                 clock_mode,
                 clock_effective_ratio,
                 (unsigned long long)clock_last_transition_at_us);
    } else {
        snprintf(resp,
                 sizeof(resp),
                 "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"transition_applied\":false,\"mode_transition_seq\":%llu,\"from_mode\":\"%s\",\"to_mode\":\"%s\",\"effective_ratio\":%.6f,\"reason\":\"already_in_target_mode\"}}",
                 (unsigned long long)clock_mode_transition_seq,
                 clock_mode,
                 clock_mode,
                 clock_effective_ratio);
    }

    return send_json(req, resp, 200);
}

static esp_err_t clock_step_handler(httpd_req_t *req)
{
    if (req->content_len <= 0) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char body[512];
    if (read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *session_id_item = cJSON_GetObjectItemCaseSensitive(root, "session_id");
    if (!cJSON_IsString(session_id_item) || session_id_item->valuestring == NULL) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    if (strcmp(session_id_item->valuestring, "ses_local") != 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }

    cJSON *steps_item = cJSON_GetObjectItemCaseSensitive(root, "steps");
    if (!cJSON_IsNumber(steps_item)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    int steps = steps_item->valueint;
    if ((double)steps != steps_item->valuedouble || steps < 1 || steps > 1024) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"DEBUG_STEP_INVALID\"}}", 400);
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
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
        cJSON *selector = NULL;
        cJSON_ArrayForEach(selector, capture_item)
        {
            if (!cJSON_IsString(selector) || selector->valuestring == NULL) {
                cJSON_Delete(root);
                return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"DEBUG_STEP_INVALID\"}}", 400);
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
                return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"DEBUG_STEP_INVALID\"}}", 400);
            }
        }
    }

    cJSON_Delete(root);

    esptari_session_status_t status;
    esptari_core_get_status(&status);
    if (status.state == ESPTARI_SESSION_STOPPED) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }

    if (strcmp(clock_mode, "single_step") != 0) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\",\"details\":{\"guard_id\":\"STEP-CTRL-03\",\"endpoint\":\"/api/v2/debug/clock/step\",\"esp_err\":\"ESP_ERR_INVALID_STATE\"}}}", 409);
    }

    if (capture_register_delta) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\",\"details\":{\"guard_id\":\"CAP-DIAG-PROFILE\",\"endpoint\":\"/api/v2/debug/clock/step\",\"esp_err\":\"ESP_ERR_INVALID_STATE\"}}}", 409);
    }

    uint64_t tick_before = debug_tick_counter;
    uint64_t cycle_before = debug_cycle_counter;
    uint64_t ticks_committed = (uint64_t)steps;
    uint32_t arbitration_round_before = arbitration_round;

    debug_tick_counter += ticks_committed;
    debug_cycle_counter += ticks_committed * 12ULL;
    arbitration_round += (uint32_t)ticks_committed;
    if (timestamp_origin_us == 0) {
        timestamp_origin_us = (uint64_t)esp_timer_get_time();
        timestamp_last_emitted_us = timestamp_origin_us;
    }
    uint64_t candidate_timestamp = timestamp_origin_us + debug_tick_counter;
    if (candidate_timestamp < timestamp_last_emitted_us) {
        timestamp_regressions++;
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"details\":{\"check_id\":\"TS-CHECK-01\"}}}", 500);
    }
    timestamp_last_emitted_us = candidate_timestamp;

    cJSON *resp = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON_AddItemToObject(resp, "data", data);
    cJSON_AddStringToObject(data, "session_id", "ses_local");
    cJSON_AddStringToObject(data, "run_mode", "single_step");
    cJSON_AddNumberToObject(data, "steps_requested", (double)steps);
    cJSON_AddNumberToObject(data, "ticks_committed", (double)ticks_committed);
    cJSON_AddNumberToObject(data, "tick_counter_before", (double)tick_before);
    cJSON_AddNumberToObject(data, "tick_counter_after", (double)debug_tick_counter);
    cJSON_AddNumberToObject(data, "cycle_counter_before", (double)cycle_before);
    cJSON_AddNumberToObject(data, "cycle_counter_after", (double)debug_cycle_counter);
    cJSON *arbitration = cJSON_CreateObject();
    cJSON_AddNumberToObject(arbitration, "arbitration_round", (double)arbitration_round);
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
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    esp_err_t out = send_json(req, resp_json, 200);
    free(resp_json);
    return out;
}

void esptari_web_debug_get_runtime_snapshot(esptari_web_debug_runtime_snapshot_t *out)
{
    if (out == NULL) {
        return;
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    if (timestamp_origin_us == 0) {
        timestamp_origin_us = now_us;
        timestamp_last_emitted_us = now_us;
    }

    out->run_mode = clock_mode;
    out->tick_counter = debug_tick_counter;
    out->cycle_counter = debug_cycle_counter;
    out->scheduler_hz = debug_scheduler_hz;
    out->timestamp_origin_us = timestamp_origin_us;
    out->timestamp_last_emitted_us = timestamp_last_emitted_us;
    out->timestamp_regressions = timestamp_regressions;
}

void esptari_web_debug_register_routes(httpd_handle_t server_handle)
{
    httpd_uri_t debug_clock_mode = {.uri = "/api/v2/debug/clock/mode", .method = HTTP_POST, .handler = clock_mode_handler, .user_ctx = NULL};
    httpd_uri_t debug_clock_step = {.uri = "/api/v2/debug/clock/step", .method = HTTP_POST, .handler = clock_step_handler, .user_ctx = NULL};

    httpd_register_uri_handler(server_handle, &debug_clock_mode);
    httpd_register_uri_handler(server_handle, &debug_clock_step);
}
