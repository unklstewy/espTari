idf_component_register(
    SRCS "src/esptari_audio.c"
    INCLUDE_DIRS "include"
    REQUIRES esptari_core esptari_memory
    PRIV_REQUIRES heap log
)
