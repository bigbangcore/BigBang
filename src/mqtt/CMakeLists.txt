# CMakeLists.txt
#
# Top-level CMake file for the Paho C++ library.
#
#*******************************************************************************
# This is part of the Paho MQTT C++ client library.
#
# Copyright (c) 2016-2017, Guilherme Maciel Ferreira
# Copyright (c) 2017, Frank Pagliughi
#
# All rights reserved. This program and the accompanying materials
# are made available under the terms of the Eclipse Public License v1.0
# and Eclipse Distribution License v1.0 which accompany this distribution.
# 
# The Eclipse Public License is available at
#   http://www.eclipse.org/legal/epl-v10.html
# and the Eclipse Distribution License is available at
#   http://www.eclipse.org/org/documents/edl-v10.php.
# 
# Contributors:
#   Guilherme Maciel Ferreira - initial version
#   Frank Pagliughi
#*******************************************************************************/

## Note: on OS X you should install XCode and the associated command-line tools

## cmake flags
cmake_minimum_required(VERSION 3.5)

## project name
project("paho-mqtt-cpp" 
    VERSION "1.1.0"
    LANGUAGES CXX
)

## --- Build options ---
message(STATUS "mqtt........................")
if(WIN32)
  option(PAHO_BUILD_STATIC "Build static library" TRUE)
  option(PAHO_BUILD_SHARED "Build shared library (DLL)" FALSE)
  option(PAHO_WITH_SSL "Build SSL-enabled library" FALSE)
  message(STATUS "win32")
else()
  option(PAHO_BUILD_STATIC "Build static library" FALSE)
  option(PAHO_BUILD_SHARED "Build shared library" TRUE)
  #option(PAHO_BUILD_STATIC "Build static library" TRUE)
  #option(PAHO_BUILD_SHARED "Build shared library" FALSE)
  #set(PAHO_BUILD_STATIC ON)
  #set(PAHO_BUILD_SHARED OFF)
  option(PAHO_WITH_SSL "Build SSL-enabled library" TRUE)
  message(STATUS "non-win32" " " "PAHO_BUILD_STATIC:" ${PAHO_BUILD_STATIC} " " "PAHO_BUILD_SHARED:" ${PAHO_BUILD_SHARED})
endif()

## --- C++11 build flags ---

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate position-independent code (-fPIC on UNIX)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- System Libraries ---

include(GNUInstallDirs)

if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    set(LIBS_SYSTEM ws2_32)
elseif(UNIX)
    set(LIBS_SYSTEM c stdc++)
endif()

## --- Build directories ---

# For the paho_mqtt_c module
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# --- Default library ---

if(PAHO_BUILD_SHARED)
    set(PAHO_CPP_LIB mqtt-shared)
    message(STATUS "shared......" ${PAHO_BUILD_SHARED})
    #set(PAHO_CPP_LIB paho-mqttpp3-static)
else()
    message(STATUS "static......")
    set(PAHO_CPP_LIB mqtt)
    ## --- Library dependencies ---
    find_package(PahoMqttC REQUIRED)
    set (THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    set(sources
            src/async_client.cpp
            src/client.cpp
            src/disconnect_options.cpp
            src/iclient_persistence.cpp
            src/message.cpp
            src/properties.cpp
            src/response_options.cpp
            src/ssl_options.cpp
            src/string_collection.cpp
            src/subscribe_options.cpp
            src/token.cpp
            src/topic.cpp
            src/connect_options.cpp
            src/will_options.cpp
            )
    add_library(mqtt ${sources})
    include_directories(./src/mqtt ${PAHO_MQTT_C_INCLUDE_DIRS})
    target_link_libraries(mqtt
            PRIVATE ${LIBS_SYSTEM}
            PUBLIC PahoMqttC::PahoMqttC Threads::Threads
            ${PAHO_MQTT_C_LIBRARIES}
            )

    target_include_directories(mqtt PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<INSTALL_INTERFACE:include>
            )
endif()

## --- Packaging settings ---

if(WIN32)
    set(CPACK_GENERATOR "ZIP")
elseif(UNIX)
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
#add_subdirectory(cmake)



