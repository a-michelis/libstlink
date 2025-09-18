cmake_minimum_required(VERSION 3.28)
project(stlink_toolset)

# Global Options
option(BUILD_SHARED_LIBS "Build libraries as shared libraries" OFF)
option(BUILD_TESTING     "Build test programs"                 OFF)


# Global dependencies placeholder
# TODO: Add global dependencies here

# Add all subprojects from src folder
add_subdirectory(src/stlink)
