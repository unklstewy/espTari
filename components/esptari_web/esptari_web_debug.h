#pragma once

#include <stdint.h>

#include "esp_http_server.h"

typedef struct {
    const char *run_mode;
    uint64_t tick_counter;
    uint64_t cycle_counter;
    uint32_t scheduler_hz;
    uint64_t timestamp_origin_us;
    uint64_t timestamp_last_emitted_us;
    uint64_t timestamp_regressions;
} esptari_web_debug_runtime_snapshot_t;

void esptari_web_debug_register_routes(httpd_handle_t server_handle);
void esptari_web_debug_get_runtime_snapshot(esptari_web_debug_runtime_snapshot_t *out);
