#include "esptari_web_catalog.h"

#include "esptari_web_catalog_read.h"
#include "esptari_web_catalog_write.h"

void esptari_web_catalog_register_routes(httpd_handle_t server_handle)
{
    httpd_uri_t catalogs_list = {.uri = "/api/v2/catalogs/list", .method = HTTP_GET, .handler = esptari_web_catalogs_list_handler, .user_ctx = NULL};
    httpd_uri_t catalogs_entries = {.uri = "/api/v2/catalogs/*", .method = HTTP_GET, .handler = esptari_web_catalogs_router_handler, .user_ctx = NULL};
    httpd_uri_t catalogs_post = {.uri = "/api/v2/catalogs/*", .method = HTTP_POST, .handler = esptari_web_catalogs_post_router_handler, .user_ctx = NULL};

    httpd_register_uri_handler(server_handle, &catalogs_list);
    httpd_register_uri_handler(server_handle, &catalogs_entries);
    httpd_register_uri_handler(server_handle, &catalogs_post);
}
