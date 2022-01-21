FetchContent_Declare(
    boost-url
    GIT_REPOSITORY https://github.com/CPPAlliance/url.git
    GIT_TAG 1093ca5fd36c5976d1bf12b014a6ec4d60896162
    GIT_SHALLOW TRUE
)

FetchContent_GetProperties(boost-url)
if(NOT boost-url_POPULATED)
    FetchContent_Populate(boost-url)
endif()

add_library(boost-url INTERFACE)
target_include_directories(boost-url INTERFACE "${boost-url_SOURCE_DIR}/include")
# Use Header-only boosturl.
target_compile_definitions(boost-url INTERFACE BOOST_URL_NO_LIB BOOST_URL_STATIC_LINK)
target_link_libraries(boost-url INTERFACE Boost::system)

add_library(Boost::URL ALIAS boost-url)
