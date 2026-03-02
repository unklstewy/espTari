#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    char wifi_ssid[33];
    char wifi_pass[65];
} esptari_net_config_t;

esp_err_t esptari_net_init(void);
void esptari_net_start(void);
esp_err_t esptari_net_wait_connected(uint32_t timeout_ms);
bool esptari_net_is_connected(void);
esp_err_t esptari_net_write_default_config(void);
