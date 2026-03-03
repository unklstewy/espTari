#include "esptari_web_catalog_write_maintenance.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esptari_web_catalog_state.h"
#include "esptari_web_catalog_utils.h"
#include "esptari_web_http_utils.h"

#define send_json esptari_web_send_json
#define json_get_string esptari_web_json_get_string

static bool read_file_index_info(const char *path, uint64_t *file_size, uint64_t *mtime_us)
{
    if (path == NULL || path[0] == '\0') {
        return false;
    }
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    if (!S_ISREG(st.st_mode)) {
        return false;
    }
    *file_size = (uint64_t)st.st_size;
    *mtime_us = (uint64_t)st.st_mtim.tv_sec * 1000000ULL + (uint64_t)st.st_mtim.tv_nsec / 1000ULL;
    return true;
}

static esp_err_t send_rescan_local_response(httpd_req_t *req,
                                            const catalog_def_t *def,
                                            uint64_t now_us,
                                            uint32_t present,
                                            uint32_t missing,
                                            uint32_t changed,
                                            uint32_t unchanged)
{
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    cJSON_AddStringToObject(data, "catalog", def->name);
    cJSON_AddStringToObject(data, "scan_id", esptari_web_catalog_last_scan_id());
    cJSON_AddNumberToObject(data, "indexed_at_us", (double)now_us);

    cJSON *stats = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "stats", stats);
    cJSON_AddNumberToObject(stats, "entries_total", (double)def->entry_count);
    cJSON_AddNumberToObject(stats, "entries_present", (double)present);
    cJSON_AddNumberToObject(stats, "entries_missing", (double)missing);
    cJSON_AddNumberToObject(stats, "entries_changed", (double)changed);
    cJSON_AddNumberToObject(stats, "entries_unchanged", (double)unchanged);

    cJSON *presence_index = cJSON_CreateArray();
    cJSON_AddItemToObject(data, "presence_index", presence_index);

    for (size_t i = 0; i < def->entry_count; i++) {
        const catalog_entry_t *entry = &def->entries[i];
        catalog_entry_runtime_t *runtime = esptari_web_catalog_runtime_at(def, i);
        bool local_present = runtime != NULL ? runtime->local_present : (entry->local_path != NULL && entry->local_path[0] != '\0');

        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "entry_id", entry->id);
        cJSON_AddStringToObject(item, "catalog", def->name);
        cJSON_AddBoolToObject(item, "local_present", local_present);
        if (local_present && entry->local_path != NULL && entry->local_path[0] != '\0') {
            cJSON_AddStringToObject(item, "local_path", entry->local_path);
        } else {
            cJSON_AddNullToObject(item, "local_path");
        }
        if (local_present && runtime != NULL) {
            cJSON_AddNumberToObject(item, "file_size", (double)runtime->indexed_file_size_bytes);
            cJSON_AddNumberToObject(item, "mtime_us", (double)runtime->indexed_mtime_us);
        } else {
            cJSON_AddNullToObject(item, "file_size");
            cJSON_AddNullToObject(item, "mtime_us");
        }
        cJSON_AddNullToObject(item, "sha256");
        cJSON_AddNumberToObject(item, "indexed_at_us", (double)now_us);
        cJSON_AddItemToArray(presence_index, item);
    }

    char *resp_json = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (resp_json == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }
    esp_err_t out = send_json(req, resp_json, 200);
    free(resp_json);
    return out;
}

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

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    cJSON_AddStringToObject(data, "catalog", def->name);
    cJSON_AddStringToObject(data, "entry_id", def->entries[(size_t)entry_index].id);
    cJSON_AddStringToObject(data, "state_before", state_before);
    cJSON_AddStringToObject(data, "state_after", "dead");
    cJSON_AddBoolToObject(data, "dead_marked", true);
    cJSON_AddStringToObject(data, "dead_source", "manual");
    cJSON_AddStringToObject(data, "last_dead_reason", runtime->last_dead_reason);
    cJSON_AddNumberToObject(data, "last_dead_marked_at_us", (double)now_us);
    cJSON_AddNumberToObject(data, "dead_retry_attempts", (double)runtime->dead_retry_attempts);
    cJSON_AddNumberToObject(data, "dead_retry_successes", (double)runtime->dead_retry_successes);
    cJSON_AddNumberToObject(data, "dead_retry_failures", (double)runtime->dead_retry_failures);
    if (runtime->last_dead_retry_at_us == 0) {
        cJSON_AddNullToObject(data, "last_dead_retry_at_us");
    } else {
        cJSON_AddNumberToObject(data, "last_dead_retry_at_us", (double)runtime->last_dead_retry_at_us);
    }
    cJSON_AddStringToObject(data,
                            "last_dead_retry_result",
                            runtime->last_dead_retry_result[0] == '\0' ? "none" : runtime->last_dead_retry_result);

    char *resp_json = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (resp_json == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }
    esp_err_t out = send_json(req, resp_json, 200);
    free(resp_json);
    return out;
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
    uint32_t changed = 0;
    uint32_t unchanged = 0;
    for (size_t i = 0; i < def->entry_count; i++) {
        const catalog_entry_t *entry = &def->entries[i];
        catalog_entry_runtime_t *runtime = esptari_web_catalog_runtime_at(def, i);

        bool was_present = runtime != NULL ? runtime->local_present : (entry->local_path != NULL && entry->local_path[0] != '\0');
        uint64_t file_size = 0;
        uint64_t mtime_us = 0;
        bool is_present = read_file_index_info(entry->local_path, &file_size, &mtime_us);

        if (runtime != NULL) {
            runtime->local_present = is_present;
            runtime->last_indexed_scan_seq = scan_seq;
            runtime->last_indexed_at_us = now_us;
            runtime->indexed_file_size_bytes = is_present ? file_size : 0;
            runtime->indexed_mtime_us = is_present ? mtime_us : 0;

            if (was_present != is_present) {
                if (is_present) {
                    runtime->last_transition_to_present_scan_seq = scan_seq;
                } else {
                    runtime->last_transition_to_missing_scan_seq = scan_seq;
                }
            }
        }

        if (was_present != is_present) {
            changed++;
        } else {
            unchanged++;
        }

        if (is_present) {
            present++;
            if (runtime != NULL) {
                runtime->first_missing_at_us = 0;
            }
        } else {
            missing++;
            if (runtime != NULL && runtime->first_missing_at_us == 0) {
                runtime->first_missing_at_us = now_us;
            }
        }
    }

    return send_rescan_local_response(req, def, now_us, present, missing, changed, unchanged);
}
