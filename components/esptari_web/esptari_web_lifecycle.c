#include "esptari_web_lifecycle.h"

#include "esptari_web_lifecycle_session.h"
#include "esptari_web_lifecycle_state.h"

void esptari_web_lifecycle_register_routes(httpd_handle_t server_handle)
{
    httpd_uri_t session = {.uri = "/api/v2/engine/session", .method = HTTP_POST, .handler = esptari_web_lifecycle_session_handler, .user_ctx = NULL};
    httpd_uri_t start = {.uri = "/api/v2/engine/session/start", .method = HTTP_POST, .handler = esptari_web_lifecycle_start_handler, .user_ctx = NULL};
    httpd_uri_t pause = {.uri = "/api/v2/engine/session/pause", .method = HTTP_POST, .handler = esptari_web_lifecycle_pause_handler, .user_ctx = NULL};
    httpd_uri_t resume = {.uri = "/api/v2/engine/session/resume", .method = HTTP_POST, .handler = esptari_web_lifecycle_resume_handler, .user_ctx = NULL};
    httpd_uri_t stop = {.uri = "/api/v2/engine/session/stop", .method = HTTP_POST, .handler = esptari_web_lifecycle_stop_handler, .user_ctx = NULL};
    httpd_uri_t reset = {.uri = "/api/v2/engine/session/reset", .method = HTTP_POST, .handler = esptari_web_lifecycle_reset_handler, .user_ctx = NULL};
    httpd_uri_t suspend_save = {.uri = "/api/v2/engine/session/suspend-save", .method = HTTP_POST, .handler = esptari_web_lifecycle_suspend_save_handler, .user_ctx = NULL};
    httpd_uri_t restore_resume = {.uri = "/api/v2/engine/session/restore-resume", .method = HTTP_POST, .handler = esptari_web_lifecycle_restore_resume_handler, .user_ctx = NULL};
    httpd_uri_t restore_validate = {.uri = "/api/v2/engine/state/restore/validate", .method = HTTP_POST, .handler = esptari_web_lifecycle_restore_validate_handler, .user_ctx = NULL};

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
