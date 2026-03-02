#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t machine_load(const char *profile_name);
bool machine_is_loaded(void);
