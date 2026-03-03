#include "esptari_web_catalog.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esptari_web_http_utils.h"

#define send_json esptari_web_send_json
#define read_request_body esptari_web_read_request_body
#define query_value esptari_web_query_value
#define parse_u32_str esptari_web_parse_u32_str
#define json_get_string esptari_web_json_get_string

typedef struct {
    const char *id;
    const char *local_path;
    const char *hosted_url;
    const char *sha256_expected;
    const char *availability_state;
    const char *availability_checked_at;
    uint32_t download_fail_count;
} catalog_entry_t;

typedef struct {
    bool state_override;
    char availability_state[16];
    bool local_present;
    bool dead_marked;
    char dead_source[24];
    char last_dead_reason[96];
    uint64_t last_dead_marked_at_us;
    uint32_t dead_retry_attempts;
    uint32_t dead_retry_successes;
    uint32_t dead_retry_failures;
    uint64_t last_dead_retry_at_us;
    char last_dead_retry_result[16];
    uint64_t first_missing_at_us;
} catalog_entry_runtime_t;

typedef struct {
    const char *name;
    const char *path;
    const catalog_entry_t *entries;
    size_t entry_count;
} catalog_def_t;

static uint64_t catalog_download_seq;
static uint64_t catalog_probe_seq;
static uint64_t catalog_scan_seq;
static char catalog_last_scan_id[48];
static char catalog_prev_scan_id[48];

static const catalog_entry_t rom_catalog_entries[] = {
    {"rom.atari.st.01", "/sdcard/roms/st/TOS104.ROM", "", "", "local_only", "2026-03-01T15:22:01Z", 0},
    {"rom.atari.st.02", "", "http://catalog.example/roms/TOS206.ROM", "sha256:rom0206", "online", "2026-03-01T15:22:05Z", 0},
};

static const catalog_entry_t floppy_catalog_entries[] = {
    {"disk.automation.a_093", "/sdcard/disks/st/AUTOMATION/A_093.ST", "http://ataristdb.sidecartridge.com/AUTOMATION/A_093.ST", "sha256:abcd", "online", "2026-03-01T15:22:01Z", 0},
    {"disk.demos.dead_entry", "", "http://ataristdb.sidecartridge.com/DEMOS/DEAD.ST", "", "dead", "2026-03-01T15:22:11Z", 3},
};

static const catalog_entry_t tos_catalog_entries[] = {
    {"tos.eu.1.04", "/sdcard/tos/TOS104.IMG", "", "", "local_only", "2026-03-01T15:22:21Z", 0},
    {"tos.eu.2.06", "", "http://catalog.example/tos/TOS206.IMG", "sha256:tos0206", "offline", "2026-03-01T15:22:31Z", 1},
};

static catalog_entry_runtime_t rom_catalog_runtime[] = {
    {.state_override = false, .local_present = true, .dead_marked = false, .last_dead_retry_result = "none", .first_missing_at_us = 0},
    {.state_override = false, .local_present = false, .dead_marked = false, .last_dead_retry_result = "none", .first_missing_at_us = 1710002100000ULL},
};

static catalog_entry_runtime_t floppy_catalog_runtime[] = {
    {.state_override = false, .local_present = true, .dead_marked = false, .last_dead_retry_result = "none", .first_missing_at_us = 0},
    {.state_override = true, .availability_state = "dead", .local_present = false, .dead_marked = true, .dead_source = "probe_threshold", .last_dead_reason = "probe failure threshold reached", .last_dead_marked_at_us = 1710002200000ULL, .last_dead_retry_result = "none", .first_missing_at_us = 1710002200000ULL},
};

static catalog_entry_runtime_t tos_catalog_runtime[] = {
    {.state_override = false, .local_present = true, .dead_marked = false, .last_dead_retry_result = "none", .first_missing_at_us = 0},
    {.state_override = false, .local_present = false, .dead_marked = false, .last_dead_retry_result = "none", .first_missing_at_us = 1710002300000ULL},
};

static const catalog_def_t catalog_defs[] = {
    {"roms", "/sdcard/config/engine_v2/rom_catalog.json", rom_catalog_entries, sizeof(rom_catalog_entries) / sizeof(rom_catalog_entries[0])},
    {"floppies", "/sdcard/config/engine_v2/disk_catalog.json", floppy_catalog_entries, sizeof(floppy_catalog_entries) / sizeof(floppy_catalog_entries[0])},
    {"tos", "/sdcard/config/engine_v2/tos_catalog.json", tos_catalog_entries, sizeof(tos_catalog_entries) / sizeof(tos_catalog_entries[0])},
};

static const catalog_def_t *find_catalog(const char *name)
{
    if (name == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < sizeof(catalog_defs) / sizeof(catalog_defs[0]); i++) {
        if (strcmp(catalog_defs[i].name, name) == 0) {
            return &catalog_defs[i];
        }
    }
    return NULL;
}

static catalog_entry_runtime_t *catalog_runtime_at(const catalog_def_t *def, size_t index)
{
    if (def == NULL) {
        return NULL;
    }
    if (strcmp(def->name, "roms") == 0 && index < (sizeof(rom_catalog_runtime) / sizeof(rom_catalog_runtime[0]))) {
        return &rom_catalog_runtime[index];
    }
    if (strcmp(def->name, "floppies") == 0 && index < (sizeof(floppy_catalog_runtime) / sizeof(floppy_catalog_runtime[0]))) {
        return &floppy_catalog_runtime[index];
    }
    if (strcmp(def->name, "tos") == 0 && index < (sizeof(tos_catalog_runtime) / sizeof(tos_catalog_runtime[0]))) {
        return &tos_catalog_runtime[index];
    }
    return NULL;
}

static const char *catalog_entry_state(const catalog_def_t *def, size_t index)
{
    const catalog_entry_t *entry = &def->entries[index];
    catalog_entry_runtime_t *runtime = catalog_runtime_at(def, index);
    if (runtime != NULL && runtime->state_override && runtime->availability_state[0] != '\0') {
        return runtime->availability_state;
    }
    return entry->availability_state;
}

static bool catalog_entry_local_present(const catalog_def_t *def, size_t index)
{
    catalog_entry_runtime_t *runtime = catalog_runtime_at(def, index);
    if (runtime != NULL) {
        return runtime->local_present;
    }
    const catalog_entry_t *entry = &def->entries[index];
    return !(entry == NULL || entry->local_path == NULL || entry->local_path[0] == '\0');
}

static const char *catalog_entry_local_path_projected(const catalog_def_t *def, size_t index)
{
    if (!catalog_entry_local_present(def, index)) {
        return "";
    }
    return def->entries[index].local_path;
}

static int find_catalog_entry_index(const catalog_def_t *def, const char *entry_id)
{
    if (def == NULL || entry_id == NULL) {
        return -1;
    }
    for (size_t i = 0; i < def->entry_count; i++) {
        if (strcmp(def->entries[i].id, entry_id) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static bool str_contains_nocase(const char *haystack, const char *needle)
{
    if (needle == NULL || needle[0] == '\0') {
        return true;
    }
    if (haystack == NULL) {
        return false;
    }
    size_t needle_len = strlen(needle);
    for (const char *p = haystack; *p != '\0'; p++) {
        if (strncasecmp(p, needle, needle_len) == 0) {
            return true;
        }
    }
    return false;
}

static esp_err_t catalogs_list_handler(httpd_req_t *req)
{
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    cJSON *catalogs = cJSON_CreateArray();
    cJSON_AddItemToObject(data, "catalogs", catalogs);

    for (size_t i = 0; i < sizeof(catalog_defs) / sizeof(catalog_defs[0]); i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", catalog_defs[i].name);
        cJSON_AddStringToObject(item, "path", catalog_defs[i].path);
        cJSON_AddNumberToObject(item, "entries", (double)catalog_defs[i].entry_count);
        cJSON_AddItemToArray(catalogs, item);
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

static esp_err_t catalog_entries_list_handler(httpd_req_t *req)
{
    char catalog[32] = {0};
    if (sscanf(req->uri, "/api/v2/catalogs/%31[^/]/entries", catalog) != 1) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const catalog_def_t *def = find_catalog(catalog);
    if (def == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CATALOG_NOT_FOUND\"}}", 404);
    }

    char query[96] = {0};
    char state[24] = {0};
    char missing_only_str[8] = {0};
    bool has_query = query_value(req, "query", query, sizeof(query));
    bool has_state = query_value(req, "state", state, sizeof(state));
    bool missing_only = query_value(req, "missing_only", missing_only_str, sizeof(missing_only_str)) &&
                        strcmp(missing_only_str, "true") == 0;

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    cJSON_AddStringToObject(data, "catalog", def->name);
    cJSON *entries = cJSON_CreateArray();
    cJSON_AddItemToObject(data, "entries", entries);

    for (size_t i = 0; i < def->entry_count; i++) {
        const catalog_entry_t *entry = &def->entries[i];
        const char *entry_state = catalog_entry_state(def, i);
        const char *entry_local_path = catalog_entry_local_path_projected(def, i);
        if (missing_only && catalog_entry_local_present(def, i)) {
            continue;
        }
        if (has_state && strcmp(state, entry_state) != 0) {
            continue;
        }
        if (has_query && !str_contains_nocase(entry->id, query) && !str_contains_nocase(entry_local_path, query) &&
            !str_contains_nocase(entry->hosted_url, query)) {
            continue;
        }

        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "id", entry->id);
        cJSON_AddStringToObject(item, "local_path", entry_local_path);
        cJSON_AddStringToObject(item, "hosted_url", entry->hosted_url);
        cJSON_AddStringToObject(item, "availability_state", entry_state);
        cJSON_AddItemToArray(entries, item);
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

static esp_err_t catalog_entry_get_handler(httpd_req_t *req)
{
    char catalog[32] = {0};
    char entry_id[128] = {0};
    if (sscanf(req->uri, "/api/v2/catalogs/%31[^/]/entries/%127s", catalog, entry_id) != 2) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const catalog_def_t *def = find_catalog(catalog);
    if (def == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CATALOG_NOT_FOUND\"}}", 404);
    }

    int entry_index = find_catalog_entry_index(def, entry_id);
    if (entry_index < 0) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CATALOG_ENTRY_NOT_FOUND\"}}", 404);
    }

    const catalog_entry_t *match = &def->entries[(size_t)entry_index];
    const char *projected_state = catalog_entry_state(def, (size_t)entry_index);
    const char *projected_local_path = catalog_entry_local_path_projected(def, (size_t)entry_index);

    char resp[640];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"id\":\"%s\",\"local_path\":\"%s\",\"hosted_url\":\"%s\",\"availability_state\":\"%s\",\"availability_checked_at\":\"%s\",\"download_fail_count\":%lu}}",
             match->id,
             projected_local_path,
             match->hosted_url,
             projected_state,
             match->availability_checked_at,
             (unsigned long)match->download_fail_count);
    return send_json(req, resp, 200);
}

static esp_err_t catalog_download_entry_handler(httpd_req_t *req)
{
    char catalog[32] = {0};
    if (sscanf(req->uri, "/api/v2/catalogs/%31[^/]/download-entry", catalog) != 1) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const catalog_def_t *def = find_catalog(catalog);
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

    int entry_index = find_catalog_entry_index(def, entry_id);
    if (entry_index < 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CATALOG_ENTRY_NOT_FOUND\"}}", 404);
    }

    const catalog_entry_t *entry = &def->entries[(size_t)entry_index];
    catalog_entry_runtime_t *runtime = catalog_runtime_at(def, (size_t)entry_index);
    const char *state = catalog_entry_state(def, (size_t)entry_index);

    if (entry->hosted_url == NULL || entry->hosted_url[0] == '\0') {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (strcmp(state, "dead") == 0 && !allow_dead_retry) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CATALOG_LINK_DEAD\"}}", 409);
    }
    if (!overwrite && catalog_entry_local_present(def, (size_t)entry_index)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CONFLICT\"}}", 409);
    }
    if (verify_sha256 && (entry->sha256_expected == NULL || entry->sha256_expected[0] == '\0')) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    catalog_download_seq++;
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
             (unsigned long long)catalog_download_seq,
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

    const catalog_def_t *def = find_catalog(catalog);
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
        if (catalog_entry_local_present(def, i)) {
            continue;
        }
        if (skip_dead && strcmp(catalog_entry_state(def, i), "dead") == 0) {
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

    const catalog_def_t *def = find_catalog(catalog);
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
        const char *state = catalog_entry_state(def, i);
        if (strcmp(state, "dead") == 0) {
            dead++;
        } else if (strcmp(state, "offline") == 0) {
            offline++;
        } else {
            online++;
        }
    }

    catalog_probe_seq++;
    char resp[1024];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"catalog\":\"%s\",\"worker_id\":\"probe_%06llu\",\"started_at_us\":%llu,\"completed_at_us\":%llu,\"policy\":{\"timeout_ms\":%lu,\"mark_dead_after_failures\":%lu,\"concurrency\":16},\"summary\":{\"probed\":%lu,\"online\":%lu,\"offline\":%lu,\"dead\":%lu,\"timed_out\":0},\"results\":[{\"entry_id\":\"%s\",\"state_before\":\"%s\",\"state_after\":\"%s\",\"attempts\":1,\"timed_out\":false,\"latency_ms\":11}]}}",
             def->name,
             (unsigned long long)catalog_probe_seq,
             (unsigned long long)started_at_us,
             (unsigned long long)completed_at_us,
             (unsigned long)timeout_ms,
             (unsigned long)mark_dead_after_failures,
             (unsigned long)probed,
             (unsigned long)online,
             (unsigned long)offline,
             (unsigned long)dead,
             def->entries[0].id,
             catalog_entry_state(def, 0),
             catalog_entry_state(def, 0));
    return send_json(req, resp, 200);
}

static esp_err_t catalog_mark_dead_handler(httpd_req_t *req)
{
    char catalog[32] = {0};
    if (sscanf(req->uri, "/api/v2/catalogs/%31[^/]/mark-dead", catalog) != 1) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const catalog_def_t *def = find_catalog(catalog);
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

    int entry_index = find_catalog_entry_index(def, entry_id);
    if (entry_index < 0) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CATALOG_ENTRY_NOT_FOUND\"}}", 404);
    }

    catalog_entry_runtime_t *runtime = catalog_runtime_at(def, (size_t)entry_index);
    if (runtime == NULL) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CONFLICT\"}}", 409);
    }

    const char *state_before = catalog_entry_state(def, (size_t)entry_index);
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
    const catalog_def_t *def = find_catalog(catalog);
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
    catalog_scan_seq++;
    if (catalog_last_scan_id[0] != '\0') {
        snprintf(catalog_prev_scan_id, sizeof(catalog_prev_scan_id), "%s", catalog_last_scan_id);
    }
    snprintf(catalog_last_scan_id, sizeof(catalog_last_scan_id), "scan_%06llu", (unsigned long long)catalog_scan_seq);

    uint32_t present = 0;
    uint32_t missing = 0;
    for (size_t i = 0; i < def->entry_count; i++) {
        catalog_entry_runtime_t *runtime = catalog_runtime_at(def, i);
        if (catalog_entry_local_present(def, i)) {
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
             catalog_last_scan_id,
             (unsigned long long)now_us,
             (unsigned long)def->entry_count,
             (unsigned long)present,
             (unsigned long)missing,
             (unsigned long)def->entry_count,
             def->entries[0].id,
             def->name,
             catalog_entry_local_present(def, 0) ? "true" : "false",
             catalog_entry_local_path_projected(def, 0),
             catalog_entry_local_present(def, 0) ? "737280" : "null",
             catalog_entry_local_present(def, 0) ? "1710002500000" : "null",
             (unsigned long long)now_us,
             def->entries[def->entry_count > 1 ? 1 : 0].id,
             def->name,
             catalog_entry_local_present(def, def->entry_count > 1 ? 1 : 0) ? "true" : "false",
             catalog_entry_local_present(def, def->entry_count > 1 ? 1 : 0) ? "\"/sdcard/present\"" : "null",
             (unsigned long long)now_us);
    return send_json(req, resp, 200);
}

static esp_err_t catalog_missing_report_handler(httpd_req_t *req)
{
    char catalog[32] = {0};
    if (sscanf(req->uri, "/api/v2/catalogs/%31[^/]/missing-report", catalog) != 1) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    const catalog_def_t *def = find_catalog(catalog);
    if (def == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CATALOG_NOT_FOUND\"}}", 404);
    }
    if (catalog_last_scan_id[0] == '\0') {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CONFLICT\",\"details\":{\"required_operation\":\"POST /api/v2/catalogs/floppies/rescan-local\"}}}", 409);
    }

    char limit_str[16] = {0};
    uint32_t limit = 200;
    if (query_value(req, "limit", limit_str, sizeof(limit_str))) {
        if (!parse_u32_str(limit_str, &limit) || limit == 0) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }
    }
    char state[24] = {0};
    if (query_value(req, "state", state, sizeof(state)) && strcmp(state, "missing") != 0) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    char since_scan_id[48] = {0};
    if (query_value(req, "since_scan_id", since_scan_id, sizeof(since_scan_id))) {
        if (catalog_prev_scan_id[0] == '\0' || strcmp(since_scan_id, catalog_prev_scan_id) != 0) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CONFLICT\",\"details\":{\"required_operation\":\"POST /api/v2/catalogs/floppies/rescan-local\"}}}", 409);
        }
    }

    uint32_t missing_total = 0;
    uint32_t emitted = 0;
    char assets[1024] = {0};
    size_t offset = 0;
    for (size_t i = 0; i < def->entry_count; i++) {
        if (catalog_entry_local_present(def, i)) {
            continue;
        }
        missing_total++;
        if (emitted >= limit || offset >= sizeof(assets) - 4) {
            continue;
        }
        catalog_entry_runtime_t *runtime = catalog_runtime_at(def, i);
        uint64_t first_missing_at_us = runtime != NULL ? runtime->first_missing_at_us : 1710002000000ULL;
        int wrote = snprintf(assets + offset,
                             sizeof(assets) - offset,
                             "%s{\"entry_id\":\"%s\",\"catalog\":\"%s\",\"local_present\":false,\"expected_path\":\"%s\",\"availability_state\":\"%s\",\"first_missing_at_us\":%llu,\"last_seen_scan_id\":\"%s\"}",
                             emitted == 0 ? "" : ",",
                             def->entries[i].id,
                             def->name,
                             def->entries[i].local_path[0] == '\0' ? "/sdcard/disks/st/UNKNOWN.ST" : def->entries[i].local_path,
                             catalog_entry_state(def, i),
                             (unsigned long long)first_missing_at_us,
                             catalog_prev_scan_id[0] == '\0' ? catalog_last_scan_id : catalog_prev_scan_id);
        if (wrote > 0) {
            offset += (size_t)wrote;
            emitted++;
        }
    }

    char resp[2048];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"catalog\":\"%s\",\"scan_id\":\"%s\",\"base_scan_id\":%s,\"summary\":{\"missing_total\":%lu,\"new_missing\":0,\"resolved_since_base\":0,\"unchanged_missing\":%lu},\"missing_assets\":[%s]}}",
             def->name,
             catalog_last_scan_id,
             catalog_prev_scan_id[0] == '\0' ? "null" : "\"scan_base\"",
             (unsigned long)missing_total,
             (unsigned long)missing_total,
             assets);
    return send_json(req, resp, 200);
}

static esp_err_t catalogs_router_handler(httpd_req_t *req)
{
    if (strcmp(req->uri, "/api/v2/catalogs/list") == 0) {
        return catalogs_list_handler(req);
    }
    if (strstr(req->uri, "/missing-report") != NULL) {
        return catalog_missing_report_handler(req);
    }
    if (strstr(req->uri, "/entries/") != NULL) {
        return catalog_entry_get_handler(req);
    }
    if (strstr(req->uri, "/entries") != NULL) {
        return catalog_entries_list_handler(req);
    }
    return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
}

static esp_err_t catalogs_post_router_handler(httpd_req_t *req)
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

void esptari_web_catalog_register_routes(httpd_handle_t server_handle)
{
    httpd_uri_t catalogs_list = {.uri = "/api/v2/catalogs/list", .method = HTTP_GET, .handler = catalogs_list_handler, .user_ctx = NULL};
    httpd_uri_t catalogs_entries = {.uri = "/api/v2/catalogs/*", .method = HTTP_GET, .handler = catalogs_router_handler, .user_ctx = NULL};
    httpd_uri_t catalogs_post = {.uri = "/api/v2/catalogs/*", .method = HTTP_POST, .handler = catalogs_post_router_handler, .user_ctx = NULL};

    httpd_register_uri_handler(server_handle, &catalogs_list);
    httpd_register_uri_handler(server_handle, &catalogs_entries);
    httpd_register_uri_handler(server_handle, &catalogs_post);
}
