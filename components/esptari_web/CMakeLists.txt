idf_component_register(
    SRCS "src/esptari_web.c"
    INCLUDE_DIRS "include"
    REQUIRES esptari_core esptari_network esp_http_server spiffs heap log vfs
)
