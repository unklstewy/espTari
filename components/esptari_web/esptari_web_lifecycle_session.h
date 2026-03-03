#pragma once

#include "esp_http_server.h"

esp_err_t esptari_web_lifecycle_session_handler(httpd_req_t *req);
esp_err_t esptari_web_lifecycle_start_handler(httpd_req_t *req);
esp_err_t esptari_web_lifecycle_pause_handler(httpd_req_t *req);
esp_err_t esptari_web_lifecycle_resume_handler(httpd_req_t *req);
esp_err_t esptari_web_lifecycle_stop_handler(httpd_req_t *req);
esp_err_t esptari_web_lifecycle_reset_handler(httpd_req_t *req);
