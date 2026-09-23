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

option(STLINK_BUILD_TESTS "Build the test suite" OFF)
option(STLINK_BUILD_DOCS "Regenerate the manual page and build the documentation site" OFF)

add_subdirectory(libstlink)

# Tests are a subdirectory that is simply not entered when they are off, so a
# normal build neither fetches anything nor defines a test target.
if (STLINK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(libstlink/tests)
endif()

# Always entered, unlike tests, because it installs the committed manual page
# and that should happen whether or not anyone can regenerate it. The option
# only controls whether the generating targets are defined.
add_subdirectory(docs)
