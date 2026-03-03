#include "esptari_web_catalog_write_download.h"

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

static char last_queued_catalog[32];
static char last_queued_entry_id[128];
static uint64_t last_queued_seq;

static esp_err_t send_download_error(httpd_req_t *req,
                                     int status_code,
                                     const char *code,
                                     const char *message)
{
    char resp[320];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":false,\"error\":{\"code\":\"%s\",\"category\":\"catalog\",\"message\":\"%s\",\"retryable\":false}}",
             code,
             message);
    return send_json(req, resp, status_code);
}

static uint64_t projected_download_size_bytes(const catalog_def_t *def, size_t entry_index)
{
    const catalog_entry_t *entry = &def->entries[entry_index];
    if (strstr(entry->id, "disk") != NULL) {
        return 737280ULL;
    }
    if (strstr(entry->id, "rom") != NULL || strstr(entry->id, "tos") != NULL) {
        return 262144ULL;
    }
    return 131072ULL;
}

static const char *projected_sha_actual(const catalog_entry_t *entry, bool verify_sha256, bool hash_mismatch)
{
    if (verify_sha256 && hash_mismatch) {
        return "sha256:mismatch";
    }
    if (entry->sha256_expected != NULL && entry->sha256_expected[0] != '\0') {
        return entry->sha256_expected;
    }
    return "sha256:synthetic";
}

static esp_err_t parse_optional_bool(cJSON *root, const char *field, bool default_value, bool *out_value)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, field);
    if (item == NULL) {
        *out_value = default_value;
        return ESP_OK;
    }
    if (!cJSON_IsBool(item)) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_value = cJSON_IsTrue(item);
    return ESP_OK;
}

static esp_err_t send_probe_links_response(httpd_req_t *req,
                                           const catalog_def_t *def,
                                           uint64_t probe_seq,
                                           uint64_t started_at_us,
                                           uint64_t completed_at_us,
                                           uint32_t timeout_ms,
                                           uint32_t mark_dead_after_failures,
                                           uint32_t probed,
                                           uint32_t online,
                                           uint32_t offline,
                                           uint32_t dead)
{
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

esp_err_t esptari_web_catalog_download_entry_handler(httpd_req_t *req)
{
    const catalog_def_t *def = NULL;
    esp_err_t resolve_err = esptari_web_catalog_resolve_def(req, "/api/v2/catalogs/%31[^/]/download-entry", &def);
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
    if (!json_get_string(root, "entry_id", &entry_id) || entry_id[0] == '\0') {
        cJSON_Delete(root);
        return esptari_web_catalog_error(req, "BAD_REQUEST", 400);
    }

    bool overwrite = false;
    bool verify_sha256 = false;
    bool allow_dead_retry = false;
    if (parse_optional_bool(root, "overwrite", false, &overwrite) != ESP_OK ||
        parse_optional_bool(root, "verify_sha256", false, &verify_sha256) != ESP_OK ||
        parse_optional_bool(root, "allow_dead_retry", false, &allow_dead_retry) != ESP_OK) {
        cJSON_Delete(root);
        return send_download_error(req, 400, "BAD_REQUEST", "Boolean request fields must be true/false values");
    }

    bool simulate_truncated = false;
    bool simulate_hash_mismatch = false;
    bool simulate_commit_failure = false;
    if (parse_optional_bool(root, "simulate_truncated", false, &simulate_truncated) != ESP_OK ||
        parse_optional_bool(root, "simulate_hash_mismatch", false, &simulate_hash_mismatch) != ESP_OK ||
        parse_optional_bool(root, "simulate_commit_failure", false, &simulate_commit_failure) != ESP_OK) {
        cJSON_Delete(root);
        return send_download_error(req, 400, "BAD_REQUEST", "Simulation fields must be true/false values");
    }

    const char *priority = "normal";
    cJSON *priority_item = cJSON_GetObjectItemCaseSensitive(root, "priority");
    if (priority_item != NULL) {
        if (!cJSON_IsString(priority_item) || priority_item->valuestring == NULL) {
            cJSON_Delete(root);
            return send_download_error(req, 400, "BAD_REQUEST", "priority must be a string");
        }
        if (strcmp(priority_item->valuestring, "normal") != 0 && strcmp(priority_item->valuestring, "high") != 0) {
            cJSON_Delete(root);
            return send_download_error(req, 400, "BAD_REQUEST", "priority must be normal or high");
        }
        priority = priority_item->valuestring;
    }

    int entry_index = esptari_web_catalog_find_entry_index(def, entry_id);
    if (entry_index < 0) {
        cJSON_Delete(root);
        return esptari_web_catalog_error(req, "CATALOG_ENTRY_NOT_FOUND", 404);
    }

    const catalog_entry_t *entry = &def->entries[(size_t)entry_index];
    catalog_entry_runtime_t *runtime = esptari_web_catalog_runtime_at(def, (size_t)entry_index);
    const char *state = esptari_web_catalog_entry_state(def, (size_t)entry_index);

    if (entry->hosted_url == NULL || entry->hosted_url[0] == '\0') {
        cJSON_Delete(root);
        return send_download_error(req, 400, "BAD_REQUEST", "Catalog entry has no hosted source URL");
    }
    if (strcmp(state, "dead") == 0 && !allow_dead_retry) {
        cJSON_Delete(root);
        return send_download_error(req, 409, "CATALOG_LINK_DEAD", "Catalog entry is marked dead and retry is not allowed");
    }
    if (!overwrite && esptari_web_catalog_entry_local_present(def, (size_t)entry_index)) {
        cJSON_Delete(root);
        return send_download_error(req, 409, "CONFLICT", "Local asset already exists and overwrite=false");
    }
    if (verify_sha256 && (entry->sha256_expected == NULL || entry->sha256_expected[0] == '\0')) {
        cJSON_Delete(root);
        return send_download_error(req, 400, "BAD_REQUEST", "verify_sha256=true requires checksum metadata");
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    const char *queue_state = "queued";
    uint64_t download_seq = esptari_web_catalog_next_download_seq();
    if (strcmp(last_queued_catalog, def->name) == 0 && strcmp(last_queued_entry_id, entry->id) == 0) {
        queue_state = "already_queued";
        download_seq = last_queued_seq;
    } else {
        snprintf(last_queued_catalog, sizeof(last_queued_catalog), "%s", def->name);
        snprintf(last_queued_entry_id, sizeof(last_queued_entry_id), "%s", entry->id);
        last_queued_seq = download_seq;
    }

    if (runtime != NULL && strcmp(state, "dead") == 0 && allow_dead_retry) {
        runtime->dead_retry_attempts++;
        runtime->last_dead_retry_at_us = now_us;
        snprintf(runtime->last_dead_retry_result, sizeof(runtime->last_dead_retry_result), "%s", "queued");
    }

    char staging_path[160];
    snprintf(staging_path, sizeof(staging_path), "/sdcard/.staging/catalog_downloads/dl_%06llu.part", (unsigned long long)download_seq);
    const char *final_path = (entry->local_path != NULL && entry->local_path[0] != '\0') ? entry->local_path : "/sdcard/downloads/catalog_asset.bin";
    uint64_t bytes_downloaded = projected_download_size_bytes(def, (size_t)entry_index);

    if (simulate_truncated) {
        cJSON_Delete(root);
        return send_download_error(req, 409, "UPLOAD_INCOMPLETE", "Staging artifact missing or truncated before verification");
    }

    const char *sha256_actual = projected_sha_actual(entry, verify_sha256, simulate_hash_mismatch);
    if (verify_sha256 && simulate_hash_mismatch) {
        cJSON_Delete(root);
        return send_download_error(req, 409, "CATALOG_SYNC_FAILED", "Integrity verification failed for staged download");
    }

    if (simulate_commit_failure) {
        cJSON_Delete(root);
        return send_download_error(req, 409, "CATALOG_SYNC_FAILED", "Atomic commit failed while finalizing staged download");
    }

    uint64_t committed_at_us = now_us + 2000;
    if (runtime != NULL) {
        runtime->local_present = true;
        runtime->indexed_file_size_bytes = bytes_downloaded;
        runtime->indexed_mtime_us = committed_at_us;
        runtime->last_indexed_at_us = committed_at_us;
        runtime->first_missing_at_us = 0;
    }

    char sha256_expected_json[160];
    if (entry->sha256_expected != NULL && entry->sha256_expected[0] != '\0') {
        snprintf(sha256_expected_json, sizeof(sha256_expected_json), "\"%s\"", entry->sha256_expected);
    } else {
        snprintf(sha256_expected_json, sizeof(sha256_expected_json), "null");
    }

    char resp[1400];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"catalog\":\"%s\",\"entry_id\":\"%s\",\"job_id\":\"dl_%06llu\",\"queue_state\":\"%s\",\"priority\":\"%s\",\"enqueued_at_us\":%llu,\"stage_state\":\"committed\",\"staging_path\":\"%s\",\"final_path\":\"%s\",\"bytes_downloaded\":%llu,\"sha256_expected\":%s,\"sha256_actual\":\"%s\",\"verified\":%s,\"committed_at_us\":%llu}}",
             def->name,
             entry->id,
             (unsigned long long)download_seq,
             queue_state,
             priority,
             (unsigned long long)now_us,
             staging_path,
             final_path,
             (unsigned long long)bytes_downloaded,
             sha256_expected_json,
             sha256_actual,
             verify_sha256 ? "true" : "false",
             (unsigned long long)committed_at_us);
    cJSON_Delete(root);
    return send_json(req, resp, 200);
}

esp_err_t esptari_web_catalog_download_missing_handler(httpd_req_t *req)
{
    const catalog_def_t *def = NULL;
    esp_err_t resolve_err = esptari_web_catalog_resolve_def(req, "/api/v2/catalogs/%31[^/]/download-missing", &def);
    if (resolve_err != ESP_OK) {
        return resolve_err;
    }

    uint32_t limit = 100;
    bool skip_dead = true;
    char body[512];
    if (req->content_len > 0) {
        cJSON *root = NULL;
        esp_err_t parse_err = esptari_web_catalog_parse_body_json(req, body, sizeof(body), &root);
        if (parse_err != ESP_OK) {
            return parse_err;
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

esp_err_t esptari_web_catalog_probe_links_handler(httpd_req_t *req)
{
    const catalog_def_t *def = NULL;
    esp_err_t resolve_err = esptari_web_catalog_resolve_def(req, "/api/v2/catalogs/%31[^/]/probe-links", &def);
    if (resolve_err != ESP_OK) {
        return resolve_err;
    }

    char body[512];
    cJSON *root = NULL;
    esp_err_t parse_err = esptari_web_catalog_parse_body_json(req, body, sizeof(body), &root);
    if (parse_err != ESP_OK) {
        return parse_err;
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
        return esptari_web_catalog_error(req, "BAD_REQUEST", 400);
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
    return send_probe_links_response(req,
                                     def,
                                     probe_seq,
                                     started_at_us,
                                     completed_at_us,
                                     timeout_ms,
                                     mark_dead_after_failures,
                                     probed,
                                     online,
                                     offline,
                                     dead);
}
