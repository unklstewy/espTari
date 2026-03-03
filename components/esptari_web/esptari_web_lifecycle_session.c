#include "esptari_web_lifecycle_session.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esptari_core.h"
#include "esptari_web_catalog_state.h"
#include "esptari_web_http_utils.h"

static esp_err_t handle_state_change(httpd_req_t *req,
                                     esp_err_t (*op)(void),
                                     const char *guard_id,
                                     const char *endpoint)
{
    esp_err_t err = op();
    if (err == ESP_OK) {
        return esptari_web_send_json(req, "{\"ok\":true}", 200);
    }

    const char *error_code = "INTERNAL_ERROR";
    int status_code = 500;
    const char *effective_guard_id = guard_id;

    if (err == ESP_ERR_INVALID_STATE) {
        error_code = "INVALID_SESSION_STATE";
        status_code = 409;
    } else if (err == ESP_ERR_NOT_FOUND) {
        error_code = "MACHINE_NOT_LOADED";
        status_code = 412;
        effective_guard_id = "G-LOADER-MACHINE-READY";
    }

    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"ok\":false,\"error\":{\"code\":\"%s\",\"details\":{\"guard_id\":\"%s\",\"endpoint\":\"%s\",\"esp_err\":\"%s\"}}}",
             error_code, effective_guard_id, endpoint, esp_err_to_name(err));
    return esptari_web_send_json(req, buf, status_code);
}

static esp_err_t send_start_error(httpd_req_t *req,
                                  const char *code,
                                  int status_code,
                                  const char *detail_key,
                                  const char *detail_value)
{
    if (detail_key == NULL || detail_value == NULL) {
        char payload[160];
        snprintf(payload, sizeof(payload), "{\"ok\":false,\"error\":{\"code\":\"%s\"}}", code);
        return esptari_web_send_json(req, payload, status_code);
    }

    char payload[320];
    snprintf(payload,
             sizeof(payload),
             "{\"ok\":false,\"error\":{\"code\":\"%s\",\"details\":{\"%s\":\"%s\"}}}",
             code,
             detail_key,
             detail_value);
    return esptari_web_send_json(req, payload, status_code);
}

static bool parse_optional_string(cJSON *root, const char *field, char *out, size_t out_len)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, field);
    if (item == NULL) {
        out[0] = '\0';
        return true;
    }
    if (!cJSON_IsString(item) || item->valuestring == NULL || item->valuestring[0] == '\0') {
        return false;
    }
    strlcpy(out, item->valuestring, out_len);
    return true;
}

static bool parse_required_string(cJSON *root, const char *field, char *out, size_t out_len)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, field);
    if (!cJSON_IsString(item) || item->valuestring == NULL || item->valuestring[0] == '\0') {
        return false;
    }
    strlcpy(out, item->valuestring, out_len);
    return true;
}

static esp_err_t resolve_catalog_entry(httpd_req_t *req,
                                       const char *catalog_name,
                                       const char *entry_id,
                                       bool require_local,
                                       const char **out_local_path)
{
    const catalog_def_t *def = esptari_web_catalog_find(catalog_name);
    if (def == NULL) {
        return send_start_error(req, "CATALOG_NOT_FOUND", 404, "catalog", catalog_name);
    }

    int entry_index = esptari_web_catalog_find_entry_index(def, entry_id);
    if (entry_index < 0) {
        return send_start_error(req, "CATALOG_ENTRY_NOT_FOUND", 404, "entry_id", entry_id);
    }

    bool local_present = esptari_web_catalog_entry_local_present(def, (size_t)entry_index);
    if (require_local && !local_present) {
        return send_start_error(req, "CONFLICT", 409, "entry_id", entry_id);
    }

    if (out_local_path != NULL) {
        *out_local_path = esptari_web_catalog_entry_local_path_projected(def, (size_t)entry_index);
    }

    return ESP_OK;
}

esp_err_t esptari_web_lifecycle_session_handler(httpd_req_t *req)
{
    char body[768];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char machine[64] = {0};
    char profile[64] = {0};
    char rom_id[96] = {0};
    char tos_id[96] = {0};
    char first_disk_id[96] = {0};
    bool disk_ids_supplied = false;

    if (!parse_required_string(root, "machine", machine, sizeof(machine)) ||
        !parse_required_string(root, "profile", profile, sizeof(profile)) ||
        !parse_required_string(root, "rom_id", rom_id, sizeof(rom_id))) {
        cJSON_Delete(root);
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    if (!parse_optional_string(root, "tos_id", tos_id, sizeof(tos_id))) {
        cJSON_Delete(root);
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *disk_ids = cJSON_GetObjectItemCaseSensitive(root, "disk_ids");
    if (disk_ids != NULL) {
        if (!cJSON_IsArray(disk_ids)) {
            cJSON_Delete(root);
            return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
        disk_ids_supplied = true;
        int disk_count = cJSON_GetArraySize(disk_ids);
        for (int i = 0; i < disk_count; i++) {
            cJSON *disk_item = cJSON_GetArrayItem(disk_ids, i);
            if (!cJSON_IsString(disk_item) || disk_item->valuestring == NULL || disk_item->valuestring[0] == '\0') {
                cJSON_Delete(root);
                return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
            }
            if (i == 0) {
                strlcpy(first_disk_id, disk_item->valuestring, sizeof(first_disk_id));
            }
        }
    }

    const char *rom_local_path = "";
    esp_err_t resolve_err = resolve_catalog_entry(req, "roms", rom_id, true, &rom_local_path);
    if (resolve_err != ESP_OK) {
        cJSON_Delete(root);
        return resolve_err;
    }

    const char *tos_local_path = "";
    if (tos_id[0] != '\0') {
        resolve_err = resolve_catalog_entry(req, "tos", tos_id, true, &tos_local_path);
        if (resolve_err != ESP_OK) {
            cJSON_Delete(root);
            return resolve_err;
        }
    }

    if (disk_ids_supplied && first_disk_id[0] != '\0') {
        resolve_err = resolve_catalog_entry(req, "floppies", first_disk_id, true, NULL);
        if (resolve_err != ESP_OK) {
            cJSON_Delete(root);
            return resolve_err;
        }
    }

    cJSON_Delete(root);

    esp_err_t err = esptari_core_start();
    if (err == ESP_ERR_INVALID_STATE) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\",\"details\":{\"guard_id\":\"%s\",\"endpoint\":\"%s\",\"esp_err\":\"%s\"}}}",
                 "G-LIFECYCLE-SESSION",
                 "/api/v2/engine/session",
                 esp_err_to_name(err));
        return esptari_web_send_json(req, buf, 409);
    }
    if (err == ESP_ERR_NOT_FOUND) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "{\"ok\":false,\"error\":{\"code\":\"MACHINE_NOT_LOADED\",\"details\":{\"guard_id\":\"%s\",\"endpoint\":\"%s\",\"esp_err\":\"%s\"}}}",
                 "G-LOADER-MACHINE-READY",
                 "/api/v2/engine/session",
                 esp_err_to_name(err));
        return esptari_web_send_json(req, buf, 412);
    }
    if (err != ESP_OK) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char resp[768];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"state\":\"running\",\"machine\":\"%s\",\"profile\":\"%s\",\"resolved\":{\"rom_id\":\"%s\",\"rom_path\":\"%s\",\"tos_id\":\"%s\",\"tos_path\":\"%s\",\"first_disk_id\":\"%s\"}}}",
             machine,
             profile,
             rom_id,
             rom_local_path,
             tos_id,
             tos_local_path,
             first_disk_id);
    return esptari_web_send_json(req, resp, 200);
}

esp_err_t esptari_web_lifecycle_start_handler(httpd_req_t *req)
{
    return handle_state_change(req,
                               esptari_core_start,
                               "G-LIFECYCLE-START",
                               "/api/v2/engine/session/start");
}

esp_err_t esptari_web_lifecycle_pause_handler(httpd_req_t *req)
{
    return handle_state_change(req,
                               esptari_core_pause,
                               "G-LIFECYCLE-PAUSE",
                               "/api/v2/engine/session/pause");
}

esp_err_t esptari_web_lifecycle_resume_handler(httpd_req_t *req)
{
    bool resume_running = true;

    if (req->content_len > 0) {
        char body[256];
        if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
            return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }

        cJSON *root = cJSON_Parse(body);
        if (root == NULL) {
            return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }

        cJSON *resume_mode_item = cJSON_GetObjectItemCaseSensitive(root, "resume_mode");
        if (resume_mode_item != NULL) {
            if (!cJSON_IsString(resume_mode_item) || resume_mode_item->valuestring == NULL) {
                cJSON_Delete(root);
                return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
            }

            if (strcmp(resume_mode_item->valuestring, "running") == 0) {
                resume_running = true;
            } else if (strcmp(resume_mode_item->valuestring, "paused") == 0) {
                resume_running = false;
            } else {
                cJSON_Delete(root);
                return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
            }
        }

        cJSON_Delete(root);
    }

    esp_err_t err = esptari_core_resume_with_mode(resume_running);
    if (err == ESP_OK) {
        return esptari_web_send_json(req, "{\"ok\":true}", 200);
    }

    if (err == ESP_ERR_INVALID_STATE) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\",\"details\":{\"guard_id\":\"%s\",\"endpoint\":\"%s\",\"esp_err\":\"%s\"}}}",
                 "G-LIFECYCLE-RESUME",
                 "/api/v2/engine/session/resume",
                 esp_err_to_name(err));
        return esptari_web_send_json(req, buf, 409);
    }

    return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
}

esp_err_t esptari_web_lifecycle_stop_handler(httpd_req_t *req)
{
    return handle_state_change(req,
                               esptari_core_stop,
                               "G-LIFECYCLE-STOP",
                               "/api/v2/engine/session/stop");
}

esp_err_t esptari_web_lifecycle_reset_handler(httpd_req_t *req)
{
    char reset_mode[8] = "warm";
    bool preserve_media = true;

    if (req->content_len > 0) {
        char body[256];
        if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
            return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }

        cJSON *root = cJSON_Parse(body);
        if (root == NULL) {
            return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }

        cJSON *mode_item = cJSON_GetObjectItemCaseSensitive(root, "mode");
        if (mode_item != NULL) {
            if (!cJSON_IsString(mode_item) || mode_item->valuestring == NULL) {
                cJSON_Delete(root);
                return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
            }

            if (strcmp(mode_item->valuestring, "warm") != 0 && strcmp(mode_item->valuestring, "cold") != 0) {
                cJSON_Delete(root);
                return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
            }

            strlcpy(reset_mode, mode_item->valuestring, sizeof(reset_mode));
        }

        cJSON *preserve_media_item = cJSON_GetObjectItemCaseSensitive(root, "preserve_media");
        if (preserve_media_item != NULL) {
            if (!cJSON_IsBool(preserve_media_item)) {
                cJSON_Delete(root);
                return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
            }
            preserve_media = cJSON_IsTrue(preserve_media_item);
        }

        cJSON_Delete(root);
    }

    esp_err_t err = esptari_core_reset();
    if (err == ESP_ERR_INVALID_STATE) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\",\"details\":{\"guard_id\":\"%s\",\"endpoint\":\"%s\",\"esp_err\":\"%s\"}}}",
                 "G-LIFECYCLE-RESET",
                 "/api/v2/engine/session/reset",
                 esp_err_to_name(err));
        return esptari_web_send_json(req, buf, 409);
    }

    if (err != ESP_OK) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    esptari_session_status_t status;
    esptari_core_get_status(&status);

    char resp[320];
    snprintf(resp, sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"state\":\"%s\",\"reset_mode\":\"%s\",\"preserve_media\":%s,\"reset_at_us\":%llu}}",
             esptari_core_state_to_string(status.state),
             reset_mode,
             preserve_media ? "true" : "false",
             (unsigned long long)status.last_transition_us);
    return esptari_web_send_json(req, resp, 200);
}
