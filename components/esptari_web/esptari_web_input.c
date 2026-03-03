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

static char input_capture_mode[32] = "mouse_over";
static char active_mapping_id[64] = "atari_st_default_v1";
static bool input_enabled = true;
static bool pointer_over_canvas;
static bool capture_active;
static bool browser_focus = true;
static bool escape_release_enabled = true;
static uint32_t escape_release_timeout_ms = 600;
static const char *escape_release_sequence[2] = {"Escape", "Escape"};
static char policy_state[24] = "enabled_idle";
static char policy_source[24] = "user_request";
static char policy_reason[64] = "init";
static uint64_t policy_changed_at_us;
static uint64_t policy_event_seq;
static char policy_owner_browser_session_id[64] = "browser_local";

static bool mode_is_mouse_over(void)
{
    return strcmp(input_capture_mode, "mouse_over") == 0;
}

static bool mode_is_click_to_capture(void)
{
    return strcmp(input_capture_mode, "click_to_capture") == 0;
}

static void set_policy_metadata(const char *source, const char *reason)
{
    if (source != NULL) {
        strlcpy(policy_source, source, sizeof(policy_source));
    }
    if (reason != NULL) {
        strlcpy(policy_reason, reason, sizeof(policy_reason));
    }
    policy_changed_at_us = (uint64_t)esp_timer_get_time();
    policy_event_seq++;
}

static void recompute_policy_state(const char *source, const char *reason)
{
    if (!input_enabled) {
        capture_active = false;
        strlcpy(policy_state, "disabled", sizeof(policy_state));
        set_policy_metadata(source, reason != NULL ? reason : "input_disabled");
        return;
    }

    if (mode_is_mouse_over()) {
        capture_active = pointer_over_canvas;
    }

    strlcpy(policy_state, capture_active ? "enabled_captured" : "enabled_idle", sizeof(policy_state));
    set_policy_metadata(source, reason != NULL ? reason : (capture_active ? "capture_active" : "capture_idle"));
}

static bool browser_policy_guard(const char *browser_session_id)
{
    if (browser_session_id == NULL || browser_session_id[0] == '\0') {
        return false;
    }
    return strcmp(browser_session_id, policy_owner_browser_session_id) == 0;
}

static bool extract_browser_session_id(cJSON *root, char *browser_session_id, size_t len)
{
    const char *browser_session = NULL;
    if (!esptari_web_json_get_string(root, "browser_session_id", &browser_session) || browser_session == NULL || browser_session[0] == '\0') {
        return false;
    }
    strlcpy(browser_session_id, browser_session, len);
    return true;
}

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

    char browser_session_id[64] = {0};
    if (!extract_browser_session_id(root, browser_session_id, sizeof(browser_session_id))) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INPUT_POLICY_SESSION_INVALID\"}}", 409);
    }
    if (!browser_policy_guard(browser_session_id)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INPUT_POLICY_VIOLATION\"}}", 409);
    }

    cJSON *events = cJSON_GetObjectItemCaseSensitive(root, "events");
    if (!cJSON_IsArray(events)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (!input_enabled) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INPUT_CAPTURE_DISABLED\"}}", 409);
    }
    if (!capture_active) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INPUT_CAPTURE_NOT_ACTIVE\"}}", 409);
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
    if (!esptari_web_query_value(req, "browser_session_id", browser_session_id, sizeof(browser_session_id)) || browser_session_id[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INPUT_POLICY_SESSION_INVALID\"}}", 409);
    }
    if (!browser_policy_guard(browser_session_id)) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INPUT_POLICY_VIOLATION\"}}", 409);
    }

    char resp[960];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"browser_session_id\":\"%s\",\"input_enabled\":%s,\"capture_mode\":\"%s\",\"capture_active\":%s,\"policy\":{\"state\":\"%s\",\"source\":\"%s\",\"reason\":\"%s\",\"changed_at_us\":%llu,\"event_seq\":%llu},\"escape_release\":{\"enabled\":%s,\"sequence\":[\"%s\",\"%s\"],\"timeout_ms\":%lu}}}",
             session_id,
             browser_session_id,
             input_enabled ? "true" : "false",
             input_capture_mode,
             capture_active ? "true" : "false",
             policy_state,
             policy_source,
             policy_reason,
             (unsigned long long)policy_changed_at_us,
             (unsigned long long)policy_event_seq,
             escape_release_enabled ? "true" : "false",
             escape_release_sequence[0],
             escape_release_sequence[1],
             (unsigned long)escape_release_timeout_ms);
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

    char browser_session_id[64] = {0};
    if (!extract_browser_session_id(root, browser_session_id, sizeof(browser_session_id))) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INPUT_POLICY_SESSION_INVALID\"}}", 409);
    }
    if (!browser_policy_guard(browser_session_id)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INPUT_POLICY_VIOLATION\"}}", 409);
    }

    cJSON *enabled_item = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    if (!cJSON_IsBool(enabled_item)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    bool requested = cJSON_IsTrue(enabled_item);
    const char *previous_state = policy_state;
    bool changed = requested != input_enabled;
    input_enabled = requested;
    if (!input_enabled) {
        capture_active = false;
    }
    recompute_policy_state("user_request", changed ? "user_toggle" : (requested ? "idempotent_enable" : "idempotent_disable"));
    cJSON_Delete(root);

    char resp[640];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"browser_session_id\":\"%s\",\"requested_enabled\":%s,\"result\":\"%s\",\"previous_policy_state\":\"%s\",\"policy\":{\"state\":\"%s\",\"source\":\"%s\",\"reason\":\"%s\",\"changed_at_us\":%llu}}}",
             session_id,
             browser_session_id,
             requested ? "true" : "false",
             changed ? "applied" : "no_op",
             previous_state,
             policy_state,
             policy_source,
             policy_reason,
             (unsigned long long)policy_changed_at_us);
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

    cJSON *enabled_item = cJSON_GetObjectItemCaseSensitive(root, "input_enabled");
    if (enabled_item == NULL) {
        enabled_item = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    }
    cJSON *mode_item = cJSON_GetObjectItemCaseSensitive(root, "capture_mode");
    cJSON *action_item = cJSON_GetObjectItemCaseSensitive(root, "request_action");
    cJSON *browser_item = cJSON_GetObjectItemCaseSensitive(root, "browser_session_id");
    if (!cJSON_IsBool(enabled_item) || !cJSON_IsString(mode_item) || mode_item->valuestring == NULL ||
        !cJSON_IsString(browser_item) || browser_item->valuestring == NULL || browser_item->valuestring[0] == '\0') {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INPUT_POLICY_SESSION_INVALID\"}}", 409);
    }
    if (strcmp(mode_item->valuestring, "mouse_over") != 0 && strcmp(mode_item->valuestring, "click_to_capture") != 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INPUT_POLICY_MODE_INVALID\"}}", 409);
    }

    const char *browser_session_id = browser_item->valuestring;
    if (!browser_policy_guard(browser_session_id)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INPUT_POLICY_VIOLATION\"}}", 409);
    }

    input_enabled = cJSON_IsTrue(enabled_item);
    strlcpy(input_capture_mode, mode_item->valuestring, sizeof(input_capture_mode));

    const char *action = "capture_config_applied";
    if (action_item != NULL) {
        if (!cJSON_IsString(action_item) || action_item->valuestring == NULL || action_item->valuestring[0] == '\0') {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INPUT_POLICY_MODE_INVALID\"}}", 409);
        }
        action = action_item->valuestring;
    }

    const char *transition_result = "no_op";
    if (!input_enabled) {
        capture_active = false;
        recompute_policy_state("system_guard", "input_disabled");
    } else if (strcmp(action, "pointer_enter_hook") == 0) {
        if (!mode_is_mouse_over()) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INPUT_POLICY_MODE_INVALID\"}}", 409);
        }
        bool prev = capture_active;
        pointer_over_canvas = true;
        recompute_policy_state("system_guard", prev ? "mouse_over_pointer_in_no_op" : "mouse_over_pointer_in");
        transition_result = prev ? "no_op" : "applied";
    } else if (strcmp(action, "pointer_leave_hook") == 0) {
        if (!mode_is_mouse_over()) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INPUT_POLICY_MODE_INVALID\"}}", 409);
        }
        bool prev = capture_active;
        pointer_over_canvas = false;
        recompute_policy_state("system_guard", prev ? "mouse_over_pointer_out" : "mouse_over_pointer_out_no_op");
        transition_result = prev ? "applied" : "no_op";
    } else if (strcmp(action, "click_acquire") == 0) {
        if (!mode_is_click_to_capture()) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INPUT_POLICY_MODE_INVALID\"}}", 409);
        }
        if (!capture_active) {
            capture_active = true;
            recompute_policy_state("system_guard", "click_to_capture_acquired");
            transition_result = "applied";
        } else {
            recompute_policy_state("system_guard", "click_to_capture_already_active");
            transition_result = "no_op";
        }
    } else if (strcmp(action, "focus_lost") == 0) {
        if (!mode_is_click_to_capture()) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INPUT_POLICY_MODE_INVALID\"}}", 409);
        }
        browser_focus = false;
        if (capture_active) {
            capture_active = false;
            recompute_policy_state("system_guard", "focus_lost_release");
            transition_result = "applied";
        } else {
            recompute_policy_state("system_guard", "focus_lost_idle");
            transition_result = "no_op";
        }
    } else if (strcmp(action, "focus_regained") == 0) {
        if (!mode_is_click_to_capture()) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INPUT_POLICY_MODE_INVALID\"}}", 409);
        }
        browser_focus = true;
        recompute_policy_state("system_guard", "focus_regained_idle");
        transition_result = "no_op";
    } else {
        recompute_policy_state("user_request", action);
        transition_result = "applied";
    }
    cJSON_Delete(root);

    char resp[720];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"browser_session_id\":\"%s\",\"input_enabled\":%s,\"capture_mode\":\"%s\",\"capture_active\":%s,\"policy\":{\"state\":\"%s\",\"source\":\"%s\",\"reason\":\"%s\",\"changed_at_us\":%llu,\"event_seq\":%llu},\"transition_result\":\"%s\"}}",
             session_id,
             browser_session_id,
             input_enabled ? "true" : "false",
             input_capture_mode,
             capture_active ? "true" : "false",
             policy_state,
             policy_source,
             policy_reason,
             (unsigned long long)policy_changed_at_us,
             (unsigned long long)policy_event_seq,
             transition_result);
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

    char browser_session_id[64] = {0};
    if (!extract_browser_session_id(root, browser_session_id, sizeof(browser_session_id))) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INPUT_POLICY_SESSION_INVALID\"}}", 409);
    }
    if (!browser_policy_guard(browser_session_id)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INPUT_POLICY_VIOLATION\"}}", 409);
    }

    cJSON *reason_item = cJSON_GetObjectItemCaseSensitive(root, "reason");
    const char *release_reason = (cJSON_IsString(reason_item) && reason_item->valuestring != NULL && reason_item->valuestring[0] != '\0')
                                     ? reason_item->valuestring
                                     : "explicit_release";

    const char *result = "no_op";
    if (mode_is_mouse_over()) {
        recompute_policy_state("system_guard", "mouse_over_release_no_op");
        result = "no_op";
    } else if (mode_is_click_to_capture()) {
        if (!capture_active) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INPUT_POLICY_INVALID_STATE\"}}", 409);
        }
        capture_active = false;
        recompute_policy_state("system_guard", release_reason);
        result = "released";
    }
    cJSON_Delete(root);

    char resp[512];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"browser_session_id\":\"%s\",\"result\":\"%s\",\"capture_active\":%s,\"released_at_us\":%llu,\"policy\":{\"state\":\"%s\",\"reason\":\"%s\"}}}",
             session_id,
             browser_session_id,
             result,
             capture_active ? "true" : "false",
             (unsigned long long)policy_changed_at_us,
             policy_state,
             policy_reason);
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
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"stream\":\"input\",\"input_enabled\":%s,\"capture_mode\":\"%s\",\"capture_active\":%s,\"policy_state\":\"%s\",\"event_seq\":%llu,\"event_timestamp_us\":%llu}}",
             session_id,
             input_enabled ? "true" : "false",
             input_capture_mode,
             capture_active ? "true" : "false",
             policy_state,
             (unsigned long long)(policy_event_seq > 0 ? policy_event_seq : 1),
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
