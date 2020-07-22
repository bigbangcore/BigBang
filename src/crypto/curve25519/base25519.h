// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CRYPTO_CURVE25519_BASE25519_H
#define CRYPTO_CURVE25519_BASE25519_H

#include <cstddef>
#include <cstdint>

#ifdef __SIZEOF_INT128__
#define DEFINED_INT128
using uint128_t = __uint128_t;
using int128_t = __int128_t;
#else
#undef DEFINED_INT128
#endif

namespace curve25519
{
// print hex(b[0, size - 1])
void Print(const void* p, size_t size, const char* prefix = "");
// print hex(b[0, 31])
void Print32(const void* p, const char* prefix = "");
// n[0, 15] = 0
void Zero16(void* b);
// n[0, 31] = 0
void Zero32(void* b);
// n[0, 63] = 0
void Zero64(void* b);
// dest[0, 31] = src[0, 31]
void Copy32(void* dest, const void* src);
// dest[0, 7] = u64, dest[8, 31] = u64
void Copy32(void* dest, const uint64_t u64);

// if uint(lhs[0, 31]) > uint(rhs[0, 31]), return 1
// if uint(lhs[0, 31]) == uint(rhs[0, 31], return 0
// if uint(lhs[0, 31]) < uint(rhs[0, 31], return -1
int Compare32(const void* lhs, const void* rhs);
// if int(lhs[0, 31]) > int(rhs[0, 31]), return 1
// if int(lhs[0, 31]) == int(rhs[0, 31], return 0
// if int(lhs[0, 31]) < int(rhs[0, 31], return -1
int CompareSigned32(const void* lhs, const void* rhs);

// if n >= prime, output = n - prime
// if n < 0, output = n + prime
void ReduceSigned32(void* output, const void* n, const void* prime);

// return n[0, 31] == 0
bool IsZero32(const void* n);
// return n[0, 31] == 1
bool IsOne32(const void* n);
// return n[0] & 1 == 0
bool IsEven(const void* n);

// uint(n[0, 31]) /= 2
void Half32(void* n);
// int(n[0, 31]) /= 2
void HalfSigned32(void* n);
// uint(n[0, 31]) <<= b
void ShiftLeft32(void* output, const void* n, size_t b);

// output[0, 31] = n1[0, 31] + n2[0, 31]. return carry = 0 or 1
uint32_t Add32(void* output, const void* n1, const void* n2);
// output[0, 31] = n1[0, 31] - n2[0, 31]. return borrow = 0 or 1
uint32_t Sub32(void* output, const void* n1, const void* n2);

// output[0, 127] = n1[0, 31] * n2[0, 31].
// output must be initialed by 0.
// output[i*16, i*16+7] is 64-bits product, [i*16+8, i*16+15] is carry, 0 <= i < 8
void Mul32(void* output, const void* n1, const void* n2);

// output[0, 15] = n1[0, 7] * n2[0, 7]
void Multiply8_8(void* output, const void* n1, const void* n2);
// output[0, 31] = n1[0, 15] * n2[0, 7]
void Multiply16_8(void* output, const void* n1, const void* n2);
// output[0, 63] = n1[0, 31] * n2[0, 31]
void Multiply32_32(void* output, const void* n1, const void* n2);

} // namespace curve25519

#endif // CRYPTO_CURVE25519_BASE25519_H
