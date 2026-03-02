#include "esptari_core.h"

#include <string.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "machine.h"

static SemaphoreHandle_t core_lock;
static esptari_session_status_t session_status;
static bool snapshot_valid;
static char snapshot_id[64];

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
    return esptari_core_start();
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
    update_state(ESPTARI_SESSION_STOPPED);
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

    strlcpy(snapshot_id, new_snapshot_id, sizeof(snapshot_id));
    snapshot_valid = true;
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

    if (strncmp(restore_snapshot_id, "incompat_", 9) == 0) {
        xSemaphoreGive(core_lock);
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (!snapshot_valid || strcmp(snapshot_id, restore_snapshot_id) != 0) {
        xSemaphoreGive(core_lock);
        return ESP_ERR_NOT_FOUND;
    }

    update_state(resume_running ? ESPTARI_SESSION_RUNNING : ESPTARI_SESSION_PAUSED);
    xSemaphoreGive(core_lock);
    return ESP_OK;
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
