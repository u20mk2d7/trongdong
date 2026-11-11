cmake_minimum_required(VERSION 3.30)

find_package(simdjson REQUIRED)
find_package(Boost REQUIRED)
find_package(absl REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)
find_package(jemalloc REQUIRED)
find_package(flatbuffers REQUIRED)

add_executable(dnse_client
    main.cpp
    gateway/wsclient.cpp
)

target_include_directories(dnse_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(dnse_client PRIVATE
    Boost::boost
    OpenSSL::SSL
    OpenSSL::Crypto
    simdjson::simdjson
)

target_compile_features(dnse_client PRIVATE cxx_std_23)

target_compile_options(dnse_client PRIVATE
    -Wall -Wextra -Wpedantic
    -Wconversion -Wsign-conversion -Wshadow
)
