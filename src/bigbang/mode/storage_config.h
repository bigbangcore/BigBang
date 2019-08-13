// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_MODE_STORAGE_CONFIG_H
#define BIGBANG_MODE_STORAGE_CONFIG_H

#include <string>

#include "mode/basic_config.h"

namespace bigbang
{
class CStorageConfig : virtual public CBasicConfig, virtual public CStorageConfigOption
{
public:
    CStorageConfig();
    virtual ~CStorageConfig();
    virtual bool PostLoad();
    virtual std::string ListConfig() const;
    virtual std::string Help() const;
};

} // namespace bigbang

#endif // BIGBANG_MODE_STORAGE_CONFIG_H
