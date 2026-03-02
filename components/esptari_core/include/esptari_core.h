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
esp_err_t esptari_core_stop(void);
esp_err_t esptari_core_reset(void);
void esptari_core_get_status(esptari_session_status_t *out_status);
const char *esptari_core_state_to_string(esptari_session_state_t state);
