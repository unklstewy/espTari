#include "esptari_web_mappings.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esptari_input.h"
#include "esptari_web_auth.h"
#include "esptari_web_http_utils.h"
#include "esptari_web_mappings_item.h"
#include "esptari_web_mappings_runtime.h"

#define send_json esptari_web_send_json
#define read_request_body esptari_web_read_request_body
#define json_get_string esptari_web_json_get_string

static char *alloc_json_buf(size_t size)
{
    return (char *)malloc(size);
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

void esptari_web_mappings_register_routes(httpd_handle_t server_handle)
{
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/input/mappings", HTTP_POST, mappings_create_handler, "input:write"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/input/mappings", HTTP_GET, mappings_list_handler, "input:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/input/mappings/active", HTTP_GET, esptari_web_mappings_active_handler, "input:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/input/mappings/apply", HTTP_POST, esptari_web_mappings_apply_handler, "input:write"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/input/mappings/*", HTTP_GET, esptari_web_mappings_get_handler, "input:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/input/mappings/*", HTTP_PATCH, esptari_web_mappings_patch_handler, "input:write"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/input/mappings/*", HTTP_DELETE, esptari_web_mappings_delete_handler, "input:write"));
}
