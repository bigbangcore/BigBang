#------------------------------------------------------------------------------
# CMake file for Bigbang
#
# Copyright (c) 2019 The Bigbang developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#------------------------------------------------------------------------------

set(sources
    timeseries.cpp timeseries.h
    blockdb.cpp blockdb.h
    blockbase.cpp blockbase.h
    blockindexdb.cpp blockindexdb.h
    walletdb.cpp walletdb.h
    txpooldata.cpp txpooldata.h
    unspentdb.cpp unspentdb.h
    delegatedb.cpp delegatedb.h
    forkdb.cpp forkdb.h
    purger.cpp purger.h
    leveldbeng.cpp leveldbeng.h
    txindexdb.cpp txindexdb.h
    ctsdb.cpp ctsdb.h
)

add_library(storage ${sources})

include_directories(../xengine ../crypto ../common ../leveldb/include ../snappy)

if (CYGWIN)
    target_link_libraries(storage
        Boost_system
        Boost_filesystem
        Boost_program_options
        Boost_thread
        Boost_date_time
        Boost_timer
        Boost_regex
        OpenSSL::SSL
        OpenSSL::Crypto
        xengine
        crypto
        common
        leveldb	
        snappy)
else()
    target_link_libraries(storage
        Boost::system
        Boost::filesystem
        Boost::program_options
        Boost::thread
        Boost::date_time
        Boost::timer
        Boost::regex
        OpenSSL::SSL
        OpenSSL::Crypto
        xengine
        crypto
        common
        leveldb	
        snappy)
endif()
