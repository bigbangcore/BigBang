#------------------------------------------------------------------------------
# CMake file for Bigbang
#
# Copyright (c) 2019 The Bigbang developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#------------------------------------------------------------------------------

set(template
    template/templateid.h   template/templateid.cpp 
    template/template.h     template/template.cpp
    template/weighted.h     template/weighted.cpp 
    template/multisig.h     template/multisig.cpp 
    template/fork.h         template/fork.cpp 
    template/mint.h         template/mint.cpp 
    template/proof.h        template/proof.cpp 
    template/delegate.h     template/delegate.cpp 
    template/exchange.h     template/exchange.cpp 
)

set(sources
    destination.h   destination.cpp 
    transaction.h 
    wallettx.h 
    proof.h
    profile.h       profile.cpp
    block.h
    forkcontext.h   
    message.h
    ${template}
)

add_library(common ${sources})

include_directories(../xengine ../crypto ../jsonrpc ./)
include_directories(${CMAKE_BINARY_DIR}/src/jsonrpc)

target_link_libraries(common
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_DATE_TIME_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    OpenSSL::SSL
    OpenSSL::Crypto
    xengine
    crypto
    jsonrpc
)