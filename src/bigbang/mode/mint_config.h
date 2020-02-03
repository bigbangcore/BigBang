// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_MODE_MINT_CONFIG_H
#define BIGBANG_MODE_MINT_CONFIG_H

#include <string>

#include "destination.h"
#include "mode/basic_config.h"
#include "uint256.h"

namespace bigbang
{
class CMintConfig : virtual public CBasicConfig, virtual public CMintConfigOption
{
public:
    CMintConfig();
    virtual ~CMintConfig();
    virtual bool PostLoad();
    virtual std::string ListConfig() const;
    virtual std::string Help() const;

protected:
    void ExtractMintParamPair(const std::string& strAddress,
                              const std::string& strKey, CDestination& dest,
                              uint256& privkey);

public:
    CDestination destMpvss;
    uint256 keyMpvss;
    CDestination destCryptonight;
    uint256 keyCryptonight;
};

} // namespace bigbang

#endif // BIGBANG_MODE_MINT_CONFIG_H
