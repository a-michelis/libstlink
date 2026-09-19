###
# libstlink, the library
#
# include/      public headers, installed, reached as <stlink/...>
# include_pv/   private headers, not installed, reached as <stlink/...> too
# src/          implementation; src/${OS}/ holds what differs per platform
###

add_library(stlink)
add_library(stlink::stlink ALIAS stlink)

target_sources(stlink
    PRIVATE
        src/error.cpp
        src/log.cpp
        src/result.cpp
)

target_include_directories(stlink
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include_pv
)

target_compile_features(stlink PUBLIC cxx_std_17)

# STLINK_EXPORT is private, so export.h declares an export while the library is
# being built and an import everywhere else. Hidden visibility elsewhere makes
# the non-Windows builds agree with Windows about what is public.
target_compile_definitions(stlink PRIVATE STLINK_EXPORT)

if (MINGW)
    # mingw-w64 ships both the Microsoft printf and a C99 one. Ask for the
    # latter, so that %zu and %llu mean what they mean everywhere else.
    target_compile_definitions(stlink PUBLIC __USE_MINGW_ANSI_STDIO=1)
endif()

if (NOT BUILD_SHARED_LIBS)
    target_compile_definitions(stlink PUBLIC STLINK_STATIC)
endif()

set_target_properties(stlink PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

install(TARGETS stlink EXPORT stlinkTargets)
install(DIRECTORY include/stlink DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
