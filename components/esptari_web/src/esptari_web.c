/**
 * @file esptari_web.c
 * @brief Web interface — HTTP server, REST API, static file serving
 *
 * SPDX-License-Identifier: MIT
 */
#include "esptari_web.h"
#include "esp_log.h"
#include "esp_http_server.h"

static const char *TAG = "esptari_web";
static httpd_handle_t s_server = NULL;

esp_err_t esptari_web_init(uint16_t port)
{
    ESP_LOGI(TAG, "Starting web server on port %u", port);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    /* TODO: Register URI handlers for:
     *   GET /           — serve Vue.js SPA
     *   GET /api/status — emulator state JSON
     *   POST /api/machine — load machine profile
     *   POST /api/control — start/stop/reset
     *   GET /api/roms   — list available ROMs
     *   WebSocket /ws   — streaming endpoint (Phase 3)
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
