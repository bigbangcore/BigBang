// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CRYPTO_CURVE25519_SC25519_SODIUM_H
#define CRYPTO_CURVE25519_SC25519_SODIUM_H

#include <cstdint>
#include <initializer_list>

namespace curve25519
{

class CSC25519Sodium
{
public:
    static const CSC25519Sodium naturalPowTable[51][26];

public:
    CSC25519Sodium();
    CSC25519Sodium(const CSC25519Sodium& sc) = default;
    CSC25519Sodium(CSC25519Sodium&& sc) = default;
    CSC25519Sodium& operator=(const CSC25519Sodium& sc) = default;
    CSC25519Sodium& operator=(CSC25519Sodium&& sc) = default;
    CSC25519Sodium(const uint64_t u64);
    CSC25519Sodium(const uint8_t* md32);
    CSC25519Sodium(std::initializer_list<uint64_t> list);
    virtual ~CSC25519Sodium();
    void Unpack(const uint8_t* md32);
    void Pack(uint8_t* md32) const;
    const uint64_t* Data() const;
    const uint8_t* Begin() const;

    CSC25519Sodium& Negative();
    const CSC25519Sodium operator-() const;
    CSC25519Sodium& operator+=(const CSC25519Sodium& b);
    CSC25519Sodium& operator-=(const CSC25519Sodium& b);
    CSC25519Sodium& operator*=(const CSC25519Sodium& b);
    CSC25519Sodium& operator*=(const uint32_t& n);
    const CSC25519Sodium operator+(const CSC25519Sodium& b) const;
    const CSC25519Sodium operator-(const CSC25519Sodium& b) const;
    const CSC25519Sodium operator*(const CSC25519Sodium& b) const;
    const CSC25519Sodium operator*(const uint32_t& n) const;
    bool operator==(const CSC25519Sodium& b) const;
    bool operator!=(const CSC25519Sodium& b) const;

protected:
    void Reduce();

protected:
    uint8_t value[64];
};

} // namespace curve25519

#endif // CRYPTO_CURVE25519_SC25519_H
