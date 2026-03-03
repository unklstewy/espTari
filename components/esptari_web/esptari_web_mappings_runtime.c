#include "esptari_web_mappings_runtime.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esptari_core.h"
#include "esptari_input.h"
#include "esptari_web_http_utils.h"

#define send_json esptari_web_send_json
#define read_request_body esptari_web_read_request_body
#define json_get_string esptari_web_json_get_string

static esp_err_t mapping_not_found(httpd_req_t *req)
{
    return send_json(req,
                     "{\"ok\":false,\"error\":{\"code\":\"INPUT_MAPPING_NOT_FOUND\"}}",
                     404);
}

esp_err_t esptari_web_mappings_active_handler(httpd_req_t *req)
{
    esptari_input_mapping_t mapping;
    esp_err_t err = esptari_input_mapping_get_active(&mapping);
    if (err == ESP_ERR_NOT_FOUND) {
        return mapping_not_found(req);
    }
    if (err != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char resp[2048];
    snprintf(resp, sizeof(resp),
             "{\"ok\":true,\"data\":{\"mapping_profile_id\":\"%s\",\"mapping_profile\":{\"mapping_profile_id\":\"%s\",\"machine\":\"%s\",\"profile\":\"%s\",\"entries\":%s,\"revision\":%lu,\"updated_at_us\":%llu}}}",
             mapping.mapping_profile_id,
             mapping.mapping_profile_id,
             mapping.machine,
             mapping.profile,
             mapping.entries_json,
             (unsigned long)mapping.revision,
             (unsigned long long)mapping.updated_at_us);
    return send_json(req, resp, 200);
}

esp_err_t esptari_web_mappings_apply_handler(httpd_req_t *req)
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
        return mapping_not_found(req);
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
