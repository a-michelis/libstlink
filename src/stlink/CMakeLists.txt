cmake_minimum_required(VERSION 3.28)
project(stlink LANGUAGES C CXX )

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

option(STLINK_BUILD_SHARED_LIBS "Build ST-Link library as a shared library" ${BUILD_SHARED_LIBS})

set(STLINK_SRC        "${PROJECT_SOURCE_DIR}/src")
set(STLINK_INCLUDE    "${PROJECT_SOURCE_DIR}/include")
set(STLINK_INCLUDE_PV "${PROJECT_SOURCE_DIR}/include_pv")

set (STLINK_SOURCES
        "${STLINK_SRC}/stlink_device.cpp"
)

# TODO: Add complementary sources depending on the build type etc

# target definition
if (STLINK_BUILD_SHARED_LIBS)
    add_library(stlink SHARED ${STLINK_SOURCES})
else ()
    add_library(stlink STATIC ${STLINK_SOURCES})
endif ()


# headers inclusion
target_include_directories( stlink SYSTEM PUBLIC
        $<BUILD_INTERFACE:${STLINK_INCLUDE}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_include_directories( stlink SYSTEM PRIVATE
        $<BUILD_INTERFACE:${STLINK_INCLUDE_PV}>
)


