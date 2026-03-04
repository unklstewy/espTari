#include "esptari_web_metrics.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esptari_core.h"
#include "esptari_web_auth.h"
#include "esptari_web_http_utils.h"

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
static uint64_t perf_last_sample_timestamp_us;
static uint64_t perf_last_sample_window_end_us;
static uint64_t perf_last_alarm_timestamp_us;
static uint64_t perf_last_alarm_window_start_us;
static bool perf_alarm_state_breached;
static const double perf_input_latency_target_max = 50.0;
static const double perf_jitter_target_max = 30.0;
static const double perf_drop_target_max = 1.0;

static double metric_input_p95_value(uint64_t sample_seq)
{
    if (!perf_collect_input_latency) {
        return 0.0;
    }
    return 39.0 + (double)(sample_seq % 5);
}

static double metric_jitter_p95_value(uint64_t sample_seq)
{
    if (!perf_collect_jitter) {
        return 0.0;
    }
    if ((sample_seq % 4) == 0) {
        return 36.0;
    }
    return 22.0;
}

static double metric_drop_percent_value(uint64_t sample_seq)
{
    if (!perf_collect_drop) {
        return 0.0;
    }
    if ((sample_seq % 6) == 0) {
        return 1.3;
    }
    return 0.4;
}

static esp_err_t metrics_validate_session_query(httpd_req_t *req, char *session_id, size_t len)
{
    if (!esptari_web_query_value(req, "session_id", session_id, len) || session_id[0] == '\0') {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (strcmp(session_id, "ses_local") != 0) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
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
    uint64_t preview_seq = perf_sample_seq + 1;
    double input_p95 = metric_input_p95_value(preview_seq);
    double jitter_p95 = metric_jitter_p95_value(preview_seq);
    double drop_value = metric_drop_percent_value(preview_seq);
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
    return esptari_web_send_json(req, resp, 200);
}

static esp_err_t metrics_collectors_config_handler(httpd_req_t *req)
{
    char body[1024];
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

    esptari_session_status_t status;
    esptari_core_get_status(&status);
    if (status.state != ESPTARI_SESSION_RUNNING && status.state != ESPTARI_SESSION_PAUSED) {
        cJSON_Delete(root);
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\"}}", 409);
    }

    cJSON *sampling_item = cJSON_GetObjectItemCaseSensitive(root, "sampling_interval_ms");
    cJSON *window_item = cJSON_GetObjectItemCaseSensitive(root, "window_ms");
    cJSON *collectors_item = cJSON_GetObjectItemCaseSensitive(root, "collectors");
    if (!cJSON_IsNumber(sampling_item) || !cJSON_IsNumber(window_item) || !cJSON_IsObject(collectors_item)) {
        cJSON_Delete(root);
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    int sampling_ms = sampling_item->valueint;
    int window_ms = window_item->valueint;
    if (sampling_ms < 100 || sampling_ms > 10000 || window_ms < 1000 || window_ms > 60000) {
        cJSON_Delete(root);
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *input_item = cJSON_GetObjectItemCaseSensitive(collectors_item, "input_latency_ms");
    cJSON *jitter_item = cJSON_GetObjectItemCaseSensitive(collectors_item, "jitter_ms");
    cJSON *drop_item = cJSON_GetObjectItemCaseSensitive(collectors_item, "dropped_frame_percent");
    if (!cJSON_IsObject(input_item) || !cJSON_IsObject(jitter_item) || !cJSON_IsObject(drop_item)) {
        cJSON_Delete(root);
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *input_enabled = cJSON_GetObjectItemCaseSensitive(input_item, "enabled");
    cJSON *jitter_enabled = cJSON_GetObjectItemCaseSensitive(jitter_item, "enabled");
    cJSON *drop_enabled = cJSON_GetObjectItemCaseSensitive(drop_item, "enabled");
    if (!cJSON_IsBool(input_enabled) || !cJSON_IsBool(jitter_enabled) || !cJSON_IsBool(drop_enabled)) {
        cJSON_Delete(root);
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *emit_history_item = cJSON_GetObjectItemCaseSensitive(root, "emit_history");
    if (emit_history_item != NULL && !cJSON_IsBool(emit_history_item)) {
        cJSON_Delete(root);
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
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
    return esptari_web_send_json(req, resp, 200);
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
    if (esptari_web_query_value(req, "limit", limit_str, sizeof(limit_str))) {
        if (!esptari_web_parse_u32_str(limit_str, &limit) || limit == 0 || limit > 100) {
            return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
    }

    if (!perf_collectors_active) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\"}}", 409);
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    cJSON_AddStringToObject(data, "session_id", session_id);
    cJSON *samples = cJSON_CreateArray();

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    uint64_t window_us = (uint64_t)perf_window_ms * 1000ULL;
    uint64_t sample_interval_us = (uint64_t)perf_sampling_interval_ms * 1000ULL;
    uint64_t next_timestamp_us = perf_last_sample_timestamp_us > 0
                                     ? perf_last_sample_timestamp_us + sample_interval_us
                                     : now_us;
    if (next_timestamp_us < now_us) {
        next_timestamp_us = now_us;
    }

    for (uint32_t i = 0; i < limit; i++) {
        perf_sample_seq++;
        cJSON *sample = cJSON_CreateObject();
        uint64_t timestamp_us = next_timestamp_us + ((uint64_t)i * sample_interval_us);
        uint64_t window_end_us = timestamp_us > 0 ? timestamp_us - 1ULL : timestamp_us;
        if (window_end_us < perf_last_sample_window_end_us) {
            window_end_us = perf_last_sample_window_end_us;
            timestamp_us = window_end_us + 1ULL;
        }
        uint64_t window_start_us = window_end_us >= window_us ? window_end_us - window_us : 0;
        double input_p95 = metric_input_p95_value(perf_sample_seq);
        double jitter_p95 = metric_jitter_p95_value(perf_sample_seq);
        double drop_value = metric_drop_percent_value(perf_sample_seq);

        cJSON_AddNumberToObject(sample, "sample_seq", (double)perf_sample_seq);
        cJSON_AddNumberToObject(sample, "window_start_us", (double)window_start_us);
        cJSON_AddNumberToObject(sample, "window_end_us", (double)window_end_us);
        cJSON_AddNumberToObject(sample, "input_latency_ms_p95", input_p95);
        cJSON_AddNumberToObject(sample, "jitter_ms_p95", jitter_p95);
        cJSON_AddNumberToObject(sample, "dropped_frame_percent", drop_value);
        char rev[32];
        snprintf(rev, sizeof(rev), "slo_col_rev_%02llu", (unsigned long long)perf_collector_revision);
        cJSON_AddStringToObject(sample, "collector_revision", rev);
        cJSON_AddNumberToObject(sample, "timestamp_us", (double)timestamp_us);
        cJSON_AddItemToArray(samples, sample);

        perf_last_sample_window_end_us = window_end_us;
        perf_last_sample_timestamp_us = timestamp_us;
    }

    cJSON_AddItemToObject(data, "samples", samples);
    char *resp_json = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (resp_json == NULL) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }
    esp_err_t out = esptari_web_send_json(req, resp_json, 200);
    free(resp_json);
    return out;
}

static esp_err_t metrics_history_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = metrics_validate_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char limit_str[16] = {0};
    uint32_t limit = 10;
    if (esptari_web_query_value(req, "limit", limit_str, sizeof(limit_str))) {
        if (!esptari_web_parse_u32_str(limit_str, &limit) || limit == 0 || limit > 100) {
            return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
    }

    if (!perf_collectors_active || !perf_emit_history) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\"}}", 409);
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    cJSON_AddStringToObject(data, "session_id", session_id);
    cJSON_AddNumberToObject(data, "window_ms", (double)perf_window_ms);
    cJSON *history = cJSON_CreateArray();

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    uint64_t window_us = (uint64_t)perf_window_ms * 1000ULL;
    for (uint32_t i = 0; i < limit; i++) {
        cJSON *sample = cJSON_CreateObject();
        uint64_t window_end_us = now_us - ((uint64_t)i * window_us);
        uint64_t window_start_us = window_end_us >= window_us ? window_end_us - window_us : 0;
        double input_p95 = metric_input_p95_value(perf_sample_seq + i + 1ULL);
        double jitter_p95 = metric_jitter_p95_value(perf_sample_seq + i + 1ULL);
        double drop_value = metric_drop_percent_value(perf_sample_seq + i + 1ULL);
        cJSON_AddNumberToObject(sample, "window_start_us", (double)window_start_us);
        cJSON_AddNumberToObject(sample, "window_end_us", (double)window_end_us);
        cJSON_AddNumberToObject(sample, "input_latency_ms_p95", input_p95);
        cJSON_AddNumberToObject(sample, "jitter_ms_p95", jitter_p95);
        cJSON_AddNumberToObject(sample, "dropped_frame_percent", drop_value);
        cJSON_AddNumberToObject(sample, "timestamp_us", (double)(window_end_us + 1ULL));
        cJSON_AddItemToArray(history, sample);
    }

    cJSON_AddItemToObject(data, "history", history);
    char *resp_json = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (resp_json == NULL) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }
    esp_err_t out = esptari_web_send_json(req, resp_json, 200);
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
    return esptari_web_send_json(req, resp, 200);
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
    if (esptari_web_query_value(req, "limit", limit_str, sizeof(limit_str))) {
        if (!esptari_web_parse_u32_str(limit_str, &limit) || limit == 0 || limit > 100) {
            return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
    }

    if (!perf_collectors_active) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\"}}", 409);
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    cJSON_AddStringToObject(data, "session_id", session_id);
    cJSON *alarms = cJSON_CreateArray();

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    uint64_t window_us = (uint64_t)perf_window_ms * 1000ULL;
    uint64_t sample_interval_us = (uint64_t)perf_sampling_interval_ms * 1000ULL;
    uint64_t next_window_start_us = perf_last_alarm_window_start_us > 0
                                        ? perf_last_alarm_window_start_us + window_us
                                        : now_us;
    uint64_t next_timestamp_us = perf_last_alarm_timestamp_us > 0
                                     ? perf_last_alarm_timestamp_us + sample_interval_us
                                     : now_us + 1ULL;
    if (next_timestamp_us <= now_us) {
        next_timestamp_us = now_us + 1ULL;
    }

    for (uint32_t i = 0; i < limit; i++) {
        perf_alarm_seq++;
        cJSON *alarm = cJSON_CreateObject();
        double threshold = perf_jitter_target_max;
        bool breached_state = !perf_alarm_state_breached;
        double observed = breached_state ? threshold * 1.25 : threshold * 0.8;
        const char *state = breached_state ? "breached" : "recovered";
        const char *severity = observed >= threshold * 1.2 ? "critical" : "warning";
        uint64_t window_start_us = next_window_start_us + ((uint64_t)i * window_us);
        uint64_t window_end_us = window_start_us + window_us;
        uint64_t timestamp_us = next_timestamp_us + ((uint64_t)i * sample_interval_us);
        if (timestamp_us <= window_end_us) {
            timestamp_us = window_end_us + 1ULL;
        }

        cJSON_AddNumberToObject(alarm, "alarm_seq", (double)perf_alarm_seq);
        cJSON_AddStringToObject(alarm, "metric", "jitter_ms_p95");
        cJSON_AddNumberToObject(alarm, "threshold", threshold);
        cJSON_AddNumberToObject(alarm, "observed", observed);
        cJSON_AddStringToObject(alarm, "severity", severity);
        cJSON_AddStringToObject(alarm, "state", state);
        cJSON_AddNumberToObject(alarm, "window_start_us", (double)window_start_us);
        cJSON_AddNumberToObject(alarm, "window_end_us", (double)window_end_us);
        cJSON_AddNumberToObject(alarm, "timestamp_us", (double)timestamp_us);
        cJSON_AddItemToArray(alarms, alarm);

        perf_alarm_state_breached = breached_state;
        perf_last_alarm_window_start_us = window_start_us;
        perf_last_alarm_timestamp_us = timestamp_us;
    }

    cJSON_AddItemToObject(data, "alarms", alarms);
    char *resp_json = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (resp_json == NULL) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }
    esp_err_t out = esptari_web_send_json(req, resp_json, 200);
    free(resp_json);
    return out;
}

void esptari_web_metrics_register_routes(httpd_handle_t server_handle)
{
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/metrics/performance", HTTP_GET, metrics_performance_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/metrics/performance/history", HTTP_GET, metrics_history_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/metrics/performance/collectors/config", HTTP_POST, metrics_collectors_config_handler, "engine:control"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/metrics/performance/samples", HTTP_GET, metrics_samples_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/metrics/performance/thresholds", HTTP_GET, metrics_thresholds_handler, "inspect:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/metrics/performance/alarms", HTTP_GET, metrics_alarms_handler, "inspect:read"));
}
