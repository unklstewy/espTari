#pragma once

#include "esp_http_server.h"

esp_err_t esptari_web_catalog_mark_dead_handler(httpd_req_t *req);
esp_err_t esptari_web_catalog_rescan_local_handler(httpd_req_t *req);
