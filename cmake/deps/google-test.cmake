if(NOT BUILD_TESTING)
    return()
endif()

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/release-1.11.0.tar.gz
    URL_HASH MD5=e8a8df240b6938bb6384155d4c37d937
)

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

if(NOT googletest_POPULATED)
    FetchContent_Populate(googletest)
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
endif()

include(GoogleTest)