// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "address.h"

#include "base32.h"

using namespace std;
using namespace xengine;

namespace bigbang
{

//////////////////////////////
// CAddress

CAddress::CAddress(const CDestination& destIn)
  : CDestination(destIn)
{
}

CAddress::CAddress(const std::string& strAddress)
{
    ParseString(strAddress);
}

bool CAddress::ParseString(const std::string& strAddress)
{
    if (strAddress.size() != ADDRESS_LEN || strAddress[0] < '0' || strAddress[0] >= '0' + PREFIX_MAX)
    {
        return false;
    }
    if (!crypto::Base32Decode(strAddress.substr(1), data.begin()))
    {
        return false;
    }
    prefix = strAddress[0] - '0';
    return true;
}

string CAddress::ToString() const
{
    string strDataBase32;
    crypto::Base32Encode(data.begin(), strDataBase32);
    return (string(1, '0' + prefix) + strDataBase32);
}

} // namespace bigbang
