#include "esptari_web_media.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esptari_core.h"
#include "esptari_web_catalog_state.h"
#include "esptari_web_http_utils.h"

#define send_json esptari_web_send_json

static char attached_rom_id[64];
static char attached_disk_id[64];
static char attached_cartridge_id[64];

static esp_err_t send_media_error(httpd_req_t *req,
                                  int status_code,
                                  const char *code,
                                  const char *category,
                                  const char *message,
                                  const char *details_json)
{
    char payload[768];
    if (details_json != NULL && details_json[0] != '\0') {
        snprintf(payload,
                 sizeof(payload),
                 "{\"ok\":false,\"error\":{\"code\":\"%s\",\"category\":\"%s\",\"message\":\"%s\",\"retryable\":false,\"details\":%s}}",
                 code,
                 category,
                 message,
                 details_json);
    } else {
        snprintf(payload,
                 sizeof(payload),
                 "{\"ok\":false,\"error\":{\"code\":\"%s\",\"category\":\"%s\",\"message\":\"%s\",\"retryable\":false}}",
                 code,
                 category,
                 message);
    }
    return send_json(req, payload, status_code);
}

static esp_err_t resolve_media_entry(httpd_req_t *req,
                                     const char *catalog_name,
                                     const char *request_field,
                                     const char *entry_id,
                                     const char **out_local_path)
{
    const catalog_def_t *def = esptari_web_catalog_find(catalog_name);
    if (def == NULL) {
        char details[256];
        snprintf(details,
                 sizeof(details),
                 "{\"catalog\":\"%s\",\"request_field\":\"%s\"}",
                 catalog_name,
                 request_field);
        return send_media_error(req,
                                404,
                                "CATALOG_NOT_FOUND",
                                "catalog",
                                "Requested catalog index is not available",
                                details);
    }

    int entry_index = esptari_web_catalog_find_entry_index(def, entry_id);
    if (entry_index < 0) {
        char details[320];
        snprintf(details,
                 sizeof(details),
                 "{\"catalog\":\"%s\",\"entry_id\":\"%s\",\"request_field\":\"%s\"}",
                 catalog_name,
                 entry_id,
                 request_field);
        return send_media_error(req,
                                404,
                                "CATALOG_ENTRY_NOT_FOUND",
                                "catalog",
                                "Requested catalog entry was not found",
                                details);
    }

    if (!esptari_web_catalog_entry_local_present(def, (size_t)entry_index)) {
        char details[384];
        snprintf(details,
                 sizeof(details),
                 "{\"catalog\":\"%s\",\"entry_id\":\"%s\",\"request_field\":\"%s\",\"remediation\":\"POST /api/v2/catalogs/%s/download-entry\"}",
                 catalog_name,
                 entry_id,
                 request_field,
                 catalog_name);
        return send_media_error(req,
                                409,
                                "CONFLICT",
                                "catalog",
                                "Catalog entry exists but local asset is missing",
                                details);
    }

    if (out_local_path != NULL) {
        *out_local_path = esptari_web_catalog_entry_local_path_projected(def, (size_t)entry_index);
    }

    return ESP_OK;
}

static esp_err_t parse_body_json(httpd_req_t *req, char *body, size_t body_len, cJSON **out_root)
{
    if (esptari_web_read_request_body(req, body, body_len) != ESP_OK) {
        return send_media_error(req, 400, "BAD_REQUEST", "request", "Invalid request body", NULL);
    }
    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_media_error(req, 400, "BAD_REQUEST", "request", "Malformed JSON", NULL);
    }
    *out_root = root;
    return ESP_OK;
}

static esp_err_t validate_session_running_or_paused(httpd_req_t *req, cJSON *root)
{
    const char *session_id = NULL;
    if (!esptari_web_json_get_string(root, "session_id", &session_id) || session_id == NULL || session_id[0] == '\0') {
        return send_media_error(req, 400, "BAD_REQUEST", "request", "session_id is required", NULL);
    }
    if (strcmp(session_id, "ses_local") != 0) {
        return send_media_error(req, 409, "ENGINE_NOT_RUNNING", "engine", "Session is not running", NULL);
    }

    esptari_session_status_t status;
    esptari_core_get_status(&status);
    if (status.state != ESPTARI_SESSION_RUNNING && status.state != ESPTARI_SESSION_PAUSED) {
        return send_media_error(req, 409, "INVALID_SESSION_STATE", "engine", "Media attach requires running or paused state", NULL);
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
        return send_media_error(req, 400, "BAD_REQUEST", "request", "rom_id is required", NULL);
    }

    const char *local_path = "";
    esp_err_t resolve_err = resolve_media_entry(req, "roms", "rom_id", rom_id, &local_path);
    if (resolve_err != ESP_OK) {
        cJSON_Delete(root);
        return resolve_err;
    }

    strlcpy(attached_rom_id, rom_id, sizeof(attached_rom_id));
    uint64_t now_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[384];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"rom_id\":\"%s\",\"catalog\":\"roms\",\"local_path\":\"%s\",\"state\":\"attached\",\"attached_at_us\":%llu}}",
             attached_rom_id,
             local_path,
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
        return send_media_error(req, 400, "BAD_REQUEST", "request", "disk_id is required", NULL);
    }

    const char *local_path = "";
    esp_err_t resolve_err = resolve_media_entry(req, "floppies", "disk_id", disk_id, &local_path);
    if (resolve_err != ESP_OK) {
        cJSON_Delete(root);
        return resolve_err;
    }

    strlcpy(attached_disk_id, disk_id, sizeof(attached_disk_id));
    uint64_t now_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[384];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"disk_id\":\"%s\",\"catalog\":\"floppies\",\"local_path\":\"%s\",\"state\":\"attached\",\"attached_at_us\":%llu}}",
             attached_disk_id,
             local_path,
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
        return send_media_error(req, 400, "BAD_REQUEST", "request", "cartridge_id is required", NULL);
    }

    const char *local_path = "";
    esp_err_t resolve_err = resolve_media_entry(req, "cartridges", "cartridge_id", cartridge_id, &local_path);
    if (resolve_err != ESP_OK) {
        cJSON_Delete(root);
        return resolve_err;
    }

    strlcpy(attached_cartridge_id, cartridge_id, sizeof(attached_cartridge_id));
    uint64_t now_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[416];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"cartridge_id\":\"%s\",\"catalog\":\"cartridges\",\"local_path\":\"%s\",\"state\":\"attached\",\"attached_at_us\":%llu}}",
             attached_cartridge_id,
             local_path,
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
