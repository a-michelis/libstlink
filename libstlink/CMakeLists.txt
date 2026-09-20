###
# libstlink, the library
#
# include/      public headers, installed, reached as <stlink/...>
# include_pv/   private headers, not installed, reached as <stlink/...> too
# src/          implementation; src/${OS}/ holds what differs per platform
###

add_library(stlink)
add_library(stlink::stlink ALIAS stlink)

if (WIN32)
    set(STLINK_OS windows)
elseif (APPLE)
    set(STLINK_OS darwin)
else ()
    set(STLINK_OS linux)
endif()

# How USB is reached. Named after the library rather than the system, because
# several systems use the same one: Windows will use WinUSB, and macOS may get
# its own, but until those exist libusb serves everywhere it is available.
if (WIN32)
    set(STLINK_USB winusb)
    message(STATUS "USB transport: WinUSB, which ships with the system")
else ()
    set(STLINK_USB libusb)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)
    message(STATUS "USB transport: libusb ${LIBUSB_VERSION}")
endif()

target_sources(stlink
    PRIVATE
        src/${STLINK_OS}/paths.cpp
        src/chip_database.cpp
        src/chip_description.cpp
        src/command.cpp
        src/error.cpp
        src/log.cpp
        src/result.cpp
        src/stlink.cpp
)

target_include_directories(stlink
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include_pv
)

target_compile_features(stlink PUBLIC cxx_std_17)

if (STLINK_USB)
    target_sources(stlink PRIVATE src/${STLINK_USB}/usb_transport.cpp)
endif()

if (STLINK_USB STREQUAL libusb)
    target_link_libraries(stlink PRIVATE PkgConfig::LIBUSB)
elseif (STLINK_USB STREQUAL winusb)
    # All three ship with Windows: winusb drives the device, setupapi finds it.
    target_link_libraries(stlink PRIVATE winusb setupapi)
endif()

# STLINK_EXPORT is private, so export.h declares an export while the library is
# being built and an import everywhere else. Hidden visibility elsewhere makes
# the non-Windows builds agree with Windows about what is public.
target_compile_definitions(stlink PRIVATE STLINK_EXPORT)

if (MINGW)
    # mingw-w64 ships both the Microsoft printf and a C99 one. Ask for the
    # latter, so that %zu and %llu mean what they mean everywhere else.
    target_compile_definitions(stlink PUBLIC __USE_MINGW_ANSI_STDIO=1)
endif()

# Where init() looks for the chip descriptions when it is not told: beside the
# program, which is the second place it tries.
install(DIRECTORY resources/chips/
        DESTINATION "${CMAKE_INSTALL_DATADIR}/stlink/config/chips"
        FILES_MATCHING PATTERN "*.chip")

if (NOT BUILD_SHARED_LIBS)
    target_compile_definitions(stlink PUBLIC STLINK_STATIC)
endif()

set_target_properties(stlink PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

install(TARGETS stlink EXPORT stlinkTargets)
install(DIRECTORY include/stlink DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
