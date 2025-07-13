cmake_minimum_required(VERSION 3.14)
project(logger_lib LANGUAGES C CXX)

include(FetchContent)

find_package(spdlog QUIET)

if(NOT spdlog_FOUND)
    message(STATUS "Fetching spdlog...")
    FetchContent_Declare(
            spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG v1.11.0
    )
    FetchContent_MakeAvailable(spdlog)
endif()

add_library(logger SHARED
        src/logger_internal.cpp
        src/custom_stderr_color_sink.cpp
        src/queue_sink.cpp
)

target_include_directories(logger
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include_pv
)

target_link_libraries(logger PRIVATE spdlog::spdlog)

set_target_properties(logger PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        POSITION_INDEPENDENT_CODE ON
)

target_compile_definitions(logger PRIVATE SPDLOGGER_DLL_EXPORTING)


install(TARGETS logger
        EXPORT loggerTargets
        RUNTIME DESTINATION ${CMAKE_RUNTIME_INSTALL_DIRECTORY}
        LIBRARY DESTINATION ${CMAKE_LIBRARY_INSTALL_DIRECTORY}
        ARCHIVE DESTINATION ${CMAKE_ARCHIVE_INSTALL_DIRECTORY})

install(DIRECTORY include/ DESTINATION ${CMAKE_INCLUDES_INSTALL_DIRECTORY})
