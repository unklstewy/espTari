#include "esptari_web_media.h"

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

static char attached_rom_id[64];
static char attached_disk_id[64];
static char attached_cartridge_id[64];

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

static esp_err_t validate_session_running_or_paused(httpd_req_t *req, cJSON *root)
{
    const char *session_id = NULL;
    if (!esptari_web_json_get_string(root, "session_id", &session_id) || session_id == NULL || session_id[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (strcmp(session_id, "ses_local") != 0) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }

    esptari_session_status_t status;
    esptari_core_get_status(&status);
    if (status.state != ESPTARI_SESSION_RUNNING && status.state != ESPTARI_SESSION_PAUSED) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\"}}", 409);
    }
    return ESP_OK;
}

static esp_err_t media_rom_attach_handler(httpd_req_t *req)
{
    char body[512];
    cJSON *root = NULL;
    esp_err_t parse_err = parse_body_json(req, body, sizeof(body), &root);
    if (parse_err != ESP_OK) {
        return parse_err;
    }
    esp_err_t guard = validate_session_running_or_paused(req, root);
    if (guard != ESP_OK) {
        cJSON_Delete(root);
        return guard;
    }

    const char *rom_id = NULL;
    if (!esptari_web_json_get_string(root, "rom_id", &rom_id) || rom_id == NULL || rom_id[0] == '\0') {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    strlcpy(attached_rom_id, rom_id, sizeof(attached_rom_id));
    uint64_t now_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[384];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"rom_id\":\"%s\",\"state\":\"attached\",\"attached_at_us\":%llu}}",
             attached_rom_id,
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

static esp_err_t media_disk_attach_handler(httpd_req_t *req)
{
    char body[512];
    cJSON *root = NULL;
    esp_err_t parse_err = parse_body_json(req, body, sizeof(body), &root);
    if (parse_err != ESP_OK) {
        return parse_err;
    }
    esp_err_t guard = validate_session_running_or_paused(req, root);
    if (guard != ESP_OK) {
        cJSON_Delete(root);
        return guard;
    }

    const char *disk_id = NULL;
    if (!esptari_web_json_get_string(root, "disk_id", &disk_id) || disk_id == NULL || disk_id[0] == '\0') {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    strlcpy(attached_disk_id, disk_id, sizeof(attached_disk_id));
    uint64_t now_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[384];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"disk_id\":\"%s\",\"state\":\"attached\",\"attached_at_us\":%llu}}",
             attached_disk_id,
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

static esp_err_t media_disk_eject_handler(httpd_req_t *req)
{
    char body[512];
    cJSON *root = NULL;
    esp_err_t parse_err = parse_body_json(req, body, sizeof(body), &root);
    if (parse_err != ESP_OK) {
        return parse_err;
    }
    esp_err_t guard = validate_session_running_or_paused(req, root);
    if (guard != ESP_OK) {
        cJSON_Delete(root);
        return guard;
    }

    bool had_disk = attached_disk_id[0] != '\0';
    attached_disk_id[0] = '\0';
    uint64_t now_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[320];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"result\":\"%s\",\"ejected_at_us\":%llu}}",
             had_disk ? "ejected" : "no_op",
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

static esp_err_t media_cartridge_attach_handler(httpd_req_t *req)
{
    char body[512];
    cJSON *root = NULL;
    esp_err_t parse_err = parse_body_json(req, body, sizeof(body), &root);
    if (parse_err != ESP_OK) {
        return parse_err;
    }
    esp_err_t guard = validate_session_running_or_paused(req, root);
    if (guard != ESP_OK) {
        cJSON_Delete(root);
        return guard;
    }

    const char *cartridge_id = NULL;
    if (!esptari_web_json_get_string(root, "cartridge_id", &cartridge_id) || cartridge_id == NULL || cartridge_id[0] == '\0') {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    strlcpy(attached_cartridge_id, cartridge_id, sizeof(attached_cartridge_id));
    uint64_t now_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[416];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"cartridge_id\":\"%s\",\"state\":\"attached\",\"attached_at_us\":%llu}}",
             attached_cartridge_id,
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

static esp_err_t media_cartridge_eject_handler(httpd_req_t *req)
{
    char body[512];
    cJSON *root = NULL;
    esp_err_t parse_err = parse_body_json(req, body, sizeof(body), &root);
    if (parse_err != ESP_OK) {
        return parse_err;
    }
    esp_err_t guard = validate_session_running_or_paused(req, root);
    if (guard != ESP_OK) {
        cJSON_Delete(root);
        return guard;
    }

    bool had_cartridge = attached_cartridge_id[0] != '\0';
    attached_cartridge_id[0] = '\0';
    uint64_t now_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[320];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"result\":\"%s\",\"ejected_at_us\":%llu}}",
             had_cartridge ? "ejected" : "no_op",
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

void esptari_web_media_register_routes(httpd_handle_t server_handle)
{
    httpd_uri_t rom_attach = {.uri = "/api/v2/media/rom/attach", .method = HTTP_POST, .handler = media_rom_attach_handler, .user_ctx = NULL};
    httpd_uri_t disk_attach = {.uri = "/api/v2/media/disk/attach", .method = HTTP_POST, .handler = media_disk_attach_handler, .user_ctx = NULL};
    httpd_uri_t disk_eject = {.uri = "/api/v2/media/disk/eject", .method = HTTP_POST, .handler = media_disk_eject_handler, .user_ctx = NULL};
    httpd_uri_t cartridge_attach = {.uri = "/api/v2/media/cartridge/attach", .method = HTTP_POST, .handler = media_cartridge_attach_handler, .user_ctx = NULL};
    httpd_uri_t cartridge_eject = {.uri = "/api/v2/media/cartridge/eject", .method = HTTP_POST, .handler = media_cartridge_eject_handler, .user_ctx = NULL};

    httpd_register_uri_handler(server_handle, &rom_attach);
    httpd_register_uri_handler(server_handle, &disk_attach);
    httpd_register_uri_handler(server_handle, &disk_eject);
    httpd_register_uri_handler(server_handle, &cartridge_attach);
    httpd_register_uri_handler(server_handle, &cartridge_eject);
}
