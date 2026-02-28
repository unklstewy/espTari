idf_component_register(
    SRCS "src/st_memory.c" "src/st_glue.c" "src/st_acia.c"
    INCLUDE_DIRS "include"
    REQUIRES esptari_loader
    PRIV_REQUIRES fatfs esp_psram
)
