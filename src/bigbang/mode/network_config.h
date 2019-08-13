// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_MODE_NETWORK_CONFIG_H
#define BIGBANG_MODE_NETWORK_CONFIG_H

#include <string>
#include <vector>

#include "mode/basic_config.h"

namespace bigbang
{
class CNetworkConfig : virtual public CBasicConfig, virtual public CNetworkConfigOption
{
public:
    CNetworkConfig();
    virtual ~CNetworkConfig();
    virtual bool PostLoad();
    virtual std::string ListConfig() const;
    virtual std::string Help() const;

public:
    unsigned short nPort;
    unsigned int nMaxInBounds;
    unsigned int nMaxOutBounds;
};

} // namespace bigbang

#endif // BIGBANG_MODE_NETWORK_CONFIG_H
