/**
 * @file esptari_network.c
 * @brief espTari Network Interface Manager
 *
 * Manages WiFi (via ESP32-C6 co-processor) and Ethernet (internal EMAC +
 * IP101 PHY) interfaces with automatic failover, DHCP/static IP, and
 * mDNS service advertisement.
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_eth.h"
#include "esp_mac.h"
#include "mdns.h"

#include "esptari_network.h"
#include "esptari_yaml.h"

static const char *TAG = "net_mgr";

/*---------------------------------------------------------------------
 * Configuration
 *-------------------------------------------------------------------*/
#define NET_CONFIG_PATH         "/spiffs/network.yaml"

/* ESP32-P4-NANO Ethernet GPIO pins */
#define ETH_MDC_GPIO            31
#define ETH_MDIO_GPIO           52
#define ETH_PHY_RST_GPIO        51
#define ETH_PHY_ADDR            1

/* Event bits for connectivity tracking */
#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT           BIT1
#define ETH_CONNECTED_BIT       BIT2
#define ANY_IP_BIT              (BIT3)

/* Max WiFi reconnect retries before marking fail */
#define WIFI_MAX_RETRY          10

/*---------------------------------------------------------------------
 * Internal state
 *-------------------------------------------------------------------*/
static struct {
    bool initialized;
    bool started;

    esptari_net_config_t config;

    /* Event group for connectivity signaling */
    EventGroupHandle_t event_group;

    /* Interface runtime state */
    esptari_if_info_t if_info[ESPTARI_IF_COUNT];

    /* WiFi */
    esp_netif_t *wifi_netif;
    int          wifi_retry_count;

    /* Ethernet */
    esp_netif_t            *eth_netif;
    esp_eth_handle_t        eth_handle;
    esp_eth_mac_t          *eth_mac;
    esp_eth_phy_t          *eth_phy;
    esp_eth_netif_glue_handle_t eth_glue;

    /* User callback */
    esptari_net_event_cb_t  event_cb;
    void                   *event_cb_data;
} s_net;

/*---------------------------------------------------------------------
 * Forward declarations
 *-------------------------------------------------------------------*/
static void notify_status(esptari_if_t iface, esptari_if_status_t status);
static esp_err_t init_wifi(void);
static esp_err_t init_ethernet(void);
static esp_err_t init_mdns(void);

/*---------------------------------------------------------------------
 * Event handlers
 *-------------------------------------------------------------------*/

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "WiFi STA started");
            s_net.if_info[ESPTARI_IF_WIFI].status = ESPTARI_IF_STATUS_STARTED;
            notify_status(ESPTARI_IF_WIFI, ESPTARI_IF_STATUS_STARTED);
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi connected to AP");
            s_net.if_info[ESPTARI_IF_WIFI].status = ESPTARI_IF_STATUS_CONNECTED;
            s_net.wifi_retry_count = 0;
            notify_status(ESPTARI_IF_WIFI, ESPTARI_IF_STATUS_CONNECTED);
            break;

        case WIFI_EVENT_STA_DISCONNECTED: {
            s_net.if_info[ESPTARI_IF_WIFI].status = ESPTARI_IF_STATUS_STARTED;
            s_net.if_info[ESPTARI_IF_WIFI].ip[0] = '\0';
            notify_status(ESPTARI_IF_WIFI, ESPTARI_IF_STATUS_STARTED);

            if (s_net.wifi_retry_count < WIFI_MAX_RETRY) {
                s_net.wifi_retry_count++;
                ESP_LOGI(TAG, "WiFi disconnected, retry %d/%d",
                         s_net.wifi_retry_count, WIFI_MAX_RETRY);
                esp_wifi_connect();
            } else {
                ESP_LOGW(TAG, "WiFi max retries reached");
                xEventGroupSetBits(s_net.event_group, WIFI_FAIL_BIT);
            }
            break;
        }

        case WIFI_EVENT_STA_STOP:
            ESP_LOGI(TAG, "WiFi STA stopped");
            s_net.if_info[ESPTARI_IF_WIFI].status = ESPTARI_IF_STATUS_DOWN;
            notify_status(ESPTARI_IF_WIFI, ESPTARI_IF_STATUS_DOWN);
            break;

        default:
            break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(s_net.if_info[ESPTARI_IF_WIFI].ip,
                 sizeof(s_net.if_info[ESPTARI_IF_WIFI].ip),
                 IPSTR, IP2STR(&event->ip_info.ip));
        snprintf(s_net.if_info[ESPTARI_IF_WIFI].netmask,
                 sizeof(s_net.if_info[ESPTARI_IF_WIFI].netmask),
                 IPSTR, IP2STR(&event->ip_info.netmask));
        snprintf(s_net.if_info[ESPTARI_IF_WIFI].gateway,
                 sizeof(s_net.if_info[ESPTARI_IF_WIFI].gateway),
                 IPSTR, IP2STR(&event->ip_info.gw));

        s_net.if_info[ESPTARI_IF_WIFI].status = ESPTARI_IF_STATUS_GOT_IP;

        ESP_LOGI(TAG, "WiFi got IP: %s", s_net.if_info[ESPTARI_IF_WIFI].ip);
        xEventGroupSetBits(s_net.event_group, WIFI_CONNECTED_BIT | ANY_IP_BIT);
        notify_status(ESPTARI_IF_WIFI, ESPTARI_IF_STATUS_GOT_IP);
    }
}

static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_base == ETH_EVENT) {
        switch (event_id) {
        case ETHERNET_EVENT_CONNECTED: {
            ESP_LOGI(TAG, "Ethernet link up");
            s_net.if_info[ESPTARI_IF_ETH].status = ESPTARI_IF_STATUS_CONNECTED;
            /* Read MAC address */
            esp_eth_ioctl(s_net.eth_handle, ETH_CMD_G_MAC_ADDR,
                          s_net.if_info[ESPTARI_IF_ETH].mac);
            ESP_LOGI(TAG, "ETH MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                     s_net.if_info[ESPTARI_IF_ETH].mac[0],
                     s_net.if_info[ESPTARI_IF_ETH].mac[1],
                     s_net.if_info[ESPTARI_IF_ETH].mac[2],
                     s_net.if_info[ESPTARI_IF_ETH].mac[3],
                     s_net.if_info[ESPTARI_IF_ETH].mac[4],
                     s_net.if_info[ESPTARI_IF_ETH].mac[5]);
            notify_status(ESPTARI_IF_ETH, ESPTARI_IF_STATUS_CONNECTED);
            break;
        }
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Ethernet link down");
            s_net.if_info[ESPTARI_IF_ETH].status = ESPTARI_IF_STATUS_STARTED;
            s_net.if_info[ESPTARI_IF_ETH].ip[0] = '\0';
            notify_status(ESPTARI_IF_ETH, ESPTARI_IF_STATUS_STARTED);
            break;
        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "Ethernet started");
            s_net.if_info[ESPTARI_IF_ETH].status = ESPTARI_IF_STATUS_STARTED;
            notify_status(ESPTARI_IF_ETH, ESPTARI_IF_STATUS_STARTED);
            break;
        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "Ethernet stopped");
            s_net.if_info[ESPTARI_IF_ETH].status = ESPTARI_IF_STATUS_DOWN;
            notify_status(ESPTARI_IF_ETH, ESPTARI_IF_STATUS_DOWN);
            break;
        default:
            break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(s_net.if_info[ESPTARI_IF_ETH].ip,
                 sizeof(s_net.if_info[ESPTARI_IF_ETH].ip),
                 IPSTR, IP2STR(&event->ip_info.ip));
        snprintf(s_net.if_info[ESPTARI_IF_ETH].netmask,
                 sizeof(s_net.if_info[ESPTARI_IF_ETH].netmask),
                 IPSTR, IP2STR(&event->ip_info.netmask));
        snprintf(s_net.if_info[ESPTARI_IF_ETH].gateway,
                 sizeof(s_net.if_info[ESPTARI_IF_ETH].gateway),
                 IPSTR, IP2STR(&event->ip_info.gw));

        s_net.if_info[ESPTARI_IF_ETH].status = ESPTARI_IF_STATUS_GOT_IP;

        ESP_LOGI(TAG, "Ethernet got IP: %s", s_net.if_info[ESPTARI_IF_ETH].ip);
        xEventGroupSetBits(s_net.event_group, ETH_CONNECTED_BIT | ANY_IP_BIT);
        notify_status(ESPTARI_IF_ETH, ESPTARI_IF_STATUS_GOT_IP);
    }
}

/*---------------------------------------------------------------------
 * Notify user callback
 *-------------------------------------------------------------------*/

static void notify_status(esptari_if_t iface, esptari_if_status_t status)
{
    if (s_net.event_cb) {
        s_net.event_cb(iface, status, s_net.event_cb_data);
    }
}

/*---------------------------------------------------------------------
 * WiFi initialization
 *-------------------------------------------------------------------*/

static esp_err_t init_wifi(void)
{
    ESP_LOGI(TAG, "Initializing WiFi (via ESP32-C6 co-processor)");

    /* Create default STA netif */
    s_net.wifi_netif = esp_netif_create_default_wifi_sta();
    if (!s_net.wifi_netif) {
        ESP_LOGE(TAG, "Failed to create WiFi STA netif");
        return ESP_FAIL;
    }

    /* Init WiFi with default config */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Register event handlers */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    /* Configure WiFi STA */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    /* Set the first configured AP as the target */
    if (s_net.config.wifi_ap_count > 0) {
        wifi_config_t wifi_config = {0};
        /* Use memcpy — both src buffers are NUL-terminated and dest is zeroed */
        size_t ssid_len = strlen(s_net.config.wifi_aps[0].ssid);
        if (ssid_len > sizeof(wifi_config.sta.ssid) - 1)
            ssid_len = sizeof(wifi_config.sta.ssid) - 1;
        memcpy(wifi_config.sta.ssid, s_net.config.wifi_aps[0].ssid, ssid_len);

        size_t pass_len = strlen(s_net.config.wifi_aps[0].password);
        if (pass_len > sizeof(wifi_config.sta.password) - 1)
            pass_len = sizeof(wifi_config.sta.password) - 1;
        memcpy(wifi_config.sta.password, s_net.config.wifi_aps[0].password, pass_len);

        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        ESP_LOGI(TAG, "WiFi target SSID: %s", wifi_config.sta.ssid);
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    } else {
        ESP_LOGW(TAG, "No WiFi APs configured");
    }

    /* Read WiFi MAC */
    esp_read_mac(s_net.if_info[ESPTARI_IF_WIFI].mac, ESP_MAC_WIFI_STA);

    ESP_LOGI(TAG, "WiFi initialized");
    return ESP_OK;
}

/*---------------------------------------------------------------------
 * Ethernet initialization
 *-------------------------------------------------------------------*/

static esp_err_t init_ethernet(void)
{
    ESP_LOGI(TAG, "Initializing Ethernet (EMAC + IP101)");

    /* MAC config */
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_esp32_emac_config_t emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    emac_config.smi_gpio.mdc_num = ETH_MDC_GPIO;
    emac_config.smi_gpio.mdio_num = ETH_MDIO_GPIO;

    s_net.eth_mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);
    if (!s_net.eth_mac) {
        ESP_LOGE(TAG, "Failed to create Ethernet MAC");
        return ESP_FAIL;
    }

    /* PHY config — IP101 */
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = ETH_PHY_ADDR;
    phy_config.reset_gpio_num = ETH_PHY_RST_GPIO;

    s_net.eth_phy = esp_eth_phy_new_ip101(&phy_config);
    if (!s_net.eth_phy) {
        ESP_LOGE(TAG, "Failed to create Ethernet PHY");
        return ESP_FAIL;
    }

    /* Install Ethernet driver */
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(s_net.eth_mac, s_net.eth_phy);
    esp_err_t ret = esp_eth_driver_install(&config, &s_net.eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ethernet driver install failed: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    /* Create default ETH netif */
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_net.eth_netif = esp_netif_new(&netif_cfg);

    /* Attach ethernet driver to TCP/IP stack */
    s_net.eth_glue = esp_eth_new_netif_glue(s_net.eth_handle);
    ESP_ERROR_CHECK(esp_netif_attach(s_net.eth_netif, s_net.eth_glue));

    /* Register event handlers */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_ETH_GOT_IP, &eth_event_handler, NULL, NULL));

    ESP_LOGI(TAG, "Ethernet initialized (MDC=%d, MDIO=%d, PHY_RST=%d, ADDR=%d)",
             ETH_MDC_GPIO, ETH_MDIO_GPIO, ETH_PHY_RST_GPIO, ETH_PHY_ADDR);
    return ESP_OK;
}

/*---------------------------------------------------------------------
 * mDNS initialization
 *-------------------------------------------------------------------*/

static esp_err_t init_mdns(void)
{
    ESP_LOGI(TAG, "Initializing mDNS as '%s.local'", s_net.config.hostname);

    esp_err_t ret = mdns_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mDNS init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_ERROR_CHECK(mdns_hostname_set(s_net.config.hostname));
    ESP_ERROR_CHECK(mdns_instance_name_set("espTari Atari Emulator"));

    /* Advertise HTTP service */
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0));

    /* Advertise espTari-specific service for discovery */
    mdns_txt_item_t esptari_txt[] = {
        {"version", "0.1.0"},
        {"type", "emulator"},
    };
    ESP_ERROR_CHECK(mdns_service_add("espTari", "_esptari", "_tcp", 8080,
                                      esptari_txt, 2));

    ESP_LOGI(TAG, "mDNS started: %s.local", s_net.config.hostname);
    return ESP_OK;
}

/*---------------------------------------------------------------------
 * Default configuration
 *-------------------------------------------------------------------*/

static void apply_default_config(esptari_net_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->wifi_enabled = true;
    cfg->eth_enabled = true;
    cfg->wifi_ip.dhcp = true;
    cfg->eth_ip.dhcp = true;
    cfg->wifi_priority = 10;
    cfg->eth_priority = 0;
    cfg->default_interface = ESPTARI_IF_WIFI;   /* WiFi first (user preference) */
    cfg->failover_enabled = true;
    cfg->failover_timeout_ms = 5000;
    cfg->mdns_enabled = true;
    strncpy(cfg->hostname, "esptari", sizeof(cfg->hostname));
}

/*---------------------------------------------------------------------
 * Public API
 *-------------------------------------------------------------------*/

esp_err_t esptari_net_init(void)
{
    if (s_net.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    memset(&s_net, 0, sizeof(s_net));
    s_net.event_group = xEventGroupCreate();
    if (!s_net.event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    /* Load configuration from SPIFFS */
    esp_err_t ret = esptari_yaml_load_file(NET_CONFIG_PATH, &s_net.config);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No config found, using defaults");
        apply_default_config(&s_net.config);
    }

    /* Initialize TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Create default event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Initialize enabled interfaces */
    if (s_net.config.wifi_enabled) {
        ret = init_wifi();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "WiFi init failed — continuing without WiFi");
            s_net.config.wifi_enabled = false;
        }
    }

    if (s_net.config.eth_enabled) {
        ret = init_ethernet();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Ethernet init failed — continuing without Ethernet");
            s_net.config.eth_enabled = false;
        }
    }

    /* mDNS */
    if (s_net.config.mdns_enabled) {
        ret = init_mdns();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "mDNS init failed — continuing without mDNS");
        }
    }

    s_net.initialized = true;
    ESP_LOGI(TAG, "Network manager initialized");
    return ESP_OK;
}

esp_err_t esptari_net_start(void)
{
    if (!s_net.initialized) return ESP_ERR_INVALID_STATE;
    if (s_net.started) return ESP_OK;

    ESP_LOGI(TAG, "Starting network interfaces");

    /* Start WiFi */
    if (s_net.config.wifi_enabled) {
        esp_err_t ret = esp_wifi_start();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "WiFi start failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "WiFi started");
        }
    }

    /* Start Ethernet */
    if (s_net.config.eth_enabled) {
        esp_err_t ret = esp_eth_start(s_net.eth_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Ethernet start failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Ethernet started");
        }
    }

    s_net.started = true;
    return ESP_OK;
}

esp_err_t esptari_net_stop(void)
{
    if (!s_net.started) return ESP_OK;

    ESP_LOGI(TAG, "Stopping network interfaces");

    if (s_net.config.wifi_enabled) {
        esp_wifi_disconnect();
        esp_wifi_stop();
    }

    if (s_net.config.eth_enabled && s_net.eth_handle) {
        esp_eth_stop(s_net.eth_handle);
    }

    s_net.started = false;
    return ESP_OK;
}

void esptari_net_deinit(void)
{
    if (!s_net.initialized) return;

    esptari_net_stop();

    if (s_net.config.wifi_enabled) {
        esp_wifi_deinit();
        if (s_net.wifi_netif) {
            esp_netif_destroy(s_net.wifi_netif);
        }
    }

    if (s_net.config.eth_enabled) {
        if (s_net.eth_glue) esp_eth_del_netif_glue(s_net.eth_glue);
        if (s_net.eth_netif) esp_netif_destroy(s_net.eth_netif);
        if (s_net.eth_handle) esp_eth_driver_uninstall(s_net.eth_handle);
        if (s_net.eth_mac) s_net.eth_mac->del(s_net.eth_mac);
        if (s_net.eth_phy) s_net.eth_phy->del(s_net.eth_phy);
    }

    if (s_net.config.mdns_enabled) {
        mdns_free();
    }

    if (s_net.event_group) {
        vEventGroupDelete(s_net.event_group);
    }

    memset(&s_net, 0, sizeof(s_net));
    ESP_LOGI(TAG, "Network manager de-initialized");
}

esp_err_t esptari_net_get_config(esptari_net_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;
    memcpy(config, &s_net.config, sizeof(*config));
    return ESP_OK;
}

esp_err_t esptari_net_set_config(const esptari_net_config_t *config,
                                  bool restart_interfaces)
{
    if (!config) return ESP_ERR_INVALID_ARG;

    /* Save to SPIFFS */
    esp_err_t ret = esptari_yaml_save_file(NET_CONFIG_PATH, config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save config");
        return ret;
    }

    /* Apply */
    memcpy(&s_net.config, config, sizeof(s_net.config));

    if (restart_interfaces && s_net.started) {
        esptari_net_stop();
        /* Re-init interfaces would go here for full hot-reconfigure */
        esptari_net_start();
    }

    return ESP_OK;
}

esp_err_t esptari_net_get_if_info(esptari_if_t iface,
                                   esptari_if_info_t *info)
{
    if (iface >= ESPTARI_IF_COUNT || !info) return ESP_ERR_INVALID_ARG;
    memcpy(info, &s_net.if_info[iface], sizeof(*info));
    return ESP_OK;
}

bool esptari_net_is_connected(void)
{
    return (s_net.if_info[ESPTARI_IF_WIFI].status == ESPTARI_IF_STATUS_GOT_IP ||
            s_net.if_info[ESPTARI_IF_ETH].status == ESPTARI_IF_STATUS_GOT_IP);
}

esp_err_t esptari_net_register_event_cb(esptari_net_event_cb_t cb,
                                         void *user_data)
{
    s_net.event_cb = cb;
    s_net.event_cb_data = user_data;
    return ESP_OK;
}

esp_err_t esptari_net_wait_connected(uint32_t timeout_ms)
{
    if (esptari_net_is_connected()) return ESP_OK;

    TickType_t ticks = (timeout_ms == 0) ? portMAX_DELAY
                                          : pdMS_TO_TICKS(timeout_ms);
    EventBits_t bits = xEventGroupWaitBits(s_net.event_group,
                                            ANY_IP_BIT,
                                            pdFALSE,   /* don't clear */
                                            pdFALSE,   /* any bit */
                                            ticks);
    return (bits & ANY_IP_BIT) ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t esptari_net_write_default_config(void)
{
    esptari_net_config_t cfg;
    apply_default_config(&cfg);
    return esptari_yaml_save_file(NET_CONFIG_PATH, &cfg);
}
