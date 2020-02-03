// Copyright (c) 2019-2020 The Bigbang developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CRYPTO_CURVE25519_FP25519_H
#define CRYPTO_CURVE25519_FP25519_H

#include <cstdint>
#include <string>

namespace curve25519
{

class CFP25519
{
public:
    CFP25519();
    CFP25519(const CFP25519& fp);
    CFP25519(CFP25519&& fp);
    CFP25519& operator=(const CFP25519& fp);
    CFP25519& operator=(CFP25519&& fp);
    CFP25519(const uint64_t u64);
    CFP25519(const uint8_t* md32);
    virtual ~CFP25519();
    void Pack(uint8_t* md32) const;

    // return (1/value) % prime
    const CFP25519 Inverse() const;
    // return (value ^ md32[0, 31]) % prime
    const CFP25519 Power(const uint8_t* md32) const;
    // return (value ^ 1/2) % prime
    const CFP25519 Sqrt() const;
    // value = value * value
    CFP25519& Square();
    // value == 0
    bool IsZero() const;
    // return 1 if value is odd, or 0
    uint8_t Parity() const;

    // value += b.value
    CFP25519& operator+=(const CFP25519& b);
    // value -= b.value
    CFP25519& operator-=(const CFP25519& b);
    // value *= b.value
    CFP25519& operator*=(const CFP25519& b);
    // return -value
    const CFP25519 operator-() const;
    // return value + b.value
    const CFP25519 operator+(const CFP25519& b) const;
    // return value - b.value
    const CFP25519 operator-(const CFP25519& b) const;
    // return value * b.value
    const CFP25519 operator*(const CFP25519& b) const;
    // return value / b.value
    const CFP25519 operator/(const CFP25519& b) const;
    // return value == b.value
    bool operator==(const CFP25519& b) const;
    // return value != b.value
    bool operator!=(const CFP25519& b) const;

protected:
    // value = (carry * 2^256 + value) % p
    void Range(uint32_t carry);
    // if value >= p, value -= p.
    // if value < 0, value += p.
    void Reduce();
    // return pow(value, (prime - 5)/8) % prime
    const CFP25519 Power58() const;

public:
    uint64_t value[4];

protected:
    static const uint64_t prime[4];
};

} // namespace curve25519

#endif // CRYPTO_CURVE25519_FP25519_H
