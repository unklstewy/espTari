#include "esptari_core.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "machine.h"

static SemaphoreHandle_t core_lock;
static esptari_session_status_t session_status;
static bool snapshot_valid;
static char snapshot_id[128];
static char last_failed_rule_id[16];

typedef struct {
    char module_id[24];
    char version[16];
} compat_module_t;

typedef struct {
    uint32_t schema_version;
    char profile[32];
    char engine_abi[16];
    compat_module_t modules[8];
    size_t module_count;
} snapshot_compat_t;

static snapshot_compat_t snapshot_compat;

#define SNAPSHOT_META_PREFIX "/spiffs/snapshot_meta_"

static uint32_t fnv1a_hash(const char *text)
{
    uint32_t hash = 2166136261u;
    for (const unsigned char *cursor = (const unsigned char *)text; *cursor != '\0'; cursor++) {
        hash ^= *cursor;
        hash *= 16777619u;
    }
    return hash;
}

static bool snapshot_id_has_embedded_metadata(const char *snapshot_name)
{
    return snapshot_name != NULL && strchr(snapshot_name, '|') != NULL;
}

static void snapshot_meta_path(const char *snapshot_name, char *out_path, size_t out_path_size)
{
    uint32_t hash = fnv1a_hash(snapshot_name);
    snprintf(out_path, out_path_size, SNAPSHOT_META_PREFIX "%08" PRIx32 ".meta", hash);
}

static void trim_newline(char *line)
{
    if (line == NULL) {
        return;
    }
    size_t length = strlen(line);
    while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
        line[length - 1] = '\0';
        length--;
    }
}

static esp_err_t save_snapshot_compat_record(const char *snapshot_name, const snapshot_compat_t *compat)
{
    if (snapshot_name == NULL || compat == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char path[96];
    snapshot_meta_path(snapshot_name, path, sizeof(path));

    FILE *file = fopen(path, "w");
    if (file == NULL) {
        return ESP_FAIL;
    }

    fprintf(file, "snapshot_id=%s\n", snapshot_name);
    fprintf(file, "schema=%" PRIu32 "\n", compat->schema_version);
    fprintf(file, "profile=%s\n", compat->profile);
    fprintf(file, "engine_abi=%s\n", compat->engine_abi);
    fprintf(file, "module_count=%u\n", (unsigned)compat->module_count);
    for (size_t index = 0; index < compat->module_count; index++) {
        fprintf(file,
                "module=%s:%s\n",
                compat->modules[index].module_id,
                compat->modules[index].version);
    }

    fclose(file);
    return ESP_OK;
}

static esp_err_t load_snapshot_compat_record(const char *snapshot_name, snapshot_compat_t *out_compat)
{
    if (snapshot_name == NULL || out_compat == NULL || snapshot_name[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    char path[96];
    snapshot_meta_path(snapshot_name, path, sizeof(path));

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    snapshot_compat_t loaded;
    memset(&loaded, 0, sizeof(loaded));

    bool id_match = false;
    bool have_schema = false;
    bool have_profile = false;
    bool have_engine_abi = false;

    char line[192];
    while (fgets(line, sizeof(line), file) != NULL) {
        trim_newline(line);

        if (strncmp(line, "snapshot_id=", 12) == 0) {
            id_match = strcmp(line + 12, snapshot_name) == 0;
            continue;
        }
        if (strncmp(line, "schema=", 7) == 0) {
            loaded.schema_version = (uint32_t)strtoul(line + 7, NULL, 10);
            have_schema = loaded.schema_version > 0;
            continue;
        }
        if (strncmp(line, "profile=", 8) == 0) {
            strlcpy(loaded.profile, line + 8, sizeof(loaded.profile));
            have_profile = loaded.profile[0] != '\0';
            continue;
        }
        if (strncmp(line, "engine_abi=", 11) == 0) {
            strlcpy(loaded.engine_abi, line + 11, sizeof(loaded.engine_abi));
            have_engine_abi = loaded.engine_abi[0] != '\0';
            continue;
        }
        if (strncmp(line, "module=", 7) == 0) {
            if (loaded.module_count >= 8) {
                fclose(file);
                return ESP_FAIL;
            }
            char module_line[96];
            strlcpy(module_line, line + 7, sizeof(module_line));
            char *separator = strchr(module_line, ':');
            if (separator == NULL) {
                fclose(file);
                return ESP_FAIL;
            }
            *separator = '\0';
            const char *module_id = module_line;
            const char *version = separator + 1;
            if (module_id[0] == '\0' || version[0] == '\0') {
                fclose(file);
                return ESP_FAIL;
            }
            strlcpy(loaded.modules[loaded.module_count].module_id,
                    module_id,
                    sizeof(loaded.modules[loaded.module_count].module_id));
            strlcpy(loaded.modules[loaded.module_count].version,
                    version,
                    sizeof(loaded.modules[loaded.module_count].version));
            loaded.module_count++;
        }
    }

    fclose(file);

    if (!id_match || !have_schema || !have_profile || !have_engine_abi || loaded.module_count == 0) {
        return ESP_FAIL;
    }

    *out_compat = loaded;
    return ESP_OK;
}

static void compat_reset_expected(snapshot_compat_t *compat)
{
    memset(compat, 0, sizeof(*compat));
    compat->schema_version = 1;
    strlcpy(compat->profile, "st_520_pal", sizeof(compat->profile));
    strlcpy(compat->engine_abi, "2.0.0", sizeof(compat->engine_abi));

    compat->module_count = 4;
    strlcpy(compat->modules[0].module_id, "cpu", sizeof(compat->modules[0].module_id));
    strlcpy(compat->modules[0].version, "2.0.0", sizeof(compat->modules[0].version));
    strlcpy(compat->modules[1].module_id, "video", sizeof(compat->modules[1].module_id));
    strlcpy(compat->modules[1].version, "2.0.0", sizeof(compat->modules[1].version));
    strlcpy(compat->modules[2].module_id, "io", sizeof(compat->modules[2].module_id));
    strlcpy(compat->modules[2].version, "2.0.0", sizeof(compat->modules[2].version));
    strlcpy(compat->modules[3].module_id, "storage", sizeof(compat->modules[3].module_id));
    strlcpy(compat->modules[3].version, "2.0.0", sizeof(compat->modules[3].version));
}

static void compat_set_last_rule(const char *rule_id)
{
    strlcpy(last_failed_rule_id, rule_id == NULL ? "" : rule_id, sizeof(last_failed_rule_id));
}

static bool parse_modules(char *modules_text, snapshot_compat_t *compat)
{
    compat->module_count = 0;
    char *module_ctx = NULL;
    for (char *module = strtok_r(modules_text, ",", &module_ctx);
         module != NULL;
         module = strtok_r(NULL, ",", &module_ctx)) {
        if (compat->module_count >= 8) {
            return false;
        }

        char *separator = strchr(module, ':');
        if (separator == NULL) {
            return false;
        }
        *separator = '\0';
        const char *module_id = module;
        const char *version = separator + 1;
        if (module_id[0] == '\0' || version[0] == '\0') {
            return false;
        }

        strlcpy(compat->modules[compat->module_count].module_id,
                module_id,
                sizeof(compat->modules[compat->module_count].module_id));
        strlcpy(compat->modules[compat->module_count].version,
                version,
                sizeof(compat->modules[compat->module_count].version));
        compat->module_count++;
    }
    return true;
}

static bool parse_snapshot_compat(const char *snapshot_name, snapshot_compat_t *out_compat)
{
    if (snapshot_name == NULL || snapshot_name[0] == '\0') {
        return false;
    }

    compat_reset_expected(out_compat);

    char local[256];
    strlcpy(local, snapshot_name, sizeof(local));

    char *ctx = NULL;
    bool first = true;
    for (char *token = strtok_r(local, "|", &ctx); token != NULL; token = strtok_r(NULL, "|", &ctx)) {
        if (first) {
            first = false;
            continue;
        }

        char *equal = strchr(token, '=');
        if (equal == NULL) {
            continue;
        }
        *equal = '\0';
        const char *key = token;
        char *value = equal + 1;

        if (strcmp(key, "schema") == 0) {
            char *end = NULL;
            unsigned long parsed = strtoul(value, &end, 10);
            if (end == value || *end != '\0' || parsed == 0) {
                return false;
            }
            out_compat->schema_version = (uint32_t)parsed;
        } else if (strcmp(key, "profile") == 0) {
            if (value[0] == '\0') {
                return false;
            }
            strlcpy(out_compat->profile, value, sizeof(out_compat->profile));
        } else if (strcmp(key, "engine") == 0) {
            if (value[0] == '\0') {
                return false;
            }
            strlcpy(out_compat->engine_abi, value, sizeof(out_compat->engine_abi));
        } else if (strcmp(key, "modules") == 0) {
            if (value[0] == '\0' || !parse_modules(value, out_compat)) {
                return false;
            }
        }
    }

    return true;
}

static const char *find_module_version(const snapshot_compat_t *compat, const char *module_id)
{
    for (size_t index = 0; index < compat->module_count; index++) {
        if (strcmp(compat->modules[index].module_id, module_id) == 0) {
            return compat->modules[index].version;
        }
    }
    return NULL;
}

static bool evaluate_restore_compatibility_locked(const snapshot_compat_t *candidate)
{
    snapshot_compat_t runtime_expected;
    compat_reset_expected(&runtime_expected);

    compat_set_last_rule("");

    if (candidate->schema_version != runtime_expected.schema_version) {
        compat_set_last_rule("RCOMP-01");
        return false;
    }

    if (strcmp(candidate->profile, runtime_expected.profile) != 0) {
        compat_set_last_rule("RCOMP-02");
        return false;
    }

    if (strcmp(candidate->engine_abi, runtime_expected.engine_abi) != 0) {
        compat_set_last_rule("RCOMP-03");
        return false;
    }

    for (size_t index = 0; index < runtime_expected.module_count; index++) {
        const char *version = find_module_version(candidate, runtime_expected.modules[index].module_id);
        if (version == NULL || strcmp(version, runtime_expected.modules[index].version) != 0) {
            compat_set_last_rule("RCOMP-04");
            return false;
        }
    }

    return true;
}

static void update_state(esptari_session_state_t next)
{
    session_status.state = next;
    session_status.transition_count++;
    session_status.last_transition_us = (uint64_t)esp_timer_get_time();
}

void esptari_core_init(void)
{
    if (core_lock == NULL) {
        core_lock = xSemaphoreCreateMutex();
    }
    if (core_lock != NULL) {
        xSemaphoreTake(core_lock, portMAX_DELAY);
        memset(&session_status, 0, sizeof(session_status));
        snapshot_valid = false;
        memset(snapshot_id, 0, sizeof(snapshot_id));
        compat_set_last_rule("");
        compat_reset_expected(&snapshot_compat);
        session_status.state = ESPTARI_SESSION_STOPPED;
        session_status.last_transition_us = (uint64_t)esp_timer_get_time();
        xSemaphoreGive(core_lock);
    }
}

esp_err_t esptari_core_start(void)
{
    if (core_lock == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!machine_is_loaded()) {
        return ESP_ERR_NOT_FOUND;
    }

    xSemaphoreTake(core_lock, portMAX_DELAY);
    if (session_status.state != ESPTARI_SESSION_STOPPED && session_status.state != ESPTARI_SESSION_PAUSED) {
        xSemaphoreGive(core_lock);
        return ESP_ERR_INVALID_STATE;
    }
    update_state(ESPTARI_SESSION_RUNNING);
    xSemaphoreGive(core_lock);
    return ESP_OK;
}

esp_err_t esptari_core_pause(void)
{
    if (core_lock == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(core_lock, portMAX_DELAY);
    if (session_status.state != ESPTARI_SESSION_RUNNING) {
        xSemaphoreGive(core_lock);
        return ESP_ERR_INVALID_STATE;
    }
    update_state(ESPTARI_SESSION_PAUSED);
    xSemaphoreGive(core_lock);
    return ESP_OK;
}

esp_err_t esptari_core_resume(void)
{
    if (core_lock == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(core_lock, portMAX_DELAY);
    if (session_status.state != ESPTARI_SESSION_PAUSED && session_status.state != ESPTARI_SESSION_SUSPENDED) {
        xSemaphoreGive(core_lock);
        return ESP_ERR_INVALID_STATE;
    }
    update_state(ESPTARI_SESSION_RUNNING);
    xSemaphoreGive(core_lock);
    return ESP_OK;
}

esp_err_t esptari_core_stop(void)
{
    if (core_lock == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(core_lock, portMAX_DELAY);
    if (session_status.state == ESPTARI_SESSION_STOPPED) {
        xSemaphoreGive(core_lock);
        return ESP_ERR_INVALID_STATE;
    }
    update_state(ESPTARI_SESSION_STOPPED);
    xSemaphoreGive(core_lock);
    return ESP_OK;
}

esp_err_t esptari_core_reset(void)
{
    if (core_lock == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(core_lock, portMAX_DELAY);
    if (session_status.state != ESPTARI_SESSION_RUNNING && session_status.state != ESPTARI_SESSION_PAUSED) {
        xSemaphoreGive(core_lock);
        return ESP_ERR_INVALID_STATE;
    }

    update_state(ESPTARI_SESSION_RUNNING);
    xSemaphoreGive(core_lock);
    return ESP_OK;
}

esp_err_t esptari_core_suspend_save(const char *new_snapshot_id)
{
    if (core_lock == NULL || new_snapshot_id == NULL || new_snapshot_id[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(core_lock, portMAX_DELAY);
    if (session_status.state != ESPTARI_SESSION_RUNNING) {
        xSemaphoreGive(core_lock);
        return ESP_ERR_INVALID_STATE;
    }

    snapshot_compat_t parsed;
    bool has_metadata_tokens = snapshot_id_has_embedded_metadata(new_snapshot_id);
    if (has_metadata_tokens) {
        if (!parse_snapshot_compat(new_snapshot_id, &parsed)) {
            xSemaphoreGive(core_lock);
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        compat_reset_expected(&parsed);
    }

    esp_err_t persist_err = save_snapshot_compat_record(new_snapshot_id, &parsed);
    if (persist_err != ESP_OK) {
        xSemaphoreGive(core_lock);
        return ESP_FAIL;
    }

    strlcpy(snapshot_id, new_snapshot_id, sizeof(snapshot_id));
    snapshot_compat = parsed;
    snapshot_valid = true;
    compat_set_last_rule("");
    update_state(ESPTARI_SESSION_SUSPENDED);
    xSemaphoreGive(core_lock);
    return ESP_OK;
}

esp_err_t esptari_core_restore_resume(const char *restore_snapshot_id, bool resume_running)
{
    if (core_lock == NULL || restore_snapshot_id == NULL || restore_snapshot_id[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(core_lock, portMAX_DELAY);
    if (session_status.state != ESPTARI_SESSION_SUSPENDED) {
        xSemaphoreGive(core_lock);
        return ESP_ERR_INVALID_STATE;
    }

    if (!snapshot_valid || strcmp(snapshot_id, restore_snapshot_id) != 0) {
        xSemaphoreGive(core_lock);
        return ESP_ERR_NOT_FOUND;
    }

    snapshot_compat_t persisted;
    esp_err_t load_err = load_snapshot_compat_record(restore_snapshot_id, &persisted);
    if (load_err == ESP_ERR_NOT_FOUND) {
        xSemaphoreGive(core_lock);
        return ESP_ERR_NOT_FOUND;
    }
    if (load_err != ESP_OK) {
        xSemaphoreGive(core_lock);
        return ESP_FAIL;
    }

    snapshot_compat = persisted;

    if (!evaluate_restore_compatibility_locked(&snapshot_compat)) {
        xSemaphoreGive(core_lock);
        return ESP_ERR_INVALID_RESPONSE;
    }

    update_state(resume_running ? ESPTARI_SESSION_RUNNING : ESPTARI_SESSION_PAUSED);
    xSemaphoreGive(core_lock);
    return ESP_OK;
}

esp_err_t esptari_core_validate_restore_compatibility(const char *restore_snapshot_id,
                                                      bool strict,
                                                      bool *out_compatible)
{
    if (core_lock == NULL || out_compatible == NULL || restore_snapshot_id == NULL || restore_snapshot_id[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(core_lock, portMAX_DELAY);
    snapshot_compat_t persisted;
    esp_err_t load_err = load_snapshot_compat_record(restore_snapshot_id, &persisted);
    if (load_err == ESP_ERR_NOT_FOUND) {
        xSemaphoreGive(core_lock);
        return ESP_ERR_NOT_FOUND;
    }
    if (load_err != ESP_OK) {
        xSemaphoreGive(core_lock);
        return ESP_FAIL;
    }

    bool compatible = evaluate_restore_compatibility_locked(&persisted);
    *out_compatible = compatible;
    xSemaphoreGive(core_lock);

    if (!compatible && strict) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

const char *esptari_core_get_last_failed_compat_rule(void)
{
    return last_failed_rule_id;
}

void esptari_core_get_status(esptari_session_status_t *out_status)
{
    if (out_status == NULL || core_lock == NULL) {
        return;
    }

    xSemaphoreTake(core_lock, portMAX_DELAY);
    *out_status = session_status;
    xSemaphoreGive(core_lock);
}

const char *esptari_core_state_to_string(esptari_session_state_t state)
{
    switch (state) {
    case ESPTARI_SESSION_STOPPED:
        return "stopped";
    case ESPTARI_SESSION_RUNNING:
        return "running";
    case ESPTARI_SESSION_PAUSED:
        return "paused";
    case ESPTARI_SESSION_SUSPENDED:
        return "suspended";
    case ESPTARI_SESSION_FAULTED:
        return "faulted";
    default:
        return "unknown";
    }
}
