cmake_minimum_required(VERSION 3.20)
project(
  libswim
  VERSION 0.1
  LANGUAGES C)

# We use the "d" suffix for the debug version of the library"
set(CMAKE_DEBUG_POSTFIX d)

find_library(UUID_LIBRARY NAMES uuid)
find_package(cmocka)

message(STATUS "UUID_LIBRARY: ${UUID_LIBRARY}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

add_compile_options(-Wall -Wextra -Werror)
add_definitions(-D_GNU_SOURCE)

enable_testing()

# Include binary directory to access generated files, e.g., config.h
include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_BINARY_DIR})
add_subdirectory(swim)
add_subdirectory(examples)

find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
  file(GLOB_RECURSE ALL_SOURCE_FILES swim/*.c swim/*.h examples/*.c)
  add_custom_target(format COMMAND ${CLANG_FORMAT} -i ${ALL_SOURCE_FILES})
endif()

find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
  file(GLOB TIDY_SOURCE_FILES swim/*.c examples/*.c)
  list(FILTER TIDY_SOURCE_FILES EXCLUDE REGEX "_test\\.c$")
  add_custom_target(tidy COMMAND ${CLANG_TIDY} -p ${CMAKE_BINARY_DIR}
                                 ${TIDY_SOURCE_FILES})
endif()
