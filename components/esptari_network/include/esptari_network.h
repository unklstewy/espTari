/**
 * @file esptari_network.h
 * @brief espTari Network Interface Manager
 *
 * Dual-interface network management (WiFi 6 + Ethernet) with
 * automatic failover, YAML-based configuration, and mDNS.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------
 * Constants
 *-------------------------------------------------------------------*/
#define ESPTARI_NET_MAX_SSID_LEN      32
#define ESPTARI_NET_MAX_PASS_LEN      64
#define ESPTARI_NET_MAX_HOSTNAME_LEN  32
#define ESPTARI_NET_MAX_AP_ENTRIES     8

/*---------------------------------------------------------------------
 * Types
 *-------------------------------------------------------------------*/

/** Network interface identifiers */
typedef enum {
    ESPTARI_IF_WIFI = 0,
    ESPTARI_IF_ETH,
    ESPTARI_IF_COUNT
} esptari_if_t;

/** Interface status */
typedef enum {
    ESPTARI_IF_STATUS_DOWN = 0,     /**< Interface not started */
    ESPTARI_IF_STATUS_STARTED,      /**< Interface started, no link */
    ESPTARI_IF_STATUS_CONNECTED,    /**< Link up, no IP yet */
    ESPTARI_IF_STATUS_GOT_IP,       /**< Fully operational */
} esptari_if_status_t;

/** WiFi access-point entry (for config) */
typedef struct {
    char ssid[ESPTARI_NET_MAX_SSID_LEN + 1];
    char password[ESPTARI_NET_MAX_PASS_LEN + 1];
} esptari_wifi_ap_t;

/** IP configuration */
typedef struct {
    bool   dhcp;                    /**< true = DHCP, false = static */
    char   ip[16];                  /**< Static IP (e.g. "192.168.1.100") */
    char   netmask[16];             /**< Subnet mask */
    char   gateway[16];             /**< Default gateway */
    char   dns1[16];                /**< Primary DNS */
    char   dns2[16];                /**< Secondary DNS */
} esptari_ip_config_t;

/** Complete network configuration */
typedef struct {
    /* WiFi */
    bool              wifi_enabled;
    esptari_wifi_ap_t wifi_aps[ESPTARI_NET_MAX_AP_ENTRIES];
    int               wifi_ap_count;
    esptari_ip_config_t wifi_ip;
    int               wifi_priority;        /**< Lower = higher priority */

    /* Ethernet */
    bool              eth_enabled;
    esptari_ip_config_t eth_ip;
    int               eth_priority;         /**< Lower = higher priority */

    /* Routing */
    esptari_if_t      default_interface;
    bool              failover_enabled;
    int               failover_timeout_ms;

    /* mDNS */
    bool              mdns_enabled;
    char              hostname[ESPTARI_NET_MAX_HOSTNAME_LEN + 1];
} esptari_net_config_t;

/** Per-interface runtime status */
typedef struct {
    esptari_if_status_t status;
    char ip[16];
    char netmask[16];
    char gateway[16];
    uint8_t mac[6];
} esptari_if_info_t;

/** Network event callback */
typedef void (*esptari_net_event_cb_t)(esptari_if_t iface,
                                       esptari_if_status_t new_status,
                                       void *user_data);

/*---------------------------------------------------------------------
 * API
 *-------------------------------------------------------------------*/

/**
 * @brief Initialize the network manager.
 *
 * Reads configuration from SPIFFS (/spiffs/network.yaml), initializes
 * esp_netif, event loop, and enabled interfaces. Call once at startup.
 *
 * @return ESP_OK on success
 */
esp_err_t esptari_net_init(void);

/**
 * @brief Start all enabled network interfaces.
 *
 * Must be called after esptari_net_init().
 *
 * @return ESP_OK on success
 */
esp_err_t esptari_net_start(void);

/**
 * @brief Stop all network interfaces.
 *
 * @return ESP_OK on success
 */
esp_err_t esptari_net_stop(void);

/**
 * @brief De-initialize the network manager and free resources.
 */
void esptari_net_deinit(void);

/**
 * @brief Get current configuration (read-only copy).
 *
 * @param[out] config Destination for configuration copy
 * @return ESP_OK on success
 */
esp_err_t esptari_net_get_config(esptari_net_config_t *config);

/**
 * @brief Apply a new configuration.
 *
 * Saves to SPIFFS and optionally restarts interfaces.
 *
 * @param[in] config New configuration
 * @param[in] restart_interfaces true to restart interfaces immediately
 * @return ESP_OK on success
 */
esp_err_t esptari_net_set_config(const esptari_net_config_t *config,
                                  bool restart_interfaces);

/**
 * @brief Get runtime info for a specific interface.
 *
 * @param[in]  iface Interface to query
 * @param[out] info  Destination for interface info
 * @return ESP_OK on success
 */
esp_err_t esptari_net_get_if_info(esptari_if_t iface,
                                   esptari_if_info_t *info);

/**
 * @brief Check if any interface has an IP address.
 *
 * @return true if at least one interface has GOT_IP status
 */
bool esptari_net_is_connected(void);

/**
 * @brief Register a callback for interface status changes.
 *
 * @param cb        Callback function
 * @param user_data Opaque pointer passed to callback
 * @return ESP_OK on success
 */
esp_err_t esptari_net_register_event_cb(esptari_net_event_cb_t cb,
                                         void *user_data);

/**
 * @brief Wait for network connectivity (blocking).
 *
 * Blocks until at least one interface gets an IP or timeout expires.
 *
 * @param timeout_ms Maximum wait time in ms (0 = wait forever)
 * @return ESP_OK if connected, ESP_ERR_TIMEOUT if timed out
 */
esp_err_t esptari_net_wait_connected(uint32_t timeout_ms);

/**
 * @brief Write a default configuration to SPIFFS.
 *
 * Creates /spiffs/network.yaml with WiFi DHCP + Ethernet DHCP defaults.
 *
 * @return ESP_OK on success
 */
esp_err_t esptari_net_write_default_config(void);

#ifdef __cplusplus
}
#endif
