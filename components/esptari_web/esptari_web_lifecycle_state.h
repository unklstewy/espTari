#pragma once

#include "esp_http_server.h"

esp_err_t esptari_web_lifecycle_suspend_save_handler(httpd_req_t *req);
esp_err_t esptari_web_lifecycle_restore_resume_handler(httpd_req_t *req);
esp_err_t esptari_web_lifecycle_restore_validate_handler(httpd_req_t *req);
