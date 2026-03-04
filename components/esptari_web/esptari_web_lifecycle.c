#include "esptari_web_lifecycle.h"

#include "esptari_web_auth.h"
#include "esptari_web_lifecycle_session.h"
#include "esptari_web_lifecycle_state.h"

void esptari_web_lifecycle_register_routes(httpd_handle_t server_handle)
{
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/engine/session", HTTP_POST, esptari_web_lifecycle_session_handler, "engine:control"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/engine/session/start", HTTP_POST, esptari_web_lifecycle_start_handler, "engine:control"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/engine/session/pause", HTTP_POST, esptari_web_lifecycle_pause_handler, "engine:control"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/engine/session/resume", HTTP_POST, esptari_web_lifecycle_resume_handler, "engine:control"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/engine/session/stop", HTTP_POST, esptari_web_lifecycle_stop_handler, "engine:control"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/engine/session/reset", HTTP_POST, esptari_web_lifecycle_reset_handler, "engine:control"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/engine/session/suspend-save", HTTP_POST, esptari_web_lifecycle_suspend_save_handler, "engine:control"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/engine/session/restore-resume", HTTP_POST, esptari_web_lifecycle_restore_resume_handler, "engine:control"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/engine/state/restore/validate", HTTP_POST, esptari_web_lifecycle_restore_validate_handler, "engine:control"));
}
