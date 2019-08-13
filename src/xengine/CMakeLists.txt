#------------------------------------------------------------------------------
# CMake file for Bigbang
#
# Copyright (c) 2019 The Bigbang developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#------------------------------------------------------------------------------

set(sources
	version.h
	type.h
	util.cpp		util.h
	rwlock.h
	cache.h
	compacttv.h
	entry/entry.cpp		entry/entry.h
	event/event.cpp		event/event.h
	event/eventproc.cpp	event/eventproc.h
	stream/circular.cpp	stream/circular.h
	stream/stream.cpp	stream/stream.h
	base/base.cpp		base/base.h
	docker/config.cpp	docker/config.h
	docker/docker.cpp	docker/docker.h
	netio/nethost.cpp	netio/nethost.h
	netio/ioclient.cpp	netio/ioclient.h
	netio/iocontainer.cpp	netio/iocontainer.h
	netio/ioproc.cpp	netio/ioproc.h
	console/console.cpp	console/console.h
	peernet/nodemngr.cpp	peernet/nodemngr.h
	peernet/epmngr.cpp	peernet/epmngr.h
	peernet/peer.cpp	peernet/peer.h
	peernet/peernet.cpp	peernet/peernet.h
	peernet/datasched.h
	http/httpcookie.cpp	http/httpcookie.h
	http/httpsse.cpp	http/httpsse.h
	http/httputil.cpp	http/httputil.h
	http/httpserver.cpp	http/httpserver.h
	http/httpget.cpp	http/httpget.h
	db/kvdb.h
	stream/datastream.h
	docker/log.h  
	docker/nettime.h  
	docker/thread.h  
	docker/timer.h
	netio/netio.h
	http/httptype.h
	http/httpevent.h
	peernet/peerevent.h
	peernet/peerinfo.h
)

add_library(xengine ${sources})

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(./)

if (CYGWIN)
	target_link_libraries(xengine
		Boost_system
		Boost_filesystem
		Boost_program_options
		Boost_thread
		Boost_date_time
		Boost_regex
		OpenSSL::SSL
		OpenSSL::Crypto
		${Readline_LIBRARY})
else()
	target_link_libraries(xengine
		Boost::system
		Boost::filesystem
		Boost::program_options
		Boost::thread
		Boost::date_time
		Boost::regex
		OpenSSL::SSL
		OpenSSL::Crypto
		${Readline_LIBRARY})
endif()
