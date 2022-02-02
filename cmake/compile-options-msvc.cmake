if(NOT MSVC)
    message(FATAL_ERROR "These options are only valid for MSVC")
endif()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
add_compile_options(
    $<$<CONFIG:Debug>:/MDd>
    $<$<NOT:$<CONFIG:Debug>>:/MD>
)

add_compile_options(
    /EHsc
    /sdl
    /GS
    /bigobj
)

add_compile_definitions(
    _WINDOWS
    _CRT_SECURE_NO_WARNINGS
    _WIN32_WINNT=0x0601
    _CRT_NON_CONFORMING_SWPRINTFS
    BOOST_ASIO_ENABLE_CANCELIO
    _SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING
    _UNICODE
    UNICODE
    NOMINMAX
)

if(PLATFORM_TYPE MATCHES "x86_64")
    add_compile_definitions(WIN64)
else()
    add_compile_definitions(WIN32)
endif()

add_link_options($<$<CONFIG:Debug>:/debug:full>)
