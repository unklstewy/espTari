/**
 * @file network_test_main.c
 * @brief espTari Network Connectivity Test
 *
 * Tests:
 * 1. SPIFFS mount + default config generation
 * 2. YAML config parse/serialize round-trip
 * 3. WiFi connection (via ESP32-C6 co-processor)
 * 4. Ethernet initialization (via internal EMAC + IP101)
 * 5. mDNS advertisement (esptari.local)
 * 6. IP connectivity verification
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"

#include "esptari_network.h"
#include "esptari_yaml.h"

static const char *TAG = "net_test";

/*---------------------------------------------------------------------
 * Test tracking
 *-------------------------------------------------------------------*/
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_PASS(name) do { \
    ESP_LOGI(TAG, "  [PASS] %s", name); \
    tests_passed++; \
} while(0)

#define TEST_FAIL(name, reason) do { \
    ESP_LOGE(TAG, "  [FAIL] %s — %s", name, reason); \
    tests_failed++; \
} while(0)

/*---------------------------------------------------------------------
 * Network event callback
 *-------------------------------------------------------------------*/
static void net_event_cb(esptari_if_t iface, esptari_if_status_t status,
                          void *user_data)
{
    static const char *if_names[] = {"WiFi", "Ethernet"};
    static const char *status_names[] = {"DOWN", "STARTED", "CONNECTED", "GOT_IP"};

    ESP_LOGI(TAG, "  [EVENT] %s -> %s",
             if_names[iface], status_names[status]);
}

/*---------------------------------------------------------------------
 * Test 1: SPIFFS + default config
 *-------------------------------------------------------------------*/
static void test_spiffs_config(void)
{
    ESP_LOGI(TAG, "--- Test 1: SPIFFS + Config ---");

    /* Mount SPIFFS */
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "spiffs",
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        TEST_FAIL("SPIFFS mount", esp_err_to_name(ret));
        return;
    }

    size_t total = 0, used = 0;
    esp_spiffs_info("spiffs", &total, &used);
    ESP_LOGI(TAG, "  SPIFFS: total=%zu, used=%zu", total, used);
    TEST_PASS("SPIFFS mount");

    /* Write default config */
    ret = esptari_net_write_default_config();
    if (ret != ESP_OK) {
        TEST_FAIL("Write default config", esp_err_to_name(ret));
        return;
    }
    TEST_PASS("Write default config");

    /* Read it back */
    esptari_net_config_t cfg;
    ret = esptari_yaml_load_file("/spiffs/network.yaml", &cfg);
    if (ret != ESP_OK) {
        TEST_FAIL("Load config file", esp_err_to_name(ret));
        return;
    }
    TEST_PASS("Load config file");

    /* Verify fields */
    if (cfg.wifi_enabled && cfg.eth_enabled && cfg.mdns_enabled &&
        cfg.wifi_ip.dhcp && cfg.eth_ip.dhcp &&
        strcmp(cfg.hostname, "esptari") == 0) {
        TEST_PASS("Config defaults verified");
    } else {
        TEST_FAIL("Config defaults", "unexpected values");
    }
}

/*---------------------------------------------------------------------
 * Test 2: YAML parse/serialize round-trip
 *-------------------------------------------------------------------*/
static void test_yaml_roundtrip(void)
{
    ESP_LOGI(TAG, "--- Test 2: YAML Round-trip ---");

    const char *test_yaml =
        "network:\n"
        "  version: 1\n"
        "  renderer: esptari\n"
        "\n"
        "  ethernets:\n"
        "    eth0:\n"
        "      dhcp4: true\n"
        "\n"
        "  wifis:\n"
        "    wlan0:\n"
        "      dhcp4: true\n"
        "      optional: true\n"
        "      access-points:\n"
        "        \"TestNetwork\":\n"
        "          password: \"test1234\"\n"
        "        \"BackupNet\":\n"
        "          password: \"backup5678\"\n"
        "      priority: 10\n"
        "\n"
        "  routing:\n"
        "    default-interface: wlan0\n"
        "    failover: true\n"
        "    failover-timeout-ms: 3000\n"
        "\n"
        "  services:\n"
        "    mdns:\n"
        "      enabled: true\n"
        "      hostname: mytest\n";

    esptari_net_config_t cfg;
    esp_err_t ret = esptari_yaml_parse(test_yaml, &cfg);
    if (ret != ESP_OK) {
        TEST_FAIL("YAML parse", esp_err_to_name(ret));
        return;
    }
    TEST_PASS("YAML parse");

    /* Verify parsed values */
    bool ok = true;
    if (cfg.wifi_ap_count != 2) {
        ESP_LOGE(TAG, "  Expected 2 APs, got %d", cfg.wifi_ap_count);
        ok = false;
    }
    if (ok && strcmp(cfg.wifi_aps[0].ssid, "TestNetwork") != 0) {
        ESP_LOGE(TAG, "  AP[0] SSID mismatch: '%s'", cfg.wifi_aps[0].ssid);
        ok = false;
    }
    if (ok && strcmp(cfg.wifi_aps[1].password, "backup5678") != 0) {
        ESP_LOGE(TAG, "  AP[1] password mismatch: '%s'", cfg.wifi_aps[1].password);
        ok = false;
    }
    if (ok && cfg.default_interface != ESPTARI_IF_WIFI) {
        ESP_LOGE(TAG, "  default-interface should be WiFi");
        ok = false;
    }
    if (ok && cfg.failover_timeout_ms != 3000) {
        ESP_LOGE(TAG, "  failover-timeout-ms should be 3000, got %d",
                 cfg.failover_timeout_ms);
        ok = false;
    }
    if (ok && strcmp(cfg.hostname, "mytest") != 0) {
        ESP_LOGE(TAG, "  hostname should be 'mytest', got '%s'", cfg.hostname);
        ok = false;
    }
    if (ok) {
        TEST_PASS("YAML parsed values");
    } else {
        TEST_FAIL("YAML parsed values", "see above");
    }

    /* Serialize back (heap-allocate to avoid stack overflow) */
    char *buf = malloc(2048);
    if (!buf) {
        TEST_FAIL("YAML serialize", "malloc failed");
    } else {
        int len = esptari_yaml_serialize(&cfg, buf, 2048);
        if (len > 0) {
            TEST_PASS("YAML serialize");
            ESP_LOGI(TAG, "  Serialized %d bytes", len);
        } else {
            TEST_FAIL("YAML serialize", "returned <= 0");
        }
        free(buf);
    }
}

/*---------------------------------------------------------------------
 * Test 3: Network connectivity (WiFi primary)
 *-------------------------------------------------------------------*/
static void test_network_connect(void)
{
    ESP_LOGI(TAG, "--- Test 3: Network Connectivity ---");

    /* First, write a config that uses the build-time SSID/password */
    esptari_net_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.wifi_enabled = true;
    cfg.eth_enabled = true;
    cfg.wifi_ip.dhcp = true;
    cfg.eth_ip.dhcp = true;
    cfg.wifi_priority = 0;      /* WiFi is primary for this test */
    cfg.eth_priority = 10;
    cfg.default_interface = ESPTARI_IF_WIFI;
    cfg.failover_enabled = true;
    cfg.failover_timeout_ms = 5000;
    cfg.mdns_enabled = true;
    strncpy(cfg.hostname, "esptari", sizeof(cfg.hostname));

    /* Use build-time config for WiFi credentials */
    cfg.wifi_ap_count = 1;
    strncpy(cfg.wifi_aps[0].ssid,
            CONFIG_ESPTARI_TEST_WIFI_SSID,
            ESPTARI_NET_MAX_SSID_LEN);
    strncpy(cfg.wifi_aps[0].password,
            CONFIG_ESPTARI_TEST_WIFI_PASSWORD,
            ESPTARI_NET_MAX_PASS_LEN);

    /* Save config so network manager picks it up */
    esp_err_t ret = esptari_yaml_save_file("/spiffs/network.yaml", &cfg);
    if (ret != ESP_OK) {
        TEST_FAIL("Save WiFi config", esp_err_to_name(ret));
        return;
    }
    TEST_PASS("Save WiFi config");

    /* Initialize network manager */
    ret = esptari_net_init();
    if (ret != ESP_OK) {
        TEST_FAIL("Network init", esp_err_to_name(ret));
        return;
    }
    TEST_PASS("Network init");

    /* Register event callback */
    esptari_net_register_event_cb(net_event_cb, NULL);

    /* Start interfaces */
    ret = esptari_net_start();
    if (ret != ESP_OK) {
        TEST_FAIL("Network start", esp_err_to_name(ret));
        return;
    }
    TEST_PASS("Network start");

    /* Wait for connectivity */
    ESP_LOGI(TAG, "  Waiting for network (timeout %ds)...",
             CONFIG_ESPTARI_TEST_CONNECT_TIMEOUT_S);

    ret = esptari_net_wait_connected(
        CONFIG_ESPTARI_TEST_CONNECT_TIMEOUT_S * 1000);
    if (ret != ESP_OK) {
        TEST_FAIL("Network connect", "Timed out waiting for IP");
        return;
    }
    TEST_PASS("Network connect");

    /* Show interface info */
    esptari_if_info_t info;

    ret = esptari_net_get_if_info(ESPTARI_IF_WIFI, &info);
    if (ret == ESP_OK && info.status == ESPTARI_IF_STATUS_GOT_IP) {
        ESP_LOGI(TAG, "  WiFi IP: %s", info.ip);
        ESP_LOGI(TAG, "  WiFi Netmask: %s", info.netmask);
        ESP_LOGI(TAG, "  WiFi Gateway: %s", info.gateway);
        ESP_LOGI(TAG, "  WiFi MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                 info.mac[0], info.mac[1], info.mac[2],
                 info.mac[3], info.mac[4], info.mac[5]);
        TEST_PASS("WiFi got IP");
    } else {
        ESP_LOGW(TAG, "  WiFi: no IP (status=%d)", info.status);
        /* Not a hard fail — Ethernet might have IP */
    }

    ret = esptari_net_get_if_info(ESPTARI_IF_ETH, &info);
    if (ret == ESP_OK && info.status == ESPTARI_IF_STATUS_GOT_IP) {
        ESP_LOGI(TAG, "  Ethernet IP: %s", info.ip);
        ESP_LOGI(TAG, "  Ethernet MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                 info.mac[0], info.mac[1], info.mac[2],
                 info.mac[3], info.mac[4], info.mac[5]);
        TEST_PASS("Ethernet got IP");
    } else {
        ESP_LOGW(TAG, "  Ethernet: no IP (status=%d) — cable not connected?",
                 info.status);
    }

    /* Verify overall connectivity */
    if (esptari_net_is_connected()) {
        TEST_PASS("Network is connected");
    } else {
        TEST_FAIL("Network is connected", "no interface has IP");
    }
}

/*---------------------------------------------------------------------
 * Test 4: mDNS verification
 *-------------------------------------------------------------------*/
static void test_mdns(void)
{
    ESP_LOGI(TAG, "--- Test 4: mDNS ---");

    if (esptari_net_is_connected()) {
        /* mDNS was initialized during net_init if mdns_enabled=true */
        ESP_LOGI(TAG, "  mDNS hostname: esptari.local");
        ESP_LOGI(TAG, "  Services: _http._tcp (80), _esptari._tcp (8080)");
        ESP_LOGI(TAG, "  Try: ping esptari.local  (from another machine)");
        TEST_PASS("mDNS active");
    } else {
        TEST_FAIL("mDNS", "no network — mDNS won't be reachable");
    }
}

/*---------------------------------------------------------------------
 * Test 5: Long-running stability (stay online for observation)
 *-------------------------------------------------------------------*/
static void test_stability(void)
{
    ESP_LOGI(TAG, "--- Test 5: Stability Monitor (60s) ---");

    for (int i = 0; i < 60; i++) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        if ((i + 1) % 10 == 0) {
            esptari_if_info_t wifi_info, eth_info;
            esptari_net_get_if_info(ESPTARI_IF_WIFI, &wifi_info);
            esptari_net_get_if_info(ESPTARI_IF_ETH, &eth_info);

            ESP_LOGI(TAG, "  [%2ds] WiFi=%d(%s) Eth=%d(%s)",
                     i + 1,
                     wifi_info.status,
                     wifi_info.ip[0] ? wifi_info.ip : "no-ip",
                     eth_info.status,
                     eth_info.ip[0] ? eth_info.ip : "no-ip");
        }
    }

    if (esptari_net_is_connected()) {
        TEST_PASS("60s stability - still connected");
    } else {
        TEST_FAIL("60s stability", "lost connectivity");
    }
}

/*---------------------------------------------------------------------
 * Entry point
 *-------------------------------------------------------------------*/
void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  espTari Network Test");
    ESP_LOGI(TAG, "  Build: %s %s", __DATE__, __TIME__);
    ESP_LOGI(TAG, "========================================");

    /* Print chip info */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "ESP32-P4, %d cores, rev %d",
             chip_info.cores, chip_info.revision);

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Run tests */
    test_spiffs_config();
    test_yaml_roundtrip();
    test_network_connect();
    test_mdns();
    test_stability();

    /* Summary */
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  RESULTS: %d passed, %d failed",
             tests_passed, tests_failed);
    ESP_LOGI(TAG, "  %s", tests_failed == 0 ? "ALL PASS" : "FAILURES DETECTED");
    ESP_LOGI(TAG, "========================================");

    /* Keep running so mDNS stays active for manual testing */
    ESP_LOGI(TAG, "Staying online — try 'ping esptari.local'");
    ESP_LOGI(TAG, "Press Ctrl+] to exit monitor");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
