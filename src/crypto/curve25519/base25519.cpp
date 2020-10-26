// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base25519.h"

#include <cstdint>
#include <stdio.h>

namespace curve25519
{

void Print(const void* p, size_t size, const char* prefix)
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

void Zero16(void* b)
{
    volatile uint64_t* p = (volatile uint64_t*)b;
    p[0] = 0;
    p[1] = 0;
}

void Zero32(void* b)
{
    volatile uint64_t* p = (volatile uint64_t*)b;
    p[0] = 0;
    p[1] = 0;
    p[2] = 0;
    p[3] = 0;
}

void Zero64(void* b)
{
    volatile uint64_t* p = (volatile uint64_t*)b;
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
#ifdef DEFINED_INT128
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
#else
    uint64_t* pl = (uint64_t*)lhs;
    uint64_t* pr = (uint64_t*)rhs;
    if (pl[3] > pr[3])
    {
        return 1;
    }
    else if (pl[3] < pr[3])
    {
        return -1;
    }
    if (pl[2] > pr[2])
    {
        return 1;
    }
    else if (pl[2] < pr[2])
    {
        return -1;
    }
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
#endif
}

int CompareSigned32(const void* lhs, const void* rhs)
{
#ifdef DEFINED_INT128
    uint128_t* pl = (uint128_t*)lhs;
    uint128_t* pr = (uint128_t*)rhs;
    if ((int128_t)pl[1] > (int128_t)pr[1])
    {
        return 1;
    }
    else if ((int128_t)pl[1] < (int128_t)pr[1])
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
#else
    uint64_t* pl = (uint64_t*)lhs;
    uint64_t* pr = (uint64_t*)rhs;
    if ((int64_t)pl[3] > (int64_t)pr[3])
    {
        return 1;
    }
    else if ((int64_t)pl[3] < (int64_t)pr[3])
    {
        return -1;
    }
    if (pl[2] > pr[2])
    {
        return 1;
    }
    else if (pl[2] < pr[2])
    {
        return -1;
    }
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
#endif
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
#ifdef DEFINED_INT128
    uint128_t* p = (uint128_t*)n;
    p[0] = (p[0] >> 1) | (p[1] << 127);
    p[1] >>= 1;
#else
    uint64_t* p = (uint64_t*)n;
    p[0] = (p[0] >> 1) | (p[1] << 63);
    p[1] = (p[1] >> 1) | (p[2] << 63);
    p[2] = (p[2] >> 1) | (p[3] << 63);
    p[3] >>= 1;
#endif
}

void HalfSigned32(void* n)
{
#ifdef DEFINED_INT128
    uint128_t* p = (uint128_t*)n;
    p[0] = (p[0] >> 1) | (p[1] << 127);
    p[1] = (int128_t)p[1] >> 1;
#else
    uint64_t* p = (uint64_t*)n;
    p[0] = (p[0] >> 1) | (p[1] << 63);
    p[1] = (p[1] >> 1) | (p[2] << 63);
    p[2] = (p[2] >> 1) | (p[3] << 63);
    p[3] = (int64_t)p[3] >> 1;
#endif
}

void ShiftLeft32(void* output, const void* n, size_t b)
{
#ifdef DEFINED_INT128
    uint128_t* p = (uint128_t*)n;
    uint128_t* po = (uint128_t*)output;

    if (b == 0)
    {
        Copy32(output, n);
    }
    else if (b >= 256)
    {
        Zero32(output);
    }
    else
    {
        uint128_t d[2];
        if (b < 128)
        {
            d[1] = (p[1] << b) | (p[0] >> (128 - b));
            d[0] = p[0] << b;
        }
        else if (b == 128)
        {
            d[1] = p[0];
            d[0] = 0;
        }
        else
        {
            d[1] = (p[0] << (b - 128));
            d[0] = 0;
        }

        po[1] = d[1];
        po[0] = d[0];

        d[1] = 0;
        d[0] = 0;
    }
#else
    uint64_t* p = (uint64_t*)n;
    uint64_t* po = (uint64_t*)output;

    if (b == 0)
    {
        Copy32(output, n);
    }
    else if (b >= 256)
    {
        Zero32(output);
    }
    else
    {
        uint64_t d[4];
        size_t left = b % 64;
        size_t right = 64 - left;
        if (b < 64)
        {
            d[3] = (p[3] << left) | (p[2] >> right);
            d[2] = (p[2] << left) | (p[1] >> right);
            d[1] = (p[1] << left) | (p[0] >> right);
            d[0] = p[0] << left;
        }
        else if (b == 64)
        {
            d[3] = p[2];
            d[2] = p[1];
            d[1] = p[0];
            d[0] = 0;
        }
        else if (b < 128)
        {
            d[3] = (p[2] << left) | (p[1] >> right);
            d[2] = (p[1] << left) | (p[0] >> right);
            d[1] = p[0] << left;
            d[0] = 0;
        }
        else if (b == 128)
        {
            d[3] = p[1];
            d[2] = p[0];
            d[1] = 0;
            d[0] = 0;
        }
        else if (b < 192)
        {
            d[3] = (p[1] << left) | (p[0] >> right);
            d[2] = p[0] << left;
            d[1] = 0;
            d[0] = 0;
        }
        else if (b == 192)
        {
            d[3] = p[0];
            d[2] = 0;
            d[1] = 0;
            d[0] = 0;
        }
        else
        {
            d[3] = p[0] << left;
            d[2] = 0;
            d[1] = 0;
            d[0] = 0;
        }

        po[3] = d[3];
        po[2] = d[2];
        po[1] = d[1];
        po[0] = d[0];

        d[3] = 0;
        d[2] = 0;
        d[1] = 0;
        d[0] = 0;
    }
#endif
}

uint32_t Add32(void* output, const void* n1, const void* n2)
{
#ifdef DEFINED_INT128
    uint128_t* po = (uint128_t*)output;
    const uint128_t* p1 = (const uint128_t*)n1;
    const uint128_t* p2 = (const uint128_t*)n2;

    uint128_t sum;
    uint32_t carry = 0;
    for (int i = 0; i < 2; i++)
    {
        sum = p1[i] + p2[i] + carry;
        if (carry)
        {
            carry = (sum > p1[i]) ? 0 : 1;
        }
        else
        {
            carry = (sum >= p1[i]) ? 0 : 1;
        }
        po[i] = sum;
    }
    sum = 0;

    return carry;
#else
    uint64_t* po = (uint64_t*)output;
    const uint64_t* p1 = (const uint64_t*)n1;
    const uint64_t* p2 = (const uint64_t*)n2;

    uint64_t sum;
    uint32_t carry = 0;
    for (int i = 0; i < 4; i++)
    {
        sum = p1[i] + p2[i] + carry;
        if (carry)
        {
            carry = (sum > p1[i]) ? 0 : 1;
        }
        else
        {
            carry = (sum >= p1[i]) ? 0 : 1;
        }
        po[i] = sum;
    }
    sum = 0;

    return carry;
#endif
}

uint32_t Sub32(void* output, const void* n1, const void* n2)
{
#ifdef DEFINED_INT128
    uint128_t* po = (uint128_t*)output;
    const uint128_t* p1 = (const uint128_t*)n1;
    const uint128_t* p2 = (const uint128_t*)n2;

    uint128_t diff;
    uint32_t borrow = 0; // p1[0] >= p2[0] ? 0 : 1;
    for (int i = 0; i < 2; i++)
    {
        diff = p1[i] - p2[i] - borrow;
        if (borrow)
        {
            borrow = p1[i] > p2[i] ? 0 : 1;
        }
        else
        {
            borrow = p1[i] >= p2[i] ? 0 : 1;
        }
        po[i] = diff;
    }
    diff = 0;

    return borrow;
#else
    uint64_t* po = (uint64_t*)output;
    const uint64_t* p1 = (const uint64_t*)n1;
    const uint64_t* p2 = (const uint64_t*)n2;

    uint64_t diff;
    uint32_t borrow = 0; // p1[0] >= p2[0] ? 0 : 1;
    for (int i = 0; i < 4; i++)
    {
        diff = p1[i] - p2[i] - borrow;
        if (borrow)
        {
            borrow = p1[i] > p2[i] ? 0 : 1;
        }
        else
        {
            borrow = p1[i] >= p2[i] ? 0 : 1;
        }
        po[i] = diff;
    }
    diff = 0;

    return borrow;
#endif
}

void Mul32(void* output, const void* n1, const void* n2)
{
#ifdef DEFINED_INT128
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
#else
    uint64_t* po = (uint64_t*)output;
    const uint32_t* p1 = (const uint32_t*)n1;
    const uint32_t* p2 = (const uint32_t*)n2;

    uint64_t product[64];
    // 1,0
    product[0] = (uint64_t)p1[0] * p2[0];
    // 2,1
    product[1] = (uint64_t)p1[0] * p2[1];
    product[2] = (uint64_t)p1[1] * p2[0];
    // 3,2
    product[3] = (uint64_t)p1[0] * p2[2];
    product[4] = (uint64_t)p1[1] * p2[1];
    product[5] = (uint64_t)p1[2] * p2[0];
    // 4,3
    product[6] = (uint64_t)p1[0] * p2[3];
    product[7] = (uint64_t)p1[1] * p2[2];
    product[8] = (uint64_t)p1[2] * p2[1];
    product[9] = (uint64_t)p1[3] * p2[0];
    // 5,4
    product[10] = (uint64_t)p1[0] * p2[4];
    product[11] = (uint64_t)p1[1] * p2[3];
    product[12] = (uint64_t)p1[2] * p2[2];
    product[13] = (uint64_t)p1[3] * p2[1];
    product[14] = (uint64_t)p1[4] * p2[0];
    // 6,5
    product[15] = (uint64_t)p1[0] * p2[5];
    product[16] = (uint64_t)p1[1] * p2[4];
    product[17] = (uint64_t)p1[2] * p2[3];
    product[18] = (uint64_t)p1[3] * p2[2];
    product[19] = (uint64_t)p1[4] * p2[1];
    product[20] = (uint64_t)p1[5] * p2[0];
    // 7,6
    product[21] = (uint64_t)p1[0] * p2[6];
    product[22] = (uint64_t)p1[1] * p2[5];
    product[23] = (uint64_t)p1[2] * p2[4];
    product[24] = (uint64_t)p1[3] * p2[3];
    product[25] = (uint64_t)p1[4] * p2[2];
    product[26] = (uint64_t)p1[5] * p2[1];
    product[27] = (uint64_t)p1[6] * p2[0];
    // 8,7
    product[28] = (uint64_t)p1[0] * p2[7];
    product[29] = (uint64_t)p1[1] * p2[6];
    product[30] = (uint64_t)p1[2] * p2[5];
    product[31] = (uint64_t)p1[3] * p2[4];
    product[32] = (uint64_t)p1[4] * p2[3];
    product[33] = (uint64_t)p1[5] * p2[2];
    product[34] = (uint64_t)p1[6] * p2[1];
    product[35] = (uint64_t)p1[7] * p2[0];
    // 9,8
    product[36] = (uint64_t)p1[1] * p2[7];
    product[37] = (uint64_t)p1[2] * p2[6];
    product[38] = (uint64_t)p1[3] * p2[5];
    product[39] = (uint64_t)p1[4] * p2[4];
    product[40] = (uint64_t)p1[5] * p2[3];
    product[41] = (uint64_t)p1[6] * p2[2];
    product[42] = (uint64_t)p1[7] * p2[1];
    // 10,9
    product[43] = (uint64_t)p1[2] * p2[7];
    product[44] = (uint64_t)p1[3] * p2[6];
    product[45] = (uint64_t)p1[4] * p2[5];
    product[46] = (uint64_t)p1[5] * p2[4];
    product[47] = (uint64_t)p1[6] * p2[3];
    product[48] = (uint64_t)p1[7] * p2[2];
    // 11,10
    product[49] = (uint64_t)p1[3] * p2[7];
    product[50] = (uint64_t)p1[4] * p2[6];
    product[51] = (uint64_t)p1[5] * p2[5];
    product[52] = (uint64_t)p1[6] * p2[4];
    product[53] = (uint64_t)p1[7] * p2[3];
    // 12,11
    product[54] = (uint64_t)p1[4] * p2[7];
    product[55] = (uint64_t)p1[5] * p2[6];
    product[56] = (uint64_t)p1[6] * p2[5];
    product[57] = (uint64_t)p1[7] * p2[4];
    // 13,12
    product[58] = (uint64_t)p1[5] * p2[7];
    product[59] = (uint64_t)p1[6] * p2[6];
    product[60] = (uint64_t)p1[7] * p2[5];
    // 14,13
    product[61] = (uint64_t)p1[6] * p2[7];
    product[62] = (uint64_t)p1[7] * p2[6];
    // 15,14
    product[63] = (uint64_t)p1[7] * p2[7];

    uint32_t* p = (uint32_t*)product;
    po[0] = (uint64_t)p[0];
    po[1] = (uint64_t)p[1] + p[2] + p[4];
    po[2] = (uint64_t)p[3] + p[5] + p[6] + p[8] + p[10];
    po[3] = (uint64_t)p[7] + p[9] + p[11] + p[12] + p[14] + p[16] + p[18];
    po[4] = (uint64_t)p[13] + p[15] + p[17] + p[19] + p[20] + p[22] + p[24] + p[26] + p[28];
    po[5] = (uint64_t)p[21] + p[23] + p[25] + p[27] + p[29] + p[30] + p[32] + p[34] + p[36] + p[38] + p[40];
    po[6] = (uint64_t)p[31] + p[33] + p[35] + p[37] + p[39] + p[41] + p[42] + p[44] + p[46] + p[48] + p[50] + p[52] + p[54];
    po[7] = (uint64_t)p[43] + p[45] + p[47] + p[49] + p[51] + p[53] + p[55] + p[56] + p[58] + p[60] + p[62] + p[64] + p[66] + p[68] + p[70];
    po[8] = (uint64_t)p[57] + p[59] + p[61] + p[63] + p[65] + p[67] + p[69] + p[71] + p[72] + p[74] + p[76] + p[78] + p[80] + p[82] + p[84];
    po[9] = (uint64_t)p[73] + p[75] + p[77] + p[79] + p[81] + p[83] + p[85] + p[86] + p[88] + p[90] + p[92] + p[94] + p[96];
    po[10] = (uint64_t)p[87] + p[89] + p[91] + p[93] + p[95] + p[97] + p[98] + p[100] + p[102] + p[104] + p[106];
    po[11] = (uint64_t)p[99] + p[101] + p[103] + p[105] + p[107] + p[108] + p[110] + p[112] + p[114];
    po[12] = (uint64_t)p[109] + p[111] + p[113] + p[115] + p[116] + p[118] + p[120];
    po[13] = (uint64_t)p[117] + p[119] + p[121] + p[122] + p[124];
    po[14] = (uint64_t)p[123] + p[125] + p[126];
    po[15] = (uint64_t)p[127];

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
    product[16] = 0;
    product[17] = 0;
    product[18] = 0;
    product[19] = 0;
    product[20] = 0;
    product[21] = 0;
    product[22] = 0;
    product[23] = 0;
    product[24] = 0;
    product[25] = 0;
    product[26] = 0;
    product[27] = 0;
    product[28] = 0;
    product[29] = 0;
    product[30] = 0;
    product[31] = 0;
    product[32] = 0;
    product[33] = 0;
    product[34] = 0;
    product[35] = 0;
    product[36] = 0;
    product[37] = 0;
    product[38] = 0;
    product[39] = 0;
    product[40] = 0;
    product[41] = 0;
    product[42] = 0;
    product[43] = 0;
    product[44] = 0;
    product[45] = 0;
    product[46] = 0;
    product[47] = 0;
    product[48] = 0;
    product[49] = 0;
    product[50] = 0;
    product[51] = 0;
    product[52] = 0;
    product[53] = 0;
    product[54] = 0;
    product[55] = 0;
    product[56] = 0;
    product[57] = 0;
    product[58] = 0;
    product[59] = 0;
    product[60] = 0;
    product[61] = 0;
    product[62] = 0;
    product[63] = 0;
#endif
}

// output[0, 15] = n16[0, 7] * n8[0, 7]
void Multiply8_8(void* output, const void* n1, const void* n2)
{
#ifdef DEFINED_INT128
    uint128_t* po = (uint128_t*)output;
    const uint64_t* p1 = (const uint64_t*)n1;
    const uint64_t* p2 = (const uint64_t*)n2;
    *po = (uint128_t)*p1 * *p2;
#else
    uint32_t* po = (uint32_t*)output;
    const uint32_t* p1 = (const uint32_t*)n1;
    const uint32_t* p2 = (const uint32_t*)n2;

    uint64_t product[4];
    // 1,0
    product[0] = (uint64_t)p1[0] * p2[0];
    // 2,1
    product[1] = (uint64_t)p1[0] * p2[1];
    product[2] = (uint64_t)p1[1] * p2[0];
    // 3,2
    product[3] = (uint64_t)p1[1] * p2[1];

    const uint32_t* p = (const uint32_t*)product;
    Zero16(po);

    po[0] = p[0];
    *(uint64_t*)&po[1] = (uint64_t)p[1] + p[2] + p[4];
    *(uint64_t*)&po[2] += (uint64_t)p[3] + p[5] + product[3];

    product[0] = 0;
    product[1] = 0;
    product[2] = 0;
    product[3] = 0;
#endif
}

void Multiply16_8(void* output, const void* n1, const void* n2)
{
#ifdef DEFINED_INT128
    uint64_t* po = (uint64_t*)output;
    const uint64_t* p1 = (const uint64_t*)n1;
    const uint64_t* p2 = (const uint64_t*)n2;

    uint128_t product[2];
    product[0] = (uint128_t)p1[0] * *p2;
    product[1] = (uint128_t)p1[1] * *p2;

    const uint64_t* p = (const uint64_t*)product;
    Zero32(po);
    po[0] = p[0];
    *(uint128_t*)&po[1] = (uint128_t)p[1] + product[1];

    product[0] = 0;
    product[1] = 0;
#else
    uint32_t* po = (uint32_t*)output;
    const uint32_t* p1 = (const uint32_t*)n1;
    const uint32_t* p2 = (const uint32_t*)n2;

    uint64_t product[8];
    // 1,0
    product[0] = (uint64_t)p1[0] * p2[0];
    // 2,1
    product[1] = (uint64_t)p1[0] * p2[1];
    product[2] = (uint64_t)p1[1] * p2[0];
    // 3,2
    product[3] = (uint64_t)p1[1] * p2[1];
    product[4] = (uint64_t)p1[2] * p2[0];
    // 4,3
    product[5] = (uint64_t)p1[2] * p2[1];
    product[6] = (uint64_t)p1[3] * p2[0];
    // 5,4
    product[7] = (uint64_t)p1[3] * p2[1];

    const uint32_t* p = (const uint32_t*)product;
    Zero32(po);
    po[0] = p[0];
    *(uint64_t*)&po[1] = (uint64_t)p[1] + p[2] + p[4];
    *(uint64_t*)&po[2] += (uint64_t)p[3] + p[5] + p[6] + p[8];
    *(uint64_t*)&po[3] += (uint64_t)p[7] + p[9] + p[10] + p[12];
    *(uint64_t*)&po[4] += (uint64_t)p[11] + p[13] + product[7];

    product[0] = 0;
    product[1] = 0;
    product[2] = 0;
    product[3] = 0;
    product[4] = 0;
    product[5] = 0;
    product[6] = 0;
    product[7] = 0;

#endif
}

void Multiply32_32(void* output, const void* n1, const void* n2)
{
#ifdef DEFINED_INT128
    uint64_t* po = (uint64_t*)output;
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
    Zero64(po);
    po[0] = p[0];
    *(uint128_t*)&po[1] = (uint128_t)p[1] + p[2] + p[4];
    *(uint128_t*)&po[2] += (uint128_t)p[3] + p[5] + p[6] + p[8] + p[10];
    *(uint128_t*)&po[3] += (uint128_t)p[7] + p[9] + p[11] + p[12] + p[14] + p[16] + p[18];
    *(uint128_t*)&po[4] += (uint128_t)p[13] + p[15] + p[17] + p[19] + p[20] + p[22] + p[24];
    *(uint128_t*)&po[5] += (uint128_t)p[21] + p[23] + p[25] + p[26] + p[28];
    *(uint128_t*)&po[6] += (uint128_t)p[27] + p[29] + product[15];

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
#else
    uint32_t* po = (uint32_t*)output;
    const uint32_t* p1 = (const uint32_t*)n1;
    const uint32_t* p2 = (const uint32_t*)n2;

    uint64_t product[64];
    // 1,0
    product[0] = (uint64_t)p1[0] * p2[0];
    // 2,1
    product[1] = (uint64_t)p1[0] * p2[1];
    product[2] = (uint64_t)p1[1] * p2[0];
    // 3,2
    product[3] = (uint64_t)p1[0] * p2[2];
    product[4] = (uint64_t)p1[1] * p2[1];
    product[5] = (uint64_t)p1[2] * p2[0];
    // 4,3
    product[6] = (uint64_t)p1[0] * p2[3];
    product[7] = (uint64_t)p1[1] * p2[2];
    product[8] = (uint64_t)p1[2] * p2[1];
    product[9] = (uint64_t)p1[3] * p2[0];
    // 5,4
    product[10] = (uint64_t)p1[0] * p2[4];
    product[11] = (uint64_t)p1[1] * p2[3];
    product[12] = (uint64_t)p1[2] * p2[2];
    product[13] = (uint64_t)p1[3] * p2[1];
    product[14] = (uint64_t)p1[4] * p2[0];
    // 6,5
    product[15] = (uint64_t)p1[0] * p2[5];
    product[16] = (uint64_t)p1[1] * p2[4];
    product[17] = (uint64_t)p1[2] * p2[3];
    product[18] = (uint64_t)p1[3] * p2[2];
    product[19] = (uint64_t)p1[4] * p2[1];
    product[20] = (uint64_t)p1[5] * p2[0];
    // 7,6
    product[21] = (uint64_t)p1[0] * p2[6];
    product[22] = (uint64_t)p1[1] * p2[5];
    product[23] = (uint64_t)p1[2] * p2[4];
    product[24] = (uint64_t)p1[3] * p2[3];
    product[25] = (uint64_t)p1[4] * p2[2];
    product[26] = (uint64_t)p1[5] * p2[1];
    product[27] = (uint64_t)p1[6] * p2[0];
    // 8,7
    product[28] = (uint64_t)p1[0] * p2[7];
    product[29] = (uint64_t)p1[1] * p2[6];
    product[30] = (uint64_t)p1[2] * p2[5];
    product[31] = (uint64_t)p1[3] * p2[4];
    product[32] = (uint64_t)p1[4] * p2[3];
    product[33] = (uint64_t)p1[5] * p2[2];
    product[34] = (uint64_t)p1[6] * p2[1];
    product[35] = (uint64_t)p1[7] * p2[0];
    // 9,8
    product[36] = (uint64_t)p1[1] * p2[7];
    product[37] = (uint64_t)p1[2] * p2[6];
    product[38] = (uint64_t)p1[3] * p2[5];
    product[39] = (uint64_t)p1[4] * p2[4];
    product[40] = (uint64_t)p1[5] * p2[3];
    product[41] = (uint64_t)p1[6] * p2[2];
    product[42] = (uint64_t)p1[7] * p2[1];
    // 10,9
    product[43] = (uint64_t)p1[2] * p2[7];
    product[44] = (uint64_t)p1[3] * p2[6];
    product[45] = (uint64_t)p1[4] * p2[5];
    product[46] = (uint64_t)p1[5] * p2[4];
    product[47] = (uint64_t)p1[6] * p2[3];
    product[48] = (uint64_t)p1[7] * p2[2];
    // 11,10
    product[49] = (uint64_t)p1[3] * p2[7];
    product[50] = (uint64_t)p1[4] * p2[6];
    product[51] = (uint64_t)p1[5] * p2[5];
    product[52] = (uint64_t)p1[6] * p2[4];
    product[53] = (uint64_t)p1[7] * p2[3];
    // 12,11
    product[54] = (uint64_t)p1[4] * p2[7];
    product[55] = (uint64_t)p1[5] * p2[6];
    product[56] = (uint64_t)p1[6] * p2[5];
    product[57] = (uint64_t)p1[7] * p2[4];
    // 13,12
    product[58] = (uint64_t)p1[5] * p2[7];
    product[59] = (uint64_t)p1[6] * p2[6];
    product[60] = (uint64_t)p1[7] * p2[5];
    // 14,13
    product[61] = (uint64_t)p1[6] * p2[7];
    product[62] = (uint64_t)p1[7] * p2[6];
    // 15,14
    product[63] = (uint64_t)p1[7] * p2[7];

    uint32_t* p = (uint32_t*)product;
    Zero64(po);
    po[0] = p[0];
    *(uint64_t*)&po[1] = (uint64_t)p[1] + p[2] + p[4];
    *(uint64_t*)&po[2] += (uint64_t)p[3] + p[5] + p[6] + p[8] + p[10];
    *(uint64_t*)&po[3] += (uint64_t)p[7] + p[9] + p[11] + p[12] + p[14] + p[16] + p[18];
    *(uint64_t*)&po[4] += (uint64_t)p[13] + p[15] + p[17] + p[19] + p[20] + p[22] + p[24] + p[26] + p[28];
    *(uint64_t*)&po[5] += (uint64_t)p[21] + p[23] + p[25] + p[27] + p[29] + p[30] + p[32] + p[34] + p[36] + p[38] + p[40];
    *(uint64_t*)&po[6] += (uint64_t)p[31] + p[33] + p[35] + p[37] + p[39] + p[41] + p[42] + p[44] + p[46] + p[48] + p[50] + p[52] + p[54];
    *(uint64_t*)&po[7] += (uint64_t)p[43] + p[45] + p[47] + p[49] + p[51] + p[53] + p[55] + p[56] + p[58] + p[60] + p[62] + p[64] + p[66] + p[68] + p[70];
    *(uint64_t*)&po[8] += (uint64_t)p[57] + p[59] + p[61] + p[63] + p[65] + p[67] + p[69] + p[71] + p[72] + p[74] + p[76] + p[78] + p[80] + p[82] + p[84];
    *(uint64_t*)&po[9] += (uint64_t)p[73] + p[75] + p[77] + p[79] + p[81] + p[83] + p[85] + p[86] + p[88] + p[90] + p[92] + p[94] + p[96];
    *(uint64_t*)&po[10] += (uint64_t)p[87] + p[89] + p[91] + p[93] + p[95] + p[97] + p[98] + p[100] + p[102] + p[104] + p[106];
    *(uint64_t*)&po[11] += (uint64_t)p[99] + p[101] + p[103] + p[105] + p[107] + p[108] + p[110] + p[112] + p[114];
    *(uint64_t*)&po[12] += (uint64_t)p[109] + p[111] + p[113] + p[115] + p[116] + p[118] + p[120];
    *(uint64_t*)&po[13] += (uint64_t)p[117] + p[119] + p[121] + p[122] + p[124];
    *(uint64_t*)&po[14] += (uint64_t)p[123] + p[125] + product[63];

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
    product[16] = 0;
    product[17] = 0;
    product[18] = 0;
    product[19] = 0;
    product[20] = 0;
    product[21] = 0;
    product[22] = 0;
    product[23] = 0;
    product[24] = 0;
    product[25] = 0;
    product[26] = 0;
    product[27] = 0;
    product[28] = 0;
    product[29] = 0;
    product[30] = 0;
    product[31] = 0;
    product[32] = 0;
    product[33] = 0;
    product[34] = 0;
    product[35] = 0;
    product[36] = 0;
    product[37] = 0;
    product[38] = 0;
    product[39] = 0;
    product[40] = 0;
    product[41] = 0;
    product[42] = 0;
    product[43] = 0;
    product[44] = 0;
    product[45] = 0;
    product[46] = 0;
    product[47] = 0;
    product[48] = 0;
    product[49] = 0;
    product[50] = 0;
    product[51] = 0;
    product[52] = 0;
    product[53] = 0;
    product[54] = 0;
    product[55] = 0;
    product[56] = 0;
    product[57] = 0;
    product[58] = 0;
    product[59] = 0;
    product[60] = 0;
    product[61] = 0;
    product[62] = 0;
    product[63] = 0;
#endif
}

} // namespace curve25519
