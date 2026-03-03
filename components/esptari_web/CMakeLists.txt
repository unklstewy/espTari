idf_component_register(
    SRCS "esptari_web.c" "esptari_web_catalog.c" "esptari_web_debug.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server esptari_core esptari_input esp_timer json
)
