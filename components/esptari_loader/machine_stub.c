#include "machine.h"

static bool s_machine_loaded;

esp_err_t machine_load(const char *profile_name)
{
    if (profile_name == NULL || profile_name[0] == '\0') {
        s_machine_loaded = false;
        return ESP_ERR_INVALID_ARG;
    }

    s_machine_loaded = true;
    return ESP_OK;
}

bool machine_is_loaded(void)
{
    return s_machine_loaded;
}
