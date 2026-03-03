#include "esptari_web_catalog_sync.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "esptari_web_http_utils.h"

static const char *TAG = "esptari_web_catalog";

#define MAX_SYNC_SCHEDULES 32
#define MAX_SYNC_JOBS 64
#define MAX_RECOVERY_QUARANTINE 16

typedef struct {
    char job_id[32];
    char trigger[16];
    char schedule_id[32];
    char job_type[48];
    char mode[64];
    char status[24];
    uint64_t created_at_us;
    uint64_t started_at_us;
    uint64_t completed_at_us;
} esptari_sync_job_t;

typedef struct {
    char schedule_id[32];
    char job_type[48];
    char mode[64];
    char cron[48];
    bool enabled;
    bool catch_up;
    uint64_t created_at_us;
    uint64_t updated_at_us;
    uint64_t last_run_at_us;
    uint64_t next_run_at_us;
    char last_result[16];
    char last_error_code[48];
} esptari_sync_schedule_t;

typedef struct {
    char schedule_id[32];
    char error_code[48];
    char reason[64];
} esptari_recovery_quarantine_t;

typedef struct {
    char recovery_run_id[48];
    uint64_t scheduler_now_us;
    uint32_t loaded;
    uint32_t validated;
    uint32_t recomputed_next_run;
    uint32_t quarantined;
    esptari_recovery_quarantine_t quarantine[MAX_RECOVERY_QUARANTINE];
} esptari_recovery_report_t;

static esptari_sync_job_t s_jobs[MAX_SYNC_JOBS];
static size_t s_job_count;
static esptari_sync_schedule_t s_schedules[MAX_SYNC_SCHEDULES];
static size_t s_schedule_count;
static char s_persisted_schedule_snapshot[8192];
static char s_recovery_parse_buffer[8192];
static bool s_initialized;
static uint64_t s_job_seq;
static uint64_t s_schedule_seq;
static esptari_recovery_report_t s_recovery_report;

static uint64_t scheduler_now_us(void)
{
    return (uint64_t)esp_timer_get_time();
}

static esp_err_t send_json_object(httpd_req_t *req, cJSON *root, int status)
{
    char *json = cJSON_PrintUnformatted(root);
    if (json == NULL) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }
    esp_err_t out = esptari_web_send_json(req, json, status);
    cJSON_free(json);
    return out;
}

static esp_err_t send_error(httpd_req_t *req, const char *code, int status)
{
    char resp[320];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":false,\"error\":{\"code\":\"%s\",\"category\":\"scheduler\",\"retryable\":false}}",
             code);
    return esptari_web_send_json(req, resp, status);
}

static bool parse_json_request(httpd_req_t *req, cJSON **json)
{
    char body[768];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        send_error(req, "BAD_REQUEST", 400);
        return false;
    }

    *json = cJSON_Parse(body);
    if (*json == NULL) {
        send_error(req, "BAD_REQUEST", 400);
        return false;
    }

    return true;
}

static const char *wildcard_tail(const char *uri, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    if (strncmp(uri, prefix, prefix_len) == 0) {
        return uri + prefix_len;
    }
    return NULL;
}

static bool parse_interval_hours_from_cron(const char *cron, uint32_t *out_hours)
{
    if (cron == NULL || out_hours == NULL) {
        return false;
    }
    if (strcmp(cron, "0 * * * *") == 0) {
        *out_hours = 1;
        return true;
    }
    const char *prefix = "0 */";
    size_t prefix_len = strlen(prefix);
    if (strncmp(cron, prefix, prefix_len) != 0) {
        return false;
    }
    const char *tail = cron + prefix_len;
    char *endptr = NULL;
    unsigned long parsed = strtoul(tail, &endptr, 10);
    if (endptr == NULL || strcmp(endptr, " * * *") != 0 || parsed == 0 || parsed > 24) {
        return false;
    }
    *out_hours = (uint32_t)parsed;
    return true;
}

static uint64_t compute_next_run_at_us(uint64_t now_us, const char *cron)
{
    uint32_t interval_hours = 1;
    if (!parse_interval_hours_from_cron(cron, &interval_hours)) {
        return now_us + 3600000000ULL;
    }
    uint64_t interval_us = (uint64_t)interval_hours * 3600000000ULL;
    uint64_t bucket = now_us / interval_us;
    return (bucket + 1ULL) * interval_us;
}

static cJSON *schedule_to_json(const esptari_sync_schedule_t *schedule)
{
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "schedule_id", schedule->schedule_id);
    cJSON_AddStringToObject(item, "job_type", schedule->job_type);
    cJSON_AddStringToObject(item, "mode", schedule->mode);
    cJSON_AddStringToObject(item, "cron", schedule->cron);
    cJSON_AddBoolToObject(item, "enabled", schedule->enabled);
    cJSON_AddBoolToObject(item, "catch_up", schedule->catch_up);
    cJSON_AddNumberToObject(item, "created_at_us", (double)schedule->created_at_us);
    cJSON_AddNumberToObject(item, "updated_at_us", (double)schedule->updated_at_us);
    if (schedule->last_run_at_us == 0) {
        cJSON_AddNullToObject(item, "last_run_at_us");
    } else {
        cJSON_AddNumberToObject(item, "last_run_at_us", (double)schedule->last_run_at_us);
    }
    cJSON_AddNumberToObject(item, "next_run_at_us", (double)schedule->next_run_at_us);
    cJSON_AddStringToObject(item, "last_result", schedule->last_result[0] == '\0' ? "none" : schedule->last_result);
    if (schedule->last_error_code[0] == '\0') {
        cJSON_AddNullToObject(item, "last_error_code");
    } else {
        cJSON_AddStringToObject(item, "last_error_code", schedule->last_error_code);
    }
    return item;
}

static bool is_known_job_type(const char *job_type)
{
    return job_type != NULL &&
           (strcmp(job_type, "floppy_catalog_sync") == 0 || strcmp(job_type, "rom_catalog_sync") == 0 ||
            strcmp(job_type, "tos_catalog_sync") == 0);
}

static bool is_known_mode(const char *mode)
{
    return mode != NULL &&
           (strcmp(mode, "catalog_only") == 0 || strcmp(mode, "catalog_and_probe_links") == 0 ||
            strcmp(mode, "catalog_probe_and_prefetch_missing") == 0);
}

static void recovery_reset(uint64_t now_us)
{
    memset(&s_recovery_report, 0, sizeof(s_recovery_report));
    s_recovery_report.scheduler_now_us = now_us;
    snprintf(s_recovery_report.recovery_run_id,
             sizeof(s_recovery_report.recovery_run_id),
             "sched_recover_%06llu",
             (unsigned long long)(now_us % 1000000ULL));
}

static void recovery_quarantine_add(const char *schedule_id, const char *reason)
{
    s_recovery_report.quarantined++;
    if (s_recovery_report.quarantined > MAX_RECOVERY_QUARANTINE) {
        return;
    }
    esptari_recovery_quarantine_t *entry = &s_recovery_report.quarantine[s_recovery_report.quarantined - 1];
    snprintf(entry->schedule_id, sizeof(entry->schedule_id), "%s", schedule_id != NULL ? schedule_id : "unknown");
    snprintf(entry->error_code, sizeof(entry->error_code), "%s", "SCRAPER_SCHEDULE_INVALID");
    snprintf(entry->reason, sizeof(entry->reason), "%s", reason != NULL ? reason : "invalid record");
}

static bool duplicate_schedule_identity(const char *job_type, const char *mode, const char *cron, const char *exclude_id)
{
    for (size_t i = 0; i < s_schedule_count; i++) {
        const esptari_sync_schedule_t *schedule = &s_schedules[i];
        if (exclude_id != NULL && strcmp(schedule->schedule_id, exclude_id) == 0) {
            continue;
        }
        if (strcmp(schedule->job_type, job_type) == 0 && strcmp(schedule->mode, mode) == 0 && strcmp(schedule->cron, cron) == 0) {
            return true;
        }
    }
    return false;
}

static bool persist_schedules(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *schedules = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "schedules", schedules);

    for (size_t i = 0; i < s_schedule_count; i++) {
        cJSON_AddItemToArray(schedules, schedule_to_json(&s_schedules[i]));
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        return false;
    }

    size_t json_len = strlen(json);
    if (json_len >= sizeof(s_persisted_schedule_snapshot)) {
        cJSON_free(json);
        return false;
    }
    memcpy(s_persisted_schedule_snapshot, json, json_len + 1);
    cJSON_free(json);
    return true;
}

static void load_schedules_if_needed(void)
{
    if (s_initialized) {
        return;
    }
    s_initialized = true;

    uint64_t now_us = scheduler_now_us();
    recovery_reset(now_us);

    memset(s_recovery_parse_buffer, 0, sizeof(s_recovery_parse_buffer));
    if (s_persisted_schedule_snapshot[0] != '\0') {
        strlcpy(s_recovery_parse_buffer, s_persisted_schedule_snapshot, sizeof(s_recovery_parse_buffer));
    } else {
        return;
    }

    cJSON *root = cJSON_Parse(s_recovery_parse_buffer);
    if (root == NULL) {
        return;
    }

    cJSON *schedules = cJSON_GetObjectItemCaseSensitive(root, "schedules");
    if (!cJSON_IsArray(schedules)) {
        cJSON_Delete(root);
        return;
    }

    cJSON *schedule = NULL;
    cJSON_ArrayForEach(schedule, schedules)
    {
        s_recovery_report.loaded++;
        if (!cJSON_IsObject(schedule) || s_schedule_count >= MAX_SYNC_SCHEDULES) {
            recovery_quarantine_add("unknown", "record is not an object or schedule capacity exhausted");
            continue;
        }

        cJSON *schedule_id = cJSON_GetObjectItemCaseSensitive(schedule, "schedule_id");
        cJSON *job_type = cJSON_GetObjectItemCaseSensitive(schedule, "job_type");
        cJSON *mode = cJSON_GetObjectItemCaseSensitive(schedule, "mode");
        cJSON *cron = cJSON_GetObjectItemCaseSensitive(schedule, "cron");
        cJSON *enabled = cJSON_GetObjectItemCaseSensitive(schedule, "enabled");
        cJSON *catch_up = cJSON_GetObjectItemCaseSensitive(schedule, "catch_up");
        cJSON *created_at_us = cJSON_GetObjectItemCaseSensitive(schedule, "created_at_us");
        cJSON *updated_at_us = cJSON_GetObjectItemCaseSensitive(schedule, "updated_at_us");
        cJSON *last_run_at_us = cJSON_GetObjectItemCaseSensitive(schedule, "last_run_at_us");
        cJSON *next_run_at_us = cJSON_GetObjectItemCaseSensitive(schedule, "next_run_at_us");
        cJSON *last_result = cJSON_GetObjectItemCaseSensitive(schedule, "last_result");
        cJSON *last_error_code = cJSON_GetObjectItemCaseSensitive(schedule, "last_error_code");

        if (!cJSON_IsString(schedule_id) || !cJSON_IsString(job_type) || !cJSON_IsString(mode) || !cJSON_IsString(cron) ||
            !cJSON_IsBool(enabled) || !cJSON_IsBool(catch_up) || !cJSON_IsNumber(created_at_us) ||
            !cJSON_IsNumber(updated_at_us) || !cJSON_IsNumber(next_run_at_us) || !is_known_job_type(job_type->valuestring) ||
            !is_known_mode(mode->valuestring) || !parse_interval_hours_from_cron(cron->valuestring, &(uint32_t){0})) {
            recovery_quarantine_add(cJSON_IsString(schedule_id) ? schedule_id->valuestring : "unknown", "validation failed");
            continue;
        }

        esptari_sync_schedule_t *out = &s_schedules[s_schedule_count++];
        snprintf(out->schedule_id, sizeof(out->schedule_id), "%s", schedule_id->valuestring);
        snprintf(out->job_type, sizeof(out->job_type), "%s", job_type->valuestring);
        snprintf(out->mode, sizeof(out->mode), "%s", mode->valuestring);
        snprintf(out->cron, sizeof(out->cron), "%s", cron->valuestring);
        out->enabled = cJSON_IsTrue(enabled);
        out->catch_up = cJSON_IsTrue(catch_up);
        out->created_at_us = (uint64_t)created_at_us->valuedouble;
        out->updated_at_us = (uint64_t)updated_at_us->valuedouble;
        out->last_run_at_us = cJSON_IsNumber(last_run_at_us) ? (uint64_t)last_run_at_us->valuedouble : 0;
        out->next_run_at_us = (uint64_t)next_run_at_us->valuedouble;
        if (cJSON_IsString(last_result) && last_result->valuestring != NULL) {
            snprintf(out->last_result, sizeof(out->last_result), "%s", last_result->valuestring);
        } else {
            snprintf(out->last_result, sizeof(out->last_result), "%s", "none");
        }
        if (cJSON_IsString(last_error_code) && last_error_code->valuestring != NULL) {
            snprintf(out->last_error_code, sizeof(out->last_error_code), "%s", last_error_code->valuestring);
        } else {
            out->last_error_code[0] = '\0';
        }

        if (out->next_run_at_us < now_us) {
            if (out->catch_up) {
                out->next_run_at_us = now_us;
            } else {
                out->next_run_at_us = compute_next_run_at_us(now_us, out->cron);
            }
            s_recovery_report.recomputed_next_run++;
        }
        s_recovery_report.validated++;

        if (strncmp(out->schedule_id, "sch_", 4) == 0) {
            uint64_t parsed = strtoull(out->schedule_id + 4, NULL, 10);
            if (parsed > s_schedule_seq) {
                s_schedule_seq = parsed;
            }
        }
    }

    cJSON_Delete(root);

    if (s_recovery_report.recomputed_next_run > 0 || s_recovery_report.quarantined > 0) {
        persist_schedules();
    }
}

static void append_job(const char *trigger,
                       const char *schedule_id,
                       const char *job_type,
                       const char *mode,
                       const char *status,
                       uint64_t now_us)
{
    if (s_job_count >= MAX_SYNC_JOBS) {
        memmove(&s_jobs[0], &s_jobs[1], sizeof(s_jobs[0]) * (MAX_SYNC_JOBS - 1));
        s_job_count = MAX_SYNC_JOBS - 1;
    }

    esptari_sync_job_t *job = &s_jobs[s_job_count++];
    s_job_seq++;
    snprintf(job->job_id, sizeof(job->job_id), "job_%06llu", (unsigned long long)s_job_seq);
    snprintf(job->trigger, sizeof(job->trigger), "%s", trigger);
    snprintf(job->schedule_id, sizeof(job->schedule_id), "%s", schedule_id != NULL ? schedule_id : "");
    snprintf(job->job_type, sizeof(job->job_type), "%s", job_type);
    snprintf(job->mode, sizeof(job->mode), "%s", mode);
    snprintf(job->status, sizeof(job->status), "%s", status);
    job->created_at_us = now_us;
    job->started_at_us = now_us;
    job->completed_at_us = now_us;
}

static int due_sort_compare(const void *lhs_ptr, const void *rhs_ptr)
{
    const esptari_sync_schedule_t *lhs = *((const esptari_sync_schedule_t **)lhs_ptr);
    const esptari_sync_schedule_t *rhs = *((const esptari_sync_schedule_t **)rhs_ptr);
    if (lhs->next_run_at_us < rhs->next_run_at_us) {
        return -1;
    }
    if (lhs->next_run_at_us > rhs->next_run_at_us) {
        return 1;
    }
    return strcmp(lhs->schedule_id, rhs->schedule_id);
}

static void scheduler_tick(void)
{
    load_schedules_if_needed();

    uint64_t now_us = scheduler_now_us();
    esptari_sync_schedule_t *due[MAX_SYNC_SCHEDULES];
    size_t due_count = 0;

    for (size_t i = 0; i < s_schedule_count; i++) {
        if (!s_schedules[i].enabled) {
            continue;
        }
        if (s_schedules[i].next_run_at_us <= now_us) {
            due[due_count++] = &s_schedules[i];
        }
    }

    if (due_count == 0) {
        return;
    }

    qsort(due, due_count, sizeof(due[0]), due_sort_compare);

    bool mutated = false;
    uint32_t dispatch_budget = 1;
    for (size_t i = 0; i < due_count; i++) {
        esptari_sync_schedule_t *schedule = due[i];
        if (dispatch_budget == 0) {
            snprintf(schedule->last_result, sizeof(schedule->last_result), "%s", "skipped");
            snprintf(schedule->last_error_code, sizeof(schedule->last_error_code), "%s", "RUNTIME_SATURATED");
            schedule->updated_at_us = now_us;
            schedule->next_run_at_us = compute_next_run_at_us(now_us, schedule->cron);
            mutated = true;
            continue;
        }

        append_job("schedule", schedule->schedule_id, schedule->job_type, schedule->mode, "completed", now_us);
        schedule->last_run_at_us = now_us;
        schedule->updated_at_us = now_us;
        schedule->next_run_at_us = compute_next_run_at_us(now_us, schedule->cron);
        snprintf(schedule->last_result, sizeof(schedule->last_result), "%s", "success");
        schedule->last_error_code[0] = '\0';
        dispatch_budget--;
        mutated = true;
    }

    if (mutated) {
        persist_schedules();
    }
}

static esptari_sync_schedule_t *find_schedule_by_id(const char *schedule_id)
{
    for (size_t i = 0; i < s_schedule_count; i++) {
        if (strcmp(s_schedules[i].schedule_id, schedule_id) == 0) {
            return &s_schedules[i];
        }
    }
    return NULL;
}

static esp_err_t handle_catalog_sync_run(httpd_req_t *req)
{
    scheduler_tick();

    cJSON *json = NULL;
    if (!parse_json_request(req, &json)) {
        return ESP_OK;
    }

    cJSON *job_type = cJSON_GetObjectItemCaseSensitive(json, "job_type");
    cJSON *mode = cJSON_GetObjectItemCaseSensitive(json, "mode");
    if (!cJSON_IsString(job_type) || !cJSON_IsString(mode) || job_type->valuestring == NULL || mode->valuestring == NULL) {
        cJSON_Delete(json);
        return send_error(req, "BAD_REQUEST", 400);
    }

    uint64_t now_us = scheduler_now_us();
    append_job("manual", NULL, job_type->valuestring, mode->valuestring, "completed", now_us);
    cJSON_Delete(json);

    const esptari_sync_job_t *job = &s_jobs[s_job_count - 1];
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    cJSON_AddStringToObject(data, "job_id", job->job_id);
    cJSON_AddStringToObject(data, "trigger", job->trigger);
    cJSON_AddStringToObject(data, "status", "queued");
    cJSON_AddNumberToObject(data, "created_at_us", (double)job->created_at_us);

    esp_err_t out = send_json_object(req, resp, 202);
    cJSON_Delete(resp);
    return out;
}

static cJSON *job_to_json(const esptari_sync_job_t *job)
{
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "job_id", job->job_id);
    cJSON_AddStringToObject(item, "trigger", job->trigger);
    cJSON_AddStringToObject(item, "schedule_id", job->schedule_id[0] == '\0' ? "" : job->schedule_id);
    cJSON_AddStringToObject(item, "job_type", job->job_type);
    cJSON_AddStringToObject(item, "mode", job->mode);
    cJSON_AddStringToObject(item, "status", job->status);
    cJSON_AddNumberToObject(item, "created_at_us", (double)job->created_at_us);
    cJSON_AddNumberToObject(item, "started_at_us", (double)job->started_at_us);
    cJSON_AddNumberToObject(item, "completed_at_us", (double)job->completed_at_us);
    return item;
}

static esp_err_t handle_catalog_sync_jobs(httpd_req_t *req)
{
    scheduler_tick();

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    cJSON *jobs = cJSON_CreateArray();
    cJSON_AddItemToObject(data, "jobs", jobs);

    for (size_t i = 0; i < s_job_count; i++) {
        cJSON_AddItemToArray(jobs, job_to_json(&s_jobs[i]));
    }

    esp_err_t out = send_json_object(req, resp, 200);
    cJSON_Delete(resp);
    return out;
}

static esp_err_t handle_catalog_sync_job_by_id(httpd_req_t *req)
{
    scheduler_tick();

    const char *job_id = wildcard_tail(req->uri, "/api/v2/catalog-sync/jobs/");
    if (job_id == NULL || job_id[0] == '\0') {
        return send_error(req, "BAD_REQUEST", 400);
    }

    const esptari_sync_job_t *job = NULL;
    for (size_t i = 0; i < s_job_count; i++) {
        if (strcmp(s_jobs[i].job_id, job_id) == 0) {
            job = &s_jobs[i];
            break;
        }
    }
    if (job == NULL) {
        return send_error(req, "SCRAPER_JOB_NOT_FOUND", 404);
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = job_to_json(job);
    cJSON_AddItemToObject(resp, "data", data);

    esp_err_t out = send_json_object(req, resp, 200);
    cJSON_Delete(resp);
    return out;
}

static esp_err_t handle_catalog_sync_create_schedule(httpd_req_t *req)
{
    scheduler_tick();
    load_schedules_if_needed();

    cJSON *json = NULL;
    if (!parse_json_request(req, &json)) {
        return ESP_OK;
    }

    cJSON *job_type = cJSON_GetObjectItemCaseSensitive(json, "job_type");
    cJSON *mode = cJSON_GetObjectItemCaseSensitive(json, "mode");
    cJSON *cron = cJSON_GetObjectItemCaseSensitive(json, "cron");
    cJSON *enabled = cJSON_GetObjectItemCaseSensitive(json, "enabled");
    cJSON *catch_up = cJSON_GetObjectItemCaseSensitive(json, "catch_up");

    uint32_t interval_hours = 0;
    if (!cJSON_IsString(job_type) || !cJSON_IsString(mode) || !cJSON_IsString(cron) || !cJSON_IsBool(enabled) ||
        !cJSON_IsBool(catch_up) || !parse_interval_hours_from_cron(cron->valuestring, &interval_hours) ||
        !is_known_job_type(job_type->valuestring) || !is_known_mode(mode->valuestring)) {
        cJSON_Delete(json);
        return send_error(req, "SCRAPER_SCHEDULE_INVALID", 400);
    }
    if (duplicate_schedule_identity(job_type->valuestring, mode->valuestring, cron->valuestring, NULL)) {
        cJSON_Delete(json);
        return send_error(req, "CONFLICT", 409);
    }
    if (s_schedule_count >= MAX_SYNC_SCHEDULES) {
        cJSON_Delete(json);
        return send_error(req, "CONFLICT", 409);
    }

    uint64_t now_us = scheduler_now_us();
    esptari_sync_schedule_t *schedule = &s_schedules[s_schedule_count++];
    s_schedule_seq++;
    snprintf(schedule->schedule_id, sizeof(schedule->schedule_id), "sch_%06llu", (unsigned long long)s_schedule_seq);
    snprintf(schedule->job_type, sizeof(schedule->job_type), "%s", job_type->valuestring);
    snprintf(schedule->mode, sizeof(schedule->mode), "%s", mode->valuestring);
    snprintf(schedule->cron, sizeof(schedule->cron), "%s", cron->valuestring);
    schedule->enabled = cJSON_IsTrue(enabled);
    schedule->catch_up = cJSON_IsTrue(catch_up);
    schedule->created_at_us = now_us;
    schedule->updated_at_us = now_us;
    schedule->last_run_at_us = 0;
    schedule->next_run_at_us = compute_next_run_at_us(now_us, schedule->cron);
    snprintf(schedule->last_result, sizeof(schedule->last_result), "%s", "none");
    schedule->last_error_code[0] = '\0';

    cJSON_Delete(json);

    if (!persist_schedules()) {
        s_schedule_count--;
        return send_error(req, "CATALOG_SYNC_FAILED", 409);
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = schedule_to_json(schedule);
    cJSON_AddItemToObject(resp, "data", data);

    esp_err_t out = send_json_object(req, resp, 201);
    cJSON_Delete(resp);
    return out;
}

static int schedule_list_compare(const void *lhs_ptr, const void *rhs_ptr)
{
    const esptari_sync_schedule_t *lhs = *((const esptari_sync_schedule_t **)lhs_ptr);
    const esptari_sync_schedule_t *rhs = *((const esptari_sync_schedule_t **)rhs_ptr);
    if (lhs->next_run_at_us < rhs->next_run_at_us) {
        return -1;
    }
    if (lhs->next_run_at_us > rhs->next_run_at_us) {
        return 1;
    }
    return strcmp(lhs->schedule_id, rhs->schedule_id);
}

static esp_err_t handle_catalog_sync_list_schedules(httpd_req_t *req)
{
    scheduler_tick();
    load_schedules_if_needed();

    esptari_sync_schedule_t *ordered[MAX_SYNC_SCHEDULES];
    for (size_t i = 0; i < s_schedule_count; i++) {
        ordered[i] = &s_schedules[i];
    }
    qsort(ordered, s_schedule_count, sizeof(ordered[0]), schedule_list_compare);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    cJSON_AddNumberToObject(data, "scheduler_now_us", (double)scheduler_now_us());
    cJSON *schedules = cJSON_CreateArray();
    cJSON_AddItemToObject(data, "schedules", schedules);

    for (size_t i = 0; i < s_schedule_count; i++) {
        cJSON_AddItemToArray(schedules, schedule_to_json(ordered[i]));
    }

    esp_err_t out = send_json_object(req, resp, 200);
    cJSON_Delete(resp);
    return out;
}

static esp_err_t handle_catalog_sync_get_schedule(httpd_req_t *req)
{
    scheduler_tick();
    load_schedules_if_needed();

    const char *schedule_id = wildcard_tail(req->uri, "/api/v2/catalog-sync/schedules/");
    if (schedule_id == NULL || schedule_id[0] == '\0') {
        return send_error(req, "BAD_REQUEST", 400);
    }

    esptari_sync_schedule_t *schedule = find_schedule_by_id(schedule_id);
    if (schedule == NULL) {
        return send_error(req, "SCRAPER_JOB_NOT_FOUND", 404);
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    cJSON_AddNumberToObject(data, "scheduler_now_us", (double)scheduler_now_us());
    cJSON_AddItemToObject(data, "schedule", schedule_to_json(schedule));

    esp_err_t out = send_json_object(req, resp, 200);
    cJSON_Delete(resp);
    return out;
}

static esp_err_t handle_catalog_sync_delete_schedule(httpd_req_t *req)
{
    load_schedules_if_needed();

    const char *schedule_id = wildcard_tail(req->uri, "/api/v2/catalog-sync/schedules/");
    if (schedule_id == NULL || schedule_id[0] == '\0') {
        return send_error(req, "BAD_REQUEST", 400);
    }

    for (size_t i = 0; i < s_schedule_count; i++) {
        if (strcmp(s_schedules[i].schedule_id, schedule_id) == 0) {
            if (i + 1 < s_schedule_count) {
                memmove(&s_schedules[i], &s_schedules[i + 1], sizeof(s_schedules[0]) * (s_schedule_count - i - 1));
            }
            s_schedule_count--;
            if (!persist_schedules()) {
                return send_error(req, "CATALOG_SYNC_FAILED", 409);
            }

            cJSON *resp = cJSON_CreateObject();
            cJSON_AddBoolToObject(resp, "ok", true);
            cJSON *data = cJSON_CreateObject();
            cJSON_AddItemToObject(resp, "data", data);
            cJSON_AddStringToObject(data, "schedule_id", schedule_id);
            cJSON_AddStringToObject(data, "status", "deleted");

            esp_err_t out = send_json_object(req, resp, 200);
            cJSON_Delete(resp);
            return out;
        }
    }

    return send_error(req, "SCRAPER_JOB_NOT_FOUND", 404);
}

static esp_err_t handle_catalog_sync_patch_schedule(httpd_req_t *req)
{
    scheduler_tick();
    load_schedules_if_needed();

    const char *schedule_id = wildcard_tail(req->uri, "/api/v2/catalog-sync/schedules/");
    if (schedule_id == NULL || schedule_id[0] == '\0') {
        return send_error(req, "BAD_REQUEST", 400);
    }

    esptari_sync_schedule_t *schedule = find_schedule_by_id(schedule_id);
    if (schedule == NULL) {
        return send_error(req, "SCRAPER_JOB_NOT_FOUND", 404);
    }

    cJSON *json = NULL;
    if (!parse_json_request(req, &json)) {
        return ESP_OK;
    }

    cJSON *job_type = cJSON_GetObjectItemCaseSensitive(json, "job_type");
    cJSON *mode = cJSON_GetObjectItemCaseSensitive(json, "mode");
    cJSON *cron = cJSON_GetObjectItemCaseSensitive(json, "cron");
    cJSON *enabled = cJSON_GetObjectItemCaseSensitive(json, "enabled");
    cJSON *catch_up = cJSON_GetObjectItemCaseSensitive(json, "catch_up");

    if ((job_type != NULL && (!cJSON_IsString(job_type) || !is_known_job_type(job_type->valuestring))) ||
        (mode != NULL && (!cJSON_IsString(mode) || !is_known_mode(mode->valuestring))) ||
        (cron != NULL && (!cJSON_IsString(cron) || !parse_interval_hours_from_cron(cron->valuestring, &(uint32_t){0}))) ||
        (enabled != NULL && !cJSON_IsBool(enabled)) || (catch_up != NULL && !cJSON_IsBool(catch_up))) {
        cJSON_Delete(json);
        return send_error(req, "SCRAPER_SCHEDULE_INVALID", 400);
    }

    esptari_sync_schedule_t updated = *schedule;
    bool has_change = false;
    if (job_type != NULL) {
        snprintf(updated.job_type, sizeof(updated.job_type), "%s", job_type->valuestring);
        has_change = true;
    }
    if (mode != NULL) {
        snprintf(updated.mode, sizeof(updated.mode), "%s", mode->valuestring);
        has_change = true;
    }
    bool cron_changed = false;
    if (cron != NULL) {
        snprintf(updated.cron, sizeof(updated.cron), "%s", cron->valuestring);
        has_change = true;
        cron_changed = true;
    }
    bool was_enabled = updated.enabled;
    if (enabled != NULL) {
        updated.enabled = cJSON_IsTrue(enabled);
        has_change = true;
    }
    if (catch_up != NULL) {
        updated.catch_up = cJSON_IsTrue(catch_up);
        has_change = true;
    }
    cJSON_Delete(json);

    if (duplicate_schedule_identity(updated.job_type, updated.mode, updated.cron, schedule->schedule_id)) {
        return send_error(req, "CONFLICT", 409);
    }

    uint64_t now_us = scheduler_now_us();
    if (has_change) {
        if (cron_changed || (was_enabled == false && updated.enabled)) {
            updated.next_run_at_us = compute_next_run_at_us(now_us, updated.cron);
        }
        if (updated.updated_at_us >= now_us) {
            now_us = updated.updated_at_us + 1;
        }
        updated.updated_at_us = now_us;
    }

    esptari_sync_schedule_t original = *schedule;
    *schedule = updated;
    if (has_change && !persist_schedules()) {
        *schedule = original;
        return send_error(req, "CATALOG_SYNC_FAILED", 409);
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = schedule_to_json(schedule);
    cJSON_AddItemToObject(resp, "data", data);

    esp_err_t out = send_json_object(req, resp, 200);
    cJSON_Delete(resp);
    return out;
}

static esp_err_t handle_catalog_sync_recovery_report(httpd_req_t *req)
{
    load_schedules_if_needed();

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    cJSON_AddStringToObject(data, "recovery_run_id", s_recovery_report.recovery_run_id);
    cJSON_AddNumberToObject(data, "scheduler_now_us", (double)s_recovery_report.scheduler_now_us);
    cJSON_AddNumberToObject(data, "loaded", s_recovery_report.loaded);
    cJSON_AddNumberToObject(data, "validated", s_recovery_report.validated);
    cJSON_AddNumberToObject(data, "recomputed_next_run", s_recovery_report.recomputed_next_run);
    cJSON_AddNumberToObject(data, "quarantined", s_recovery_report.quarantined);
    cJSON *quarantine = cJSON_CreateArray();
    cJSON_AddItemToObject(data, "quarantine", quarantine);
    uint32_t quarantine_count = s_recovery_report.quarantined;
    if (quarantine_count > MAX_RECOVERY_QUARANTINE) {
        quarantine_count = MAX_RECOVERY_QUARANTINE;
    }
    for (uint32_t i = 0; i < quarantine_count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "schedule_id", s_recovery_report.quarantine[i].schedule_id);
        cJSON_AddStringToObject(item, "error_code", s_recovery_report.quarantine[i].error_code);
        cJSON_AddStringToObject(item, "reason", s_recovery_report.quarantine[i].reason);
        cJSON_AddItemToArray(quarantine, item);
    }

    esp_err_t out = send_json_object(req, resp, 200);
    cJSON_Delete(resp);
    return out;
}

static esp_err_t handle_ebins_catalog(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *items = cJSON_AddArrayToObject(root, "items");

    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "name", "cpu_core.ebin");
    cJSON_AddStringToObject(item, "version", "1.0.0");
    cJSON_AddBoolToObject(item, "loaded", true);
    cJSON_AddItemToArray(items, item);

    esp_err_t out = send_json_object(req, root, 200);
    cJSON_Delete(root);
    return out;
}

static esp_err_t handle_ebins_rescan(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "queued");
    cJSON_AddNumberToObject(root, "queuedAtMs", (double)(scheduler_now_us() / 1000ULL));
    esp_err_t out = send_json_object(req, root, 202);
    cJSON_Delete(root);
    return out;
}

static esp_err_t handle_ebins_validate(httpd_req_t *req)
{
    cJSON *json = NULL;
    if (!parse_json_request(req, &json)) {
        return ESP_OK;
    }

    const cJSON *name = cJSON_GetObjectItemCaseSensitive(json, "name");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", cJSON_IsString(name) ? name->valuestring : "unknown");
    cJSON_AddBoolToObject(root, "valid", true);
    cJSON_AddStringToObject(root, "status", "ok");

    cJSON_Delete(json);
    esp_err_t out = send_json_object(req, root, 200);
    cJSON_Delete(root);
    return out;
}

static esp_err_t handle_ebins_load(httpd_req_t *req)
{
    cJSON *json = NULL;
    if (!parse_json_request(req, &json)) {
        return ESP_OK;
    }

    const cJSON *name = cJSON_GetObjectItemCaseSensitive(json, "name");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", cJSON_IsString(name) ? name->valuestring : "unknown");
    cJSON_AddStringToObject(root, "status", "loaded");

    cJSON_Delete(json);
    esp_err_t out = send_json_object(req, root, 200);
    cJSON_Delete(root);
    return out;
}

static esp_err_t handle_ebins_unload(httpd_req_t *req)
{
    cJSON *json = NULL;
    if (!parse_json_request(req, &json)) {
        return ESP_OK;
    }

    const cJSON *name = cJSON_GetObjectItemCaseSensitive(json, "name");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", cJSON_IsString(name) ? name->valuestring : "unknown");
    cJSON_AddStringToObject(root, "status", "unloaded");

    cJSON_Delete(json);
    esp_err_t out = send_json_object(req, root, 200);
    cJSON_Delete(root);
    return out;
}

void esptari_web_catalog_sync_register_routes(httpd_handle_t server_handle)
{
    httpd_uri_t sync_jobs_run = {.uri = "/api/v2/catalog-sync/jobs/run", .method = HTTP_POST, .handler = handle_catalog_sync_run, .user_ctx = NULL};
    httpd_uri_t sync_jobs_list = {.uri = "/api/v2/catalog-sync/jobs", .method = HTTP_GET, .handler = handle_catalog_sync_jobs, .user_ctx = NULL};
    httpd_uri_t sync_job_get = {.uri = "/api/v2/catalog-sync/jobs/*", .method = HTTP_GET, .handler = handle_catalog_sync_job_by_id, .user_ctx = NULL};
    httpd_uri_t sync_schedule_create = {.uri = "/api/v2/catalog-sync/schedules", .method = HTTP_POST, .handler = handle_catalog_sync_create_schedule, .user_ctx = NULL};
    httpd_uri_t sync_schedule_list = {.uri = "/api/v2/catalog-sync/schedules", .method = HTTP_GET, .handler = handle_catalog_sync_list_schedules, .user_ctx = NULL};
    httpd_uri_t sync_schedule_get = {.uri = "/api/v2/catalog-sync/schedules/*", .method = HTTP_GET, .handler = handle_catalog_sync_get_schedule, .user_ctx = NULL};
    httpd_uri_t sync_schedule_patch = {.uri = "/api/v2/catalog-sync/schedules/*", .method = HTTP_PATCH, .handler = handle_catalog_sync_patch_schedule, .user_ctx = NULL};
    httpd_uri_t sync_schedule_delete = {.uri = "/api/v2/catalog-sync/schedules/*", .method = HTTP_DELETE, .handler = handle_catalog_sync_delete_schedule, .user_ctx = NULL};
    httpd_uri_t sync_recovery_report = {.uri = "/api/v2/catalog-sync/recovery", .method = HTTP_GET, .handler = handle_catalog_sync_recovery_report, .user_ctx = NULL};
    httpd_uri_t ebins_catalog = {.uri = "/api/v2/ebins/catalog", .method = HTTP_GET, .handler = handle_ebins_catalog, .user_ctx = NULL};
    httpd_uri_t ebins_rescan = {.uri = "/api/v2/ebins/rescan", .method = HTTP_POST, .handler = handle_ebins_rescan, .user_ctx = NULL};
    httpd_uri_t ebins_validate = {.uri = "/api/v2/ebins/validate", .method = HTTP_POST, .handler = handle_ebins_validate, .user_ctx = NULL};
    httpd_uri_t ebins_load = {.uri = "/api/v2/ebins/load", .method = HTTP_POST, .handler = handle_ebins_load, .user_ctx = NULL};
    httpd_uri_t ebins_unload = {.uri = "/api/v2/ebins/unload", .method = HTTP_POST, .handler = handle_ebins_unload, .user_ctx = NULL};

    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &sync_jobs_run));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &sync_jobs_list));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &sync_job_get));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &sync_schedule_create));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &sync_schedule_list));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &sync_schedule_get));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &sync_schedule_patch));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &sync_schedule_delete));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &sync_recovery_report));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &ebins_catalog));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &ebins_rescan));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &ebins_validate));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &ebins_load));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &ebins_unload));

    ESP_LOGI(TAG, "Registered catalog sync and ebin routes");
}
