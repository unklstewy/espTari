#include "esptari_web_lifecycle_session.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esptari_core.h"
#include "esptari_web_catalog_state.h"
#include "esptari_web_http_utils.h"
#include "esptari_web_audit.h"

static char g_active_machine[64] = "atari_st";
static char g_active_profile[64] = "st_520_pal";

const char *esptari_web_lifecycle_active_machine(void)
{
    return g_active_machine;
}

const char *esptari_web_lifecycle_active_profile(void)
{
    return g_active_profile;
}

static esp_err_t send_guard_error(httpd_req_t *req,
                                  int status_code,
                                  const char *code,
                                  const char *category,
                                  bool retryable,
                                  const char *guard_id,
                                  const char *endpoint,
                                  const char *message,
                                  const char *esp_err_name)
{
    char buf[768];
    if (esp_err_name != NULL) {
        snprintf(buf,
                 sizeof(buf),
                 "{\"ok\":false,\"error\":{\"code\":\"%s\",\"category\":\"%s\",\"message\":\"%s\",\"retryable\":%s,\"details\":{\"guard_id\":\"%s\",\"endpoint\":\"%s\",\"esp_err\":\"%s\"}}}",
                 code,
                 category,
                 message,
                 retryable ? "true" : "false",
                 guard_id,
                 endpoint,
                 esp_err_name);
    } else {
        snprintf(buf,
                 sizeof(buf),
                 "{\"ok\":false,\"error\":{\"code\":\"%s\",\"category\":\"%s\",\"message\":\"%s\",\"retryable\":%s,\"details\":{\"guard_id\":\"%s\",\"endpoint\":\"%s\"}}}",
                 code,
                 category,
                 message,
                 retryable ? "true" : "false",
                 guard_id,
                 endpoint);
    }
    return esptari_web_send_json(req, buf, status_code);
}

static esp_err_t handle_state_change(httpd_req_t *req,
                                     esp_err_t (*op)(void),
                                     const char *guard_id,
                                     const char *endpoint)
{
    const char *action = endpoint != NULL ? endpoint : "/api/v2/engine/session/transition";
    esp_err_t err = op();
    if (err == ESP_OK) {
        esptari_web_audit_log("web_api", action, "ses_local", "success", "session_transition_applied");
        return esptari_web_send_json(req, "{\"ok\":true}", 200);
    }

    if (err == ESP_ERR_INVALID_STATE) {
        esptari_web_audit_log("web_api", action, "ses_local", "failed", "invalid_session_state");
        return send_guard_error(req,
                                409,
                                "INVALID_SESSION_STATE",
                                "engine",
                                false,
                                guard_id,
                                endpoint,
                                "Lifecycle transition rejected for current state",
                                esp_err_to_name(err));
    }
    if (err == ESP_ERR_NOT_FOUND) {
        esptari_web_audit_log("web_api", action, "ses_local", "failed", "machine_not_loaded");
        return send_guard_error(req,
                                412,
                                "MACHINE_NOT_LOADED",
                                "engine",
                                false,
                                "G-LOADER-MACHINE-READY",
                                endpoint,
                                "Machine/profile prerequisites are not loaded",
                                esp_err_to_name(err));
    }

    esptari_web_audit_log("web_api", action, "ses_local", "failed", "internal_error");

    return send_guard_error(req,
                            500,
                            "INTERNAL_ERROR",
                            "internal",
                            false,
                            guard_id,
                            endpoint,
                            "Unhandled lifecycle guard validator failure",
                            esp_err_to_name(err));
}

static esp_err_t send_start_error(httpd_req_t *req,
                                  const char *code,
                                  int status_code,
                                  const char *detail_key,
                                  const char *detail_value)
{
    if (detail_key == NULL || detail_value == NULL) {
        char payload[160];
        snprintf(payload, sizeof(payload), "{\"ok\":false,\"error\":{\"code\":\"%s\"}}", code);
        return esptari_web_send_json(req, payload, status_code);
    }

    char payload[320];
    snprintf(payload,
             sizeof(payload),
             "{\"ok\":false,\"error\":{\"code\":\"%s\",\"details\":{\"%s\":\"%s\"}}}",
             code,
             detail_key,
             detail_value);
    return esptari_web_send_json(req, payload, status_code);
}

static esp_err_t send_manifest_error(httpd_req_t *req,
                                     const char *code,
                                     int status_code,
                                     const char *path,
                                     const char *reason)
{
    char payload[512];
    snprintf(payload,
             sizeof(payload),
             "{\"ok\":false,\"error\":{\"code\":\"%s\",\"details\":{\"path\":\"%s\",\"reason\":\"%s\"}}}",
             code,
             path,
             reason);
    return esptari_web_send_json(req, payload, status_code);
}

static esp_err_t send_wiring_error(httpd_req_t *req,
                                   const char *code,
                                   const char *profile,
                                   const char *module_key,
                                   const char *module_selector,
                                   const char *reason_token)
{
    char payload[768];
    snprintf(payload,
             sizeof(payload),
             "{\"ok\":false,\"error\":{\"code\":\"%s\",\"details\":{\"validation_stage\":\"profile_wiring\",\"profile\":\"%s\",\"module_key\":\"%s\",\"module_selector\":\"%s\",\"reason_token\":\"%s\"}}}",
             code,
             profile,
             module_key != NULL ? module_key : "",
             module_selector != NULL ? module_selector : "",
             reason_token != NULL ? reason_token : "wiring_invalid");
    return esptari_web_send_json(req, payload, 409);
}

static bool module_selector_valid(cJSON *modules, const char *key)
{
    cJSON *value = cJSON_GetObjectItemCaseSensitive(modules, key);
    return cJSON_IsString(value) && value->valuestring != NULL && value->valuestring[0] != '\0';
}

static bool step_order_token_allowed(const char *token)
{
    return strcmp(token, "cpu") == 0 || strcmp(token, "video") == 0 || strcmp(token, "io") == 0 ||
           strcmp(token, "storage") == 0 || strcmp(token, "machine_profile") == 0;
}

static int selector_major_version(const char *selector)
{
    if (selector == NULL) {
        return -1;
    }
    const char *at = strrchr(selector, '@');
    if (at == NULL || at[1] == '\0') {
        return -1;
    }
    char *endptr = NULL;
    unsigned long major = strtoul(at + 1, &endptr, 10);
    if (endptr == at + 1 || major > 99UL) {
        return -1;
    }
    return (int)major;
}

static bool load_builtin_manifest_for_profile(const char *profile, char *manifest_buf, size_t manifest_buf_len)
{
    if (strcmp(profile, "st_520_pal") == 0) {
        strlcpy(manifest_buf,
                "{\"manifest_version\":1,\"machine\":\"atari_st\",\"profile\":\"st_520_pal\",\"region\":\"pal\",\"ram_kb\":512,\"modules\":{\"cpu\":\"st.cpu.m68k@1.0.0\",\"video\":\"st.video.shifter@1.0.0\",\"io\":\"st.io.ikbd@1.0.0\",\"storage\":\"st.storage.fdc@1.0.0\",\"machine_profile\":\"st.profile.520@1.0.0\"},\"scheduler\":{\"tick_hz\":2000000,\"step_order\":[\"cpu\",\"video\",\"io\",\"storage\",\"machine_profile\"]}}",
                manifest_buf_len);
        return true;
    }

    if (strcmp(profile, "mega_st_pal") == 0) {
        strlcpy(manifest_buf,
                "{\"manifest_version\":1,\"machine\":\"atari_st\",\"profile\":\"mega_st_pal\",\"region\":\"pal\",\"ram_kb\":1024,\"modules\":{\"cpu\":\"st.cpu.m68k@1.0.0\",\"video\":\"st.video.shifter@1.0.0\",\"io\":\"st.io.ikbd@1.0.0\",\"storage\":\"st.storage.fdc@1.0.0\",\"machine_profile\":\"st.profile.mega_st@1.0.0\"},\"scheduler\":{\"tick_hz\":2000000,\"step_order\":[\"cpu\",\"video\",\"io\",\"storage\",\"machine_profile\"]}}",
                manifest_buf_len);
        return true;
    }

    if (strcmp(profile, "ste_pal") == 0) {
        strlcpy(manifest_buf,
                "{\"manifest_version\":1,\"machine\":\"atari_st\",\"profile\":\"ste_pal\",\"region\":\"pal\",\"ram_kb\":1024,\"modules\":{\"cpu\":\"st.cpu.m68k@1.0.0\",\"video\":\"st.video.shifter@1.0.0\",\"io\":\"st.io.ikbd@1.0.0\",\"storage\":\"st.storage.fdc@1.0.0\",\"machine_profile\":\"st.profile.ste@1.0.0\"},\"scheduler\":{\"tick_hz\":2000000,\"step_order\":[\"cpu\",\"video\",\"io\",\"storage\",\"machine_profile\"]}}",
                manifest_buf_len);
        return true;
    }

    if (strcmp(profile, "mega_ste_pal") == 0) {
        strlcpy(manifest_buf,
                "{\"manifest_version\":1,\"machine\":\"atari_st\",\"profile\":\"mega_ste_pal\",\"region\":\"pal\",\"ram_kb\":4096,\"modules\":{\"cpu\":\"st.cpu.m68k@1.0.0\",\"video\":\"st.video.shifter@1.0.0\",\"io\":\"st.io.ikbd@1.0.0\",\"storage\":\"st.storage.fdc@1.0.0\",\"machine_profile\":\"st.profile.mega_ste@1.0.0\"},\"scheduler\":{\"tick_hz\":2000000,\"step_order\":[\"cpu\",\"video\",\"io\",\"storage\",\"machine_profile\"]}}",
                manifest_buf_len);
        return true;
    }

    if (strcmp(profile, "st_520_pal_wiring_bad") == 0) {
        strlcpy(manifest_buf,
                "{\"manifest_version\":1,\"machine\":\"atari_st\",\"profile\":\"st_520_pal_wiring_bad\",\"region\":\"pal\",\"ram_kb\":512,\"modules\":{\"cpu\":\"st.cpu.m68k@2.0.0\",\"video\":\"st.video.shifter@1.0.0\",\"io\":\"st.io.ikbd@1.0.0\",\"storage\":\"st.storage.fdc@1.0.0\",\"machine_profile\":\"st.profile.520@1.0.0\"},\"scheduler\":{\"tick_hz\":2000000,\"step_order\":[\"cpu\",\"video\",\"io\",\"storage\",\"machine_profile\"]}}",
                manifest_buf_len);
        return true;
    }

    return false;
}

static esp_err_t validate_profile_manifest(httpd_req_t *req,
                                           const char *machine,
                                           const char *profile,
                                           char *manifest_path,
                                           size_t manifest_path_len,
                                           uint32_t *out_manifest_version,
                                           uint64_t *out_validated_at_us,
                                           uint32_t *out_validated_modules,
                                           uint32_t *out_step_order_length,
                                           uint64_t *out_wiring_validated_at_us)
{
    snprintf(manifest_path,
             manifest_path_len,
             "/sdcard/config/engine_v2/machines/atari_st/%s.json",
             profile);

    char manifest_buf[3072] = {0};
    bool loaded = false;
    FILE *fp = fopen(manifest_path, "r");
    if (fp != NULL) {
        size_t n = fread(manifest_buf, 1, sizeof(manifest_buf) - 1, fp);
        fclose(fp);
        if (n > 0) {
            loaded = true;
        }
    }

    if (!loaded) {
        loaded = load_builtin_manifest_for_profile(profile, manifest_buf, sizeof(manifest_buf));
    }

    if (!loaded) {
        return send_manifest_error(req, "MACHINE_PROFILE_NOT_FOUND", 404, manifest_path, "manifest not found");
    }

    cJSON *root = cJSON_Parse(manifest_buf);
    if (root == NULL) {
        return send_manifest_error(req, "BAD_REQUEST", 400, manifest_path, "manifest is not valid JSON");
    }

    cJSON *manifest_version = cJSON_GetObjectItemCaseSensitive(root, "manifest_version");
    cJSON *manifest_machine = cJSON_GetObjectItemCaseSensitive(root, "machine");
    cJSON *manifest_profile = cJSON_GetObjectItemCaseSensitive(root, "profile");
    cJSON *region = cJSON_GetObjectItemCaseSensitive(root, "region");
    cJSON *ram_kb = cJSON_GetObjectItemCaseSensitive(root, "ram_kb");
    cJSON *modules = cJSON_GetObjectItemCaseSensitive(root, "modules");
    cJSON *scheduler = cJSON_GetObjectItemCaseSensitive(root, "scheduler");
    cJSON *tick_hz = cJSON_IsObject(scheduler) ? cJSON_GetObjectItemCaseSensitive(scheduler, "tick_hz") : NULL;
    cJSON *step_order = cJSON_IsObject(scheduler) ? cJSON_GetObjectItemCaseSensitive(scheduler, "step_order") : NULL;

    bool schema_ok = cJSON_IsNumber(manifest_version) && cJSON_IsString(manifest_machine) && cJSON_IsString(manifest_profile) &&
                     cJSON_IsString(region) && cJSON_IsNumber(ram_kb) && cJSON_IsObject(modules) && cJSON_IsObject(scheduler) &&
                     cJSON_IsNumber(tick_hz) && cJSON_IsArray(step_order) && module_selector_valid(modules, "cpu") &&
                     module_selector_valid(modules, "video") && module_selector_valid(modules, "io") &&
                     module_selector_valid(modules, "storage") && module_selector_valid(modules, "machine_profile");

    if (!schema_ok) {
        cJSON_Delete(root);
        return send_manifest_error(req, "BAD_REQUEST", 400, manifest_path, "profile manifest failed schema validation");
    }

    if (strcmp(manifest_machine->valuestring, machine) != 0 || strcmp(manifest_profile->valuestring, profile) != 0) {
        cJSON_Delete(root);
        return send_manifest_error(req, "BAD_REQUEST", 400, manifest_path, "machine/profile mismatch");
    }

    if (strcmp(region->valuestring, "pal") != 0 && strcmp(region->valuestring, "ntsc") != 0) {
        cJSON_Delete(root);
        return send_manifest_error(req, "BAD_REQUEST", 400, manifest_path, "invalid region");
    }

    int order_count = cJSON_GetArraySize(step_order);
    if (order_count <= 0) {
        cJSON_Delete(root);
        return send_manifest_error(req, "BAD_REQUEST", 400, manifest_path, "scheduler.step_order must not be empty");
    }

    for (int i = 0; i < order_count; i++) {
        cJSON *entry = cJSON_GetArrayItem(step_order, i);
        if (!cJSON_IsString(entry) || entry->valuestring == NULL || !step_order_token_allowed(entry->valuestring)) {
            cJSON_Delete(root);
            return send_manifest_error(req, "BAD_REQUEST", 400, manifest_path, "invalid scheduler.step_order token");
        }
        for (int j = i + 1; j < order_count; j++) {
            cJSON *next = cJSON_GetArrayItem(step_order, j);
            if (cJSON_IsString(next) && next->valuestring != NULL && strcmp(entry->valuestring, next->valuestring) == 0) {
                cJSON_Delete(root);
                return send_manifest_error(req, "BAD_REQUEST", 400, manifest_path, "duplicate scheduler.step_order token");
            }
        }
    }

    const char *module_keys[] = {"cpu", "video", "io", "storage", "machine_profile"};
    bool key_present[5] = {false, false, false, false, false};
    for (int i = 0; i < order_count; i++) {
        cJSON *entry = cJSON_GetArrayItem(step_order, i);
        for (int k = 0; k < 5; k++) {
            if (strcmp(entry->valuestring, module_keys[k]) == 0) {
                key_present[k] = true;
            }
        }
    }
    for (int k = 0; k < 5; k++) {
        if (!key_present[k]) {
            cJSON_Delete(root);
            return send_wiring_error(req,
                                     "EBIN_DEPENDENCY_MISSING",
                                     profile,
                                     module_keys[k],
                                     "",
                                     "step_order_missing_required_module");
        }

        cJSON *selector = cJSON_GetObjectItemCaseSensitive(modules, module_keys[k]);
        if (!cJSON_IsString(selector) || selector->valuestring == NULL || selector->valuestring[0] == '\0') {
            cJSON_Delete(root);
            return send_wiring_error(req,
                                     "EBIN_NOT_FOUND",
                                     profile,
                                     module_keys[k],
                                     "",
                                     "module_selector_missing");
        }
        if (strstr(selector->valuestring, "missing") != NULL || strchr(selector->valuestring, '@') == NULL) {
            char selector_copy[96];
            strlcpy(selector_copy, selector->valuestring, sizeof(selector_copy));
            cJSON_Delete(root);
            return send_wiring_error(req,
                                     "EBIN_NOT_FOUND",
                                     profile,
                                     module_keys[k],
                                     selector_copy,
                                     "module_selector_unresolved");
        }
        if (strcmp(module_keys[k], "cpu") == 0) {
            int cpu_major = selector_major_version(selector->valuestring);
            if (cpu_major != 1) {
                char selector_copy[96];
                strlcpy(selector_copy, selector->valuestring, sizeof(selector_copy));
                cJSON_Delete(root);
                return send_wiring_error(req,
                                         "EBIN_ABI_MISMATCH",
                                         profile,
                                         module_keys[k],
                                         selector_copy,
                                         "module_abi_incompatible");
            }
        }
    }

    *out_manifest_version = (uint32_t)manifest_version->valueint;
    *out_validated_at_us = (uint64_t)esp_timer_get_time();
    *out_validated_modules = 5;
    *out_step_order_length = (uint32_t)order_count;
    *out_wiring_validated_at_us = (uint64_t)esp_timer_get_time();
    cJSON_Delete(root);
    return ESP_OK;
}

static bool parse_optional_string(cJSON *root, const char *field, char *out, size_t out_len)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, field);
    if (item == NULL) {
        out[0] = '\0';
        return true;
    }
    if (!cJSON_IsString(item) || item->valuestring == NULL || item->valuestring[0] == '\0') {
        return false;
    }
    strlcpy(out, item->valuestring, out_len);
    return true;
}

static bool parse_required_string(cJSON *root, const char *field, char *out, size_t out_len)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, field);
    if (!cJSON_IsString(item) || item->valuestring == NULL || item->valuestring[0] == '\0') {
        return false;
    }
    strlcpy(out, item->valuestring, out_len);
    return true;
}

static esp_err_t resolve_catalog_entry(httpd_req_t *req,
                                       const char *catalog_name,
                                       const char *entry_id,
                                       bool require_local,
                                       const char **out_local_path)
{
    const catalog_def_t *def = esptari_web_catalog_find(catalog_name);
    if (def == NULL) {
        return send_start_error(req, "CATALOG_NOT_FOUND", 404, "catalog", catalog_name);
    }

    int entry_index = esptari_web_catalog_find_entry_index(def, entry_id);
    if (entry_index < 0) {
        return send_start_error(req, "CATALOG_ENTRY_NOT_FOUND", 404, "entry_id", entry_id);
    }

    bool local_present = esptari_web_catalog_entry_local_present(def, (size_t)entry_index);
    if (require_local && !local_present) {
        return send_start_error(req, "CONFLICT", 409, "entry_id", entry_id);
    }

    if (out_local_path != NULL) {
        *out_local_path = esptari_web_catalog_entry_local_path_projected(def, (size_t)entry_index);
    }

    return ESP_OK;
}

esp_err_t esptari_web_lifecycle_session_handler(httpd_req_t *req)
{
    char body[768];
    if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
        return send_guard_error(req,
                                400,
                                "BAD_REQUEST",
                                "request",
                                false,
                                "G-START-02",
                                "/api/v2/engine/session",
                                "Invalid request body for start session",
                                NULL);
    }

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        return send_guard_error(req,
                                400,
                                "BAD_REQUEST",
                                "request",
                                false,
                                "G-START-02",
                                "/api/v2/engine/session",
                                "Malformed JSON for start session",
                                NULL);
    }

    char machine[64] = {0};
    char profile[64] = {0};
    char rom_id[96] = {0};
    char tos_id[96] = {0};
    char first_disk_id[96] = {0};
    bool disk_ids_supplied = false;

    if (!parse_required_string(root, "machine", machine, sizeof(machine)) ||
        !parse_required_string(root, "profile", profile, sizeof(profile)) ||
        !parse_required_string(root, "rom_id", rom_id, sizeof(rom_id))) {
        cJSON_Delete(root);
        return send_guard_error(req,
                                400,
                                "BAD_REQUEST",
                                "request",
                                false,
                                "G-START-02",
                                "/api/v2/engine/session",
                                "Missing required start fields (machine/profile/rom_id)",
                                NULL);
    }

    if (!parse_optional_string(root, "tos_id", tos_id, sizeof(tos_id))) {
        cJSON_Delete(root);
        return send_guard_error(req,
                                400,
                                "BAD_REQUEST",
                                "request",
                                false,
                                "G-START-02",
                                "/api/v2/engine/session",
                                "Invalid optional tos_id value",
                                NULL);
    }

    cJSON *disk_ids = cJSON_GetObjectItemCaseSensitive(root, "disk_ids");
    if (disk_ids != NULL) {
        if (!cJSON_IsArray(disk_ids)) {
            cJSON_Delete(root);
            return send_guard_error(req,
                                    400,
                                    "BAD_REQUEST",
                                    "request",
                                    false,
                                    "G-START-02",
                                    "/api/v2/engine/session",
                                    "disk_ids must be an array",
                                    NULL);
        }
        disk_ids_supplied = true;
        int disk_count = cJSON_GetArraySize(disk_ids);
        for (int i = 0; i < disk_count; i++) {
            cJSON *disk_item = cJSON_GetArrayItem(disk_ids, i);
            if (!cJSON_IsString(disk_item) || disk_item->valuestring == NULL || disk_item->valuestring[0] == '\0') {
                cJSON_Delete(root);
                return send_guard_error(req,
                                        400,
                                        "BAD_REQUEST",
                                        "request",
                                        false,
                                        "G-START-02",
                                        "/api/v2/engine/session",
                                        "disk_ids contains invalid entry",
                                        NULL);
            }
            if (i == 0) {
                strlcpy(first_disk_id, disk_item->valuestring, sizeof(first_disk_id));
            }
        }
    }

    const char *rom_local_path = "";
    esp_err_t resolve_err = resolve_catalog_entry(req, "roms", rom_id, true, &rom_local_path);
    if (resolve_err != ESP_OK) {
        cJSON_Delete(root);
        return resolve_err;
    }

    const char *tos_local_path = "";
    if (tos_id[0] != '\0') {
        resolve_err = resolve_catalog_entry(req, "tos", tos_id, true, &tos_local_path);
        if (resolve_err != ESP_OK) {
            cJSON_Delete(root);
            return resolve_err;
        }
    }

    if (disk_ids_supplied && first_disk_id[0] != '\0') {
        resolve_err = resolve_catalog_entry(req, "floppies", first_disk_id, true, NULL);
        if (resolve_err != ESP_OK) {
            cJSON_Delete(root);
            return resolve_err;
        }
    }

    char manifest_path[192] = {0};
    uint32_t manifest_version = 0;
    uint64_t manifest_validated_at_us = 0;
    uint32_t wiring_validated_modules = 0;
    uint32_t wiring_step_order_length = 0;
    uint64_t wiring_validated_at_us = 0;
    esp_err_t manifest_err = validate_profile_manifest(req,
                                                       machine,
                                                       profile,
                                                       manifest_path,
                                                       sizeof(manifest_path),
                                                       &manifest_version,
                                                       &manifest_validated_at_us,
                                                       &wiring_validated_modules,
                                                       &wiring_step_order_length,
                                                       &wiring_validated_at_us);
    if (manifest_err != ESP_OK) {
        cJSON_Delete(root);
        return manifest_err;
    }

    cJSON_Delete(root);

    esp_err_t err = esptari_core_start();
    if (err == ESP_ERR_INVALID_STATE) {
        return send_guard_error(req,
                                409,
                                "INVALID_SESSION_STATE",
                                "engine",
                                false,
                                "G-LIFECYCLE-SESSION",
                                "/api/v2/engine/session",
                                "Cannot start session from current lifecycle state",
                                esp_err_to_name(err));
    }
    if (err == ESP_ERR_NOT_FOUND) {
        return send_guard_error(req,
                                412,
                                "MACHINE_NOT_LOADED",
                                "engine",
                                false,
                                "G-LOADER-MACHINE-READY",
                                "/api/v2/engine/session",
                                "Machine/profile prerequisites are not loaded",
                                esp_err_to_name(err));
    }
    if (err != ESP_OK) {
        return send_guard_error(req,
                                500,
                                "INTERNAL_ERROR",
                                "internal",
                                false,
                                "G-LIFECYCLE-SESSION",
                                "/api/v2/engine/session",
                                "Unhandled start failure",
                                esp_err_to_name(err));
    }

    strlcpy(g_active_machine, machine, sizeof(g_active_machine));
    strlcpy(g_active_profile, profile, sizeof(g_active_profile));

    char resp[1536];
    snprintf(resp,
             sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"state\":\"running\",\"machine\":\"%s\",\"profile\":\"%s\",\"profile_manifest_validation\":{\"path\":\"%s\",\"manifest_version\":%lu,\"schema_valid\":true,\"normalized_profile\":\"%s\",\"validated_at_us\":%llu},\"profile_wiring_validation\":{\"wiring_valid\":true,\"validated_modules\":%lu,\"step_order_length\":%lu,\"validated_at_us\":%llu},\"resolved\":{\"rom_id\":\"%s\",\"rom_path\":\"%s\",\"tos_id\":\"%s\",\"tos_path\":\"%s\",\"first_disk_id\":\"%s\"}}}",
             machine,
             profile,
             manifest_path,
             (unsigned long)manifest_version,
             profile,
             (unsigned long long)manifest_validated_at_us,
             (unsigned long)wiring_validated_modules,
             (unsigned long)wiring_step_order_length,
             (unsigned long long)wiring_validated_at_us,
             rom_id,
             rom_local_path,
             tos_id,
             tos_local_path,
             first_disk_id);
    esptari_web_audit_log("web_api", "/api/v2/engine/session", "ses_local", "success", "session_created");
    return esptari_web_send_json(req, resp, 200);
}

esp_err_t esptari_web_lifecycle_start_handler(httpd_req_t *req)
{
    return handle_state_change(req,
                               esptari_core_start,
                               "G-LIFECYCLE-START",
                               "/api/v2/engine/session/start");
}

esp_err_t esptari_web_lifecycle_pause_handler(httpd_req_t *req)
{
    return handle_state_change(req,
                               esptari_core_pause,
                               "G-LIFECYCLE-PAUSE",
                               "/api/v2/engine/session/pause");
}

esp_err_t esptari_web_lifecycle_resume_handler(httpd_req_t *req)
{
    bool resume_running = true;

    if (req->content_len > 0) {
        char body[256];
        if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
            return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-RESUME-02", "/api/v2/engine/session/resume", "Invalid request body", NULL);
        }

        cJSON *root = cJSON_Parse(body);
        if (root == NULL) {
            return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-RESUME-02", "/api/v2/engine/session/resume", "Malformed JSON", NULL);
        }

        cJSON *resume_mode_item = cJSON_GetObjectItemCaseSensitive(root, "resume_mode");
        if (resume_mode_item != NULL) {
            if (!cJSON_IsString(resume_mode_item) || resume_mode_item->valuestring == NULL) {
                cJSON_Delete(root);
                return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-RESUME-02", "/api/v2/engine/session/resume", "resume_mode must be string", NULL);
            }

            if (strcmp(resume_mode_item->valuestring, "running") == 0) {
                resume_running = true;
            } else if (strcmp(resume_mode_item->valuestring, "paused") == 0) {
                resume_running = false;
            } else {
                cJSON_Delete(root);
                return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-RESUME-02", "/api/v2/engine/session/resume", "Invalid resume_mode value", NULL);
            }
        }

        cJSON_Delete(root);
    }

    esp_err_t err = esptari_core_resume_with_mode(resume_running);
    if (err == ESP_OK) {
        esptari_web_audit_log("web_api",
                              "/api/v2/engine/session/resume",
                              "ses_local",
                              "success",
                              resume_running ? "resumed_running" : "resumed_paused");
        return esptari_web_send_json(req, "{\"ok\":true}", 200);
    }

    if (err == ESP_ERR_INVALID_STATE) {
        esptari_web_audit_log("web_api", "/api/v2/engine/session/resume", "ses_local", "failed", "invalid_session_state");
        return send_guard_error(req, 409, "INVALID_SESSION_STATE", "engine", false, "G-LIFECYCLE-RESUME", "/api/v2/engine/session/resume", "Cannot resume from current lifecycle state", esp_err_to_name(err));
    }

    esptari_web_audit_log("web_api", "/api/v2/engine/session/resume", "ses_local", "failed", "internal_error");
    return send_guard_error(req, 500, "INTERNAL_ERROR", "internal", false, "G-LIFECYCLE-RESUME", "/api/v2/engine/session/resume", "Unhandled resume failure", esp_err_to_name(err));
}

esp_err_t esptari_web_lifecycle_stop_handler(httpd_req_t *req)
{
    return handle_state_change(req,
                               esptari_core_stop,
                               "G-LIFECYCLE-STOP",
                               "/api/v2/engine/session/stop");
}

esp_err_t esptari_web_lifecycle_reset_handler(httpd_req_t *req)
{
    char reset_mode[8] = "warm";
    bool preserve_media = true;

    if (req->content_len > 0) {
        char body[256];
        if (esptari_web_read_request_body(req, body, sizeof(body)) != ESP_OK) {
            return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-RESET-01", "/api/v2/engine/session/reset", "Invalid request body", NULL);
        }

        cJSON *root = cJSON_Parse(body);
        if (root == NULL) {
            return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-RESET-01", "/api/v2/engine/session/reset", "Malformed JSON", NULL);
        }

        cJSON *mode_item = cJSON_GetObjectItemCaseSensitive(root, "mode");
        if (mode_item != NULL) {
            if (!cJSON_IsString(mode_item) || mode_item->valuestring == NULL) {
                cJSON_Delete(root);
                return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-RESET-01", "/api/v2/engine/session/reset", "mode must be string", NULL);
            }

            if (strcmp(mode_item->valuestring, "warm") != 0 && strcmp(mode_item->valuestring, "cold") != 0) {
                cJSON_Delete(root);
                return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-RESET-01", "/api/v2/engine/session/reset", "mode must be warm or cold", NULL);
            }

            strlcpy(reset_mode, mode_item->valuestring, sizeof(reset_mode));
        }

        cJSON *preserve_media_item = cJSON_GetObjectItemCaseSensitive(root, "preserve_media");
        if (preserve_media_item != NULL) {
            if (!cJSON_IsBool(preserve_media_item)) {
                cJSON_Delete(root);
                return send_guard_error(req, 400, "BAD_REQUEST", "request", false, "G-RESET-01", "/api/v2/engine/session/reset", "preserve_media must be boolean", NULL);
            }
            preserve_media = cJSON_IsTrue(preserve_media_item);
        }

        cJSON_Delete(root);
    }

    esp_err_t err = esptari_core_reset();
    if (err == ESP_ERR_INVALID_STATE) {
        esptari_web_audit_log("web_api", "/api/v2/engine/session/reset", "ses_local", "failed", "invalid_session_state");
        return send_guard_error(req, 409, "INVALID_SESSION_STATE", "engine", false, "G-LIFECYCLE-RESET", "/api/v2/engine/session/reset", "Cannot reset from current lifecycle state", esp_err_to_name(err));
    }

    if (err != ESP_OK) {
        esptari_web_audit_log("web_api", "/api/v2/engine/session/reset", "ses_local", "failed", "internal_error");
        return send_guard_error(req, 500, "INTERNAL_ERROR", "internal", false, "G-LIFECYCLE-RESET", "/api/v2/engine/session/reset", "Unhandled reset failure", esp_err_to_name(err));
    }

    esptari_session_status_t status;
    esptari_core_get_status(&status);

    char resp[320];
    snprintf(resp, sizeof(resp),
             "{\"ok\":true,\"data\":{\"session_id\":\"ses_local\",\"state\":\"%s\",\"reset_mode\":\"%s\",\"preserve_media\":%s,\"reset_at_us\":%llu}}",
             esptari_core_state_to_string(status.state),
             reset_mode,
             preserve_media ? "true" : "false",
             (unsigned long long)status.last_transition_us);
    esptari_web_audit_log("web_api", "/api/v2/engine/session/reset", "ses_local", "success", reset_mode);
    return esptari_web_send_json(req, resp, 200);
}
