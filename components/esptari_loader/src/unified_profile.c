/**
 * @file unified_profile.c
 * @brief Menuconfig-driven unified profile resolver
 *
 * This module centralizes how runtime code derives a machine profile from
 * Kconfig selections. It is intentionally small and heavily documented because
 * it becomes the authoritative bridge between menuconfig and machine loading.
 *
 * SPDX-License-Identifier: MIT
 */

#include "esptari_loader.h"
#include "sdkconfig.h"
#include "esp_log.h"

static const char *TAG = "loader_unified";

bool esptari_loader_unified_enabled(void)
{
#ifdef CONFIG_ESPTARI_UNIFIED_EBIN_ENABLE
    return true;
#else
    return false;
#endif
}

const char *esptari_loader_get_resolved_profile_name(void)
{
#ifdef CONFIG_ESPTARI_UNIFIED_EBIN_ENABLE
    #if defined(CONFIG_ESPTARI_UNIFIED_PROFILE_ST)
        return "st";
    #elif defined(CONFIG_ESPTARI_UNIFIED_PROFILE_STFM)
        return "stfm";
    #elif defined(CONFIG_ESPTARI_UNIFIED_PROFILE_STE)
        return "ste";
    #elif defined(CONFIG_ESPTARI_UNIFIED_PROFILE_MEGA_ST)
        return "mega_st";
    #elif defined(CONFIG_ESPTARI_UNIFIED_PROFILE_MEGA_STE_16MB)
        return "mega_ste";
    #elif defined(CONFIG_ESPTARI_UNIFIED_PROFILE_MEGA_STE_MONOLITH)
        return "mega_ste_monolith";
    #else
        return "mega_ste";
    #endif
#else
    return CONFIG_ESPTARI_DEFAULT_MACHINE;
#endif
}

void esptari_loader_log_unified_config(void)
{
    if (!esptari_loader_unified_enabled()) {
        ESP_LOGI(TAG, "Unified profile mode: disabled (legacy default='%s')",
                 CONFIG_ESPTARI_DEFAULT_MACHINE);
        return;
    }

    ESP_LOGI(TAG, "Unified profile mode: enabled");
    ESP_LOGI(TAG, "Resolved base profile: %s",
             esptari_loader_get_resolved_profile_name());

#ifdef CONFIG_ESPTARI_UNIFIED_ALLOW_RUNTIME_PROFILE_OVERRIDE
    ESP_LOGI(TAG, "Runtime profile override: enabled");
#else
    ESP_LOGI(TAG, "Runtime profile override: disabled");
#endif

    ESP_LOGI(TAG,
             "Modules cpu68000=%d v_shifter=%d v_ste=%d a_ym2149=%d a_dma=%d io_mfp=%d io_fdc=%d io_blitter=%d",
#ifdef CONFIG_ESPTARI_UNIFIED_MODULE_CPU_M68000
             1,
#else
             0,
#endif
#ifdef CONFIG_ESPTARI_UNIFIED_MODULE_VIDEO_SHIFTER
             1,
#else
             0,
#endif
#ifdef CONFIG_ESPTARI_UNIFIED_MODULE_VIDEO_STE_SHIFTER
             1,
#else
             0,
#endif
#ifdef CONFIG_ESPTARI_UNIFIED_MODULE_AUDIO_YM2149
             1,
#else
             0,
#endif
#ifdef CONFIG_ESPTARI_UNIFIED_MODULE_AUDIO_DMA
             1,
#else
             0,
#endif
#ifdef CONFIG_ESPTARI_UNIFIED_MODULE_IO_MFP68901
             1,
#else
             0,
#endif
#ifdef CONFIG_ESPTARI_UNIFIED_MODULE_IO_FDC_WD1772
             1,
#else
             0,
#endif
#ifdef CONFIG_ESPTARI_UNIFIED_MODULE_IO_BLITTER
             1
#else
             0
#endif
    );
}
