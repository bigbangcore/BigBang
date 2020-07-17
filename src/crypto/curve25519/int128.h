// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CRYPTO_CURVE25519_INT128_H
#define CRYPTO_CURVE25519_INT128_H

#include <algorithm>

// little-endian uint128_t
struct CUint128
{
    uint64_t l;
    uint64_t h;

public:
    CUint128() {}

    CUint128(const int i)
      : l(i), h(0)
    {
    }

    CUint128(const CUint128& i)
      : l(i.l), h(i.h)
    {
    }

    CUint128(const uint32_t i)
      : l(i), h(0)
    {
    }

    CUint128(const uint64_t i)
      : l(i), h(0)
    {
    }

    CUint128(const uint64_t l, const uint64_t h)
      : l(l), h(h)
    {
    }

    CUint128& operator=(const uint64_t i)
    {
        l = i;
        h = 0;
        return *this;
    }

    inline operator uint64_t()
    {
        return l;
    }

    CUint128& operator+=(const CUint128& n)
    {
        l += n.l;
        h += n.h + (l >= n.l ? 0 : 1);
        return *this;
    }

    CUint128& operator-=(const CUint128& n)
    {
        uint64_t borrow = (l >= n.l) ? 0 : 1;
        l -= n.l;
        h -= n.h + borrow;
        return *this;
    }

    // ignore overflow
    CUint128& operator*=(const uint64_t n)
    {
        uint32_t* p1 = (uint32_t*)this;
        const uint32_t* p2 = (const uint32_t*)&n;
        uint64_t product[7];
        product[0] = (uint64_t)p1[0] * p2[0];
        product[1] = (uint64_t)p1[0] * p2[1];
        product[2] = (uint64_t)p1[1] * p2[0];
        product[3] = (uint64_t)p1[1] * p2[1];
        product[4] = (uint64_t)p1[2] * p2[0];
        product[5] = (uint64_t)p1[2] * p2[1];
        product[6] = (uint64_t)p1[3] * p2[0];

        l = 0;
        h = 0;
        uint32_t* p = (uint32_t*)product;
        p1[0] = p[0];
        *(uint64_t*)(p1 + 1) = (uint64_t)p[1] + p[2] + p[4];
        *(uint64_t*)(p1 + 2) += (uint64_t)p[3] + p[5] + p[6] + p[8];
        p1[3] += p[7] + p[9] + p[10] + p[12];

        return *this;
    }

    // ignore overflow
    CUint128& operator*=(const uint32_t n)
    {
        uint32_t* p1 = (uint32_t*)this;
        uint64_t product[4];
        product[0] = (uint64_t)p1[0] * n;
        product[1] = (uint64_t)p1[1] * n;
        product[2] = (uint64_t)p1[2] * n;
        product[3] = (uint64_t)p1[3] * n;

        l = 0;
        h = 0;
        uint32_t* p = (uint32_t*)product;
        p1[0] = p[0];
        *(uint64_t*)(p1 + 1) = (uint64_t)p[1] + p[2];
        *(uint64_t*)(p1 + 2) += (uint64_t)p[3] + p[4];
        p1[3] += p[5] + p[6];

        return *this;
    }

    CUint128& operator>>=(const int bits)
    {
        if (bits < 0 || bits >= 128)
        {
            l = 0;
            h = 0;
        }
        else if (bits < 64)
        {
            l = (l >> bits) | (h << (64 - bits));
            h = h >> bits;
        }
        else
        {
            l = h >> (bits - 64);
            h = 0;
        }
        return *this;
    }

    CUint128& operator<<=(const int bits)
    {
        if (bits < 0 || bits >= 128)
        {
            l = 0;
            h = 0;
        }
        else if (bits < 64)
        {
            h = (h << bits) | (l >> (64 - bits));
            l = l << bits;
        }
        else
        {
            h = l << (bits - 64);
            l = 0;
        }
        return *this;
    }

    CUint128& operator&=(const CUint128& n)
    {
        l &= n.l;
        h &= n.h;
        return *this;
    }

    CUint128& operator|=(const CUint128& n)
    {
        l |= n.l;
        h |= n.h;
        return *this;
    }
};

// little-endian int128_t
struct CInt128
{
    uint64_t l;
    int64_t h;

public:
    CInt128() {}

    CInt128(const int i)
      : l(i & 0x7FFFFFFF), h((int64_t)i & 0x8000000000000000)
    {
    }

    CInt128(const CInt128& i)
      : l(i.l), h(i.h)
    {
    }

    CInt128(const uint64_t i)
      : l(i), h(0)
    {
    }

    CInt128(const CUint128& i)
      : l(i.l), h(i.h)
    {
    }

    inline operator CUint128()
    {
        return CUint128(l, (uint64_t)h);
    }

    CInt128& operator>>=(const int bits)
    {
        if (bits < 0 || bits >= 128)
        {
            h >>= 63;
            l = h;
        }
        else if (bits < 64)
        {
            l = (l >> bits) | (h << (64 - bits));
            h = h >> bits;
        }
        else
        {
            l = h >> (bits - 64);
            h >>= 63;
        }
        return *this;
    }
};

// uint128_t functions
inline bool operator>(const CUint128& lhs, const CUint128& rhs)
{
    return (lhs.h > rhs.h) || (lhs.h == rhs.h && lhs.l > rhs.l);
}

inline bool operator<(const CUint128& lhs, const CUint128& rhs)
{
    return (lhs.h < rhs.h) || (lhs.h == rhs.h && lhs.l < rhs.l);
}

inline bool operator>=(const CUint128& lhs, const CUint128& rhs)
{
    return !(lhs < rhs);
}

inline bool operator<=(const CUint128& lhs, const CUint128& rhs)
{
    return !(lhs > rhs);
}

inline CUint128 operator+(const CUint128& lhs, const CUint128& rhs)
{
    return CUint128(lhs) += rhs;
}

inline CUint128 operator+(CUint128&& lhs, const CUint128& rhs)
{
    return lhs += rhs;
}

inline CUint128 operator+(const CUint128& lhs, const uint64_t rhs)
{
    return CUint128(lhs) += CUint128(rhs);
}

inline CUint128 operator+(CUint128&& lhs, const uint64_t rhs)
{
    return lhs += CUint128(rhs);
}

inline CUint128 operator+(const CUint128& lhs, const int rhs)
{
    return CUint128(lhs) += CUint128(rhs);
}

inline CUint128 operator+(CUint128&& lhs, const int rhs)
{
    return lhs += CUint128(rhs);
}

inline CUint128 operator-(const CUint128& lhs, const CUint128& rhs)
{
    return CUint128(lhs) -= rhs;
}

inline CUint128 operator-(CUint128&& lhs, const CUint128& rhs)
{
    return lhs -= rhs;
}

inline CUint128 operator-(const CUint128& lhs, const int rhs)
{
    return CUint128(lhs) -= CUint128(rhs);
}

inline CUint128 operator-(CUint128&& lhs, const int rhs)
{
    return lhs -= CUint128(rhs);
}

inline CUint128 operator*(const CUint128& lhs, const uint64_t rhs)
{
    return CUint128(lhs) *= rhs;
}

inline CUint128 operator*(CUint128&& lhs, const uint64_t rhs)
{
    return lhs *= rhs;
}

inline CUint128 operator*(const CUint128& lhs, const uint32_t rhs)
{
    return CUint128(lhs) *= rhs;
}

inline CUint128 operator*(CUint128&& lhs, const uint32_t rhs)
{
    return lhs *= rhs;
}

inline CUint128 operator*(const CUint128& lhs, const int rhs)
{
    return CUint128(lhs) *= (uint32_t)rhs;
}

inline CUint128 operator*(CUint128&& lhs, const int rhs)
{
    return lhs *= (uint32_t)rhs;
}

inline CUint128 operator>>(const CUint128& i, const int bits)
{
    return CUint128(i) >>= bits;
}

inline CUint128 operator>>(CUint128&& i, const int bits)
{
    return i >>= bits;
}

inline CUint128 operator>>(const CUint128& i, const size_t bits)
{
    return CUint128(i) >>= (int)bits;
}

inline CUint128 operator>>(CUint128&& i, const size_t bits)
{
    return i >>= (int)bits;
}

inline CUint128 operator<<(const CUint128& i, const int bits)
{
    return CUint128(i) <<= bits;
}

inline CUint128 operator<<(CUint128&& i, const int bits)
{
    return i <<= bits;
}

inline CUint128 operator<<(const CUint128& i, const size_t bits)
{
    return CUint128(i) <<= (int)bits;
}

inline CUint128 operator<<(CUint128&& i, const size_t bits)
{
    return i <<= (int)bits;
}

inline CUint128 operator&(const CUint128& lhs, const CUint128& rhs)
{
    return CUint128(lhs) &= rhs;
}

inline CUint128 operator&(CUint128&& lhs, const CUint128& rhs)
{
    return lhs &= rhs;
}

inline CUint128 operator&(const CUint128& lhs, const uint64_t rhs)
{
    return CUint128(lhs) &= CUint128(rhs);
}

inline CUint128 operator&(CUint128&& lhs, const uint64_t rhs)
{
    return lhs &= CUint128(rhs);
}

inline CUint128 operator|(const CUint128& lhs, const CUint128& rhs)
{
    return CUint128(lhs) |= rhs;
}

inline CUint128 operator|(CUint128&& lhs, const CUint128& rhs)
{
    return lhs |= rhs;
}

// int128_t functions
inline bool operator>(const CInt128& lhs, const CInt128& rhs)
{
    return (lhs.h > rhs.h) || (lhs.h == rhs.h && lhs.l > rhs.l);
}

inline bool operator<(const CInt128& lhs, const CInt128& rhs)
{
    return (lhs.h < rhs.h) || (lhs.h == rhs.h && lhs.l < rhs.l);
}

inline CInt128 operator>>(const CInt128& i, const int bits)
{
    return CInt128(i) >>= bits;
}

inline CInt128 operator>>(CInt128&& i, const int bits)
{
    return i >>= bits;
}

#ifdef __SIZEOF_INT128__

using uint128_t = __uint128_t;
using int128_t = __int128_t;

// inline uint64_t Uint128ToUint64(const uint128_t& n)
// {
//     return (uint64_t)n;
// }

// inline uint32_t Uint128ToUint32(const uint128_t& n)
// {
//     return (uint32_t)n;
// }

// inline int Uint128ToInt(const uint128_t& n)
// {
//     return (int)n;
// }

#else

// #include <boost/multiprecision/cpp_int.hpp>

// using uint128_t = boost::multiprecision::uint128_t;
// using int128_t = boost::multiprecision::int128_t;

// inline uint64_t Uint128ToUint64(const uint128_t& n)
// {
//     return n.convert_to<uint64_t>();
// }

// inline uint32_t Uint128ToUint32(const uint128_t& n)
// {
//     return n.convert_to<uint32_t>();
// }

// inline int Uint128ToInt(const uint128_t& n)
// {
//     return n.convert_to<int>();
// }

#endif

#endif //CRYPTO_CURVE25519_INT128_H