idf_component_register(
    SRCS "src/esptari_stream.c"
    INCLUDE_DIRS "include"
    REQUIRES esptari_video esptari_audio esptari_network
    PRIV_REQUIRES esp_psram
)
