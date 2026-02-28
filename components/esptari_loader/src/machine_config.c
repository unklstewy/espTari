/**
 * @file machine_config.c
 * @brief Machine profile configuration parsing and management
 * 
 * Parses JSON machine profile files and loads the specified components
 * to create a complete emulated machine.
 * 
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include "esptari_loader.h"
#include "machine.h"

static const char *TAG = "machine";

/* Current machine state */
static machine_state_t s_machine = {0};
static bool s_machine_loaded = false;

/* Base path for machine profiles */
#define MACHINE_PROFILES_PATH "/sdcard/machines"
#define CORES_PATH            "/sdcard/cores"
#define TOS_PATH              "/sdcard/roms/tos"

/* Registry functions (from registry.c) */
extern void registry_clear(void);
extern esp_err_t registry_add(void *interface, component_type_t type, const char *role);
extern esp_err_t registry_init_all(void *config);
extern void registry_reset_all(void);
extern void registry_shutdown_all(void);

/**
 * @brief Parse component configuration from JSON
 */
static void parse_component(cJSON *json, machine_component_t *comp)
{
    if (!json || !comp) return;
    
    memset(comp, 0, sizeof(machine_component_t));
    
    cJSON *file = cJSON_GetObjectItem(json, "file");
    if (file && cJSON_IsString(file)) {
        strncpy(comp->file, file->valuestring, sizeof(comp->file) - 1);
    }
    
    cJSON *clock = cJSON_GetObjectItem(json, "clock_hz");
    if (clock && cJSON_IsNumber(clock)) {
        comp->clock_hz = (uint32_t)clock->valuedouble;
    }
    
    cJSON *role = cJSON_GetObjectItem(json, "role");
    if (role && cJSON_IsString(role)) {
        strncpy(comp->role, role->valuestring, sizeof(comp->role) - 1);
    }
    
    cJSON *optional = cJSON_GetObjectItem(json, "optional");
    if (optional && cJSON_IsBool(optional)) {
        comp->optional = cJSON_IsTrue(optional);
    }
}

/**
 * @brief Parse memory configuration from JSON
 */
static void parse_memory(cJSON *json, machine_memory_t *mem)
{
    if (!json || !mem) return;
    
    memset(mem, 0, sizeof(machine_memory_t));
    mem->tos_required = true;  /* Default */
    
    cJSON *ram = cJSON_GetObjectItem(json, "ram_kb");
    if (ram && cJSON_IsNumber(ram)) {
        mem->ram_size = (uint32_t)ram->valuedouble * 1024;
    }
    
    cJSON *tos = cJSON_GetObjectItem(json, "tos_file");
    if (tos && cJSON_IsString(tos)) {
        strncpy(mem->tos_file, tos->valuestring, sizeof(mem->tos_file) - 1);
    }
    
    cJSON *req = cJSON_GetObjectItem(json, "tos_required");
    if (req && cJSON_IsBool(req)) {
        mem->tos_required = cJSON_IsTrue(req);
    }
}

esp_err_t machine_parse_profile(const char *path, machine_profile_t *profile)
{
    if (!path || !profile) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(profile, 0, sizeof(machine_profile_t));
    
    /* Open and read file */
    FILE *f = fopen(path, "r");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open profile: %s", path);
        return ESP_ERR_NOT_FOUND;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0 || size > 16384) {
        fclose(f);
        ESP_LOGE(TAG, "Invalid file size: %ld", size);
        return ESP_ERR_INVALID_SIZE;
    }
    
    char *json_str = malloc(size + 1);
    if (!json_str) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    
    size_t read = fread(json_str, 1, size, f);
    fclose(f);
    json_str[read] = '\0';
    
    /* Parse JSON */
    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    
    if (!root) {
        ESP_LOGE(TAG, "JSON parse error: %s", cJSON_GetErrorPtr());
        return ESP_ERR_INVALID_ARG;
    }
    
    /* Extract fields */
    cJSON *item;
    
    item = cJSON_GetObjectItem(root, "machine");
    if (item && cJSON_IsString(item)) {
        strncpy(profile->id, item->valuestring, sizeof(profile->id) - 1);
    }
    
    item = cJSON_GetObjectItem(root, "display_name");
    if (item && cJSON_IsString(item)) {
        strncpy(profile->display_name, item->valuestring, 
                sizeof(profile->display_name) - 1);
    }
    
    item = cJSON_GetObjectItem(root, "description");
    if (item && cJSON_IsString(item)) {
        strncpy(profile->description, item->valuestring,
                sizeof(profile->description) - 1);
    }
    
    item = cJSON_GetObjectItem(root, "year");
    if (item && cJSON_IsNumber(item)) {
        profile->year = (uint16_t)item->valuedouble;
    }
    
    /* Parse components */
    cJSON *components = cJSON_GetObjectItem(root, "components");
    if (components) {
        item = cJSON_GetObjectItem(components, "cpu");
        if (item) parse_component(item, &profile->cpu);
        
        item = cJSON_GetObjectItem(components, "mmu");
        if (item) parse_component(item, &profile->mmu);
        
        item = cJSON_GetObjectItem(components, "video");
        if (item) parse_component(item, &profile->video);
        
        /* Parse audio array */
        cJSON *audio = cJSON_GetObjectItem(components, "audio");
        if (audio && cJSON_IsArray(audio)) {
            int count = cJSON_GetArraySize(audio);
            if (count > MACHINE_MAX_AUDIO) count = MACHINE_MAX_AUDIO;
            profile->audio_count = count;
            
            for (int i = 0; i < count; i++) {
                parse_component(cJSON_GetArrayItem(audio, i), &profile->audio[i]);
            }
        }
        
        /* Parse I/O array or individual items */
        cJSON *io = cJSON_GetObjectItem(components, "io");
        if (io && cJSON_IsArray(io)) {
            int count = cJSON_GetArraySize(io);
            if (count > MACHINE_MAX_IO) count = MACHINE_MAX_IO;
            profile->io_count = count;
            
            for (int i = 0; i < count; i++) {
                parse_component(cJSON_GetArrayItem(io, i), &profile->io[i]);
            }
        }
        
        /* Also check for named I/O components */
        item = cJSON_GetObjectItem(components, "blitter");
        if (item && profile->io_count < MACHINE_MAX_IO) {
            parse_component(item, &profile->io[profile->io_count]);
            strcpy(profile->io[profile->io_count].role, "blitter");
            profile->io_count++;
        }
    }
    
    /* Parse memory */
    item = cJSON_GetObjectItem(root, "memory");
    if (item) {
        parse_memory(item, &profile->memory);
    }
    
    cJSON_Delete(root);
    
    ESP_LOGI(TAG, "Parsed profile: %s (%s)", profile->id, profile->display_name);
    
    return ESP_OK;
}

/**
 * @brief Load a component from SD card
 */
static esp_err_t load_component_by_name(const char *filename, 
                                         component_type_t type,
                                         const char *role,
                                         void **interface_out)
{
    if (!filename || filename[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    
    /* Build path based on type */
    char path[128];
    const char *subdir;
    
    switch (type) {
        case COMPONENT_TYPE_CPU:   subdir = "cpu"; break;
        case COMPONENT_TYPE_VIDEO: subdir = "video"; break;
        case COMPONENT_TYPE_AUDIO: subdir = "audio"; break;
        case COMPONENT_TYPE_IO:    subdir = "io"; break;
        default:                   return ESP_ERR_INVALID_ARG;
    }
    
    snprintf(path, sizeof(path), "%s/%s/%s", CORES_PATH, subdir, filename);
    
    esp_err_t err = loader_load_component(path, type, interface_out);
    if (err != ESP_OK) {
        return err;
    }
    
    /* Register component */
    return registry_add(*interface_out, type, role);
}

/**
 * @brief Load TOS ROM from SD card
 */
static esp_err_t load_tos_rom(const machine_memory_t *mem, 
                               uint8_t **rom_out, 
                               uint32_t *size_out)
{
    if (!mem->tos_file[0]) {
        if (mem->tos_required) {
            ESP_LOGE(TAG, "TOS ROM required but not specified");
            return ESP_ERR_NOT_FOUND;
        }
        return ESP_OK;
    }
    
    char path[128];
    snprintf(path, sizeof(path), "%s/%s", TOS_PATH, mem->tos_file);
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open TOS: %s", path);
        return ESP_ERR_NOT_FOUND;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    /* TOS should be 192KB or 256KB typically */
    if (size < 65536 || size > 524288) {
        fclose(f);
        ESP_LOGE(TAG, "Invalid TOS size: %ld", size);
        return ESP_ERR_INVALID_SIZE;
    }
    
    /* Allocate in PSRAM */
    uint8_t *rom = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!rom) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    
    size_t read = fread(rom, 1, size, f);
    fclose(f);
    
    if (read != (size_t)size) {
        heap_caps_free(rom);
        return ESP_ERR_INVALID_SIZE;
    }
    
    *rom_out = rom;
    *size_out = (uint32_t)size;
    
    ESP_LOGI(TAG, "Loaded TOS: %s (%lu bytes)", mem->tos_file, (unsigned long)size);
    
    return ESP_OK;
}

esp_err_t machine_load(const char *profile_name)
{
    if (!profile_name) {
        return ESP_ERR_INVALID_ARG;
    }
    
    /* Unload existing machine */
    if (s_machine_loaded) {
        machine_unload();
    }
    
    /* Build profile path */
    char path[128];
    snprintf(path, sizeof(path), "%s/%s.json", MACHINE_PROFILES_PATH, profile_name);
    
    /* Parse profile */
    esp_err_t err = machine_parse_profile(path, &s_machine.profile);
    if (err != ESP_OK) {
        return err;
    }
    
    /* Clear registry */
    registry_clear();
    
    /* Allocate RAM in PSRAM */
    if (s_machine.profile.memory.ram_size > 0) {
        s_machine.ram = heap_caps_malloc(s_machine.profile.memory.ram_size,
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_machine.ram) {
            ESP_LOGE(TAG, "Failed to allocate %lu bytes RAM",
                     (unsigned long)s_machine.profile.memory.ram_size);
            return ESP_ERR_NO_MEM;
        }
        memset(s_machine.ram, 0, s_machine.profile.memory.ram_size);
        ESP_LOGI(TAG, "Allocated %lu bytes RAM",
                 (unsigned long)s_machine.profile.memory.ram_size);
    }
    
    /* Load TOS ROM */
    err = load_tos_rom(&s_machine.profile.memory, &s_machine.rom, &s_machine.rom_size);
    if (err != ESP_OK && s_machine.profile.memory.tos_required) {
        heap_caps_free(s_machine.ram);
        s_machine.ram = NULL;
        return err;
    }
    
    /* Load components */
    machine_profile_t *p = &s_machine.profile;
    
    /* CPU */
    if (p->cpu.file[0]) {
        err = load_component_by_name(p->cpu.file, COMPONENT_TYPE_CPU, "cpu",
                                      (void**)&s_machine.cpu);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to load CPU: %s", p->cpu.file);
            machine_unload();
            return err;
        }
    }
    
    /* Video */
    if (p->video.file[0]) {
        err = load_component_by_name(p->video.file, COMPONENT_TYPE_VIDEO, "video",
                                      (void**)&s_machine.video);
        if (err != ESP_OK && !p->video.optional) {
            ESP_LOGE(TAG, "Failed to load video: %s", p->video.file);
            machine_unload();
            return err;
        }
    }
    
    /* Audio components */
    for (int i = 0; i < p->audio_count; i++) {
        if (p->audio[i].file[0]) {
            err = load_component_by_name(p->audio[i].file, COMPONENT_TYPE_AUDIO,
                                          p->audio[i].role,
                                          (void**)&s_machine.audio[i]);
            if (err != ESP_OK && !p->audio[i].optional) {
                ESP_LOGE(TAG, "Failed to load audio: %s", p->audio[i].file);
                machine_unload();
                return err;
            }
        }
    }
    
    /* I/O components */
    for (int i = 0; i < p->io_count; i++) {
        if (p->io[i].file[0]) {
            err = load_component_by_name(p->io[i].file, COMPONENT_TYPE_IO,
                                          p->io[i].role,
                                          (void**)&s_machine.io[i]);
            if (err != ESP_OK && !p->io[i].optional) {
                ESP_LOGE(TAG, "Failed to load I/O: %s", p->io[i].file);
                machine_unload();
                return err;
            }
        }
    }
    
    s_machine_loaded = true;
    
    ESP_LOGI(TAG, "Machine loaded: %s", p->display_name);
    
    return ESP_OK;
}

esp_err_t machine_unload(void)
{
    if (!s_machine_loaded) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Unloading machine: %s", s_machine.profile.display_name);
    
    /* Stop emulation */
    s_machine.running = false;
    
    /* Shutdown all components */
    registry_shutdown_all();
    
    /* Unload components */
    if (s_machine.cpu) {
        loader_unload_component(s_machine.cpu);
        s_machine.cpu = NULL;
    }
    
    if (s_machine.video) {
        loader_unload_component(s_machine.video);
        s_machine.video = NULL;
    }
    
    for (int i = 0; i < MACHINE_MAX_AUDIO; i++) {
        if (s_machine.audio[i]) {
            loader_unload_component(s_machine.audio[i]);
            s_machine.audio[i] = NULL;
        }
    }
    
    for (int i = 0; i < MACHINE_MAX_IO; i++) {
        if (s_machine.io[i]) {
            loader_unload_component(s_machine.io[i]);
            s_machine.io[i] = NULL;
        }
    }
    
    /* Free memory */
    if (s_machine.ram) {
        heap_caps_free(s_machine.ram);
        s_machine.ram = NULL;
    }
    
    if (s_machine.rom) {
        heap_caps_free(s_machine.rom);
        s_machine.rom = NULL;
    }
    
    /* Clear registry */
    registry_clear();
    
    memset(&s_machine, 0, sizeof(s_machine));
    s_machine_loaded = false;
    
    return ESP_OK;
}

machine_state_t* machine_get_state(void)
{
    return s_machine_loaded ? &s_machine : NULL;
}

cpu_interface_t* machine_get_cpu(void)
{
    return s_machine_loaded ? s_machine.cpu : NULL;
}

video_interface_t* machine_get_video(void)
{
    return s_machine_loaded ? s_machine.video : NULL;
}

audio_interface_t* machine_get_audio(int index)
{
    if (!s_machine_loaded || index < 0 || index >= MACHINE_MAX_AUDIO) {
        return NULL;
    }
    return s_machine.audio[index];
}

io_interface_t* machine_get_io(int index)
{
    if (!s_machine_loaded || index < 0 || index >= MACHINE_MAX_IO) {
        return NULL;
    }
    return s_machine.io[index];
}

esp_err_t machine_swap_component(int type, const char *filename)
{
    /* TODO: Implement hot-swap */
    (void)type;
    (void)filename;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t machine_list_profiles(char **names, size_t max_count, size_t *count)
{
    if (!names || !count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *count = 0;
    
    DIR *dir = opendir(MACHINE_PROFILES_PATH);
    if (!dir) {
        ESP_LOGW(TAG, "Cannot open: %s", MACHINE_PROFILES_PATH);
        return ESP_ERR_NOT_FOUND;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && *count < max_count) {
        size_t len = strlen(entry->d_name);
        if (len > 5 && strcmp(entry->d_name + len - 5, ".json") == 0) {
            /* Remove .json extension */
            char *name = malloc(len - 4);
            if (name) {
                strncpy(name, entry->d_name, len - 5);
                name[len - 5] = '\0';
                names[*count] = name;
                (*count)++;
            }
        }
    }
    
    closedir(dir);
    
    ESP_LOGI(TAG, "Found %zu machine profiles", *count);
    
    return ESP_OK;
}

esp_err_t machine_reset(void)
{
    if (!s_machine_loaded) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Resetting machine");
    
    /* Reset all components */
    registry_reset_all();
    
    /* Clear RAM */
    if (s_machine.ram && s_machine.profile.memory.ram_size > 0) {
        memset(s_machine.ram, 0, s_machine.profile.memory.ram_size);
    }
    
    return ESP_OK;
}

int machine_run_frame(int cycles_per_frame)
{
    if (!s_machine_loaded || !s_machine.cpu) {
        return 0;
    }
    
    int cycles_executed = 0;
    
    while (cycles_executed < cycles_per_frame) {
        /* Execute CPU */
        int cycles = s_machine.cpu->execute(cycles_per_frame - cycles_executed);
        if (cycles <= 0) {
            break;
        }
        
        cycles_executed += cycles;
        
        /* Clock video */
        if (s_machine.video && s_machine.video->clock) {
            s_machine.video->clock(cycles);
        }
        
        /* Clock audio */
        for (int i = 0; i < s_machine.profile.audio_count; i++) {
            if (s_machine.audio[i] && s_machine.audio[i]->clock) {
                s_machine.audio[i]->clock(cycles);
            }
        }
        
        /* Clock I/O */
        for (int i = 0; i < s_machine.profile.io_count; i++) {
            if (s_machine.io[i] && s_machine.io[i]->clock) {
                s_machine.io[i]->clock(cycles);
            }
        }
    }
    
    return cycles_executed;
}
