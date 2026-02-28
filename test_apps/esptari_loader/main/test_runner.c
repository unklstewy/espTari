/**
 * @file test_runner.c
 * @brief Test runner for esptari_loader component tests
 * 
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ebin_format.h"
#include "esptari_loader.h"
#include "component_api.h"

static const char *TAG = "test_runner";

void setUp(void)
{
    /* Called before each test */
}

void tearDown(void)
{
    /* Called after each test */
}

/*===========================================================================*/
/* EBIN Parser Tests (inline)                                                */
/*===========================================================================*/

static void test_ebin_magic_number(void)
{
    TEST_ASSERT_EQUAL_HEX32(0x4E494245, EBIN_MAGIC);
    uint32_t expected = ('N' << 24) | ('I' << 16) | ('B' << 8) | 'E';
    TEST_ASSERT_EQUAL_HEX32(expected, EBIN_MAGIC);
}

static void test_ebin_header_size(void)
{
    TEST_ASSERT_EQUAL(60, sizeof(ebin_header_t));
    TEST_ASSERT_EQUAL(60, EBIN_HEADER_SIZE);
}

static void test_ebin_reloc_size(void)
{
    TEST_ASSERT_EQUAL(8, sizeof(ebin_reloc_t));
}

static void test_component_types_match(void)
{
    TEST_ASSERT_EQUAL(COMPONENT_TYPE_CPU, EBIN_TYPE_CPU);
    TEST_ASSERT_EQUAL(COMPONENT_TYPE_VIDEO, EBIN_TYPE_VIDEO);
    TEST_ASSERT_EQUAL(COMPONENT_TYPE_AUDIO, EBIN_TYPE_AUDIO);
    TEST_ASSERT_EQUAL(COMPONENT_TYPE_IO, EBIN_TYPE_IO);
}

static void test_ebin_flags_distinct(void)
{
    TEST_ASSERT_EQUAL_HEX32(0x01, EBIN_FLAG_HAS_SYMBOLS);
    TEST_ASSERT_EQUAL_HEX32(0x02, EBIN_FLAG_DEBUG);
    TEST_ASSERT_EQUAL_HEX32(0x04, EBIN_FLAG_COMPRESSED);
    uint32_t all_flags = EBIN_FLAG_HAS_SYMBOLS | EBIN_FLAG_DEBUG | EBIN_FLAG_COMPRESSED;
    TEST_ASSERT_EQUAL_HEX32(0x07, all_flags);
}

/*===========================================================================*/
/* Relocator Tests                                                           */
/*===========================================================================*/

extern esp_err_t relocator_apply(const ebin_reloc_t *relocs, 
                                  size_t reloc_count,
                                  void *code_base,
                                  void *data_base,
                                  uint32_t code_size,
                                  uint32_t data_size);

static void test_relocator_absolute(void)
{
    uint8_t code[64];
    memset(code, 0, sizeof(code));
    
    uint32_t original_addr = 0x00000100;
    memcpy(&code[4], &original_addr, sizeof(uint32_t));
    
    ebin_reloc_t reloc = {
        .offset = 4,
        .type = EBIN_RELOC_ABSOLUTE,
        .section = 0,
        .reserved = 0
    };
    
    esp_err_t err = relocator_apply(&reloc, 1, code, NULL, sizeof(code), 0);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    uint32_t relocated_addr;
    memcpy(&relocated_addr, &code[4], sizeof(uint32_t));
    uint32_t expected = original_addr + (uint32_t)(uintptr_t)code;
    TEST_ASSERT_EQUAL_HEX32(expected, relocated_addr);
}

static void test_relocator_zero_relocs(void)
{
    uint8_t code[16];
    memset(code, 0xAA, sizeof(code));
    
    esp_err_t err = relocator_apply(NULL, 0, code, NULL, sizeof(code), 0);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    for (int i = 0; i < 16; i++) {
        TEST_ASSERT_EQUAL_HEX8(0xAA, code[i]);
    }
}

/*===========================================================================*/
/* Registry Tests                                                            */
/*===========================================================================*/

static void test_loader_init(void)
{
    esp_err_t err = loader_init();
    TEST_ASSERT_EQUAL(ESP_OK, err);
    loader_shutdown();
}

static void test_loader_list_empty(void)
{
    loader_init();
    
    component_info_t infos[8];
    size_t count = 99;
    
    esp_err_t err = loader_list_components(infos, 8, &count);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(0, count);
    
    loader_shutdown();
}

static void test_loader_invalid_args(void)
{
    loader_init();
    
    void *interface = NULL;
    esp_err_t err = loader_load_component(NULL, COMPONENT_TYPE_CPU, &interface);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    
    err = loader_load_component("/test.ebin", COMPONENT_TYPE_CPU, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    
    loader_shutdown();
}

/*===========================================================================*/
/* Test Task                                                                 */
/*===========================================================================*/

static void test_task(void *pvParameters)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   espTari Loader Unit Tests");
    ESP_LOGI(TAG, "========================================");
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    UNITY_BEGIN();
    
    /* EBIN Format Tests */
    RUN_TEST(test_ebin_magic_number);
    RUN_TEST(test_ebin_header_size);
    RUN_TEST(test_ebin_reloc_size);
    RUN_TEST(test_component_types_match);
    RUN_TEST(test_ebin_flags_distinct);
    
    /* Relocator Tests */
    RUN_TEST(test_relocator_absolute);
    RUN_TEST(test_relocator_zero_relocs);
    
    /* Registry Tests */
    RUN_TEST(test_loader_init);
    RUN_TEST(test_loader_list_empty);
    RUN_TEST(test_loader_invalid_args);
    
    UNITY_END();
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   Tests Complete!");
    ESP_LOGI(TAG, "========================================");
    
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    
    xTaskCreate(test_task, "test_task", 8192, NULL, 5, NULL);
}
