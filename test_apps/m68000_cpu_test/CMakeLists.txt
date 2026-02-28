# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.16)

# Add component paths
set(EXTRA_COMPONENT_DIRS 
    "${CMAKE_CURRENT_LIST_DIR}/../../components"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(m68000_cpu_test)
