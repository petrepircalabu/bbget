add_library(bbget-obj OBJECT
    boost_url_link.cpp
    certs.hpp
    certs.cpp
    connection.hpp
    connection.cpp
    downloader.hpp
    overload.hpp
    proxy.hpp
    proxy.cpp
    stream.hpp
)

target_link_libraries(bbget-obj PUBLIC
    Boost::URL
    cppcodec
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog::spdlog
)

add_executable(bbget
    main.cpp
)

target_link_libraries(bbget PRIVATE
    bbget-obj
    Boost::program_options
)
