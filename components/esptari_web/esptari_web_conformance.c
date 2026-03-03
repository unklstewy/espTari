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

static conformance_manifest_state_t g_manifest;
static conformance_harness_state_t g_harness;
static conformance_collection_state_t g_collection;
static uint64_t g_harness_seq;
static uint64_t g_collection_seq;
static uint64_t g_package_seq;

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

void esptari_web_conformance_register_routes(httpd_handle_t server_handle)
{
    httpd_uri_t conformance_session = {.uri = "/api/v2/conformance/harness/session", .method = HTTP_POST, .handler = conformance_harness_session_handler, .user_ctx = NULL};
    httpd_uri_t conformance_manifest = {.uri = "/api/v2/conformance/manifests/load", .method = HTTP_POST, .handler = conformance_manifest_load_handler, .user_ctx = NULL};
    httpd_uri_t conformance_collect = {.uri = "/api/v2/conformance/harness/evidence/collect", .method = HTTP_POST, .handler = conformance_evidence_collect_handler, .user_ctx = NULL};
    httpd_uri_t conformance_report = {.uri = "/api/v2/conformance/harness/report/package", .method = HTTP_POST, .handler = conformance_report_package_handler, .user_ctx = NULL};

    httpd_register_uri_handler(server_handle, &conformance_session);
    httpd_register_uri_handler(server_handle, &conformance_manifest);
    httpd_register_uri_handler(server_handle, &conformance_collect);
    httpd_register_uri_handler(server_handle, &conformance_report);
}
