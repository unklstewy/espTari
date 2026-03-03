#pragma once

#include <stdint.h>

#include "esp_http_server.h"

typedef struct {
    uint64_t dropped_packets_total;
} esptari_web_stream_runtime_snapshot_t;

void esptari_web_stream_register_routes(httpd_handle_t server_handle);
void esptari_web_stream_get_runtime_snapshot(esptari_web_stream_runtime_snapshot_t *out);
