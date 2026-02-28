idf_component_register(SRCS "sdcard_test_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES esptari_loader fatfs sdmmc vfs driver esp_driver_sdmmc)
