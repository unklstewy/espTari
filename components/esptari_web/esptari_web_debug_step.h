#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t esptari_web_debug_clock_step_handler(httpd_req_t *req);