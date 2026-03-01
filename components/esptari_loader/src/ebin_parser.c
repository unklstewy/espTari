/**
 * @file ebin_parser.c
 * @brief EBIN file parser implementation
 * 
 * Parses .ebin files from SD card, validates headers, and extracts
 * code/data sections for loading into PSRAM.
 * 
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"
#include "ebin_format.h"

static const char *TAG = "ebin_parser";

/**
 * @brief Validate EBIN header
 */
static esp_err_t validate_header(const ebin_header_t *header)
{
    /* Check magic number */
    if (header->magic != EBIN_MAGIC) {
        ESP_LOGE(TAG, "Invalid magic: 0x%08lX (expected 0x%08X)", 
                 (unsigned long)header->magic, EBIN_MAGIC);
        return ESP_ERR_INVALID_ARG;
    }
    
    /* Check version */
    if (header->version > EBIN_VERSION) {
        ESP_LOGE(TAG, "Unsupported version: %d (max %d)", 
                 header->version, EBIN_VERSION);
        return ESP_ERR_INVALID_VERSION;
    }
    
    /* Check type */
    if (header->type < EBIN_TYPE_CPU || header->type > EBIN_TYPE_SYSTEM) {
        ESP_LOGE(TAG, "Invalid component type: %d", header->type);
        return ESP_ERR_INVALID_ARG;
    }
    
    /* Sanity check sizes */
    if (header->code_size == 0) {
        ESP_LOGE(TAG, "Code size cannot be zero");
        return ESP_ERR_INVALID_SIZE;
    }
    
    /* Check code section doesn't overlap header */
    if (header->code_offset < sizeof(ebin_header_t)) {
        ESP_LOGE(TAG, "Code offset too small: %lu", 
                 (unsigned long)header->code_offset);
        return ESP_ERR_INVALID_ARG;
    }
    
    return ESP_OK;
}

/**
 * @brief Get component type name for logging
 */
static const char* type_name(uint16_t type)
{
    switch (type) {
        case EBIN_TYPE_CPU:   return "CPU";
        case EBIN_TYPE_VIDEO: return "Video";
        case EBIN_TYPE_AUDIO: return "Audio";
        case EBIN_TYPE_IO:    return "I/O";
        case EBIN_TYPE_SYSTEM:return "System";
        default:              return "Unknown";
    }
}

esp_err_t ebin_parse_file(const char *path, ebin_header_t *header)
{
    if (!path || !header) {
        return ESP_ERR_INVALID_ARG;
    }
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open: %s", path);
        return ESP_ERR_NOT_FOUND;
    }
    
    /* Read header */
    size_t read = fread(header, 1, sizeof(ebin_header_t), f);
    fclose(f);
    
    if (read != sizeof(ebin_header_t)) {
        ESP_LOGE(TAG, "Failed to read header: got %zu bytes", read);
        return ESP_ERR_INVALID_SIZE;
    }
    
    /* Validate */
    esp_err_t err = validate_header(header);
    if (err != ESP_OK) {
        return err;
    }
    
    ESP_LOGI(TAG, "EBIN: %s, type=%s, code=%luB, data=%luB, bss=%luB",
             path, type_name(header->type),
             (unsigned long)header->code_size,
             (unsigned long)header->data_size,
             (unsigned long)header->bss_size);
    
    return ESP_OK;
}

esp_err_t ebin_read_code(const char *path, const ebin_header_t *header, 
                          void *buffer, size_t buffer_size)
{
    if (!path || !header || !buffer) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (buffer_size < header->code_size) {
        ESP_LOGE(TAG, "Buffer too small: %zu < %lu", 
                 buffer_size, (unsigned long)header->code_size);
        return ESP_ERR_INVALID_SIZE;
    }
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        return ESP_ERR_NOT_FOUND;
    }
    
    /* Seek to code section */
    if (fseek(f, header->code_offset, SEEK_SET) != 0) {
        fclose(f);
        return ESP_ERR_INVALID_STATE;
    }
    
    /* Read code */
    size_t read = fread(buffer, 1, header->code_size, f);
    fclose(f);
    
    if (read != header->code_size) {
        ESP_LOGE(TAG, "Failed to read code: got %zu bytes", read);
        return ESP_ERR_INVALID_SIZE;
    }
    
    return ESP_OK;
}

esp_err_t ebin_read_data(const char *path, const ebin_header_t *header,
                          void *buffer, size_t buffer_size)
{
    if (!path || !header || !buffer) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (header->data_size == 0) {
        return ESP_OK;  /* No data section */
    }
    
    if (buffer_size < header->data_size) {
        ESP_LOGE(TAG, "Buffer too small: %zu < %lu",
                 buffer_size, (unsigned long)header->data_size);
        return ESP_ERR_INVALID_SIZE;
    }
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        return ESP_ERR_NOT_FOUND;
    }
    
    /* Seek to data section */
    if (fseek(f, header->data_offset, SEEK_SET) != 0) {
        fclose(f);
        return ESP_ERR_INVALID_STATE;
    }
    
    /* Read data */
    size_t read = fread(buffer, 1, header->data_size, f);
    fclose(f);
    
    if (read != header->data_size) {
        ESP_LOGE(TAG, "Failed to read data: got %zu bytes", read);
        return ESP_ERR_INVALID_SIZE;
    }
    
    return ESP_OK;
}

esp_err_t ebin_read_relocations(const char *path, const ebin_header_t *header,
                                 ebin_reloc_t *relocs, size_t max_count,
                                 size_t *actual_count)
{
    if (!path || !header || !relocs || !actual_count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *actual_count = 0;
    
    if (header->reloc_count == 0) {
        return ESP_OK;  /* No relocations */
    }
    
    size_t count = header->reloc_count;
    if (count > max_count) {
        ESP_LOGW(TAG, "Truncating relocations: %zu > %zu", count, max_count);
        count = max_count;
    }
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        return ESP_ERR_NOT_FOUND;
    }
    
    /* Seek to relocation table */
    if (fseek(f, header->reloc_offset, SEEK_SET) != 0) {
        fclose(f);
        return ESP_ERR_INVALID_STATE;
    }
    
    /* Read relocations */
    size_t read = fread(relocs, sizeof(ebin_reloc_t), count, f);
    fclose(f);
    
    if (read != count) {
        ESP_LOGE(TAG, "Failed to read relocations: got %zu entries", read);
        return ESP_ERR_INVALID_SIZE;
    }
    
    *actual_count = count;
    ESP_LOGD(TAG, "Read %zu relocations", count);
    
    return ESP_OK;
}

uint32_t ebin_total_size(const ebin_header_t *header)
{
    if (!header) {
        return 0;
    }
    
    /* Code + Data + BSS, aligned to 8 bytes */
    uint32_t size = header->code_size + header->data_size + header->bss_size;
    return (size + 7) & ~7;
}

uint32_t ebin_required_interface_version(const ebin_header_t *header)
{
    if (!header) {
        return 0;
    }
    return header->interface_version;
}

bool ebin_check_interface_compatible(uint32_t component_version, 
                                      uint32_t required_version)
{
    /* Major version must match exactly */
    uint16_t comp_major = (component_version >> 16) & 0xFFFF;
    uint16_t req_major = (required_version >> 16) & 0xFFFF;
    
    if (comp_major != req_major) {
        return false;
    }
    
    /* Component minor version must be >= required */
    uint16_t comp_minor = component_version & 0xFFFF;
    uint16_t req_minor = required_version & 0xFFFF;
    
    return comp_minor >= req_minor;
}
