# MC68000 CPU Core Component
# Build as EBIN loadable component using ebin_builder.py
#
# The ebin_builder.py compiles C sources into a position-independent EBIN
# binary that can be loaded at runtime into PSRAM by the esptari_loader.
#
# Usage (standalone):
#   python3 ebin_builder.py m68000.c m68000_ea.c m68000_ops.c \
#       -o m68000.ebin -t cpu -I . -I ../../components/esptari_loader/include

set(COMPONENT_NAME "m68000")

# Source files for the MC68000 core
set(M68K_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/m68000.c
    ${CMAKE_CURRENT_SOURCE_DIR}/m68000_ea.c
    ${CMAKE_CURRENT_SOURCE_DIR}/m68000_ops.c
)

# Include directories
set(M68K_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/components/esptari_loader/include
)

# Build -I flags for ebin_builder
set(M68K_INCLUDE_FLAGS "")
foreach(inc ${M68K_INCLUDE_DIRS})
    list(APPEND M68K_INCLUDE_FLAGS "-I" "${inc}")
endforeach()

# EBIN builder script
set(EBIN_BUILDER ${CMAKE_SOURCE_DIR}/tools/ebin_builder/ebin_builder.py)

# Output EBIN file
set(M68K_EBIN ${CMAKE_CURRENT_BINARY_DIR}/${COMPONENT_NAME}.ebin)

# Header files for dependency tracking
set(M68K_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/m68000_internal.h
    ${CMAKE_SOURCE_DIR}/components/esptari_loader/include/component_api.h
    ${CMAKE_SOURCE_DIR}/components/esptari_loader/include/ebin_format.h
)

# Add custom command to build EBIN
# ebin_builder.py CLI: sources... -o OUTPUT -t TYPE [-I INCLUDE]... [-e ENTRY] [--debug] [-v]
add_custom_command(
    OUTPUT ${M68K_EBIN}
    COMMAND python3 ${EBIN_BUILDER}
        ${M68K_SOURCES}
        -o ${M68K_EBIN}
        -t cpu
        ${M68K_INCLUDE_FLAGS}
        --interface-version 0x00010000
        -v
    DEPENDS ${M68K_SOURCES} ${M68K_HEADERS} ${EBIN_BUILDER}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building ${COMPONENT_NAME}.ebin (MC68000 CPU core)"
    VERBATIM
)

# Custom target for the EBIN
add_custom_target(${COMPONENT_NAME}_ebin ALL
    DEPENDS ${M68K_EBIN}
)

# Copy built EBIN to SD card cores directory for deployment
set(SDCARD_CORES_DIR ${CMAKE_SOURCE_DIR}/sdcard/cores/cpu)
add_custom_command(TARGET ${COMPONENT_NAME}_ebin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SDCARD_CORES_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${M68K_EBIN} ${SDCARD_CORES_DIR}/
    COMMENT "Copying ${COMPONENT_NAME}.ebin to ${SDCARD_CORES_DIR}"
)
