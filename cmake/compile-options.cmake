# Compiler setup
include(CheckCXXCompilerFlag)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_compile_definitions(_FILE_OFFSET_BITS=64)

if(MSVC)
    include(compile-options-msvc)
endif()