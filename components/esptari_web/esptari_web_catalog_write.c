#include "esptari_web_catalog_write.h"

#include <string.h>

#include "esptari_web_catalog_write_download.h"
#include "esptari_web_catalog_write_maintenance.h"
#include "esptari_web_http_utils.h"

#define send_json esptari_web_send_json

esp_err_t esptari_web_catalogs_post_router_handler(httpd_req_t *req)
{
    if (strstr(req->uri, "/download-entry") != NULL) {
        return esptari_web_catalog_download_entry_handler(req);
    }
    if (strstr(req->uri, "/download-missing") != NULL) {
        return esptari_web_catalog_download_missing_handler(req);
    }
    if (strstr(req->uri, "/probe-links") != NULL) {
        return esptari_web_catalog_probe_links_handler(req);
    }
    if (strstr(req->uri, "/mark-dead") != NULL) {
        return esptari_web_catalog_mark_dead_handler(req);
    }
    if (strstr(req->uri, "/rescan-local") != NULL) {
        return esptari_web_catalog_rescan_local_handler(req);
    }
    return send_json(req, "{\"ok\":false,\"error\":{\"code\":\"BAD_REQUEST\"}}", 400);
}
