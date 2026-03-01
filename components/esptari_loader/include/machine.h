/**
 * @file machine.h
 * @brief Machine profile configuration and management
 * 
 * Machine profiles define complete Atari system configurations,
 * specifying which components to load and how to configure them.
 * 
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "component_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of audio components per machine */
#define MACHINE_MAX_AUDIO   4

/** Maximum number of I/O components per machine */
#define MACHINE_MAX_IO      8

/**
 * @brief Component slot configuration
 */
typedef struct {
    char     file[64];      /**< Component filename (e.g., "cpu_68000.ebin") */
    uint32_t clock_hz;      /**< Clock frequency (0 = default) */
    char     role[16];      /**< Component role (e.g., "psg", "dma") */
    bool     optional;      /**< True if component is optional */
} machine_component_t;

/**
 * @brief Memory configuration
 */
typedef struct {
    uint32_t ram_size;      /**< RAM size in bytes */
    char     tos_file[64];  /**< TOS ROM filename */
    bool     tos_required;  /**< True if TOS is required */
} machine_memory_t;

/**
 * @brief Complete machine profile
 */
typedef struct {
    char     id[32];             /**< Machine identifier (e.g., "atari_ste") */
    char     display_name[64];   /**< Display name (e.g., "Atari STe") */
    char     description[256];   /**< Description text */
    uint16_t year;               /**< Release year */
    
    /* Component slots */
    machine_component_t cpu;     /**< CPU configuration */
    machine_component_t mmu;     /**< MMU configuration */
    machine_component_t video;   /**< Video configuration */
    machine_component_t unified; /**< Unified monolithic system component */
    machine_component_t audio[MACHINE_MAX_AUDIO];  /**< Audio components */
    int                 audio_count;               /**< Number of audio components */
    machine_component_t io[MACHINE_MAX_IO];        /**< I/O components */
    int                 io_count;                  /**< Number of I/O components */
    
    /* Memory configuration */
    machine_memory_t    memory;  /**< Memory configuration */
} machine_profile_t;

/**
 * @brief Loaded machine state
 */
typedef struct {
    machine_profile_t    profile;      /**< Active profile */
    cpu_interface_t     *cpu;          /**< Loaded CPU interface */
    video_interface_t   *video;        /**< Loaded video interface */
    system_interface_t  *system;       /**< Loaded unified system interface */
    audio_interface_t   *audio[MACHINE_MAX_AUDIO];  /**< Loaded audio interfaces */
    io_interface_t      *io[MACHINE_MAX_IO];        /**< Loaded I/O interfaces */
    uint8_t             *ram;          /**< Allocated RAM */
    uint8_t             *rom;          /**< Loaded TOS ROM */
    uint32_t             rom_size;     /**< ROM size */
    bus_interface_t      bus;          /**< Bus interface */
    bool                 running;      /**< Emulation running */
} machine_state_t;

/**
 * @brief Parse a machine profile from JSON file
 * 
 * @param[in]  path    Path to JSON profile file
 * @param[out] profile Profile structure to fill
 * 
 * @return ESP_OK on success
 */
esp_err_t machine_parse_profile(const char *path, machine_profile_t *profile);

/**
 * @brief Load a machine profile and all its components
 * 
 * @param[in] profile_name Profile name (e.g., "atari_ste")
 * 
 * @return ESP_OK on success
 */
esp_err_t machine_load(const char *profile_name);

/**
 * @brief Unload current machine and all components
 * 
 * @return ESP_OK on success
 */
esp_err_t machine_unload(void);

/**
 * @brief Get current machine state
 * 
 * @return Pointer to machine state, or NULL if not loaded
 */
machine_state_t* machine_get_state(void);

/**
 * @brief Get CPU interface of current machine
 * 
 * @return CPU interface, or NULL if not loaded
 */
cpu_interface_t* machine_get_cpu(void);

/**
 * @brief Get video interface of current machine
 * 
 * @return Video interface, or NULL if not loaded
 */
video_interface_t* machine_get_video(void);

/**
 * @brief Get audio interface of current machine
 * 
 * @param[in] index Audio component index
 * 
 * @return Audio interface, or NULL if not available
 */
audio_interface_t* machine_get_audio(int index);

/**
 * @brief Get I/O interface of current machine
 * 
 * @param[in] index I/O component index
 * 
 * @return I/O interface, or NULL if not available
 */
io_interface_t* machine_get_io(int index);

/**
 * @brief Get unified system interface of current machine
 *
 * @return System interface, or NULL if machine is not unified or not loaded
 */
system_interface_t* machine_get_system(void);

/**
 * @brief Hot-swap a component (e.g., accelerator board)
 * 
 * @param[in] type      Component type to swap
 * @param[in] filename  New component filename
 * 
 * @return ESP_OK on success
 */
esp_err_t machine_swap_component(int type, const char *filename);

/**
 * @brief List available machine profiles on SD card
 * 
 * @param[out] names     Array of profile names
 * @param[in]  max_count Maximum number of names
 * @param[out] count     Actual number found
 * 
 * @return ESP_OK on success
 */
esp_err_t machine_list_profiles(char **names, size_t max_count, size_t *count);

/**
 * @brief Reset the current machine
 * 
 * Resets all loaded components to their initial state.
 * 
 * @return ESP_OK on success
 */
esp_err_t machine_reset(void);

/**
 * @brief Run one frame of emulation
 * 
 * @param[in] cycles_per_frame Target cycles per frame
 * 
 * @return Actual cycles executed
 */
int machine_run_frame(int cycles_per_frame);

#ifdef __cplusplus
}
#endif
