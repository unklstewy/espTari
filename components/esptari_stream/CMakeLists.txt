idf_component_register(
    SRCS "src/esptari_stream.c"
    INCLUDE_DIRS "include"
    REQUIRES esptari_video esptari_audio esp_http_server
    PRIV_REQUIRES esp_driver_jpeg esp_psram esp_timer heap freertos log
)
