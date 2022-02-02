# Check for boost

set(Boost_NO_WARN_NEW_VERSIONS ON)
set(Boost_USE_STATIC_LIBS ON)
if(MSVC)
    set(Boost_USE_STATIC_RUNTIME ON)
endif()


find_package(Boost 1.78.0 REQUIRED
    COMPONENTS
        program_options
        system
)

include_directories(${Boost_INCLUDE_DIRS})

add_compile_definitions(BOOST_BEAST_USE_STD_STRING_VIEW)
