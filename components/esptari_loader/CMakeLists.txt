idf_component_register(
    SRCS
        "src/ebin_parser.c"
        "src/relocator.c"
        "src/loader.c"
        "src/registry.c"
        "src/machine_config.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        esp_psram
        fatfs
    PRIV_REQUIRES
        esp_partition
        json
)

# Strict compilation warnings
target_compile_options(${COMPONENT_LIB} PRIVATE 
    -Wall 
    -Wextra 
    -Werror
    -Wno-unused-parameter
)
