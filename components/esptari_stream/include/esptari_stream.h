#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t esptari_stream_init(httpd_handle_t server);
void esptari_stream_start(void);
