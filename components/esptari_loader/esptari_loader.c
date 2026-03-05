#include "esptari_loader.h"
#include "machine.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"

static const char *TAG = "esptari_loader";

#define MACHINE_PROFILE_DIR "/sdcard/ebins/atari_st/machine_profile"
#define STARTUP_JOURNAL_PATH "/sdcard/ebins/atari_st/machine_profile/startup_recovery_journal_v1.log"
#define FALLBACK_MODULE_ID "st.profile.520"
#define FALLBACK_MODULE_VERSION "1.0.0"
#define FALLBACK_PROFILE_NAME "st_default"

typedef struct {
    uint32_t boot_count;
    bool previous_entry_valid;
    bool previous_loaded_from_runtime;
    char previous_module_id[64];
    char previous_module_version[24];
    char previous_profile_name[64];
    char previous_fallback_reason[64];
    bool replayed_recovery;
    char replay_from_module_id[64];
    char replay_from_module_version[24];
    char fallback_reason[64];
    uint32_t rejected_unsigned_candidates;
} startup_recovery_journal_t;

static char s_resolved_profile_name[64] = FALLBACK_PROFILE_NAME;
static char s_resolved_module_id[64] = FALLBACK_MODULE_ID;
static char s_resolved_module_version[24] = FALLBACK_MODULE_VERSION;
static bool s_loaded_from_runtime_ebin;
static startup_recovery_journal_t s_startup_journal;

static void startup_journal_apply_kv(const char *key, const char *value)
{
    if (key == NULL || value == NULL) {
        return;
    }

    if (strcmp(key, "boot_count") == 0) {
        s_startup_journal.boot_count = (uint32_t)strtoul(value, NULL, 10);
        return;
    }

    if (strcmp(key, "source") == 0) {
        s_startup_journal.previous_loaded_from_runtime = (strcmp(value, "runtime_ebin") == 0);
        s_startup_journal.previous_entry_valid = true;
        return;
    }

    if (strcmp(key, "module_id") == 0) {
        strlcpy(s_startup_journal.previous_module_id,
                value,
                sizeof(s_startup_journal.previous_module_id));
        s_startup_journal.previous_entry_valid = true;
        return;
    }

    if (strcmp(key, "module_version") == 0) {
        strlcpy(s_startup_journal.previous_module_version,
                value,
                sizeof(s_startup_journal.previous_module_version));
        s_startup_journal.previous_entry_valid = true;
        return;
    }

    if (strcmp(key, "profile_name") == 0) {
        strlcpy(s_startup_journal.previous_profile_name,
                value,
                sizeof(s_startup_journal.previous_profile_name));
        s_startup_journal.previous_entry_valid = true;
        return;
    }

    if (strcmp(key, "fallback_reason") == 0) {
        strlcpy(s_startup_journal.previous_fallback_reason,
                value,
                sizeof(s_startup_journal.previous_fallback_reason));
    }
}

static void startup_journal_load_from_sd(void)
{
    memset(&s_startup_journal, 0, sizeof(s_startup_journal));

    FILE *handle = fopen(STARTUP_JOURNAL_PATH, "r");
    if (handle == NULL) {
        return;
    }

    char line[196] = {0};
    while (fgets(line, sizeof(line), handle) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        char *equals = strchr(line, '=');
        if (equals == NULL || equals == line || equals[1] == '\0') {
            continue;
        }

        *equals = '\0';
        startup_journal_apply_kv(line, equals + 1);
    }

    fclose(handle);
}

static void startup_journal_prepare_replay_metadata(void)
{
    s_startup_journal.replayed_recovery = false;
    s_startup_journal.replay_from_module_id[0] = '\0';
    s_startup_journal.replay_from_module_version[0] = '\0';

    if (s_loaded_from_runtime_ebin) {
        return;
    }

    if (s_startup_journal.previous_entry_valid &&
        s_startup_journal.previous_loaded_from_runtime &&
        s_startup_journal.previous_module_id[0] != '\0') {
        s_startup_journal.replayed_recovery = true;
        strlcpy(s_startup_journal.replay_from_module_id,
                s_startup_journal.previous_module_id,
                sizeof(s_startup_journal.replay_from_module_id));
        strlcpy(s_startup_journal.replay_from_module_version,
                s_startup_journal.previous_module_version,
                sizeof(s_startup_journal.replay_from_module_version));

        if (s_startup_journal.fallback_reason[0] == '\0') {
            strlcpy(s_startup_journal.fallback_reason,
                    "runtime_candidate_missing_or_rejected",
                    sizeof(s_startup_journal.fallback_reason));
        }
    }
}

static void startup_journal_persist_to_sd(void)
{
    FILE *handle = fopen(STARTUP_JOURNAL_PATH, "w");
    if (handle == NULL) {
        ESP_LOGW(TAG, "Failed to persist startup recovery journal at %s", STARTUP_JOURNAL_PATH);
        return;
    }

    fprintf(handle, "schema=ebin_startup_recovery_journal_v1\n");
    fprintf(handle, "boot_count=%lu\n", (unsigned long)s_startup_journal.boot_count);
    fprintf(handle, "source=%s\n", s_loaded_from_runtime_ebin ? "runtime_ebin" : "fallback");
    fprintf(handle, "module_id=%s\n", s_resolved_module_id);
    fprintf(handle, "module_version=%s\n", s_resolved_module_version);
    fprintf(handle, "profile_name=%s\n", s_resolved_profile_name);
    fprintf(handle, "fallback_reason=%s\n", s_startup_journal.fallback_reason[0] != '\0' ? s_startup_journal.fallback_reason : "none");
    fprintf(handle, "replayed_recovery=%s\n", s_startup_journal.replayed_recovery ? "true" : "false");
    fprintf(handle, "replay_from_module_id=%s\n", s_startup_journal.replay_from_module_id[0] != '\0' ? s_startup_journal.replay_from_module_id : "none");
    fprintf(handle, "replay_from_module_version=%s\n", s_startup_journal.replay_from_module_version[0] != '\0' ? s_startup_journal.replay_from_module_version : "none");
        fprintf(handle,
            "rejected_unsigned_candidates=%lu\n",
            (unsigned long)s_startup_journal.rejected_unsigned_candidates);
    fclose(handle);
}

static bool signature_sidecar_is_valid(const char *filename)
{
    if (filename == NULL || filename[0] == '\0') {
        return false;
    }

    char signature_path[256] = {0};
    int written = snprintf(signature_path,
                           sizeof(signature_path),
                           "%s/%s.sig",
                           MACHINE_PROFILE_DIR,
                           filename);
    if (written <= 0 || written >= (int)sizeof(signature_path)) {
        return false;
    }

    FILE *handle = fopen(signature_path, "r");
    if (handle == NULL) {
        return false;
    }

    char line[160] = {0};
    char *result = fgets(line, sizeof(line), handle);
    fclose(handle);

    if (result == NULL) {
        return false;
    }

    return strncmp(line, "ESPTARI-DEV-SIG:", strlen("ESPTARI-DEV-SIG:")) == 0;
}

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

    if (strcmp(module_id, "st.profile.1040") == 0) {
        return "st_default";
    }

    return FALLBACK_PROFILE_NAME;
}

static void discover_machine_profile_from_sd(void)
{
    strlcpy(s_resolved_profile_name, FALLBACK_PROFILE_NAME, sizeof(s_resolved_profile_name));
    strlcpy(s_resolved_module_id, FALLBACK_MODULE_ID, sizeof(s_resolved_module_id));
    strlcpy(s_resolved_module_version, FALLBACK_MODULE_VERSION, sizeof(s_resolved_module_version));
    s_loaded_from_runtime_ebin = false;
    s_startup_journal.fallback_reason[0] = '\0';
    s_startup_journal.rejected_unsigned_candidates = 0;

    DIR *dir = opendir(MACHINE_PROFILE_DIR);
    if (dir == NULL) {
        ESP_LOGW(TAG, "No runtime machine_profile EBIN directory at %s; using fallback", MACHINE_PROFILE_DIR);
        strlcpy(s_startup_journal.fallback_reason,
                "runtime_dir_missing",
                sizeof(s_startup_journal.fallback_reason));
        return;
    }

    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        char module_id[64] = {0};
        char version[24] = {0};
        if (!parse_ebin_filename(entry->d_name, module_id, sizeof(module_id), version, sizeof(version))) {
            continue;
        }

        if (!signature_sidecar_is_valid(entry->d_name)) {
            ESP_LOGW(TAG,
                     "Rejected unsigned runtime machine_profile EBIN candidate: %s",
                     entry->d_name);
            s_startup_journal.rejected_unsigned_candidates++;
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

    if (!s_loaded_from_runtime_ebin && s_startup_journal.fallback_reason[0] == '\0') {
        if (s_startup_journal.rejected_unsigned_candidates > 0) {
            strlcpy(s_startup_journal.fallback_reason,
                    "unsigned_candidates_rejected",
                    sizeof(s_startup_journal.fallback_reason));
        } else {
            strlcpy(s_startup_journal.fallback_reason,
                    "no_runtime_candidates",
                    sizeof(s_startup_journal.fallback_reason));
        }
    }
}

esp_err_t loader_init(void)
{
    startup_journal_load_from_sd();
    s_startup_journal.boot_count += 1;

    discover_machine_profile_from_sd();
    startup_journal_prepare_replay_metadata();

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

    startup_journal_persist_to_sd();

    if (s_startup_journal.replayed_recovery) {
        ESP_LOGW(TAG,
                 "Deterministic startup recovery replayed: runtime %s@%s -> fallback %s@%s",
                 s_startup_journal.replay_from_module_id,
                 s_startup_journal.replay_from_module_version,
                 s_resolved_module_id,
                 s_resolved_module_version);
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
