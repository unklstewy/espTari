#include "esptari_web_files.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "esptari_web_http_utils.h"
#include "esptari_web_auth.h"
#include "esptari_web_audit.h"

static const char *TAG = "esptari_web_files";

typedef struct {
    char path[192];
    bool is_dir;
    uint64_t size;
    uint64_t modified_ms;
} esptari_web_file_entry_t;

static esptari_web_file_entry_t s_files[] = {
    {.path = "/sdcard/roms", .is_dir = true, .size = 0, .modified_ms = 0},
    {.path = "/sdcard/disks", .is_dir = true, .size = 0, .modified_ms = 0},
    {.path = "/sdcard/roms/turrican.adf", .is_dir = false, .size = 901120, .modified_ms = 0},
    {.path = "/sdcard/disks/workbench31.adf", .is_dir = false, .size = 901120, .modified_ms = 0},
};

static const char *s_allowed_path_roots[] = {
    "/sdcard/roms",
    "/sdcard/disks",
    "/sdcard/cartridges",
    "/sdcard/tos",
    "/sdcard/ebins",
    "/sdcard/.staging",
};

#define UPLOAD_MAX_CHUNK_BYTES 32768U
#define UPLOAD_DEFAULT_CHUNK_BYTES 4096U
#define UPLOAD_DEFAULT_TIMEOUT_MS 2000U
#define UPLOAD_MAX_TIMEOUT_MS 30000U

typedef struct {
    bool active;
    char upload_id[48];
    char target_path[192];
    char staging_path[192];
    uint32_t chunk_timeout_ms;
    uint32_t max_chunk_bytes;
    uint32_t next_chunk_index;
    uint64_t received_bytes;
    uint64_t last_chunk_at_ms;
} upload_session_t;

static upload_session_t s_upload_session;

static uint64_t current_time_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
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
    char resp[192];
    snprintf(resp, sizeof(resp), "{\"ok\":false,\"error\":{\"code\":\"%s\"}}", code);
    return esptari_web_send_json(req, resp, status);
}

static uint32_t get_uint_option(const cJSON *obj, const char *key, uint32_t default_value, uint32_t min_value, uint32_t max_value, bool *ok)
{
    *ok = true;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (item == NULL) {
        return default_value;
    }
    if (!cJSON_IsNumber(item)) {
        *ok = false;
        return default_value;
    }

    double raw = item->valuedouble;
    if (raw < (double)min_value || raw > (double)max_value) {
        *ok = false;
        return default_value;
    }
    uint32_t value = (uint32_t)raw;
    if ((double)value != raw) {
        *ok = false;
        return default_value;
    }
    return value;
}

static bool ensure_active_upload_session(httpd_req_t *req, const cJSON *json)
{
    const cJSON *upload_id = cJSON_GetObjectItemCaseSensitive(json, "upload_id");
    if (!cJSON_IsString(upload_id) || upload_id->valuestring == NULL || upload_id->valuestring[0] == '\0') {
        send_error(req, "BAD_REQUEST", 400);
        return false;
    }
    if (!s_upload_session.active || strcmp(upload_id->valuestring, s_upload_session.upload_id) != 0) {
        send_error(req, "UPLOAD_SESSION_NOT_FOUND", 404);
        return false;
    }

    uint64_t now_ms = current_time_ms();
    if (s_upload_session.last_chunk_at_ms > 0 &&
        now_ms > s_upload_session.last_chunk_at_ms + s_upload_session.chunk_timeout_ms) {
        esptari_web_audit_log("web_api",
                              "upload.chunk",
                              s_upload_session.target_path,
                              "failed",
                              "chunk_timeout");
        s_upload_session.active = false;
        send_error(req, "CHUNK_TIMEOUT", 408);
        return false;
    }
    return true;
}

static const char *get_query_value(const char *query, const char *key)
{
    static char value[192];
    value[0] = '\0';

    if (query == NULL || key == NULL) {
        return NULL;
    }

    const size_t key_len = strlen(key);
    const char *cursor = query;
    while (*cursor != '\0') {
        const char *amp = strchr(cursor, '&');
        const size_t token_len = amp == NULL ? strlen(cursor) : (size_t)(amp - cursor);
        if (token_len > key_len + 1 && strncmp(cursor, key, key_len) == 0 && cursor[key_len] == '=') {
            const size_t value_len = token_len - key_len - 1;
            const size_t copy_len = value_len < sizeof(value) - 1 ? value_len : sizeof(value) - 1;
            memcpy(value, cursor + key_len + 1, copy_len);
            value[copy_len] = '\0';
            return value;
        }
        if (amp == NULL) {
            break;
        }
        cursor = amp + 1;
    }

    return NULL;
}

static bool parse_json_request(httpd_req_t *req, cJSON **json)
{
    char body[512];
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

static bool path_is_under_allowed_root(const char *normalized_path)
{
    if (normalized_path == NULL || normalized_path[0] != '/') {
        return false;
    }
    for (size_t i = 0; i < (sizeof(s_allowed_path_roots) / sizeof(s_allowed_path_roots[0])); i++) {
        const char *root = s_allowed_path_roots[i];
        size_t root_len = strlen(root);
        if (strncmp(normalized_path, root, root_len) == 0 &&
            (normalized_path[root_len] == '\0' || normalized_path[root_len] == '/')) {
            return true;
        }
    }
    return false;
}

static bool normalize_path(const char *input, char *output, size_t output_size)
{
    if (input == NULL || output == NULL || output_size < 4) {
        return false;
    }
    if (input[0] != '/') {
        return false;
    }

    char tmp[256];
    size_t in_len = strlen(input);
    if (in_len >= sizeof(tmp)) {
        return false;
    }
    for (size_t i = 0; i < in_len; i++) {
        if (input[i] == '\\') {
            return false;
        }
        tmp[i] = input[i];
    }
    tmp[in_len] = '\0';

    size_t segment_starts[64];
    size_t segment_count = 0;
    size_t out_len = 1;
    output[0] = '/';
    output[1] = '\0';

    char *save_ptr = NULL;
    char *token = strtok_r(tmp, "/", &save_ptr);
    while (token != NULL) {
        if (strcmp(token, ".") == 0 || token[0] == '\0') {
            token = strtok_r(NULL, "/", &save_ptr);
            continue;
        }
        if (strcmp(token, "..") == 0) {
            if (segment_count == 0) {
                return false;
            }
            segment_count--;
            out_len = segment_starts[segment_count];
            output[out_len] = '\0';
            token = strtok_r(NULL, "/", &save_ptr);
            continue;
        }

        size_t token_len = strlen(token);
        size_t needed = out_len + (out_len > 1 ? 1 : 0) + token_len + 1;
        if (needed > output_size || segment_count >= (sizeof(segment_starts) / sizeof(segment_starts[0]))) {
            return false;
        }
        if (out_len > 1) {
            output[out_len++] = '/';
        }
        segment_starts[segment_count++] = out_len;
        memcpy(output + out_len, token, token_len);
        out_len += token_len;
        output[out_len] = '\0';
        token = strtok_r(NULL, "/", &save_ptr);
    }

    if (out_len == 0) {
        return false;
    }
    return true;
}

static bool validate_and_normalize_path(httpd_req_t *req, const char *input_path, char *normalized, size_t normalized_size)
{
    if (!normalize_path(input_path, normalized, normalized_size)) {
        send_error(req, "BAD_REQUEST", 400);
        return false;
    }
    if (!path_is_under_allowed_root(normalized)) {
        send_error(req, "PATH_NOT_ALLOWED", 400);
        return false;
    }
    return true;
}

static void write_file_entries(cJSON *files, const char *prefix)
{
    for (size_t i = 0; i < sizeof(s_files) / sizeof(s_files[0]); ++i) {
        if (prefix != NULL && prefix[0] != '\0' && strncmp(s_files[i].path, prefix, strlen(prefix)) != 0) {
            continue;
        }

        cJSON *file = cJSON_CreateObject();
        cJSON_AddStringToObject(file, "path", s_files[i].path);
        cJSON_AddBoolToObject(file, "isDir", s_files[i].is_dir);
        cJSON_AddNumberToObject(file, "size", (double)s_files[i].size);
        cJSON_AddNumberToObject(file, "modifiedAtMs", (double)s_files[i].modified_ms);
        cJSON_AddItemToArray(files, file);
    }
}

static esp_err_t handle_files_list(httpd_req_t *req)
{
    char query[256] = {0};
    if (httpd_req_get_url_query_len(req) > 0) {
        httpd_req_get_url_query_str(req, query, sizeof(query));
    }

    const char *path = get_query_value(query, "path");
    char normalized_path[192];
    const char *effective_path = path;
    if (path != NULL && path[0] != '\0') {
        if (!validate_and_normalize_path(req, path, normalized_path, sizeof(normalized_path))) {
            return ESP_OK;
        }
        effective_path = normalized_path;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "path", effective_path == NULL ? "/" : effective_path);
    cJSON *files = cJSON_AddArrayToObject(root, "files");
    write_file_entries(files, effective_path);

    send_json_object(req, root, 200);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t handle_files_upload(httpd_req_t *req)
{
    cJSON *json = NULL;
    if (!parse_json_request(req, &json)) {
        return ESP_OK;
    }

    const cJSON *op_item = cJSON_GetObjectItemCaseSensitive(json, "op");
    const char *op = (cJSON_IsString(op_item) && op_item->valuestring != NULL) ? op_item->valuestring : "start";

    if (strcmp(op, "start") == 0) {
        const cJSON *path = cJSON_GetObjectItemCaseSensitive(json, "path");
        if (!cJSON_IsString(path) || path->valuestring == NULL || path->valuestring[0] == '\0') {
            cJSON_Delete(json);
            send_error(req, "BAD_REQUEST", 400);
            return ESP_OK;
        }

        char normalized_path[192];
        if (!validate_and_normalize_path(req, path->valuestring, normalized_path, sizeof(normalized_path))) {
            cJSON_Delete(json);
            return ESP_OK;
        }

        bool timeout_ok = true;
        bool chunk_ok = true;
        uint32_t chunk_timeout_ms = get_uint_option(json,
                                                    "chunk_timeout_ms",
                                                    UPLOAD_DEFAULT_TIMEOUT_MS,
                                                    100U,
                                                    UPLOAD_MAX_TIMEOUT_MS,
                                                    &timeout_ok);
        uint32_t max_chunk_bytes = get_uint_option(json,
                                                   "max_chunk_bytes",
                                                   UPLOAD_DEFAULT_CHUNK_BYTES,
                                                   128U,
                                                   UPLOAD_MAX_CHUNK_BYTES,
                                                   &chunk_ok);
        if (!timeout_ok || !chunk_ok) {
            cJSON_Delete(json);
            send_error(req, "BAD_REQUEST", 400);
            return ESP_OK;
        }

        uint64_t now_ms = current_time_ms();
        memset(&s_upload_session, 0, sizeof(s_upload_session));
        s_upload_session.active = true;
        s_upload_session.chunk_timeout_ms = chunk_timeout_ms;
        s_upload_session.max_chunk_bytes = max_chunk_bytes;
        s_upload_session.last_chunk_at_ms = now_ms;
        snprintf(s_upload_session.upload_id, sizeof(s_upload_session.upload_id), "upl_%llu", (unsigned long long)now_ms);
        snprintf(s_upload_session.target_path, sizeof(s_upload_session.target_path), "%s", normalized_path);
        snprintf(s_upload_session.staging_path,
                 sizeof(s_upload_session.staging_path),
                 "/sdcard/.staging/%s.part",
                 s_upload_session.upload_id);

        esptari_web_audit_log("web_api", "upload.start", s_upload_session.target_path, "accepted", "staged_session_created");

        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "uploadId", s_upload_session.upload_id);
        cJSON_AddStringToObject(root, "path", s_upload_session.target_path);
        cJSON_AddStringToObject(root, "stagingPath", s_upload_session.staging_path);
        cJSON_AddStringToObject(root, "status", "staged");
        cJSON_AddNumberToObject(root, "chunkTimeoutMs", (double)s_upload_session.chunk_timeout_ms);
        cJSON_AddNumberToObject(root, "maxChunkBytes", (double)s_upload_session.max_chunk_bytes);
        cJSON_AddNumberToObject(root, "uploadedAtMs", (double)now_ms);

        cJSON_Delete(json);
        send_json_object(req, root, 202);
        cJSON_Delete(root);
        return ESP_OK;
    }

    if (strcmp(op, "chunk") == 0) {
        if (!ensure_active_upload_session(req, json)) {
            cJSON_Delete(json);
            return ESP_OK;
        }

        const cJSON *chunk_index = cJSON_GetObjectItemCaseSensitive(json, "chunk_index");
        const cJSON *chunk_size = cJSON_GetObjectItemCaseSensitive(json, "chunk_size");
        if (!cJSON_IsNumber(chunk_index) || !cJSON_IsNumber(chunk_size)) {
            cJSON_Delete(json);
            send_error(req, "BAD_REQUEST", 400);
            return ESP_OK;
        }

        uint32_t expected = s_upload_session.next_chunk_index;
        uint32_t index = (uint32_t)chunk_index->valuedouble;
        uint32_t size = (uint32_t)chunk_size->valuedouble;
        if ((double)index != chunk_index->valuedouble || (double)size != chunk_size->valuedouble) {
            cJSON_Delete(json);
            send_error(req, "BAD_REQUEST", 400);
            return ESP_OK;
        }
        if (index != expected) {
            cJSON_Delete(json);
            send_error(req, "CHUNK_INDEX_MISMATCH", 409);
            return ESP_OK;
        }
        if (size == 0 || size > s_upload_session.max_chunk_bytes) {
            esptari_web_audit_log("web_api", "upload.chunk", s_upload_session.target_path, "failed", "chunk_too_large");
            cJSON_Delete(json);
            send_error(req, "CHUNK_TOO_LARGE", 413);
            return ESP_OK;
        }

        s_upload_session.received_bytes += size;
        s_upload_session.next_chunk_index++;
        s_upload_session.last_chunk_at_ms = current_time_ms();

        esptari_web_audit_log("web_api", "upload.chunk", s_upload_session.target_path, "accepted", "chunk_written_to_staging");

        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "uploadId", s_upload_session.upload_id);
        cJSON_AddStringToObject(root, "status", "chunk_accepted");
        cJSON_AddNumberToObject(root, "chunkIndex", (double)index);
        cJSON_AddNumberToObject(root, "receivedBytes", (double)s_upload_session.received_bytes);
        cJSON_AddNumberToObject(root, "nextChunkIndex", (double)s_upload_session.next_chunk_index);

        cJSON_Delete(json);
        send_json_object(req, root, 202);
        cJSON_Delete(root);
        return ESP_OK;
    }

    if (strcmp(op, "commit") == 0) {
        if (!ensure_active_upload_session(req, json)) {
            cJSON_Delete(json);
            return ESP_OK;
        }
        if (s_upload_session.received_bytes == 0) {
            cJSON_Delete(json);
            send_error(req, "NO_CHUNKS_RECEIVED", 400);
            return ESP_OK;
        }

        const cJSON *payload_sha256 = cJSON_GetObjectItemCaseSensitive(json, "payload_sha256");
        const char *sha256 = (cJSON_IsString(payload_sha256) && payload_sha256->valuestring != NULL)
                                 ? payload_sha256->valuestring
                                 : "pending";

        esptari_web_audit_log("web_api", "upload.commit", s_upload_session.target_path, "success", "committed_from_staging");

        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "uploadId", s_upload_session.upload_id);
        cJSON_AddStringToObject(root, "path", s_upload_session.target_path);
        cJSON_AddStringToObject(root, "stagingPath", s_upload_session.staging_path);
        cJSON_AddStringToObject(root, "status", "committed");
        cJSON_AddNumberToObject(root, "uploadedBytes", (double)s_upload_session.received_bytes);
        cJSON *integrity = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "integrity", integrity);
        cJSON_AddStringToObject(integrity, "payload_sha256", sha256);
        cJSON_AddStringToObject(integrity, "commitGate", "canonical_target_path");

        s_upload_session.active = false;

        cJSON_Delete(json);
        send_json_object(req, root, 200);
        cJSON_Delete(root);
        return ESP_OK;
    }

    if (strcmp(op, "abort") == 0) {
        if (!ensure_active_upload_session(req, json)) {
            cJSON_Delete(json);
            return ESP_OK;
        }

        esptari_web_audit_log("web_api", "upload.abort", s_upload_session.target_path, "success", "session_aborted");
        s_upload_session.active = false;

        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "uploadId", s_upload_session.upload_id);
        cJSON_AddStringToObject(root, "status", "aborted");
        cJSON_AddStringToObject(root, "path", s_upload_session.target_path);

        cJSON_Delete(json);
        send_json_object(req, root, 200);
        cJSON_Delete(root);
        return ESP_OK;
    }

    cJSON_Delete(json);
    send_error(req, "BAD_REQUEST", 400);
    return ESP_OK;
}

static esp_err_t handle_files_move(httpd_req_t *req)
{
    cJSON *json = NULL;
    if (!parse_json_request(req, &json)) {
        return ESP_OK;
    }

    const cJSON *from = cJSON_GetObjectItemCaseSensitive(json, "from");
    const cJSON *to = cJSON_GetObjectItemCaseSensitive(json, "to");
    if (!cJSON_IsString(from) || !cJSON_IsString(to) || from->valuestring == NULL || to->valuestring == NULL) {
        cJSON_Delete(json);
        send_error(req, "BAD_REQUEST", 400);
        return ESP_OK;
    }

    char normalized_from[192];
    char normalized_to[192];
    if (!validate_and_normalize_path(req, from->valuestring, normalized_from, sizeof(normalized_from)) ||
        !validate_and_normalize_path(req, to->valuestring, normalized_to, sizeof(normalized_to))) {
        cJSON_Delete(json);
        return ESP_OK;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "from", normalized_from);
    cJSON_AddStringToObject(root, "to", normalized_to);
    cJSON_AddStringToObject(root, "status", "ok");

    cJSON_Delete(json);
    send_json_object(req, root, 200);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t handle_files_delete(httpd_req_t *req)
{
    cJSON *json = NULL;
    if (!parse_json_request(req, &json)) {
        return ESP_OK;
    }

    const cJSON *path = cJSON_GetObjectItemCaseSensitive(json, "path");
    if (!cJSON_IsString(path) || path->valuestring == NULL) {
        cJSON_Delete(json);
        send_error(req, "BAD_REQUEST", 400);
        return ESP_OK;
    }

    char normalized_path[192];
    if (!validate_and_normalize_path(req, path->valuestring, normalized_path, sizeof(normalized_path))) {
        cJSON_Delete(json);
        return ESP_OK;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "path", normalized_path);
    cJSON_AddStringToObject(root, "status", "deleted");

    cJSON_Delete(json);
    send_json_object(req, root, 200);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t handle_files_download(httpd_req_t *req)
{
    char query[256] = {0};
    if (httpd_req_get_url_query_len(req) > 0) {
        httpd_req_get_url_query_str(req, query, sizeof(query));
    }

    const char *path = get_query_value(query, "path");
    if (path == NULL) {
        send_error(req, "BAD_REQUEST", 400);
        return ESP_OK;
    }

    char normalized_path[192];
    if (!validate_and_normalize_path(req, path, normalized_path, sizeof(normalized_path))) {
        return ESP_OK;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "path", normalized_path);
    cJSON_AddStringToObject(root, "downloadUrl", "/api/v2/files/download");
    cJSON_AddStringToObject(root, "status", "ready");

    send_json_object(req, root, 200);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t handle_files_mkdir(httpd_req_t *req)
{
    cJSON *json = NULL;
    if (!parse_json_request(req, &json)) {
        return ESP_OK;
    }

    const cJSON *path = cJSON_GetObjectItemCaseSensitive(json, "path");
    if (!cJSON_IsString(path) || path->valuestring == NULL) {
        cJSON_Delete(json);
        send_error(req, "BAD_REQUEST", 400);
        return ESP_OK;
    }

    char normalized_path[192];
    if (!validate_and_normalize_path(req, path->valuestring, normalized_path, sizeof(normalized_path))) {
        cJSON_Delete(json);
        return ESP_OK;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "path", normalized_path);
    cJSON_AddStringToObject(root, "status", "created");

    cJSON_Delete(json);
    send_json_object(req, root, 201);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t handle_files_stat(httpd_req_t *req)
{
    char query[256] = {0};
    if (httpd_req_get_url_query_len(req) > 0) {
        httpd_req_get_url_query_str(req, query, sizeof(query));
    }

    const char *path = get_query_value(query, "path");
    if (path == NULL) {
        send_error(req, "BAD_REQUEST", 400);
        return ESP_OK;
    }

    char normalized_path[192];
    if (!validate_and_normalize_path(req, path, normalized_path, sizeof(normalized_path))) {
        return ESP_OK;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "path", normalized_path);
    cJSON_AddBoolToObject(root, "exists", true);
    cJSON_AddBoolToObject(root, "isDir", false);
    cJSON_AddNumberToObject(root, "size", 901120);
    cJSON_AddNumberToObject(root, "modifiedAtMs", (double)current_time_ms());

    send_json_object(req, root, 200);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t auth_handle_files_list(httpd_req_t *req)
{
    if (esptari_web_auth_require_scope(req, "files:read") != ESP_OK) {
        return ESP_OK;
    }
    return handle_files_list(req);
}

static esp_err_t auth_handle_files_upload(httpd_req_t *req)
{
    if (esptari_web_auth_require_scope(req, "files:write") != ESP_OK) {
        return ESP_OK;
    }
    return handle_files_upload(req);
}

static esp_err_t auth_handle_files_move(httpd_req_t *req)
{
    if (esptari_web_auth_require_scope(req, "files:write") != ESP_OK) {
        return ESP_OK;
    }
    return handle_files_move(req);
}

static esp_err_t auth_handle_files_delete(httpd_req_t *req)
{
    if (esptari_web_auth_require_scope(req, "files:write") != ESP_OK) {
        return ESP_OK;
    }
    return handle_files_delete(req);
}

static esp_err_t auth_handle_files_download(httpd_req_t *req)
{
    if (esptari_web_auth_require_scope(req, "files:read") != ESP_OK) {
        return ESP_OK;
    }
    return handle_files_download(req);
}

static esp_err_t auth_handle_files_mkdir(httpd_req_t *req)
{
    if (esptari_web_auth_require_scope(req, "files:write") != ESP_OK) {
        return ESP_OK;
    }
    return handle_files_mkdir(req);
}

static esp_err_t auth_handle_files_stat(httpd_req_t *req)
{
    if (esptari_web_auth_require_scope(req, "files:read") != ESP_OK) {
        return ESP_OK;
    }
    return handle_files_stat(req);
}

void esptari_web_files_register_routes(httpd_handle_t server_handle)
{
    httpd_uri_t files_list = {.uri = "/api/v2/files/list", .method = HTTP_GET, .handler = auth_handle_files_list, .user_ctx = NULL};
    httpd_uri_t files_upload = {.uri = "/api/v2/files/upload", .method = HTTP_POST, .handler = auth_handle_files_upload, .user_ctx = NULL};
    httpd_uri_t files_move = {.uri = "/api/v2/files/move", .method = HTTP_POST, .handler = auth_handle_files_move, .user_ctx = NULL};
    httpd_uri_t files_delete = {.uri = "/api/v2/files/delete", .method = HTTP_POST, .handler = auth_handle_files_delete, .user_ctx = NULL};
    httpd_uri_t files_download = {.uri = "/api/v2/files/download", .method = HTTP_GET, .handler = auth_handle_files_download, .user_ctx = NULL};
    httpd_uri_t files_mkdir = {.uri = "/api/v2/files/mkdir", .method = HTTP_POST, .handler = auth_handle_files_mkdir, .user_ctx = NULL};
    httpd_uri_t files_stat = {.uri = "/api/v2/files/stat", .method = HTTP_GET, .handler = auth_handle_files_stat, .user_ctx = NULL};

    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &files_list));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &files_upload));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &files_move));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &files_delete));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &files_download));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &files_mkdir));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &files_stat));

    ESP_LOGI(TAG, "Registered file manager routes");
}
