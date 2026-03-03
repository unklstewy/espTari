#include "esptari_web_catalog_read.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esptari_web_catalog_state.h"
#include "esptari_web_http_utils.h"

#define send_json esptari_web_send_json
#define query_value esptari_web_query_value
#define parse_u32_str esptari_web_parse_u32_str

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

esp_err_t esptari_web_catalogs_list_handler(httpd_req_t *req)
{
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    cJSON *catalogs = cJSON_CreateArray();
    cJSON_AddItemToObject(data, "catalogs", catalogs);

    for (size_t i = 0; i < esptari_web_catalog_count(); i++) {
        const catalog_def_t *catalog_def = esptari_web_catalog_at(i);
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", catalog_def->name);
        cJSON_AddStringToObject(item, "path", catalog_def->path);
        cJSON_AddNumberToObject(item, "entries", (double)catalog_def->entry_count);
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

    const catalog_def_t *def = esptari_web_catalog_find(catalog);
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
        const char *entry_state = esptari_web_catalog_entry_state(def, i);
        const char *entry_local_path = esptari_web_catalog_entry_local_path_projected(def, i);
        if (missing_only && esptari_web_catalog_entry_local_present(def, i)) {
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

    const catalog_def_t *def = esptari_web_catalog_find(catalog);
    if (def == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CATALOG_NOT_FOUND\"}}", 404);
    }

    int entry_index = esptari_web_catalog_find_entry_index(def, entry_id);
    if (entry_index < 0) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CATALOG_ENTRY_NOT_FOUND\"}}", 404);
    }

    const catalog_entry_t *match = &def->entries[(size_t)entry_index];
    const char *projected_state = esptari_web_catalog_entry_state(def, (size_t)entry_index);
    const char *projected_local_path = esptari_web_catalog_entry_local_path_projected(def, (size_t)entry_index);

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

static esp_err_t catalog_missing_report_handler(httpd_req_t *req)
{
    char catalog[32] = {0};
    if (sscanf(req->uri, "/api/v2/catalogs/%31[^/]/missing-report", catalog) != 1) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    const catalog_def_t *def = esptari_web_catalog_find(catalog);
    if (def == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CATALOG_NOT_FOUND\"}}", 404);
    }
    if (esptari_web_catalog_last_scan_id()[0] == '\0') {
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
        if (esptari_web_catalog_prev_scan_id()[0] == '\0' || strcmp(since_scan_id, esptari_web_catalog_prev_scan_id()) != 0) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CONFLICT\",\"details\":{\"required_operation\":\"POST /api/v2/catalogs/floppies/rescan-local\"}}}", 409);
        }
    }

    uint32_t missing_total = 0;
    uint32_t emitted = 0;
    char assets[1024] = {0};
    size_t offset = 0;
    for (size_t i = 0; i < def->entry_count; i++) {
        if (esptari_web_catalog_entry_local_present(def, i)) {
            continue;
        }
        missing_total++;
        if (emitted >= limit || offset >= sizeof(assets) - 4) {
            continue;
        }
        catalog_entry_runtime_t *runtime = esptari_web_catalog_runtime_at(def, i);
        uint64_t first_missing_at_us = runtime != NULL ? runtime->first_missing_at_us : 1710002000000ULL;
        int wrote = snprintf(assets + offset,
                             sizeof(assets) - offset,
                             "%s{\"entry_id\":\"%s\",\"catalog\":\"%s\",\"local_present\":false,\"expected_path\":\"%s\",\"availability_state\":\"%s\",\"first_missing_at_us\":%llu,\"last_seen_scan_id\":\"%s\"}",
                             emitted == 0 ? "" : ",",
                             def->entries[i].id,
                             def->name,
                             def->entries[i].local_path[0] == '\0' ? "/sdcard/disks/st/UNKNOWN.ST" : def->entries[i].local_path,
                             esptari_web_catalog_entry_state(def, i),
                             (unsigned long long)first_missing_at_us,
                             esptari_web_catalog_prev_scan_id()[0] == '\0' ? esptari_web_catalog_last_scan_id() : esptari_web_catalog_prev_scan_id());
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
             esptari_web_catalog_last_scan_id(),
             esptari_web_catalog_prev_scan_id()[0] == '\0' ? "null" : "\"scan_base\"",
             (unsigned long)missing_total,
             (unsigned long)missing_total,
             assets);
    return send_json(req, resp, 200);
}

esp_err_t esptari_web_catalogs_router_handler(httpd_req_t *req)
{
    if (strcmp(req->uri, "/api/v2/catalogs/list") == 0) {
        return esptari_web_catalogs_list_handler(req);
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
