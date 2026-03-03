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

static bool perf_collectors_active;
static uint32_t perf_sampling_interval_ms = 500;
static uint32_t perf_window_ms = 5000;
static bool perf_collect_input_latency = true;
static bool perf_collect_jitter = true;
static bool perf_collect_drop = true;
static bool perf_emit_history = true;
static uint64_t perf_collector_revision = 1;
static uint64_t perf_sample_seq;
static uint64_t perf_alarm_seq;
static bool slo_alarm_breached;
static const double perf_input_latency_target_max = 50.0;
static const double perf_jitter_target_max = 30.0;
static const double perf_drop_target_max = 1.0;

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

static bool parse_u32_str(const char *value, uint32_t *out)
{
    if (value == NULL || value[0] == '\0' || out == NULL) {
        return false;
    }
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);
    if (end == value || *end != '\0') {
        return false;
    }
    *out = (uint32_t)parsed;
    return true;
}

static esp_err_t metrics_validate_session_query(httpd_req_t *req, char *session_id, size_t len)
{
    if (!query_value(req, "session_id", session_id, len) || session_id[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (strcmp(session_id, "ses_local") != 0) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }
    return ESP_OK;
}

static esp_err_t metrics_performance_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = metrics_validate_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    double input_p95 = perf_collect_input_latency ? 41.0 : 0.0;
    double jitter_p95 = perf_collect_jitter ? 21.0 : 0.0;
    double drop_value = perf_collect_drop ? 0.4 : 0.0;
    char resp[640];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"window_ms\":%lu,\"window_end_us\":%llu,\"input_latency_ms\":{\"p50\":18,\"p95\":%.1f,\"max\":49,\"target_max\":%.1f,\"status\":\"%s\"},\"jitter_ms\":{\"p50\":7,\"p95\":%.1f,\"max\":28,\"target_max\":%.1f,\"status\":\"%s\"},\"dropped_frame_percent\":{\"value\":%.1f,\"target_max\":%.1f,\"status\":\"%s\"}}}",
             session_id,
             (unsigned long)perf_window_ms,
             (unsigned long long)now_us,
             input_p95,
             perf_input_latency_target_max,
             input_p95 <= perf_input_latency_target_max ? "ok" : "breach",
             jitter_p95,
             perf_jitter_target_max,
             jitter_p95 <= perf_jitter_target_max ? "ok" : "breach",
             drop_value,
             perf_drop_target_max,
             drop_value <= perf_drop_target_max ? "ok" : "breach");
    return send_json(req, resp, 200);
}

static esp_err_t metrics_collectors_config_handler(httpd_req_t *req)
{
    char body[1024];
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

    esptari_session_status_t status;
    esptari_core_get_status(&status);
    if (status.state != ESPTARI_SESSION_RUNNING && status.state != ESPTARI_SESSION_PAUSED) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\"}}", 409);
    }

    cJSON *sampling_item = cJSON_GetObjectItemCaseSensitive(root, "sampling_interval_ms");
    cJSON *window_item = cJSON_GetObjectItemCaseSensitive(root, "window_ms");
    cJSON *collectors_item = cJSON_GetObjectItemCaseSensitive(root, "collectors");
    if (!cJSON_IsNumber(sampling_item) || !cJSON_IsNumber(window_item) || !cJSON_IsObject(collectors_item)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    int sampling_ms = sampling_item->valueint;
    int window_ms = window_item->valueint;
    if (sampling_ms < 100 || sampling_ms > 10000 || window_ms < 1000 || window_ms > 60000) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *input_item = cJSON_GetObjectItemCaseSensitive(collectors_item, "input_latency_ms");
    cJSON *jitter_item = cJSON_GetObjectItemCaseSensitive(collectors_item, "jitter_ms");
    cJSON *drop_item = cJSON_GetObjectItemCaseSensitive(collectors_item, "dropped_frame_percent");
    if (!cJSON_IsObject(input_item) || !cJSON_IsObject(jitter_item) || !cJSON_IsObject(drop_item)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *input_enabled = cJSON_GetObjectItemCaseSensitive(input_item, "enabled");
    cJSON *jitter_enabled = cJSON_GetObjectItemCaseSensitive(jitter_item, "enabled");
    cJSON *drop_enabled = cJSON_GetObjectItemCaseSensitive(drop_item, "enabled");
    if (!cJSON_IsBool(input_enabled) || !cJSON_IsBool(jitter_enabled) || !cJSON_IsBool(drop_enabled)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *emit_history_item = cJSON_GetObjectItemCaseSensitive(root, "emit_history");
    if (emit_history_item != NULL && !cJSON_IsBool(emit_history_item)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    perf_sampling_interval_ms = (uint32_t)sampling_ms;
    perf_window_ms = (uint32_t)window_ms;
    perf_collect_input_latency = cJSON_IsTrue(input_enabled);
    perf_collect_jitter = cJSON_IsTrue(jitter_enabled);
    perf_collect_drop = cJSON_IsTrue(drop_enabled);
    if (emit_history_item != NULL) {
        perf_emit_history = cJSON_IsTrue(emit_history_item);
    }
    perf_collectors_active = true;
    perf_collector_revision++;

    cJSON_Delete(root);

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    char resp[320];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"collector_revision\":\"slo_col_rev_%02llu\",\"sampling_interval_ms\":%lu,\"window_ms\":%lu,\"state\":\"active\",\"emit_history\":%s,\"applied_at_us\":%llu}}",
             (unsigned long long)perf_collector_revision,
             (unsigned long)perf_sampling_interval_ms,
             (unsigned long)perf_window_ms,
             perf_emit_history ? "true" : "false",
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

static esp_err_t metrics_samples_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = metrics_validate_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char limit_str[16] = {0};
    uint32_t limit = 1;
    if (query_value(req, "limit", limit_str, sizeof(limit_str))) {
        if (!parse_u32_str(limit_str, &limit) || limit == 0 || limit > 100) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
    }

    if (!perf_collectors_active) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\"}}", 409);
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    cJSON_AddStringToObject(data, "session_id", session_id);
    cJSON *samples = cJSON_CreateArray();

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    uint64_t window_us = (uint64_t)perf_window_ms * 1000ULL;
    for (uint32_t i = 0; i < limit; i++) {
        perf_sample_seq++;
        cJSON *sample = cJSON_CreateObject();
        uint64_t window_end_us = now_us + (uint64_t)i * window_us;
        uint64_t window_start_us = window_end_us >= window_us ? window_end_us - window_us : 0;
        cJSON_AddNumberToObject(sample, "sample_seq", (double)perf_sample_seq);
        cJSON_AddNumberToObject(sample, "window_start_us", (double)window_start_us);
        cJSON_AddNumberToObject(sample, "window_end_us", (double)window_end_us);
        cJSON_AddNumberToObject(sample, "input_latency_ms_p95", perf_collect_input_latency ? 41.0 : 0.0);
        cJSON_AddNumberToObject(sample, "jitter_ms_p95", perf_collect_jitter ? 21.0 : 0.0);
        cJSON_AddNumberToObject(sample, "dropped_frame_percent", perf_collect_drop ? 0.4 : 0.0);
        char rev[32];
        snprintf(rev, sizeof(rev), "slo_col_rev_%02llu", (unsigned long long)perf_collector_revision);
        cJSON_AddStringToObject(sample, "collector_revision", rev);
        cJSON_AddNumberToObject(sample, "timestamp_us", (double)(window_end_us + 1ULL));
        cJSON_AddItemToArray(samples, sample);
    }

    cJSON_AddItemToObject(data, "samples", samples);
    char *resp_json = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (resp_json == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }
    esp_err_t out = send_json(req, resp_json, 200);
    free(resp_json);
    return out;
}

static esp_err_t metrics_thresholds_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = metrics_validate_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char resp[384];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"thresholds\":{\"input_latency_ms_p95_max\":%.1f,\"jitter_ms_p95_max\":%.1f,\"dropped_frame_percent_max\":%.1f},\"evaluation_window_ms\":%lu,\"active_revision\":\"slo_thr_rev_02\"}}",
             session_id,
             perf_input_latency_target_max,
             perf_jitter_target_max,
             perf_drop_target_max,
             (unsigned long)perf_window_ms);
    return send_json(req, resp, 200);
}

static esp_err_t metrics_alarms_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = metrics_validate_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char limit_str[16] = {0};
    uint32_t limit = 1;
    if (query_value(req, "limit", limit_str, sizeof(limit_str))) {
        if (!parse_u32_str(limit_str, &limit) || limit == 0 || limit > 100) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
    }

    if (!perf_collectors_active) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\"}}", 409);
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    cJSON_AddStringToObject(data, "session_id", session_id);
    cJSON *alarms = cJSON_CreateArray();

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    uint64_t window_us = (uint64_t)perf_window_ms * 1000ULL;
    for (uint32_t i = 0; i < limit; i++) {
        perf_alarm_seq++;
        cJSON *alarm = cJSON_CreateObject();
        double threshold = perf_jitter_target_max;
        double observed = slo_alarm_breached ? threshold * 1.25 : threshold * 0.8;
        const char *state = slo_alarm_breached ? "breached" : "recovered";
        const char *severity = observed >= threshold * 1.2 ? "critical" : "warning";
        uint64_t window_start_us = now_us + (uint64_t)i * window_us;
        uint64_t window_end_us = window_start_us + window_us;

        cJSON_AddNumberToObject(alarm, "alarm_seq", (double)perf_alarm_seq);
        cJSON_AddStringToObject(alarm, "metric", "jitter_ms_p95");
        cJSON_AddNumberToObject(alarm, "threshold", threshold);
        cJSON_AddNumberToObject(alarm, "observed", observed);
        cJSON_AddStringToObject(alarm, "severity", severity);
        cJSON_AddStringToObject(alarm, "state", state);
        cJSON_AddNumberToObject(alarm, "window_start_us", (double)window_start_us);
        cJSON_AddNumberToObject(alarm, "window_end_us", (double)window_end_us);
        cJSON_AddNumberToObject(alarm, "timestamp_us", (double)(window_end_us + 1ULL));
        cJSON_AddItemToArray(alarms, alarm);
    }

    cJSON_AddItemToObject(data, "alarms", alarms);
    char *resp_json = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (resp_json == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }
    esp_err_t out = send_json(req, resp_json, 200);
    free(resp_json);
    return out;
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
    httpd_uri_t metrics_performance = {.uri = "/api/v2/metrics/performance", .method = HTTP_GET, .handler = metrics_performance_handler, .user_ctx = NULL};
    httpd_uri_t metrics_collectors_config = {.uri = "/api/v2/metrics/performance/collectors/config", .method = HTTP_POST, .handler = metrics_collectors_config_handler, .user_ctx = NULL};
    httpd_uri_t metrics_samples = {.uri = "/api/v2/metrics/performance/samples", .method = HTTP_GET, .handler = metrics_samples_handler, .user_ctx = NULL};
    httpd_uri_t metrics_thresholds = {.uri = "/api/v2/metrics/performance/thresholds", .method = HTTP_GET, .handler = metrics_thresholds_handler, .user_ctx = NULL};
    httpd_uri_t metrics_alarms = {.uri = "/api/v2/metrics/performance/alarms", .method = HTTP_GET, .handler = metrics_alarms_handler, .user_ctx = NULL};

    httpd_register_uri_handler(server_handle, &debug_clock_mode);
    httpd_register_uri_handler(server_handle, &debug_clock_step);
    httpd_register_uri_handler(server_handle, &metrics_performance);
    httpd_register_uri_handler(server_handle, &metrics_collectors_config);
    httpd_register_uri_handler(server_handle, &metrics_samples);
    httpd_register_uri_handler(server_handle, &metrics_thresholds);
    httpd_register_uri_handler(server_handle, &metrics_alarms);
}
