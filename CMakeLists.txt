###
# libstlink
#
# Orchestration only: this file defines no targets of its own. The library and
# each tool own their own CMakeLists.txt, so a component can be read, built or
# removed without reading this one.
###

cmake_minimum_required(VERSION 3.16)

project(stlink
        VERSION 0.1.0
        DESCRIPTION "Library and tools for STMicroelectronics ST-LINK programmers"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(GNUInstallDirs)

add_subdirectory(libstlink)
