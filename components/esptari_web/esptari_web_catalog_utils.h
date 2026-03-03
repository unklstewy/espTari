#pragma once

#include <stddef.h>

#include "cJSON.h"
#include "esp_http_server.h"
#include "esptari_web_catalog_state.h"

esp_err_t esptari_web_catalog_error(httpd_req_t *req, const char *code, int status_code);
esp_err_t esptari_web_catalog_parse_body_json(httpd_req_t *req,
                                              char *body_buf,
                                              size_t body_buf_size,
                                              cJSON **out_root);
esp_err_t esptari_web_catalog_resolve_def(httpd_req_t *req,
                                          const char *uri_pattern,
                                          const catalog_def_t **out_def);
esp_err_t esptari_web_catalog_resolve_entry(httpd_req_t *req,
                                            const char *uri_pattern,
                                            const catalog_def_t **out_def,
                                            int *out_entry_index,
                                            char *out_entry_id,
                                            size_t out_entry_id_size);
