/**
 * @file sdcard_test_main.c
 * @brief EBIN Loader SD Card Integration Test
 *
 * Tests loading EBIN components from SD card:
 * 1. Mounts SD card at /sdcard
 * 2. Loads test_component.ebin from /sdcard/components/
 * 3. Tests the component interface functions
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_heap_caps.h"

#include "esptari_loader.h"
#include "component_api.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"

static const char *TAG = "sdcard_test";

#define MOUNT_POINT "/sdcard"
#define COMPONENT_PATH "/sdcard/components/test_component.ebin"

/* SD card pin configuration for ESP32-P4-NANO */
#define SD_CMD_PIN  44
#define SD_CLK_PIN  43
#define SD_D0_PIN   39
#define SD_D1_PIN   40
#define SD_D2_PIN   41
#define SD_D3_PIN   42

static sdmmc_card_t *s_card = NULL;

/**
 * @brief Mount SD card
 */
static esp_err_t mount_sdcard(void)
{
    ESP_LOGI(TAG, "Mounting SD card...");
    
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    
    /* Initialize LDO power for SD card on ESP32-P4 */
    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = 4,  /* LDO channel for SD card IO */
    };
    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;
    
    esp_err_t ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LDO power control: %s", esp_err_to_name(ret));
        return ret;
    }
    host.pwr_ctrl_handle = pwr_ctrl_handle;
    
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;  /* 4-bit bus */
    
    /* Configure GPIO pins for ESP32-P4-NANO */
    slot_config.clk = SD_CLK_PIN;
    slot_config.cmd = SD_CMD_PIN;
    slot_config.d0 = SD_D0_PIN;
    slot_config.d1 = SD_D1_PIN;
    slot_config.d2 = SD_D2_PIN;
    slot_config.d3 = SD_D3_PIN;
    
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    
    ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, 
                                             &mount_config, &s_card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SD card: %s", esp_err_to_name(ret));
        }
        return ret;
    }
    
    ESP_LOGI(TAG, "SD card mounted successfully");
    sdmmc_card_print_info(stdout, s_card);
    
    return ESP_OK;
}

/**
 * @brief Unmount SD card
 */
static void unmount_sdcard(void)
{
    if (s_card) {
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, s_card);
        ESP_LOGI(TAG, "SD card unmounted");
        s_card = NULL;
    }
}

/**
 * @brief List files in components directory
 */
static void list_components_dir(void)
{
    ESP_LOGI(TAG, "Listing /sdcard/components/:");
    
    DIR *dir = opendir("/sdcard/components");
    if (dir == NULL) {
        ESP_LOGW(TAG, "Cannot open /sdcard/components - does it exist?");
        ESP_LOGI(TAG, "Please create the directory and copy test_component.ebin to it");
        return;
    }
    
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        ESP_LOGI(TAG, "  %s", entry->d_name);
        count++;
    }
    closedir(dir);
    
    if (count == 0) {
        ESP_LOGW(TAG, "  (empty directory)");
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   EBIN Loader SD Card Test");
    ESP_LOGI(TAG, "========================================");
    
    /* Debug: Show available memory */
    ESP_LOGI(TAG, "MALLOC_CAP_EXEC free: %zu", heap_caps_get_free_size(MALLOC_CAP_EXEC));
    ESP_LOGI(TAG, "MALLOC_CAP_SPIRAM free: %zu", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    
    /* Mount SD card */
    esp_err_t err = mount_sdcard();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SD card mount failed, cannot continue");
        ESP_LOGI(TAG, "Make sure an SD card is inserted with components/ directory");
        return;
    }
    
    /* List what's in the components directory */
    list_components_dir();
    
    /* Check if test component exists */
    FILE *f = fopen(COMPONENT_PATH, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Test component not found: %s", COMPONENT_PATH);
        ESP_LOGI(TAG, "Copy test_component.ebin to SD card /components/ directory");
        unmount_sdcard();
        return;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    ESP_LOGI(TAG, "Found test component: %ld bytes", size);
    
    /* Initialize loader */
    ESP_LOGI(TAG, "Initializing loader...");
    err = loader_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Loader init failed: %s", esp_err_to_name(err));
        unmount_sdcard();
        return;
    }
    
    /* Load test component */
    ESP_LOGI(TAG, "Loading test component from SD card...");
    io_interface_t *io = NULL;
    err = loader_load_component(COMPONENT_PATH, COMPONENT_TYPE_IO, (void**)&io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load component: %s", esp_err_to_name(err));
        loader_shutdown();
        unmount_sdcard();
        return;
    }
    
    ESP_LOGI(TAG, "Component loaded successfully from SD card!");
    ESP_LOGI(TAG, "  Name: %s", io->name);
    ESP_LOGI(TAG, "  Interface version: 0x%08lX", (unsigned long)io->interface_version);
    
    /* Initialize component */
    ESP_LOGI(TAG, "Initializing component...");
    int init_result = io->init(NULL);
    ESP_LOGI(TAG, "  Init result: %d", init_result);
    
    /* Test read/write operations */
    ESP_LOGI(TAG, "Testing read/write...");
    
    /* Byte test */
    io->write_byte(0xFF0010, 0xAB);
    uint8_t rb = io->read_byte(0xFF0010);
    ESP_LOGI(TAG, "  Write 0xAB, Read 0x%02X - %s", rb, rb == 0xAB ? "PASS" : "FAIL");
    
    /* Word test */
    io->write_word(0xFF0020, 0x1234);
    uint16_t rw = io->read_word(0xFF0020);
    ESP_LOGI(TAG, "  Write 0x1234, Read 0x%04X - %s", rw, rw == 0x1234 ? "PASS" : "FAIL");
    
    /* Reset test */
    ESP_LOGI(TAG, "Testing reset...");
    io->reset();
    uint8_t after_reset = io->read_byte(0xFF0010);
    ESP_LOGI(TAG, "  After reset: 0x%02X - %s", after_reset, after_reset == 0x00 ? "PASS" : "FAIL");
    
    /* IRQ test */
    ESP_LOGI(TAG, "Testing IRQ functions...");
    bool irq = io->irq_pending();
    ESP_LOGI(TAG, "  IRQ pending: %s", irq ? "yes" : "no");
    
    /* Shutdown component */
    ESP_LOGI(TAG, "Shutting down component...");
    io->shutdown();
    
    /* Unload */
    ESP_LOGI(TAG, "Unloading component...");
    loader_unload_component(io);
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== SD Card EBIN Load Test Complete ===");
}
