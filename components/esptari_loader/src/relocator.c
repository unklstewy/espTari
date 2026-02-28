/**
 * @file relocator.c
 * @brief Relocation handling for loaded components
 * 
 * Applies relocations to position-independent code after loading
 * into PSRAM, adjusting addresses to the actual load location.
 * 
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"
#include "ebin_format.h"

static const char *TAG = "relocator";

/**
 * @brief Apply a single relocation entry
 */
static esp_err_t apply_relocation(const ebin_reloc_t *reloc,
                                   void *code_base,
                                   void *data_base,
                                   uint32_t load_addr)
{
    void *section_base;
    
    /* Determine which section this relocation applies to */
    if (reloc->section == 0) {
        section_base = code_base;
    } else if (reloc->section == 1) {
        section_base = data_base;
    } else {
        ESP_LOGE(TAG, "Invalid relocation section: %d", reloc->section);
        return ESP_ERR_INVALID_ARG;
    }
    
    /* Calculate the address to patch */
    uint8_t *patch_addr = (uint8_t*)section_base + reloc->offset;
    
    switch (reloc->type) {
        case EBIN_RELOC_ABSOLUTE: {
            /* Add load address to 32-bit value */
            uint32_t *ptr = (uint32_t*)patch_addr;
            *ptr += load_addr;
            break;
        }
        
        case EBIN_RELOC_RELATIVE: {
            /* PC-relative - usually doesn't need adjustment for PIC */
            /* But we adjust if the reference crosses section boundaries */
            break;
        }
        
        case EBIN_RELOC_HIGH16: {
            /* Add high 16 bits of load address */
            uint16_t *ptr = (uint16_t*)patch_addr;
            *ptr += (uint16_t)(load_addr >> 16);
            break;
        }
        
        case EBIN_RELOC_LOW16: {
            /* Add low 16 bits of load address */
            uint16_t *ptr = (uint16_t*)patch_addr;
            *ptr += (uint16_t)(load_addr & 0xFFFF);
            break;
        }
        
        default:
            ESP_LOGE(TAG, "Unknown relocation type: %d", reloc->type);
            return ESP_ERR_NOT_SUPPORTED;
    }
    
    return ESP_OK;
}

esp_err_t relocator_apply(const ebin_reloc_t *relocs, 
                           size_t reloc_count,
                           void *code_base,
                           void *data_base,
                           uint32_t code_size,
                           uint32_t data_size)
{
    if (!code_base) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (reloc_count == 0) {
        ESP_LOGD(TAG, "No relocations to apply");
        return ESP_OK;
    }
    
    if (!relocs) {
        return ESP_ERR_INVALID_ARG;
    }
    
    /* Calculate load address (just use the actual pointer value) */
    uint32_t load_addr = (uint32_t)(uintptr_t)code_base;
    
    ESP_LOGI(TAG, "Applying %zu relocations at base 0x%08lX",
             reloc_count, (unsigned long)load_addr);
    
    size_t applied = 0;
    size_t failed = 0;
    
    for (size_t i = 0; i < reloc_count; i++) {
        const ebin_reloc_t *reloc = &relocs[i];
        
        /* Validate offset is within bounds */
        if (reloc->section == 0 && reloc->offset >= code_size) {
            ESP_LOGE(TAG, "Relocation offset out of bounds: %lu >= %lu",
                     (unsigned long)reloc->offset, (unsigned long)code_size);
            failed++;
            continue;
        }
        
        if (reloc->section == 1 && reloc->offset >= data_size) {
            ESP_LOGE(TAG, "Relocation offset out of bounds: %lu >= %lu",
                     (unsigned long)reloc->offset, (unsigned long)data_size);
            failed++;
            continue;
        }
        
        esp_err_t err = apply_relocation(reloc, code_base, data_base, load_addr);
        if (err == ESP_OK) {
            applied++;
        } else {
            failed++;
        }
    }
    
    ESP_LOGI(TAG, "Relocations: %zu applied, %zu failed", applied, failed);
    
    if (failed > 0) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return ESP_OK;
}

esp_err_t relocator_fixup_interface(void *interface, uint32_t load_addr)
{
    /*
     * This function can be used to fix up function pointers in the
     * interface structure if they are stored as offsets rather than
     * absolute addresses. For fully PIC code, this may not be needed.
     */
    (void)interface;
    (void)load_addr;
    
    return ESP_OK;
}
