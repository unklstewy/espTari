#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    ESPTARI_SESSION_STOPPED = 0,
    ESPTARI_SESSION_RUNNING,
    ESPTARI_SESSION_PAUSED,
    ESPTARI_SESSION_SUSPENDED,
    ESPTARI_SESSION_FAULTED
} esptari_session_state_t;

typedef struct {
    esptari_session_state_t state;
    uint64_t transition_count;
    uint64_t last_transition_us;
} esptari_session_status_t;

void esptari_core_init(void);
esp_err_t esptari_core_start(void);
esp_err_t esptari_core_pause(void);
esp_err_t esptari_core_resume(void);
esp_err_t esptari_core_resume_with_mode(bool resume_running);
esp_err_t esptari_core_stop(void);
esp_err_t esptari_core_reset(void);
esp_err_t esptari_core_suspend_save(const char *snapshot_id);
esp_err_t esptari_core_restore_resume(const char *snapshot_id, bool resume_running);
esp_err_t esptari_core_validate_restore_compatibility(const char *snapshot_id,
                                                      bool strict,
                                                      bool *out_compatible);
const char *esptari_core_get_last_failed_compat_rule(void);
void esptari_core_get_status(esptari_session_status_t *out_status);
const char *esptari_core_state_to_string(esptari_session_state_t state);
