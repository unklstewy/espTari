#pragma once

#include "esp_http_server.h"

esp_err_t esptari_web_catalog_download_entry_handler(httpd_req_t *req);
esp_err_t esptari_web_catalog_download_missing_handler(httpd_req_t *req);
esp_err_t esptari_web_catalog_probe_links_handler(httpd_req_t *req);
