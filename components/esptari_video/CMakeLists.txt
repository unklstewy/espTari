idf_component_register(
    SRCS "src/esptari_video.c"
    INCLUDE_DIRS "include"
    REQUIRES esptari_core esptari_memory
    PRIV_REQUIRES esp_psram
)
