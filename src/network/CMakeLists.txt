#------------------------------------------------------------------------------
# CMake file for Bigbang
#
# Copyright (c) 2019 The Bigbang developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#------------------------------------------------------------------------------

set(sources
	proto.cpp proto.h
	peerevent.h
	peer.cpp peer.h
	peernet.cpp peernet.h
)

add_library(network ${sources})

include_directories(../xengine ../common ../crypto ../storage)

if (CYGWIN)
	target_link_libraries(network
		Boost_system
		Boost_filesystem
		Boost_program_options
		Boost_thread
		Boost_date_time
		Boost_regex
        OpenSSL::SSL
        OpenSSL::Crypto
		${sodium_LIBRARY_RELEASE}
		xengine
		crypto
		storage)
else()
	target_link_libraries(network
		Boost::system
		Boost::filesystem
		Boost::program_options
		Boost::thread
		Boost::date_time
		Boost::regex
        OpenSSL::SSL
        OpenSSL::Crypto
		${sodium_LIBRARY_RELEASE}
		xengine
		crypto
		storage)
endif()