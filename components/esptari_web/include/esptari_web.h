/**
 * @file esptari_web.h
 * @brief Web interface — HTTP server, REST API, static file serving
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and start the web server
 * @param port HTTP port (default 80)
 * @return ESP_OK on success
 */
esp_err_t esptari_web_init(uint16_t port);

/**
 * @brief Stop the web server
 */
void esptari_web_stop(void);

/**
 * @brief Check if web server is running
 */
bool esptari_web_is_running(void);

#ifdef __cplusplus
}
#endif
