if (MSVC)
    add_compile_definitions(SPDLOG_WCHAR_SUPPORT)
    add_compile_definitions(SPDLOG_WCHAR_FILENAMES)
endif()

# Check for spdlog
find_package(spdlog CONFIG REQUIRED)
