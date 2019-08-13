// Copyright (c) 2019 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CRYPTO_CURVE25519_ED25519_H
#define CRYPTO_CURVE25519_ED25519_H

#include <cstdint>
#include <string>
#include <vector>

#include "fp25519.h"
#include "sc25519.h"

namespace curve25519
{

class CEdwards25519
{
public:
    CEdwards25519();
    CEdwards25519(const CEdwards25519& ed) = default;
    CEdwards25519(CEdwards25519&& ed) = default;
    CEdwards25519& operator=(const CEdwards25519& ed) = default;
    CEdwards25519& operator=(CEdwards25519&& ed) = default;
    CEdwards25519(const CFP25519& x, const CFP25519& y, bool fPrescalar = false);
    CEdwards25519(const CFP25519& x, const CFP25519& y, const CFP25519& z);
    CEdwards25519(const CFP25519& x, const CFP25519& y, const CFP25519& z, const CFP25519& t);
    ~CEdwards25519();
    template <typename T>
    void Generate(const T& t)
    {
        *this = base.ScalarMult(t);
    }
    bool Unpack(const uint8_t* md32);
    void Pack(uint8_t* md32) const;
    const CEdwards25519 ScalarMult(const uint8_t* u8, std::size_t size) const;
    template <typename T>
    const CEdwards25519 ScalarMult(const T& t) const
    {
        return ScalarMult((const uint8_t*)&t, sizeof(T));
    }
    const CEdwards25519 ScalarMult(const CSC25519& s) const
    {
        return ScalarMult((const uint8_t*)s.Data(), 32);
    }
    const CEdwards25519 operator-() const
    {
        return CEdwards25519(-fX, fY, fZ, -fT);
    }
    CEdwards25519& operator+=(const CEdwards25519& q)
    {
        return Add(q);
    }
    CEdwards25519& operator-=(const CEdwards25519& q)
    {
        return Add(-q);
    }
    const CEdwards25519 operator+(const CEdwards25519& q) const
    {
        return (CEdwards25519(*this).Add(q));
    }
    const CEdwards25519 operator-(const CEdwards25519& q) const
    {
        return (CEdwards25519(*this).Add(-q));
    }
    bool operator==(const CEdwards25519& q) const
    {
        return (fX * q.fZ == q.fX * fZ && fY * q.fZ == q.fY * fZ);
    }
    bool operator!=(const CEdwards25519& q) const
    {
        return (!(*this == q));
    }

protected:
    CEdwards25519& Add(const CEdwards25519& q);
    CEdwards25519& Double();
    void FromP1P1(const CFP25519& x, const CFP25519& y, const CFP25519& z, const CFP25519& t);
    void CalcPrescalar() const;
    void AddPrescalar(const CEdwards25519& q);

public:
    CFP25519 fX;
    CFP25519 fY;
    CFP25519 fZ;
    CFP25519 fT;

protected:
    mutable std::vector<CEdwards25519> preScalar;
    static const CFP25519 ecd;
    static const CFP25519 ecd2;
    static const CEdwards25519 base;
};

} // namespace curve25519

#endif // CRYPTO_CURVE25519_ED25519_H
