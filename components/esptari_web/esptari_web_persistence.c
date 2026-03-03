#include "esptari_web_persistence.h"

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

static uint64_t state_seq = 1;
static char latest_snapshot_id[64] = "state_000001";
static uint64_t latest_saved_at_us;

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

static esp_err_t validate_session_local(httpd_req_t *req, cJSON *root)
{
    const char *session_id = NULL;
    if (!esptari_web_json_get_string(root, "session_id", &session_id) || session_id == NULL || session_id[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (strcmp(session_id, "ses_local") != 0) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }
    return ESP_OK;
}

static esp_err_t state_save_handler(httpd_req_t *req)
{
    char body[768];
    cJSON *root = NULL;
    esp_err_t parse_err = parse_body_json(req, body, sizeof(body), &root);
    if (parse_err != ESP_OK) {
        return parse_err;
    }
    esp_err_t guard = validate_session_local(req, root);
    if (guard != ESP_OK) {
        cJSON_Delete(root);
        return guard;
    }

    const char *name = NULL;
    if (esptari_web_json_get_string(root, "name", &name) && name != NULL && name[0] == '\0') {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    state_seq++;
    snprintf(latest_snapshot_id, sizeof(latest_snapshot_id), "state_%06llu", (unsigned long long)state_seq);
    latest_saved_at_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[512];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"snapshot_id\":\"%s\",\"name\":\"%s\",\"saved_at_us\":%llu}}",
             latest_snapshot_id,
             (name != NULL && name[0] != '\0') ? name : "auto",
             (unsigned long long)latest_saved_at_us);
    return send_json(req, resp, 200);
}

static esp_err_t state_restore_handler(httpd_req_t *req)
{
    char body[768];
    cJSON *root = NULL;
    esp_err_t parse_err = parse_body_json(req, body, sizeof(body), &root);
    if (parse_err != ESP_OK) {
        return parse_err;
    }
    esp_err_t guard = validate_session_local(req, root);
    if (guard != ESP_OK) {
        cJSON_Delete(root);
        return guard;
    }

    const char *snapshot_id = NULL;
    if (!esptari_web_json_get_string(root, "snapshot_id", &snapshot_id) || snapshot_id == NULL || snapshot_id[0] == '\0') {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (strcmp(snapshot_id, latest_snapshot_id) != 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"SNAPSHOT_NOT_FOUND\"}}", 404);
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[448];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"snapshot_id\":\"%s\",\"restored_at_us\":%llu,\"result\":\"restored\"}}",
             snapshot_id,
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

static esp_err_t state_list_handler(httpd_req_t *req)
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
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"snapshots\":[{\"snapshot_id\":\"%s\",\"saved_at_us\":%llu,\"profile\":\"st_520_pal\",\"state\":\"available\"}]}}",
             session_id,
             latest_snapshot_id,
             (unsigned long long)latest_saved_at_us);
    return send_json(req, resp, 200);
}

void esptari_web_persistence_register_routes(httpd_handle_t server_handle)
{
    httpd_uri_t state_save = {.uri = "/api/v2/engine/state/save", .method = HTTP_POST, .handler = state_save_handler, .user_ctx = NULL};
    httpd_uri_t state_restore = {.uri = "/api/v2/engine/state/restore", .method = HTTP_POST, .handler = state_restore_handler, .user_ctx = NULL};
    httpd_uri_t state_list = {.uri = "/api/v2/engine/state/list", .method = HTTP_GET, .handler = state_list_handler, .user_ctx = NULL};

    httpd_register_uri_handler(server_handle, &state_save);
    httpd_register_uri_handler(server_handle, &state_restore);
    httpd_register_uri_handler(server_handle, &state_list);
}
