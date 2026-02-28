/**
 * @file test_runner.c
 * @brief Test runner for esptari_loader component tests
 * 
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include "unity.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "test_runner";

void setUp(void)
{
    /* Called before each test */
}

void tearDown(void)
{
    /* Called after each test */
}

void app_main(void)
{
    /* Initialize NVS (may be needed by some tests) */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   espTari Loader Unit Tests");
    ESP_LOGI(TAG, "========================================");
    
    /* Short delay to allow serial monitor to connect */
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    UNITY_BEGIN();
    
    /* Unity will automatically discover and run all TEST_CASE functions */
    unity_run_all_tests();
    
    UNITY_END();
    
    ESP_LOGI(TAG, "Tests complete. Restarting in 5 seconds...");
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
}
