// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CRYPTO_CURVE25519_INT128_H
#define CRYPTO_CURVE25519_INT128_H

#ifdef __SIZEOF_INT128__

// GNU
using uint128_t = __uint128_t;
using int128_t = __int128_t;

#else

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

inline CUint128& operator+=(CUint128& lhs, const CUint128& rhs)
{
    lhs.l += rhs.l;
    lhs.h += rhs.h + (lhs.l > rhs.l ? 0 : 1);
    return lhs;
}

inline CUint128 operator+(const CUint128& lhs, const CUint128& rhs)
{
    return CUint128(lhs) += rhs;
}

inline CUint128 operator+(const CUint128& lhs, const uint64_t rhs)
{
    return CUint128(lhs) += CUint128(rhs);
}

inline CUint128 operator+(const CUint128& lhs, const int rhs)
{
    return CUint128(lhs) += CUint128(rhs);
}

inline CUint128& operator-=(CUint128& lhs, const CUint128& rhs)
{
    uint64_t borrow = (lhs.l >= rhs.l) ? 0 : 1;
    lhs.l -= rhs.l;
    lhs.h -= rhs.h + borrow;
    return lhs;
}

inline CUint128 operator-(const CUint128& lhs, const CUint128& rhs)
{
    return CUint128(lhs) -= rhs;
}

inline CUint128 operator-(const CUint128& lhs, const int rhs)
{
    return CUint128(lhs) -= CUint128(rhs);
}

inline CUint128& operator>>=(CUint128& i, const int bits)
{
    if (bits < 0 || bits >= 128)
    {
        i = 0;
    }
    else if (bits < 64)
    {
        i.l = (i.l >> bits) | (i.h << (64 - bits));
        i.h = i.h >> bits;
    }
    else
    {
        i.l = i.h >> (bits - 64);
        i.h = 0;
    }
    return i;
}

inline CUint128 operator>>(const CUint128& i, const int bits)
{
    return CUint128(i) >>= bits;
}

inline CUint128 operator>>(const CUint128& i, const size_t bits)
{
    return CUint128(i) >>= (int)bits;
}

inline CUint128& operator<<=(CUint128& i, const int bits)
{
    if (bits < 0 || bits >= 128)
    {
        i = 0;
    }
    else if (bits < 64)
    {
        i.h = (i.h << bits) | (i.l >> (64 - bits));
        i.l = i.l << bits;
    }
    else
    {
        i.h = i.l << (bits - 64);
        i.l = 0;
    }
    return i;
}

inline CUint128 operator<<(const CUint128& i, const int bits)
{
    return CUint128(i) <<= bits;
}

inline CUint128 operator<<(const CUint128& i, const size_t bits)
{
    return CUint128(i) <<= (int)bits;
}

inline CUint128& operator&=(CUint128& lhs, const CUint128& rhs)
{
    lhs.l &= rhs.l;
    lhs.h &= rhs.h;
    return lhs;
}

inline CUint128 operator&(const CUint128& lhs, const CUint128& rhs)
{
    return CUint128(lhs) &= rhs;
}

inline CUint128 operator&(const CUint128& lhs, const uint64_t rhs)
{
    return CUint128(lhs) &= CUint128(rhs);
}

inline CUint128& operator|=(CUint128& lhs, const CUint128& rhs)
{
    lhs.l |= rhs.l;
    lhs.h |= rhs.h;
    return lhs;
}

inline CUint128 operator|(const CUint128& lhs, const CUint128& rhs)
{
    return CUint128(lhs) |= rhs;
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

inline CInt128& operator>>=(CInt128& i, const int bits)
{
    if (bits < 0 || bits >= 128)
    {
        i.h >>= 63;
        i.l = i.h;
    }
    else if (bits < 64)
    {
        i.l = (i.l >> bits) | (i.h << (64 - bits));
        i.h = i.h >> bits;
    }
    else
    {
        i.l = i.h >> (bits - 64);
        i.h >>= 63;
    }
    return i;
}

inline CInt128 operator>>(const CInt128& i, const int bits)
{
    return CInt128(i) >>= bits;
}

#define uint128_t CUint128
#define int128_t CInt128

#endif

#endif //CRYPTO_CURVE25519_INT128_H