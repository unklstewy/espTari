/**
 * @file esptari_yaml.h
 * @brief Lightweight YAML subset parser for network configuration
 *
 * Parses the Netplan-style YAML used for espTari network config.
 * This is NOT a full YAML parser — it handles the subset needed
 * for key: value pairs, nested mappings, and simple lists.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "esp_err.h"
#include "esptari_network.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parse a network YAML configuration string.
 *
 * @param[in]  yaml_str Null-terminated YAML string
 * @param[out] config   Parsed configuration output
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on parse error
 */
esp_err_t esptari_yaml_parse(const char *yaml_str,
                              esptari_net_config_t *config);

/**
 * @brief Serialize a network configuration to YAML string.
 *
 * @param[in]  config Configuration to serialize
 * @param[out] buf    Output buffer
 * @param[in]  buf_sz Buffer size in bytes
 * @return Number of bytes written (excluding NUL), or -1 on error
 */
int esptari_yaml_serialize(const esptari_net_config_t *config,
                            char *buf, int buf_sz);

/**
 * @brief Load network config from SPIFFS file.
 *
 * @param[in]  path   File path (e.g. "/spiffs/network.yaml")
 * @param[out] config Parsed configuration
 * @return ESP_OK, ESP_ERR_NOT_FOUND, or ESP_ERR_INVALID_ARG
 */
esp_err_t esptari_yaml_load_file(const char *path,
                                  esptari_net_config_t *config);

/**
 * @brief Save network config to SPIFFS file.
 *
 * @param[in] path   File path (e.g. "/spiffs/network.yaml")
 * @param[in] config Configuration to save
 * @return ESP_OK on success
 */
esp_err_t esptari_yaml_save_file(const char *path,
                                  const esptari_net_config_t *config);

#ifdef __cplusplus
}
#endif
