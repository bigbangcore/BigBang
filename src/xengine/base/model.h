// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XENGINE_BASE_MODEL_H
#define XENGINE_BASE_MODEL_H

#include <string>

#include "base/base.h"

namespace xengine
{

class IModel : public IBase
{
public:
    IModel(const std::string& strOwnKeyIn = "")
      : IBase(strOwnKeyIn) {}
};

} // namespace xengine

#endif // XENGINE_BASE_MODEL_H
