cmake_minimum_required(VERSION 3.14)
project(st-logger LANGUAGES C CXX)

include(FetchContent)

find_package(spdlog QUIET)

if(NOT spdlog_FOUND)
    message(STATUS "Fetching spdlog...")

    set(BUILD_SHARED_LIBS_TMP ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS ON)

    FetchContent_Declare(
            spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG v1.11.0
            CMAKE_ARGS -DBUILD_SHARED_LIBS=ON
    )
    FetchContent_MakeAvailable(spdlog)

    set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_TMP})
endif()

add_library(st-logger SHARED
        src/callback_sink.cpp
        src/logging.cpp
)

target_include_directories(st-logger
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include_pv
)

target_link_libraries(st-logger PRIVATE spdlog::spdlog)

set_target_properties(st-logger PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        POSITION_INDEPENDENT_CODE ON
)

target_compile_definitions(st-logger PRIVATE STLOGGER_DLL_EXPORTING)


install(TARGETS st-logger
        EXPORT st-loggerTargets
        RUNTIME DESTINATION ${CMAKE_RUNTIME_INSTALL_DIRECTORY}
        LIBRARY DESTINATION ${CMAKE_LIBRARY_INSTALL_DIRECTORY}
        ARCHIVE DESTINATION ${CMAKE_ARCHIVE_INSTALL_DIRECTORY})

install(DIRECTORY include/ DESTINATION ${CMAKE_INCLUDES_INSTALL_DIRECTORY})
