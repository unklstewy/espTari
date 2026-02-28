idf_component_register(
    SRCS "src/esptari_core.c"
    INCLUDE_DIRS "include"
    REQUIRES esptari_loader esptari_memory
    PRIV_REQUIRES freertos esp_timer
)
