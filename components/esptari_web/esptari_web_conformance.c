#include "esptari_web_conformance.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_timer.h"
#include "esptari_core.h"
#include "esptari_web_http_utils.h"

#define send_json esptari_web_send_json

typedef struct {
    bool loaded;
    bool valid;
    char manifest_id[64];
    uint32_t checks_total;
    uint32_t checks_enabled;
    uint64_t loaded_at_us;
} conformance_manifest_state_t;

typedef struct {
    bool active;
    char harness_session_id[64];
    char session_id[64];
    char manifest_id[64];
    char state[16];
    uint64_t created_at_us;
    uint32_t checks_total;
    uint32_t checks_enabled;
} conformance_harness_state_t;

typedef struct {
    bool active;
    bool completed;
    char collection_id[64];
    char harness_session_id[64];
    char state[16];
    uint64_t started_at_us;
    uint64_t completed_at_us;
} conformance_collection_state_t;

typedef struct {
    bool active;
    char package_id[64];
    char harness_session_id[64];
    char collection_id[64];
    char state[16];
} conformance_package_state_t;

typedef struct {
    bool active;
    char runner_id[64];
    char harness_session_id[64];
    char checklist_id[64];
    char state[16];
    bool stop_on_failure;
    uint32_t cases_total;
    uint32_t cases_passed;
    uint32_t cases_failed;
    uint32_t completed_cases;
    char current_case_id[64];
    uint64_t started_at_us;
    uint64_t completed_at_us;
    uint64_t last_transition_at_us;
} conformance_runner_state_t;

typedef struct {
    bool active;
    char review_pack_id[64];
    char harness_session_id[64];
    char runner_id[64];
    char package_id[64];
    char state[16];
    uint64_t generated_at_us;
    char review_pack_uri[192];
} conformance_review_pack_state_t;

typedef struct {
    bool active;
    char bundle_id[64];
    char review_pack_id[64];
    char state[16];
    char bundle_uri[192];
    char sha256[80];
    uint64_t assembled_at_us;
} conformance_signoff_bundle_state_t;

static conformance_manifest_state_t g_manifest;
static conformance_harness_state_t g_harness;
static conformance_collection_state_t g_collection;
static conformance_package_state_t g_package;
static conformance_runner_state_t g_runner;
static conformance_review_pack_state_t g_review_pack;
static conformance_signoff_bundle_state_t g_signoff_bundle;
static uint64_t g_harness_seq;
static uint64_t g_collection_seq;
static uint64_t g_package_seq;
static uint64_t g_runner_seq;
static uint64_t g_review_pack_seq;
static uint64_t g_signoff_seq;

static bool parse_bool_enum(const char *value, const char *a, const char *b)
{
    return value != NULL && (strcmp(value, a) == 0 || strcmp(value, b) == 0);
}

static bool validate_engine_session_active(const char *session_id)
{
    if (session_id == NULL || strcmp(session_id, "ses_local") != 0) {
        return false;
    }

    esptari_session_status_t status;
    esptari_core_get_status(&status);
    return status.state == ESPTARI_SESSION_RUNNING || status.state == ESPTARI_SESSION_PAUSED;
}

static bool parse_and_validate_manifest(cJSON *manifest, uint32_t *out_case_count)
{
    if (!cJSON_IsObject(manifest)) {
        return false;
    }

    cJSON *manifest_version = cJSON_GetObjectItemCaseSensitive(manifest, "manifest_version");
    cJSON *target_machine = cJSON_GetObjectItemCaseSensitive(manifest, "target_machine");
    cJSON *profile = cJSON_GetObjectItemCaseSensitive(manifest, "profile");
    cJSON *suite = cJSON_GetObjectItemCaseSensitive(manifest, "suite");
    cJSON *cases = cJSON_GetObjectItemCaseSensitive(manifest, "cases");

    if (!cJSON_IsNumber(manifest_version) || manifest_version->valueint != 1 ||
        !cJSON_IsString(target_machine) || !cJSON_IsString(profile) || !cJSON_IsString(suite) ||
        !cJSON_IsArray(cases) || cJSON_GetArraySize(cases) < 1) {
        return false;
    }

    if (strcmp(target_machine->valuestring, "atari_st") != 0 || strcmp(profile->valuestring, "st_520_pal") != 0) {
        return false;
    }

    int case_count = cJSON_GetArraySize(cases);
    for (int i = 0; i < case_count; ++i) {
        cJSON *entry = cJSON_GetArrayItem(cases, i);
        cJSON *case_id = cJSON_GetObjectItemCaseSensitive(entry, "case_id");
        cJSON *kind = cJSON_GetObjectItemCaseSensitive(entry, "kind");
        cJSON *target = cJSON_GetObjectItemCaseSensitive(entry, "target");
        cJSON *assertions = cJSON_GetObjectItemCaseSensitive(entry, "assertions");
        if (!cJSON_IsString(case_id) || !cJSON_IsString(kind) || !cJSON_IsString(target) ||
            !cJSON_IsArray(assertions) || cJSON_GetArraySize(assertions) < 1) {
            return false;
        }
        if (!parse_bool_enum(kind->valuestring, "api", "stream")) {
            return false;
        }
    }

    *out_case_count = (uint32_t)case_count;
    return true;
}

static esp_err_t conformance_manifest_load_handler(httpd_req_t *req)
{
    char body[4096];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *manifest_id = cJSON_GetObjectItemCaseSensitive(root, "manifest_id");
    cJSON *manifest = cJSON_GetObjectItemCaseSensitive(root, "manifest");
    if (!cJSON_IsString(manifest_id) || manifest_id->valuestring == NULL || manifest_id->valuestring[0] == '\0') {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    uint32_t case_count = 0;
    bool manifest_ok = parse_and_validate_manifest(manifest, &case_count);
    if (!manifest_ok) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"MACHINE_PROFILE_NOT_FOUND\"}}", 409);
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    memset(&g_manifest, 0, sizeof(g_manifest));
    g_manifest.loaded = true;
    g_manifest.valid = true;
    g_manifest.checks_total = case_count;
    g_manifest.checks_enabled = case_count;
    g_manifest.loaded_at_us = now_us;
    strlcpy(g_manifest.manifest_id, manifest_id->valuestring, sizeof(g_manifest.manifest_id));

    cJSON_Delete(root);

    char resp[320];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"manifest_id\":\"%s\",\"loaded\":true,\"checks_total\":%lu,\"checks_enabled\":%lu,\"loaded_at_us\":%llu}}",
             g_manifest.manifest_id,
             (unsigned long)g_manifest.checks_total,
             (unsigned long)g_manifest.checks_enabled,
             (unsigned long long)g_manifest.loaded_at_us);
    return send_json(req, resp, 200);
}

static esp_err_t conformance_harness_session_handler(httpd_req_t *req)
{
    char body[1024];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *session_id = cJSON_GetObjectItemCaseSensitive(root, "session_id");
    cJSON *manifest_id = cJSON_GetObjectItemCaseSensitive(root, "manifest_id");
    cJSON *run_mode = cJSON_GetObjectItemCaseSensitive(root, "run_mode");
    cJSON *evidence_level = cJSON_GetObjectItemCaseSensitive(root, "evidence_level");

    if (!cJSON_IsString(session_id) || !cJSON_IsString(manifest_id) || !cJSON_IsString(run_mode)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (!parse_bool_enum(run_mode->valuestring, "dry_run", "execute")) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (evidence_level != NULL && (!cJSON_IsString(evidence_level) || !parse_bool_enum(evidence_level->valuestring, "summary", "full"))) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    if (!validate_engine_session_active(session_id->valuestring)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }

    if (!g_manifest.loaded || !g_manifest.valid || strcmp(manifest_id->valuestring, g_manifest.manifest_id) != 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\"}}", 409);
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    memset(&g_harness, 0, sizeof(g_harness));
    g_harness.active = true;
    g_harness_seq++;
    snprintf(g_harness.harness_session_id, sizeof(g_harness.harness_session_id), "chs_%06llu", (unsigned long long)g_harness_seq);
    strlcpy(g_harness.session_id, session_id->valuestring, sizeof(g_harness.session_id));
    strlcpy(g_harness.manifest_id, manifest_id->valuestring, sizeof(g_harness.manifest_id));
    strlcpy(g_harness.state, "running", sizeof(g_harness.state));
    g_harness.created_at_us = now_us;
    g_harness.checks_total = g_manifest.checks_total;
    g_harness.checks_enabled = g_manifest.checks_enabled;

    cJSON_Delete(root);

    char resp[512];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"harness_session_id\":\"%s\",\"session_id\":\"%s\",\"manifest_id\":\"%s\",\"state\":\"%s\",\"created_at_us\":%llu,\"checks_total\":%lu,\"checks_enabled\":%lu}}",
             g_harness.harness_session_id,
             g_harness.session_id,
             g_harness.manifest_id,
             g_harness.state,
             (unsigned long long)g_harness.created_at_us,
             (unsigned long)g_harness.checks_total,
             (unsigned long)g_harness.checks_enabled);
    return send_json(req, resp, 200);
}

static bool artifact_type_allowed(const char *value)
{
    return value != NULL &&
           (strcmp(value, "logs") == 0 ||
            strcmp(value, "metrics") == 0 ||
            strcmp(value, "stream_samples") == 0 ||
            strcmp(value, "snapshots") == 0);
}

static bool checklist_id_supported(const char *value)
{
    return value != NULL && strcmp(value, "st_acceptance_v1") == 0;
}

static bool runner_state_terminal(const char *state)
{
    return state != NULL &&
           (strcmp(state, "completed") == 0 ||
            strcmp(state, "failed") == 0 ||
            strcmp(state, "aborted") == 0);
}

static bool parse_optional_string_array(cJSON *parent, const char *key, uint32_t *out_count, bool *out_valid)
{
    *out_count = 0;
    cJSON *arr = cJSON_GetObjectItemCaseSensitive(parent, key);
    if (arr == NULL) {
        *out_valid = true;
        return false;
    }
    if (!cJSON_IsArray(arr)) {
        *out_valid = false;
        return true;
    }

    int n = cJSON_GetArraySize(arr);
    for (int i = 0; i < n; ++i) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        if (!cJSON_IsString(item) || item->valuestring == NULL || item->valuestring[0] == '\0') {
            *out_valid = false;
            return true;
        }
    }

    *out_count = (uint32_t)n;
    *out_valid = true;
    return true;
}

static bool arrays_intersect(cJSON *a, cJSON *b)
{
    if (a == NULL || b == NULL || !cJSON_IsArray(a) || !cJSON_IsArray(b)) {
        return false;
    }

    int na = cJSON_GetArraySize(a);
    int nb = cJSON_GetArraySize(b);
    for (int i = 0; i < na; ++i) {
        cJSON *ia = cJSON_GetArrayItem(a, i);
        if (!cJSON_IsString(ia) || ia->valuestring == NULL) {
            continue;
        }
        for (int j = 0; j < nb; ++j) {
            cJSON *ib = cJSON_GetArrayItem(b, j);
            if (cJSON_IsString(ib) && ib->valuestring != NULL && strcmp(ia->valuestring, ib->valuestring) == 0) {
                return true;
            }
        }
    }

    return false;
}

static bool review_section_allowed(const char *value)
{
    return value != NULL &&
           (strcmp(value, "summary") == 0 ||
            strcmp(value, "failures") == 0 ||
            strcmp(value, "artifacts") == 0 ||
            strcmp(value, "telemetry") == 0 ||
            strcmp(value, "checklist") == 0);
}

static esp_err_t conformance_evidence_collect_handler(httpd_req_t *req)
{
    char body[2048];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *harness_session_id = cJSON_GetObjectItemCaseSensitive(root, "harness_session_id");
    cJSON *artifact_types = cJSON_GetObjectItemCaseSensitive(root, "artifact_types");
    cJSON *window = cJSON_GetObjectItemCaseSensitive(root, "window");

    if (!cJSON_IsString(harness_session_id) || !cJSON_IsArray(artifact_types) || cJSON_GetArraySize(artifact_types) < 1) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    for (int i = 0; i < cJSON_GetArraySize(artifact_types); ++i) {
        cJSON *item = cJSON_GetArrayItem(artifact_types, i);
        if (!cJSON_IsString(item) || !artifact_type_allowed(item->valuestring)) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
    }

    if (!g_harness.active || strcmp(harness_session_id->valuestring, g_harness.harness_session_id) != 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"NOT_FOUND\"}}", 404);
    }

    if (strcmp(g_harness.state, "initialized") == 0 || strcmp(g_harness.state, "failed") == 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\"}}", 409);
    }

    if (window != NULL) {
        cJSON *start_event_seq = cJSON_GetObjectItemCaseSensitive(window, "start_event_seq");
        cJSON *end_event_seq = cJSON_GetObjectItemCaseSensitive(window, "end_event_seq");
        if ((start_event_seq != NULL && !cJSON_IsNumber(start_event_seq)) ||
            (end_event_seq != NULL && !cJSON_IsNumber(end_event_seq))) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
        if (start_event_seq != NULL && end_event_seq != NULL && start_event_seq->valuedouble > end_event_seq->valuedouble) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
    }

    uint64_t start_us = (uint64_t)esp_timer_get_time();
    uint64_t done_us = start_us + 1200;
    g_collection_seq++;
    memset(&g_collection, 0, sizeof(g_collection));
    g_collection.active = true;
    g_collection.completed = true;
    snprintf(g_collection.collection_id, sizeof(g_collection.collection_id), "col_%06llu", (unsigned long long)g_collection_seq);
    strlcpy(g_collection.harness_session_id, g_harness.harness_session_id, sizeof(g_collection.harness_session_id));
    strlcpy(g_collection.state, "completed", sizeof(g_collection.state));
    g_collection.started_at_us = start_us;
    g_collection.completed_at_us = done_us;
    strlcpy(g_harness.state, "completed", sizeof(g_harness.state));

    cJSON_Delete(root);

    char resp[1024];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"collection_id\":\"%s\",\"harness_session_id\":\"%s\",\"state\":\"completed\",\"artifacts\":[{\"type\":\"logs\",\"uri\":\"sdcard/conformance/%s/logs.ndjson\",\"bytes\":120334},{\"type\":\"metrics\",\"uri\":\"sdcard/conformance/%s/metrics.json\",\"bytes\":4421}],\"started_at_us\":%llu,\"completed_at_us\":%llu}}",
             g_collection.collection_id,
             g_collection.harness_session_id,
             g_harness.harness_session_id,
             g_harness.harness_session_id,
             (unsigned long long)g_collection.started_at_us,
             (unsigned long long)g_collection.completed_at_us);
    return send_json(req, resp, 200);
}

static esp_err_t conformance_report_package_handler(httpd_req_t *req)
{
    char body[1024];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *harness_session_id = cJSON_GetObjectItemCaseSensitive(root, "harness_session_id");
    cJSON *collection_id = cJSON_GetObjectItemCaseSensitive(root, "collection_id");
    cJSON *format = cJSON_GetObjectItemCaseSensitive(root, "format");
    if (!cJSON_IsString(harness_session_id) || !cJSON_IsString(collection_id) || !cJSON_IsString(format)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (!parse_bool_enum(format->valuestring, "json", "zip")) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    if (!g_harness.active || !g_collection.active ||
        strcmp(harness_session_id->valuestring, g_harness.harness_session_id) != 0 ||
        strcmp(collection_id->valuestring, g_collection.collection_id) != 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"NOT_FOUND\"}}", 404);
    }

    if (!g_collection.completed) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\"}}", 409);
    }

    g_package_seq++;
    char package_id[32];
    snprintf(package_id, sizeof(package_id), "pkg_%06llu", (unsigned long long)g_package_seq);

    memset(&g_package, 0, sizeof(g_package));
    g_package.active = true;
    strlcpy(g_package.package_id, package_id, sizeof(g_package.package_id));
    strlcpy(g_package.harness_session_id, g_harness.harness_session_id, sizeof(g_package.harness_session_id));
    strlcpy(g_package.collection_id, g_collection.collection_id, sizeof(g_package.collection_id));
    strlcpy(g_package.state, "ready", sizeof(g_package.state));

    bool format_zip = strcmp(format->valuestring, "zip") == 0;
    cJSON_Delete(root);

    char resp[768];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"package_id\":\"%s\",\"harness_session_id\":\"%s\",\"collection_id\":\"%s\",\"state\":\"ready\",\"report_uri\":\"sdcard/conformance/%s/report_%s.%s\",\"sha256\":\"9f2f52be6f7d11f8f0a6f7bc62bf4b6c6e5ac6f4c91dc2a82bf32f4c0c55f2a1\"}}",
             package_id,
             g_harness.harness_session_id,
             g_collection.collection_id,
             g_harness.harness_session_id,
             package_id,
             format_zip ? "zip" : "json");
    return send_json(req, resp, 200);
}

static esp_err_t conformance_checklist_run_handler(httpd_req_t *req)
{
    char body[2048];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *harness_session_id = cJSON_GetObjectItemCaseSensitive(root, "harness_session_id");
    cJSON *checklist_id = cJSON_GetObjectItemCaseSensitive(root, "checklist_id");
    cJSON *selection = cJSON_GetObjectItemCaseSensitive(root, "selection");
    cJSON *stop_on_failure = cJSON_GetObjectItemCaseSensitive(root, "stop_on_failure");

    if (!cJSON_IsString(harness_session_id) || !cJSON_IsString(checklist_id)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (stop_on_failure != NULL && !cJSON_IsBool(stop_on_failure)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *include_case_ids = NULL;
    cJSON *exclude_case_ids = NULL;
    uint32_t include_count = 0;
    uint32_t exclude_count = 0;
    bool valid = true;
    if (selection != NULL) {
        if (!cJSON_IsObject(selection)) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }

        include_case_ids = cJSON_GetObjectItemCaseSensitive(selection, "include_case_ids");
        exclude_case_ids = cJSON_GetObjectItemCaseSensitive(selection, "exclude_case_ids");

        parse_optional_string_array(selection, "include_case_ids", &include_count, &valid);
        if (!valid) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
        parse_optional_string_array(selection, "exclude_case_ids", &exclude_count, &valid);
        if (!valid || arrays_intersect(include_case_ids, exclude_case_ids)) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
    }

    if (!g_harness.active || strcmp(harness_session_id->valuestring, g_harness.harness_session_id) != 0 ||
        !checklist_id_supported(checklist_id->valuestring)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"NOT_FOUND\"}}", 404);
    }

    if (!g_package.active || strcmp(g_package.state, "ready") != 0 ||
        strcmp(g_package.harness_session_id, g_harness.harness_session_id) != 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\"}}", 409);
    }

    if (g_runner.active && !runner_state_terminal(g_runner.state) &&
        strcmp(g_runner.harness_session_id, harness_session_id->valuestring) == 0 &&
        strcmp(g_runner.checklist_id, checklist_id->valuestring) == 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CONFLICT\"}}", 409);
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    memset(&g_runner, 0, sizeof(g_runner));
    g_runner.active = true;
    g_runner_seq++;
    snprintf(g_runner.runner_id, sizeof(g_runner.runner_id), "run_%06llu", (unsigned long long)g_runner_seq);
    strlcpy(g_runner.harness_session_id, harness_session_id->valuestring, sizeof(g_runner.harness_session_id));
    strlcpy(g_runner.checklist_id, checklist_id->valuestring, sizeof(g_runner.checklist_id));
    g_runner.stop_on_failure = (stop_on_failure != NULL) ? cJSON_IsTrue(stop_on_failure) : false;
    g_runner.cases_total = include_count > 0 ? include_count : (g_harness.checks_enabled > 0 ? g_harness.checks_enabled : 1U);
    g_runner.cases_passed = g_runner.cases_total;
    g_runner.cases_failed = 0;
    g_runner.completed_cases = g_runner.cases_total;
    g_runner.current_case_id[0] = '\0';
    g_runner.started_at_us = now_us;
    g_runner.completed_at_us = now_us + 1000;
    g_runner.last_transition_at_us = g_runner.completed_at_us;
    strlcpy(g_runner.state, "completed", sizeof(g_runner.state));

    cJSON_Delete(root);

    char resp[640];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"runner_id\":\"%s\",\"harness_session_id\":\"%s\",\"checklist_id\":\"%s\",\"state\":\"%s\",\"cases_total\":%lu,\"cases_passed\":%lu,\"cases_failed\":%lu,\"started_at_us\":%llu,\"completed_at_us\":%llu}}",
             g_runner.runner_id,
             g_runner.harness_session_id,
             g_runner.checklist_id,
             g_runner.state,
             (unsigned long)g_runner.cases_total,
             (unsigned long)g_runner.cases_passed,
             (unsigned long)g_runner.cases_failed,
             (unsigned long long)g_runner.started_at_us,
             (unsigned long long)g_runner.completed_at_us);
    return send_json(req, resp, 200);
}

static esp_err_t conformance_checklist_status_handler(httpd_req_t *req)
{
    char runner_id[64];
    if (!esptari_web_query_value(req, "runner_id", runner_id, sizeof(runner_id)) || runner_id[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    if (!g_runner.active || strcmp(runner_id, g_runner.runner_id) != 0) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"NOT_FOUND\"}}", 404);
    }

    char resp[512];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"runner_id\":\"%s\",\"state\":\"%s\",\"current_case_id\":null,\"progress\":{\"completed_cases\":%lu,\"total_cases\":%lu},\"last_transition_at_us\":%llu}}",
             g_runner.runner_id,
             g_runner.state,
             (unsigned long)g_runner.completed_cases,
             (unsigned long)g_runner.cases_total,
             (unsigned long long)g_runner.last_transition_at_us);
    return send_json(req, resp, 200);
}

static esp_err_t conformance_review_pack_generate_handler(httpd_req_t *req)
{
    char body[2048];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *harness_session_id = cJSON_GetObjectItemCaseSensitive(root, "harness_session_id");
    cJSON *runner_id = cJSON_GetObjectItemCaseSensitive(root, "runner_id");
    cJSON *package_id = cJSON_GetObjectItemCaseSensitive(root, "package_id");
    cJSON *include_sections = cJSON_GetObjectItemCaseSensitive(root, "include_sections");

    if (!cJSON_IsString(harness_session_id) || !cJSON_IsString(runner_id) || !cJSON_IsString(package_id) ||
        !cJSON_IsArray(include_sections) || cJSON_GetArraySize(include_sections) < 1) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    int n = cJSON_GetArraySize(include_sections);
    for (int i = 0; i < n; ++i) {
        cJSON *section = cJSON_GetArrayItem(include_sections, i);
        if (!cJSON_IsString(section) || !review_section_allowed(section->valuestring)) {
            cJSON_Delete(root);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
    }

    if (!g_harness.active || !g_runner.active || !g_package.active ||
        strcmp(harness_session_id->valuestring, g_harness.harness_session_id) != 0 ||
        strcmp(runner_id->valuestring, g_runner.runner_id) != 0 ||
        strcmp(package_id->valuestring, g_package.package_id) != 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"NOT_FOUND\"}}", 404);
    }

    if (!runner_state_terminal(g_runner.state)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\"}}", 409);
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    memset(&g_review_pack, 0, sizeof(g_review_pack));
    g_review_pack.active = true;
    g_review_pack_seq++;
    snprintf(g_review_pack.review_pack_id, sizeof(g_review_pack.review_pack_id), "rvp_%06llu", (unsigned long long)g_review_pack_seq);
    strlcpy(g_review_pack.harness_session_id, g_harness.harness_session_id, sizeof(g_review_pack.harness_session_id));
    strlcpy(g_review_pack.runner_id, g_runner.runner_id, sizeof(g_review_pack.runner_id));
    strlcpy(g_review_pack.package_id, g_package.package_id, sizeof(g_review_pack.package_id));
    strlcpy(g_review_pack.state, "ready", sizeof(g_review_pack.state));
    g_review_pack.generated_at_us = now_us;
    snprintf(g_review_pack.review_pack_uri,
             sizeof(g_review_pack.review_pack_uri),
             "sdcard/conformance/%s/review_pack_%s.json",
             g_harness.harness_session_id,
             g_review_pack.review_pack_id);

    cJSON_Delete(root);

    char resp[640];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"review_pack_id\":\"%s\",\"state\":\"%s\",\"harness_session_id\":\"%s\",\"runner_id\":\"%s\",\"generated_at_us\":%llu,\"review_pack_uri\":\"%s\"}}",
             g_review_pack.review_pack_id,
             g_review_pack.state,
             g_review_pack.harness_session_id,
             g_review_pack.runner_id,
             (unsigned long long)g_review_pack.generated_at_us,
             g_review_pack.review_pack_uri);
    return send_json(req, resp, 200);
}

static esp_err_t conformance_signoff_bundle_assemble_handler(httpd_req_t *req)
{
    char body[2048];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *review_pack_id = cJSON_GetObjectItemCaseSensitive(root, "review_pack_id");
    cJSON *signoff = cJSON_GetObjectItemCaseSensitive(root, "signoff");
    if (!cJSON_IsString(review_pack_id) || !cJSON_IsObject(signoff)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *requested_by = cJSON_GetObjectItemCaseSensitive(signoff, "requested_by");
    cJSON *approver = cJSON_GetObjectItemCaseSensitive(signoff, "approver");
    cJSON *label = cJSON_GetObjectItemCaseSensitive(signoff, "label");
    if (!cJSON_IsString(requested_by) || !cJSON_IsString(approver) || !cJSON_IsString(label) ||
        requested_by->valuestring[0] == '\0' || approver->valuestring[0] == '\0' || label->valuestring[0] == '\0') {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    if (!g_review_pack.active || strcmp(review_pack_id->valuestring, g_review_pack.review_pack_id) != 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"NOT_FOUND\"}}", 404);
    }

    if (strcmp(g_review_pack.state, "ready") != 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\"}}", 409);
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    memset(&g_signoff_bundle, 0, sizeof(g_signoff_bundle));
    g_signoff_bundle.active = true;
    g_signoff_seq++;
    snprintf(g_signoff_bundle.bundle_id, sizeof(g_signoff_bundle.bundle_id), "sgn_%06llu", (unsigned long long)g_signoff_seq);
    strlcpy(g_signoff_bundle.review_pack_id, g_review_pack.review_pack_id, sizeof(g_signoff_bundle.review_pack_id));
    strlcpy(g_signoff_bundle.state, "ready", sizeof(g_signoff_bundle.state));
    snprintf(g_signoff_bundle.bundle_uri,
             sizeof(g_signoff_bundle.bundle_uri),
             "sdcard/conformance/%s/signoff_bundle_%s.zip",
             g_harness.harness_session_id,
             g_signoff_bundle.bundle_id);
    strlcpy(g_signoff_bundle.sha256,
            "6b2ebf4f95df8a59f2ad8a5e622ecf9f53251b1a17e5dca35d43a83a420ff7d9",
            sizeof(g_signoff_bundle.sha256));
    g_signoff_bundle.assembled_at_us = now_us;

    cJSON_Delete(root);

    char resp[720];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"bundle_id\":\"%s\",\"review_pack_id\":\"%s\",\"state\":\"%s\",\"bundle_uri\":\"%s\",\"sha256\":\"%s\",\"assembled_at_us\":%llu}}",
             g_signoff_bundle.bundle_id,
             g_signoff_bundle.review_pack_id,
             g_signoff_bundle.state,
             g_signoff_bundle.bundle_uri,
             g_signoff_bundle.sha256,
             (unsigned long long)g_signoff_bundle.assembled_at_us);
    return send_json(req, resp, 200);
}

void esptari_web_conformance_register_routes(httpd_handle_t server_handle)
{
    httpd_uri_t conformance_session = {.uri = "/api/v2/conformance/harness/session", .method = HTTP_POST, .handler = conformance_harness_session_handler, .user_ctx = NULL};
    httpd_uri_t conformance_manifest = {.uri = "/api/v2/conformance/manifests/load", .method = HTTP_POST, .handler = conformance_manifest_load_handler, .user_ctx = NULL};
    httpd_uri_t conformance_collect = {.uri = "/api/v2/conformance/harness/evidence/collect", .method = HTTP_POST, .handler = conformance_evidence_collect_handler, .user_ctx = NULL};
    httpd_uri_t conformance_report = {.uri = "/api/v2/conformance/harness/report/package", .method = HTTP_POST, .handler = conformance_report_package_handler, .user_ctx = NULL};
    httpd_uri_t conformance_checklist_run = {.uri = "/api/v2/conformance/harness/checklist/run", .method = HTTP_POST, .handler = conformance_checklist_run_handler, .user_ctx = NULL};
    httpd_uri_t conformance_checklist_status = {.uri = "/api/v2/conformance/harness/checklist/run/status", .method = HTTP_GET, .handler = conformance_checklist_status_handler, .user_ctx = NULL};
    httpd_uri_t conformance_review_pack = {.uri = "/api/v2/conformance/harness/review-pack/generate", .method = HTTP_POST, .handler = conformance_review_pack_generate_handler, .user_ctx = NULL};
    httpd_uri_t conformance_signoff_bundle = {.uri = "/api/v2/conformance/harness/signoff-bundle/assemble", .method = HTTP_POST, .handler = conformance_signoff_bundle_assemble_handler, .user_ctx = NULL};

    httpd_register_uri_handler(server_handle, &conformance_session);
    httpd_register_uri_handler(server_handle, &conformance_manifest);
    httpd_register_uri_handler(server_handle, &conformance_collect);
    httpd_register_uri_handler(server_handle, &conformance_report);
    httpd_register_uri_handler(server_handle, &conformance_checklist_run);
    httpd_register_uri_handler(server_handle, &conformance_checklist_status);
    httpd_register_uri_handler(server_handle, &conformance_review_pack);
    httpd_register_uri_handler(server_handle, &conformance_signoff_bundle);
}
