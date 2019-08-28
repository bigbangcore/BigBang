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
)

add_executable(test_big ${sources})

include_directories(../src/bigbang ../src/xengine ../src/crypto ../src/common ../src/storage ../src/network ../src/mpvss)

target_link_libraries(test_big
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    OpenSSL::SSL
    OpenSSL::Crypto
    mpvss
    crypto
)

add_executable(test_ctsdb test_big_main.cpp test_big.h test_big.cpp ctsdb_test.cpp)

target_link_libraries(test_ctsdb
    ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    OpenSSL::SSL
    OpenSSL::Crypto
    mpvss
    crypto
    storage
)
