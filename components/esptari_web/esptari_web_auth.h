#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "esp_http_server.h"

void esptari_web_auth_set_token_mode(bool enabled);
bool esptari_web_auth_token_mode_enabled(void);
void esptari_web_auth_set_static_token(const char *token);
esp_err_t esptari_web_auth_require_scope(httpd_req_t *req, const char *required_scope);
