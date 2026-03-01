/**
 * @file loader.c
 * @brief Component loader implementation
 * 
 * Loads .ebin components from SD card into PSRAM, applies relocations,
 * and returns the component interface.
 * 
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esptari_loader.h"
#include "ebin_format.h"

static const char *TAG = "loader";

/* Maximum number of loaded components */
#define MAX_LOADED_COMPONENTS 16

/* Maximum relocations per component */
#define MAX_RELOCATIONS 4096

/* EBIN parser functions (from ebin_parser.c) */
extern esp_err_t ebin_parse_file(const char *path, ebin_header_t *header);
extern esp_err_t ebin_read_code(const char *path, const ebin_header_t *header,
                                 void *buffer, size_t buffer_size);
extern esp_err_t ebin_read_data(const char *path, const ebin_header_t *header,
                                 void *buffer, size_t buffer_size);
extern esp_err_t ebin_read_relocations(const char *path, const ebin_header_t *header,
                                        ebin_reloc_t *relocs, size_t max_count,
                                        size_t *actual_count);
extern uint32_t ebin_total_size(const ebin_header_t *header);
extern bool ebin_check_interface_compatible(uint32_t component_version,
                                             uint32_t required_version);

/* Relocator functions (from relocator.c) */
extern esp_err_t relocator_apply(const ebin_reloc_t *relocs, size_t reloc_count,
                                  void *code_base, void *data_base,
                                  uint32_t code_size, uint32_t data_size);

/* Loaded component registry */
static ebin_loaded_t s_components[MAX_LOADED_COMPONENTS];
static int s_component_count = 0;
static bool s_initialized = false;

/* Expected interface versions */
static const uint32_t s_interface_versions[] = {
    0,                  /* Invalid */
    CPU_INTERFACE_V1,   /* CPU */
    VIDEO_INTERFACE_V1, /* Video */
    AUDIO_INTERFACE_V1, /* Audio */
    IO_INTERFACE_V1,    /* I/O */
    SYSTEM_INTERFACE_V1,/* System */
};

esp_err_t loader_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }
    
    memset(s_components, 0, sizeof(s_components));
    s_component_count = 0;
    
    /* Check PSRAM availability */
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "Loader initialized, PSRAM free: %zu bytes", psram_free);
    
    if (psram_free < 1024 * 1024) {
        ESP_LOGW(TAG, "Low PSRAM available, component loading may fail");
    }
    
    s_initialized = true;
    return ESP_OK;
}

void loader_shutdown(void)
{
    if (!s_initialized) {
        return;
    }
    
    /* Unload all components */
    for (int i = 0; i < s_component_count; i++) {
        if (s_components[i].code_base) {
            heap_caps_free(s_components[i].code_base);
        }
    }
    
    memset(s_components, 0, sizeof(s_components));
    s_component_count = 0;
    s_initialized = false;
    
    ESP_LOGI(TAG, "Loader shutdown");
}

/**
 * @brief Find free slot in component registry
 */
static int find_free_slot(void)
{
    for (int i = 0; i < MAX_LOADED_COMPONENTS; i++) {
        if (s_components[i].code_base == NULL) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Find component by interface pointer
 */
static int find_component_by_interface(void *interface)
{
    for (int i = 0; i < MAX_LOADED_COMPONENTS; i++) {
        if (s_components[i].interface == interface) {
            return i;
        }
    }
    return -1;
}

esp_err_t loader_load_component(const char *path,
                                 component_type_t type,
                                 void **interface_out)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!path || !interface_out) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *interface_out = NULL;
    
    /* Find free slot */
    int slot = find_free_slot();
    if (slot < 0) {
        ESP_LOGE(TAG, "No free component slots");
        return ESP_ERR_NO_MEM;
    }
    
    ebin_loaded_t *comp = &s_components[slot];
    memset(comp, 0, sizeof(ebin_loaded_t));
    
    /* Parse header */
    esp_err_t err = ebin_parse_file(path, &comp->header);
    if (err != ESP_OK) {
        return err;
    }
    
    /* Verify type matches */
    if (comp->header.type != (uint16_t)type) {
        ESP_LOGE(TAG, "Type mismatch: got %d, expected %d",
                 comp->header.type, type);
        return ESP_ERR_INVALID_ARG;
    }
    
    /* Check interface version compatibility */
    uint32_t required = s_interface_versions[type];
    if (!ebin_check_interface_compatible(required, comp->header.interface_version)) {
        ESP_LOGE(TAG, "Interface version mismatch: 0x%08lX vs 0x%08lX",
                 (unsigned long)required, 
                 (unsigned long)comp->header.interface_version);
        return ESP_ERR_INVALID_VERSION;
    }
    
    /* Calculate total size needed */
    uint32_t total_size = ebin_total_size(&comp->header);
    
    ESP_LOGI(TAG, "Loading component: %s (%lu bytes)", path, 
             (unsigned long)total_size);
    
    /* Allocate memory for code + data + bss */
    /* Note: We need MALLOC_CAP_EXEC for code to be executable.
     * On ESP32-P4, only internal RAM (L2MEM) supports execution.
     * Don't combine with MALLOC_CAP_8BIT as it may not be available.
     */
    void *mem = heap_caps_malloc(total_size, MALLOC_CAP_EXEC);
    if (!mem) {
        /* Fall back to SPIRAM - will NOT be executable */
        ESP_LOGW(TAG, "No executable memory, trying SPIRAM (will NOT be executable!)");
        mem = heap_caps_malloc(total_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!mem) {
        ESP_LOGE(TAG, "Failed to allocate %lu bytes in PSRAM",
                 (unsigned long)total_size);
        return ESP_ERR_NO_MEM;
    }
    
    /* Set up section pointers */
    comp->code_base = mem;
    comp->data_base = (uint8_t*)mem + comp->header.code_size;
    comp->bss_base = (uint8_t*)comp->data_base + comp->header.data_size;
    
    /* Clear BSS */
    memset(comp->bss_base, 0, comp->header.bss_size);
    
    /* Read code section */
    err = ebin_read_code(path, &comp->header, comp->code_base, 
                          comp->header.code_size);
    if (err != ESP_OK) {
        heap_caps_free(mem);
        comp->code_base = NULL;
        return err;
    }
    
    /* Read data section */
    if (comp->header.data_size > 0) {
        err = ebin_read_data(path, &comp->header, comp->data_base,
                              comp->header.data_size);
        if (err != ESP_OK) {
            heap_caps_free(mem);
            comp->code_base = NULL;
            return err;
        }
    }
    
    /* Read and apply relocations */
    if (comp->header.reloc_count > 0) {
        ebin_reloc_t *relocs = malloc(comp->header.reloc_count * sizeof(ebin_reloc_t));
        if (!relocs) {
            heap_caps_free(mem);
            comp->code_base = NULL;
            return ESP_ERR_NO_MEM;
        }
        
        size_t actual_count;
        err = ebin_read_relocations(path, &comp->header, relocs,
                                     comp->header.reloc_count, &actual_count);
        if (err == ESP_OK) {
            err = relocator_apply(relocs, actual_count,
                                   comp->code_base, comp->data_base,
                                   comp->header.code_size, comp->header.data_size);
        }
        
        free(relocs);
        
        if (err != ESP_OK) {
            heap_caps_free(mem);
            comp->code_base = NULL;
            return err;
        }
    }

    /* Synchronize memory and instruction cache before executing loaded code.
     * This is required because we loaded code into memory via data writes,
     * and both the data cache and instruction cache may have stale data.
     * On ESP32-P4, we must:
     * 1. Complete all pending memory writes with fence
     * 2. Write back data cache to memory with cache_hal_writeback_addr
     * 3. Invalidate instruction cache with fence.i
     */
    __asm__ __volatile__("fence rw, rw" ::: "memory");
    extern void cache_hal_writeback_addr(uint32_t vaddr, uint32_t size);
    cache_hal_writeback_addr((uint32_t)comp->code_base, comp->header.code_size);
    __asm__ __volatile__("fence.i" ::: "memory");
    
    /* Call entry point to get interface */
    component_entry_fn entry = (component_entry_fn)((uint8_t*)comp->code_base + 
                                                      comp->header.entry_offset);
    comp->interface = entry();
    
    if (!comp->interface) {
        ESP_LOGE(TAG, "Component entry returned NULL");
        heap_caps_free(mem);
        comp->code_base = NULL;
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    /* Store path */
    strncpy(comp->path, path, sizeof(comp->path) - 1);
    
    /* Update count */
    if (slot >= s_component_count) {
        s_component_count = slot + 1;
    }
    
    *interface_out = comp->interface;
    
    ESP_LOGI(TAG, "Component loaded at 0x%08lX", (unsigned long)(uintptr_t)mem);
    
    return ESP_OK;
}

esp_err_t loader_unload_component(void *interface)
{
    if (!s_initialized || !interface) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int slot = find_component_by_interface(interface);
    if (slot < 0) {
        ESP_LOGE(TAG, "Component not found");
        return ESP_ERR_NOT_FOUND;
    }
    
    ebin_loaded_t *comp = &s_components[slot];
    
    ESP_LOGI(TAG, "Unloading component: %s", comp->path);
    
    /* Free PSRAM */
    if (comp->code_base) {
        heap_caps_free(comp->code_base);
    }
    
    /* Clear slot */
    memset(comp, 0, sizeof(ebin_loaded_t));
    
    return ESP_OK;
}

esp_err_t loader_get_info(void *interface, component_info_t *info)
{
    if (!interface || !info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int slot = find_component_by_interface(interface);
    if (slot < 0) {
        return ESP_ERR_NOT_FOUND;
    }
    
    ebin_loaded_t *comp = &s_components[slot];
    
    info->path = comp->path;
    info->type = (component_type_t)comp->header.type;
    info->interface_version = comp->header.interface_version;
    info->code_size = comp->header.code_size;
    info->data_size = comp->header.data_size;
    info->base_addr = comp->code_base;
    
    /* Get name from interface */
    switch (comp->header.type) {
        case EBIN_TYPE_CPU:
            info->name = ((cpu_interface_t*)interface)->name;
            break;
        case EBIN_TYPE_VIDEO:
            info->name = ((video_interface_t*)interface)->name;
            break;
        case EBIN_TYPE_AUDIO:
            info->name = ((audio_interface_t*)interface)->name;
            break;
        case EBIN_TYPE_IO:
            info->name = ((io_interface_t*)interface)->name;
            break;
        case EBIN_TYPE_SYSTEM:
            info->name = ((system_interface_t*)interface)->name;
            break;
        default:
            info->name = "Unknown";
    }
    
    return ESP_OK;
}

esp_err_t loader_list_components(component_info_t *infos,
                                  size_t max_count,
                                  size_t *count)
{
    if (!infos || !count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *count = 0;
    
    for (int i = 0; i < s_component_count && *count < max_count; i++) {
        if (s_components[i].code_base) {
            loader_get_info(s_components[i].interface, &infos[*count]);
            (*count)++;
        }
    }
    
    return ESP_OK;
}

esp_err_t loader_scan_components(component_type_t type,
                                  char **paths,
                                  size_t max_count,
                                  size_t *count)
{
    if (!paths || !count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *count = 0;
    
    /* Determine directory based on type */
    const char *subdir;
    switch (type) {
        case COMPONENT_TYPE_CPU:   subdir = "cpu"; break;
        case COMPONENT_TYPE_VIDEO: subdir = "video"; break;
        case COMPONENT_TYPE_AUDIO: subdir = "audio"; break;
        case COMPONENT_TYPE_IO:    subdir = "io"; break;
        case COMPONENT_TYPE_SYSTEM:subdir = "system"; break;
        default:                   subdir = ""; break;
    }
    
    char dirpath[128];
    if (type == 0) {
        snprintf(dirpath, sizeof(dirpath), "/sdcard/cores");
    } else {
        snprintf(dirpath, sizeof(dirpath), "/sdcard/cores/%s", subdir);
    }
    
    DIR *dir = opendir(dirpath);
    if (!dir) {
        ESP_LOGW(TAG, "Cannot open directory: %s", dirpath);
        return ESP_ERR_NOT_FOUND;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && *count < max_count) {
        /* Check for .ebin extension */
        size_t len = strlen(entry->d_name);
        if (len > 5 && strcmp(entry->d_name + len - 5, ".ebin") == 0) {
            /* Allocate and build full path */
            size_t path_len = strlen(dirpath) + len + 2;
            paths[*count] = malloc(path_len);
            if (paths[*count]) {
                snprintf(paths[*count], path_len, "%s/%s", dirpath, entry->d_name);
                (*count)++;
            }
        }
    }
    
    closedir(dir);
    
    ESP_LOGI(TAG, "Found %zu components in %s", *count, dirpath);
    
    return ESP_OK;
}
