#include "esptari_web_catalog.h"

#include "esptari_web_auth.h"
#include "esptari_web_catalog_read.h"
#include "esptari_web_catalog_write.h"

void esptari_web_catalog_register_routes(httpd_handle_t server_handle)
{
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/catalogs/list", HTTP_GET, esptari_web_catalogs_list_handler, "files:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/catalogs/*", HTTP_GET, esptari_web_catalogs_router_handler, "files:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/catalogs/*", HTTP_POST, esptari_web_catalogs_post_router_handler, "files:write"));
}
