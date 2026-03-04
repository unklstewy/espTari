#include "esptari_web_catalog_sync.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "esptari_web_auth.h"
#include "esptari_web_http_utils.h"

static const char *TAG = "esptari_web_catalog";

#define MAX_SYNC_SCHEDULES 32
#define MAX_SYNC_JOBS 64
#define MAX_RECOVERY_QUARANTINE 16
#define MAX_EBIN_CATALOG_ENTRIES 64

typedef struct {
    char job_id[32];
    char trigger[16];
    char schedule_id[32];
    char job_type[48];
    char mode[64];
    char status[24];
    uint64_t created_at_us;
    uint64_t started_at_us;
    uint64_t completed_at_us;
} esptari_sync_job_t;

typedef struct {
    char schedule_id[32];
    char job_type[48];
    char mode[64];
    char cron[48];
    bool enabled;
    bool catch_up;
    uint64_t created_at_us;
    uint64_t updated_at_us;
    uint64_t last_run_at_us;
    uint64_t next_run_at_us;
    char last_result[16];
    char last_error_code[48];
} esptari_sync_schedule_t;

typedef struct {
    char schedule_id[32];
    char error_code[48];
    char reason[64];
} esptari_recovery_quarantine_t;

typedef struct {
    char recovery_run_id[48];
    uint64_t scheduler_now_us;
    uint32_t loaded;
    uint32_t validated;
    uint32_t recomputed_next_run;
    uint32_t quarantined;
    esptari_recovery_quarantine_t quarantine[MAX_RECOVERY_QUARANTINE];
} esptari_recovery_report_t;

typedef struct {
    char machine[24];
    char component[24];
    char module_id[64];
    char version[24];
    char path[256];
} ebin_catalog_entry_t;

typedef struct {
    bool has_active_module;
    bool has_last_known_good;
    char active_module_id[64];
    char active_abi_version[16];
    char last_known_good_module_id[64];
    char last_known_good_abi_version[16];
    char state[24];
    char last_stage[24];
    char last_error_reason[96];
    char last_fault_code[48];
    char last_recovery_action[64];
    char last_recovered_module_id[64];
    uint64_t last_fault_at_us;
    uint64_t transition_seq;
    uint64_t updated_at_us;
} ebin_runtime_state_t;

static esptari_sync_job_t s_jobs[MAX_SYNC_JOBS];
static size_t s_job_count;
static esptari_sync_schedule_t s_schedules[MAX_SYNC_SCHEDULES];
static size_t s_schedule_count;
static char s_persisted_schedule_snapshot[8192];
static char s_recovery_parse_buffer[8192];
static bool s_initialized;
static uint64_t s_job_seq;
static uint64_t s_schedule_seq;
static esptari_recovery_report_t s_recovery_report;
static ebin_runtime_state_t s_ebin_runtime = {
    .has_active_module = false,
    .has_last_known_good = false,
    .active_module_id = "",
    .active_abi_version = "",
    .last_known_good_module_id = "",
    .last_known_good_abi_version = "",
    .state = "idle",
    .last_stage = "",
    .last_error_reason = "",
    .last_fault_code = "",
    .last_recovery_action = "none",
    .last_recovered_module_id = "",
    .last_fault_at_us = 0,
    .transition_seq = 0,
    .updated_at_us = 0,
};

static const ebin_catalog_entry_t s_ebin_catalog_baked[] = {
    {.machine = "atari_st", .component = "cpu", .module_id = "st.cpu.m68k", .version = "1.0.0", .path = "/sdcard/ebins/atari_st/cpu/st.cpu.m68k-1.0.0.ebin"},
    {.machine = "atari_st", .component = "cpu", .module_id = "st.cpu.m68k", .version = "0.9.0", .path = "/sdcard/ebins/atari_st/cpu/st.cpu.m68k-0.9.0.ebin"},
    {.machine = "atari_st", .component = "video", .module_id = "st.video.shifter", .version = "1.0.0", .path = "/sdcard/ebins/atari_st/video/st.video.shifter-1.0.0.ebin"},
    {.machine = "atari_st", .component = "io", .module_id = "st.io.ikbd", .version = "1.0.0", .path = "/sdcard/ebins/atari_st/io/st.io.ikbd-1.0.0.ebin"},
    {.machine = "atari_st", .component = "storage", .module_id = "st.storage.fdc", .version = "1.0.0", .path = "/sdcard/ebins/atari_st/storage/st.storage.fdc-1.0.0.ebin"},
    {.machine = "atari_st", .component = "audio", .module_id = "st.audio.psg", .version = "1.0.0", .path = "/sdcard/ebins/atari_st/audio/st.audio.psg-1.0.0.ebin"},
    {.machine = "atari_st", .component = "machine_profile", .module_id = "st.profile.520", .version = "1.0.0", .path = "/sdcard/ebins/atari_st/machine_profile/st.profile.520-1.0.0.ebin"},
};

static ebin_catalog_entry_t s_ebin_catalog_runtime[MAX_EBIN_CATALOG_ENTRIES];
static size_t s_ebin_catalog_runtime_count;

static uint64_t scheduler_now_us(void)
{
    return (uint64_t)esp_timer_get_time();
}

static esp_err_t send_json_object(httpd_req_t *req, cJSON *root, int status)
{
    char *json = cJSON_PrintUnformatted(root);
    if (json == NULL) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }
    esp_err_t out = esptari_web_send_json(req, json, status);
    cJSON_free(json);
    return out;
}

static esp_err_t send_error(httpd_req_t *req, const char *code, int status)
{
    char resp[320];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":false,\"error\":{\"code\":\"%s\",\"category\":\"scheduler\",\"retryable\":false}}",
             code);
    return esptari_web_send_json(req, resp, status);
}

static bool parse_json_request(httpd_req_t *req, cJSON **json)
{
    char body[768];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        send_error(req, "BAD_REQUEST", 400);
        return false;
    }

    *json = cJSON_Parse(body);
    if (*json == NULL) {
        send_error(req, "BAD_REQUEST", 400);
        return false;
    }

    return true;
}

static esp_err_t send_ebin_validation_error(httpd_req_t *req,
                                            const char *code,
                                            int status,
                                            const char *field,
                                            const char *reason)
{
    char payload[512];
    snprintf(payload,
             sizeof(payload),
             "{\"ok\":false,\"error\":{\"code\":\"%s\",\"category\":\"ebin\",\"retryable\":false,\"details\":{\"field\":\"%s\",\"reason\":\"%s\"}}}",
             code,
             field != NULL ? field : "",
             reason != NULL ? reason : "validation_failed");
    return esptari_web_send_json(req, payload, status);
}

static bool is_known_ebin_module_type(const char *module_type)
{
    return module_type != NULL &&
           (strcmp(module_type, "cpu") == 0 || strcmp(module_type, "video") == 0 || strcmp(module_type, "io") == 0 ||
            strcmp(module_type, "storage") == 0 || strcmp(module_type, "audio") == 0 ||
            strcmp(module_type, "machine_profile") == 0);
}

static bool parse_semver_major(const char *version, uint32_t *out_major)
{
    if (version == NULL || out_major == NULL || version[0] == '\0') {
        return false;
    }

    char *endptr = NULL;
    unsigned long major = strtoul(version, &endptr, 10);
    if (endptr == version || major > 999UL || endptr == NULL || *endptr != '.') {
        return false;
    }
    const char *minor = endptr + 1;
    if (*minor == '\0') {
        return false;
    }
    unsigned long minor_value = strtoul(minor, &endptr, 10);
    (void)minor_value;
    if (endptr == minor || endptr == NULL || *endptr != '.') {
        return false;
    }
    const char *patch = endptr + 1;
    if (*patch == '\0') {
        return false;
    }
    unsigned long patch_value = strtoul(patch, &endptr, 10);
    (void)patch_value;
    if (endptr == patch || endptr == NULL || *endptr != '\0') {
        return false;
    }

    *out_major = (uint32_t)major;
    return true;
}

static bool has_ebin_suffix(const char *name)
{
    if (name == NULL) {
        return false;
    }
    size_t len = strlen(name);
    return len > 5 && strcmp(name + len - 5, ".ebin") == 0;
}

static bool parse_ebin_filename(const char *filename,
                                char *out_module_id,
                                size_t out_module_id_len,
                                char *out_version,
                                size_t out_version_len)
{
    if (filename == NULL || out_module_id == NULL || out_version == NULL) {
        return false;
    }
    if (!has_ebin_suffix(filename)) {
        return false;
    }

    char stem[160] = {0};
    strlcpy(stem, filename, sizeof(stem));
    size_t stem_len = strlen(stem);
    if (stem_len <= 5) {
        return false;
    }
    stem[stem_len - 5] = '\0';

    char *dash = strrchr(stem, '-');
    if (dash == NULL || dash == stem || dash[1] == '\0') {
        return false;
    }
    *dash = '\0';

    uint32_t major = 0;
    if (!parse_semver_major(dash + 1, &major)) {
        return false;
    }

    strlcpy(out_module_id, stem, out_module_id_len);
    strlcpy(out_version, dash + 1, out_version_len);
    return true;
}

static bool runtime_catalog_append(const char *machine,
                                   const char *component,
                                   const char *module_id,
                                   const char *version,
                                   const char *path)
{
    if (s_ebin_catalog_runtime_count >= MAX_EBIN_CATALOG_ENTRIES) {
        return false;
    }

    ebin_catalog_entry_t *entry = &s_ebin_catalog_runtime[s_ebin_catalog_runtime_count++];
    snprintf(entry->machine, sizeof(entry->machine), "%s", machine);
    snprintf(entry->component, sizeof(entry->component), "%s", component);
    snprintf(entry->module_id, sizeof(entry->module_id), "%s", module_id);
    snprintf(entry->version, sizeof(entry->version), "%s", version);
    snprintf(entry->path, sizeof(entry->path), "%s", path);
    return true;
}

static void refresh_runtime_catalog(void)
{
    static const char *machine = "atari_st";
    static const char *components[] = {"cpu", "video", "io", "storage", "audio", "machine_profile"};

    s_ebin_catalog_runtime_count = 0;

    for (size_t component_index = 0; component_index < (sizeof(components) / sizeof(components[0])); component_index++) {
        const char *component = components[component_index];
        char dir_path[256];
        snprintf(dir_path, sizeof(dir_path), "/sdcard/ebins/%s/%s", machine, component);

        DIR *dir = opendir(dir_path);
        if (dir == NULL) {
            continue;
        }

        struct dirent *item = NULL;
        while ((item = readdir(dir)) != NULL) {
            const char *name = item->d_name;
            if (name == NULL || name[0] == '.') {
                continue;
            }

            char module_id[64] = {0};
            char version[24] = {0};
            if (!parse_ebin_filename(name, module_id, sizeof(module_id), version, sizeof(version))) {
                continue;
            }

            char full_path[320] = {0};
            if (strlcpy(full_path, dir_path, sizeof(full_path)) >= sizeof(full_path)) {
                continue;
            }
            if (strlcat(full_path, "/", sizeof(full_path)) >= sizeof(full_path)) {
                continue;
            }
            if (strlcat(full_path, name, sizeof(full_path)) >= sizeof(full_path)) {
                continue;
            }
            if (!runtime_catalog_append(machine, component, module_id, version, full_path)) {
                break;
            }
        }

        closedir(dir);
    }

    if (s_ebin_catalog_runtime_count == 0) {
        for (size_t i = 0; i < (sizeof(s_ebin_catalog_baked) / sizeof(s_ebin_catalog_baked[0])) &&
                           i < MAX_EBIN_CATALOG_ENTRIES;
             i++) {
            runtime_catalog_append(s_ebin_catalog_baked[i].machine,
                                   s_ebin_catalog_baked[i].component,
                                   s_ebin_catalog_baked[i].module_id,
                                   s_ebin_catalog_baked[i].version,
                                   s_ebin_catalog_baked[i].path);
        }
    }
}

static void get_active_catalog(const ebin_catalog_entry_t **out_entries, size_t *out_count)
{
    refresh_runtime_catalog();
    if (out_entries != NULL) {
        *out_entries = s_ebin_catalog_runtime;
    }
    if (out_count != NULL) {
        *out_count = s_ebin_catalog_runtime_count;
    }
}

static bool string_array_non_empty(const cJSON *array)
{
    if (!cJSON_IsArray(array) || cJSON_GetArraySize(array) <= 0) {
        return false;
    }
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, array)
    {
        if (!cJSON_IsString(item) || item->valuestring == NULL || item->valuestring[0] == '\0') {
            return false;
        }
    }
    return true;
}

static bool machine_target_contains_atari_st(const cJSON *array)
{
    if (!cJSON_IsArray(array)) {
        return false;
    }
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, array)
    {
        if (cJSON_IsString(item) && item->valuestring != NULL && strcmp(item->valuestring, "atari_st") == 0) {
            return true;
        }
    }
    return false;
}

static bool is_valid_sha256_hex(const char *value)
{
    if (value == NULL || strlen(value) != 64) {
        return false;
    }
    for (size_t i = 0; i < 64; i++) {
        char c = value[i];
        bool is_digit = (c >= '0' && c <= '9');
        bool is_lower_hex = (c >= 'a' && c <= 'f');
        bool is_upper_hex = (c >= 'A' && c <= 'F');
        if (!is_digit && !is_lower_hex && !is_upper_hex) {
            return false;
        }
    }
    return true;
}

static bool dependencies_schema_valid(const cJSON *dependencies)
{
    if (!cJSON_IsArray(dependencies)) {
        return false;
    }

    cJSON *dep = NULL;
    cJSON_ArrayForEach(dep, dependencies)
    {
        if (!cJSON_IsObject(dep)) {
            return false;
        }
        cJSON *dep_module_id = cJSON_GetObjectItemCaseSensitive(dep, "module_id");
        cJSON *dep_abi_range = cJSON_GetObjectItemCaseSensitive(dep, "abi_range");
        cJSON *dep_required = cJSON_GetObjectItemCaseSensitive(dep, "required");
        if (!cJSON_IsString(dep_module_id) || dep_module_id->valuestring == NULL || dep_module_id->valuestring[0] == '\0' ||
            !cJSON_IsString(dep_abi_range) || dep_abi_range->valuestring == NULL || dep_abi_range->valuestring[0] == '\0' ||
            !cJSON_IsBool(dep_required)) {
            return false;
        }
    }

    return true;
}

static bool ebin_dependency_available(const char *module_id)
{
    if (module_id == NULL || module_id[0] == '\0') {
        return false;
    }

    const ebin_catalog_entry_t *entries = NULL;
    size_t count = 0;
    get_active_catalog(&entries, &count);
    for (size_t i = 0; i < count; i++) {
        if (strcmp(module_id, entries[i].module_id) == 0) {
            return true;
        }
    }
    return false;
}

static bool parse_semver_triplet(const char *version, uint32_t *major, uint32_t *minor, uint32_t *patch)
{
    if (version == NULL || major == NULL || minor == NULL || patch == NULL || version[0] == '\0') {
        return false;
    }

    char *endptr = NULL;
    unsigned long value_major = strtoul(version, &endptr, 10);
    if (endptr == version || endptr == NULL || *endptr != '.') {
        return false;
    }
    const char *minor_ptr = endptr + 1;
    unsigned long value_minor = strtoul(minor_ptr, &endptr, 10);
    if (endptr == minor_ptr || endptr == NULL || *endptr != '.') {
        return false;
    }
    const char *patch_ptr = endptr + 1;
    unsigned long value_patch = strtoul(patch_ptr, &endptr, 10);
    if (endptr == patch_ptr || endptr == NULL || *endptr != '\0') {
        return false;
    }

    *major = (uint32_t)value_major;
    *minor = (uint32_t)value_minor;
    *patch = (uint32_t)value_patch;
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
    if (!parse_semver_triplet(left, &left_major, &left_minor, &left_patch) ||
        !parse_semver_triplet(right, &right_major, &right_minor, &right_patch)) {
        return strcmp(left, right);
    }
    if (left_major != right_major) {
        return left_major > right_major ? 1 : -1;
    }
    if (left_minor != right_minor) {
        return left_minor > right_minor ? 1 : -1;
    }
    if (left_patch != right_patch) {
        return left_patch > right_patch ? 1 : -1;
    }
    return 0;
}

static bool resolver_policy_valid(const char *policy)
{
    return policy != NULL && (strcmp(policy, "latest_compatible") == 0 || strcmp(policy, "pinned") == 0);
}

static esp_err_t append_resolved_entry(cJSON *resolved,
                                       const ebin_catalog_entry_t *entry,
                                       const char *component,
                                       const char *policy)
{
    cJSON *item = cJSON_CreateObject();
    if (item == NULL) {
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(item, "component", component);
    cJSON_AddStringToObject(item, "module_id", entry->module_id);
    cJSON_AddStringToObject(item, "version", entry->version);
    cJSON_AddStringToObject(item, "path", entry->path);
    cJSON_AddStringToObject(item, "selection_policy", policy);
    cJSON_AddItemToArray(resolved, item);
    return ESP_OK;
}

static bool catalog_contains_module_id(const char *module_id)
{
    if (module_id == NULL || module_id[0] == '\0') {
        return false;
    }

    const ebin_catalog_entry_t *entries = NULL;
    size_t count = 0;
    get_active_catalog(&entries, &count);
    for (size_t i = 0; i < count; i++) {
        if (strcmp(entries[i].module_id, module_id) == 0) {
            return true;
        }
    }
    return false;
}

static void runtime_mark_stage(const char *state, const char *stage)
{
    if (state != NULL) {
        snprintf(s_ebin_runtime.state, sizeof(s_ebin_runtime.state), "%s", state);
    }
    if (stage != NULL) {
        snprintf(s_ebin_runtime.last_stage, sizeof(s_ebin_runtime.last_stage), "%s", stage);
    }
    s_ebin_runtime.last_error_reason[0] = '\0';
    snprintf(s_ebin_runtime.last_recovery_action, sizeof(s_ebin_runtime.last_recovery_action), "%s", "none");
    s_ebin_runtime.last_recovered_module_id[0] = '\0';
    s_ebin_runtime.transition_seq++;
    s_ebin_runtime.updated_at_us = scheduler_now_us();
}

static void runtime_mark_failure(const char *stage, const char *reason)
{
    snprintf(s_ebin_runtime.state, sizeof(s_ebin_runtime.state), "%s", "recoverable_error");
    if (stage != NULL) {
        snprintf(s_ebin_runtime.last_stage, sizeof(s_ebin_runtime.last_stage), "%s", stage);
    }
    snprintf(s_ebin_runtime.last_error_reason,
             sizeof(s_ebin_runtime.last_error_reason),
             "%s",
             reason != NULL ? reason : "failure");
    snprintf(s_ebin_runtime.last_recovery_action,
             sizeof(s_ebin_runtime.last_recovery_action),
             "%s",
             "pending_recovery");
    s_ebin_runtime.last_recovered_module_id[0] = '\0';
    s_ebin_runtime.transition_seq++;
    s_ebin_runtime.updated_at_us = scheduler_now_us();
    s_ebin_runtime.last_fault_at_us = s_ebin_runtime.updated_at_us;
}

static void runtime_set_active_module(const char *module_id, const char *abi_version)
{
    s_ebin_runtime.has_active_module = true;
    snprintf(s_ebin_runtime.active_module_id,
             sizeof(s_ebin_runtime.active_module_id),
             "%s",
             module_id != NULL ? module_id : "");
    snprintf(s_ebin_runtime.active_abi_version,
             sizeof(s_ebin_runtime.active_abi_version),
             "%s",
             abi_version != NULL ? abi_version : "");
}

static void runtime_commit_last_known_good(const char *module_id, const char *abi_version)
{
    if (module_id == NULL || module_id[0] == '\0') {
        return;
    }
    s_ebin_runtime.has_last_known_good = true;
    snprintf(s_ebin_runtime.last_known_good_module_id,
             sizeof(s_ebin_runtime.last_known_good_module_id),
             "%s",
             module_id);
    snprintf(s_ebin_runtime.last_known_good_abi_version,
             sizeof(s_ebin_runtime.last_known_good_abi_version),
             "%s",
             abi_version != NULL ? abi_version : "");
}

static void runtime_apply_activation_recovery(bool prev_has_active_module,
                                              const char *prev_active_module_id,
                                              const char *prev_active_abi_version,
                                              bool force_fallback)
{
    const char *fallback_module_id = "st.cpu.m68k";
    const char *fallback_abi_version = "1.0.0";

    if (force_fallback && catalog_contains_module_id(fallback_module_id)) {
        runtime_set_active_module(fallback_module_id, fallback_abi_version);
        snprintf(s_ebin_runtime.state, sizeof(s_ebin_runtime.state), "%s", "running");
        snprintf(s_ebin_runtime.last_recovery_action,
                 sizeof(s_ebin_runtime.last_recovery_action),
                 "%s",
                 "fallback_module_set_activated");
        snprintf(s_ebin_runtime.last_recovered_module_id,
                 sizeof(s_ebin_runtime.last_recovered_module_id),
                 "%s",
                 fallback_module_id);
        runtime_commit_last_known_good(fallback_module_id, fallback_abi_version);
        return;
    }

    if (prev_has_active_module && prev_active_module_id != NULL && prev_active_module_id[0] != '\0') {
        runtime_set_active_module(prev_active_module_id, prev_active_abi_version);
        snprintf(s_ebin_runtime.state, sizeof(s_ebin_runtime.state), "%s", "running");
        snprintf(s_ebin_runtime.last_recovery_action,
                 sizeof(s_ebin_runtime.last_recovery_action),
                 "%s",
                 "rollback_previous_active_module");
        snprintf(s_ebin_runtime.last_recovered_module_id,
                 sizeof(s_ebin_runtime.last_recovered_module_id),
                 "%s",
                 prev_active_module_id);
        runtime_commit_last_known_good(prev_active_module_id, prev_active_abi_version);
        return;
    }

    if (s_ebin_runtime.has_last_known_good && s_ebin_runtime.last_known_good_module_id[0] != '\0') {
        runtime_set_active_module(s_ebin_runtime.last_known_good_module_id, s_ebin_runtime.last_known_good_abi_version);
        snprintf(s_ebin_runtime.state, sizeof(s_ebin_runtime.state), "%s", "running");
        snprintf(s_ebin_runtime.last_recovery_action,
                 sizeof(s_ebin_runtime.last_recovery_action),
                 "%s",
                 "rollback_last_known_good_snapshot");
        snprintf(s_ebin_runtime.last_recovered_module_id,
                 sizeof(s_ebin_runtime.last_recovered_module_id),
                 "%s",
                 s_ebin_runtime.last_known_good_module_id);
        return;
    }

    if (catalog_contains_module_id(fallback_module_id)) {
        runtime_set_active_module(fallback_module_id, fallback_abi_version);
        snprintf(s_ebin_runtime.state, sizeof(s_ebin_runtime.state), "%s", "running");
        snprintf(s_ebin_runtime.last_recovery_action,
                 sizeof(s_ebin_runtime.last_recovery_action),
                 "%s",
                 "fallback_module_set_activated");
        snprintf(s_ebin_runtime.last_recovered_module_id,
                 sizeof(s_ebin_runtime.last_recovered_module_id),
                 "%s",
                 fallback_module_id);
        runtime_commit_last_known_good(fallback_module_id, fallback_abi_version);
        return;
    }

    s_ebin_runtime.has_active_module = false;
    s_ebin_runtime.active_module_id[0] = '\0';
    s_ebin_runtime.active_abi_version[0] = '\0';
    snprintf(s_ebin_runtime.state, sizeof(s_ebin_runtime.state), "%s", "idle");
    snprintf(s_ebin_runtime.last_recovery_action,
             sizeof(s_ebin_runtime.last_recovery_action),
             "%s",
             "recovery_to_idle_no_fallback");
    s_ebin_runtime.last_recovered_module_id[0] = '\0';
}

static esp_err_t send_ebin_orchestration_error(httpd_req_t *req,
                                               const char *code,
                                               int status,
                                               const char *stage,
                                               const char *reason,
                                               const char *module_id)
{
    snprintf(s_ebin_runtime.last_fault_code,
             sizeof(s_ebin_runtime.last_fault_code),
             "%s",
             code != NULL ? code : "EBIN_INVALID");
    if (stage != NULL && stage[0] != '\0') {
        snprintf(s_ebin_runtime.last_stage, sizeof(s_ebin_runtime.last_stage), "%s", stage);
    }
    if (reason != NULL && reason[0] != '\0') {
        snprintf(s_ebin_runtime.last_error_reason, sizeof(s_ebin_runtime.last_error_reason), "%s", reason);
    }
    if (s_ebin_runtime.last_fault_at_us == 0) {
        s_ebin_runtime.last_fault_at_us = scheduler_now_us();
    }

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return esptari_web_send_json(req, "{\"ok\":false,\"error\":{\"code\":\"INTERNAL_ERROR\"}}", 500);
    }
    cJSON_AddBoolToObject(root, "ok", false);
    cJSON *error = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "error", error);
    cJSON_AddStringToObject(error, "code", code != NULL ? code : "EBIN_INVALID");
    cJSON_AddStringToObject(error, "category", "ebin");
    cJSON_AddBoolToObject(error, "retryable", true);

    cJSON *details = cJSON_CreateObject();
    cJSON_AddItemToObject(error, "details", details);
    cJSON_AddStringToObject(details, "stage", stage != NULL ? stage : "unknown");
    cJSON_AddStringToObject(details, "reason", reason != NULL ? reason : "orchestration_failed");
    cJSON_AddStringToObject(details, "runtime_state", s_ebin_runtime.state);
    cJSON_AddStringToObject(details, "module_id", module_id != NULL ? module_id : "");
    cJSON_AddNumberToObject(details, "transition_seq", (double)s_ebin_runtime.transition_seq);

    cJSON *fault = cJSON_CreateObject();
    cJSON_AddItemToObject(details, "fault_telemetry", fault);
    cJSON_AddStringToObject(fault, "fault_code", s_ebin_runtime.last_fault_code);
    cJSON_AddStringToObject(fault, "fault_stage", s_ebin_runtime.last_stage);
    cJSON_AddStringToObject(fault, "fault_reason", s_ebin_runtime.last_error_reason);
    cJSON_AddStringToObject(fault, "recovery_action", s_ebin_runtime.last_recovery_action);
    cJSON_AddStringToObject(fault, "recovered_module_id", s_ebin_runtime.last_recovered_module_id);
    cJSON_AddStringToObject(fault,
                            "last_known_good_module_id",
                            s_ebin_runtime.has_last_known_good ? s_ebin_runtime.last_known_good_module_id : "");
    cJSON_AddNumberToObject(fault, "fault_at_us", (double)s_ebin_runtime.last_fault_at_us);

    esp_err_t out = send_json_object(req, root, status);
    cJSON_Delete(root);
    return out;
}

static const char *wildcard_tail(const char *uri, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    if (strncmp(uri, prefix, prefix_len) == 0) {
        return uri + prefix_len;
    }
    return NULL;
}

static bool parse_interval_hours_from_cron(const char *cron, uint32_t *out_hours)
{
    if (cron == NULL || out_hours == NULL) {
        return false;
    }
    if (strcmp(cron, "0 * * * *") == 0) {
        *out_hours = 1;
        return true;
    }
    const char *prefix = "0 */";
    size_t prefix_len = strlen(prefix);
    if (strncmp(cron, prefix, prefix_len) != 0) {
        return false;
    }
    const char *tail = cron + prefix_len;
    char *endptr = NULL;
    unsigned long parsed = strtoul(tail, &endptr, 10);
    if (endptr == NULL || strcmp(endptr, " * * *") != 0 || parsed == 0 || parsed > 24) {
        return false;
    }
    *out_hours = (uint32_t)parsed;
    return true;
}

static uint64_t compute_next_run_at_us(uint64_t now_us, const char *cron)
{
    uint32_t interval_hours = 1;
    if (!parse_interval_hours_from_cron(cron, &interval_hours)) {
        return now_us + 3600000000ULL;
    }
    uint64_t interval_us = (uint64_t)interval_hours * 3600000000ULL;
    uint64_t bucket = now_us / interval_us;
    return (bucket + 1ULL) * interval_us;
}

static cJSON *schedule_to_json(const esptari_sync_schedule_t *schedule)
{
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "schedule_id", schedule->schedule_id);
    cJSON_AddStringToObject(item, "job_type", schedule->job_type);
    cJSON_AddStringToObject(item, "mode", schedule->mode);
    cJSON_AddStringToObject(item, "cron", schedule->cron);
    cJSON_AddBoolToObject(item, "enabled", schedule->enabled);
    cJSON_AddBoolToObject(item, "catch_up", schedule->catch_up);
    cJSON_AddNumberToObject(item, "created_at_us", (double)schedule->created_at_us);
    cJSON_AddNumberToObject(item, "updated_at_us", (double)schedule->updated_at_us);
    if (schedule->last_run_at_us == 0) {
        cJSON_AddNullToObject(item, "last_run_at_us");
    } else {
        cJSON_AddNumberToObject(item, "last_run_at_us", (double)schedule->last_run_at_us);
    }
    cJSON_AddNumberToObject(item, "next_run_at_us", (double)schedule->next_run_at_us);
    cJSON_AddStringToObject(item, "last_result", schedule->last_result[0] == '\0' ? "none" : schedule->last_result);
    if (schedule->last_error_code[0] == '\0') {
        cJSON_AddNullToObject(item, "last_error_code");
    } else {
        cJSON_AddStringToObject(item, "last_error_code", schedule->last_error_code);
    }
    return item;
}

static bool is_known_job_type(const char *job_type)
{
    return job_type != NULL &&
           (strcmp(job_type, "floppy_catalog_sync") == 0 || strcmp(job_type, "rom_catalog_sync") == 0 ||
            strcmp(job_type, "tos_catalog_sync") == 0);
}

static bool is_known_mode(const char *mode)
{
    return mode != NULL &&
           (strcmp(mode, "catalog_only") == 0 || strcmp(mode, "catalog_and_probe_links") == 0 ||
            strcmp(mode, "catalog_probe_and_prefetch_missing") == 0);
}

static void recovery_reset(uint64_t now_us)
{
    memset(&s_recovery_report, 0, sizeof(s_recovery_report));
    s_recovery_report.scheduler_now_us = now_us;
    snprintf(s_recovery_report.recovery_run_id,
             sizeof(s_recovery_report.recovery_run_id),
             "sched_recover_%06llu",
             (unsigned long long)(now_us % 1000000ULL));
}

static void recovery_quarantine_add(const char *schedule_id, const char *reason)
{
    s_recovery_report.quarantined++;
    if (s_recovery_report.quarantined > MAX_RECOVERY_QUARANTINE) {
        return;
    }
    esptari_recovery_quarantine_t *entry = &s_recovery_report.quarantine[s_recovery_report.quarantined - 1];
    snprintf(entry->schedule_id, sizeof(entry->schedule_id), "%s", schedule_id != NULL ? schedule_id : "unknown");
    snprintf(entry->error_code, sizeof(entry->error_code), "%s", "SCRAPER_SCHEDULE_INVALID");
    snprintf(entry->reason, sizeof(entry->reason), "%s", reason != NULL ? reason : "invalid record");
}

static bool duplicate_schedule_identity(const char *job_type, const char *mode, const char *cron, const char *exclude_id)
{
    for (size_t i = 0; i < s_schedule_count; i++) {
        const esptari_sync_schedule_t *schedule = &s_schedules[i];
        if (exclude_id != NULL && strcmp(schedule->schedule_id, exclude_id) == 0) {
            continue;
        }
        if (strcmp(schedule->job_type, job_type) == 0 && strcmp(schedule->mode, mode) == 0 && strcmp(schedule->cron, cron) == 0) {
            return true;
        }
    }
    return false;
}

static bool persist_schedules(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *schedules = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "schedules", schedules);

    for (size_t i = 0; i < s_schedule_count; i++) {
        cJSON_AddItemToArray(schedules, schedule_to_json(&s_schedules[i]));
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        return false;
    }

    size_t json_len = strlen(json);
    if (json_len >= sizeof(s_persisted_schedule_snapshot)) {
        cJSON_free(json);
        return false;
    }
    memcpy(s_persisted_schedule_snapshot, json, json_len + 1);
    cJSON_free(json);
    return true;
}

static void load_schedules_if_needed(void)
{
    if (s_initialized) {
        return;
    }
    s_initialized = true;

    uint64_t now_us = scheduler_now_us();
    recovery_reset(now_us);

    memset(s_recovery_parse_buffer, 0, sizeof(s_recovery_parse_buffer));
    if (s_persisted_schedule_snapshot[0] != '\0') {
        strlcpy(s_recovery_parse_buffer, s_persisted_schedule_snapshot, sizeof(s_recovery_parse_buffer));
    } else {
        return;
    }

    cJSON *root = cJSON_Parse(s_recovery_parse_buffer);
    if (root == NULL) {
        return;
    }

    cJSON *schedules = cJSON_GetObjectItemCaseSensitive(root, "schedules");
    if (!cJSON_IsArray(schedules)) {
        cJSON_Delete(root);
        return;
    }

    cJSON *schedule = NULL;
    cJSON_ArrayForEach(schedule, schedules)
    {
        s_recovery_report.loaded++;
        if (!cJSON_IsObject(schedule) || s_schedule_count >= MAX_SYNC_SCHEDULES) {
            recovery_quarantine_add("unknown", "record is not an object or schedule capacity exhausted");
            continue;
        }

        cJSON *schedule_id = cJSON_GetObjectItemCaseSensitive(schedule, "schedule_id");
        cJSON *job_type = cJSON_GetObjectItemCaseSensitive(schedule, "job_type");
        cJSON *mode = cJSON_GetObjectItemCaseSensitive(schedule, "mode");
        cJSON *cron = cJSON_GetObjectItemCaseSensitive(schedule, "cron");
        cJSON *enabled = cJSON_GetObjectItemCaseSensitive(schedule, "enabled");
        cJSON *catch_up = cJSON_GetObjectItemCaseSensitive(schedule, "catch_up");
        cJSON *created_at_us = cJSON_GetObjectItemCaseSensitive(schedule, "created_at_us");
        cJSON *updated_at_us = cJSON_GetObjectItemCaseSensitive(schedule, "updated_at_us");
        cJSON *last_run_at_us = cJSON_GetObjectItemCaseSensitive(schedule, "last_run_at_us");
        cJSON *next_run_at_us = cJSON_GetObjectItemCaseSensitive(schedule, "next_run_at_us");
        cJSON *last_result = cJSON_GetObjectItemCaseSensitive(schedule, "last_result");
        cJSON *last_error_code = cJSON_GetObjectItemCaseSensitive(schedule, "last_error_code");

        if (!cJSON_IsString(schedule_id) || !cJSON_IsString(job_type) || !cJSON_IsString(mode) || !cJSON_IsString(cron) ||
            !cJSON_IsBool(enabled) || !cJSON_IsBool(catch_up) || !cJSON_IsNumber(created_at_us) ||
            !cJSON_IsNumber(updated_at_us) || !cJSON_IsNumber(next_run_at_us) || !is_known_job_type(job_type->valuestring) ||
            !is_known_mode(mode->valuestring) || !parse_interval_hours_from_cron(cron->valuestring, &(uint32_t){0})) {
            recovery_quarantine_add(cJSON_IsString(schedule_id) ? schedule_id->valuestring : "unknown", "validation failed");
            continue;
        }

        esptari_sync_schedule_t *out = &s_schedules[s_schedule_count++];
        snprintf(out->schedule_id, sizeof(out->schedule_id), "%s", schedule_id->valuestring);
        snprintf(out->job_type, sizeof(out->job_type), "%s", job_type->valuestring);
        snprintf(out->mode, sizeof(out->mode), "%s", mode->valuestring);
        snprintf(out->cron, sizeof(out->cron), "%s", cron->valuestring);
        out->enabled = cJSON_IsTrue(enabled);
        out->catch_up = cJSON_IsTrue(catch_up);
        out->created_at_us = (uint64_t)created_at_us->valuedouble;
        out->updated_at_us = (uint64_t)updated_at_us->valuedouble;
        out->last_run_at_us = cJSON_IsNumber(last_run_at_us) ? (uint64_t)last_run_at_us->valuedouble : 0;
        out->next_run_at_us = (uint64_t)next_run_at_us->valuedouble;
        if (cJSON_IsString(last_result) && last_result->valuestring != NULL) {
            snprintf(out->last_result, sizeof(out->last_result), "%s", last_result->valuestring);
        } else {
            snprintf(out->last_result, sizeof(out->last_result), "%s", "none");
        }
        if (cJSON_IsString(last_error_code) && last_error_code->valuestring != NULL) {
            snprintf(out->last_error_code, sizeof(out->last_error_code), "%s", last_error_code->valuestring);
        } else {
            out->last_error_code[0] = '\0';
        }

        if (out->next_run_at_us < now_us) {
            if (out->catch_up) {
                out->next_run_at_us = now_us;
            } else {
                out->next_run_at_us = compute_next_run_at_us(now_us, out->cron);
            }
            s_recovery_report.recomputed_next_run++;
        }
        s_recovery_report.validated++;

        if (strncmp(out->schedule_id, "sch_", 4) == 0) {
            uint64_t parsed = strtoull(out->schedule_id + 4, NULL, 10);
            if (parsed > s_schedule_seq) {
                s_schedule_seq = parsed;
            }
        }
    }

    cJSON_Delete(root);

    if (s_recovery_report.recomputed_next_run > 0 || s_recovery_report.quarantined > 0) {
        persist_schedules();
    }
}

static void append_job(const char *trigger,
                       const char *schedule_id,
                       const char *job_type,
                       const char *mode,
                       const char *status,
                       uint64_t now_us)
{
    if (s_job_count >= MAX_SYNC_JOBS) {
        memmove(&s_jobs[0], &s_jobs[1], sizeof(s_jobs[0]) * (MAX_SYNC_JOBS - 1));
        s_job_count = MAX_SYNC_JOBS - 1;
    }

    esptari_sync_job_t *job = &s_jobs[s_job_count++];
    s_job_seq++;
    snprintf(job->job_id, sizeof(job->job_id), "job_%06llu", (unsigned long long)s_job_seq);
    snprintf(job->trigger, sizeof(job->trigger), "%s", trigger);
    snprintf(job->schedule_id, sizeof(job->schedule_id), "%s", schedule_id != NULL ? schedule_id : "");
    snprintf(job->job_type, sizeof(job->job_type), "%s", job_type);
    snprintf(job->mode, sizeof(job->mode), "%s", mode);
    snprintf(job->status, sizeof(job->status), "%s", status);
    job->created_at_us = now_us;
    job->started_at_us = now_us;
    job->completed_at_us = now_us;
}

static int due_sort_compare(const void *lhs_ptr, const void *rhs_ptr)
{
    const esptari_sync_schedule_t *lhs = *((const esptari_sync_schedule_t **)lhs_ptr);
    const esptari_sync_schedule_t *rhs = *((const esptari_sync_schedule_t **)rhs_ptr);
    if (lhs->next_run_at_us < rhs->next_run_at_us) {
        return -1;
    }
    if (lhs->next_run_at_us > rhs->next_run_at_us) {
        return 1;
    }
    return strcmp(lhs->schedule_id, rhs->schedule_id);
}

static void scheduler_tick(void)
{
    load_schedules_if_needed();

    uint64_t now_us = scheduler_now_us();
    esptari_sync_schedule_t *due[MAX_SYNC_SCHEDULES];
    size_t due_count = 0;

    for (size_t i = 0; i < s_schedule_count; i++) {
        if (!s_schedules[i].enabled) {
            continue;
        }
        if (s_schedules[i].next_run_at_us <= now_us) {
            due[due_count++] = &s_schedules[i];
        }
    }

    if (due_count == 0) {
        return;
    }

    qsort(due, due_count, sizeof(due[0]), due_sort_compare);

    bool mutated = false;
    uint32_t dispatch_budget = 1;
    for (size_t i = 0; i < due_count; i++) {
        esptari_sync_schedule_t *schedule = due[i];
        if (dispatch_budget == 0) {
            snprintf(schedule->last_result, sizeof(schedule->last_result), "%s", "skipped");
            snprintf(schedule->last_error_code, sizeof(schedule->last_error_code), "%s", "RUNTIME_SATURATED");
            schedule->updated_at_us = now_us;
            schedule->next_run_at_us = compute_next_run_at_us(now_us, schedule->cron);
            mutated = true;
            continue;
        }

        append_job("schedule", schedule->schedule_id, schedule->job_type, schedule->mode, "completed", now_us);
        schedule->last_run_at_us = now_us;
        schedule->updated_at_us = now_us;
        schedule->next_run_at_us = compute_next_run_at_us(now_us, schedule->cron);
        snprintf(schedule->last_result, sizeof(schedule->last_result), "%s", "success");
        schedule->last_error_code[0] = '\0';
        dispatch_budget--;
        mutated = true;
    }

    if (mutated) {
        persist_schedules();
    }
}

static esptari_sync_schedule_t *find_schedule_by_id(const char *schedule_id)
{
    for (size_t i = 0; i < s_schedule_count; i++) {
        if (strcmp(s_schedules[i].schedule_id, schedule_id) == 0) {
            return &s_schedules[i];
        }
    }
    return NULL;
}

static esp_err_t handle_catalog_sync_run(httpd_req_t *req)
{
    scheduler_tick();

    cJSON *json = NULL;
    if (!parse_json_request(req, &json)) {
        return ESP_OK;
    }

    cJSON *job_type = cJSON_GetObjectItemCaseSensitive(json, "job_type");
    cJSON *mode = cJSON_GetObjectItemCaseSensitive(json, "mode");
    if (!cJSON_IsString(job_type) || !cJSON_IsString(mode) || job_type->valuestring == NULL || mode->valuestring == NULL) {
        cJSON_Delete(json);
        return send_error(req, "BAD_REQUEST", 400);
    }

    uint64_t now_us = scheduler_now_us();
    append_job("manual", NULL, job_type->valuestring, mode->valuestring, "completed", now_us);
    cJSON_Delete(json);

    const esptari_sync_job_t *job = &s_jobs[s_job_count - 1];
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    cJSON_AddStringToObject(data, "job_id", job->job_id);
    cJSON_AddStringToObject(data, "trigger", job->trigger);
    cJSON_AddStringToObject(data, "status", "queued");
    cJSON_AddNumberToObject(data, "created_at_us", (double)job->created_at_us);

    esp_err_t out = send_json_object(req, resp, 202);
    cJSON_Delete(resp);
    return out;
}

static cJSON *job_to_json(const esptari_sync_job_t *job)
{
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "job_id", job->job_id);
    cJSON_AddStringToObject(item, "trigger", job->trigger);
    cJSON_AddStringToObject(item, "schedule_id", job->schedule_id[0] == '\0' ? "" : job->schedule_id);
    cJSON_AddStringToObject(item, "job_type", job->job_type);
    cJSON_AddStringToObject(item, "mode", job->mode);
    cJSON_AddStringToObject(item, "status", job->status);
    cJSON_AddNumberToObject(item, "created_at_us", (double)job->created_at_us);
    cJSON_AddNumberToObject(item, "started_at_us", (double)job->started_at_us);
    cJSON_AddNumberToObject(item, "completed_at_us", (double)job->completed_at_us);
    return item;
}

static esp_err_t handle_catalog_sync_jobs(httpd_req_t *req)
{
    scheduler_tick();

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    cJSON *jobs = cJSON_CreateArray();
    cJSON_AddItemToObject(data, "jobs", jobs);

    for (size_t i = 0; i < s_job_count; i++) {
        cJSON_AddItemToArray(jobs, job_to_json(&s_jobs[i]));
    }

    esp_err_t out = send_json_object(req, resp, 200);
    cJSON_Delete(resp);
    return out;
}

static esp_err_t handle_catalog_sync_job_by_id(httpd_req_t *req)
{
    scheduler_tick();

    const char *job_id = wildcard_tail(req->uri, "/api/v2/catalog-sync/jobs/");
    if (job_id == NULL || job_id[0] == '\0') {
        return send_error(req, "BAD_REQUEST", 400);
    }

    const esptari_sync_job_t *job = NULL;
    for (size_t i = 0; i < s_job_count; i++) {
        if (strcmp(s_jobs[i].job_id, job_id) == 0) {
            job = &s_jobs[i];
            break;
        }
    }
    if (job == NULL) {
        return send_error(req, "SCRAPER_JOB_NOT_FOUND", 404);
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = job_to_json(job);
    cJSON_AddItemToObject(resp, "data", data);

    esp_err_t out = send_json_object(req, resp, 200);
    cJSON_Delete(resp);
    return out;
}

static esp_err_t handle_catalog_sync_create_schedule(httpd_req_t *req)
{
    scheduler_tick();
    load_schedules_if_needed();

    cJSON *json = NULL;
    if (!parse_json_request(req, &json)) {
        return ESP_OK;
    }

    cJSON *job_type = cJSON_GetObjectItemCaseSensitive(json, "job_type");
    cJSON *mode = cJSON_GetObjectItemCaseSensitive(json, "mode");
    cJSON *cron = cJSON_GetObjectItemCaseSensitive(json, "cron");
    cJSON *enabled = cJSON_GetObjectItemCaseSensitive(json, "enabled");
    cJSON *catch_up = cJSON_GetObjectItemCaseSensitive(json, "catch_up");

    uint32_t interval_hours = 0;
    if (!cJSON_IsString(job_type) || !cJSON_IsString(mode) || !cJSON_IsString(cron) || !cJSON_IsBool(enabled) ||
        !cJSON_IsBool(catch_up) || !parse_interval_hours_from_cron(cron->valuestring, &interval_hours) ||
        !is_known_job_type(job_type->valuestring) || !is_known_mode(mode->valuestring)) {
        cJSON_Delete(json);
        return send_error(req, "SCRAPER_SCHEDULE_INVALID", 400);
    }
    if (duplicate_schedule_identity(job_type->valuestring, mode->valuestring, cron->valuestring, NULL)) {
        cJSON_Delete(json);
        return send_error(req, "CONFLICT", 409);
    }
    if (s_schedule_count >= MAX_SYNC_SCHEDULES) {
        cJSON_Delete(json);
        return send_error(req, "CONFLICT", 409);
    }

    uint64_t now_us = scheduler_now_us();
    esptari_sync_schedule_t *schedule = &s_schedules[s_schedule_count++];
    s_schedule_seq++;
    snprintf(schedule->schedule_id, sizeof(schedule->schedule_id), "sch_%06llu", (unsigned long long)s_schedule_seq);
    snprintf(schedule->job_type, sizeof(schedule->job_type), "%s", job_type->valuestring);
    snprintf(schedule->mode, sizeof(schedule->mode), "%s", mode->valuestring);
    snprintf(schedule->cron, sizeof(schedule->cron), "%s", cron->valuestring);
    schedule->enabled = cJSON_IsTrue(enabled);
    schedule->catch_up = cJSON_IsTrue(catch_up);
    schedule->created_at_us = now_us;
    schedule->updated_at_us = now_us;
    schedule->last_run_at_us = 0;
    schedule->next_run_at_us = compute_next_run_at_us(now_us, schedule->cron);
    snprintf(schedule->last_result, sizeof(schedule->last_result), "%s", "none");
    schedule->last_error_code[0] = '\0';

    cJSON_Delete(json);

    if (!persist_schedules()) {
        s_schedule_count--;
        return send_error(req, "CATALOG_SYNC_FAILED", 409);
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = schedule_to_json(schedule);
    cJSON_AddItemToObject(resp, "data", data);

    esp_err_t out = send_json_object(req, resp, 201);
    cJSON_Delete(resp);
    return out;
}

static int schedule_list_compare(const void *lhs_ptr, const void *rhs_ptr)
{
    const esptari_sync_schedule_t *lhs = *((const esptari_sync_schedule_t **)lhs_ptr);
    const esptari_sync_schedule_t *rhs = *((const esptari_sync_schedule_t **)rhs_ptr);
    if (lhs->next_run_at_us < rhs->next_run_at_us) {
        return -1;
    }
    if (lhs->next_run_at_us > rhs->next_run_at_us) {
        return 1;
    }
    return strcmp(lhs->schedule_id, rhs->schedule_id);
}

static esp_err_t handle_catalog_sync_list_schedules(httpd_req_t *req)
{
    scheduler_tick();
    load_schedules_if_needed();

    esptari_sync_schedule_t *ordered[MAX_SYNC_SCHEDULES];
    for (size_t i = 0; i < s_schedule_count; i++) {
        ordered[i] = &s_schedules[i];
    }
    qsort(ordered, s_schedule_count, sizeof(ordered[0]), schedule_list_compare);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    cJSON_AddNumberToObject(data, "scheduler_now_us", (double)scheduler_now_us());
    cJSON *schedules = cJSON_CreateArray();
    cJSON_AddItemToObject(data, "schedules", schedules);

    for (size_t i = 0; i < s_schedule_count; i++) {
        cJSON_AddItemToArray(schedules, schedule_to_json(ordered[i]));
    }

    esp_err_t out = send_json_object(req, resp, 200);
    cJSON_Delete(resp);
    return out;
}

static esp_err_t handle_catalog_sync_get_schedule(httpd_req_t *req)
{
    scheduler_tick();
    load_schedules_if_needed();

    const char *schedule_id = wildcard_tail(req->uri, "/api/v2/catalog-sync/schedules/");
    if (schedule_id == NULL || schedule_id[0] == '\0') {
        return send_error(req, "BAD_REQUEST", 400);
    }

    esptari_sync_schedule_t *schedule = find_schedule_by_id(schedule_id);
    if (schedule == NULL) {
        return send_error(req, "SCRAPER_JOB_NOT_FOUND", 404);
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    cJSON_AddNumberToObject(data, "scheduler_now_us", (double)scheduler_now_us());
    cJSON_AddItemToObject(data, "schedule", schedule_to_json(schedule));

    esp_err_t out = send_json_object(req, resp, 200);
    cJSON_Delete(resp);
    return out;
}

static esp_err_t handle_catalog_sync_delete_schedule(httpd_req_t *req)
{
    load_schedules_if_needed();

    const char *schedule_id = wildcard_tail(req->uri, "/api/v2/catalog-sync/schedules/");
    if (schedule_id == NULL || schedule_id[0] == '\0') {
        return send_error(req, "BAD_REQUEST", 400);
    }

    for (size_t i = 0; i < s_schedule_count; i++) {
        if (strcmp(s_schedules[i].schedule_id, schedule_id) == 0) {
            if (i + 1 < s_schedule_count) {
                memmove(&s_schedules[i], &s_schedules[i + 1], sizeof(s_schedules[0]) * (s_schedule_count - i - 1));
            }
            s_schedule_count--;
            if (!persist_schedules()) {
                return send_error(req, "CATALOG_SYNC_FAILED", 409);
            }

            cJSON *resp = cJSON_CreateObject();
            cJSON_AddBoolToObject(resp, "ok", true);
            cJSON *data = cJSON_CreateObject();
            cJSON_AddItemToObject(resp, "data", data);
            cJSON_AddStringToObject(data, "schedule_id", schedule_id);
            cJSON_AddStringToObject(data, "status", "deleted");

            esp_err_t out = send_json_object(req, resp, 200);
            cJSON_Delete(resp);
            return out;
        }
    }

    return send_error(req, "SCRAPER_JOB_NOT_FOUND", 404);
}

static esp_err_t handle_catalog_sync_patch_schedule(httpd_req_t *req)
{
    scheduler_tick();
    load_schedules_if_needed();

    const char *schedule_id = wildcard_tail(req->uri, "/api/v2/catalog-sync/schedules/");
    if (schedule_id == NULL || schedule_id[0] == '\0') {
        return send_error(req, "BAD_REQUEST", 400);
    }

    esptari_sync_schedule_t *schedule = find_schedule_by_id(schedule_id);
    if (schedule == NULL) {
        return send_error(req, "SCRAPER_JOB_NOT_FOUND", 404);
    }

    cJSON *json = NULL;
    if (!parse_json_request(req, &json)) {
        return ESP_OK;
    }

    cJSON *job_type = cJSON_GetObjectItemCaseSensitive(json, "job_type");
    cJSON *mode = cJSON_GetObjectItemCaseSensitive(json, "mode");
    cJSON *cron = cJSON_GetObjectItemCaseSensitive(json, "cron");
    cJSON *enabled = cJSON_GetObjectItemCaseSensitive(json, "enabled");
    cJSON *catch_up = cJSON_GetObjectItemCaseSensitive(json, "catch_up");

    if ((job_type != NULL && (!cJSON_IsString(job_type) || !is_known_job_type(job_type->valuestring))) ||
        (mode != NULL && (!cJSON_IsString(mode) || !is_known_mode(mode->valuestring))) ||
        (cron != NULL && (!cJSON_IsString(cron) || !parse_interval_hours_from_cron(cron->valuestring, &(uint32_t){0}))) ||
        (enabled != NULL && !cJSON_IsBool(enabled)) || (catch_up != NULL && !cJSON_IsBool(catch_up))) {
        cJSON_Delete(json);
        return send_error(req, "SCRAPER_SCHEDULE_INVALID", 400);
    }

    esptari_sync_schedule_t updated = *schedule;
    bool has_change = false;
    if (job_type != NULL) {
        snprintf(updated.job_type, sizeof(updated.job_type), "%s", job_type->valuestring);
        has_change = true;
    }
    if (mode != NULL) {
        snprintf(updated.mode, sizeof(updated.mode), "%s", mode->valuestring);
        has_change = true;
    }
    bool cron_changed = false;
    if (cron != NULL) {
        snprintf(updated.cron, sizeof(updated.cron), "%s", cron->valuestring);
        has_change = true;
        cron_changed = true;
    }
    bool was_enabled = updated.enabled;
    if (enabled != NULL) {
        updated.enabled = cJSON_IsTrue(enabled);
        has_change = true;
    }
    if (catch_up != NULL) {
        updated.catch_up = cJSON_IsTrue(catch_up);
        has_change = true;
    }
    cJSON_Delete(json);

    if (duplicate_schedule_identity(updated.job_type, updated.mode, updated.cron, schedule->schedule_id)) {
        return send_error(req, "CONFLICT", 409);
    }

    uint64_t now_us = scheduler_now_us();
    if (has_change) {
        if (cron_changed || (was_enabled == false && updated.enabled)) {
            updated.next_run_at_us = compute_next_run_at_us(now_us, updated.cron);
        }
        if (updated.updated_at_us >= now_us) {
            now_us = updated.updated_at_us + 1;
        }
        updated.updated_at_us = now_us;
    }

    esptari_sync_schedule_t original = *schedule;
    *schedule = updated;
    if (has_change && !persist_schedules()) {
        *schedule = original;
        return send_error(req, "CATALOG_SYNC_FAILED", 409);
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = schedule_to_json(schedule);
    cJSON_AddItemToObject(resp, "data", data);

    esp_err_t out = send_json_object(req, resp, 200);
    cJSON_Delete(resp);
    return out;
}

static esp_err_t handle_catalog_sync_recovery_report(httpd_req_t *req)
{
    load_schedules_if_needed();

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "data", data);
    cJSON_AddStringToObject(data, "recovery_run_id", s_recovery_report.recovery_run_id);
    cJSON_AddNumberToObject(data, "scheduler_now_us", (double)s_recovery_report.scheduler_now_us);
    cJSON_AddNumberToObject(data, "loaded", s_recovery_report.loaded);
    cJSON_AddNumberToObject(data, "validated", s_recovery_report.validated);
    cJSON_AddNumberToObject(data, "recomputed_next_run", s_recovery_report.recomputed_next_run);
    cJSON_AddNumberToObject(data, "quarantined", s_recovery_report.quarantined);
    cJSON *quarantine = cJSON_CreateArray();
    cJSON_AddItemToObject(data, "quarantine", quarantine);
    uint32_t quarantine_count = s_recovery_report.quarantined;
    if (quarantine_count > MAX_RECOVERY_QUARANTINE) {
        quarantine_count = MAX_RECOVERY_QUARANTINE;
    }
    for (uint32_t i = 0; i < quarantine_count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "schedule_id", s_recovery_report.quarantine[i].schedule_id);
        cJSON_AddStringToObject(item, "error_code", s_recovery_report.quarantine[i].error_code);
        cJSON_AddStringToObject(item, "reason", s_recovery_report.quarantine[i].reason);
        cJSON_AddItemToArray(quarantine, item);
    }

    esp_err_t out = send_json_object(req, resp, 200);
    cJSON_Delete(resp);
    return out;
}

static esp_err_t handle_ebins_catalog(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *items = cJSON_AddArrayToObject(root, "items");

    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "name", "cpu_core.ebin");
    cJSON_AddStringToObject(item, "version", "1.0.0");
    cJSON_AddBoolToObject(item, "loaded", true);
    cJSON_AddItemToArray(items, item);

    esp_err_t out = send_json_object(req, root, 200);
    cJSON_Delete(root);
    return out;
}

static esp_err_t handle_ebins_rescan(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "queued");
    cJSON_AddNumberToObject(root, "queuedAtMs", (double)(scheduler_now_us() / 1000ULL));
    esp_err_t out = send_json_object(req, root, 202);
    cJSON_Delete(root);
    return out;
}

static esp_err_t handle_ebins_validate(httpd_req_t *req)
{
    cJSON *json = NULL;
    if (!parse_json_request(req, &json)) {
        return ESP_OK;
    }

    if (!cJSON_IsObject(json)) {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "BAD_REQUEST", 400, "payload", "request body must be an object");
    }

    cJSON *module_id = cJSON_GetObjectItemCaseSensitive(json, "module_id");
    cJSON *module_type = cJSON_GetObjectItemCaseSensitive(json, "module_type");
    cJSON *machine_targets = cJSON_GetObjectItemCaseSensitive(json, "machine_targets");
    cJSON *abi_version = cJSON_GetObjectItemCaseSensitive(json, "abi_version");
    cJSON *api_contract_version = cJSON_GetObjectItemCaseSensitive(json, "api_contract_version");
    cJSON *exports = cJSON_GetObjectItemCaseSensitive(json, "exports");
    cJSON *dependencies = cJSON_GetObjectItemCaseSensitive(json, "dependencies");
    cJSON *build_fingerprint = cJSON_GetObjectItemCaseSensitive(json, "build_fingerprint");
    cJSON *payload_sha256 = cJSON_GetObjectItemCaseSensitive(json, "payload_sha256");
    cJSON *signature = cJSON_GetObjectItemCaseSensitive(json, "signature");

    if (!cJSON_IsString(module_id) || module_id->valuestring == NULL || module_id->valuestring[0] == '\0') {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "BAD_REQUEST", 400, "module_id", "required non-empty string");
    }
    if (!cJSON_IsString(module_type) || module_type->valuestring == NULL || module_type->valuestring[0] == '\0') {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "BAD_REQUEST", 400, "module_type", "required non-empty string");
    }
    if (!cJSON_IsArray(machine_targets)) {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "BAD_REQUEST", 400, "machine_targets", "required array of target machines");
    }
    if (!cJSON_IsString(abi_version) || abi_version->valuestring == NULL || abi_version->valuestring[0] == '\0') {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "BAD_REQUEST", 400, "abi_version", "required semver string");
    }
    if (!cJSON_IsString(api_contract_version) || api_contract_version->valuestring == NULL || api_contract_version->valuestring[0] == '\0') {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "BAD_REQUEST", 400, "api_contract_version", "required semver string");
    }
    if (!cJSON_IsArray(exports)) {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "BAD_REQUEST", 400, "exports", "required array of exported symbols");
    }
    if (!cJSON_IsArray(dependencies)) {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "BAD_REQUEST", 400, "dependencies", "required dependency array");
    }
    if (!cJSON_IsString(build_fingerprint) || build_fingerprint->valuestring == NULL || build_fingerprint->valuestring[0] == '\0') {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "BAD_REQUEST", 400, "build_fingerprint", "required non-empty string");
    }
    if (!cJSON_IsString(payload_sha256) || payload_sha256->valuestring == NULL || payload_sha256->valuestring[0] == '\0') {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "BAD_REQUEST", 400, "payload_sha256", "required SHA-256 hex string");
    }

    if (!is_known_ebin_module_type(module_type->valuestring)) {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "EBIN_INVALID", 400, "module_type", "unsupported module type");
    }
    if (!string_array_non_empty(machine_targets) || !machine_target_contains_atari_st(machine_targets)) {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "EBIN_INVALID", 400, "machine_targets", "must include atari_st target");
    }
    if (!string_array_non_empty(exports)) {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "EBIN_INVALID", 400, "exports", "must include at least one export symbol");
    }
    if (!dependencies_schema_valid(dependencies)) {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "EBIN_INVALID", 400, "dependencies", "dependency entries must include module_id abi_range required");
    }
    if (!is_valid_sha256_hex(payload_sha256->valuestring)) {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "EBIN_INVALID", 400, "payload_sha256", "must be 64-char hex string");
    }

    uint32_t abi_major = 0;
    uint32_t api_contract_major = 0;
    if (!parse_semver_major(abi_version->valuestring, &abi_major)) {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "EBIN_INVALID", 400, "abi_version", "must be semantic version MAJOR.MINOR.PATCH");
    }
    if (!parse_semver_major(api_contract_version->valuestring, &api_contract_major)) {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "EBIN_INVALID", 400, "api_contract_version", "must be semantic version MAJOR.MINOR.PATCH");
    }
    if (abi_major != 1U) {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "EBIN_ABI_MISMATCH", 409, "abi_version", "unsupported major ABI; expected 1.x.x");
    }

    if (signature != NULL && !cJSON_IsNull(signature)) {
        if (!cJSON_IsObject(signature)) {
            cJSON_Delete(json);
            return send_ebin_validation_error(req, "EBIN_INVALID", 400, "signature", "signature must be object when provided");
        }
        cJSON *algorithm = cJSON_GetObjectItemCaseSensitive(signature, "algorithm");
        cJSON *key_id = cJSON_GetObjectItemCaseSensitive(signature, "key_id");
        cJSON *value = cJSON_GetObjectItemCaseSensitive(signature, "value");
        if (!cJSON_IsString(algorithm) || algorithm->valuestring == NULL || algorithm->valuestring[0] == '\0' ||
            !cJSON_IsString(key_id) || key_id->valuestring == NULL || key_id->valuestring[0] == '\0' ||
            !cJSON_IsString(value) || value->valuestring == NULL || value->valuestring[0] == '\0') {
            cJSON_Delete(json);
            return send_ebin_validation_error(req, "EBIN_INVALID", 400, "signature", "signature requires algorithm key_id value strings");
        }
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "data", data);
    cJSON_AddStringToObject(data, "module_id", module_id->valuestring);
    cJSON_AddBoolToObject(data, "valid", true);
    cJSON_AddStringToObject(data, "status", "ok");
    cJSON *normalized = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "normalized", normalized);
    cJSON_AddStringToObject(normalized, "machine", "atari_st");
    cJSON_AddStringToObject(normalized, "module_type", module_type->valuestring);
    cJSON_AddNumberToObject(normalized, "abi_major", (double)abi_major);
    cJSON_AddNumberToObject(normalized, "api_contract_major", (double)api_contract_major);
    cJSON_AddNumberToObject(normalized, "dependency_count", (double)cJSON_GetArraySize(dependencies));
    cJSON_AddNumberToObject(normalized, "export_count", (double)cJSON_GetArraySize(exports));

    cJSON_Delete(json);
    esp_err_t out = send_json_object(req, root, 200);
    cJSON_Delete(root);
    return out;
}

static esp_err_t handle_ebins_load(httpd_req_t *req)
{
    cJSON *json = NULL;
    if (!parse_json_request(req, &json)) {
        return ESP_OK;
    }

    if (!cJSON_IsObject(json)) {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "BAD_REQUEST", 400, "payload", "request body must be an object");
    }

    cJSON *module_id = cJSON_GetObjectItemCaseSensitive(json, "module_id");
    cJSON *abi_version = cJSON_GetObjectItemCaseSensitive(json, "abi_version");
    cJSON *payload_sha256 = cJSON_GetObjectItemCaseSensitive(json, "payload_sha256");
    cJSON *signature = cJSON_GetObjectItemCaseSensitive(json, "signature");
    cJSON *dependencies = cJSON_GetObjectItemCaseSensitive(json, "dependencies");
    cJSON *simulate_fail_stage = cJSON_GetObjectItemCaseSensitive(json, "simulate_fail_stage");
    cJSON *force_fallback = cJSON_GetObjectItemCaseSensitive(json, "force_fallback");
    bool prefer_fallback_recovery = cJSON_IsBool(force_fallback) && cJSON_IsTrue(force_fallback);

    bool prev_has_active_module = s_ebin_runtime.has_active_module;
    char prev_active_module_id[64];
    char prev_active_abi_version[16];
    snprintf(prev_active_module_id, sizeof(prev_active_module_id), "%s", s_ebin_runtime.active_module_id);
    snprintf(prev_active_abi_version, sizeof(prev_active_abi_version), "%s", s_ebin_runtime.active_abi_version);

    if (!cJSON_IsString(module_id) || module_id->valuestring == NULL || module_id->valuestring[0] == '\0') {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "BAD_REQUEST", 400, "module_id", "required non-empty string");
    }
    if (!cJSON_IsString(abi_version) || abi_version->valuestring == NULL || abi_version->valuestring[0] == '\0') {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "BAD_REQUEST", 400, "abi_version", "required semver string");
    }
    if (!cJSON_IsString(payload_sha256) || payload_sha256->valuestring == NULL || payload_sha256->valuestring[0] == '\0') {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "BAD_REQUEST", 400, "payload_sha256", "required SHA-256 hex string");
    }
    if (!cJSON_IsArray(dependencies)) {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "BAD_REQUEST", 400, "dependencies", "required dependency array");
    }

    uint32_t abi_major = 0;
    if (!parse_semver_major(abi_version->valuestring, &abi_major)) {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "EBIN_INVALID", 400, "abi_version", "must be semantic version MAJOR.MINOR.PATCH");
    }

    runtime_mark_stage("loading", "resolve");

    if (!catalog_contains_module_id(module_id->valuestring)) {
        runtime_mark_failure("resolve", "load_resolve_failed_module_not_found");
        cJSON_Delete(json);
        return send_ebin_orchestration_error(req,
                                             "EBIN_NOT_FOUND",
                                             404,
                                             "resolve",
                                             "load_resolve_failed_module_not_found",
                                             module_id->valuestring);
    }
    if (cJSON_IsString(simulate_fail_stage) && simulate_fail_stage->valuestring != NULL &&
        strcmp(simulate_fail_stage->valuestring, "resolve") == 0) {
        runtime_mark_failure("resolve", "load_resolve_failed_simulated");
        cJSON_Delete(json);
        return send_ebin_orchestration_error(req,
                                             "EBIN_NOT_FOUND",
                                             404,
                                             "resolve",
                                             "load_resolve_failed_simulated",
                                             module_id->valuestring);
    }

    runtime_mark_stage("loading", "validate");

    // Gate 1: integrity
    if (!is_valid_sha256_hex(payload_sha256->valuestring)) {
        runtime_mark_failure("validate", "load_validate_failed_invalid_hash");
        cJSON_Delete(json);
        return send_ebin_orchestration_error(req,
                                             "EBIN_INVALID",
                                             400,
                                             "validate",
                                             "load_validate_failed_invalid_hash",
                                             module_id->valuestring);
    }

    // Gate 2: signature
    if (!cJSON_IsObject(signature)) {
        runtime_mark_failure("validate", "load_validate_failed_missing_signature");
        cJSON_Delete(json);
        return send_ebin_orchestration_error(req,
                                             "EBIN_SIGNATURE_INVALID",
                                             409,
                                             "validate",
                                             "load_validate_failed_missing_signature",
                                             module_id->valuestring);
    }
    cJSON *algorithm = cJSON_GetObjectItemCaseSensitive(signature, "algorithm");
    cJSON *key_id = cJSON_GetObjectItemCaseSensitive(signature, "key_id");
    cJSON *value = cJSON_GetObjectItemCaseSensitive(signature, "value");
    if (!cJSON_IsString(algorithm) || algorithm->valuestring == NULL || algorithm->valuestring[0] == '\0' ||
        !cJSON_IsString(key_id) || key_id->valuestring == NULL || key_id->valuestring[0] == '\0' ||
        !cJSON_IsString(value) || value->valuestring == NULL || value->valuestring[0] == '\0') {
        runtime_mark_failure("validate", "load_validate_failed_signature_fields");
        cJSON_Delete(json);
        return send_ebin_orchestration_error(req,
                                             "EBIN_SIGNATURE_INVALID",
                                             409,
                                             "validate",
                                             "load_validate_failed_signature_fields",
                                             module_id->valuestring);
    }

    // Gate 3: dependency compatibility
    if (abi_major != 1U) {
        runtime_mark_failure("validate", "load_validate_failed_module_abi_incompatible");
        cJSON_Delete(json);
        return send_ebin_orchestration_error(req,
                                             "EBIN_ABI_MISMATCH",
                                             409,
                                             "validate",
                                             "load_validate_failed_module_abi_incompatible",
                                             module_id->valuestring);
    }
    if (!dependencies_schema_valid(dependencies)) {
        runtime_mark_failure("validate", "load_validate_failed_dependency_schema");
        cJSON_Delete(json);
        return send_ebin_orchestration_error(req,
                                             "EBIN_INVALID",
                                             400,
                                             "validate",
                                             "load_validate_failed_dependency_schema",
                                             module_id->valuestring);
    }

    cJSON *dep = NULL;
    cJSON_ArrayForEach(dep, dependencies)
    {
        cJSON *dep_module_id = cJSON_GetObjectItemCaseSensitive(dep, "module_id");
        cJSON *dep_abi_range = cJSON_GetObjectItemCaseSensitive(dep, "abi_range");
        cJSON *dep_required = cJSON_GetObjectItemCaseSensitive(dep, "required");
        if (cJSON_IsBool(dep_required) && cJSON_IsTrue(dep_required) &&
            (!cJSON_IsString(dep_module_id) || dep_module_id->valuestring == NULL ||
             !ebin_dependency_available(dep_module_id->valuestring))) {
            runtime_mark_failure("validate", "load_validate_failed_required_dependency_missing");
            cJSON_Delete(json);
            return send_ebin_orchestration_error(req,
                                                 "EBIN_DEPENDENCY_MISSING",
                                                 409,
                                                 "validate",
                                                 "load_validate_failed_required_dependency_missing",
                                                 module_id->valuestring);
        }
        if (cJSON_IsString(dep_abi_range) && dep_abi_range->valuestring != NULL &&
            strcmp(dep_abi_range->valuestring, "1.0.x") != 0) {
            runtime_mark_failure("validate", "load_validate_failed_dependency_abi_range_unsupported");
            cJSON_Delete(json);
            return send_ebin_orchestration_error(req,
                                                 "EBIN_ABI_MISMATCH",
                                                 409,
                                                 "validate",
                                                 "load_validate_failed_dependency_abi_range_unsupported",
                                                 module_id->valuestring);
        }
    }

    runtime_mark_stage("loading", "bind");
    if (s_ebin_runtime.has_active_module && strcmp(s_ebin_runtime.active_module_id, module_id->valuestring) != 0) {
        runtime_mark_failure("bind", "load_bind_failed_runtime_busy_with_other_module");
        runtime_apply_activation_recovery(
            prev_has_active_module, prev_active_module_id, prev_active_abi_version, prefer_fallback_recovery);
        cJSON_Delete(json);
        return send_ebin_orchestration_error(req,
                                             "EBIN_INVALID",
                                             409,
                                             "bind",
                                             "load_bind_failed_runtime_busy_with_other_module",
                                             module_id->valuestring);
    }
    if (cJSON_IsString(simulate_fail_stage) && simulate_fail_stage->valuestring != NULL &&
        strcmp(simulate_fail_stage->valuestring, "bind") == 0) {
        runtime_mark_failure("bind", "load_bind_failed_simulated");
        runtime_apply_activation_recovery(
            prev_has_active_module, prev_active_module_id, prev_active_abi_version, prefer_fallback_recovery);
        cJSON_Delete(json);
        return send_ebin_orchestration_error(req,
                                             "EBIN_INVALID",
                                             409,
                                             "bind",
                                             "load_bind_failed_simulated",
                                             module_id->valuestring);
    }

    runtime_mark_stage("loading", "init");
    if (cJSON_IsString(simulate_fail_stage) && simulate_fail_stage->valuestring != NULL &&
        strcmp(simulate_fail_stage->valuestring, "init") == 0) {
        runtime_mark_failure("init", "load_init_failed_simulated");
        runtime_apply_activation_recovery(
            prev_has_active_module, prev_active_module_id, prev_active_abi_version, prefer_fallback_recovery);
        cJSON_Delete(json);
        return send_ebin_orchestration_error(req,
                                             "EBIN_INVALID",
                                             409,
                                             "init",
                                             "load_init_failed_simulated",
                                             module_id->valuestring);
    }

    runtime_set_active_module(module_id->valuestring, abi_version->valuestring);
    runtime_mark_stage("running", "init");
    runtime_commit_last_known_good(module_id->valuestring, abi_version->valuestring);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "data", data);
    cJSON_AddStringToObject(data, "module_id", module_id->valuestring);
    cJSON_AddStringToObject(data, "status", "loaded");
    cJSON *transitions = cJSON_CreateArray();
    cJSON_AddItemToObject(data, "transitions", transitions);
    cJSON_AddItemToArray(transitions, cJSON_CreateString("resolve"));
    cJSON_AddItemToArray(transitions, cJSON_CreateString("validate"));
    cJSON_AddItemToArray(transitions, cJSON_CreateString("bind"));
    cJSON_AddItemToArray(transitions, cJSON_CreateString("init"));
    cJSON_AddStringToObject(data, "runtime_state", s_ebin_runtime.state);
    cJSON_AddNumberToObject(data, "transition_seq", (double)s_ebin_runtime.transition_seq);
    cJSON *fault = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "fault_telemetry", fault);
    cJSON_AddStringToObject(fault, "fault_code", s_ebin_runtime.last_fault_code);
    cJSON_AddStringToObject(fault, "fault_stage", s_ebin_runtime.last_stage);
    cJSON_AddStringToObject(fault, "fault_reason", s_ebin_runtime.last_error_reason);
    cJSON_AddStringToObject(fault, "recovery_action", s_ebin_runtime.last_recovery_action);
    cJSON_AddStringToObject(fault, "recovered_module_id", s_ebin_runtime.last_recovered_module_id);
    cJSON_AddStringToObject(fault,
                            "last_known_good_module_id",
                            s_ebin_runtime.has_last_known_good ? s_ebin_runtime.last_known_good_module_id : "");
    cJSON_AddNumberToObject(fault, "fault_at_us", (double)s_ebin_runtime.last_fault_at_us);
    cJSON_AddNumberToObject(data, "loaded_at_us", (double)s_ebin_runtime.updated_at_us);

    cJSON_Delete(json);
    esp_err_t out = send_json_object(req, root, 200);
    cJSON_Delete(root);
    return out;
}

static esp_err_t handle_ebins_resolve(httpd_req_t *req)
{
    cJSON *json = NULL;
    if (!parse_json_request(req, &json)) {
        return ESP_OK;
    }

    if (!cJSON_IsObject(json)) {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "BAD_REQUEST", 400, "payload", "request body must be an object");
    }

    cJSON *machine = cJSON_GetObjectItemCaseSensitive(json, "machine");
    cJSON *components = cJSON_GetObjectItemCaseSensitive(json, "components");
    cJSON *version_policy = cJSON_GetObjectItemCaseSensitive(json, "version_policy");
    cJSON *pinned_versions = cJSON_GetObjectItemCaseSensitive(json, "pinned_versions");
    cJSON *force_ambiguous_component = cJSON_GetObjectItemCaseSensitive(json, "force_ambiguous_component");

    const char *policy = (cJSON_IsString(version_policy) && version_policy->valuestring != NULL)
                             ? version_policy->valuestring
                             : "latest_compatible";

    if (!cJSON_IsString(machine) || machine->valuestring == NULL || machine->valuestring[0] == '\0') {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "BAD_REQUEST", 400, "machine", "required non-empty string");
    }
    if (!cJSON_IsArray(components) || cJSON_GetArraySize(components) <= 0) {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "BAD_REQUEST", 400, "components", "required non-empty component array");
    }
    if (!resolver_policy_valid(policy)) {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "BAD_REQUEST", 400, "version_policy", "must be latest_compatible or pinned");
    }
    if (strcmp(policy, "pinned") == 0 && !cJSON_IsObject(pinned_versions)) {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "BAD_REQUEST", 400, "pinned_versions", "required object when version_policy is pinned");
    }
    if (strcmp(machine->valuestring, "atari_st") != 0) {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "EBIN_NOT_FOUND", 404, "machine", "resolver_machine_not_indexed");
    }

    cJSON *resolved = cJSON_CreateArray();
    if (resolved == NULL) {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "INTERNAL_ERROR", 500, "resolver", "allocation_failed");
    }

    const ebin_catalog_entry_t *catalog_entries = NULL;
    size_t catalog_count = 0;
    get_active_catalog(&catalog_entries, &catalog_count);

    cJSON *component_item = NULL;
    cJSON_ArrayForEach(component_item, components)
    {
        if (!cJSON_IsString(component_item) || component_item->valuestring == NULL || component_item->valuestring[0] == '\0') {
            cJSON_Delete(resolved);
            cJSON_Delete(json);
            return send_ebin_validation_error(req, "BAD_REQUEST", 400, "components", "all component entries must be non-empty strings");
        }

        const char *component = component_item->valuestring;
        if (cJSON_IsString(force_ambiguous_component) && force_ambiguous_component->valuestring != NULL &&
            strcmp(force_ambiguous_component->valuestring, component) == 0) {
            cJSON_Delete(resolved);
            cJSON_Delete(json);
            return send_ebin_validation_error(req, "EBIN_INVALID", 409, "components", "resolver_ambiguous_selection");
        }

        const ebin_catalog_entry_t *selected = NULL;
        const char *pinned_version = NULL;
        if (strcmp(policy, "pinned") == 0) {
            cJSON *pin = cJSON_GetObjectItemCaseSensitive(pinned_versions, component);
            if (!cJSON_IsString(pin) || pin->valuestring == NULL || pin->valuestring[0] == '\0') {
                cJSON_Delete(resolved);
                cJSON_Delete(json);
                return send_ebin_validation_error(req, "BAD_REQUEST", 400, "pinned_versions", "missing pinned version for requested component");
            }
            pinned_version = pin->valuestring;
        }

        for (size_t i = 0; i < catalog_count; i++) {
            const ebin_catalog_entry_t *candidate = &catalog_entries[i];
            if (strcmp(candidate->machine, machine->valuestring) != 0 || strcmp(candidate->component, component) != 0) {
                continue;
            }
            if (strcmp(policy, "pinned") == 0) {
                if (strcmp(candidate->version, pinned_version) == 0) {
                    selected = candidate;
                    break;
                }
                continue;
            }

            if (selected == NULL || compare_semver(candidate->version, selected->version) > 0) {
                selected = candidate;
            }
        }

        if (selected == NULL) {
            cJSON_Delete(resolved);
            cJSON_Delete(json);
            return send_ebin_validation_error(req, "EBIN_NOT_FOUND", 404, "components", "resolver_component_not_found");
        }

        if (append_resolved_entry(resolved, selected, component, policy) != ESP_OK) {
            cJSON_Delete(resolved);
            cJSON_Delete(json);
            return send_ebin_validation_error(req, "INTERNAL_ERROR", 500, "resolver", "allocation_failed");
        }
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "data", data);
    cJSON_AddStringToObject(data, "machine", machine->valuestring);
    cJSON_AddStringToObject(data, "version_policy", policy);
    cJSON_AddItemToObject(data, "resolved", resolved);
    cJSON_AddNumberToObject(data, "resolved_count", (double)cJSON_GetArraySize(resolved));

    cJSON_Delete(json);
    esp_err_t out = send_json_object(req, root, 200);
    cJSON_Delete(root);
    return out;
}

static esp_err_t handle_ebins_unload(httpd_req_t *req)
{
    cJSON *json = NULL;
    if (!parse_json_request(req, &json)) {
        return ESP_OK;
    }

    cJSON *module_id = cJSON_GetObjectItemCaseSensitive(json, "module_id");
    if (!cJSON_IsString(module_id) || module_id->valuestring == NULL || module_id->valuestring[0] == '\0') {
        module_id = cJSON_GetObjectItemCaseSensitive(json, "name");
    }
    cJSON *simulate_fail_stage = cJSON_GetObjectItemCaseSensitive(json, "simulate_fail_stage");

    if (!cJSON_IsString(module_id) || module_id->valuestring == NULL || module_id->valuestring[0] == '\0') {
        cJSON_Delete(json);
        return send_ebin_validation_error(req, "BAD_REQUEST", 400, "module_id", "required non-empty string");
    }

    if (!s_ebin_runtime.has_active_module) {
        runtime_mark_failure("pause", "unload_pause_failed_no_active_module");
        cJSON_Delete(json);
        return send_ebin_orchestration_error(req,
                                             "EBIN_NOT_FOUND",
                                             404,
                                             "pause",
                                             "unload_pause_failed_no_active_module",
                                             module_id->valuestring);
    }
    if (strcmp(s_ebin_runtime.active_module_id, module_id->valuestring) != 0) {
        runtime_mark_failure("pause", "unload_pause_failed_module_not_active");
        cJSON_Delete(json);
        return send_ebin_orchestration_error(req,
                                             "EBIN_INVALID",
                                             409,
                                             "pause",
                                             "unload_pause_failed_module_not_active",
                                             module_id->valuestring);
    }

    runtime_mark_stage("unloading", "pause");
    if (cJSON_IsString(simulate_fail_stage) && simulate_fail_stage->valuestring != NULL &&
        strcmp(simulate_fail_stage->valuestring, "pause") == 0) {
        runtime_mark_failure("pause", "unload_pause_failed_simulated");
        cJSON_Delete(json);
        return send_ebin_orchestration_error(req,
                                             "EBIN_INVALID",
                                             409,
                                             "pause",
                                             "unload_pause_failed_simulated",
                                             module_id->valuestring);
    }

    runtime_mark_stage("unloading", "drain");
    if (cJSON_IsString(simulate_fail_stage) && simulate_fail_stage->valuestring != NULL &&
        strcmp(simulate_fail_stage->valuestring, "drain") == 0) {
        runtime_mark_failure("drain", "unload_drain_failed_simulated");
        cJSON_Delete(json);
        return send_ebin_orchestration_error(req,
                                             "EBIN_INVALID",
                                             409,
                                             "drain",
                                             "unload_drain_failed_simulated",
                                             module_id->valuestring);
    }

    runtime_mark_stage("unloading", "deinit");
    if (cJSON_IsString(simulate_fail_stage) && simulate_fail_stage->valuestring != NULL &&
        strcmp(simulate_fail_stage->valuestring, "deinit") == 0) {
        runtime_mark_failure("deinit", "unload_deinit_failed_simulated");
        cJSON_Delete(json);
        return send_ebin_orchestration_error(req,
                                             "EBIN_INVALID",
                                             409,
                                             "deinit",
                                             "unload_deinit_failed_simulated",
                                             module_id->valuestring);
    }

    runtime_mark_stage("unloading", "release");
    if (cJSON_IsString(simulate_fail_stage) && simulate_fail_stage->valuestring != NULL &&
        strcmp(simulate_fail_stage->valuestring, "release") == 0) {
        runtime_mark_failure("release", "unload_release_failed_simulated");
        cJSON_Delete(json);
        return send_ebin_orchestration_error(req,
                                             "EBIN_INVALID",
                                             409,
                                             "release",
                                             "unload_release_failed_simulated",
                                             module_id->valuestring);
    }

    s_ebin_runtime.has_active_module = false;
    s_ebin_runtime.active_module_id[0] = '\0';
    s_ebin_runtime.active_abi_version[0] = '\0';
    runtime_mark_stage("idle", "release");

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "ok", true);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "data", data);
    cJSON_AddStringToObject(data, "module_id", module_id->valuestring);
    cJSON_AddStringToObject(data, "status", "unloaded");
    cJSON *transitions = cJSON_CreateArray();
    cJSON_AddItemToObject(data, "transitions", transitions);
    cJSON_AddItemToArray(transitions, cJSON_CreateString("pause"));
    cJSON_AddItemToArray(transitions, cJSON_CreateString("drain"));
    cJSON_AddItemToArray(transitions, cJSON_CreateString("deinit"));
    cJSON_AddItemToArray(transitions, cJSON_CreateString("release"));
    cJSON_AddStringToObject(data, "runtime_state", s_ebin_runtime.state);
    cJSON_AddNumberToObject(data, "transition_seq", (double)s_ebin_runtime.transition_seq);
    cJSON *fault = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "fault_telemetry", fault);
    cJSON_AddStringToObject(fault, "fault_code", s_ebin_runtime.last_fault_code);
    cJSON_AddStringToObject(fault, "fault_stage", s_ebin_runtime.last_stage);
    cJSON_AddStringToObject(fault, "fault_reason", s_ebin_runtime.last_error_reason);
    cJSON_AddStringToObject(fault, "recovery_action", s_ebin_runtime.last_recovery_action);
    cJSON_AddStringToObject(fault, "recovered_module_id", s_ebin_runtime.last_recovered_module_id);
    cJSON_AddStringToObject(fault,
                            "last_known_good_module_id",
                            s_ebin_runtime.has_last_known_good ? s_ebin_runtime.last_known_good_module_id : "");
    cJSON_AddNumberToObject(fault, "fault_at_us", (double)s_ebin_runtime.last_fault_at_us);
    cJSON_AddNumberToObject(data, "unloaded_at_us", (double)s_ebin_runtime.updated_at_us);

    cJSON_Delete(json);
    esp_err_t out = send_json_object(req, root, 200);
    cJSON_Delete(root);
    return out;
}

void esptari_web_catalog_sync_register_routes(httpd_handle_t server_handle)
{
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/catalog-sync/jobs/run", HTTP_POST, handle_catalog_sync_run, "files:write"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/catalog-sync/jobs", HTTP_GET, handle_catalog_sync_jobs, "files:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/catalog-sync/jobs/*", HTTP_GET, handle_catalog_sync_job_by_id, "files:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/catalog-sync/schedules", HTTP_POST, handle_catalog_sync_create_schedule, "files:write"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/catalog-sync/schedules", HTTP_GET, handle_catalog_sync_list_schedules, "files:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/catalog-sync/schedules/*", HTTP_GET, handle_catalog_sync_get_schedule, "files:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/catalog-sync/schedules/*", HTTP_PATCH, handle_catalog_sync_patch_schedule, "files:write"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/catalog-sync/schedules/*", HTTP_DELETE, handle_catalog_sync_delete_schedule, "files:write"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/catalog-sync/recovery", HTTP_GET, handle_catalog_sync_recovery_report, "files:read"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/ebins/catalog", HTTP_GET, handle_ebins_catalog, "ebin:manage"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/ebins/rescan", HTTP_POST, handle_ebins_rescan, "ebin:manage"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/ebins/resolve", HTTP_POST, handle_ebins_resolve, "ebin:manage"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/ebins/validate", HTTP_POST, handle_ebins_validate, "ebin:manage"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/ebins/load", HTTP_POST, handle_ebins_load, "ebin:manage"));
    ESP_ERROR_CHECK(esptari_web_auth_register_protected_route(server_handle, "/api/v2/ebins/unload", HTTP_POST, handle_ebins_unload, "ebin:manage"));

    ESP_LOGI(TAG, "Registered catalog sync and ebin routes");
}
