add_executable(bbget main.cpp)

target_link_libraries(bbget PRIVATE
    Boost::program_options
    Boost::URL
    spdlog::spdlog
)
