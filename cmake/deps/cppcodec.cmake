FetchContent_Declare(
    cppcodec
    GIT_REPOSITORY https://github.com/tplgy/cppcodec.git
    GIT_TAG 9838f9eaf077e42121cb42361e9a1613901fc5e8
    GIT_SHALLOW TRUE
)

FetchContent_GetProperties(cppcodec)
if(NOT cppcodec_POPULATED)
    FetchContent_Populate(cppcodec)
endif()

add_library(cppcodec INTERFACE)
target_include_directories(cppcodec INTERFACE "${cppcodec_SOURCE_DIR}/")
