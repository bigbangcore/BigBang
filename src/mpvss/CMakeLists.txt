#------------------------------------------------------------------------------
# CMake file for Bigbang
#
# Copyright (c) 2019 The Bigbang developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#------------------------------------------------------------------------------

# if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
#     add_definitions(-D__GLIBCXX_TYPE_INT_N_0=__int128 -D__GLIBCXX_BITSIZE_INT_N_0=128)
# endif()

set(sources
    parallel.h
    mpbox.cpp mpbox.h
    mpinterpolation.cpp mpinterpolation.h
    mpvss.cpp mpvss.h
)

add_library(mpvss ${sources})

include_directories(../xengine ../crypto ./)

target_link_libraries(mpvss
    xengine
)
