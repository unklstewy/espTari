/**
 * @file registry.c
 * @brief Component registry for tracking loaded components
 * 
 * Provides centralized registration and lookup of loaded components
 * with lifecycle management callbacks.
 * 
 * SPDX-License-Identifier: MIT
 */

#include <string.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esptari_loader.h"
#include "component_api.h"

static const char *TAG = "registry";

/* Registry entry */
typedef struct {
    void            *interface;     /* Component interface */
    component_type_t type;          /* Component type */
    char             name[32];      /* Component name */
    char             role[16];      /* Component role (e.g., "psg") */
    bool             initialized;   /* init() called */
} registry_entry_t;

/* Maximum registry entries */
#define MAX_REGISTRY_ENTRIES 32

/* Registry storage */
static registry_entry_t s_registry[MAX_REGISTRY_ENTRIES];
static int s_registry_count = 0;

/**
 * @brief Clear the registry
 */
void registry_clear(void)
{
    memset(s_registry, 0, sizeof(s_registry));
    s_registry_count = 0;
    ESP_LOGD(TAG, "Registry cleared");
}

/**
 * @brief Register a component
 */
esp_err_t registry_add(void *interface, component_type_t type, const char *role)
{
    if (!interface) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (s_registry_count >= MAX_REGISTRY_ENTRIES) {
        ESP_LOGE(TAG, "Registry full");
        return ESP_ERR_NO_MEM;
    }
    
    registry_entry_t *entry = &s_registry[s_registry_count];
    entry->interface = interface;
    entry->type = type;
    entry->initialized = false;
    
    /* Get name from interface */
    switch (type) {
        case COMPONENT_TYPE_CPU:
            strncpy(entry->name, ((cpu_interface_t*)interface)->name, 
                    sizeof(entry->name) - 1);
            break;
        case COMPONENT_TYPE_VIDEO:
            strncpy(entry->name, ((video_interface_t*)interface)->name,
                    sizeof(entry->name) - 1);
            break;
        case COMPONENT_TYPE_AUDIO:
            strncpy(entry->name, ((audio_interface_t*)interface)->name,
                    sizeof(entry->name) - 1);
            break;
        case COMPONENT_TYPE_IO:
            strncpy(entry->name, ((io_interface_t*)interface)->name,
                    sizeof(entry->name) - 1);
            break;
        case COMPONENT_TYPE_SYSTEM:
            strncpy(entry->name, ((system_interface_t*)interface)->name,
                    sizeof(entry->name) - 1);
            break;
        default:
            strcpy(entry->name, "Unknown");
    }
    
    if (role) {
        strncpy(entry->role, role, sizeof(entry->role) - 1);
    }
    
    s_registry_count++;
    
    ESP_LOGI(TAG, "Registered: %s (type=%d, role=%s)", 
             entry->name, type, role ? role : "");
    
    return ESP_OK;
}

/**
 * @brief Remove a component from registry
 */
esp_err_t registry_remove(void *interface)
{
    for (int i = 0; i < s_registry_count; i++) {
        if (s_registry[i].interface == interface) {
            /* Shift remaining entries */
            for (int j = i; j < s_registry_count - 1; j++) {
                s_registry[j] = s_registry[j + 1];
            }
            s_registry_count--;
            memset(&s_registry[s_registry_count], 0, sizeof(registry_entry_t));
            return ESP_OK;
        }
    }
    
    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief Find component by type
 */
void* registry_find_by_type(component_type_t type)
{
    for (int i = 0; i < s_registry_count; i++) {
        if (s_registry[i].type == type) {
            return s_registry[i].interface;
        }
    }
    return NULL;
}

/**
 * @brief Find component by type and role
 */
void* registry_find_by_role(component_type_t type, const char *role)
{
    if (!role) {
        return registry_find_by_type(type);
    }
    
    for (int i = 0; i < s_registry_count; i++) {
        if (s_registry[i].type == type && 
            strcmp(s_registry[i].role, role) == 0) {
            return s_registry[i].interface;
        }
    }
    return NULL;
}

/**
 * @brief Find component by name
 */
void* registry_find_by_name(const char *name)
{
    if (!name) {
        return NULL;
    }
    
    for (int i = 0; i < s_registry_count; i++) {
        if (strcmp(s_registry[i].name, name) == 0) {
            return s_registry[i].interface;
        }
    }
    return NULL;
}

/**
 * @brief Get all components of a type
 */
int registry_get_all(component_type_t type, void **interfaces, int max_count)
{
    int count = 0;
    
    for (int i = 0; i < s_registry_count && count < max_count; i++) {
        if (type == 0 || s_registry[i].type == type) {
            interfaces[count++] = s_registry[i].interface;
        }
    }
    
    return count;
}

/**
 * @brief Initialize all registered components
 */
esp_err_t registry_init_all(void *config)
{
    int errors = 0;
    
    for (int i = 0; i < s_registry_count; i++) {
        if (s_registry[i].initialized) {
            continue;
        }
        
        int result = -1;
        
        switch (s_registry[i].type) {
            case COMPONENT_TYPE_CPU: {
                cpu_interface_t *cpu = s_registry[i].interface;
                if (cpu->init) {
                    result = cpu->init(config);
                }
                break;
            }
            case COMPONENT_TYPE_VIDEO: {
                video_interface_t *video = s_registry[i].interface;
                if (video->init) {
                    result = video->init(config);
                }
                break;
            }
            case COMPONENT_TYPE_AUDIO: {
                audio_interface_t *audio = s_registry[i].interface;
                if (audio->init) {
                    result = audio->init(config);
                }
                break;
            }
            case COMPONENT_TYPE_IO: {
                io_interface_t *io = s_registry[i].interface;
                if (io->init) {
                    result = io->init(config);
                }
                break;
            }
            case COMPONENT_TYPE_SYSTEM: {
                system_interface_t *system = s_registry[i].interface;
                if (system->init) {
                    result = system->init(config);
                }
                break;
            }
            default:
                break;
        }
        
        if (result == 0) {
            s_registry[i].initialized = true;
            ESP_LOGI(TAG, "Initialized: %s", s_registry[i].name);
        } else {
            ESP_LOGE(TAG, "Failed to initialize: %s", s_registry[i].name);
            errors++;
        }
    }
    
    return (errors == 0) ? ESP_OK : ESP_ERR_INVALID_STATE;
}

/**
 * @brief Reset all registered components
 */
void registry_reset_all(void)
{
    for (int i = 0; i < s_registry_count; i++) {
        switch (s_registry[i].type) {
            case COMPONENT_TYPE_CPU: {
                cpu_interface_t *cpu = s_registry[i].interface;
                if (cpu->reset) cpu->reset();
                break;
            }
            case COMPONENT_TYPE_VIDEO: {
                video_interface_t *video = s_registry[i].interface;
                if (video->reset) video->reset();
                break;
            }
            case COMPONENT_TYPE_AUDIO: {
                audio_interface_t *audio = s_registry[i].interface;
                if (audio->reset) audio->reset();
                break;
            }
            case COMPONENT_TYPE_IO: {
                io_interface_t *io = s_registry[i].interface;
                if (io->reset) io->reset();
                break;
            }
            case COMPONENT_TYPE_SYSTEM: {
                system_interface_t *system = s_registry[i].interface;
                if (system->reset) system->reset();
                break;
            }
            default:
                break;
        }
        
        ESP_LOGD(TAG, "Reset: %s", s_registry[i].name);
    }
}

/**
 * @brief Shutdown all registered components
 */
void registry_shutdown_all(void)
{
    for (int i = 0; i < s_registry_count; i++) {
        if (!s_registry[i].initialized) {
            continue;
        }
        
        switch (s_registry[i].type) {
            case COMPONENT_TYPE_CPU: {
                cpu_interface_t *cpu = s_registry[i].interface;
                if (cpu->shutdown) cpu->shutdown();
                break;
            }
            case COMPONENT_TYPE_VIDEO: {
                video_interface_t *video = s_registry[i].interface;
                if (video->shutdown) video->shutdown();
                break;
            }
            case COMPONENT_TYPE_AUDIO: {
                audio_interface_t *audio = s_registry[i].interface;
                if (audio->shutdown) audio->shutdown();
                break;
            }
            case COMPONENT_TYPE_IO: {
                io_interface_t *io = s_registry[i].interface;
                if (io->shutdown) io->shutdown();
                break;
            }
            case COMPONENT_TYPE_SYSTEM: {
                system_interface_t *system = s_registry[i].interface;
                if (system->shutdown) system->shutdown();
                break;
            }
            default:
                break;
        }
        
        s_registry[i].initialized = false;
        ESP_LOGD(TAG, "Shutdown: %s", s_registry[i].name);
    }
}

/**
 * @brief Get registry entry count
 */
int registry_count(void)
{
    return s_registry_count;
}
