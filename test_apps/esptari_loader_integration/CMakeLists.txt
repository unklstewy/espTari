# Integration test for EBIN loader
cmake_minimum_required(VERSION 3.16)

# Add the esptari_loader component
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esptari_loader_integration)
