#include "esptari_web_input.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esptari_web_http_utils.h"

#define send_json esptari_web_send_json

static bool input_policy_enabled = true;
static bool input_capture_enabled;
static char input_capture_mode[32] = "mouse_over";
static char active_mapping_id[64] = "atari_st_default_v1";

static esp_err_t parse_body_json(httpd_req_t *req, char *body, size_t body_len, cJSON **out_root)
{
    if (esptari_web_read_request_body(req, body, body_len) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    *out_root = root;
    return ESP_OK;
}

static bool extract_session_id(cJSON *root, char *session_id, size_t len)
{
    const char *session = NULL;
    if (!esptari_web_json_get_string(root, "session_id", &session) || session == NULL || session[0] == '\0') {
        return false;
    }
    strlcpy(session_id, session, len);
    return strcmp(session_id, "ses_local") == 0;
}

static esp_err_t input_devices_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    if (!esptari_web_query_value(req, "session_id", session_id, sizeof(session_id)) || session_id[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (strcmp(session_id, "ses_local") != 0) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }

    char resp[640];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"devices\":[{\"id\":\"kbd_0\",\"type\":\"keyboard\",\"connected\":true},{\"id\":\"mouse_0\",\"type\":\"mouse\",\"connected\":true}]}}",
             session_id);
    return send_json(req, resp, 200);
}

static esp_err_t input_events_inject_handler(httpd_req_t *req)
{
    char body[768];
    cJSON *root = NULL;
    esp_err_t parse_err = parse_body_json(req, body, sizeof(body), &root);
    if (parse_err != ESP_OK) {
        return parse_err;
    }

    char session_id[64] = {0};
    if (!extract_session_id(root, session_id, sizeof(session_id))) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *events = cJSON_GetObjectItemCaseSensitive(root, "events");
    if (!cJSON_IsArray(events)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    int accepted = cJSON_GetArraySize(events);
    uint64_t now_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[320];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"accepted\":%d,\"processed_at_us\":%llu}}",
             session_id,
             accepted,
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

static esp_err_t input_capture_state_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    if (!esptari_web_query_value(req, "session_id", session_id, sizeof(session_id)) || session_id[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (strcmp(session_id, "ses_local") != 0) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }

    char browser_session_id[64] = {0};
    if (!esptari_web_query_value(req, "browser_session_id", browser_session_id, sizeof(browser_session_id))) {
        strlcpy(browser_session_id, "browser_local", sizeof(browser_session_id));
    }

    char resp[448];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"browser_session_id\":\"%s\",\"policy_enabled\":%s,\"capture_enabled\":%s,\"capture_mode\":\"%s\"}}",
             session_id,
             browser_session_id,
             input_policy_enabled ? "true" : "false",
             input_capture_enabled ? "true" : "false",
             input_capture_mode);
    return send_json(req, resp, 200);
}

static esp_err_t input_policy_enabled_handler(httpd_req_t *req)
{
    char body[512];
    cJSON *root = NULL;
    esp_err_t parse_err = parse_body_json(req, body, sizeof(body), &root);
    if (parse_err != ESP_OK) {
        return parse_err;
    }
    char session_id[64] = {0};
    if (!extract_session_id(root, session_id, sizeof(session_id))) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *enabled_item = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    if (!cJSON_IsBool(enabled_item)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    bool requested = cJSON_IsTrue(enabled_item);
    const char *result = requested == input_policy_enabled ? "no_op" : "applied";
    input_policy_enabled = requested;
    uint64_t now_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[352];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"enabled\":%s,\"result\":\"%s\",\"changed_at_us\":%llu}}",
             session_id,
             input_policy_enabled ? "true" : "false",
             result,
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

static esp_err_t input_capture_config_handler(httpd_req_t *req)
{
    char body[768];
    cJSON *root = NULL;
    esp_err_t parse_err = parse_body_json(req, body, sizeof(body), &root);
    if (parse_err != ESP_OK) {
        return parse_err;
    }
    char session_id[64] = {0};
    if (!extract_session_id(root, session_id, sizeof(session_id))) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *enabled_item = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    cJSON *mode_item = cJSON_GetObjectItemCaseSensitive(root, "capture_mode");
    if (!cJSON_IsBool(enabled_item) || !cJSON_IsString(mode_item) || mode_item->valuestring == NULL) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (strcmp(mode_item->valuestring, "mouse_over") != 0 && strcmp(mode_item->valuestring, "click_to_capture") != 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    input_capture_enabled = cJSON_IsTrue(enabled_item);
    strlcpy(input_capture_mode, mode_item->valuestring, sizeof(input_capture_mode));
    uint64_t now_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[384];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"capture_enabled\":%s,\"capture_mode\":\"%s\",\"changed_at_us\":%llu}}",
             session_id,
             input_capture_enabled ? "true" : "false",
             input_capture_mode,
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

static esp_err_t input_capture_release_handler(httpd_req_t *req)
{
    char body[512];
    cJSON *root = NULL;
    esp_err_t parse_err = parse_body_json(req, body, sizeof(body), &root);
    if (parse_err != ESP_OK) {
        return parse_err;
    }
    char session_id[64] = {0};
    if (!extract_session_id(root, session_id, sizeof(session_id))) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    bool was_enabled = input_capture_enabled;
    input_capture_enabled = false;
    uint64_t now_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[352];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"result\":\"%s\",\"released_at_us\":%llu}}",
             session_id,
             was_enabled ? "released" : "no_op",
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

static esp_err_t input_mappings_load_handler(httpd_req_t *req)
{
    char body[768];
    cJSON *root = NULL;
    esp_err_t parse_err = parse_body_json(req, body, sizeof(body), &root);
    if (parse_err != ESP_OK) {
        return parse_err;
    }
    char session_id[64] = {0};
    if (!extract_session_id(root, session_id, sizeof(session_id))) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const char *mapping_profile_id = NULL;
    if (!esptari_web_json_get_string(root, "mapping_profile_id", &mapping_profile_id) || mapping_profile_id == NULL || mapping_profile_id[0] == '\0') {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    strlcpy(active_mapping_id, mapping_profile_id, sizeof(active_mapping_id));
    uint64_t now_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[384];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"mapping_profile_id\":\"%s\",\"loaded_at_us\":%llu}}",
             session_id,
             active_mapping_id,
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

static esp_err_t input_mappings_update_handler(httpd_req_t *req)
{
    char body[1024];
    cJSON *root = NULL;
    esp_err_t parse_err = parse_body_json(req, body, sizeof(body), &root);
    if (parse_err != ESP_OK) {
        return parse_err;
    }
    char session_id[64] = {0};
    if (!extract_session_id(root, session_id, sizeof(session_id))) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *entries = cJSON_GetObjectItemCaseSensitive(root, "entries");
    if (entries != NULL && !cJSON_IsArray(entries)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    int changed = entries != NULL ? cJSON_GetArraySize(entries) : 0;
    uint64_t now_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[384];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"mapping_profile_id\":\"%s\",\"changed_entries\":%d,\"updated_at_us\":%llu}}",
             session_id,
             active_mapping_id,
             changed,
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

static esp_err_t input_stream_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    if (!esptari_web_query_value(req, "session_id", session_id, sizeof(session_id)) || session_id[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (strcmp(session_id, "ses_local") != 0) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    char resp[448];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"stream\":\"input\",\"policy_enabled\":%s,\"capture_mode\":\"%s\",\"event_seq\":1,\"event_timestamp_us\":%llu}}",
             session_id,
             input_policy_enabled ? "true" : "false",
             input_capture_mode,
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

void esptari_web_input_register_routes(httpd_handle_t server_handle)
{
    httpd_uri_t input_devices = {.uri = "/api/v2/input/devices", .method = HTTP_GET, .handler = input_devices_handler, .user_ctx = NULL};
    httpd_uri_t input_events_inject = {.uri = "/api/v2/input/events/inject", .method = HTTP_POST, .handler = input_events_inject_handler, .user_ctx = NULL};
    httpd_uri_t input_capture_state = {.uri = "/api/v2/input/capture/state", .method = HTTP_GET, .handler = input_capture_state_handler, .user_ctx = NULL};
    httpd_uri_t input_policy_enabled_uri = {.uri = "/api/v2/input/policy/enabled", .method = HTTP_POST, .handler = input_policy_enabled_handler, .user_ctx = NULL};
    httpd_uri_t input_capture_config = {.uri = "/api/v2/input/capture/config", .method = HTTP_POST, .handler = input_capture_config_handler, .user_ctx = NULL};
    httpd_uri_t input_capture_release = {.uri = "/api/v2/input/capture/release", .method = HTTP_POST, .handler = input_capture_release_handler, .user_ctx = NULL};
    httpd_uri_t input_mappings_load = {.uri = "/api/v2/input/mappings/load", .method = HTTP_POST, .handler = input_mappings_load_handler, .user_ctx = NULL};
    httpd_uri_t input_mappings_update = {.uri = "/api/v2/input/mappings/update", .method = HTTP_POST, .handler = input_mappings_update_handler, .user_ctx = NULL};
    httpd_uri_t input_stream = {.uri = "/api/v2/input/stream", .method = HTTP_GET, .handler = input_stream_handler, .user_ctx = NULL};

    httpd_register_uri_handler(server_handle, &input_devices);
    httpd_register_uri_handler(server_handle, &input_events_inject);
    httpd_register_uri_handler(server_handle, &input_capture_state);
    httpd_register_uri_handler(server_handle, &input_policy_enabled_uri);
    httpd_register_uri_handler(server_handle, &input_capture_config);
    httpd_register_uri_handler(server_handle, &input_capture_release);
    httpd_register_uri_handler(server_handle, &input_mappings_load);
    httpd_register_uri_handler(server_handle, &input_mappings_update);
    httpd_register_uri_handler(server_handle, &input_stream);
}
