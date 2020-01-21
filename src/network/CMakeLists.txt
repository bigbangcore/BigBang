#------------------------------------------------------------------------------
# CMake file for Bigbang
#
# Copyright (c) 2019-2020 The Bigbang developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#------------------------------------------------------------------------------

set(sources
    proto.cpp   proto.h
    peerevent.h
    peer.cpp    peer.h
    peernet.cpp peernet.h
)

add_library(network ${sources})

include_directories(../xengine ../common ../crypto ../storage)

target_link_libraries(network
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_DATE_TIME_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    OpenSSL::SSL
    OpenSSL::Crypto
    ${sodium_LIBRARY_RELEASE}
    xengine
    crypto
    storage
)