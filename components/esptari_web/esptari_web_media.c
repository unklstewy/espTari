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
static char attached_disk_ids[2][64];
static char attached_cartridge_id[64];
static char media_error_payload_buf[1024];
static char media_resp_buf[2048];
static char media_details_buf[768];
static uint64_t media_attach_event_seq;
static uint64_t media_runtime_generation;
static esptari_web_media_attach_event_t last_rom_attach_events[4];
static size_t last_rom_attach_event_count;
static esptari_web_media_disk_state_event_t last_disk_state_events[4];
static size_t last_disk_state_event_count;

static void clear_last_rom_attach_events(void)
{
    memset(last_rom_attach_events, 0, sizeof(last_rom_attach_events));
    last_rom_attach_event_count = 0;
}

static void push_rom_attach_event(const char *rom_id,
                                  const char *phase,
                                  const char *result,
                                  const char *request_id,
                                  const char *error_code,
                                  const char *error_message)
{
    if (last_rom_attach_event_count >= (sizeof(last_rom_attach_events) / sizeof(last_rom_attach_events[0]))) {
        return;
    }

    esptari_web_media_attach_event_t *event = &last_rom_attach_events[last_rom_attach_event_count++];
    media_attach_event_seq++;
    event->event_seq = media_attach_event_seq;
    event->event_timestamp_us = (uint64_t)esp_timer_get_time();
    strlcpy(event->media_id, rom_id != NULL ? rom_id : "", sizeof(event->media_id));
    strlcpy(event->phase, phase != NULL ? phase : "", sizeof(event->phase));
    strlcpy(event->result, result != NULL ? result : "", sizeof(event->result));
    strlcpy(event->request_id, request_id != NULL ? request_id : "", sizeof(event->request_id));

    if (error_code != NULL && error_code[0] != '\0') {
        event->has_error = true;
        strlcpy(event->error_code, error_code, sizeof(event->error_code));
        strlcpy(event->error_message, error_message != NULL ? error_message : "", sizeof(event->error_message));
    }
}

static void clear_last_disk_state_events(void)
{
    memset(last_disk_state_events, 0, sizeof(last_disk_state_events));
    last_disk_state_event_count = 0;
}

static void push_disk_state_event(const char *drive,
                                  const char *state,
                                  const char *disk_id,
                                  bool has_disk_id,
                                  const char *request_id)
{
    if (last_disk_state_event_count >= (sizeof(last_disk_state_events) / sizeof(last_disk_state_events[0]))) {
        return;
    }

    esptari_web_media_disk_state_event_t *event = &last_disk_state_events[last_disk_state_event_count++];
    media_attach_event_seq++;
    event->event_seq = media_attach_event_seq;
    event->event_timestamp_us = (uint64_t)esp_timer_get_time();
    strlcpy(event->drive, drive != NULL ? drive : "A", sizeof(event->drive));
    strlcpy(event->state, state != NULL ? state : "empty", sizeof(event->state));
    event->has_disk_id = has_disk_id;
    if (has_disk_id && disk_id != NULL) {
        strlcpy(event->disk_id, disk_id, sizeof(event->disk_id));
    }
    strlcpy(event->request_id, request_id != NULL ? request_id : "", sizeof(event->request_id));
}

void esptari_web_media_get_last_disk_state_events(const esptari_web_media_disk_state_event_t **out_events,
                                                  size_t *out_count)
{
    if (out_events != NULL) {
        *out_events = last_disk_state_events;
    }
    if (out_count != NULL) {
        *out_count = last_disk_state_event_count;
    }
}

static bool parse_drive_field(cJSON *root, char *out_drive)
{
    const char *drive = NULL;
    if (!esptari_web_json_get_string(root, "drive", &drive) || drive == NULL || drive[0] == '\0') {
        return false;
    }
    if ((strcmp(drive, "A") != 0 && strcmp(drive, "B") != 0) || drive[1] != '\0') {
        return false;
    }
    out_drive[0] = drive[0];
    out_drive[1] = '\0';
    return true;
}

void esptari_web_media_get_last_rom_attach_events(const esptari_web_media_attach_event_t **out_events,
                                                  size_t *out_count)
{
    if (out_events != NULL) {
        *out_events = last_rom_attach_events;
    }
    if (out_count != NULL) {
        *out_count = last_rom_attach_event_count;
    }
}

static esp_err_t send_media_error(httpd_req_t *req,
                                  int status_code,
                                  const char *code,
                                  const char *category,
                                  const char *message,
                                  const char *details_json)
{
    if (details_json != NULL && details_json[0] != '\0') {
        snprintf(media_error_payload_buf,
                 sizeof(media_error_payload_buf),
                 "{\"ok\":false,\"error\":{\"code\":\"%s\",\"category\":\"%s\",\"message\":\"%s\",\"retryable\":false,\"details\":%s}}",
                 code,
                 category,
                 message,
                 details_json);
    } else {
        snprintf(media_error_payload_buf,
                 sizeof(media_error_payload_buf),
                 "{\"ok\":false,\"error\":{\"code\":\"%s\",\"category\":\"%s\",\"message\":\"%s\",\"retryable\":false}}",
                 code,
                 category,
                 message);
    }
    return send_json(req, media_error_payload_buf, status_code);
}

static esp_err_t resolve_media_entry(httpd_req_t *req,
                                     const char *catalog_name,
                                     const char *request_field,
                                     const char *entry_id,
                                     const char **out_local_path)
{
    const catalog_def_t *def = esptari_web_catalog_find(catalog_name);
    if (def == NULL) {
        snprintf(media_details_buf,
             sizeof(media_details_buf),
                 "{\"catalog\":\"%s\",\"request_field\":\"%s\"}",
                 catalog_name,
                 request_field);
        return send_media_error(req,
                                404,
                                "CATALOG_NOT_FOUND",
                                "catalog",
                                "Requested catalog index is not available",
                                media_details_buf);
    }

    int entry_index = esptari_web_catalog_find_entry_index(def, entry_id);
    if (entry_index < 0) {
        snprintf(media_details_buf,
             sizeof(media_details_buf),
                 "{\"catalog\":\"%s\",\"entry_id\":\"%s\",\"request_field\":\"%s\"}",
                 catalog_name,
                 entry_id,
                 request_field);
        return send_media_error(req,
                                404,
                                "CATALOG_ENTRY_NOT_FOUND",
                                "catalog",
                                "Requested catalog entry was not found",
                                media_details_buf);
    }

    if (!esptari_web_catalog_entry_local_present(def, (size_t)entry_index)) {
        snprintf(media_details_buf,
             sizeof(media_details_buf),
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
                                media_details_buf);
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

    cJSON *force_apply_fail_item = cJSON_GetObjectItemCaseSensitive(root, "force_apply_fail");
    bool force_apply_fail = cJSON_IsTrue(force_apply_fail_item);

    char previous_rom_id[64] = {0};
    strlcpy(previous_rom_id, attached_rom_id, sizeof(previous_rom_id));

    char request_id[48];
    snprintf(request_id,
             sizeof(request_id),
             "req_%llu",
             (unsigned long long)esp_timer_get_time());

    clear_last_rom_attach_events();
    push_rom_attach_event(rom_id, "validated", "in_progress", request_id, NULL, NULL);
    uint64_t mounted_at_us = (uint64_t)esp_timer_get_time();
    push_rom_attach_event(rom_id, "mounted", "in_progress", request_id, NULL, NULL);

    if (force_apply_fail) {
        strlcpy(attached_rom_id, previous_rom_id, sizeof(attached_rom_id));
        push_rom_attach_event(rom_id,
                              "failed",
                              "failed",
                              request_id,
                              "MEDIA_ATTACH_FAILED",
                              "ROM apply phase failed; previous ROM restored");
        cJSON_Delete(root);

        snprintf(media_details_buf,
             sizeof(media_details_buf),
                 "{\"session_id\":\"ses_local\",\"rom_id\":\"%s\",\"failed_phase\":\"applied\",\"request_id\":\"%s\",\"validation_stage\":\"rom_attach_apply\"}",
                 rom_id,
                 request_id);
        return send_media_error(req,
                                409,
                                "MEDIA_ATTACH_FAILED",
                                "engine",
                                "ROM apply phase failed; previous ROM restored",
                                media_details_buf);
    }

    strlcpy(attached_rom_id, rom_id, sizeof(attached_rom_id));
    media_runtime_generation++;
    uint64_t applied_at_us = (uint64_t)esp_timer_get_time();
    push_rom_attach_event(rom_id, "applied", "applied", request_id, NULL, NULL);
    cJSON_Delete(root);

    snprintf(media_resp_buf,
             sizeof(media_resp_buf),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"rom\":{\"id\":\"%s\",\"catalog\":\"rom\",\"local_path\":\"%s\",\"binding\":{\"machine\":\"atari_st\",\"profile\":\"st_520_pal\",\"binding_result\":\"matched\"}},\"rom_id\":\"%s\",\"result\":\"applied\",\"phase\":\"applied\",\"phase_history\":[\"validated\",\"mounted\",\"applied\"],\"request_id\":\"%s\",\"mount\":{\"slot\":\"rom.primary\",\"mounted_path\":\"%s\",\"mounted_at_us\":%llu},\"apply\":{\"applied_at_us\":%llu,\"runtime_generation\":%llu},\"attached_at_us\":%llu}}",
             rom_id,
             local_path,
             attached_rom_id,
             request_id,
             local_path,
             (unsigned long long)mounted_at_us,
             (unsigned long long)applied_at_us,
             (unsigned long long)media_runtime_generation,
             (unsigned long long)applied_at_us);
    return send_json(req, media_resp_buf, 200);
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

    char drive[2] = {0};
    if (!parse_drive_field(root, drive)) {
        cJSON_Delete(root);
        return send_media_error(req, 400, "BAD_REQUEST", "request", "Drive must be one of A or B", NULL);
    }
    size_t drive_index = (drive[0] == 'B') ? 1u : 0u;

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

    cJSON *force_active_fail_item = cJSON_GetObjectItemCaseSensitive(root, "force_active_fail");
    bool force_active_fail = cJSON_IsTrue(force_active_fail_item);
    bool write_protect = true;
    cJSON *write_protect_item = cJSON_GetObjectItemCaseSensitive(root, "write_protect");
    if (write_protect_item != NULL && cJSON_IsBool(write_protect_item)) {
        write_protect = cJSON_IsTrue(write_protect_item);
    }

    char previous_disk_id[64] = {0};
    strlcpy(previous_disk_id, attached_disk_ids[drive_index], sizeof(previous_disk_id));

    char request_id[48];
    snprintf(request_id,
             sizeof(request_id),
             "req_%llu",
             (unsigned long long)esp_timer_get_time());

    clear_last_disk_state_events();
    uint64_t mounted_at_us = (uint64_t)esp_timer_get_time();
    push_disk_state_event(drive, "mounted", disk_id, true, request_id);

    if (force_active_fail) {
        strlcpy(attached_disk_ids[drive_index], previous_disk_id, sizeof(attached_disk_ids[drive_index]));
        push_disk_state_event(drive, "failed", disk_id, true, request_id);
        cJSON_Delete(root);

        snprintf(media_details_buf,
             sizeof(media_details_buf),
                 "{\"session_id\":\"ses_local\",\"drive\":\"%s\",\"disk_id\":\"%s\",\"failed_phase\":\"active\",\"request_id\":\"%s\"}",
                 drive,
                 disk_id,
                 request_id);
        return send_media_error(req,
                                409,
                                "MEDIA_ATTACH_FAILED",
                                "engine",
                                "Disk active phase failed; previous disk restored",
                                media_details_buf);
    }

    strlcpy(attached_disk_ids[drive_index], disk_id, sizeof(attached_disk_ids[drive_index]));
    media_runtime_generation++;
    uint64_t active_at_us = (uint64_t)esp_timer_get_time();
    push_disk_state_event(drive, "active", disk_id, true, request_id);
    cJSON_Delete(root);

    snprintf(media_resp_buf,
             sizeof(media_resp_buf),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"drive\":\"%s\",\"disk\":{\"id\":\"%s\",\"catalog\":\"disk\",\"local_path\":\"%s\",\"format\":\"st\",\"write_protect\":%s,\"binding_result\":\"matched\"},\"disk_id\":\"%s\",\"result\":\"active\",\"phase_history\":[\"validated\",\"mounted\",\"active\"],\"request_id\":\"%s\",\"mount\":{\"mounted_at_us\":%llu,\"mounted_path\":\"%s\"},\"runtime\":{\"active_at_us\":%llu,\"generation\":%llu},\"attached_at_us\":%llu}}",
             drive,
             disk_id,
             local_path,
             write_protect ? "true" : "false",
             attached_disk_ids[drive_index],
             request_id,
             (unsigned long long)mounted_at_us,
             local_path,
             (unsigned long long)active_at_us,
             (unsigned long long)media_runtime_generation,
             (unsigned long long)active_at_us);
    return send_json(req, media_resp_buf, 200);
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

    char drive[2] = {0};
    if (!parse_drive_field(root, drive)) {
        cJSON_Delete(root);
        return send_media_error(req, 400, "BAD_REQUEST", "request", "Drive must be one of A or B", NULL);
    }
    size_t drive_index = (drive[0] == 'B') ? 1u : 0u;

    char request_id[48];
    snprintf(request_id,
             sizeof(request_id),
             "req_%llu",
             (unsigned long long)esp_timer_get_time());

    bool had_disk = attached_disk_ids[drive_index][0] != '\0';
    char ejected_disk_id[64] = {0};
    strlcpy(ejected_disk_id, attached_disk_ids[drive_index], sizeof(ejected_disk_id));
    attached_disk_ids[drive_index][0] = '\0';
    uint64_t ejected_at_us = (uint64_t)esp_timer_get_time();

    clear_last_disk_state_events();
    push_disk_state_event(drive, "ejected", NULL, false, request_id);

    cJSON_Delete(root);

    char ejected_disk_json[96] = "null";
    if (had_disk) {
        snprintf(ejected_disk_json, sizeof(ejected_disk_json), "\"%s\"", ejected_disk_id);
    }
    snprintf(media_resp_buf,
             sizeof(media_resp_buf),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"drive\":\"%s\",\"result\":\"%s\",\"phase_history\":[\"detached\",\"ejected\"],\"request_id\":\"%s\",\"ejected_disk_id\":%s,\"ejected_at_us\":%llu}}",
             drive,
             had_disk ? "ejected" : "no_op",
             request_id,
             ejected_disk_json,
             (unsigned long long)ejected_at_us);
    return send_json(req, media_resp_buf, 200);
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
