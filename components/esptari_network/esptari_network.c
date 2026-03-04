#include "esptari_network.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <time.h>
#include <string.h>

static bool connected;
static const char *TAG = "net_mgr";
static bool ip_event_handler_registered;
static EventGroupHandle_t wifi_event_group;
static esp_netif_t *wifi_sta_netif;
static int s_retry_num;

static const int WIFI_CONNECTED_BIT = BIT0;
static const int WIFI_FAIL_BIT = BIT1;

static bool system_time_valid(void)
{
    time_t now = 0;
    time(&now);
    return now >= 1704067200;  // 2024-01-01T00:00:00Z
}

static void log_ip_address(void)
{
    bool found = false;
    esp_netif_t *netif = NULL;

    while ((netif = esp_netif_next(netif)) != NULL) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
            continue;
        }

        if (ip_info.ip.addr == 0) {
            continue;
        }

        const char *ifkey = esp_netif_get_ifkey(netif);
        ESP_LOGI(TAG, "Device IP (%s): " IPSTR,
                 ifkey ? ifkey : "unknown",
                 IP2STR(&ip_info.ip));
        found = true;
    }

    if (!found) {
        ESP_LOGI(TAG, "IP not available yet (no configured netif has IPv4)");
    }
}

static void on_ip_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;

    if (event_id == IP_EVENT_STA_GOT_IP || event_id == IP_EVENT_ETH_GOT_IP) {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;
        connected = true;
        s_retry_num = 0;
        if (wifi_event_group != NULL) {
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        }
        ESP_LOGI(TAG, "Device IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static void on_wifi_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;
    (void)event_data;

    if (event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi station started, connecting...");
        esp_wifi_connect();
        return;
    }

    if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        connected = false;
        if (s_retry_num < CONFIG_ESPTARI_WIFI_MAX_RETRY) {
            s_retry_num++;
            ESP_LOGW(TAG, "Wi-Fi disconnected, retry %d/%d",
                     s_retry_num, CONFIG_ESPTARI_WIFI_MAX_RETRY);
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "Wi-Fi connect failed after %d retries", CONFIG_ESPTARI_WIFI_MAX_RETRY);
            if (wifi_event_group != NULL) {
                xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
            }
        }
    }
}

esp_err_t esptari_net_init(void)
{
    connected = false;
    s_retry_num = 0;

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        connected = false;
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        connected = false;
        return err;
    }

    if (!ip_event_handler_registered) {
        err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, NULL);
        if (err != ESP_OK) {
            connected = false;
            return err;
        }

        err = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL);
        if (err != ESP_OK) {
            connected = false;
            return err;
        }
        ip_event_handler_registered = true;
    }

    if (wifi_event_group == NULL) {
        wifi_event_group = xEventGroupCreate();
        if (wifi_event_group == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

#if CONFIG_ESPTARI_WIFI_ENABLE
    if (strlen(CONFIG_ESPTARI_WIFI_SSID) == 0) {
        ESP_LOGW(TAG, "Wi-Fi enabled but SSID is empty; skipping Wi-Fi start");
        return ESP_OK;
    }

    if (wifi_sta_netif == NULL) {
        wifi_sta_netif = esp_netif_create_default_wifi_sta();
        if (wifi_sta_netif == NULL) {
            return ESP_FAIL;
        }
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Wi-Fi init failed (%s); continuing without Wi-Fi", esp_err_to_name(err));
        return ESP_OK;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, CONFIG_ESPTARI_WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, CONFIG_ESPTARI_WIFI_PASSWORD, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    if (strlen(CONFIG_ESPTARI_WIFI_PASSWORD) == 0) {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }

    ESP_LOGI(TAG, "Initializing Wi-Fi station for SSID: %s", CONFIG_ESPTARI_WIFI_SSID);

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi set mode failed (%s); continuing without Wi-Fi", esp_err_to_name(err));
        return ESP_OK;
    }

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi set config failed (%s); continuing without Wi-Fi", esp_err_to_name(err));
        return ESP_OK;
    }

    err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Wi-Fi start failed (%s); continuing without Wi-Fi", esp_err_to_name(err));
        return ESP_OK;
    }
#else
    ESP_LOGI(TAG, "Wi-Fi disabled by configuration");
#endif

    return ESP_OK;
}

void esptari_net_start(void)
{
    if (connected) {
        log_ip_address();
    }
}

esp_err_t esptari_net_wait_connected(uint32_t timeout_ms)
{
#if CONFIG_ESPTARI_WIFI_ENABLE
    if (wifi_event_group != NULL) {
        EventBits_t bits = xEventGroupWaitBits(
            wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(timeout_ms));

        if (bits & WIFI_CONNECTED_BIT) {
            connected = true;
        } else if (bits & WIFI_FAIL_BIT) {
            connected = false;
        }
    }
#else
    (void)timeout_ms;
#endif

    log_ip_address();
    return connected ? ESP_OK : ESP_ERR_TIMEOUT;
}

bool esptari_net_is_connected(void)
{
    return connected;
}

esp_err_t esptari_net_write_default_config(void)
{
    return ESP_OK;
}

esp_err_t esptari_net_sync_time(uint32_t timeout_ms)
{
    if (!connected) {
        return ESP_ERR_INVALID_STATE;
    }

    if (system_time_valid()) {
        ESP_LOGI(TAG, "System clock already valid; skipping SNTP sync");
        return ESP_OK;
    }

    if (esp_sntp_enabled()) {
        esp_sntp_stop();
    }

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "0.pool.ntp.org");
    esp_sntp_setservername(1, "1.pool.ntp.org");
    esp_sntp_setservername(2, "2.pool.ntp.org");
    esp_sntp_init();

    const TickType_t step_ticks = pdMS_TO_TICKS(250);
    const TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    TickType_t elapsed_ticks = 0;

    while (elapsed_ticks <= timeout_ticks) {
        if (system_time_valid()) {
            time_t now = 0;
            time(&now);
            struct tm utc_now = {0};
            gmtime_r(&now, &utc_now);
            ESP_LOGI(TAG,
                     "SNTP sync complete: %04d-%02d-%02dT%02d:%02d:%02dZ",
                     utc_now.tm_year + 1900,
                     utc_now.tm_mon + 1,
                     utc_now.tm_mday,
                     utc_now.tm_hour,
                     utc_now.tm_min,
                     utc_now.tm_sec);
            return ESP_OK;
        }
        vTaskDelay(step_ticks);
        elapsed_ticks += step_ticks;
    }

    ESP_LOGW(TAG, "SNTP sync timed out after %lu ms", (unsigned long)timeout_ms);
    return ESP_ERR_TIMEOUT;
}
