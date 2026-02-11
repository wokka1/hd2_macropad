cmake_minimum_required(VERSION 3.16.0)

# Read version from version.h to set PROJECT_VER
# This ensures ESP-IDF app metadata matches our version.h
file(READ "${CMAKE_SOURCE_DIR}/src/version.h" VERSION_HEADER)
string(REGEX MATCH "#define SW_VER \"([^\"]+)\"" _ ${VERSION_HEADER})
set(PROJECT_VER ${CMAKE_MATCH_1})
message(STATUS "Project version from version.h: ${PROJECT_VER}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(DEMO_LVGL)