// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "templateid.h"

//////////////////////////////
// CTemplateId

CTemplateId::CTemplateId() {}

CTemplateId::CTemplateId(const uint256& data)
  : uint256(data)
{
}

CTemplateId::CTemplateId(const uint16 type, const uint256& hash)
  : uint256((hash << 16) | (uint64)(type))
{
}

uint16 CTemplateId::GetType() const
{
    return (Get32() & 0xFFFF);
}

CTemplateId& CTemplateId::operator=(uint64 b)
{
    *((uint256*)this) = b;
    return *this;
}
