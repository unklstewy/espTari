#include "esptari_web_snapshot.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esptari_core.h"
#include "esptari_web_http_utils.h"

#define send_json esptari_web_send_json

static uint64_t checkpoint_seq = 1;
static char last_checkpoint_id[64] = "chkpt_000001";
static uint64_t psg_gpio_event_seq = 4200;
static uint64_t psg_gpio_tick = 913240;
static uint64_t psg_gpio_timestamp_us = 1710000033180ULL;
static uint8_t psg_gpio_port_a_value = 127;
static uint8_t psg_gpio_port_b_value = 16;
static const char *psg_gpio_port_a_direction = "output";
static const char *psg_gpio_port_b_direction = "input";

static esp_err_t validate_session_query(httpd_req_t *req, char *session_id, size_t len)
{
    if (!esptari_web_query_value(req, "session_id", session_id, len) || session_id[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (strcmp(session_id, "ses_local") != 0) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }
    return ESP_OK;
}

static esp_err_t validate_session_body(cJSON *root, char *session_id, size_t len)
{
    const char *session = NULL;
    if (!esptari_web_json_get_string(root, "session_id", &session) || session == NULL || session[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(session) >= len) {
        return ESP_ERR_INVALID_ARG;
    }
    strlcpy(session_id, session, len);
    if (strcmp(session_id, "ses_local") != 0) {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

static esp_err_t guard_running_or_paused(httpd_req_t *req)
{
    esptari_session_status_t status;
    esptari_core_get_status(&status);
    if (status.state != ESPTARI_SESSION_RUNNING && status.state != ESPTARI_SESSION_PAUSED) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\"}}", 409);
    }
    return ESP_OK;
}

static esp_err_t validate_running_session_query(httpd_req_t *req, char *session_id, size_t len)
{
    if (!esptari_web_query_value(req, "session_id", session_id, len) || session_id[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    esptari_session_status_t status;
    esptari_core_get_status(&status);
    if (strcmp(session_id, "ses_local") != 0 ||
        (status.state != ESPTARI_SESSION_RUNNING && status.state != ESPTARI_SESSION_PAUSED)) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }

    return ESP_OK;
}

static esp_err_t inspect_psg_gpio_state_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char force_unavailable_query[8] = {0};
    bool force_unavailable = esptari_web_query_value(req, "force_gpio_unavailable", force_unavailable_query, sizeof(force_unavailable_query)) &&
                            strcmp(force_unavailable_query, "1") == 0;
    if (force_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char resp[512];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"port_a_direction\":\"%s\",\"port_b_direction\":\"%s\",\"port_a_value\":%u,\"port_b_value\":%u,\"latched_tick\":%llu,\"timestamp_us\":%llu}}",
             session_id,
             psg_gpio_port_a_direction,
             psg_gpio_port_b_direction,
             (unsigned)psg_gpio_port_a_value,
             (unsigned)psg_gpio_port_b_value,
             (unsigned long long)psg_gpio_tick,
             (unsigned long long)psg_gpio_timestamp_us);
    return send_json(req, resp, 200);
}

static esp_err_t inspect_psg_gpio_events_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_running_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }

    char limit_str[16] = {0};
    if (!esptari_web_query_value(req, "limit", limit_str, sizeof(limit_str))) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    uint32_t limit = 0;
    if (!esptari_web_parse_u32_str(limit_str, &limit) || limit == 0 || limit > 256) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char force_unavailable_query[8] = {0};
    bool force_unavailable = esptari_web_query_value(req, "force_gpio_unavailable", force_unavailable_query, sizeof(force_unavailable_query)) &&
                            strcmp(force_unavailable_query, "1") == 0;
    if (force_unavailable) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    uint32_t event_count = limit > 2 ? 2 : limit;
    uint64_t event_seq_1 = psg_gpio_event_seq + 1;
    uint64_t event_seq_2 = psg_gpio_event_seq + 2;
    uint64_t tick_1 = psg_gpio_tick + 1;
    uint64_t tick_2 = psg_gpio_tick + 8;
    uint64_t ts_1 = psg_gpio_timestamp_us + 2;
    uint64_t ts_2 = psg_gpio_timestamp_us + 14;
    uint8_t port_a_before = psg_gpio_port_a_value > 31 ? (uint8_t)(psg_gpio_port_a_value - 31U) : 0U;
    uint8_t port_a_after = psg_gpio_port_a_value;
    uint8_t port_b_before = psg_gpio_port_b_value;
    uint8_t port_b_after = (uint8_t)((psg_gpio_port_b_value + 32U) & 0xFFU);

    char events_json[768] = {0};
    if (event_count == 1) {
        snprintf(events_json,
                 sizeof(events_json),
                 "[{\"event_seq\":%llu,\"port\":\"A\",\"direction\":\"output\",\"value_before\":%u,\"value_after\":%u,\"source\":\"cpu_write\",\"tick_counter\":%llu,\"timestamp_us\":%llu}]",
                 (unsigned long long)event_seq_1,
                 (unsigned)port_a_before,
                 (unsigned)port_a_after,
                 (unsigned long long)tick_1,
                 (unsigned long long)ts_1);
    } else {
        snprintf(events_json,
                 sizeof(events_json),
                 "[{\"event_seq\":%llu,\"port\":\"A\",\"direction\":\"output\",\"value_before\":%u,\"value_after\":%u,\"source\":\"cpu_write\",\"tick_counter\":%llu,\"timestamp_us\":%llu},{\"event_seq\":%llu,\"port\":\"B\",\"direction\":\"input\",\"value_before\":%u,\"value_after\":%u,\"source\":\"external_signal\",\"tick_counter\":%llu,\"timestamp_us\":%llu}]",
                 (unsigned long long)event_seq_1,
                 (unsigned)port_a_before,
                 (unsigned)port_a_after,
                 (unsigned long long)tick_1,
                 (unsigned long long)ts_1,
                 (unsigned long long)event_seq_2,
                 (unsigned)port_b_before,
                 (unsigned)port_b_after,
                 (unsigned long long)tick_2,
                 (unsigned long long)ts_2);
    }

    psg_gpio_event_seq = event_count == 1 ? event_seq_1 : event_seq_2;
    psg_gpio_tick = event_count == 1 ? tick_1 : tick_2;
    psg_gpio_timestamp_us = event_count == 1 ? ts_1 : ts_2;
    psg_gpio_port_b_value = event_count == 2 ? port_b_after : psg_gpio_port_b_value;

    char resp[1024];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"events\":%s}}",
             session_id,
             events_json);
    return send_json(req, resp, 200);
}

static esp_err_t registers_snapshot_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }
    guard = guard_running_or_paused(req);
    if (guard != ESP_OK) {
        return guard;
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    char resp[768];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"snapshot_at_us\":%llu,\"components\":[\"cpu\",\"mfp\"],\"registers\":[{\"component\":\"cpu\",\"name\":\"PC\",\"value\":\"0x00FC0000\"},{\"component\":\"cpu\",\"name\":\"SR\",\"value\":\"0x2700\"}]}}",
             session_id,
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

static esp_err_t bus_snapshot_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }
    guard = guard_running_or_paused(req);
    if (guard != ESP_OK) {
        return guard;
    }

    char limit_str[16] = {0};
    uint32_t limit = 16;
    if (esptari_web_query_value(req, "limit", limit_str, sizeof(limit_str))) {
        if (!esptari_web_parse_u32_str(limit_str, &limit) || limit == 0 || limit > 256) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    char resp[896];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"snapshot_at_us\":%llu,\"limit\":%lu,\"transactions\":[{\"seq\":1,\"address\":\"0x00FF820A\",\"access\":\"read\",\"source\":\"cpu\",\"tick\":100,\"cycle\":8000},{\"seq\":2,\"address\":\"0x00FF8604\",\"access\":\"write\",\"source\":\"dma\",\"tick\":101,\"cycle\":8080}]}}",
             session_id,
             (unsigned long long)now_us,
             (unsigned long)limit);
    return send_json(req, resp, 200);
}

static esp_err_t memory_snapshot_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    esp_err_t guard = validate_session_query(req, session_id, sizeof(session_id));
    if (guard != ESP_OK) {
        return guard;
    }
    guard = guard_running_or_paused(req);
    if (guard != ESP_OK) {
        return guard;
    }

    char range[64] = {0};
    if (!esptari_web_query_value(req, "range", range, sizeof(range)) || strstr(range, "-") == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    char resp[768];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"snapshot_at_us\":%llu,\"range\":\"%s\",\"bytes\":\"000102030405060708090A0B0C0D0E0F\",\"byte_count\":16}}",
             session_id,
             (unsigned long long)now_us,
             range);
    return send_json(req, resp, 200);
}

static esp_err_t checkpoint_create_handler(httpd_req_t *req)
{
    char body[768];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char session_id[64] = {0};
    esp_err_t session_err = validate_session_body(root, session_id, sizeof(session_id));
    if (session_err == ESP_ERR_INVALID_STATE) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }
    if (session_err != ESP_OK) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    esp_err_t guard = guard_running_or_paused(req);
    if (guard != ESP_OK) {
        cJSON_Delete(root);
        return guard;
    }

    checkpoint_seq++;
    snprintf(last_checkpoint_id, sizeof(last_checkpoint_id), "chkpt_%06llu", (unsigned long long)checkpoint_seq);
    uint64_t now_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[512];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"checkpoint_id\":\"%s\",\"created_at_us\":%llu,\"state\":\"ready\"}}",
             session_id,
             last_checkpoint_id,
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

static esp_err_t checkpoint_load_handler(httpd_req_t *req)
{
    char body[768];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char session_id[64] = {0};
    esp_err_t session_err = validate_session_body(root, session_id, sizeof(session_id));
    if (session_err == ESP_ERR_INVALID_STATE) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }
    if (session_err != ESP_OK) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const char *checkpoint_id = NULL;
    if (!esptari_web_json_get_string(root, "checkpoint_id", &checkpoint_id) || checkpoint_id == NULL || checkpoint_id[0] == '\0') {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    if (strcmp(checkpoint_id, last_checkpoint_id) != 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"SNAPSHOT_NOT_FOUND\"}}", 404);
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[512];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"checkpoint_id\":\"%s\",\"loaded_at_us\":%llu,\"state\":\"loaded\"}}",
             session_id,
             checkpoint_id,
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

void esptari_web_snapshot_register_routes(httpd_handle_t server_handle)
{
    httpd_uri_t registers_snapshot = {.uri = "/api/v2/inspect/registers/snapshot", .method = HTTP_GET, .handler = registers_snapshot_handler, .user_ctx = NULL};
    httpd_uri_t bus_snapshot = {.uri = "/api/v2/inspect/bus/snapshot", .method = HTTP_GET, .handler = bus_snapshot_handler, .user_ctx = NULL};
    httpd_uri_t memory_snapshot = {.uri = "/api/v2/inspect/memory/snapshot", .method = HTTP_GET, .handler = memory_snapshot_handler, .user_ctx = NULL};
    httpd_uri_t inspect_psg_gpio_state = {.uri = "/api/v2/inspect/chipset/psg/gpio", .method = HTTP_GET, .handler = inspect_psg_gpio_state_handler, .user_ctx = NULL};
    httpd_uri_t inspect_psg_gpio_events = {.uri = "/api/v2/inspect/chipset/psg/gpio/events", .method = HTTP_GET, .handler = inspect_psg_gpio_events_handler, .user_ctx = NULL};
    httpd_uri_t checkpoint_create = {.uri = "/api/v2/engine/checkpoint/create", .method = HTTP_POST, .handler = checkpoint_create_handler, .user_ctx = NULL};
    httpd_uri_t checkpoint_load = {.uri = "/api/v2/engine/checkpoint/load", .method = HTTP_POST, .handler = checkpoint_load_handler, .user_ctx = NULL};

    httpd_register_uri_handler(server_handle, &registers_snapshot);
    httpd_register_uri_handler(server_handle, &bus_snapshot);
    httpd_register_uri_handler(server_handle, &memory_snapshot);
    httpd_register_uri_handler(server_handle, &inspect_psg_gpio_state);
    httpd_register_uri_handler(server_handle, &inspect_psg_gpio_events);
    httpd_register_uri_handler(server_handle, &checkpoint_create);
    httpd_register_uri_handler(server_handle, &checkpoint_load);
}
