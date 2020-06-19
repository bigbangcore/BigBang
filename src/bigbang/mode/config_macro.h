// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_MODE_CONFIG_MACRO_H
#define BIGBANG_MODE_CONFIG_MACRO_H

#include <boost/program_options.hpp>

#include "defs.h"

// basic config
#define MAGICNUM_MAINNET 0x3b54beae
#define MAGICNUM_TESTNET 0xa006c295
#define MAGICNUM SWITCH_PARAM(MAGICNUM_MAINNET, MAGICNUM_TESTNET)

// rpc config
#define DEFAULT_RPCPORT_MAINNET 9902
#define DEFAULT_RPCPORT_TESTNET 9904
#define DEFAULT_RPCPORT SWITCH_PARAM(DEFAULT_RPCPORT_MAINNET, DEFAULT_RPCPORT_TESTNET)

#define DEFAULT_RPC_MAX_CONNECTIONS 5
#define DEFAULT_RPC_CONNECT_TIMEOUT 600 //120

// network config
#define DEFAULT_P2PPORT_MAINNET 9901
#define DEFAULT_P2PPORT_TESTNET 9903
#define DEFAULT_P2PPORT SWITCH_PARAM(DEFAULT_P2PPORT_MAINNET, DEFAULT_P2PPORT_TESTNET)

#define DEFAULT_DNSEED_PORT 9906
#define DEFAULT_MAX_INBOUNDS 125
#define DEFAULT_MAX_OUTBOUNDS 10
#define DEFAULT_CONNECT_TIMEOUT 5

// storage config
#define DEFAULT_DB_CONNECTION 8

// add options
template <typename T>
inline void AddOpt(boost::program_options::options_description& desc,
                   const std::string& name, T& var)
{
    desc.add_options()(name.c_str(), boost::program_options::value<T>(&var));
}
template <typename T>
inline void AddOpt(boost::program_options::options_description& desc,
                   const std::string& name, T& var, const T& def)
{
    desc.add_options()(name.c_str(), boost::program_options::value<T>(&var)->default_value(def));
}
template <typename T>
inline void AddOpt(boost::program_options::options_description& desc,
                   const std::string& name)
{
    desc.add_options()(name.c_str(), boost::program_options::value<T>());
}
template <typename T>
inline void AddOpt(boost::program_options::options_description& desc,
                   const std::string& name, const T& def)
{
    desc.add_options()(name.c_str(), boost::program_options::value<T>()->default_value(def));
}

#endif // BIGBANG_MODE_CONFIG_MACRO_H
