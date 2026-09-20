###
# libstlink tests
#
# Entered only when STLINK_BUILD_TESTS is on, so an ordinary build defines no
# test target and, more to the point, fetches nothing. GoogleTest is a test
# dependency and never reaches a released artefact.
###

include(FetchContent)

# Pinned, so what the tests run against does not change under them.
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
)

# Windows: use the same runtime as everything else rather than gtest's default.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

include(GoogleTest)

###
# One executable per area under test. Each links the library and sees its
# private headers, since most of what needs testing is internal.
###

function(stlink_test name)
    add_executable(${name} ${ARGN})

    # The objects, not the library: a test needs what is deliberately not
    # exported, and linking the objects sidesteps visibility entirely.
    target_link_libraries(${name} PRIVATE stlink_objects GTest::gtest_main GTest::gmock)

    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include_pv)

    # Discovers every TEST() as its own ctest entry, so a failure names the case.
    gtest_discover_tests(${name} DISCOVERY_TIMEOUT 30)
endfunction()

stlink_test(test_chip_description test_chip_description.cpp)
stlink_test(test_chip_database    test_chip_database.cpp)
stlink_test(test_command          test_command.cpp)
