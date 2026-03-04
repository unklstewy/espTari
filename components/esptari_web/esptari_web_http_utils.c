#include "esptari_web_http_utils.h"

#include <stdlib.h>

esp_err_t esptari_web_send_json(httpd_req_t *req, const char *json, int status_code)
{
    httpd_resp_set_type(req, "application/json");
    const char *status = "500 Internal Server Error";
    switch (status_code) {
    case 200:
        status = "200 OK";
        break;
    case 201:
        status = "201 Created";
        break;
    case 202:
        status = "202 Accepted";
        break;
    case 400:
        status = "400 Bad Request";
        break;
    case 401:
        status = "401 Unauthorized";
        break;
    case 403:
        status = "403 Forbidden";
        break;
    case 404:
        status = "404 Not Found";
        break;
    case 409:
        status = "409 Conflict";
        break;
    case 412:
        status = "412 Precondition Failed";
        break;
    default:
        break;
    }
    httpd_resp_set_status(req, status);
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

esp_err_t esptari_web_read_request_body(httpd_req_t *req, char *out_buf, size_t out_buf_size)
{
    if (req->content_len <= 0 || (size_t)req->content_len >= out_buf_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    int received = httpd_req_recv(req, out_buf, req->content_len);
    if (received <= 0) {
        return ESP_FAIL;
    }
    out_buf[received] = '\0';
    return ESP_OK;
}

bool esptari_web_query_value(httpd_req_t *req, const char *key, char *out, size_t out_len)
{
    if (httpd_req_get_url_query_len(req) <= 0) {
        return false;
    }

    char query[256] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        return false;
    }
    return httpd_query_key_value(query, key, out, out_len) == ESP_OK;
}

bool esptari_web_parse_u32_str(const char *value, uint32_t *out)
{
    if (value == NULL || value[0] == '\0' || out == NULL) {
        return false;
    }
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);
    if (end == value || *end != '\0') {
        return false;
    }
    *out = (uint32_t)parsed;
    return true;
}

bool esptari_web_json_get_string(cJSON *root, const char *key, const char **value)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return false;
    }
    *value = item->valuestring;
    return true;
}
