/**
 * @file esptari_web.c
 * @brief Web interface — HTTP server, REST API, static file serving
 *
 * Static files are served from /sdcard/www/ when an SD card is present.
 * If a requested file doesn't exist on the SD card (or the card isn't
 * mounted), an embedded fallback landing page is returned.  This lets
 * users customise the web UI by simply dropping files onto the card.
 *
 * SPDX-License-Identifier: MIT
 */
#include "esptari_web.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_chip_info.h"
#include "esp_system.h"
#include "esp_mac.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

static const char *TAG = "esptari_web";
static httpd_handle_t s_server = NULL;

#define WEB_ROOT "/sdcard/www"
#define FILE_BUF_SIZE 2048          /* streaming chunk size */

/* ── Embedded fallback landing page ────────────────────────────────── */

static const char INDEX_HTML[] =
    "<!DOCTYPE html>"
    "<html><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>espTari</title>"
    "<style>"
    "body{font-family:system-ui,-apple-system,sans-serif;background:#1a1a2e;color:#e0e0e0;"
    "margin:0;display:flex;justify-content:center;align-items:center;min-height:100vh}"
    ".card{background:#16213e;border-radius:12px;padding:2rem 3rem;box-shadow:0 4px 24px rgba(0,0,0,.4);"
    "max-width:480px;width:90%%;text-align:center}"
    "h1{color:#0f9b58;margin:0 0 .5rem}h2{color:#8899aa;font-weight:400;font-size:.9rem;margin:0 0 1.5rem}"
    "table{width:100%%;border-collapse:collapse;text-align:left}"
    "td{padding:.35rem .5rem;border-bottom:1px solid #1a1a2e}"
    "td:first-child{color:#8899aa;width:40%%}"
    ".ok{color:#0f9b58}.warn{color:#f5a623}"
    "a{color:#4fc3f7;text-decoration:none}"
    ".hint{margin-top:1.5rem;font-size:.8rem;color:#556;line-height:1.5}"
    "</style></head><body>"
    "<div class='card'>"
    "<h1>&#127918; espTari</h1>"
    "<h2>Atari ST Emulator &middot; ESP32-P4</h2>"
    "<table>"
    "<tr><td>Status</td><td class='ok'>Online</td></tr>"
    "<tr><td>Free heap</td><td id='heap'>—</td></tr>"
    "<tr><td>Free PSRAM</td><td id='psram'>—</td></tr>"
    "<tr><td>Uptime</td><td id='uptime'>—</td></tr>"
    "</table>"
    "<p style='margin-top:1.5rem;font-size:.85rem;color:#556'>"
    "API: <a href='/api/status'>/api/status</a></p>"
    "<p class='hint'>&#128161; Place custom web files in <code>/sdcard/www/</code> "
    "to override this page.</p>"
    "</div>"
    "<script>"
    "async function poll(){try{const r=await fetch('/api/status');const j=await r.json();"
    "document.getElementById('heap').textContent=(j.free_heap/1024).toFixed(0)+' KB';"
    "document.getElementById('psram').textContent=(j.free_psram/1024/1024).toFixed(1)+' MB';"
    "const s=Math.floor(j.uptime_ms/1000);const m=Math.floor(s/60);const h=Math.floor(m/60);"
    "document.getElementById('uptime').textContent="
    "(h?h+'h ':'')+(m%%60)+'m '+(s%%60)+'s';"
    "}catch(e){}}poll();setInterval(poll,3000);"
    "</script></body></html>";

/* ── MIME type lookup ──────────────────────────────────────────────── */

typedef struct { const char *ext; const char *mime; } mime_entry_t;

static const mime_entry_t mime_table[] = {
    { ".html", "text/html" },
    { ".htm",  "text/html" },
    { ".css",  "text/css" },
    { ".js",   "application/javascript" },
    { ".json", "application/json" },
    { ".png",  "image/png" },
    { ".jpg",  "image/jpeg" },
    { ".jpeg", "image/jpeg" },
    { ".gif",  "image/gif" },
    { ".svg",  "image/svg+xml" },
    { ".ico",  "image/x-icon" },
    { ".woff", "font/woff" },
    { ".woff2","font/woff2" },
    { ".ttf",  "font/ttf" },
    { ".wasm", "application/wasm" },
    { ".map",  "application/json" },
    { ".txt",  "text/plain" },
};

static const char *mime_for_path(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (dot) {
        for (size_t i = 0; i < sizeof(mime_table) / sizeof(mime_table[0]); i++) {
            if (strcasecmp(dot, mime_table[i].ext) == 0) {
                return mime_table[i].mime;
            }
        }
    }
    return "application/octet-stream";
}

/* ── File serving helper ───────────────────────────────────────────── */

/**
 * Try to send a file from WEB_ROOT.  Returns ESP_OK if the file was
 * found and sent, ESP_ERR_NOT_FOUND otherwise.
 */
static esp_err_t try_send_file(httpd_req_t *req, const char *rel_path)
{
    /* Reject path-traversal attempts */
    if (strstr(rel_path, "..")) {
        return ESP_ERR_NOT_FOUND;
    }

    char filepath[256];
    int n = snprintf(filepath, sizeof(filepath), "%s%s", WEB_ROOT, rel_path);
    if (n < 0 || (size_t)n >= sizeof(filepath)) {
        return ESP_ERR_NOT_FOUND;
    }

    struct stat st;
    /* If the path points to a directory, try index.html inside it */
    if (stat(filepath, &st) == 0 && S_ISDIR(st.st_mode)) {
        size_t len = strlen(filepath);
        if (len > 0 && filepath[len - 1] != '/') {
            strlcat(filepath, "/", sizeof(filepath));
        }
        strlcat(filepath, "index.html", sizeof(filepath));
    }

    FILE *f = fopen(filepath, "r");
    if (!f) {
        return ESP_ERR_NOT_FOUND;
    }

    /* Determine size for Content-Length (optional but nice) */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    httpd_resp_set_type(req, mime_for_path(filepath));

    /* Stream the file in chunks to keep RAM usage low */
    char *buf = malloc(FILE_BUF_SIZE);
    if (!buf) {
        fclose(f);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_OK;                  /* we consumed the request */
    }

    esp_err_t ret = ESP_OK;
    size_t remaining = (size_t)size;
    while (remaining > 0) {
        size_t to_read = remaining < FILE_BUF_SIZE ? remaining : FILE_BUF_SIZE;
        size_t got = fread(buf, 1, to_read, f);
        if (got == 0) break;
        if (httpd_resp_send_chunk(req, buf, got) != ESP_OK) {
            ret = ESP_FAIL;
            break;
        }
        remaining -= got;
    }
    /* Finish chunked response */
    if (ret == ESP_OK) {
        httpd_resp_send_chunk(req, NULL, 0);
    }

    free(buf);
    fclose(f);

    ESP_LOGD(TAG, "Served %s (%ld bytes)", filepath, size);
    return ESP_OK;
}

/* ── URI handlers ──────────────────────────────────────────────────── */

static esp_err_t static_file_handler(httpd_req_t *req)
{
    const char *uri = req->uri;

    /* Strip query string if present */
    const char *q = strchr(uri, '?');
    char clean_uri[128];
    if (q) {
        size_t len = q - uri;
        if (len >= sizeof(clean_uri)) len = sizeof(clean_uri) - 1;
        memcpy(clean_uri, uri, len);
        clean_uri[len] = '\0';
        uri = clean_uri;
    }

    /* 1. Try exact file on SD card */
    if (try_send_file(req, uri) == ESP_OK) {
        return ESP_OK;
    }

    /* 2. For SPA routing: try /index.html on SD card for non-API paths */
    if (strncmp(uri, "/api/", 5) != 0) {
        if (try_send_file(req, "/index.html") == ESP_OK) {
            return ESP_OK;
        }
    }

    /* 3. Embedded fallback for root */
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, sizeof(INDEX_HTML) - 1);
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    char buf[256];
    int len = snprintf(buf, sizeof(buf),
        "{\"status\":\"ok\","
        "\"free_heap\":%lu,"
        "\"free_psram\":%lu,"
        "\"uptime_ms\":%lld,"
        "\"version\":\"0.1.0\"}",
        (unsigned long)esp_get_free_heap_size(),
        (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        (long long)(esp_log_timestamp())
    );
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, len);
}

static const httpd_uri_t uri_status = {
    .uri       = "/api/status",
    .method    = HTTP_GET,
    .handler   = status_get_handler,
};

/* ── Server lifecycle ──────────────────────────────────────────────── */

esp_err_t esptari_web_init(uint16_t port)
{
    ESP_LOGI(TAG, "Starting web server on port %u", port);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    config.uri_match_fn = httpd_uri_match_wildcard;

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Register API routes first (matched before the wildcard) */
    httpd_register_uri_handler(s_server, &uri_status);

    /* Wildcard catch-all — serves SD card files or embedded fallback */
    const httpd_uri_t uri_catch_all = {
        .uri       = "/*",
        .method    = HTTP_GET,
        .handler   = static_file_handler,
    };
    httpd_register_uri_handler(s_server, &uri_catch_all);

    /* Check if SD card web root exists */
    struct stat st;
    if (stat(WEB_ROOT, &st) == 0 && S_ISDIR(st.st_mode)) {
        ESP_LOGI(TAG, "Serving web UI from SD card (%s)", WEB_ROOT);
    } else {
        ESP_LOGI(TAG, "SD card web root not found — using embedded fallback");
        ESP_LOGI(TAG, "Tip: place web files in %s/ on the SD card", WEB_ROOT);
    }

    /* TODO: Register more API handlers:
     *   POST /api/machine — load machine profile
     *   POST /api/control — start/stop/reset
     *   GET  /api/roms    — list available ROMs
     *   WS   /ws          — streaming endpoint (Phase 3)
     */

    ESP_LOGI(TAG, "Web server started on port %u", port);
    return ESP_OK;
}

void esptari_web_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "Web server stopped");
    }
}

bool esptari_web_is_running(void)
{
    return s_server != NULL;
}
