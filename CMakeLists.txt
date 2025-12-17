cmake_minimum_required(VERSION 3.20)
project(libswim VERSION 0.1 LANGUAGES C)

# We use the "d" suffix for the debug version of the library"
set(CMAKE_DEBUG_POSTFIX d)

find_library(UUID_LIBRARY NAMES uuid)

message(STATUS "UUID_LIBRARY: ${UUID_LIBRARY}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

add_compile_options(-Wall -Wextra -Werror)

# Include binary directory to access generated files, e.g., config.h
include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR})
add_subdirectory(swim)
add_subdirectory(examples)
