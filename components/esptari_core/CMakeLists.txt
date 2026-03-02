idf_component_register(
    SRCS "esptari_core.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos esp_timer esptari_loader
)
