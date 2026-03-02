#pragma once

#include "esp_err.h"

esp_err_t loader_init(void);
void esptari_loader_log_unified_config(void);
const char *esptari_loader_get_resolved_profile_name(void);
