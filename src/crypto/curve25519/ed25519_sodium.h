// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CRYPTO_CURVE25519_ED25519_SODIUM_H
#define CRYPTO_CURVE25519_ED25519_SODIUM_H

#include <cstdint>
#include <string>
#include <vector>

#include "sc25519_sodium.h"
#include "uint256.h"

namespace curve25519
{

class CEdwards25519Sodium
{
public:
    CEdwards25519Sodium();
    CEdwards25519Sodium(const CEdwards25519Sodium& ed) = default;
    CEdwards25519Sodium(CEdwards25519Sodium&& ed) = default;
    CEdwards25519Sodium& operator=(const CEdwards25519Sodium& ed) = default;
    CEdwards25519Sodium& operator=(CEdwards25519Sodium&& ed) = default;
    ~CEdwards25519Sodium();
    void Generate(const uint256& n);
    void Generate(const CSC25519Sodium& n);
    bool Unpack(const uint8_t* md32);
    void Pack(uint8_t* md32) const;
    const CEdwards25519Sodium ScalarMult(const uint256& n) const;
    const CEdwards25519Sodium ScalarMult(const CSC25519Sodium& n) const;

    CEdwards25519Sodium& operator+=(const CEdwards25519Sodium& q);
    CEdwards25519Sodium& operator-=(const CEdwards25519Sodium& q);
    const CEdwards25519Sodium operator+(const CEdwards25519Sodium& q) const;
    const CEdwards25519Sodium operator-(const CEdwards25519Sodium& q) const;
    bool operator==(const CEdwards25519Sodium& q) const;
    bool operator!=(const CEdwards25519Sodium& q) const;

protected:
    uint8_t point[32];

public:
    bool fInit;
};

} // namespace curve25519

#endif // CRYPTO_CURVE25519_ED25519_H
