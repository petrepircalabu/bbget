find_package(OpenSSL REQUIRED)

if (WIN32)
    target_link_libraries(OpenSSL::Crypto INTERFACE Crypt32)
endif()
