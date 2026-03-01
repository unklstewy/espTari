set(CORE_SRCS
    "${CMAKE_CURRENT_LIST_DIR}/../../cores/cpu/m68000/m68000.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../cores/cpu/m68000/m68000_ops.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../cores/cpu/m68000/m68000_ea.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../cores/video/shifter/shifter.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../cores/audio/ym2149/ym2149.c"
    "${CMAKE_CURRENT_LIST_DIR}/../../cores/misc/mfp68901/mfp68901.c"
)

idf_component_register(
    SRCS "src/esptari_core.c" ${CORE_SRCS}
    INCLUDE_DIRS "include"
    REQUIRES esptari_loader esptari_memory esptari_video
    PRIV_REQUIRES freertos esp_timer
)
