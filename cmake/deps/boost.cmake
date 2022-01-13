# Check for boost

set(Boost_NO_WARN_NEW_VERSIONS ON)

find_package(Boost 1.78.0 REQUIRED
    COMPONENTS
        program_options
        system
)

include_directories(${Boost_INCLUDE_DIRS})
