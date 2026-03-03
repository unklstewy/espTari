#include "esptari_web_catalog_utils.h"

#include <stdio.h>

#include "esptari_web_http_utils.h"

esp_err_t esptari_web_catalog_error(httpd_req_t *req, const char *code, int status_code)
{
    char payload[160];
    snprintf(payload, sizeof(payload), "{\"ok\":false,\"error\":{\"code\":\"%s\"}}", code);
    return esptari_web_send_json(req, payload, status_code);
}

esp_err_t esptari_web_catalog_parse_body_json(httpd_req_t *req,
                                              char *body_buf,
                                              size_t body_buf_size,
                                              cJSON **out_root)
{
    if (esptari_web_read_request_body(req, body_buf, body_buf_size) != ESP_OK) {
        return esptari_web_catalog_error(req, "BAD_REQUEST", 400);
    }

    cJSON *root = cJSON_Parse(body_buf);
    if (root == NULL) {
        return esptari_web_catalog_error(req, "BAD_REQUEST", 400);
    }

    *out_root = root;
    return ESP_OK;
}

esp_err_t esptari_web_catalog_resolve_def(httpd_req_t *req,
                                          const char *uri_pattern,
                                          const catalog_def_t **out_def)
{
    char catalog[32] = {0};
    if (sscanf(req->uri, uri_pattern, catalog) != 1) {
        return esptari_web_catalog_error(req, "BAD_REQUEST", 400);
    }

    const catalog_def_t *def = esptari_web_catalog_find(catalog);
    if (def == NULL) {
        return esptari_web_catalog_error(req, "CATALOG_NOT_FOUND", 404);
    }

    *out_def = def;
    return ESP_OK;
}

esp_err_t esptari_web_catalog_resolve_entry(httpd_req_t *req,
                                            const char *uri_pattern,
                                            const catalog_def_t **out_def,
                                            int *out_entry_index,
                                            char *out_entry_id,
                                            size_t out_entry_id_size)
{
    char catalog[32] = {0};
    if (sscanf(req->uri, uri_pattern, catalog, out_entry_id) != 2) {
        return esptari_web_catalog_error(req, "BAD_REQUEST", 400);
    }

    const catalog_def_t *def = esptari_web_catalog_find(catalog);
    if (def == NULL) {
        return esptari_web_catalog_error(req, "CATALOG_NOT_FOUND", 404);
    }

    int entry_index = esptari_web_catalog_find_entry_index(def, out_entry_id);
    if (entry_index < 0) {
        return esptari_web_catalog_error(req, "CATALOG_ENTRY_NOT_FOUND", 404);
    }

    *out_def = def;
    *out_entry_index = entry_index;
    (void)out_entry_id_size;
    return ESP_OK;
}
