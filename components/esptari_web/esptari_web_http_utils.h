#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_http_server.h"
#include "cJSON.h"

esp_err_t esptari_web_send_json(httpd_req_t *req, const char *json, int status_code);
esp_err_t esptari_web_read_request_body(httpd_req_t *req, char *out_buf, size_t out_buf_size);
bool esptari_web_query_value(httpd_req_t *req, const char *key, char *out, size_t out_len);
bool esptari_web_parse_u32_str(const char *value, uint32_t *out);
bool esptari_web_json_get_string(cJSON *root, const char *key, const char **value);