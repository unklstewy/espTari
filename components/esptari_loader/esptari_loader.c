#include "esptari_loader.h"

#include <stdio.h>

esp_err_t loader_init(void)
{
    return ESP_OK;
}

void esptari_loader_log_unified_config(void)
{
}

const char *esptari_loader_get_resolved_profile_name(void)
{
    return "st_default";
}
