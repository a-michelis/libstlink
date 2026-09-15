###
# System-specific definitions, header files and libraries
###

# Check for system-specific additional header files and libraries
include(CheckIncludeFile)
include(CheckLibraryExists)

set(STLINK_DEFINITIONS)      # collect all system-specific compiler definitions here
set(STLINK_COMPILE_OPTIONS)  # collect all system-specific compiler flags here

# ====================================================================================================

## Windows-specific build configuration
if (WIN32)
    message(STATUS "Configuring for Windows (Windows 10+)")

    # Compiler definitions for Windows
    list(APPEND STLINK_DEFINITIONS
        -D_WIN32_WINNT=0x0A00   # Minimum system requirement: Windows 10
        -D_WIN32=1              # Define _WIN32 for platform specific code
        -DWIN32_LEAN_AND_MEAN   # Keep windows.h from pulling in winsock.h v1, which conflicts with
                                # winsock2.h regardless of include order (libusb.h drags in windows.h
                                # ahead of remote.h/win32_socket.h in some TUs)
    )

    # Include directories containing windows-specific header files
    list(APPEND STLINK_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/win32)

    link_libraries(ws2_32)      # Link Windows-Socket library

    # TODO: Additional win32_socket ???
    list(APPEND STLINK_SOURCES src/stlink-env/win32/win32_socket.c)
    list(APPEND STLINK_HEADERS src/stlink-env/win32/win32_socket.h)

endif()

## Additional fixes for MSVC
if (MSVC)
    # Use string.h rather than strings.h and disable annoying warnings
    list(APPEND STLINK_DEFINITIONS
        -DHAVE_STRING_H
        -D_CRT_SECURE_NO_WARNINGS
        -D_CRT_NONSTDC_NO_WARNINGS
        )
    list(APPEND STLINK_COMPILE_OPTIONS /wd4710)
endif()

# ====================================================================================================

### Threading implementation
# One header, one implementation per platform, selected here rather than with
# preprocessor branches inside a single file.

if (WIN32)
    list(APPEND STLINK_SOURCES src/stlink-env/win32/stlink_thread.c)
else ()
    list(APPEND STLINK_SOURCES src/stlink-env/posix/stlink_thread.c)
endif()


### USB transport implementation

# Only one implementation of the interface in src/stlink-lib/usb_backend.h is
# compiled, so it is named here rather than globbed with the rest of the
# library. Selecting it also decides which libraries have to be found.

set(STLINK_USB_BACKEND "libusb" CACHE STRING "USB transport implementation to build")
set_property(CACHE STLINK_USB_BACKEND PROPERTY STRINGS libusb winusb)

# Every implementation there is, so that the ones not picked can be kept out of
# the library sources.
set(STLINK_USB_BACKEND_SOURCES_ALL usb_libusb.c usb_winusb.c)

if (STLINK_USB_BACKEND STREQUAL "libusb")
    find_package(libusb REQUIRED)
    set(STLINK_USB_BACKEND_SOURCE usb_libusb.c)
    set(STLINK_USB_BACKEND_LIBRARIES libusb::libusb)
elseif (STLINK_USB_BACKEND STREQUAL "winusb")
    if (NOT WIN32)
        message(FATAL_ERROR "STLINK_USB_BACKEND=winusb is only available on Windows")
    endif()

    # Both ship with the Windows SDK and with mingw-w64, so there is nothing to
    # find and nothing to bundle: winusb.dll and setupapi.dll are part of the
    # system. This is the point of the backend.
    set(STLINK_USB_BACKEND_SOURCE usb_winusb.c)
    set(STLINK_USB_BACKEND_LIBRARIES winusb setupapi)
else ()
    message(FATAL_ERROR "Unknown STLINK_USB_BACKEND '${STLINK_USB_BACKEND}'")
endif()

message(STATUS "USB transport: ${STLINK_USB_BACKEND}")


### Stack Smashing Protection Library (SSP)

CHECK_LIBRARY_EXISTS(ssp __stack_chk_fail "" _stack_chk_fail_exists)
if (_stack_chk_fail_exists)
    if (WIN32)
        set(SSP_LIB -static ssp) # static linking
    else ()
        set(SSP_LIB ssp)
    endif()
else ()
    set(SSP_LIB "")
endif()


### Check for C standard library include files [POSIX]

CHECK_INCLUDE_FILE(sys/mman.h STLINK_HAVE_SYS_MMAN_H)
if (STLINK_HAVE_SYS_MMAN_H)
    list(APPEND STLINK_DEFINITIONS -DSTLINK_HAVE_SYS_MMAN_H)
else ()
    if (WIN32 OR MSVC OR MINGW)
        list(APPEND STLINK_SOURCES src/stlink-env/win32/sys_mmap.c)
        list(APPEND STLINK_HEADERS src/stlink-env/win32/sys_mmap.h)
    endif()
endif()

CHECK_INCLUDE_FILE(sys/time.h STLINK_HAVE_SYS_TIME_H)
if (STLINK_HAVE_SYS_TIME_H)
    list(APPEND STLINK_DEFINITIONS -DSTLINK_HAVE_SYS_TIME_H)
else ()
    if (WIN32 OR MSVC OR MINGW)
        list(APPEND STLINK_SOURCES src/stlink-env/win32/sys_time.c)
        list(APPEND STLINK_HEADERS src/stlink-env/win32/sys_time.h)
    endif()
endif()

CHECK_INCLUDE_FILE(unistd.h STLINK_HAVE_UNISTD_H)
if (STLINK_HAVE_UNISTD_H)
    list(APPEND STLINK_DEFINITIONS -DSTLINK_HAVE_UNISTD_H)
else ()
    if (WIN32 OR MSVC OR MINGW)
        list(APPEND STLINK_HEADERS src/stlink-env/win32/unistd.h)
    endif()
endif()


### Check for non C standard library include files [POSIX]

if (NOT WIN32)
    CHECK_INCLUDE_FILE(dirent.h STLINK_HAVE_DIRENT_H)
    if (STLINK_HAVE_DIRENT_H)
        list(APPEND STLINK_DEFINITIONS -DSTLINK_HAVE_DIRENT_H)
    else ()
        message(FATAL_ERROR "dirent.h not found! init_chipids can not be compiled for Unix/Linux.")
    endif()
else () # WIN32
    CHECK_INCLUDE_FILE(windows.h STLINK_HAVE_WINDOWS_H)
    message(STATUS "Windows platform detected. Using Windows API for init_chipids.")
    list(APPEND STLINK_DEFINITIONS -DSTLINK_HAVE_WINDOWS_H)
    # use GetModuleFileNameA and FindFirstFileA
endif()

# ====================================================================================================

### Propagate system-specific additional definitions, sources, headers and libraries to parent scope
set(STLINK_SOURCES ${STLINK_SOURCES} PARENT_SCOPE)
set(STLINK_HEADERS ${STLINK_HEADERS} PARENT_SCOPE)
set(STLINK_DEFINITIONS ${STLINK_DEFINITIONS} PARENT_SCOPE)
set(STLINK_COMPILE_OPTIONS ${STLINK_COMPILE_OPTIONS} PARENT_SCOPE)
set(STLINK_INCLUDE_DIRS ${STLINK_INCLUDE_DIRS} PARENT_SCOPE)
set(STLINK_USB_BACKEND_SOURCE ${STLINK_USB_BACKEND_SOURCE} PARENT_SCOPE)
set(STLINK_USB_BACKEND_SOURCES_ALL ${STLINK_USB_BACKEND_SOURCES_ALL} PARENT_SCOPE)
set(STLINK_USB_BACKEND_LIBRARIES ${STLINK_USB_BACKEND_LIBRARIES} PARENT_SCOPE)
