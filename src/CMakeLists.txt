add_executable(bbget
    boost_url_link.cpp
    certs.hpp
    certs.cpp
    connection.hpp
    connection.cpp
    main.cpp
)

target_link_libraries(bbget PRIVATE
    Boost::program_options
    Boost::URL
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog::spdlog
)
