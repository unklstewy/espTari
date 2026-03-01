/**
 * @file esptari_loader.h
 * @brief espTari Dynamic Component Loader - Public API
 * 
 * This module provides dynamic loading of emulation components (CPU, video,
 * audio, I/O) from SD card into PSRAM at runtime. Components are stored as
 * .ebin (ESP Binary) files containing position-independent code.
 * 
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "component_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Component types
 */
typedef enum {
    COMPONENT_TYPE_CPU    = 1,  /**< CPU emulation (68000, 68030, etc.) */
    COMPONENT_TYPE_VIDEO  = 2,  /**< Video chip (Shifter, VIDEL) */
    COMPONENT_TYPE_AUDIO  = 3,  /**< Audio chip (YM2149, DMA Sound) */
    COMPONENT_TYPE_IO     = 4,  /**< I/O chips (MFP, Blitter, ACIA) */
    COMPONENT_TYPE_SYSTEM = 5,  /**< Unified monolithic system component */
} component_type_t;

/**
 * @brief Component information structure
 */
typedef struct {
    const char       *name;             /**< Component name */
    const char       *path;             /**< Path loaded from */
    component_type_t  type;             /**< Component type */
    uint32_t          interface_version;/**< Interface version */
    uint32_t          code_size;        /**< Code section size */
    uint32_t          data_size;        /**< Data section size */
    void             *base_addr;        /**< PSRAM base address */
} component_info_t;

/**
 * @brief Initialize the component loader
 * 
 * Must be called before any other loader functions. Initializes
 * the PSRAM allocator and component registry.
 * 
 * @return ESP_OK on success
 */
esp_err_t loader_init(void);

/**
 * @brief Shutdown the component loader
 * 
 * Unloads all components and frees resources.
 */
void loader_shutdown(void);

/**
 * @brief Load a component from SD card
 * 
 * Loads an .ebin file from the specified path, performs relocation,
 * and returns the component interface.
 * 
 * @param[in]  path          Path to .ebin file (e.g., "/sdcard/cores/cpu_68000.ebin")
 * @param[in]  type          Expected component type
 * @param[out] interface_out Pointer to receive interface (cast to appropriate type)
 * 
 * @return ESP_OK on success
 * @return ESP_ERR_NO_MEM if PSRAM allocation fails
 * @return ESP_ERR_NOT_FOUND if file not found
 * @return ESP_ERR_INVALID_ARG if type mismatch
 * @return ESP_ERR_INVALID_VERSION if interface version incompatible
 */
esp_err_t loader_load_component(const char *path, 
                                 component_type_t type, 
                                 void **interface_out);

/**
 * @brief Unload a component
 * 
 * Calls the component's shutdown function and frees PSRAM.
 * 
 * @param[in] interface Component interface to unload
 * 
 * @return ESP_OK on success
 */
esp_err_t loader_unload_component(void *interface);

/**
 * @brief Get information about a loaded component
 * 
 * @param[in]  interface Component interface
 * @param[out] info      Information structure to fill
 * 
 * @return ESP_OK on success
 */
esp_err_t loader_get_info(void *interface, component_info_t *info);

/**
 * @brief List all loaded components
 * 
 * @param[out] infos     Array to fill with component info
 * @param[in]  max_count Maximum number of entries
 * @param[out] count     Actual number of loaded components
 * 
 * @return ESP_OK on success
 */
esp_err_t loader_list_components(component_info_t *infos, 
                                  size_t max_count, 
                                  size_t *count);

/**
 * @brief Scan SD card for available components
 * 
 * @param[in]  type      Component type to scan for, or 0 for all
 * @param[out] paths     Array of paths to fill
 * @param[in]  max_count Maximum number of paths
 * @param[out] count     Actual number of components found
 * 
 * @return ESP_OK on success
 */
esp_err_t loader_scan_components(component_type_t type,
                                  char **paths,
                                  size_t max_count,
                                  size_t *count);

/**
 * @brief Check whether menuconfig-driven unified profile mode is enabled
 *
 * @return true if unified mode is enabled
 */
bool esptari_loader_unified_enabled(void);

/**
 * @brief Get machine profile name resolved from menuconfig unified settings
 *
 * Returns a machine profile identifier (without .json extension), such as
 * "mega_ste". In non-unified mode this falls back to
 * CONFIG_ESPTARI_DEFAULT_MACHINE.
 *
 * @return Profile name string (static storage)
 */
const char *esptari_loader_get_resolved_profile_name(void);

/**
 * @brief Log resolved unified configuration and capability matrix
 */
void esptari_loader_log_unified_config(void);

#ifdef __cplusplus
}
#endif
