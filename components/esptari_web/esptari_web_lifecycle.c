#include "esptari_web_lifecycle.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esptari_core.h"
#include "esptari_web_http_utils.h"

#define send_json esptari_web_send_json
#define read_request_body esptari_web_read_request_body
#define json_get_string esptari_web_json_get_string

static esp_err_t handle_state_change(httpd_req_t *req,
                                     esp_err_t (*op)(void),
                                     const char *guard_id,
                                     const char *endpoint)
{
    esp_err_t err = op();
    if (err == ESP_OK) {
        return send_json(req, "{\"ok\":true}", 200);
    }

    const char *error_code = "INTERNAL_ERROR";
    int status_code = 500;
    const char *effective_guard_id = guard_id;

    if (err == ESP_ERR_INVALID_STATE) {
        error_code = "INVALID_SESSION_STATE";
        status_code = 409;
    } else if (err == ESP_ERR_NOT_FOUND) {
        error_code = "MACHINE_NOT_LOADED";
        status_code = 412;
        effective_guard_id = "G-LOADER-MACHINE-READY";
    }

    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"ok\":false,\"error\":{\"code\":\"%s\",\"details\":{\"guard_id\":\"%s\",\"endpoint\":\"%s\",\"esp_err\":\"%s\"}}}",
             error_code, effective_guard_id, endpoint, esp_err_to_name(err));
    return send_json(req, buf, status_code);
}

static esp_err_t session_handler(httpd_req_t *req)
{
    return handle_state_change(req,
                               esptari_core_start,
                               "G-LIFECYCLE-SESSION",
                               "/api/v2/engine/session");
}

static esp_err_t start_handler(httpd_req_t *req)
{
    return handle_state_change(req,
                               esptari_core_start,
                               "G-LIFECYCLE-START",
                               "/api/v2/engine/session/start");
}

static esp_err_t pause_handler(httpd_req_t *req)
{
    return handle_state_change(req,
                               esptari_core_pause,
                               "G-LIFECYCLE-PAUSE",
                               "/api/v2/engine/session/pause");
}

static esp_err_t resume_handler(httpd_req_t *req)
{
    bool resume_running = true;

    if (req->content_len > 0) {
        char body[256];
        if (read_request_body(req, body, sizeof(body)) != ESP_OK) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }

        cJSON *root = cJSON_Parse(body);
        if (root == NULL) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }

        cJSON *resume_mode_item = cJSON_GetObjectItemCaseSensitive(root, "resume_mode");
        if (resume_mode_item != NULL) {
            if (!cJSON_IsString(resume_mode_item) || resume_mode_item->valuestring == NULL) {
                cJSON_Delete(root);
                return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
            }

            if (strcmp(resume_mode_item->valuestring, "running") == 0) {
                resume_running = true;
            } else if (strcmp(resume_mode_item->valuestring, "paused") == 0) {
                resume_running = false;
            } else {
                cJSON_Delete(root);
                return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
            }
        }

        cJSON_Delete(root);
    }

    esp_err_t err = esptari_core_resume_with_mode(resume_running);
    if (err == ESP_OK) {
        return send_json(req, "{\"ok\":true}", 200);
    }

    if (err == ESP_ERR_INVALID_STATE) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\",\"details\":{\"guard_id\":\"%s\",\"endpoint\":\"%s\",\"esp_err\":\"%s\"}}}",
                 "G-LIFECYCLE-RESUME",
                 "/api/v2/engine/session/resume",
                 esp_err_to_name(err));
        return send_json(req, buf, 409);
    }

    return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
}

static esp_err_t stop_handler(httpd_req_t *req)
{
    return handle_state_change(req,
                               esptari_core_stop,
                               "G-LIFECYCLE-STOP",
                               "/api/v2/engine/session/stop");
}

static esp_err_t reset_handler(httpd_req_t *req)
{
    char reset_mode[8] = "warm";
    bool preserve_media = true;

    if (req->content_len > 0) {
        char body[256];
        if (read_request_body(req, body, sizeof(body)) != ESP_OK) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }

        cJSON *root = cJSON_Parse(body);
        if (root == NULL) {
            return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }

        cJSON *mode_item = cJSON_GetObjectItemCaseSensitive(root, "mode");
        if (mode_item != NULL) {
            if (!cJSON_IsString(mode_item) || mode_item->valuestring == NULL) {
                cJSON_Delete(root);
                return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
            }

            if (strcmp(mode_item->valuestring, "warm") != 0 && strcmp(mode_item->valuestring, "cold") != 0) {
                cJSON_Delete(root);
                return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
            }

            strlcpy(reset_mode, mode_item->valuestring, sizeof(reset_mode));
        }

        cJSON *preserve_media_item = cJSON_GetObjectItemCaseSensitive(root, "preserve_media");
        if (preserve_media_item != NULL) {
            if (!cJSON_IsBool(preserve_media_item)) {
                cJSON_Delete(root);
                return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
            }
            preserve_media = cJSON_IsTrue(preserve_media_item);
        }

        cJSON_Delete(root);
    }

    esp_err_t err = esptari_core_reset();
    if (err == ESP_ERR_INVALID_STATE) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\",\"details\":{\"guard_id\":\"%s\",\"endpoint\":\"%s\",\"esp_err\":\"%s\"}}}",
                 "G-LIFECYCLE-RESET",
                 "/api/v2/engine/session/reset",
                 esp_err_to_name(err));
        return send_json(req, buf, 409);
    }

    if (err != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    esptari_session_status_t status;
    esptari_core_get_status(&status);

    char resp[320];
    snprintf(resp, sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"state\":\"%s\",\"reset_mode\":\"%s\",\"preserve_media\":%s,\"reset_at_us\":%llu}}",
             esptari_core_state_to_string(status.state),
             reset_mode,
             preserve_media ? "true" : "false",
             (unsigned long long)status.last_transition_us);
    return send_json(req, resp, 200);
}

static esp_err_t suspend_save_handler(httpd_req_t *req)
{
    char body[512];
    if (read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const char *snapshot_id = NULL;
    char snapshot_id_copy[128];
    if (!json_get_string(root, "snapshot_id", &snapshot_id)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    strlcpy(snapshot_id_copy, snapshot_id, sizeof(snapshot_id_copy));
    cJSON_Delete(root);

    esp_err_t err = esptari_core_suspend_save(snapshot_id_copy);
    if (err == ESP_ERR_INVALID_STATE) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\"}}", 409);
    }
    if (err == ESP_ERR_INVALID_ARG) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (err != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char resp[256];
    snprintf(resp, sizeof(resp),
             "{\"ok\":true,\"data\":{\"snapshot_id\":\"%s\",\"session_state\":\"suspended\"}}",
             snapshot_id_copy);
    return send_json(req, resp, 200);
}

static esp_err_t restore_resume_handler(httpd_req_t *req)
{
    char body[512];
    if (read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const char *snapshot_id = NULL;
    const char *resume_mode = NULL;
    char snapshot_id_copy[128];
    char resume_mode_copy[16];
    if (!json_get_string(root, "snapshot_id", &snapshot_id) ||
        !json_get_string(root, "resume_mode", &resume_mode)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
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
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    cJSON_Delete(root);

    esp_err_t err = esptari_core_restore_resume(snapshot_id_copy, resume_running);
    if (err == ESP_ERR_INVALID_STATE) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_SUSPENDED\"}}", 409);
    }
    if (err == ESP_ERR_NOT_FOUND) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"SNAPSHOT_NOT_FOUND\"}}", 404);
    }
    if (err == ESP_ERR_INVALID_RESPONSE) {
        const char *rule_id = esptari_core_get_last_failed_compat_rule();
        char incompatible_resp[256];
        snprintf(incompatible_resp,
                 sizeof(incompatible_resp),
                 "{\"ok\":false,\"error\":{\"code\":\"SNAPSHOT_INCOMPATIBLE\",\"details\":{\"rule_id\":\"%s\",\"guard_id\":\"REST-RES-03\"}}}",
                 (rule_id != NULL && rule_id[0] != '\0') ? rule_id : "RCOMP-UNKNOWN");
        return send_json(req, incompatible_resp, 409);
    }
    if (err == ESP_ERR_INVALID_ARG) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (err != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    char resp[320];
    snprintf(resp, sizeof(resp),
             "{\"ok\":true,\"data\":{\"snapshot_id\":\"%s\",\"session_state\":\"%s\"}}",
             snapshot_id_copy,
             resume_running ? "running" : "paused");
    return send_json(req, resp, 200);
}

static esp_err_t restore_validate_handler(httpd_req_t *req)
{
    char body[512];
    if (read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }

    const char *snapshot_id = NULL;
    char snapshot_id_copy[128];
    if (!json_get_string(root, "snapshot_id", &snapshot_id)) {
        cJSON_Delete(root);
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
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
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
    }
    if (err == ESP_ERR_NOT_FOUND) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"SNAPSHOT_NOT_FOUND\"}}", 404);
    }
    if (err == ESP_ERR_INVALID_STATE) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"ENGINE_NOT_RUNNING\"}}", 409);
    }
    if (err == ESP_ERR_INVALID_RESPONSE) {
        const char *rule_id = esptari_core_get_last_failed_compat_rule();
        char incompatible_resp[256];
        snprintf(incompatible_resp,
                 sizeof(incompatible_resp),
                 "{\"ok\":false,\"error\":{\"code\":\"SNAPSHOT_INCOMPATIBLE\",\"details\":{\"rule_id\":\"%s\"}}}",
                 (rule_id != NULL && rule_id[0] != '\0') ? rule_id : "RCOMP-UNKNOWN");
        return send_json(req, incompatible_resp, 409);
    }
    if (err != ESP_OK) {
        return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
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
        return send_json(req, fixed_resp, 200);
    }

    return send_json(req, resp, 200);
}

void esptari_web_lifecycle_register_routes(httpd_handle_t server_handle)
{
    httpd_uri_t session = {.uri = "/api/v2/engine/session", .method = HTTP_POST, .handler = session_handler, .user_ctx = NULL};
    httpd_uri_t start = {.uri = "/api/v2/engine/session/start", .method = HTTP_POST, .handler = start_handler, .user_ctx = NULL};
    httpd_uri_t pause = {.uri = "/api/v2/engine/session/pause", .method = HTTP_POST, .handler = pause_handler, .user_ctx = NULL};
    httpd_uri_t resume = {.uri = "/api/v2/engine/session/resume", .method = HTTP_POST, .handler = resume_handler, .user_ctx = NULL};
    httpd_uri_t stop = {.uri = "/api/v2/engine/session/stop", .method = HTTP_POST, .handler = stop_handler, .user_ctx = NULL};
    httpd_uri_t reset = {.uri = "/api/v2/engine/session/reset", .method = HTTP_POST, .handler = reset_handler, .user_ctx = NULL};
    httpd_uri_t suspend_save = {.uri = "/api/v2/engine/session/suspend-save", .method = HTTP_POST, .handler = suspend_save_handler, .user_ctx = NULL};
    httpd_uri_t restore_resume = {.uri = "/api/v2/engine/session/restore-resume", .method = HTTP_POST, .handler = restore_resume_handler, .user_ctx = NULL};
    httpd_uri_t restore_validate = {.uri = "/api/v2/engine/state/restore/validate", .method = HTTP_POST, .handler = restore_validate_handler, .user_ctx = NULL};

    httpd_register_uri_handler(server_handle, &session);
    httpd_register_uri_handler(server_handle, &start);
    httpd_register_uri_handler(server_handle, &pause);
    httpd_register_uri_handler(server_handle, &resume);
    httpd_register_uri_handler(server_handle, &stop);
    httpd_register_uri_handler(server_handle, &reset);
    httpd_register_uri_handler(server_handle, &suspend_save);
    httpd_register_uri_handler(server_handle, &restore_resume);
    httpd_register_uri_handler(server_handle, &restore_validate);
}
