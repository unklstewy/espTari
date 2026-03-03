#include "esptari_web_lifecycle_session.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esptari_core.h"
#include "esptari_web_http_utils.h"

static esp_err_t handle_state_change(httpd_req_t *req,
                                     esp_err_t (*op)(void),
                                     const char *guard_id,
                                     const char *endpoint)
{
    esp_err_t err = op();
    if (err == ESP_OK) {
        return esptari_web_send_json(req, "{\"ok\":true}", 200);
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
    return esptari_web_send_json(req, buf, status_code);
}

esp_err_t esptari_web_lifecycle_session_handler(httpd_req_t *req)
{
    return handle_state_change(req,
                               esptari_core_start,
                               "G-LIFECYCLE-SESSION",
                               "/api/v2/engine/session");
}

esp_err_t esptari_web_lifecycle_start_handler(httpd_req_t *req)
{
    return handle_state_change(req,
                               esptari_core_start,
                               "G-LIFECYCLE-START",
                               "/api/v2/engine/session/start");
}

esp_err_t esptari_web_lifecycle_pause_handler(httpd_req_t *req)
{
    return handle_state_change(req,
                               esptari_core_pause,
                               "G-LIFECYCLE-PAUSE",
                               "/api/v2/engine/session/pause");
}

esp_err_t esptari_web_lifecycle_resume_handler(httpd_req_t *req)
{
    bool resume_running = true;

    if (req->content_len > 0) {
        char body[256];
        if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
            return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }

        cJSON *root = cJSON_Parse(body);
        if (root == NULL) {
            return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }

        cJSON *resume_mode_item = cJSON_GetObjectItemCaseSensitive(root, "resume_mode");
        if (resume_mode_item != NULL) {
            if (!cJSON_IsString(resume_mode_item) || resume_mode_item->valuestring == NULL) {
                cJSON_Delete(root);
                return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
            }

            if (strcmp(resume_mode_item->valuestring, "running") == 0) {
                resume_running = true;
            } else if (strcmp(resume_mode_item->valuestring, "paused") == 0) {
                resume_running = false;
            } else {
                cJSON_Delete(root);
                return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
            }
        }

        cJSON_Delete(root);
    }

    esp_err_t err = esptari_core_resume_with_mode(resume_running);
    if (err == ESP_OK) {
        return esptari_web_send_json(req, "{\"ok\":true}", 200);
    }

    if (err == ESP_ERR_INVALID_STATE) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "{\"ok\":false,\"error\":{\"code\":\"INVALID_SESSION_STATE\",\"details\":{\"guard_id\":\"%s\",\"endpoint\":\"%s\",\"esp_err\":\"%s\"}}}",
                 "G-LIFECYCLE-RESUME",
                 "/api/v2/engine/session/resume",
                 esp_err_to_name(err));
        return esptari_web_send_json(req, buf, 409);
    }

    return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
}

esp_err_t esptari_web_lifecycle_stop_handler(httpd_req_t *req)
{
    return handle_state_change(req,
                               esptari_core_stop,
                               "G-LIFECYCLE-STOP",
                               "/api/v2/engine/session/stop");
}

esp_err_t esptari_web_lifecycle_reset_handler(httpd_req_t *req)
{
    char reset_mode[8] = "warm";
    bool preserve_media = true;

    if (req->content_len > 0) {
        char body[256];
        if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
            return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }

        cJSON *root = cJSON_Parse(body);
        if (root == NULL) {
            return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
        }

        cJSON *mode_item = cJSON_GetObjectItemCaseSensitive(root, "mode");
        if (mode_item != NULL) {
            if (!cJSON_IsString(mode_item) || mode_item->valuestring == NULL) {
                cJSON_Delete(root);
                return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
            }

            if (strcmp(mode_item->valuestring, "warm") != 0 && strcmp(mode_item->valuestring, "cold") != 0) {
                cJSON_Delete(root);
                return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
            }

            strlcpy(reset_mode, mode_item->valuestring, sizeof(reset_mode));
        }

        cJSON *preserve_media_item = cJSON_GetObjectItemCaseSensitive(root, "preserve_media");
        if (preserve_media_item != NULL) {
            if (!cJSON_IsBool(preserve_media_item)) {
                cJSON_Delete(root);
                return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
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
        return esptari_web_send_json(req, buf, 409);
    }

    if (err != ESP_OK) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
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
    return esptari_web_send_json(req, resp, 200);
}
