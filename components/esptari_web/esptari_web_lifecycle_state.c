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

static esp_err_t send_guard_error(httpd_req_t *req,
                                  int status_code,
                                  const char *code,
                                  const char *category,
                                  bool retryable,
                                  const char *guard_id,
                                  const char *endpoint,
                                  const char *message)
{
    char payload[768];
    snprintf(payload,
             sizeof(payload),
             "{\"ok\":false,\"error\":{\"code\":\"%s\",\"category\":\"%s\",\"message\":\"%s\",\"retryable\":%s,\"details\":{\"guard_id\":\"%s\",\"endpoint\":\"%s\"}}}",
             code,
             category,
             message,
             retryable ? "true" : "false",
             guard_id,
             endpoint);
    return esptari_web_send_json(req, payload, status_code);
}

esp_err_t esptari_web_lifecycle_suspend_save_handler(httpd_req_t *req)
{
    char body[512];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-SUSPEND-01", "/api/v2/engine/session/suspend-save", "Invalid suspend-save request body");
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-SUSPEND-01", "/api/v2/engine/session/suspend-save", "Malformed suspend-save JSON");
    }

    const char *snapshot_id = NULL;
    char snapshot_id_copy[128];
    if (!esptari_web_json_get_string(root, "snapshot_id", &snapshot_id)) {
        cJSON_Delete(root);
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-SUSPEND-01", "/api/v2/engine/session/suspend-save", "snapshot_id is required");
    }
    strlcpy(snapshot_id_copy, snapshot_id, sizeof(snapshot_id_copy));
    cJSON_Delete(root);

    esp_err_t err = esptari_core_suspend_save(snapshot_id_copy);
    if (err == ESP_ERR_INVALID_STATE) {
        return send_guard_error(req, 409, "INVALID_SESSION_STATE", "engine", false, "G-SUSPEND-01", "/api/v2/engine/session/suspend-save", "Suspend-save allowed only from running state");
    }
    if (err == ESP_ERR_INVALID_ARG) {
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-SUSPEND-01", "/api/v2/engine/session/suspend-save", "Invalid suspend-save arguments");
    }
    if (err != ESP_OK) {
        return send_guard_error(req, 500, "INTERNAL_ERROR", "internal", false, "G-SUSPEND-01", "/api/v2/engine/session/suspend-save", "Unhandled suspend-save failure");
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
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-RESTORE-01", "/api/v2/engine/session/restore-resume", "Invalid restore-resume request body");
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-RESTORE-01", "/api/v2/engine/session/restore-resume", "Malformed restore-resume JSON");
    }

    const char *snapshot_id = NULL;
    const char *resume_mode = NULL;
    char snapshot_id_copy[128];
    char resume_mode_copy[16];
    if (!esptari_web_json_get_string(root, "snapshot_id", &snapshot_id) ||
        !esptari_web_json_get_string(root, "resume_mode", &resume_mode)) {
        cJSON_Delete(root);
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-RESTORE-01", "/api/v2/engine/session/restore-resume", "snapshot_id and resume_mode are required");
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
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-RESUME-02", "/api/v2/engine/session/restore-resume", "Invalid resume_mode value");
    }
    cJSON_Delete(root);

    esp_err_t err = esptari_core_restore_resume(snapshot_id_copy, resume_running);
    if (err == ESP_ERR_INVALID_STATE) {
        return send_guard_error(req, 409, "ENGINE_NOT_SUSPENDED", "engine", false, "G-RESTORE-01", "/api/v2/engine/session/restore-resume", "Restore-resume requires suspended state");
    }
    if (err == ESP_ERR_NOT_FOUND) {
        return send_guard_error(req, 404, "SNAPSHOT_NOT_FOUND", "snapshot", false, "G-RESTORE-01", "/api/v2/engine/session/restore-resume", "Requested snapshot was not found");
    }
    if (err == ESP_ERR_INVALID_RESPONSE) {
        const char *rule_id = esptari_core_get_last_failed_compat_rule();
        char incompatible_resp[512];
        snprintf(incompatible_resp,
                 sizeof(incompatible_resp),
                 "{\"ok\":false,\"error\":{\"code\":\"SNAPSHOT_INCOMPATIBLE\",\"category\":\"snapshot\",\"message\":\"Snapshot compatibility validation failed\",\"retryable\":false,\"details\":{\"rule_id\":\"%s\",\"guard_id\":\"G-RESTORE-01\",\"endpoint\":\"/api/v2/engine/session/restore-resume\"}}}",
                 (rule_id != NULL && rule_id[0] != '\0') ? rule_id : "RCOMP-UNKNOWN");
        return esptari_web_send_json(req, incompatible_resp, 409);
    }
    if (err == ESP_ERR_INVALID_ARG) {
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-RESTORE-01", "/api/v2/engine/session/restore-resume", "Invalid restore-resume arguments");
    }
    if (err != ESP_OK) {
        return send_guard_error(req, 500, "INTERNAL_ERROR", "internal", false, "G-RESTORE-01", "/api/v2/engine/session/restore-resume", "Unhandled restore-resume failure");
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
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-RESTORE-01", "/api/v2/engine/state/restore/validate", "Invalid restore-validate request body");
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-RESTORE-01", "/api/v2/engine/state/restore/validate", "Malformed restore-validate JSON");
    }

    const char *snapshot_id = NULL;
    char snapshot_id_copy[128];
    if (!esptari_web_json_get_string(root, "snapshot_id", &snapshot_id)) {
        cJSON_Delete(root);
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-RESTORE-01", "/api/v2/engine/state/restore/validate", "snapshot_id is required");
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
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-RESTORE-01", "/api/v2/engine/state/restore/validate", "Invalid restore-validate arguments");
    }
    if (err == ESP_ERR_NOT_FOUND) {
        return send_guard_error(req, 404, "SNAPSHOT_NOT_FOUND", "snapshot", false, "G-RESTORE-01", "/api/v2/engine/state/restore/validate", "Requested snapshot was not found");
    }
    if (err == ESP_ERR_INVALID_STATE) {
        return send_guard_error(req, 409, "ENGINE_NOT_RUNNING", "engine", true, "G-RESTORE-01", "/api/v2/engine/state/restore/validate", "No active session for restore validation");
    }
    if (err == ESP_ERR_INVALID_RESPONSE) {
        const char *rule_id = esptari_core_get_last_failed_compat_rule();
        char incompatible_resp[512];
        snprintf(incompatible_resp,
                 sizeof(incompatible_resp),
                 "{\"ok\":false,\"error\":{\"code\":\"SNAPSHOT_INCOMPATIBLE\",\"category\":\"snapshot\",\"message\":\"Snapshot compatibility validation failed\",\"retryable\":false,\"details\":{\"rule_id\":\"%s\",\"guard_id\":\"G-RESTORE-01\",\"endpoint\":\"/api/v2/engine/state/restore/validate\"}}}",
                 (rule_id != NULL && rule_id[0] != '\0') ? rule_id : "RCOMP-UNKNOWN");
        return esptari_web_send_json(req, incompatible_resp, 409);
    }
    if (err != ESP_OK) {
        return send_guard_error(req, 500, "INTERNAL_ERROR", "internal", false, "G-RESTORE-01", "/api/v2/engine/state/restore/validate", "Unhandled restore-validate failure");
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
