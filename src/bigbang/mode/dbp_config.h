// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_DBP_CONFIG_H
#define BIGBANG_DBP_CONFIG_H

#include "mode/basic_config.h"
#include "mode/auto_options.h"

namespace bigbang
{
class CBbDbpBasicConfig : virtual public CBasicConfig,
                          virtual public CBbDbpBasicConfigOption
{
public:
    CBbDbpBasicConfig();
    virtual ~CBbDbpBasicConfig();
    virtual bool PostLoad();
    virtual std::string ListConfig() const;
    virtual std::string Help() const;

public:
    unsigned int nDbpPort;
};

class CBbDbpServerConfig : virtual public CBbDbpBasicConfig,
                           virtual public CBbDbpServerConfigOption
{
public:
    CBbDbpServerConfig();
    virtual ~CBbDbpServerConfig();
    virtual bool PostLoad();
    virtual std::string ListConfig() const;
    virtual std::string Help() const;

public:
    boost::asio::ip::tcp::endpoint epDbp;
};

class CBbDbpClientConfig : virtual public CBbDbpBasicConfig,
                           virtual public CBbDbpClientConfigOption
{
public:
    CBbDbpClientConfig();
    virtual ~CBbDbpClientConfig();
    virtual bool PostLoad();
    virtual std::string ListConfig() const;
    virtual std::string Help() const;
public:
    boost::asio::ip::tcp::endpoint epParentHost;
};
} // namespace bigbang

#endif  // BIGBANG_DBP_CONFIG_H
