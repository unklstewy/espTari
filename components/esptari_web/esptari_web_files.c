#include "esptari_web_files.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "esptari_web_auth.h"
#include "esptari_web_http_utils.h"

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

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "path", path == NULL ? "/" : path);
    cJSON *files = cJSON_AddArrayToObject(root, "files");
    write_file_entries(files, path);

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

    const cJSON *path = cJSON_GetObjectItemCaseSensitive(json, "path");
    if (!cJSON_IsString(path) || path->valuestring == NULL) {
        cJSON_Delete(json);
        send_error(req, "BAD_REQUEST", 400);
        return ESP_OK;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "path", path->valuestring);
    cJSON_AddStringToObject(root, "status", "accepted");
    cJSON_AddNumberToObject(root, "uploadedAtMs", (double)current_time_ms());

    cJSON_Delete(json);
    send_json_object(req, root, 202);
    cJSON_Delete(root);
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

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "from", from->valuestring);
    cJSON_AddStringToObject(root, "to", to->valuestring);
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

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "path", path->valuestring);
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

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "path", path);
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

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "path", path->valuestring);
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

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "path", path);
    cJSON_AddBoolToObject(root, "exists", true);
    cJSON_AddBoolToObject(root, "isDir", false);
    cJSON_AddNumberToObject(root, "size", 901120);
    cJSON_AddNumberToObject(root, "modifiedAtMs", (double)current_time_ms());

    send_json_object(req, root, 200);
    cJSON_Delete(root);
    return ESP_OK;
}

void esptari_web_files_register_routes(httpd_handle_t server_handle)
{
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/files/list", HTTP_GET, handle_files_list, "files:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/files/upload", HTTP_POST, handle_files_upload, "files:write"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/files/move", HTTP_POST, handle_files_move, "files:write"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/files/delete", HTTP_POST, handle_files_delete, "files:write"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/files/download", HTTP_GET, handle_files_download, "files:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/files/mkdir", HTTP_POST, handle_files_mkdir, "files:write"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/files/stat", HTTP_GET, handle_files_stat, "files:read"));

    ESP_LOGI(TAG, "Registered file manager routes");
}
