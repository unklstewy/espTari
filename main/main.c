/**
 * @file main.c
 * @brief espTari - Atari ST/STe/Mega/Falcon Emulator for ESP32-P4
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
#include "esp_ota_ops.h"
#include "nvs_flash.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

#include "esptari_loader.h"
#include "esptari_core.h"
#include "esptari_network.h"
#include "esptari_video.h"
#include "esptari_audio.h"
#include "esptari_input.h"
#include "esptari_storage.h"
#include "esptari_web.h"
#include "esptari_stream.h"

static const char *TAG = "espTari";

// Version information
#define ESPTARI_VERSION_MAJOR 0
#define ESPTARI_VERSION_MINOR 1
#define ESPTARI_VERSION_PATCH 0
#define ESPTARI_VERSION_STRING "0.1.0"

/**
 * @brief Initialize NVS flash
 */
static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

/**
 * @brief Initialize SPIFFS for network configuration
 */
static esp_err_t init_spiffs(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");
    
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/config",
        .partition_label = "spiffs",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format SPIFFS");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }
    
    size_t total = 0, used = 0;
    ret = esp_spiffs_info("spiffs", &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS: total=%zu, used=%zu", total, used);
    }
    
    return ESP_OK;
}

/**
 * @brief Initialize SD card for ROM and component storage
 */
static esp_err_t init_sdcard(void)
{
    ESP_LOGI(TAG, "Initializing SD card");
    
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 10,
        .allocation_unit_size = 16 * 1024
    };
    
    sdmmc_card_t *card;
    const char mount_point[] = "/sdcard";
    
    ESP_LOGI(TAG, "Using SDMMC peripheral");
    
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;  // 4-bit SD mode
    
    esp_err_t ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, 
                                             &mount_config, &card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem on SD card");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SD card (%s)", esp_err_to_name(ret));
        }
        return ret;
    }
    
    ESP_LOGI(TAG, "SD card mounted");
    sdmmc_card_print_info(stdout, card);
    
    return ESP_OK;
}

/**
 * @brief Mark OTA partition as valid after successful boot
 */
static void mark_ota_valid(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "Marking OTA partition as valid");
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "espTari v%s starting...", ESPTARI_VERSION_STRING);
    ESP_LOGI(TAG, "Build: %s %s", __DATE__, __TIME__);
    
    // Print chip info
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "ESP32-P4 with %d CPU cores, revision %d", 
             chip_info.cores, chip_info.revision);
    
    // Initialize NVS
    ESP_ERROR_CHECK(init_nvs());
    
    // Initialize SPIFFS for configuration
    ESP_ERROR_CHECK(init_spiffs());
    
    // Initialize SD card
    esp_err_t ret = init_sdcard();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card not available - some features disabled");
    }
    
    // Initialize component loader
    ret = loader_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize component loader");
    }
    
    // Initialize network manager (WiFi + Ethernet + mDNS)
    ret = esptari_net_init();
    if (ret == ESP_OK) {
        esptari_net_start();
        ESP_LOGI(TAG, "Waiting for network connectivity...");
        ret = esptari_net_wait_connected(15000);  // 15s timeout
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Network connected");
        } else {
            ESP_LOGW(TAG, "Network not available — web interface disabled");
        }
    } else {
        ESP_LOGW(TAG, "Network init failed: %s", esp_err_to_name(ret));
    }

    // Initialize core emulation framework
    esptari_core_init();

    // Initialize web server (if network is up)
    if (esptari_net_is_connected()) {
        esptari_web_init(CONFIG_ESPTARI_WEB_PORT);
    }

    // Mark OTA as successful if we got this far
    mark_ota_valid();
    
    ESP_LOGI(TAG, "Initialization complete");
    ESP_LOGI(TAG, "Web interface: http://esptari.local:%d", CONFIG_ESPTARI_WEB_PORT);

    // TODO: Load default machine profile (CONFIG_ESPTARI_DEFAULT_MACHINE)
    // TODO: Auto-start emulation if CONFIG_ESPTARI_AUTO_START
    
    // Main loop — keep alive
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
