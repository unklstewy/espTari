/**
 * @file integration_test_main.c
 * @brief Integration test for EBIN component loading
 * 
 * This test app:
 * 1. Mounts SPIFFS containing test_component.ebin
 * 2. Loads the component using the loader
 * 3. Calls interface functions to verify it works
 * 4. Unloads the component
 * 
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "esp_heap_caps.h"
#include "esptari_loader.h"
#include "component_api.h"

static const char *TAG = "integration_test";

/**
 * @brief Mount SPIFFS partition containing components
 */
static esp_err_t mount_spiffs(void)
{
    ESP_LOGI(TAG, "Mounting SPIFFS...");
    
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/components",
        .partition_label = "components",
        .max_files = 5,
        .format_if_mount_failed = false
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount SPIFFS");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "SPIFFS partition not found");
        } else {
            ESP_LOGE(TAG, "SPIFFS init failed: %s", esp_err_to_name(ret));
        }
        return ret;
    }
    
    size_t total = 0, used = 0;
    esp_spiffs_info("components", &total, &used);
    ESP_LOGI(TAG, "SPIFFS mounted: %zu/%zu bytes used", used, total);
    
    return ESP_OK;
}

/**
 * @brief Run the integration test
 */
static void run_test(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   EBIN Loader Integration Test");
    ESP_LOGI(TAG, "========================================");
    
    /* Debug: Show available memory */
    ESP_LOGI(TAG, "MALLOC_CAP_EXEC free: %zu", heap_caps_get_free_size(MALLOC_CAP_EXEC));
    ESP_LOGI(TAG, "MALLOC_CAP_32BIT free: %zu", heap_caps_get_free_size(MALLOC_CAP_32BIT));
    void *exec_test = heap_caps_malloc(1024, MALLOC_CAP_EXEC);
    ESP_LOGI(TAG, "Test EXEC alloc (1KB): %p", exec_test);
    if (exec_test) heap_caps_free(exec_test);
    
    /* Mount SPIFFS */
    esp_err_t err = mount_spiffs();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed, cannot continue");
        return;
    }
    
    /* Initialize loader */
    ESP_LOGI(TAG, "Initializing loader...");
    err = loader_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Loader init failed: %s", esp_err_to_name(err));
        return;
    }
    
    /* Load test component */
    ESP_LOGI(TAG, "Loading test component...");
    io_interface_t *io = NULL;
    err = loader_load_component("/components/test_component.ebin", 
                                 COMPONENT_TYPE_IO, 
                                 (void**)&io);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Component load failed: %s", esp_err_to_name(err));
        loader_shutdown();
        return;
    }
    
    ESP_LOGI(TAG, "Component loaded successfully!");
    ESP_LOGI(TAG, "  Name: %s", io->name);
    ESP_LOGI(TAG, "  Interface version: 0x%08lX", (unsigned long)io->interface_version);
    
    /* Initialize component */
    ESP_LOGI(TAG, "Initializing component...");
    int init_result = io->init(NULL);
    ESP_LOGI(TAG, "  Init result: %d", init_result);
    
    /* Test read/write */
    ESP_LOGI(TAG, "Testing read/write...");
    io->write_byte(0xFF0010, 0xAB);
    uint8_t val = io->read_byte(0xFF0010);
    ESP_LOGI(TAG, "  Write 0xAB, Read 0x%02X - %s", 
             val, val == 0xAB ? "PASS" : "FAIL");
    
    io->write_word(0xFF0020, 0x1234);
    uint16_t wval = io->read_word(0xFF0020);
    ESP_LOGI(TAG, "  Write 0x1234, Read 0x%04X - %s",
             wval, wval == 0x1234 ? "PASS" : "FAIL");
    
    /* Test reset */
    ESP_LOGI(TAG, "Testing reset...");
    io->reset();
    val = io->read_byte(0xFF0010);
    ESP_LOGI(TAG, "  After reset: 0x%02X - %s",
             val, val == 0x00 ? "PASS" : "FAIL");
    
    /* Test IRQ functions */
    ESP_LOGI(TAG, "Testing IRQ functions...");
    bool irq = io->irq_pending();
    ESP_LOGI(TAG, "  IRQ pending: %s", irq ? "yes" : "no");
    
    /* Shutdown component */
    ESP_LOGI(TAG, "Shutting down component...");
    io->shutdown();
    
    /* Unload component */
    ESP_LOGI(TAG, "Unloading component...");
    err = loader_unload_component(io);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Component unloaded successfully");
    } else {
        ESP_LOGE(TAG, "Unload failed: %s", esp_err_to_name(err));
    }
    
    /* Shutdown loader */
    loader_shutdown();
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   Integration Test Complete!");
    ESP_LOGI(TAG, "========================================");
}

void app_main(void)
{
    /* Initialize NVS */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    
    /* Run test after short delay */
    vTaskDelay(pdMS_TO_TICKS(500));
    run_test();
    
    /* Keep running */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

