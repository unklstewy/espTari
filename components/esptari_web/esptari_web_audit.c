#include "esptari_web_audit.h"

#include <stdint.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "esptari_web_audit";
static const char *AUDIT_LOG_PATH = "/sdcard/ebins/esptari_audit.jsonl";

void esptari_web_audit_log(const char *actor,
                           const char *action,
                           const char *target,
                           const char *status,
                           const char *reason)
{
    FILE *fp = fopen(AUDIT_LOG_PATH, "a");
    if (fp == NULL) {
        ESP_LOGW(TAG, "audit sink unavailable: %s", AUDIT_LOG_PATH);
        return;
    }

    uint64_t timestamp_us = (uint64_t)esp_timer_get_time();
    fprintf(fp,
            "{\"timestamp_us\":%llu,\"actor\":\"%s\",\"action\":\"%s\",\"target\":\"%s\",\"status\":\"%s\",\"reason\":\"%s\"}\n",
            (unsigned long long)timestamp_us,
            actor != NULL ? actor : "system",
            action != NULL ? action : "unknown",
            target != NULL ? target : "",
            status != NULL ? status : "unknown",
            reason != NULL ? reason : "");
    fclose(fp);
}
