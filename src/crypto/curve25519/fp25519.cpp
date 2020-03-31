// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "fp25519.h"

#include <cstring>
#include <iostream>

#include "base25519.h"

namespace curve25519
{

const uint64_t CFP25519::prime[4] = { 0xFFFFFFFFFFFFFFED, 0xFFFFFFFFFFFFFFFF,
                                      0xFFFFFFFFFFFFFFFF, 0x7FFFFFFFFFFFFFFF };
static const uint64_t halfPrime[4] = { 0xFFFFFFFFFFFFFFF6, 0xFFFFFFFFFFFFFFFF,
                                       0xFFFFFFFFFFFFFFFF, 0x3FFFFFFFFFFFFFFF };
static const uint64_t minusOne[4] = { 0xFFFFFFFFFFFFFFEC, 0xFFFFFFFFFFFFFFFF,
                                      0xFFFFFFFFFFFFFFFF, 0x7FFFFFFFFFFFFFFF };

CFP25519::CFP25519()
{
    Zero32(value);
}

CFP25519::CFP25519(const CFP25519& fp)
{
    Copy32(value, fp.value);
}

CFP25519::CFP25519(CFP25519&& fp)
{
    Copy32(value, fp.value);
    Zero32(fp.value);
}

CFP25519& CFP25519::operator=(const CFP25519& fp)
{
    Copy32(value, fp.value);
    return *this;
}

CFP25519& CFP25519::operator=(CFP25519&& fp)
{
    Copy32(value, fp.value);
    Zero32(fp.value);
    return *this;
}

CFP25519::CFP25519(const uint64_t u64)
{
    Copy32(value, u64);
}

CFP25519::CFP25519(const uint8_t* md32)
{
    Copy32(value, md32);
    value[3] &= 0x7FFFFFFFFFFFFFFFUL;
    Reduce();
}

CFP25519::~CFP25519()
{
}

void CFP25519::Pack(uint8_t* md32) const
{
    Copy32(md32, value);

    uint64_t* u = (uint64_t*)md32;
    if (u[3] == 0x7FFFFFFFFFFFFFFFULL && u[2] == 0xFFFFFFFFFFFFFFFFULL
        && u[1] == 0xFFFFFFFFFFFFFFFFULL && u[0] >= 0xFFFFFFFFFFFFFFEDULL)
    {
        u[0] -= 0xFFFFFFFFFFFFFFEDULL;
        u[1] = u[2] = u[3] = 0;
    }
}

const CFP25519 CFP25519::Inverse() const
{
    // Binary extended gcd algorithm
    if (IsZero())
    {
        return CFP25519();
    }

    // u <- prime, v <- n, B <- 0, D <- 1
    uint64_t u[4];
    Copy32(u, prime);
    uint64_t v[4];
    Copy32(v, value);
    uint64_t B[4];
    Zero32(B);
    uint64_t D[4];
    Zero32(D);
    D[0] = 1;

    // v == 1, break
    // Reduce B and D to avoid overflow
    while (!IsOne32(v))
    {
        // while u % 2 == 0: u /= 2, B = (B % 2 == 0) ? B/2 : (B - prime)/2
        if (IsEven(u))
        {
            do
            {
                HalfSigned32(u);
                // if b % 2 == 0,
                if (IsEven(B))
                {
                    HalfSigned32(B);
                }
                else
                {
                    HalfSigned32(B);
                    Sub32(B, B, halfPrime);
                }
            } while (IsEven(u));

            ReduceSigned32(B, B, prime);
        }

        // while v % 2 == 0: v /= 2, D = (D % 2 == 0) ? D/2 : (D - prime)/2
        if (IsEven(v))
        {
            do
            {
                HalfSigned32(v);
                if (IsEven(D))
                {
                    HalfSigned32(D);
                }
                else
                {
                    HalfSigned32(D);
                    Sub32(D, D, halfPrime);
                }
            } while (IsEven(v));

            ReduceSigned32(D, D, prime);
        }

        // if u >= v: u -= v, B -= D
        // else: v -= u, D -= B
        if (CompareSigned32(u, v) >= 0)
        {
            Sub32(u, u, v);
            Sub32(B, B, D);
            ReduceSigned32(B, B, prime);
        }
        else
        {
            Sub32(v, v, u);
            Sub32(D, D, B);
            ReduceSigned32(D, D, prime);
        }
    }

    ReduceSigned32(D, D, prime);

    return CFP25519((uint8_t*)D);
}

const CFP25519 CFP25519::Power(const uint8_t* md32) const
{
    // WINDOWSIZE = 4;
    const CFP25519& x = *this;
    CFP25519 pre[16];
    pre[0] = CFP25519(1);
    pre[1] = x;
    for (int i = 2; i < 16; i += 2)
    {
        pre[i] = pre[i >> 1];
        pre[i].Square();
        pre[i + 1] = pre[i] * x;
    }

    CFP25519 r(1);
    for (int i = 31; i >= 0; i--)
    {
        r.Square();
        r.Square();
        r.Square();
        r.Square();
        r *= pre[(md32[i] >> 4)];
        r.Square();
        r.Square();
        r.Square();
        r.Square();
        r *= pre[(md32[i] & 15)];
    }

    return r;
}

const CFP25519 CFP25519::Sqrt() const
{
    const uint8_t b[32] = { 0xb0, 0xa0, 0x0e, 0x4a, 0x27, 0x1b, 0xee, 0xc4, 0x78, 0xe4, 0x2f, 0xad, 0x06, 0x18, 0x43, 0x2f,
                            0xa7, 0xd7, 0xfb, 0x3d, 0x99, 0x00, 0x4d, 0x2b, 0x0b, 0xdf, 0xc1, 0x4f, 0x80, 0x24, 0x83, 0x2b }; /* sqrt(-1) */

    if (!IsZero())
    {
        CFP25519 z58, z38, z14;
        // z58 = (value ^ (p-5)/8) % p
        z58 = Power58();
        // z38 = (value ^ (p+3)/8) % p
        z38 = z58;
        z38 *= *this;
        // z14 = (value ^ (p-1)/4) % p
        z14 = z58;
        z14 *= z38;

        // if (value ^ (p-1)/4) % p == 1, return (value ^ (p+3)/8) % p
        // if (value ^ (p-1)/4) % p == -1, return (value ^ (p+3)/8) * (2 ^ (p-1)/4) % p
        // if (z14.IsOne())
        if (IsOne32(z14.value))
        {
            return z38;
        }
        else if (Compare32(z14.value, minusOne) == 0)
        {
            return z38 * CFP25519(b);
        }
    }
    return CFP25519();
}

CFP25519& CFP25519::Square()
{
    *this *= *this;
    return *this;
}

bool CFP25519::IsZero() const
{
    return IsZero32(value);
}

uint8_t CFP25519::Parity() const
{
    bool over = (value[3] == prime[3] && value[2] == prime[2]
                 && value[1] == prime[1] && value[0] >= prime[0]);
    return ((uint8_t)((value[0] ^ over) & 1));
}

CFP25519& CFP25519::operator+=(const CFP25519& b)
{
    Add32(value, value, b.value);
    Reduce();

    return *this;
}

CFP25519& CFP25519::operator-=(const CFP25519& b)
{
    uint32_t borrow = Sub32(value, value, b.value);
    if (borrow > 0)
    {
        Add32(value, value, prime);
    }

    return *this;
}

CFP25519& CFP25519::operator*=(const CFP25519& b)
{
    __uint128_t m[8] = { 0 };
    Mul32(m, value, b.value);

    uint64_t carry = 0;
    for (int i = 0; i < 4; i++)
    {
        m[i] += carry + m[i + 4] * 38;
        carry = m[i] >> 64;
        value[i] = m[i];
    }
    Range(carry);

    return *this;
}

const CFP25519 CFP25519::operator-() const
{
    CFP25519 p;
    p -= *this;
    return p;
}

const CFP25519 CFP25519::operator+(const CFP25519& b) const
{
    return CFP25519(*this) += b;
}

const CFP25519 CFP25519::operator-(const CFP25519& b) const
{
    return CFP25519(*this) -= b;
}

const CFP25519 CFP25519::operator*(const CFP25519& b) const
{
    return CFP25519(*this) *= b;
}

const CFP25519 CFP25519::operator/(const CFP25519& b) const
{
    return CFP25519(*this) *= b.Inverse();
}

bool CFP25519::operator==(const CFP25519& b) const
{
    return Compare32(value, b.value) == 0;
}

bool CFP25519::operator!=(const CFP25519& b) const
{
    return !(*this == b);
}

void CFP25519::Range(uint32_t carry)
{
    while ((carry = ((carry << 1) + (value[3] >> 63)) * 19))
    {
        value[3] &= 0x7FFFFFFFFFFFFFFFUL;
        for (int i = 0; i < 4; i++)
        {
            uint64_t sum = value[i] + carry;
            carry = (sum >= value[i]) ? 0 : 1;
            value[i] = sum;
        }
    }
    Reduce();
}

void CFP25519::Reduce()
{
    while (Compare32(value, prime) >= 0)
    {
        Sub32(value, value, prime);
    }
}

const CFP25519 CFP25519::Power58() const
{
    // g^(2^0)
    CFP25519 g((uint8_t*)value);
    // g^(2^1)
    g.Square();
    // g^3
    CFP25519 g3 = *this * g;
    // g^(2^2) ... g^(2^252)
    for (int i = 2; i <= 252; i++)
    {
        g.Square();
    }
    // g^((prime-5)/8) = g^(2^252) / g3
    g *= g3.Inverse();

    return g;
}

} // namespace curve25519
