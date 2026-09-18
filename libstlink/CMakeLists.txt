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
        # sources are added here as they are written
)

target_include_directories(stlink
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include_pv
)

target_compile_features(stlink PUBLIC cxx_std_17)

install(TARGETS stlink EXPORT stlinkTargets)
install(DIRECTORY include/stlink DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
