#------------------------------------------------------------------------------
# CMake file for Bigbang
#
# Copyright (c) 2019 The Bigbang developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.5.1)

project(Bigbang LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}/cmake" CACHE PATH "The path to the cmake directory")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_DIR})

execute_process(
        COMMAND git rev-parse HEAD 
        COMMAND cut -c 1-8
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE vGitCommitID
        OUTPUT_STRIP_TRAILING_WHITESPACE)
        
set(GIT_VERSION 0x${vGitCommitID})
configure_file(
        "${PROJECT_SOURCE_DIR}/src/bigbang/git_version.h.in"
        "${PROJECT_BINARY_DIR}/src/bigbang/git_version.h")

if(CYGWIN)
    set(CMAKE_CXX_STANDARD_REQUIRED OFF)
elseif(UNIX)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED OFF)
    set(CMAKE_CXX_EXTENSIONS OFF)
    
else()
    message(FATAL_ERROR "Unsupported plantform")
endif()

# build type
if(CMAKE_BUILD_TYPE STREQUAL Debug)
    if(APPLE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -rdynamic")
    endif()
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

# warnings
if(UNIX AND NOT CYGWIN)
    string(APPEND CMAKE_CXX_FLAGS " -Wall -Wextra -Werror -Wno-char-subscripts -Wno-unused-parameter -Wno-sign-compare -Wno-reorder -Wno-unused-local-typedefs -Wno-implicit-fallthrough -Wno-overloaded-virtual -Wno-strict-aliasing")
    if (APPLE)
        string(APPEND CMAKE_CXX_FLAGS " -fvisibility-inlines-hidden -fvisibility=hidden")
    endif()
endif()

# libs & linkings
if(UNIX AND NOT APPLE)
    string(APPEND CMAKE_CXX_FLAGS " -lrt")
    if(NOT CYGWIN)
        link_libraries(-ldl)
    endif()
endif()

# packages
if(UNIX AND NOT CYGWIN)
    set(Boost_USE_STATIC_LIBS ON)
    set(OPENSSL_USE_STATIC_LIBS ON)
    set(sodium_USE_STATIC_LIBS ON)
    set(Readline_USE_STATIC_LIBS ON)
    set(Boost_USE_MULTITHREADED ON)
    set(Protobuf_USE_STATIC_LIBS ON)
endif()

set(Boost_USE_MULTITHREADED ON)
find_package(Boost 1.58.0 REQUIRED COMPONENTS
    program_options
    filesystem
    system
    thread
    date_time
    regex
    unit_test_framework
    timer
    chrono
    log
    locale
)
find_package(OpenSSL 1.0.0 REQUIRED) 
find_package(sodium 1.0.8 REQUIRED)
find_package(Readline 5.0 REQUIRED)
find_package(Protobuf 3.0.0 REQUIRED)

if(OPENSSL_VERSION VERSION_EQUAL 1.1.0 OR 
    (OPENSSL_VERSION VERSION_GREATER 1.1.0 AND OPENSSL_VERSION VERSION_LESS 1.2.0))
    add_definitions(-DUSE_SSL_110)
endif()

if(CYGWIN)
    add_definitions(-DBOOST_LOG_DYN_LINK)
endif()

# sub directories
add_subdirectory(src)
add_subdirectory(test)
