#include "esptari_loader.h"
#include "machine.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"

static const char *TAG = "esptari_loader";

#define MACHINE_PROFILE_DIR "/sdcard/ebins/atari_st/machine_profile"
#define FALLBACK_MODULE_ID "st.profile.520"
#define FALLBACK_MODULE_VERSION "1.0.0"
#define FALLBACK_PROFILE_NAME "st_default"

static char s_resolved_profile_name[64] = FALLBACK_PROFILE_NAME;
static char s_resolved_module_id[64] = FALLBACK_MODULE_ID;
static char s_resolved_module_version[24] = FALLBACK_MODULE_VERSION;
static bool s_loaded_from_runtime_ebin;

static bool parse_semver(const char *version, uint32_t *major, uint32_t *minor, uint32_t *patch)
{
    if (version == NULL || major == NULL || minor == NULL || patch == NULL || version[0] == '\0') {
        return false;
    }

    char *end = NULL;
    unsigned long parsed_major = strtoul(version, &end, 10);
    if (end == version || end == NULL || *end != '.') {
        return false;
    }

    const char *minor_start = end + 1;
    unsigned long parsed_minor = strtoul(minor_start, &end, 10);
    if (end == minor_start || end == NULL || *end != '.') {
        return false;
    }

    const char *patch_start = end + 1;
    unsigned long parsed_patch = strtoul(patch_start, &end, 10);
    if (end == patch_start || end == NULL || *end != '\0') {
        return false;
    }

    *major = (uint32_t)parsed_major;
    *minor = (uint32_t)parsed_minor;
    *patch = (uint32_t)parsed_patch;
    return true;
}

static bool parse_ebin_filename(const char *filename,
                                char *module_id,
                                size_t module_id_len,
                                char *version,
                                size_t version_len)
{
    if (filename == NULL || module_id == NULL || version == NULL) {
        return false;
    }

    size_t len = strlen(filename);
    if (len <= 5 || strcmp(filename + len - 5, ".ebin") != 0) {
        return false;
    }

    char stem[160] = {0};
    strlcpy(stem, filename, sizeof(stem));
    stem[len - 5] = '\0';

    char *dash = strrchr(stem, '-');
    if (dash == NULL || dash == stem || dash[1] == '\0') {
        return false;
    }

    *dash = '\0';

    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;
    if (!parse_semver(dash + 1, &major, &minor, &patch)) {
        return false;
    }

    strlcpy(module_id, stem, module_id_len);
    strlcpy(version, dash + 1, version_len);
    return true;
}

static int compare_semver(const char *left, const char *right)
{
    uint32_t left_major = 0;
    uint32_t left_minor = 0;
    uint32_t left_patch = 0;
    uint32_t right_major = 0;
    uint32_t right_minor = 0;
    uint32_t right_patch = 0;

    bool left_valid = parse_semver(left, &left_major, &left_minor, &left_patch);
    bool right_valid = parse_semver(right, &right_major, &right_minor, &right_patch);
    if (!left_valid && !right_valid) {
        return 0;
    }
    if (!left_valid) {
        return -1;
    }
    if (!right_valid) {
        return 1;
    }

    if (left_major != right_major) {
        return (left_major > right_major) ? 1 : -1;
    }
    if (left_minor != right_minor) {
        return (left_minor > right_minor) ? 1 : -1;
    }
    if (left_patch != right_patch) {
        return (left_patch > right_patch) ? 1 : -1;
    }
    return 0;
}

static const char *module_id_to_profile_name(const char *module_id)
{
    if (module_id == NULL || module_id[0] == '\0') {
        return FALLBACK_PROFILE_NAME;
    }

    if (strcmp(module_id, "st.profile.520") == 0) {
        return "st_520_pal";
    }

    return FALLBACK_PROFILE_NAME;
}

static void discover_machine_profile_from_sd(void)
{
    strlcpy(s_resolved_profile_name, FALLBACK_PROFILE_NAME, sizeof(s_resolved_profile_name));
    strlcpy(s_resolved_module_id, FALLBACK_MODULE_ID, sizeof(s_resolved_module_id));
    strlcpy(s_resolved_module_version, FALLBACK_MODULE_VERSION, sizeof(s_resolved_module_version));
    s_loaded_from_runtime_ebin = false;

    DIR *dir = opendir(MACHINE_PROFILE_DIR);
    if (dir == NULL) {
        ESP_LOGW(TAG, "No runtime machine_profile EBIN directory at %s; using fallback", MACHINE_PROFILE_DIR);
        return;
    }

    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        char module_id[64] = {0};
        char version[24] = {0};
        if (!parse_ebin_filename(entry->d_name, module_id, sizeof(module_id), version, sizeof(version))) {
            continue;
        }

        int ordering = compare_semver(version, s_resolved_module_version);
        if (!s_loaded_from_runtime_ebin || ordering > 0) {
            strlcpy(s_resolved_module_id, module_id, sizeof(s_resolved_module_id));
            strlcpy(s_resolved_module_version, version, sizeof(s_resolved_module_version));
            strlcpy(s_resolved_profile_name,
                    module_id_to_profile_name(module_id),
                    sizeof(s_resolved_profile_name));
            s_loaded_from_runtime_ebin = true;
        }
    }

    closedir(dir);
}

esp_err_t loader_init(void)
{
    discover_machine_profile_from_sd();

    if (s_loaded_from_runtime_ebin) {
        ESP_LOGI(TAG,
                 "Resolved machine profile from runtime EBIN: %s@%s -> %s",
                 s_resolved_module_id,
                 s_resolved_module_version,
                 s_resolved_profile_name);
    } else {
        ESP_LOGI(TAG,
                 "Using fallback machine profile module: %s@%s -> %s",
                 s_resolved_module_id,
                 s_resolved_module_version,
                 s_resolved_profile_name);
    }

    esp_err_t err = machine_load(s_resolved_profile_name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "machine_load(%s) failed: %s", s_resolved_profile_name, esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

void esptari_loader_log_unified_config(void)
{
    ESP_LOGI(TAG,
             "Unified boot profile loaded: %s (module=%s@%s, source=%s)",
             s_resolved_profile_name,
             s_resolved_module_id,
             s_resolved_module_version,
             s_loaded_from_runtime_ebin ? "runtime_ebin" : "fallback");
}

const char *esptari_loader_get_resolved_profile_name(void)
{
    return s_resolved_profile_name;
}
