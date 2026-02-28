# Shifter ST Video Controller Component
# Build as EBIN loadable component using ebin_builder.py

set(COMPONENT_NAME "shifter")

# Source files
set(SHIFTER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/shifter.c
)

# Include directories
set(SHIFTER_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/components/esptari_loader/include
)

# Build -I flags for ebin_builder
set(SHIFTER_INCLUDE_FLAGS "")
foreach(inc ${SHIFTER_INCLUDE_DIRS})
    list(APPEND SHIFTER_INCLUDE_FLAGS "-I" "${inc}")
endforeach()

# EBIN builder script
set(EBIN_BUILDER ${CMAKE_SOURCE_DIR}/tools/ebin_builder/ebin_builder.py)

# Output EBIN file
set(SHIFTER_EBIN ${CMAKE_CURRENT_BINARY_DIR}/${COMPONENT_NAME}.ebin)

# Header files for dependency tracking
set(SHIFTER_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/shifter_internal.h
    ${CMAKE_SOURCE_DIR}/components/esptari_loader/include/component_api.h
    ${CMAKE_SOURCE_DIR}/components/esptari_loader/include/ebin_format.h
)

# Add custom command to build EBIN
add_custom_command(
    OUTPUT ${SHIFTER_EBIN}
    COMMAND python3 ${EBIN_BUILDER}
        ${SHIFTER_SOURCES}
        -o ${SHIFTER_EBIN}
        -t video
        ${SHIFTER_INCLUDE_FLAGS}
        --interface-version 0x00010000
        -v
    DEPENDS ${SHIFTER_SOURCES} ${SHIFTER_HEADERS} ${EBIN_BUILDER}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building ${COMPONENT_NAME}.ebin (Shifter ST Video)"
    VERBATIM
)

# Custom target for the EBIN
add_custom_target(${COMPONENT_NAME}_ebin ALL
    DEPENDS ${SHIFTER_EBIN}
)

# Copy built EBIN to SD card video directory
set(SDCARD_VIDEO_DIR ${CMAKE_SOURCE_DIR}/sdcard/cores/video)
add_custom_command(TARGET ${COMPONENT_NAME}_ebin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SDCARD_VIDEO_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHIFTER_EBIN} ${SDCARD_VIDEO_DIR}/
    COMMENT "Copying ${COMPONENT_NAME}.ebin to ${SDCARD_VIDEO_DIR}"
)
