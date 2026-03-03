#include "esptari_web_lifecycle_state.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esptari_core.h"
#include "esptari_web_http_utils.h"

esp_err_t esptari_web_lifecycle_suspend_save_handler(httpd_req_t *req)
{
    char body[512];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const char *snapshot_id = NULL;
    char snapshot_id_copy[128];
    if (!esptari_web_json_get_string(root, "snapshot_id", &snapshot_id)) {
        cJSON_Delete(root);
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    strlcpy(snapshot_id_copy, snapshot_id, sizeof(snapshot_id_copy));
    cJSON_Delete(root);

    esp_err_t err = esptari_core_suspend_save(snapshot_id_copy);
    if (err == ESP_ERR_INVALID_STATE) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\"}}", 409);
    }
    if (err == ESP_ERR_INVALID_ARG) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (err != ESP_OK) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char resp[256];
    snprintf(resp, sizeof(resp),
             "{\"ok\":true,\"data\":{\"snapshot_id\":\"%s\",\"session_state\":\"suspended\"}}",
             snapshot_id_copy);
    return esptari_web_send_json(req, resp, 200);
}

esp_err_t esptari_web_lifecycle_restore_resume_handler(httpd_req_t *req)
{
    char body[512];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const char *snapshot_id = NULL;
    const char *resume_mode = NULL;
    char snapshot_id_copy[128];
    char resume_mode_copy[16];
    if (!esptari_web_json_get_string(root, "snapshot_id", &snapshot_id) ||
        !esptari_web_json_get_string(root, "resume_mode", &resume_mode)) {
        cJSON_Delete(root);
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    strlcpy(snapshot_id_copy, snapshot_id, sizeof(snapshot_id_copy));
    strlcpy(resume_mode_copy, resume_mode, sizeof(resume_mode_copy));

    bool resume_running = false;
    if (strcmp(resume_mode_copy, "running") == 0) {
        resume_running = true;
    } else if (strcmp(resume_mode_copy, "paused") == 0) {
        resume_running = false;
    } else {
        cJSON_Delete(root);
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    cJSON_Delete(root);

    esp_err_t err = esptari_core_restore_resume(snapshot_id_copy, resume_running);
    if (err == ESP_ERR_INVALID_STATE) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_SUSPENDED\"}}", 409);
    }
    if (err == ESP_ERR_NOT_FOUND) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"SNAPSHOT_NOT_FOUND\"}}", 404);
    }
    if (err == ESP_ERR_INVALID_RESPONSE) {
        const char *rule_id = esptari_core_get_last_failed_compat_rule();
        char incompatible_resp[256];
        snprintf(incompatible_resp,
                 sizeof(incompatible_resp),
                 "{\"ok\":false,\"error\":{\"code\":\"SNAPSHOT_INCOMPATIBLE\",\"details\":{\"rule_id\":\"%s\",\"guard_id\":\"REST-RES-03\"}}}",
                 (rule_id != NULL && rule_id[0] != '\0') ? rule_id : "RCOMP-UNKNOWN");
        return esptari_web_send_json(req, incompatible_resp, 409);
    }
    if (err == ESP_ERR_INVALID_ARG) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (err != ESP_OK) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char resp[320];
    snprintf(resp, sizeof(resp),
             "{\"ok\":true,\"data\":{\"snapshot_id\":\"%s\",\"session_state\":\"%s\"}}",
             snapshot_id_copy,
             resume_running ? "running" : "paused");
    return esptari_web_send_json(req, resp, 200);
}

esp_err_t esptari_web_lifecycle_restore_validate_handler(httpd_req_t *req)
{
    char body[512];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const char *snapshot_id = NULL;
    char snapshot_id_copy[128];
    if (!esptari_web_json_get_string(root, "snapshot_id", &snapshot_id)) {
        cJSON_Delete(root);
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    strlcpy(snapshot_id_copy, snapshot_id, sizeof(snapshot_id_copy));

    bool strict = true;
    cJSON *strict_item = cJSON_GetObjectItemCaseSensitive(root, "strict");
    if (cJSON_IsBool(strict_item)) {
        strict = cJSON_IsTrue(strict_item);
    }
    cJSON_Delete(root);

    bool compatible = false;
    esp_err_t err = esptari_core_validate_restore_compatibility(snapshot_id_copy, strict, &compatible);
    if (err == ESP_ERR_INVALID_ARG) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (err == ESP_ERR_NOT_FOUND) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"SNAPSHOT_NOT_FOUND\"}}", 404);
    }
    if (err == ESP_ERR_INVALID_STATE) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }
    if (err == ESP_ERR_INVALID_RESPONSE) {
        const char *rule_id = esptari_core_get_last_failed_compat_rule();
        char incompatible_resp[256];
        snprintf(incompatible_resp,
                 sizeof(incompatible_resp),
                 "{\"ok\":false,\"error\":{\"code\":\"SNAPSHOT_INCOMPATIBLE\",\"details\":{\"rule_id\":\"%s\"}}}",
                 (rule_id != NULL && rule_id[0] != '\0') ? rule_id : "RCOMP-UNKNOWN");
        return esptari_web_send_json(req, incompatible_resp, 409);
    }
    if (err != ESP_OK) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    const char *failed_rule_id = esptari_core_get_last_failed_compat_rule();
    uint64_t validated_at_us = (uint64_t)esp_timer_get_time();
    char resp[640];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"snapshot_id\":\"%s\",\"compatible\":%s,\"evaluated_rules\":[\"RCOMP-01\",\"RCOMP-02\",\"RCOMP-03\",\"RCOMP-04\"],\"failed_rule_id\":%s,\"error_code\":%s,\"validated_at_us\":%llu}}",
             snapshot_id_copy,
             compatible ? "true" : "false",
             (failed_rule_id != NULL && failed_rule_id[0] != '\0') ? "\"" : "null",
             compatible ? "null" : "\"SNAPSHOT_INCOMPATIBLE\"",
             (unsigned long long)validated_at_us);

    if (failed_rule_id != NULL && failed_rule_id[0] != '\0') {
        char fixed_resp[640];
        snprintf(fixed_resp,
                 sizeof(fixed_resp),
                 "{\"ok\":true,\"data\":{\"snapshot_id\":\"%s\",\"compatible\":%s,\"evaluated_rules\":[\"RCOMP-01\",\"RCOMP-02\",\"RCOMP-03\",\"RCOMP-04\"],\"failed_rule_id\":\"%s\",\"error_code\":%s,\"validated_at_us\":%llu}}",
                 snapshot_id_copy,
                 compatible ? "true" : "false",
                 failed_rule_id,
                 compatible ? "null" : "\"SNAPSHOT_INCOMPATIBLE\"",
                 (unsigned long long)validated_at_us);
        return esptari_web_send_json(req, fixed_resp, 200);
    }

    return esptari_web_send_json(req, resp, 200);
}
