# Network Test App
# Tests WiFi/Ethernet connectivity and mDNS for ESP32-P4-NANO

cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(network_test)
