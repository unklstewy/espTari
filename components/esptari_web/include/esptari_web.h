#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_http_server.h"

void esptari_web_init(uint16_t port);
bool esptari_web_is_running(void);
httpd_handle_t esptari_web_get_server(void);
void esptari_web_start_file_server(void);
