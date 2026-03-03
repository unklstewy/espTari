#pragma once

#include "esp_http_server.h"

esp_err_t esptari_web_catalogs_list_handler(httpd_req_t *req);
esp_err_t esptari_web_catalogs_router_handler(httpd_req_t *req);
