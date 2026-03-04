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

static void build_snapshot_id_from_request(const char *name, char *snapshot_id_out, size_t snapshot_id_len)
{
    if (name != NULL && name[0] != '\0') {
        char sanitized[64] = {0};
        size_t cursor = 0;
        for (size_t i = 0; name[i] != '\0' && cursor < sizeof(sanitized) - 1; ++i) {
            char c = name[i];
            if ((c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9')) {
                sanitized[cursor++] = c;
            } else {
                sanitized[cursor++] = '_';
            }
        }
        sanitized[cursor] = '\0';
        snprintf(snapshot_id_out, snapshot_id_len, "snap_%s", sanitized);
        return;
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    snprintf(snapshot_id_out, snapshot_id_len, "snap_%llu", (unsigned long long)now_us);
}

esp_err_t esptari_web_lifecycle_suspend_save_handler(httpd_req_t *req)
{
    esptari_session_status_t before_status;
    esptari_core_get_status(&before_status);

    char body[512];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-SUSPEND-01", "/api/v2/engine/session/suspend-save", "Invalid suspend-save request body");
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-SUSPEND-01", "/api/v2/engine/session/suspend-save", "Malformed suspend-save JSON");
    }

    const char *session_id = NULL;
    const char *name = NULL;
    const char *reason = NULL;
    bool auto_resume = false;
    bool include_stream_state = true;

    if (!esptari_web_json_get_string(root, "session_id", &session_id) || session_id == NULL || session_id[0] == '\0') {
        cJSON_Delete(root);
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "SUSP-REQ-01", "/api/v2/engine/session/suspend-save", "session_id is required");
    }

    cJSON *name_item = cJSON_GetObjectItemCaseSensitive(root, "name");
    if (name_item != NULL) {
        if (!cJSON_IsString(name_item) || name_item->valuestring == NULL) {
            cJSON_Delete(root);
            return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "SUSP-REQ-01", "/api/v2/engine/session/suspend-save", "name must be a string");
        }
        name = name_item->valuestring;
        if (strlen(name) >= 64) {
            cJSON_Delete(root);
            return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "SUSP-REQ-01", "/api/v2/engine/session/suspend-save", "name is too long");
        }
    }

    cJSON *reason_item = cJSON_GetObjectItemCaseSensitive(root, "reason");
    if (reason_item != NULL) {
        if (!cJSON_IsString(reason_item) || reason_item->valuestring == NULL) {
            cJSON_Delete(root);
            return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "SUSP-REQ-01", "/api/v2/engine/session/suspend-save", "reason must be a string");
        }
        reason = reason_item->valuestring;
        (void)reason;
    }

    cJSON *auto_resume_item = cJSON_GetObjectItemCaseSensitive(root, "auto_resume");
    if (auto_resume_item != NULL) {
        if (!cJSON_IsBool(auto_resume_item)) {
            cJSON_Delete(root);
            return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "SUSP-REQ-01", "/api/v2/engine/session/suspend-save", "auto_resume must be boolean");
        }
        auto_resume = cJSON_IsTrue(auto_resume_item);
    }

    cJSON *include_stream_state_item = cJSON_GetObjectItemCaseSensitive(root, "include_stream_state");
    if (include_stream_state_item != NULL) {
        if (!cJSON_IsBool(include_stream_state_item)) {
            cJSON_Delete(root);
            return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "SUSP-REQ-01", "/api/v2/engine/session/suspend-save", "include_stream_state must be boolean");
        }
        include_stream_state = cJSON_IsTrue(include_stream_state_item);
    }

    char session_id_copy[32] = {0};
    strlcpy(session_id_copy, session_id, sizeof(session_id_copy));

    char snapshot_id_copy[128] = {0};
    build_snapshot_id_from_request(name, snapshot_id_copy, sizeof(snapshot_id_copy));
    cJSON_Delete(root);

    if (strcmp(session_id_copy, "ses_local") != 0) {
        return send_guard_error(req, 409, "ENGINE_NOT_RUNNING", "engine", true, "SUSP-REQ-01", "/api/v2/engine/session/suspend-save", "Unknown or inactive session_id");
    }

    if (before_status.state != ESPTARI_SESSION_RUNNING) {
        return send_guard_error(req, 409, "INVALID_SESSION_STATE", "engine", false, "SUSP-REQ-01", "/api/v2/engine/session/suspend-save", "suspend-save is allowed only from running state");
    }

    char force_fail_query[8] = {0};
    bool force_save_fail = esptari_web_query_value(req, "force_save_fail", force_fail_query, sizeof(force_fail_query)) &&
                           (strcmp(force_fail_query, "1") == 0 || strcmp(force_fail_query, "true") == 0);

    if (force_save_fail) {
        esptari_session_status_t after_status;
        esptari_core_get_status(&after_status);
        char rollback_error[1024];
        snprintf(rollback_error,
                 sizeof(rollback_error),
                 "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"category\":\"internal\",\"message\":\"Injected suspend-save persistence failure\",\"retryable\":false,\"details\":{\"guard_id\":\"SUSP-REQ-03\",\"endpoint\":\"/api/v2/engine/session/suspend-save\",\"lifecycle_transition\":\"%s->%s\",\"transition_events\":[\"suspend_requested\",\"snapshot_persist_failed\",\"rollback_committed\"],\"rollback_to_state\":\"%s\",\"rollback_state_preserved\":%s}}}",
                 esptari_core_state_to_string(before_status.state),
                 esptari_core_state_to_string(after_status.state),
                 esptari_core_state_to_string(before_status.state),
                 before_status.state == after_status.state ? "true" : "false");
        return esptari_web_send_json(req, rollback_error, 500);
    }

    esp_err_t err = esptari_core_suspend_save(snapshot_id_copy);
    if (err == ESP_ERR_INVALID_STATE) {
        return send_guard_error(req, 409, "INVALID_SESSION_STATE", "engine", false, "SUSP-REQ-01", "/api/v2/engine/session/suspend-save", "suspend-save is allowed only from running state");
    }
    if (err == ESP_ERR_INVALID_ARG) {
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-SUSPEND-01", "/api/v2/engine/session/suspend-save", "Invalid suspend-save arguments");
    }
    if (err != ESP_OK) {
        esptari_session_status_t after_status;
        esptari_core_get_status(&after_status);
        char rollback_error[1024];
        snprintf(rollback_error,
                 sizeof(rollback_error),
                 "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\",\"category\":\"internal\",\"message\":\"Unhandled suspend-save failure\",\"retryable\":false,\"details\":{\"guard_id\":\"SUSP-REQ-03\",\"endpoint\":\"/api/v2/engine/session/suspend-save\",\"lifecycle_transition\":\"%s->%s\",\"transition_events\":[\"suspend_requested\",\"snapshot_persist_failed\",\"rollback_committed\"],\"rollback_to_state\":\"%s\",\"rollback_state_preserved\":%s}}}",
                 esptari_core_state_to_string(before_status.state),
                 esptari_core_state_to_string(after_status.state),
                 esptari_core_state_to_string(before_status.state),
                 before_status.state == after_status.state ? "true" : "false");
        return esptari_web_send_json(req, rollback_error, 500);
    }

    esptari_session_status_t after_status;
    esptari_core_get_status(&after_status);

    char resp[640];
    snprintf(resp, sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"state\":\"suspended\",\"snapshot_id\":\"%s\",\"saved_at_us\":%llu,\"lifecycle_transition\":\"running->suspended\",\"auto_resume\":%s,\"include_stream_state\":%s}}",
             session_id_copy,
             snapshot_id_copy,
             (unsigned long long)after_status.last_transition_us,
             auto_resume ? "true" : "false",
             include_stream_state ? "true" : "false");
    return esptari_web_send_json(req, resp, 200);
}

esp_err_t esptari_web_lifecycle_restore_resume_handler(httpd_req_t *req)
{
    esptari_session_status_t before_status;
    esptari_core_get_status(&before_status);

    char body[512];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-RESTORE-01", "/api/v2/engine/session/restore-resume", "Invalid restore-resume request body");
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-RESTORE-01", "/api/v2/engine/session/restore-resume", "Malformed restore-resume JSON");
    }

    const char *session_id = NULL;
    const char *snapshot_id = NULL;
    const char *resume_mode = NULL;
    const char *reason = NULL;
    char session_id_copy[32];
    char snapshot_id_copy[128];
    char resume_mode_copy[16];
    if (!esptari_web_json_get_string(root, "session_id", &session_id) ||
        !esptari_web_json_get_string(root, "snapshot_id", &snapshot_id) ||
        !esptari_web_json_get_string(root, "resume_mode", &resume_mode)) {
        cJSON_Delete(root);
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "REST-RES-01", "/api/v2/engine/session/restore-resume", "session_id, snapshot_id and resume_mode are required");
    }
    strlcpy(session_id_copy, session_id, sizeof(session_id_copy));
    strlcpy(snapshot_id_copy, snapshot_id, sizeof(snapshot_id_copy));
    strlcpy(resume_mode_copy, resume_mode, sizeof(resume_mode_copy));

    cJSON *reason_item = cJSON_GetObjectItemCaseSensitive(root, "reason");
    if (reason_item != NULL) {
        if (!cJSON_IsString(reason_item) || reason_item->valuestring == NULL) {
            cJSON_Delete(root);
            return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "REST-RES-01", "/api/v2/engine/session/restore-resume", "reason must be a string");
        }
        reason = reason_item->valuestring;
        (void)reason;
    }

    bool resume_running = false;
    if (strcmp(resume_mode_copy, "running") == 0) {
        resume_running = true;
    } else if (strcmp(resume_mode_copy, "paused") == 0) {
        resume_running = false;
    } else {
        cJSON_Delete(root);
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "REST-RES-01", "/api/v2/engine/session/restore-resume", "Invalid resume_mode value");
    }
    cJSON_Delete(root);

    if (strcmp(session_id_copy, "ses_local") != 0) {
        return send_guard_error(req, 409, "ENGINE_NOT_RUNNING", "engine", true, "REST-RES-01", "/api/v2/engine/session/restore-resume", "Unknown or inactive session_id");
    }

    if (before_status.state != ESPTARI_SESSION_SUSPENDED) {
        return send_guard_error(req, 409, "ENGINE_NOT_SUSPENDED", "engine", false, "REST-RES-01", "/api/v2/engine/session/restore-resume", "Restore-resume requires suspended state");
    }

    esp_err_t err = esptari_core_restore_resume(snapshot_id_copy, resume_running);
    if (err == ESP_ERR_INVALID_STATE) {
        return send_guard_error(req, 409, "ENGINE_NOT_SUSPENDED", "engine", false, "REST-RES-01", "/api/v2/engine/session/restore-resume", "Restore-resume requires suspended state");
    }
    if (err == ESP_ERR_NOT_FOUND) {
        return send_guard_error(req, 404, "SNAPSHOT_NOT_FOUND", "snapshot", false, "REST-RES-03", "/api/v2/engine/session/restore-resume", "Requested snapshot was not found");
    }
    if (err == ESP_ERR_INVALID_RESPONSE) {
        const char *rule_id = esptari_core_get_last_failed_compat_rule();
        char incompatible_resp[640];
        snprintf(incompatible_resp,
                 sizeof(incompatible_resp),
                 "{\"ok\":false,\"error\":{\"code\":\"SNAPSHOT_INCOMPATIBLE\",\"category\":\"snapshot\",\"message\":\"Snapshot compatibility validation failed\",\"retryable\":false,\"details\":{\"session_id\":\"%s\",\"snapshot_id\":\"%s\",\"rule_id\":\"%s\",\"guard_id\":\"REST-RES-03\",\"endpoint\":\"/api/v2/engine/session/restore-resume\"}}}",
                 session_id_copy,
                 snapshot_id_copy,
                 (rule_id != NULL && rule_id[0] != '\0') ? rule_id : "RCOMP-UNKNOWN");
        return esptari_web_send_json(req, incompatible_resp, 409);
    }
    if (err == ESP_ERR_INVALID_ARG) {
        return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "REST-RES-01", "/api/v2/engine/session/restore-resume", "Invalid restore-resume arguments");
    }
    if (err != ESP_OK) {
        return send_guard_error(req, 500, "SNAPSHOT_RESTORE_FAILED", "snapshot", false, "REST-RES-03", "/api/v2/engine/session/restore-resume", "Unhandled restore-resume failure");
    }

    esptari_session_status_t after_status;
    esptari_core_get_status(&after_status);

    char resp[512];
    snprintf(resp, sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"%s\",\"snapshot_id\":\"%s\",\"state\":\"%s\",\"restored_at_us\":%llu,\"lifecycle_transition\":\"%s\"}}",
             session_id_copy,
             snapshot_id_copy,
             resume_running ? "running" : "paused",
             (unsigned long long)after_status.last_transition_us,
             resume_running ? "suspended->running" : "suspended->paused");
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
