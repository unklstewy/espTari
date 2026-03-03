idf_component_register(
    SRCS "esptari_web.c" "esptari_web_catalog.c" "esptari_web_core_status.c" "esptari_web_debug.c" "esptari_web_debug_mode.c" "esptari_web_debug_step.c" "esptari_web_debug_state.c" "esptari_web_http_utils.c" "esptari_web_stream.c" "esptari_web_lifecycle.c" "esptari_web_lifecycle_session.c" "esptari_web_lifecycle_state.c" "esptari_web_mappings.c" "esptari_web_metrics.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server esptari_core esptari_input esp_timer json
)
