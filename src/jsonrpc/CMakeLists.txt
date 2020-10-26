#------------------------------------------------------------------------------
# CMake file for Bigbang
#
# Copyright (c) 2019-2020 The Bigbang developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#------------------------------------------------------------------------------

# auto generate files
set(auto_files
    ${CMAKE_CURRENT_BINARY_DIR}/rpc/auto_protocol.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/rpc/auto_protocol.h
)
if(MSVC)
    set(PYTHON_PATH "python.exe")
else()
    set(PYTHON_PATH "python")
endif()
add_custom_command(OUTPUT ${auto_files}
    COMMAND "${PYTHON_PATH}" "\"${CMAKE_SOURCE_DIR}/script/generator.py\"" "\"rpc\"" "\"${CMAKE_CURRENT_BINARY_DIR}\""
    DEPENDS "${CMAKE_SOURCE_DIR}/script/template/rpc.json"
            "${CMAKE_SOURCE_DIR}/script/generator.py"
            "${CMAKE_SOURCE_DIR}/script/auto_protocol.py"
)

set(sources
    rpc/rpc.h
    rpc/rpc_error.cpp   rpc/rpc_error.h
    rpc/rpc_req.cpp     rpc/rpc_req.h
    rpc/rpc_resp.cpp    rpc/rpc_resp.h
    rpc/rpc_type.h
    ${auto_files}
)

add_library(jsonrpc ${sources})

include_directories(../xengine ./ ${CMAKE_CURRENT_BINARY_DIR}/src)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(jsonrpc
    xengine
)
