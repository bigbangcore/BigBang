#------------------------------------------------------------------------------
# CMake file for Bigbang
#
# Copyright (c) 2019 The Bigbang developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#------------------------------------------------------------------------------

# auto generate files
set(auto_files
    ${CMAKE_CURRENT_BINARY_DIR}/mode/auto_options.h
    ${CMAKE_CURRENT_BINARY_DIR}/mode/auto_config.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/mode/auto_config.h
)
add_custom_command(OUTPUT ${auto_files}
    COMMAND "${CMAKE_SOURCE_DIR}/script/generator.py" "'mode'" "'${CMAKE_CURRENT_BINARY_DIR}'"
    DEPENDS "${CMAKE_SOURCE_DIR}/script/template/rpc.json"
            "${CMAKE_SOURCE_DIR}/script/template/options.json"
            "${CMAKE_SOURCE_DIR}/script/template/mode.json"
            "${CMAKE_SOURCE_DIR}/script/generator.py"
            "${CMAKE_SOURCE_DIR}/script/auto_options.py"
            "${CMAKE_SOURCE_DIR}/script/auto_rpc.py"
            "${CMAKE_SOURCE_DIR}/script/auto_protocol.py"
)

set(src
    bigbang.cpp
    address.cpp         address.h
    blockmaker.cpp      blockmaker.h
    config.cpp          config.h
    core.cpp            core.h
    dispatcher.cpp      dispatcher.h
    entry.cpp           entry.h
    error.cpp           error.h
    miner.cpp           miner.h
    netchn.cpp          netchn.h
    delegatedchn.cpp    delegatedchn.h
    network.cpp         network.h
    rpcclient.cpp       rpcclient.h
    rpcmod.cpp          rpcmod.h
    consensus.cpp       consensus.h
    blockmaker.cpp      blockmaker.h
    rpcclient.cpp       rpcclient.h
    miner.cpp           miner.h
    schedule.cpp        schedule.h
    service.cpp         service.h
    txpool.cpp          txpool.h
    wallet.cpp          wallet.h
    blockchain.cpp      blockchain.h
    forkmanager.cpp     forkmanager.h
    datastat.cpp        datastat.h
    event.h
    base.h
    param.h
    storage.h
    version.h
)
set(mode_src
    mode/basic_config.cpp   mode/basic_config.h
    mode/fork_config.cpp    mode/fork_config.h
    mode/mint_config.cpp    mode/mint_config.h
    mode/network_config.cpp mode/network_config.h
    mode/rpc_config.cpp     mode/rpc_config.h
    mode/storage_config.cpp mode/storage_config.h
    mode/config_macro.h
    mode/config_type.h
    mode/mode.h
    mode/mode_impl.h
    mode/mode_type.h
    mode/module_type.h
)

set(sources
    ${auto_files}
    ${src}
    ${mode_src}
)

add_executable(bigbang ${sources})

include_directories(../xengine ../crypto ../common ../storage ../network ../delegate ../mpvss ../jsonrpc ./)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_BINARY_DIR}/src/jsonrpc)
include_directories(${Protobuf_INCLUDE_DIRS})

target_link_libraries(bigbang
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_DATE_TIME_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    OpenSSL::SSL
    OpenSSL::Crypto
    ${Protobuf_LIBRARIES}
    ${Readline_LIBRARY}
    xengine
    crypto    
    storage    
    network
    delegate    
    jsonrpc
    common
)

# create symbol link
set(exec_link bigbang-cli bigbang-server bigbang-miner)
foreach(link ${exec_link})
    add_custom_command(TARGET bigbang POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink bigbang ${link}
    )
endforeach()

# install to path
foreach(link bigbang ${exec_link})
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/${link}"
        DESTINATION bin
    )
endforeach()
