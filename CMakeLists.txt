# espTari - Atari ST/STe/Mega/Falcon Emulator for ESP32-P4
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.16)

# Include component directories
set(EXTRA_COMPONENT_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}/components
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(espTari)
