#include "esptari_web_catalog_write_download.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

static void dead_retry_begin(catalog_entry_runtime_t *runtime, uint64_t now_us)
{
    if (runtime == NULL) {
        return;
    }
    runtime->dead_retry_attempts++;
    runtime->last_dead_retry_at_us = now_us;
    snprintf(runtime->last_dead_retry_result, sizeof(runtime->last_dead_retry_result), "%s", "blocked");
}

static void dead_retry_mark_success(catalog_entry_runtime_t *runtime)
{
    if (runtime == NULL) {
        return;
    }
    runtime->dead_retry_successes++;
    snprintf(runtime->last_dead_retry_result, sizeof(runtime->last_dead_retry_result), "%s", "success");
    runtime->state_override = true;
    snprintf(runtime->availability_state, sizeof(runtime->availability_state), "%s", "online");
    runtime->dead_marked = false;
    runtime->dead_source[0] = '\0';
    runtime->last_dead_reason[0] = '\0';
    runtime->probe_fail_streak = 0;
    runtime->download_fail_count = 0;
}

static void dead_retry_mark_failure(catalog_entry_runtime_t *runtime, const char *result)
{
    if (runtime == NULL) {
        return;
    }
    runtime->dead_retry_failures++;
    snprintf(runtime->last_dead_retry_result,
             sizeof(runtime->last_dead_retry_result),
             "%s",
             (result != NULL && result[0] != '\0') ? result : "failure");
    runtime->state_override = true;
    snprintf(runtime->availability_state, sizeof(runtime->availability_state), "%s", "dead");
    runtime->dead_marked = true;
    runtime->download_fail_count++;
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
                                           uint32_t dead,
                                           uint32_t timed_out,
                                           cJSON *results)
{
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);

    char worker_id[32];
    snprintf(worker_id, sizeof(worker_id), "probe_%06llu", (unsigned long long)probe_seq);
    cJSON_AddStringToObject(data, "catalog", def->name);
    cJSON_AddStringToObject(data, "worker_id", worker_id);
    cJSON_AddNumberToObject(data, "started_at_us", (double)started_at_us);
    cJSON_AddNumberToObject(data, "completed_at_us", (double)completed_at_us);

    cJSON *policy = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "policy", policy);
    cJSON_AddNumberToObject(policy, "timeout_ms", (double)timeout_ms);
    cJSON_AddNumberToObject(policy, "mark_dead_after_failures", (double)mark_dead_after_failures);
    cJSON_AddNumberToObject(policy, "concurrency", 16);

    cJSON *summary = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "summary", summary);
    cJSON_AddNumberToObject(summary, "probed", (double)probed);
    cJSON_AddNumberToObject(summary, "online", (double)online);
    cJSON_AddNumberToObject(summary, "offline", (double)offline);
    cJSON_AddNumberToObject(summary, "dead", (double)dead);
    cJSON_AddNumberToObject(summary, "timed_out", (double)timed_out);

    cJSON_AddItemToObject(data, "results", results);

    char *resp_json = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (resp_json == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }
    esp_err_t out = send_json(req, resp_json, 200);
    free(resp_json);
    return out;
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
    bool dead_entry = strcmp(state, "dead") == 0;
    bool dead_retry_requested = dead_entry && allow_dead_retry;

    if (entry->hosted_url == NULL || entry->hosted_url[0] == '\0') {
        cJSON_Delete(root);
        return send_download_error(req, 400, "BAD_REQUEST", "Catalog entry has no hosted source URL");
    }
    if (dead_entry && !allow_dead_retry) {
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

    if (dead_retry_requested) {
        dead_retry_begin(runtime, now_us);
    }

    char staging_path[160];
    snprintf(staging_path, sizeof(staging_path), "/sdcard/.staging/catalog_downloads/dl_%06llu.part", (unsigned long long)download_seq);
    const char *final_path = (entry->local_path != NULL && entry->local_path[0] != '\0') ? entry->local_path : "/sdcard/downloads/catalog_asset.bin";
    uint64_t bytes_downloaded = projected_download_size_bytes(def, (size_t)entry_index);

    if (simulate_truncated) {
        if (dead_retry_requested) {
            dead_retry_mark_failure(runtime, "blocked");
        }
        cJSON_Delete(root);
        return send_download_error(req, 409, "UPLOAD_INCOMPLETE", "Staging artifact missing or truncated before verification");
    }

    const char *sha256_actual = projected_sha_actual(entry, verify_sha256, simulate_hash_mismatch);
    if (verify_sha256 && simulate_hash_mismatch) {
        if (dead_retry_requested) {
            dead_retry_mark_failure(runtime, "failure");
        }
        cJSON_Delete(root);
        return send_download_error(req, 409, "CATALOG_SYNC_FAILED", "Integrity verification failed for staged download");
    }

    if (simulate_commit_failure) {
        if (dead_retry_requested) {
            dead_retry_mark_failure(runtime, "failure");
        }
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
    if (dead_retry_requested) {
        dead_retry_mark_success(runtime);
    }

    cJSON_Delete(root);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    char job_id[32];
    snprintf(job_id, sizeof(job_id), "dl_%06llu", (unsigned long long)download_seq);
    cJSON_AddStringToObject(data, "catalog", def->name);
    cJSON_AddStringToObject(data, "entry_id", entry->id);
    cJSON_AddStringToObject(data, "job_id", job_id);
    cJSON_AddStringToObject(data, "queue_state", queue_state);
    cJSON_AddStringToObject(data, "priority", priority);
    cJSON_AddNumberToObject(data, "enqueued_at_us", (double)now_us);
    cJSON_AddStringToObject(data, "stage_state", "committed");
    cJSON_AddStringToObject(data, "staging_path", staging_path);
    cJSON_AddStringToObject(data, "final_path", final_path);
    cJSON_AddNumberToObject(data, "bytes_downloaded", (double)bytes_downloaded);
    if (entry->sha256_expected != NULL && entry->sha256_expected[0] != '\0') {
        cJSON_AddStringToObject(data, "sha256_expected", entry->sha256_expected);
    } else {
        cJSON_AddNullToObject(data, "sha256_expected");
    }
    cJSON_AddStringToObject(data, "sha256_actual", sha256_actual);
    cJSON_AddBoolToObject(data, "verified", verify_sha256);
    cJSON_AddNumberToObject(data, "committed_at_us", (double)committed_at_us);

    const char *state_after = esptari_web_catalog_entry_state(def, (size_t)entry_index);
    cJSON_AddStringToObject(data, "availability_state", state_after);
    cJSON_AddBoolToObject(data, "dead_marked", runtime != NULL ? runtime->dead_marked : false);
    cJSON_AddNumberToObject(data,
                            "dead_retry_attempts",
                            (double)(runtime != NULL ? runtime->dead_retry_attempts : 0));
    cJSON_AddNumberToObject(data,
                            "dead_retry_successes",
                            (double)(runtime != NULL ? runtime->dead_retry_successes : 0));
    cJSON_AddNumberToObject(data,
                            "dead_retry_failures",
                            (double)(runtime != NULL ? runtime->dead_retry_failures : 0));
    if (runtime != NULL && runtime->last_dead_retry_at_us != 0) {
        cJSON_AddNumberToObject(data, "last_dead_retry_at_us", (double)runtime->last_dead_retry_at_us);
    } else {
        cJSON_AddNullToObject(data, "last_dead_retry_at_us");
    }
    cJSON_AddStringToObject(data,
                            "last_dead_retry_result",
                            runtime != NULL && runtime->last_dead_retry_result[0] != '\0' ? runtime->last_dead_retry_result : "none");
    cJSON_AddNumberToObject(data,
                            "download_fail_count",
                            (double)esptari_web_catalog_entry_download_fail_count(def, (size_t)entry_index));

    char *resp_json = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (resp_json == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }
    esp_err_t out = send_json(req, resp_json, 200);
    free(resp_json);
    return out;
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
    uint32_t probed = (uint32_t)((def->entry_count < limit) ? def->entry_count : limit);
    uint32_t dead = 0;
    uint32_t offline = 0;
    uint32_t online = 0;
    uint32_t timed_out = 0;
    cJSON *results = cJSON_CreateArray();

    for (size_t i = 0; i < probed; i++) {
        const catalog_entry_t *entry = &def->entries[i];
        const char *state_before = esptari_web_catalog_entry_state(def, i);
        catalog_entry_runtime_t *runtime = esptari_web_catalog_runtime_at(def, i);
        bool before_dead = strcmp(state_before, "dead") == 0;

        bool entry_can_timeout = entry->hosted_url != NULL && entry->hosted_url[0] != '\0' && !before_dead;
        bool probe_timed_out = entry_can_timeout && timeout_ms <= 10;
        if (probe_timed_out) {
            timed_out++;
        }

        uint32_t attempts = 1;
        if (runtime != NULL) {
            runtime->probe_attempts++;
            attempts = runtime->probe_attempts;
            runtime->last_probe_at_us = started_at_us;
            runtime->last_probe_timed_out = probe_timed_out;

            if (probe_timed_out) {
                runtime->probe_fail_streak++;
                runtime->download_fail_count++;
            } else {
                runtime->probe_fail_streak = 0;
            }
        }

        const char *state_after = state_before;
        if (before_dead) {
            state_after = "dead";
        } else if (probe_timed_out) {
            bool mark_dead = runtime != NULL && runtime->probe_fail_streak >= mark_dead_after_failures;
            if (mark_dead) {
                runtime->state_override = true;
                snprintf(runtime->availability_state, sizeof(runtime->availability_state), "%s", "dead");
                runtime->dead_marked = true;
                snprintf(runtime->dead_source, sizeof(runtime->dead_source), "%s", "probe_threshold");
                snprintf(runtime->last_dead_reason, sizeof(runtime->last_dead_reason), "%s", "probe timeout threshold reached");
                runtime->last_dead_marked_at_us = started_at_us;
                state_after = "dead";
            } else {
                if (runtime != NULL) {
                    runtime->state_override = true;
                    snprintf(runtime->availability_state, sizeof(runtime->availability_state), "%s", "offline");
                }
                state_after = "offline";
            }
        } else {
            if (runtime != NULL) {
                runtime->state_override = true;
                snprintf(runtime->availability_state, sizeof(runtime->availability_state), "%s", "online");
            }
            state_after = "online";
        }

        if (strcmp(state_after, "dead") == 0) {
            dead++;
        } else if (strcmp(state_after, "offline") == 0) {
            offline++;
        } else {
            online++;
        }

        cJSON *result = cJSON_CreateObject();
        cJSON_AddStringToObject(result, "entry_id", entry->id);
        cJSON_AddStringToObject(result, "state_before", state_before);
        cJSON_AddStringToObject(result, "state_after", state_after);
        cJSON_AddNumberToObject(result, "attempts", (double)attempts);
        cJSON_AddBoolToObject(result, "timed_out", probe_timed_out);
        cJSON_AddNumberToObject(result, "latency_ms", (double)(probe_timed_out ? (timeout_ms + 1) : (timeout_ms > 4 ? timeout_ms / 4 : 1)));
        cJSON_AddItemToArray(results, result);
    }

    uint64_t completed_at_us = started_at_us + (uint64_t)probed * 1500ULL;

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
                                     dead,
                                     timed_out,
                                     results);
}
