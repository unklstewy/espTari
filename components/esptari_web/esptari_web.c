#include "esptari_web.h"

#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esptari_core.h"

static const char *TAG = "esptari_web";
static httpd_handle_t server_handle;

static esp_err_t send_json(httpd_req_t *req, const char *json, int status_code)
{
    httpd_resp_set_type(req, "application/json");
    const char *status = "500 Internal Server Error";
    switch (status_code) {
    case 200:
        status = "200 OK";
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

static esp_err_t health_handler(httpd_req_t *req)
{
    return send_json(req,
        "{\"ok\":true,\"data\":{\"service\":\"esptari\",\"health\":\"ok\"}}",
        200);
}

static esp_err_t status_handler(httpd_req_t *req)
{
    esptari_session_status_t status;
    esptari_core_get_status(&status);

    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"ok\":true,\"data\":{\"session_state\":\"%s\",\"transition_count\":%llu,\"last_transition_us\":%llu}}",
             esptari_core_state_to_string(status.state),
             (unsigned long long)status.transition_count,
             (unsigned long long)status.last_transition_us);
    return send_json(req, buf, 200);
}

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

static esp_err_t pause_handler(httpd_req_t *req)
{
    return handle_state_change(req,
                               esptari_core_pause,
                               "G-LIFECYCLE-PAUSE",
                               "/api/v2/engine/session/pause");
}

static esp_err_t resume_handler(httpd_req_t *req)
{
    return handle_state_change(req,
                               esptari_core_resume,
                               "G-LIFECYCLE-RESUME",
                               "/api/v2/engine/session/resume");
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
    return handle_state_change(req,
                               esptari_core_reset,
                               "G-LIFECYCLE-RESET",
                               "/api/v2/engine/session/reset");
}

void esptari_web_init(uint16_t port)
{
    if (server_handle != NULL) {
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;

    if (httpd_start(&server_handle, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start web server");
        server_handle = NULL;
        return;
    }

    httpd_uri_t health = {.uri = "/api/v2/engine/health", .method = HTTP_GET, .handler = health_handler, .user_ctx = NULL};
    httpd_uri_t status = {.uri = "/api/v2/engine/status", .method = HTTP_GET, .handler = status_handler, .user_ctx = NULL};
    httpd_uri_t session = {.uri = "/api/v2/engine/session", .method = HTTP_POST, .handler = session_handler, .user_ctx = NULL};
    httpd_uri_t pause = {.uri = "/api/v2/engine/session/pause", .method = HTTP_POST, .handler = pause_handler, .user_ctx = NULL};
    httpd_uri_t resume = {.uri = "/api/v2/engine/session/resume", .method = HTTP_POST, .handler = resume_handler, .user_ctx = NULL};
    httpd_uri_t stop = {.uri = "/api/v2/engine/session/stop", .method = HTTP_POST, .handler = stop_handler, .user_ctx = NULL};
    httpd_uri_t reset = {.uri = "/api/v2/engine/session/reset", .method = HTTP_POST, .handler = reset_handler, .user_ctx = NULL};

    httpd_register_uri_handler(server_handle, &health);
    httpd_register_uri_handler(server_handle, &status);
    httpd_register_uri_handler(server_handle, &session);
    httpd_register_uri_handler(server_handle, &pause);
    httpd_register_uri_handler(server_handle, &resume);
    httpd_register_uri_handler(server_handle, &stop);
    httpd_register_uri_handler(server_handle, &reset);

    ESP_LOGI(TAG, "Web API ready on port %u", (unsigned)port);
}

bool esptari_web_is_running(void)
{
    return server_handle != NULL;
}

httpd_handle_t esptari_web_get_server(void)
{
    return server_handle;
}

void esptari_web_start_file_server(void)
{
}
