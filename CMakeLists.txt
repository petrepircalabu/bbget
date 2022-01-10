cmake_minimum_required (VERSION 3.13)
cmake_policy(SET CMP0048 NEW)

set(vcpkg "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
if(NOT CMAKE_TOOLCHAIN_FILE AND EXISTS "${vcpkg}")
  set(CMAKE_TOOLCHAIN_FILE "${vcpkg}"
      CACHE FILEPATH "CMake toolchain file")
  message(STATUS "vcpkg toolchain found: ${CMAKE_TOOLCHAIN_FILE}")
endif()

project(bbget VERSION "0.1")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(compile-options)
include(dependencies)

add_subdirectory(src)
