#include "esptari_web_mappings_item.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esptari_input.h"
#include "esptari_web_http_utils.h"

#define send_json esptari_web_send_json
#define read_request_body esptari_web_read_request_body
#define json_get_string esptari_web_json_get_string

static const char *MAPPINGS_PREFIX = "/api/v2/input/mappings/";

static char *alloc_json_buf(size_t size)
{
    return (char *)malloc(size);
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

static esp_err_t mapping_not_found(httpd_req_t *req)
{
    return send_json(req,
                     "{\"ok\":false,\"error\":{\"code\":\"INPUT_MAPPING_NOT_FOUND\"}}",
                     404);
}

esp_err_t esptari_web_mappings_get_handler(httpd_req_t *req)
{
    const char *mapping_profile_id = mapping_id_from_uri(req->uri);
    if (mapping_profile_id == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    esptari_input_mapping_t mapping;
    esp_err_t err = esptari_input_mapping_get(mapping_profile_id, &mapping);
    if (err == ESP_ERR_NOT_FOUND) {
        return mapping_not_found(req);
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

esp_err_t esptari_web_mappings_patch_handler(httpd_req_t *req)
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
        return mapping_not_found(req);
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

esp_err_t esptari_web_mappings_delete_handler(httpd_req_t *req)
{
    const char *mapping_profile_id = mapping_id_from_uri(req->uri);
    if (mapping_profile_id == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    esp_err_t err = esptari_input_mapping_delete(mapping_profile_id);
    if (err == ESP_ERR_NOT_FOUND) {
        return mapping_not_found(req);
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
