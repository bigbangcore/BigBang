#------------------------------------------------------------------------------
# CMake file for Bigbang
#
# Copyright (c) 2019 The Bigbang developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#------------------------------------------------------------------------------

set(sources
    test_big_main.cpp
    test_big.h test_big.cpp
    uint256_tests.cpp
    rpc_tests.cpp
    version_tests.cpp
    mpvss_tests.cpp
    ipv6_tests.cpp
    crypto_tests.cpp
    actor_tests.cpp
    timer_tests.cpp
    netchn_tests.cpp
)

add_executable(test_big ${sources})

include_directories(${CMAKE_CURRENT_BINARY_DIR}/../src/bigbang)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../src/jsonrpc)
include_directories(../src ../src/bigbang ../src/xengine ../src/crypto ../src/common ../src/storage ../src/network ../src/mpvss ../src/bigbang ../src/jsonrpc)

target_link_libraries(test_big
    libbigbang
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_CHRONO_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_DATE_TIME_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${Protobuf_LIBRARIES}
    ${Readline_LIBRARY}
    OpenSSL::SSL
    OpenSSL::Crypto
    mpvss
    crypto
    xengine
    storage    
    network
    delegate    
    jsonrpc
    common
)

add_executable(test_ctsdb test_big_main.cpp test_big.h test_big.cpp ctsdb_test.cpp)

target_link_libraries(test_ctsdb
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    OpenSSL::SSL
    OpenSSL::Crypto
    mpvss
    crypto
    storage
)
