// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BIGBANG_ADDRESS_H
#define BIGBANG_ADDRESS_H

#include "destination.h"

namespace bigbang
{

class CAddress : public CDestination
{
public:
    enum
    {
        ADDRESS_LEN = 57
    };

    CAddress(const CDestination& destIn = CDestination());
    CAddress(const std::string& strAddress);
    virtual ~CAddress() = default;

    virtual bool ParseString(const std::string& strAddress);
    virtual std::string ToString() const;
};

} // namespace bigbang

#endif //CBIGBANG_ADDRESS_H
