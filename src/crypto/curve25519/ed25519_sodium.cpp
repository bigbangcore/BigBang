// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ed25519_sodium.h"

#include <cstring>
#include <sodium.h>

namespace curve25519
{

CEdwards25519Sodium::CEdwards25519Sodium()
  : point{ 0 }, fInit(false)
{
}

CEdwards25519Sodium::~CEdwards25519Sodium()
{
}

void CEdwards25519Sodium::Generate(const uint256& n)
{
    Generate(CSC25519Sodium(n.begin()));
}

void CEdwards25519Sodium::Generate(const CSC25519Sodium& n)
{
    crypto_scalarmult_ed25519_base_noclamp(point, n.Begin());
    fInit = true;
}

bool CEdwards25519Sodium::Unpack(const uint8_t* md32)
{
    if (crypto_core_ed25519_is_valid_point(md32))
    {
        memmove(point, md32, 32);
        fInit = true;
        return true;
    }

    return false;
}

void CEdwards25519Sodium::Pack(uint8_t* md32) const
{
    memmove(md32, point, 32);
}

const CEdwards25519Sodium CEdwards25519Sodium::ScalarMult(const uint256& n) const
{
    CSC25519Sodium sc(n.begin());
    return ScalarMult(sc);
}

const CEdwards25519Sodium CEdwards25519Sodium::ScalarMult(const CSC25519Sodium& n) const
{
    CEdwards25519Sodium q;
    uint8_t md32[32];
    if (crypto_scalarmult_ed25519_noclamp(md32, n.Begin(), point) == 0)
    {
        q.Unpack(md32);
    }
    return q;
}

CEdwards25519Sodium& CEdwards25519Sodium::operator+=(const CEdwards25519Sodium& q)
{
    if (!fInit)
    {
        *this = q;
    }
    else
    {
        crypto_core_ed25519_add(point, point, q.point);
    }
    return *this;
}

CEdwards25519Sodium& CEdwards25519Sodium::operator-=(const CEdwards25519Sodium& q)
{
    if (!fInit)
    {
        *this = q;
    }
    else
    {
        crypto_core_ed25519_sub(point, point, q.point);
    }
    return *this;
}

const CEdwards25519Sodium CEdwards25519Sodium::operator+(const CEdwards25519Sodium& q) const
{
    return CEdwards25519Sodium(*this) += q;
}

const CEdwards25519Sodium CEdwards25519Sodium::operator-(const CEdwards25519Sodium& q) const
{
    return CEdwards25519Sodium(*this) -= q;
}

bool CEdwards25519Sodium::operator==(const CEdwards25519Sodium& q) const
{
    return memcmp(point, q.point, 32) == 0;
}

bool CEdwards25519Sodium::operator!=(const CEdwards25519Sodium& q) const
{
    return (!(*this == q));
}

} // namespace curve25519
