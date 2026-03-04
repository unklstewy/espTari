#include "esptari_web_persistence.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esptari_core.h"
#include "esptari_web_http_utils.h"

#define send_json esptari_web_send_json

static uint64_t state_seq = 1;
static char latest_snapshot_id[64] = "state_000001";
static uint64_t latest_saved_at_us;
static bool has_saved_snapshot;

#define SNAPSHOT_INDEX_PATH "/spiffs/snapshot_index_v1.log"
#define SNAPSHOT_INDEX_MAX_ENTRIES 32

typedef struct {
    char snapshot_id[64];
    char session_id[32];
    char profile[32];
    char name[64];
    uint64_t saved_at_us;
    bool corrupted;
} snapshot_index_entry_t;

static snapshot_index_entry_t snapshot_index[SNAPSHOT_INDEX_MAX_ENTRIES];
static size_t snapshot_index_count;
static bool snapshot_index_loaded;

static esp_err_t parse_body_json(httpd_req_t *req, char *body, size_t body_len, cJSON **out_root)
{
    if (esptari_web_read_request_body(req, body, body_len) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    *out_root = root;
    return ESP_OK;
}

static esp_err_t validate_session_local(httpd_req_t *req, cJSON *root)
{
    const char *session_id = NULL;
    if (!esptari_web_json_get_string(root, "session_id", &session_id) || session_id == NULL || session_id[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (strcmp(session_id, "ses_local") != 0) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }
    return ESP_OK;
}

static void sanitize_field(char *text)
{
    if (text == NULL) {
        return;
    }
    for (char *cursor = text; *cursor != '\0'; cursor++) {
        if (*cursor == '|') {
            *cursor = '_';
        }
    }
}

static void parse_token(char **ctx, char *out, size_t out_len)
{
    out[0] = '\0';
    char *token = strtok_r(NULL, "|", ctx);
    if (token != NULL) {
        snprintf(out, out_len, "%s", token);
    }
}

static void load_snapshot_index_if_needed(void)
{
    if (snapshot_index_loaded) {
        return;
    }

    snapshot_index_loaded = true;
    snapshot_index_count = 0;

    FILE *file = fopen(SNAPSHOT_INDEX_PATH, "r");
    if (file == NULL) {
        return;
    }

    char line[320];
    while (fgets(line, sizeof(line), file) != NULL && snapshot_index_count < SNAPSHOT_INDEX_MAX_ENTRIES) {
        size_t length = strlen(line);
        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[length - 1] = '\0';
            length--;
        }
        if (line[0] == '\0') {
            continue;
        }

        snapshot_index_entry_t entry = {0};
        char buffer[320];
        snprintf(buffer, sizeof(buffer), "%s", line);
        char *ctx = NULL;
        char *version = strtok_r(buffer, "|", &ctx);
        if (version == NULL || strcmp(version, "v1") != 0) {
            entry.corrupted = true;
            snprintf(entry.snapshot_id, sizeof(entry.snapshot_id), "corrupted_%u", (unsigned)(snapshot_index_count + 1));
            snprintf(entry.name, sizeof(entry.name), "invalid_index_version");
            snapshot_index[snapshot_index_count++] = entry;
            continue;
        }

        parse_token(&ctx, entry.snapshot_id, sizeof(entry.snapshot_id));
        parse_token(&ctx, entry.session_id, sizeof(entry.session_id));
        parse_token(&ctx, entry.profile, sizeof(entry.profile));

        char saved_at_text[32] = {0};
        parse_token(&ctx, saved_at_text, sizeof(saved_at_text));
        parse_token(&ctx, entry.name, sizeof(entry.name));

        if (entry.snapshot_id[0] == '\0' || entry.session_id[0] == '\0' || entry.profile[0] == '\0' || saved_at_text[0] == '\0') {
            entry.corrupted = true;
            if (entry.snapshot_id[0] == '\0') {
                snprintf(entry.snapshot_id, sizeof(entry.snapshot_id), "corrupted_%u", (unsigned)(snapshot_index_count + 1));
            }
            if (entry.name[0] == '\0') {
                snprintf(entry.name, sizeof(entry.name), "invalid_index_entry");
            }
            snapshot_index[snapshot_index_count++] = entry;
            continue;
        }

        char *end = NULL;
        unsigned long long parsed_saved_at = strtoull(saved_at_text, &end, 10);
        if (end == saved_at_text || *end != '\0') {
            entry.corrupted = true;
            if (entry.name[0] == '\0') {
                snprintf(entry.name, sizeof(entry.name), "invalid_saved_at");
            }
            snapshot_index[snapshot_index_count++] = entry;
            continue;
        }

        entry.saved_at_us = (uint64_t)parsed_saved_at;
        if (entry.name[0] == '\0') {
            snprintf(entry.name, sizeof(entry.name), "auto");
        }
        snapshot_index[snapshot_index_count++] = entry;
    }

    fclose(file);
}

static void persist_snapshot_index(void)
{
    FILE *file = fopen(SNAPSHOT_INDEX_PATH, "w");
    if (file == NULL) {
        return;
    }

    for (size_t i = 0; i < snapshot_index_count; i++) {
        if (snapshot_index[i].corrupted) {
            continue;
        }
        fprintf(file,
                "v1|%s|%s|%s|%llu|%s\n",
                snapshot_index[i].snapshot_id,
                snapshot_index[i].session_id,
                snapshot_index[i].profile,
                (unsigned long long)snapshot_index[i].saved_at_us,
                snapshot_index[i].name);
    }

    fclose(file);
}

static void upsert_snapshot_index_entry(const char *snapshot_id,
                                        const char *session_id,
                                        const char *profile,
                                        const char *name,
                                        uint64_t saved_at_us)
{
    load_snapshot_index_if_needed();

    for (size_t i = 0; i < snapshot_index_count; i++) {
        if (!snapshot_index[i].corrupted && strcmp(snapshot_index[i].snapshot_id, snapshot_id) == 0) {
            snprintf(snapshot_index[i].session_id, sizeof(snapshot_index[i].session_id), "%s", session_id);
            snprintf(snapshot_index[i].profile, sizeof(snapshot_index[i].profile), "%s", profile);
            snprintf(snapshot_index[i].name, sizeof(snapshot_index[i].name), "%s", name);
            sanitize_field(snapshot_index[i].name);
            snapshot_index[i].saved_at_us = saved_at_us;
            persist_snapshot_index();
            return;
        }
    }

    if (snapshot_index_count >= SNAPSHOT_INDEX_MAX_ENTRIES) {
        for (size_t i = 1; i < snapshot_index_count; i++) {
            snapshot_index[i - 1] = snapshot_index[i];
        }
        snapshot_index_count--;
    }

    snapshot_index_entry_t *entry = &snapshot_index[snapshot_index_count++];
    memset(entry, 0, sizeof(*entry));
    snprintf(entry->snapshot_id, sizeof(entry->snapshot_id), "%s", snapshot_id);
    snprintf(entry->session_id, sizeof(entry->session_id), "%s", session_id);
    snprintf(entry->profile, sizeof(entry->profile), "%s", profile);
    snprintf(entry->name, sizeof(entry->name), "%s", name);
    sanitize_field(entry->name);
    entry->saved_at_us = saved_at_us;
    entry->corrupted = false;

    persist_snapshot_index();
}

static esp_err_t state_save_handler(httpd_req_t *req)
{
    char body[768];
    cJSON *root = NULL;
    esp_err_t parse_err = parse_body_json(req, body, sizeof(body), &root);
    if (parse_err != ESP_OK) {
        return parse_err;
    }
    esp_err_t guard = validate_session_local(req, root);
    if (guard != ESP_OK) {
        cJSON_Delete(root);
        return guard;
    }

    const char *name = NULL;
    if (esptari_web_json_get_string(root, "name", &name) && name != NULL && name[0] == '\0') {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    state_seq++;
    snprintf(latest_snapshot_id, sizeof(latest_snapshot_id), "state_%06llu", (unsigned long long)state_seq);
    latest_saved_at_us = (uint64_t)esp_timer_get_time();
    has_saved_snapshot = true;
    const char *snapshot_name = (name != NULL && name[0] != '\0') ? name : "auto";
    upsert_snapshot_index_entry(latest_snapshot_id, "ses_local", "st_520_pal", snapshot_name, latest_saved_at_us);
    cJSON_Delete(root);

    char resp[512];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"snapshot_id\":\"%s\",\"name\":\"%s\",\"saved_at_us\":%llu}}",
             latest_snapshot_id,
             snapshot_name,
             (unsigned long long)latest_saved_at_us);
    return send_json(req, resp, 200);
}

static esp_err_t state_restore_handler(httpd_req_t *req)
{
    char body[768];
    cJSON *root = NULL;
    esp_err_t parse_err = parse_body_json(req, body, sizeof(body), &root);
    if (parse_err != ESP_OK) {
        return parse_err;
    }
    esp_err_t guard = validate_session_local(req, root);
    if (guard != ESP_OK) {
        cJSON_Delete(root);
        return guard;
    }

    const char *snapshot_id = NULL;
    if (!esptari_web_json_get_string(root, "snapshot_id", &snapshot_id) || snapshot_id == NULL || snapshot_id[0] == '\0') {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (!has_saved_snapshot || strcmp(snapshot_id, latest_snapshot_id) != 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"SNAPSHOT_NOT_FOUND\"}}", 404);
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);

    char resp[448];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"snapshot_id\":\"%s\",\"restored_at_us\":%llu,\"result\":\"restored\"}}",
             snapshot_id,
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

static esp_err_t state_list_handler(httpd_req_t *req)
{
    char session_id[64] = {0};
    if (!esptari_web_query_value(req, "session_id", session_id, sizeof(session_id)) || session_id[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (strcmp(session_id, "ses_local") != 0) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }

    char profile_filter[32] = {0};
    char saved_after_text[32] = {0};
    char saved_before_text[32] = {0};
    bool has_profile_filter = esptari_web_query_value(req, "profile", profile_filter, sizeof(profile_filter));
    bool has_saved_after = esptari_web_query_value(req, "saved_after_us", saved_after_text, sizeof(saved_after_text));
    bool has_saved_before = esptari_web_query_value(req, "saved_before_us", saved_before_text, sizeof(saved_before_text));

    uint64_t saved_after_us = 0;
    uint64_t saved_before_us = UINT64_MAX;
    if (has_saved_after && saved_after_text[0] != '\0') {
        char *end = NULL;
        unsigned long long parsed = strtoull(saved_after_text, &end, 10);
        if (end == saved_after_text || *end != '\0') {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
        saved_after_us = (uint64_t)parsed;
    }
    if (has_saved_before && saved_before_text[0] != '\0') {
        char *end = NULL;
        unsigned long long parsed = strtoull(saved_before_text, &end, 10);
        if (end == saved_before_text || *end != '\0') {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
        saved_before_us = (uint64_t)parsed;
    }
    if (saved_after_us > saved_before_us) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    load_snapshot_index_if_needed();

    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    cJSON *snapshots = cJSON_CreateArray();
    cJSON *index_warnings = cJSON_CreateObject();

    if (root == NULL || data == NULL || snapshots == NULL || index_warnings == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(data);
        cJSON_Delete(snapshots);
        cJSON_Delete(index_warnings);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    size_t corrupted_entries = 0;
    size_t visible_entries = 0;
    for (size_t i = 0; i < snapshot_index_count; i++) {
        const snapshot_index_entry_t *entry = &snapshot_index[i];
        if (entry->corrupted) {
            corrupted_entries++;
        }

        if (!entry->corrupted) {
            if (strcmp(entry->session_id, session_id) != 0) {
                continue;
            }
            if (has_profile_filter && profile_filter[0] != '\0' && strcmp(entry->profile, profile_filter) != 0) {
                continue;
            }
            if (entry->saved_at_us < saved_after_us || entry->saved_at_us > saved_before_us) {
                continue;
            }
        }

        cJSON *item = cJSON_CreateObject();
        if (item == NULL) {
            cJSON_Delete(root);
            cJSON_Delete(data);
            cJSON_Delete(snapshots);
            cJSON_Delete(index_warnings);
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
        }

        cJSON_AddStringToObject(item, "snapshot_id", entry->snapshot_id);
        cJSON_AddStringToObject(item, "profile", entry->profile[0] != '\0' ? entry->profile : "unknown");
        cJSON_AddStringToObject(item, "name", entry->name[0] != '\0' ? entry->name : "auto");
        cJSON_AddNumberToObject(item, "saved_at_us", (double)entry->saved_at_us);
        cJSON_AddStringToObject(item, "state", entry->corrupted ? "corrupted" : "available");
        cJSON_AddItemToArray(snapshots, item);
        visible_entries++;
    }

    cJSON_AddBoolToObject(root, "ok", true);
    cJSON_AddStringToObject(data, "session_id", session_id);
    cJSON_AddNumberToObject(data, "total_entries", (double)visible_entries);
    cJSON_AddNumberToObject(index_warnings, "corrupted_entries", (double)corrupted_entries);
    cJSON_AddItemToObject(data, "index_warnings", index_warnings);
    cJSON_AddItemToObject(data, "snapshots", snapshots);
    cJSON_AddItemToObject(root, "data", data);

    char *resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (resp == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    esp_err_t result = send_json(req, resp, 200);
    cJSON_free(resp);
    return result;
}

void esptari_web_persistence_register_routes(httpd_handle_t server_handle)
{
    httpd_uri_t state_save = {.uri = "/api/v2/engine/state/save", .method = HTTP_POST, .handler = state_save_handler, .user_ctx = NULL};
    httpd_uri_t state_restore = {.uri = "/api/v2/engine/state/restore", .method = HTTP_POST, .handler = state_restore_handler, .user_ctx = NULL};
    httpd_uri_t state_list = {.uri = "/api/v2/engine/state/list", .method = HTTP_GET, .handler = state_list_handler, .user_ctx = NULL};

    httpd_register_uri_handler(server_handle, &state_save);
    httpd_register_uri_handler(server_handle, &state_restore);
    httpd_register_uri_handler(server_handle, &state_list);
}
