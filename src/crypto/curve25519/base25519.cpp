// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base25519.h"

#include <cstdint>
#include <stdio.h>

namespace curve25519
{

void Print(const void* p, size_t size, const char* prefix = "")
{
    uint8_t* q = (uint8_t*)p;
    printf("%s : ", prefix);
    for (int i = size - 1; i >= 0; i--)
    {
        printf("%02x", q[i]);
    }
    printf("\n");
}

void Print32(const void* p, const char* prefix)
{
    Print(p, 32, prefix);
}

void Zero32(void* b)
{
    uint64_t* p = (uint64_t*)b;
    p[0] = 0;
    p[1] = 0;
    p[2] = 0;
    p[3] = 0;
}

void Zero64(void* b)
{
    uint64_t* p = (uint64_t*)b;
    p[0] = 0;
    p[1] = 0;
    p[2] = 0;
    p[3] = 0;
    p[4] = 0;
    p[5] = 0;
    p[6] = 0;
    p[7] = 0;
}

void Copy32(void* dest, const void* src)
{
    uint64_t* d = (uint64_t*)dest;
    const uint64_t* s = (const uint64_t*)src;
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
    d[3] = s[3];
}

void Copy32(void* dest, const uint64_t u64)
{
    uint64_t* d = (uint64_t*)dest;
    d[0] = u64;
    d[1] = 0;
    d[2] = 0;
    d[3] = 0;
}

int Compare32(const void* lhs, const void* rhs)
{
    uint128_t* pl = (uint128_t*)lhs;
    uint128_t* pr = (uint128_t*)rhs;
    if (pl[1] > pr[1])
    {
        return 1;
    }
    else if (pl[1] < pr[1])
    {
        return -1;
    }
    if (pl[0] > pr[0])
    {
        return 1;
    }
    else if (pl[0] < pr[0])
    {
        return -1;
    }
    return 0;
}

int CompareSigned32(const void* lhs, const void* rhs)
{
    int128_t* pl = (int128_t*)lhs;
    int128_t* pr = (int128_t*)rhs;
    if (pl[1] > pr[1])
    {
        return 1;
    }
    else if (pl[1] < pr[1])
    {
        return -1;
    }
    if ((uint128_t)pl[0] > (uint128_t)pr[0])
    {
        return 1;
    }
    else if ((uint128_t)pl[0] < (uint128_t)pr[0])
    {
        return -1;
    }
    return 0;
}

void ReduceSigned32(void* output, const void* n, const void* prime)
{
    if (((uint8_t*)n)[31] & 0x80)
    {
        Add32(output, n, prime);
    }
    else if (Compare32(n, prime) >= 0)
    {
        Sub32(output, n, prime);
    }
}

bool IsZero32(const void* n)
{
    uint64_t* p = (uint64_t*)n;
    return !(p[0] | p[1] | p[2] | p[3]);
}

bool IsOne32(const void* n)
{
    uint64_t* p = (uint64_t*)n;
    return !((p[0] - 1) | p[1] | p[2] | p[3]);
}

bool IsEven(const void* n)
{
    return !(((uint8_t*)n)[0] & 1);
}

void Half32(void* n)
{
    uint128_t* p = (uint128_t*)n;
    p[0] = (p[0] >> 1) | (p[1] << 127);
    p[1] >>= 1;
}

void HalfSigned32(void* n)
{
    int128_t* p = (int128_t*)n;
    p[0] = ((uint128_t)p[0] >> 1) | ((uint128_t)p[1] << 127);
    p[1] >>= 1;
}

void ShiftLeft32(void* output, const void* n, size_t b)
{
    uint128_t* p = (uint128_t*)n;
    if (b < 128)
    {
        uint128_t d[2];

        d[1] = (p[1] << b) | (p[0] >> (128 - b));
        d[0] = p[0] << b;
        ((uint128_t*)output)[1] = d[1];
        ((uint128_t*)output)[0] = d[0];

        d[0] = 0;
        d[1] = 0;
    }
    else if (b < 256)
    {
        uint128_t d = (p[0] >> (b - 128));

        ((uint128_t*)output)[1] = d;
        ((uint128_t*)output)[0] = 0;

        d = 0;
    }
    else
    {
        Zero32(output);
    }
}

uint32_t Add32(void* output, const void* n1, const void* n2)
{
    uint128_t* po = (uint128_t*)output;
    const uint128_t* p1 = (const uint128_t*)n1;
    const uint128_t* p2 = (const uint128_t*)n2;

    uint128_t sum[2];
    sum[0] = p1[0] + p2[0];
    sum[1] = p1[1] + p2[1];

    uint32_t carry = sum[0] >= p1[0] ? 0 : 1;
    if (carry)
    {
        sum[1] += carry;
        carry = sum[1] > p1[1] ? 0 : 1;
    }
    else
    {
        carry = sum[1] >= p1[1] ? 0 : 1;
    }

    po[0] = sum[0];
    po[1] = sum[1];

    sum[0] = 0;
    sum[1] = 0;

    return carry;
}

uint32_t Sub32(void* output, const void* n1, const void* n2)
{
    uint128_t* po = (uint128_t*)output;
    const uint128_t* p1 = (const uint128_t*)n1;
    const uint128_t* p2 = (const uint128_t*)n2;

    uint128_t diff[2];
    diff[0] = p1[0] - p2[0];
    diff[1] = p1[1] - p2[1];

    uint32_t borrow = p1[0] >= p2[0] ? 0 : 1;
    if (borrow)
    {
        diff[1] -= borrow;
        borrow = p1[1] > p2[1] ? 0 : 1;
    }
    else
    {
        borrow = p1[1] >= p2[1] ? 0 : 1;
    }

    po[0] = diff[0];
    po[1] = diff[1];

    diff[0] = 0;
    diff[1] = 0;

    return borrow;
}

void Mul32(void* output, const void* n1, const void* n2)
{
    uint128_t* po = (uint128_t*)output;
    const uint64_t* p1 = (const uint64_t*)n1;
    const uint64_t* p2 = (const uint64_t*)n2;

    uint128_t product[16];
    // 1,0
    product[0] = (uint128_t)p1[0] * p2[0];
    // 2,1
    product[1] = (uint128_t)p1[0] * p2[1];
    product[2] = (uint128_t)p1[1] * p2[0];
    // 3,2
    product[3] = (uint128_t)p1[0] * p2[2];
    product[4] = (uint128_t)p1[1] * p2[1];
    product[5] = (uint128_t)p1[2] * p2[0];
    // 4,3
    product[6] = (uint128_t)p1[0] * p2[3];
    product[7] = (uint128_t)p1[1] * p2[2];
    product[8] = (uint128_t)p1[2] * p2[1];
    product[9] = (uint128_t)p1[3] * p2[0];
    // 5,4
    product[10] = (uint128_t)p1[1] * p2[3];
    product[11] = (uint128_t)p1[2] * p2[2];
    product[12] = (uint128_t)p1[3] * p2[1];
    // 6,5
    product[13] = (uint128_t)p1[2] * p2[3];
    product[14] = (uint128_t)p1[3] * p2[2];
    // 7,6
    product[15] = (uint128_t)p1[3] * p2[3];

    uint64_t* p = (uint64_t*)product;
    po[0] = (uint128_t)p[0];
    po[1] = (uint128_t)p[1] + p[2] + p[4];
    po[2] = (uint128_t)p[3] + p[5] + p[6] + p[8] + p[10];
    po[3] = (uint128_t)p[7] + p[9] + p[11] + p[12] + p[14] + p[16] + p[18];
    po[4] = (uint128_t)p[13] + p[15] + p[17] + p[19] + p[20] + p[22] + p[24];
    po[5] = (uint128_t)p[21] + p[23] + p[25] + p[26] + p[28];
    po[6] = (uint128_t)p[27] + p[29] + p[30];
    po[7] = (uint128_t)p[31];

    product[0] = 0;
    product[1] = 0;
    product[2] = 0;
    product[3] = 0;
    product[4] = 0;
    product[5] = 0;
    product[6] = 0;
    product[7] = 0;
    product[8] = 0;
    product[9] = 0;
    product[10] = 0;
    product[11] = 0;
    product[12] = 0;
    product[13] = 0;
    product[14] = 0;
    product[15] = 0;
}

} // namespace curve25519
