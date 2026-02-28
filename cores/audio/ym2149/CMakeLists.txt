# YM2149 PSG Sound Chip Component
# Build as EBIN loadable component using ebin_builder.py

set(COMPONENT_NAME "ym2149")

# Source files
set(YM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ym2149.c
)

# Include directories
set(YM_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/components/esptari_loader/include
)

# Build -I flags for ebin_builder
set(YM_INCLUDE_FLAGS "")
foreach(inc ${YM_INCLUDE_DIRS})
    list(APPEND YM_INCLUDE_FLAGS "-I" "${inc}")
endforeach()

# EBIN builder script
set(EBIN_BUILDER ${CMAKE_SOURCE_DIR}/tools/ebin_builder/ebin_builder.py)

# Output EBIN file
set(YM_EBIN ${CMAKE_CURRENT_BINARY_DIR}/${COMPONENT_NAME}.ebin)

# Header files for dependency tracking
set(YM_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/ym2149_internal.h
    ${CMAKE_SOURCE_DIR}/components/esptari_loader/include/component_api.h
    ${CMAKE_SOURCE_DIR}/components/esptari_loader/include/ebin_format.h
)

# Add custom command to build EBIN
add_custom_command(
    OUTPUT ${YM_EBIN}
    COMMAND python3 ${EBIN_BUILDER}
        ${YM_SOURCES}
        -o ${YM_EBIN}
        -t audio
        ${YM_INCLUDE_FLAGS}
        --interface-version 0x00010000
        -v
    DEPENDS ${YM_SOURCES} ${YM_HEADERS} ${EBIN_BUILDER}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building ${COMPONENT_NAME}.ebin (YM2149 PSG)"
    VERBATIM
)

# Custom target for the EBIN
add_custom_target(${COMPONENT_NAME}_ebin ALL
    DEPENDS ${YM_EBIN}
)

# Copy built EBIN to SD card audio directory
set(SDCARD_AUDIO_DIR ${CMAKE_SOURCE_DIR}/sdcard/cores/audio)
add_custom_command(TARGET ${COMPONENT_NAME}_ebin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SDCARD_AUDIO_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${YM_EBIN} ${SDCARD_AUDIO_DIR}/
    COMMENT "Copying ${COMPONENT_NAME}.ebin to ${SDCARD_AUDIO_DIR}"
)
