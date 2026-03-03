#pragma once

#include "esp_http_server.h"

esp_err_t esptari_web_mappings_get_handler(httpd_req_t *req);
esp_err_t esptari_web_mappings_patch_handler(httpd_req_t *req);
esp_err_t esptari_web_mappings_delete_handler(httpd_req_t *req);
