#include "esptari_web_auth.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esptari_web_http_utils.h"

typedef struct {
    const char *token;
    const char *scopes_csv;
} esptari_web_token_scope_t;

static bool s_token_mode_enabled;
static char s_static_token[128] = "esptari-admin-token";

static const esptari_web_token_scope_t s_token_scope_map[] = {
    {.token = "esptari-admin-token", .scopes_csv = "*"},
    {.token = "esptari-files-token", .scopes_csv = "files:read,files:write"},
    {.token = "esptari-ebin-token", .scopes_csv = "ebin:manage,files:read"},
};

static esp_err_t send_auth_error(httpd_req_t *req, const char *code, int status, const char *scope)
{
    char response[320];
    snprintf(response,
             sizeof(response),
             "{\"ok\":false,\"error\":{\"code\":\"%s\",\"category\":\"auth\",\"retryable\":false,\"details\":{\"required_scope\":\"%s\"}}}",
             code,
             scope != NULL ? scope : "");
    return esptari_web_send_json(req, response, status);
}

static const char *lookup_scopes_for_token(const char *token)
{
    for (size_t i = 0; i < (sizeof(s_token_scope_map) / sizeof(s_token_scope_map[0])); i++) {
        if (strcmp(token, s_token_scope_map[i].token) == 0) {
            return s_token_scope_map[i].scopes_csv;
        }
    }
    return NULL;
}

static bool token_has_scope(const char *scopes_csv, const char *required_scope)
{
    if (scopes_csv == NULL || required_scope == NULL || required_scope[0] == '\0') {
        return false;
    }
    if (strcmp(scopes_csv, "*") == 0) {
        return true;
    }

    size_t required_len = strlen(required_scope);
    const char *cursor = scopes_csv;
    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == ',') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        const char *end = strchr(cursor, ',');
        size_t token_len = end == NULL ? strlen(cursor) : (size_t)(end - cursor);
        while (token_len > 0 && cursor[token_len - 1] == ' ') {
            token_len--;
        }
        if (token_len == required_len && strncmp(cursor, required_scope, required_len) == 0) {
            return true;
        }
        if (end == NULL) {
            break;
        }
        cursor = end + 1;
    }
    return false;
}

static bool parse_bearer_token(httpd_req_t *req, char *out_token, size_t out_len)
{
    if (req == NULL || out_token == NULL || out_len == 0) {
        return false;
    }

    char authorization[192];
    if (httpd_req_get_hdr_value_str(req, "Authorization", authorization, sizeof(authorization)) != ESP_OK) {
        return false;
    }
    const char *prefix = "Bearer ";
    size_t prefix_len = strlen(prefix);
    if (strncmp(authorization, prefix, prefix_len) != 0) {
        return false;
    }
    const char *token = authorization + prefix_len;
    if (token[0] == '\0') {
        return false;
    }

    size_t token_len = strlen(token);
    if (token_len >= out_len) {
        return false;
    }
    memcpy(out_token, token, token_len + 1);
    return true;
}

void esptari_web_auth_set_token_mode(bool enabled)
{
    s_token_mode_enabled = enabled;
}

bool esptari_web_auth_token_mode_enabled(void)
{
    return s_token_mode_enabled;
}

void esptari_web_auth_set_static_token(const char *token)
{
    if (token == NULL || token[0] == '\0') {
        return;
    }
    snprintf(s_static_token, sizeof(s_static_token), "%s", token);
}

esp_err_t esptari_web_auth_require_scope(httpd_req_t *req, const char *required_scope)
{
    if (!s_token_mode_enabled) {
        return ESP_OK;
    }

    char bearer_token[160];
    if (!parse_bearer_token(req, bearer_token, sizeof(bearer_token))) {
        send_auth_error(req, "UNAUTHORIZED", 401, required_scope);
        return ESP_ERR_INVALID_STATE;
    }

    if (strcmp(bearer_token, s_static_token) == 0) {
        return ESP_OK;
    }

    const char *scopes_csv = lookup_scopes_for_token(bearer_token);
    if (scopes_csv == NULL || !token_has_scope(scopes_csv, required_scope)) {
        send_auth_error(req, "FORBIDDEN", 403, required_scope);
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}
