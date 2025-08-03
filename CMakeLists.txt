cmake_minimum_required(VERSION 3.20)

project(cpp-http-server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
endif()
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(Boost QUIET CONFIG)
if(NOT Boost_FOUND)
    find_package(Boost REQUIRED COMPONENTS system filesystem)
else()
    find_package(Boost REQUIRED COMPONENTS system filesystem CONFIG)
endif()

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

set(SERVER_SOURCES
    src/main.cpp
    src/server.cpp
    src/connection.cpp
    src/ssl_connection.cpp
    src/request.cpp
    src/response.cpp
    src/compression.cpp
)

set(SERVER_HEADERS
    include/server.hpp
    include/connection.hpp
    include/ssl_connection.hpp
    include/request.hpp
    include/response.hpp
    include/thread_pool.hpp
    include/compression.hpp
)

add_executable(http_server ${SERVER_SOURCES} ${SERVER_HEADERS})

target_link_libraries(http_server 
    PRIVATE 
    Boost::system 
    Boost::filesystem
    Threads::Threads
    nlohmann_json::nlohmann_json
    ZLIB::ZLIB
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_compile_features(http_server PRIVATE cxx_std_20)

set_property(TARGET http_server PROPERTY POSITION_INDEPENDENT_CODE ON)

if(BUILD_TESTING OR CMAKE_BUILD_TYPE STREQUAL "Debug")
    find_package(GTest QUIET)
    
    if(NOT GTest_FOUND)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0
        )
        FetchContent_MakeAvailable(googletest)
    endif()

    enable_testing()

    set(TEST_SOURCES
        test/test_server.cpp
        test/test_request.cpp
        test/test_response.cpp
        test/test_security.cpp
        test/test_performance.cpp
        test/test_http_protocol.cpp
        test/test_https.cpp
        src/request.cpp
        src/response.cpp
        src/connection.cpp
        src/ssl_connection.cpp
        src/server.cpp
        src/compression.cpp
    )

    add_executable(test_runner ${TEST_SOURCES})
    
    target_link_libraries(test_runner 
        PRIVATE 
        GTest::gtest_main
        Boost::system 
        Boost::filesystem
        Threads::Threads
        nlohmann_json::nlohmann_json
        ZLIB::ZLIB
        OpenSSL::SSL
        OpenSSL::Crypto
    )

    target_compile_features(test_runner PRIVATE cxx_std_20)

    include(GoogleTest)
    gtest_discover_tests(test_runner)
endif()

install(TARGETS http_server
    RUNTIME DESTINATION bin
)

install(DIRECTORY config/
    DESTINATION share/http_server/config
)