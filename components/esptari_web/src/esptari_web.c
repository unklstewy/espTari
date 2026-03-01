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
#include "esptari_core.h"
#include "esptari_network.h"
#include "esptari_input.h"
#include "esptari_stream.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_chip_info.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_app_format.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

static const char *TAG = "esptari_web";
static httpd_handle_t s_server = NULL;

#define WEB_ROOT "/sdcard/www"
#define FILE_BUF_SIZE 2048          /* streaming chunk size */
#define STACKTRACE_PATH "/sdcard/logs/stacktrace.txt"

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

/* ── /api/system — emulator state ──────────────────────────────────── */

static const char *state_to_str(esptari_state_t s)
{
    switch (s) {
        case ESPTARI_STATE_STOPPED: return "stopped";
        case ESPTARI_STATE_RUNNING: return "running";
        case ESPTARI_STATE_PAUSED:  return "paused";
        case ESPTARI_STATE_ERROR:   return "error";
        default:                    return "unknown";
    }
}

static esp_err_t system_get_handler(httpd_req_t *req)
{
    esptari_state_t st = esptari_core_get_state();
    char buf[384];
    int len = snprintf(buf, sizeof(buf),
        "{\"state\":\"%s\","
        "\"free_heap\":%lu,"
        "\"total_heap\":%lu,"
        "\"free_psram\":%lu,"
        "\"total_psram\":%lu,"
        "\"min_free_heap\":%lu,"
        "\"uptime_ms\":%lld}",
        state_to_str(st),
        (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        (unsigned long)heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
        (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        (unsigned long)heap_caps_get_total_size(MALLOC_CAP_SPIRAM),
        (unsigned long)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
        (long long)(esp_log_timestamp())
    );
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, len);
}

static const httpd_uri_t uri_system = {
    .uri       = "/api/system",
    .method    = HTTP_GET,
    .handler   = system_get_handler,
};

/* ── POST /api/system — control emulator (start/stop/pause/reset) ──── */

static esp_err_t system_post_handler(httpd_req_t *req)
{
    char body[128];
    int len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_OK;
    }
    body[len] = '\0';

    /* Minimal JSON parse: find "action":"<value>" */
    const char *a = strstr(body, "\"action\"");
    if (!a) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing action"); return ESP_OK; }
    const char *q1 = strchr(a + 8, '"'); /* opening quote after colon */
    if (!q1) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON"); return ESP_OK; }
    q1++;  /* skip quote */
    const char *q2 = strchr(q1, '"');
    if (!q2) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON"); return ESP_OK; }
    char action[16] = {0};
    size_t alen = q2 - q1;
    if (alen >= sizeof(action)) alen = sizeof(action) - 1;
    memcpy(action, q1, alen);

    esp_err_t ret = ESP_OK;
    if (strcmp(action, "start") == 0) {
        ret = esptari_core_start();
    } else if (strcmp(action, "stop") == 0) {
        esptari_core_stop();
    } else if (strcmp(action, "pause") == 0) {
        esptari_core_pause();
    } else if (strcmp(action, "resume") == 0) {
        esptari_core_resume();
    } else if (strcmp(action, "reset") == 0) {
        esptari_core_reset();
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Unknown action");
        return ESP_OK;
    }

    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(ret));
        return ESP_OK;
    }

    /* Return updated state */
    return system_get_handler(req);
}

static const httpd_uri_t uri_system_post = {
    .uri       = "/api/system",
    .method    = HTTP_POST,
    .handler   = system_post_handler,
};

/* ── /api/debug/stacktrace — dump/retrieve CPU stack trace ─────────── */

static esp_err_t debug_stacktrace_post_handler(httpd_req_t *req)
{
    char query[96] = {0};
    uint32_t words = 128U;

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char words_str[16] = {0};
        if (httpd_query_key_value(query, "words", words_str, sizeof(words_str)) == ESP_OK) {
            char *end = NULL;
            unsigned long parsed = strtoul(words_str, &end, 10);
            if (end != words_str && parsed > 0UL) {
                words = (uint32_t)parsed;
            }
        }
    }

    esp_err_t ret = esptari_core_dump_stacktrace(STACKTRACE_PATH, words);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(ret));
        return ESP_OK;
    }

    struct stat st;
    long size = 0;
    if (stat(STACKTRACE_PATH, &st) == 0) {
        size = (long)st.st_size;
    }

    char buf[256];
    int len = snprintf(buf, sizeof(buf),
        "{\"status\":\"ok\",\"path\":\"%s\",\"words\":%lu,\"size\":%ld}",
        STACKTRACE_PATH,
        (unsigned long)words,
        size);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, len);
}

static esp_err_t debug_stacktrace_get_handler(httpd_req_t *req)
{
    FILE *f = fopen(STACKTRACE_PATH, "r");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "No stacktrace available");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "text/plain");
    char *buf = malloc(FILE_BUF_SIZE);
    if (!buf) {
        fclose(f);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_OK;
    }

    esp_err_t ret = ESP_OK;
    while (1) {
        size_t got = fread(buf, 1, FILE_BUF_SIZE, f);
        if (got == 0) {
            break;
        }
        if (httpd_resp_send_chunk(req, buf, got) != ESP_OK) {
            ret = ESP_FAIL;
            break;
        }
    }

    if (ret == ESP_OK) {
        httpd_resp_send_chunk(req, NULL, 0);
    }

    free(buf);
    fclose(f);
    return ESP_OK;
}

static const httpd_uri_t uri_debug_stacktrace_get = {
    .uri       = "/api/debug/stacktrace",
    .method    = HTTP_GET,
    .handler   = debug_stacktrace_get_handler,
};

static const httpd_uri_t uri_debug_stacktrace_post = {
    .uri       = "/api/debug/stacktrace",
    .method    = HTTP_POST,
    .handler   = debug_stacktrace_post_handler,
};

/* ── /api/machines — list machine profiles from SD card ────────────── */

static esp_err_t machines_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");

    DIR *dir = opendir("/sdcard/machines");
    if (!dir) {
        return httpd_resp_send(req, "[]", 2);
    }

    /* Build a JSON array by reading each .json file and emitting it */
    httpd_resp_send_chunk(req, "[", 1);
    bool first = true;
    struct dirent *ent;
    char path[296];        /* /sdcard/machines/ (18) + d_name (255) + NUL */
    char fbuf[512];

    while ((ent = readdir(dir)) != NULL) {
        /* Skip non-json files */
        const char *dot = strrchr(ent->d_name, '.');
        if (!dot || strcasecmp(dot, ".json") != 0) continue;

        snprintf(path, sizeof(path), "/sdcard/machines/%s", ent->d_name);
        FILE *f = fopen(path, "r");
        if (!f) continue;

        size_t n = fread(fbuf, 1, sizeof(fbuf) - 1, f);
        fclose(f);
        if (n == 0) continue;
        fbuf[n] = '\0';

        if (!first) httpd_resp_send_chunk(req, ",", 1);
        httpd_resp_send_chunk(req, fbuf, n);
        first = false;
    }
    closedir(dir);

    httpd_resp_send_chunk(req, "]", 1);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t uri_machines = {
    .uri       = "/api/machines",
    .method    = HTTP_GET,
    .handler   = machines_get_handler,
};

/* ── /api/roms — list ROM/TOS files from SD card ──────────────────── */

static void list_roms_in_dir(httpd_req_t *req, const char *dirpath,
                             const char *category, bool *first)
{
    DIR *dir = opendir(dirpath);
    if (!dir) return;

    struct dirent *ent;
    struct stat st;
    char path[296];        /* dirpath + / + d_name (255) + NUL */
    char json[320];

    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;   /* skip hidden / .gitkeep */

        snprintf(path, sizeof(path), "%s/%s", dirpath, ent->d_name);
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        int len = snprintf(json, sizeof(json),
            "%s{\"name\":\"%s\",\"category\":\"%s\",\"size\":%ld}",
            (*first) ? "" : ",",
            ent->d_name, category, (long)st.st_size);
        httpd_resp_send_chunk(req, json, len);
        *first = false;
    }
    closedir(dir);
}

static esp_err_t roms_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send_chunk(req, "[", 1);

    bool first = true;
    list_roms_in_dir(req, "/sdcard/roms/tos",        "tos",       &first);
    list_roms_in_dir(req, "/sdcard/roms/cartridges",  "cartridge", &first);
    list_roms_in_dir(req, "/sdcard/roms/bios",        "bios",      &first);

    httpd_resp_send_chunk(req, "]", 1);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t uri_roms = {
    .uri       = "/api/roms",
    .method    = HTTP_GET,
    .handler   = roms_get_handler,
};

/* ── /api/network/status — network interface info ─────────────────── */

static const char *if_status_str(esptari_if_status_t s)
{
    switch (s) {
        case ESPTARI_IF_STATUS_DOWN:      return "down";
        case ESPTARI_IF_STATUS_STARTED:   return "started";
        case ESPTARI_IF_STATUS_CONNECTED: return "connected";
        case ESPTARI_IF_STATUS_GOT_IP:    return "got_ip";
        default:                          return "unknown";
    }
}

static esp_err_t network_status_get_handler(httpd_req_t *req)
{
    esptari_net_config_t cfg;
    esptari_if_info_t wifi_info = {0};
    esptari_if_info_t eth_info  = {0};

    esptari_net_get_config(&cfg);
    esptari_net_get_if_info(ESPTARI_IF_WIFI, &wifi_info);
    esptari_net_get_if_info(ESPTARI_IF_ETH,  &eth_info);
    bool connected = esptari_net_is_connected();

    char buf[640];
    int len = snprintf(buf, sizeof(buf),
        "{"
        "\"connected\":%s,"
        "\"hostname\":\"%s\","
        "\"wifi\":{"
            "\"enabled\":%s,"
            "\"status\":\"%s\","
            "\"ip\":\"%s\","
            "\"netmask\":\"%s\","
            "\"gateway\":\"%s\","
            "\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\""
        "},"
        "\"ethernet\":{"
            "\"enabled\":%s,"
            "\"status\":\"%s\","
            "\"ip\":\"%s\","
            "\"netmask\":\"%s\","
            "\"gateway\":\"%s\","
            "\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\""
        "},"
        "\"mdns_enabled\":%s"
        "}",
        connected ? "true" : "false",
        cfg.hostname,
        cfg.wifi_enabled ? "true" : "false",
        if_status_str(wifi_info.status),
        wifi_info.ip, wifi_info.netmask, wifi_info.gateway,
        wifi_info.mac[0], wifi_info.mac[1], wifi_info.mac[2],
        wifi_info.mac[3], wifi_info.mac[4], wifi_info.mac[5],
        cfg.eth_enabled ? "true" : "false",
        if_status_str(eth_info.status),
        eth_info.ip, eth_info.netmask, eth_info.gateway,
        eth_info.mac[0], eth_info.mac[1], eth_info.mac[2],
        eth_info.mac[3], eth_info.mac[4], eth_info.mac[5],
        cfg.mdns_enabled ? "true" : "false"
    );
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, len);
}

static const httpd_uri_t uri_network_status = {
    .uri       = "/api/network/status",
    .method    = HTTP_GET,
    .handler   = network_status_get_handler,
};

/* ── Stream stats endpoint ─────────────────────────────────────────── */

static esp_err_t stream_stats_get_handler(httpd_req_t *req)
{
    esptari_stream_stats_t st;
    esptari_stream_get_stats(&st);

    char buf[256];
    int len = snprintf(buf, sizeof(buf),
        "{\"frames_sent\":%lu,\"audio_chunks_sent\":%lu,"
        "\"bytes_sent\":%llu,\"fps\":%.1f,"
        "\"clients\":%lu,\"dropped_frames\":%lu,"
        "\"encode_time_us\":%lu,\"jpeg_quality\":%u}",
        (unsigned long)st.frames_sent,
        (unsigned long)st.audio_chunks_sent,
        (unsigned long long)st.bytes_sent,
        (double)st.fps,
        (unsigned long)st.clients,
        (unsigned long)st.dropped_frames,
        (unsigned long)st.encode_time_us,
        (unsigned)st.jpeg_quality);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, len);
}

static const httpd_uri_t uri_stream_stats = {
    .uri       = "/api/stream/stats",
    .method    = HTTP_GET,
    .handler   = stream_stats_get_handler,
};

/* ── /api/config — emulation configuration (read/write to SD card) ──── */

#define CONFIG_PATH "/sdcard/config/esptari.json"
#define CONFIG_MAX_SIZE 2048

static esp_err_t config_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    FILE *f = fopen(CONFIG_PATH, "r");
    if (!f) {
        /* Return defaults */
        const char *def =
            "{\"machine\":\"st\","
            "\"display\":{\"resolution\":\"low\",\"crt_effects\":false},"
            "\"audio\":{\"sample_rate\":44100,\"volume\":80},"
            "\"memory\":{\"ram_kb\":1024}}";
        return httpd_resp_send(req, def, strlen(def));
    }
    char *buf = malloc(CONFIG_MAX_SIZE);
    if (!buf) { fclose(f); httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM"); return ESP_OK; }
    size_t n = fread(buf, 1, CONFIG_MAX_SIZE - 1, f);
    fclose(f);
    buf[n] = '\0';
    httpd_resp_send(req, buf, n);
    free(buf);
    return ESP_OK;
}

static const httpd_uri_t uri_config_get = {
    .uri = "/api/config", .method = HTTP_GET, .handler = config_get_handler,
};

static esp_err_t config_put_handler(httpd_req_t *req)
{
    if (req->content_len > CONFIG_MAX_SIZE) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Config too large");
        return ESP_OK;
    }
    char *buf = malloc(req->content_len + 1);
    if (!buf) { httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM"); return ESP_OK; }
    int len = httpd_req_recv(req, buf, req->content_len);
    if (len <= 0) { free(buf); httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body"); return ESP_OK; }
    buf[len] = '\0';

    FILE *f = fopen(CONFIG_PATH, "w");
    if (!f) { free(buf); httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot write config"); return ESP_OK; }
    fwrite(buf, 1, len, f);
    fclose(f);
    free(buf);

    ESP_LOGI(TAG, "Configuration saved (%d bytes)", len);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"ok\":true}", 11);
}

static const httpd_uri_t uri_config_put = {
    .uri = "/api/config", .method = HTTP_PUT, .handler = config_put_handler,
};

/* ── /api/config/machine — get/set active machine ──────────────────── */

static char s_active_machine[32] = "st";  /* default */

static esp_err_t config_machine_get_handler(httpd_req_t *req)
{
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "{\"machine\":\"%s\"}", s_active_machine);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, len);
}

static const httpd_uri_t uri_config_machine_get = {
    .uri = "/api/config/machine", .method = HTTP_GET, .handler = config_machine_get_handler,
};

static esp_err_t config_machine_put_handler(httpd_req_t *req)
{
    char body[128];
    int len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body"); return ESP_OK; }
    body[len] = '\0';

    /* Parse "machine":"<value>" */
    const char *m = strstr(body, "\"machine\"");
    if (!m) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing machine"); return ESP_OK; }
    const char *q1 = strchr(m + 9, '"');
    if (!q1) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad JSON"); return ESP_OK; }
    q1++;
    const char *q2 = strchr(q1, '"');
    if (!q2 || (size_t)(q2 - q1) >= sizeof(s_active_machine)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad machine name");
        return ESP_OK;
    }
    memcpy(s_active_machine, q1, q2 - q1);
    s_active_machine[q2 - q1] = '\0';
    ESP_LOGI(TAG, "Active machine set to '%s'", s_active_machine);

    /* Map to enum and call core */
    esptari_machine_t mach = ESPTARI_MACHINE_ST;
    if (strcmp(s_active_machine, "stfm") == 0) mach = ESPTARI_MACHINE_STFM;
    else if (strcmp(s_active_machine, "mega_st") == 0) mach = ESPTARI_MACHINE_MEGA_ST;
    else if (strcmp(s_active_machine, "ste") == 0) mach = ESPTARI_MACHINE_STE;
    else if (strcmp(s_active_machine, "mega_ste") == 0) mach = ESPTARI_MACHINE_MEGA_STE;
    else if (strcmp(s_active_machine, "tt030") == 0) mach = ESPTARI_MACHINE_TT030;
    else if (strcmp(s_active_machine, "falcon030") == 0) mach = ESPTARI_MACHINE_FALCON030;
    esptari_core_load_machine(mach);

    return config_machine_get_handler(req);
}

static const httpd_uri_t uri_config_machine_put = {
    .uri = "/api/config/machine", .method = HTTP_PUT, .handler = config_machine_put_handler,
};

/* ── /api/disks — list disk images from SD card ────────────────────── */

static void list_disks_in_dir(httpd_req_t *req, const char *dirpath,
                              const char *dtype, bool *first)
{
    DIR *dir = opendir(dirpath);
    if (!dir) return;

    struct dirent *ent;
    struct stat st;
    char path[296];
    char json[384];

    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        snprintf(path, sizeof(path), "%s/%s", dirpath, ent->d_name);
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        int len = snprintf(json, sizeof(json),
            "%s{\"name\":\"%s\",\"type\":\"%s\",\"size\":%ld}",
            (*first) ? "" : ",", ent->d_name, dtype, (long)st.st_size);
        httpd_resp_send_chunk(req, json, len);
        *first = false;
    }
    closedir(dir);
}

static esp_err_t disks_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send_chunk(req, "[", 1);

    bool first = true;
    list_disks_in_dir(req, "/sdcard/disks/floppy",  "floppy", &first);
    list_disks_in_dir(req, "/sdcard/disks/hard",    "hard",   &first);

    httpd_resp_send_chunk(req, "]", 1);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t uri_disks = {
    .uri = "/api/disks", .method = HTTP_GET, .handler = disks_get_handler,
};

/* ── POST /api/disks/mount — mount/eject disk ──────────────────────── */

static char s_floppy_a[128] = "";   /* currently mounted floppy A: */
static char s_floppy_b[128] = "";   /* currently mounted floppy B: */

static esp_err_t disks_mount_handler(httpd_req_t *req)
{
    char body[256];
    int len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body"); return ESP_OK; }
    body[len] = '\0';

    /* Parse drive (0=A, 1=B) and path */
    int drive = 0;
    const char *d = strstr(body, "\"drive\"");
    if (d) {
        const char *colon = strchr(d + 7, ':');
        if (colon) drive = atoi(colon + 1);
    }

    const char *p = strstr(body, "\"path\"");
    char disk_path[128] = "";
    if (p) {
        const char *q1 = strchr(p + 6, '"');
        if (q1) {
            q1++;
            const char *q2 = strchr(q1, '"');
            if (q2 && (size_t)(q2 - q1) < sizeof(disk_path)) {
                memcpy(disk_path, q1, q2 - q1);
                disk_path[q2 - q1] = '\0';
            }
        }
    }

    char *dest = (drive == 1) ? s_floppy_b : s_floppy_a;
    strlcpy(dest, disk_path, 128);
    ESP_LOGI(TAG, "Mounted drive %c: %s", 'A' + drive, disk_path[0] ? disk_path : "(ejected)");

    char resp[256];
    int rlen = snprintf(resp, sizeof(resp),
        "{\"ok\":true,\"drive\":%d,\"path\":\"%s\"}", drive, dest);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, rlen);
}

static const httpd_uri_t uri_disks_mount = {
    .uri = "/api/disks/mount", .method = HTTP_POST, .handler = disks_mount_handler,
};

/* ── /api/network/config — read/write network configuration ────────── */

static esp_err_t network_config_get_handler(httpd_req_t *req)
{
    esptari_net_config_t cfg;
    esptari_net_get_config(&cfg);

    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "{\"wifi_enabled\":%s,\"eth_enabled\":%s,"
        "\"hostname\":\"%s\",\"mdns_enabled\":%s,"
        "\"wifi_ssid\":\"%s\","
        "\"wifi_dhcp\":%s,\"wifi_ip\":\"%s\","
        "\"wifi_netmask\":\"%s\",\"wifi_gateway\":\"%s\","
        "\"eth_dhcp\":%s,\"eth_ip\":\"%s\","
        "\"eth_netmask\":\"%s\",\"eth_gateway\":\"%s\","
        "\"failover_enabled\":%s}",
        cfg.wifi_enabled ? "true" : "false",
        cfg.eth_enabled ? "true" : "false",
        cfg.hostname, cfg.mdns_enabled ? "true" : "false",
        (cfg.wifi_ap_count > 0) ? cfg.wifi_aps[0].ssid : "",
        cfg.wifi_ip.dhcp ? "true" : "false",
        cfg.wifi_ip.ip, cfg.wifi_ip.netmask, cfg.wifi_ip.gateway,
        cfg.eth_ip.dhcp ? "true" : "false",
        cfg.eth_ip.ip, cfg.eth_ip.netmask, cfg.eth_ip.gateway,
        cfg.failover_enabled ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, len);
}

static const httpd_uri_t uri_network_config_get = {
    .uri = "/api/network/config", .method = HTTP_GET, .handler = network_config_get_handler,
};

static esp_err_t network_config_put_handler(httpd_req_t *req)
{
    /* For now, save the raw JSON to the SPIFFS config file */
    if (req->content_len > 1024) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Config too large");
        return ESP_OK;
    }
    char *buf = malloc(req->content_len + 1);
    if (!buf) { httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM"); return ESP_OK; }
    int len = httpd_req_recv(req, buf, req->content_len);
    if (len <= 0) { free(buf); httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty"); return ESP_OK; }
    buf[len] = '\0';

    /* TODO: Parse JSON and call esptari_net_set_config() */
    ESP_LOGI(TAG, "Network config update received (%d bytes) — restart to apply", len);
    free(buf);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"ok\":true,\"note\":\"Restart to apply\"}", 37);
}

static const httpd_uri_t uri_network_config_put = {
    .uri = "/api/network/config", .method = HTTP_PUT, .handler = network_config_put_handler,
};

/* ── /api/network/scan — scan for WiFi networks ───────────────────── */

static esp_err_t network_scan_get_handler(httpd_req_t *req)
{
    /* WiFi scan — returns list of visible APs */
    /* The ESP32-C6 co-processor handles WiFi. For now scan is not
       directly accessible through esp_wifi (remote), so return
       a status message indicating this. Real implementation would
       call esp_wifi_scan_start / esp_wifi_scan_get_ap_records. */
    httpd_resp_set_type(req, "application/json");
    const char *msg = "{\"supported\":false,\"note\":\"WiFi scan via ESP32-C6 co-processor not yet implemented\",\"networks\":[]}";
    return httpd_resp_send(req, msg, strlen(msg));
}

static const httpd_uri_t uri_network_scan = {
    .uri = "/api/network/scan", .method = HTTP_GET, .handler = network_scan_get_handler,
};

/* ── /api/ota — firmware update management ─────────────────────────── */

static esp_err_t ota_status_get_handler(httpd_req_t *req)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *update = esp_ota_get_next_update_partition(NULL);
    const esp_app_desc_t *app = esp_app_get_description();

    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "{\"version\":\"%s\","
        "\"idf_version\":\"%s\","
        "\"compile_date\":\"%s\","
        "\"compile_time\":\"%s\","
        "\"running_partition\":\"%s\","
        "\"next_update_partition\":\"%s\","
        "\"secure_version\":%lu}",
        app->version, app->idf_ver,
        app->date, app->time,
        running ? running->label : "unknown",
        update ? update->label : "none",
        (unsigned long)app->secure_version);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, len);
}

static const httpd_uri_t uri_ota_status = {
    .uri = "/api/ota/status", .method = HTTP_GET, .handler = ota_status_get_handler,
};

static esp_err_t ota_upload_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "OTA upload started, content_len=%d", req->content_len);
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No update partition");
        return ESP_OK;
    }

    esp_ota_handle_t ota_handle;
    esp_err_t ret = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(ret));
        return ESP_OK;
    }

    char *buf = malloc(4096);
    if (!buf) {
        esp_ota_abort(ota_handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_OK;
    }

    int received = 0;
    int total = req->content_len;
    while (received < total) {
        int read = httpd_req_recv(req, buf, 4096);
        if (read <= 0) {
            ESP_LOGE(TAG, "OTA recv error at %d/%d", received, total);
            free(buf);
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive error");
            return ESP_OK;
        }
        ret = esp_ota_write(ota_handle, buf, read);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(ret));
            free(buf);
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(ret));
            return ESP_OK;
        }
        received += read;
    }
    free(buf);

    ret = esp_ota_end(ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(ret));
        return ESP_OK;
    }

    ret = esp_ota_set_boot_partition(update_partition);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, esp_err_to_name(ret));
        return ESP_OK;
    }

    ESP_LOGI(TAG, "OTA update successful (%d bytes). Restarting...", total);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true,\"message\":\"Firmware updated, rebooting...\"}", 54);

    /* Give time for response to be sent, then reboot */
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;  /* never reached */
}

static const httpd_uri_t uri_ota_upload = {
    .uri = "/api/ota/upload", .method = HTTP_POST, .handler = ota_upload_handler,
};

static esp_err_t ota_rollback_handler(httpd_req_t *req)
{
    esp_err_t ret = esp_ota_mark_app_invalid_rollback_and_reboot();
    if (ret != ESP_OK) {
        char msg[96];
        snprintf(msg, sizeof(msg), "{\"ok\":false,\"error\":\"%s\"}", esp_err_to_name(ret));
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, msg, strlen(msg));
    }
    /* Should not reach here — reboot happens above */
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"ok\":true}", 11);
}

static const httpd_uri_t uri_ota_rollback = {
    .uri = "/api/ota/rollback", .method = HTTP_POST, .handler = ota_rollback_handler,
};

/* ── /ws/input — WebSocket endpoint for keyboard/mouse/joystick ────── */

static esp_err_t ws_input_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        /* WS handshake */
        ESP_LOGI(TAG, "Input WS client connected (fd=%d)", httpd_req_to_sockfd(req));
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(ws_pkt));
    ws_pkt.type = HTTPD_WS_TYPE_BINARY;

    /* First call to get length */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) return ret;
    if (ws_pkt.len == 0) return ESP_OK;
    if (ws_pkt.len > 64) return ESP_OK; /* ignore oversized */

    uint8_t buf[64];
    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) return ret;

    /*
     * Binary input protocol:
     *  [0] = type:  0x01=key, 0x02=mouse, 0x03=joystick
     *
     *  Key:    [0x01][scancode:1][pressed:1]
     *  Mouse:  [0x02][dx:2LE][dy:2LE][buttons:1]  (buttons: bit0=left, bit1=right)
     *  Joy:    [0x03][port:1][state:1]             (state: bit0=up..bit4=fire)
     */
    if (ws_pkt.len < 2) return ESP_OK;

    switch (buf[0]) {
    case 0x01: /* Key */
        if (ws_pkt.len >= 3) {
            esptari_input_key(buf[1], buf[2] != 0);
        }
        break;
    case 0x02: /* Mouse */
        if (ws_pkt.len >= 6) {
            esptari_mouse_t m = {
                .dx = (int16_t)(buf[1] | (buf[2] << 8)),
                .dy = (int16_t)(buf[3] | (buf[4] << 8)),
                .left = (buf[5] & 1) != 0,
                .right = (buf[5] & 2) != 0,
            };
            esptari_input_mouse(&m);
        }
        break;
    case 0x03: /* Joystick */
        if (ws_pkt.len >= 3) {
            esptari_joystick_t j = {
                .up    = (buf[2] & 0x01) != 0,
                .down  = (buf[2] & 0x02) != 0,
                .left  = (buf[2] & 0x04) != 0,
                .right = (buf[2] & 0x08) != 0,
                .fire  = (buf[2] & 0x10) != 0,
            };
            esptari_input_joystick(buf[1], &j);
        }
        break;
    default:
        ESP_LOGD(TAG, "Unknown input type 0x%02x", buf[0]);
        break;
    }

    return ESP_OK;
}

static const httpd_uri_t uri_ws_input = {
    .uri       = "/ws/input",
    .method    = HTTP_GET,
    .handler   = ws_input_handler,
    .is_websocket = true,
    .handle_ws_control_frames = true,
};

/* ── Server lifecycle ──────────────────────────────────────────────── */

esp_err_t esptari_web_init(uint16_t port)
{
    ESP_LOGI(TAG, "Starting web server on port %u", port);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 24;       /* 21 API + /ws + /ws/input + wildcard */
    config.stack_size = 8192;           /* extra stack for OTA upload */

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Register API routes first (matched before the wildcard) */
    httpd_register_uri_handler(s_server, &uri_status);
    httpd_register_uri_handler(s_server, &uri_system);
    httpd_register_uri_handler(s_server, &uri_system_post);
    httpd_register_uri_handler(s_server, &uri_debug_stacktrace_get);
    httpd_register_uri_handler(s_server, &uri_debug_stacktrace_post);
    httpd_register_uri_handler(s_server, &uri_machines);
    httpd_register_uri_handler(s_server, &uri_roms);
    httpd_register_uri_handler(s_server, &uri_network_status);
    httpd_register_uri_handler(s_server, &uri_stream_stats);
    httpd_register_uri_handler(s_server, &uri_config_get);
    httpd_register_uri_handler(s_server, &uri_config_put);
    httpd_register_uri_handler(s_server, &uri_config_machine_get);
    httpd_register_uri_handler(s_server, &uri_config_machine_put);
    httpd_register_uri_handler(s_server, &uri_disks);
    httpd_register_uri_handler(s_server, &uri_disks_mount);
    httpd_register_uri_handler(s_server, &uri_network_config_get);
    httpd_register_uri_handler(s_server, &uri_network_config_put);
    httpd_register_uri_handler(s_server, &uri_network_scan);
    httpd_register_uri_handler(s_server, &uri_ota_status);
    httpd_register_uri_handler(s_server, &uri_ota_upload);
    httpd_register_uri_handler(s_server, &uri_ota_rollback);
    httpd_register_uri_handler(s_server, &uri_ws_input);

    ESP_LOGI(TAG, "Web server started — 22 handlers registered");
    ESP_LOGI(TAG, "Call esptari_web_start_file_server() after registering WS endpoints");
    return ESP_OK;
}

void esptari_web_start_file_server(void)
{
    if (!s_server) return;

    /* Wildcard catch-all — MUST be registered last (after /ws etc.) */
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

httpd_handle_t esptari_web_get_server(void)
{
    return s_server;
}
