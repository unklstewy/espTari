idf_component_register(
    SRCS "src/esptari_storage.c"
    INCLUDE_DIRS "include"
    REQUIRES fatfs spiffs nvs_flash
    PRIV_REQUIRES sdmmc
)
