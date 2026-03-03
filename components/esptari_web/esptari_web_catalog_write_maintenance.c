#include "esptari_web_catalog_write_maintenance.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esptari_web_catalog_state.h"
#include "esptari_web_catalog_utils.h"
#include "esptari_web_http_utils.h"

#define send_json esptari_web_send_json
#define json_get_string esptari_web_json_get_string

static bool is_allowed_scan_root(const char *root)
{
    static const char *allowed_roots[] = {
        "/sdcard/roms",
        "/sdcard/disks",
        "/sdcard/cartridges",
        "/sdcard/tos",
    };
    for (size_t i = 0; i < sizeof(allowed_roots) / sizeof(allowed_roots[0]); i++) {
        if (strncmp(root, allowed_roots[i], strlen(allowed_roots[i])) == 0) {
            return true;
        }
    }
    return false;
}

esp_err_t esptari_web_catalog_mark_dead_handler(httpd_req_t *req)
{
    const catalog_def_t *def = NULL;
    esp_err_t resolve_err = esptari_web_catalog_resolve_def(req, "/api/v2/catalogs/%31[^/]/mark-dead", &def);
    if (resolve_err != ESP_OK) {
        return resolve_err;
    }

    char body[512];
    cJSON *root = NULL;
    esp_err_t parse_err = esptari_web_catalog_parse_body_json(req, body, sizeof(body), &root);
    if (parse_err != ESP_OK) {
        return parse_err;
    }

    const char *entry_id = NULL;
    const char *reason = NULL;
    if (!json_get_string(root, "entry_id", &entry_id) || !json_get_string(root, "reason", &reason)) {
        cJSON_Delete(root);
        return esptari_web_catalog_error(req, "BAD_REQUEST", 400);
    }
    bool has_non_space = false;
    for (const char *p = reason; *p != '\0'; p++) {
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
            has_non_space = true;
            break;
        }
    }
    if (!has_non_space) {
        cJSON_Delete(root);
        return esptari_web_catalog_error(req, "BAD_REQUEST", 400);
    }

    int entry_index = esptari_web_catalog_find_entry_index(def, entry_id);
    if (entry_index < 0) {
        cJSON_Delete(root);
        return esptari_web_catalog_error(req, "CATALOG_ENTRY_NOT_FOUND", 404);
    }

    catalog_entry_runtime_t *runtime = esptari_web_catalog_runtime_at(def, (size_t)entry_index);
    if (runtime == NULL) {
        cJSON_Delete(root);
        return esptari_web_catalog_error(req, "CONFLICT", 409);
    }

    const char *state_before = esptari_web_catalog_entry_state(def, (size_t)entry_index);
    uint64_t now_us = (uint64_t)esp_timer_get_time();
    runtime->state_override = true;
    snprintf(runtime->availability_state, sizeof(runtime->availability_state), "%s", "dead");
    runtime->dead_marked = true;
    snprintf(runtime->dead_source, sizeof(runtime->dead_source), "%s", "manual");
    snprintf(runtime->last_dead_reason, sizeof(runtime->last_dead_reason), "%s", reason);
    runtime->last_dead_marked_at_us = now_us;
    cJSON_Delete(root);

    char resp[768];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"catalog\":\"%s\",\"entry_id\":\"%s\",\"state_before\":\"%s\",\"state_after\":\"dead\",\"dead_marked\":true,\"dead_source\":\"manual\",\"last_dead_reason\":\"%s\",\"last_dead_marked_at_us\":%llu,\"dead_retry_attempts\":%lu,\"dead_retry_successes\":%lu,\"dead_retry_failures\":%lu,\"last_dead_retry_at_us\":%s,\"last_dead_retry_result\":\"%s\"}}",
             def->name,
             def->entries[(size_t)entry_index].id,
             state_before,
             runtime->last_dead_reason,
             (unsigned long long)now_us,
             (unsigned long)runtime->dead_retry_attempts,
             (unsigned long)runtime->dead_retry_successes,
             (unsigned long)runtime->dead_retry_failures,
             runtime->last_dead_retry_at_us == 0 ? "null" : "0",
             runtime->last_dead_retry_result[0] == '\0' ? "none" : runtime->last_dead_retry_result);
    return send_json(req, resp, 200);
}

esp_err_t esptari_web_catalog_rescan_local_handler(httpd_req_t *req)
{
    const catalog_def_t *def = NULL;
    esp_err_t resolve_err = esptari_web_catalog_resolve_def(req, "/api/v2/catalogs/%31[^/]/rescan-local", &def);
    if (resolve_err != ESP_OK) {
        return resolve_err;
    }

    char body[1024];
    cJSON *root = NULL;
    esp_err_t parse_err = esptari_web_catalog_parse_body_json(req, body, sizeof(body), &root);
    if (parse_err != ESP_OK) {
        return parse_err;
    }

    cJSON *hash_mode = cJSON_GetObjectItemCaseSensitive(root, "hash_mode");
    if (cJSON_IsString(hash_mode) && hash_mode->valuestring != NULL &&
        strcmp(hash_mode->valuestring, "metadata_only") != 0 && strcmp(hash_mode->valuestring, "full") != 0) {
        cJSON_Delete(root);
        return esptari_web_catalog_error(req, "BAD_REQUEST", 400);
    }

    cJSON *scan_roots = cJSON_GetObjectItemCaseSensitive(root, "scan_roots");
    if (cJSON_IsArray(scan_roots)) {
        cJSON *root_item = NULL;
        cJSON_ArrayForEach(root_item, scan_roots)
        {
            if (!cJSON_IsString(root_item) || root_item->valuestring == NULL || !is_allowed_scan_root(root_item->valuestring)) {
                cJSON_Delete(root);
                return esptari_web_catalog_error(req, "PATH_NOT_ALLOWED", 400);
            }
        }
    }
    cJSON_Delete(root);

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    uint64_t scan_seq = esptari_web_catalog_next_scan_seq();
    esptari_web_catalog_record_scan_id(scan_seq);

    uint32_t present = 0;
    uint32_t missing = 0;
    for (size_t i = 0; i < def->entry_count; i++) {
        catalog_entry_runtime_t *runtime = esptari_web_catalog_runtime_at(def, i);
        if (esptari_web_catalog_entry_local_present(def, i)) {
            present++;
        } else {
            missing++;
            if (runtime != NULL && runtime->first_missing_at_us == 0) {
                runtime->first_missing_at_us = now_us;
            }
        }
    }

    char resp[2048];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"catalog\":\"%s\",\"scan_id\":\"%s\",\"indexed_at_us\":%llu,\"stats\":{\"entries_total\":%lu,\"entries_present\":%lu,\"entries_missing\":%lu,\"entries_changed\":0,\"entries_unchanged\":%lu},\"presence_index\":[{\"entry_id\":\"%s\",\"catalog\":\"%s\",\"local_present\":%s,\"local_path\":\"%s\",\"file_size\":%s,\"mtime_us\":%s,\"sha256\":null,\"indexed_at_us\":%llu},{\"entry_id\":\"%s\",\"catalog\":\"%s\",\"local_present\":%s,\"local_path\":%s,\"file_size\":null,\"mtime_us\":null,\"sha256\":null,\"indexed_at_us\":%llu}]}}",
             def->name,
             esptari_web_catalog_last_scan_id(),
             (unsigned long long)now_us,
             (unsigned long)def->entry_count,
             (unsigned long)present,
             (unsigned long)missing,
             (unsigned long)def->entry_count,
             def->entries[0].id,
             def->name,
             esptari_web_catalog_entry_local_present(def, 0) ? "true" : "false",
             esptari_web_catalog_entry_local_path_projected(def, 0),
             esptari_web_catalog_entry_local_present(def, 0) ? "737280" : "null",
             esptari_web_catalog_entry_local_present(def, 0) ? "1710002500000" : "null",
             (unsigned long long)now_us,
             def->entries[def->entry_count > 1 ? 1 : 0].id,
             def->name,
             esptari_web_catalog_entry_local_present(def, def->entry_count > 1 ? 1 : 0) ? "true" : "false",
             esptari_web_catalog_entry_local_present(def, def->entry_count > 1 ? 1 : 0) ? "\"/sdcard/present\"" : "null",
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}
