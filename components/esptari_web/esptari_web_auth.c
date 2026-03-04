#include "esptari_web_auth.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "esptari_web_http_utils.h"

#ifndef CONFIG_ESPTARI_API_TOKEN_CLIENT_ID
#define CONFIG_ESPTARI_API_TOKEN_CLIENT_ID "esptari-smoke"
#endif

#ifndef CONFIG_ESPTARI_API_TOKEN_CLIENT_SECRET
#define CONFIG_ESPTARI_API_TOKEN_CLIENT_SECRET "esptari-smoke-secret"
#endif

#ifndef CONFIG_ESPTARI_API_TOKEN_DEFAULT_SCOPE
#define CONFIG_ESPTARI_API_TOKEN_DEFAULT_SCOPE "ebin:manage"
#endif

#ifndef CONFIG_ESPTARI_API_TOKEN_TTL_SECONDS
#define CONFIG_ESPTARI_API_TOKEN_TTL_SECONDS 3600
#endif

static const char *TAG = "esptari_web_auth";

#define MAX_TOKENS 16
#define MAX_TOKEN_LEN 96
#define MAX_SCOPE_LEN 96
#define MAX_PROTECTED_ROUTES 160

typedef struct {
    bool in_use;
    char token[MAX_TOKEN_LEN];
    char scope[MAX_SCOPE_LEN];
    uint64_t issued_at_us;
    uint64_t expires_at_us;
} auth_token_entry_t;

typedef struct {
    bool in_use;
    esp_err_t (*handler)(httpd_req_t *);
    char required_scope[MAX_SCOPE_LEN];
} auth_protected_route_entry_t;

static auth_token_entry_t s_tokens[MAX_TOKENS];
static uint64_t s_token_seq = 0;
static auth_protected_route_entry_t s_protected_routes[MAX_PROTECTED_ROUTES];

static esp_err_t send_auth_error(httpd_req_t *req, const char *code, int status, const char *required_scope, const char *granted_scope)
{
    char payload[448];
    if (granted_scope != NULL && granted_scope[0] != '\0') {
        snprintf(payload,
                 sizeof(payload),
                 "{\"ok\":false,\"error\":{\"code\":\"%s\",\"category\":\"auth\",\"retryable\":false,\"details\":{\"required_scope\":\"%s\",\"granted_scope\":\"%s\"}}}",
                 code,
                 required_scope != NULL ? required_scope : "",
                 granted_scope);
    } else {
        snprintf(payload,
                 sizeof(payload),
                 "{\"ok\":false,\"error\":{\"code\":\"%s\",\"category\":\"auth\",\"retryable\":false,\"details\":{\"required_scope\":\"%s\"}}}",
                 code,
                 required_scope != NULL ? required_scope : "");
    }
    return esptari_web_send_json(req, payload, status);
}

static bool parse_bearer_from_header(httpd_req_t *req, char *token_out, size_t token_out_len)
{
    size_t hdr_len = httpd_req_get_hdr_value_len(req, "Authorization");
    if (hdr_len == 0 || hdr_len + 1 >= 192) {
        return false;
    }

    char header[192] = {0};
    if (httpd_req_get_hdr_value_str(req, "Authorization", header, sizeof(header)) != ESP_OK) {
        return false;
    }

    const char *prefix = "Bearer ";
    size_t prefix_len = strlen(prefix);
    if (strncmp(header, prefix, prefix_len) != 0) {
        return false;
    }

    const char *token = header + prefix_len;
    if (token[0] == '\0') {
        return false;
    }

    size_t token_len = strlen(token);
    if (token_len == 0 || token_len >= token_out_len) {
        return false;
    }
    memcpy(token_out, token, token_len + 1);
    return true;
}

static auth_token_entry_t *find_token(const char *token)
{
    if (token == NULL || token[0] == '\0') {
        return NULL;
    }
    for (size_t i = 0; i < MAX_TOKENS; ++i) {
        if (s_tokens[i].in_use && strcmp(s_tokens[i].token, token) == 0) {
            return &s_tokens[i];
        }
    }
    return NULL;
}

static auth_token_entry_t *allocate_token_slot(void)
{
    for (size_t i = 0; i < MAX_TOKENS; ++i) {
        if (!s_tokens[i].in_use) {
            return &s_tokens[i];
        }
    }

    size_t oldest = 0;
    uint64_t oldest_issued = s_tokens[0].issued_at_us;
    for (size_t i = 1; i < MAX_TOKENS; ++i) {
        if (s_tokens[i].issued_at_us < oldest_issued) {
            oldest = i;
            oldest_issued = s_tokens[i].issued_at_us;
        }
    }
    return &s_tokens[oldest];
}

static bool scope_allows(const char *granted_scope, const char *required_scope)
{
    if (required_scope == NULL || required_scope[0] == '\0') {
        return true;
    }
    if (granted_scope == NULL || granted_scope[0] == '\0') {
        return false;
    }

    if (strcmp(granted_scope, required_scope) == 0) {
        return true;
    }

    size_t required_len = strlen(required_scope);
    const char *cursor = granted_scope;
    while (*cursor != '\0') {
        while (*cursor == ' ') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }
        const char *end = strchr(cursor, ' ');
        size_t token_len = end == NULL ? strlen(cursor) : (size_t)(end - cursor);
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

esp_err_t esptari_web_auth_require_scope(httpd_req_t *req, const char *required_scope)
{
#if CONFIG_ESPTARI_API_AUTH_MODE_DEV_OPEN
    (void)req;
    (void)required_scope;
    return ESP_OK;
#else
    char token[MAX_TOKEN_LEN] = {0};
    if (!parse_bearer_from_header(req, token, sizeof(token))) {
        return send_auth_error(req, "UNAUTHORIZED", 401, required_scope, NULL);
    }

    auth_token_entry_t *entry = find_token(token);
    if (entry == NULL) {
        return send_auth_error(req, "UNAUTHORIZED", 401, required_scope, NULL);
    }

    uint64_t now_us = (uint64_t)esp_timer_get_time();
    if (entry->expires_at_us <= now_us) {
        entry->in_use = false;
        return send_auth_error(req, "UNAUTHORIZED", 401, required_scope, NULL);
    }

    if (!scope_allows(entry->scope, required_scope)) {
        return send_auth_error(req, "FORBIDDEN", 403, required_scope, entry->scope);
    }

    return ESP_OK;
#endif
}

static esp_err_t protected_route_dispatcher(httpd_req_t *req)
{
    if (req == NULL || req->user_ctx == NULL) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    auth_protected_route_entry_t *entry = (auth_protected_route_entry_t *)req->user_ctx;
    if (!entry->in_use || entry->handler == NULL) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    if (esptari_web_auth_require_scope(req, entry->required_scope) != ESP_OK) {
        return ESP_OK;
    }

    return entry->handler(req);
}

esp_err_t esptari_web_auth_register_protected_route(httpd_handle_t server_handle,
                                                    const char *uri,
                                                    httpd_method_t method,
                                                    esp_err_t (*handler)(httpd_req_t *),
                                                    const char *required_scope)
{
    if (server_handle == NULL || uri == NULL || handler == NULL || required_scope == NULL || required_scope[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    auth_protected_route_entry_t *slot = NULL;
    for (size_t i = 0; i < MAX_PROTECTED_ROUTES; ++i) {
        if (!s_protected_routes[i].in_use) {
            slot = &s_protected_routes[i];
            break;
        }
    }
    if (slot == NULL) {
        ESP_LOGE(TAG, "No free protected-route slots for %s", uri);
        return ESP_ERR_NO_MEM;
    }

    slot->in_use = true;
    slot->handler = handler;
    snprintf(slot->required_scope, sizeof(slot->required_scope), "%s", required_scope);

    httpd_uri_t route = {
        .uri = uri,
        .method = method,
        .handler = protected_route_dispatcher,
        .user_ctx = slot,
    };

    return httpd_register_uri_handler(server_handle, &route);
}

static esp_err_t handle_auth_token(httpd_req_t *req)
{
#if CONFIG_ESPTARI_API_AUTH_MODE_DEV_OPEN
    return esptari_web_send_json(req,
                                 "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\",\"category\":\"auth\",\"retryable\":false,\"details\":{\"reason\":\"auth_mode_dev_open\"}}}",
                                 400);
#else
    char body[384] = {0};
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return esptari_web_send_json(req,
                                     "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\",\"category\":\"request\",\"retryable\":false}}",
                                     400);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return esptari_web_send_json(req,
                                     "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\",\"category\":\"request\",\"retryable\":false}}",
                                     400);
    }

    const char *grant_type = NULL;
    const char *client_id = NULL;
    const char *client_secret = NULL;
    const char *scope = NULL;

    bool grant_ok = esptari_web_json_get_string(root, "grant_type", &grant_type);
    bool client_id_ok = esptari_web_json_get_string(root, "client_id", &client_id);
    bool client_secret_ok = esptari_web_json_get_string(root, "client_secret", &client_secret);
    bool scope_ok = esptari_web_json_get_string(root, "scope", &scope);

    if (!grant_ok || strcmp(grant_type, "client_credentials") != 0 || !client_id_ok || !client_secret_ok) {
        cJSON_Delete(root);
        return esptari_web_send_json(req,
                                     "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\",\"category\":\"request\",\"retryable\":false}}",
                                     400);
    }

    if (strcmp(client_id, CONFIG_ESPTARI_API_TOKEN_CLIENT_ID) != 0 ||
        strcmp(client_secret, CONFIG_ESPTARI_API_TOKEN_CLIENT_SECRET) != 0) {
        cJSON_Delete(root);
        return send_auth_error(req, "UNAUTHORIZED", 401, CONFIG_ESPTARI_API_TOKEN_DEFAULT_SCOPE, NULL);
    }

    const char *effective_scope = (scope_ok && scope != NULL && scope[0] != '\0') ? scope : CONFIG_ESPTARI_API_TOKEN_DEFAULT_SCOPE;

    auth_token_entry_t *slot = allocate_token_slot();
    uint64_t now_us = (uint64_t)esp_timer_get_time();
    ++s_token_seq;
    snprintf(slot->token, sizeof(slot->token), "tk_%08llx_%08llx", (unsigned long long)(now_us & 0xFFFFFFFFULL), (unsigned long long)(s_token_seq & 0xFFFFFFFFULL));
    snprintf(slot->scope, sizeof(slot->scope), "%s", effective_scope);
    slot->issued_at_us = now_us;
    slot->expires_at_us = now_us + ((uint64_t)CONFIG_ESPTARI_API_TOKEN_TTL_SECONDS * 1000000ULL);
    slot->in_use = true;

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    cJSON_AddStringToObject(data, "access_token", slot->token);
    cJSON_AddStringToObject(data, "token_type", "Bearer");
    cJSON_AddNumberToObject(data, "expires_in", CONFIG_ESPTARI_API_TOKEN_TTL_SECONDS);
    cJSON_AddStringToObject(data, "scope", slot->scope);

    char *json = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    cJSON_Delete(root);
    if (json == NULL) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }

    esp_err_t out = esptari_web_send_json(req, json, 200);
    cJSON_free(json);
    ESP_LOGI(TAG, "Minted API token scope=%s ttl=%d", slot->scope, CONFIG_ESPTARI_API_TOKEN_TTL_SECONDS);
    return out;
#endif
}

esp_err_t esptari_web_auth_register_routes(httpd_handle_t server_handle)
{
#if CONFIG_ESPTARI_API_AUTH_MODE_TOKEN
    httpd_uri_t auth_token = {.uri = "/api/v2/auth/token", .method = HTTP_POST, .handler = handle_auth_token, .user_ctx = NULL};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_handle, &auth_token));
    ESP_LOGI(TAG, "Registered token auth route");
#else
    ESP_LOGI(TAG, "Auth mode dev-open: token route disabled");
#endif
    return ESP_OK;
}
