#include "esptari_web_mappings.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esptari_core.h"
#include "esptari_input.h"

static const char *MAPPINGS_PREFIX = "/api/v2/input/mappings/";

static char *alloc_json_buf(size_t size)
{
    return (char *)malloc(size);
}

static esp_err_t send_json(httpd_req_t *req, const char *json, int status_code)
{
    httpd_resp_set_type(req, "application/json");
    const char *status = "500 Internal Server Error";
    switch (status_code) {
    case 200:
        status = "200 OK";
        break;
    case 201:
        status = "201 Created";
        break;
    case 400:
        status = "400 Bad Request";
        break;
    case 404:
        status = "404 Not Found";
        break;
    case 409:
        status = "409 Conflict";
        break;
    case 412:
        status = "412 Precondition Failed";
        break;
    default:
        break;
    }
    httpd_resp_set_status(req, status);
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t read_request_body(httpd_req_t *req, char *out_buf, size_t out_buf_size)
{
    if (req->content_len <= 0 || (size_t)req->content_len >= out_buf_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    int received = httpd_req_recv(req, out_buf, req->content_len);
    if (received <= 0) {
        return ESP_FAIL;
    }
    out_buf[received] = '\0';
    return ESP_OK;
}

static bool json_get_string(cJSON *root, const char *key, const char **value)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return false;
    }
    *value = item->valuestring;
    return true;
}

static const char *mapping_id_from_uri(const char *uri)
{
    if (strncmp(uri, MAPPINGS_PREFIX, strlen(MAPPINGS_PREFIX)) != 0) {
        return NULL;
    }

    const char *mapping_id = uri + strlen(MAPPINGS_PREFIX);
    if (mapping_id[0] == '\0' || strchr(mapping_id, '/') != NULL) {
        return NULL;
    }

    if (strcmp(mapping_id, "active") == 0 || strcmp(mapping_id, "apply") == 0) {
        return NULL;
    }

    return mapping_id;
}

static esp_err_t mapping_not_found(httpd_req_t *req, const char *endpoint)
{
    (void)endpoint;
    return send_json(req,
                     "{\"ok\":false,\"error\":{\"code\":\"INPUT_MAPPING_NOT_FOUND\"}}",
                     404);
}

static esp_err_t mappings_create_handler(httpd_req_t *req)
{
    char body[1024];
    if (read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const char *mapping_profile_id = NULL;
    const char *machine = NULL;
    const char *profile = NULL;
    cJSON *entries = cJSON_GetObjectItemCaseSensitive(root, "entries");

    if (!json_get_string(root, "mapping_profile_id", &mapping_profile_id) ||
        !json_get_string(root, "machine", &machine) ||
        !json_get_string(root, "profile", &profile)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char *entries_json = NULL;
    if (entries != NULL) {
        entries_json = cJSON_PrintUnformatted(entries);
    }

    esptari_input_mapping_t created;
    esp_err_t err = esptari_input_mapping_create(mapping_profile_id,
                                                  machine,
                                                  profile,
                                                  entries_json,
                                                  &created);
    if (entries_json != NULL) {
        free(entries_json);
    }
    cJSON_Delete(root);

    if (err == ESP_ERR_INVALID_STATE) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CONFLICT\"}}", 409);
    }
    if (err != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char resp[1024];
    snprintf(resp, sizeof(resp),
             "{\"ok\":true,\"data\":{\"mapping_profile_id\":\"%s\",\"machine\":\"%s\",\"profile\":\"%s\",\"revision\":%lu,\"updated_at_us\":%llu}}",
             created.mapping_profile_id,
             created.machine,
             created.profile,
             (unsigned long)created.revision,
             (unsigned long long)created.updated_at_us);
    return send_json(req, resp, 201);
}

static esp_err_t mappings_list_handler(httpd_req_t *req)
{
    char query[128] = {0};
    char machine[32] = {0};
    if (httpd_req_get_url_query_len(req) > 0) {
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
            httpd_query_key_value(query, "machine", machine, sizeof(machine));
        }
    }

    esptari_input_mapping_summary_t items[8];
    int count = esptari_input_mapping_list(machine[0] == '\0' ? NULL : machine,
                                           items,
                                           8);

    char *resp = alloc_json_buf(2048);
    if (resp == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    int offset = snprintf(resp, 2048, "{\"ok\":true,\"data\":{\"items\":[");
    for (int index = 0; index < count && offset < 1920; index++) {
        offset += snprintf(resp + offset,
                           2048 - (size_t)offset,
                           "%s{\"mapping_profile_id\":\"%s\",\"machine\":\"%s\",\"profile\":\"%s\",\"revision\":%lu,\"updated_at_us\":%llu}",
                           index == 0 ? "" : ",",
                           items[index].mapping_profile_id,
                           items[index].machine,
                           items[index].profile,
                           (unsigned long)items[index].revision,
                           (unsigned long long)items[index].updated_at_us);
    }
    snprintf(resp + offset, 2048 - (size_t)offset, "]}}" );
    esp_err_t ret = send_json(req, resp, 200);
    free(resp);
    return ret;
}

static esp_err_t mappings_get_handler(httpd_req_t *req)
{
    const char *mapping_profile_id = mapping_id_from_uri(req->uri);
    if (mapping_profile_id == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    esptari_input_mapping_t mapping;
    esp_err_t err = esptari_input_mapping_get(mapping_profile_id, &mapping);
    if (err == ESP_ERR_NOT_FOUND) {
        return mapping_not_found(req, req->uri);
    }
    if (err != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char *resp = alloc_json_buf(2048);
    if (resp == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }
    snprintf(resp, 2048,
             "{\"ok\":true,\"data\":{\"mapping_profile_id\":\"%s\",\"mapping_profile\":{\"mapping_profile_id\":\"%s\",\"machine\":\"%s\",\"profile\":\"%s\",\"entries\":%s,\"revision\":%lu,\"updated_at_us\":%llu}}}",
             mapping.mapping_profile_id,
             mapping.mapping_profile_id,
             mapping.machine,
             mapping.profile,
             mapping.entries_json,
             (unsigned long)mapping.revision,
             (unsigned long long)mapping.updated_at_us);
    esp_err_t ret = send_json(req, resp, 200);
    free(resp);
    return ret;
}

static esp_err_t mappings_patch_handler(httpd_req_t *req)
{
    const char *mapping_profile_id = mapping_id_from_uri(req->uri);
    if (mapping_profile_id == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    char body[1024];
    if (read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const char *profile = NULL;
    json_get_string(root, "profile", &profile);

    cJSON *entries = cJSON_GetObjectItemCaseSensitive(root, "entries");
    char *entries_json = NULL;
    if (entries != NULL) {
        entries_json = cJSON_PrintUnformatted(entries);
    }

    bool changed = false;
    esptari_input_mapping_t mapping;
    esp_err_t err = esptari_input_mapping_patch(mapping_profile_id,
                                                profile,
                                                entries_json,
                                                &changed,
                                                &mapping);
    if (entries_json != NULL) {
        free(entries_json);
    }
    cJSON_Delete(root);

    if (err == ESP_ERR_NOT_FOUND) {
        return mapping_not_found(req, req->uri);
    }
    if (err != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char resp[1200];
    snprintf(resp, sizeof(resp),
             "{\"ok\":true,\"data\":{\"mapping_profile_id\":\"%s\",\"result\":\"%s\",\"revision\":%lu,\"updated_at_us\":%llu}}",
             mapping.mapping_profile_id,
             changed ? "updated" : "no_op",
             (unsigned long)mapping.revision,
             (unsigned long long)mapping.updated_at_us);
    return send_json(req, resp, 200);
}

static esp_err_t mappings_delete_handler(httpd_req_t *req)
{
    const char *mapping_profile_id = mapping_id_from_uri(req->uri);
    if (mapping_profile_id == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    esp_err_t err = esptari_input_mapping_delete(mapping_profile_id);
    if (err == ESP_ERR_NOT_FOUND) {
        return mapping_not_found(req, req->uri);
    }
    if (err == ESP_ERR_INVALID_STATE) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CONFLICT\"}}", 409);
    }
    if (err != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char resp[256];
    snprintf(resp, sizeof(resp),
             "{\"ok\":true,\"data\":{\"deleted\":\"%s\"}}",
             mapping_profile_id);
    return send_json(req, resp, 200);
}

static esp_err_t mappings_active_handler(httpd_req_t *req)
{
    esptari_input_mapping_t mapping;
    esp_err_t err = esptari_input_mapping_get_active(&mapping);
    if (err == ESP_ERR_NOT_FOUND) {
        return mapping_not_found(req, req->uri);
    }
    if (err != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char *resp = alloc_json_buf(2048);
    if (resp == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }
    snprintf(resp, 2048,
             "{\"ok\":true,\"data\":{\"mapping_profile_id\":\"%s\",\"mapping_profile\":{\"mapping_profile_id\":\"%s\",\"machine\":\"%s\",\"profile\":\"%s\",\"entries\":%s,\"revision\":%lu,\"updated_at_us\":%llu}}}",
             mapping.mapping_profile_id,
             mapping.mapping_profile_id,
             mapping.machine,
             mapping.profile,
             mapping.entries_json,
             (unsigned long)mapping.revision,
             (unsigned long long)mapping.updated_at_us);
    esp_err_t ret = send_json(req, resp, 200);
    free(resp);
    return ret;
}

static esp_err_t mappings_apply_handler(httpd_req_t *req)
{
    esptari_session_status_t session_status;
    esptari_core_get_status(&session_status);
    if (session_status.state != ESPTARI_SESSION_RUNNING) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\"}}", 409);
    }

    char body[1024];
    if (read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const char *mapping_profile_id = NULL;
    if (!json_get_string(root, "mapping_profile_id", &mapping_profile_id)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    bool has_expected_revision = false;
    uint32_t expected_revision = 0;
    cJSON *expected_revision_item = cJSON_GetObjectItemCaseSensitive(root, "expected_revision");
    if (cJSON_IsNumber(expected_revision_item) && expected_revision_item->valueint >= 0) {
        has_expected_revision = true;
        expected_revision = (uint32_t)expected_revision_item->valueint;
    }
    cJSON_Delete(root);

    esptari_input_apply_result_t result;
    esp_err_t err = esptari_input_mapping_apply(mapping_profile_id,
                                                has_expected_revision,
                                                expected_revision,
                                                &result);
    if (err == ESP_ERR_NOT_FOUND) {
        return mapping_not_found(req, req->uri);
    }
    if (err == ESP_ERR_INVALID_STATE) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"CONFLICT\"}}", 409);
    }
    if (err != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char resp[1024];
    snprintf(resp, sizeof(resp),
             "{\"ok\":true,\"data\":{\"result\":\"%s\",\"previous_mapping_profile_id\":\"%s\",\"active_mapping_profile_id\":\"%s\",\"active_mapping_revision\":%lu,\"cutover_tick\":%llu}}",
             result.no_op ? "no_op" : "applied",
             result.previous_mapping_profile_id,
             result.active_mapping_profile_id,
             (unsigned long)result.active_mapping_revision,
             (unsigned long long)result.cutover_tick);
    return send_json(req, resp, 200);
}

void esptari_web_mappings_register_routes(httpd_handle_t server_handle)
{
    httpd_uri_t mappings_create = {.uri = "/api/v2/input/mappings", .method = HTTP_POST, .handler = mappings_create_handler, .user_ctx = NULL};
    httpd_uri_t mappings_list = {.uri = "/api/v2/input/mappings", .method = HTTP_GET, .handler = mappings_list_handler, .user_ctx = NULL};
    httpd_uri_t mappings_item_get = {.uri = "/api/v2/input/mappings/*", .method = HTTP_GET, .handler = mappings_get_handler, .user_ctx = NULL};
    httpd_uri_t mappings_item_patch = {.uri = "/api/v2/input/mappings/*", .method = HTTP_PATCH, .handler = mappings_patch_handler, .user_ctx = NULL};
    httpd_uri_t mappings_item_delete = {.uri = "/api/v2/input/mappings/*", .method = HTTP_DELETE, .handler = mappings_delete_handler, .user_ctx = NULL};
    httpd_uri_t mappings_active = {.uri = "/api/v2/input/mappings/active", .method = HTTP_GET, .handler = mappings_active_handler, .user_ctx = NULL};
    httpd_uri_t mappings_apply = {.uri = "/api/v2/input/mappings/apply", .method = HTTP_POST, .handler = mappings_apply_handler, .user_ctx = NULL};

    httpd_register_uri_handler(server_handle, &mappings_create);
    httpd_register_uri_handler(server_handle, &mappings_list);
    httpd_register_uri_handler(server_handle, &mappings_active);
    httpd_register_uri_handler(server_handle, &mappings_apply);
    httpd_register_uri_handler(server_handle, &mappings_item_get);
    httpd_register_uri_handler(server_handle, &mappings_item_patch);
    httpd_register_uri_handler(server_handle, &mappings_item_delete);
}
