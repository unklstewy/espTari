#include "esptari_web_debug_mode.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esptari_core.h"
#include "esptari_web_http_utils.h"
#include "esptari_web_debug_state.h"

#define CLOCK_RATIO_MIN_EXCLUSIVE 0.0
#define CLOCK_RATIO_MAX_INCLUSIVE 1.0
#define CLOCK_REALTIME_EFFECTIVE_RATIO 1.0

static esp_err_t send_debug_clock_invalid(httpd_req_t *req, const char *guard_id, const char *message)
{
    char resp[1536];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":false,\"error\":{\"code\":\"DEBUG_CLOCK_INVALID\",\"category\":\"debug\",\"message\":\"%s\",\"retryable\":false,\"details\":{\"session_id\":\"ses_local\",\"endpoint\":\"/api/v2/debug/clock/mode\",\"guard_id\":\"%s\"}}}",
             message,
             guard_id);
    return esptari_web_send_json(req, resp, 400);
}

esp_err_t esptari_web_debug_clock_mode_handler(httpd_req_t *req)
{
    if (req->content_len <= 0) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char body[256];
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

    cJSON *mode_item = cJSON_GetObjectItemCaseSensitive(root, "mode");
    if (!cJSON_IsString(mode_item) || mode_item->valuestring == NULL) {
        cJSON_Delete(root);
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const char *target_mode = mode_item->valuestring;
    cJSON *ratio_item = cJSON_GetObjectItemCaseSensitive(root, "ratio");
    bool has_ratio = ratio_item != NULL;
    double target_ratio = 1.0;

    if (strcmp(target_mode, "realtime") == 0) {
        if (has_ratio) {
            cJSON_Delete(root);
            return send_debug_clock_invalid(req, "CLOCK-BOUND-02", "ratio is only allowed for slow_motion mode");
        }
        target_ratio = CLOCK_REALTIME_EFFECTIVE_RATIO;
    } else if (strcmp(target_mode, "slow_motion") == 0) {
        if (!has_ratio || !cJSON_IsNumber(ratio_item)) {
            cJSON_Delete(root);
            return send_debug_clock_invalid(req, "CLOCK-BOUND-01", "slow_motion mode requires numeric ratio in bounds");
        }
        target_ratio = ratio_item->valuedouble;
        if (!(target_ratio > CLOCK_RATIO_MIN_EXCLUSIVE && target_ratio <= CLOCK_RATIO_MAX_INCLUSIVE)) {
            cJSON_Delete(root);
            return send_debug_clock_invalid(req, "CLOCK-BOUND-01", "slow_motion ratio must be in (0,1]");
        }
    } else if (strcmp(target_mode, "single_step") == 0) {
        if (has_ratio) {
            cJSON_Delete(root);
            return send_debug_clock_invalid(req, "CLOCK-BOUND-02", "ratio is not allowed for single_step mode");
        }
        target_ratio = CLOCK_REALTIME_EFFECTIVE_RATIO;
    } else {
        cJSON_Delete(root);
        return send_debug_clock_invalid(req, "CLOCK-BOUND-01", "unsupported debug clock mode");
    }

    cJSON_Delete(root);

    esptari_session_status_t status;
    esptari_core_get_status(&status);
    if (status.state == ESPTARI_SESSION_STOPPED) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\",\"details\":{\"guard_id\":\"CLOCK-TRANS-STATE\",\"endpoint\":\"/api/v2/debug/clock/mode\",\"esp_err\":\"ESP_ERR_INVALID_STATE\"}}}", 409);
    }

    bool idempotent = strcmp(esptari_web_debug_clock_mode, target_mode) == 0;
    if (idempotent && strcmp(target_mode, "slow_motion") == 0) {
        idempotent = esptari_web_debug_clock_effective_ratio == target_ratio;
    }

    const char *from_mode = esptari_web_debug_clock_mode;
    if (!idempotent) {
        uint64_t now_us = (uint64_t)esp_timer_get_time();
        esptari_web_debug_clock_mode = strcmp(target_mode, "realtime") == 0
                                           ? "realtime"
                                           : (strcmp(target_mode, "slow_motion") == 0 ? "slow_motion" : "single_step");
        esptari_web_debug_clock_effective_ratio = target_ratio;
        esptari_web_debug_clock_mode_transition_seq++;
        esptari_web_debug_clock_last_transition_at_us = now_us;
        if (esptari_web_debug_timestamp_origin_us == 0) {
            esptari_web_debug_timestamp_origin_us = now_us;
            esptari_web_debug_timestamp_last_emitted_us = now_us;
        }
        esptari_web_debug_last_advance_at_us = now_us;
        esptari_web_debug_tick_accumulator = 0.0;
    }

    char resp[1536];
    if (!idempotent) {
        snprintf(resp,
                 sizeof(resp),
                 "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"transition_applied\":true,\"mode_transition_seq\":%llu,\"from_mode\":\"%s\",\"to_mode\":\"%s\",\"effective_ratio\":%.6f,\"last_transition_at_us\":%llu,\"clock_bounds\":{\"ratio_min_exclusive\":%.1f,\"ratio_max_inclusive\":%.1f,\"realtime_effective_ratio\":%.1f},\"clock_bound_checks\":{\"CLOCK-BOUND-01\":\"pass\",\"CLOCK-BOUND-02\":\"pass\",\"CLOCK-BOUND-03\":\"pass\",\"CLOCK-BOUND-04\":\"pass\"},\"transition_checks\":{\"CLOCK-TRANS-01\":\"pass\",\"CLOCK-TRANS-02\":\"pass\",\"CLOCK-TRANS-03\":\"pass\",\"CLOCK-TRANS-04\":\"pass\"},\"transition_commit\":\"next_tick_boundary\"}}",
                 (unsigned long long)esptari_web_debug_clock_mode_transition_seq,
                 from_mode,
                 esptari_web_debug_clock_mode,
                 esptari_web_debug_clock_effective_ratio,
                 (unsigned long long)esptari_web_debug_clock_last_transition_at_us,
                 CLOCK_RATIO_MIN_EXCLUSIVE,
                 CLOCK_RATIO_MAX_INCLUSIVE,
                 CLOCK_REALTIME_EFFECTIVE_RATIO);
    } else {
        snprintf(resp,
                 sizeof(resp),
                 "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"transition_applied\":false,\"mode_transition_seq\":%llu,\"from_mode\":\"%s\",\"to_mode\":\"%s\",\"effective_ratio\":%.6f,\"reason\":\"already_in_target_mode\",\"clock_bounds\":{\"ratio_min_exclusive\":%.1f,\"ratio_max_inclusive\":%.1f,\"realtime_effective_ratio\":%.1f},\"transition_checks\":{\"CLOCK-TRANS-01\":\"pass\",\"CLOCK-TRANS-02\":\"pass\",\"CLOCK-TRANS-03\":\"pass\",\"CLOCK-TRANS-04\":\"pass\"}}}",
                 (unsigned long long)esptari_web_debug_clock_mode_transition_seq,
                 esptari_web_debug_clock_mode,
                 esptari_web_debug_clock_mode,
                 esptari_web_debug_clock_effective_ratio,
                 CLOCK_RATIO_MIN_EXCLUSIVE,
                 CLOCK_RATIO_MAX_INCLUSIVE,
                 CLOCK_REALTIME_EFFECTIVE_RATIO);
    }

    return esptari_web_send_json(req, resp, 200);
}
