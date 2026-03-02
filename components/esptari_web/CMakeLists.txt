idf_component_register(
    SRCS "esptari_web.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server esptari_core esptari_input esp_timer json
)
