#include "esptari_web.h"

#include "esp_log.h"
#include "esptari_web_catalog_sync.h"
#include "esptari_web_catalog.h"
#include "esptari_web_conformance.h"
#include "esptari_web_core_status.h"
#include "esptari_web_debug.h"
#include "esptari_web_files.h"
#include "esptari_web_auth.h"
#include "esptari_web_lifecycle.h"
#include "esptari_web_media.h"
#include "esptari_web_input.h"
#include "esptari_web_mappings.h"
#include "esptari_web_metrics.h"
#include "esptari_web_persistence.h"
#include "esptari_web_snapshot.h"
#include "esptari_web_stream.h"

static const char *TAG = "esptari_web";
static httpd_handle_t server_handle;

void esptari_web_init(uint16_t port)
{
    if (server_handle != NULL) {
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 128;
    config.stack_size = 10240;

    if (httpd_start(&server_handle, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start web server");
        server_handle = NULL;
        return;
    }

    esptari_web_auth_set_token_mode(true);

    esptari_web_core_status_register_routes(server_handle);
    esptari_web_lifecycle_register_routes(server_handle);
    esptari_web_media_register_routes(server_handle);
    esptari_web_input_register_routes(server_handle);
    esptari_web_mappings_register_routes(server_handle);
    esptari_web_stream_register_routes(server_handle);
    esptari_web_debug_register_routes(server_handle);
    esptari_web_metrics_register_routes(server_handle);
    esptari_web_persistence_register_routes(server_handle);
    esptari_web_snapshot_register_routes(server_handle);
    esptari_web_conformance_register_routes(server_handle);
    esptari_web_catalog_register_routes(server_handle);
    esptari_web_files_register_routes(server_handle);
    esptari_web_catalog_sync_register_routes(server_handle);

    ESP_LOGI(TAG, "Web API ready on port %u", (unsigned)port);
}

bool esptari_web_is_running(void)
{
    return server_handle != NULL;
}

httpd_handle_t esptari_web_get_server(void)
{
    return server_handle;
}

void esptari_web_start_file_server(void)
{
}
