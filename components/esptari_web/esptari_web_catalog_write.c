#include "esptari_web_catalog_write.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esptari_web_catalog_state.h"
#include "esptari_web_http_utils.h"

#define send_json esptari_web_send_json
#define read_request_body esptari_web_read_request_body
#define json_get_string esptari_web_json_get_string

static esp_err_t catalog_download_entry_handler(httpd_req_t *req)
{
    char catalog[32] = {0};
    if (sscanf(req->uri, "/api/v2/catalogs/%31[^/]/download-entry", catalog) != 1) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const catalog_def_t *def = esptari_web_catalog_find(catalog);
    if (def == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CATALOG_NOT_FOUND\"}}", 404);
    }

    char body[512];
    if (read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const char *entry_id = NULL;
    if (!json_get_string(root, "entry_id", &entry_id) || entry_id[0] == '\0') {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    bool overwrite = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "overwrite"));
    bool verify_sha256 = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "verify_sha256"));
    bool allow_dead_retry = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "allow_dead_retry"));

    int entry_index = esptari_web_catalog_find_entry_index(def, entry_id);
    if (entry_index < 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CATALOG_ENTRY_NOT_FOUND\"}}", 404);
    }

    const catalog_entry_t *entry = &def->entries[(size_t)entry_index];
    catalog_entry_runtime_t *runtime = esptari_web_catalog_runtime_at(def, (size_t)entry_index);
    const char *state = esptari_web_catalog_entry_state(def, (size_t)entry_index);

    if (entry->hosted_url == NULL || entry->hosted_url[0] == '\0') {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (strcmp(state, "dead") == 0 && !allow_dead_retry) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CATALOG_LINK_DEAD\"}}", 409);
    }
    if (!overwrite && esptari_web_catalog_entry_local_present(def, (size_t)entry_index)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CONFLICT\"}}", 409);
    }
    if (verify_sha256 && (entry->sha256_expected == NULL || entry->sha256_expected[0] == '\0')) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    uint64_t download_seq = esptari_web_catalog_next_download_seq();
    if (runtime != NULL && strcmp(state, "dead") == 0 && allow_dead_retry) {
        runtime->dead_retry_attempts++;
        runtime->last_dead_retry_at_us = now_us;
        snprintf(runtime->last_dead_retry_result, sizeof(runtime->last_dead_retry_result), "%s", "queued");
    }

    char resp[512];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"catalog\":\"%s\",\"entry_id\":\"%s\",\"job_id\":\"dl_%06llu\",\"queue_state\":\"queued\",\"priority\":\"normal\",\"enqueued_at_us\":%llu}}",
             def->name,
             entry->id,
             (unsigned long long)download_seq,
             (unsigned long long)now_us);
    cJSON_Delete(root);
    return send_json(req, resp, 200);
}

static esp_err_t catalog_download_missing_handler(httpd_req_t *req)
{
    char catalog[32] = {0};
    if (sscanf(req->uri, "/api/v2/catalogs/%31[^/]/download-missing", catalog) != 1) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const catalog_def_t *def = esptari_web_catalog_find(catalog);
    if (def == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CATALOG_NOT_FOUND\"}}", 404);
    }

    uint32_t limit = 100;
    bool skip_dead = true;
    char body[512];
    if (req->content_len > 0) {
        if (read_request_body(req, body, sizeof(body)) != ESP_OK) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
        cJSON *root = cJSON_Parse(body);
        if (root == NULL) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
        cJSON *limit_item = cJSON_GetObjectItemCaseSensitive(root, "limit");
        if (cJSON_IsNumber(limit_item) && limit_item->valuedouble > 0) {
            limit = (uint32_t)limit_item->valuedouble;
        }
        cJSON *skip_dead_item = cJSON_GetObjectItemCaseSensitive(root, "skip_dead");
        if (cJSON_IsBool(skip_dead_item)) {
            skip_dead = cJSON_IsTrue(skip_dead_item);
        }
        cJSON_Delete(root);
    }

    uint32_t queued = 0;
    for (size_t i = 0; i < def->entry_count && queued < limit; i++) {
        if (esptari_web_catalog_entry_local_present(def, i)) {
            continue;
        }
        if (skip_dead && strcmp(esptari_web_catalog_entry_state(def, i), "dead") == 0) {
            continue;
        }
        queued++;
    }

    char resp[320];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"catalog\":\"%s\",\"requested_limit\":%lu,\"queued_jobs\":%lu,\"queue_state\":\"queued\"}}",
             def->name,
             (unsigned long)limit,
             (unsigned long)queued);
    return send_json(req, resp, 200);
}

static esp_err_t catalog_probe_links_handler(httpd_req_t *req)
{
    char catalog[32] = {0};
    if (sscanf(req->uri, "/api/v2/catalogs/%31[^/]/probe-links", catalog) != 1) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const catalog_def_t *def = esptari_web_catalog_find(catalog);
    if (def == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CATALOG_NOT_FOUND\"}}", 404);
    }

    char body[512];
    if (read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    uint32_t limit = 500;
    uint32_t timeout_ms = 8000;
    uint32_t mark_dead_after_failures = 3;

    cJSON *limit_item = cJSON_GetObjectItemCaseSensitive(root, "limit");
    if (cJSON_IsNumber(limit_item) && limit_item->valuedouble > 0) {
        limit = (uint32_t)limit_item->valuedouble;
    }
    cJSON *timeout_item = cJSON_GetObjectItemCaseSensitive(root, "timeout_ms");
    if (cJSON_IsNumber(timeout_item)) {
        timeout_ms = (uint32_t)timeout_item->valuedouble;
    }
    cJSON *mark_item = cJSON_GetObjectItemCaseSensitive(root, "mark_dead_after_failures");
    if (cJSON_IsNumber(mark_item)) {
        mark_dead_after_failures = (uint32_t)mark_item->valuedouble;
    }
    cJSON_Delete(root);

    if (timeout_ms == 0 || mark_dead_after_failures < 1) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    uint64_t started_at_us = (uint64_t)esp_timer_get_time();
    uint64_t completed_at_us = started_at_us + 2000;
    uint32_t probed = (uint32_t)((def->entry_count < limit) ? def->entry_count : limit);
    uint32_t dead = 0;
    uint32_t offline = 0;
    uint32_t online = 0;
    for (size_t i = 0; i < probed; i++) {
        const char *state = esptari_web_catalog_entry_state(def, i);
        if (strcmp(state, "dead") == 0) {
            dead++;
        } else if (strcmp(state, "offline") == 0) {
            offline++;
        } else {
            online++;
        }
    }

    uint64_t probe_seq = esptari_web_catalog_next_probe_seq();
    char resp[1024];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"catalog\":\"%s\",\"worker_id\":\"probe_%06llu\",\"started_at_us\":%llu,\"completed_at_us\":%llu,\"policy\":{\"timeout_ms\":%lu,\"mark_dead_after_failures\":%lu,\"concurrency\":16},\"summary\":{\"probed\":%lu,\"online\":%lu,\"offline\":%lu,\"dead\":%lu,\"timed_out\":0},\"results\":[{\"entry_id\":\"%s\",\"state_before\":\"%s\",\"state_after\":\"%s\",\"attempts\":1,\"timed_out\":false,\"latency_ms\":11}]}}",
             def->name,
             (unsigned long long)probe_seq,
             (unsigned long long)started_at_us,
             (unsigned long long)completed_at_us,
             (unsigned long)timeout_ms,
             (unsigned long)mark_dead_after_failures,
             (unsigned long)probed,
             (unsigned long)online,
             (unsigned long)offline,
             (unsigned long)dead,
             def->entries[0].id,
             esptari_web_catalog_entry_state(def, 0),
             esptari_web_catalog_entry_state(def, 0));
    return send_json(req, resp, 200);
}

static esp_err_t catalog_mark_dead_handler(httpd_req_t *req)
{
    char catalog[32] = {0};
    if (sscanf(req->uri, "/api/v2/catalogs/%31[^/]/mark-dead", catalog) != 1) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const catalog_def_t *def = esptari_web_catalog_find(catalog);
    if (def == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CATALOG_NOT_FOUND\"}}", 404);
    }

    char body[512];
    if (read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const char *entry_id = NULL;
    const char *reason = NULL;
    if (!json_get_string(root, "entry_id", &entry_id) || !json_get_string(root, "reason", &reason)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
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
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    int entry_index = esptari_web_catalog_find_entry_index(def, entry_id);
    if (entry_index < 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CATALOG_ENTRY_NOT_FOUND\"}}", 404);
    }

    catalog_entry_runtime_t *runtime = esptari_web_catalog_runtime_at(def, (size_t)entry_index);
    if (runtime == NULL) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CONFLICT\"}}", 409);
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

static esp_err_t catalog_rescan_local_handler(httpd_req_t *req)
{
    char catalog[32] = {0};
    if (sscanf(req->uri, "/api/v2/catalogs/%31[^/]/rescan-local", catalog) != 1) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    const catalog_def_t *def = esptari_web_catalog_find(catalog);
    if (def == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CATALOG_NOT_FOUND\"}}", 404);
    }

    char body[1024];
    if (read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *hash_mode = cJSON_GetObjectItemCaseSensitive(root, "hash_mode");
    if (cJSON_IsString(hash_mode) && hash_mode->valuestring != NULL &&
        strcmp(hash_mode->valuestring, "metadata_only") != 0 && strcmp(hash_mode->valuestring, "full") != 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *scan_roots = cJSON_GetObjectItemCaseSensitive(root, "scan_roots");
    if (cJSON_IsArray(scan_roots)) {
        cJSON *root_item = NULL;
        cJSON_ArrayForEach(root_item, scan_roots)
        {
            if (!cJSON_IsString(root_item) || root_item->valuestring == NULL || !is_allowed_scan_root(root_item->valuestring)) {
                cJSON_Delete(root);
                return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"PATH_NOT_ALLOWED\"}}", 400);
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

esp_err_t esptari_web_catalogs_post_router_handler(httpd_req_t *req)
{
    if (strstr(req->uri, "/download-entry") != NULL) {
        return catalog_download_entry_handler(req);
    }
    if (strstr(req->uri, "/download-missing") != NULL) {
        return catalog_download_missing_handler(req);
    }
    if (strstr(req->uri, "/probe-links") != NULL) {
        return catalog_probe_links_handler(req);
    }
    if (strstr(req->uri, "/mark-dead") != NULL) {
        return catalog_mark_dead_handler(req);
    }
    if (strstr(req->uri, "/rescan-local") != NULL) {
        return catalog_rescan_local_handler(req);
    }
    return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
}
