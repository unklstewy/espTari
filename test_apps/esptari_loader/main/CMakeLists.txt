idf_component_register(
    SRCS
        "test_runner.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        unity
        esptari_loader
        nvs_flash
)

# Register test sources from the component's test directory
target_sources(__idf_main PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../components/esptari_loader/test/test_ebin_parser.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../components/esptari_loader/test/test_relocator.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../components/esptari_loader/test/test_registry.c"
)

target_include_directories(__idf_main PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../components/esptari_loader/include"
)
