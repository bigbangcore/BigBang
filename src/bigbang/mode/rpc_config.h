// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_MODE_RPC_CONFIG_H
#define BIGBANG_MODE_RPC_CONFIG_H

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <string>
#include <vector>

#include "mode/basic_config.h"

namespace bigbang
{

class CRPCBasicConfig : virtual public CBasicConfig, virtual public CRPCBasicConfigOption
{
public:
    CRPCBasicConfig();
    virtual ~CRPCBasicConfig();
    virtual bool PostLoad();
    virtual std::string ListConfig() const;
    virtual std::string Help() const;

public:
    unsigned int nRPCPort;
};

class CRPCServerConfig : virtual public CRPCBasicConfig, virtual public CRPCServerConfigOption
{
public:
    CRPCServerConfig();
    virtual ~CRPCServerConfig();
    virtual bool PostLoad();
    virtual std::string ListConfig() const;
    virtual std::string Help() const;

public:
    boost::asio::ip::tcp::endpoint epRPC;
};

class CRPCClientConfig : virtual public CRPCBasicConfig, virtual public CRPCClientConfigOption
{
public:
    CRPCClientConfig();
    virtual ~CRPCClientConfig();
    virtual bool PostLoad();
    virtual std::string ListConfig() const;
    virtual std::string Help() const;
};

} // namespace bigbang

#endif // BIGBANG_MODE_RPC_CONFIG_H
