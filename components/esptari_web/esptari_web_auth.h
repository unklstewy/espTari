#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t esptari_web_auth_register_routes(httpd_handle_t server_handle);
esp_err_t esptari_web_auth_require_scope(httpd_req_t *req, const char *required_scope);
esp_err_t esptari_web_auth_register_protected_route(httpd_handle_t server_handle,
													const char *uri,
													httpd_method_t method,
													esp_err_t (*handler)(httpd_req_t *),
													const char *required_scope);
