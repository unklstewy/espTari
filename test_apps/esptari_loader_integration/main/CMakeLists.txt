idf_component_register(
    SRCS "integration_test_main.c"
    INCLUDE_DIRS "."
    REQUIRES esptari_loader spiffs nvs_flash
)
