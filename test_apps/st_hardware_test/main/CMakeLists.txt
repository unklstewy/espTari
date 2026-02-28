idf_component_register(SRCS "st_hw_test_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES esptari_loader esptari_memory fatfs sdmmc vfs driver esp_driver_sdmmc esp_timer)
